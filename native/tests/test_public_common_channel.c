/* Task 032B1: behavior of the INSTALLED public common Channel ABI.
 *
 * Every scenario runs in its own process so provider gates, failpoints and
 * lifetime logs cannot leak between cases. Only public ams_mel_ir_channel_*
 * functions create and use the common views; the test-build admission
 * observer is used solely to prove the existing Session permit is reused. */
#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include "admission_observation.h"
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int ams_mel_test_completion_wait(unsigned, unsigned, uint64_t);
extern int ams_mel_test_completion_owner(unsigned, uint64_t, uint64_t *);
typedef int (*release_fn)(unsigned, unsigned);
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)

enum family { C2, IMAGE, HEALTH, INSTRUMENTATION, TRACK };
static const char *const family_names[] = {"c2", "image", "health", "instrumentation", "track"};
static const char *const destroyed_events[] = {"c2_channel_destroyed\n", "channel_destroyed\n",
    "health_channel_destroyed\n", "instrumentation_channel_destroyed\n", "track_channel_destroyed\n"};
static const ams_mel_ir_channel_comms_test_request_v1 high_ids = {0x80000001U, 0xf0000002U, 0xe0000003U};

typedef struct {
    enum family family;
    char path[64];
    void *pin;
    release_fn release;
    ams_mel_session *session;
    ams_mel_ir_c2 *c2;
    ams_mel_ir_stream *stream;
    ams_mel_ir_health *health;
    ams_mel_ir_instrumentation *instrumentation;
    ams_mel_ir_track *track;
} fixture;

static ams_mel_string_view_v1 text(const char *s)
{ ams_mel_string_view_v1 v = {s, strlen(s)}; return v; }

/* A NULL event counts every provider-side lifetime event. Test-probe
 * "completion_*" lines are excluded: the shared 032A engine preallocates its
 * worker input (holding the probe FinalOwner) before locking the weak state,
 * so a refused submission logs only that facade-local destruction. */
static unsigned count(const char *path, const char *event)
{
    FILE *file = fopen(path, "r"); char line[160]; unsigned result = 0;
    CHECK(file != NULL);
    while (fgets(line, sizeof line, file))
        if (event ? !strcmp(line, event) : strncmp(line, "completion_", 11) != 0) ++result;
    CHECK(fclose(file) == 0);
    return result;
}

static void open_fixture(fixture *f, enum family family, const char *scenario, uint32_t limit)
{
    memset(f, 0, sizeof *f);
    f->family = family;
    strcpy(f->path, "/tmp/ams-public-channel-XXXXXX");
    int fd = mkstemp(f->path);
    CHECK(fd >= 0 && close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", f->path, 1) == 0);
    ams_mel_session_options_v1 options = {limit};
    CHECK(ams_mel_session_open_with_options(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
        &options, &f->session, NULL, 0, NULL) == AMS_MEL_OK);
    f->pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(f->pin != NULL);
    *(void **)(&f->release) = dlsym(f->pin, "mock_completion_release_family");
    CHECK(f->release != NULL);
    ams_mel_uci_id_v1 channel_id = {{0}, text("public")};
    ams_mel_uci_id_v1 platform_id = {{0}, text("platform")};
    ams_mel_component_location_v1 location = {0.0, 0.0, 0.0, text("key"), text("system")};
    switch (family) {
    case C2: {
        ams_mel_ir_c2_config_v1 c = {AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL,
                                     channel_id, platform_id, location};
        CHECK(ams_mel_ir_c2_open(f->session, &c, &f->c2, NULL, 0, NULL) == AMS_MEL_OK);
        break;
    }
    case IMAGE: {
        ams_mel_ir_stream_config_v1 c = {AMS_MEL_IR_CHANNEL_IRST_IMAGE, channel_id,
                                         platform_id, location, 2U, 64U, 2U};
        CHECK(ams_mel_ir_stream_open(f->session, &c, &f->stream, NULL, 0, NULL) == AMS_MEL_OK);
        break;
    }
    case HEALTH: {
        ams_mel_ir_health_config_v1 c = {channel_id, AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS,
                                         platform_id, location};
        CHECK(ams_mel_ir_health_open(f->session, &c, &f->health, NULL, 0, NULL) == AMS_MEL_OK);
        break;
    }
    case INSTRUMENTATION: {
        ams_mel_ir_instrumentation_config_v1 c = {0};
        c.channel_type = AMS_MEL_IR_CHANNEL_INSTRUMENTATION;
        c.channel_id = channel_id; c.platform_id = platform_id; c.sensor_location = location;
        CHECK(ams_mel_ir_instrumentation_open(f->session, &c, &f->instrumentation,
            NULL, 0, NULL) == AMS_MEL_OK);
        break;
    }
    case TRACK: {
        ams_mel_ir_track_config_v1 c = {0};
        c.channel_type = AMS_MEL_IR_CHANNEL_IRST_TRACK;
        c.channel_id = channel_id; c.platform_id = platform_id; c.sensor_location = location;
        CHECK(ams_mel_ir_track_open(f->session, &c, &f->track, NULL, 0, NULL) == AMS_MEL_OK);
        break;
    }
    }
}

/* Completion worker owners are counted per process so a Session is closed only
 * after every worker has dropped its provider future (existing 032A pattern).
 * Refused submissions also destroy one preallocated owner and are counted. */
static uint64_t returns_admitted, comms_admitted;
static void drain(void)
{
    uint64_t observed = 0;
    if (returns_admitted) CHECK(ams_mel_test_completion_owner(1U, returns_admitted, &observed));
    if (comms_admitted) CHECK(ams_mel_test_completion_owner(2U, comms_admitted, &observed));
}

static ams_mel_status_t view_from(const fixture *f, ams_mel_ir_channel **out)
{
    switch (f->family) {
    case C2: return ams_mel_ir_channel_from_c2(f->c2, out, NULL, 0, NULL);
    case IMAGE: return ams_mel_ir_channel_from_stream(f->stream, out, NULL, 0, NULL);
    case HEALTH: return ams_mel_ir_channel_from_health(f->health, out, NULL, 0, NULL);
    case INSTRUMENTATION:
        return ams_mel_ir_channel_from_instrumentation(f->instrumentation, out, NULL, 0, NULL);
    case TRACK: return ams_mel_ir_channel_from_track(f->track, out, NULL, 0, NULL);
    }
    return AMS_MEL_INTERNAL_ERROR;
}

static ams_mel_status_t typed_capability(const fixture *f, ams_mel_ir_channel_capability **out,
                                         char *diagnostic, size_t capacity)
{
    switch (f->family) {
    case C2: return ams_mel_ir_c2_get_capabilities(f->c2, out, diagnostic, capacity, NULL);
    case IMAGE: return ams_mel_ir_stream_get_capabilities(f->stream, out, diagnostic, capacity, NULL);
    case HEALTH: return ams_mel_ir_health_get_capabilities(f->health, out, diagnostic, capacity, NULL);
    case INSTRUMENTATION: return ams_mel_ir_instrumentation_get_capabilities(
        f->instrumentation, out, diagnostic, capacity, NULL);
    case TRACK: return ams_mel_ir_track_get_capabilities(f->track, out, diagnostic, capacity, NULL);
    }
    return AMS_MEL_INTERNAL_ERROR;
}

/* Image "enable" is the typed Start transition to Running. */
static void typed_enable(const fixture *f)
{
    switch (f->family) {
    case C2: CHECK(ams_mel_ir_c2_enable(f->c2, NULL, 0, NULL) == AMS_MEL_OK); break;
    case IMAGE: CHECK(ams_mel_ir_stream_start(f->stream, NULL, 0, NULL) == AMS_MEL_OK); break;
    case HEALTH: CHECK(ams_mel_ir_health_enable(f->health, NULL, 0, NULL) == AMS_MEL_OK); break;
    case INSTRUMENTATION:
        CHECK(ams_mel_ir_instrumentation_enable(f->instrumentation, NULL, 0, NULL) == AMS_MEL_OK);
        break;
    case TRACK: CHECK(ams_mel_ir_track_enable(f->track, NULL, 0, NULL) == AMS_MEL_OK); break;
    }
}

static void typed_close(fixture *f)
{
    switch (f->family) {
    case C2: CHECK(ams_mel_ir_c2_close(&f->c2, NULL, 0, NULL) == AMS_MEL_OK && !f->c2); break;
    case IMAGE: CHECK(ams_mel_ir_stream_close(&f->stream, NULL, 0, NULL) == AMS_MEL_OK && !f->stream); break;
    case HEALTH: CHECK(ams_mel_ir_health_close(&f->health, NULL, 0, NULL) == AMS_MEL_OK && !f->health); break;
    case INSTRUMENTATION:
        CHECK(ams_mel_ir_instrumentation_close(&f->instrumentation, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(!f->instrumentation);
        break;
    case TRACK: CHECK(ams_mel_ir_track_close(&f->track, NULL, 0, NULL) == AMS_MEL_OK && !f->track); break;
    }
}

/* Closes the typed owner and Session, unloads the test pin, and proves the
 * complete provider graph tore down exactly once. */
static void teardown(fixture *f)
{
    drain();
    if (f->c2 || f->stream || f->health || f->instrumentation || f->track) typed_close(f);
    CHECK(ams_mel_session_close(&f->session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(dlclose(f->pin) == 0);
    f->pin = NULL;
    CHECK(count(f->path, destroyed_events[f->family]) == 1U);
    CHECK(count(f->path, "control_destroyed\n") == 1U);
    CHECK(count(f->path, "manager_destroyed\n") == 1U);
    CHECK(count(f->path, "library_unloaded\n") == 1U);
}

static int same_view(ams_mel_string_view_v1 a, ams_mel_string_view_v1 b)
{ return a.size == b.size && (a.size == 0U || memcmp(a.data, b.data, a.size) == 0); }
static int same_id(ams_mel_uci_id_v1 a, ams_mel_uci_id_v1 b)
{ return !memcmp(a.uuid, b.uuid, sizeof a.uuid) && same_view(a.descriptive_label, b.descriptive_label); }
static int same_u32(ams_mel_u32_span_v1 a, ams_mel_u32_span_v1 b)
{ return a.size == b.size && (a.size == 0U || !memcmp(a.data, b.data, a.size * sizeof *a.data)); }

/* Reusable value-based comparison of the complete ChannelCapability snapshot.
 * The two snapshots must be distinct storage: pointer identity is not proof. */
static void same_capability(const ams_mel_ir_channel_capability_v1 *a,
                            const ams_mel_ir_channel_capability_v1 *b)
{
    CHECK(a != b);
    CHECK(same_id(a->channel_id, b->channel_id) && same_id(a->platform_id, b->platform_id));
    CHECK(a->height == b->height && a->width == b->width && a->bit_depth == b->bit_depth);
    CHECK(a->row_pitch == b->row_pitch && a->buffer_size == b->buffer_size);
    CHECK(a->image_size == b->image_size && a->number_of_bands == b->number_of_bands);
    CHECK(a->pixel_format == b->pixel_format && same_u32(a->sensor_types, b->sensor_types));
    CHECK(a->sensor_location.offset_x_m == b->sensor_location.offset_x_m);
    CHECK(a->sensor_location.offset_y_m == b->sensor_location.offset_y_m);
    CHECK(a->sensor_location.offset_z_m == b->sensor_location.offset_z_m);
    CHECK(same_view(a->sensor_location.key, b->sensor_location.key));
    CHECK(same_view(a->sensor_location.system_name, b->sensor_location.system_name));
    CHECK(same_u32(a->channel_types, b->channel_types));
    CHECK(a->task_schedule_depth == b->task_schedule_depth);
    CHECK(a->odc_available == b->odc_available && a->nuc_available == b->nuc_available);
    CHECK(same_u32(a->metadata_capabilities, b->metadata_capabilities));
    CHECK(a->image_bands.size == b->image_bands.size);
    for (size_t i = 0; i < a->image_bands.size; ++i) {
        const ams_mel_ir_image_band_v1 *x = &a->image_bands.data[i], *y = &b->image_bands.data[i];
        CHECK(x->band_index == y->band_index && x->bands.size == y->bands.size);
        for (size_t j = 0; j < x->bands.size; ++j)
            CHECK(x->bands.data[j].type == y->bands.data[j].type &&
                  x->bands.data[j].min_wavelength_m == y->bands.data[j].min_wavelength_m &&
                  x->bands.data[j].max_wavelength_m == y->bands.data[j].max_wavelength_m);
    }
    CHECK(same_u32(a->nav_frames, b->nav_frames));
}

static const ams_mel_ir_channel_capability_v1 *viewed(const ams_mel_ir_channel_capability *owner)
{
    const ams_mel_ir_channel_capability_v1 *value = NULL;
    CHECK(ams_mel_ir_channel_capability_view(owner, &value, NULL, 0, NULL) == AMS_MEL_OK && value);
    return value;
}

/* Generic and typed capability snapshots are equivalent by value. */
static void capability_fidelity(const fixture *f, const ams_mel_ir_channel *view)
{
    ams_mel_ir_channel_capability *generic = NULL, *typed = NULL;
    CHECK(ams_mel_ir_channel_get_capabilities(view, &generic, NULL, 0, NULL) == AMS_MEL_OK && generic);
    CHECK(typed_capability(f, &typed, NULL, 0) == AMS_MEL_OK && typed);
    same_capability(viewed(generic), viewed(typed));
    CHECK(ams_mel_ir_channel_capability_close(&generic, NULL, 0, NULL) == AMS_MEL_OK && !generic);
    CHECK(ams_mel_ir_channel_capability_close(&typed, NULL, 0, NULL) == AMS_MEL_OK && !typed);
}

/* Every public operation on an expired view fails cleanly with NULL output. */
static void expired(const ams_mel_ir_channel *view)
{
    ams_mel_ir_return_request *ret = NULL;
    ams_mel_ir_channel_comms_request *reply = NULL;
    ams_mel_ir_channel_capability *capability = NULL;
    CHECK(ams_mel_ir_channel_send_keepalive(view, &ret, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_channel_submit_comms_test(view, &high_ids, &reply, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_channel_get_capabilities(view, &capability, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(!ret && !reply && !capability);
}

/* KeepAlive, high-ID CommsTest with cached repeated Wait, and Capabilities.
 * Under "completion-scale" every family's provider future is held until the
 * test releases its completion family (1 = Return, 2 = Comms). */
static void exercise(const fixture *f, const ams_mel_ir_channel *view)
{
    ams_mel_ir_return_request *ret = NULL;
    ams_mel_ir_channel_comms_request *reply = NULL;
    ams_mel_ir_return_result_v1 result = {99U, 99U};
    ams_mel_ir_channel_comms_test_result_v1 comms = {0U, 0U, 99U};
    char small[2], large[64];
    CHECK(ams_mel_ir_channel_send_keepalive(view, &ret, NULL, 0, NULL) == AMS_MEL_OK && ret);
    ++returns_admitted;
    CHECK(ams_mel_ir_return_request_wait(ret, 0U, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(f->release(1U, 1U));
    CHECK(ams_mel_ir_return_request_wait(ret, 15000U, &result, large, sizeof large, NULL) == AMS_MEL_OK);
    CHECK(result.value == AMS_MEL_IR_RETURN_SUCCESS && result.error_code == AMS_MEL_ERROR_NONE);
    result.value = 99U;
    CHECK(ams_mel_ir_return_request_wait(ret, 0U, &result, small, sizeof small, NULL) == AMS_MEL_OK);
    CHECK(result.value == AMS_MEL_IR_RETURN_SUCCESS);
    CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK && !ret);
    CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_submit_comms_test(view, &high_ids, &reply, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(reply != NULL);
    ++comms_admitted;
    CHECK(f->release(2U, 1U));
    CHECK(ams_mel_ir_channel_comms_request_wait(reply, 15000U, &comms, large, sizeof large, NULL) ==
          AMS_MEL_OK);
    CHECK(comms.command_id == 0x80000001U && comms.request_id == 0xe0000003U);
    CHECK(comms.error_code == AMS_MEL_ERROR_NONE);
    memset(&comms, 0, sizeof comms);
    CHECK(ams_mel_ir_channel_comms_request_wait(reply, 0U, &comms, small, sizeof small, NULL) ==
          AMS_MEL_OK);
    CHECK(comms.command_id == 0x80000001U && comms.request_id == 0xe0000003U);
    CHECK(comms.error_code == AMS_MEL_ERROR_NONE);
    CHECK(ams_mel_ir_channel_comms_request_close(&reply, NULL, 0, NULL) == AMS_MEL_OK && !reply);
    capability_fidelity(f, view);
}

/* Typed family-specific sends keep their Enabled-only rules. */
static void typed_enabled_only(const fixture *f, ams_mel_status_t expected)
{
    if (f->family == INSTRUMENTATION) {
        ams_mel_ir_instrumentation_level_command_v1 level = {7U, AMS_MEL_IR_PRIORITY_NORMAL};
        ams_mel_ir_instrumentation_request *request = NULL;
        CHECK(ams_mel_ir_instrumentation_submit_level(f->instrumentation, &level, &request,
            NULL, 0, NULL) == expected);
        CHECK(!request);
    } else if (f->family == TRACK) {
        ams_mel_ir_track_data_update_v1 update = {0};
        ams_mel_ir_system_track_data_response_v1 response = {0};
        ams_mel_ir_track_update_request *update_request = NULL;
        ams_mel_ir_track_system_response_request *response_request = NULL;
        update.capability_uuid.descriptive_label = text("cap");
        update.activity_uuid.descriptive_label = text("act");
        update.entity_uuid.descriptive_label = text("entity");
        CHECK(ams_mel_ir_track_submit_update(f->track, &update, &update_request,
            NULL, 0, NULL) == expected);
        CHECK(ams_mel_ir_track_submit_system_track_data_response(f->track, &response,
            &response_request, NULL, 0, NULL) == expected);
        CHECK(!update_request && !response_request);
    }
}

/* Attached common operations, then again after typed Enable (Image: Running). */
static void scenario_lifecycle(enum family family)
{
    fixture f;
    ams_mel_ir_channel *view = NULL;
    open_fixture(&f, family, "completion-scale", 4U);
    CHECK(view_from(&f, &view) == AMS_MEL_OK && view);
    typed_enabled_only(&f, AMS_MEL_PROVIDER_FAILED);
    exercise(&f, view);
    typed_enable(&f);
    exercise(&f, view);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK && !view);
    teardown(&f);
    CHECK(unlink(f.path) == 0);
}

/* Typed owner and Session close first while the public view is kept alive. */
static void scenario_weak(enum family family)
{
    fixture f;
    ams_mel_ir_channel *view = NULL;
    open_fixture(&f, family, "completion-scale", 4U);
    CHECK(view_from(&f, &view) == AMS_MEL_OK && view);
    typed_close(&f);
    teardown(&f);
    const unsigned lines = count(f.path, NULL);
    expired(view);
    CHECK(count(f.path, "keepalive_sent\n") == 0U && count(f.path, "comms_sent\n") == 0U);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK && !view);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK && !view);
    CHECK(count(f.path, NULL) == lines);
    CHECK(unlink(f.path) == 0);
}

/* Closing the view first leaves the typed owner completely usable. */
static void scenario_close_first(enum family family)
{
    fixture f;
    ams_mel_ir_channel *view = NULL;
    ams_mel_ir_channel_capability *capability = NULL;
    open_fixture(&f, family, "completion-scale", 4U);
    CHECK(view_from(&f, &view) == AMS_MEL_OK && view);
    const unsigned lines = count(f.path, NULL);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK && !view);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK && !view);
    CHECK(count(f.path, NULL) == lines);
    CHECK(typed_capability(&f, &capability, NULL, 0) == AMS_MEL_OK && capability);
    CHECK(ams_mel_ir_channel_capability_close(&capability, NULL, 0, NULL) == AMS_MEL_OK);
    typed_enable(&f);
    CHECK(typed_capability(&f, &capability, NULL, 0) == AMS_MEL_OK && capability);
    CHECK(ams_mel_ir_channel_capability_close(&capability, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count(f.path, destroyed_events[family]) == 0U);
    teardown(&f);
    CHECK(unlink(f.path) == 0);
}

/* A held public KeepAlive survives view Close, typed Close and Session Close:
 * the admitted request, not the view, owns the provider graph. The Session
 * limit of one also proves the view reuses the existing Session admission. */
static void scenario_pending(enum family family)
{
    fixture f;
    ams_mel_ir_channel *view = NULL;
    ams_mel_ir_return_request *ret = NULL, *refused = NULL;
    ams_mel_test_admission_token *token = NULL;
    ams_mel_ir_return_result_v1 result = {99U, 99U};
    uint32_t active = 99U, maximum = 0U;
    uint64_t observed = 0U;
    open_fixture(&f, family, "completion-scale", 1U);
    CHECK(ams_mel_test_admission(f.session, 0U, &token, NULL, NULL));
    CHECK(view_from(&f, &view) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_send_keepalive(view, &ret, NULL, 0, NULL) == AMS_MEL_OK && ret);
    CHECK(ams_mel_test_completion_wait(1U, 4U, 1U));
    CHECK(ams_mel_ir_channel_send_keepalive(view, &refused, NULL, 0, NULL) ==
          AMS_MEL_RESOURCE_EXHAUSTED && !refused);
    CHECK(count(f.path, "keepalive_sent\n") == 1U);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK && !view);
    typed_close(&f);
    CHECK(ams_mel_session_close(&f.session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_admission(NULL, 1U, &token, &active, &maximum) && active == 1U && maximum == 1U);
    CHECK(count(f.path, destroyed_events[family]) == 0U);
    CHECK(count(f.path, "control_destroyed\n") == 0U && count(f.path, "manager_destroyed\n") == 0U);
    CHECK(ams_mel_ir_return_request_wait(ret, 0U, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(f.release(1U, 1U));
    CHECK(ams_mel_ir_return_request_wait(ret, 15000U, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.value == AMS_MEL_IR_RETURN_SUCCESS && result.error_code == AMS_MEL_ERROR_NONE);
    /* One refused preallocation plus the admitted worker's FinalOwner. */
    CHECK(ams_mel_test_completion_owner(1U, 2U, &observed));
    CHECK(ams_mel_test_admission(NULL, 1U, &token, &active, &maximum) && active == 0U);
    CHECK(ams_mel_test_admission(NULL, 2U, &token, NULL, NULL));
    CHECK(count(f.path, destroyed_events[family]) == 1U);
    CHECK(count(f.path, "control_destroyed\n") == 1U && count(f.path, "manager_destroyed\n") == 1U);
    CHECK(ams_mel_ir_return_request_wait(ret, 0U, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK && !ret);
    CHECK(dlclose(f.pin) == 0);
    CHECK(count(f.path, "library_unloaded\n") == 1U);
    CHECK(unlink(f.path) == 0);
}

/* Independent views: closing one leaves the other usable; neither owns the
 * provider after the typed owner closes. */
static void scenario_multiple(enum family family)
{
    fixture f;
    ams_mel_ir_channel *first = NULL, *second = NULL;
    open_fixture(&f, family, "completion-scale", 4U);
    CHECK(view_from(&f, &first) == AMS_MEL_OK && view_from(&f, &second) == AMS_MEL_OK);
    CHECK(first && second && first != second);
    CHECK(ams_mel_ir_channel_close(&first, NULL, 0, NULL) == AMS_MEL_OK && !first);
    exercise(&f, second);
    CHECK(view_from(&f, &first) == AMS_MEL_OK && first);
    typed_close(&f);
    teardown(&f);
    expired(first);
    expired(second);
    CHECK(ams_mel_ir_channel_close(&second, NULL, 0, NULL) == AMS_MEL_OK && !second);
    CHECK(ams_mel_ir_channel_close(&first, NULL, 0, NULL) == AMS_MEL_OK && !first);
    CHECK(unlink(f.path) == 0);
}

/* A generic capability snapshot remains readable after the view, typed owner,
 * Session and provider library are all gone. */
static void scenario_snapshot(enum family family)
{
    fixture f;
    ams_mel_ir_channel *view = NULL;
    ams_mel_ir_channel_capability *generic = NULL, *typed = NULL;
    /* Rich provider capabilities populate IDs, location, bands and nav frames. */
    open_fixture(&f, family, family == C2 ? "capability-rich" :
        family == IMAGE ? "image-capability-rich" : "completion-scale", 4U);
    CHECK(view_from(&f, &view) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_get_capabilities(view, &generic, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(typed_capability(&f, &typed, NULL, 0) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK);
    typed_close(&f);
    teardown(&f);
    same_capability(viewed(generic), viewed(typed));
    const ams_mel_ir_channel_capability_v1 *value = viewed(generic);
    CHECK(value->channel_types.size >= 1U && value->metadata_capabilities.size >= 1U);
    if (family != IMAGE) {
        /* Shared rich_channel_capability(): the complete populated shape. */
        CHECK(value->channel_id.uuid[0] == 0x11U && value->platform_id.uuid[0] == 0x31U);
        CHECK(value->sensor_location.offset_x_m == 1.25 && value->sensor_location.offset_y_m == -2.5);
        CHECK(value->image_bands.size == 2U && value->image_bands.data[0].bands.size == 2U);
        CHECK(value->nav_frames.size == 2U && value->sensor_types.size == 2U);
    } else CHECK(value->height == 200U && value->width == 320U);
    CHECK(ams_mel_ir_channel_capability_close(&generic, NULL, 0, NULL) == AMS_MEL_OK && !generic);
    CHECK(ams_mel_ir_channel_capability_close(&typed, NULL, 0, NULL) == AMS_MEL_OK && !typed);
    CHECK(unlink(f.path) == 0);
}

/* Pointer and diagnostic preconditions for all nine exports. */
static void scenario_invalid(enum family family)
{
    fixture f;
    ams_mel_ir_channel *view = NULL, *sentinel = (ams_mel_ir_channel *)&f;
    char diagnostic[8];
    size_t required = 99U;
    open_fixture(&f, family, "completion-scale", 4U);
    const unsigned lines = count(f.path, NULL);
#define CONVERT(src, out, d, c, r) \
    (family == C2 ? ams_mel_ir_channel_from_c2((src) ? f.c2 : NULL, out, d, c, r) : \
     family == IMAGE ? ams_mel_ir_channel_from_stream((src) ? f.stream : NULL, out, d, c, r) : \
     family == HEALTH ? ams_mel_ir_channel_from_health((src) ? f.health : NULL, out, d, c, r) : \
     family == INSTRUMENTATION ? ams_mel_ir_channel_from_instrumentation( \
         (src) ? f.instrumentation : NULL, out, d, c, r) : \
     ams_mel_ir_channel_from_track((src) ? f.track : NULL, out, d, c, r))
    CHECK(CONVERT(0, &view, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && !view);
    CHECK(CONVERT(1, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    view = sentinel;
    CHECK(CONVERT(1, &view, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT && view == sentinel);
    view = NULL;
    CHECK(CONVERT(1, &view, NULL, sizeof diagnostic, &required) == AMS_MEL_INVALID_ARGUMENT && !view);
    CHECK(CONVERT(1, &view, diagnostic, sizeof diagnostic, &required) == AMS_MEL_OK && view);
    CHECK(diagnostic[0] == '\0' && required == 1U);
#undef CONVERT
    ams_mel_ir_return_request *ret = NULL, *ret_sentinel = (ams_mel_ir_return_request *)&f;
    ams_mel_ir_channel_comms_request *reply = NULL, *reply_sentinel = (ams_mel_ir_channel_comms_request *)&f;
    ams_mel_ir_channel_capability *cap = NULL, *cap_sentinel = (ams_mel_ir_channel_capability *)&f;
    CHECK(ams_mel_ir_channel_send_keepalive(NULL, &ret, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_send_keepalive(view, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_send_keepalive(view, &ret_sentinel, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_send_keepalive(view, &ret, NULL, 4U, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_submit_comms_test(NULL, &high_ids, &reply, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_submit_comms_test(view, NULL, &reply, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_submit_comms_test(view, &high_ids, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_submit_comms_test(view, &high_ids, &reply_sentinel, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_submit_comms_test(view, &high_ids, &reply, NULL, 4U, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_get_capabilities(NULL, &cap, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_get_capabilities(view, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_get_capabilities(view, &cap_sentinel, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_get_capabilities(view, &cap, NULL, 4U, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(!ret && !reply && !cap);
    CHECK(ret_sentinel == (ams_mel_ir_return_request *)&f);
    CHECK(reply_sentinel == (ams_mel_ir_channel_comms_request *)&f);
    CHECK(cap_sentinel == (ams_mel_ir_channel_capability *)&f);
    CHECK(count(f.path, NULL) == lines);
    CHECK(ams_mel_ir_channel_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 4U, NULL) == AMS_MEL_INVALID_ARGUMENT && view);
    CHECK(ams_mel_ir_channel_close(&view, diagnostic, sizeof diagnostic, &required) == AMS_MEL_OK);
    CHECK(!view && diagnostic[0] == '\0' && required == 1U);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK && !view);
    teardown(&f);
    CHECK(unlink(f.path) == 0);
}

/* Test-build-only view allocation failpoint: INTERNAL_ERROR, NULL output, no
 * provider call, and an unaffected typed owner. */
static void scenario_allocation(enum family family)
{
    fixture f;
    ams_mel_ir_channel *view = NULL;
    ams_mel_ir_channel_capability *capability = NULL;
    char diagnostic[64] = {0};
    open_fixture(&f, family, "completion-scale", 4U);
    const unsigned lines = count(f.path, NULL);
    CHECK(setenv("AMS_MEL_TEST_CHANNEL_VIEW_FAILURE", "allocation", 1) == 0);
    ams_mel_status_t status = AMS_MEL_OK;
    switch (family) {
    case C2: status = ams_mel_ir_channel_from_c2(f.c2, &view, diagnostic, sizeof diagnostic, NULL); break;
    case IMAGE: status = ams_mel_ir_channel_from_stream(f.stream, &view, diagnostic, sizeof diagnostic, NULL); break;
    case HEALTH: status = ams_mel_ir_channel_from_health(f.health, &view, diagnostic, sizeof diagnostic, NULL); break;
    case INSTRUMENTATION: status = ams_mel_ir_channel_from_instrumentation(f.instrumentation, &view,
        diagnostic, sizeof diagnostic, NULL); break;
    case TRACK: status = ams_mel_ir_channel_from_track(f.track, &view, diagnostic, sizeof diagnostic, NULL); break;
    }
    CHECK(unsetenv("AMS_MEL_TEST_CHANNEL_VIEW_FAILURE") == 0);
    CHECK(status == AMS_MEL_INTERNAL_ERROR && !view);
    CHECK(!strcmp(diagnostic, "Channel view allocation failed"));
    CHECK(count(f.path, NULL) == lines);
    CHECK(typed_capability(&f, &capability, NULL, 0) == AMS_MEL_OK && capability);
    CHECK(ams_mel_ir_channel_capability_close(&capability, NULL, 0, NULL) == AMS_MEL_OK);
    typed_enable(&f);
    CHECK(view_from(&f, &view) == AMS_MEL_OK && view);
    exercise(&f, view);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK);
    teardown(&f);
    CHECK(unlink(f.path) == 0);
}

/* Legacy C2 inherited-service exports and the generic path agree on status,
 * result, and provider diagnostic for one immediate provider scenario, while
 * Attached and again while Enabled. Pointer identity and allocation behavior
 * are deliberately not compared. */
static void compare_c2(const char *scenario, int comms_scenario)
{
    fixture f;
    ams_mel_ir_channel *view = NULL;
    open_fixture(&f, C2, scenario, 8U);
    CHECK(view_from(&f, &view) == AMS_MEL_OK);
    for (int enabled = 0; enabled < 2; ++enabled) {
        if (enabled) typed_enable(&f);
        ams_mel_ir_return_request *old_ret = NULL, *new_ret = NULL;
        ams_mel_ir_channel_comms_request *old_reply = NULL, *new_reply = NULL;
        ams_mel_ir_return_result_v1 old_result = {77U, 77U}, new_result = {77U, 77U};
        ams_mel_ir_channel_comms_test_result_v1 old_comms = {7U, 7U, 7U}, new_comms = {7U, 7U, 7U};
        char old_text[512] = {0}, new_text[512] = {0};
        size_t old_required = 0U, new_required = 0U;
        if (!comms_scenario) {
            CHECK(ams_mel_ir_c2_send_keepalive(f.c2, &old_ret, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_channel_send_keepalive(view, &new_ret, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(old_ret && new_ret && old_ret != new_ret);
            const ams_mel_status_t a = ams_mel_ir_return_request_wait(old_ret, 15000U, &old_result,
                old_text, sizeof old_text, &old_required);
            const ams_mel_status_t b = ams_mel_ir_return_request_wait(new_ret, 15000U, &new_result,
                new_text, sizeof new_text, &new_required);
            CHECK(a == b && old_result.value == new_result.value);
            CHECK(old_result.error_code == new_result.error_code);
            CHECK(!strcmp(old_text, new_text) && old_required == new_required);
            CHECK(ams_mel_ir_return_request_close(&old_ret, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_return_request_close(&new_ret, NULL, 0, NULL) == AMS_MEL_OK);
            returns_admitted += 2U;
        } else {
            CHECK(ams_mel_ir_c2_submit_comms_test(f.c2, &high_ids, &old_reply, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_channel_submit_comms_test(view, &high_ids, &new_reply, NULL, 0, NULL) ==
                  AMS_MEL_OK);
            const ams_mel_status_t a = ams_mel_ir_channel_comms_request_wait(old_reply, 15000U,
                &old_comms, old_text, sizeof old_text, &old_required);
            const ams_mel_status_t b = ams_mel_ir_channel_comms_request_wait(new_reply, 15000U,
                &new_comms, new_text, sizeof new_text, &new_required);
            CHECK(a == b && !memcmp(&old_comms, &new_comms, sizeof old_comms));
            CHECK(!strcmp(old_text, new_text) && old_required == new_required);
            CHECK(ams_mel_ir_channel_comms_request_close(&old_reply, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_channel_comms_request_close(&new_reply, NULL, 0, NULL) == AMS_MEL_OK);
            comms_admitted += 2U;
        }
        capability_fidelity(&f, view);
    }
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK);
    teardown(&f);
    CHECK(unlink(f.path) == 0);
}

/* A Failed typed C2 owner: legacy and generic paths both refuse KeepAlive,
 * CommsTest and Capabilities with PROVIDER_FAILED, NULL output, no send. */
static void compare_c2_refusals(void)
{
    fixture f;
    ams_mel_ir_channel *view = NULL;
    ams_mel_ir_return_request *old_ret = NULL, *new_ret = NULL;
    ams_mel_ir_channel_comms_request *old_reply = NULL, *new_reply = NULL;
    ams_mel_ir_channel_capability *old_cap = NULL, *new_cap = NULL;
    open_fixture(&f, C2, "c2-enable-fail", 4U);
    CHECK(view_from(&f, &view) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_enable(f.c2, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_c2_send_keepalive(f.c2, &old_ret, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_channel_send_keepalive(view, &new_ret, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_c2_submit_comms_test(f.c2, &high_ids, &old_reply, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_channel_submit_comms_test(view, &high_ids, &new_reply, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_c2_get_capabilities(f.c2, &old_cap, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_channel_get_capabilities(view, &new_cap, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(!old_ret && !new_ret && !old_reply && !new_reply && !old_cap && !new_cap);
    CHECK(count(f.path, "keepalive_sent\n") == 0U && count(f.path, "comms_sent\n") == 0U);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK);
    teardown(&f);
    CHECK(unlink(f.path) == 0);
}

static enum family family_of(const char *name, const char **rest)
{
    for (unsigned i = 0; i < 5U; ++i) {
        const size_t n = strlen(family_names[i]);
        if (!strncmp(name, family_names[i], n) && name[n] == '-') {
            *rest = name + n + 1;
            return (enum family)i;
        }
    }
    fprintf(stderr, "unknown family: %s\n", name);
    exit(2);
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    const char *name = argv[1];
    if (!strcmp(name, "c2-legacy-keepalive")) { compare_c2("public-channel-c2", 0); return 0; }
    if (!strcmp(name, "c2-legacy-keepalive-fail")) { compare_c2("keepalive-fail", 0); return 0; }
    if (!strcmp(name, "c2-legacy-keepalive-reject")) { compare_c2("keepalive-reject", 0); return 0; }
    if (!strcmp(name, "c2-legacy-comms")) { compare_c2("comms-high", 1); return 0; }
    if (!strcmp(name, "c2-legacy-comms-reject")) { compare_c2("comms-reject", 1); return 0; }
    if (!strcmp(name, "c2-legacy-refusal")) { compare_c2_refusals(); return 0; }
    const char *rest = NULL;
    const enum family family = family_of(name, &rest);
    if (!strcmp(rest, "lifecycle")) scenario_lifecycle(family);
    else if (!strcmp(rest, "weak")) scenario_weak(family);
    else if (!strcmp(rest, "close-first")) scenario_close_first(family);
    else if (!strcmp(rest, "pending")) scenario_pending(family);
    else if (!strcmp(rest, "multiple")) scenario_multiple(family);
    else if (!strcmp(rest, "snapshot")) scenario_snapshot(family);
    else if (!strcmp(rest, "invalid")) scenario_invalid(family);
    else if (!strcmp(rest, "allocation")) scenario_allocation(family);
    else { fprintf(stderr, "unknown scenario: %s\n", name); return 2; }
    return 0;
}
