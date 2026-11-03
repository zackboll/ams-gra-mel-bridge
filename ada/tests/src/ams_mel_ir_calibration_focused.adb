with Ada.Command_Line;
with AMS_MEL_IR_Calibration_Configuration_Tests;
with AMS_MEL_IR_Calibration_Status_Tests;
with AMS_MEL_IR_Camera_Protection_Tests;
with AMS_MEL_IR_Camera_Command_Tests;
with AMS_MEL_IR_C2_Tests;
with AMS_MEL_IR_C2_Metadata_Tests;
with AMS_MEL_IR_C2_Common_Tests;

procedure AMS_MEL_IR_Calibration_Focused is
   Provider : constant String := Ada.Command_Line.Argument (1);
begin
   AMS_MEL_IR_Calibration_Status_Tests.Run (Provider);
   AMS_MEL_IR_Calibration_Configuration_Tests.Run (Provider);
   AMS_MEL_IR_Camera_Protection_Tests.Run (Provider);
   AMS_MEL_IR_Camera_Command_Tests.Run (Provider);
   AMS_MEL_IR_C2_Tests.Run (Provider);
   AMS_MEL_IR_C2_Metadata_Tests.Run (Provider);
   AMS_MEL_IR_C2_Common_Tests.Run (Provider);
end AMS_MEL_IR_Calibration_Focused;
