#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d %s (%s)\n",__FILE__,__LINE__,#x,diag); exit(1); } } while (0)
#define D diag, sizeof diag, &required
static char diag[512];
static size_t required;
static unsigned (*calls)(unsigned), (*e1)(unsigned);
static uint32_t (*input)(unsigned);
static void (*change)(unsigned);
static ams_mel_status_t invoke(unsigned m, const ams_mel_rf_virtual_aperture *va,
    uint32_t *scalar, ams_mel_rf_va_local_function_list **list,
    ams_mel_rf_va_local_function_status **statuses, char *d, size_t cap)
{
    switch(m) {
    case 0: return ams_mel_rf_virtual_aperture_is_cached_waveform_supported(va,scalar,d,cap,&required);
    case 1: return ams_mel_rf_virtual_aperture_dynamic_weights_supported(va,scalar,d,cap,&required);
    case 2: return ams_mel_rf_virtual_aperture_get_local_functions(va,list,d,cap,&required);
    default: return ams_mel_rf_virtual_aperture_get_local_function_status(va,0xDEADBEEFU,0x80000001U,statuses,d,cap,&required);
    }
}
static void catalog(ams_mel_rf_va_local_function_list *owner, unsigned generation)
{
    ams_mel_rf_va_local_function_info_span_v1 v={NULL,0};
    CHECK(ams_mel_rf_va_local_function_list_view(owner,&v,D)==AMS_MEL_OK);
    if(generation==2) { CHECK(v.size==0 && !v.data); return; }
    if(generation==1) { CHECK(v.size==1 && v.data[0].local_function_type_id==42 && v.data[0].instance_count==4); return; }
    if(generation==3) { CHECK(v.size==1 && v.data[0].instance_count==(uint64_t)SIZE_MAX); return; }
    const uint32_t ids[]={0,7,0x80000001U,UINT32_MAX};
    const uint64_t counts[]={2,3,1,0};
    CHECK(v.size==4 && v.data);
    for(unsigned i=0;i<4;++i) CHECK(v.data[i].local_function_type_id==ids[i] && v.data[i].instance_count==counts[i]);
}
static void status(ams_mel_rf_va_local_function_status *owner, unsigned generation)
{
    ams_mel_u32_span_v1 v={NULL,0};
    const uint32_t a[]={3,0,1,2,3}, b[]={2,0,2};
    CHECK(ams_mel_rf_va_local_function_status_view(owner,&v,D)==AMS_MEL_OK);
    if(generation==2) { CHECK(v.size==0 && !v.data); return; }
    CHECK(v.size==(generation==1 ? 3U : 5U));
    CHECK(v.data && !memcmp(v.data,generation==1 ? b : a,v.size*sizeof(uint32_t)));
}
int main(void)
{
    char path[]="/tmp/ams-rf-lf-XXXXXX";
    int fd=mkstemp(path); CHECK(fd>=0 && close(fd)==0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG",path,1)==0);
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL); CHECK(lib);
    void *sym=dlsym(lib,"mock_rf_va_lf_calls"); memcpy(&calls,&sym,sizeof calls);
    sym=dlsym(lib,"mock_rf_va_query_calls"); memcpy(&e1,&sym,sizeof e1);
    sym=dlsym(lib,"mock_rf_va_lf_input"); memcpy(&input,&sym,sizeof input);
    sym=dlsym(lib,"mock_rf_va_lf_change"); memcpy(&change,&sym,sizeof change);
    CHECK(calls && e1 && input && change);
    /* Reuse the existing mock and its exact Claim fixture. */
    static const ams_mel_string_view_v1 local[]={{"alpha",5},{"µ-local",8},{"",0}};
    ams_mel_uci_id_v1 caps[3]={{{0},{"first",5}},{{0},{"",0}},{{0},{"µ-third",8}}};
    caps[0].uuid[1]=0x80; caps[0].uuid[2]=0xff; caps[1].uuid[0]=0xff; caps[2].uuid[15]=0x80;
    ams_mel_rf_virtual_aperture_config_v1 config={0xFEDCBA98U,0x80000001U,{local,3},{"definition/β.json",18},{caps,3}};
    ams_mel_rf_c2 *parent=NULL;
    ams_mel_rf_virtual_aperture_request *request=NULL;
    ams_mel_rf_virtual_aperture *va=NULL;
    ams_mel_rf_virtual_aperture_result_v1 result={0};
    CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER,"c2:element-ok",&parent,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_submit_virtual_aperture(parent,&config,&request,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_wait(request,3000,&result,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(request,&va,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&request,D)==AMS_MEL_OK);
    for(unsigned m=0;m<4;++m) CHECK(calls(m)==0);
    ams_mel_rf_va_local_function_list *a=NULL,*b=NULL,*empty=NULL;
    ams_mel_rf_va_local_function_status *sa=NULL,*sb=NULL,*se=NULL;
    uint32_t scalar=99;
    for(unsigned m=0;m<4;++m) {
        CHECK(invoke(m,va,&scalar,&a,&sa,diag,sizeof diag)==AMS_MEL_OK);
        CHECK(calls(m)==1);
        if(m<2) CHECK(scalar==(m==0 ? 1U : 0U));
    }
    catalog(a,0); status(sa,0);
    CHECK(input(0)==0xDEADBEEFU && input(1)==0x80000001U);
    for(unsigned m=0;m<6;++m) CHECK(e1(m)==0);
    ams_mel_rf_va_instance_status_report *report=NULL;
    CHECK(ams_mel_rf_virtual_aperture_get_instance_status_report(va,0,&report,D)==AMS_MEL_OK);
    CHECK(e1(5)==1 && calls(3)==1);
    CHECK(ams_mel_rf_va_instance_status_report_close(&report,D)==AMS_MEL_OK);
    ams_mel_rf_element_group_snapshot *groups=NULL;
    const ams_mel_rf_element_group_snapshot_options_v1 options={0};
    CHECK(ams_mel_rf_virtual_aperture_get_element_groups(va,&options,&groups,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_element_group_snapshot_close(&groups,D)==AMS_MEL_OK);
    for(unsigned m=0;m<4;++m) CHECK(calls(m)==1);
    change(1);
    CHECK(invoke(0,va,&scalar,&b,&sb,diag,sizeof diag)==AMS_MEL_OK && scalar==0);
    CHECK(invoke(1,va,&scalar,&b,&sb,diag,sizeof diag)==AMS_MEL_OK && scalar==1);
    CHECK(invoke(2,va,&scalar,&b,&sb,diag,sizeof diag)==AMS_MEL_OK);
    CHECK(invoke(3,va,&scalar,&empty,&sb,diag,sizeof diag)==AMS_MEL_OK);
    /* Requested type is absent in B; no precheck; counts and length differ. */
    catalog(b,1); status(sb,1); catalog(a,0); status(sa,0);
    CHECK(ams_mel_rf_va_local_function_list_close(&b,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_va_local_function_status_close(&sb,D)==AMS_MEL_OK);
    change(2);
    CHECK(invoke(2,va,&scalar,&empty,&se,diag,sizeof diag)==AMS_MEL_OK);
    CHECK(invoke(3,va,&scalar,&b,&se,diag,sizeof diag)==AMS_MEL_OK);
    catalog(empty,2); status(se,2);
    change(3); CHECK(invoke(2,va,&scalar,&b,&sb,diag,sizeof diag)==AMS_MEL_OK); catalog(b,3);
    CHECK(ams_mel_rf_va_local_function_list_close(&b,D)==AMS_MEL_OK);
    change(0);
    for(unsigned m=0;m<4;++m) {
        unsigned before[4]; for(unsigned k=0;k<4;++k) before[k]=calls(k);
        scalar=99;
        CHECK(invoke(m,NULL,&scalar,&b,&sb,diag,sizeof diag)==AMS_MEL_INVALID_ARGUMENT);
        CHECK(invoke(m,va,NULL,NULL,NULL,diag,sizeof diag)==AMS_MEL_INVALID_ARGUMENT);
        CHECK(invoke(m,va,&scalar,&b,&sb,NULL,1)==AMS_MEL_INVALID_ARGUMENT);
        if(m>=2) CHECK(invoke(m,va,&scalar,&a,&sa,diag,sizeof diag)==AMS_MEL_INVALID_ARGUMENT);
        CHECK(scalar==99 && !b && !sb);
        for(unsigned k=0;k<4;++k) CHECK(calls(k)==before[k]);
        const char *failures[]={"standard","unknown","allocation"};
        for(unsigned f=0;f<3;++f) {
            CHECK(setenv("AMS_MEL_TEST_LF_EXCEPTION",failures[f],1)==0);
            CHECK(invoke(m,va,&scalar,&b,&sb,diag,sizeof diag)==(f==2 ? AMS_MEL_INTERNAL_ERROR : AMS_MEL_PROVIDER_EXCEPTION));
            CHECK(calls(m)==before[m]+f+1 && scalar==99 && !b && !sb);
            for(unsigned k=0;k<4;++k) if(k!=m) CHECK(calls(k)==before[k]);
        }
        CHECK(unsetenv("AMS_MEL_TEST_LF_EXCEPTION")==0);
        CHECK(invoke(m,va,&scalar,&b,&sb,diag,sizeof diag)==AMS_MEL_OK);
        CHECK(ams_mel_rf_va_local_function_list_close(&b,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_va_local_function_status_close(&sb,D)==AMS_MEL_OK);
    }
    CHECK(setenv("AMS_MEL_TEST_LF_UNKNOWN_STATUS","1",1)==0);
    CHECK(invoke(3,va,&scalar,&b,&sb,diag,sizeof diag)==AMS_MEL_PROVIDER_FAILED && !sb);
    CHECK(unsetenv("AMS_MEL_TEST_LF_UNKNOWN_STATUS")==0); status(sa,0);
    const char *points[]={"lf-list-owner","lf-list-copy","lf-status-owner","lf-status-copy"};
    for(unsigned f=0;f<4;++f) {
        CHECK(setenv("AMS_MEL_TEST_RF_VA_FAILURE",points[f],1)==0);
        CHECK(invoke(f<2 ? 2 : 3,va,&scalar,&b,&sb,diag,sizeof diag)==AMS_MEL_INTERNAL_ERROR && !b && !sb);
        CHECK(unsetenv("AMS_MEL_TEST_RF_VA_FAILURE")==0);
        catalog(a,0); status(sa,0);
        CHECK(invoke(f<2 ? 2 : 3,va,&scalar,&b,&sb,diag,sizeof diag)==AMS_MEL_OK);
        CHECK(ams_mel_rf_va_local_function_list_close(&b,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_va_local_function_status_close(&sb,D)==AMS_MEL_OK);
    }
    ams_mel_rf_va_local_function_info_span_v1 cv={NULL,77};
    ams_mel_u32_span_v1 sv={NULL,88};
    unsigned before[4]; for(unsigned m=0;m<4;++m) before[m]=calls(m);
    CHECK(ams_mel_rf_va_local_function_list_view(NULL,&cv,D)==AMS_MEL_INVALID_ARGUMENT && cv.size==77);
    CHECK(ams_mel_rf_va_local_function_status_view(NULL,&sv,D)==AMS_MEL_INVALID_ARGUMENT && sv.size==88);
    CHECK(ams_mel_rf_va_local_function_list_view(a,NULL,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_va_local_function_status_view(sa,NULL,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_va_local_function_list_view(a,&cv,NULL,1,&required)==AMS_MEL_INVALID_ARGUMENT && cv.size==77);
    CHECK(ams_mel_rf_va_local_function_status_view(sa,&sv,NULL,1,&required)==AMS_MEL_INVALID_ARGUMENT && sv.size==88);
    CHECK(ams_mel_rf_va_local_function_list_close(NULL,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_va_local_function_status_close(NULL,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_va_local_function_list_close(&a,NULL,1,&required)==AMS_MEL_INVALID_ARGUMENT && a);
    CHECK(ams_mel_rf_va_local_function_status_close(&sa,NULL,1,&required)==AMS_MEL_INVALID_ARGUMENT && sa);
    catalog(a,0); status(sa,0);
    for(unsigned m=0;m<4;++m) CHECK(calls(m)==before[m]);
    CHECK(ams_mel_rf_c2_close(&parent,D)==AMS_MEL_OK);
    for(unsigned m=0;m<4;++m) {
        CHECK(invoke(m,va,&scalar,&b,&sb,diag,sizeof diag)==AMS_MEL_OK);
        CHECK(ams_mel_rf_va_local_function_list_close(&b,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_va_local_function_status_close(&sb,D)==AMS_MEL_OK);
    }
    CHECK(dlclose(lib)==0);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK);
    scalar=99;
    for(unsigned m=0;m<4;++m) CHECK(invoke(m,va,&scalar,&b,&sb,diag,sizeof diag)==AMS_MEL_INVALID_ARGUMENT && scalar==99);
    FILE *log=fopen(path,"r"); CHECK(log);
    char text[8192]; size_t n=fread(text,1,sizeof text-1,log); text[n]=0; CHECK(fclose(log)==0);
    char *v=strstr(text,"rf_va_destroyed\n"),*s=strstr(text,"rf_c2_shutdown\n"),*c=strstr(text,"rf_c2_destroyed\n"),*u=strstr(text,"library_unloaded\n");
    CHECK(v && s && c && u && v<s && s<c && c<u && !strstr(text,"rf_forbidden_call\n"));
    catalog(a,0); status(sa,0); catalog(empty,2); status(se,2);
    CHECK(ams_mel_rf_va_local_function_list_close(&a,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_va_local_function_list_close(&a,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_va_local_function_status_close(&sa,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_va_local_function_status_close(&sa,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_va_local_function_list_close(&empty,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_va_local_function_status_close(&se,D)==AMS_MEL_OK);
    CHECK(unlink(path)==0);
    puts("PASS: required booleans, independent live LF snapshots, rollback and actual DSO unload");
    return 0;
}