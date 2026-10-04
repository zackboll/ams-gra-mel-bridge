#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
#define D diagnostic, sizeof diagnostic, &required
static char diagnostic[1024], lifetime[]="/tmp/ams-va-notify-XXXXXX";
static size_t required;
static unsigned (*last)(void), (*emit)(unsigned,unsigned,unsigned), (*queries)(unsigned), (*other)(unsigned);
static size_t (*value)(unsigned,unsigned);
static void (*hold_removal)(int,int);
static void *library;
static void unpin_test_library(void)
{ if (library) { CHECK(dlclose(library)==0); library=NULL; } }
extern void ams_mel_test_va_signal_hold(int,int);
extern unsigned ams_mel_test_va_signal_waiters(const ams_mel_rf_va_status_subscription *);
extern void ams_mel_test_va_signal_preset(ams_mel_rf_va_status_subscription *,uint64_t);
extern void ams_mel_test_va_signal_last(ams_mel_rf_va_status_subscription_statistics_v1 *);
static ams_mel_rf_virtual_aperture *claim(ams_mel_rf_c2 **parent, const char *scenario)
{
    static const ams_mel_string_view_v1 local[]={{"alpha",5},{"µ-local",8},{"",0}};
    ams_mel_uci_id_v1 caps[3]={{{0},{"first",5}},{{0},{"",0}},{{0},{"µ-third",8}}};
    caps[0].uuid[1]=0x80; caps[0].uuid[2]=0xff; caps[1].uuid[0]=0xff; caps[2].uuid[15]=0x80;
    ams_mel_rf_virtual_aperture_config_v1 config={0xfedcba98U,0x80000001U,{local,3},{"definition/β.json",18},{caps,3}};
    ams_mel_rf_virtual_aperture_request *request=NULL;
    ams_mel_rf_virtual_aperture *va=NULL;
    ams_mel_rf_virtual_aperture_result_v1 result={0};
    CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER,scenario,parent,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_submit_virtual_aperture(*parent,&config,&request,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_wait(request,3000,&result,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(request,&va,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&request,D)==AMS_MEL_OK);
    return va;
}
static ams_mel_rf_va_status_subscription_statistics_v1 stats(ams_mel_rf_va_status_subscription *s)
{
    ams_mel_rf_va_status_subscription_statistics_v1 result={0};
    CHECK(ams_mel_rf_va_status_subscription_get_statistics(s,&result,D)==AMS_MEL_OK);
    CHECK(result.pending<=1 && result.stopped<=1);
    return result;
}
static void poll_stopped(ams_mel_rf_va_status_subscription *s)
{ CHECK(ams_mel_rf_va_status_subscription_wait(s,0,D)==AMS_MEL_STREAM_STOPPED); }
static void no_reads(const unsigned before[6])
{ for(unsigned i=0;i<6;++i) CHECK(queries(i)==before[i]); }
static void reset_log(void)
{ FILE *f=fopen(lifetime,"w"); CHECK(f && fclose(f)==0); }
static void teardown_log(void)
{
    char text[16384]; FILE *f=fopen(lifetime,"r"); CHECK(f);
    size_t n=fread(text,1,sizeof text-1,f); text[n]=0; CHECK(fclose(f)==0);
    char *r=strstr(text,"rf_va_callback_removed\n"), *v=strstr(text,"rf_va_destroyed\n");
    char *s=strstr(text,"rf_c2_shutdown\n"), *c=strstr(text,"rf_c2_destroyed\n");
    CHECK(r && v && s && c && r<v && v<s && s<c);
    CHECK(!strstr(text,"library_unloaded\n") && !strstr(text,"rf_forbidden_call\n"));
}
static void ordinary(const char *scenario)
{
    ams_mel_rf_c2 *parent=NULL,*parent2=NULL;
    ams_mel_rf_virtual_aperture *va=claim(&parent,scenario);
    unsigned index=last(), before[6];
    ams_mel_rf_va_status_subscription *s=NULL,*second=NULL;
    for(unsigned i=0;i<6;++i) before[i]=queries(i);
    CHECK(ams_mel_rf_va_status_subscription_open(NULL,&s,D)==AMS_MEL_INVALID_ARGUMENT && !s);
    CHECK(ams_mel_rf_va_status_subscription_open(va,NULL,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_va_status_subscription_unsubscribe(va,NULL,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_va_status_subscription_open(va,&s,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT && !s);
    CHECK(value(index,0)==0);
    // Public C2 closure does not prevent registration or later removal.
    CHECK(ams_mel_rf_c2_close(&parent,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_va_status_subscription_open(va,&s,D)==AMS_MEL_OK);
    unpin_test_library();
    CHECK(value(index,0)==1 && stats(s).stopped==0);
    CHECK(ams_mel_rf_va_status_subscription_open(va,&s,D)==AMS_MEL_INVALID_ARGUMENT);
    ams_mel_rf_va_status_subscription_statistics_v1 untouched={11,22,33,44,55,66};
    CHECK(ams_mel_rf_va_status_subscription_get_statistics(NULL,&untouched,D)==AMS_MEL_INVALID_ARGUMENT && untouched.pending==55);
    CHECK(ams_mel_rf_va_status_subscription_get_statistics(s,&untouched,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT && untouched.pending==55);
    CHECK(ams_mel_rf_va_status_subscription_get_statistics(s,NULL,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_va_status_subscription_wait(NULL,0,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_va_status_subscription_close(NULL,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_va_status_subscription_close(&s,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT && s);
    if(strstr(scenario,"sync")) {
        CHECK(stats(s).pending==1);
        CHECK(ams_mel_rf_va_status_subscription_wait(s,0,D)==AMS_MEL_OK);
    }
    CHECK(ams_mel_rf_va_status_subscription_wait(s,0,D)==AMS_MEL_TIMEOUT);
    CHECK(ams_mel_rf_va_status_subscription_wait(s,1,D)==AMS_MEL_TIMEOUT);
    CHECK(ams_mel_rf_va_status_subscription_open(va,&second,D)==AMS_MEL_PROVIDER_FAILED && !second);
    CHECK(value(index,0)==1);
    no_reads(before);
    // Subscribe first, query later on this application thread.
    uint32_t current=99;
    ams_mel_rf_va_instance_status_report *old=NULL;
    const ams_mel_rf_va_instance_status_report_v1 *old_view=NULL;
    CHECK(ams_mel_rf_virtual_aperture_get_status(va,&current,D)==AMS_MEL_OK && current==1);
    CHECK(ams_mel_rf_virtual_aperture_get_instance_status_report(va,42,&old,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_va_instance_status_report_view(old,&old_view,D)==AMS_MEL_OK && old_view->status==1);
    // E3 is an explicit application-thread read, never a callback-time read.
    ams_mel_rf_element_group_snapshot *descriptors=NULL;
    const ams_mel_rf_element_group_snapshot_v1 *descriptor_view=NULL;
    const ams_mel_rf_element_group_snapshot_options_v1 descriptor_options={1};
    CHECK(ams_mel_rf_virtual_aperture_get_element_groups(va,&descriptor_options,&descriptors,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_element_group_snapshot_view(descriptors,&descriptor_view,D)==AMS_MEL_OK);
    CHECK(descriptor_view->data_pipes_included && descriptor_view->descriptors.size==3);
    for(unsigned i=0;i<6;++i) before[i]=queries(i);
    uint64_t initial=stats(s).callback_entries, delivered=stats(s).notifications_delivered;
    CHECK(emit(index,3,0)==1); // Holds mock nonrecursive getter mutex throughout callback.
    no_reads(before);
    CHECK(stats(s).callback_entries==initial+3 && stats(s).callbacks_coalesced==2 && stats(s).pending);
    CHECK(ams_mel_rf_va_status_subscription_wait(s,0,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(stats(s).pending);
    CHECK(ams_mel_rf_virtual_aperture_get_status(va,&current,D)==AMS_MEL_OK && current==2);
    CHECK(stats(s).pending); // Queries never consume the notification.
    CHECK(ams_mel_rf_va_status_subscription_wait(s,0,D)==AMS_MEL_OK);
    CHECK(stats(s).notifications_delivered==delivered+1 && !stats(s).pending);
    CHECK(ams_mel_rf_va_status_subscription_wait(s,0,D)==AMS_MEL_TIMEOUT);
    CHECK(stats(s).notifications_delivered==delivered+1);
    CHECK(emit(index,1,0)==1 && stats(s).pending);
    CHECK(setenv("AMS_MEL_TEST_VA_QUERY_EXCEPTION","standard",1)==0);
    CHECK(ams_mel_rf_virtual_aperture_get_status(va,&current,D)==AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(unsetenv("AMS_MEL_TEST_VA_QUERY_EXCEPTION")==0);
    CHECK(stats(s).pending && ams_mel_rf_va_status_subscription_wait(s,0,D)==AMS_MEL_OK);
    CHECK(old_view->status==1);
    ams_mel_rf_virtual_aperture *wrong=claim(&parent2,scenario);
    unsigned wrong_index=last();
    ams_mel_rf_va_status_subscription *wrong_s=NULL;
    CHECK(ams_mel_rf_va_status_subscription_open(wrong,&wrong_s,D)==AMS_MEL_OK);
    CHECK(value(index,2)==value(wrong_index,2)); // Repeated provider keys do not identify owners.
    CHECK(emit(index,1,0)==1);
    CHECK(ams_mel_rf_va_status_subscription_unsubscribe(wrong,s,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(!stats(s).stopped && stats(s).pending && value(index,1)==0 && value(wrong_index,1)==0);
    CHECK(!stats(wrong_s).stopped);
    CHECK(ams_mel_rf_virtual_aperture_close(&wrong,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&parent2,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_va_status_subscription_close(&wrong_s,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_va_status_subscription_unsubscribe(va,s,NULL,1,NULL)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(!stats(s).stopped && value(index,1)==0);
    for(unsigned i=0;i<6;++i) before[i]=queries(i);
    reset_log();
    CHECK(ams_mel_rf_va_status_subscription_unsubscribe(va,s,D)==AMS_MEL_OK);
    CHECK(stats(s).stopped && !stats(s).pending && value(index,1)==1);
    CHECK(value(index,2)==value(index,3) && value(index,5)==1 && other(index)==1);
    CHECK(value(index,2)==(strstr(scenario,"zero") ? 0 : strstr(scenario,"max") ? SIZE_MAX : 0x12345678U));
    poll_stopped(s);
    CHECK(ams_mel_rf_va_status_subscription_unsubscribe(va,s,D)==AMS_MEL_OK && value(index,1)==1);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK && !va);
    CHECK(value(index,1)==1 && value(index,7)==0);
    no_reads(before);
    teardown_log();
    CHECK(descriptor_view->descriptors.data[0].data_pipes.data[0].associated_endpoint_ids.data[2]==UINT64_MAX);
    CHECK(ams_mel_rf_element_group_snapshot_close(&descriptors,D)==AMS_MEL_OK);
    // The intentional E2 exact-callable DSO pin remains, not snapshot retention.
    teardown_log();
    uint64_t late=stats(s).callbacks_after_stop;
    for(unsigned i=0;i<6;++i) before[i]=queries(i);
    CHECK(emit(index,1,1)==1); // Different LIVE BaseVA, original already destroyed.
    no_reads(before);
    CHECK(stats(s).callbacks_after_stop==late+1 && !stats(s).pending);
    poll_stopped(s);
    CHECK(ams_mel_rf_va_status_subscription_unsubscribe(va,s,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_rf_va_status_subscription_close(&s,D)==AMS_MEL_OK && !s);
    CHECK(ams_mel_rf_va_status_subscription_close(&s,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_va_instance_status_report_close(&old,D)==AMS_MEL_OK);
}
static void preparation(void)
{
    const char *failures[]={"subscription-owner","subscription-state","subscription-control","subscription-callable","subscription-retention"};
    for(unsigned i=0;i<5;++i) {
        ams_mel_rf_c2 *p=NULL;
        ams_mel_rf_virtual_aperture *va=claim(&p,"c2:va-notify-reference");
        unsigned index=last(); ams_mel_rf_va_status_subscription *s=NULL;
        CHECK(setenv("AMS_MEL_TEST_RF_VA_FAILURE",failures[i],1)==0);
        CHECK(ams_mel_rf_va_status_subscription_open(va,&s,D)==AMS_MEL_INTERNAL_ERROR && !s && value(index,0)==0);
        CHECK(unsetenv("AMS_MEL_TEST_RF_VA_FAILURE")==0);
        CHECK(ams_mel_rf_va_status_subscription_open(va,&s,D)==AMS_MEL_OK);
        unpin_test_library();
        CHECK(ams_mel_rf_va_status_subscription_close(&s,D)==AMS_MEL_OK && value(index,1)==0);
        ams_mel_rf_va_status_subscription_statistics_v1 closed={0};
        ams_mel_test_va_signal_last(&closed);
        CHECK(closed.stopped && !closed.pending);
        CHECK(ams_mel_rf_va_status_subscription_open(va,&s,D)==AMS_MEL_PROVIDER_FAILED && !s);
        CHECK(ams_mel_rf_c2_close(&p,D)==AMS_MEL_OK);
        reset_log();
        CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK && value(index,1)==1);
        teardown_log();
        CHECK(emit(index,1,1)==1);
        ams_mel_test_va_signal_last(&closed);
        CHECK(closed.stopped && !closed.pending && closed.callbacks_after_stop==1);
    }
}
static void stored_throw(void)
{
    ams_mel_rf_c2 *p=NULL;
    ams_mel_rf_virtual_aperture *va=claim(&p,"c2:va-notify-reference-sync-stored-throw");
    unsigned index=last(), before[6]; ams_mel_rf_va_status_subscription *s=NULL;
    for(unsigned i=0;i<6;++i) before[i]=queries(i);
    CHECK(ams_mel_rf_va_status_subscription_open(va,&s,D)==AMS_MEL_PROVIDER_EXCEPTION && !s);
    unpin_test_library();
    CHECK(strstr(diagnostic,"stored VA callback") && value(index,0)==1);
    ams_mel_rf_va_status_subscription_statistics_v1 stopped={0};
    ams_mel_test_va_signal_last(&stopped);
    CHECK(stopped.stopped && !stopped.pending && stopped.callback_entries==1);
    no_reads(before);
    CHECK(ams_mel_rf_va_status_subscription_open(va,&s,D)==AMS_MEL_PROVIDER_FAILED && !s && value(index,0)==1);
    uint32_t status=99; CHECK(ams_mel_rf_virtual_aperture_get_status(va,&status,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_close(&p,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK && value(index,1)==0 && !value(index,7));
    CHECK(emit(index,1,1)==1); // Exact referenced callable remains valid, no invented key.
    ams_mel_test_va_signal_last(&stopped);
    CHECK(stopped.stopped && !stopped.pending && stopped.callbacks_after_stop==1);
}
static void removal_errors(void)
{
    const char *failures[]={"standard","unknown","allocation","long"};
    for(unsigned i=0;i<4;++i) for(unsigned both=0;both<2;++both) for(unsigned automatic=0;automatic<2;++automatic) {
        ams_mel_rf_c2 *p=NULL;
        ams_mel_rf_virtual_aperture *va=claim(&p,both?"c2:va-notify-shutdown-throw":"c2:va-notify-copy");
        unsigned index=last(); ams_mel_rf_va_status_subscription *s=NULL;
        CHECK(ams_mel_rf_va_status_subscription_open(va,&s,D)==AMS_MEL_OK);
        unpin_test_library();
        CHECK(setenv("AMS_MEL_TEST_VA_REMOVE_EXCEPTION",failures[i],1)==0);
        ams_mel_status_t expected=i==2?AMS_MEL_INTERNAL_ERROR:AMS_MEL_PROVIDER_EXCEPTION;
        char cached[1024]={0};
        if(!automatic) {
            CHECK(ams_mel_rf_va_status_subscription_unsubscribe(va,s,D)==expected && value(index,1)==1);
            strcpy(cached,diagnostic); CHECK(cached[0]);
            CHECK(unsetenv("AMS_MEL_TEST_VA_REMOVE_EXCEPTION")==0);
            CHECK(ams_mel_rf_va_status_subscription_unsubscribe(va,s,D)==expected && !strcmp(cached,diagnostic) && value(index,1)==1);
            poll_stopped(s);
        }
        CHECK(ams_mel_rf_c2_close(&p,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==(both?AMS_MEL_PROVIDER_EXCEPTION:expected));
        if(both) CHECK(strstr(diagnostic,"C2 shutdown") && strstr(diagnostic,"removal:"));
        else if(!automatic) CHECK(!strcmp(cached,diagnostic));
        else CHECK(diagnostic[0]);
        CHECK(unsetenv("AMS_MEL_TEST_VA_REMOVE_EXCEPTION")==0);
        CHECK(!va && value(index,1)==1 && !value(index,7));
        CHECK(emit(index,1,1)==1 && stats(s).callbacks_after_stop==1);
        poll_stopped(s);
        CHECK(ams_mel_rf_va_status_subscription_close(&s,D)==AMS_MEL_OK);
    }
}
struct Thread { unsigned index; ams_mel_rf_va_status_subscription *s; ams_mel_status_t result; };
struct Closer { ams_mel_rf_virtual_aperture **va; ams_mel_status_t result; };
static void *closer(void *arg) { struct Closer *c=arg; c->result=ams_mel_rf_virtual_aperture_close(c->va,NULL,0,NULL); return NULL; }
static void *emitter(void *arg) { struct Thread *t=arg; CHECK(emit(t->index,1,2)==1); return NULL; }
static void *waiter(void *arg) { struct Thread *t=arg; t->result=ams_mel_rf_va_status_subscription_wait(t->s,3000,NULL,0,NULL); return NULL; }
static void races(void)
{
    ams_mel_rf_c2 *p=NULL;
    ams_mel_rf_virtual_aperture *va=claim(&p,"c2:va-notify-reference");
    unsigned index=last(); ams_mel_rf_va_status_subscription *s=NULL;
    CHECK(ams_mel_rf_va_status_subscription_open(va,&s,D)==AMS_MEL_OK);
    unpin_test_library();
    struct Thread concurrent={index,s,AMS_MEL_INTERNAL_ERROR}; pthread_t threads[8];
    for(unsigned i=0;i<8;++i) CHECK(pthread_create(&threads[i],NULL,emitter,&concurrent)==0);
    for(unsigned i=0;i<8;++i) CHECK(pthread_join(threads[i],NULL)==0);
    CHECK(stats(s).callback_entries==8 && stats(s).callbacks_coalesced==7);
    CHECK(ams_mel_rf_va_status_subscription_wait(s,0,D)==AMS_MEL_OK);
    ams_mel_test_va_signal_preset(s,UINT64_MAX-1);
    CHECK(emit(index,3,0)==1 && stats(s).callback_entries==UINT64_MAX && stats(s).callbacks_coalesced==UINT64_MAX);
    CHECK(ams_mel_rf_va_status_subscription_wait(s,0,D)==AMS_MEL_OK);
    CHECK(emit(index,1,0)==1 && ams_mel_rf_va_status_subscription_wait(s,0,D)==AMS_MEL_OK);
    CHECK(stats(s).notifications_delivered==UINT64_MAX);
    struct Thread t={index,s,AMS_MEL_INTERNAL_ERROR}; pthread_t thread;
    CHECK(pthread_create(&thread,NULL,waiter,&t)==0);
    while(!ams_mel_test_va_signal_waiters(s)) sched_yield(); // Harness alarm bounds the gate.
    CHECK(emit(index,1,0)==1);
    CHECK(pthread_join(thread,NULL)==0 && t.result==AMS_MEL_OK);
    int notify[2], release[2]; CHECK(pipe(notify)==0 && pipe(release)==0);
    ams_mel_test_va_signal_hold(notify[1],release[0]);
    CHECK(pthread_create(&thread,NULL,emitter,&t)==0);
    char byte; CHECK(read(notify[0],&byte,1)==1); // Callback entered but has not locked signal.
    struct Thread w={index,s,AMS_MEL_INTERNAL_ERROR}; pthread_t waiting;
    CHECK(pthread_create(&waiting,NULL,waiter,&w)==0);
    while(!ams_mel_test_va_signal_waiters(s)) sched_yield();
    CHECK(ams_mel_rf_c2_close(&p,D)==AMS_MEL_OK);
    reset_log();
    int removal_notify[2], removal_release[2];
    CHECK(pipe(removal_notify)==0 && pipe(removal_release)==0);
    hold_removal(removal_notify[1],removal_release[0]);
    struct Closer closing={&va,AMS_MEL_INTERNAL_ERROR}; pthread_t close_thread;
    CHECK(pthread_create(&close_thread,NULL,closer,&closing)==0);
    CHECK(read(removal_notify[0],&byte,1)==1); // VA Close is blocked INSIDE provider removal.
    CHECK(pthread_join(waiting,NULL)==0 && w.result==AMS_MEL_STREAM_STOPPED);
    CHECK(stats(s).stopped && !stats(s).pending && value(index,7)==1);
    CHECK(write(removal_release[1],"r",1)==1 && pthread_join(close_thread,NULL)==0 && closing.result==AMS_MEL_OK);
    CHECK(write(release[1],"r",1)==1 && pthread_join(thread,NULL)==0);
    CHECK(stats(s).stopped && !stats(s).pending && stats(s).callbacks_after_stop==UINT64_MAX);
    poll_stopped(s); teardown_log();
    CHECK(close(notify[0])==0 && close(notify[1])==0 && close(release[0])==0 && close(release[1])==0);
    CHECK(close(removal_notify[0])==0 && close(removal_notify[1])==0 && close(removal_release[0])==0 && close(removal_release[1])==0);
    CHECK(ams_mel_rf_va_status_subscription_close(&s,D)==AMS_MEL_OK);
}
static void job_lifetime(void)
{
    ams_mel_rf_c2 *p=NULL;
    ams_mel_rf_virtual_aperture *va=claim(&p,"c2:va-notify-reference");
    unsigned index=last(); ams_mel_rf_va_status_subscription *s=NULL;
    CHECK(ams_mel_rf_va_status_subscription_open(va,&s,D)==AMS_MEL_OK); unpin_test_library();
    const ams_mel_rf_frequency_range_v1 frequencies[]={{1000000.25,2000000.5},{987654321.125,987654322.875}};
    const uint64_t endpoints[]={0,UINT64_C(0x8000000000000000),UINT64_MAX};
    const uint32_t instances[]={0,42,UINT32_MAX};
    ams_mel_rf_job_request_config_v1 config={0xfedcba98U,0x80000001U,0x7ffffffeU,1,
        {instances,3},{{"rx/µ-main",10},0.625,{frequencies,2},{endpoints,3},{"products/β",11}}};
    ams_mel_rf_job_request *request=NULL; ams_mel_rf_job *job=NULL; ams_mel_rf_job_result_v1 result={0};
    CHECK(ams_mel_rf_virtual_aperture_submit_job(va,&config,&request,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_wait(request,3000,&result,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_claim(request,&job,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_close(&request,D)==AMS_MEL_OK);
    CHECK(emit(index,1,0)==1 && stats(s).pending);
    CHECK(ams_mel_rf_c2_close(&p,D)==AMS_MEL_OK);
    reset_log();
    CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK);
    CHECK(value(index,1)==1 && value(index,7)==1); // Job keeps provider VA, not active reception.
    CHECK(emit(index,1,0)==1 && stats(s).callbacks_after_stop==1 && !stats(s).pending);
    poll_stopped(s);
    const ams_mel_rf_job_info_v1 *info=NULL;
    CHECK(ams_mel_rf_job_view(job,&info,D)==AMS_MEL_OK && info->va_instance_id==42);
    CHECK(ams_mel_rf_job_close(&job,D)==AMS_MEL_OK && !value(index,7));
    teardown_log();
    CHECK(ams_mel_rf_va_status_subscription_close(&s,D)==AMS_MEL_OK);
}
int main(int argc,char **argv)
{
    CHECK(argc==2); alarm(20); // Bounded harness watchdog, never sleeps as progress evidence.
    int fd=mkstemp(lifetime); CHECK(fd>=0 && close(fd)==0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG",lifetime,1)==0);
    library=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL); CHECK(library);
    *(void **)(&last)=dlsym(library,"mock_rf_va_notification_last");
    *(void **)(&value)=dlsym(library,"mock_rf_va_notification_value");
    *(void **)(&emit)=dlsym(library,"mock_rf_va_notification_emit");
    *(void **)(&queries)=dlsym(library,"mock_rf_va_query_calls");
    *(void **)(&other)=dlsym(library,"mock_rf_va_notification_other");
    *(void **)(&hold_removal)=dlsym(library,"mock_rf_va_notification_hold_removal");
    CHECK(last && value && emit && queries && other && hold_removal);
    if(!strcmp(argv[1],"preparation")) preparation();
    else if(!strcmp(argv[1],"stored-throw")) stored_throw();
    else if(!strcmp(argv[1],"removal-errors")) removal_errors();
    else if(!strcmp(argv[1],"races")) races();
    else if(!strcmp(argv[1],"job-lifetime")) job_lifetime();
    else ordinary(argv[1]);
    unpin_test_library(); CHECK(unlink(lifetime)==0);
    puts("PASS: RF VA signal-only notifications, coalescing and safe teardown");
    return 0;
}
