#define _POSIX_C_SOURCE 200809L
#include <math.h>
#define main inherited_regression
#define value inherited_value
#include "test_rf_job_product_stream_params.c"
#undef value
#undef main

static int64_t binary_value(unsigned i,unsigned m,unsigned f,unsigned n)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    int64_t (*fn)(unsigned,unsigned,unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f13_value"); CHECK(fn);
    int64_t result=fn(i,m,f,n); CHECK(dlclose(lib)==0); return result;
}
static double binary_number(unsigned i,unsigned m)
{
    void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL);
    double (*fn)(unsigned,unsigned); CHECK(lib);
    *(void **)(&fn)=dlsym(lib,"mock_rf_f13_double"); CHECK(fn);
    double result=fn(i,m); CHECK(dlclose(lib)==0); return result;
}
static void reject_binary(ams_mel_rf_job *job,ams_mel_rf_job_interval_config_span_v12 span)
{
    unsigned before=mock("mock_rf_job_add_calls");
    CHECK(ams_mel_rf_job_add_intervals_v12(job,span,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(mock("mock_rf_job_add_calls")==before);
}
int main(void)
{
    /* The inherited suite already repeats internally; do not wrap it. */
    CHECK(inherited_regression()==0);
    for(unsigned repeat=0;repeat<50;++repeat) {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job *job=claimed("c2:job-ok",&c2,&va);
        uint32_t a[]={1,0,0,1,0}; uint32_t b[]={0,1,1};
        uint32_t chips[]={0,1,0,1,1,0,0};
        ams_mel_rf_modulation_config_v2 tables[2][4]={0};
        ams_mel_rf_tx_event_config_v3 events[2][2]={0};
        int32_t refs0[]={0,-1,1,0,3,1}; int32_t refs1[]={1,2,-1,1};
        int32_t refs2[]={2,1,-1,2}; int32_t refs3[]={1,0,1};
        ams_mel_rf_job_interval_config_v12 inputs[2]={0};
        ams_mel_rf_receive_event_config_v4 rx[2]={0};
        ams_mel_rf_stokes_vector_v1 stokes[2]={{1,-2,3,-4},{5,-6,7,-8}};
        tables[0][0].modulation.kind=3;
        tables[0][0].binary_data=(ams_mel_u32_span_v1){a,5};
        tables[0][1].modulation.kind=4;
        tables[0][1].binary_data=(ams_mel_u32_span_v1){chips,7};
        tables[0][1].modulation.chip_duration_femtoseconds=INT64_MAX;
        tables[0][1].pulse_duration_femtoseconds=INT64_MIN;
        tables[0][1].frequency_offset_hz=-0.0;
        tables[0][2]=tables[0][0]; /* Aliased sources, independent OOK entries. */
        tables[0][3].modulation.kind=0;
        tables[1][0].modulation.kind=2;
        tables[1][0].modulation.frequency_extent_hz=31.5;
        tables[1][1]=tables[0][1];
        tables[1][1].binary_data=(ams_mel_u32_span_v1){a,5};
        tables[1][1].modulation.chip_duration_femtoseconds=-59;
        tables[1][1].pulse_duration_femtoseconds=37;
        tables[1][1].frequency_offset_hz=-INFINITY;
        tables[1][2].modulation.kind=3;
        tables[1][2].binary_data=(ams_mel_u32_span_v1){b,3};
        for(unsigned i=0;i<2;++i) {
            inputs[i].interval.interval.interval.interval.sequence_repeat_count=5-2*i;
            inputs[i].interval.interval.interval.interval.interval_id=40+i;
            rx[i].event.event.event.event_id=80+i;
            rx[i].event.polarization=(ams_mel_rf_stokes_vector_span_v1){&stokes[1],1};
            inputs[i].interval.interval.interval.interval.receive_events=(ams_mel_rf_receive_event_config_span_v4){&rx[i],1};
            inputs[i].has_modulations=inputs[i].has_transmit_events=1;
            inputs[i].modulations=(ams_mel_rf_modulation_config_span_v2){tables[i],4-i};
            inputs[i].transmit_events=(ams_mel_rf_tx_event_config_span_v3){events[i],2};
            for(unsigned e=0;e<2;++e) {
                events[i][e].event.event.event_id=7; /* Position, not ID, identifies owners. */
                events[i][e].event.polarization=(ams_mel_rf_stokes_vector_span_v1){stokes,1};
                events[i][e].has_modulation_indices=1;
            }
        }
        events[0][0].modulation_indices=(ams_mel_i32_span_v1){refs0,6};
        events[0][1].modulation_indices=(ams_mel_i32_span_v1){refs1,4};
        events[1][0].modulation_indices=(ams_mel_i32_span_v1){refs2,4};
        events[1][1].modulation_indices=(ams_mel_i32_span_v1){refs3,3};
        /* Poison inactive spans for every selected binary value. */
        for(unsigned i=0;i<2;++i) for(unsigned m=0;m<4-i;++m)
            tables[i][m].modulation.phase_chips_rad=(ams_mel_double_span_v1){(void *)1,SIZE_MAX};
        tables[0][3].binary_data=(ams_mel_u32_span_v1){(void *)1,SIZE_MAX};
        tables[1][0].binary_data=(ams_mel_u32_span_v1){(void *)1,SIZE_MAX};
        ams_mel_rf_job_interval_config_span_v12 span={inputs,2};
        CHECK(ams_mel_rf_job_add_intervals_v12(job,span,D)==AMS_MEL_OK);
        {
            void *lib=dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER,RTLD_NOW|RTLD_LOCAL); CHECK(lib);
            double (*tx)(unsigned,unsigned,unsigned,unsigned);
            double (*receive)(unsigned,unsigned,unsigned,unsigned);
            uint64_t (*scalar)(unsigned,unsigned,unsigned,unsigned);
            *(void **)(&tx)=dlsym(lib,"mock_rf_f11_double"); CHECK(tx);
            *(void **)(&receive)=dlsym(lib,"mock_rf_f5_double"); CHECK(receive);
            *(void **)(&scalar)=dlsym(lib,"mock_rf_f10_value"); CHECK(scalar);
            for(unsigned i=0;i<2;++i) {
                CHECK(scalar(i,0,1,0)==1 && scalar(i,0,14,0)==80+i && scalar(i,0,15,0)==40+i);
                for(unsigned n=0;n<4;++n) {
                    const double expected_tx[]={1,-2,3,-4},expected_rx[]={5,-6,7,-8};
                    CHECK(tx(i,0,0,n)==expected_tx[n] && tx(i,1,0,n)==expected_tx[n]);
                    CHECK(receive(i,0,0,n)==expected_rx[n]);
                }
            }
            CHECK(dlclose(lib)==0);
        }
        for(unsigned i=0;i<2;++i) for(unsigned m=0;m<4-i;++m) {
            CHECK(binary_value(i,m,1,0)==1);
            CHECK(binary_value(i,m,2,0)==tables[i][m].modulation.kind);
            if(tables[i][m].modulation.kind>=3) {
                CHECK(binary_value(i,m,4,0)==(int64_t)tables[i][m].binary_data.size);
                for(unsigned n=0;n<tables[i][m].binary_data.size;++n)
                    CHECK(binary_value(i,m,8,n)==tables[i][m].binary_data.data[n]);
            }
        }
        CHECK(binary_value(0,1,3,0)==INT64_MAX && binary_value(0,1,7,0)==INT64_MIN);
        CHECK(binary_value(1,1,3,0)==-59 && binary_value(1,1,7,0)==37);
        CHECK(signbit(binary_number(0,1)) && binary_number(0,1)==0);
        CHECK(isinf(binary_number(1,1)) && signbit(binary_number(1,1)));
        for(unsigned i=0;i<2;++i) for(unsigned e=0;e<2;++e)
            for(unsigned n=0;n<events[i][e].modulation_indices.size;++n)
                CHECK(binary_value(i,e,6,n)==events[i][e].modulation_indices.data[n]);
        a[0]=0; chips[1]=0;
        CHECK(binary_value(0,0,8,0)==1 && binary_value(0,2,8,0)==1 && binary_value(0,1,8,1)==1);
        a[0]=1; chips[1]=1;
        refs1[1]=0; reject_binary(job,span); refs1[1]=2;
        inputs[0].interval.interval.interval.interval.sequence_repeat_count=4; reject_binary(job,span);
        inputs[0].interval.interval.interval.interval.sequence_repeat_count=UINT64_MAX; reject_binary(job,span);
        inputs[0].interval.interval.interval.interval.sequence_repeat_count=5;
        b[1]=2; reject_binary(job,span); b[1]=UINT32_MAX; reject_binary(job,span); b[1]=1;
        tables[1][2].binary_data=(ams_mel_u32_span_v1){NULL,3}; reject_binary(job,span);
        tables[1][2].binary_data=(ams_mel_u32_span_v1){(void *)1,SIZE_MAX}; reject_binary(job,span);
        tables[1][2].binary_data=(ams_mel_u32_span_v1){b,3};
        tables[1][2].modulation.kind=5; reject_binary(job,span); tables[1][2].modulation.kind=3;
        inputs[1].has_modulations=2; reject_binary(job,span); inputs[1].has_modulations=1;
        inputs[1].has_transmit_events=2; reject_binary(job,span); inputs[1].has_transmit_events=1;
        events[1][1].has_modulation_indices=2; reject_binary(job,span); events[1][1].has_modulation_indices=1;
        reject_binary(job,(ams_mel_rf_job_interval_config_span_v12){NULL,1});
        reject_binary(job,(ams_mel_rf_job_interval_config_span_v12){(void *)1,SIZE_MAX});
        unsigned before=mock("mock_rf_job_add_calls");
        CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE","later-binary-vector",1)==0);
        CHECK(ams_mel_rf_job_add_intervals_v12(job,span,D)==AMS_MEL_INTERNAL_ERROR);
        CHECK(mock("mock_rf_job_add_calls")==before);
        CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE")==0);
        CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE","later-binary-definition",1)==0);
        CHECK(ams_mel_rf_job_add_intervals_v12(job,span,D)==AMS_MEL_INTERNAL_ERROR);
        CHECK(mock("mock_rf_job_add_calls")==before);
        CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE")==0);
        CHECK(ams_mel_rf_job_add_intervals_v12(job,span,D)==AMS_MEL_OK);
        {
            const double frequencies[]={0.0,-0.0,13.25,INFINITY,-INFINITY,NAN};
            for(unsigned f=0;f<sizeof frequencies/sizeof frequencies[0];++f) {
                tables[0][1].frequency_offset_hz=frequencies[f];
                CHECK(ams_mel_rf_job_add_intervals_v12(job,span,D)==AMS_MEL_OK);
                double actual=binary_number(0,1);
                if(isnan(frequencies[f])) CHECK(isnan(actual));
                else { CHECK(actual==frequencies[f]); CHECK(!!signbit(actual)==!!signbit(frequencies[f])); }
            }
            tables[0][1].frequency_offset_hz=-0.0;
        }
        /* TX presence zero ignores poisoned references and control flags. */
        inputs[0].has_transmit_events=0;
        events[0][0].has_modulation_indices=UINT32_MAX;
        CHECK(ams_mel_rf_job_add_intervals_v12(job,span,D)==AMS_MEL_OK);
        events[0][0].has_modulation_indices=1;
        inputs[0].has_transmit_events=1;
        for(unsigned length=63;length<=65;++length) {
            uint32_t boundary[65]; for(unsigned n=0;n<length;++n) boundary[n]=n%2;
            inputs[0].has_transmit_events=0;
            inputs[0].modulations.size=1;
            inputs[0].interval.interval.interval.interval.sequence_repeat_count=length;
            tables[0][0].binary_data=(ams_mel_u32_span_v1){boundary,length};
            CHECK(ams_mel_rf_job_add_intervals_v12(job,(ams_mel_rf_job_interval_config_span_v12){inputs,1},D)==AMS_MEL_OK);
            for(unsigned n=0;n<length;++n) CHECK(binary_value(0,0,8,n)==n%2);
        }
        inputs[0].interval.interval.interval.interval.sequence_repeat_count=0;
        tables[0][0].binary_data=(ams_mel_u32_span_v1){NULL,0};
        CHECK(ams_mel_rf_job_add_intervals_v12(job,(ams_mel_rf_job_interval_config_span_v12){inputs,1},D)==AMS_MEL_OK);
        for(unsigned length=0;length<=1;++length) for(unsigned bit=0;bit<=1;++bit) {
            uint32_t one=bit;
            tables[0][0].modulation.kind=4;
            tables[0][0].binary_data=(ams_mel_u32_span_v1){&one,length};
            tables[0][0].pulse_duration_femtoseconds=INT64_MAX;
            tables[0][0].modulation.chip_duration_femtoseconds=INT64_MIN;
            CHECK(ams_mel_rf_job_add_intervals_v12(job,(ams_mel_rf_job_interval_config_span_v12){inputs,1},D)==AMS_MEL_OK);
            CHECK(binary_value(0,0,4,0)==length);
            if(length) CHECK(binary_value(0,0,8,0)==bit);
            CHECK(binary_value(0,0,7,0)==INT64_MAX && binary_value(0,0,3,0)==INT64_MIN);
        }
        ams_mel_rf_modulation_config_v1 old={0}; old.kind=3;
        ams_mel_rf_job_interval_config_v11 v11={0};
        v11.has_modulations=1; v11.modulations=(ams_mel_rf_modulation_config_span_v1){&old,1};
        CHECK(ams_mel_rf_job_add_intervals_v11(job,(ams_mel_rf_job_interval_config_span_v11){&v11,1},D)==AMS_MEL_INVALID_ARGUMENT);
        old.kind=4;
        CHECK(ams_mel_rf_job_add_intervals_v11(job,(ams_mel_rf_job_interval_config_span_v11){&v11,1},D)==AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_rf_c2_close(&c2,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_flush(job,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_close(&job,D)==AMS_MEL_OK);
    }
    puts("PASS: native binary modulation focused 50/50"); return 0;
}
