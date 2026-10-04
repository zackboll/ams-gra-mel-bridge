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
static unsigned (*calls)(unsigned), (*e3)(unsigned), (*e1)(unsigned), (*e4)(unsigned), (*live)(void);
static uint64_t (*input)(unsigned);
static void (*change)(unsigned);
static const ams_mel_string_view_v1 group={"",0}, pipe_key={"a",1};
typedef ams_mel_rf_va_data_pipe_connections_snapshot Snapshot;
static void text(ams_mel_string_view_v1 v, const char *s)
{ if(v.size!=strlen(s)) fprintf(stderr,"text expected '%s', received %zu bytes\n",s,v.size); CHECK(v.size==strlen(s)); if(v.size) CHECK(v.data && !memcmp(v.data,s,v.size)); else CHECK(!v.data); }
static const ams_mel_rf_va_data_pipe_connections_snapshot_v1 *view(Snapshot *s)
{
    const ams_mel_rf_va_data_pipe_connections_snapshot_v1 *v=NULL;
    CHECK(ams_mel_rf_va_data_pipe_connections_snapshot_view(s,&v,D)==AMS_MEL_OK && v);
    return v;
}
static Snapshot *snapshot(ams_mel_rf_virtual_aperture *va)
{
    Snapshot *s=NULL; unsigned before=calls(0);
    CHECK(ams_mel_rf_virtual_aperture_get_data_pipes(va,&s,D)==AMS_MEL_OK && s);
    CHECK(calls(0)==before+1); return s;
}
static void close_snapshot(Snapshot **s)
{ CHECK(ams_mel_rf_va_data_pipe_connections_snapshot_close(s,D)==AMS_MEL_OK && !*s); }
static void check(Snapshot *s, unsigned generation)
{
    const ams_mel_rf_va_data_pipe_connections_snapshot_v1 *v=view(s);
    CHECK(v->groups.size==3 && v->groups.data);
    const ams_mel_rf_va_data_pipe_group_v1 *g=v->groups.data;
    text(g[0].element_group_lookup_label,generation ? "later/group" : "");
    text(g[1].element_group_lookup_label,"rx/main"); text(g[2].element_group_lookup_label,"µ-group");
    CHECK(!g[2].data_pipes.data && !g[2].data_pipes.size);
    for(unsigned i=0;i<2;++i) {
        CHECK(g[i].data_pipes.size==2 && g[i].data_pipes.data);
        const ams_mel_rf_data_pipe_info_v1 *p=g[i].data_pipes.data;
        text(p[0].lookup_label,i ? "default" : generation ? "default" : "a");
        text(p[1].lookup_label,i ? "z" : generation ? "later/pipe" : "default");
        const unsigned populated=generation && !i ? 1U : 0U;
        text(p[populated].label,generation ? "later-β" : "returned-µ");
        text(p[1-populated].label,generation ? "later-β" : "");
        const ams_mel_u64_span_v1 ids=p[populated].associated_endpoint_ids;
        CHECK(ids.data && ids.size==(generation ? 2U : 3U));
        CHECK(ids.data[0]==(generation ? 7U : 0U) && ids.data[ids.size-1]==UINT64_MAX);
        if(!generation) CHECK(ids.data[1]==UINT64_C(0x8000000000000000));
        CHECK(!p[1-populated].associated_endpoint_ids.size && !p[1-populated].associated_endpoint_ids.data);
    }
}
static ams_mel_status_t mutate(ams_mel_rf_virtual_aperture *va, unsigned many,
    ams_mel_string_view_v1 g, ams_mel_string_view_v1 p, uint32_t *accepted)
{
    const uint64_t ids[]={UINT64_MAX,7,0,7,UINT64_C(0x8000000000000000)};
    return many ? ams_mel_rf_virtual_aperture_associate_data_pipe_endpoints(va,g,p,(ams_mel_u64_span_v1){ids,5},accepted,D) :
        ams_mel_rf_virtual_aperture_associate_data_pipe_endpoint(va,g,p,UINT64_MAX,accepted,D);
}
int main(void)
{
    char path[]="/tmp/ams-rf-pipes-XXXXXX";
    int fd=mkstemp(path); CHECK(fd>=0 && close(fd)==0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG",path,1)==0);
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL); CHECK(lib);
#define SYMBOL(target,name) do { void *sym=dlsym(lib,name); memcpy(&target,&sym,sizeof target); CHECK(target); } while(0)
    SYMBOL(calls,"mock_rf_connection_calls"); SYMBOL(e3,"mock_rf_element_calls");
    SYMBOL(e1,"mock_rf_va_query_calls"); SYMBOL(e4,"mock_rf_va_lf_calls");
    SYMBOL(live,"mock_rf_connection_live"); SYMBOL(change,"mock_rf_connection_change");
    SYMBOL(input,"mock_rf_connection_input");
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
    for(unsigned m=0;m<5;++m) CHECK(calls(m)==0); // Claim does not query E5.
    ams_mel_rf_element_group_snapshot *descriptor=NULL;
    const ams_mel_rf_element_group_snapshot_options_v1 options={1};
    CHECK(ams_mel_rf_virtual_aperture_get_element_groups(va,&options,&descriptor,D)==AMS_MEL_OK);
    CHECK(e3(7)==3 && calls(0)==0);
    CHECK(ams_mel_rf_element_group_snapshot_close(&descriptor,D)==AMS_MEL_OK);
    unsigned e3_before[10]; for(unsigned m=0;m<10;++m) e3_before[m]=e3(m);
    Snapshot *a=snapshot(va), *b=NULL;
    CHECK(calls(0)==1 && calls(1)==4 && calls(2)==4 && !live()); check(a,0);
    const uint64_t single[]={0,UINT64_C(0x8000000000000000),UINT64_MAX};
    uint32_t accepted=99;
    for(unsigned i=0;i<3;++i) {
        unsigned q=calls(0), s=calls(3), sets=calls(4), labels=calls(1), ids=calls(2);
        CHECK(ams_mel_rf_virtual_aperture_associate_data_pipe_endpoint(va,group,pipe_key,single[i],&accepted,D)==AMS_MEL_OK && accepted==1);
        CHECK(input(0)==single[i] && calls(0)==q+1 && calls(3)==s+1 && calls(4)==sets);
        CHECK(calls(1)==labels && calls(2)==ids && !live());
    }
    unsigned q=calls(0), s=calls(3), sets=calls(4);
    CHECK(mutate(va,1,group,pipe_key,&accepted)==AMS_MEL_OK && accepted==1);
    CHECK(calls(0)==q+1 && calls(3)==s && calls(4)==sets+1 && input(5)==4);
    const uint64_t expected[]={0,7,UINT64_C(0x8000000000000000),UINT64_MAX};
    for(unsigned i=0;i<4;++i) CHECK(input(i+1)==expected[i]);
    sets=calls(4);
    CHECK(ams_mel_rf_virtual_aperture_associate_data_pipe_endpoints(va,group,pipe_key,(ams_mel_u64_span_v1){NULL,0},&accepted,D)==AMS_MEL_OK && accepted==1);
    CHECK(calls(4)==sets+1 && input(5)==0);
    // Fresh-object provider: true association does not force future persistence.
    b=snapshot(va); check(b,0); close_snapshot(&b);
    CHECK(setenv("AMS_MEL_TEST_CONNECTION_CASE","false",1)==0);
    for(unsigned many=0;many<2;++many) for(unsigned retry=0;retry<2;++retry) {
        q=calls(0); s=calls(3+many); accepted=99;
        CHECK(mutate(va,many,group,pipe_key,&accepted)==AMS_MEL_OK && accepted==0);
        CHECK(calls(0)==q+1 && calls(3+many)==s+1);
    }
    CHECK(unsetenv("AMS_MEL_TEST_CONNECTION_CASE")==0);
    // Invalid caller values must make no provider call or publish any output.
    const ams_mel_string_view_v1 invalid[]={{NULL,1},{"x\0y",3},{"\xc0\xaf",2},{"",SIZE_MAX}};
    q=calls(0);
    CHECK(ams_mel_rf_virtual_aperture_get_data_pipes(NULL,&b,D)==AMS_MEL_INVALID_ARGUMENT && !b);
    CHECK(ams_mel_rf_virtual_aperture_get_data_pipes(va,NULL,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_virtual_aperture_get_data_pipes(va,&a,D)==AMS_MEL_INVALID_ARGUMENT && a);
    CHECK(ams_mel_rf_virtual_aperture_get_data_pipes(va,&b,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT && !b);
    for(unsigned many=0;many<2;++many) {
        accepted=99;
        CHECK(mutate(NULL,many,group,pipe_key,&accepted)==AMS_MEL_INVALID_ARGUMENT && accepted==99);
        CHECK(mutate(va,many,group,pipe_key,NULL)==AMS_MEL_INVALID_ARGUMENT);
        for(unsigned i=0;i<4;++i) {
            CHECK(mutate(va,many,invalid[i],pipe_key,&accepted)==AMS_MEL_INVALID_ARGUMENT && accepted==99);
            CHECK(mutate(va,many,group,invalid[i],&accepted)==AMS_MEL_INVALID_ARGUMENT && accepted==99);
        }
    }
    CHECK(ams_mel_rf_virtual_aperture_associate_data_pipe_endpoint(va,group,pipe_key,0,&accepted,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_virtual_aperture_associate_data_pipe_endpoints(va,group,pipe_key,(ams_mel_u64_span_v1){NULL,1},&accepted,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_virtual_aperture_associate_data_pipe_endpoints(va,group,pipe_key,(ams_mel_u64_span_v1){single,SIZE_MAX},&accepted,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_virtual_aperture_associate_data_pipe_endpoints(va,group,pipe_key,(ams_mel_u64_span_v1){single,3},&accepted,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(calls(0)==q);
    for(unsigned missing=0;missing<3;++missing) for(unsigned many=0;many<2;++many) {
        if(missing==2) CHECK(setenv("AMS_MEL_TEST_CONNECTION_CASE","null",1)==0);
        accepted=99; q=calls(0); s=calls(3); sets=calls(4);
        CHECK(mutate(va,many,missing==0 ? (ams_mel_string_view_v1){"missing",7} : group,
            missing==1 ? (ams_mel_string_view_v1){"missing",7} : pipe_key,&accepted)==AMS_MEL_PROVIDER_FAILED && accepted==99);
        CHECK(strstr(diag,missing==0 ? "ElementGroup" : missing==1 ? "DataPipe lookup" : "null"));
        CHECK(calls(0)==q+1 && calls(3)==s && calls(4)==sets);
        CHECK(unsetenv("AMS_MEL_TEST_CONNECTION_CASE")==0);
        CHECK(mutate(va,many,group,pipe_key,&accepted)==AMS_MEL_OK);
    }
    const char *categories[]={"outer","inner","label"};
    for(unsigned c=0;c<3;++c) for(unsigned nul=0;nul<2;++nul) {
        CHECK(setenv("AMS_MEL_TEST_CONNECTION_BAD_STRING",categories[c],1)==0);
        CHECK(setenv("AMS_MEL_TEST_CONNECTION_CASE",nul ? "nul" : "utf8",1)==0);
        CHECK(ams_mel_rf_virtual_aperture_get_data_pipes(va,&b,D)==AMS_MEL_PROVIDER_FAILED && !b && !live());
        CHECK(unsetenv("AMS_MEL_TEST_CONNECTION_BAD_STRING")==0 && unsetenv("AMS_MEL_TEST_CONNECTION_CASE")==0);
        check(a,0); b=snapshot(va); close_snapshot(&b);
    }
    CHECK(setenv("AMS_MEL_TEST_CONNECTION_CASE","null",1)==0);
    CHECK(ams_mel_rf_virtual_aperture_get_data_pipes(va,&b,D)==AMS_MEL_PROVIDER_FAILED && !b && !live());
    CHECK(unsetenv("AMS_MEL_TEST_CONNECTION_CASE")==0);
    const char *kinds[]={"standard","unknown","allocation"};
    for(unsigned method=0;method<5;++method) for(unsigned kind=0;kind<3;++kind) {
        char number[8]; CHECK(snprintf(number,sizeof number,"%u",method)>0);
        CHECK(setenv("AMS_MEL_TEST_CONNECTION_THROW_METHOD",number,1)==0);
        CHECK(setenv("AMS_MEL_TEST_CONNECTION_THROW_KIND",kinds[kind],1)==0);
        const ams_mel_status_t failure=kind==2 ? AMS_MEL_INTERNAL_ERROR : AMS_MEL_PROVIDER_EXCEPTION;
        q=calls(method); accepted=99;
        if(method<3) CHECK(ams_mel_rf_virtual_aperture_get_data_pipes(va,&b,D)==failure && !b);
        else CHECK(mutate(va,method==4,group,pipe_key,&accepted)==failure && accepted==99);
        CHECK(calls(method)==q+1 && !live());
        if(method==0) for(unsigned many=0;many<2;++many) CHECK(mutate(va,many,group,pipe_key,&accepted)==failure && accepted==99);
        CHECK(unsetenv("AMS_MEL_TEST_CONNECTION_THROW_METHOD")==0 && unsetenv("AMS_MEL_TEST_CONNECTION_THROW_KIND")==0);
        check(a,0); b=snapshot(va); close_snapshot(&b);
    }
    const char *points[]={"connections-owner","connections-group","connections-pipe","connections-endpoints"};
    for(unsigned i=0;i<4;++i) {
        CHECK(setenv("AMS_MEL_TEST_RF_VA_FAILURE",points[i],1)==0);
        CHECK(ams_mel_rf_virtual_aperture_get_data_pipes(va,&b,D)==AMS_MEL_INTERNAL_ERROR && !b && !live());
        CHECK(unsetenv("AMS_MEL_TEST_RF_VA_FAILURE")==0); check(a,0); b=snapshot(va); close_snapshot(&b);
    }
    const char *cases[]={"empty","empty-groups","long"};
    for(unsigned i=0;i<3;++i) {
        CHECK(setenv("AMS_MEL_TEST_CONNECTION_CASE",cases[i],1)==0); b=snapshot(va);
        const ams_mel_rf_va_data_pipe_connections_snapshot_v1 *v=view(b);
        if(!i) CHECK(!v->groups.size && !v->groups.data);
        else if(i==1) { CHECK(v->groups.size==3); for(unsigned j=0;j<3;++j) CHECK(!v->groups.data[j].data_pipes.size && !v->groups.data[j].data_pipes.data); }
        else { const ams_mel_rf_data_pipe_info_v1 *p=v->groups.data[0].data_pipes.data; CHECK(p[0].lookup_label.size==90 && p[0].label.size==102); }
        close_snapshot(&b); CHECK(unsetenv("AMS_MEL_TEST_CONNECTION_CASE")==0);
    }
    change(1); b=snapshot(va); check(b,1); check(a,0); close_snapshot(&b); check(a,0); change(0);
    // Persistent shared object is provider behavior: alias snapshot observes it.
    CHECK(setenv("AMS_MEL_TEST_CONNECTION_CASE","persistent",1)==0);
    CHECK(ams_mel_rf_virtual_aperture_associate_data_pipe_endpoint(va,group,pipe_key,9,&accepted,D)==AMS_MEL_OK);
    b=snapshot(va);
    const ams_mel_rf_va_data_pipe_connections_snapshot_v1 *v=view(b);
    CHECK(v->groups.data[0].data_pipes.data[0].associated_endpoint_ids.size==4);
    CHECK(v->groups.data[1].data_pipes.data[0].associated_endpoint_ids.size==4);
    CHECK(v->groups.data[1].data_pipes.data[0].associated_endpoint_ids.data[1]==9);
    close_snapshot(&b); check(a,0); CHECK(unsetenv("AMS_MEL_TEST_CONNECTION_CASE")==0);
    // Views/Close validate without allocation/provider calls.
    const ams_mel_rf_va_data_pipe_connections_snapshot_v1 *untouched=view(a), *original=untouched;
    q=calls(0);
    CHECK(ams_mel_rf_va_data_pipe_connections_snapshot_view(NULL,&untouched,D)==AMS_MEL_INVALID_ARGUMENT && untouched==original);
    CHECK(ams_mel_rf_va_data_pipe_connections_snapshot_view(a,&untouched,D)==AMS_MEL_INVALID_ARGUMENT && untouched==original);
    CHECK(ams_mel_rf_va_data_pipe_connections_snapshot_view(a,NULL,D)==AMS_MEL_INVALID_ARGUMENT);
    untouched=NULL;
    CHECK(ams_mel_rf_va_data_pipe_connections_snapshot_view(a,&untouched,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT && !untouched);
    CHECK(ams_mel_rf_va_data_pipe_connections_snapshot_close(NULL,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_va_data_pipe_connections_snapshot_close(&a,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT && a);
    check(a,0); CHECK(calls(0)==q);
    for(unsigned m=0;m<10;++m) CHECK(e3(m)==e3_before[m]);
    for(unsigned m=0;m<6;++m) CHECK(e1(m)==0);
    for(unsigned m=0;m<4;++m) CHECK(e4(m)==0);
    CHECK(ams_mel_rf_c2_close(&parent,D)==AMS_MEL_OK);
    b=snapshot(va); check(b,0); close_snapshot(&b);
    for(unsigned many=0;many<2;++many) CHECK(mutate(va,many,group,pipe_key,&accepted)==AMS_MEL_OK && accepted==1);
    CHECK(dlclose(lib)==0); // Own reference gone; no registrations/Job/ProductRx.
    CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK && !va);
    CHECK(ams_mel_rf_virtual_aperture_get_data_pipes(va,&b,D)==AMS_MEL_INVALID_ARGUMENT && !b);
    accepted=99;
    for(unsigned many=0;many<2;++many) CHECK(mutate(va,many,group,pipe_key,&accepted)==AMS_MEL_INVALID_ARGUMENT && accepted==99);
    FILE *f=fopen(path,"r"); CHECK(f); char log[65536]; size_t n=fread(log,1,sizeof log-1,f); log[n]=0; CHECK(fclose(f)==0);
    char *destroy=strstr(log,"rf_va_destroyed\n"), *shutdown=strstr(log,"rf_c2_shutdown\n"), *c2=strstr(log,"rf_c2_destroyed\n"), *unload=strstr(log,"library_unloaded\n");
    CHECK(destroy && shutdown && c2 && unload && destroy<shutdown && shutdown<c2 && c2<unload && !strstr(log,"rf_forbidden_call\n"));
    check(a,0); close_snapshot(&a); close_snapshot(&a); CHECK(unlink(path)==0);
    puts("PASS: VA DataPipe snapshots, exact set mutations, independent calls and actual DSO unload");
    return 0;
}
