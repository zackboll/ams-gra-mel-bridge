with Ada.Command_Line;
with Ada.Text_IO;
with AMS.MEL.RF.Admin;
with AMS.MEL.RF.C2;
with AMS.MEL.Status;
with Interfaces;

procedure AMS_MEL_Squall_RF_VA is
   package Admin renames AMS.MEL.RF.Admin;
   package C2 renames AMS.MEL.RF.C2;
   package Status renames AMS.MEL.Status;
   use type C2.Request_Outcome;
   use type Interfaces.Unsigned_32;
begin
   if Ada.Command_Line.Argument_Count /= 2 then
      raise Program_Error with "usage: ams_mel_squall_rf_va PROVIDER PROFILE";
   end if;
   declare
      Provider : constant String := Ada.Command_Line.Argument (1);
      Profile : constant String := Ada.Command_Line.Argument (2);
      Control : Admin.Admin_MEL := Admin.Open (Provider, Profile);
   begin
      if not Admin.Command_State (Control, Status.Standby) or else
        not Admin.Command_State (Control, Status.Operate_Rx_Only) then
         raise Program_Error with "Squall Admin rejected state";
      end if;
      declare
         Parent : C2.C2_MEL := C2.Open (Provider, Profile);
         Config : constant C2.Virtual_Aperture_Config :=
           C2.Create_Virtual_Aperture_Config (0, 1, "");
         Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, Config);
      begin
         if C2.Outcome (C2.Wait (Request, 10_000)) /= C2.Created then
            raise Program_Error with "Squall VA was not created";
         end if;
         declare
            VA : C2.Virtual_Aperture := C2.Claim (Request);
         begin
            C2.Close (Request);
            if C2.VA_Instance_ID_Count (VA) = 0 or else
              C2.VA_Instance_ID_At (VA, 1) /= 0 or else
              C2.Element_Group_Label_Count (VA) = 0 or else
              C2.Element_Group_Label_At (VA, 1) /= "0" or else
              not C2.Is_Single_Group (VA) then
               raise Program_Error with "Squall VA snapshot mismatch";
            end if;
            C2.Close (Parent);
            if C2.VA_Instance_ID_At (VA, 1) /= 0 or else
              C2.Element_Group_Label_At (VA, 1) /= "0" or else
              not C2.Is_Single_Group (VA) then
               raise Program_Error with "Squall VA invalid after parent Close";
            end if;
            C2.Close (VA);
         end;
      end;
      Admin.Close (Control);
   end;
   Ada.Text_IO.Put_Line ("PASS: safe Ada Squall RF VirtualAperture parent-first snapshot");
end AMS_MEL_Squall_RF_VA;