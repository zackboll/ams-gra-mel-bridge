#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>

typedef ams_mel_ir_scheduling_update_track_list_metadata stream_t;
typedef ams_mel_ir_scheduling_update_track_list_event event_t;
typedef ams_mel_ir_update_track_list_response_v1 view_t;
#define DIAG char *, size_t, size_t *
typedef ams_mel_status_t (*open_t)(ams_mel_ir_scheduling *, size_t, stream_t **,
                                  ams_mel_ir_return_t *, DIAG);
typedef ams_mel_status_t (*receive_t)(stream_t *, uint32_t, event_t **, DIAG);
typedef ams_mel_status_t (*get_counters_t)(const stream_t *, ams_mel_ir_metadata_counters_v1 *, DIAG);
typedef ams_mel_status_t (*close_t)(stream_t **, DIAG);
typedef ams_mel_status_t (*event_view_t)(const event_t *, const view_t **, DIAG);
typedef ams_mel_status_t (*event_close_t)(event_t **, DIAG);
#define SIGNATURE(name) _Static_assert(_Generic(&ams_mel_ir_scheduling_update_track_list_##name, \
                                               name##_t: 1, default: 0), #name)
SIGNATURE(open);
SIGNATURE(receive);
SIGNATURE(get_counters);
SIGNATURE(close);
SIGNATURE(event_view);
SIGNATURE(event_close);

_Static_assert(_Generic(((view_t *)0)->command_id, uint32_t: 1, default: 0), "command_id type");
_Static_assert(_Generic(((view_t *)0)->response, ams_mel_ir_track_list_response_t: 1, default: 0),
               "response type");
_Static_assert(sizeof(view_t) == 8 && _Alignof(view_t) == 4, "record layout");
_Static_assert(offsetof(view_t, command_id) == 0 && offsetof(view_t, response) == 4, "offsets");

int main(void)
{
    const size_t layout[] = {
        sizeof(view_t), _Alignof(view_t), offsetof(view_t, command_id), offsetof(view_t, response),
        sizeof(stream_t *), _Alignof(stream_t *), sizeof(event_t *), _Alignof(event_t *),
        AMS_MEL_IR_TRACK_LIST_RESPONSE_NONE, AMS_MEL_IR_TRACK_LIST_RESPONSE_REJECTED,
        AMS_MEL_IR_TRACK_LIST_RESPONSE_REJECTED_EXPIRED, AMS_MEL_IR_TRACK_LIST_RESPONSE_ACCEPTED,
        AMS_MEL_IR_METADATA_UPDATE_TRACK_LIST_RESPONSE
    };
    for (size_t i = 0; i < sizeof layout / sizeof *layout; ++i) printf("%zu ", layout[i]);
    puts("");
    return 0;
}
