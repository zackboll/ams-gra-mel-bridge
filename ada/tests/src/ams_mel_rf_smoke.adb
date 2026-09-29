with Ada.Environment_Variables;
with Ada.Text_IO;
with AMS_MEL_RF_Product_Rx_Tests;
with AMS_MEL_RF_Tests;

procedure AMS_MEL_RF_Smoke is
   use Ada.Environment_Variables;
   Directory : constant String :=
     (if Exists ("AMS_MEL_TEST_PROVIDER_DIR")
      then Value ("AMS_MEL_TEST_PROVIDER_DIR")
      else "native/build-tests/test-providers");
   Provider  : constant String := Directory & "/libmock_rf_provider.so";
begin
   AMS_MEL_RF_Tests.Run (Provider);
   AMS_MEL_RF_Product_Rx_Tests.Run (Provider);
   Ada.Text_IO.Put_Line ("PASS: safe Ada RF DataMEL/MFA/ProductRx contract");
end AMS_MEL_RF_Smoke;
