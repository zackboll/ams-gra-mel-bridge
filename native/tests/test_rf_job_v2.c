#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d %s: %s\n",__FILE__,__LINE__,#x,diag); exit(1); } } while (0)
#define D diag, sizeof diag, &required
static char diag[512], log_path[]="/tmp/ams-f1-XXXXXX", log_text[32768];
static size_t required;
static const ams_mel_string_view_v1 local[]={{"alpha",5},{"µ-local",8},{"",0}};
static ams_mel_uci_id_v1 caps[3];
static const ams_mel_rf_frequency_range_v1 ranges[]={{1000000.25,2000000.5},{987654321.125,987654322.875}}, final_range={-1.25,3.5};
static const uint64_t endpoints[]={0,UINT64_C(0x8000000000000000),UINT64_MAX}, secondary[]={7,42}, nine[]={9};
static const uint32_t instances[]={42,0,42,UINT32_MAX}, modes[]={UINT32_MAX,7,7,0x80000000U};
static const uint8_t capability[]={0,0xff,0x80,0,7}, activity[]={0xde,0xad,0,0xbe,0xef};
static const ams_mel_rf_rx_data_pipe_endpoint_config_v1 pipes[]={
    {{"products/β",11},{endpoints,3}},{{"secondary",9},{secondary,2}},{{"products/β",11},{nine,1}}
};
static const ams_mel_rf_rx_data_pipe_endpoint_config_v1 default_pipe={{"default",7},{nine,1}};
static const ams_mel_rf_rx_element_group_config_v2 groups[]={
    {{"rx/α",5},0.625,{ranges,2},{pipes,2}},
    {{"",0},0.5,{NULL,0},{&default_pipe,1}},
    {{"rx/γ",5},1.0,{&final_range,1},{NULL,0}}
};
static const ams_mel_rf_job_request_config_v2 config={
    0xFEDCBA98U,0x80000001U,0x7FFFFFFEU,1,{instances,4},{groups,3},
    {-5,123456789012345},{42,999999999999999},-123456789012345,
    {capability,5},{activity,5},{modes,4},INT64_MAX
};
static const char *logfile(void)
{
    FILE *f=fopen(log_path,"r"); CHECK(f);
    size_t n=fread(log_text,1,sizeof log_text-1,f); log_text[n]=0; CHECK(fclose(f)==0); return log_text;
}
static unsigned count(const char *event)
{ unsigned n=0; const char *p=logfile(); while((p=strstr(p,event))) { ++n; p+=strlen(event); } return n; }
static void reset(void)
{ FILE *f=fopen(log_path,"w"); CHECK(f && fclose(f)==0); }
static unsigned mock(const char *name)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    unsigned (*fn)(void); CHECK(lib); *(void **)(&fn)=dlsym(lib,name); CHECK(fn);
    unsigned result=fn(); CHECK(dlclose(lib)==0); return result;
}
static unsigned mock_index(const char *name, unsigned index)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    unsigned (*fn)(unsigned); CHECK(lib); *(void **)(&fn)=dlsym(lib,name); CHECK(fn);
    unsigned result=fn(index); CHECK(dlclose(lib)==0); return result;
}
static void no_queries(void)
{
    const struct { const char *name; unsigned methods; } queries[]={
        {"mock_rf_va_query_calls",6}, {"mock_rf_element_calls",10},
        {"mock_rf_connection_calls",5}, {"mock_rf_tx_query_calls",4}, {"mock_rf_va_lf_calls",4}
    };
    for(unsigned q=0;q<sizeof queries/sizeof queries[0];++q)
        for(unsigned i=0;i<queries[q].methods;++i) CHECK(mock_index(queries[q].name,i)==0);
}
static void open_va(ams_mel_rf_c2 **c2, ams_mel_rf_virtual_aperture **va)
{
    const ams_mel_rf_virtual_aperture_config_v1 vc={0xFEDCBA98U,0x80000001U,{local,3},{"definition/β.json",18},{caps,3}};
    ams_mel_rf_virtual_aperture_request *r=NULL; ams_mel_rf_virtual_aperture_result_v1 result={0};
    CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER,"c2:f1",c2,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_submit_virtual_aperture(*c2,&vc,&r,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_wait(r,3000,&result,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(r,va,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&r,D)==AMS_MEL_OK);
}
static void close_parent(ams_mel_rf_c2 **c2, ams_mel_rf_virtual_aperture **va)
{
    CHECK(ams_mel_rf_c2_close(c2,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_close(va,D)==AMS_MEL_OK);
}
static void finish(ams_mel_rf_virtual_aperture *va, const ams_mel_rf_job_request_config_v2 *value)
{
    ams_mel_rf_job_request *r=NULL; ams_mel_rf_job *job=NULL; ams_mel_rf_job_result_v1 result={0};
    unsigned creates=count("rf_job_command_created\n"), requests=count("rf_job_requested\n");
    CHECK(ams_mel_rf_virtual_aperture_submit_job_v2(va,value,&r,D)==AMS_MEL_OK && r);
    CHECK(count("rf_job_command_created\n")==creates+3 && count("rf_job_requested\n")==requests+1);
    CHECK(ams_mel_rf_job_request_wait(r,3000,&result,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_claim(r,&job,D)==AMS_MEL_OK && job);
    CHECK(ams_mel_rf_job_request_close(&r,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_close(&job,D)==AMS_MEL_OK);
    no_queries();
}
static void invalid(ams_mel_rf_virtual_aperture *va, const ams_mel_rf_job_request_config_v2 *value)
{
    ams_mel_rf_job_request *r=NULL;
    unsigned before=count("rf_job_command_created\n"), requests=count("rf_job_requested\n");
    CHECK(ams_mel_rf_virtual_aperture_submit_job_v2(va,value,&r,D)==AMS_MEL_INVALID_ARGUMENT && !r);
    CHECK(count("rf_job_command_created\n")==before && count("rf_job_requested\n")==requests);
}
static void validation(ams_mel_rf_virtual_aperture *va)
{
    ams_mel_rf_job_request_config_v2 v=config;
    v.rx_groups=(ams_mel_rf_rx_element_group_config_span_v2){NULL,0}; invalid(va,&v);
    v=config; v.rx_groups=(ams_mel_rf_rx_element_group_config_span_v2){NULL,1}; invalid(va,&v);
    v=config; v.rx_groups.size=SIZE_MAX; invalid(va,&v);
    for(unsigned which=0;which<2;++which) for(unsigned fraction=0;fraction<2;++fraction) {
        v=config; (which ? &v.max_complete_time : &v.min_start_time)->fractional_femtoseconds=fraction ? INT64_C(1000000000000000) : -1; invalid(va,&v);
    }
    v=config; v.instance_selection=(ams_mel_u32_span_v1){NULL,1}; invalid(va,&v);
    v=config; v.tx_power_mode_ids=(ams_mel_u32_span_v1){NULL,1}; invalid(va,&v);
    v=config; v.capability_id=(ams_mel_u8_span_v1){NULL,1}; invalid(va,&v);
    v=config; v.activity_id=(ams_mel_u8_span_v1){NULL,1}; invalid(va,&v);
    v=config; v.is_interruptable=2; invalid(va,&v);
    const ams_mel_string_view_v1 bad_labels[]={{NULL,1},{"a\0b",3},{"\xc0\xaf",2},{"",SIZE_MAX}};
    const double duties[]={NAN,INFINITY,-INFINITY,0,-0.1,1.01};
    const ams_mel_rf_frequency_range_v1 bad_ranges[]={{NAN,1},{1,NAN},{INFINITY,1},{2,1}};
    ams_mel_rf_rx_element_group_config_v2 g[3];
    for(unsigned i=0;i<4;++i) {
        memcpy(g,groups,sizeof g); v=config; v.rx_groups.data=g; g[2].label=bad_labels[i]; invalid(va,&v);
        ams_mel_rf_rx_data_pipe_endpoint_config_v1 p=default_pipe;
        memcpy(g,groups,sizeof g); g[1].data_pipe_endpoint_configs.data=&p; p.data_pipe_label=bad_labels[i]; invalid(va,&v);
        memcpy(g,groups,sizeof g); g[2].expected_center_frequencies=(ams_mel_rf_frequency_range_span_v1){&bad_ranges[i],1}; invalid(va,&v);
    }
    for(unsigned i=0;i<6;++i) { memcpy(g,groups,sizeof g); v=config; v.rx_groups.data=g; g[2].desired_duty_factor=duties[i]; invalid(va,&v); }
    memcpy(g,groups,sizeof g); v=config; v.rx_groups.data=g;
    g[2].expected_center_frequencies=(ams_mel_rf_frequency_range_span_v1){NULL,1}; invalid(va,&v);
    memcpy(g,groups,sizeof g); g[2].data_pipe_endpoint_configs=(ams_mel_rf_rx_data_pipe_endpoint_config_span_v1){NULL,1}; invalid(va,&v);
    ams_mel_rf_rx_data_pipe_endpoint_config_v1 p=default_pipe;
    memcpy(g,groups,sizeof g); g[1].data_pipe_endpoint_configs.data=&p;
    p.endpoint_ids=(ams_mel_u64_span_v1){NULL,0}; invalid(va,&v);
    p.endpoint_ids=(ams_mel_u64_span_v1){NULL,1}; invalid(va,&v);
    const uint64_t dup[]={UINT64_MAX,UINT64_MAX}; p.endpoint_ids=(ams_mel_u64_span_v1){dup,2}; invalid(va,&v);
    ams_mel_rf_job_request *r=NULL;
    CHECK(ams_mel_rf_virtual_aperture_submit_job_v2(NULL,&config,&r,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_virtual_aperture_submit_job_v2(va,NULL,&r,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_virtual_aperture_submit_job_v2(va,&config,NULL,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_virtual_aperture_submit_job_v2(va,&config,&r,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT);
    no_queries();
}
static void command_failures(void)
{
    const char *stages[]={"create-std","create-unknown","create-alloc","null","tx","mode","duty","frequency","endpoint"};
    for(unsigned group=1;group<=3;++group) for(unsigned stage=0;stage<9;++stage) {
        reset(); ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        open_va(&c2,&va);
        // The primary middle/final group omit frequency/endpoints respectively.
        // Add matching input only for the failing call; requestJob must not see it.
        ams_mel_rf_rx_element_group_config_v2 g[3]; memcpy(g,groups,sizeof g);
        g[group-1].expected_center_frequencies=(ams_mel_rf_frequency_range_span_v1){ranges,2};
        g[group-1].data_pipe_endpoint_configs=(ams_mel_rf_rx_data_pipe_endpoint_config_span_v1){pipes,2};
        ams_mel_rf_job_request_config_v2 v=config; v.rx_groups.data=g;
        char failure[64]; CHECK(snprintf(failure,sizeof failure,"%u:%s",group,stages[stage])>0);
        CHECK(setenv("AMS_MEL_TEST_F1_FAILURE",failure,1)==0);
        unsigned creates=count("rf_job_command_created\n"), destroys=count("rf_job_command_destroyed\n"), requests=count("rf_job_requested\n");
        ams_mel_rf_job_request *r=NULL;
        ams_mel_status_t expected=stage==2 ? AMS_MEL_INTERNAL_ERROR : stage==3 || stage==4 ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_PROVIDER_EXCEPTION;
        CHECK(ams_mel_rf_virtual_aperture_submit_job_v2(va,&v,&r,D)==expected && !r);
        CHECK(count("rf_job_command_created\n")==creates+group);
        CHECK(count("rf_job_command_destroyed\n")==destroys+group-(stage<=3 ? 1U : 0U));
        CHECK(count("rf_job_requested\n")==requests);
        CHECK(unsetenv("AMS_MEL_TEST_F1_FAILURE")==0);
        finish(va,&config); // The failed sibling claim must not poison the live VA.
        CHECK(!count("rf_c2_shutdown\n"));
        CHECK(ams_mel_rf_c2_close(&c2,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK);
        const char *s=logfile(), *destroyed=strstr(s,"rf_va_destroyed\n"), *shutdown=strstr(s,"rf_c2_shutdown\n");
        CHECK(destroyed && shutdown && destroyed<shutdown);
        const char *p=s;
        while((p=strstr(p,"rf_job_command_destroyed\n"))) { CHECK(p<destroyed); ++p; }
        CHECK(count("rf_job_command_created\n")==count("rf_job_command_destroyed\n")+(stage<=3 ? 1U : 0U));
        CHECK(!count("rf_forbidden_call\n"));
    }
}
static void fidelity(ams_mel_rf_virtual_aperture *va)
{
    const char *cases[]={"primary","boundaries","empty","zero","binary","repeated"};
    uint8_t bytes[257], reverse[257], zero=0;
    const uint32_t selected[]={UINT32_MAX,7,UINT32_MAX,0}, power[]={9,0,9,UINT32_MAX,0x80000000U};
    for(unsigned i=0;i<257;++i) { bytes[i]=(uint8_t)i; reverse[i]=(uint8_t)(256-i); }
    for(unsigned i=0;i<6;++i) {
        ams_mel_rf_job_request_config_v2 v=config;
        ams_mel_rf_rx_element_group_config_v2 g[3]; memcpy(g,groups,sizeof g);
        CHECK(setenv("AMS_MEL_TEST_F1_CASE",cases[i],1)==0);
        if(i==1) {
            v.min_start_time=(ams_mel_rf_utc_time_v1){INT64_MIN,0}; v.max_complete_time.seconds=INT64_MAX;
            v.duration_femtoseconds=INT64_MIN; v.lookahead_femtoseconds=INT64_MIN;
            v.instance_selection=(ams_mel_u32_span_v1){selected,4}; v.tx_power_mode_ids=(ams_mel_u32_span_v1){power,5};
        } else if(i==2) {
            v.capability_id=(ams_mel_u8_span_v1){NULL,0}; v.activity_id=v.capability_id;
            v.tx_power_mode_ids=(ams_mel_u32_span_v1){NULL,0}; v.duration_femtoseconds=0; v.lookahead_femtoseconds=0;
            v.min_start_time.seconds=INT64_MAX; v.max_complete_time.seconds=INT64_MIN;
        } else if(i==3) {
            v.capability_id=(ams_mel_u8_span_v1){&zero,1}; v.activity_id=v.capability_id; v.duration_femtoseconds=INT64_MAX;
        } else if(i==4) {
            v.capability_id=(ams_mel_u8_span_v1){bytes,257}; v.activity_id=(ams_mel_u8_span_v1){reverse,257};
        } else if(i==5) {
            g[0].data_pipe_endpoint_configs.size=3; g[1].label=g[0].label; v.rx_groups.data=g;
        }
        finish(va,&v);
    }
    CHECK(unsetenv("AMS_MEL_TEST_F1_CASE")==0);
}
static void async_failures(ams_mel_rf_virtual_aperture *va)
{
    const char *cases[]={"submit-std","submit-unknown","submit-alloc","invalid-future","future-std","future-unknown","future-alloc","rejected","null-job"};
    for(unsigned i=0;i<9;++i) {
        CHECK(setenv("AMS_MEL_TEST_F1_CASE",cases[i],1)==0);
        ams_mel_rf_job_request *r=NULL; ams_mel_rf_job *job=NULL; ams_mel_rf_job_result_v1 result={0};
        const ams_mel_status_t expected=i==2 || i==6 ? AMS_MEL_INTERNAL_ERROR : i==3 || i>=7 ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_PROVIDER_EXCEPTION;
        ams_mel_status_t status=ams_mel_rf_virtual_aperture_submit_job_v2(va,&config,&r,D);
        if(i<4) CHECK(status==expected && !r);
        else {
            CHECK(status==AMS_MEL_OK && r);
            CHECK(ams_mel_rf_job_request_wait(r,3000,&result,D)==expected);
            CHECK(ams_mel_rf_job_request_claim(r,&job,D)==expected && !job);
            CHECK(ams_mel_rf_job_request_close(&r,D)==AMS_MEL_OK);
        }
        CHECK(unsetenv("AMS_MEL_TEST_F1_CASE")==0); finish(va,&config);
    }
}
static void retention(const char *self)
{
    const char *failpoints[]={"worker-launch","post-provider-allocation","publication"};
    for(unsigned i=0;i<3;++i) {
        pid_t child=fork(); CHECK(child>=0);
        if(!child) {
            execl(self,self,failpoints[i],(char *)NULL); _exit(127);
        }
        int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    }
}
int main(int argc, char **argv)
{
    int fd=mkstemp(log_path); CHECK(fd>=0 && close(fd)==0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG",log_path,1)==0);
    caps[0].uuid[1]=0x80; caps[0].uuid[2]=0xff; caps[0].descriptive_label=(ams_mel_string_view_v1){"first",5};
    caps[1].uuid[0]=0xff; caps[1].descriptive_label=(ams_mel_string_view_v1){"",0};
    caps[2].uuid[15]=0x80; caps[2].descriptive_label=(ams_mel_string_view_v1){"µ-third",8};
    ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
    if(argc==2) {
        open_va(&c2,&va);
        CHECK(setenv("AMS_MEL_TEST_F1_CASE","delayed",1)==0);
        CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE",argv[1],1)==0);
        ams_mel_rf_job_request *r=NULL;
        CHECK(ams_mel_rf_virtual_aperture_submit_job_v2(va,&config,&r,D)==AMS_MEL_INTERNAL_ERROR && !r);
        close_parent(&c2,&va);
        CHECK(!count("rf_va_destroyed\n") && !count("rf_c2_shutdown\n"));
        CHECK(mock("mock_rf_job_release_one")==1);
        CHECK(!count("rf_va_destroyed\n") && !count("rf_c2_shutdown\n"));
        CHECK(unlink(log_path)==0); return 0;
    }
    CHECK(argc==1);
    open_va(&c2,&va); validation(va); fidelity(va); async_failures(va); close_parent(&c2,&va);
    CHECK(!count("rf_forbidden_call\n") && count("rf_c2_shutdown\n")==1);
    command_failures();
    // Delayed request: timeout, parent-first claim and non-cancelling abandonment.
    for(unsigned abandon=0;abandon<2;++abandon) {
        reset(); open_va(&c2,&va); CHECK(setenv("AMS_MEL_TEST_F1_CASE","delayed",1)==0);
        // Keep the test observation DSO loaded across baseline/release/wait.
        // Otherwise cleanup may unload it between calls, resetting its counters
        // before the next dlopen and making a completed shutdown look absent.
        void *observation=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL); CHECK(observation);
        unsigned shutdown_before=mock("mock_rf_job_shutdown_count");
        ams_mel_rf_job_request *r=NULL; ams_mel_rf_job *job=NULL; ams_mel_rf_job_result_v1 result={0};
        CHECK(ams_mel_rf_virtual_aperture_submit_job_v2(va,&config,&r,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_request_wait(r,0,&result,D)==AMS_MEL_TIMEOUT);
        close_parent(&c2,&va); CHECK(!count("rf_c2_shutdown\n"));
        if(abandon) CHECK(ams_mel_rf_job_request_close(&r,D)==AMS_MEL_OK);
        CHECK(mock("mock_rf_job_release_one")==1);
        if(!abandon) {
            CHECK(ams_mel_rf_job_request_wait(r,3000,&result,D)==AMS_MEL_OK);
            CHECK(ams_mel_rf_job_request_claim(r,&job,D)==AMS_MEL_OK);
            CHECK(ams_mel_rf_job_request_close(&r,D)==AMS_MEL_OK);
            CHECK(ams_mel_rf_job_close(&job,D)==AMS_MEL_OK);
        } else {
            // Existing deterministic cleanup barrier, not a timing sleep.
            unsigned (*wait_shutdown)(unsigned); *(void **)(&wait_shutdown)=dlsym(observation,"mock_rf_job_wait_shutdown_after"); CHECK(wait_shutdown);
            CHECK(wait_shutdown(shutdown_before)>0);
        }
        const char *s=logfile(), *commands=strstr(s,"rf_job_command_destroyed\n"), *destroyed=strstr(s,"rf_va_destroyed\n"), *shutdown=strstr(s,"rf_c2_shutdown\n");
        CHECK(commands && destroyed && shutdown && commands<destroyed && destroyed<shutdown);
        CHECK(dlclose(observation)==0);
        CHECK(unsetenv("AMS_MEL_TEST_F1_CASE")==0);
    }
    retention(argv[0]); CHECK(unlink(log_path)==0);
    puts("PASS: RF JobRequest v2 fidelity, validation, group rollback and shared async retention");
    return 0;
}
