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
static char diag[512], log_path[]="/tmp/ams-f3-XXXXXX", log_text[32768];
static size_t required;
static const ams_mel_string_view_v1 local[]={{"alpha",5},{"µ-local",8},{"",0}};
static ams_mel_uci_id_v1 caps[3];
static const ams_mel_rf_frequency_range_v1 ranges[]={{1000000.25,2000000.5},{987654321.125,987654322.875}}, final_range={-1.25,3.5};
static const uint64_t endpoints[]={0,UINT64_C(0x8000000000000000),UINT64_MAX}, secondary[]={7,42};
static const uint32_t instances[]={42,0,42,UINT32_MAX}, modes[]={UINT32_MAX,7,7,0x80000000U};
static const uint8_t capability[]={0,0xff,0x80,0,7}, activity[]={0xde,0xad,0,0xbe,0xef};
static const ams_mel_rf_rx_data_pipe_endpoint_config_v1 pipes[]={
    {{"products/β",11},{endpoints,3}},{{"secondary",9},{secondary,2}}
};
static ams_mel_rf_pointing_v1 points[]={
    {.kind=AMS_MEL_RF_POINTING_ECEF,.ecef={{1.25,-2.5,3.75},{-4.5,5.625,-6.75},{-7,123456789012345}}},
    {.kind=AMS_MEL_RF_POINTING_PLATFORM_RELATIVE,.platform_relative={-0.75,0.25}},
    {.kind=AMS_MEL_RF_POINTING_LLA,.lla={0.125,-1.25,12345.5,11.25,-12.5,13.75,{42,999999999999999}}},
    {.kind=AMS_MEL_RF_POINTING_FACE_RELATIVE,.face_relative={1.5,-0.5}},
    {.kind=AMS_MEL_RF_POINTING_FACE_RELATIVE,.face_relative={1.5,-0.5}},
    {.kind=AMS_MEL_RF_POINTING_BASELINE_RELATIVE,.baseline_relative_conic_rad=-2.25}
};
static const ams_mel_rf_frequency_range_v1 tx_ranges[]={{100000000.25,100000001.5},{915000000.0,915000000.0}};
static const ams_mel_rf_job_element_group_config_v4 groups[]={
    {.mode=AMS_MEL_RF_ELEMENT_GROUP_MODE_RX,.rx={{{"rx/a",4},0.625,{ranges,2},{pipes,2}},{points,2}}},
    {.mode=AMS_MEL_RF_ELEMENT_GROUP_MODE_TX,.tx={{"tx/a",4},0xDEADBEEFU,0.375,{tx_ranges,2}}},
    {.mode=AMS_MEL_RF_ELEMENT_GROUP_MODE_RX,.rx={{{"rx/b",4},0.625,{ranges,2},{pipes,2}},{points,2}}},
    {.mode=AMS_MEL_RF_ELEMENT_GROUP_MODE_TX,.tx={{"tx/b",4},UINT32_MAX,1.0,{&final_range,1}}}
};
static const ams_mel_rf_job_request_config_v4 config={
    0xFEDCBA98U,0x80000001U,0x7FFFFFFEU,1,{instances,4},{groups,4},
    {-5,123456789012345},{42,999999999999999},-123456789012345,
    {capability,5},{activity,5},{modes,4},INT64_MAX,1,
    {.kind=AMS_MEL_RF_POINTING_LLA,.lla={-8.125,9.25,-999.5,-21.25,22.5,-23.75,{INT64_MIN,0}}}
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
static void finish(ams_mel_rf_virtual_aperture *va, const ams_mel_rf_job_request_config_v4 *value)
{
    ams_mel_rf_job_request *r=NULL; ams_mel_rf_job *job=NULL; ams_mel_rf_job_result_v1 result={0};
    unsigned creates=count("rf_job_command_created\n"), requests=count("rf_job_requested\n");
    CHECK(ams_mel_rf_virtual_aperture_submit_job_v4(va,value,&r,D)==AMS_MEL_OK && r);
    CHECK(count("rf_job_command_created\n")==creates+value->element_groups.size && count("rf_job_requested\n")==requests+1);
    CHECK(ams_mel_rf_job_request_wait(r,3000,&result,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_claim(r,&job,D)==AMS_MEL_OK && job);
    CHECK(ams_mel_rf_job_request_close(&r,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_close(&job,D)==AMS_MEL_OK);
    no_queries();
}
static void invalid(ams_mel_rf_virtual_aperture *va, const ams_mel_rf_job_request_config_v4 *value)
{
    ams_mel_rf_job_request *r=NULL;
    unsigned before=count("rf_job_command_created\n"), requests=count("rf_job_requested\n");
    CHECK(ams_mel_rf_virtual_aperture_submit_job_v4(va,value,&r,D)==AMS_MEL_INVALID_ARGUMENT && !r);
    CHECK(count("rf_job_command_created\n")==before && count("rf_job_requested\n")==requests);
}

static void validation(ams_mel_rf_virtual_aperture *va)
{
    ams_mel_rf_job_request_config_v4 v=config;
    v.element_groups=(ams_mel_rf_job_element_group_config_span_v4){NULL,0}; invalid(va,&v);
    v=config; v.element_groups.data=NULL; invalid(va,&v);
    v=config; v.element_groups.size=SIZE_MAX; invalid(va,&v);
    for(unsigned which=0;which<2;++which) for(unsigned fraction=0;fraction<2;++fraction) {
        v=config; (which ? &v.max_complete_time : &v.min_start_time)->fractional_femtoseconds=fraction ? INT64_C(1000000000000000) : -1; invalid(va,&v);
    }
    v=config; v.instance_selection=(ams_mel_u32_span_v1){NULL,1}; invalid(va,&v);
    v=config; v.tx_power_mode_ids=(ams_mel_u32_span_v1){NULL,1}; invalid(va,&v);
    v=config; v.capability_id=(ams_mel_u8_span_v1){NULL,1}; invalid(va,&v);
    v=config; v.activity_id=(ams_mel_u8_span_v1){NULL,1}; invalid(va,&v);
    v=config; v.is_interruptable=2; invalid(va,&v);
    v=config; v.has_estimated_stab_point=2; invalid(va,&v);
    v=config; v.estimated_stab_point.kind=99; invalid(va,&v);
    v=config; v.estimated_stab_point.lla.time_of_validity.fractional_femtoseconds=-1; invalid(va,&v);
    ams_mel_rf_job_request *out=NULL;
    CHECK(ams_mel_rf_virtual_aperture_submit_job_v4(NULL,&config,&out,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_virtual_aperture_submit_job_v4(va,NULL,&out,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_virtual_aperture_submit_job_v4(va,&config,NULL,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_virtual_aperture_submit_job_v4(va,&config,&out,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT);
    const ams_mel_string_view_v1 labels[]={{NULL,1},{"a\0b",3},{"\xc0\xaf",2},{"",SIZE_MAX}};
    const double duties[]={NAN,INFINITY,-INFINITY,0,-0.1,1.01};
    for(unsigned index=0;index<4;++index) {
        ams_mel_rf_job_element_group_config_v4 g[4];
        memcpy(g,groups,sizeof g); v=config; v.element_groups.data=g;
        g[index].mode=99; invalid(va,&v); g[index]=groups[index];
        for(unsigned i=0;i<4;++i) {
            if(index%2) g[index].tx.label=labels[i]; else g[index].rx.group.label=labels[i];
            invalid(va,&v); g[index]=groups[index];
        }
        for(unsigned i=0;i<6;++i) {
            if(index%2) g[index].tx.desired_duty_factor=duties[i]; else g[index].rx.group.desired_duty_factor=duties[i];
            invalid(va,&v); g[index]=groups[index];
        }
        ams_mel_rf_frequency_range_span_v1 *span=index%2 ? &g[index].tx.expected_center_frequencies : &g[index].rx.group.expected_center_frequencies;
        *span=(ams_mel_rf_frequency_range_span_v1){NULL,1}; invalid(va,&v);
        *span=(ams_mel_rf_frequency_range_span_v1){ranges,SIZE_MAX}; invalid(va,&v);
        const ams_mel_rf_frequency_range_v1 bad[]={{NAN,1},{0,INFINITY},{-INFINITY,0},{2,1}};
        for(unsigned i=0;i<4;++i) { *span=(ams_mel_rf_frequency_range_span_v1){bad+i,1}; invalid(va,&v); }
        if(index%2==0) {
            g[index]=groups[index]; g[index].rx.group.data_pipe_endpoint_configs=(ams_mel_rf_rx_data_pipe_endpoint_config_span_v1){NULL,1}; invalid(va,&v);
            g[index]=groups[index]; g[index].rx.expected_pointing_angles=(ams_mel_rf_pointing_span_v1){NULL,1}; invalid(va,&v);
            ams_mel_rf_pointing_v1 p=points[0]; p.kind=99;
            g[index]=groups[index]; g[index].rx.expected_pointing_angles=(ams_mel_rf_pointing_span_v1){&p,1}; invalid(va,&v);
            p=points[0]; p.ecef.time_of_validity.fractional_femtoseconds=-1; invalid(va,&v);
        }
    }
    CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE","pointing-preparation",1)==0);
    ams_mel_rf_job_request *request=NULL;
    unsigned creates=count("rf_job_command_created\n"), requests=count("rf_job_requested\n");
    CHECK(ams_mel_rf_virtual_aperture_submit_job_v4(va,&config,&request,D)==AMS_MEL_INTERNAL_ERROR && !request);
    CHECK(creates==count("rf_job_command_created\n") && requests==count("rf_job_requested\n"));
    CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE")==0);
}
static void fidelity(ams_mel_rf_virtual_aperture *va)
{
    finish(va,&config);
    ams_mel_rf_job_element_group_config_v4 g[4]; memcpy(g,groups,sizeof g);
    ams_mel_rf_job_request_config_v4 v=config; v.element_groups.data=g;
    for(unsigned i=0;i<4;++i) {
        if(i%2) {
            g[i].rx.group.label=(ams_mel_string_view_v1){"\xff",1}; g[i].rx.group.desired_duty_factor=NAN;
            g[i].rx.group.expected_center_frequencies=(ams_mel_rf_frequency_range_span_v1){NULL,SIZE_MAX};
            g[i].rx.group.data_pipe_endpoint_configs=(ams_mel_rf_rx_data_pipe_endpoint_config_span_v1){NULL,SIZE_MAX};
            g[i].rx.expected_pointing_angles=(ams_mel_rf_pointing_span_v1){NULL,SIZE_MAX};
        } else {
            g[i].tx.label=(ams_mel_string_view_v1){"\xff",1}; g[i].tx.desired_duty_factor=NAN;
            g[i].tx.expected_center_frequencies=(ams_mel_rf_frequency_range_span_v1){NULL,SIZE_MAX};
        }
    }
    finish(va,&v);
    ams_mel_rf_pointing_v1 poison={.kind=99,.ecef={{0,0,0},{0,0,0},{0,-1}}};
    g[1].rx.expected_pointing_angles=(ams_mel_rf_pointing_span_v1){&poison,1}; finish(va,&v);
    poison.kind=AMS_MEL_RF_POINTING_ECEF; finish(va,&v);
    const uint32_t powers[]={0,1,0x80000000U,UINT32_MAX};
    CHECK(setenv("AMS_MEL_TEST_F3_CASE","tx-only",1)==0);
    v=config; v.element_groups=(ams_mel_rf_job_element_group_config_span_v4){g+1,1};
    for(unsigned i=0;i<4;++i) {
        char text[32]; CHECK(snprintf(text,sizeof text,"%u",powers[i])>0);
        CHECK(setenv("AMS_MEL_TEST_F3_POWER",text,1)==0); g[1].tx.tx_power_level=powers[i]; finish(va,&v);
    }
    CHECK(unsetenv("AMS_MEL_TEST_F3_POWER")==0);
    CHECK(setenv("AMS_MEL_TEST_F3_CASE","rx-only",1)==0);
    v.element_groups=(ams_mel_rf_job_element_group_config_span_v4){g,1}; finish(va,&v);
    CHECK(setenv("AMS_MEL_TEST_F3_CASE","repeated",1)==0);
    g[0]=groups[1]; g[1]=groups[0]; g[2]=groups[3];
    g[0].tx.label=g[1].rx.group.label=g[2].tx.label=(ams_mel_string_view_v1){"same",4};
    v.element_groups=(ams_mel_rf_job_element_group_config_span_v4){g,3}; finish(va,&v);
    CHECK(setenv("AMS_MEL_TEST_F3_CASE","primary",1)==0);
}
static void command_failures(void)
{
    const char *stages[]={"create-std","create-unknown","create-alloc","null","mismatch",
        "mode-std","mode-unknown","mode-alloc","duty-std","duty-unknown","duty-alloc",
        "power-std","power-unknown","power-alloc","frequency-std","frequency-unknown","frequency-alloc"};
    for(unsigned position=0;position<3;++position) for(unsigned stage=0;stage<17;++stage) {
        reset(); ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL; open_va(&c2,&va);
        CHECK(setenv("AMS_MEL_TEST_F3_CASE",position==0 ? "tx-only" : "primary",1)==0);
        ams_mel_rf_job_request_config_v4 v=config;
        if(position==0) v.element_groups=(ams_mel_rf_job_element_group_config_span_v4){groups+1,1};
        unsigned index=position==0 ? 1 : position==1 ? 2 : 4;
        char failure[80]; CHECK(snprintf(failure,sizeof failure,"%u:%s",index,stages[stage])>0);
        CHECK(setenv("AMS_MEL_TEST_F3_FAILURE",failure,1)==0);
        ams_mel_rf_job_request *r=NULL;
        ams_mel_status_t expected=strstr(stages[stage],"alloc") ? AMS_MEL_INTERNAL_ERROR : stage==3 || stage==4 ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_PROVIDER_EXCEPTION;
        CHECK(ams_mel_rf_virtual_aperture_submit_job_v4(va,&v,&r,D)==expected && !r);
        CHECK(!count("rf_job_requested\n"));
        CHECK(count("rf_job_command_created\n")==index);
        CHECK(count("rf_job_command_destroyed\n")==index-(stage<4 ? 1U : 0U));
        CHECK(unsetenv("AMS_MEL_TEST_F3_FAILURE")==0);
        finish(va,&v); close_parent(&c2,&va);
        const char *s=logfile(), *destroyed=strstr(s,"rf_va_destroyed\n"), *shutdown=strstr(s,"rf_c2_shutdown\n");
        CHECK(destroyed && shutdown && destroyed<shutdown);
        const char *p=s; while((p=strstr(p,"rf_job_command_destroyed\n"))) { CHECK(p<destroyed); ++p; }
        CHECK(!count("rf_forbidden_call\n"));
    }
    reset(); ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL; open_va(&c2,&va);
    CHECK(setenv("AMS_MEL_TEST_F3_CASE","primary",1)==0);
    CHECK(setenv("AMS_MEL_TEST_F3_FAILURE","1:mismatch",1)==0);
    ams_mel_rf_job_request *r=NULL;
    CHECK(ams_mel_rf_virtual_aperture_submit_job_v4(va,&config,&r,D)==AMS_MEL_PROVIDER_FAILED && !r);
    CHECK(!count("rf_job_requested\n"));
    CHECK(unsetenv("AMS_MEL_TEST_F3_FAILURE")==0); finish(va,&config); close_parent(&c2,&va);
}
static void async_failures(ams_mel_rf_virtual_aperture *va)
{
    const char *cases[]={"submit-std","submit-unknown","submit-alloc","invalid-future","future-std","future-unknown","future-alloc","rejected","null-job"};
    for(unsigned i=0;i<9;++i) {
        CHECK(setenv("AMS_MEL_TEST_F1_CASE",cases[i],1)==0);
        ams_mel_rf_job_request *r=NULL; ams_mel_rf_job *job=NULL; ams_mel_rf_job_result_v1 result={0};
        const ams_mel_status_t expected=i==2 || i==6 ? AMS_MEL_INTERNAL_ERROR : i==3 || i>=7 ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_PROVIDER_EXCEPTION;
        ams_mel_status_t status=ams_mel_rf_virtual_aperture_submit_job_v4(va,&config,&r,D);
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
    CHECK(setenv("AMS_MEL_TEST_F3_CASE","primary",1)==0);
    caps[0].uuid[1]=0x80; caps[0].uuid[2]=0xff; caps[0].descriptive_label=(ams_mel_string_view_v1){"first",5};
    caps[1].uuid[0]=0xff; caps[1].descriptive_label=(ams_mel_string_view_v1){"",0};
    caps[2].uuid[15]=0x80; caps[2].descriptive_label=(ams_mel_string_view_v1){"µ-third",8};
    ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
    if(argc==2) {
        open_va(&c2,&va);
        CHECK(setenv("AMS_MEL_TEST_F1_CASE","delayed",1)==0);
        CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE",argv[1],1)==0);
        ams_mel_rf_job_request *r=NULL;
        CHECK(ams_mel_rf_virtual_aperture_submit_job_v4(va,&config,&r,D)==AMS_MEL_INTERNAL_ERROR && !r);
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
        CHECK(ams_mel_rf_virtual_aperture_submit_job_v4(va,&config,&r,D)==AMS_MEL_OK);
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
    puts("PASS: RF JobRequest v4 mixed ordered RX/TX, exact power, inactive payloads, rollback and shared async retention");
    return 0;
}
