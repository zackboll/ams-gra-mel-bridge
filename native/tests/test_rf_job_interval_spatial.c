#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d %s: %s\n",__FILE__,__LINE__,#x,diag); exit(1); } } while (0)
#define D diag, sizeof diag, &required
static char diag[512];
static size_t required;
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

static unsigned mock(const char *name)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    unsigned (*fn)(void); CHECK(lib); *(void **)(&fn)=dlsym(lib,name); CHECK(fn);
    unsigned result=fn(); CHECK(dlclose(lib)==0); return result;
}
static void no_queries(void)
{
    const struct {const char *name; unsigned methods;} queries[]={
        {"mock_rf_va_query_calls",6},{"mock_rf_element_calls",10},
        {"mock_rf_connection_calls",5},{"mock_rf_tx_query_calls",4},{"mock_rf_va_lf_calls",4}};
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL); CHECK(lib);
    for(unsigned q=0;q<sizeof queries/sizeof queries[0];++q) {
        unsigned (*fn)(unsigned); *(void **)(&fn)=dlsym(lib,queries[q].name); CHECK(fn);
        for(unsigned i=0;i<queries[q].methods;++i) CHECK(fn(i)==0);
    }
    CHECK(dlclose(lib)==0);
}
static ams_mel_rf_job *claimed(const char *scenario, ams_mel_rf_c2 **c2, ams_mel_rf_virtual_aperture **va)
{
    ams_mel_rf_virtual_aperture_request *vr=NULL;
    ams_mel_rf_virtual_aperture_result_v1 vresult={0};
    ams_mel_rf_job_request *jr=NULL; ams_mel_rf_job *job=NULL;
    ams_mel_rf_job_result_v1 result={0};
    CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER,scenario,c2,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_c2_submit_virtual_aperture(*c2,&va_config,&vr,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_wait(vr,3000,&vresult,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_claim(vr,va,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_request_close(&vr,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_virtual_aperture_submit_job(*va,&config,&jr,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_wait(jr,3000,&result,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_claim(jr,&job,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_request_close(&jr,D)==AMS_MEL_OK);
    return job;
}
static ams_mel_rf_pointing_v1 points[]={
    {.kind=AMS_MEL_RF_POINTING_ECEF,.ecef={{1.25,-2.5,3.75},{-4.5,5.625,-6.75},{-7,123456789012345}}},
    {.kind=AMS_MEL_RF_POINTING_PLATFORM_RELATIVE,.platform_relative={-0.75,0.25}},
    {.kind=AMS_MEL_RF_POINTING_FACE_RELATIVE,.face_relative={1.5,-0.5}},
    {.kind=AMS_MEL_RF_POINTING_FACE_RELATIVE,.face_relative={1.5,-0.5}},
    {.kind=AMS_MEL_RF_POINTING_LLA,.lla={0.125,-1.25,12345.5,11.25,-12.5,13.75,{42,999999999999999}}},
    {.kind=AMS_MEL_RF_POINTING_BASELINE_RELATIVE,.baseline_relative_conic_rad=-2.25}
};
static uint64_t gx[]={2,0,2}, gy[]={7,7,1};
static ams_mel_rf_receive_event_config_v2 events[]={
    {{0x80000001U,{"rx/µ-main",10},-123,456789,987654321.125,2000000.5,0x100000007ULL,3,-999},1,{gx,3}},
    {{UINT32_MAX,{"β-secondary",12},777,-888,-0.0,INFINITY,0,0x100000009ULL,INT64_MAX-1},999,{gy,3}}
};
static ams_mel_rf_job_interval_config_v3 intervals[]={
    {0,0x10203040U,-111,9876543210123LL,0x100000003ULL,222,-333,1,0x100000005ULL,
     123456789.25,2500000.5,0xABCDEF01U,AMS_MEL_RF_INTERVAL_STATUS_NEVER,{points,4},{events,2}},
    {-1,42,12,-13,2,-14,15,0,3,NAN,INFINITY,43,AMS_MEL_RF_INTERVAL_STATUS_NEVER,{points+4,2},{NULL,0}}
};
static ams_mel_rf_job_interval_config_span_v3 span={intervals,2};
static void invalid(ams_mel_rf_job *job, ams_mel_rf_job_interval_config_span_v3 input)
{
    unsigned before=mock("mock_rf_job_add_calls");
    CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,input,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(mock("mock_rf_job_add_calls")==before);
}
static void validation(ams_mel_rf_job *job)
{
    invalid(job,(ams_mel_rf_job_interval_config_span_v3){NULL,1});
    invalid(job,(ams_mel_rf_job_interval_config_span_v3){intervals,SIZE_MAX});
    ams_mel_rf_job_interval_config_v3 bad=intervals[0];
    ams_mel_rf_job_interval_config_span_v3 one={&bad,1};
    bad.phase_coherence_with_prior=2; invalid(job,one);
    bad=intervals[0]; bad.status_enable=3; invalid(job,one);
    bad=intervals[0]; bad.stab_points=(ams_mel_rf_pointing_span_v1){NULL,1}; invalid(job,one);
    bad=intervals[0]; bad.stab_points.size=SIZE_MAX; invalid(job,one);
    bad=intervals[0]; bad.receive_events=(ams_mel_rf_receive_event_config_span_v2){NULL,1}; invalid(job,one);
    bad=intervals[0]; bad.receive_events.size=SIZE_MAX; invalid(job,one);
    ams_mel_rf_receive_event_config_v2 event=events[0];
    bad=intervals[0]; bad.receive_events=(ams_mel_rf_receive_event_config_span_v2){&event,1};
    const ams_mel_string_view_v1 labels[]={{NULL,1},{"\xff",1},{"a\0b",3}};
    for(unsigned i=0;i<3;++i) {event.event.element_group_label=labels[i]; invalid(job,one);}
    event=events[0]; event.applicable_rx_element_groups=(ams_mel_u64_span_v1){NULL,1}; invalid(job,one);
    event=events[0]; event.applicable_rx_element_groups.size=SIZE_MAX; invalid(job,one);
    event=events[0];
    ams_mel_rf_pointing_v1 point=points[0]; bad.stab_points=(ams_mel_rf_pointing_span_v1){&point,1};
    point.kind=5; invalid(job,one);
    for(unsigned kind=0;kind<2;++kind) for(unsigned high=0;high<2;++high) {
        point=kind ? points[4] : points[0];
        (kind ? &point.lla.time_of_validity : &point.ecef.time_of_validity)->fractional_femtoseconds=
            high ? INT64_C(1000000000000000) : -1; invalid(job,one);
    }
#if SIZE_MAX < UINT64_MAX
    point=points[0]; event.stab_point_index=UINT64_MAX; invalid(job,one);
    event=events[0]; uint64_t group=UINT64_MAX; event.applicable_rx_element_groups=(ams_mel_u64_span_v1){&group,1}; invalid(job,one);
    event=events[0]; event.event.agc_processing_iterations=UINT64_MAX; invalid(job,one);
    event=events[0]; event.event.ignored_post_agc_iterations=UINT64_MAX; invalid(job,one);
    event=events[0]; bad.sequence_repeat_count=UINT64_MAX; invalid(job,one);
    bad=intervals[0]; bad.iterations_per_signal=UINT64_MAX; invalid(job,one);
#endif
    CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,(ams_mel_rf_job_interval_config_span_v3){NULL,0},D)==AMS_MEL_OK);
    event=events[0]; event.event.element_group_label=(ams_mel_string_view_v1){NULL,0};
    event.stab_point_index=SIZE_MAX; uint64_t groups[]={SIZE_MAX,0,SIZE_MAX};
    event.applicable_rx_element_groups=(ams_mel_u64_span_v1){groups,3};
    bad=intervals[0]; bad.stab_points=(ams_mel_rf_pointing_span_v1){NULL,0};
    bad.receive_events=(ams_mel_rf_receive_event_config_span_v2){&event,1};
    CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,one,D)==AMS_MEL_OK);
    CHECK(mock("mock_rf_job_spatial_boundary")==1);
    printf("PASS: size_t width %zu, max %llu forwarded without local index relationship\n",
           sizeof(size_t)*8,(unsigned long long)SIZE_MAX);
}
int main(void)
{
    capabilities[0].uuid[1]=0x80; capabilities[0].uuid[2]=0xff;
    capabilities[0].descriptive_label=(ams_mel_string_view_v1){"first",5};
    capabilities[1].uuid[0]=0xff; capabilities[1].descriptive_label=(ams_mel_string_view_v1){"",0};
    capabilities[2].uuid[15]=0x80; capabilities[2].descriptive_label=(ams_mel_string_view_v1){"µ-third",8};
    for(unsigned repeat=0;repeat<50;++repeat) {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job *job=claimed("c2:job-ok",&c2,&va);
        if(!repeat) validation(job);
        // Poison inactive UTC in every relative variant, not just one.
        for(unsigned p=0;p<6;++p) if(points[p].kind>=2) {
            points[p].ecef.time_of_validity.fractional_femtoseconds=-1;
            points[p].lla.time_of_validity.fractional_femtoseconds=1000000000000000LL;
        }
        CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,span,D)==AMS_MEL_OK);
        CHECK(mock("mock_rf_job_interval_fidelity_v3")==1);
        if (!repeat) {
            unsigned before=mock("mock_rf_job_add_calls");
            CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE","interval-spatial-allocation",1)==0);
            CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,span,D)==AMS_MEL_INTERNAL_ERROR);
            CHECK(mock("mock_rf_job_add_calls")==before);
            CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE")==0);
        }
        // Swap the poison boundaries too: each inactive payload ignores both.
        for(unsigned p=0;p<6;++p) if(points[p].kind>=2) {
            points[p].ecef.time_of_validity.fractional_femtoseconds=1000000000000000LL;
            points[p].lla.time_of_validity.fractional_femtoseconds=-1;
        }
        CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,span,D)==AMS_MEL_OK);
        CHECK(mock("mock_rf_job_interval_fidelity_v3")==1);
        CHECK(setenv("AMS_MEL_TEST_F4_CASE","special",1)==0);
        points[0].ecef.location_m.x=INFINITY; points[0].ecef.velocity_mps.z=-INFINITY;
        points[1].platform_relative=(ams_mel_rf_az_el_v1){-INFINITY,INFINITY};
        points[2].face_relative=(ams_mel_rf_az_el_v1){NAN,-0.0}; points[3].face_relative=points[2].face_relative;
        points[4].lla.latitude_rad=-0.0; points[4].lla.longitude_rad=INFINITY;
        points[4].lla.altitude_m=-INFINITY; points[4].lla.velocity_north_mps=NAN;
        points[5].baseline_relative_conic_rad=-0.0;
        CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,span,D)==AMS_MEL_OK);
        CHECK(mock("mock_rf_job_interval_fidelity_v3")==1);
        CHECK(unsetenv("AMS_MEL_TEST_F4_CASE")==0);
        points[0].ecef.location_m.x=1.25; points[0].ecef.velocity_mps.z=-6.75;
        points[1].platform_relative=(ams_mel_rf_az_el_v1){-0.75,0.25};
        points[2].face_relative=(ams_mel_rf_az_el_v1){1.5,-0.5}; points[3].face_relative=points[2].face_relative;
        points[4].lla.latitude_rad=0.125; points[4].lla.longitude_rad=-1.25;
        points[4].lla.altitude_m=12345.5; points[4].lla.velocity_north_mps=11.25;
        points[5].baseline_relative_conic_rad=-2.25;
        unsigned before=mock("mock_rf_job_add_calls");
        intervals[0].status_enable=AMS_MEL_RF_INTERVAL_STATUS_ALWAYS;
        intervals[1].status_enable=AMS_MEL_RF_INTERVAL_STATUS_ON_EXCEPTION;
        CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        CHECK(mock("mock_rf_job_add_calls")==before);
        ams_mel_rf_job_interval_status *stream=NULL;
        const ams_mel_rf_job_interval_status_options_v1 options={2,1,0};
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,D)==AMS_MEL_OK);
        CHECK(setenv("AMS_MEL_TEST_F4_STATUS","enabled",1)==0);
        CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,span,D)==AMS_MEL_OK);
        CHECK(mock("mock_rf_job_interval_fidelity_v3")==1);
        const char *failures[]={"std","unknown","alloc"};
        for(unsigned f=0;f<3;++f) {
            before=mock("mock_rf_job_add_calls"); CHECK(setenv("AMS_MEL_TEST_F4_ADD_FAILURE",failures[f],1)==0);
            CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,span,D)==(f==2 ? AMS_MEL_INTERNAL_ERROR : AMS_MEL_PROVIDER_EXCEPTION));
            CHECK(mock("mock_rf_job_add_calls")==before+1);
            CHECK(unsetenv("AMS_MEL_TEST_F4_ADD_FAILURE")==0);
            CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,span,D)==AMS_MEL_OK);
            CHECK(mock("mock_rf_job_interval_fidelity_v3")==1);
        }
        CHECK(ams_mel_rf_job_interval_status_close(&stream,D)==AMS_MEL_OK);
        before=mock("mock_rf_job_add_calls");
        CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        CHECK(mock("mock_rf_job_add_calls")==before);
        intervals[0].status_enable=intervals[1].status_enable=AMS_MEL_RF_INTERVAL_STATUS_NEVER;
        CHECK(unsetenv("AMS_MEL_TEST_F4_STATUS")==0);
        CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,span,D)==AMS_MEL_OK);
        if(repeat%2) CHECK(ams_mel_rf_job_finalize(job,D)==AMS_MEL_OK);
        else { ams_mel_rf_job_cancel_result_v1 answer={0}; CHECK(ams_mel_rf_job_cancel(job,&answer,D)==AMS_MEL_OK); }
        before=mock("mock_rf_job_add_calls");
        CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        CHECK(mock("mock_rf_job_add_calls")==before);
        if(repeat%2) { ams_mel_rf_job_status_t status=0; CHECK(ams_mel_rf_job_wait_status(job,3000,&status,D)==AMS_MEL_OK); }
        no_queries();
        CHECK(ams_mel_rf_job_close(&job,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,D)==AMS_MEL_OK);
    }
    // A callback may have been stored before registration threw: normal-return
    // evidence is required, and this Job cannot reuse that registration.
    {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job *job=claimed("c2:status-throw",&c2,&va);
        ams_mel_rf_job_interval_status *stream=NULL;
        const ams_mel_rf_job_interval_status_options_v1 options={2,1,0};
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,D)==AMS_MEL_PROVIDER_EXCEPTION && !stream);
        intervals[0].status_enable=AMS_MEL_RF_INTERVAL_STATUS_ALWAYS;
        unsigned before=mock("mock_rf_job_add_calls");
        CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        CHECK(mock("mock_rf_job_add_calls")==before);
        intervals[0].status_enable=AMS_MEL_RF_INTERVAL_STATUS_NEVER;
        CHECK(ams_mel_rf_job_add_rx_intervals_v3(job,span,D)==AMS_MEL_OK);
        no_queries();
        CHECK(ams_mel_rf_job_close(&job,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,D)==AMS_MEL_OK);
    }
    puts("PASS: native RX JobInterval spatial focused repeat 50/50"); return 0;
}
