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

static uint64_t observed(unsigned i,unsigned c,unsigned w,unsigned field)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    uint64_t (*fn)(unsigned,unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f7_value"); CHECK(fn);
    uint64_t result=fn(i,c,w,field); CHECK(dlclose(lib)==0); return result;
}
static void invalid(ams_mel_rf_job *job,ams_mel_rf_job_interval_config_span_v6 span)
{
    unsigned before=mock("mock_rf_job_add_calls");
    CHECK(ams_mel_rf_job_add_rx_intervals_v6(job,span,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(mock("mock_rf_job_add_calls")==before);
}
static void verify(ams_mel_rf_job *job,ams_mel_rf_job_interval_config_span_v6 span)
{
    CHECK(ams_mel_rf_job_add_rx_intervals_v6(job,span,D)==AMS_MEL_OK);
    for(unsigned i=0;i<span.size;++i) {
        const ams_mel_rf_lf_command_span_v1 commands=span.data[i].local_function_commands;
        CHECK(observed(i,0,0,0)==commands.size);
        for(unsigned c=0;c<commands.size;++c) {
            CHECK(observed(i,c,0,1)==commands.data[c].local_function_type_id);
            CHECK(observed(i,c,0,2)==commands.data[c].local_function_instance);
            CHECK(observed(i,c,0,3)==commands.data[c].address_values.size);
            for(unsigned w=0;w<commands.data[c].address_values.size;++w) {
                CHECK(observed(i,c,w,4)==commands.data[c].address_values.data[w].address);
                CHECK(observed(i,c,w,5)==commands.data[c].address_values.data[w].value);
            }
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
        ams_mel_rf_lf_address_value_v1 writes[]={{0,0},{UINT64_MAX,UINT64_MAX},
            {UINT64_C(0x8000000000000000),UINT64_C(0x0123456789ABCDEF)},
            {0,UINT64_C(0xFFFFFFFF00000000)}};
        ams_mel_rf_lf_address_value_v1 duplicates[]={{0,7},{0,7}},last[]={{42,99}};
        ams_mel_rf_lf_command_v1 commands[]={
            {UINT32_C(0x80000001),sizeof(size_t)==8?UINT64_C(0x100000003):3,{writes,4}},
            {UINT32_C(0x80000001),sizeof(size_t)==8?UINT64_C(0x100000003):3,{duplicates,2}},
            {UINT32_MAX,SIZE_MAX,{NULL,0}}};
        ams_mel_rf_lf_command_v1 final={0,0,{last,1}};
        ams_mel_rf_job_interval_config_v6 intervals[3]={0};
        intervals[0].local_function_commands=(ams_mel_rf_lf_command_span_v1){commands,3};
        intervals[2].local_function_commands=(ams_mel_rf_lf_command_span_v1){&final,1};
        ams_mel_rf_job_interval_config_span_v6 span={intervals,3};
        CHECK(ams_mel_rf_job_add_rx_intervals_v6(job,(ams_mel_rf_job_interval_config_span_v6){NULL,0},D)==AMS_MEL_OK);
        verify(job,span);
        writes[1].value=1; commands[0].local_function_type_id=1;
        CHECK(observed(0,0,1,5)==UINT64_MAX && observed(0,0,0,1)==UINT32_C(0x80000001));
        writes[1].value=UINT64_MAX;
        const uint32_t types[]={0,1,UINT32_C(0x80000000),UINT32_MAX};
        for(unsigned t=0;t<4;++t) { commands[0].local_function_type_id=types[t]; verify(job,span); }
        commands[0].local_function_instance=1; verify(job,span);
        commands[0].local_function_instance=SIZE_MAX; verify(job,span);
        writes[0]=(ams_mel_rf_lf_address_value_v1){1,1}; verify(job,span);
        writes[0]=(ams_mel_rf_lf_address_value_v1){0,0};
        commands[0].address_values=(ams_mel_rf_lf_address_value_span_v1){writes,1}; verify(job,span);
        commands[0].address_values=(ams_mel_rf_lf_address_value_span_v1){writes,4};
        ams_mel_rf_lf_command_span_v1 saved=intervals[0].local_function_commands;
        intervals[0].local_function_commands=(ams_mel_rf_lf_command_span_v1){commands,1}; verify(job,span);
        intervals[0].local_function_commands=(ams_mel_rf_lf_command_span_v1){NULL,0}; verify(job,span);
        intervals[0].local_function_commands=saved;
        if(sizeof(size_t)<8) { commands[0].local_function_instance=UINT64_MAX; invalid(job,span); commands[0].local_function_instance=SIZE_MAX; }
        invalid(job,(ams_mel_rf_job_interval_config_span_v6){NULL,1});
        invalid(job,(ams_mel_rf_job_interval_config_span_v6){intervals,SIZE_MAX/sizeof(*intervals)+1});
        final.address_values=(ams_mel_rf_lf_address_value_span_v1){NULL,1}; invalid(job,span);
        final.address_values=(ams_mel_rf_lf_address_value_span_v1){last,SIZE_MAX/sizeof(*last)+1}; invalid(job,span);
        final.address_values=(ams_mel_rf_lf_address_value_span_v1){last,1};
        intervals[2].local_function_commands=(ams_mel_rf_lf_command_span_v1){NULL,1}; invalid(job,span);
        intervals[2].local_function_commands=(ams_mel_rf_lf_command_span_v1){&final,SIZE_MAX/sizeof(final)+1}; invalid(job,span);
        intervals[2].local_function_commands=(ams_mel_rf_lf_command_span_v1){&final,1};
        unsigned before=mock("mock_rf_job_add_calls");
        CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE","interval-lf-allocation",1)==0);
        CHECK(ams_mel_rf_job_add_rx_intervals_v6(job,span,D)==AMS_MEL_INTERNAL_ERROR);
        CHECK(mock("mock_rf_job_add_calls")==before); CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE")==0);
        intervals[0].interval.status_enable=AMS_MEL_RF_INTERVAL_STATUS_ALWAYS;
        CHECK(ams_mel_rf_job_add_rx_intervals_v6(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        CHECK(mock("mock_rf_job_add_calls")==before);
        ams_mel_rf_job_interval_status *stream=NULL;
        const ams_mel_rf_job_interval_status_options_v1 options={2,1,0};
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,D)==AMS_MEL_OK);
        verify(job,span); intervals[0].interval.status_enable=AMS_MEL_RF_INTERVAL_STATUS_ON_EXCEPTION; verify(job,span);
        const char *failures[]={"std","unknown","alloc"};
        for(unsigned f=0;f<3;++f) {
            before=mock("mock_rf_job_add_calls"); CHECK(setenv("AMS_MEL_TEST_F4_ADD_FAILURE",failures[f],1)==0);
            CHECK(ams_mel_rf_job_add_rx_intervals_v6(job,span,D)==(f==2?AMS_MEL_INTERNAL_ERROR:AMS_MEL_PROVIDER_EXCEPTION));
            CHECK(mock("mock_rf_job_add_calls")==before+1);
            CHECK(unsetenv("AMS_MEL_TEST_F4_ADD_FAILURE")==0); verify(job,span);
        }
        CHECK(ams_mel_rf_job_interval_status_close(&stream,D)==AMS_MEL_OK);
        before=mock("mock_rf_job_add_calls");
        CHECK(ams_mel_rf_job_add_rx_intervals_v6(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        CHECK(mock("mock_rf_job_add_calls")==before);
        intervals[0].interval.status_enable=0; verify(job,span);
        CHECK(ams_mel_rf_c2_close(&c2,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK);
        verify(job,span);
        if(repeat%2) { CHECK(ams_mel_rf_job_finalize(job,D)==AMS_MEL_OK); ams_mel_rf_job_status_t status=0; CHECK(ams_mel_rf_job_wait_status(job,3000,&status,D)==AMS_MEL_OK); }
        else { ams_mel_rf_job_cancel_result_v1 answer={0}; CHECK(ams_mel_rf_job_cancel(job,&answer,D)==AMS_MEL_OK); }
        before=mock("mock_rf_job_add_calls");
        CHECK(ams_mel_rf_job_add_rx_intervals_v6(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        CHECK(mock("mock_rf_job_add_calls")==before);
        CHECK(ams_mel_rf_job_close(&job,D)==AMS_MEL_OK);
    }
    puts("PASS: native LF commands focused 50/50"); return 0;
}
