#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
#define D diag, sizeof diag, &required
static char diag[512];
static size_t required;
static unsigned (*calls)(unsigned);
static uint32_t (*input)(unsigned);
static ams_mel_rf_virtual_aperture *claim(ams_mel_rf_c2 **parent)
{
    static const ams_mel_string_view_v1 local[] = {{"alpha",5},{"µ-local",8},{"",0}};
    ams_mel_uci_id_v1 caps[3] = {{{0},{"first",5}},{{0},{"",0}},{{0},{"µ-third",8}}};
    caps[0].uuid[1]=0x80; caps[0].uuid[2]=0xff; caps[1].uuid[0]=0xff; caps[2].uuid[15]=0x80;
    ams_mel_rf_virtual_aperture_config_v1 config = {0xFEDCBA98U,0x80000001U,{local,3},{"definition/β.json",18},{caps,3}};
    ams_mel_rf_virtual_aperture_request *request=NULL;
    ams_mel_rf_virtual_aperture *va=NULL;
    ams_mel_rf_virtual_aperture_result_v1 result={0};
    CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER,"c2:va-query-changing",parent,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_submit_virtual_aperture(*parent,&config,&request,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_wait(request,3000,&result,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(request,&va,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&request,D)==AMS_MEL_OK);
    return va;
}
static void check_list(ams_mel_rf_va_instance_list *list, const uint32_t *ids, size_t count)
{
    ams_mel_u32_span_v1 span={NULL,0};
    CHECK(ams_mel_rf_va_instance_list_view(list,&span,D)==AMS_MEL_OK);
    CHECK(span.size==count);
    if (count) CHECK(span.data && memcmp(span.data,ids,count*sizeof *ids)==0);
    else CHECK(span.data==NULL);
}
static void check_report(ams_mel_rf_va_instance_status_report *report, uint32_t status)
{
    const ams_mel_rf_va_instance_status_report_v1 *view=NULL;
    const uint32_t first[]={3,0,2,1}, last[]={1,1,3};
    CHECK(ams_mel_rf_va_instance_status_report_view(report,&view,D)==AMS_MEL_OK);
    CHECK(view->va_instance_id==42 && view->status==status && view->local_functions.size==3);
    const ams_mel_rf_va_local_function_status_v1 *g=view->local_functions.data;
    CHECK(g && g[0].local_function_type_id==0 && g[1].local_function_type_id==0x80000001U && g[2].local_function_type_id==UINT32_MAX);
    CHECK(g[0].statuses.size==4 && !memcmp(g[0].statuses.data,first,sizeof first));
    CHECK(g[1].statuses.size==0 && g[1].statuses.data==NULL);
    CHECK(g[2].statuses.size==3 && !memcmp(g[2].statuses.data,last,sizeof last));
}
static ams_mel_status_t invoke(unsigned method, ams_mel_rf_virtual_aperture *va,
    uint32_t *scalar, ams_mel_rf_va_instance_list **list, ams_mel_rf_va_instance_status_report **report)
{
    switch(method) {
    case 0: return ams_mel_rf_virtual_aperture_get_id(va,scalar,D);
    case 1: return ams_mel_rf_virtual_aperture_get_status(va,scalar,D);
    case 2: return ams_mel_rf_virtual_aperture_get_instance_status(va,0xDEADBEEFU,scalar,D);
    case 3: return ams_mel_rf_virtual_aperture_get_all_instances(va,list,D);
    case 4: return ams_mel_rf_virtual_aperture_get_instances(va,0xFEDCBA98U,list,D);
    case 5: return ams_mel_rf_virtual_aperture_get_instance_status_report(va,0xDEADBEEFU,report,D);
    default: return AMS_MEL_INTERNAL_ERROR;
    }
}
int main(void)
{
    char path[]="/tmp/ams-rf-va-query-XXXXXX";
    int fd=mkstemp(path); CHECK(fd>=0 && close(fd)==0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG",path,1)==0);
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL); CHECK(lib);
    *(void **)(&calls)=dlsym(lib,"mock_rf_va_query_calls");
    *(void **)(&input)=dlsym(lib,"mock_rf_va_query_input"); CHECK(calls && input);
    ams_mel_rf_c2 *parent=NULL;
    ams_mel_rf_virtual_aperture *va=claim(&parent);
    for(unsigned m=0;m<6;++m) CHECK(calls(m)==0); // Claim reads none of these.
    const ams_mel_rf_virtual_aperture_info_v1 *info=NULL;
    CHECK(ams_mel_rf_virtual_aperture_view(va,&info,D)==AMS_MEL_OK);
    CHECK(info->va_instance_ids.size==3 && info->va_instance_ids.data[0]==0 && info->va_instance_ids.data[1]==3 && info->va_instance_ids.data[2]==9);
    for(unsigned m=0;m<6;++m) {
        uint32_t scalar=0x12345678U;
        ams_mel_rf_va_instance_list *list=NULL;
        ams_mel_rf_va_instance_status_report *report=NULL;
        CHECK(invoke(m,NULL,&scalar,&list,&report)==AMS_MEL_INVALID_ARGUMENT);
        CHECK(invoke(m,va,NULL,NULL,NULL)==AMS_MEL_INVALID_ARGUMENT);
        CHECK(scalar==0x12345678U && !list && !report && calls(m)==0);
    }
    uint32_t scalar=0;
    CHECK(ams_mel_rf_virtual_aperture_get_id(va,&scalar,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(calls(0)==0);
    CHECK(ams_mel_rf_virtual_aperture_get_status(va,&scalar,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT && calls(1)==0);
    CHECK(ams_mel_rf_virtual_aperture_get_instance_status(va,UINT32_MAX,&scalar,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT && calls(2)==0);
    ams_mel_rf_va_instance_list *a=NULL,*b=NULL,*empty=NULL;
    ams_mel_rf_va_instance_status_report *ra=NULL,*rb=NULL;
    CHECK(ams_mel_rf_virtual_aperture_get_all_instances(va,&a,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT && calls(3)==0);
    CHECK(ams_mel_rf_virtual_aperture_get_instances(va,UINT32_MAX,&a,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT && calls(4)==0);
    CHECK(ams_mel_rf_virtual_aperture_get_instance_status_report(va,UINT32_MAX,&ra,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT && calls(5)==0);
    CHECK(ams_mel_rf_virtual_aperture_get_id(va,&scalar,D)==AMS_MEL_OK && scalar==0x87654321U && calls(0)==1);
    for(uint32_t s=0;s<4;++s) CHECK(ams_mel_rf_virtual_aperture_get_status(va,&scalar,D)==AMS_MEL_OK && scalar==s && calls(1)==s+1);
    CHECK(ams_mel_rf_virtual_aperture_get_instance_status(va,UINT32_MAX,&scalar,D)==AMS_MEL_OK && scalar==3 && input(2)==UINT32_MAX && calls(2)==1);
    CHECK(ams_mel_rf_virtual_aperture_get_all_instances(va,&a,D)==AMS_MEL_OK && calls(3)==1);
    const uint32_t ids[]={UINT32_MAX,7,0,7}, changed[]={2,42}, faces[]={42,2,42};
    check_list(a,ids,4);
    CHECK(ams_mel_rf_virtual_aperture_get_all_instances(va,&b,D)==AMS_MEL_OK && calls(3)==2);
    check_list(b,changed,2); check_list(a,ids,4);
    CHECK(ams_mel_rf_va_instance_list_close(&b,D)==AMS_MEL_OK); check_list(a,ids,4);
    CHECK(ams_mel_rf_virtual_aperture_get_instances(va,0xFEDCBA98U,&b,D)==AMS_MEL_OK && calls(4)==1 && input(4)==0xFEDCBA98U);
    check_list(b,faces,3);
    CHECK(ams_mel_rf_virtual_aperture_get_instances(va,UINT32_MAX,&empty,D)==AMS_MEL_OK && calls(4)==2 && input(4)==UINT32_MAX); check_list(empty,NULL,0);
    CHECK(ams_mel_rf_virtual_aperture_get_instance_status_report(va,0xDEADBEEFU,&ra,D)==AMS_MEL_OK && calls(5)==1 && input(5)==0xDEADBEEFU);
    check_report(ra,2);
    CHECK(ams_mel_rf_virtual_aperture_get_instance_status_report(va,UINT32_MAX,&rb,D)==AMS_MEL_OK && calls(5)==2 && input(5)==UINT32_MAX);
    check_report(rb,1); check_report(ra,2);
    CHECK(ams_mel_rf_va_instance_status_report_close(&rb,D)==AMS_MEL_OK); check_report(ra,2);
    // Exact separate-call totals prove no query implemented via another method.
    const unsigned expected_calls[]={1,4,1,2,2,2};
    for(unsigned m=0;m<6;++m) CHECK(calls(m)==expected_calls[m]);
    unsigned before[6]; for(unsigned m=0;m<6;++m) before[m]=calls(m);
    ams_mel_rf_va_instance_list *original=a;
    CHECK(ams_mel_rf_virtual_aperture_get_all_instances(va,&a,D)==AMS_MEL_INVALID_ARGUMENT && a==original);
    CHECK(ams_mel_rf_virtual_aperture_get_instances(va,0,&a,D)==AMS_MEL_INVALID_ARGUMENT && a==original);
    ams_mel_rf_va_instance_status_report *original_report=ra;
    CHECK(ams_mel_rf_virtual_aperture_get_instance_status_report(va,0,&ra,D)==AMS_MEL_INVALID_ARGUMENT && ra==original_report);
    ams_mel_u32_span_v1 sentinel={ids,99};
    CHECK(ams_mel_rf_va_instance_list_view(NULL,&sentinel,D)==AMS_MEL_INVALID_ARGUMENT && sentinel.data==ids && sentinel.size==99);
    CHECK(ams_mel_rf_va_instance_list_view(a,&sentinel,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT && sentinel.size==99);
    const ams_mel_rf_va_instance_status_report_v1 *sentinel_report=(const void *)&scalar;
    CHECK(ams_mel_rf_va_instance_status_report_view(ra,&sentinel_report,D)==AMS_MEL_INVALID_ARGUMENT && sentinel_report==(const void *)&scalar);
    sentinel_report=NULL;
    CHECK(ams_mel_rf_va_instance_status_report_view(ra,&sentinel_report,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT && !sentinel_report);
    for(unsigned m=0;m<6;++m) CHECK(before[m]==calls(m));
    const char *exceptions[]={"standard","unknown","allocation"};
    for(unsigned e=0;e<3;++e) for(unsigned m=0;m<6;++m) {
        ams_mel_rf_va_instance_list *list=NULL;
        ams_mel_rf_va_instance_status_report *report=NULL;
        scalar=0x12345678U; unsigned n=calls(m);
        unsigned others[6]; for(unsigned k=0;k<6;++k) others[k]=calls(k);
        CHECK(setenv("AMS_MEL_TEST_VA_QUERY_EXCEPTION",exceptions[e],1)==0);
        CHECK(invoke(m,va,&scalar,&list,&report)==(e==2 ? AMS_MEL_INTERNAL_ERROR : AMS_MEL_PROVIDER_EXCEPTION));
        CHECK(calls(m)==n+1 && scalar==0x12345678U && !list && !report);
        CHECK(unsetenv("AMS_MEL_TEST_VA_QUERY_EXCEPTION")==0);
        CHECK(invoke(m,va,&scalar,&list,&report)==AMS_MEL_OK && calls(m)==n+2);
        for(unsigned k=0;k<6;++k) if(k!=m) CHECK(calls(k)==others[k]);
        CHECK(ams_mel_rf_va_instance_list_close(&list,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_va_instance_status_report_close(&report,D)==AMS_MEL_OK);
    }
    CHECK(setenv("AMS_MEL_TEST_VA_UNKNOWN_STATUS","1",1)==0);
    scalar=0x12345678U;
    CHECK(ams_mel_rf_virtual_aperture_get_status(va,&scalar,D)==AMS_MEL_PROVIDER_FAILED && scalar==0x12345678U);
    CHECK(ams_mel_rf_virtual_aperture_get_instance_status(va,0,&scalar,D)==AMS_MEL_PROVIDER_FAILED && scalar==0x12345678U);
    CHECK(ams_mel_rf_virtual_aperture_get_instance_status_report(va,0,&rb,D)==AMS_MEL_PROVIDER_FAILED && !rb);
    CHECK(unsetenv("AMS_MEL_TEST_VA_UNKNOWN_STATUS")==0);
    CHECK(setenv("AMS_MEL_TEST_VA_BAD_REPORT","1",1)==0);
    CHECK(ams_mel_rf_virtual_aperture_get_instance_status_report(va,0,&rb,D)==AMS_MEL_PROVIDER_FAILED && !rb);
    CHECK(unsetenv("AMS_MEL_TEST_VA_BAD_REPORT")==0); check_report(ra,2);
    const char *allocations[]={"query-list-owner","query-list-copy","query-report-owner","query-report-nested"};
    for(unsigned f=0;f<4;++f) {
        ams_mel_rf_va_instance_list *list=NULL;
        CHECK(setenv("AMS_MEL_TEST_RF_VA_FAILURE",allocations[f],1)==0);
        CHECK(invoke(f<2 ? 3 : 5,va,&scalar,&list,&rb)==AMS_MEL_INTERNAL_ERROR && !list && !rb);
        CHECK(unsetenv("AMS_MEL_TEST_RF_VA_FAILURE")==0);
        CHECK(invoke(f<2 ? 3 : 5,va,&scalar,&list,&rb)==AMS_MEL_OK);
        CHECK(ams_mel_rf_va_instance_list_close(&list,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_va_instance_status_report_close(&rb,D)==AMS_MEL_OK);
        check_list(a,ids,4); check_report(ra,2);
    }
    CHECK(ams_mel_rf_c2_close(&parent,D)==AMS_MEL_OK);
    for(unsigned m=0;m<6;++m) {
        ams_mel_rf_va_instance_list *list=NULL;
        CHECK(invoke(m,va,&scalar,&list,&rb)==AMS_MEL_OK);
        CHECK(ams_mel_rf_va_instance_list_close(&list,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_va_instance_status_report_close(&rb,D)==AMS_MEL_OK);
    }
    CHECK(info->va_instance_ids.size==3 && info->va_instance_ids.data[2]==9);
    // Release the test dlopen reference BEFORE VA closure/unload assertion.
    CHECK(dlclose(lib)==0);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK && !va);
    for(unsigned m=0;m<6;++m) CHECK(invoke(m,va,&scalar,&a,&rb)==AMS_MEL_INVALID_ARGUMENT);
    FILE *log=fopen(path,"r"); CHECK(log);
    char text[8192]; size_t length=fread(text,1,sizeof text-1,log); text[length]=0; CHECK(fclose(log)==0);
    char *v=strstr(text,"rf_va_destroyed\n"), *s=strstr(text,"rf_c2_shutdown\n"), *c=strstr(text,"rf_c2_destroyed\n"), *u=strstr(text,"library_unloaded\n");
    CHECK(v && s && c && u && v<s && s<c && c<u && !strstr(text,"rf_forbidden_call\n"));
    check_list(a,ids,4); check_list(b,faces,3); check_list(empty,NULL,0); check_report(ra,2);
    CHECK(ams_mel_rf_va_instance_list_close(&a,D)==AMS_MEL_OK && !a);
    CHECK(ams_mel_rf_va_instance_list_close(&a,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_va_instance_list_close(&b,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_va_instance_list_close(&empty,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_va_instance_status_report_close(&ra,D)==AMS_MEL_OK && !ra);
    CHECK(ams_mel_rf_va_instance_status_report_close(&ra,D)==AMS_MEL_OK);
    CHECK(unlink(path)==0);
    puts("PASS: RF live VA queries, nested snapshots, failures and isolated DSO unload");
    return 0;
}
