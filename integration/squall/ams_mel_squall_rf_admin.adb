with Ada.Command_Line;
with Ada.Text_IO;
with AMS.MEL.RF.Admin;
with AMS.MEL.Status;

procedure AMS_MEL_Squall_RF_Admin is
   package Admin renames AMS.MEL.RF.Admin;
   package Status renames AMS.MEL.Status;
begin
   if Ada.Command_Line.Argument_Count /= 2 then
      raise Program_Error with "usage: ams_mel_squall_rf_admin PROVIDER PROFILE";
   end if;
   declare
      Object : Admin.Admin_MEL := Admin.Open
        (Ada.Command_Line.Argument (1), Ada.Command_Line.Argument (2));
   begin
      if not Admin.Is_Open (Object)
        or else not Admin.Command_State (Object, Status.Standby)
        or else not Admin.Command_State (Object, Status.Operate_Rx_Only)
      then
         raise Program_Error with "Squall RF Admin state command rejected";
      end if;
      Admin.Close (Object);
      if Admin.Is_Open (Object) then
         raise Program_Error with "Squall RF Admin close left owner open";
      end if;
   end;
   Ada.Text_IO.Put_Line ("PASS: safe Ada Squall RF Admin Standby -> Operate_Rx_Only");
end AMS_MEL_Squall_RF_Admin;