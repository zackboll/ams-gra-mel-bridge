with Ada.Command_Line;
with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.RF;
with AMS.MEL.RF.Product_Rx;
with AMS.MEL.RF.Admin;
with AMS.MEL.RF.C2;
with AMS.MEL.Status;
with Interfaces;

procedure AMS_MEL_Squall_RF is
   package RF renames AMS.MEL.RF;
   package RX renames AMS.MEL.RF.Product_Rx;
   package Admin renames AMS.MEL.RF.Admin;
   package C2 renames AMS.MEL.RF.C2;
   package Status renames AMS.MEL.Status;
   use type C2.Request_Outcome;
   use type C2.Job_Status;
   use type C2.Cancel_Error;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Integer_16;
   use type Interfaces.Integer_64;
   use type Interfaces.Unsigned_64;
   use type AMS.MEL.Provider_Version_Number;
   use type RF.Job_Data_Format;
   use type RX.Create_Outcome;
   use type RX.Counter;

   function Fingerprint (Value : RX.Event) return Interfaces.Unsigned_64 is
      Hash : Interfaces.Unsigned_64 := 1_469_598_103_934_665_603;
      function Word (Sample : Interfaces.Integer_16) return Interfaces.Unsigned_64
      is (Interfaces.Unsigned_64 (Interfaces.Integer_64 (Sample) mod 65_536));
      procedure Accumulate (Samples : RX.Complex_I16_Array) is
      begin
         for Sample of Samples loop
            Hash := (Hash xor Word (Sample.Real)) * 1_099_511_628_211;
            Hash := (Hash xor Word (Sample.Imag)) * 1_099_511_628_211;
         end loop;
      end Accumulate;
   begin
      RX.With_Samples (Value, Accumulate'Access);
      return Hash;
   end Fingerprint;

   function Nonzero (Value : RX.Event) return Boolean is
      Found : Boolean := False;
      procedure Scan (Samples : RX.Complex_I16_Array) is
      begin
         for Sample of Samples loop
            Found := Found or else Sample.Real /= 0 or else Sample.Imag /= 0;
         end loop;
      end Scan;
   begin
      RX.With_Samples (Value, Scan'Access);
      return Found;
   end Nonzero;
begin
   if Ada.Command_Line.Argument_Count /= 2 then
      raise Program_Error with "usage: ams_mel_squall_rf PROVIDER PROFILE";
   end if;
   declare
      Provider   : constant String := Ada.Command_Line.Argument (1);
      Profile    : constant String := Ada.Command_Line.Argument (2);
   begin
      declare
         Data    : RF.Data_MEL := RF.Open (Provider, Profile);
         Version : constant AMS.MEL.Provider_Version := RF.Query_Provider_Version (Data);
         MFA     : constant RF.MFA_Info := RF.Snapshot_MFA_Info (Data);
      begin
         if AMS.MEL.API_Version (Version) /= 1
           or else AMS.MEL.Library_Version (Version) /= 1
           or else AMS.MEL.Vendor (Version) /= "Squall"
           or else AMS.MEL.Description (Version) /= "Squall Simulator RF MEL"
           or else RF.Reported_Num_Faces (MFA) /= 1
           or else RF.Face_Count (MFA) /= 1
           or else RF.Face_ID (RF.Face_At (MFA, 1)) /= 0
           or else not RF.Supports_Receive (RF.Face_At (MFA, 1))
           or else RF.Supports_Transmit (RF.Face_At (MFA, 1))
         then
            raise Program_Error with "Squall RF version/MFA mismatch";
         end if;
         Ada.Text_IO.Put_Line
           ("Squall RF provider: 1 / 1 / "
            & AMS.MEL.Vendor (Version)
            & " / "
            & AMS.MEL.Description (Version));
         Ada.Text_IO.Put_Line ("Squall RF MFA faces:" & Natural'Image (RF.Face_Count (MFA)));
         declare
            Supported : Boolean := False;
         begin
            for I in 1 .. RF.Supported_Data_Format_Count (MFA) loop
               Supported :=
                 Supported or else RF.Supported_Data_Format_At (MFA, I) = RF.Complex_INT16;
            end loop;
            if not Supported then
               raise Program_Error with "Squall RF lacks ComplexINT16";
            end if;
         end;
         declare
            Config  : constant RX.Config := RX.Create_Config (4_096, 256, 65_536);
            Request : RX.Create_Request := RX.Submit (Data, Config);
         begin
            if RX.Outcome (RX.Wait (Request, 20_000)) /= RX.Created then
               raise Program_Error with "Squall RF create failed";
            end if;
            declare
               Endpoint : RX.Endpoint := RX.Claim (Request);
            begin
               if RX.Assigned_Data_Format (Endpoint) /= RF.Complex_INT16
                 or else RX.Endpoint_ID (Endpoint) = 0
               then
                  raise Program_Error with "Squall RF claim mismatch";
               end if;
               RX.Close (Request);
                declare
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
                      VA_Request : C2.Virtual_Aperture_Request :=
                        C2.Submit_Virtual_Aperture (Parent, VA_Config);
                   begin
                      if C2.Outcome (C2.Wait (VA_Request, 10_000)) /= C2.Created then
                         raise Program_Error with "Squall VA rejected";
                      end if;
                      declare
                         VA : C2.Virtual_Aperture := C2.Claim (VA_Request);
                         Group : constant C2.RX_Element_Group_Config :=
                           C2.Create_RX_Element_Group ("0", 1.0, "default");
                         Job_Config : constant C2.Job_Config :=
                           C2.Create_Job_Config (16#1234_5678#, 1, Group);
                         Job_Request : C2.Job_Request := C2.Submit_Job (VA, Job_Config);
                      begin
                         C2.Close (VA_Request);
                         if C2.Outcome (C2.Wait (Job_Request, 10_000)) /= C2.Created then
                            raise Program_Error with "Squall Job rejected";
                         end if;
                         declare
                            Job : C2.Job := C2.Claim (Job_Request);
                         begin
                            C2.Close (Job_Request);
                            C2.Finalize_Job (Job);
                            begin
                               declare
                                  Unexpected : constant C2.Job_Status := C2.Wait_Job_Status (Job, 0);
                               begin
                                  raise Program_Error with "Squall Job finished before receive" & Unexpected'Image;
                               end;
                            exception
                               when C2.Timeout_Error => null;
                            end;
                            C2.Close (VA);
                            C2.Close (Parent);
               declare
                  A           : constant RX.Event := RX.Receive (Endpoint, 30_000);
                  Original    : constant Interfaces.Unsigned_64 := Fingerprint (A);
                  Differences : Natural := 0;
               begin
                  if RX.Sample_Count (A) = 0 or else not Nonzero (A) then
                     raise Program_Error with "Squall RF event A empty/zero";
                  end if;
                  for I in 1 .. 7 loop
                     declare
                        Later : constant RX.Event := RX.Receive (Endpoint, 30_000);
                     begin
                        if RX.Sample_Count (Later) = 0 or else not Nonzero (Later) then
                           raise Program_Error with "Squall RF later event empty/zero";
                        end if;
                        if Fingerprint (Later) /= Original then
                           Differences := Differences + 1;
                        end if;
                     end;
                     if Fingerprint (A) /= Original then
                        raise Program_Error with "Squall RF event A changed";
                     end if;
                  end loop;
                  if Differences = 0 then
                     raise Program_Error with "Squall RF data did not vary";
                  end if;
                  declare
                     Stats : constant RX.Counters := RX.Statistics (Endpoint);
                  begin
                     if Stats.Malformed_Or_Unsupported /= 0
                       or else Stats.Products_Dropped_Queue_Full /= 0
                       or else Stats.Allocation_Failures /= 0
                     then
                        raise Program_Error with "Squall RF dropped/malformed callback";
                     end if;
                  end;
                  declare
                     Result : constant C2.Cancel_Result := C2.Cancel_Job (Job);
                  begin
                     if not C2.Cancelled (Result) or else C2.Error_Code (Result) not in C2.None
                     then
                        raise Program_Error with "Squall Job cancellation failed";
                     end if;
                  end;
                  if C2.Wait_Job_Status (Job, 10_000) /= C2.Complete
                    or else C2.Job_Request_ID (Job) /= 16#1234_5678#
                    or else C2.RX_Stream_ID_Count (Job) = 0
                  then
                     raise Program_Error with "Squall Job completion/snapshot mismatch";
                  end if;
                  C2.Close (Job);
                  RX.Close (Endpoint);
                  RF.Close (Data);
                  if Fingerprint (A) /= Original then
                     raise Program_Error with "Squall RF event A lost ownership";
                  end if;
               end;
                         end;
                      end;
                   end;
                   Admin.Close (Control);
                end;
            end;
         end;
      end;
   end;
   Ada.Text_IO.Put_Line ("PASS: safe Ada Squall RF ComplexINT16 receive");
end AMS_MEL_Squall_RF;
