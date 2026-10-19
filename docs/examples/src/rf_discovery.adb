with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.RF;
with AMS.MEL.RF.Admin;
with AMS.MEL.Status;
with Example_Support;
with Interfaces;

procedure RF_Discovery is
   package RF renames AMS.MEL.RF;
   package Admin renames AMS.MEL.RF.Admin;
   use Example_Support;
   use type Interfaces.Unsigned_32;
   use type RF.Job_Data_Format;
begin
   declare
      Data     : RF.Data_MEL := RF.Open (Provider_Path, "success");
      Version  : constant AMS.MEL.Provider_Version := RF.Query_Provider_Version (Data);
      Snapshot : constant RF.MFA_Info := RF.Snapshot_MFA_Info (Data);
   begin
      Require (AMS.MEL.Vendor (Version)'Length > 0, "provider vendor");
      Ada.Text_IO.Put_Line ("RF vendor: " & AMS.MEL.Vendor (Version));
      RF.Close (Data);
      Require
        (RF.Face_Count (Snapshot) = 2
         and then RF.Face_ID (RF.Face_At (Snapshot, 1)) = 7
         and then RF.Face_ID (RF.Face_At (Snapshot, 2)) = 42,
         "owned face snapshot");
      Require
        (RF.Supports_Receive (RF.Face_At (Snapshot, 1))
         and then RF.Supported_Data_Format_Count (Snapshot) = 4
         and then RF.Supported_Data_Format_At (Snapshot, 2) = RF.Complex_INT16,
         "advertised receive capability");
      Ada.Text_IO.Put_Line ("owned after Data Close: indices 1,2 -> Face_ID 7,42; formats=4");
   end;
   declare
      Administration : Admin.Admin_MEL := Admin.Open (Provider_Path, Configuration);
      Accepted       : constant Boolean :=
        Admin.Command_State (Administration, AMS.MEL.Status.Standby);
   begin
      if Accepted then
         Ada.Text_IO.Put_Line ("Standby command accepted (not physical completion)");
      else
         Ada.Text_IO.Put_Line ("Standby command rejected");
      end if;
      Admin.Close (Administration);
   end;
exception
   when Error : others =>
      Report_Failure (Error);
end RF_Discovery;
