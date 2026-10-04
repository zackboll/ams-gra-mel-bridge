/* Shared C11 signature check, used by both raw-language layout probes. */
static inline void check_rf_local_function_signatures(void)
{
    ams_mel_status_t (*cached)(const ams_mel_rf_virtual_aperture *, uint32_t *, char *, size_t, size_t *) = ams_mel_rf_virtual_aperture_is_cached_waveform_supported;
    ams_mel_status_t (*dynamic)(const ams_mel_rf_virtual_aperture *, uint32_t *, char *, size_t, size_t *) = ams_mel_rf_virtual_aperture_dynamic_weights_supported;
    ams_mel_status_t (*catalog)(const ams_mel_rf_virtual_aperture *, ams_mel_rf_va_local_function_list **, char *, size_t, size_t *) = ams_mel_rf_virtual_aperture_get_local_functions;
    ams_mel_status_t (*catalog_view)(const ams_mel_rf_va_local_function_list *, ams_mel_rf_va_local_function_info_span_v1 *, char *, size_t, size_t *) = ams_mel_rf_va_local_function_list_view;
    ams_mel_status_t (*catalog_close)(ams_mel_rf_va_local_function_list **, char *, size_t, size_t *) = ams_mel_rf_va_local_function_list_close;
    ams_mel_status_t (*status)(const ams_mel_rf_virtual_aperture *, uint32_t, uint32_t, ams_mel_rf_va_local_function_status **, char *, size_t, size_t *) = ams_mel_rf_virtual_aperture_get_local_function_status;
    ams_mel_status_t (*status_view)(const ams_mel_rf_va_local_function_status *, ams_mel_u32_span_v1 *, char *, size_t, size_t *) = ams_mel_rf_va_local_function_status_view;
    ams_mel_status_t (*status_close)(ams_mel_rf_va_local_function_status **, char *, size_t, size_t *) = ams_mel_rf_va_local_function_status_close;
    (void)cached; (void)dynamic; (void)catalog; (void)catalog_view; (void)catalog_close;
    (void)status; (void)status_view; (void)status_close;
}