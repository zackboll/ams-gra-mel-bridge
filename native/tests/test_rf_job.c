#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <math.h>
#include <sched.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
static char path[] = "/tmp/ams-rf-job-XXXXXX";
static char text[32768];
static char diag[2048];
static size_t required;
static const char *logfile(void)
{
    FILE *f=fopen(path,"r"); size_t n;
    CHECK(f); n=fread(text,1,sizeof text-1,f); text[n]=0; CHECK(fclose(f)==0); return text;
}
static void reset(void)
{ FILE *f=fopen(path,"w"); CHECK(f); CHECK(fclose(f)==0); }
static unsigned count(const char *event)
{
    unsigned n=0; const char *p=logfile();
    while ((p=strstr(p,event))!=NULL) { ++n; p+=strlen(event); }
    return n;
}
static unsigned mock_call(const char *symbol)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    unsigned (*operation)(void);
    unsigned result;
    CHECK(lib); *(void **)(&operation)=dlsym(lib,symbol); CHECK(operation);
    result=operation(); CHECK(dlclose(lib)==0); return result;
}
static unsigned mock_wait(unsigned baseline)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    unsigned (*operation)(unsigned);
    unsigned result;
    CHECK(lib); *(void **)(&operation)=dlsym(lib,"mock_rf_job_wait_shutdown_after"); CHECK(operation);
    result=operation(baseline); CHECK(dlclose(lib)==0); return result;
}
static const ams_mel_string_view_v1 local[]={{"alpha",5},{"µ-local",8},{"",0}};
static ams_mel_uci_id_v1 capabilities[3];
static const ams_mel_rf_virtual_aperture_config_v1 va_config={
    UINT32_C(0xFEDCBA98),UINT32_C(0x80000001),{local,3},{"definition/β.json",18},{capabilities,3}
};
static const ams_mel_rf_frequency_range_v1 frequencies[]={
    {1000000.25,2000000.5},{987654321.125,987654322.875}
};
static const uint64_t endpoints[]={0,UINT64_C(0x8000000000000000),UINT64_MAX};
static const uint32_t instances[]={0,42,UINT32_MAX};
static const ams_mel_rf_job_request_config_v1 config={
    UINT32_C(0xFEDCBA98),UINT32_C(0x80000001),UINT32_C(0x7FFFFFFE),1,
    {instances,3},{{"rx/µ-main",10},0.625,{frequencies,2},{endpoints,3},{"products/β",11}}
};
static void open_va(const char *scenario, ams_mel_rf_c2 **c2, ams_mel_rf_virtual_aperture **va)
{
    ams_mel_rf_virtual_aperture_request *r=NULL;
    ams_mel_rf_virtual_aperture_result_v1 result={0};
    ams_mel_status_t opened=ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER,scenario,c2,diag,sizeof diag,&required);
    if (opened!=AMS_MEL_OK) fprintf(stderr,"open %s: %d %s\n",scenario,opened,diag);
    CHECK(opened==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_submit_virtual_aperture(*c2,&va_config,&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_wait(r,3000,&result,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(r,va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
}
static ams_mel_rf_job_request *submit(ams_mel_rf_virtual_aperture *va)
{
    ams_mel_rf_job_request *r=NULL;
    CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&config,&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(r); return r;
}
static ams_mel_status_t wait_for(ams_mel_rf_job_request *r, ams_mel_rf_job_result_v1 *result)
{ return ams_mel_rf_job_request_wait(r,3000,result,diag,sizeof diag,&required); }
static void check_snapshot(ams_mel_rf_job *job)
{
    const ams_mel_rf_job_info_v1 *info=NULL;
    CHECK(ams_mel_rf_job_view(job,&info,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(info && info->actual_start_seconds==-123456789 &&
          info->actual_start_femtoseconds==999999999999999LL &&
          info->total_job_duration_femtoseconds==7654321098765LL);
    CHECK(info->va_instance_id==42 && info->va_definition_id==UINT32_C(0xABCDEF01) &&
          info->job_details_id==UINT32_C(0x10203040) &&
          info->job_request_id==UINT32_C(0xFEDCBA98) && info->lookahead_femtoseconds==-12345);
    CHECK(info->rx_stream_ids.size==3 && info->rx_stream_ids.data[0]==0 &&
          info->rx_stream_ids.data[1]==3 && info->rx_stream_ids.data[2]==UINT32_MAX);
}
static void close_all(ams_mel_rf_job_request **r, ams_mel_rf_virtual_aperture **va,
                      ams_mel_rf_c2 **c2)
{
    CHECK(ams_mel_rf_job_request_close(r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_close(va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(c2,diag,sizeof diag,&required)==AMS_MEL_OK);
}
static ams_mel_rf_job *claimed(const char *scenario, ams_mel_rf_c2 **c2,
                              ams_mel_rf_virtual_aperture **va)
{
    ams_mel_rf_job_request *r;
    ams_mel_rf_job *job=NULL;
    ams_mel_rf_job_result_v1 result={0};
    open_va(scenario,c2,va); r=submit(*va);
    CHECK(wait_for(r,&result)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_claim(r,&job,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    return job;
}

static const ams_mel_rf_receive_event_config_v1 rx_events[]={
    {0x80000001U,{"rx/µ-main",10},-123,456789,987654321.125,2000000.5,0x100000007ULL,3,-999},
    {UINT32_MAX,{"β-secondary",12},777,-888,-0.0,INFINITY,0,0x100000009ULL,INT64_MAX-1}
};
static const ams_mel_rf_job_interval_config_v1 rx_intervals[]={
    {AMS_MEL_RF_JOB_INTERVAL_CONTINUE_FROM_PREVIOUS_FS,0x10203040U,-111,9876543210123LL,
     0x100000003ULL,222,-333,1,0x100000005ULL,123456789.25,2500000.5,0xABCDEF01U,{rx_events,2}},
    {-1,42,12,-13,2,-14,15,0,3,NAN,INFINITY,43,{NULL,0}}
};
static const ams_mel_rf_job_interval_config_span_v1 rx_span={rx_intervals,2};
static void interval_tests(void)
{
    CHECK(AMS_MEL_RF_JOB_INTERVAL_CONTINUE_FROM_PREVIOUS_FS==0);
    for (unsigned repeat=0;repeat<50;++repeat) {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job_status_t status=99; ams_mel_rf_job_cancel_result_v1 cancelled={0};
        reset(); ams_mel_rf_job *job=claimed("c2:finalize-cancel",&c2,&va);
        unsigned add=mock_call("mock_rf_job_add_calls"), flush=mock_call("mock_rf_job_flush_calls"),
                 remaining=mock_call("mock_rf_job_remaining_calls");
        CHECK(ams_mel_rf_job_add_rx_intervals(job,rx_span,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(mock_call("mock_rf_job_add_calls")==add+1 && mock_call("mock_rf_job_interval_fidelity")==1);
        CHECK(ams_mel_rf_job_flush(job,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_flush(job,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(mock_call("mock_rf_job_flush_calls")==flush+2);
        CHECK(ams_mel_rf_job_cancel_remaining_intervals(job,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_finalize(job,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_wait_status(job,0,&status,diag,sizeof diag,&required)==AMS_MEL_TIMEOUT);
        CHECK(ams_mel_rf_job_add_rx_intervals(job,rx_span,diag,sizeof diag,&required)==AMS_MEL_PROVIDER_FAILED);
        CHECK(strstr(diag,"Finalize"));
        CHECK(ams_mel_rf_job_flush(job,diag,sizeof diag,&required)==AMS_MEL_PROVIDER_FAILED);
        CHECK(mock_call("mock_rf_job_add_calls")==add+1 && mock_call("mock_rf_job_flush_calls")==flush+2);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(!strstr(logfile(),"rf_c2_shutdown\n") && !strstr(logfile(),"rf_va_destroyed\n"));
        CHECK(ams_mel_rf_job_cancel_remaining_intervals(job,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(mock_call("mock_rf_job_remaining_calls")==remaining+2);
        check_snapshot(job);
        CHECK(ams_mel_rf_job_cancel(job,&cancelled,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_wait_status(job,3000,&status,diag,sizeof diag,&required)==AMS_MEL_OK &&
              status==AMS_MEL_RF_JOB_STATUS_COMPLETE);
        CHECK(ams_mel_rf_job_cancel_remaining_intervals(job,diag,sizeof diag,&required)==AMS_MEL_PROVIDER_FAILED);
        CHECK(mock_call("mock_rf_job_remaining_calls")==remaining+2);
        check_snapshot(job);
        CHECK(ams_mel_rf_job_close(&job,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(count("rf_job_destroyed\n")==1 && count("rf_va_destroyed\n")==1 &&
              count("rf_c2_shutdown\n")==1 && count("library_unloaded\n")==1);
    }
    puts("PASS: native RX JobInterval focused repeat 50/50");
    {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        reset(); ams_mel_rf_job *job=claimed("c2:job-ok",&c2,&va);
        unsigned before=mock_call("mock_rf_job_add_calls");
        CHECK(ams_mel_rf_job_add_rx_intervals(job,(ams_mel_rf_job_interval_config_span_v1){NULL,1},NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT);
        ams_mel_rf_job_interval_config_v1 bad=rx_intervals[0];
        ams_mel_rf_job_interval_config_span_v1 one={&bad,1};
        bad.receive_events.data=NULL;
        CHECK(ams_mel_rf_job_add_rx_intervals(job,one,NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT);
        bad=rx_intervals[0]; bad.phase_coherence_with_prior=2;
        CHECK(ams_mel_rf_job_add_rx_intervals(job,one,NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT);
        ams_mel_rf_receive_event_config_v1 event=rx_events[0];
        bad=rx_intervals[0]; bad.receive_events=(ams_mel_rf_receive_event_config_span_v1){&event,1};
        const ams_mel_string_view_v1 labels[]={{NULL,1},{"\xff",1},{"a\0b",3}};
        for (size_t i=0;i<3;++i) {
            event.element_group_label=labels[i];
            CHECK(ams_mel_rf_job_add_rx_intervals(job,one,NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT);
        }
#if SIZE_MAX < UINT64_MAX
        event=rx_events[0]; event.agc_processing_iterations=UINT64_MAX;
        CHECK(ams_mel_rf_job_add_rx_intervals(job,one,NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT);
        event=rx_events[0]; event.ignored_post_agc_iterations=UINT64_MAX;
        CHECK(ams_mel_rf_job_add_rx_intervals(job,one,NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT);
        event=rx_events[0]; bad.sequence_repeat_count=UINT64_MAX;
        CHECK(ams_mel_rf_job_add_rx_intervals(job,one,NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT);
        bad.sequence_repeat_count=1; bad.iterations_per_signal=UINT64_MAX;
        CHECK(ams_mel_rf_job_add_rx_intervals(job,one,NULL,0,NULL)==AMS_MEL_INVALID_ARGUMENT);
#endif
        CHECK(mock_call("mock_rf_job_add_calls")==before);
        CHECK(ams_mel_rf_job_add_rx_intervals(job,(ams_mel_rf_job_interval_config_span_v1){NULL,0},NULL,0,NULL)==AMS_MEL_OK);
        event=rx_events[0]; event.element_group_label=(ams_mel_string_view_v1){NULL,0};
        bad=rx_intervals[0]; bad.receive_events=(ams_mel_rf_receive_event_config_span_v1){&event,1};
        CHECK(ams_mel_rf_job_add_rx_intervals(job,one,NULL,0,NULL)==AMS_MEL_OK);
        ams_mel_rf_job_interval_config_v1 boundaries[4]={rx_intervals[1],rx_intervals[1],rx_intervals[1],rx_intervals[1]};
        const int64_t starts[]={0,1,-1,INT64_MAX};
        for (size_t i=0;i<4;++i) boundaries[i].interval_start_femtoseconds=starts[i];
        CHECK(ams_mel_rf_job_add_rx_intervals(job,(ams_mel_rf_job_interval_config_span_v1){boundaries,4},NULL,0,NULL)==AMS_MEL_OK);
        CHECK(mock_call("mock_rf_job_start_boundary")==1);
        CHECK(ams_mel_rf_job_close(&job,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,NULL,0,NULL)==AMS_MEL_OK);
    }
    const char *scenarios[]={"c2:add-throw","c2:add-unknown","c2:add-alloc",
        "c2:flush-throw","c2:flush-unknown","c2:flush-alloc",
        "c2:remaining-throw","c2:remaining-unknown","c2:remaining-alloc"};
    for (unsigned i=0;i<9;++i) {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        reset(); ams_mel_rf_job *job=claimed(scenarios[i],&c2,&va);
        const char *counter=i<3?"mock_rf_job_add_calls":i<6?"mock_rf_job_flush_calls":"mock_rf_job_remaining_calls";
        unsigned before=mock_call(counter);
        ams_mel_status_t result=i<3?ams_mel_rf_job_add_rx_intervals(job,rx_span,diag,sizeof diag,&required):
            i<6?ams_mel_rf_job_flush(job,diag,sizeof diag,&required):
                ams_mel_rf_job_cancel_remaining_intervals(job,diag,sizeof diag,&required);
        CHECK(result==(i%3==2?AMS_MEL_INTERNAL_ERROR:AMS_MEL_PROVIDER_EXCEPTION));
        CHECK(mock_call(counter)==before+1);
        check_snapshot(job);
        CHECK(ams_mel_rf_job_close(&job,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,NULL,0,NULL)==AMS_MEL_OK);
    }
    for (unsigned i=0;i<3;++i) {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        reset(); ams_mel_rf_job *job=claimed(i==0?"c2:finalize-complete":i==1?"c2:cancel-throw":"c2:finalize-throw",&c2,&va);
        unsigned add=mock_call("mock_rf_job_add_calls"), flush=mock_call("mock_rf_job_flush_calls"), remaining=mock_call("mock_rf_job_remaining_calls");
        if (i==1) {
            ams_mel_rf_job_cancel_result_v1 result={0};
            CHECK(ams_mel_rf_job_cancel(job,&result,NULL,0,NULL)==AMS_MEL_PROVIDER_EXCEPTION);
        } else {
            ams_mel_rf_job_status_t status=99;
            CHECK(ams_mel_rf_job_finalize(job,NULL,0,NULL)==(i==0?AMS_MEL_OK:AMS_MEL_PROVIDER_EXCEPTION));
            if (i==0) CHECK(ams_mel_rf_job_wait_status(job,3000,&status,NULL,0,NULL)==AMS_MEL_OK);
        }
        CHECK(ams_mel_rf_job_add_rx_intervals(job,rx_span,NULL,0,NULL)==AMS_MEL_PROVIDER_FAILED);
        CHECK(ams_mel_rf_job_flush(job,NULL,0,NULL)==AMS_MEL_PROVIDER_FAILED);
        CHECK(ams_mel_rf_job_cancel_remaining_intervals(job,NULL,0,NULL)==(i==1?AMS_MEL_PROVIDER_FAILED:AMS_MEL_OK));
        CHECK(mock_call("mock_rf_job_add_calls")==add && mock_call("mock_rf_job_flush_calls")==flush);
        CHECK(mock_call("mock_rf_job_remaining_calls")==remaining+(i==1?0U:1U));
        CHECK(ams_mel_rf_job_close(&job,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,NULL,0,NULL)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,NULL,0,NULL)==AMS_MEL_OK);
    }
}

static void lifecycle_case(const char *scenario, ams_mel_status_t start,
                           ams_mel_status_t finish, ams_mel_rf_job_status_t expected)
{
    ams_mel_rf_c2 *c2=NULL;
    ams_mel_rf_virtual_aperture *va=NULL;
    ams_mel_rf_job *job;
    ams_mel_rf_job_status_t status=99;
    unsigned before=mock_call("mock_rf_job_finalize_calls");
    reset(); job=claimed(scenario,&c2,&va);
    CHECK(ams_mel_rf_job_wait_status(job,0,&status,diag,sizeof diag,&required)==AMS_MEL_PROVIDER_FAILED);
    CHECK(strstr(diag,"not been attempted") && status==99);
    CHECK(ams_mel_rf_job_finalize(job,diag,sizeof diag,&required)==start);
    char first[sizeof diag]; strcpy(first,diag);
    CHECK(ams_mel_rf_job_finalize(job,diag,sizeof diag,&required)==start);
    CHECK(strcmp(first,diag)==0 && mock_call("mock_rf_job_finalize_calls")==before+1);
    CHECK(ams_mel_rf_job_wait_status(job,3000,&status,diag,sizeof diag,&required)==finish);
    if (finish==AMS_MEL_OK) CHECK(status==expected);
    else CHECK(status==99);
    strcpy(first,diag);
    CHECK(ams_mel_rf_job_wait_status(job,0,&status,diag,sizeof diag,&required)==finish);
    CHECK(strcmp(first,diag)==0);
    check_snapshot(job);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&c2,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_close(&job,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(count("rf_job_destroyed\n")==1 && count("rf_c2_shutdown\n")==1);
}
static void lifecycle_tests(void)
{
    static const struct { const char *name; ams_mel_rf_job_status_t value; } statuses[]={
        {"c2:finalize-complete",AMS_MEL_RF_JOB_STATUS_COMPLETE},
        {"c2:finalize-none",AMS_MEL_RF_JOB_STATUS_NONE},
        {"c2:finalize-progress",AMS_MEL_RF_JOB_STATUS_IN_PROGRESS},
        {"c2:finalize-invalid-id",AMS_MEL_RF_JOB_STATUS_FAILED_INVALID_ID},
        {"c2:finalize-interrupted",AMS_MEL_RF_JOB_STATUS_FAILED_INTERRUPTED},
        {"c2:finalize-invalid-state",AMS_MEL_RF_JOB_STATUS_FAILED_INVALID_STATE}};
    for (size_t i=0;i<sizeof statuses/sizeof *statuses;++i)
        lifecycle_case(statuses[i].name,AMS_MEL_OK,AMS_MEL_OK,statuses[i].value);
    lifecycle_case("c2:finalize-throw",AMS_MEL_PROVIDER_EXCEPTION,AMS_MEL_PROVIDER_EXCEPTION,0);
    lifecycle_case("c2:finalize-unknown-throw",AMS_MEL_PROVIDER_EXCEPTION,AMS_MEL_PROVIDER_EXCEPTION,0);
    lifecycle_case("c2:finalize-invalid",AMS_MEL_PROVIDER_FAILED,AMS_MEL_PROVIDER_FAILED,0);
    lifecycle_case("c2:finalize-future-throw",AMS_MEL_OK,AMS_MEL_PROVIDER_EXCEPTION,0);
    lifecycle_case("c2:finalize-future-unknown",AMS_MEL_OK,AMS_MEL_PROVIDER_EXCEPTION,0);
    lifecycle_case("c2:finalize-unknown",AMS_MEL_OK,AMS_MEL_PROVIDER_FAILED,0);
    {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job_status_t status=99;
        reset(); ams_mel_rf_job *job=claimed("c2:finalize-future-throw",&c2,&va);
        CHECK(ams_mel_rf_job_finalize(job,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_wait_status(job,3000,&status,diag,sizeof diag,&required)==AMS_MEL_PROVIDER_EXCEPTION);
        CHECK(status==99); check_snapshot(job);
        CHECK(ams_mel_rf_job_close(&job,diag,sizeof diag,&required)==AMS_MEL_OK);
        const char *events=logfile();
        CHECK(strstr(events,"rf_job_destroyed\n") < strstr(events,"rf_va_destroyed\n"));
        CHECK(strstr(events,"rf_va_destroyed\n") < strstr(events,"rf_c2_shutdown\n"));
    }
    const char *cancels[]={"c2:cancel-false","c2:cancel-unknown","c2:cancel-throw",
                           "c2:cancel-unknown-throw"};
    for (size_t i=0;i<sizeof cancels/sizeof *cancels;++i) {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        reset(); ams_mel_rf_job *job=claimed(cancels[i],&c2,&va);
        ams_mel_rf_job_cancel_result_v1 result={77,88};
        unsigned calls=mock_call("mock_rf_job_cancel_calls");
        ams_mel_status_t expected=i==0 ? AMS_MEL_OK : i==1 ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_PROVIDER_EXCEPTION;
        CHECK(ams_mel_rf_job_cancel(job,&result,diag,sizeof diag,&required)==expected);
        char first[sizeof diag]; strcpy(first,diag);
        CHECK(ams_mel_rf_job_cancel(job,&result,diag,sizeof diag,&required)==expected);
        CHECK(strcmp(first,diag)==0 && mock_call("mock_rf_job_cancel_calls")==calls+1);
        if (i==0) CHECK(result.cancelled==0 && result.error_code==AMS_MEL_RF_CANCEL_ERROR_NONE);
        else CHECK(result.cancelled==77 && result.error_code==88);
        calls=mock_call("mock_rf_job_finalize_calls");
        CHECK(ams_mel_rf_job_finalize(job,diag,sizeof diag,&required)==AMS_MEL_PROVIDER_FAILED);
        CHECK(strstr(diag,"cancellation was already attempted"));
        CHECK(mock_call("mock_rf_job_finalize_calls")==calls);
        CHECK(ams_mel_rf_job_close(&job,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,diag,sizeof diag,&required)==AMS_MEL_OK);
    }
    {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job_cancel_result_v1 result={77,88};
        unsigned before=mock_call("mock_rf_job_finalize_calls");
        reset(); ams_mel_rf_job *job=claimed("c2:cancel-success",&c2,&va);
        CHECK(ams_mel_rf_job_cancel(job,&result,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(result.cancelled==1 && result.error_code==AMS_MEL_RF_CANCEL_ERROR_NONE);
        CHECK(ams_mel_rf_job_finalize(job,diag,sizeof diag,&required)==AMS_MEL_PROVIDER_FAILED);
        CHECK(mock_call("mock_rf_job_finalize_calls")==before);
        check_snapshot(job);
        CHECK(ams_mel_rf_job_close(&job,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,diag,sizeof diag,&required)==AMS_MEL_OK);
    }
    for (unsigned repeat=0;repeat<50;++repeat) {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job_status_t status=99;
        ams_mel_rf_job_cancel_result_v1 result={0};
        unsigned f=mock_call("mock_rf_job_finalize_calls"), c=mock_call("mock_rf_job_cancel_calls");
        reset(); ams_mel_rf_job *job=claimed("c2:finalize-cancel",&c2,&va);
        CHECK(ams_mel_rf_job_finalize(job,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_wait_status(job,0,&status,diag,sizeof diag,&required)==AMS_MEL_TIMEOUT && status==99);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(!strstr(logfile(),"rf_c2_shutdown\n") && !strstr(logfile(),"rf_va_destroyed\n"));
        CHECK(ams_mel_rf_job_cancel(job,&result,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(result.cancelled==1 && result.error_code==AMS_MEL_RF_CANCEL_ERROR_NONE);
        CHECK(ams_mel_rf_job_cancel(job,&result,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_finalize(job,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_wait_status(job,3000,&status,diag,sizeof diag,&required)==AMS_MEL_OK &&
              status==AMS_MEL_RF_JOB_STATUS_COMPLETE);
        CHECK(mock_call("mock_rf_job_finalize_calls")==f+1 && mock_call("mock_rf_job_cancel_calls")==c+1);
        check_snapshot(job);
        CHECK(ams_mel_rf_job_close(&job,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(count("rf_job_destroyed\n")==1 && count("rf_va_destroyed\n")==1 &&
              count("rf_c2_shutdown\n")==1 && count("library_unloaded\n")==1);
    }
    /* Abandonment leaves the future, detail, VA, claim and DSO with worker. */
    {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        unsigned shutdown=mock_call("mock_rf_job_shutdown_count");
        reset(); ams_mel_rf_job *job=claimed("c2:finalize-abandon",&c2,&va);
        CHECK(ams_mel_rf_job_finalize(job,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_close(&job,diag,sizeof diag,&required)==AMS_MEL_OK && !job);
        CHECK(!strstr(logfile(),"rf_job_destroyed\n") && !strstr(logfile(),"rf_c2_shutdown\n") &&
              !strstr(logfile(),"library_unloaded\n"));
        CHECK(mock_call("mock_rf_job_resolve_finalize")==1);
        CHECK(mock_wait(shutdown)==1);
        for (unsigned spin=0;spin<1000000 && !strstr(logfile(),"library_unloaded\n");++spin)
            CHECK(sched_yield()==0);
        const char *events=logfile();
        const char *detail=strstr(events,"rf_job_destroyed\n");
        const char *va_end=strstr(events,"rf_va_destroyed\n");
        const char *c2_end=strstr(events,"rf_c2_shutdown\n");
        const char *dso=strstr(events,"library_unloaded\n");
        if (!(detail && va_end && c2_end && dso && detail<va_end && va_end<c2_end && c2_end<dso))
            fprintf(stderr,"abandon log: %s\n",events);
        CHECK(detail && va_end && c2_end && dso && detail<va_end && va_end<c2_end && c2_end<dso);
        CHECK(!strstr(events,"rf_job_cancel\n"));
    }
    {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job_status_t status=99;
        ams_mel_rf_job_cancel_result_v1 result={0};
        reset(); ams_mel_rf_job *job=claimed("c2:finalize-shutdown-throw",&c2,&va);
        CHECK(ams_mel_rf_job_finalize(job,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_cancel(job,&result,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_wait_status(job,3000,&status,diag,sizeof diag,&required)==AMS_MEL_OK &&
              status==AMS_MEL_RF_JOB_STATUS_COMPLETE);
        CHECK(ams_mel_rf_job_close(&job,diag,sizeof diag,&required)==AMS_MEL_PROVIDER_EXCEPTION);
        CHECK(count("rf_c2_shutdown\n")==1 && !strstr(logfile(),"library_unloaded\n"));
    }
}
static void retention_case(const char *failure)
{
    ams_mel_rf_c2 *c2=NULL;
    ams_mel_rf_virtual_aperture *va=NULL;
    ams_mel_rf_job_request *request=NULL;
    if (!strcmp(failure,"finalize-worker-launch")) {
        ams_mel_rf_job *job;
        ams_mel_rf_job_status_t status=99;
        reset(); job=claimed("c2:finalize-delayed",&c2,&va);
        CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE",failure,1)==0);
        unsigned before=mock_call("mock_rf_job_finalize_calls");
        CHECK(ams_mel_rf_job_finalize(job,diag,sizeof diag,&required)==AMS_MEL_INTERNAL_ERROR);
        CHECK(ams_mel_rf_job_finalize(job,diag,sizeof diag,&required)==AMS_MEL_INTERNAL_ERROR);
        CHECK(mock_call("mock_rf_job_finalize_calls")==before+1);
        CHECK(ams_mel_rf_job_wait_status(job,0,&status,diag,sizeof diag,&required)==AMS_MEL_INTERNAL_ERROR && status==99);
        CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE")==0);
        CHECK(ams_mel_rf_job_close(&job,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(!strstr(logfile(),"rf_job_destroyed\n") && !strstr(logfile(),"rf_va_destroyed\n") &&
              !strstr(logfile(),"rf_c2_shutdown\n") && !strstr(logfile(),"library_unloaded\n"));
        return;
    }
    reset(); open_va("c2:job-delayed",&c2,&va);
    CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE",failure,1)==0);
    CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&config,&request,diag,sizeof diag,&required)==
          AMS_MEL_INTERNAL_ERROR && request==NULL);
    CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE")==0);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&c2,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(!strstr(logfile(),"rf_c2_shutdown\n") &&
          !strstr(logfile(),"rf_va_destroyed\n") &&
          !strstr(logfile(),"library_unloaded\n"));
}
static void isolated_retention(const char *self, const char *failure)
{
    pid_t child=fork(); int status;
    CHECK(child>=0);
    if (!child) { execl(self,self,failure,(char *)NULL); _exit(127); }
    CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
}
int main(int argc, char **argv)
{
    int fd=mkstemp(path); CHECK(fd>=0); CHECK(close(fd)==0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG",path,1)==0);
    capabilities[0].uuid[1]=0x80; capabilities[0].uuid[2]=0xff;
    capabilities[0].descriptive_label=(ams_mel_string_view_v1){"first",5};
    capabilities[1].uuid[0]=0xff; capabilities[1].descriptive_label=(ams_mel_string_view_v1){"",0};
    capabilities[2].uuid[15]=0x80; capabilities[2].descriptive_label=(ams_mel_string_view_v1){"µ-third",8};
    if (argc==2) { retention_case(argv[1]); CHECK(unlink(path)==0); return 0; }
    CHECK(argc==1);
    isolated_retention(argv[0],"worker-launch");
    isolated_retention(argv[0],"post-provider-allocation");
    isolated_retention(argv[0],"publication");
    isolated_retention(argv[0],"finalize-worker-launch");
    interval_tests();
    lifecycle_tests();
    ams_mel_rf_c2 *c2=NULL;
    ams_mel_rf_virtual_aperture *va=NULL;
    ams_mel_rf_job_request *r=NULL;
    ams_mel_rf_job *job=NULL;
    ams_mel_rf_job_result_v1 result={99};
    reset(); open_va("c2:job-ok",&c2,&va); r=submit(va);
    CHECK(wait_for(r,&result)==AMS_MEL_OK && result.error_code==AMS_MEL_ERROR_NONE);
    CHECK(wait_for(r,&result)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_claim(r,&job,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_claim(r,&(ams_mel_rf_job *){NULL},diag,sizeof diag,&required)==AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_rf_job_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&c2,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(!strstr(logfile(),"rf_c2_shutdown\n")); check_snapshot(job);
    CHECK(ams_mel_rf_job_close(&job,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(strstr(logfile(),"rf_job_command_created\n") && strstr(logfile(),"rf_job_mode_checked\n"));
    CHECK(count("rf_c2_shutdown\n")==1 && count("rf_job_destroyed\n")==1 &&
          !strstr(logfile(),"rf_forbidden_call\n"));

    reset(); open_va("c2:job-delayed",&c2,&va); r=submit(va);
    result.error_code=77;
    CHECK(ams_mel_rf_job_request_wait(r,0,&result,diag,sizeof diag,&required)==AMS_MEL_TIMEOUT && result.error_code==77);
    CHECK(ams_mel_rf_job_request_wait(r,0,&result,diag,sizeof diag,&required)==AMS_MEL_TIMEOUT);
    CHECK(ams_mel_rf_job_request_claim(r,&job,diag,sizeof diag,&required)==AMS_MEL_TIMEOUT);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&c2,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(!strstr(logfile(),"rf_c2_shutdown\n"));
    CHECK(mock_call("mock_rf_job_release_one")==1);
    CHECK(wait_for(r,&result)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_claim(r,&job,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(!strstr(logfile(),"rf_c2_shutdown\n")); check_snapshot(job);
    CHECK(ams_mel_rf_job_close(&job,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(count("rf_c2_shutdown\n")==1);

    unsigned baseline=mock_call("mock_rf_job_shutdown_count");
    reset(); open_va("c2:job-delayed",&c2,&va); r=submit(va);
    CHECK(ams_mel_rf_job_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&c2,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(mock_call("mock_rf_job_release_one")==1);
    CHECK(mock_wait(baseline)==1);
    CHECK(count("rf_job_destroyed\n")==1 && count("rf_c2_shutdown\n")==1);

    reset(); open_va("c2:job-ok",&c2,&va);
    ams_mel_rf_job_request_config_v1 invalid=config;
    invalid.rx_group.label=(ams_mel_string_view_v1){NULL,1};
    CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&invalid,&r,diag,sizeof diag,&required)==AMS_MEL_INVALID_ARGUMENT);
    invalid=config; invalid.rx_group.label=(ams_mel_string_view_v1){"\xff",1};
    CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&invalid,&r,diag,sizeof diag,&required)==AMS_MEL_INVALID_ARGUMENT);
    invalid=config; invalid.rx_group.label=(ams_mel_string_view_v1){"a\0b",3};
    CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&invalid,&r,diag,sizeof diag,&required)==AMS_MEL_INVALID_ARGUMENT);
    invalid=config; invalid.rx_group.data_pipe_label=(ams_mel_string_view_v1){NULL,1};
    CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&invalid,&r,diag,sizeof diag,&required)==AMS_MEL_INVALID_ARGUMENT);
    invalid=config; invalid.rx_group.data_pipe_label=(ams_mel_string_view_v1){"\xff",1};
    CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&invalid,&r,diag,sizeof diag,&required)==AMS_MEL_INVALID_ARGUMENT);
    invalid=config; invalid.rx_group.data_pipe_label=(ams_mel_string_view_v1){"a\0b",3};
    CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&invalid,&r,diag,sizeof diag,&required)==AMS_MEL_INVALID_ARGUMENT);
    const double bad_duties[]={NAN,INFINITY,-INFINITY,0.0,-0.1,1.01};
    for (size_t i=0;i<sizeof bad_duties/sizeof bad_duties[0];++i) {
        invalid=config; invalid.rx_group.desired_duty_factor=bad_duties[i];
        CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&invalid,&r,diag,sizeof diag,&required)==AMS_MEL_INVALID_ARGUMENT);
    }
    const ams_mel_rf_frequency_range_v1 bad_ranges[]={
        {NAN,1},{1,NAN},{INFINITY,1},{1,-INFINITY},{2,1}
    };
    for (size_t i=0;i<sizeof bad_ranges/sizeof bad_ranges[0];++i) {
        invalid=config; invalid.rx_group.expected_center_frequencies=(ams_mel_rf_frequency_range_span_v1){&bad_ranges[i],1};
        CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&invalid,&r,diag,sizeof diag,&required)==AMS_MEL_INVALID_ARGUMENT);
    }
    const uint64_t duplicate[]={0,0};
    invalid=config; invalid.rx_group.endpoint_ids=(ams_mel_u64_span_v1){duplicate,2};
    CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&invalid,&r,diag,sizeof diag,&required)==AMS_MEL_INVALID_ARGUMENT);
    invalid=config; invalid.rx_group.endpoint_ids=(ams_mel_u64_span_v1){NULL,1};
    CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&invalid,&r,diag,sizeof diag,&required)==AMS_MEL_INVALID_ARGUMENT);
    invalid=config; invalid.rx_group.expected_center_frequencies=
        (ams_mel_rf_frequency_range_span_v1){NULL,1};
    CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&invalid,&r,diag,sizeof diag,&required)==AMS_MEL_INVALID_ARGUMENT);
    invalid=config; invalid.instance_selection=(ams_mel_u32_span_v1){NULL,1};
    CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&invalid,&r,diag,sizeof diag,&required)==AMS_MEL_INVALID_ARGUMENT);
    invalid=config; invalid.is_interruptable=2;
    CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&invalid,&r,diag,sizeof diag,&required)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(!strstr(logfile(),"rf_job_requested\n") && !strstr(logfile(),"rf_job_command_created\n"));
    CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&c2,diag,sizeof diag,&required)==AMS_MEL_OK);

    const struct { const char *scenario; ams_mel_status_t expected; uint32_t code; } failures[]={
        {"c2:job-failure",AMS_MEL_PROVIDER_FAILED,AMS_MEL_ERROR_INVALID_PARAMETERS},
        {"c2:job-unknown-error",AMS_MEL_PROVIDER_FAILED,AMS_MEL_ERROR_NONE},
        {"c2:job-long-failure",AMS_MEL_PROVIDER_FAILED,AMS_MEL_ERROR_INVALID_PARAMETERS},
        {"c2:job-future-throw",AMS_MEL_PROVIDER_EXCEPTION,AMS_MEL_ERROR_NONE},
        {"c2:job-future-unknown",AMS_MEL_PROVIDER_EXCEPTION,AMS_MEL_ERROR_NONE},
        {"c2:job-null",AMS_MEL_PROVIDER_FAILED,AMS_MEL_ERROR_NONE}
    };
    for (size_t i=0;i<sizeof failures/sizeof failures[0];++i) {
        reset(); open_va(failures[i].scenario,&c2,&va); r=submit(va);
        CHECK(wait_for(r,&result)==failures[i].expected && result.error_code==failures[i].code);
        CHECK(wait_for(r,&result)==failures[i].expected);
        CHECK(ams_mel_rf_job_request_claim(r,&job,diag,sizeof diag,&required)==failures[i].expected && !job);
        if (i==2) CHECK(required>512 && strstr(diag,"µ end"));
        close_all(&r,&va,&c2);
    }
    const struct { const char *scenario; ams_mel_status_t expected; } submit_failures[]={
        {"c2:job-invalid-future",AMS_MEL_PROVIDER_FAILED},
        {"c2:job-command-null",AMS_MEL_PROVIDER_FAILED},
        {"c2:job-command-tx",AMS_MEL_PROVIDER_FAILED},
        {"c2:job-command-throw",AMS_MEL_PROVIDER_EXCEPTION},
        {"c2:job-setter-throw",AMS_MEL_PROVIDER_EXCEPTION},
        {"c2:job-submit-throw",AMS_MEL_PROVIDER_EXCEPTION}
    };
    for (size_t i=0;i<sizeof submit_failures/sizeof submit_failures[0];++i) {
        reset(); open_va(submit_failures[i].scenario,&c2,&va);
        CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&config,&r,diag,sizeof diag,&required)==submit_failures[i].expected && !r);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(!strstr(logfile(),"rf_forbidden_call\n"));
    }
    reset(); open_va("c2:job-getter-throw",&c2,&va); r=submit(va);
    CHECK(wait_for(r,&result)==AMS_MEL_OK);
    unsigned before=mock_call("mock_rf_job_get_calls");
    CHECK(ams_mel_rf_job_request_claim(r,&job,diag,sizeof diag,&required)==AMS_MEL_PROVIDER_EXCEPTION && !job);
    CHECK(mock_call("mock_rf_job_get_calls")==before+1);
    CHECK(ams_mel_rf_job_request_claim(r,&job,diag,sizeof diag,&required)==AMS_MEL_PROVIDER_EXCEPTION && !job);
    CHECK(mock_call("mock_rf_job_get_calls")==before+1);
    close_all(&r,&va,&c2);
    CHECK(count("rf_job_destroyed\n")==1);

    reset(); open_va("c2:job-ok",&c2,&va); r=submit(va);
    CHECK(wait_for(r,&result)==AMS_MEL_OK);
    CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE","job-owner",1)==0);
    CHECK(ams_mel_rf_job_request_claim(r,&job,diag,sizeof diag,&required)==AMS_MEL_INTERNAL_ERROR);
    CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE")==0);
    CHECK(ams_mel_rf_job_request_claim(r,&job,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_close(&job,diag,sizeof diag,&required)==AMS_MEL_OK);
    close_all(&r,&va,&c2);

    reset(); open_va("c2:job-ok",&c2,&va); r=submit(va);
    CHECK(wait_for(r,&result)==AMS_MEL_OK);
    CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE","job-snapshot",1)==0);
    CHECK(ams_mel_rf_job_request_claim(r,&job,diag,sizeof diag,&required)==AMS_MEL_INTERNAL_ERROR && !job);
    CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE")==0);
    CHECK(ams_mel_rf_job_request_claim(r,&job,diag,sizeof diag,&required)==AMS_MEL_INTERNAL_ERROR);
    close_all(&r,&va,&c2);

    reset(); open_va("c2:job-delayed",&c2,&va); r=submit(va);
    ams_mel_rf_job_request *r2=submit(va);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&c2,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(mock_call("mock_rf_job_release_one")==1);
    CHECK(wait_for(r2,&result)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_claim(r2,&job,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_close(&r2,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_close(&job,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(!strstr(logfile(),"rf_c2_shutdown\n"));
    CHECK(mock_call("mock_rf_job_release_one")==1);
    CHECK(wait_for(r,&result)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_claim(r,&job,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_close(&job,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(count("rf_c2_shutdown\n")==1);

    reset(); open_va("c2:job-shutdown-throw",&c2,&va); r=submit(va);
    CHECK(mock_call("mock_rf_job_release_one")==1);
    CHECK(wait_for(r,&result)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_claim(r,&job,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&c2,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_close(&job,diag,sizeof diag,&required)==AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(count("rf_c2_shutdown\n")==1);

    CHECK(unlink(path)==0);
    puts("PASS: RF Job C11 ownership and snapshot contract");
    return 0;
}
