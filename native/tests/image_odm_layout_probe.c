#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>
/* IRMEL-ADA-009: exact Image OpticalDistortionMap signatures and layouts for
 * the private Python/Rust mirrors, compiled as C11. Prints one line of
 * size/alignment/offset values; the mirrors compare their own layouts with it.
 * As a CTest target it also fails closed on any LP64 layout drift. */
typedef ams_mel_status_t (*open_signature)(ams_mel_ir_stream *, size_t,
    ams_mel_ir_image_odm_metadata **, ams_mel_ir_return_t *, char *, size_t, size_t *);
typedef ams_mel_status_t (*receive_signature)(ams_mel_ir_image_odm_metadata *, uint32_t,
    ams_mel_ir_image_odm_event **, char *, size_t, size_t *);
typedef ams_mel_status_t (*counters_signature)(const ams_mel_ir_image_odm_metadata *,
    ams_mel_ir_metadata_counters_v1 *, char *, size_t, size_t *);
typedef ams_mel_status_t (*close_signature)(ams_mel_ir_image_odm_metadata **, char *, size_t,
    size_t *);
typedef ams_mel_status_t (*view_signature)(const ams_mel_ir_image_odm_event *,
    const ams_mel_ir_optical_distortion_map_v1 **, char *, size_t, size_t *);
typedef ams_mel_status_t (*event_close_signature)(ams_mel_ir_image_odm_event **, char *, size_t,
    size_t *);
_Static_assert(_Generic(&ams_mel_ir_image_odm_open, open_signature: 1, default: 0), "open");
_Static_assert(_Generic(&ams_mel_ir_image_odm_receive, receive_signature: 1, default: 0),
               "receive");
_Static_assert(_Generic(&ams_mel_ir_image_odm_get_counters, counters_signature: 1, default: 0),
               "counters");
_Static_assert(_Generic(&ams_mel_ir_image_odm_close, close_signature: 1, default: 0), "close");
_Static_assert(_Generic(&ams_mel_ir_image_odm_event_view, view_signature: 1, default: 0), "view");
_Static_assert(_Generic(&ams_mel_ir_image_odm_event_close, event_close_signature: 1, default: 0),
               "event close");
/* Field types: five uint32, four double extrema, double FRD/RowCol parts. */
#define ODM ((ams_mel_ir_optical_distortion_map_v1 *)0)
_Static_assert(_Generic(ODM->width, uint32_t: 1, default: 0), "width");
_Static_assert(_Generic(ODM->pixels_per_odm, uint32_t: 1, default: 0), "pixels per ODM");
_Static_assert(_Generic(ODM->right_min, double: 1, default: 0), "extremum");
_Static_assert(_Generic(ODM->down_max, double: 1, default: 0), "extremum");
_Static_assert(_Generic(ODM->frd.data, const ams_mel_ir_forward_right_down_v1 *: 1, default: 0),
               "FRD span");
_Static_assert(_Generic(ODM->row_col.data, const ams_mel_ir_row_col_v1 *: 1, default: 0),
               "canonical RowCol span");
_Static_assert(_Generic(((ams_mel_ir_forward_right_down_v1 *)0)->down, double: 1, default: 0),
               "FRD component");
#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(ams_mel_ir_forward_right_down_v1) == 24, "FRD size");
_Static_assert(offsetof(ams_mel_ir_forward_right_down_v1, right) == 8, "FRD right");
_Static_assert(offsetof(ams_mel_ir_forward_right_down_v1, down) == 16, "FRD down");
_Static_assert(sizeof(ams_mel_ir_row_col_v1) == 16, "RowCol unchanged");
_Static_assert(sizeof(ams_mel_ir_forward_right_down_span_v1) == 16, "FRD span size");
_Static_assert(sizeof(ams_mel_ir_row_col_span_v1) == 16, "RowCol span size");
_Static_assert(offsetof(ams_mel_ir_optical_distortion_map_v1, pixels_per_odm) == 16, "pixels");
_Static_assert(offsetof(ams_mel_ir_optical_distortion_map_v1, right_min) == 24, "right min");
_Static_assert(offsetof(ams_mel_ir_optical_distortion_map_v1, down_max) == 48, "down max");
_Static_assert(offsetof(ams_mel_ir_optical_distortion_map_v1, frd) == 56, "frd");
_Static_assert(offsetof(ams_mel_ir_optical_distortion_map_v1, row_col) == 72, "row col");
_Static_assert(sizeof(ams_mel_ir_optical_distortion_map_v1) == 88, "map size");
#endif
int main(void)
{
    printf("%zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu "
           "%zu %zu %zu %zu %zu %zu %zu\n",
        sizeof(ams_mel_ir_forward_right_down_v1), _Alignof(ams_mel_ir_forward_right_down_v1),
        offsetof(ams_mel_ir_forward_right_down_v1, forward),
        offsetof(ams_mel_ir_forward_right_down_v1, right),
        offsetof(ams_mel_ir_forward_right_down_v1, down),
        sizeof(ams_mel_ir_forward_right_down_span_v1),
        offsetof(ams_mel_ir_forward_right_down_span_v1, size),
        sizeof(ams_mel_ir_row_col_span_v1), offsetof(ams_mel_ir_row_col_span_v1, size),
        sizeof(ams_mel_ir_optical_distortion_map_v1),
        _Alignof(ams_mel_ir_optical_distortion_map_v1),
        offsetof(ams_mel_ir_optical_distortion_map_v1, width),
        offsetof(ams_mel_ir_optical_distortion_map_v1, height),
        offsetof(ams_mel_ir_optical_distortion_map_v1, right_size),
        offsetof(ams_mel_ir_optical_distortion_map_v1, down_size),
        offsetof(ams_mel_ir_optical_distortion_map_v1, pixels_per_odm),
        offsetof(ams_mel_ir_optical_distortion_map_v1, right_min),
        offsetof(ams_mel_ir_optical_distortion_map_v1, right_max),
        offsetof(ams_mel_ir_optical_distortion_map_v1, down_min),
        offsetof(ams_mel_ir_optical_distortion_map_v1, down_max),
        offsetof(ams_mel_ir_optical_distortion_map_v1, frd),
        offsetof(ams_mel_ir_optical_distortion_map_v1, row_col),
        sizeof(ams_mel_ir_row_col_v1), offsetof(ams_mel_ir_row_col_v1, column),
        sizeof(ams_mel_ir_metadata_counters_v1),
        offsetof(ams_mel_ir_metadata_counters_v1, events_dropped_queue_full),
        offsetof(ams_mel_ir_metadata_counters_v1, malformed_or_unsupported));
    return 0;
}
