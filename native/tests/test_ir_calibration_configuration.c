#define _POSIX_C_SOURCE 200809L
/* IRMEL-ADA-005 complete C11 request/callback contract. */
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
extern int ams_mel_test_calibration_callback(unsigned);
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return EXIT_FAILURE; } } while(0)
typedef int (*barrier_fn)(unsigned,unsigned,unsigned);
typedef int (*release_fn)(unsigned,unsigned);
typedef int (*last_fn)(uint32_t *,unsigned *);
static barrier_fn send_barrier;
static release_fn release_family;
static last_fn last_command;
_Static_assert(sizeof(ams_mel_ir_calibration_configuration_command_v1) == 8, "command");
_Static_assert(offsetof(ams_mel_ir_calibration_configuration_command_v1,request_id) == 4, "request ID");
_Static_assert(sizeof(ams_mel_ir_subsystem_calibration_v1) == 88, "entry");
_Static_assert(offsetof(ams_mel_ir_subsystem_calibration_v1,calibration_item_names) == 40, "names");
_Static_assert(offsetof(ams_mel_ir_subsystem_calibration_v1,subsystem_component_ids) == 56, "components");
_Static_assert(offsetof(ams_mel_ir_subsystem_calibration_v1,capability_ids) == 72, "capabilities");
_Static_assert(sizeof(ams_mel_ir_calibration_configuration_result_v1) == 24, "result");
static ams_mel_string_view_v1 text(const char *value)
{ ams_mel_string_view_v1 result = {value, strlen(value)}; return result; }
/* Resolves test-only provider controls; used only while a Session (or a
 * pending request graph) keeps the provider loaded. */
static int bind_provider(void)
{
    void *library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(library != NULL);
    *(void **)(&send_barrier) = dlsym(library, "mock_c2_send_barrier");
    *(void **)(&release_family) = dlsym(library, "mock_completion_release_family");
    *(void **)(&last_command) = dlsym(library, "mock_calibration_last_command");
    CHECK(send_barrier && release_family && last_command);
    CHECK(dlclose(library) == 0);
    return EXIT_SUCCESS;
}

struct owners {
    ams_mel_session *session;
    ams_mel_ir_c2 *c2;
};

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


static int is_text(ams_mel_string_view_v1 value, const char *want)
{ return value.size == strlen(want) && (!value.size || (value.data && memcmp(value.data,want,value.size)==0)); }
static int id_is(const ams_mel_uci_id_v1 *id, uint32_t seed, const char *label)
{
    if (!is_text(id->descriptive_label,label)) return 0;
    for (unsigned i=0;i<16U;++i) if(id->uuid[i] != (uint8_t)(seed+i)) return 0;
    return 1;
}
/* Predicate used by mutation controls too: every collection/alternative is observed. */
static int rich_is(const ams_mel_ir_calibration_configuration_v1 *value, uint32_t seed)
{
    if(value->size!=3U || !value->data) return 0;
    for(unsigned i=0;i<3U;++i) {
        const ams_mel_ir_subsystem_calibration_v1 *e=&value->data[i];
        const char *label=i==0U?"":i==1U?"calibration-1":"calibration-2";
        if(!id_is(&e->calibration_id,seed+i,label) || e->accepted_interface!=i) return 0;
        if(e->calibration_item_names.size!=(i==0U?0U:i==1U?3U:1U) ||
           e->subsystem_component_ids.size!=(i==0U?0U:i==1U?1U:2U) ||
           e->capability_ids.size!=(i==1U?2U:0U)) return 0;
    }
    const ams_mel_ir_subsystem_calibration_v1 *e=&value->data[1];
    if(!is_text(e->calibration_item_names.data[0],"") || !is_text(e->calibration_item_names.data[1],"one") ||
       e->calibration_item_names.data[2].size!=700U) return 0;
    for(unsigned i=0;i<700U;++i) if(e->calibration_item_names.data[2].data[i]!='z') return 0;
    if(!id_is(&e->subsystem_component_ids.data[0],32U,"component") ||
       !id_is(&e->capability_ids.data[0],64U,"capability-a") || !id_is(&e->capability_ids.data[1],96U,"")) return 0;
    e=&value->data[2];
    return is_text(e->calibration_item_names.data[0],"last") &&
        id_is(&e->subsystem_component_ids.data[0],128U,"") && id_is(&e->subsystem_component_ids.data[1],160U,"component-b");
}
static ams_mel_status_t submit(struct owners *o, uint32_t id, uint32_t rid,
    ams_mel_ir_calibration_configuration_request **q)
{
    ams_mel_ir_calibration_configuration_command_v1 c={id,rid};
    return ams_mel_ir_c2_submit_calibration_configuration(o->c2,&c,q,NULL,0,NULL);
}
static int invalid_arguments(void)
{
    struct owners o;
    ams_mel_ir_calibration_configuration_request *q = NULL;
    ams_mel_ir_calibration_configuration_metadata *m = NULL;
    ams_mel_ir_calibration_configuration_event *event = NULL;
    const ams_mel_ir_calibration_configuration_v1 *view = NULL;
    ams_mel_ir_calibration_configuration_result_v1 result;
    ams_mel_ir_calibration_configuration_command_v1 command = {0U, UINT32_MAX};
    ams_mel_ir_metadata_counters_v1 counters;
    CHECK(open_owner("calibration-rich", 0, 1, &o) == EXIT_SUCCESS);
    unsigned before = 0, after = 0;
    (void)last_command(NULL, &before);
    CHECK(ams_mel_ir_c2_submit_calibration_configuration(NULL, &command, &q, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_submit_calibration_configuration(o.c2, NULL, &q, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_submit_calibration_configuration(o.c2, &command, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_submit_calibration_configuration(o.c2, &command, &q, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    (void)last_command(NULL, &after);
    CHECK(before == after && !q);
    CHECK(ams_mel_ir_calibration_configuration_request_wait(NULL, 0, &result, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_calibration_configuration_request_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_calibration_configuration_open(NULL, 1, &m, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_calibration_configuration_open(o.c2, 0, &m, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_calibration_configuration_receive(NULL, 0, &event, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_calibration_configuration_get_counters(NULL, &counters, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_calibration_configuration_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_calibration_configuration_event_view(NULL, &view, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_calibration_configuration_event_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
static int fidelity(void)
{
    struct owners o;
    ams_mel_ir_calibration_configuration_request *q[4]={NULL,NULL,NULL,NULL};
    ams_mel_ir_calibration_configuration_result_v1 result;
    const uint32_t ids[]={0U,UINT32_MAX,0x80000001U,42U};
    CHECK(open_owner("calibration-rich",0,1,&o)==EXIT_SUCCESS);
    for(unsigned i=0;i<4U;++i) {
        uint32_t got[2]={0};
        CHECK(submit(&o,ids[i],ids[3U-i],&q[i])==AMS_MEL_OK);
        CHECK(last_command(got,NULL) && got[0]==ids[i] && got[1]==ids[3U-i]);
    }
    CHECK(close_owner(&o)==EXIT_SUCCESS);
    for(unsigned i=0;i<4U;++i) {
        CHECK(ams_mel_ir_calibration_configuration_request_wait(q[i],15000,&result,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(result.error_code==AMS_MEL_ERROR_NONE && rich_is(&result.configuration,ids[i]));
        CHECK(ams_mel_ir_calibration_configuration_request_wait(q[i],0,&result,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(rich_is(&result.configuration,ids[i]));
        /* Mutation control: removal and wrong accepted interface are detected. */
        ams_mel_ir_subsystem_calibration_v1 entries[3];
        memcpy(entries,result.configuration.data,sizeof entries);
        ams_mel_ir_calibration_configuration_v1 mutated={entries,3};
        entries[1].capability_ids.size=1; CHECK(!rich_is(&mutated,ids[i]));
        entries[1]=result.configuration.data[1]; entries[1].accepted_interface=2;
        CHECK(!rich_is(&mutated,ids[i]));
        CHECK(ams_mel_ir_calibration_configuration_request_close(&q[i],NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_ir_calibration_configuration_request_close(&q[i],NULL,0,NULL)==AMS_MEL_OK);
    }
    for(unsigned n=0;n<2U;++n) {
        CHECK(open_owner(n==0U?"calibration-empty":"calibration-one",0,1,&o)==EXIT_SUCCESS);
        CHECK(submit(&o,1U,2U,&q[0])==AMS_MEL_OK);
        CHECK(ams_mel_ir_calibration_configuration_request_wait(q[0],15000,&result,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(result.configuration.size==n);
        CHECK(ams_mel_ir_calibration_configuration_request_close(&q[0],NULL,0,NULL)==AMS_MEL_OK);
        CHECK(close_owner(&o)==EXIT_SUCCESS);
    }
    return EXIT_SUCCESS;
}
static int outcomes(void)
{
    const char *scenarios[]={"no-calibration","calibration-reject","calibration-reject-long","calibration-null",
        "calibration-future-throw","calibration-sentinel","calibration-unknown","calibration-nul","calibration-utf8","calibration-unknown-error","calibration-rich"};
    const ams_mel_status_t expected[]={AMS_MEL_COMMAND_REJECTED,AMS_MEL_COMMAND_REJECTED,AMS_MEL_COMMAND_REJECTED,
        AMS_MEL_PROVIDER_FAILED,AMS_MEL_PROVIDER_EXCEPTION,AMS_MEL_PROVIDER_FAILED,AMS_MEL_PROVIDER_FAILED,
        AMS_MEL_PROVIDER_FAILED,AMS_MEL_PROVIDER_FAILED,AMS_MEL_PROVIDER_FAILED,AMS_MEL_INTERNAL_ERROR};
    for(unsigned i=0;i<sizeof expected/sizeof expected[0];++i) {
        struct owners o; ams_mel_ir_calibration_configuration_request *q=NULL;
        ams_mel_ir_calibration_configuration_result_v1 result;
        CHECK(open_owner(scenarios[i],0,1,&o)==EXIT_SUCCESS);
        if(i==10U) CHECK(setenv("AMS_MEL_TEST_CALIBRATION_COPY_FAILURE","allocation",1)==0);
        CHECK(submit(&o,1,2,&q)==AMS_MEL_OK);
        char d[800];size_t needed=0;
        CHECK(ams_mel_ir_calibration_configuration_request_wait(q,15000,&result,d,sizeof d,&needed)==expected[i]);
        if(i<3U) CHECK(result.error_code==(i==0U?AMS_MEL_ERROR_UNSUPPORTED:AMS_MEL_ERROR_INVALID_PARAMETERS));
        if(i==2U) CHECK(strlen(d)==700U && needed==701U);
        CHECK(ams_mel_ir_calibration_configuration_request_wait(q,0,&result,NULL,0,NULL)==expected[i]);
        CHECK(ams_mel_ir_calibration_configuration_request_close(&q,NULL,0,NULL)==AMS_MEL_OK);
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
    struct owners o;ams_mel_ir_calibration_configuration_request *q=NULL,*refused=NULL;
    ams_mel_ir_calibration_configuration_result_v1 result;
    ams_mel_test_admission_token *token=NULL;
    unsigned before,after;size_t count=99;
    CHECK(open_owner("calibration-held",1,0,&o)==EXIT_SUCCESS);
    (void)last_command(NULL,&before);
    CHECK(submit(&o,1,2,&q)==AMS_MEL_PROVIDER_FAILED);
    (void)last_command(NULL,&after);CHECK(before==after && q==NULL);
    CHECK(ams_mel_test_c2_requests(o.c2,&count) && count==0U);
    CHECK(ams_mel_test_admission(o.session,0,&token,NULL,NULL));
    CHECK(admission_is(&token,0U)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(o.c2,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(submit(&o,UINT32_MAX,0,&q)==AMS_MEL_OK);
    CHECK(admission_is(&token,1U)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_calibration_configuration_request_wait(q,0,&result,NULL,0,NULL)==AMS_MEL_TIMEOUT);
    (void)last_command(NULL,&before);
    CHECK(submit(&o,3,4,&refused)==AMS_MEL_RESOURCE_EXHAUSTED && !refused);
    (void)last_command(NULL,&after);CHECK(before==after);
    CHECK(admission_is(&token,1U)==EXIT_SUCCESS); /* refused submission is negative control */
    CHECK(ams_mel_ir_calibration_configuration_request_close(&q,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(admission_is(&token,1U)==EXIT_SUCCESS);
    CHECK(release_family(9U,1U));
    uint32_t active=0,maximum=0;
    CHECK(ams_mel_test_admission(NULL,3,&token,&active,&maximum) && active==0U && maximum==1U);
    CHECK(submit(&o,42,9,&q)==AMS_MEL_OK);
    void *pin=dlopen(AMS_MEL_TEST_MOCK_PROVIDER,RTLD_NOW|RTLD_NOLOAD);CHECK(pin);
    CHECK(close_owner(&o)==EXIT_SUCCESS); /* pending future pins physical provider graph */
    CHECK(ams_mel_ir_calibration_configuration_request_wait(q,0,&result,NULL,0,NULL)==AMS_MEL_TIMEOUT);
    CHECK(release_family(9U,1U));
    CHECK(ams_mel_ir_calibration_configuration_request_wait(q,15000,&result,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(rich_is(&result.configuration,42U));
    CHECK(ams_mel_test_admission(NULL,3,&token,&active,&maximum) && active==0U);
    CHECK(dlclose(pin)==0);
    CHECK(rich_is(&result.configuration,42U));
    CHECK(ams_mel_ir_calibration_configuration_request_close(&q,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_test_admission(NULL,2,&token,NULL,NULL));
    for(unsigned i=0;i<2U;++i) {
        CHECK(open_owner(i?"calibration-invalid-future":"calibration-send-throw",1,1,&o)==EXIT_SUCCESS);
        CHECK(submit(&o,1,2,&q)==(i?AMS_MEL_PROVIDER_FAILED:AMS_MEL_PROVIDER_EXCEPTION));
        CHECK(!q && ams_mel_test_c2_requests(o.c2,&count) && count==0U);
        CHECK(close_owner(&o)==EXIT_SUCCESS);
    }
    return EXIT_SUCCESS;
}
static int callbacks(void)
{
    const char *scenarios[]={"calibration-callback-rich","calibration-callback-overflow","calibration-callback-malformed","calibration-callback-allocation"};
    for(unsigned i=0;i<4U;++i) {
        struct owners o;ams_mel_ir_calibration_configuration_metadata *m=NULL,*again=NULL;
        ams_mel_ir_calibration_configuration_event *events[3]={NULL,NULL,NULL};
        const ams_mel_ir_calibration_configuration_v1 *view=NULL;
        ams_mel_ir_metadata_counters_v1 c;
        CHECK(open_owner(scenarios[i],0,0,&o)==EXIT_SUCCESS); /* Attached registration allowed */
        if(i==3U) CHECK(setenv("AMS_MEL_TEST_CALIBRATION_COPY_FAILURE","allocation",1)==0);
        CHECK(ams_mel_ir_c2_calibration_configuration_open(o.c2,i==0U?3U:1U,&m,NULL,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_ir_c2_calibration_configuration_open(o.c2,1,&again,NULL,NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT && !again);
        CHECK(ams_mel_ir_calibration_configuration_get_counters(m,&c,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(c.events_received==(i==0U?3U:i==1U?5U:i==2U?6U:2U));
        CHECK(c.events_dropped_queue_full==(i==1U?4U:0U));
        CHECK(c.malformed_or_unsupported==(i==2U?5U:i==3U?1U:0U));
        CHECK(close_owner(&o)==EXIT_SUCCESS);
        for(unsigned j=0;j<(i==0U?3U:1U);++j) {
            CHECK(ams_mel_ir_calibration_configuration_receive(m,0,&events[j],NULL,0,NULL)==AMS_MEL_OK);
            CHECK(ams_mel_ir_calibration_configuration_event_view(events[j],&view,NULL,0,NULL)==AMS_MEL_OK);
            if(i==0U && j==1U) CHECK(view->size==0U);
            else CHECK(rich_is(view,i==0U?(j==0U?10U:20U):i==1U?0U:i==2U?10U:1U));
        }
        ams_mel_ir_calibration_configuration_event *empty=NULL;
        CHECK(ams_mel_ir_calibration_configuration_receive(m,0,&empty,NULL,0,NULL)==AMS_MEL_STREAM_STOPPED);
        CHECK(ams_mel_ir_calibration_configuration_close(&m,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_ir_calibration_configuration_event_view(events[0],&view,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(rich_is(view,i==0U?10U:i==1U?0U:i==2U?10U:1U));
        for(unsigned j=0;j<3U;++j) CHECK(ams_mel_ir_calibration_configuration_event_close(&events[j],NULL,0,NULL)==AMS_MEL_OK);
    }
    return EXIT_SUCCESS;
}
static int optional_and_independent(void)
{
    const char *scenarios[]={"no-calibration","calibration-register-fail","calibration-register-throw"};
    for(unsigned i=0;i<3U;++i) {
        struct owners o;ams_mel_ir_calibration_configuration_metadata *m=NULL;
        ams_mel_ir_return_t registration=UINT32_MAX;
        CHECK(open_owner(scenarios[i],0,1,&o)==EXIT_SUCCESS);
        CHECK(ams_mel_ir_c2_calibration_configuration_open(o.c2,2,&m,&registration,NULL,0,NULL)==
            (i==2U?AMS_MEL_PROVIDER_EXCEPTION:AMS_MEL_PROVIDER_FAILED));
        CHECK(!m);
        if(i<2U) CHECK(registration==(i?AMS_MEL_IR_RETURN_FAIL:AMS_MEL_IR_RETURN_NOT_SUPPORTED));
        CHECK(ams_mel_ir_c2_calibration_configuration_open(o.c2,2,&m,NULL,NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT);
        CHECK(close_owner(&o)==EXIT_SUCCESS);
    }
    struct owners o;ams_mel_ir_calibration_configuration_metadata *m=NULL;
    ams_mel_ir_calibration_configuration_event *event=NULL;
    ams_mel_ir_calibration_configuration_request *q=NULL;
    ams_mel_ir_calibration_configuration_result_v1 result;
    CHECK(open_owner("calibration-sync-held",0,1,&o)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_calibration_configuration_open(o.c2,2,&m,NULL,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(submit(&o,5,6,&q)==AMS_MEL_OK); /* callback during unlocked send */
    CHECK(ams_mel_ir_calibration_configuration_receive(m,0,&event,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_calibration_configuration_request_wait(q,0,&result,NULL,0,NULL)==AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_calibration_configuration_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(release_family(9U,1U));
    CHECK(ams_mel_ir_calibration_configuration_request_wait(q,15000,&result,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(rich_is(&result.configuration,5U));
    CHECK(ams_mel_ir_calibration_configuration_receive(m,0,&event,NULL,0,NULL)==AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_calibration_configuration_request_close(&q,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_calibration_configuration_close(&m,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(close_owner(&o)==EXIT_SUCCESS);
    CHECK(open_owner("calibration-late",0,1,&o)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_calibration_configuration_open(o.c2,3,&m,NULL,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_calibration_configuration_close(&m,NULL,0,NULL)==AMS_MEL_OK);
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

static int isolated_post_send(const char *failure)
{
    char log[] = "/tmp/ams-calibration-post-send-XXXXXX";
    struct owners o;
    ams_mel_ir_calibration_configuration_request *request = NULL;
    const ams_mel_ir_calibration_configuration_command_v1 c = {6U, 6U};
    size_t count = 0;
    char d[96];
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner("calibration-held", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_CALIBRATION_CONFIGURATION_POST_SEND_FAILURE", failure, 1) == 0);
    CHECK(ams_mel_ir_c2_submit_calibration_configuration(o.c2, &c, &request, d, sizeof d, NULL) == AMS_MEL_INTERNAL_ERROR);
    CHECK(request == NULL);
    CHECK(strcmp(d, strcmp(failure, "allocation") == 0 ?
                 "facade allocation failed after provider send" : "facade worker launch failed") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_CALIBRATION_CONFIGURATION_POST_SEND_FAILURE") == 0);
    CHECK(ams_mel_test_c2_requests(o.c2, &count) && count == 1U);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(occurrences(log, "c2_channel_destroyed") == 0U);
    CHECK(occurrences(log, "library_unloaded") == 0U);
    CHECK(unlink(log) == 0);
    printf("PASS: isolated C2 CalibrationConfigurationCmd post-send %s retention\n", failure);
    return EXIT_SUCCESS; /* retained future intentionally never consumed */
}

/* Isolated process: deferred cleanup failure is never reported as success. */
static int isolated_finish_failure(void)
{
    struct owners o;
    ams_mel_ir_calibration_configuration_request *request = NULL;
    ams_mel_ir_calibration_configuration_result_v1 result;
    const ams_mel_ir_calibration_configuration_command_v1 c = {7U, 7U};
    char d[96];
    void *pin;
    CHECK(setenv("AMS_MEL_TEST_C2_FINISH_FAILURE", "exception", 1) == 0);
    CHECK(open_owner("calibration-held", 0, 1, &o) == EXIT_SUCCESS);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin != NULL);
    CHECK(ams_mel_ir_c2_submit_calibration_configuration(o.c2, &c, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(release_family(9U, 1) == 1);
    CHECK(ams_mel_ir_calibration_configuration_request_wait(request, 15000, &result, d, sizeof d, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(strcmp(d, "deferred C2 cleanup failed") == 0);
    CHECK(ams_mel_ir_calibration_configuration_request_wait(request, 0, &result, d, sizeof d, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_calibration_configuration_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    printf("PASS: isolated C2 CalibrationConfigurationCmd deferred-cleanup failure\n");
    return EXIT_SUCCESS; /* graph intentionally retained; pin never released */
}

static void *release_callback(void *unused)
{
    (void)unused;
    if (!ams_mel_test_calibration_callback(2U)) abort();
    return NULL;
}
static int callback_in_flight(void)
{
    struct owners o;
    ams_mel_ir_calibration_configuration_metadata *m = NULL;
    pthread_t release;
    CHECK(open_owner("calibration-in-flight", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_test_calibration_callback(0U));
    CHECK(ams_mel_ir_c2_calibration_configuration_open(o.c2, 1, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_calibration_callback(1U)); // actually inside bridge callback, counted in flight
    CHECK(ams_mel_ir_calibration_configuration_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(pthread_create(&release, NULL, release_callback, NULL) == 0);
    CHECK(close_owner(&o) == EXIT_SUCCESS); // joins callback before physical provider destruction
    CHECK(pthread_join(release, NULL) == 0);
    puts("PASS: Close while calibration bridge callback is counted in flight");
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
    CHECK(optional_and_independent()==EXIT_SUCCESS);
    puts("PASS: CalibrationConfiguration complete nested C11 contracts and mutation controls");
    return EXIT_SUCCESS;
}
