#define _POSIX_C_SOURCE 200809L
/* IRMEL-ADA-006 complete C11 CalibrationStatus request/callback contract. */
#include <ams_mel/abi.h>
#include "admission_observation.h"
#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
extern int ams_mel_test_calibration_status_callback(unsigned);
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return EXIT_FAILURE; } } while(0)
#define STATUS_SEND 10U
typedef int (*barrier_fn)(unsigned,unsigned,unsigned);
typedef int (*release_fn)(unsigned,unsigned);
typedef int (*observe_fn)(uint32_t *,unsigned *,unsigned *);
static barrier_fn send_barrier;
static release_fn release_family;
static observe_fn observe;
_Static_assert(sizeof(ams_mel_ir_calibration_status_command_v1) == 8, "command");
_Static_assert(offsetof(ams_mel_ir_calibration_status_command_v1,request_id) == 4, "request ID");
_Static_assert(sizeof(ams_mel_ir_completed_calibration_item_v1) == 40, "item");
_Static_assert(offsetof(ams_mel_ir_completed_calibration_item_v1,result) == 16, "item result");
_Static_assert(offsetof(ams_mel_ir_completed_calibration_item_v1,fail_reason) == 24, "item reason");
_Static_assert(sizeof(ams_mel_ir_completed_calibration_v1) == 80, "completed");
_Static_assert(offsetof(ams_mel_ir_completed_calibration_v1,time_tag_ns) == 32, "time tag");
_Static_assert(offsetof(ams_mel_ir_completed_calibration_v1,result) == 40, "result");
_Static_assert(offsetof(ams_mel_ir_completed_calibration_v1,fail_reason) == 48, "reason");
_Static_assert(offsetof(ams_mel_ir_completed_calibration_v1,calibration_items) == 64, "items");
_Static_assert(sizeof(ams_mel_ir_calibration_status_v1) == 32, "status");
_Static_assert(offsetof(ams_mel_ir_calibration_status_v1,completed_calibrations) == 16, "completed span");
_Static_assert(sizeof(ams_mel_ir_calibration_status_result_v1) == 40, "result");
_Static_assert(AMS_MEL_IR_CALIBRATION_RESULT_NOT_SET == 0U && AMS_MEL_IR_CALIBRATION_RESULT_COMPLETED == 1U &&
               AMS_MEL_IR_CALIBRATION_RESULT_FAILED == 2U && AMS_MEL_IR_CALIBRATION_RESULT_INTERRUPTED == 3U &&
               AMS_MEL_IR_CALIBRATION_RESULT_NOT_CALIBRATED == 4U, "pinned results");
static ams_mel_string_view_v1 text(const char *value)
{ ams_mel_string_view_v1 result = {value, strlen(value)}; return result; }
static int bind_provider(void)
{
    void *library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(library != NULL);
    *(void **)(&send_barrier) = dlsym(library, "mock_c2_send_barrier");
    *(void **)(&release_family) = dlsym(library, "mock_completion_release_family");
    *(void **)(&observe) = dlsym(library, "mock_calibration_status_observe");
    CHECK(send_barrier && release_family && observe);
    CHECK(dlclose(library) == 0);
    return EXIT_SUCCESS;
}
struct owners { ams_mel_session *session; ams_mel_ir_c2 *c2; };
/* enable = 1: Enabled C2 (send-eligible); 0: Attached only. */
static int open_owner(const char *scenario, uint32_t limit, int enable, struct owners *o)
{
    ams_mel_ir_c2_config_v1 value;
    memset(o, 0, sizeof *o);
    if (limit) {
        ams_mel_session_options_v1 options = {limit};
        CHECK(ams_mel_session_open_with_options(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
              &options, &o->session, NULL, 0, NULL) == AMS_MEL_OK);
    } else {
        CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &o->session,
                                   NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(bind_provider() == EXIT_SUCCESS);
    memset(&value, 0, sizeof value);
    value.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
    value.channel_id.descriptive_label = text("c2");
    value.platform_id.descriptive_label = text("platform");
    value.sensor_location.key = text("key"); value.sensor_location.system_name = text("system");
    CHECK(ams_mel_ir_c2_open(o->session, &value, &o->c2, NULL, 0, NULL) == AMS_MEL_OK);
    if (enable) CHECK(ams_mel_ir_c2_enable(o->c2, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}
static int close_owner(struct owners *o)
{
    if (o->c2) CHECK(ams_mel_ir_c2_close(&o->c2, NULL, 0, NULL) == AMS_MEL_OK);
    if (o->session) CHECK(ams_mel_session_close(&o->session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}
static unsigned sends(void) { unsigned value = 0; (void)observe(NULL, &value, NULL); return value; }
static unsigned registrations(void) { unsigned value = 0; (void)observe(NULL, NULL, &value); return value; }

/* ---- Fidelity predicates (also used by the executed mutation controls) ---- */
static int is_text(ams_mel_string_view_v1 value, const char *want)
{ return value.size == strlen(want) && (!value.size || (value.data && memcmp(value.data,want,value.size)==0)); }
static int is_repeated(ams_mel_string_view_v1 value, char c, size_t n)
{
    if (value.size != n || (n && !value.data)) return 0;
    for (size_t i = 0; i < n; ++i) if (value.data[i] != c) return 0;
    return 1;
}
static int id_is(const ams_mel_uci_id_v1 *id, uint32_t seed, const char *label)
{
    if (!is_text(id->descriptive_label,label)) return 0;
    for (unsigned i=0;i<16U;++i) if(id->uuid[i] != (uint8_t)(seed+i)) return 0;
    return 1;
}
static int item_is(const ams_mel_ir_completed_calibration_item_v1 *item, const char *name,
                   uint32_t result, const char *why)
{ return is_text(item->item_name,name) && item->result==result && is_text(item->fail_reason,why); }
static const int64_t time_tags[5] = {0, -1, INT64_MIN, INT64_MAX, INT64_C(1700000000123456789)};
/* Completed calibration number i (0..4) of status_values(seed). */
static int completed_is(const ams_mel_ir_completed_calibration_v1 *e, uint32_t seed, unsigned i)
{
    char label[32];
    const ams_mel_ir_completed_calibration_item_v1 *items = e->calibration_items.data;
    const size_t counts[5] = {0U, 1U, 3U, 2U, 0U};
    if (i == 0U) label[0] = '\0'; else snprintf(label, sizeof label, "completed-%u", i);
    if (!id_is(&e->calibration_id, seed + 100U + i, label) || e->time_tag_ns != time_tags[i] ||
        e->result != i || e->calibration_items.size != counts[i] || (counts[i] && !items)) return 0;
    switch (i) {
    case 0: return is_text(e->fail_reason, "");
    case 1: return is_text(e->fail_reason, "") && item_is(&items[0], "alpha", 1U, "");
    case 2: return is_repeated(e->fail_reason, 'f', 700U) && item_is(&items[0], "", 2U, "item failure") &&
                   is_text(items[1].item_name, "beta") && items[1].result == 3U &&
                   is_repeated(items[1].fail_reason, 'q', 600U) && item_is(&items[2], "gamma", 0U, "");
    case 3: return is_text(e->fail_reason, "interrupted by operator") &&
                   item_is(&items[0], "delta", 4U, "not calibrated") &&
                   item_is(&items[1], "delta", 4U, "not calibrated");
    default: return is_text(e->fail_reason, "\xc3\xbcnicode not calibrated");
    }
}
static int active_is(const ams_mel_uci_id_span_v1 *span, uint32_t seed)
{
    return span->size == 3U && span->data && id_is(&span->data[0], seed, "") &&
           id_is(&span->data[1], seed + 40U, "active-b") && id_is(&span->data[2], seed, "");
}
/* Rich status: three active (with an ordered duplicate) and five completed. */
static int rich_is(const ams_mel_ir_calibration_status_v1 *value, uint32_t seed)
{
    if (!active_is(&value->active_calibration_ids, seed) ||
        value->completed_calibrations.size != 5U || !value->completed_calibrations.data) return 0;
    for (unsigned i = 0; i < 5U; ++i)
        if (!completed_is(&value->completed_calibrations.data[i], seed, i)) return 0;
    return 1;
}
enum shape { SHAPE_EMPTY, SHAPE_ACTIVE_ONLY, SHAPE_COMPLETED_ONLY, SHAPE_ONE };
static int shape_is(const ams_mel_ir_calibration_status_v1 *value, uint32_t seed, enum shape s)
{
    const ams_mel_uci_id_span_v1 *a = &value->active_calibration_ids;
    const ams_mel_ir_completed_calibration_span_v1 *c = &value->completed_calibrations;
    switch (s) {
    case SHAPE_EMPTY: return a->size == 0U && c->size == 0U;
    case SHAPE_ACTIVE_ONLY: return active_is(a, seed) && c->size == 0U;
    case SHAPE_COMPLETED_ONLY:
        if (a->size != 0U || c->size != 5U) return 0;
        for (unsigned i = 0; i < 5U; ++i) if (!completed_is(&c->data[i], seed, i)) return 0;
        return 1;
    default:
        return a->size == 1U && id_is(&a->data[0], seed + 40U, "active-b") && c->size == 1U &&
               completed_is(&c->data[0], seed, 2U);
    }
}
/* Predicate mutation controls: each single loss must be detected. */
static int mutations_detected(const ams_mel_ir_calibration_status_v1 *value, uint32_t seed)
{
    ams_mel_ir_completed_calibration_v1 completed[5];
    ams_mel_ir_completed_calibration_item_v1 items[3];
    ams_mel_ir_calibration_status_v1 mutated = *value;
    CHECK(rich_is(value, seed));
    memcpy(completed, value->completed_calibrations.data, sizeof completed);
    mutated.completed_calibrations.data = completed;
    CHECK(rich_is(&mutated, seed));
    completed[2].calibration_items.size = 2U;                 /* lose a nested item */
    CHECK(!rich_is(&mutated, seed));
    completed[2] = value->completed_calibrations.data[2];
    memcpy(items, completed[2].calibration_items.data, sizeof items);
    completed[2].calibration_items.data = items;
    items[1].result = 0U;                                     /* lose an item result */
    CHECK(!rich_is(&mutated, seed));
    items[1] = value->completed_calibrations.data[2].calibration_items.data[1];
    items[1].fail_reason.size = 599U;                         /* truncate item reason */
    CHECK(!rich_is(&mutated, seed));
    completed[2] = value->completed_calibrations.data[2];
    completed[3].result = 2U;                                 /* remap overall result */
    CHECK(!rich_is(&mutated, seed));
    completed[3] = value->completed_calibrations.data[3];
    completed[2].time_tag_ns = INT64_MIN + 1;                 /* lose time precision */
    CHECK(!rich_is(&mutated, seed));
    completed[2] = value->completed_calibrations.data[2];
    CHECK(rich_is(&mutated, seed));
    mutated.active_calibration_ids.size = 0U;                 /* lose one collection */
    CHECK(!rich_is(&mutated, seed));
    mutated = *value;
    mutated.completed_calibrations.size = 0U;                 /* lose the other */
    CHECK(!rich_is(&mutated, seed));
    mutated = *value;
    mutated.active_calibration_ids.size = 2U;                 /* lose the duplicate */
    CHECK(!rich_is(&mutated, seed));
    return EXIT_SUCCESS;
}
static ams_mel_status_t submit(struct owners *o, uint32_t id, uint32_t rid,
    ams_mel_ir_calibration_status_request **q)
{
    ams_mel_ir_calibration_status_command_v1 c={id,rid};
    return ams_mel_ir_c2_submit_calibration_status(o->c2,&c,q,NULL,0,NULL);
}

static int invalid_arguments(void)
{
    struct owners o;
    ams_mel_ir_calibration_status_request *q = NULL;
    ams_mel_ir_calibration_status_metadata *m = NULL;
    ams_mel_ir_calibration_status_event *event = NULL;
    const ams_mel_ir_calibration_status_v1 *view = NULL;
    ams_mel_ir_calibration_status_result_v1 result;
    ams_mel_ir_calibration_status_command_v1 command = {0U, UINT32_MAX};
    ams_mel_ir_metadata_counters_v1 counters;
    unsigned before;
    CHECK(open_owner("calstatus-rich", 0, 1, &o) == EXIT_SUCCESS);
    before = sends();
    CHECK(ams_mel_ir_c2_submit_calibration_status(NULL, &command, &q, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_submit_calibration_status(o.c2, NULL, &q, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_submit_calibration_status(o.c2, &command, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_submit_calibration_status(o.c2, &command, &q, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    q = (ams_mel_ir_calibration_status_request *)&command; /* non-null output is refused */
    CHECK(ams_mel_ir_c2_submit_calibration_status(o.c2, &command, &q, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    q = NULL;
    CHECK(before == sends());
    CHECK(ams_mel_ir_calibration_status_request_wait(NULL, 0, &result, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_calibration_status_request_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_calibration_status_open(NULL, 1, &m, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_calibration_status_open(o.c2, 0, &m, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_calibration_status_open(o.c2, 1, NULL, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_calibration_status_receive(NULL, 0, &event, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_calibration_status_get_counters(NULL, &counters, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_calibration_status_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_calibration_status_event_view(NULL, &view, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_calibration_status_event_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    /* Argument refusal of Open is not a registration attempt. */
    CHECK(ams_mel_ir_c2_calibration_status_open(o.c2, 1, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK && m);
    CHECK(ams_mel_ir_calibration_status_close(&m, NULL, 0, NULL) == AMS_MEL_OK && !m);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
static int fidelity(void)
{
    struct owners o;
    ams_mel_ir_calibration_status_request *q[4]={NULL,NULL,NULL,NULL};
    ams_mel_ir_calibration_status_result_v1 result;
    const uint32_t ids[]={0U,UINT32_MAX,0x80000001U,42U};
    const char *shapes[]={"calstatus-empty","calstatus-active-only","calstatus-completed-only","calstatus-one"};
    CHECK(open_owner("calstatus-rich",0,1,&o)==EXIT_SUCCESS);
    for(unsigned i=0;i<4U;++i) {
        uint32_t got[2]={0};
        CHECK(submit(&o,ids[i],ids[3U-i],&q[i])==AMS_MEL_OK);
        CHECK(observe(got,NULL,NULL) && got[0]==ids[i] && got[1]==ids[3U-i]); /* both fields independent */
    }
    CHECK(close_owner(&o)==EXIT_SUCCESS); /* views outlive C2 and Session */
    for(unsigned i=0;i<4U;++i) {
        const ams_mel_ir_calibration_status_v1 *first;
        CHECK(ams_mel_ir_calibration_status_request_wait(q[i],15000,&result,NULL,0,NULL)==AMS_MEL_OK);
        /* No correlation: the mock seeds data by command ID only; request ID never appears. */
        CHECK(result.error_code==AMS_MEL_ERROR_NONE && rich_is(&result.status,ids[i]));
        first=&result.status;
        CHECK(mutations_detected(first,ids[i])==EXIT_SUCCESS);
        {
            const ams_mel_ir_completed_calibration_v1 *data=result.status.completed_calibrations.data;
            CHECK(ams_mel_ir_calibration_status_request_wait(q[i],0,&result,NULL,0,NULL)==AMS_MEL_OK);
            CHECK(result.status.completed_calibrations.data==data && rich_is(&result.status,ids[i])); /* cached */
        }
        CHECK(ams_mel_ir_calibration_status_request_close(&q[i],NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_ir_calibration_status_request_close(&q[i],NULL,0,NULL)==AMS_MEL_OK);
    }
    for(unsigned n=0;n<4U;++n) {
        CHECK(open_owner(shapes[n],0,1,&o)==EXIT_SUCCESS);
        CHECK(submit(&o,7U,8U,&q[0])==AMS_MEL_OK);
        CHECK(ams_mel_ir_calibration_status_request_wait(q[0],15000,&result,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(shape_is(&result.status,7U,(enum shape)n));
        CHECK(ams_mel_ir_calibration_status_request_close(&q[0],NULL,0,NULL)==AMS_MEL_OK);
        CHECK(close_owner(&o)==EXIT_SUCCESS);
    }
    return EXIT_SUCCESS;
}
static int outcomes(void)
{
    const char *scenarios[]={"no-calstatus","calstatus-reject","calstatus-reject-long","calstatus-null",
        "calstatus-future-throw","calstatus-sentinel","calstatus-unknown","calstatus-item-sentinel",
        "calstatus-item-unknown","calstatus-nul","calstatus-item-nul","calstatus-utf8","calstatus-item-utf8",
        "calstatus-completed-utf8","calstatus-unknown-error","calstatus-rich"};
    const ams_mel_status_t expected[]={AMS_MEL_COMMAND_REJECTED,AMS_MEL_COMMAND_REJECTED,AMS_MEL_COMMAND_REJECTED,
        AMS_MEL_PROVIDER_FAILED,AMS_MEL_PROVIDER_EXCEPTION,AMS_MEL_PROVIDER_FAILED,AMS_MEL_PROVIDER_FAILED,
        AMS_MEL_PROVIDER_FAILED,AMS_MEL_PROVIDER_FAILED,AMS_MEL_PROVIDER_FAILED,AMS_MEL_PROVIDER_FAILED,
        AMS_MEL_PROVIDER_FAILED,AMS_MEL_PROVIDER_FAILED,AMS_MEL_PROVIDER_FAILED,AMS_MEL_PROVIDER_FAILED,
        AMS_MEL_INTERNAL_ERROR};
    _Static_assert(sizeof scenarios/sizeof scenarios[0]==sizeof expected/sizeof expected[0],"outcome table");
    for(unsigned i=0;i<sizeof expected/sizeof expected[0];++i) {
        struct owners o; ams_mel_ir_calibration_status_request *q=NULL;
        ams_mel_ir_calibration_status_result_v1 result;
        char d[800];size_t needed=0;
        CHECK(open_owner(scenarios[i],0,1,&o)==EXIT_SUCCESS);
        if(i==15U) CHECK(setenv("AMS_MEL_TEST_CALIBRATION_STATUS_COPY_FAILURE","allocation",1)==0);
        CHECK(submit(&o,1,2,&q)==AMS_MEL_OK);
        CHECK(ams_mel_ir_calibration_status_request_wait(q,15000,&result,d,sizeof d,&needed)==expected[i]);
        if(i<3U) CHECK(result.error_code==(i==0U?AMS_MEL_ERROR_UNSUPPORTED:AMS_MEL_ERROR_INVALID_STATE));
        if(i==1U) CHECK(strcmp(d,"calibration status rejected")==0);
        if(i==2U) CHECK(strlen(d)==700U && needed==701U);
        if(i==3U) CHECK(strcmp(d,"provider returned null successful CalibrationStatus")==0);
        if(i==4U) CHECK(strcmp(d,"mock calibration status future exception")==0);
        if(i>=5U && i<=13U) CHECK(strcmp(d,"provider returned malformed CalibrationStatus data")==0 &&
                                  result.status.active_calibration_ids.size==0U &&
                                  result.status.completed_calibrations.size==0U);
        if(i==14U) CHECK(strcmp(d,"provider returned unknown MEL error code")==0);
        CHECK(ams_mel_ir_calibration_status_request_wait(q,0,&result,NULL,0,NULL)==expected[i]);
        CHECK(ams_mel_ir_calibration_status_request_close(&q,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(close_owner(&o)==EXIT_SUCCESS);
    }
    return EXIT_SUCCESS;
}

static int admission_is(ams_mel_test_admission_token **token,uint32_t expected)
{
    uint32_t active=99,maximum=0;
    CHECK(ams_mel_test_admission(NULL,1,token,&active,&maximum));
    CHECK(active==expected && maximum==1U);return EXIT_SUCCESS;
}
static int lifecycle(void)
{
    struct owners o, other;ams_mel_ir_calibration_status_request *q=NULL,*refused=NULL,*independent=NULL;
    ams_mel_ir_calibration_status_result_v1 result;
    ams_mel_test_admission_token *token=NULL;
    unsigned before;size_t count=99;
    uint32_t active=0,maximum=0;
    /* Attached (not Enabled) refuses before provider code and admission. */
    CHECK(open_owner("calstatus-held",1,0,&o)==EXIT_SUCCESS);
    before=sends();
    CHECK(submit(&o,1,2,&q)==AMS_MEL_PROVIDER_FAILED);
    CHECK(before==sends() && q==NULL);
    CHECK(ams_mel_test_c2_requests(o.c2,&count) && count==0U);
    CHECK(ams_mel_test_admission(o.session,0,&token,NULL,NULL));
    CHECK(admission_is(&token,0U)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(o.c2,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(submit(&o,UINT32_MAX,0,&q)==AMS_MEL_OK);
    CHECK(admission_is(&token,1U)==EXIT_SUCCESS);
    CHECK(ams_mel_test_c2_requests(o.c2,&count) && count==1U);
    /* Timeout is not cancellation; repeated timeouts leave the request pending. */
    CHECK(ams_mel_ir_calibration_status_request_wait(q,0,&result,NULL,0,NULL)==AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_calibration_status_request_wait(q,1,&result,NULL,0,NULL)==AMS_MEL_TIMEOUT);
    /* Admission is per Session: a second Session with its own limit still admits. */
    CHECK(open_owner("calstatus-held",1,1,&other)==EXIT_SUCCESS);
    CHECK(submit(&other,5,5,&independent)==AMS_MEL_OK);
    before=sends();
    CHECK(submit(&o,3,4,&refused)==AMS_MEL_RESOURCE_EXHAUSTED && !refused);
    CHECK(before==sends());
    CHECK(admission_is(&token,1U)==EXIT_SUCCESS); /* refused submission is negative control */
    CHECK(release_family(STATUS_SEND,2U));
    CHECK(ams_mel_ir_calibration_status_request_wait(q,15000,&result,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(rich_is(&result.status,UINT32_MAX));
    CHECK(ams_mel_ir_calibration_status_request_wait(independent,15000,&result,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(rich_is(&result.status,5U));
    CHECK(ams_mel_test_admission(NULL,3,&token,&active,&maximum) && active==0U && maximum==1U);
    CHECK(ams_mel_ir_calibration_status_request_close(&q,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_calibration_status_request_close(&independent,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(close_owner(&other)==EXIT_SUCCESS);
    /* Request Close before completion is not cancellation; the permit stays held. */
    CHECK(submit(&o,9,9,&q)==AMS_MEL_OK);
    CHECK(admission_is(&token,1U)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_calibration_status_request_close(&q,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(admission_is(&token,1U)==EXIT_SUCCESS);
    CHECK(release_family(STATUS_SEND,1U));
    active=0;
    CHECK(ams_mel_test_admission(NULL,3,&token,&active,&maximum) && active==0U);
    /* C2 and Session Close while pending: the graph pins the provider. */
    CHECK(submit(&o,42,9,&q)==AMS_MEL_OK);
    void *pin=dlopen(AMS_MEL_TEST_MOCK_PROVIDER,RTLD_NOW|RTLD_NOLOAD);CHECK(pin);
    CHECK(close_owner(&o)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_calibration_status_request_wait(q,0,&result,NULL,0,NULL)==AMS_MEL_TIMEOUT);
    CHECK(release_family(STATUS_SEND,1U));
    CHECK(ams_mel_ir_calibration_status_request_wait(q,15000,&result,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(rich_is(&result.status,42U));
    CHECK(ams_mel_test_admission(NULL,3,&token,&active,&maximum) && active==0U);
    CHECK(dlclose(pin)==0);
    CHECK(rich_is(&result.status,42U)); /* owned by the request, not the provider */
    CHECK(ams_mel_ir_calibration_status_request_close(&q,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_test_admission(NULL,2,&token,NULL,NULL));
    for(unsigned i=0;i<2U;++i) {
        CHECK(open_owner(i?"calstatus-invalid-future":"calstatus-send-throw",1,1,&o)==EXIT_SUCCESS);
        CHECK(submit(&o,1,2,&q)==(i?AMS_MEL_PROVIDER_FAILED:AMS_MEL_PROVIDER_EXCEPTION));
        CHECK(!q && ams_mel_test_c2_requests(o.c2,&count) && count==0U);
        CHECK(ams_mel_test_admission(o.session,0,&token,NULL,NULL));
        CHECK(admission_is(&token,0U)==EXIT_SUCCESS); /* permit released exactly */
        CHECK(ams_mel_test_admission(NULL,2,&token,NULL,NULL));
        CHECK(close_owner(&o)==EXIT_SUCCESS);
    }
    return EXIT_SUCCESS;
}

static int callbacks(void)
{
    const char *scenarios[]={"calstatus-callback-rich","calstatus-callback-overflow","calstatus-callback-malformed",
                             "calstatus-callback-allocation","calstatus-callback-shapes"};
    const unsigned drained[]={3U,1U,1U,1U,4U};
    const uint64_t received[]={3U,5U,11U,2U,4U}, dropped[]={0U,4U,0U,0U,0U}, malformed[]={0U,0U,10U,1U,0U};
    for(unsigned i=0;i<5U;++i) {
        struct owners o;ams_mel_ir_calibration_status_metadata *m=NULL,*again=NULL;
        ams_mel_ir_calibration_status_event *events[4]={NULL,NULL,NULL,NULL};
        const ams_mel_ir_calibration_status_v1 *view=NULL;
        ams_mel_ir_metadata_counters_v1 c;
        ams_mel_ir_return_t registration=UINT32_MAX;
        CHECK(open_owner(scenarios[i],0,0,&o)==EXIT_SUCCESS); /* Attached registration allowed */
        if(i==3U) CHECK(setenv("AMS_MEL_TEST_CALIBRATION_STATUS_COPY_FAILURE","allocation",1)==0);
        CHECK(ams_mel_ir_c2_calibration_status_open(o.c2,i==0U?3U:i==4U?4U:1U,&m,&registration,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(registration==AMS_MEL_IR_RETURN_SUCCESS);
        CHECK(ams_mel_ir_c2_calibration_status_open(o.c2,1,&again,NULL,NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT && !again);
        CHECK(ams_mel_ir_calibration_status_get_counters(m,&c,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(c.events_received==received[i]);       /* synchronous delivery during registration */
        CHECK(c.events_dropped_queue_full==dropped[i]);
        CHECK(c.malformed_or_unsupported==malformed[i]);
        CHECK(close_owner(&o)==EXIT_SUCCESS);
        for(unsigned j=0;j<drained[i];++j) {
            CHECK(ams_mel_ir_calibration_status_receive(m,0,&events[j],NULL,0,NULL)==AMS_MEL_OK); /* drains after stop */
            CHECK(ams_mel_ir_calibration_status_event_view(events[j],&view,NULL,0,NULL)==AMS_MEL_OK);
            if(i==0U && j==1U) CHECK(shape_is(view,0U,SHAPE_EMPTY));
            else if(i==4U) CHECK(shape_is(view,30U,(enum shape)j));
            else CHECK(rich_is(view,i==0U?(j==0U?10U:20U):i==1U?0U:i==2U?10U:1U));
        }
        ams_mel_ir_calibration_status_event *empty=NULL;
        CHECK(ams_mel_ir_calibration_status_receive(m,0,&empty,NULL,0,NULL)==AMS_MEL_STREAM_STOPPED && !empty);
        CHECK(ams_mel_ir_calibration_status_close(&m,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_ir_calibration_status_close(&m,NULL,0,NULL)==AMS_MEL_OK);
        /* Event views are independent of the metadata, C2 and Session owners. */
        CHECK(ams_mel_ir_calibration_status_event_view(events[0],&view,NULL,0,NULL)==AMS_MEL_OK);
        if(i==4U) CHECK(shape_is(view,30U,SHAPE_EMPTY));
        else CHECK(rich_is(view,i==0U?10U:i==1U?0U:i==2U?10U:1U));
        for(unsigned j=0;j<4U;++j) CHECK(ams_mel_ir_calibration_status_event_close(&events[j],NULL,0,NULL)==AMS_MEL_OK);
    }
    return EXIT_SUCCESS;
}
/* Drive a callback synchronously through the mock; the rejected request never
 * copies a status, so only the callback can consume the copy failpoint. */
static int emit_capacity_callback(struct owners *o, uint32_t seed, uint32_t mode)
{
    ams_mel_ir_calibration_status_request *request = NULL;
    ams_mel_ir_calibration_status_result_v1 result;
    CHECK(submit(o,seed,mode,&request)==AMS_MEL_OK);
    CHECK(ams_mel_ir_calibration_status_request_wait(request,15000,&result,NULL,0,NULL)==AMS_MEL_COMMAND_REJECTED);
    CHECK(result.error_code==AMS_MEL_ERROR_UNSUPPORTED);
    CHECK(ams_mel_ir_calibration_status_request_close(&request,NULL,0,NULL)==AMS_MEL_OK);
    return EXIT_SUCCESS;
}
static int callback_capacity(void)
{
    struct owners o;
    ams_mel_ir_calibration_status_metadata *metadata = NULL;
    ams_mel_ir_calibration_status_event *event = NULL;
    const ams_mel_ir_calibration_status_v1 *view = NULL;
    ams_mel_ir_metadata_counters_v1 counters;
    const char *failpoint = "AMS_MEL_TEST_CALIBRATION_STATUS_COPY_FAILURE";
    CHECK(open_owner("calstatus-callback-capacity",0,1,&o)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_calibration_status_open(o.c2,1,&metadata,NULL,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(emit_capacity_callback(&o,41U,0U)==EXIT_SUCCESS); /* copy and fill queue */
    CHECK(setenv(failpoint,"allocation",1)==0);
    CHECK(emit_capacity_callback(&o,42U,0U)==EXIT_SUCCESS); /* valid, full: no copy */
    CHECK(getenv(failpoint)!=NULL && strcmp(getenv(failpoint),"allocation")==0);
    CHECK(ams_mel_ir_calibration_status_get_counters(metadata,&counters,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(counters.events_received==2U && counters.events_dropped_queue_full==1U &&
          counters.malformed_or_unsupported==0U);
    /* All nine existing corruptions, then null, while the first event fills the
     * queue. Invalid data must not be misclassified as queue-full or copied. */
    for(uint32_t mode=1U;mode<=10U;++mode) {
        CHECK(emit_capacity_callback(&o,43U,mode)==EXIT_SUCCESS);
        CHECK(getenv(failpoint)!=NULL && strcmp(getenv(failpoint),"allocation")==0);
        CHECK(ams_mel_ir_calibration_status_get_counters(metadata,&counters,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(counters.events_received==2U+mode && counters.events_dropped_queue_full==1U &&
              counters.malformed_or_unsupported==mode);
    }
    CHECK(ams_mel_ir_calibration_status_receive(metadata,0,&event,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_calibration_status_event_view(event,&view,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(rich_is(view,41U)); /* original retained; DROP-INCOMING order and full fidelity */
    CHECK(ams_mel_ir_calibration_status_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(emit_capacity_callback(&o,44U,0U)==EXIT_SUCCESS); /* space: copy allocation fails */
    CHECK(getenv(failpoint)==NULL); /* proves the same failpoint is live on the copy path */
    CHECK(ams_mel_ir_calibration_status_receive(metadata,0,&event,NULL,0,NULL)==AMS_MEL_TIMEOUT && !event);
    CHECK(ams_mel_ir_calibration_status_get_counters(metadata,&counters,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(counters.events_received==13U && counters.events_dropped_queue_full==1U &&
          counters.malformed_or_unsupported==11U);
    CHECK(emit_capacity_callback(&o,45U,0U)==EXIT_SUCCESS); /* recovery after capacity returns */
    CHECK(ams_mel_ir_calibration_status_receive(metadata,0,&event,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_calibration_status_event_view(event,&view,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(rich_is(view,45U));
    CHECK(ams_mel_ir_calibration_status_get_counters(metadata,&counters,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(counters.events_received==14U && counters.events_dropped_queue_full==1U &&
          counters.malformed_or_unsupported==11U);
    CHECK(ams_mel_ir_calibration_status_close(&metadata,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(close_owner(&o)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_calibration_status_event_view(event,&view,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(rich_is(view,45U)); /* snapshot outlives mock payload and all provider owners */
    CHECK(ams_mel_ir_calibration_status_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);
    puts("PASS: full CalibrationStatus queue skips copies without changing malformed classification");
    return EXIT_SUCCESS;
}
static int optional_and_independent(void)
{
    const char *scenarios[]={"no-calstatus","calstatus-register-fail","calstatus-register-throw"};
    for(unsigned i=0;i<3U;++i) {
        struct owners o;ams_mel_ir_calibration_status_metadata *m=NULL;
        ams_mel_ir_calibration_configuration_metadata *configuration=NULL;
        ams_mel_ir_return_t registration=UINT32_MAX;
        char d[128];
        unsigned before;
        CHECK(open_owner(scenarios[i],0,1,&o)==EXIT_SUCCESS);
        before=registrations();
        CHECK(ams_mel_ir_c2_calibration_status_open(o.c2,2,&m,&registration,d,sizeof d,NULL)==
            (i==2U?AMS_MEL_PROVIDER_EXCEPTION:AMS_MEL_PROVIDER_FAILED));
        CHECK(!m && registrations()==before+1U);
        if(i<2U) CHECK(registration==(i?AMS_MEL_IR_RETURN_FAIL:AMS_MEL_IR_RETURN_NOT_SUPPORTED));
        else CHECK(registration==UINT32_MAX);
        CHECK(strcmp(d,i==0U?"provider does not support CalibrationStatus":
                     i==1U?"CalibrationStatus callback registration failed":
                     "provider exception during CalibrationStatus registration")==0);
        /* One-shot including failure: no second provider registration. */
        CHECK(ams_mel_ir_c2_calibration_status_open(o.c2,2,&m,NULL,NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT);
        CHECK(registrations()==before+1U);
        /* Independent of the CalibrationConfiguration owner (never registered by it). */
        if(i==0U) {
            CHECK(ams_mel_ir_c2_calibration_configuration_open(o.c2,1,&configuration,&registration,NULL,0,NULL)==
                  AMS_MEL_PROVIDER_FAILED && registration==AMS_MEL_IR_RETURN_NOT_SUPPORTED);
            CHECK(registrations()==before+1U);
        }
        CHECK(close_owner(&o)==EXIT_SUCCESS);
    }
    /* Request without registration; registration without request. */
    {
        struct owners o;ams_mel_ir_calibration_status_request *q=NULL;
        ams_mel_ir_calibration_status_result_v1 result;
        unsigned before;
        CHECK(open_owner("calstatus-rich",0,1,&o)==EXIT_SUCCESS);
        before=registrations();
        CHECK(submit(&o,3,4,&q)==AMS_MEL_OK);
        CHECK(ams_mel_ir_calibration_status_request_wait(q,15000,&result,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(rich_is(&result.status,3U) && registrations()==before);
        CHECK(ams_mel_ir_calibration_status_request_close(&q,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(close_owner(&o)==EXIT_SUCCESS);
    }
    {
        struct owners o;ams_mel_ir_calibration_status_metadata *m=NULL;
        unsigned before;
        /* Provider hooks are used only while a Session keeps the provider loaded. */
        CHECK(open_owner("calstatus-callback-rich",0,1,&o)==EXIT_SUCCESS);
        before=sends();
        CHECK(ams_mel_ir_c2_calibration_status_open(o.c2,3,&m,NULL,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(sends()==before);
        CHECK(ams_mel_ir_calibration_status_close(&m,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(close_owner(&o)==EXIT_SUCCESS);
    }
    /* Callback delivered during unlocked send does not complete the request, and
     * metadata closure does not cancel the outstanding request. */
    struct owners o;ams_mel_ir_calibration_status_metadata *m=NULL;
    ams_mel_ir_calibration_status_event *event=NULL;
    const ams_mel_ir_calibration_status_v1 *view=NULL;
    ams_mel_ir_calibration_status_request *q=NULL;
    ams_mel_ir_calibration_status_result_v1 result;
    CHECK(open_owner("calstatus-sync-held",0,1,&o)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_calibration_status_open(o.c2,2,&m,NULL,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(submit(&o,5,6,&q)==AMS_MEL_OK); /* callback during unlocked send */
    CHECK(ams_mel_ir_calibration_status_receive(m,0,&event,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_calibration_status_event_view(event,&view,NULL,0,NULL)==AMS_MEL_OK && rich_is(view,77U));
    CHECK(ams_mel_ir_calibration_status_request_wait(q,0,&result,NULL,0,NULL)==AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_calibration_status_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_calibration_status_close(&m,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_calibration_status_request_wait(q,0,&result,NULL,0,NULL)==AMS_MEL_TIMEOUT);
    CHECK(release_family(STATUS_SEND,1U));
    CHECK(ams_mel_ir_calibration_status_request_wait(q,15000,&result,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(rich_is(&result.status,5U));
    CHECK(ams_mel_ir_calibration_status_request_close(&q,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(close_owner(&o)==EXIT_SUCCESS);
    /* Late callback after public Close touches only the captured state. */
    CHECK(open_owner("calstatus-late",0,1,&o)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_calibration_status_open(o.c2,3,&m,NULL,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_calibration_status_close(&m,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(close_owner(&o)==EXIT_SUCCESS); /* invokes retained callback after owner close */
    return EXIT_SUCCESS;
}

static unsigned occurrences(const char *path, const char *event)
{
    FILE *file = fopen(path, "r");
    char line[192];
    unsigned count = 0;
    if (!file) return 0;
    while (fgets(line, sizeof line, file))
        if (strncmp(line, event, strlen(event)) == 0 && line[strlen(event)] == '\n') ++count;
    fclose(file);
    return count;
}
static int make_log(char *path)
{
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    return EXIT_SUCCESS;
}
/* Isolated process: post-send allocation/worker-launch failure retains the
 * accounted future graph; the provider is never destroyed or unloaded. */
static int isolated_post_send(const char *failure)
{
    char log[] = "/tmp/ams-calibration-status-post-send-XXXXXX";
    struct owners o;
    ams_mel_ir_calibration_status_request *request = NULL;
    const ams_mel_ir_calibration_status_command_v1 c = {6U, 6U};
    ams_mel_test_admission_token *token = NULL;
    size_t count = 0;
    char d[96];
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner("calstatus-held", 1, 1, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_test_admission(o.session, 0, &token, NULL, NULL));
    CHECK(setenv("AMS_MEL_TEST_CALIBRATION_STATUS_POST_SEND_FAILURE", failure, 1) == 0);
    CHECK(ams_mel_ir_c2_submit_calibration_status(o.c2, &c, &request, d, sizeof d, NULL) == AMS_MEL_INTERNAL_ERROR);
    CHECK(request == NULL);
    CHECK(strcmp(d, strcmp(failure, "allocation") == 0 ?
                 "facade allocation failed after provider send" : "facade worker launch failed") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_CALIBRATION_STATUS_POST_SEND_FAILURE") == 0);
    CHECK(ams_mel_test_c2_requests(o.c2, &count) && count == 1U);
    CHECK(admission_is(&token, 1U) == EXIT_SUCCESS); /* retained permit, Session-specific */
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(occurrences(log, "c2_channel_destroyed") == 0U);
    CHECK(occurrences(log, "library_unloaded") == 0U);
    CHECK(unlink(log) == 0);
    printf("PASS: isolated C2 CalibrationStatusCmd post-send %s retention\n", failure);
    return EXIT_SUCCESS; /* retained future intentionally never consumed */
}
/* Isolated process: deferred cleanup failure is never reported as success. */
static int isolated_finish_failure(void)
{
    struct owners o;
    ams_mel_ir_calibration_status_request *request = NULL;
    ams_mel_ir_calibration_status_result_v1 result;
    const ams_mel_ir_calibration_status_command_v1 c = {7U, 7U};
    char d[96];
    void *pin;
    CHECK(setenv("AMS_MEL_TEST_C2_FINISH_FAILURE", "exception", 1) == 0);
    CHECK(open_owner("calstatus-held", 0, 1, &o) == EXIT_SUCCESS);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin != NULL);
    CHECK(ams_mel_ir_c2_submit_calibration_status(o.c2, &c, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(release_family(STATUS_SEND, 1) == 1);
    CHECK(ams_mel_ir_calibration_status_request_wait(request, 15000, &result, d, sizeof d, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(strcmp(d, "deferred C2 cleanup failed") == 0);
    CHECK(ams_mel_ir_calibration_status_request_wait(request, 0, &result, d, sizeof d, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_calibration_status_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    printf("PASS: isolated C2 CalibrationStatusCmd deferred-cleanup failure\n");
    return EXIT_SUCCESS; /* graph intentionally retained; pin never released */
}
static void *release_callback(void *unused)
{
    (void)unused;
    if (!ams_mel_test_calibration_status_callback(2U)) abort();
    return NULL;
}
/* Isolated process: a callback counted in flight while the public owner and
 * the C2 channel close; physical teardown waits for it to return. */
static int callback_in_flight(void)
{
    char log[] = "/tmp/ams-calibration-status-in-flight-XXXXXX";
    struct owners o;
    ams_mel_ir_calibration_status_metadata *m = NULL;
    ams_mel_ir_calibration_status_event *event = NULL;
    pthread_t release;
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner("calstatus-in-flight", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_test_calibration_status_callback(0U));
    CHECK(ams_mel_ir_c2_calibration_status_open(o.c2, 1, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_calibration_status_callback(1U)); /* inside bridge callback, counted in flight */
    CHECK(ams_mel_ir_calibration_status_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(pthread_create(&release, NULL, release_callback, NULL) == 0);
    CHECK(close_owner(&o) == EXIT_SUCCESS); /* joins callback before physical provider destruction */
    CHECK(pthread_join(release, NULL) == 0);
    CHECK(occurrences(log, "calibration_status_callback_returned") == 1U);
    CHECK(unlink(log) == 0);
    (void)event;
    puts("PASS: Close while calibration status bridge callback is counted in flight");
    return EXIT_SUCCESS;
}
int main(int argc,char **argv)
{
    if(argc==2 && strcmp(argv[1],"callback-in-flight")==0) return callback_in_flight();
    if(argc==3 && strcmp(argv[1],"post-send")==0) return isolated_post_send(argv[2]);
    if(argc==2 && strcmp(argv[1],"finish-failure")==0) return isolated_finish_failure();
    CHECK(invalid_arguments()==EXIT_SUCCESS);
    CHECK(fidelity()==EXIT_SUCCESS);
    CHECK(outcomes()==EXIT_SUCCESS);
    CHECK(lifecycle()==EXIT_SUCCESS);
    CHECK(callbacks()==EXIT_SUCCESS);
    CHECK(callback_capacity()==EXIT_SUCCESS);
    CHECK(optional_and_independent()==EXIT_SUCCESS);
    puts("PASS: CalibrationStatus complete nested C11 contracts and mutation controls");
    return EXIT_SUCCESS;
}
