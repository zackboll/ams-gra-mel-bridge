#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
static char path[] = "/tmp/ams-rf-va-XXXXXX";
static char text[16384];
static const char *logfile(void)
{
    FILE *f = fopen(path,"r"); size_t n;
    CHECK(f); n = fread(text,1,sizeof text-1,f); text[n] = 0; CHECK(fclose(f)==0); return text;
}
static void reset(void)
{ FILE *f=fopen(path,"w"); CHECK(f); CHECK(fclose(f)==0); }
static char diag[512];
static size_t required;
static const ams_mel_string_view_v1 local[] = {
    {"alpha",5}, {"µ-local",8}, {"",0}
};
static ams_mel_uci_id_v1 capabilities[3];
static const ams_mel_rf_virtual_aperture_config_v1 config = {
    UINT32_C(0xFEDCBA98), UINT32_C(0x80000001),
    {local,3}, {"definition/β.json",18}, {capabilities,3}
};
static void open_c2(ams_mel_rf_c2 **out, const char *scenario)
{ CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER,scenario,out,diag,sizeof diag,&required)==AMS_MEL_OK); }
static ams_mel_rf_virtual_aperture_request *submit(ams_mel_rf_c2 *parent)
{
    ams_mel_rf_virtual_aperture_request *r=NULL;
    CHECK(ams_mel_rf_c2_submit_virtual_aperture(parent,&config,&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(r); return r;
}
static ams_mel_status_t wait_for(ams_mel_rf_virtual_aperture_request *r,
                                  ams_mel_rf_virtual_aperture_result_v1 *result)
{ return ams_mel_rf_virtual_aperture_request_wait(r,3000,result,diag,sizeof diag,&required); }
static void release_one(void)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    unsigned (*release)(void);
    CHECK(lib); *(void **)(&release)=dlsym(lib,"mock_rf_va_release_one");
    CHECK(release && release()==1); CHECK(dlclose(lib)==0);
}
static void check_view(ams_mel_rf_virtual_aperture *va)
{
    const ams_mel_rf_virtual_aperture_info_v1 *info=NULL;
    CHECK(ams_mel_rf_virtual_aperture_view(va,&info,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(info && info->va_instance_ids.size==3 && info->va_instance_ids.data[0]==0 &&
          info->va_instance_ids.data[1]==3 && info->va_instance_ids.data[2]==9);
    CHECK(info->is_single_group==1 && info->element_group_labels.size==3);
    CHECK(info->element_group_labels.data[0].size==8 &&
          memcmp(info->element_group_labels.data[0].data,"group/β",8)==0);
    CHECK(info->element_group_labels.data[1].size==0 &&
          info->element_group_labels.data[2].size==1 &&
          info->element_group_labels.data[2].data[0]=='0');
}
int main(void)
{
    int fd=mkstemp(path); CHECK(fd>=0); CHECK(close(fd)==0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG",path,1)==0);
    capabilities[0].uuid[0]=0; capabilities[0].uuid[1]=0x80; capabilities[0].uuid[2]=0xff;
    capabilities[0].descriptive_label=(ams_mel_string_view_v1){"first",5};
    capabilities[1].uuid[0]=0xff; capabilities[1].descriptive_label=(ams_mel_string_view_v1){"",0};
    capabilities[2].uuid[15]=0x80;
    capabilities[2].descriptive_label=(ams_mel_string_view_v1){"µ-third",8};
    ams_mel_rf_c2 *parent=NULL;
    reset(); open_c2(&parent,"c2:va-ok");
    ams_mel_rf_virtual_aperture_request *r=submit(parent);
    ams_mel_rf_virtual_aperture_result_v1 result={99};
    CHECK(wait_for(r,&result)==AMS_MEL_OK && result.error_code==AMS_MEL_ERROR_NONE);
    CHECK(wait_for(r,&result)==AMS_MEL_OK);
    ams_mel_rf_virtual_aperture *va=NULL;
    CHECK(ams_mel_rf_virtual_aperture_request_claim(r,&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(r,&(ams_mel_rf_virtual_aperture *){NULL},diag,sizeof diag,&required)==AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&parent,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(strstr(logfile(),"rf_c2_shutdown\n")==NULL);
    check_view(va);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(va==NULL);
    {
        const char *s=logfile(); const char *destroy=strstr(s,"rf_va_destroyed\n");
        const char *shut=strstr(s,"rf_c2_shutdown\n");
        const char *unload=strstr(s,"library_unloaded\n");
        CHECK(destroy && shut && unload && destroy<shut && shut<unload);
    }
    reset(); open_c2(&parent,"c2:va-failure"); r=submit(parent);
    CHECK(wait_for(r,&result)==AMS_MEL_PROVIDER_FAILED);
    CHECK(result.error_code==AMS_MEL_ERROR_INVALID_PARAMETERS && strstr(diag,"mock VA rejected"));
    CHECK(ams_mel_rf_virtual_aperture_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&parent,diag,sizeof diag,&required)==AMS_MEL_OK);
    reset(); open_c2(&parent,"c2:va-unknown-error"); r=submit(parent);
    CHECK(wait_for(r,&result)==AMS_MEL_PROVIDER_FAILED);
    CHECK(strstr(diag,"unknown MEL error code")!=NULL);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&parent,diag,sizeof diag,&required)==AMS_MEL_OK);
    reset(); open_c2(&parent,"c2:va-long-failure"); r=submit(parent);
    CHECK(wait_for(r,&result)==AMS_MEL_PROVIDER_FAILED);
    CHECK(result.error_code==AMS_MEL_ERROR_INVALID_PARAMETERS && required>800);
    {
        size_t total=required, again=0;
        char *full=calloc(total,1); CHECK(full);
        CHECK(ams_mel_rf_virtual_aperture_request_wait(r,0,&result,full,total,&again)==AMS_MEL_PROVIDER_FAILED);
        CHECK(again==total && strstr(full,"µ end")!=NULL);
        free(full);
    }
    CHECK(ams_mel_rf_virtual_aperture_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&parent,diag,sizeof diag,&required)==AMS_MEL_OK);
    for (size_t i=0;i<3;++i) {
        const char *cases[]={"c2:va-future-throw","c2:va-future-unknown","c2:va-null"};
        open_c2(&parent,cases[i]); r=submit(parent);
        CHECK(wait_for(r,&result)==(i==2 ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_PROVIDER_EXCEPTION));
        CHECK(ams_mel_rf_virtual_aperture_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&parent,diag,sizeof diag,&required)==AMS_MEL_OK);
    }
    open_c2(&parent,"c2:va-invalid-future"); r=NULL;
    CHECK(ams_mel_rf_c2_submit_virtual_aperture(parent,&config,&r,diag,sizeof diag,&required)==AMS_MEL_PROVIDER_FAILED);
    CHECK(r==NULL && strstr(diag,"invalid VirtualAperture future"));
    CHECK(ams_mel_rf_c2_close(&parent,diag,sizeof diag,&required)==AMS_MEL_OK);
    open_c2(&parent,"c2:va-submit-throw"); r=NULL;
    CHECK(ams_mel_rf_c2_submit_virtual_aperture(parent,&config,&r,diag,sizeof diag,&required)==AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(r==NULL && strstr(diag,"mock VA submit exception"));
    CHECK(ams_mel_rf_c2_close(&parent,diag,sizeof diag,&required)==AMS_MEL_OK);
    reset(); open_c2(&parent,"c2:va-shutdown-throw"); r=submit(parent);
    CHECK(wait_for(r,&result)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(r,&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&parent,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(!va && strstr(diag,"mock C2 shutdown exception"));
    CHECK(strstr(logfile(),"rf_c2_destroyed\n")==NULL &&
          strstr(logfile(),"library_unloaded\n")==NULL);
    reset(); open_c2(&parent,"c2:va-ok");
    CHECK(setenv("AMS_MEL_TEST_RF_VA_FAILURE","worker-launch",1)==0);
    r=NULL;
    CHECK(ams_mel_rf_c2_submit_virtual_aperture(parent,&config,&r,diag,sizeof diag,&required)==AMS_MEL_INTERNAL_ERROR);
    CHECK(r==NULL && strstr(diag,"retained"));
    CHECK(unsetenv("AMS_MEL_TEST_RF_VA_FAILURE")==0);
    CHECK(ams_mel_rf_c2_close(&parent,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(strstr(logfile(),"rf_c2_shutdown\n")==NULL && strstr(logfile(),"library_unloaded\n")==NULL);
    for (size_t i=0;i<2;++i) {
        const char *mode=i==0 ? "post-provider-allocation" : "publication";
        reset(); open_c2(&parent,"c2:va-ok");
        CHECK(setenv("AMS_MEL_TEST_RF_VA_FAILURE",mode,1)==0);
        r=NULL;
        CHECK(ams_mel_rf_c2_submit_virtual_aperture(parent,&config,&r,diag,sizeof diag,&required)==AMS_MEL_INTERNAL_ERROR);
        CHECK(r==NULL && strstr(diag,"retained"));
        CHECK(unsetenv("AMS_MEL_TEST_RF_VA_FAILURE")==0);
        CHECK(ams_mel_rf_c2_close(&parent,diag,sizeof diag,&required)==AMS_MEL_OK);
        CHECK(strstr(logfile(),"rf_c2_shutdown\n")==NULL &&
              strstr(logfile(),"library_unloaded\n")==NULL);
    }
    reset(); open_c2(&parent,"c2:va-ok"); r=submit(parent);
    CHECK(wait_for(r,&result)==AMS_MEL_OK);
    CHECK(setenv("AMS_MEL_TEST_RF_VA_FAILURE","va-owner",1)==0);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(r,&va,diag,sizeof diag,&required)==AMS_MEL_INTERNAL_ERROR);
    CHECK(va==NULL);
    CHECK(unsetenv("AMS_MEL_TEST_RF_VA_FAILURE")==0);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(r,&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&parent,diag,sizeof diag,&required)==AMS_MEL_OK);
    reset(); open_c2(&parent,"c2:va-ok"); r=submit(parent);
    CHECK(wait_for(r,&result)==AMS_MEL_OK);
    CHECK(setenv("AMS_MEL_TEST_RF_VA_FAILURE","va-snapshot",1)==0);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(r,&va,diag,sizeof diag,&required)==AMS_MEL_INTERNAL_ERROR);
    CHECK(unsetenv("AMS_MEL_TEST_RF_VA_FAILURE")==0);
    CHECK(va==NULL);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(r,&va,diag,sizeof diag,&required)==AMS_MEL_INTERNAL_ERROR);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&parent,diag,sizeof diag,&required)==AMS_MEL_OK);
    reset(); open_c2(&parent,"c2:va-delayed");
    ams_mel_rf_virtual_aperture_request *r2=submit(parent);
    r=submit(parent);
    CHECK(ams_mel_rf_c2_close(&parent,diag,sizeof diag,&required)==AMS_MEL_OK);
    release_one();
    CHECK(wait_for(r,&result)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(r,&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(strstr(logfile(),"rf_c2_shutdown\n")==NULL);
    release_one();
    CHECK(wait_for(r2,&result)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(r2,&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&r2,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(strstr(logfile(),"rf_c2_shutdown\n")!=NULL);
    reset(); open_c2(&parent,"c2:va-ok"); r=NULL;
    {
        ams_mel_rf_virtual_aperture_config_v1 invalid=config;
        invalid.local_function_info.data=NULL;
        CHECK(ams_mel_rf_c2_submit_virtual_aperture(parent,&invalid,&r,diag,sizeof diag,&required)==AMS_MEL_INVALID_ARGUMENT);
        invalid=config; invalid.capability_ids.data=NULL;
        CHECK(ams_mel_rf_c2_submit_virtual_aperture(parent,&invalid,&r,diag,sizeof diag,&required)==AMS_MEL_INVALID_ARGUMENT);
        invalid=config; invalid.va_definition_file_info=(ams_mel_string_view_v1){"a\0b",3};
        CHECK(ams_mel_rf_c2_submit_virtual_aperture(parent,&invalid,&r,diag,sizeof diag,&required)==AMS_MEL_INVALID_ARGUMENT);
        CHECK(r==NULL && strstr(logfile(),"rf_va_requested\n")==NULL);
    }
    CHECK(ams_mel_rf_c2_close(&parent,diag,sizeof diag,&required)==AMS_MEL_OK);
    reset(); open_c2(&parent,"c2:va-delayed"); r=submit(parent);
    CHECK(ams_mel_rf_virtual_aperture_request_wait(r,0,&result,diag,sizeof diag,&required)==AMS_MEL_TIMEOUT);
    CHECK(ams_mel_rf_virtual_aperture_request_wait(r,0,&result,diag,sizeof diag,&required)==AMS_MEL_TIMEOUT);
    CHECK(ams_mel_rf_c2_close(&parent,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(strstr(logfile(),"rf_c2_shutdown\n")==NULL);
    release_one();
    CHECK(wait_for(r,&result)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(r,&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    check_view(va);
    CHECK(strstr(logfile(),"rf_c2_shutdown\n")==NULL);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(strstr(logfile(),"rf_c2_shutdown\n")!=NULL);
    reset(); open_c2(&parent,"c2:va-delayed"); r=submit(parent);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&parent,diag,sizeof diag,&required)==AMS_MEL_OK);
    release_one();
    /* Completion is asynchronous: observe logged shutdown without canceling. */
    for (unsigned i=0;i<100000 && !strstr(logfile(),"rf_c2_shutdown\n");++i)
        (void)sched_yield();
    CHECK(strstr(logfile(),"rf_va_destroyed\n") && strstr(logfile(),"rf_c2_shutdown\n"));
    reset(); open_c2(&parent,"c2:va-getter-throw"); r=submit(parent);
    CHECK(wait_for(r,&result)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(r,&va,diag,sizeof diag,&required)==AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(va==NULL && strstr(diag,"mock VA getter exception"));
    CHECK(ams_mel_rf_virtual_aperture_request_close(&r,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&parent,diag,sizeof diag,&required)==AMS_MEL_OK);
    CHECK(unlink(path)==0);
    puts("PASS: RF C2/VA C11 contract"); return 0;
}