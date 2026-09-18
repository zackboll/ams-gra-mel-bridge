#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef AMS_MEL_TEST_MOCK_PROVIDER
#error "mock provider path is required"
#endif

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return EXIT_FAILURE; } } while (0)

static ams_mel_string_view_v1 text(const char *value)
{ ams_mel_string_view_v1 result={value,strlen(value)};return result; }
static int text_is(ams_mel_string_view_v1 value,const char *expected)
{ return value.size==strlen(expected)&&(value.size==0U||memcmp(value.data,expected,value.size)==0); }

static ams_mel_ir_health_config_v1 configuration(void)
{
    ams_mel_ir_health_config_v1 value;memset(&value,0,sizeof value);
    value.channel_id.descriptive_label=text("IR Health channel");
    value.channel_type=AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS;
    value.platform_id.descriptive_label=text("test platform");
    value.sensor_location.offset_x_m=1.25;value.sensor_location.offset_y_m=-2.5;
    value.sensor_location.offset_z_m=3.75;value.sensor_location.key=text("station-1");
    value.sensor_location.system_name=text("mock-aircraft");return value;
}

static int open_health(const char *scenario,ams_mel_session **session,ams_mel_ir_health **health)
{
    ams_mel_ir_health_config_v1 config=configuration();
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER,scenario,"",session,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_health_open(*session,&config,health,NULL,0,NULL)==AMS_MEL_OK);return EXIT_SUCCESS;
}

static int receive(ams_mel_ir_health_metadata *metadata,ams_mel_ir_health_metadata_event **event,const ams_mel_ir_health_metadata_event_v1 **view)
{
    CHECK(ams_mel_ir_health_metadata_receive(metadata,1000,event,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_health_metadata_event_view(*event,view,NULL,0,NULL)==AMS_MEL_OK);return EXIT_SUCCESS;
}

static int test_rich(void)
{
    ams_mel_session *session=NULL;ams_mel_ir_health *health=NULL;
    ams_mel_ir_health_metadata *metadata=NULL;ams_mel_ir_health_metadata_event *event=NULL;
    const ams_mel_ir_health_metadata_event_v1 *view=NULL;
    CHECK(open_health("health-rich",&session,&health)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_health_metadata_open(health,32,&metadata,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_health_enable(health,NULL,0,NULL)==AMS_MEL_OK);

    CHECK(receive(metadata,&event,&view)==EXIT_SUCCESS);
    CHECK(view->kind==AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS);
    CHECK(view->mfa_status.state==AMS_MEL_IR_MFA_STATE_MAINTENANCE);
    CHECK(text_is(view->mfa_status.state_description,"healthy-\xCE\xB1"));
    CHECK(text_is(view->mfa_status.mode_description,"mode-\xE2\x82\xAC"));
    CHECK(view->mfa_status.transition_status==AMS_MEL_STATE_TRANSITION_TRANSITIONING);
    CHECK(text_is(view->mfa_status.about.model,"model"));
    CHECK(text_is(view->mfa_status.about.serial_number,"serial"));
    CHECK(text_is(view->mfa_status.about.software_version,"software"));
    CHECK(text_is(view->mfa_status.about.bootloader_software_version,"boot"));
    CHECK(text_is(view->mfa_status.about.hardware_version,"hardware"));
    CHECK(view->mfa_status.components.size==2U);
    CHECK(view->mfa_status.components.data[0].state==AMS_MEL_COMPONENT_STATE_OPERATIONAL);
    CHECK(view->mfa_status.components.data[0].temperature.temperature_c==42.25);
    CHECK(view->mfa_status.components.data[0].temperature.state==AMS_MEL_TEMPERATURE_STATE_NORMAL);
    CHECK(text_is(view->mfa_status.components.data[0].installation_location_id.key,"rack-\xCE\xB2"));
    CHECK(view->mfa_status.components.data[0].installation_details.location.offset_y_m==-2.5);
    CHECK(view->mfa_status.components.data[0].installation_details.orientation.roll==0.1);
    CHECK(view->mfa_status.components.data[0].installation_details.boresight.yaw==-0.6);
    CHECK(ams_mel_ir_health_metadata_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);

    CHECK(receive(metadata,&event,&view)==EXIT_SUCCESS);
    CHECK(view->kind==AMS_MEL_IR_HEALTH_METADATA_BIT_STATUS);
    CHECK(view->bit_status.active_bits.size==3U&&view->bit_status.completed_bits.size==2U&&view->bit_status.faults.size==1U);
    CHECK(view->bit_status.active_bits.data[0].estimated_completion_time_ns==-5);
    CHECK(text_is(view->bit_status.completed_bits.data[0].bit_items.data[1].bit_item_name,"item-\xCE\xB3"));
    CHECK(view->bit_status.faults.data[0].ambiguity_groups.size==2U);
    CHECK(ams_mel_ir_health_metadata_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);

    CHECK(receive(metadata,&event,&view)==EXIT_SUCCESS);
    CHECK(view->kind==AMS_MEL_IR_HEALTH_METADATA_SUBSYSTEM_STATUS);
    CHECK(view->subsystem_status.subsystem_id==UINT32_C(0x80000001));
    CHECK(view->subsystem_status.status_sequence_number==UINT32_C(0xf0000002));
    CHECK(view->subsystem_status.failure==AMS_MEL_IR_FAILURE_MAJOR);
    CHECK(view->subsystem_status.subsystem_count==99U&&view->subsystem_status.subsystems.size==2U);
    CHECK(view->subsystem_status.csci_count==88U&&view->subsystem_status.csci.size==2U);
    CHECK(text_is(view->subsystem_status.csci.data[0].csci,"flight-\xCE\xB4"));
    CHECK(view->subsystem_status.csci.data[0].mode==AMS_MEL_IR_CSCI_MODE_OPERATIONAL);
    CHECK(view->subsystem_status.csci.data[0].version.engineering_revision==4U);
    CHECK(view->subsystem_status.csci.data[0].connection_established==1U);
    CHECK(view->subsystem_status.csci.data[1].connection_established==0U);
    CHECK(ams_mel_ir_health_metadata_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);

    CHECK(receive(metadata,&event,&view)==EXIT_SUCCESS);
    CHECK(view->kind==AMS_MEL_IR_HEALTH_METADATA_DISCRETE_STATUS);
    CHECK(view->discrete_status.size==3U);
    CHECK(text_is(view->discrete_status.data[0].name,"duplicate"));
    CHECK(text_is(view->discrete_status.data[1].name,"duplicate"));
    CHECK(text_is(view->discrete_status.data[1].value,""));
    CHECK(text_is(view->discrete_status.data[2].value,"\xE2\x82\xAC"));
    CHECK(ams_mel_ir_health_metadata_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);

    for(uint32_t kind=AMS_MEL_SECURITY_EVENT_NONE;kind<=AMS_MEL_SECURITY_EVENT_SANITIZATION;++kind){
        CHECK(receive(metadata,&event,&view)==EXIT_SUCCESS);
        CHECK(view->kind==AMS_MEL_IR_HEALTH_METADATA_SECURITY_AUDIT);
        CHECK(view->security_audit.event.kind==kind);
        CHECK(view->security_audit.event_timestamp_ns==-987654321);
        CHECK(view->security_audit.artifacts.size==2U);
        CHECK(view->security_audit.outcome==AMS_MEL_SECURITY_OUTCOME_FAILURE);
        CHECK(view->security_audit.severity==AMS_MEL_SECURITY_SEVERITY_WARNING);
        if(kind==AMS_MEL_SECURITY_EVENT_AUTHENTICATION){CHECK(view->security_audit.event.category==5U);CHECK(text_is(view->security_audit.event.details,"auth-\xCE\xB6"));CHECK(text_is(view->security_audit.event.service_id.descriptive_label,"auth service"));}
        CHECK(ams_mel_ir_health_metadata_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);
    }
    CHECK(receive(metadata,&event,&view)==EXIT_SUCCESS);
    CHECK(view->kind==AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS_DETAILED);
    CHECK(view->mfa_status_detailed.size==3U);
    CHECK(text_is(view->mfa_status_detailed.data[2].value,"\xCE\xBC"));

    CHECK(ams_mel_session_close(&session,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_health_close(&health,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(text_is(view->mfa_status_detailed.data[0].name,"load"));
    CHECK(ams_mel_ir_health_metadata_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_health_metadata_receive(metadata,0,&event,NULL,0,NULL)==AMS_MEL_STREAM_STOPPED);
    CHECK(ams_mel_ir_health_metadata_close(&metadata,NULL,0,NULL)==AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_capability(void)
{
    ams_mel_session *session=NULL;ams_mel_ir_health *health=NULL;
    ams_mel_ir_channel_capability *owner=NULL;const ams_mel_ir_channel_capability_v1 *view=NULL;
    CHECK(open_health("health-capability",&session,&health)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_health_get_capabilities(health,&owner,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_capability_view(owner,&view,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(view->channel_types.size==1U&&view->channel_types.data[0]==AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS);
    CHECK(view->metadata_capabilities.size==4U);
    CHECK(view->metadata_capabilities.data[0]==AMS_MEL_IR_METADATA_MFA_STATUS);
    CHECK(view->metadata_capabilities.data[1]==AMS_MEL_IR_METADATA_MFA_STATUS_DETAILED);
    CHECK(view->metadata_capabilities.data[2]==AMS_MEL_IR_METADATA_BIT_STATUS);
    CHECK(view->metadata_capabilities.data[3]==AMS_MEL_IR_METADATA_SUBSYSTEM_STATUS_RESP);
    CHECK(ams_mel_session_close(&session,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_health_close(&health,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(text_is(view->channel_id.descriptive_label,"channel-\xCE\xB1"));
    CHECK(ams_mel_ir_channel_capability_close(&owner,NULL,0,NULL)==AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_overflow(void)
{
    ams_mel_session *session=NULL;ams_mel_ir_health *health=NULL;ams_mel_ir_health_metadata *metadata=NULL;
    ams_mel_ir_health_metadata_event *event=NULL;const ams_mel_ir_health_metadata_event_v1 *view=NULL;
    ams_mel_ir_metadata_counters_v1 counters;
    CHECK(open_health("health-overflow",&session,&health)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_health_metadata_open(health,2,&metadata,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_health_metadata_get_counters(metadata,&counters,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(counters.events_received==12U&&counters.events_dropped_queue_full==10U&&counters.malformed_or_unsupported==0U);
    CHECK(receive(metadata,&event,&view)==EXIT_SUCCESS&&view->kind==AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS);
    CHECK(ams_mel_ir_health_metadata_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(receive(metadata,&event,&view)==EXIT_SUCCESS&&view->kind==AMS_MEL_IR_HEALTH_METADATA_BIT_STATUS);
    CHECK(ams_mel_ir_health_metadata_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_health_metadata_receive(metadata,0,&event,NULL,0,NULL)==AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_health_metadata_close(&metadata,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_health_close(&health,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session,NULL,0,NULL)==AMS_MEL_OK);return EXIT_SUCCESS;
}

static int test_detach_retry(void)
{
    ams_mel_session *session=NULL;ams_mel_ir_health *health=NULL;
    CHECK(open_health("health-detach-fail",&session,&health)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_health_close(&health,NULL,0,NULL)==AMS_MEL_PROVIDER_FAILED&&health!=NULL);
    CHECK(ams_mel_session_close(&session,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_health_close(&health,NULL,0,NULL)==AMS_MEL_OK&&health==NULL);return EXIT_SUCCESS;
}

static int file_contains_order(const char *path,const char *a,const char *b,const char *c,const char *d,const char *e)
{
    FILE *file=fopen(path,"rb");char data[4096];size_t size;
    CHECK(file!=NULL);size=fread(data,1,sizeof data-1U,file);fclose(file);data[size]='\0';
    const char *pa=strstr(data,a),*pb=strstr(data,b),*pc=strstr(data,c),*pd=strstr(data,d),*pe=strstr(data,e);
    CHECK(pa&&pb&&pc&&pd&&pe&&pa<pb&&pb<pc&&pc<pd&&pd<pe);return EXIT_SUCCESS;
}

static int test_allocation_failure(void)
{
    ams_mel_session *session=NULL;ams_mel_ir_health *health=NULL;ams_mel_ir_health_metadata *metadata=NULL;
    ams_mel_ir_health_metadata_event *event=NULL;const ams_mel_ir_health_metadata_event_v1 *view=NULL;
    CHECK(open_health("health-allocation",&session,&health)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_health_metadata_open(health,8,&metadata,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(receive(metadata,&event,&view)==EXIT_SUCCESS&&view->kind==AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS);
    CHECK(ams_mel_ir_health_metadata_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_health_metadata_receive(metadata,0,&event,NULL,0,NULL)==AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_health_close(&health,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_health_metadata_close(&metadata,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session,NULL,0,NULL)==AMS_MEL_OK);return EXIT_SUCCESS;
}

static int test_malformed_nested_value(void)
{
    ams_mel_session *session=NULL;ams_mel_ir_health *health=NULL;
    ams_mel_ir_health_metadata *metadata=NULL;ams_mel_ir_health_metadata_event *event=NULL;
    const ams_mel_ir_health_metadata_event_v1 *view=NULL;ams_mel_ir_metadata_counters_v1 counters={0};
    CHECK(open_health("health-malformed",&session,&health)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_health_metadata_open(health,4,&metadata,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(receive(metadata,&event,&view)==EXIT_SUCCESS);
    CHECK(view->kind==AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS);
    CHECK(text_is(view->mfa_status.state_description,"healthy-\xCE\xB1"));
    CHECK(ams_mel_ir_health_metadata_event_close(&event,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_health_metadata_get_counters(metadata,&counters,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(counters.events_received==2U&&counters.malformed_or_unsupported==1U);
    CHECK(ams_mel_ir_health_metadata_close(&metadata,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_health_close(&health,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session,NULL,0,NULL)==AMS_MEL_OK);return EXIT_SUCCESS;
}

static int run_partial_child(const char *log)
{
    ams_mel_session *session=NULL;ams_mel_ir_health *health=NULL;ams_mel_ir_health_metadata *metadata=NULL;
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG",log,1)==0);
    CHECK(open_health("health-register-partial",&session,&health)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_health_metadata_open(health,8,&metadata,NULL,0,NULL)==AMS_MEL_PROVIDER_FAILED);
    CHECK(metadata==NULL);CHECK(ams_mel_ir_health_close(&health,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session,NULL,0,NULL)==AMS_MEL_OK);return EXIT_SUCCESS;
}

static int test_partial_registration(void)
{
    char path[]="/tmp/ams-health-partial-XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);close(fd);
    pid_t child=fork();CHECK(child>=0);if(child==0)_exit(run_partial_child(path));
    int status=0;CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    CHECK(file_contains_order(path,"retained_health_callback_invoked","health_channel_destroyed","control_destroyed","manager_destroyed","library_unloaded")==EXIT_SUCCESS);
    unlink(path);return EXIT_SUCCESS;
}

static int run_quiescence_child(const char *log,const char *base)
{
    char start[512];ams_mel_session *session=NULL;ams_mel_ir_health *health=NULL;ams_mel_ir_health_metadata *metadata=NULL;
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG",log,1)==0);
    CHECK(setenv("AMS_MEL_TEST_HEALTH_CALLBACK_BARRIER",base,1)==0);
    CHECK(open_health("health-metadata-nonquiescing-disable",&session,&health)==EXIT_SUCCESS);
    CHECK(ams_mel_ir_health_metadata_open(health,8,&metadata,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_health_enable(health,NULL,0,NULL)==AMS_MEL_OK);
    snprintf(start,sizeof start,"%s.start",base);FILE *marker=fopen(start,"wb");CHECK(marker!=NULL);fputs("start\n",marker);fclose(marker);
    CHECK(ams_mel_ir_health_close(&health,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_ir_health_metadata_close(&metadata,NULL,0,NULL)==AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session,NULL,0,NULL)==AMS_MEL_OK);return EXIT_SUCCESS;
}

static int test_quiescence(void)
{
    char log[]="/tmp/ams-health-log-XXXXXX",base[]="/tmp/ams-health-barrier-XXXXXX";
    int fd=mkstemp(log);CHECK(fd>=0);close(fd);fd=mkstemp(base);CHECK(fd>=0);close(fd);unlink(base);
    pid_t child=fork();CHECK(child>=0);if(child==0)_exit(run_quiescence_child(log,base));
    int status=0;CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    CHECK(file_contains_order(log,"health_callback_entered","disable_returned_with_health_callback_active","health_callback_returned","health_channel_destroyed","library_unloaded")==EXIT_SUCCESS);
    char path[512];snprintf(path,sizeof path,"%s.start",base);unlink(path);snprintf(path,sizeof path,"%s.entered",base);unlink(path);snprintf(path,sizeof path,"%s.release",base);unlink(path);unlink(log);return EXIT_SUCCESS;
}

int main(void)
{
    CHECK(test_rich()==EXIT_SUCCESS);CHECK(test_capability()==EXIT_SUCCESS);
    CHECK(test_overflow()==EXIT_SUCCESS);CHECK(test_malformed_nested_value()==EXIT_SUCCESS);
    CHECK(test_detach_retry()==EXIT_SUCCESS);
    CHECK(test_allocation_failure()==EXIT_SUCCESS);CHECK(test_partial_registration()==EXIT_SUCCESS);
    CHECK(test_quiescence()==EXIT_SUCCESS);
    puts("PASS: native IR Health/Status contract");return EXIT_SUCCESS;
}
