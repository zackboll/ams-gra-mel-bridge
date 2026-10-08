#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d %s: %s\n",__FILE__,__LINE__,#x,diag); exit(1); } } while (0)
#define D diag, sizeof diag, &required
static char diag[512];
static size_t required;
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

static unsigned mock(const char *name)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    unsigned (*fn)(void); CHECK(lib); *(void **)(&fn)=dlsym(lib,name); CHECK(fn);
    unsigned result=fn(); CHECK(dlclose(lib)==0); return result;
}
static void no_queries(void)
{
    CHECK(mock("mock_rf_getter_calls")==0);
    CHECK(mock("mock_rf_quantize_calls")==0);
    CHECK(mock("mock_rf_forbidden_calls")==0);
    const struct {const char *name; unsigned methods;} queries[]={
        {"mock_rf_va_query_calls",6},{"mock_rf_element_calls",10},
        {"mock_rf_connection_calls",5},{"mock_rf_tx_query_calls",4},{"mock_rf_va_lf_calls",4}};
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL); CHECK(lib);
    for(unsigned q=0;q<sizeof queries/sizeof queries[0];++q) {
        unsigned (*fn)(unsigned); *(void **)(&fn)=dlsym(lib,queries[q].name); CHECK(fn);
        for(unsigned i=0;i<queries[q].methods;++i) CHECK(fn(i)==0);
    }
    CHECK(dlclose(lib)==0);
}
static ams_mel_rf_job *claimed(const char *scenario, ams_mel_rf_c2 **c2, ams_mel_rf_virtual_aperture **va)
{
    ams_mel_rf_virtual_aperture_request *vr=NULL;
    ams_mel_rf_virtual_aperture_result_v1 vresult={0};
    ams_mel_rf_job_request *jr=NULL; ams_mel_rf_job *job=NULL;
    ams_mel_rf_job_result_v1 result={0};
    CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER,scenario,c2,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_submit_virtual_aperture(*c2,&va_config,&vr,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_wait(vr,3000,&vresult,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(vr,va,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&vr,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_submit_job(*va,&config,&jr,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_wait(jr,3000,&result,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_claim(jr,&job,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_close(&jr,D)==AMS_MEL_OK);
    return job;
}

static int exact(double a, double b)
{ return isnan(a) ? isnan(b) : a==b && !!signbit(a)==!!signbit(b); }
static void invalid(ams_mel_rf_job *job, ams_mel_rf_job_interval_config_span_v5 span)
{
    unsigned before=mock("mock_rf_job_add_calls");
    CHECK(ams_mel_rf_job_add_rx_intervals_v5(job,span,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(mock("mock_rf_job_add_calls")==before);
}
static void verify(ams_mel_rf_job *job, ams_mel_rf_job_interval_config_span_v5 span)
{
    unsigned before=mock("mock_rf_job_add_calls");
    CHECK(ams_mel_rf_job_add_rx_intervals_v5(job,span,D)==AMS_MEL_OK);
    CHECK(mock("mock_rf_job_add_calls")==before+1);
    CHECK(mock("mock_rf_lf_defaults")==1);
    for(unsigned i=0;i<span.size;++i) for(unsigned e=0;e<span.data[i].receive_events.size;++e) {
        void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
        int64_t (*get_scalar)(unsigned,unsigned,unsigned);
        double (*get_number)(unsigned,unsigned,unsigned,unsigned);
        CHECK(lib);
        *(void **)(&get_scalar)=dlsym(lib,"mock_rf_f6_scalar"); CHECK(get_scalar);
        *(void **)(&get_number)=dlsym(lib,"mock_rf_f6_double"); CHECK(get_number);
        const ams_mel_rf_receive_event_config_v4 *ev=&span.data[i].receive_events.data[e];
        const ams_mel_rf_pulse_detection_settings_v1 zero={0};
        const ams_mel_rf_pulse_detection_settings_v1 *s=ev->has_pulse_detection_settings ? &ev->pulse_detection_settings : &zero;
        CHECK(get_scalar(i,e,0)==s->reference);
        CHECK(get_scalar(i,e,1)==s->leading_edge_m_of_n.m && get_scalar(i,e,2)==s->leading_edge_m_of_n.n);
        CHECK(get_scalar(i,e,3)==s->trailing_edge_m_of_n.m && get_scalar(i,e,4)==s->trailing_edge_m_of_n.n);
        CHECK(get_scalar(i,e,5)==s->min_pulse_width_femtoseconds);
        CHECK(get_scalar(i,e,6)==s->timetag_amplitude_threshold && get_scalar(i,e,7)==(int64_t)s->thresholds.size);
        for(unsigned t=0;t<s->thresholds.size;++t) {
            CHECK(exact(s->thresholds.data[t].leading_edge_db,get_number(i,e,t,0)));
            CHECK(exact(s->thresholds.data[t].trailing_edge_db,get_number(i,e,t,1)));
        }
        CHECK(dlclose(lib)==0);
    }
    no_queries();
}
int main(void)
{
    capabilities[0].uuid[1]=0x80; capabilities[0].uuid[2]=0xff;
    capabilities[0].descriptive_label=(ams_mel_string_view_v1){"first",5};
    capabilities[1].uuid[0]=0xff; capabilities[1].descriptive_label=(ams_mel_string_view_v1){"",0};
    capabilities[2].uuid[15]=0x80; capabilities[2].descriptive_label=(ams_mel_string_view_v1){"µ-third",8};
    ams_mel_rf_pulse_threshold_v1 thresholds[]={{12.25,-3.5},{-0.0,INFINITY},{NAN,-INFINITY},{12.25,-3.5}};
    ams_mel_rf_pulse_threshold_v1 large[8192];
    for(unsigned t=0;t<8192;++t) large[t]=(ams_mel_rf_pulse_threshold_v1){(double)t,-(double)t};
    ams_mel_rf_receive_event_config_v4 events[3]={0};
    events[0].has_pulse_detection_settings=1;
    events[0].pulse_detection_settings=(ams_mel_rf_pulse_detection_settings_v1){1,{3,7},{5,9},-123456789012345LL,1,{thresholds,4}};
    events[1].pulse_detection_settings=(ams_mel_rf_pulse_detection_settings_v1){UINT32_MAX,{255,0},{0,255},INT64_MAX,UINT32_MAX,{(void *)1,SIZE_MAX}};
    events[2].has_pulse_detection_settings=1;
    events[2].pulse_detection_settings=(ams_mel_rf_pulse_detection_settings_v1){2,{255,0},{0,255},INT64_MIN,0,{NULL,0}};
    ams_mel_rf_job_interval_config_v5 intervals[2]={0};
    intervals[0].receive_events=(ams_mel_rf_receive_event_config_span_v4){events,2};
    intervals[1].receive_events=(ams_mel_rf_receive_event_config_span_v4){events+2,1};
    ams_mel_rf_job_interval_config_span_v5 span={intervals,2};
    for(unsigned repeat=0;repeat<50;++repeat) {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job *job=claimed("c2:job-ok",&c2,&va);
        verify(job,span);
        for(unsigned ref=0;ref<3;++ref) for(unsigned tag=0;tag<2;++tag) {
            events[0].pulse_detection_settings.reference=ref;
            events[0].pulse_detection_settings.timetag_amplitude_threshold=tag; verify(job,span);
        }
        uint32_t *fields[]={&events[2].pulse_detection_settings.reference,&events[2].pulse_detection_settings.timetag_amplitude_threshold,&events[2].has_pulse_detection_settings};
        for(unsigned f=0;f<3;++f) {
            uint32_t old=*fields[f]; *fields[f]=(f==0?3:2); invalid(job,span);
            *fields[f]=UINT32_MAX; invalid(job,span); *fields[f]=old;
        }
        events[2].has_pulse_detection_settings=0; verify(job,span); events[2].has_pulse_detection_settings=1;
        const int64_t widths[]={INT64_MIN,-1,0,1,INT64_MAX};
        const uint8_t mn[][2]={{0,0},{1,1},{3,7},{255,255},{255,0}};
        for(unsigned w=0;w<5;++w) {
            events[0].pulse_detection_settings.min_pulse_width_femtoseconds=widths[w];
            events[0].pulse_detection_settings.leading_edge_m_of_n=(ams_mel_rf_pulse_m_of_n_v1){mn[w][0],mn[w][1]};
            verify(job,span);
        }
        const ams_mel_rf_pulse_threshold_span_v1 spans[]={{NULL,0},{thresholds,1},{thresholds,4},{large,8192}};
        for(unsigned t=0;t<4;++t) { events[0].pulse_detection_settings.thresholds=spans[t]; verify(job,span); }
        events[0].pulse_detection_settings.thresholds=(ams_mel_rf_pulse_threshold_span_v1){NULL,1}; invalid(job,span);
        events[0].pulse_detection_settings.thresholds=(ams_mel_rf_pulse_threshold_span_v1){thresholds,SIZE_MAX/sizeof(*thresholds)+1}; invalid(job,span);
        events[0].pulse_detection_settings.thresholds=spans[2];
        unsigned before=mock("mock_rf_job_add_calls");
        CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE","interval-spatial-allocation",1)==0);
        CHECK(ams_mel_rf_job_add_rx_intervals_v5(job,span,D)==AMS_MEL_INTERNAL_ERROR);
        CHECK(mock("mock_rf_job_add_calls")==before); CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE")==0);
        intervals[0].status_enable=AMS_MEL_RF_INTERVAL_STATUS_ALWAYS;
        CHECK(ams_mel_rf_job_add_rx_intervals_v5(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        CHECK(mock("mock_rf_job_add_calls")==before);
        ams_mel_rf_job_interval_status *stream=NULL;
        const ams_mel_rf_job_interval_status_options_v1 options={2,1,0};
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,D)==AMS_MEL_OK);
        verify(job,span); intervals[0].status_enable=AMS_MEL_RF_INTERVAL_STATUS_ON_EXCEPTION; verify(job,span);
        const char *failures[]={"std","unknown","alloc"};
        for(unsigned f=0;f<3;++f) {
            before=mock("mock_rf_job_add_calls"); CHECK(setenv("AMS_MEL_TEST_F4_ADD_FAILURE",failures[f],1)==0);
            CHECK(ams_mel_rf_job_add_rx_intervals_v5(job,span,D)==(f==2?AMS_MEL_INTERNAL_ERROR:AMS_MEL_PROVIDER_EXCEPTION));
            CHECK(mock("mock_rf_job_add_calls")==before+1);
            CHECK(unsetenv("AMS_MEL_TEST_F4_ADD_FAILURE")==0); verify(job,span);
        }
        CHECK(ams_mel_rf_job_interval_status_close(&stream,D)==AMS_MEL_OK);
        before=mock("mock_rf_job_add_calls");
        CHECK(ams_mel_rf_job_add_rx_intervals_v5(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        CHECK(mock("mock_rf_job_add_calls")==before);
        intervals[0].status_enable=0; verify(job,span);
        CHECK(ams_mel_rf_c2_close(&c2,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK);
        verify(job,span);
        if(repeat%2) { CHECK(ams_mel_rf_job_finalize(job,D)==AMS_MEL_OK); ams_mel_rf_job_status_t status=0; CHECK(ams_mel_rf_job_wait_status(job,3000,&status,D)==AMS_MEL_OK); }
        else { ams_mel_rf_job_cancel_result_v1 answer={0}; CHECK(ams_mel_rf_job_cancel(job,&answer,D)==AMS_MEL_OK); }
        before=mock("mock_rf_job_add_calls");
        CHECK(ams_mel_rf_job_add_rx_intervals_v5(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        CHECK(mock("mock_rf_job_add_calls")==before);
        CHECK(ams_mel_rf_job_close(&job,D)==AMS_MEL_OK);
    }
    puts("PASS: native pulse detection focused 50/50"); return 0;
}
