with Ada.Command_Line;
with Ada.Text_IO;
with AMS.MEL.RF.Admin;
with AMS.MEL.RF.C2;
with AMS.MEL.Status;
with Interfaces;

procedure AMS_MEL_Squall_RF_Job is
   package Admin renames AMS.MEL.RF.Admin;
   package C2 renames AMS.MEL.RF.C2;
   package Status renames AMS.MEL.Status;
   use type C2.Request_Outcome;
   use type C2.Job_Status;
   use type C2.Cancel_Error;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Integer_64;
   procedure Verify (Object : C2.Job; ID : Interfaces.Unsigned_32) is
      Start : constant Interfaces.Integer_64 := C2.Actual_Start_Seconds (Object);
      Fraction : constant Interfaces.Integer_64 := C2.Actual_Start_Femtoseconds (Object);
      Duration : constant Interfaces.Integer_64 := C2.Total_Job_Duration_Femtoseconds (Object);
      Lookahead : constant Interfaces.Integer_64 := C2.Lookahead_Femtoseconds (Object);
      Details : constant Interfaces.Unsigned_32 := C2.Job_Details_ID (Object);
   begin
      if C2.VA_Instance_ID (Object) /= 0
        or else C2.VA_Definition_ID (Object) /= 0
        or else C2.Job_Request_ID (Object) /= ID
        or else C2.RX_Stream_ID_Count (Object) < 1
        or else C2.RX_Stream_ID_At (Object, 1) /= 0
        or else Fraction < 0 or else Fraction >= 1_000_000_000_000_000
      then
         raise Program_Error with "Squall Job snapshot mismatch";
      end if;
      Ada.Text_IO.Put_Line
        ("Squall Job snapshot: request" & ID'Image & " details" & Details'Image &
         " start" & Start'Image & "/" & Fraction'Image &
         " duration" & Duration'Image & " lookahead" & Lookahead'Image);
   end Verify;
begin
   if Ada.Command_Line.Argument_Count /= 2 then
      raise Program_Error with "usage: ams_mel_squall_rf_job PROVIDER PROFILE";
   end if;
   declare
      Provider : constant String := Ada.Command_Line.Argument (1);
      Profile : constant String := Ada.Command_Line.Argument (2);
      Control : Admin.Admin_MEL := Admin.Open (Provider, Profile);
   begin
      if not Admin.Command_State (Control, Status.Standby)
        or else not Admin.Command_State (Control, Status.Operate_Rx_Only)
      then
         raise Program_Error with "Squall Admin rejected RX state";
      end if;
      declare
         Parent : C2.C2_MEL := C2.Open (Provider, Profile);
         VA_Config : constant C2.Virtual_Aperture_Config :=
           C2.Create_Virtual_Aperture_Config (0, 1, "");
         VA_Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, VA_Config);
      begin
         if C2.Outcome (C2.Wait (VA_Request, 10_000)) /= C2.Created then
            raise Program_Error with "Squall VA rejected";
         end if;
         declare
            VA : C2.Virtual_Aperture := C2.Claim (VA_Request);
            Group : constant C2.RX_Element_Group_Config :=
              C2.Create_RX_Element_Group ("0", 1.0, "default");
         begin
            C2.Close (VA_Request);
            for Sequence in 0 .. 1 loop
               declare
                  ID : constant Interfaces.Unsigned_32 := 16#1234_5678# + Interfaces.Unsigned_32 (Sequence);
                  Config : constant C2.Job_Config := C2.Create_Job_Config (ID, 1, Group);
                  Request : C2.Job_Request := C2.Submit_Job (VA, Config);
               begin
                  if C2.Outcome (C2.Wait (Request, 10_000)) /= C2.Created then
                     raise Program_Error with "Squall Job rejected";
                  end if;
                  declare
                     Object : C2.Job := C2.Claim (Request);
                  begin
                     C2.Close (Request);
                     Verify (Object, ID);
                     if Sequence = 1 then
                        C2.Finalize_Job (Object);
                        begin
                           declare
                              Pending : constant C2.Job_Status := C2.Wait_Job_Status (Object, 0);
                           begin
                              raise Program_Error with "Squall Job unexpectedly completed" & Pending'Image;
                           end;
                        exception
                           when C2.Timeout_Error => null;
                        end;
                        C2.Close (VA);
                        C2.Close (Parent);
                        declare
                           Result : constant C2.Cancel_Result := C2.Cancel_Job (Object);
                        begin
                           if not C2.Cancelled (Result) or else C2.Error_Code (Result) not in C2.None then
                              raise Program_Error with "Squall Job cancellation failed";
                           end if;
                        end;
                        if C2.Wait_Job_Status (Object, 10_000) /= C2.Complete then
                           raise Program_Error with "Squall Job did not complete";
                        end if;
                        Verify (Object, ID);
                     end if;
                     C2.Close (Object);
                  end;
               end;
            end loop;
         end;
      end;
      Admin.Close (Control);
   end;
    Ada.Text_IO.Put_Line ("PASS: safe Ada Squall RF two Jobs and parent-first lifecycle");
end AMS_MEL_Squall_RF_Job;
