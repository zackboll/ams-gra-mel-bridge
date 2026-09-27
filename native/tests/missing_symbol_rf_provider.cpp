/* Task 033B: a loadable RF-shaped DSO that deliberately exports no
 * createDataMEL, proving AMS_MEL_SYMBOL_NOT_FOUND and DSO release. */
extern "C" __attribute__((visibility("default"))) int mock_rf_missing_symbol_marker(void)
{
    return 33;
}
