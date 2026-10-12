#define _POSIX_C_SOURCE 200809L
#include <math.h>
#define main inherited_regression
#define value inherited_value
#include "test_rf_job_product_stream_params.c"
#undef value
#undef main

static uint64_t tx_value(unsigned i,unsigned e,unsigned f,unsigned g)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    uint64_t (*fn)(unsigned,unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f10_value"); CHECK(fn);
    uint64_t result=fn(i,e,f,g); CHECK(dlclose(lib)==0); return result;
}
static int64_t value(unsigned i,unsigned m,unsigned f,unsigned n)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    int64_t (*fn)(unsigned,unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f12_value"); CHECK(fn);
    int64_t result=fn(i,m,f,n); CHECK(dlclose(lib)==0); return result;
}
static double number(unsigned i,unsigned m,unsigned n)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    double (*fn)(unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f12_double"); CHECK(fn);
    double result=fn(i,m,n); CHECK(dlclose(lib)==0); return result;
}
static void reject(ams_mel_rf_job *job,ams_mel_rf_job_interval_config_span_v11 span)
{
    unsigned before=mock("mock_rf_job_add_calls");
    CHECK(ams_mel_rf_job_add_intervals_v11(job,span,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(mock("mock_rf_job_add_calls")==before);
    CHECK(value(0,0,0,0)==3 && value(1,0,0,0)==2);
}
int main(void)
{
    CHECK(inherited_regression()==0);
    for(unsigned repeat=0;repeat<50;++repeat) {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job *job=claimed("c2:job-ok",&c2,&va);
        double chips[]={1.25,-0.0,INFINITY,NAN,1.25};
        double other[]={-2.5,3.75,-2.5};
        ams_mel_rf_modulation_config_v1 tables[2][3]={0};
        tables[0][0].kind=AMS_MEL_RF_MODULATION_TONE;
        tables[0][0].phase_chips_rad=(ams_mel_double_span_v1){(void *)1,SIZE_MAX};
        tables[0][1]=(ams_mel_rf_modulation_config_v1){AMS_MEL_RF_MODULATION_PHASE_CODE,INT64_MIN,{chips,5},NAN};
        tables[0][2]=(ams_mel_rf_modulation_config_v1){AMS_MEL_RF_MODULATION_LINEAR_FREQUENCY,INT64_MAX,{(void *)1,SIZE_MAX},-INFINITY};
        tables[1][0]=(ams_mel_rf_modulation_config_v1){AMS_MEL_RF_MODULATION_LINEAR_FREQUENCY,0,{(void *)1,SIZE_MAX},-0.0};
        tables[1][1]=(ams_mel_rf_modulation_config_v1){AMS_MEL_RF_MODULATION_PHASE_CODE,INT64_MAX,{other,3},INFINITY};
        int32_t first[]={2,-1,0,2,1}; int32_t second[]={1,0,-1,1};
        ams_mel_rf_tx_event_config_v3 events[2][2]={0};
        events[0][0].has_modulation_indices=1;
        events[0][0].modulation_indices=(ams_mel_i32_span_v1){first,5};
        events[0][1].modulation_indices=(ams_mel_i32_span_v1){(void *)1,SIZE_MAX};
        events[1][0].has_modulation_indices=1;
        events[1][0].modulation_indices=(ams_mel_i32_span_v1){second,4};
        events[1][1].has_modulation_indices=1; /* explicit empty */
        ams_mel_rf_job_interval_config_v11 inputs[2]={0};
        ams_mel_rf_receive_event_config_v4 rx[2]={0};
        ams_mel_rf_stokes_vector_v1 stokes[2]={{1,2,3,4},{5,6,7,8}};
        for(unsigned i=0;i<2;++i) {
            rx[i].event.event.event.event_id=80+i;
            rx[i].event.polarization=(ams_mel_rf_stokes_vector_span_v1){stokes,2};
            inputs[i].interval.interval.interval.interval.receive_events=(ams_mel_rf_receive_event_config_span_v4){&rx[i],1};
            inputs[i].has_transmit_events=1;
            inputs[i].transmit_events=(ams_mel_rf_tx_event_config_span_v3){events[i],2};
            inputs[i].has_modulations=1;
            inputs[i].modulations=(ams_mel_rf_modulation_config_span_v1){tables[i],3-i};
            for(unsigned e=0;e<2;++e) {
                events[i][e].event.event.event_id=10*i+e;
                events[i][e].event.polarization=(ams_mel_rf_stokes_vector_span_v1){stokes,2};
                events[i][e].event.phase_offset_rad=3.25+i+e;
            }
        }
        ams_mel_rf_job_interval_config_span_v11 span={inputs,2};
        const char *trace=getenv("AMS_MEL_F12_TRACE_FILE"); CHECK(trace!=NULL);
        FILE *trace_file=fopen(trace,"w"); CHECK(trace_file!=NULL); CHECK(fclose(trace_file)==0);
        CHECK(setenv("AMS_MEL_TEST_RF_TX_TRACE",trace,1)==0);
        CHECK(ams_mel_rf_job_add_intervals_v11(job,span,D)==AMS_MEL_OK);
        CHECK(unsetenv("AMS_MEL_TEST_RF_TX_TRACE")==0);
        trace_file=fopen(trace,"r"); CHECK(trace_file!=NULL);
        char line[256]; unsigned index_calls=0,table_calls=0,tx_calls=0;
        while(fgets(line,sizeof line,trace_file)) {
            if(strcmp(line,"setModIndex\n")==0) ++index_calls;
            if(strcmp(line,"setModulations\n")==0) ++table_calls;
            if(strcmp(line,"setTxEvents\n")==0) ++tx_calls;
        }
        CHECK(fclose(trace_file)==0);
        CHECK(index_calls==3 && table_calls==2 && tx_calls==2);
        CHECK(value(0,0,0,0)==3 && value(1,0,0,0)==2);
        for(unsigned i=0;i<2;++i) for(unsigned m=0;m<3-i;++m) {
            CHECK(value(i,m,1,0)==1 && value(i,m,2,0)==tables[i][m].kind);
        }
        CHECK(value(0,1,3,0)==INT64_MIN && value(1,1,3,0)==INT64_MAX);
        CHECK(value(0,1,4,0)==5 && value(1,1,4,0)==3);
        CHECK(number(0,1,0)==1.25 && signbit(number(0,1,1)));
        CHECK(isinf(number(0,1,2)) && isnan(number(0,1,3)) && number(0,1,4)==1.25);
        CHECK(number(1,1,0)==-2.5 && number(1,1,1)==3.75 && number(1,1,2)==-2.5);
        CHECK(isinf(number(0,2,0)) && signbit(number(0,2,0)) && signbit(number(1,0,0)));
        for(unsigned n=0;n<5;++n) CHECK(value(0,0,6,n)==first[n]);
        for(unsigned n=0;n<4;++n) CHECK(value(1,0,6,n)==second[n]);
        CHECK(value(0,1,5,0)==1 && value(0,1,6,0)==-1 && value(1,1,5,0)==0);
        for(unsigned i=0;i<2;++i) {
            CHECK(tx_value(i,0,1,0)==1 && tx_value(i,0,14,0)==80+i);
            CHECK(tx_value(i,0,11,0)==1 && tx_value(i,1,11,0)==1);
        }
        chips[0]=99; first[0]=0;
        CHECK(number(0,1,0)==1.25 && value(0,0,6,0)==2);
        chips[0]=1.25; first[0]=2;
        const int32_t invalid[]={-2,INT32_MIN,3,INT32_MAX};
        for(unsigned n=0;n<4;++n) { first[0]=invalid[n]; reject(job,span); }
        first[0]=2;
        second[0]=2; reject(job,span); second[0]=1; /* valid only in other interval */
        tables[1][1].kind=3; reject(job,span); tables[1][1].kind=1;
        tables[1][1].phase_chips_rad=(ams_mel_double_span_v1){NULL,1}; reject(job,span);
        tables[1][1].phase_chips_rad=(ams_mel_double_span_v1){(void *)1,SIZE_MAX}; reject(job,span);
        tables[1][1].phase_chips_rad=(ams_mel_double_span_v1){other,3};
        inputs[1].has_modulations=2; reject(job,span); inputs[1].has_modulations=1;
        inputs[1].has_transmit_events=2; reject(job,span); inputs[1].has_transmit_events=1;
        events[1][1].has_modulation_indices=2; reject(job,span); events[1][1].has_modulation_indices=1;
        events[1][1].modulation_indices=(ams_mel_i32_span_v1){NULL,1}; reject(job,span);
        events[1][1].modulation_indices=(ams_mel_i32_span_v1){(void *)1,SIZE_MAX}; reject(job,span);
        events[1][1].modulation_indices=(ams_mel_i32_span_v1){NULL,0};
        inputs[1].modulations=(ams_mel_rf_modulation_config_span_v1){NULL,1}; reject(job,span);
        inputs[1].modulations=(ams_mel_rf_modulation_config_span_v1){(void *)1,SIZE_MAX}; reject(job,span);
        inputs[1].modulations=(ams_mel_rf_modulation_config_span_v1){tables[1],2};
        reject(job,(ams_mel_rf_job_interval_config_span_v11){NULL,1});
        reject(job,(ams_mel_rf_job_interval_config_span_v11){(void *)1,SIZE_MAX});
        inputs[1].transmit_events=(ams_mel_rf_tx_event_config_span_v3){NULL,1}; reject(job,span);
        inputs[1].transmit_events=(ams_mel_rf_tx_event_config_span_v3){(void *)1,SIZE_MAX}; reject(job,span);
        inputs[1].transmit_events=(ams_mel_rf_tx_event_config_span_v3){events[1],2};
        CHECK(ams_mel_rf_job_add_intervals_v11(job,span,D)==AMS_MEL_OK);
        inputs[1].has_transmit_events=0;
        const char *failures[]={"later-modulation-definition","later-modulation-references","later-tx-interval"};
        inputs[1].has_transmit_events=1;
        for(unsigned f=0;f<3;++f) {
            unsigned before=mock("mock_rf_job_add_calls");
            const char *trace=getenv("AMS_MEL_F12_TRACE_FILE");
            CHECK(trace!=NULL);
            FILE *file=fopen(trace,"w"); CHECK(file!=NULL); CHECK(fclose(file)==0);
            CHECK(setenv("AMS_MEL_TEST_RF_TX_TRACE",trace,1)==0);
            CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE",failures[f],1)==0);
            CHECK(ams_mel_rf_job_add_intervals_v11(job,span,D)==AMS_MEL_INTERNAL_ERROR);
            CHECK(mock("mock_rf_job_add_calls")==before);
            CHECK(value(0,0,0,0)==3 && value(1,0,0,0)==2);
            CHECK(unsetenv("AMS_MEL_TEST_RF_TX_TRACE")==0);
            file=fopen(trace,"r"); CHECK(file!=NULL);
            char text[2048]={0}; CHECK(fread(text,1,sizeof text-1,file)>0); CHECK(fclose(file)==0);
            CHECK(strstr(text,failures[f])!=NULL && strstr(text,"preparation-unwound")!=NULL);
            CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE")==0);
            CHECK(ams_mel_rf_job_add_intervals_v11(job,span,D)==AMS_MEL_OK);
        }
        inputs[1].has_transmit_events=0;
        inputs[1].transmit_events=(ams_mel_rf_tx_event_config_span_v3){(void *)1,SIZE_MAX};
        CHECK(ams_mel_rf_job_add_intervals_v11(job,span,D)==AMS_MEL_OK);
        CHECK(value(1,0,0,0)==2); /* unused definitions, no TX */
        inputs[0].has_transmit_events=0; inputs[0].has_modulations=0;
        inputs[0].modulations=(ams_mel_rf_modulation_config_span_v1){(void *)1,SIZE_MAX};
        CHECK(ams_mel_rf_job_add_intervals_v11(job,span,D)==AMS_MEL_OK);
        CHECK(value(0,0,0,0)==0);
        inputs[0].has_transmit_events=1;
        inputs[0].has_modulations=1;
        inputs[0].modulations=(ams_mel_rf_modulation_config_span_v1){NULL,0};
        first[0]=-1; first[2]=-1; first[3]=-1; first[4]=-1;
        CHECK(ams_mel_rf_job_add_intervals_v11(job,span,D)==AMS_MEL_OK);
        CHECK(value(0,0,0,0)==0 && value(0,0,6,0)==-1);
        first[0]=0;
        unsigned before=mock("mock_rf_job_add_calls");
        CHECK(ams_mel_rf_job_add_intervals_v11(job,span,D)==AMS_MEL_INVALID_ARGUMENT);
        CHECK(mock("mock_rf_job_add_calls")==before && value(0,0,6,0)==-1);
        inputs[0].has_modulations=0;
        CHECK(ams_mel_rf_job_add_intervals_v11(job,span,D)==AMS_MEL_INVALID_ARGUMENT);
        CHECK(mock("mock_rf_job_add_calls")==before);
        first[0]=-1;
        CHECK(ams_mel_rf_job_add_intervals_v11(job,span,D)==AMS_MEL_OK);
        ams_mel_rf_job_interval_config_v10 frozen={0};
        ams_mel_rf_tx_event_config_v2 old={0};
        frozen.has_transmit_events=1;
        frozen.transmit_events=(ams_mel_rf_tx_event_config_span_v2){&old,1};
        CHECK(ams_mel_rf_job_add_intervals_v10(job,(ams_mel_rf_job_interval_config_span_v10){&frozen,1},D)==AMS_MEL_OK);
        CHECK(value(0,0,0,0)==0 && value(0,0,5,0)==1 && value(0,0,6,0)==-1);
        ams_mel_rf_job_interval_config_v9 older={0};
        ams_mel_rf_tx_event_config_v1 oldest={0};
        older.has_transmit_events=1;
        older.transmit_events=(ams_mel_rf_tx_event_config_span_v1){&oldest,1};
        CHECK(ams_mel_rf_job_add_intervals_v9(job,(ams_mel_rf_job_interval_config_span_v9){&older,1},D)==AMS_MEL_OK);
        CHECK(value(0,0,0,0)==0 && value(0,0,6,0)==-1);
        CHECK(ams_mel_rf_job_add_rx_intervals_v8(job,(ams_mel_rf_job_interval_config_span_v8){&inputs[0].interval,1},D)==AMS_MEL_OK);
        CHECK(value(0,0,0,0)==0 && tx_value(0,0,0,0)==0 && tx_value(0,0,14,0)==80);
        CHECK(ams_mel_rf_job_add_rx_intervals_v7(job,(ams_mel_rf_job_interval_config_span_v7){&inputs[0].interval.interval,1},D)==AMS_MEL_OK);
        CHECK(value(0,0,0,0)==0 && tx_value(0,0,0,0)==0 && tx_value(0,0,14,0)==80);
        CHECK(ams_mel_rf_job_flush(job,D)==AMS_MEL_OK);
        no_queries();
        CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,D)==AMS_MEL_OK);
        if(repeat%2) {
            CHECK(ams_mel_rf_job_finalize(job,D)==AMS_MEL_OK);
            ams_mel_rf_job_status_t status=0;
            CHECK(ams_mel_rf_job_wait_status(job,3000,&status,D)==AMS_MEL_OK);
        } else {
            ams_mel_rf_job_cancel_result_v1 answer={0};
            CHECK(ams_mel_rf_job_cancel(job,&answer,D)==AMS_MEL_OK);
        }
        CHECK(ams_mel_rf_job_add_intervals_v11(job,span,D)==AMS_MEL_PROVIDER_FAILED);
        CHECK(ams_mel_rf_job_close(&job,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,D)==AMS_MEL_OK);
    }
    puts("PASS: value modulations focused 50/50"); return 0;
}
