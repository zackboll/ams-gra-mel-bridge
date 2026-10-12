#define main product_stream_regression
#include "test_rf_job_product_stream_params.c"
#undef main
#include <unistd.h>
static uint64_t scalar(unsigned i, unsigned e, unsigned f)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    uint64_t (*fn)(unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f5_scalar"); CHECK(fn);
    uint64_t result=fn(i,e,f); CHECK(dlclose(lib)==0); return result;
}
static double number(unsigned i, unsigned e, unsigned v, unsigned c)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    double (*fn)(unsigned,unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f5_double"); CHECK(fn);
    double result=fn(i,e,v,c); CHECK(dlclose(lib)==0); return result;
}
static uint64_t control_at(unsigned i,unsigned e,unsigned f)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    uint64_t (*fn)(unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f11_value"); CHECK(fn);
    uint64_t value=fn(i,e,f); CHECK(dlclose(lib)==0); return value;
}
static double component_at(unsigned i,unsigned e,unsigned v,unsigned c)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    double (*fn)(unsigned,unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f11_double"); CHECK(fn);
    double value=fn(i,e,v,c); CHECK(dlclose(lib)==0); return value;
}
static uint64_t control(unsigned e,unsigned f) { return control_at(0,e,f); }
static double component(unsigned e,unsigned v,unsigned c) { return component_at(0,e,v,c); }
static uint64_t inherited(unsigned i,unsigned e,unsigned f,unsigned g)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    uint64_t (*fn)(unsigned,unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f10_value"); CHECK(fn);
    uint64_t result=fn(i,e,f,g); CHECK(dlclose(lib)==0); return result;
}
static void same_number(double actual,double expected)
{
    if(isnan(expected)) CHECK(isnan(actual));
    else { CHECK(actual==expected); if(expected==0) CHECK(!!signbit(actual)==!!signbit(expected)); }
}
static double inherited_number(unsigned i,unsigned e,unsigned f)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    double (*fn)(unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f10_double"); CHECK(fn);
    double result=fn(i,e,f); CHECK(dlclose(lib)==0); return result;
}
static void observe_fixture(ams_mel_rf_tx_event_config_v2 events[2][2])
{
    for(unsigned i=0;i<2;++i) {
        CHECK(inherited(i,0,0,0)==2 && inherited(i,0,1,0)==1);
        CHECK(inherited(i,0,14,0)==80+i);
        CHECK(inherited(i,0,15,0)==10+i && inherited(i,0,16,0)==20+i);
        CHECK(scalar(i,0,9)==9+i && number(i,0,0,4)==3.25+i);
        for(unsigned e=0;e<2;++e) {
            const ams_mel_rf_tx_event_config_v2 *v=&events[i][e];
            const uint64_t fields[]={v->polarization.size,v->polarization_beam_steer_correction,
                v->execution_type,v->termination_type,v->allow_delay_start,v->iteration_hold_count,
                v->iteration_termination_count,v->channelization_enabled,1};
            for(unsigned f=0;f<9;++f) CHECK(control_at(i,e,f)==fields[f]);
            CHECK(control_at(i,e,9)==1);
            same_number(component_at(i,e,0,4),v->phase_offset_rad);
            for(unsigned n=0;n<v->polarization.size;++n) {
                const ams_mel_rf_stokes_vector_v1 *t=&v->polarization.data[n];
                const double values[]={t->s0,t->s1,t->s2,t->s3};
                for(unsigned c=0;c<4;++c) same_number(component_at(i,e,n,c),values[c]);
            }
            CHECK(inherited(i,e,2,0)==v->event.event_id);
            CHECK(inherited(i,e,3,0)==v->event.stab_point_index);
            CHECK(inherited(i,e,4,0)==v->event.applicable_tx_element_groups.size);
            for(unsigned g=0;g<v->event.applicable_tx_element_groups.size;++g)
                CHECK(inherited(i,e,5,g)==v->event.applicable_tx_element_groups.data[g]);
            const uint64_t times[]={(uint64_t)v->event.start_femtoseconds,(uint64_t)v->event.duration_femtoseconds,
                (uint64_t)v->event.rise_duration_femtoseconds,(uint64_t)v->event.fall_duration_femtoseconds};
            for(unsigned f=0;f<4;++f) CHECK(inherited(i,e,6+f,0)==times[f]);
            CHECK(inherited(i,e,10,0)==1 && inherited(i,e,11,0)==1);
            same_number(inherited_number(i,e,0),v->event.center_frequency_hz);
            same_number(inherited_number(i,e,1),v->event.tx_attenuation_db);
            CHECK(inherited(i,e,12,0)==v->event.element_group_label.size);
            for(unsigned c=0;c<v->event.element_group_label.size;++c)
                CHECK(inherited(i,e,13,c)==(unsigned char)v->event.element_group_label.data[c]);
        }
    }
}
static void nested_qualification(ams_mel_rf_job *job)
{
    ams_mel_rf_stokes_vector_v1 stokes[4][2]={
        {{1,-0.0,INFINITY,NAN},{-5,6,7,8}},{{9,10,-INFINITY,12},{0,0,0,0}},
        {{13,14,15,16},{0,0,0,0}},{{-17,18,19,20},{21,22,23,24}}};
    const char *labels[]={"first/β","second","third/µ","fourth"};
    uint64_t groups[]={0,SIZE_MAX};
    ams_mel_rf_tx_event_config_v2 events[2][2]={0};
    ams_mel_rf_receive_event_config_v4 rx[2]={0};
    ams_mel_rf_job_interval_config_v10 inputs[2]={0};
    const double phases[]={-0.0,INFINITY,NAN,-4.25};
    const unsigned counts[]={2,1,0,2};
    for(unsigned i=0;i<2;++i) {
        rx[i].event.event.event.event_id=80+i;
        rx[i].event.phase_offset_rad=3.25+i; rx[i].event.iteration_hold_count=9+i;
        inputs[i].interval.interval.interval.interval.receive_events=(ams_mel_rf_receive_event_config_span_v4){&rx[i],1};
        inputs[i].has_transmit_events=1;
        inputs[i].interval.interval.interval.interval.interval_id=10+i;
        inputs[i].interval.interval.interval.interval.interval_start_femtoseconds=20+i;
        inputs[i].transmit_events=(ams_mel_rf_tx_event_config_span_v2){events[i],2};
        for(unsigned e=0;e<2;++e) {
            unsigned k=i*2+e; ams_mel_rf_tx_event_config_v2 *v=&events[i][e];
            v->event.event_id=17+k; v->event.element_group_label=(ams_mel_string_view_v1){labels[k],strlen(labels[k])};
            v->event.start_femtoseconds=-7-(int)k; v->event.duration_femtoseconds=11+k;
            v->event.rise_duration_femtoseconds=3+k; v->event.fall_duration_femtoseconds=-3-(int)k;
            v->event.stab_point_index=SIZE_MAX-k;
            v->event.center_frequency_hz=100.25+k;
            v->event.tx_attenuation_db=-12.5-k;
            v->event.applicable_tx_element_groups=(ams_mel_u64_span_v1){groups,2};
            v->polarization=(ams_mel_rf_stokes_vector_span_v1){stokes[k],counts[k]};
            v->phase_offset_rad=phases[k]; v->polarization_beam_steer_correction=k%2;
            v->execution_type=k%2; v->termination_type=1-k%2;
            v->allow_delay_start=(k/2)%2; v->channelization_enabled=(k==0 || k==3);
            v->iteration_hold_count=SIZE_MAX-k; v->iteration_termination_count=2+k;
        }
    }
    ams_mel_rf_job_interval_config_span_v10 span={inputs,2};
    CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_OK); observe_fixture(events);
    uint32_t *independent[]={&events[1][1].polarization_beam_steer_correction,
        &events[1][1].execution_type,&events[1][1].termination_type,
        &events[1][1].allow_delay_start,&events[1][1].channelization_enabled};
    for(unsigned f=0;f<5;++f) {
        *independent[f]^=1;
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_OK);
        observe_fixture(events);
        *independent[f]^=1;
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_OK);
        observe_fixture(events);
    }
    for(unsigned trial=0;trial<13;++trial) {
        ams_mel_rf_tx_event_config_v2 saved=events[1][1];
        ams_mel_rf_tx_event_config_span_v2 saved_span=inputs[1].transmit_events;
        unsigned before=mock("mock_rf_job_add_calls");
        switch(trial) {
        case 0: events[1][1].event.element_group_label=(ams_mel_string_view_v1){"\xff",1}; break;
        case 1: events[1][1].event.element_group_label=(ams_mel_string_view_v1){"x\0y",3}; break;
        case 2: events[1][1].event.applicable_tx_element_groups=(ams_mel_u64_span_v1){NULL,1}; break;
        case 3: events[1][1].event.applicable_tx_element_groups=(ams_mel_u64_span_v1){groups,SIZE_MAX}; break;
        case 4: inputs[1].transmit_events=(ams_mel_rf_tx_event_config_span_v2){events[1],SIZE_MAX}; break;
        case 5: events[1][1].polarization=(ams_mel_rf_stokes_vector_span_v1){NULL,1}; break;
        case 6: events[1][1].polarization=(ams_mel_rf_stokes_vector_span_v1){stokes[3],3}; break;
        case 7: events[1][1].execution_type=2; break;
        case 8: events[1][1].termination_type=2; break;
        case 9: events[1][1].allow_delay_start=2; break;
        case 10: events[1][1].channelization_enabled=2; break;
        case 11: events[1][1].polarization_beam_steer_correction=2; break;
        default:
            if(SIZE_MAX==UINT64_MAX) continue;
            events[1][1].iteration_hold_count=UINT64_MAX; break;
        }
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_INVALID_ARGUMENT);
        CHECK(mock("mock_rf_job_add_calls")==before);
        events[1][1]=saved; inputs[1].transmit_events=saved_span;
        observe_fixture(events);
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_OK); observe_fixture(events);
    }
    const char *hooks[]={"later-tx-polarization","later-tx-interval"};
    for(unsigned h=0;h<2;++h) {
        char path[]="/tmp/ams-f11-hook-XXXXXX"; int fd=mkstemp(path); CHECK(fd>=0); CHECK(close(fd)==0);
        CHECK(setenv("AMS_MEL_TEST_RF_TX_TRACE",path,1)==0);
        CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE",hooks[h],1)==0);
        unsigned before=mock("mock_rf_job_add_calls");
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_INTERNAL_ERROR);
        CHECK(mock("mock_rf_job_add_calls")==before); observe_fixture(events);
        FILE *file=fopen(path,"r"); CHECK(file); char log[512]={0}; CHECK(fread(log,1,sizeof log-1,file)>0);
        CHECK(fclose(file)==0); CHECK(strstr(log,hooks[h]));
        CHECK(strstr(log,"preparation-unwound")); CHECK(unlink(path)==0);
        CHECK(unsetenv("AMS_MEL_TEST_RF_TX_TRACE")==0); CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE")==0);
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_OK); observe_fixture(events);
    }
    CHECK(ams_mel_rf_job_add_rx_intervals_v8(job,(ams_mel_rf_job_interval_config_span_v8){&inputs[0].interval,1},D)==AMS_MEL_OK);
    CHECK(inherited(0,0,0,0)==0 && inherited(0,0,14,0)==80 && scalar(0,0,9)==9);
    CHECK(inherited(0,0,15,0)==10 && inherited(0,0,16,0)==20);
    CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_OK);
    CHECK(ams_mel_rf_job_add_rx_intervals_v7(job,(ams_mel_rf_job_interval_config_span_v7){&inputs[0].interval.interval,1},D)==AMS_MEL_OK);
    CHECK(inherited(0,0,0,0)==0 && inherited(0,0,14,0)==80 && number(0,0,0,4)==3.25);
    CHECK(inherited(0,0,15,0)==10 && inherited(0,0,16,0)==20);
}
int main(void)
{
    CHECK(product_stream_regression()==0);
    for(unsigned repeat=0;repeat<50;++repeat) {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job *job=claimed("c2:job-ok",&c2,&va);
        nested_qualification(job);
        ams_mel_rf_stokes_vector_v1 stokes[2]={{1,-0.0,INFINITY,NAN},{-5,6,7,8}};
        ams_mel_rf_tx_event_config_v2 events[2]={0};
        events[0].event.event_id=17; events[1].event.event_id=29;
        events[0].polarization=(ams_mel_rf_stokes_vector_span_v1){stokes,2};
        events[0].polarization_beam_steer_correction=1;
        events[0].phase_offset_rad=-0.0;
        events[0].execution_type=AMS_MEL_RF_EXECUTION_CONDITIONAL;
        events[0].termination_type=AMS_MEL_RF_EVENT_TERMINATION_CANCEL;
        events[0].allow_delay_start=1; events[0].iteration_hold_count=SIZE_MAX;
        events[0].iteration_termination_count=1; events[0].channelization_enabled=1;
        ams_mel_rf_job_interval_config_v10 intervals[2]={0};
        ams_mel_rf_receive_event_config_v4 rx={0};
        rx.event.phase_offset_rad=3.25;
        rx.event.iteration_hold_count=9;
        intervals[0].interval.interval.interval.interval.receive_events=
            (ams_mel_rf_receive_event_config_span_v4){&rx,1};
        intervals[0].has_transmit_events=1;
        intervals[0].transmit_events=(ams_mel_rf_tx_event_config_span_v2){events,2};
        intervals[1].transmit_events=(ams_mel_rf_tx_event_config_span_v2){(void *)1,SIZE_MAX};
        ams_mel_rf_job_interval_config_span_v10 span={intervals,2};
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_OK);
        CHECK(scalar(0,0,9)==9 && scalar(0,0,6)==AMS_MEL_RF_EXECUTION_NORMAL && number(0,0,0,4)==3.25);
        CHECK(control(0,0)==2 && control(0,1)==1 && control(0,2)==1 && control(0,3)==1);
        CHECK(control(0,4)==1 && control(0,5)==SIZE_MAX && control(0,6)==1 && control(0,7)==1 && control(0,8)==1);
        CHECK(component(0,0,0)==1 && signbit(component(0,0,1)) && isinf(component(0,0,2)) && isnan(component(0,0,3)));
        CHECK(component(0,1,0)==-5 && component(0,1,3)==8 && signbit(component(0,0,4)));
        for(unsigned f=0;f<8;++f) CHECK(control(1,f)==0);
        uint32_t *invalid[]={&events[1].polarization_beam_steer_correction,&events[1].execution_type,
            &events[1].termination_type,&events[1].allow_delay_start,&events[1].channelization_enabled,
            &intervals[1].has_transmit_events};
        for(unsigned k=0;k<sizeof invalid/sizeof invalid[0];++k) {
            unsigned before=mock("mock_rf_job_add_calls"); *invalid[k]=2;
            CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_INVALID_ARGUMENT);
            CHECK(mock("mock_rf_job_add_calls")==before); *invalid[k]=0;
            CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_OK);
        }
        events[1].polarization=(ams_mel_rf_stokes_vector_span_v1){(void *)1,3};
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_INVALID_ARGUMENT);
        events[1].polarization=(ams_mel_rf_stokes_vector_span_v1){NULL,1};
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_INVALID_ARGUMENT);
        events[1].polarization=(ams_mel_rf_stokes_vector_span_v1){stokes,1};
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_OK); CHECK(control(1,0)==1);
        unsigned before=mock("mock_rf_job_add_calls");
        intervals[1].has_transmit_events=1;
        intervals[1].transmit_events=(ams_mel_rf_tx_event_config_span_v2){NULL,1};
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_INVALID_ARGUMENT);
        CHECK(mock("mock_rf_job_add_calls")==before);
        intervals[1].has_transmit_events=0;
        CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE","interval-tx-allocation",1)==0);
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_INTERNAL_ERROR);
        CHECK(mock("mock_rf_job_add_calls")==before);
        CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE")==0);
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_OK);
        CHECK(scalar(0,0,9)==9);
        const char *failures[]={"std","unknown","alloc"};
        for(unsigned f=0;f<3;++f) {
            CHECK(setenv("AMS_MEL_TEST_F4_ADD_FAILURE",failures[f],1)==0);
            CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==
                  (f==2?AMS_MEL_INTERNAL_ERROR:AMS_MEL_PROVIDER_EXCEPTION));
            CHECK(unsetenv("AMS_MEL_TEST_F4_ADD_FAILURE")==0);
            CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_OK);
        }
        ams_mel_rf_job_interval_config_v9 old={0}; old.has_transmit_events=1;
        old.transmit_events=(ams_mel_rf_tx_event_config_span_v1){&events[0].event,1};
        CHECK(ams_mel_rf_job_add_intervals_v9(job,(ams_mel_rf_job_interval_config_span_v9){&old,1},D)==AMS_MEL_OK);
        for(unsigned f=0;f<8;++f) CHECK(control(0,f)==0);
        CHECK(component(0,0,4)==0);
        intervals[0].transmit_events=(ams_mel_rf_tx_event_config_span_v2){NULL,0};
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_flush(job,D)==AMS_MEL_OK);
        intervals[0].interval.interval.interval.interval.status_enable=AMS_MEL_RF_INTERVAL_STATUS_ALWAYS;
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        ams_mel_rf_job_interval_status *stream=NULL;
        const ams_mel_rf_job_interval_status_options_v1 options={2,1,0};
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_OK);
        CHECK(status_mode()==AMS_MEL_RF_INTERVAL_STATUS_ALWAYS);
        intervals[0].interval.interval.interval.interval.status_enable=AMS_MEL_RF_INTERVAL_STATUS_ON_EXCEPTION;
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_OK);
        CHECK(status_mode()==AMS_MEL_RF_INTERVAL_STATUS_ON_EXCEPTION);
        CHECK(ams_mel_rf_job_interval_status_close(&stream,D)==AMS_MEL_OK);
        intervals[0].interval.interval.interval.interval.status_enable=AMS_MEL_RF_INTERVAL_STATUS_NEVER;
        CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK); CHECK(ams_mel_rf_c2_close(&c2,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_OK);
        no_queries();
        if(repeat%2) {
            CHECK(ams_mel_rf_job_finalize(job,D)==AMS_MEL_OK);
            ams_mel_rf_job_status_t status=0;
            CHECK(ams_mel_rf_job_wait_status(job,3000,&status,D)==AMS_MEL_OK);
        } else {
            ams_mel_rf_job_cancel_result_v1 answer={0};
            CHECK(ams_mel_rf_job_cancel(job,&answer,D)==AMS_MEL_OK);
        }
        CHECK(ams_mel_rf_job_add_intervals_v10(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        CHECK(ams_mel_rf_job_close(&job,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK); CHECK(ams_mel_rf_c2_close(&c2,D)==AMS_MEL_OK);
    }
    puts("PASS: TX controls focused 50/50"); return 0;
}
