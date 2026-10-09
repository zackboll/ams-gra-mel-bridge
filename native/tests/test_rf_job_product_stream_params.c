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

static uint64_t value(unsigned i,unsigned index,unsigned field)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    uint64_t (*fn)(unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f8_value"); CHECK(fn);
    uint64_t result=fn(i,index,field); CHECK(dlclose(lib)==0); return result;
}
static uint64_t lf_value(unsigned field)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    uint64_t (*fn)(unsigned,unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f7_value"); CHECK(fn);
    uint64_t result=fn(0,0,0,field); CHECK(dlclose(lib)==0); return result;
}
static unsigned status_mode(void)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    unsigned (*fn)(unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_status_mode"); CHECK(fn);
    unsigned result=fn(0); CHECK(dlclose(lib)==0); return result;
}
static void invalid(ams_mel_rf_job *job,ams_mel_rf_job_interval_config_span_v7 span)
{
    unsigned before=mock("mock_rf_job_add_calls");
    CHECK(ams_mel_rf_job_add_rx_intervals_v7(job,span,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(mock("mock_rf_job_add_calls")==before);
}
static void verify(ams_mel_rf_job *job,ams_mel_rf_job_interval_config_span_v7 span)
{
    CHECK(ams_mel_rf_job_add_rx_intervals_v7(job,span,D)==AMS_MEL_OK);
    for(unsigned i=0;i<span.size;++i) {
        CHECK(value(i,0,6)==0 && value(i,0,7)==0);
        if(!span.data[i].has_product_stream_params) {
            CHECK(value(i,0,0)==0 && value(i,0,1)==0); continue;
        }
        const ams_mel_rf_product_stream_params_v1 p=span.data[i].product_stream_params;
        CHECK(value(i,0,0)==p.applicable_rx_element_groups.size);
        CHECK(value(i,0,1)==p.endpoints.size);
        for(unsigned g=0;g<p.applicable_rx_element_groups.size;++g)
            CHECK(value(i,g,2)==p.applicable_rx_element_groups.data[g]);
        for(unsigned e=0;e<p.endpoints.size;++e) {
            CHECK(value(i,e,3)==p.endpoints.data[e].endpoint_id);
            CHECK(value(i,e,4)==p.endpoints.data[e].start_address);
            CHECK(value(i,e,5)==p.endpoints.data[e].max_bytes);
        }
    }
    no_queries();
}
int main(void)
{
    capabilities[0].uuid[1]=0x80; capabilities[0].uuid[2]=0xff;
    capabilities[0].descriptive_label=(ams_mel_string_view_v1){"first",5};
    capabilities[1].uuid[0]=0xff; capabilities[2].uuid[15]=0x80;
    capabilities[2].descriptive_label=(ams_mel_string_view_v1){"µ-third",8};
    for(unsigned repeat=0;repeat<50;++repeat) {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job *job=claimed("c2:job-ok",&c2,&va);
        ams_mel_rf_job_interval_config_v1 v1={0};
        ams_mel_rf_job_interval_config_v2 v2={0};
        ams_mel_rf_job_interval_config_v3 v3={0};
        ams_mel_rf_job_interval_config_v4 v4={0};
        ams_mel_rf_job_interval_config_v5 v5={0};
        ams_mel_rf_job_interval_config_v6 v6={0};
        CHECK(ams_mel_rf_job_add_rx_intervals(job,(ams_mel_rf_job_interval_config_span_v1){&v1,1},D)==AMS_MEL_OK);
        CHECK(value(0,0,0)==0 && value(0,0,1)==0);
        CHECK(status_mode()==AMS_MEL_RF_INTERVAL_STATUS_NEVER);
        CHECK(ams_mel_rf_job_add_rx_intervals_v2(job,(ams_mel_rf_job_interval_config_span_v2){&v2,1},D)==AMS_MEL_OK);
        CHECK(value(0,0,0)==0 && value(0,0,1)==0);
        CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,(ams_mel_rf_job_interval_config_span_v3){&v3,1},D)==AMS_MEL_OK);
        CHECK(value(0,0,0)==0 && value(0,0,1)==0);
        CHECK(ams_mel_rf_job_add_rx_intervals_v4(job,(ams_mel_rf_job_interval_config_span_v4){&v4,1},D)==AMS_MEL_OK);
        CHECK(value(0,0,0)==0 && value(0,0,1)==0);
        CHECK(ams_mel_rf_job_add_rx_intervals_v5(job,(ams_mel_rf_job_interval_config_span_v5){&v5,1},D)==AMS_MEL_OK);
        CHECK(value(0,0,0)==0 && value(0,0,1)==0);
        CHECK(ams_mel_rf_job_add_rx_intervals_v6(job,(ams_mel_rf_job_interval_config_span_v6){&v6,1},D)==AMS_MEL_OK);
        CHECK(value(0,0,0)==0 && value(0,0,1)==0);
        uint64_t groups[]={2,0,2,SIZE_MAX};
        ams_mel_rf_product_stream_endpoint_v1 eps[]={
            {0,0,0},{UINT64_MAX,UINT64_C(0x8000000000000000),UINT64_MAX},
            {UINT64_MAX,UINT64_C(0x8000000000000000),UINT64_MAX},
            {UINT64_C(0x0123456789ABCDEF),1,0}};
        ams_mel_rf_job_interval_config_v7 intervals[5]={0};
        ams_mel_rf_lf_address_value_v1 write={UINT64_MAX,UINT64_MAX};
        ams_mel_rf_lf_command_v1 command={UINT32_MAX,SIZE_MAX,{&write,1}};
        intervals[0].interval.local_function_commands=(ams_mel_rf_lf_command_span_v1){&command,1};
        ams_mel_rf_pointing_v1 point={.kind=AMS_MEL_RF_POINTING_FACE_RELATIVE,.face_relative={1.5,-0.5}};
        uint64_t event_groups[]={7,7,1};
        ams_mel_rf_stokes_vector_v1 polarization={1.0,-0.0,2.25,-3.5};
        ams_mel_rf_pulse_threshold_v1 threshold={12.25,-3.5};
        ams_mel_rf_receive_event_config_v4 event={0};
        event.event.event.event.element_group_label=(ams_mel_string_view_v1){"rx/µ-main",10};
        event.event.event.stab_point_index=999;
        event.event.event.applicable_rx_element_groups=(ams_mel_u64_span_v1){event_groups,3};
        event.event.polarization=(ams_mel_rf_stokes_vector_span_v1){&polarization,1};
        event.event.execution_type=AMS_MEL_RF_EXECUTION_CONDITIONAL;
        event.has_pulse_detection_settings=1;
        event.pulse_detection_settings.thresholds=(ams_mel_rf_pulse_threshold_span_v1){&threshold,1};
        intervals[0].interval.interval.receive_events=(ams_mel_rf_receive_event_config_span_v4){&event,1};
        intervals[0].interval.interval.stab_points=(ams_mel_rf_pointing_span_v1){&point,1};
        intervals[0].has_product_stream_params=1;
        intervals[0].product_stream_params=(ams_mel_rf_product_stream_params_v1){{groups,4},{eps,4}};
        intervals[1].product_stream_params=(ams_mel_rf_product_stream_params_v1){{NULL,SIZE_MAX},{NULL,SIZE_MAX}};
        intervals[2].has_product_stream_params=1;
        intervals[3].has_product_stream_params=1;
        intervals[3].product_stream_params.applicable_rx_element_groups=(ams_mel_u64_span_v1){groups,3};
        intervals[4].has_product_stream_params=1;
        intervals[4].product_stream_params.endpoints=(ams_mel_rf_product_stream_endpoint_span_v1){eps,4};
        ams_mel_rf_job_interval_config_span_v7 span={intervals,5};
        verify(job,span);
        CHECK(lf_value(0)==1 && lf_value(1)==UINT32_MAX && lf_value(2)==SIZE_MAX &&
              lf_value(3)==1 && lf_value(4)==UINT64_MAX && lf_value(5)==UINT64_MAX);
        groups[0]=1; eps[1]=(ams_mel_rf_product_stream_endpoint_v1){1,1,1};
        CHECK(value(0,0,2)==2 && value(0,1,3)==UINT64_MAX && value(0,1,4)==UINT64_C(0x8000000000000000) && value(0,1,5)==UINT64_MAX);
        groups[0]=2; eps[1]=eps[2];
        const uint64_t scalars[]={0,1,UINT64_C(0x8000000000000000),UINT64_MAX};
        for(unsigned s=0;s<4;++s) { eps[0]=(ams_mel_rf_product_stream_endpoint_v1){scalars[s],scalars[s],scalars[s]}; verify(job,span); }
        invalid(job,(ams_mel_rf_job_interval_config_span_v7){NULL,1});
        invalid(job,(ams_mel_rf_job_interval_config_span_v7){intervals,SIZE_MAX/sizeof(*intervals)+1});
        CHECK(ams_mel_rf_job_add_rx_intervals_v7(job,(ams_mel_rf_job_interval_config_span_v7){NULL,0},D)==AMS_MEL_OK);
        intervals[2].has_product_stream_params=2; invalid(job,span);
        intervals[2].has_product_stream_params=UINT32_MAX; invalid(job,span);
        intervals[2].has_product_stream_params=1;
        intervals[2].product_stream_params.applicable_rx_element_groups=(ams_mel_u64_span_v1){NULL,1}; invalid(job,span);
        intervals[2].product_stream_params.applicable_rx_element_groups=(ams_mel_u64_span_v1){groups,SIZE_MAX/sizeof(*groups)+1}; invalid(job,span);
        intervals[2].product_stream_params.applicable_rx_element_groups=(ams_mel_u64_span_v1){NULL,0};
        intervals[4].product_stream_params.endpoints=(ams_mel_rf_product_stream_endpoint_span_v1){NULL,1}; invalid(job,span);
        intervals[4].product_stream_params.endpoints=(ams_mel_rf_product_stream_endpoint_span_v1){eps,SIZE_MAX/sizeof(*eps)+1}; invalid(job,span);
        intervals[4].product_stream_params.endpoints=(ams_mel_rf_product_stream_endpoint_span_v1){eps,4};
        intervals[4].interval.local_function_commands=(ams_mel_rf_lf_command_span_v1){NULL,1}; invalid(job,span);
        intervals[4].interval.local_function_commands=(ams_mel_rf_lf_command_span_v1){NULL,0};
        if(sizeof(size_t)<8) { groups[0]=UINT64_MAX; invalid(job,span); groups[0]=2; }
        unsigned before=mock("mock_rf_job_add_calls");
        CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE","interval-product-stream-allocation",1)==0);
        CHECK(ams_mel_rf_job_add_rx_intervals_v7(job,span,D)==AMS_MEL_INTERNAL_ERROR);
        CHECK(mock("mock_rf_job_add_calls")==before); CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE")==0); verify(job,span);
        intervals[0].interval.interval.status_enable=AMS_MEL_RF_INTERVAL_STATUS_ALWAYS;
        CHECK(ams_mel_rf_job_add_rx_intervals_v7(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        ams_mel_rf_job_interval_status *stream=NULL;
        const ams_mel_rf_job_interval_status_options_v1 options={2,1,0};
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,D)==AMS_MEL_OK);
        v2.status_enable=AMS_MEL_RF_INTERVAL_STATUS_ALWAYS;
        v3.status_enable=AMS_MEL_RF_INTERVAL_STATUS_ALWAYS;
        v4.status_enable=AMS_MEL_RF_INTERVAL_STATUS_ALWAYS;
        v5.status_enable=AMS_MEL_RF_INTERVAL_STATUS_ALWAYS;
        v6.interval.status_enable=AMS_MEL_RF_INTERVAL_STATUS_ALWAYS;
        CHECK(ams_mel_rf_job_add_rx_intervals_v2(job,(ams_mel_rf_job_interval_config_span_v2){&v2,1},D)==AMS_MEL_OK);
        CHECK(status_mode()==AMS_MEL_RF_INTERVAL_STATUS_ALWAYS);
        CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,(ams_mel_rf_job_interval_config_span_v3){&v3,1},D)==AMS_MEL_OK);
        CHECK(status_mode()==AMS_MEL_RF_INTERVAL_STATUS_ALWAYS);
        CHECK(ams_mel_rf_job_add_rx_intervals_v4(job,(ams_mel_rf_job_interval_config_span_v4){&v4,1},D)==AMS_MEL_OK);
        CHECK(status_mode()==AMS_MEL_RF_INTERVAL_STATUS_ALWAYS);
        CHECK(ams_mel_rf_job_add_rx_intervals_v5(job,(ams_mel_rf_job_interval_config_span_v5){&v5,1},D)==AMS_MEL_OK);
        CHECK(status_mode()==AMS_MEL_RF_INTERVAL_STATUS_ALWAYS);
        CHECK(ams_mel_rf_job_add_rx_intervals_v6(job,(ams_mel_rf_job_interval_config_span_v6){&v6,1},D)==AMS_MEL_OK);
        CHECK(status_mode()==AMS_MEL_RF_INTERVAL_STATUS_ALWAYS);
        verify(job,span); intervals[0].interval.interval.status_enable=AMS_MEL_RF_INTERVAL_STATUS_ON_EXCEPTION; verify(job,span);
        CHECK(status_mode()==AMS_MEL_RF_INTERVAL_STATUS_ON_EXCEPTION);
        const char *failures[]={"std","unknown","alloc"};
        for(unsigned f=0;f<3;++f) {
            CHECK(setenv("AMS_MEL_TEST_F4_ADD_FAILURE",failures[f],1)==0);
            CHECK(ams_mel_rf_job_add_rx_intervals_v7(job,span,D)==(f==2?AMS_MEL_INTERNAL_ERROR:AMS_MEL_PROVIDER_EXCEPTION));
            CHECK(unsetenv("AMS_MEL_TEST_F4_ADD_FAILURE")==0); verify(job,span);
        }
        CHECK(ams_mel_rf_job_interval_status_close(&stream,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_add_rx_intervals_v7(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        intervals[0].interval.interval.status_enable=0;
        CHECK(ams_mel_rf_c2_close(&c2,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK); verify(job,span);
        if(repeat%2) { CHECK(ams_mel_rf_job_finalize(job,D)==AMS_MEL_OK); ams_mel_rf_job_status_t status=0; CHECK(ams_mel_rf_job_wait_status(job,3000,&status,D)==AMS_MEL_OK); }
        else { ams_mel_rf_job_cancel_result_v1 answer={0}; CHECK(ams_mel_rf_job_cancel(job,&answer,D)==AMS_MEL_OK); }
        CHECK(ams_mel_rf_job_add_rx_intervals_v7(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        CHECK(ams_mel_rf_job_close(&job,D)==AMS_MEL_OK);
    }
    puts("PASS: native product stream focused 50/50"); return 0;
}

