#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
extern void ams_mel_test_rf_status_hold(unsigned, int, int);
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
static unsigned (*emit)(unsigned,unsigned,unsigned,unsigned);
static unsigned (*registrations)(void);
static unsigned (*mode)(unsigned);
static unsigned (*start_boundary)(void);
static unsigned (*shutdowns)(void);
static unsigned (*resolve_finalize)(void);
static unsigned (*wait_shutdown)(unsigned);
static char lifetime_path[]="/tmp/ams-rf-status-XXXXXX";
static int has_lifetime(const char *event)
{
    char data[32768]; FILE *file=fopen(lifetime_path,"r"); CHECK(file);
    size_t size=fread(data,1,sizeof data-1,file); data[size]=0;
    CHECK(fclose(file)==0); return strstr(data,event)!=NULL;
}
static void create_job(const char *scenario, ams_mel_rf_job **job)
{
    ams_mel_rf_c2 *c2=NULL;
    ams_mel_rf_virtual_aperture *va=NULL;
    ams_mel_rf_virtual_aperture_request *vr=NULL;
    ams_mel_rf_job_request *jr=NULL;
    ams_mel_rf_virtual_aperture_result_v1 vresult={0};
    ams_mel_rf_job_result_v1 jresult={0};
    const ams_mel_string_view_v1 local[]={{"alpha",5},{"µ-local",8},{"",0}};
    ams_mel_uci_id_v1 capabilities[3]={0};
    capabilities[0].uuid[1]=0x80; capabilities[0].uuid[2]=0xff;
    capabilities[1].uuid[0]=0xff; capabilities[2].uuid[15]=0x80;
    capabilities[0].descriptive_label=(ams_mel_string_view_v1){"first",5};
    capabilities[2].descriptive_label=(ams_mel_string_view_v1){"µ-third",8};
    ams_mel_rf_virtual_aperture_config_v1 vc={0xfedcba98U,0x80000001U,
        {local,3},{"definition/β.json",18},{capabilities,3}};
    const ams_mel_rf_frequency_range_v1 frequencies[]={{1000000.25,2000000.5},{987654321.125,987654322.875}};
    const uint64_t endpoints[]={0,UINT64_C(0x8000000000000000),UINT64_MAX};
    const uint32_t instances[]={0,42,UINT32_MAX};
    ams_mel_rf_job_request_config_v1 jc={0xfedcba98U,0x80000001U,0x7ffffffeU,1,
        {instances,3},{{"rx/µ-main",10},0.625,{frequencies,2},{endpoints,3},{"products/β",11}}};
    CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER,scenario,&c2,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_submit_virtual_aperture(c2,&vc,&vr,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_wait(vr,3000,&vresult,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(vr,&va,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&vr,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&jc,&jr,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_wait(jr,3000,&jresult,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_claim(jr,job,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_close(&jr,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&c2,NULL,0,NULL)==AMS_MEL_OK);
}
static const ams_mel_rf_job_interval_status_v1 *view(ams_mel_rf_job_interval_status_event *event)
{
    const ams_mel_rf_job_interval_status_v1 *result=NULL;
    CHECK(ams_mel_rf_job_interval_status_event_view(event,&result,NULL,0,NULL)==AMS_MEL_OK);
    return result;
}
static void rich(const ams_mel_rf_job_interval_status_v1 *v)
{
    CHECK(v->interval_id==0xfedcba98U && v->completion_status==2);
    CHECK(v->event_log.size==3 && v->activity_id.size==37);
    CHECK(v->event_log.data[0].event_id==0 && v->event_log.data[1].event_id==17 && v->event_log.data[2].event_id==UINT32_MAX);
    CHECK(v->event_log.data[0].trigger==1 && v->event_log.data[1].trigger==2 && v->event_log.data[2].trigger==7);
    CHECK(v->event_log.data[0].time_seconds==-124 && v->event_log.data[0].time_fractional_femtoseconds==112);
    CHECK(v->event_log.data[1].time_seconds==-125 && v->event_log.data[1].time_fractional_femtoseconds==113);
    CHECK(v->event_log.data[2].time_seconds==-123 && v->event_log.data[2].time_fractional_femtoseconds==111);
    for (size_t i=0;i<37;++i) CHECK(v->activity_id.data[i]==(i==1?0x80:i==2?0xff:i));
}
static void *thread_emit(void *unused)
{ (void)unused; CHECK(emit(2,7,0,0xfedcba98U)==1); return NULL; }
struct waiter { ams_mel_rf_job_interval_status *stream; ams_mel_status_t result; };
static void *thread_receive(void *arg)
{
    struct waiter *w=arg;
    ams_mel_rf_job_interval_status_event *event=NULL;
    w->result=ams_mel_rf_job_interval_status_receive(w->stream,3000,&event,NULL,0,NULL);
    CHECK(!event); return NULL;
}
int main(void)
{
    int log=mkstemp(lifetime_path); CHECK(log>=0 && close(log)==0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG",lifetime_path,1)==0);
    void *provider=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    CHECK(provider);
    *(void **)(&emit)=dlsym(provider,"mock_rf_status_emit");
    *(void **)(&registrations)=dlsym(provider,"mock_rf_status_registrations");
    *(void **)(&mode)=dlsym(provider,"mock_rf_status_mode");
    *(void **)(&start_boundary)=dlsym(provider,"mock_rf_job_start_boundary");
    *(void **)(&shutdowns)=dlsym(provider,"mock_rf_job_shutdown_count");
    *(void **)(&resolve_finalize)=dlsym(provider,"mock_rf_job_resolve_finalize");
    *(void **)(&wait_shutdown)=dlsym(provider,"mock_rf_job_wait_shutdown_after");
    CHECK(emit && registrations && mode && start_boundary && shutdowns && resolve_finalize && wait_shutdown);
    /* No registration yet: old unload behavior is unchanged. Reload controls
     * after this unload before keeping function pointers for registered cases. */
    CHECK(dlclose(provider)==0);
    CHECK(has_lifetime("library_unloaded\n"));
    provider=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL); CHECK(provider);
    *(void **)(&emit)=dlsym(provider,"mock_rf_status_emit");
    *(void **)(&registrations)=dlsym(provider,"mock_rf_status_registrations");
    *(void **)(&mode)=dlsym(provider,"mock_rf_status_mode");
    *(void **)(&start_boundary)=dlsym(provider,"mock_rf_job_start_boundary");
    *(void **)(&shutdowns)=dlsym(provider,"mock_rf_job_shutdown_count");
    *(void **)(&resolve_finalize)=dlsym(provider,"mock_rf_job_resolve_finalize");
    *(void **)(&wait_shutdown)=dlsym(provider,"mock_rf_job_wait_shutdown_after");
    for (unsigned run=0;run<50;++run) {
        ams_mel_rf_job *job=NULL;
        ams_mel_rf_job_interval_status *stream=NULL,*second=NULL;
        ams_mel_rf_job_interval_status_event *held=NULL,*event=NULL;
        ams_mel_rf_job_interval_status_options_v1 options={2,3,37};
        ams_mel_rf_job_interval_status_counters_v1 counters={0};
        create_job(run%2?"c2:status-reference":"c2:status-sync",&job);
        unsigned before=registrations();
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(registrations()==before+1);
        if (run==0) {
            CHECK(dlclose(provider)==0);
            FILE *file=fopen(lifetime_path,"w"); CHECK(file && fclose(file)==0);
        }
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&second,NULL,0,NULL)==AMS_MEL_PROVIDER_FAILED);
        CHECK(!second && registrations()==before+1);
        if (run%2) CHECK(emit(2,7,0,0xfedcba98U)==1);
        CHECK(ams_mel_rf_job_interval_status_receive(stream,0,&held,NULL,0,NULL)==AMS_MEL_OK);
        rich(view(held));
        ams_mel_rf_job_interval_config_v2 configs[4]={0};
        const int64_t starts[]={0,1,-1,INT64_MAX};
        for (unsigned i=0;i<4;++i) { configs[i].interval.interval_start_femtoseconds=starts[i]; configs[i].status_enable=i%3; }
        CHECK(ams_mel_rf_job_add_rx_intervals_v2(job,(ams_mel_rf_job_interval_config_span_v2){configs,4},NULL,0,NULL)==AMS_MEL_OK);
        for (unsigned i=0;i<4;++i) CHECK(mode(i)==i%3);
        CHECK(start_boundary()==1);
        for (unsigned i=0;i<25;++i) {
            CHECK(emit(i,i%8,1,i)==1);
            CHECK(ams_mel_rf_job_interval_status_receive(stream,0,&event,NULL,0,NULL)==AMS_MEL_OK);
            CHECK(view(event)->completion_status==i && !view(event)->activity_id.size && !view(event)->event_log.size);
            CHECK(ams_mel_rf_job_interval_status_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);
        }
        for (unsigned i=0;i<8;++i) {
            CHECK(emit(2,i,0,i)==1);
            CHECK(ams_mel_rf_job_interval_status_receive(stream,0,&event,NULL,0,NULL)==AMS_MEL_OK);
            CHECK(view(event)->event_log.data[2].trigger==i);
            CHECK(ams_mel_rf_job_interval_status_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);
        }
        CHECK(emit(99,7,0,0)==1 && emit(2,99,0,0)==1);
        CHECK(emit(2,7,2,0)==1 && emit(2,7,3,0)==1);
        CHECK(setenv("AMS_MEL_TEST_RF_STATUS_FAILURE","activity-allocation",1)==0);
        CHECK(emit(2,7,0,0)==1);
        CHECK(unsetenv("AMS_MEL_TEST_RF_STATUS_FAILURE")==0);
        CHECK(emit(2,7,1,101)==1 && emit(2,7,1,102)==1 && emit(2,7,1,103)==1);
        CHECK(ams_mel_rf_job_interval_status_receive(stream,0,NULL,NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_job_interval_status_receive(stream,0,&event,NULL,0,NULL)==AMS_MEL_OK && view(event)->interval_id==101);
        CHECK(ams_mel_rf_job_interval_status_receive(stream,0,&event,NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_job_interval_status_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_interval_status_receive(stream,0,&event,NULL,0,NULL)==AMS_MEL_OK && view(event)->interval_id==102);
        CHECK(ams_mel_rf_job_interval_status_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_interval_status_receive(stream,1,&event,NULL,0,NULL)==AMS_MEL_TIMEOUT && !event);
        CHECK(ams_mel_rf_job_interval_status_get_counters(stream,&counters,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(counters.callback_entries==42 && counters.events_queued==36 && counters.events_delivered==36);
        CHECK(counters.queue_full_drops==1 && counters.malformed_drops==2 && counters.oversize_drops==2 && counters.allocation_failures==1);
        pthread_t worker;
        CHECK(pthread_create(&worker,NULL,thread_emit,NULL)==0 && pthread_join(worker,NULL)==0);
        CHECK(ams_mel_rf_job_interval_status_receive(stream,0,&event,NULL,0,NULL)==AMS_MEL_OK);
        rich(view(event));
        CHECK(ams_mel_rf_job_interval_status_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);
        struct waiter waiting={stream,AMS_MEL_INTERNAL_ERROR};
        CHECK(pthread_create(&worker,NULL,thread_receive,&waiting)==0);
        CHECK(ams_mel_rf_job_close(&job,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(has_lifetime("rf_job_destroyed\n") && has_lifetime("rf_va_destroyed\n") &&
              has_lifetime("rf_c2_shutdown\n") && !has_lifetime("library_unloaded\n"));
        CHECK(pthread_join(worker,NULL)==0 && waiting.result==AMS_MEL_STREAM_STOPPED);
        CHECK(emit(2,7,0,0)==1);
        CHECK(ams_mel_rf_job_interval_status_get_counters(stream,&counters,NULL,0,NULL)==AMS_MEL_OK && counters.callbacks_after_close==1);
        CHECK(ams_mel_rf_job_interval_status_close(&stream,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(emit(2,7,0,0)==1);
        rich(view(held));
        CHECK(ams_mel_rf_job_interval_status_event_close(&held,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_interval_status_event_close(&held,NULL,0,NULL)==AMS_MEL_OK);
    }
    {
        ams_mel_rf_job *job=NULL;
        ams_mel_rf_job_interval_status *stream=NULL;
        ams_mel_rf_job_interval_status_event *event=NULL;
        const ams_mel_rf_job_interval_status_options_v1 options={1,3,37};
        create_job("c2:finalize-delayed",&job);
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,NULL,0,NULL)==AMS_MEL_OK);
        unsigned before=shutdowns();
        CHECK(ams_mel_rf_job_finalize(job,NULL,0,NULL)==AMS_MEL_OK);
        struct timespec start,end;
        CHECK(clock_gettime(CLOCK_MONOTONIC,&start)==0);
        CHECK(ams_mel_rf_job_close(&job,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(clock_gettime(CLOCK_MONOTONIC,&end)==0);
        CHECK((end.tv_sec-start.tv_sec)*1000000000L+end.tv_nsec-start.tv_nsec<500000000L);
        CHECK(ams_mel_rf_job_interval_status_receive(stream,0,&event,NULL,0,NULL)==AMS_MEL_STREAM_STOPPED);
        CHECK(emit(2,7,0,0)==1 && shutdowns()==before);
        CHECK(resolve_finalize()==1 && wait_shutdown(before)==1);
        CHECK(ams_mel_rf_job_interval_status_close(&stream,NULL,0,NULL)==AMS_MEL_OK);
    }
    for (unsigned stage=1;stage<=2;++stage) {
        ams_mel_rf_job *job=NULL;
        ams_mel_rf_job_interval_status *stream=NULL;
        ams_mel_rf_job_interval_status_event *event=NULL;
        const ams_mel_rf_job_interval_status_options_v1 options={1,3,37};
        int admitted[2],release[2]; char byte;
        pthread_t worker;
        CHECK(pipe(admitted)==0 && pipe(release)==0);
        create_job("c2:status-reference",&job);
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,NULL,0,NULL)==AMS_MEL_OK);
        ams_mel_test_rf_status_hold(stage,admitted[1],release[0]);
        CHECK(pthread_create(&worker,NULL,thread_emit,NULL)==0);
        CHECK(read(admitted[0],&byte,1)==1);
        if (stage==1) CHECK(ams_mel_rf_job_interval_status_close(&stream,NULL,0,NULL)==AMS_MEL_OK);
        else CHECK(ams_mel_rf_job_close(&job,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(write(release[1],"x",1)==1 && pthread_join(worker,NULL)==0);
        if (stream) {
            ams_mel_rf_job_interval_status_counters_v1 counters;
            CHECK(ams_mel_rf_job_interval_status_receive(stream,0,&event,NULL,0,NULL)==AMS_MEL_STREAM_STOPPED);
            CHECK(ams_mel_rf_job_interval_status_get_counters(stream,&counters,NULL,0,NULL)==AMS_MEL_OK);
            CHECK(counters.events_queued==0 && counters.callbacks_after_close==1);
            CHECK(ams_mel_rf_job_interval_status_close(&stream,NULL,0,NULL)==AMS_MEL_OK);
        }
        if (job) {
            ams_mel_rf_job_cancel_result_v1 cancel={0};
            CHECK(ams_mel_rf_job_finalize(job,NULL,0,NULL)==AMS_MEL_OK);
            CHECK(ams_mel_rf_job_cancel(job,&cancel,NULL,0,NULL)==AMS_MEL_OK);
            CHECK(ams_mel_rf_job_close(&job,NULL,0,NULL)==AMS_MEL_OK);
        }
        CHECK(close(admitted[0])==0 && close(admitted[1])==0 && close(release[0])==0 && close(release[1])==0);
    }
    {
        ams_mel_rf_job *job=NULL;
        ams_mel_rf_job_interval_status *stream=NULL,*second=NULL;
        ams_mel_rf_job_interval_status_event *event=NULL;
        ams_mel_rf_job_interval_status_options_v1 options={0,3,37};
        create_job("c2:status-reference",&job);
        unsigned before=registrations();
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT);
        options.queue_capacity=SIZE_MAX;
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT);
        options=(ams_mel_rf_job_interval_status_options_v1){1,0,0};
        CHECK(setenv("AMS_MEL_TEST_RF_STATUS_FAILURE","pre-registration",1)==0);
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,NULL,0,NULL)==AMS_MEL_INTERNAL_ERROR);
        CHECK(registrations()==before && !stream);
        CHECK(unsetenv("AMS_MEL_TEST_RF_STATUS_FAILURE")==0);
        ams_mel_rf_job_interval_config_v2 config={0};
        config.status_enable=1;
        CHECK(ams_mel_rf_job_add_rx_intervals_v2(job,(ams_mel_rf_job_interval_config_span_v2){&config,1},NULL,0,NULL)==AMS_MEL_PROVIDER_FAILED);
        config.status_enable=99;
        CHECK(ams_mel_rf_job_add_rx_intervals_v2(job,(ams_mel_rf_job_interval_config_span_v2){&config,1},NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(emit(2,7,0,0)==1 && emit(0,0,1,0)==1);
        CHECK(ams_mel_rf_job_interval_status_receive(stream,0,&event,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(!view(event)->activity_id.size && !view(event)->event_log.size);
        CHECK(ams_mel_rf_job_interval_status_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(emit(0,0,1,0)==1);
        CHECK(ams_mel_rf_job_interval_status_close(&stream,NULL,0,NULL)==AMS_MEL_OK);
        config.status_enable=1;
        CHECK(ams_mel_rf_job_add_rx_intervals_v2(job,(ams_mel_rf_job_interval_config_span_v2){&config,1},NULL,0,NULL)==AMS_MEL_PROVIDER_FAILED);
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&second,NULL,0,NULL)==AMS_MEL_PROVIDER_FAILED);
        CHECK(registrations()==before+1);
        CHECK(ams_mel_rf_job_close(&job,NULL,0,NULL)==AMS_MEL_OK);
    }
    for (unsigned cancelled=0;cancelled<=1;++cancelled) {
        ams_mel_rf_job *job=NULL;
        ams_mel_rf_job_interval_status *stream=NULL;
        const ams_mel_rf_job_interval_status_options_v1 options={1,3,37};
        create_job("c2:job-ok",&job);
        unsigned before=registrations();
        if (cancelled) {
            ams_mel_rf_job_cancel_result_v1 cancel;
            CHECK(ams_mel_rf_job_cancel(job,&cancel,NULL,0,NULL)==AMS_MEL_OK);
        } else CHECK(ams_mel_rf_job_finalize(job,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,NULL,0,NULL)==AMS_MEL_PROVIDER_FAILED);
        CHECK(registrations()==before && !stream);
        CHECK(ams_mel_rf_job_close(&job,NULL,0,NULL)==AMS_MEL_OK);
    }
    {
        ams_mel_rf_job *job=NULL;
        ams_mel_rf_job_interval_status *stream=NULL;
        const ams_mel_rf_job_interval_status_options_v1 options={1,3,37};
        create_job("c2:status-throw",&job);
        unsigned before=registrations();
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,NULL,0,NULL)==AMS_MEL_PROVIDER_EXCEPTION);
        CHECK(!stream && registrations()==before+1);
        CHECK(emit(2,7,0,0)==1);
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,NULL,0,NULL)==AMS_MEL_PROVIDER_FAILED);
        CHECK(registrations()==before+1);
        ams_mel_rf_job_cancel_result_v1 cancel={0};
        CHECK(ams_mel_rf_job_cancel(job,&cancel,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_close(&job,NULL,0,NULL)==AMS_MEL_OK);
    }
    puts("PASS: native RF interval status focused repeat 50/50");
    CHECK(!has_lifetime("rf_forbidden_call\n"));
    CHECK(unlink(lifetime_path)==0);
    return 0;
}