#include "tx_controls_header_probe.cpp"
#include "value_modulations_header_probe.cpp"
#include "rf_admin_c2_header_compile_probe.cpp"
#include "tx_events_header_probe.cpp"
int main() { return value_modulations_contract::probe() || tx_controls_contract::tx_controls_header_probe() || tx_contract::tx_events_header_probe() || rf_admin_c2_header_compile_probe() || pulse_detection_header_probe() || lf_header_probe() || product_stream_header_probe() || endpoint_map_probe::endpoint_map_header_probe(); }
