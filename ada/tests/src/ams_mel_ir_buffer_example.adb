with Ada.Command_Line;
with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Buffer;

procedure AMS_MEL_IR_Buffer_Example is
   --  Usage: ams_mel_ir_buffer_example /absolute/provider.so instance
   package B renames AMS.MEL.IR.Buffer;
   use type AMS.MEL.IR.Pixel_Array;
   use type B.Flag_Array;
begin
   if Ada.Command_Line.Argument_Count /= 2 then
      raise Constraint_Error with "expected provider library path and instance";
   end if;
   declare
      Parent  : AMS.MEL.Session :=
        AMS.MEL.Open (Ada.Command_Line.Argument (1), Ada.Command_Line.Argument (2), "");
      Object  : B.Host_Buffer := B.Open (Parent, 64, -42);
      Pattern : constant AMS.MEL.IR.Pixel_Array (1 .. 64) := [others => 16#A5#];
   begin
      B.Write_Storage (Object, 0, Pattern);
      B.Set_Flags (Object, [B.Host, B.Host]);
      B.Add_Flag (Object, B.GPU);
      AMS.MEL.Close (Parent);
      if B.Read_Storage (Object, 0, 64) /= Pattern
        or else B.Flags (Object) /= [B.Host, B.Host, B.GPU]
      then
         raise Program_Error with "provider changed standalone Buffer bytes or flags";
      end if;
      Ada.Text_IO.Put_Line ("Provider size:" & B.Properties (Object).Provider_Size'Image);
      B.Close (Object);
   end;
end AMS_MEL_IR_Buffer_Example;
