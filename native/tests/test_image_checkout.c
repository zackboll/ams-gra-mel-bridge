#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)
#define ARGS NULL, 0, NULL
#define CALL(f, ...) CHECK(f(__VA_ARGS__, ARGS) == AMS_MEL_OK)
static ams_mel_session *session;
static ams_mel_ir_stream *image;
static ams_mel_ir_channel *view;
static ams_mel_ir_buffer *buffer;
static ams_mel_ir_image_checkout *receiver;
static void (*deliver)(unsigned, int);
static void (*release_hook)(void (*)(void));
static uint32_t result;
static char log_path[] = "/tmp/checkout021-log-XXXXXX";
static unsigned events(const char *event)
{
    FILE *file = fopen(log_path, "r"); char line[256]; unsigned count = 0;
    CHECK(file);
    while (fgets(line, sizeof line, file)) if (!strcmp(line, event)) ++count;
    CHECK(!fclose(file)); return count;
}
static unsigned position(const char *event)
{
    FILE *file = fopen(log_path, "r"); char line[256]; unsigned count = 0;
    CHECK(file);
    while (fgets(line, sizeof line, file)) {
        ++count;
        if (!strcmp(line, event)) { CHECK(!fclose(file)); return count; }
    }
    CHECK(!fclose(file)); return 0;
}
static void blocked(void)
{
    CHECK(ams_mel_ir_channel_unregister_buffer(view, buffer, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(result == UINT32_MAX);
    CHECK(ams_mel_ir_buffer_close(&buffer, ARGS) == AMS_MEL_PROVIDER_FAILED && buffer);
    CHECK(ams_mel_ir_stream_close(&image, ARGS) == AMS_MEL_PROVIDER_FAILED && image);
    CHECK(ams_mel_ir_stream_stop(image, ARGS) == AMS_MEL_PROVIDER_FAILED);
}
static void not_supported(void *pin)
{
    ams_mel_ir_image_checkout_lease *lease = NULL;
    ams_mel_ir_image_checkout_counters_v1 counters;
    CALL(ams_mel_ir_image_checkout_open, image, 1, 1, &receiver);
    CALL(ams_mel_ir_buffer_open, session, 16, -20, &buffer, &result);
    CALL(ams_mel_ir_channel_register_buffer, view, buffer, &result);
    CALL(ams_mel_ir_stream_start, image);
    deliver(1, 0);
    CHECK(!dlclose(pin)); /* No test pin can keep the provider mapped below. */
    CALL(ams_mel_ir_image_checkout_receive, receiver, 0, &lease);
    CHECK(lease && !events("application_checkout_released\n"));
    CALL(ams_mel_ir_channel_disable, view, &result);
    blocked();

    CHECK(ams_mel_ir_image_checkout_lease_close(&lease, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(result == AMS_MEL_IR_RETURN_NOT_SUPPORTED && !lease);
    CHECK(events("checkout_released_1\n") == 1);
    CALL(ams_mel_ir_image_checkout_lease_close, &lease, &result);
    CHECK(result == UINT32_MAX && !lease);
    CALL(ams_mel_ir_image_checkout_get_counters, receiver, &counters);
    CHECK(counters.callbacks_received == 1 && counters.checkouts_accepted == 1);
    CHECK(counters.releases_succeeded == 0 && counters.releases_uncertain == 1);

    CALL(ams_mel_ir_image_checkout_close, &receiver);
    CALL(ams_mel_session_close, &session);
    blocked(); /* Neither receiver nor session Close retires the obligation. */
    CALL(ams_mel_ir_channel_close, &view);
    CHECK(events("application_checkout_released\n") == 1);
    CHECK(!events("application_unregister_entered\n"));
    CHECK(!events("application_checkout_destroyed\n"));
    CHECK(!events("foundation_buffer_destroyed_with_storage\n"));
    CHECK(!events("library_unloaded\n"));
}
static void reenter(void)
{
    blocked();
    deliver(1, 0);
}
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
static int entered, resume;
static void barrier(void)
{
    CHECK(!pthread_mutex_lock(&mutex)); entered = 1; CHECK(!pthread_cond_broadcast(&ready));
    while (!resume) CHECK(!pthread_cond_wait(&ready, &mutex));
    CHECK(!pthread_mutex_unlock(&mutex));
}
static void *close_lease(void *raw)
{
    ams_mel_ir_image_checkout_lease **lease = raw; uint32_t value;
    CALL(ams_mel_ir_image_checkout_lease_close, lease, &value);
    CHECK(value == AMS_MEL_IR_RETURN_SUCCESS); return NULL;
}
static void *wait_receive(void *unused)
{
    ams_mel_ir_image_checkout_lease *lease = NULL; (void)unused;
    CHECK(ams_mel_ir_image_checkout_receive(receiver, 10000, &lease, ARGS) == AMS_MEL_STREAM_STOPPED);
    CHECK(!lease); return NULL;
}
int main(int argc, char **argv)
{
    char scenario[128]; void *pin, *symbol;
    ams_mel_ir_image_control_config_v1 config = {0};
    ams_mel_ir_image_checkout_lease *first = NULL, *second = NULL;
    ams_mel_ir_image_checkout *duplicate = NULL;
    ams_mel_ir_image_checkout_counters_v1 counters;
    const char *which; int fd;
    CHECK(argc == 2); which = argv[1];
    CHECK(snprintf(scenario, sizeof scenario, "checkout021-%s", which) > 0);
    fd = mkstemp(log_path); CHECK(fd >= 0 && !close(fd));
    CHECK(!setenv("AMS_MEL_TEST_LIFETIME_LOG", log_path, 1));
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_LOCAL); CHECK(pin);
    symbol = dlsym(pin, "mock_checkout_deliver"); CHECK(symbol); memcpy(&deliver, &symbol, sizeof deliver);
    symbol = dlsym(pin, "mock_checkout_release_hook"); CHECK(symbol); memcpy(&release_hook, &symbol, sizeof release_hook);
    CALL(ams_mel_session_open, AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &session);
    config.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    if (!strcmp(which, "full-acquisition")) {
        ams_mel_ir_stream_config_v1 full = {0};
        full.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
        full.buffer_count = 1; full.buffer_size = 16; full.queue_capacity = 1;
        /* A normal mock advertises Mono8; this tests the mode, not capability. */
        CALL(ams_mel_session_close, &session);
        CALL(ams_mel_session_open, AMS_MEL_TEST_MOCK_PROVIDER, "valid", "", &session);
        CALL(ams_mel_ir_stream_open, session, &full, &image);
        CHECK(ams_mel_ir_image_checkout_open(image, 1, 1, &receiver, ARGS) == AMS_MEL_PROVIDER_FAILED && !receiver);
        CALL(ams_mel_ir_stream_close, &image); CALL(ams_mel_session_close, &session);
        CHECK(!dlclose(pin)); CHECK(!unlink(log_path)); return 0;
    }
    CALL(ams_mel_ir_image_control_open, session, &config, &image);
    CALL(ams_mel_ir_channel_from_stream, image, &view);
    if (!strcmp(which, "not-supported")) {
        not_supported(pin);
        CHECK(!unlink(log_path)); return 0;
    }
    CHECK(ams_mel_ir_image_checkout_open(image, 0, 2, &receiver, ARGS) == AMS_MEL_INVALID_ARGUMENT && !receiver);
    if (!strcmp(which, "no-receiver") || !strcmp(which, "no-registration")) {
        if (!strcmp(which, "no-registration")) CALL(ams_mel_ir_image_checkout_open, image, 2, 3, &receiver);
        CALL(ams_mel_ir_stream_start, image); deliver(1, 0);
        CHECK(events("application_checkout_released\n") == 1 && events("application_checkout_destroyed\n") == 1);
        if (receiver) {
            CHECK(ams_mel_ir_image_checkout_receive(receiver, 0, &first, ARGS) == AMS_MEL_TIMEOUT);
            CALL(ams_mel_ir_image_checkout_get_counters, receiver, &counters);
            CHECK(counters.callbacks_rejected == 1 && counters.checkouts_accepted == 0);
            CALL(ams_mel_ir_stream_stop, image);
            CHECK(ams_mel_ir_image_checkout_receive(receiver, 0, &first, ARGS) == AMS_MEL_STREAM_STOPPED);
        }
        CALL(ams_mel_ir_image_checkout_close, &receiver);
        CALL(ams_mel_ir_stream_close, &image); CALL(ams_mel_ir_channel_close, &view);
        CALL(ams_mel_session_close, &session); CHECK(!dlclose(pin)); CHECK(!unlink(log_path)); return 0;
    }
    CALL(ams_mel_ir_image_checkout_open, image,
        !strcmp(which, "reentry-bound") ? 1 : 2,
        !strcmp(which, "reentry-bound") ? 1 : 3, &receiver);
    CHECK(ams_mel_ir_image_checkout_open(image, 2, 3, &receiver, ARGS) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_checkout_open(image, 2, 3, &duplicate, ARGS) == AMS_MEL_PROVIDER_FAILED && !duplicate);
    CHECK(ams_mel_ir_image_checkout_receive(receiver, 0, &first, ARGS) == AMS_MEL_TIMEOUT && !first);
    CHECK(ams_mel_ir_image_checkout_receive(receiver, 1, &first, ARGS) == AMS_MEL_TIMEOUT && !first);
    deliver(1, 1); /* Null never acquires or leaks a HOLD slot. */
    deliver(1, 0); /* Attached and unregistered: rejection/release. */
    CALL(ams_mel_ir_buffer_open, session, 16, -20, &buffer, &result);
    if (!strcmp(which, "registration-reentry")) {
        void (*install)(void (*)(void));
        symbol = dlsym(pin, "mock_registration_reentry"); CHECK(symbol); memcpy(&install, &symbol, sizeof install);
        install(reenter);
    }
    CALL(ams_mel_ir_channel_register_buffer, view, buffer, &result);
    CALL(ams_mel_ir_stream_start, image);
    /* enable's synchronous callback is rejected until Running is published. */
    CHECK(ams_mel_ir_image_checkout_receive(receiver, 0, &first, ARGS) == AMS_MEL_TIMEOUT);
    if (!strcmp(which, "admission-failure")) {
        CHECK(!setenv("AMS_MEL_TEST_ENQUEUE_ALLOCATION", "fail", 1)); deliver(1, 0);
        CHECK(!unsetenv("AMS_MEL_TEST_ENQUEUE_ALLOCATION"));
        CHECK(ams_mel_ir_image_checkout_receive(receiver, 0, &first, ARGS) == AMS_MEL_TIMEOUT);
    }
    if (!strcmp(which, "wait-close")) {
        pthread_t thread; char marker[] = "/tmp/checkout021-wait-XXXXXX";
        fd = mkstemp(marker); CHECK(fd >= 0 && !close(fd) && !unlink(marker));
        CHECK(!setenv("AMS_MEL_TEST_CHECKOUT_WAIT", marker, 1));
        CHECK(!pthread_create(&thread, NULL, wait_receive, NULL));
        for (unsigned i = 0; access(marker, F_OK) && i < 10000; ++i) {
            struct timespec delay = {0, 1000000}; nanosleep(&delay, NULL);
        }
        CHECK(!access(marker, F_OK));
        CALL(ams_mel_ir_image_checkout_close, &receiver);
        CHECK(!pthread_join(thread, NULL)); CHECK(!unlink(marker));
    } else {
        deliver(1, 0);
        CALL(ams_mel_ir_image_checkout_receive, receiver, 0, &first);
        CHECK(first);
        CHECK(ams_mel_ir_image_checkout_receive(receiver, 0, &first, ARGS) == AMS_MEL_INVALID_ARGUMENT);
        blocked();
        CALL(ams_mel_session_close, &session);
        if (!strcmp(which, "drain-provider-fail")) {
            deliver(2, 0);
            CHECK(ams_mel_ir_image_checkout_close(&receiver, ARGS) == AMS_MEL_PROVIDER_FAILED && !receiver);
            blocked();
            CHECK(ams_mel_ir_image_checkout_lease_close(&first, &result, ARGS) == AMS_MEL_PROVIDER_FAILED && !first);
            CALL(ams_mel_ir_image_checkout_lease_close, &first, &result);
            CHECK(events("application_checkout_released\n") == 4);
            CHECK(events("application_checkout_destroyed\n") == 1);
            CHECK(!dlclose(pin)); CHECK(!events("library_unloaded\n"));
            CHECK(!unlink(log_path)); return 0;
        }
        if (!strcmp(which, "drain-fail")) {
            CHECK(!setenv("AMS_MEL_TEST_RELEASE_SLOTS", "exhaust", 1));
            /* Use the established fail-safe admission engine; no release can
             * be attempted without reserved uncertain ownership. */
            deliver(1, 0);
            CHECK(ams_mel_ir_image_checkout_close(&receiver, ARGS) == AMS_MEL_PROVIDER_FAILED);
            CHECK(!receiver); blocked();
            CHECK(ams_mel_ir_image_checkout_lease_close(&first, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
            CHECK(!unsetenv("AMS_MEL_TEST_RELEASE_SLOTS"));
            CHECK(!first); CHECK(!dlclose(pin)); CHECK(!events("library_unloaded\n"));
            CHECK(!unlink(log_path)); return 0;
        }
        if (!strcmp(which, "hold-exhaustion")) {
            CHECK(!setenv("AMS_MEL_TEST_RETENTION_SLOTS", "exhaust", 1));
            deliver(1, 0);
            CHECK(!unsetenv("AMS_MEL_TEST_RETENTION_SLOTS"));
            blocked(); CALL(ams_mel_ir_image_checkout_close, &receiver);
            /* No retry of the unattempted retained wrapper; original lease
             * still has its own preallocated release admission. */
            CALL(ams_mel_ir_image_checkout_lease_close, &first, &result);
            CHECK(!dlclose(pin)); CHECK(!events("library_unloaded\n"));
            CHECK(!unlink(log_path)); return 0;
        }
        if (!strcmp(which, "fail") || !strcmp(which, "throw")) {
            const unsigned released = events("application_checkout_released\n");
            CHECK(ams_mel_ir_image_checkout_lease_close(&first, &result, ARGS) ==
                (!strcmp(which, "throw") ? AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_PROVIDER_FAILED));
            CHECK(result == (!strcmp(which, "throw") ? UINT32_MAX : AMS_MEL_IR_RETURN_FAIL));
            CHECK(!first);
            CALL(ams_mel_ir_image_checkout_lease_close, &first, &result);
            CHECK(events("application_checkout_released\n") == released + 1);
            CHECK(events("application_checkout_destroyed\n") == 1); /* only initial rejection */
            CALL(ams_mel_ir_image_checkout_get_counters, receiver, &counters);
            CHECK(counters.releases_uncertain == 1);
            CHECK(ams_mel_ir_channel_disable(view, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
            blocked(); CALL(ams_mel_ir_image_checkout_close, &receiver);
            CALL(ams_mel_ir_channel_close, &view);
            CHECK(!dlclose(pin));
            CHECK(!events("library_unloaded\n") && !events("foundation_buffer_destroyed_with_storage\n"));
            CHECK(!unlink(log_path)); return 0;
        }
        if (!strcmp(which, "overflow")) {
            deliver(8, 0);
            CALL(ams_mel_ir_image_checkout_get_counters, receiver, &counters);
            CHECK(counters.checkouts_accepted == 3 && counters.queue_overflow == 6);
        }
        if (!strcmp(which, "reentry") || !strcmp(which, "reentry-bound")) release_hook(reenter);
        if (!strcmp(which, "concurrent-release")) {
            pthread_t thread; release_hook(barrier);
            CHECK(!pthread_create(&thread, NULL, close_lease, &first));
            CHECK(!pthread_mutex_lock(&mutex));
            while (!entered) CHECK(!pthread_cond_wait(&ready, &mutex));
            CHECK(!pthread_mutex_unlock(&mutex));
            CALL(ams_mel_ir_channel_disable, view, &result); blocked();
            deliver(1, 0); /* callback completion after Disable, deferred release */
            CHECK(!pthread_mutex_lock(&mutex)); resume = 1; CHECK(!pthread_cond_broadcast(&ready));
            CHECK(!pthread_mutex_unlock(&mutex)); CHECK(!pthread_join(thread, NULL));
        } else {
            CALL(ams_mel_ir_image_checkout_lease_close, &first, &result);
            CHECK(result == AMS_MEL_IR_RETURN_SUCCESS && !first);
        }
        CALL(ams_mel_ir_image_checkout_lease_close, &first, &result); CHECK(result == UINT32_MAX);
        if (!strcmp(which, "overflow")) {
            CALL(ams_mel_ir_image_checkout_receive, receiver, 0, &second);
            CALL(ams_mel_ir_image_checkout_lease_close, &second, &result);
            CHECK(events("checkout_released_3\n") == 1 && !events("checkout_released_4\n"));
            CALL(ams_mel_ir_image_checkout_receive, receiver, 0, &second);
            CALL(ams_mel_ir_image_checkout_lease_close, &second, &result);
            CHECK(events("checkout_released_4\n") == 1);
        }
        if (!strcmp(which, "reentry") || !strcmp(which, "reentry-bound")) {
            CALL(ams_mel_ir_image_checkout_receive, receiver, 0, &second);
            CHECK(second);
        } else {
            deliver(1, 0);
            if (strcmp(which, "concurrent-release")) CALL(ams_mel_ir_image_checkout_receive, receiver, 0, &second);
        }
        deliver(1, 0);
        CALL(ams_mel_ir_image_checkout_get_counters, receiver, &counters);
        CHECK(counters.callbacks_received >= 5 && counters.releases_uncertain == 0);
        if (!strcmp(which, "close-reentry")) release_hook(reenter);
        CALL(ams_mel_ir_image_checkout_close, &receiver);
        CALL(ams_mel_ir_channel_disable, view, &result);
        if (second) { blocked(); CALL(ams_mel_ir_image_checkout_lease_close, &second, &result); }
        deliver(1, 0); /* Closed receiver still releases actual wrappers. */
    }
    CALL(ams_mel_ir_channel_disable, view, &result);
    CALL(ams_mel_ir_image_checkout_close, &receiver);
    CHECK(ams_mel_ir_image_checkout_open(image, 2, 3, &duplicate, ARGS) == AMS_MEL_PROVIDER_FAILED);
    if (!strcmp(which, "unregister-reentry")) {
        void (*install)(void (*)(void));
        symbol = dlsym(pin, "mock_registration_reentry"); CHECK(symbol); memcpy(&install, &symbol, sizeof install);
        install(reenter);
    }
    CALL(ams_mel_ir_channel_unregister_buffer, view, buffer, &result);
    CALL(ams_mel_ir_buffer_close, &buffer);
    CALL(ams_mel_ir_stream_close, &image);
    CALL(ams_mel_ir_channel_close, &view);
    CALL(ams_mel_session_close, &session);
    CHECK(events("application_checkout_released\n") == events("application_checkout_destroyed\n"));
    if (!strcmp(which, "overflow")) {
        CHECK(position("checkout_released_3\n") < position("checkout_released_4\n"));
    }
    if (!strcmp(which, "normal")) {
        CHECK(position("checkout_released_2\n") < position("checkout_released_4\n"));
        CHECK(position("checkout_released_4\n") < position("checkout_released_3\n"));
        /* Queue drains id 4; independent lease id 3 closes afterward. */
    }
    CHECK(events("frame_produced\n") == 0 && events("buffer_registered\n") == 0);
    CHECK(position("foundation_buffer_destroyed_with_storage\n") > position("checkout_destroyed_1\n"));
    CHECK(!dlclose(pin));
    CHECK(position("library_unloaded\n") > position("foundation_buffer_destroyed_with_storage\n"));
    CHECK(!unlink(log_path)); return 0;
}
