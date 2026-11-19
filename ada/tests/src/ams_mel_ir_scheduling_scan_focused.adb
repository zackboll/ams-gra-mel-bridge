with Ada.Command_Line;
with AMS_MEL_IR_Scheduling_Scan_Tests;

procedure AMS_MEL_IR_Scheduling_Scan_Focused is
begin
   AMS_MEL_IR_Scheduling_Scan_Tests.Run (Ada.Command_Line.Argument (1));
end AMS_MEL_IR_Scheduling_Scan_Focused;
