with Ada.Command_Line;
with AMS_MEL_IR_Scheduling_Abort_Tests;

procedure AMS_MEL_IR_Scheduling_Abort_Focused is
begin
   AMS_MEL_IR_Scheduling_Abort_Tests.Run (Ada.Command_Line.Argument (1));
end AMS_MEL_IR_Scheduling_Abort_Focused;
