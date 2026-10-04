#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d %s (%s)\n",__FILE__,__LINE__,#x,diag); exit(1); } } while (0)
#define D diag, sizeof diag, &required
static char diag[512];
static size_t required;
static unsigned (*calls)(unsigned), (*live)(unsigned), (*queries)(unsigned);
static void (*change)(unsigned);
static ams_mel_rf_element_group_snapshot_options_v1 options={0};
static ams_mel_rf_virtual_aperture *claim(ams_mel_rf_c2 **parent)
{
    static const ams_mel_string_view_v1 local[]={{"alpha",5},{"µ-local",8},{"",0}};
    ams_mel_uci_id_v1 caps[3]={{{0},{"first",5}},{{0},{"",0}},{{0},{"µ-third",8}}};
    caps[0].uuid[1]=0x80; caps[0].uuid[2]=0xff; caps[1].uuid[0]=0xff; caps[2].uuid[15]=0x80;
    ams_mel_rf_virtual_aperture_config_v1 config={0xFEDCBA98U,0x80000001U,{local,3},{"definition/β.json",18},{caps,3}};
    ams_mel_rf_virtual_aperture_request *request=NULL;
    ams_mel_rf_virtual_aperture *va=NULL;
    ams_mel_rf_virtual_aperture_result_v1 result={0};
    CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER,"c2:element-ok",parent,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_submit_virtual_aperture(*parent,&config,&request,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_wait(request,3000,&result,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(request,&va,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&request,D)==AMS_MEL_OK);
    return va;
}
static void text(ams_mel_string_view_v1 value, const char *expected)
{ CHECK(value.size==strlen(expected)); if(value.size) CHECK(value.data && !memcmp(value.data,expected,value.size)); }
static const ams_mel_rf_element_group_snapshot_v1 *view(ams_mel_rf_element_group_snapshot *owner)
{
    const ams_mel_rf_element_group_snapshot_v1 *value=NULL;
    // calls is unavailable only after the actual DSO-unload boundary below.
    CHECK(ams_mel_rf_element_group_snapshot_view(owner,&value,D)==AMS_MEL_OK && value);
    return value;
}
static void check(ams_mel_rf_element_group_snapshot *owner, unsigned included, unsigned generation)
{
    const ams_mel_rf_element_group_snapshot_v1 *v=view(owner);
    CHECK(v->data_pipes_included==included && v->descriptors.size==3 && v->descriptors.data);
    const ams_mel_rf_element_group_descriptor_v1 *g=v->descriptors.data;
    text(g[0].lookup_label,""); text(g[1].lookup_label,"z-map"); text(g[2].lookup_label,"µ-map");
    CHECK(g[0].mode==AMS_MEL_RF_ELEMENT_GROUP_MODE_RX && g[1].mode==AMS_MEL_RF_ELEMENT_GROUP_MODE_TX);
    for(size_t i=0;i<3;++i) {
        CHECK(g[i].max_rf_bandwidth_hz==(generation ? 42.25 : 12345678.25));
        CHECK(g[i].max_sample_rate_samples_per_second==2500000.5 && g[i].max_data_rate_bits_per_second==987654321.125 && g[i].max_duty_factor==0.625);
        text(g[i].label,generation ? "descriptor-later-β" : i==0 ? "returned-empty-key" : i==1 ? "returned-z" : "returned-µ");
        if(!included || i==1) { CHECK(!g[i].data_pipes.data && !g[i].data_pipes.size); continue; }
        CHECK(g[i].data_pipes.size==2 && g[i].data_pipes.data);
        const ams_mel_rf_data_pipe_info_v1 *p=g[i].data_pipes.data;
        text(p[0].lookup_label,"a-pipe"); text(p[1].lookup_label,"µ-pipe");
        text(p[0].label,generation ? "pipe-later-β" : "pipe-returned-µ");
        text(p[1].label,generation ? "pipe-later-β" : "");
        CHECK(!p[1].associated_endpoint_ids.data && !p[1].associated_endpoint_ids.size);
        CHECK(p[0].associated_endpoint_ids.size==(generation ? 2U : 3U));
        const uint64_t *ids=p[0].associated_endpoint_ids.data; CHECK(ids);
        CHECK(ids[0]==(generation ? 7U : 0U));
        if(!generation) CHECK(ids[1]==UINT64_C(0x8000000000000000));
        CHECK(ids[generation ? 1 : 2]==UINT64_MAX);
    }
}
static ams_mel_rf_element_group_snapshot *snapshot(ams_mel_rf_virtual_aperture *va, unsigned included)
{
    ams_mel_rf_element_group_snapshot *owner=NULL;
    unsigned before[10]; for(unsigned m=0;m<10;++m) before[m]=calls(m);
    options.include_data_pipes=included;
    CHECK(ams_mel_rf_virtual_aperture_get_element_groups(va,&options,&owner,D)==AMS_MEL_OK && owner);
    CHECK(calls(0)==before[0]+1);
    for(unsigned m=1;m<7;++m) CHECK(calls(m)==before[m]+3);
    CHECK(calls(7)==before[7]+(included ? 3U : 0U));
    for(unsigned m=8;m<10;++m) CHECK(calls(m)==before[m]+(included ? 4U : 0U));
    CHECK(!live(0) && !live(1));
    return owner;
}
static void close_snapshot(ams_mel_rf_element_group_snapshot **owner)
{
    unsigned before[10]; for(unsigned m=0;m<10;++m) before[m]=calls(m);
    CHECK(ams_mel_rf_element_group_snapshot_close(owner,D)==AMS_MEL_OK && !*owner);
    for(unsigned m=0;m<10;++m) CHECK(calls(m)==before[m]);
}
int main(void)
{
    char path[]="/tmp/ams-element-XXXXXX"; int fd=mkstemp(path); CHECK(fd>=0 && close(fd)==0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG",path,1)==0);
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL); CHECK(lib);
    void *symbol=dlsym(lib,"mock_rf_element_calls"); memcpy(&calls,&symbol,sizeof calls); CHECK(calls);
    symbol=dlsym(lib,"mock_rf_element_live"); memcpy(&live,&symbol,sizeof live); CHECK(live);
    symbol=dlsym(lib,"mock_rf_element_change"); memcpy(&change,&symbol,sizeof change); CHECK(change);
    symbol=dlsym(lib,"mock_rf_va_query_calls"); memcpy(&queries,&symbol,sizeof queries); CHECK(queries);
    ams_mel_rf_c2 *parent=NULL; ams_mel_rf_virtual_aperture *va=claim(&parent);
    for(unsigned m=0;m<10;++m) CHECK(calls(m)==0); // Claim unchanged
    ams_mel_rf_element_group_snapshot *a=snapshot(va,1), *basic=snapshot(va,0), *b=NULL;
    check(a,1,0); check(basic,0,0);
    unsigned before=calls(0);
    CHECK(ams_mel_rf_virtual_aperture_get_element_groups(NULL,&options,&b,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_virtual_aperture_get_element_groups(va,NULL,&b,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_virtual_aperture_get_element_groups(va,&options,NULL,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_virtual_aperture_get_element_groups(va,&options,&a,D)==AMS_MEL_INVALID_ARGUMENT);
    options.include_data_pipes=2;
    CHECK(ams_mel_rf_virtual_aperture_get_element_groups(va,&options,&b,D)==AMS_MEL_INVALID_ARGUMENT);
    options.include_data_pipes=0;
    CHECK(ams_mel_rf_virtual_aperture_get_element_groups(va,&options,&b,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(calls(0)==before);
    const ams_mel_rf_element_group_snapshot_v1 *v=view(a), *saved=v;
    CHECK(ams_mel_rf_element_group_snapshot_view(a,&v,D)==AMS_MEL_INVALID_ARGUMENT && v==saved);
    CHECK(ams_mel_rf_element_group_snapshot_view(NULL,&v,D)==AMS_MEL_INVALID_ARGUMENT && v==saved);
    CHECK(ams_mel_rf_element_group_snapshot_view(a,NULL,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_element_group_snapshot_close(NULL,D)==AMS_MEL_INVALID_ARGUMENT);
    for(unsigned method=0;method<10;++method) for(unsigned kind=0;kind<3;++kind) {
        char index[12]; snprintf(index,sizeof index,"%u",method);
        CHECK(setenv("AMS_MEL_TEST_ELEMENT_THROW_METHOD",index,1)==0);
        CHECK(setenv("AMS_MEL_TEST_ELEMENT_THROW_KIND",kind==0 ? "standard" : kind==1 ? "unknown" : "allocation",1)==0);
        options.include_data_pipes=1;
        CHECK(ams_mel_rf_virtual_aperture_get_element_groups(va,&options,&b,D)==(kind==2 ? AMS_MEL_INTERNAL_ERROR : AMS_MEL_PROVIDER_EXCEPTION) && !b);
        CHECK(!live(0) && !live(1)); check(a,1,0);
        CHECK(unsetenv("AMS_MEL_TEST_ELEMENT_THROW_METHOD")==0);
        CHECK(unsetenv("AMS_MEL_TEST_ELEMENT_THROW_KIND")==0);
        b=snapshot(va,1); close_snapshot(&b);
    }
    const char *categories[]={"outer-key","descriptor-label","pipe-key","pipe-label"};
    for(unsigned c=0;c<4;++c) for(unsigned nul=0;nul<2;++nul) {
        CHECK(setenv("AMS_MEL_TEST_ELEMENT_BAD_STRING",categories[c],1)==0);
        CHECK(setenv("AMS_MEL_TEST_ELEMENT_CASE",nul ? "nul" : "utf8",1)==0);
        CHECK(ams_mel_rf_virtual_aperture_get_element_groups(va,&options,&b,D)==AMS_MEL_PROVIDER_FAILED && !b);
        if(c>=2) { b=snapshot(va,0); close_snapshot(&b); options.include_data_pipes=1; }
        CHECK(unsetenv("AMS_MEL_TEST_ELEMENT_BAD_STRING")==0);
        CHECK(unsetenv("AMS_MEL_TEST_ELEMENT_CASE")==0); check(a,1,0);
    }
    const char *malformed[]={"null-descriptor","mode","null-pipe","forbidden-pipes"};
    for(unsigned c=0;c<4;++c) {
        CHECK(setenv("AMS_MEL_TEST_ELEMENT_CASE",malformed[c],1)==0);
        options.include_data_pipes=1;
        CHECK(ams_mel_rf_virtual_aperture_get_element_groups(va,&options,&b,D)==(c==3 ? AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_PROVIDER_FAILED) && !b);
        if(c>=2) { b=snapshot(va,0); close_snapshot(&b); }
        CHECK(unsetenv("AMS_MEL_TEST_ELEMENT_CASE")==0); CHECK(!live(0) && !live(1));
    }
    const char *allocation[]={"element-owner","element-descriptor","element-pipe","element-endpoints"};
    for(unsigned c=0;c<4;++c) {
        CHECK(setenv("AMS_MEL_TEST_RF_VA_FAILURE",allocation[c],1)==0); options.include_data_pipes=1;
        CHECK(ams_mel_rf_virtual_aperture_get_element_groups(va,&options,&b,D)==AMS_MEL_INTERNAL_ERROR && !b);
        CHECK(!live(0) && !live(1)); check(a,1,0);
        CHECK(unsetenv("AMS_MEL_TEST_RF_VA_FAILURE")==0); b=snapshot(va,1); close_snapshot(&b);
    }
    const char *cases[]={"empty","empty-pipes","alias","long","numeric"};
    for(unsigned c=0;c<5;++c) for(unsigned include=0;include<2;++include) {
        CHECK(setenv("AMS_MEL_TEST_ELEMENT_CASE",cases[c],1)==0); options.include_data_pipes=include;
        unsigned counts[10]; for(unsigned m=0;m<10;++m) counts[m]=calls(m);
        CHECK(ams_mel_rf_virtual_aperture_get_element_groups(va,&options,&b,D)==AMS_MEL_OK);
        CHECK(calls(0)==counts[0]+1);
        for(unsigned m=1;m<7;++m) CHECK(calls(m)==counts[m]+(c==0 ? 0U : 3U));
        CHECK(calls(7)==counts[7]+(include && c!=0 ? 3U : 0U));
        for(unsigned m=8;m<10;++m) CHECK(calls(m)==counts[m]+(include && c>1 ? 4U : 0U));
        v=view(b); CHECK(v->data_pipes_included==include);
        if(c==0) CHECK(v->descriptors.size==0 && !v->descriptors.data);
        else {
            CHECK(v->descriptors.size==3);
            const ams_mel_rf_element_group_descriptor_v1 *g=v->descriptors.data;
            if(c==1) for(unsigned i=0;i<3;++i) CHECK(!g[i].data_pipes.size && !g[i].data_pipes.data);
            if(c==2) { text(g[0].label,"returned-empty-key"); text(g[2].label,"returned-empty-key");
                if(include) for(unsigned i=0;i<2;++i) CHECK(g[0].data_pipes.data[i].associated_endpoint_ids.size==3); }
            if(c==3) { CHECK(g[0].lookup_label.size==110 && g[0].label.size==122);
                if(include) CHECK(g[0].data_pipes.data[0].lookup_label.size==90 && g[0].data_pipes.data[0].label.size==102); }
            if(c==4) { CHECK(g[0].max_rf_bandwidth_hz==0 && signbit(g[0].max_rf_bandwidth_hz));
                CHECK(isinf(g[0].max_sample_rate_samples_per_second) && !signbit(g[0].max_sample_rate_samples_per_second));
                CHECK(isnan(g[0].max_data_rate_bits_per_second) && g[0].max_duty_factor==-0.625); }
        }
        close_snapshot(&b); close_snapshot(&b); CHECK(!live(0) && !live(1));
        CHECK(unsetenv("AMS_MEL_TEST_ELEMENT_CASE")==0); check(a,1,0);
    }
    change(1); b=snapshot(va,1); check(b,1,1); check(a,1,0); close_snapshot(&b); check(a,1,0);
    CHECK(ams_mel_rf_c2_close(&parent,D)==AMS_MEL_OK); b=snapshot(va,1); check(b,1,1); close_snapshot(&b);
    for(unsigned m=0;m<6;++m) CHECK(queries(m)==0);
    before=calls(0); check(a,1,0); close_snapshot(&basic); CHECK(calls(0)==before);
    CHECK(dlclose(lib)==0); // own dlopen ref released before actual unload proof
    CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK && !va);
    CHECK(ams_mel_rf_virtual_aperture_get_element_groups(va,&options,&b,D)==AMS_MEL_INVALID_ARGUMENT && !b);
    FILE *log=fopen(path,"r"); CHECK(log); char buffer[65536]; size_t n=fread(buffer,1,sizeof buffer-1,log); buffer[n]=0; CHECK(fclose(log)==0);
    char *destroy=strstr(buffer,"rf_va_destroyed\n"), *shutdown=strstr(buffer,"rf_c2_shutdown\n"), *c2=strstr(buffer,"rf_c2_destroyed\n"), *unload=strstr(buffer,"library_unloaded\n");
    CHECK(destroy && shutdown && c2 && unload && destroy<shutdown && shutdown<c2 && c2<unload && !strstr(buffer,"rf_forbidden_call\n"));
    // Only Claim invoked the old accessors; this live operation did not use them.
    char *labels=strstr(buffer,"rf_va_get_labels\n"), *single=strstr(buffer,"rf_va_is_single\n");
    CHECK(labels && !strstr(labels+1,"rf_va_get_labels\n"));
    CHECK(single && !strstr(single+1,"rf_va_is_single\n"));
    check(a,1,0);
    CHECK(ams_mel_rf_element_group_snapshot_close(&a,D)==AMS_MEL_OK && !a);
    CHECK(ams_mel_rf_element_group_snapshot_close(&a,D)==AMS_MEL_OK);
    CHECK(unlink(path)==0);
    puts("PASS: element descriptors, optional pipes, numeric/endpoint fidelity, cleanup and actual DSO unload");
    return 0;
}
