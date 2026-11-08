with Ada.Command_Line;
with AMS_MEL_IR_Image_ODM_Tests;

procedure AMS_MEL_IR_Image_ODM_Focused is
begin
   AMS_MEL_IR_Image_ODM_Tests.Run (Ada.Command_Line.Argument (1));
end AMS_MEL_IR_Image_ODM_Focused;
