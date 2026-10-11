#define main product_stream_regression
#include "test_rf_job_product_stream_params.c"
#undef main
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
static uint64_t control(unsigned e,unsigned f)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    uint64_t (*fn)(unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f11_value"); CHECK(fn);
    uint64_t value=fn(0,e,f); CHECK(dlclose(lib)==0); return value;
}
static double component(unsigned e,unsigned v,unsigned c)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    double (*fn)(unsigned,unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f11_double"); CHECK(fn);
    double value=fn(0,e,v,c); CHECK(dlclose(lib)==0); return value;
}
int main(void)
{
    CHECK(product_stream_regression()==0);
    for(unsigned repeat=0;repeat<50;++repeat) {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job *job=claimed("c2:job-ok",&c2,&va);
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
