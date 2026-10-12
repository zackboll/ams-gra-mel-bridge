#define _POSIX_C_SOURCE 200809L
#include <math.h>
#define main inherited_regression
#include "test_rf_job_product_stream_params.c"
#undef main

static int64_t observed(unsigned i, unsigned m, unsigned p, unsigned s, unsigned f)
{
    void *lib = dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER, RTLD_NOW | RTLD_LOCAL);
    int64_t (*fn)(unsigned, unsigned, unsigned, unsigned, unsigned); CHECK(lib);
    *(void **)(&fn) = dlsym(lib, "mock_rf_f14_value"); CHECK(fn);
    int64_t result = fn(i,m,p,s,f); CHECK(dlclose(lib)==0); return result;
}
static double scalar(unsigned i, unsigned m, unsigned p, unsigned f)
{
    void *lib = dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER, RTLD_NOW | RTLD_LOCAL);
    double (*fn)(unsigned, unsigned, unsigned, unsigned); CHECK(lib);
    *(void **)(&fn) = dlsym(lib, "mock_rf_f14_double"); CHECK(fn);
    double result = fn(i,m,p,f); CHECK(dlclose(lib)==0); return result;
}
static int64_t reference(unsigned i, unsigned e, unsigned f, unsigned n)
{
    void *lib = dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER, RTLD_NOW | RTLD_LOCAL);
    int64_t (*fn)(unsigned, unsigned, unsigned, unsigned); CHECK(lib);
    *(void **)(&fn) = dlsym(lib, "mock_rf_f13_value"); CHECK(fn);
    int64_t result = fn(i,e,f,n); CHECK(dlclose(lib)==0); return result;
}
static void reject_custom(ams_mel_rf_job *job, ams_mel_rf_job_interval_config_span_v13 span)
{
    unsigned before = mock("mock_rf_job_add_calls");
    CHECK(ams_mel_rf_job_add_intervals_v13(job,span,D)==AMS_MEL_INVALID_ARGUMENT);
    CHECK(mock("mock_rf_job_add_calls")==before);
    CHECK(observed(0,0,0,0,2)==3 && scalar(0,0,0,0)==90);
}
int main(void)
{
    capabilities[0].uuid[1]=0x80; capabilities[0].uuid[2]=0xff;
    capabilities[0].descriptive_label=(ams_mel_string_view_v1){"first",5};
    capabilities[1].uuid[0]=0xff; capabilities[2].uuid[15]=0x80;
    capabilities[2].descriptive_label=(ams_mel_string_view_v1){"µ-third",8};
    for(unsigned repeat=0;repeat<50;++repeat) {
        ams_mel_rf_c2 *c2=NULL; ams_mel_rf_virtual_aperture *va=NULL;
        ams_mel_rf_job *job=claimed("c2:job-ok",&c2,&va);
        ams_mel_rf_waveform_profile_params_v1 profiles[3] = {
            {INT64_MIN,1,0,1,0,1,0,90,-180,-0.0,INFINITY,-INFINITY,NAN},
            {INT64_MAX,0,1,0,1,0,1,91,-179,17,19,23,2},
            {-37,0,0,0,0,0,0,92,-178,29,31,37,-3}
        };
        ams_mel_rf_waveform_profile_iterations_v1 steps[]={{INT32_MIN,2},{0,0},{INT32_MAX,2},{-7,1}};
        ams_mel_rf_event_waveform_profile_pattern_v1 patterns[]={{{steps,3}},{{NULL,0}},{{steps+3,1}}};
        ams_mel_rf_modulation_config_v3 tables[2][2]={0};
        ams_mel_rf_job_interval_config_v13 intervals[2]={0};
        ams_mel_rf_tx_event_config_v3 events[2][2]={0};
        int32_t refs[]={0,-1,1,0};
        for(unsigned i=0;i<2;++i) {
            intervals[i].has_modulations=intervals[i].has_transmit_events=1;
            intervals[i].modulations=(ams_mel_rf_modulation_config_span_v3){tables[i],2};
            intervals[i].transmit_events=(ams_mel_rf_tx_event_config_span_v3){events[i],2};
            for(unsigned m=0;m<2;++m) {
                tables[i][m].modulation.modulation.kind=5;
                tables[i][m].modulation.modulation.phase_chips_rad=(ams_mel_double_span_v1){(void *)1,SIZE_MAX};
                tables[i][m].modulation.binary_data=(ams_mel_u32_span_v1){(void *)1,SIZE_MAX};
                tables[i][m].custom_waveform.profiles=(ams_mel_rf_waveform_profile_params_span_v1){profiles,3-i-m};
                tables[i][m].custom_waveform.patterns=(ams_mel_rf_event_waveform_profile_pattern_span_v1){patterns,i+m ? 2 : 3};
            }
            for(unsigned e=0;e<2;++e) {
                events[i][e].has_modulation_indices=1;
                events[i][e].modulation_indices=(ams_mel_i32_span_v1){refs,4};
            }
        }
        /* Definitions with shorter profile tables use their own valid steps. */
        ams_mel_rf_waveform_profile_iterations_v1 local[]={{-3,0},{0,0}};
        ams_mel_rf_event_waveform_profile_pattern_v1 local_patterns[]={{{local,2}},{{NULL,0}}};
        tables[0][1].custom_waveform.patterns.data=local_patterns;
        tables[1][0].custom_waveform.patterns.data=local_patterns;
        tables[1][1].custom_waveform.patterns.data=local_patterns;
        ams_mel_rf_job_interval_config_span_v13 span={intervals,2};
        CHECK(ams_mel_rf_job_add_intervals_v13(job,span,D)==AMS_MEL_OK);
        for(unsigned i=0;i<2;++i) for(unsigned e=0;e<2;++e) {
            CHECK(reference(i,e,5,0)==4);
            for(unsigned n=0;n<4;++n) CHECK(reference(i,e,6,n)==refs[n]);
        }
        for(unsigned i=0;i<2;++i) for(unsigned m=0;m<2;++m) {
            CHECK(observed(i,m,0,0,0)==1 && observed(i,m,0,0,1)==1);
            CHECK(observed(i,m,0,0,2)==3-i-m);
            CHECK(observed(i,m,0,0,3)==(i+m ? 2 : 3));
            CHECK(observed(i,m,1,0,4)==0);
            for(unsigned p=0;p<3-i-m;++p) {
                const ams_mel_rf_waveform_profile_params_v1 *v=&profiles[p];
                uint32_t flags[]={v->update_phase,v->update_delta_phase,v->update_frequency_offset,v->update_delta_frequency,v->update_frequency_slope,v->update_amplitude_factor};
                double numbers[]={v->phase_deg,v->delta_phase_deg,v->frequency_offset_hz,v->delta_frequency_hz,v->frequency_slope_hz_sec,v->amplitude_factor};
                CHECK(observed(i,m,p,0,7)==v->chip_duration_femtoseconds);
                for(unsigned f=0;f<6;++f) {
                    CHECK(observed(i,m,p,0,8+f)==flags[f]);
                    double actual=scalar(i,m,p,f);
                    if(isnan(numbers[f])) CHECK(isnan(actual));
                    else { CHECK(actual==numbers[f]); CHECK(!!signbit(actual)==!!signbit(numbers[f])); }
                }
            }
            const ams_mel_rf_custom_waveform_config_v1 *w=&tables[i][m].custom_waveform;
            for(unsigned p=0;p<w->patterns.size;++p) {
                CHECK(observed(i,m,p,0,4)==(int64_t)w->patterns.data[p].steps.size);
                for(unsigned s=0;s<w->patterns.data[p].steps.size;++s) {
                    CHECK(observed(i,m,p,s,5)==w->patterns.data[p].steps.data[s].num_chip_iterations);
                    CHECK(observed(i,m,p,s,6)==w->patterns.data[p].steps.data[s].waveform_profile_index);
                }
            }
        }
        profiles[0].phase_deg=7; steps[0].num_chip_iterations=3;
        CHECK(scalar(0,0,0,0)==90 && observed(0,0,0,0,5)==INT32_MIN);
        profiles[0].phase_deg=90; steps[0].num_chip_iterations=INT32_MIN;
        uint32_t *flags[]={&profiles[2].update_phase,&profiles[2].update_delta_phase,&profiles[2].update_frequency_offset,&profiles[2].update_delta_frequency,&profiles[2].update_frequency_slope,&profiles[2].update_amplitude_factor};
        for(unsigned f=0;f<6;++f) {
            *flags[f]=2; reject_custom(job,span); *flags[f]=UINT32_MAX; reject_custom(job,span); *flags[f]=0;
        }
        int32_t invalid[]={-1,INT32_MIN,3,INT32_MAX};
        for(unsigned n=0;n<4;++n) for(int iteration=-1;iteration<=0;++iteration) {
            steps[2].waveform_profile_index=invalid[n]; steps[2].num_chip_iterations=iteration;
            reject_custom(job,span);
        }
        steps[2]=(ams_mel_rf_waveform_profile_iterations_v1){INT32_MAX,2};
        tables[0][0].custom_waveform.profiles.size=2; reject_custom(job,span);
        tables[0][0].custom_waveform.profiles.size=3;
        patterns[2].steps=(ams_mel_rf_waveform_profile_iterations_span_v1){NULL,1}; reject_custom(job,span);
        patterns[2].steps=(ams_mel_rf_waveform_profile_iterations_span_v1){(void *)1,SIZE_MAX}; reject_custom(job,span);
        patterns[2].steps=(ams_mel_rf_waveform_profile_iterations_span_v1){steps+3,1};
        tables[1][1].custom_waveform.profiles.data=NULL; reject_custom(job,span);
        tables[1][1].custom_waveform.profiles.data=profiles;
        tables[1][1].custom_waveform.profiles.size=SIZE_MAX;
        tables[1][1].custom_waveform.profiles.data=(void *)1; reject_custom(job,span);
        tables[1][1].custom_waveform.profiles=(ams_mel_rf_waveform_profile_params_span_v1){profiles,1};
        tables[1][1].custom_waveform.patterns.data=NULL; reject_custom(job,span);
        tables[1][1].custom_waveform.patterns=(ams_mel_rf_event_waveform_profile_pattern_span_v1){(void *)1,SIZE_MAX};
        reject_custom(job,span);
        tables[1][1].custom_waveform.patterns=(ams_mel_rf_event_waveform_profile_pattern_span_v1){local_patterns,2};
        /* Index 2 exists in another definition but not this final table. */
        local[1].waveform_profile_index=2; reject_custom(job,span);
        local[1].waveform_profile_index=0;
        intervals[1].has_transmit_events=0;
        profiles[2].update_phase=2; reject_custom(job,span); profiles[2].update_phase=0;
        intervals[1].has_transmit_events=1;
        tables[1][1].modulation.modulation.kind=6; reject_custom(job,span);
        tables[1][1].modulation.modulation.kind=5;
        unsigned before=mock("mock_rf_job_add_calls");
        CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE","later-custom-step",1)==0);
        CHECK(ams_mel_rf_job_add_intervals_v13(job,span,D)==AMS_MEL_INTERNAL_ERROR);
        CHECK(mock("mock_rf_job_add_calls")==before && scalar(0,0,0,0)==90);
        CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE")==0);
        CHECK(ams_mel_rf_job_add_intervals_v13(job,span,D)==AMS_MEL_OK);
        /* The provider exception path still maps through the shared pipeline;
         * later valid submission recovers on the same Job. */
        CHECK(setenv("AMS_MEL_TEST_F4_ADD_FAILURE","std",1)==0);
        CHECK(ams_mel_rf_job_add_intervals_v13(job,span,D)==AMS_MEL_PROVIDER_EXCEPTION);
        CHECK(unsetenv("AMS_MEL_TEST_F4_ADD_FAILURE")==0);
        CHECK(ams_mel_rf_job_add_intervals_v13(job,span,D)==AMS_MEL_OK);
        ams_mel_rf_job_interval_config_v12 old={0};
        old.has_modulations=1;
        old.modulations=(ams_mel_rf_modulation_config_span_v2){&tables[0][0].modulation,1};
        CHECK(ams_mel_rf_job_add_intervals_v12(job,(ams_mel_rf_job_interval_config_span_v12){&old,1},D)==AMS_MEL_INVALID_ARGUMENT);
        /* Exercise every inherited alternative through the new element stride,
         * with poisoned inactive custom payloads, then observe the actual kind. */
        uint32_t bits[]={1}; double phases[]={0.25};
        for(unsigned kind=0;kind<5;++kind) {
            ams_mel_rf_modulation_config_v3 legacy={0};
            legacy.modulation.modulation.kind=kind;
            legacy.modulation.modulation.phase_chips_rad=(ams_mel_double_span_v1){phases,1};
            legacy.modulation.binary_data=(ams_mel_u32_span_v1){bits,1};
            legacy.modulation.pulse_duration_femtoseconds=-37;
            legacy.modulation.frequency_offset_hz=-0.0;
            legacy.custom_waveform.profiles=(ams_mel_rf_waveform_profile_params_span_v1){(void *)1,SIZE_MAX};
            legacy.custom_waveform.patterns=(ams_mel_rf_event_waveform_profile_pattern_span_v1){(void *)1,SIZE_MAX};
            ams_mel_rf_job_interval_config_v13 one={0};
            one.interval.interval.interval.interval.sequence_repeat_count=1;
            one.has_modulations=1;
            one.modulations=(ams_mel_rf_modulation_config_span_v3){&legacy,1};
            CHECK(ams_mel_rf_job_add_intervals_v13(job,(ams_mel_rf_job_interval_config_span_v13){&one,1},D)==AMS_MEL_OK);
            CHECK(reference(0,0,2,0)==kind);
            if(kind==4) CHECK(reference(0,0,7,0)==-37);
        }
        ams_mel_rf_modulation_config_v3 empty={0}; empty.modulation.modulation.kind=5;
        ams_mel_rf_event_waveform_profile_pattern_v1 empty_patterns[3]={0};
        empty.custom_waveform.patterns=(ams_mel_rf_event_waveform_profile_pattern_span_v1){empty_patterns,3};
        ams_mel_rf_job_interval_config_v13 one={0};
        one.has_modulations=1; one.modulations=(ams_mel_rf_modulation_config_span_v3){&empty,1};
        CHECK(ams_mel_rf_job_add_intervals_v13(job,(ams_mel_rf_job_interval_config_span_v13){&one,1},D)==AMS_MEL_OK);
        CHECK(observed(0,0,0,0,2)==0 && observed(0,0,0,0,3)==3);
        for(unsigned p=0;p<3;++p) CHECK(observed(0,0,p,0,4)==0);
        one.has_modulations=0; one.modulations=(ams_mel_rf_modulation_config_span_v3){(void *)1,SIZE_MAX};
        one.transmit_events=(ams_mel_rf_tx_event_config_span_v3){(void *)1,SIZE_MAX};
        CHECK(ams_mel_rf_job_add_intervals_v13(job,(ams_mel_rf_job_interval_config_span_v13){&one,1},D)==AMS_MEL_OK);
        CHECK(reference(0,0,0,0)==0);
        one.has_modulations=1; one.modulations=(ams_mel_rf_modulation_config_span_v3){NULL,0};
        CHECK(ams_mel_rf_job_add_intervals_v13(job,(ams_mel_rf_job_interval_config_span_v13){&one,1},D)==AMS_MEL_OK);
        CHECK(reference(0,0,0,0)==0);
        CHECK(ams_mel_rf_virtual_aperture_close(&va,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_add_intervals_v13(job,(ams_mel_rf_job_interval_config_span_v13){&one,1},D)==AMS_MEL_OK);
        ams_mel_rf_job_cancel_result_v1 cancelled={0};
        CHECK(ams_mel_rf_job_cancel(job,&cancelled,D)==AMS_MEL_OK);
        CHECK(ams_mel_rf_job_add_intervals_v13(job,(ams_mel_rf_job_interval_config_span_v13){&one,1},D)==AMS_MEL_PROVIDER_FAILED);
        CHECK(ams_mel_rf_job_close(&job,D)==AMS_MEL_OK);
    }
    puts("PASS: native CustomWaveform focused 50/50"); return 0;
}
