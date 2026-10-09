#include "rf_admin_c2_header_compile_probe.cpp"
int main() { return rf_admin_c2_header_compile_probe() || pulse_detection_header_probe() || lf_header_probe() || product_stream_header_probe() || endpoint_map_probe::endpoint_map_header_probe(); }
