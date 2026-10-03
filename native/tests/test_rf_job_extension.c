#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
extern size_t ams_mel_test_rf_status_last(ams_mel_rf_job_interval_status_counters_v1 *);
static unsigned (*calls)(void), (*wait_callback)(void), (*shutdowns)(void);
static unsigned (*hold)(unsigned), (*wait_shutdown)(unsigned);
static unsigned (*at)(unsigned,uint32_t *,uint32_t *,int64_t *);
static unsigned (*destructions)(void), (*wait_destroy)(unsigned);
static char lifetime[]="/tmp/ams-rf-extension-XXXXXX";
static void graph_order(void)
{
    char data[65536]; FILE *file=fopen(lifetime,"r"); CHECK(file);
    size_t length=fread(data,1,sizeof data-1,file); data[length]=0; CHECK(fclose(file)==0);
    char *j=strstr(data,"rf_job_destroyed\n"),*v=strstr(data,"rf_va_destroyed\n"),*s=strstr(data,"rf_c2_shutdown\n"),*d=strstr(data,"rf_c2_destroyed\n");
    CHECK(j && v && s && d && j<v && v<s && s<d && !strstr(data,"rf_forbidden_call\n"));
}
static void snapshot(ams_mel_rf_job *job)
{
    const ams_mel_rf_job_info_v1 *v=NULL;
    CHECK(ams_mel_rf_job_view(job,&v,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(v->actual_start_seconds==-123456789 && v->actual_start_femtoseconds==999999999999999LL);
    CHECK(v->total_job_duration_femtoseconds==7654321098765LL && v->lookahead_femtoseconds==-12345);
    CHECK(v->va_instance_id==42 && v->va_definition_id==0xabcdef01U && v->job_details_id==0x10203040U && v->job_request_id==0xfedcba98U);
    CHECK(v->rx_stream_ids.size==3 && v->rx_stream_ids.data[0]==0 && v->rx_stream_ids.data[1]==3 && v->rx_stream_ids.data[2]==UINT32_MAX);
}
static ams_mel_rf_job *open_job(const char *scenario, ams_mel_rf_c2 **c2, ams_mel_rf_virtual_aperture **va)
{
    ams_mel_rf_virtual_aperture_request *vr=NULL;
    ams_mel_rf_job_request *jr=NULL;
    ams_mel_rf_job *job=NULL;
    ams_mel_rf_virtual_aperture_result_v1 vresult={0};
    ams_mel_rf_job_result_v1 jresult={0};
    const ams_mel_string_view_v1 local[]={{"alpha",5},{"µ-local",8},{"",0}};
    ams_mel_uci_id_v1 caps[3]={0};
    caps[0].uuid[1]=0x80; caps[0].uuid[2]=0xff; caps[1].uuid[0]=0xff; caps[2].uuid[15]=0x80;
    caps[0].descriptive_label=(ams_mel_string_view_v1){"first",5};
    caps[2].descriptive_label=(ams_mel_string_view_v1){"µ-third",8};
    const ams_mel_rf_virtual_aperture_config_v1 vc={0xfedcba98U,0x80000001U,{local,3},{"definition/β.json",18},{caps,3}};
    const ams_mel_rf_frequency_range_v1 frequencies[]={{1000000.25,2000000.5},{987654321.125,987654322.875}};
    const uint64_t endpoints[]={0,UINT64_C(0x8000000000000000),UINT64_MAX};
    const uint32_t instances[]={0,42,UINT32_MAX};
    const ams_mel_rf_job_request_config_v1 jc={0xfedcba98U,0x80000001U,0x7ffffffeU,1,{instances,3},{{"rx/µ-main",10},0.625,{frequencies,2},{endpoints,3},{"products/β",11}}};
    CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER,scenario,c2,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_submit_virtual_aperture(*c2,&vc,&vr,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_wait(vr,3000,&vresult,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(vr,va,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&vr,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_submit_job(*va,&jc,&jr,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_wait(jr,3000,&jresult,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_claim(jr,&job,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_close(&jr,NULL,0,NULL)==AMS_MEL_OK);
    return job;
}
static void parents(ams_mel_rf_c2 **c2, ams_mel_rf_virtual_aperture **va)
{
    CHECK(ams_mel_rf_virtual_aperture_close(va,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(c2,NULL,0,NULL)==AMS_MEL_OK);
}
static void recorded(unsigned index,uint32_t interval,uint32_t event,int64_t duration)
{
    uint32_t i=0,e=0; int64_t d=0;
    CHECK(calls()==index+1 && at(index,&i,&e,&d)==1);
    CHECK(i==interval && e==event && d==duration);
}
static void extend(ams_mel_rf_job *job,uint32_t interval,uint32_t event,int64_t duration)
{
    unsigned before=calls();
    CHECK(ams_mel_rf_job_extend_event(job,interval,event,duration,NULL,0,NULL)==AMS_MEL_OK);
    recorded(before,interval,event,duration); snapshot(job);
}
static const ams_mel_rf_job_interval_status_v1 *view(ams_mel_rf_job_interval_status_event *event,unsigned sequence)
{
    const ams_mel_rf_job_interval_status_v1 *v=NULL;
    CHECK(ams_mel_rf_job_interval_status_event_view(event,&v,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(v->interval_id==0xfedcba98U && v->completion_status==AMS_MEL_RF_INTERVAL_COMPLETION_STARTED);
    CHECK(v->event_log.size==1 && v->event_log.data[0].event_id==0x80000001U && v->event_log.data[0].trigger==AMS_MEL_RF_LOG_TRIGGER_EVENT_EXTENDED);
    CHECK(v->event_log.data[0].time_seconds==1234567890+(int64_t)sequence && v->event_log.data[0].time_fractional_femtoseconds==987654321000+(int64_t)sequence);
    return v;
}
struct Invocation { ams_mel_rf_job *job; _Atomic unsigned returned; };
static void *invoke(void *argument)
{
    struct Invocation *v=argument;
    CHECK(ams_mel_rf_job_extend_event(v->job,0xfedcba98U,0x80000001U,123456789,NULL,0,NULL)==AMS_MEL_OK);
    atomic_store(&v->returned,1); return NULL;
}
int main(void)
{
    int fd=mkstemp(lifetime); CHECK(fd>=0 && close(fd)==0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG",lifetime,1)==0);
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL); CHECK(lib);
#define LOAD(name,symbol) do { *(void **)(&name)=dlsym(lib,symbol); CHECK(name); } while (0)
    LOAD(calls,"mock_rf_extension_count"); LOAD(at,"mock_rf_extension_at");
    LOAD(hold,"mock_rf_extension_hold"); LOAD(wait_callback,"mock_rf_extension_wait");
    LOAD(shutdowns,"mock_rf_job_shutdown_count"); LOAD(wait_shutdown,"mock_rf_job_wait_shutdown_after");
    LOAD(destructions,"mock_rf_extension_destructions"); LOAD(wait_destroy,"mock_rf_extension_wait_destroy");
    CHECK(ams_mel_rf_job_extend_event(NULL,0,0,0,NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT && calls()==0);
    for (unsigned round=0;round<50;++round) {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job *job=open_job("c2:job-ok",&c2,&va);
        const int64_t durations[]={0,1,-1,123456789,INT64_MIN,INT64_MAX};
        unsigned before=calls();
        CHECK(ams_mel_rf_job_extend_event(job,0,0,0,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT && calls()==before);
        for (size_t i=0;i<6;++i) extend(job,0xfedcba98U,0x80000001U,durations[i]);
        extend(job,0,UINT32_MAX,1); extend(job,UINT32_MAX,0,-1);
        extend(job,17,42,123456789); extend(job,17,42,123456789);
        CHECK(ams_mel_rf_job_finalize(job,NULL,0,NULL)==AMS_MEL_OK);
        ams_mel_rf_job_status_t status=99;
        CHECK(ams_mel_rf_job_wait_status(job,3000,&status,NULL,0,NULL)==AMS_MEL_OK && status==AMS_MEL_RF_JOB_STATUS_COMPLETE);
        extend(job,17,42,1);
        CHECK(ams_mel_rf_job_cancel_remaining_intervals(job,NULL,0,NULL)==AMS_MEL_OK);
        parents(&c2,&va); extend(job,17,42,1);
        CHECK(ams_mel_rf_job_close(&job,NULL,0,NULL)==AMS_MEL_OK);

        /* Full submitted-interval -> command -> provider callback path. */
        FILE *log=fopen(lifetime,"w"); CHECK(log && fclose(log)==0);
        job=open_job(round%2 ? "c2:extension-reference" : "c2:extension-feedback",&c2,&va);
        ams_mel_rf_job_interval_status *stream=NULL;
        const ams_mel_rf_job_interval_status_options_v1 options={2,1,0};
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,NULL,0,NULL)==AMS_MEL_OK);
        ams_mel_rf_receive_event_config_v1 rx={0}; rx.event_id=0x80000001U;
        rx.element_group_label=(ams_mel_string_view_v1){"rx/µ-main",10};
        rx.duration_femtoseconds=1000000000; rx.max_extension_femtoseconds=500000000;
        ams_mel_rf_job_interval_config_v2 config={0}; config.interval.interval_id=0xfedcba98U;
        config.interval.sequence_duration_femtoseconds=2000000000;
        config.interval.receive_events=(ams_mel_rf_receive_event_config_span_v1){&rx,1};
        config.status_enable=AMS_MEL_RF_INTERVAL_STATUS_ALWAYS;
        CHECK(ams_mel_rf_job_add_rx_intervals_v2(job,(ams_mel_rf_job_interval_config_span_v2){&config,1},NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_flush(job,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_finalize(job,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_wait_status(job,0,&status,NULL,0,NULL)==AMS_MEL_TIMEOUT);
        parents(&c2,&va);
        struct Invocation invocation={job,0}; pthread_t thread;
        before=calls(); CHECK(hold(1)==1 && pthread_create(&thread,NULL,invoke,&invocation)==0);
        CHECK(wait_callback()==1 && atomic_load(&invocation.returned)==0);
        ams_mel_rf_job_interval_status_event *first=NULL,*second=NULL;
        CHECK(ams_mel_rf_job_interval_status_receive(stream,0,&first,NULL,0,NULL)==AMS_MEL_OK);
        view(first,1); recorded(before,0xfedcba98U,0x80000001U,123456789);
        CHECK(hold(0)==1 && pthread_join(thread,NULL)==0 && atomic_load(&invocation.returned)==1);
        extend(job,0xfedcba98U,0x80000001U,123456789);
        CHECK(ams_mel_rf_job_interval_status_receive(stream,0,&second,NULL,0,NULL)==AMS_MEL_OK);
        view(first,1); view(second,2);
        CHECK(ams_mel_rf_job_interval_status_event_close(&second,NULL,0,NULL)==AMS_MEL_OK);
        if (round%2) {
            CHECK(ams_mel_rf_job_interval_status_close(&stream,NULL,0,NULL)==AMS_MEL_OK);
            extend(job,0xfedcba98U,0x80000001U,1);
            ams_mel_rf_job_interval_status_counters_v1 counters;
            CHECK(ams_mel_test_rf_status_last(&counters)==0 && counters.callbacks_after_close==1);
        }
        ams_mel_rf_job_cancel_result_v1 cancel={0};
        CHECK(ams_mel_rf_job_cancel(job,&cancel,NULL,0,NULL)==AMS_MEL_OK && cancel.cancelled==1);
        CHECK(ams_mel_rf_job_wait_status(job,3000,&status,NULL,0,NULL)==AMS_MEL_OK && status==AMS_MEL_RF_JOB_STATUS_COMPLETE);
        before=calls();
        CHECK(ams_mel_rf_job_extend_event(job,0,0,0,NULL,0,NULL)==AMS_MEL_PROVIDER_FAILED && calls()==before);
        snapshot(job); unsigned baseline=shutdowns(), destroyed=destructions();
        CHECK(ams_mel_rf_job_close(&job,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(wait_shutdown(baseline)==1);
        CHECK(wait_destroy(destroyed)==1);
        graph_order();
        if (stream) {
            CHECK(ams_mel_rf_job_interval_status_receive(stream,0,&second,NULL,0,NULL)==AMS_MEL_STREAM_STOPPED);
            CHECK(ams_mel_rf_job_interval_status_close(&stream,NULL,0,NULL)==AMS_MEL_OK);
        }
        view(first,1); CHECK(ams_mel_rf_job_interval_status_event_close(&first,NULL,0,NULL)==AMS_MEL_OK);
    }
    const char *scenarios[]={"c2:extend-standard","c2:extend-unknown","c2:extend-alloc","c2:extend-throw","c2:cancel-false","c2:cancel-throw"};
    for (size_t i=0;i<6;++i) {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job *job=open_job(scenarios[i],&c2,&va);
        char diagnostic[512]; size_t required=0; unsigned before=calls();
        if (i<4) {
            CHECK(ams_mel_rf_job_extend_event(job,UINT32_MAX,0,INT64_MIN,diagnostic,sizeof diagnostic,&required)==(i==2 ? AMS_MEL_INTERNAL_ERROR : AMS_MEL_PROVIDER_EXCEPTION));
            recorded(before,UINT32_MAX,0,INT64_MIN); snapshot(job);
            CHECK(diagnostic[0]!=0);
            if (i==3) { CHECK(required==1205 && strlen(diagnostic)==510); for (size_t b=0;b<510;b+=2) CHECK((unsigned char)diagnostic[b]==0xc2 && (unsigned char)diagnostic[b+1]==0xb5); }
            /* An explicit retry is another attempt, never implicit/cached. */
            CHECK(ams_mel_rf_job_extend_event(job,0,0,0,NULL,0,&required)==(i==2 ? AMS_MEL_INTERNAL_ERROR : AMS_MEL_PROVIDER_EXCEPTION));
            CHECK(calls()==before+2);
            CHECK(ams_mel_rf_job_finalize(job,NULL,0,NULL)==AMS_MEL_OK);
            ams_mel_rf_job_status_t status=99;
            CHECK(ams_mel_rf_job_wait_status(job,3000,&status,NULL,0,NULL)==AMS_MEL_OK && status==AMS_MEL_RF_JOB_STATUS_COMPLETE);
        } else {
            ams_mel_rf_job_cancel_result_v1 cancel={0};
            CHECK(ams_mel_rf_job_cancel(job,&cancel,NULL,0,NULL)==(i==5 ? AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_OK));
            if (i==4) CHECK(cancel.cancelled==0 && cancel.error_code==AMS_MEL_RF_CANCEL_ERROR_NONE);
            CHECK(ams_mel_rf_job_extend_event(job,0,0,0,diagnostic,sizeof diagnostic,&required)==AMS_MEL_PROVIDER_FAILED);
            CHECK(strstr(diagnostic,"Cancel") && calls()==before); snapshot(job);
        }
        parents(&c2,&va); CHECK(ams_mel_rf_job_close(&job,NULL,0,NULL)==AMS_MEL_OK);
    }
    /* No registration is required even in the active feedback provider. */
    ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
    ams_mel_rf_job *job=open_job("c2:extension-feedback",&c2,&va);
    CHECK(ams_mel_rf_job_finalize(job,NULL,0,NULL)==AMS_MEL_OK);
    extend(job,0xfedcba98U,0x80000001U,1);
    ams_mel_rf_job_cancel_result_v1 cancel={0}; ams_mel_rf_job_status_t status=99;
    CHECK(ams_mel_rf_job_cancel(job,&cancel,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_wait_status(job,3000,&status,NULL,0,NULL)==AMS_MEL_OK);
    parents(&c2,&va); CHECK(ams_mel_rf_job_close(&job,NULL,0,NULL)==AMS_MEL_OK);
    graph_order();
    CHECK(dlclose(lib)==0 && unlink(lifetime)==0);
    puts("PASS: native RF event extension active/feedback focused repeat 50/50"); return 0;
}
