with Ada.Command_Line;
with Ada.Text_IO;
with AMS.MEL.RF.C2;

procedure AMS_MEL_Squall_RF_C2 is
   package C2 renames AMS.MEL.RF.C2;
begin
   if Ada.Command_Line.Argument_Count /= 2 then
      raise Program_Error with "usage: ams_mel_squall_rf_c2 PROVIDER PROFILE";
   end if;
   declare
      Object : C2.C2_MEL := C2.Open
        (Ada.Command_Line.Argument (1), Ada.Command_Line.Argument (2));
   begin
      if not C2.Is_Open (Object) then
         raise Program_Error with "Squall RF C2 owner not open";
      end if;
      C2.Close (Object);
      if C2.Is_Open (Object) then
         raise Program_Error with "Squall RF C2 close left owner open";
      end if;
   end;
   Ada.Text_IO.Put_Line ("PASS: safe Ada Squall RF C2 open/close");
end AMS_MEL_Squall_RF_C2;
