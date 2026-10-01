#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
static char path[] = "/tmp/ams-rf-admin-XXXXXX";
static char *text(void)
{
    FILE *file = fopen(path, "r");
    static char buf[8192];
    size_t length;
    CHECK(file != NULL);
    length = fread(buf, 1, sizeof buf - 1, file);
    buf[length] = 0;
    CHECK(fclose(file) == 0);
    return buf;
}
static void reset_log(void)
{
    FILE *file = fopen(path, "w"); CHECK(file != NULL); CHECK(fclose(file) == 0);
}
static void open_admin(ams_mel_rf_admin **out, const char *scenario)
{
    char diag[256] = {0}; size_t required = 0;
    CHECK(ams_mel_rf_admin_open(AMS_MEL_TEST_MOCK_RF_PROVIDER, scenario, out,
                               diag, sizeof diag, &required) == AMS_MEL_OK);
    CHECK(*out != NULL);
}
static void run(void)
{
    ams_mel_rf_admin *admin = NULL;
    char diag[256] = {0}; size_t required = 0; uint32_t accepted = 77;
    const int fd = mkstemp(path); CHECK(fd >= 0); CHECK(close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    open_admin(&admin, "admin:ok");
    CHECK(ams_mel_rf_admin_command_state(admin, AMS_MEL_IR_MFA_STATE_STANDBY,
          &accepted, diag, sizeof diag, &required) == AMS_MEL_OK && accepted == 1);
    CHECK(ams_mel_rf_admin_command_state(admin, AMS_MEL_IR_MFA_STATE_OPERATE_RX_ONLY,
          &accepted, diag, sizeof diag, &required) == AMS_MEL_OK && accepted == 1);
    CHECK(ams_mel_rf_admin_command_state(admin, AMS_MEL_IR_MFA_STATE_DEGRADED,
          &accepted, diag, sizeof diag, &required) == AMS_MEL_OK && accepted == 1);
    CHECK(ams_mel_rf_admin_command_state(admin, UINT32_MAX, &accepted, diag,
          sizeof diag, &required) == AMS_MEL_INVALID_ARGUMENT && accepted == 1);
    CHECK(ams_mel_rf_admin_command_state(admin, AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE,
          &accepted, diag, sizeof diag, &required) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_admin_close(&admin, diag, sizeof diag, &required) == AMS_MEL_OK);
    CHECK(admin == NULL);
    CHECK(ams_mel_rf_admin_close(&admin, diag, sizeof diag, &required) == AMS_MEL_OK);
    CHECK(ams_mel_rf_admin_open(AMS_MEL_TEST_MOCK_RF_PROVIDER, "admin:ok",
          NULL, diag, sizeof diag, &required) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_admin_open("/no/such/rf/admin.so", "admin:ok", &admin,
          diag, sizeof diag, &required) == AMS_MEL_LIBRARY_LOAD_FAILED);
    CHECK(admin == NULL);
    CHECK(ams_mel_rf_admin_open(AMS_MEL_TEST_MISSING_SYMBOL_RF_PROVIDER,
          "admin:ok", &admin, diag, sizeof diag, &required) == AMS_MEL_SYMBOL_NOT_FOUND);
    CHECK(admin == NULL);
    {
        char *log = text();
        char *shut = strstr(log, "rf_admin_shutdown\n");
        char *destroy = strstr(log, "rf_admin_destroyed\n");
        char *unload = strstr(log, "library_unloaded\n");
        CHECK(shut && destroy && unload && shut < destroy && destroy < unload);
        CHECK(strstr(log, "rf_admin_state_6\n") && strstr(log, "rf_admin_state_8\n") &&
              strstr(log, "rf_admin_state_14\n"));
        CHECK(strstr(shut + 1, "rf_admin_shutdown\n") == NULL);
    }
    for (size_t i = 0; i < 7; ++i) {
        const char *scenarios[] = {"admin:reject", "admin:command-throw", "admin:no-uci",
                                   "admin:no-status", "admin:factory-null", "admin:factory-throw",
                                   "admin:factory-throw-unknown"};
        reset_log();
        if (i >= 4) {
            ams_mel_status_t result = ams_mel_rf_admin_open(AMS_MEL_TEST_MOCK_RF_PROVIDER,
                scenarios[i], &admin, diag, sizeof diag, &required);
            CHECK(admin == NULL);
            CHECK(result == (i == 4 ? AMS_MEL_FACTORY_FAILED : AMS_MEL_PROVIDER_EXCEPTION));
            CHECK(strstr(text(), "library_unloaded\n") != NULL);
        } else {
            open_admin(&admin, scenarios[i]); accepted = 77;
            ams_mel_status_t result = ams_mel_rf_admin_command_state(admin,
                AMS_MEL_IR_MFA_STATE_OPERATE, &accepted, diag, sizeof diag, &required);
            CHECK(result == (i == 0 ? AMS_MEL_OK : i == 1 ? AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_PROVIDER_FAILED));
            CHECK(accepted == (i == 0 ? 0U : 77U));
            CHECK(ams_mel_rf_admin_close(&admin, diag, sizeof diag, &required) == AMS_MEL_OK);
        }
    }
    reset_log(); open_admin(&admin, "admin:shutdown-throw");
    CHECK(ams_mel_rf_admin_close(&admin, diag, sizeof diag, &required) == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(admin == NULL && strstr(diag, "mock Admin shutdown exception") != NULL);
    CHECK(strstr(text(), "rf_admin_shutdown\n") != NULL);
    CHECK(strstr(text(), "rf_admin_destroyed\n") == NULL);
    CHECK(strstr(text(), "library_unloaded\n") == NULL);
    CHECK(ams_mel_rf_admin_close(&admin, diag, sizeof diag, &required) == AMS_MEL_OK);
    CHECK(unlink(path) == 0);
}
int main(void) { run(); puts("PASS: RF Admin C11 contract"); return 0; }
