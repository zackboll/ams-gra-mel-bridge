#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include "rf_transmit_power_abi_probe.h"
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
static unsigned (*calls)(unsigned), (*e1)(unsigned), (*e3)(unsigned), (*e4)(unsigned), (*e5)(unsigned);
static unsigned (*getters)(void), (*tx_modes)(void), (*tx_direct)(void), (*forbidden)(void);
static uint64_t (*id)(unsigned,unsigned);
static double (*number)(unsigned,unsigned);
static void (*change)(unsigned);
static const double a[]={47.125,-3.5,17.75,63.25}, b[]={-28.625,91.5,-6.25,12.875};
static uint64_t group=SIZE_MAX, weight;
static double attenuation=-12.75, frequency=987654321.125, u=-0.75, v=0.625;
static ams_mel_status_t invoke(unsigned m, const ams_mel_rf_virtual_aperture *va,
    double *out, char *d, size_t cap)
{
    switch(m) {
    case 0: return ams_mel_rf_virtual_aperture_get_tx_radiated_power(va,group,0xFEDCBA98U,attenuation,weight,frequency,u,v,0xDEADBEEFU,out,d,cap,&required);
    case 1: return ams_mel_rf_virtual_aperture_get_tx_peak_radiated_power(va,group,0xFEDCBA98U,attenuation,frequency,0xDEADBEEFU,out,d,cap,&required);
    case 2: return ams_mel_rf_virtual_aperture_get_tx_aperture_gain(va,group,0xFEDCBA98U,weight,frequency,u,v,0xDEADBEEFU,out,d,cap,&required);
    default: return ams_mel_rf_virtual_aperture_get_max_tx_attenuation(va,group,0xFEDCBA98U,0xDEADBEEFU,out,d,cap,&required);
    }
}
static void independent(unsigned m, const unsigned before[4], unsigned delta)
{ for(unsigned j=0;j<4;++j) CHECK(calls(j)==before[j]+(j==m ? delta : 0)); }
static void counts(unsigned before[4])
{ for(unsigned j=0;j<4;++j) before[j]=calls(j); }
static void success(unsigned m, ams_mel_rf_virtual_aperture *va, double expected)
{
    unsigned before[4]; counts(before); double value=111;
    CHECK(invoke(m,va,&value,diag,sizeof diag)==AMS_MEL_OK && value==expected);
    independent(m,before,1);
    CHECK(id(m,0)==group && id(m,2)==0xFEDCBA98U && id(m,3)==0xDEADBEEFU);
    if(m==0 || m==2) CHECK(id(m,1)==weight && number(m,2)==u && number(m,3)==v);
    if(m<2) CHECK(number(m,0)==attenuation);
    if(m<3) CHECK(number(m,1)==frequency);
}
int main(void)
{
    check_rf_transmit_power_signatures();
    char path[]="/tmp/ams-rf-tx-XXXXXX"; int fd=mkstemp(path); CHECK(fd>=0 && close(fd)==0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG",path,1)==0);
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL); CHECK(lib);
#define LOAD(target,name) do { void *s=dlsym(lib,name); memcpy(&target,&s,sizeof target); CHECK(target); } while(0)
    LOAD(calls,"mock_rf_tx_query_calls"); LOAD(id,"mock_rf_tx_query_id");
    LOAD(number,"mock_rf_tx_query_double"); LOAD(change,"mock_rf_tx_query_change");
    LOAD(e1,"mock_rf_va_query_calls"); LOAD(e3,"mock_rf_element_calls");
    LOAD(e4,"mock_rf_va_lf_calls"); LOAD(e5,"mock_rf_connection_calls");
    LOAD(getters,"mock_rf_getter_calls"); LOAD(tx_modes,"mock_rf_tx_collection_calls");
    LOAD(tx_direct,"mock_rf_tx_direct_calls"); LOAD(forbidden,"mock_rf_forbidden_calls");
    static const ams_mel_string_view_v1 local[]={{"alpha",5},{"µ-local",8},{"",0}};
    ams_mel_uci_id_v1 caps[3]={{{0},{"first",5}},{{0},{"",0}},{{0},{"µ-third",8}}};
    caps[0].uuid[1]=0x80; caps[0].uuid[2]=0xff; caps[1].uuid[0]=0xff; caps[2].uuid[15]=0x80;
    ams_mel_rf_virtual_aperture_config_v1 config={0xFEDCBA98U,0x80000001U,{local,3},{"definition/β.json",18},{caps,3}};
    ams_mel_rf_c2 *parent=NULL; ams_mel_rf_virtual_aperture_request *request=NULL;
    ams_mel_rf_virtual_aperture *va=NULL; ams_mel_rf_virtual_aperture_result_v1 result={0};
    CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER,"c2:va-ok",&parent,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_submit_virtual_aperture(parent,&config,&request,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_wait(request,3000,&result,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(request,&va,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&request,D)==AMS_MEL_OK);
    for(unsigned m=0;m<4;++m) CHECK(calls(m)==0);
    weight=sizeof(size_t)==8 ? UINT64_C(0x8000000000000000) : SIZE_MAX;
    for(unsigned m=0;m<4;++m) success(m,va,a[m]);
    double copies[4];
    for(unsigned m=0;m<4;++m) CHECK(invoke(m,va,&copies[m],diag,sizeof diag)==AMS_MEL_OK && copies[m]==a[m]);
    change(1); for(unsigned m=0;m<4;++m) { success(m,va,b[m]); CHECK(copies[m]==a[m]); } change(0);
    for(unsigned m=0;m<4;++m) {
        unsigned before[4]; counts(before); double out=111;
        CHECK(invoke(m,NULL,&out,diag,sizeof diag)==AMS_MEL_INVALID_ARGUMENT && out==111);
        CHECK(invoke(m,va,NULL,diag,sizeof diag)==AMS_MEL_INVALID_ARGUMENT);
        CHECK(invoke(m,va,&out,NULL,1)==AMS_MEL_INVALID_ARGUMENT && out==111);
        independent(m,before,0);
        for(unsigned boundary=0;boundary<2;++boundary) {
            group=boundary ? SIZE_MAX : 0; weight=group; success(m,va,a[m]);
        }
#if SIZE_MAX < UINT64_MAX
        group=(uint64_t)SIZE_MAX+1; counts(before);
        CHECK(invoke(m,va,&out,diag,sizeof diag)==AMS_MEL_INVALID_ARGUMENT && out==111); independent(m,before,0);
        if(m==0 || m==2) {
            group=0; weight=(uint64_t)SIZE_MAX+1; counts(before);
            CHECK(invoke(m,va,&out,diag,sizeof diag)==AMS_MEL_INVALID_ARGUMENT && out==111); independent(m,before,0);
        }
#endif
        group=SIZE_MAX; weight=SIZE_MAX;
        const char *exceptions[]={"standard","unknown","allocation"};
        for(unsigned k=0;k<3;++k) {
            counts(before); CHECK(setenv("AMS_MEL_TEST_TX_EXCEPTION",exceptions[k],1)==0);
            CHECK(invoke(m,va,&out,diag,sizeof diag)==(k==2 ? AMS_MEL_INTERNAL_ERROR : AMS_MEL_PROVIDER_EXCEPTION) && out==111);
            independent(m,before,1); CHECK(unsetenv("AMS_MEL_TEST_TX_EXCEPTION")==0); success(m,va,a[m]);
        }
        const char *special[]={"positive-zero","negative-zero","positive-infinity","negative-infinity","nan"};
        for(unsigned k=0;k<5;++k) {
            counts(before); CHECK(setenv("AMS_MEL_TEST_TX_RESULT",special[k],1)==0);
            CHECK(invoke(m,va,&out,diag,sizeof diag)==AMS_MEL_OK); independent(m,before,1);
            if(k<2) CHECK(out==0 && !!signbit(out)==(k==1));
            else if(k<4) CHECK(isinf(out) && !!signbit(out)==(k==3)); else CHECK(isnan(out));
            CHECK(unsetenv("AMS_MEL_TEST_TX_RESULT")==0); out=111;
        }
    }
    // Representative IEEE inputs, including U/V outside any assumed interval.
    attenuation=-0.0; frequency=-INFINITY; u=NAN; v=INFINITY;
    double out=0; CHECK(invoke(0,va,&out,diag,sizeof diag)==AMS_MEL_OK && out==a[0]);
    CHECK(number(0,0)==0 && signbit(number(0,0)) && isinf(number(0,1)) && signbit(number(0,1)) && isnan(number(0,2)) && isinf(number(0,3)) && !signbit(number(0,3)));
    attenuation=NAN; frequency=-987.25; u=-2.75; v=-0.0;
    CHECK(invoke(0,va,&out,diag,sizeof diag)==AMS_MEL_OK);
    CHECK(isnan(number(0,0)) && number(0,1)==frequency && number(0,2)==u && number(0,3)==0 && signbit(number(0,3)));
    attenuation=-12.75; frequency=987654321.125; u=-0.75; v=0.625;
    for(unsigned m=0;m<6;++m) CHECK(e1(m)==0);
    for(unsigned m=0;m<10;++m) CHECK(e3(m)==0);
    for(unsigned m=0;m<4;++m) CHECK(e4(m)==0);
    for(unsigned m=0;m<5;++m) CHECK(e5(m)==0);
    CHECK(!getters() && !tx_modes() && !tx_direct() && !forbidden());
    CHECK(dlclose(lib)==0); // No subscription, Job, ProductRx or other registration.
    CHECK(ams_mel_rf_c2_close(&parent,D)==AMS_MEL_OK);
    for(unsigned m=0;m<4;++m) success(m,va,a[m]);
    for(unsigned m=0;m<6;++m) CHECK(e1(m)==0);
    for(unsigned m=0;m<10;++m) CHECK(e3(m)==0);
    for(unsigned m=0;m<4;++m) CHECK(e4(m)==0);
    for(unsigned m=0;m<5;++m) CHECK(e5(m)==0);
    CHECK(!getters() && !tx_modes() && !tx_direct() && !forbidden());
    CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK && !va);
    out=111;
    for(unsigned m=0;m<4;++m) CHECK(invoke(m,va,&out,diag,sizeof diag)==AMS_MEL_INVALID_ARGUMENT && out==111);
    FILE *f=fopen(path,"r"); CHECK(f); char text[16384]; size_t n=fread(text,1,sizeof text-1,f); text[n]=0; CHECK(fclose(f)==0);
    char *destroy=strstr(text,"rf_va_destroyed\n"), *shutdown=strstr(text,"rf_c2_shutdown\n"), *c2=strstr(text,"rf_c2_destroyed\n"), *unload=strstr(text,"library_unloaded\n");
    CHECK(destroy && shutdown && c2 && unload && destroy<shutdown && shutdown<c2 && c2<unload && !strstr(text,"rf_forbidden_call\n"));
    for(unsigned m=0;m<4;++m) CHECK(copies[m]==a[m]);
    CHECK(unlink(path)==0);
    puts("PASS: four independent TX power queries, exact scalars/IEEE values and actual DSO unload"); return 0;
}
