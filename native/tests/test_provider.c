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

static int expect_open_failure_without_diagnostic(const char *instance,
                                                  ams_mel_status_t expected)
{
    ams_mel_session *session = NULL;
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, instance,
          "aperture-A", &session, NULL, 0, NULL) == expected);
    CHECK(session == NULL);
    return EXIT_SUCCESS;
}

static int test_invalid_exception_diagnostic(void)
{
    ams_mel_session *session = NULL;
    char diagnostic[64] = {0};
    size_t required = 0;
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER,
          "invalid-diagnostic", "", &session, diagnostic, sizeof diagnostic,
          &required) == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(session == NULL);
    CHECK(strcmp(diagnostic, "provider exception") == 0);
    CHECK(required == strlen("provider exception") + 1);
    return EXIT_SUCCESS;
}

static int expect_lifetime_log(const char *expected)
{
    FILE *stream = fopen(AMS_MEL_TEST_LIFETIME_LOG, "rb");
    char contents[1024] = {0};
    size_t count;
    CHECK(stream != NULL);
    count = fread(contents, 1, sizeof contents - 1, stream);
    CHECK(!ferror(stream));
    CHECK(fclose(stream) == 0);
    contents[count] = '\0';
    CHECK(strcmp(contents, expected) == 0);
    return EXIT_SUCCESS;
}

static int expect_instrumented_open_failure(const char *instance,
                                            ams_mel_status_t expected,
                                            const char *events)
{
    (void)remove(AMS_MEL_TEST_LIFETIME_LOG);
    CHECK(expect_open_failure(AMS_MEL_TEST_MOCK_PROVIDER, instance, expected) ==
          EXIT_SUCCESS);
    CHECK(expect_lifetime_log(events) == EXIT_SUCCESS);
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

static int test_version_failure(const char *instance,
                                ams_mel_status_t expected)
{
    ams_mel_session *session = NULL;
    char vendor[32] = "vendor unchanged";
    char description[32] = "description unchanged";
    char diagnostic[128] = {0};
    ams_mel_provider_version_v1 version = {
        UINT32_C(0xaaaaaaaa), UINT32_C(0xbbbbbbbb), vendor, sizeof vendor,
        123, description, sizeof description, 456
    };
    CHECK(open_instance(AMS_MEL_TEST_MOCK_PROVIDER, instance, &session,
                        diagnostic, sizeof diagnostic) == AMS_MEL_OK);
    CHECK(ams_mel_session_get_provider_version(session, &version, diagnostic,
          sizeof diagnostic, NULL) == expected);
    CHECK(version.api_version == UINT32_C(0xaaaaaaaa));
    CHECK(version.library_version == UINT32_C(0xbbbbbbbb));
    CHECK(version.vendor_required == 123);
    CHECK(version.description_required == 456);
    CHECK(strcmp(vendor, "vendor unchanged") == 0);
    CHECK(strcmp(description, "description unchanged") == 0);
    CHECK(diagnostic[0] != '\0');
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
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

static int test_utf8_diagnostic_truncation(void)
{
    ams_mel_session *session = NULL;
    ams_mel_provider_version_v1 version = {0};
    char diagnostic[7] = "xxxxxx";
    size_t required = 0;
    CHECK(open_instance(AMS_MEL_TEST_MOCK_PROVIDER, "throw-version-utf8",
                        &session, diagnostic, sizeof diagnostic) == AMS_MEL_OK);
    CHECK(ams_mel_session_get_provider_version(session, &version, diagnostic,
          sizeof diagnostic, &required) == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(strcmp(diagnostic, "mock ") == 0);
    CHECK(required == strlen("mock \xC2\xB5 exception") + 1);
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
    CHECK(expect_instrumented_open_failure("null-manager",
          AMS_MEL_FACTORY_FAILED,
          "manager_factory_called\nlibrary_unloaded\n") == EXIT_SUCCESS);
    CHECK(expect_instrumented_open_failure("null-control",
          AMS_MEL_FACTORY_FAILED,
          "manager_factory_called\ncontrol_factory_called\nmanager_destroyed\n"
          "library_unloaded\n") == EXIT_SUCCESS);
    CHECK(expect_instrumented_open_failure("throw-manager",
          AMS_MEL_PROVIDER_EXCEPTION,
          "manager_factory_called\nlibrary_unloaded\n") == EXIT_SUCCESS);
    (void)remove(AMS_MEL_TEST_LIFETIME_LOG);
    CHECK(test_invalid_exception_diagnostic() == EXIT_SUCCESS);
    CHECK(expect_instrumented_open_failure("throw-control",
          AMS_MEL_PROVIDER_EXCEPTION,
          "manager_factory_called\ncontrol_factory_called\nmanager_destroyed\n"
          "library_unloaded\n") == EXIT_SUCCESS);
    CHECK(expect_instrumented_open_failure("init-fail",
          AMS_MEL_INITIALIZATION_FAILED,
          "manager_factory_called\ncontrol_factory_called\ninit_called\n"
          "control_destroyed\nmanager_destroyed\nlibrary_unloaded\n") ==
          EXIT_SUCCESS);
    CHECK(expect_instrumented_open_failure("throw-init",
          AMS_MEL_PROVIDER_EXCEPTION,
          "manager_factory_called\ncontrol_factory_called\ninit_called\n"
          "control_destroyed\nmanager_destroyed\nlibrary_unloaded\n") ==
          EXIT_SUCCESS);
    CHECK(expect_instrumented_open_failure("bad-alloc-manager",
          AMS_MEL_INTERNAL_ERROR,
          "manager_factory_called\nlibrary_unloaded\n") == EXIT_SUCCESS);
    CHECK(expect_instrumented_open_failure("bad-alloc-control",
          AMS_MEL_INTERNAL_ERROR,
          "manager_factory_called\ncontrol_factory_called\nmanager_destroyed\n"
          "library_unloaded\n") == EXIT_SUCCESS);
    CHECK(expect_instrumented_open_failure("bad-alloc-init",
          AMS_MEL_INTERNAL_ERROR,
          "manager_factory_called\ncontrol_factory_called\ninit_called\n"
          "control_destroyed\nmanager_destroyed\nlibrary_unloaded\n") ==
          EXIT_SUCCESS);
    CHECK(expect_open_failure_without_diagnostic("bad-alloc-manager",
          AMS_MEL_INTERNAL_ERROR) == EXIT_SUCCESS);
    CHECK(test_version_exception() == EXIT_SUCCESS);
    CHECK(test_utf8_diagnostic_truncation() == EXIT_SUCCESS);
    CHECK(test_version_failure("bad-alloc-version", AMS_MEL_INTERNAL_ERROR) ==
          EXIT_SUCCESS);
    CHECK(test_version_failure("invalid-utf8-version",
          AMS_MEL_PROVIDER_EXCEPTION) == EXIT_SUCCESS);
    CHECK(test_version_failure("nul-version", AMS_MEL_PROVIDER_EXCEPTION) ==
          EXIT_SUCCESS);
    (void)remove(AMS_MEL_TEST_LIFETIME_LOG);
    CHECK(test_success() == EXIT_SUCCESS);
    CHECK(expect_lifetime_log(
          "manager_factory_called\ncontrol_factory_called\ninit_called\n"
          "control_destroyed\nmanager_destroyed\nlibrary_unloaded\n") ==
          EXIT_SUCCESS);
    for (int cycle = 1; cycle < 20; ++cycle) {
        CHECK(test_success() == EXIT_SUCCESS);
    }
    puts("PASS: C provider load/init/version/close contract");
    return EXIT_SUCCESS;
}
