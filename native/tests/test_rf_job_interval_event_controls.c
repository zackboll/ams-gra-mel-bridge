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
static uint64_t scalar(unsigned i, unsigned e, unsigned f)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    uint64_t (*fn)(unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f5_scalar"); CHECK(fn);
    uint64_t result=fn(i,e,f); CHECK(dlclose(lib)==0); return result;
}
static double number(unsigned i, unsigned e, unsigned v, unsigned c)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    double (*fn)(unsigned,unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f5_double"); CHECK(fn);
    double result=fn(i,e,v,c); CHECK(dlclose(lib)==0); return result;
}
static unsigned byte(unsigned i, unsigned b)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    unsigned (*fn)(unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f5_byte"); CHECK(fn);
    unsigned result=fn(i,b); CHECK(dlclose(lib)==0); return result;
}
static int exact(double a, double b)
{ return isnan(a) ? isnan(b) : a==b && !!signbit(a)==!!signbit(b); }
static void invalid(ams_mel_rf_job *job, ams_mel_rf_job_interval_config_span_v4 span)
{
    unsigned before=mock("mock_rf_job_add_calls");
    CHECK(ams_mel_rf_job_add_rx_intervals_v4(job,span,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(mock("mock_rf_job_add_calls")==before);
}
static void verify(ams_mel_rf_job *job, ams_mel_rf_job_interval_config_span_v4 span)
{
    unsigned before=mock("mock_rf_job_add_calls");
    CHECK(ams_mel_rf_job_add_rx_intervals_v4(job,span,D)==AMS_MEL_OK);
    CHECK(mock("mock_rf_job_add_calls")==before+1);
    for(unsigned i=0;i<span.size;++i) {
        const ams_mel_rf_job_interval_config_v4 *in=&span.data[i];
        CHECK(scalar(i,0,0)==in->tx_power_mode_id && scalar(i,0,1)==in->execution_type);
        CHECK(scalar(i,0,2)==in->activity_id.size && scalar(i,0,3)==in->stab_points.size);
        for(unsigned b=0;b<in->activity_id.size;++b) CHECK(byte(i,b)==in->activity_id.data[b]);
        for(unsigned e=0;e<in->receive_events.size;++e) {
            const ams_mel_rf_receive_event_config_v3 *ev=&in->receive_events.data[e];
            CHECK(scalar(i,e,4)==ev->polarization.size);
            CHECK(scalar(i,e,5)==ev->polarization_beam_steer_correction);
            CHECK(scalar(i,e,6)==ev->execution_type && scalar(i,e,7)==ev->termination_type);
            CHECK(scalar(i,e,8)==ev->allow_delay_start);
            CHECK(scalar(i,e,9)==ev->iteration_hold_count && scalar(i,e,10)==ev->iteration_termination_count);
            CHECK(scalar(i,e,11)==ev->channelization_enabled);
            CHECK(scalar(i,e,12)==ev->event.stab_point_index);
            CHECK(scalar(i,e,13)==ev->event.applicable_rx_element_groups.size && scalar(i,e,14)==1);
            CHECK(exact(ev->phase_offset_rad,number(i,e,0,4)));
            for(unsigned p=0;p<ev->polarization.size;++p) {
                const ams_mel_rf_stokes_vector_v1 *v=&ev->polarization.data[p];
                const double components[]={v->s0,v->s1,v->s2,v->s3};
                for(unsigned c=0;c<4;++c) CHECK(exact(components[c],number(i,e,p,c)));
            }
        }
    }
    no_queries();
}
int main(void)
{
    capabilities[0].uuid[1]=0x80; capabilities[0].uuid[2]=0xff;
    capabilities[0].descriptive_label=(ams_mel_string_view_v1){"first",5};
    capabilities[1].uuid[0]=0xff; capabilities[1].descriptive_label=(ams_mel_string_view_v1){"",0};
    capabilities[2].uuid[15]=0x80; capabilities[2].descriptive_label=(ams_mel_string_view_v1){"µ-third",8};
    ams_mel_rf_stokes_vector_v1 stokes[]={{1,-0.0,INFINITY,NAN},{-INFINITY,2.25,-3.5,4.75},{0,0,0,0}};
    uint8_t activity[]={0,0xff,0x80,0,0x7f}, large[8192];
    for(unsigned i=0;i<sizeof large;++i) large[i]=(uint8_t)i;
    uint64_t groups[]={2,0,2};
    ams_mel_rf_pointing_v1 point={.kind=AMS_MEL_RF_POINTING_FACE_RELATIVE,.face_relative={1.5,-0.5}};
    ams_mel_rf_receive_event_config_v3 events[2]={0};
    events[0].event.event.element_group_label=(ams_mel_string_view_v1){"rx/µ-main",10};
    events[0].event.stab_point_index=999;
    events[0].event.applicable_rx_element_groups=(ams_mel_u64_span_v1){groups,3};
    events[0].polarization=(ams_mel_rf_stokes_vector_span_v1){stokes,2};
    events[0].polarization_beam_steer_correction=1; events[0].phase_offset_rad=-0.0;
    events[0].execution_type=1; events[0].termination_type=1; events[0].allow_delay_start=1;
    events[0].iteration_hold_count=(uint64_t)SIZE_MAX;
    events[0].iteration_termination_count=(uint64_t)SIZE_MAX-1; events[0].channelization_enabled=1;
    ams_mel_rf_job_interval_config_v4 intervals[2]={0};
    intervals[0].sequence_repeat_count=1;
    intervals[0].stab_points=(ams_mel_rf_pointing_span_v1){&point,1};
    intervals[0].receive_events=(ams_mel_rf_receive_event_config_span_v3){events,2};
    intervals[0].tx_power_mode_id=0xDEADBEEFU; intervals[0].activity_id=(ams_mel_u8_span_v1){activity,sizeof activity};
    intervals[0].execution_type=1;
    intervals[1].tx_power_mode_id=UINT32_MAX; intervals[1].activity_id=(ams_mel_u8_span_v1){large,sizeof large};
    ams_mel_rf_job_interval_config_span_v4 span={intervals,2};
    for(unsigned repeat=0;repeat<50;++repeat) {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job *job=claimed("c2:job-ok",&c2,&va);
        verify(job,span);
        for(unsigned size=0;size<3;++size) { events[0].polarization.size=size; verify(job,span); }
        events[0].polarization.size=3; invalid(job,span); events[0].polarization.size=2;
        const ams_mel_rf_stokes_vector_v1 *saved=events[0].polarization.data;
        events[0].polarization.data=NULL; invalid(job,span); events[0].polarization.data=saved;
        stokes[1]=stokes[0]; verify(job,span); stokes[1]=(ams_mel_rf_stokes_vector_v1){-INFINITY,2.25,-3.5,4.75};
        uint32_t *fields[]={&events[0].polarization_beam_steer_correction,&events[0].allow_delay_start,
            &events[0].channelization_enabled,&events[0].execution_type,&events[0].termination_type,&intervals[0].execution_type};
        for(unsigned f=0;f<sizeof fields/sizeof fields[0];++f) {
            uint32_t old=*fields[f];
            *fields[f]=0; verify(job,span); *fields[f]=1; verify(job,span);
            *fields[f]=2; invalid(job,span); *fields[f]=UINT32_MAX; invalid(job,span); *fields[f]=old;
        }
        const uint64_t counts[]={0,1,UINT64_C(0x8000000000000000),UINT64_MAX};
        for(unsigned c=0;c<4;++c) {
            events[0].iteration_hold_count=events[0].iteration_termination_count=counts[c];
            if(counts[c]>(uint64_t)SIZE_MAX) invalid(job,span); else verify(job,span);
        }
        events[0].iteration_hold_count=(uint64_t)SIZE_MAX; events[0].iteration_termination_count=(uint64_t)SIZE_MAX-1;
        const double phases[]={0,-0.0,INFINITY,-INFINITY,NAN,123.75};
        for(unsigned p=0;p<6;++p) { events[0].phase_offset_rad=phases[p]; verify(job,span); }
        events[0].phase_offset_rad=-0.0;
        events[0].execution_type=0; events[0].termination_type=1; verify(job,span);
        events[0].execution_type=1; events[0].termination_type=0; verify(job,span); events[0].termination_type=1;
        const uint32_t powers[]={0,1,0x80000000U,UINT32_MAX};
        for(unsigned p=0;p<4;++p) { intervals[0].tx_power_mode_id=powers[p]; verify(job,span); }
        intervals[0].tx_power_mode_id=0xDEADBEEFU;
        intervals[0].activity_id.data=NULL; invalid(job,span); intervals[0].activity_id.data=activity;
        invalid(job,(ams_mel_rf_job_interval_config_span_v4){NULL,1});
        intervals[0].status_enable=AMS_MEL_RF_INTERVAL_STATUS_ALWAYS;
        unsigned before=mock("mock_rf_job_add_calls");
        CHECK(ams_mel_rf_job_add_rx_intervals_v4(job,span,D)==AMS_MEL_PROVIDER_FAILED && mock("mock_rf_job_add_calls")==before);
        ams_mel_rf_job_interval_status *stream=NULL;
        const ams_mel_rf_job_interval_status_options_v1 options={2,1,0};
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,D)==AMS_MEL_OK);
        verify(job,span); intervals[0].status_enable=AMS_MEL_RF_INTERVAL_STATUS_ON_EXCEPTION; verify(job,span);
        const char *failures[]={"std","unknown","alloc"};
        for(unsigned f=0;f<3;++f) {
            before=mock("mock_rf_job_add_calls"); CHECK(setenv("AMS_MEL_TEST_F4_ADD_FAILURE",failures[f],1)==0);
            CHECK(ams_mel_rf_job_add_rx_intervals_v4(job,span,D)==(f==2 ? AMS_MEL_INTERNAL_ERROR : AMS_MEL_PROVIDER_EXCEPTION));
            CHECK(mock("mock_rf_job_add_calls")==before+1);
            CHECK(unsetenv("AMS_MEL_TEST_F4_ADD_FAILURE")==0); verify(job,span);
        }
        CHECK(ams_mel_rf_job_interval_status_close(&stream,D)==AMS_MEL_OK);
        before=mock("mock_rf_job_add_calls");
        CHECK(ams_mel_rf_job_add_rx_intervals_v4(job,span,D)==AMS_MEL_PROVIDER_FAILED && mock("mock_rf_job_add_calls")==before);
        intervals[0].status_enable=AMS_MEL_RF_INTERVAL_STATUS_NEVER; verify(job,span);
        if(repeat%2) { CHECK(ams_mel_rf_job_finalize(job,D)==AMS_MEL_OK); ams_mel_rf_job_status_t status=0; CHECK(ams_mel_rf_job_wait_status(job,3000,&status,D)==AMS_MEL_OK); }
        else { ams_mel_rf_job_cancel_result_v1 answer={0}; CHECK(ams_mel_rf_job_cancel(job,&answer,D)==AMS_MEL_OK); }
        before=mock("mock_rf_job_add_calls");
        CHECK(ams_mel_rf_job_add_rx_intervals_v4(job,span,D)==AMS_MEL_PROVIDER_FAILED && mock("mock_rf_job_add_calls")==before);
        CHECK(ams_mel_rf_job_close(&job,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,D)==AMS_MEL_OK);
    }
    puts("PASS: native RX event controls focused 50/50"); return 0;
}
