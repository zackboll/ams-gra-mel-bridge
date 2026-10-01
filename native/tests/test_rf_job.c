#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <math.h>
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
    CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER,scenario,c2,diag,sizeof diag,&required)==AMS_MEL_OK);
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
static void retention_case(const char *failure)
{
    ams_mel_rf_c2 *c2=NULL;
    ams_mel_rf_virtual_aperture *va=NULL;
    ams_mel_rf_job_request *request=NULL;
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
