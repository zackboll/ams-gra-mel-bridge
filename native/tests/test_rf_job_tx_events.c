#include <math.h>
/* Reuse the F8 fixture and its full regression exercise without duplicating the
 * production builder. This translation unit is compiled as C11 in Release too. */
#define main product_stream_regression
#include "test_rf_job_product_stream_params.c"
#undef main
static uint64_t map_value(unsigned i,const char *g,const char *p,unsigned f)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    uint64_t (*fn)(unsigned,const char *,const char *,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f9_value"); CHECK(fn);
    uint64_t answer=fn(i,g,p,f); CHECK(dlclose(lib)==0); return answer;
}
static uint64_t tx_value(unsigned i,unsigned e,unsigned f,unsigned g)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    uint64_t (*fn)(unsigned,unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f10_value"); CHECK(fn);
    uint64_t answer=fn(i,e,f,g); CHECK(dlclose(lib)==0); return answer;
}
static double tx_double(unsigned i,unsigned e,unsigned f)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    double (*fn)(unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f10_double"); CHECK(fn);
    double answer=fn(i,e,f); CHECK(dlclose(lib)==0); return answer;
}
static void invalid_tx(ams_mel_rf_job *job,ams_mel_rf_job_interval_config_span_v9 span)
{
    unsigned before=mock("mock_rf_job_add_calls");
    CHECK(ams_mel_rf_job_add_intervals_v9(job,span,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(mock("mock_rf_job_add_calls")==before);
}
int main(void)
{
    CHECK(product_stream_regression()==0);
    for(unsigned repeat=0;repeat<50;++repeat) {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job *job=claimed("c2:job-ok",&c2,&va);
        char label[]="rx/β ";
        ams_mel_rf_interval_endpoint_connection_v1 entries[]={
            {{label,sizeof label-1},{"IQ",2},1},
            {{label,sizeof label-1},{"PDW",3},0},
            {{label,sizeof label-1},{"IQ",2},UINT64_MAX},
            {{"",0},{"",0},UINT64_C(0x8000000000000000)},
            {{"",0},{"default",7},1}};
        uint64_t group=UINT64_MAX;
        ams_mel_rf_product_stream_endpoint_v1 ep={1,UINT64_MAX,1};
        ams_mel_rf_lf_address_value_v1 write={UINT64_MAX,1};
        ams_mel_rf_lf_command_v1 command={42,0,{&write,1}};
        ams_mel_rf_job_interval_config_v9 input[3]={0};
        uint64_t tx_groups[]={0,UINT64_MAX,0};
        ams_mel_rf_tx_event_config_v1 tx[2]={
            {UINT32_MAX,{label,sizeof label-1},INT64_MIN,INT64_MAX,-0.0,UINT64_MAX,
             {tx_groups,3},-12.5,INT64_MAX,INT64_MIN},
            {0,{label,sizeof label-1},-7,9,1.25,0,{NULL,0},INFINITY,-8,6}};
        input[0].has_transmit_events=1;
        input[0].transmit_events=(ams_mel_rf_tx_event_config_span_v1){tx,2};
        input[1].transmit_events=(ams_mel_rf_tx_event_config_span_v1){(void *)1,SIZE_MAX};
        input[2].has_transmit_events=1;
        input[0].interval.has_endpoint_connections=1;
        input[0].interval.endpoint_connections=(ams_mel_rf_interval_endpoint_connection_span_v1){entries,5};
        input[0].interval.interval.has_product_stream_params=1;
        input[0].interval.interval.product_stream_params=(ams_mel_rf_product_stream_params_v1){{&group,1},{&ep,1}};
        input[0].interval.interval.interval.local_function_commands=(ams_mel_rf_lf_command_span_v1){&command,1};
        input[1].interval.endpoint_connections=(ams_mel_rf_interval_endpoint_connection_span_v1){(void *)1,SIZE_MAX};
        input[2].interval.has_endpoint_connections=1; /* explicit empty */
        ams_mel_rf_job_interval_config_span_v9 span={input,3};
        CHECK(ams_mel_rf_job_add_intervals_v9(job,span,D)==AMS_MEL_OK);
        CHECK(tx_value(0,0,0,0)==2 && tx_value(0,0,1,0)==0);
        CHECK(tx_value(0,0,2,0)==UINT32_MAX && tx_value(0,1,2,0)==0);
        CHECK(tx_value(0,0,3,0)==UINT64_MAX && tx_value(0,1,3,0)==0);
        CHECK(tx_value(0,0,4,0)==3 && tx_value(0,0,5,1)==UINT64_MAX);
        CHECK(tx_value(0,0,6,0)==(uint64_t)INT64_MIN && tx_value(0,0,7,0)==INT64_MAX);
        CHECK(tx_value(0,0,8,0)==INT64_MAX && tx_value(0,0,9,0)==(uint64_t)INT64_MIN);
        CHECK(signbit(tx_double(0,0,0)) && tx_double(0,0,1)==-12.5 && isinf(tx_double(0,1,1)));
        for(unsigned e=0;e<2;++e) {
            CHECK(tx_value(0,e,10,0)==1 && tx_value(0,e,11,0)==1);
            CHECK(tx_value(0,e,12,0)==sizeof label-1);
            for(unsigned c=0;c<sizeof label-1;++c) CHECK(tx_value(0,e,13,c)==(unsigned char)label[c]);
        }
        CHECK(tx_value(1,0,0,0)==0 && tx_value(2,0,0,0)==0);
        input[0].has_transmit_events=2; invalid_tx(job,span); input[0].has_transmit_events=1;
        input[0].transmit_events=(ams_mel_rf_tx_event_config_span_v1){NULL,1}; invalid_tx(job,span);
        input[0].transmit_events=(ams_mel_rf_tx_event_config_span_v1){(void *)1,SIZE_MAX}; invalid_tx(job,span);
        input[0].transmit_events=(ams_mel_rf_tx_event_config_span_v1){tx,2};
        ams_mel_u64_span_v1 saved_groups=tx[0].applicable_tx_element_groups;
        tx[0].applicable_tx_element_groups=(ams_mel_u64_span_v1){NULL,1}; invalid_tx(job,span);
        tx[0].applicable_tx_element_groups=(ams_mel_u64_span_v1){(void *)1,SIZE_MAX}; invalid_tx(job,span);
        tx[0].applicable_tx_element_groups=saved_groups;
        ams_mel_rf_receive_event_config_v4 rx[2]={0};
        rx[0].event.event.event.event_id=42; rx[1].event.event.event.event_id=7;
        input[0].interval.interval.interval.interval.receive_events=(ams_mel_rf_receive_event_config_span_v4){rx,2};
        CHECK(ams_mel_rf_job_add_intervals_v9(job,span,D)==AMS_MEL_OK);
        CHECK(tx_value(0,0,0,0)==2 && tx_value(0,0,1,0)==2);
        CHECK(tx_value(0,0,14,0)==42 && tx_value(0,1,14,0)==7);
        input[0].has_transmit_events=0;
        CHECK(ams_mel_rf_job_add_intervals_v9(job,span,D)==AMS_MEL_OK);
        CHECK(tx_value(0,0,0,0)==0 && tx_value(0,0,1,0)==2);
        CHECK(tx_value(0,0,14,0)==42 && tx_value(0,1,14,0)==7);
        input[0].has_transmit_events=1;
        CHECK(ams_mel_rf_job_add_intervals_v9(job,span,D)==AMS_MEL_OK);
        CHECK(map_value(0,label,"IQ",0)==2 && map_value(0,label,"IQ",1)==2);
        CHECK(map_value(0,label,"IQ",3)==UINT64_MAX && map_value(0,label,"PDW",2)==1 && map_value(0,label,"PDW",3)==0);
        CHECK(map_value(0,"","",3)==UINT64_C(0x8000000000000000) && map_value(0,"","default",3)==1);
        CHECK(map_value(1,"","",0)==0 && map_value(2,"","",0)==0);
        CHECK(value(0,0,3)==1 && value(0,0,4)==UINT64_MAX && lf_value(4)==UINT64_MAX);
        label[0]='X'; entries[2].endpoint_id=0;
        CHECK(map_value(0,"rx/β ","IQ",3)==UINT64_MAX); label[0]='r'; entries[2].endpoint_id=UINT64_MAX;
        input[0].interval.has_endpoint_connections=2; invalid_tx(job,span); input[0].interval.has_endpoint_connections=1;
        input[0].interval.endpoint_connections=(ams_mel_rf_interval_endpoint_connection_span_v1){NULL,1}; invalid_tx(job,span);
        input[0].interval.endpoint_connections=(ams_mel_rf_interval_endpoint_connection_span_v1){(void *)1,SIZE_MAX}; invalid_tx(job,span);
        input[0].interval.endpoint_connections=(ams_mel_rf_interval_endpoint_connection_span_v1){entries,5};
        const ams_mel_string_view_v1 bad[]={{NULL,1},{"x",SIZE_MAX},{"\xff",1},{"x\0y",3}};
        for(unsigned k=0;k<sizeof bad/sizeof bad[0];++k) {
            ams_mel_string_view_v1 saved=entries[0].element_group_label;
            entries[0].element_group_label=bad[k]; invalid_tx(job,span); entries[0].element_group_label=saved;
            saved=tx[0].element_group_label; tx[0].element_group_label=bad[k]; invalid_tx(job,span); tx[0].element_group_label=saved;
        }
        unsigned before=mock("mock_rf_job_add_calls");
        CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE","interval-tx-allocation",1)==0);
        CHECK(ams_mel_rf_job_add_intervals_v9(job,span,D)==AMS_MEL_INTERNAL_ERROR);
        CHECK(mock("mock_rf_job_add_calls")==before); CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE")==0);
        CHECK(ams_mel_rf_job_add_intervals_v9(job,span,D)==AMS_MEL_OK); no_queries();
        CHECK(ams_mel_rf_job_flush(job,D)==AMS_MEL_OK);
        input[0].interval.interval.interval.interval.status_enable=AMS_MEL_RF_INTERVAL_STATUS_ALWAYS;
        CHECK(ams_mel_rf_job_add_intervals_v9(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        ams_mel_rf_job_interval_status *stream=NULL;
        const ams_mel_rf_job_interval_status_options_v1 options={2,1,0};
        CHECK(ams_mel_rf_job_interval_status_open(job,&options,&stream,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_add_intervals_v9(job,span,D)==AMS_MEL_OK);
        CHECK(status_mode()==AMS_MEL_RF_INTERVAL_STATUS_ALWAYS);
        const char *failures[]={"std","unknown","alloc"};
        for(unsigned f=0;f<3;++f) {
            before=mock("mock_rf_job_add_calls");
            CHECK(setenv("AMS_MEL_TEST_F4_ADD_FAILURE",failures[f],1)==0);
            CHECK(ams_mel_rf_job_add_intervals_v9(job,span,D)==(f==2?AMS_MEL_INTERNAL_ERROR:AMS_MEL_PROVIDER_EXCEPTION));
            CHECK(mock("mock_rf_job_add_calls")==before+1);
            CHECK(unsetenv("AMS_MEL_TEST_F4_ADD_FAILURE")==0);
            CHECK(ams_mel_rf_job_add_intervals_v9(job,span,D)==AMS_MEL_OK);
        }
        CHECK(ams_mel_rf_job_interval_status_close(&stream,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_add_intervals_v9(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        input[0].interval.interval.interval.interval.status_enable=AMS_MEL_RF_INTERVAL_STATUS_NEVER;
        /* Old v7 remains default-empty after a populated v8 submission. */
        CHECK(ams_mel_rf_job_add_rx_intervals_v7(job,(ams_mel_rf_job_interval_config_span_v7){&input[0].interval.interval,1},D)==AMS_MEL_OK);
        CHECK(map_value(0,"","",0)==0);
        CHECK(ams_mel_rf_c2_close(&c2,D)==AMS_MEL_OK); CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_add_intervals_v9(job,span,D)==AMS_MEL_OK);
        if(repeat%2) {
            CHECK(ams_mel_rf_job_finalize(job,D)==AMS_MEL_OK);
            ams_mel_rf_job_status_t status=0;
            CHECK(ams_mel_rf_job_wait_status(job,3000,&status,D)==AMS_MEL_OK);
        } else {
            ams_mel_rf_job_cancel_result_v1 answer={0};
            CHECK(ams_mel_rf_job_cancel(job,&answer,D)==AMS_MEL_OK);
        }
        CHECK(ams_mel_rf_job_add_intervals_v9(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        CHECK(ams_mel_rf_job_close(&job,D)==AMS_MEL_OK);
    }
    puts("PASS: TX events focused 50/50"); return 0;
}
