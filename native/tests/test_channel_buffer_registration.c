#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)
#define ARGS NULL, 0, NULL
#define CALL(fn, ...) CHECK(fn(__VA_ARGS__, ARGS) == AMS_MEL_OK)
typedef ams_mel_status_t (*operation)(const ams_mel_ir_channel *, ams_mel_ir_buffer *,
    ams_mel_ir_return_t *, char *, size_t, size_t *);
static operation const signatures[] = {
    ams_mel_ir_channel_register_buffer, ams_mel_ir_channel_unregister_buffer};
static ams_mel_ir_c2 *c2;
static ams_mel_ir_health *health;
static ams_mel_ir_stacked *stacked;
static ams_mel_ir_stream *image;
static ams_mel_ir_channel *view, *second, *wrong;
static ams_mel_ir_buffer *buffer;
static ams_mel_session *session;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
static int entered, released, removing;
static unsigned detach_before_operation;
static char log_path[] = "/tmp/ams-registration020-XXXXXX";

static unsigned count(const char *event)
{
    FILE *f = fopen(log_path, "r"); char line[256]; unsigned n = 0;
    CHECK(f);
    while (fgets(line, sizeof line, f)) if (!strcmp(line, event)) ++n;
    CHECK(!fclose(f)); return n;
}
static unsigned position(const char *event)
{
    FILE *f = fopen(log_path, "r"); char line[256]; unsigned n = 0;
    CHECK(f);
    while (fgets(line, sizeof line, f)) {
        ++n;
        if (!strcmp(line, event)) { CHECK(!fclose(f)); return n; }
    }
    CHECK(!fclose(f)); return 0;
}
static unsigned last_position(const char *event)
{
    FILE *f = fopen(log_path, "r"); char line[256]; unsigned n = 0, last = 0;
    CHECK(f);
    while (fgets(line, sizeof line, f)) { ++n; if (!strcmp(line, event)) last = n; }
    CHECK(!fclose(f)); return last;
}
static ams_mel_status_t enable(void)
{
    if (c2) return ams_mel_ir_c2_enable(c2, ARGS);
    if (health) return ams_mel_ir_health_enable(health, ARGS);
    if (stacked) return ams_mel_ir_stacked_enable(stacked, ARGS);
    return ams_mel_ir_stream_start(image, ARGS);
}
static ams_mel_status_t close_typed(void)
{
    if (c2) return ams_mel_ir_c2_close(&c2, ARGS);
    if (health) return ams_mel_ir_health_close(&health, ARGS);
    if (stacked) return ams_mel_ir_stacked_close(&stacked, ARGS);
    return ams_mel_ir_stream_close(&image, ARGS);
}
static void inaccessible(void)
{
    uint8_t bytes[16] = {0}; uint32_t flags = 0, result;
    size_t n;
    ams_mel_ir_buffer_info_v1 info;
    CHECK(ams_mel_ir_buffer_read_storage(buffer, 0, bytes, sizeof bytes, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_buffer_write_storage(buffer, 0, bytes, sizeof bytes, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_buffer_set_flags(buffer, &flags, 1, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_buffer_add_flag(buffer, flags, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_buffer_get_flags(buffer, &flags, 1, &n, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(result == UINT32_MAX);
    CHECK(ams_mel_ir_buffer_get_info(buffer, &info, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_buffer_close(&buffer, ARGS) == AMS_MEL_PROVIDER_FAILED && buffer);
    CHECK(close_typed() == AMS_MEL_PROVIDER_FAILED);
    CHECK(c2 || health || stacked || image);
}
static void reenter(void)
{
    uint32_t result = 0;
    inaccessible();
    CHECK(enable() == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_channel_disable(second, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(result == UINT32_MAX);
    CHECK(ams_mel_ir_channel_register_buffer(second, buffer, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(result == UINT32_MAX);
    CHECK(ams_mel_ir_channel_unregister_buffer(second, buffer, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(result == UINT32_MAX);
    if (image) CHECK(ams_mel_ir_stream_stop(image, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(count("channel_detached\n") == detach_before_operation);

}
static void barrier(void)
{
    CHECK(!pthread_mutex_lock(&mutex)); entered = 1;
    CHECK(!pthread_cond_broadcast(&ready));
    while (!released) CHECK(!pthread_cond_wait(&ready, &mutex));
    CHECK(!pthread_mutex_unlock(&mutex));
}
static void *operation_thread(void *unused)
{
    uint32_t result;
    (void)unused;
    CHECK(signatures[removing](second, buffer, &result, ARGS) == AMS_MEL_OK);
    CHECK(result == AMS_MEL_IR_RETURN_SUCCESS);
    return NULL;
}
static void concurrent(void (*install)(void (*)(void)), int remove)
{
    pthread_t thread;
    entered = released = 0; removing = remove;
    install(barrier);
    CHECK(!pthread_create(&thread, NULL, operation_thread, NULL));
    CHECK(!pthread_mutex_lock(&mutex));
    while (!entered) CHECK(!pthread_cond_wait(&ready, &mutex));
    CHECK(!pthread_mutex_unlock(&mutex));
    reenter();
    CHECK(!pthread_mutex_lock(&mutex)); released = 1;
    CHECK(!pthread_cond_broadcast(&ready)); CHECK(!pthread_mutex_unlock(&mutex));
    CHECK(!pthread_join(thread, NULL));
}
int main(int argc, char **argv)
{
    ams_mel_ir_c2_config_v1 cc = {0};
    ams_mel_ir_health_config_v1 hc = {0};
    ams_mel_ir_stacked_config_v1 sc = {0};
    ams_mel_ir_image_control_config_v1 ic = {0};
    ams_mel_ir_c2 *other = NULL;
    ams_mel_session *foreign = NULL;
    ams_mel_ir_buffer *foreign_buffer = NULL;
    uint32_t result, flags[] = {AMS_MEL_IR_BUFFER_HOST};
    uint8_t pattern[16], bytes[16];
    char scenario[100]; const char *which;
    int fd;
    void *pin, *symbol;
    void (*install)(void (*)(void));
    void (*install_enable)(void (*)(void));
    void (*drop)(void);
    CHECK(argc == 3 && signatures[0] && signatures[1]); which = argv[2];
    CHECK(snprintf(scenario, sizeof scenario, "registration020-%s", which) > 0);
    fd = mkstemp(log_path); CHECK(fd >= 0); CHECK(!close(fd));
    CHECK(!setenv("AMS_MEL_TEST_LIFETIME_LOG", log_path, 1));
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_LOCAL); CHECK(pin);
    symbol = dlsym(pin, "mock_registration_reentry"); CHECK(symbol); memcpy(&install, &symbol, sizeof install);
    symbol = dlsym(pin, "mock_registration_drop_retained"); CHECK(symbol); memcpy(&drop, &symbol, sizeof drop);
    symbol = dlsym(pin, "mock_enable_reentry"); CHECK(symbol); memcpy(&install_enable, &symbol, sizeof install_enable);
    CALL(ams_mel_session_open, AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &session);
    cc.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
    hc.channel_type = AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS;
    sc.channel_type = AMS_MEL_IR_CHANNEL_STACKED_IMAGE;
    ic.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    if (!strcmp(argv[1], "c2")) {
        CALL(ams_mel_ir_c2_open, session, &cc, &c2);
        CALL(ams_mel_ir_channel_from_c2, c2, &view);
        CALL(ams_mel_ir_channel_from_c2, c2, &second);
    } else if (!strcmp(argv[1], "health")) {
        CALL(ams_mel_ir_health_open, session, &hc, &health);
        CALL(ams_mel_ir_channel_from_health, health, &view);
        CALL(ams_mel_ir_channel_from_health, health, &second);
    } else if (!strcmp(argv[1], "stacked")) {
        CALL(ams_mel_ir_stacked_open, session, &sc, &stacked);
        CALL(ams_mel_ir_channel_from_stacked, stacked, &view);
        CALL(ams_mel_ir_channel_from_stacked, stacked, &second);
    } else {
        CALL(ams_mel_ir_image_control_open, session, &ic, &image);
        CALL(ams_mel_ir_channel_from_stream, image, &view);
        CALL(ams_mel_ir_channel_from_stream, image, &second);
    }
    CALL(ams_mel_ir_c2_open, session, &cc, &other);
    CALL(ams_mel_ir_channel_from_c2, other, &wrong);
    CALL(ams_mel_ir_buffer_open, session, 16, -20, &buffer, &result);
    CHECK(result == AMS_MEL_IR_RETURN_SUCCESS);
    memset(pattern, 0xa5, sizeof pattern);
    CALL(ams_mel_ir_buffer_write_storage, buffer, 0, pattern, sizeof pattern);
    CALL(ams_mel_ir_buffer_set_flags, buffer, flags, 1);
    /* Identical instance/path/manager class is not native Session identity. */
    CALL(ams_mel_session_open, AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &foreign);
    CALL(ams_mel_ir_buffer_open, foreign, 16, -20, &foreign_buffer, &result);
    CHECK(ams_mel_ir_channel_register_buffer(view, foreign_buffer, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(result == UINT32_MAX && count("application_register_entered\n") == 0);
    CALL(ams_mel_ir_buffer_close, &foreign_buffer);
    CALL(ams_mel_session_close, &foreign);
    for (unsigned op = 0; op != 2; ++op) {
        CHECK(signatures[op](NULL, buffer, &result, ARGS) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(result == UINT32_MAX);
        CHECK(signatures[op](view, NULL, &result, ARGS) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(signatures[op](view, buffer, NULL, ARGS) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(signatures[op](view, buffer, &result, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    }
    CHECK(ams_mel_ir_channel_unregister_buffer(view, buffer, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
    if (!strcmp(which, "concurrent")) concurrent(install, 0);
    else {
        ams_mel_status_t status;
        if (!strcmp(which, "reentry")) install(reenter);
        status = ams_mel_ir_channel_register_buffer(view, buffer, &result, ARGS);
        if (!strncmp(which, "register-", 9)) {
            CHECK(status == (!strcmp(which, "register-throw") ? AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_PROVIDER_FAILED));
            if (!strcmp(which, "register-throw")) CHECK(result == UINT32_MAX);
            else if (!strcmp(which, "register-bad-pointer")) CHECK(result == AMS_MEL_IR_RETURN_BAD_POINTER);
            else if (!strcmp(which, "register-unsupported")) CHECK(result == AMS_MEL_IR_RETURN_NOT_SUPPORTED);
            else if (!strcmp(which, "register-unimplemented")) CHECK(result == AMS_MEL_IR_RETURN_NOT_IMPLEMENTED);
            else CHECK(result == AMS_MEL_IR_RETURN_FAIL);
            CHECK(enable() == AMS_MEL_PROVIDER_FAILED);
        } else CHECK(status == AMS_MEL_OK && result == AMS_MEL_IR_RETURN_SUCCESS);
    }
    inaccessible();
    CHECK(ams_mel_ir_channel_register_buffer(view, buffer, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_channel_register_buffer(second, buffer, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(count("application_register_entered\n") == 1);
    CHECK(ams_mel_ir_channel_unregister_buffer(wrong, buffer, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(result == UINT32_MAX);
    CHECK(count("application_unregister_entered\n") == 0);
    CALL(ams_mel_ir_c2_close, &other);
    detach_before_operation = count("channel_detached\n");
    CALL(ams_mel_ir_channel_close, &wrong);
    if (!strcmp(which, "callback-fail")) {
        CHECK(count("application_checkout_released\n") == 1);
        CHECK(enable() == AMS_MEL_PROVIDER_FAILED);
        CHECK(ams_mel_ir_channel_unregister_buffer(view, buffer, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
        CHECK(result == UINT32_MAX);
        inaccessible();
        CALL(ams_mel_session_close, &session);
        CALL(ams_mel_ir_channel_close, &view);
        CALL(ams_mel_ir_channel_close, &second);
        CHECK(!dlclose(pin));
        CHECK(count("library_unloaded\n") == 0);
        CHECK(count("foundation_buffer_destroyed_with_storage\n") == 1); /* foreign only */
        CHECK(!unlink(log_path));
        return 0;
    }
    if (strncmp(which, "register-", 9)) {
        for (unsigned cycle = 0; cycle != 3; ++cycle) {
            if (!strcmp(which, "reentry")) install_enable(reenter);
            CHECK(enable() == AMS_MEL_OK);
            CHECK(ams_mel_ir_channel_register_buffer(view, buffer, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
            CHECK(ams_mel_ir_channel_unregister_buffer(view, buffer, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
            CHECK(result == UINT32_MAX);
            CALL(ams_mel_ir_channel_disable, view, &result);
            CHECK(result == AMS_MEL_IR_RETURN_SUCCESS);
        }
    }
    CALL(ams_mel_session_close, &session);
    if (!strcmp(which, "concurrent")) concurrent(install, 1);
    else {
        ams_mel_status_t status;
        if (!strcmp(which, "reentry")) install(reenter);
        status = ams_mel_ir_channel_unregister_buffer(second, buffer, &result, ARGS);
        if (!strncmp(which, "unregister-", 11) && strcmp(which, "unregister-callback")) {
            CHECK(status == (!strcmp(which, "unregister-throw") ? AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_PROVIDER_FAILED));
            inaccessible();
            CALL(ams_mel_ir_channel_unregister_buffer, view, buffer, &result);
        } else CHECK(status == AMS_MEL_OK);
    }
    if (!strcmp(which, "retain-unregistered")) { inaccessible(); drop(); }
    CALL(ams_mel_ir_buffer_read_storage, buffer, 0, bytes, sizeof bytes);
    CHECK(!memcmp(bytes, pattern, sizeof pattern));
    CHECK(ams_mel_ir_channel_unregister_buffer(view, buffer, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(result == UINT32_MAX);
    CHECK(close_typed() == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_register_buffer(second, buffer, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(result == UINT32_MAX);
    CALL(ams_mel_ir_buffer_close, &buffer);
    CHECK(ams_mel_ir_channel_register_buffer(second, buffer, &result, ARGS) == AMS_MEL_INVALID_ARGUMENT);
    CALL(ams_mel_ir_channel_close, &view);
    CALL(ams_mel_ir_channel_close, &second);
    CHECK(count("application_identity_verified\n") >= 1);
    CHECK(count("foundation_release_forbidden\n") == 0);
    CHECK(count("buffer_registered\n") == 0 && count("buffer_released\n") == 0);
    CHECK(count("frame_produced\n") == 0);
    CHECK(position("foundation_buffer_destroyed_with_storage\n") != 0);
    CHECK(!dlclose(pin));
    CHECK(count("foundation_buffer_destroyed_with_storage\n") == 2);
    CHECK(last_position("control_destroyed\n") > last_position("foundation_buffer_destroyed_with_storage\n"));
    CHECK(last_position("manager_destroyed\n") > last_position("control_destroyed\n"));
    CHECK(position("library_unloaded\n") > last_position("manager_destroyed\n"));
    CHECK(!unlink(log_path));
    return 0;
}
