with Ada.Command_Line;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with AMS.MEL;
with AMS.MEL.RF;
with AMS.MEL.RF.Product_Rx;
with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;

procedure AMS_MEL_Squall_RF is
   package RF renames AMS.MEL.RF;
   package RX renames AMS.MEL.RF.Product_Rx;
   package CS renames Interfaces.C.Strings;
   use type Interfaces.C.int;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Integer_16;
   use type Interfaces.Integer_64;
   use type Interfaces.Unsigned_64;
   use type AMS.MEL.Provider_Version_Number;
   use type RF.Job_Data_Format;
   use type RX.Create_Outcome;
   use type RX.Counter;
   use type System.Address;

   function DL_Open (Path : CS.chars_ptr; Flags : Interfaces.C.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function DL_Sym (Handle : System.Address; Name : CS.chars_ptr) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   type Start_Access is
     access function
       (Path, Profile : CS.chars_ptr; Error : System.Address; Capacity : Interfaces.C.size_t)
        return Interfaces.C.int
   with Convention => C;
   type Stop_Access is access procedure with Convention => C;
   function To_Start is new Ada.Unchecked_Conversion (System.Address, Start_Access);
   function To_Stop is new Ada.Unchecked_Conversion (System.Address, Stop_Access);

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
   if Ada.Command_Line.Argument_Count /= 3 then
      raise Program_Error with "usage: ams_mel_squall_rf PROVIDER PROFILE JOB_HELPER";
   end if;
   declare
      Path       : CS.chars_ptr := CS.New_String (Ada.Command_Line.Argument (3));
      Start_Name : CS.chars_ptr := CS.New_String ("squall_rf_test_job_start");
      Stop_Name  : CS.chars_ptr := CS.New_String ("squall_rf_test_job_stop");
      Library    : constant System.Address := DL_Open (Path, 2);
      Start      : constant Start_Access := To_Start (DL_Sym (Library, Start_Name));
      Stop       : constant Stop_Access := To_Stop (DL_Sym (Library, Stop_Name));
      Provider   : CS.chars_ptr := CS.New_String (Ada.Command_Line.Argument (1));
      Profile    : CS.chars_ptr := CS.New_String (Ada.Command_Line.Argument (2));
      Error      : aliased Interfaces.C.char_array (0 .. 511) := [others => Interfaces.C.nul];
   begin
      CS.Free (Path);
      CS.Free (Start_Name);
      CS.Free (Stop_Name);
      if Library = System.Null_Address or else Start = null or else Stop = null then
         raise Program_Error with "RF job helper unavailable";
      end if;
      declare
         Data    : RF.Data_MEL :=
           RF.Open (Ada.Command_Line.Argument (1), Ada.Command_Line.Argument (2));
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
               if Start (Provider, Profile, Error'Address, Error'Length) /= 1 then
                  raise Program_Error with Interfaces.C.To_Ada (Error);
               end if;
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
                     then
                        raise Program_Error with "Squall RF dropped/malformed callback";
                     end if;
                  end;
                  Stop.all;
                  RX.Close (Endpoint);
                  RF.Close (Data);
                  if Fingerprint (A) /= Original then
                     raise Program_Error with "Squall RF event A lost ownership";
                  end if;
               end;
            end;
         end;
      end;
      CS.Free (Provider);
      CS.Free (Profile);
   end;
   Ada.Text_IO.Put_Line ("PASS: safe Ada Squall RF ComplexINT16 receive");
end AMS_MEL_Squall_RF;
