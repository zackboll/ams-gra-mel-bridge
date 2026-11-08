with Ada.Command_Line;
with AMS_MEL_IR_Erase_Tests;

procedure AMS_MEL_IR_Erase_Focused is
begin
   if Ada.Command_Line.Argument_Count = 1 then
      AMS_MEL_IR_Erase_Tests.Run (Ada.Command_Line.Argument (1));
   else
      AMS_MEL_IR_Erase_Tests.Run_Isolated
        (Ada.Command_Line.Argument (1), Ada.Command_Line.Argument (2));
   end if;
end AMS_MEL_IR_Erase_Focused;
