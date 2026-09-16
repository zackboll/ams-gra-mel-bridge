#include <ams_mel/abi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef AMS_MEL_TEST_MOCK_PROVIDER
#error "mock provider path is required"
#endif
#ifndef AMS_MEL_TEST_MISSING_SYMBOL_PROVIDER
#error "missing-symbol provider path is required"
#endif
#ifndef AMS_MEL_TEST_LIFETIME_LOG
#error "lifetime log path is required"
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return EXIT_FAILURE; \
    } \
} while (0)

static ams_mel_status_t open_instance(const char *path, const char *instance,
                                      ams_mel_session **session,
                                      char *diagnostic, size_t capacity)
{
    size_t required = 0;
    ams_mel_status_t status = ams_mel_session_open(
        path, instance, "aperture-A", session, diagnostic, capacity, &required);
    CHECK(required >= 1);
    return status;
}

static int expect_open_failure(const char *path, const char *instance,
                               ams_mel_status_t expected)
{
    ams_mel_session *session = NULL;
    char diagnostic[128] = {0};
    CHECK(open_instance(path, instance, &session, diagnostic,
                        sizeof diagnostic) == expected);
    CHECK(session == NULL);
    CHECK(diagnostic[0] != '\0');
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_success(void)
{
    ams_mel_session *session = NULL;
    char diagnostic[128] = "not cleared";
    size_t diagnostic_required = 99;
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "success",
          "aperture-A", &session, diagnostic, sizeof diagnostic,
          &diagnostic_required) == AMS_MEL_OK);
    CHECK(session != NULL);
    CHECK(diagnostic[0] == '\0' && diagnostic_required == 1);

    ams_mel_provider_version_v1 version = {
        UINT32_MAX, UINT32_MAX, NULL, 0, 0, NULL, 0, 0
    };
    CHECK(ams_mel_session_get_provider_version(session, &version, NULL, 0,
          NULL) == AMS_MEL_BUFFER_TOO_SMALL);
    CHECK(version.api_version == UINT32_MAX);
    CHECK(version.library_version == UINT32_MAX);
    CHECK(version.vendor_required == strlen("Mock IR Provider \xC2\xB5") + 1);
    CHECK(version.description_required ==
          strlen("Deterministic task 001 provider") + 1);

    char vendor[64] = "unchanged";
    char description[64] = "unchanged";
    version.vendor = vendor;
    version.vendor_capacity = version.vendor_required - 1;
    version.description = description;
    version.description_capacity = sizeof description;
    CHECK(ams_mel_session_get_provider_version(session, &version, NULL, 0,
          NULL) == AMS_MEL_BUFFER_TOO_SMALL);
    CHECK(strcmp(vendor, "unchanged") == 0);
    CHECK(strcmp(description, "unchanged") == 0);
    CHECK(version.api_version == UINT32_MAX);

    version.vendor_capacity = sizeof vendor;
    CHECK(ams_mel_session_get_provider_version(session, &version, diagnostic,
          sizeof diagnostic, &diagnostic_required) == AMS_MEL_OK);
    CHECK(version.api_version == UINT32_C(0x12345678));
    CHECK(version.library_version == UINT32_C(0x90abcdef));
    CHECK(strcmp(vendor, "Mock IR Provider \xC2\xB5") == 0);
    CHECK(strcmp(description, "Deterministic task 001 provider") == 0);

    CHECK(ams_mel_session_close(&session, diagnostic, sizeof diagnostic,
          &diagnostic_required) == AMS_MEL_OK);
    CHECK(session == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_arguments(void)
{
    ams_mel_session *session = NULL;
    ams_mel_session *sentinel = (ams_mel_session *)(uintptr_t)1;
    ams_mel_provider_version_v1 version = {0};
    CHECK(ams_mel_session_open(NULL, "x", "", &session, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, NULL, "", &session,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "x", NULL, &session,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "x", "", NULL,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "x", "", &sentinel,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "x", "", &session,
          NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_session_get_provider_version(NULL, &version, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_session_close(NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(session == NULL);
    return EXIT_SUCCESS;
}

static int test_lifetime_log(void)
{
    FILE *stream = fopen(AMS_MEL_TEST_LIFETIME_LOG, "r");
    char contents[1024] = {0};
    size_t count;
    CHECK(stream != NULL);
    count = fread(contents, 1, sizeof contents - 1, stream);
    CHECK(fclose(stream) == 0);
    contents[count] = '\0';
    const char *control = strstr(contents, "control_destroyed\n");
    const char *manager = strstr(contents, "manager_destroyed\n");
    const char *unloaded = strstr(contents, "library_unloaded\n");
    CHECK(control != NULL && manager != NULL && unloaded != NULL);
    CHECK(control < manager && manager < unloaded);
    return EXIT_SUCCESS;
}

static int test_version_exception(void)
{
    ams_mel_session *session = NULL;
    ams_mel_provider_version_v1 version = {0};
    char diagnostic[128] = {0};
    CHECK(open_instance(AMS_MEL_TEST_MOCK_PROVIDER, "throw-version", &session,
                        diagnostic, sizeof diagnostic) == AMS_MEL_OK);
    CHECK(ams_mel_session_get_provider_version(session, &version, diagnostic,
          sizeof diagnostic, NULL) == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(strstr(diagnostic, "mock version exception") != NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

int main(void)
{
    (void)remove(AMS_MEL_TEST_LIFETIME_LOG);
    CHECK(test_arguments() == EXIT_SUCCESS);
    CHECK(expect_open_failure("/definitely/missing/libirmel.so", "x",
          AMS_MEL_LIBRARY_LOAD_FAILED) == EXIT_SUCCESS);
    CHECK(expect_open_failure(AMS_MEL_TEST_MISSING_SYMBOL_PROVIDER, "x",
          AMS_MEL_SYMBOL_NOT_FOUND) == EXIT_SUCCESS);
    CHECK(expect_open_failure(AMS_MEL_TEST_MOCK_PROVIDER, "null-manager",
          AMS_MEL_FACTORY_FAILED) == EXIT_SUCCESS);
    CHECK(expect_open_failure(AMS_MEL_TEST_MOCK_PROVIDER, "null-control",
          AMS_MEL_FACTORY_FAILED) == EXIT_SUCCESS);
    (void)remove(AMS_MEL_TEST_LIFETIME_LOG);
    CHECK(expect_open_failure(AMS_MEL_TEST_MOCK_PROVIDER, "init-fail",
          AMS_MEL_INITIALIZATION_FAILED) == EXIT_SUCCESS);
    CHECK(test_lifetime_log() == EXIT_SUCCESS);
    CHECK(expect_open_failure(AMS_MEL_TEST_MOCK_PROVIDER, "throw-control",
          AMS_MEL_PROVIDER_EXCEPTION) == EXIT_SUCCESS);
    CHECK(expect_open_failure(AMS_MEL_TEST_MOCK_PROVIDER, "throw-init",
          AMS_MEL_PROVIDER_EXCEPTION) == EXIT_SUCCESS);
    CHECK(test_version_exception() == EXIT_SUCCESS);
    (void)remove(AMS_MEL_TEST_LIFETIME_LOG);
    CHECK(test_success() == EXIT_SUCCESS);
    CHECK(test_lifetime_log() == EXIT_SUCCESS);
    for (int cycle = 1; cycle < 20; ++cycle) {
        CHECK(test_success() == EXIT_SUCCESS);
    }
    puts("PASS: C provider load/init/version/close contract");
    return EXIT_SUCCESS;
}