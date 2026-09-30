#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
static char path[] = "/tmp/ams-rf-c2-XXXXXX";
static char log_text[8192];
static const char *log_contents(void)
{
    FILE *file = fopen(path, "r");
    size_t size;
    CHECK(file != NULL);
    size = fread(log_text, 1, sizeof log_text - 1, file);
    log_text[size] = 0;
    CHECK(fclose(file) == 0);
    return log_text;
}
static void reset_log(void)
{
    FILE *file = fopen(path, "w"); CHECK(file != NULL); CHECK(fclose(file) == 0);
}
static void open_c2(ams_mel_rf_c2 **out, const char *scenario)
{
    char diag[256] = {0}; size_t required = 0;
    CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER, scenario, out,
          diag, sizeof diag, &required) == AMS_MEL_OK);
    CHECK(*out != NULL);
}
int main(void)
{
    ams_mel_rf_c2 *c2 = NULL;
    char diag[256] = {0}; size_t required = 0;
    const int fd = mkstemp(path); CHECK(fd >= 0); CHECK(close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(ams_mel_rf_c2_open(NULL, "c2:ok", &c2, diag, sizeof diag, &required)
          == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER, NULL, &c2,
          diag, sizeof diag, &required) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER, "c2:ok", NULL,
          diag, sizeof diag, &required) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER, "c2:ok", &c2,
          NULL, 1, &required) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(log_contents()[0] == 0);
    CHECK(ams_mel_rf_c2_open("/no/such/c2.so", "c2:ok", &c2, diag,
          sizeof diag, &required) == AMS_MEL_LIBRARY_LOAD_FAILED);
    CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MISSING_SYMBOL_RF_PROVIDER, "c2:ok",
          &c2, diag, sizeof diag, &required) == AMS_MEL_SYMBOL_NOT_FOUND);
    reset_log();
    open_c2(&c2, "c2:ok");
    CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER, "c2:ok",
          &c2, diag, sizeof diag, &required) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(strstr(log_contents(), "rf_c2_factory_called\n") != NULL);
    CHECK(strstr(log_contents(), "library_unloaded\n") == NULL);
    {
        void *lib = dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER, RTLD_NOW | RTLD_LOCAL);
        unsigned (*factories)(void);
        unsigned (*shutdowns)(void);
        CHECK(lib != NULL);
        *(void **)(&factories) = dlsym(lib, "mock_rf_c2_factory_calls");
        *(void **)(&shutdowns) = dlsym(lib, "mock_rf_c2_shutdown_calls");
        CHECK(factories && shutdowns && factories() == 1 && shutdowns() == 0);
        CHECK(dlclose(lib) == 0);
    }
    CHECK(ams_mel_rf_c2_close(&c2, diag, sizeof diag, &required) == AMS_MEL_OK);
    CHECK(c2 == NULL);
    {
        const char *log = log_contents();
        const char *factory = strstr(log, "rf_c2_factory_called\n");
        const char *shutdown = strstr(log, "rf_c2_shutdown\n");
        const char *destroy = strstr(log, "rf_c2_destroyed\n");
        const char *unload = strstr(log, "library_unloaded\n");
        CHECK(factory && shutdown && destroy && unload);
        CHECK(factory < shutdown && shutdown < destroy && destroy < unload);
        CHECK(strstr(shutdown + 1, "rf_c2_shutdown\n") == NULL);
        CHECK(strstr(log, "rf_forbidden_call\n") == NULL);
    }
    CHECK(ams_mel_rf_c2_close(&c2, diag, sizeof diag, &required) == AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(NULL, diag, sizeof diag, &required) == AMS_MEL_INVALID_ARGUMENT);
    {
        const char *scenarios[] = {"c2:factory-null", "c2:factory-throw",
                                   "c2:factory-throw-unknown"};
        for (size_t i = 0; i < 3; ++i) {
            reset_log();
            ams_mel_status_t status = ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER,
                scenarios[i], &c2, diag, sizeof diag, &required);
            CHECK(status == (i == 0 ? AMS_MEL_FACTORY_FAILED : AMS_MEL_PROVIDER_EXCEPTION));
            CHECK(c2 == NULL);
            CHECK(strstr(log_contents(), "rf_c2_factory_called\n") != NULL);
            CHECK(strstr(log_contents(), "library_unloaded\n") != NULL);
            CHECK(strstr(diag, i == 0 ? "createC2MEL returned null" :
                i == 1 ? "mock RF factory exception" : "unknown provider factory exception") != NULL);
        }
    }
    reset_log(); open_c2(&c2, "c2:shutdown-throw");
    CHECK(ams_mel_rf_c2_close(&c2, diag, sizeof diag, &required) == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(c2 == NULL && strstr(diag, "mock C2 shutdown exception") != NULL);
    CHECK(strstr(log_contents(), "rf_c2_shutdown\n") != NULL);
    CHECK(strstr(log_contents(), "rf_c2_destroyed\n") == NULL);
    CHECK(strstr(log_contents(), "library_unloaded\n") == NULL);
    CHECK(ams_mel_rf_c2_close(&c2, diag, sizeof diag, &required) == AMS_MEL_OK);
    CHECK(strstr(log_contents(), "rf_c2_shutdown\n") != NULL);
    CHECK(strstr(strstr(log_contents(), "rf_c2_shutdown\n") + 1,
                 "rf_c2_shutdown\n") == NULL);
    CHECK(strstr(log_contents(), "rf_c2_destroyed\n") == NULL);
    CHECK(unlink(path) == 0);
    puts("PASS: RF C2 C11 lifecycle");
    return 0;
}