with Ada.Environment_Variables;
with Ada.Exceptions;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with AMS.MEL.RF.C2;
with AMS.MEL.RF.C2.Interval_Status;
with AMS.MEL.IR;
with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;
with GNAT.Source_Info;

procedure AMS_MEL_RF_Job_Extension is
   package C2 renames AMS.MEL.RF.C2;
   package Status renames C2.Interval_Status;
   package CS renames Interfaces.C.Strings;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Integer_64;
   use type Interfaces.C.unsigned;
   use type Interfaces.C.int;
   use type C2.Request_Outcome;
   use type C2.Job_Status;
   use type Status.Completion_Status;
   use type Status.Log_Trigger;
   use type System.Address;
   function DL_Open (Path : CS.chars_ptr; Flags : Interfaces.C.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function DL_Sym (Handle : System.Address; Name : CS.chars_ptr) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   function DL_Close (Handle : System.Address) return Interfaces.C.int
   with Import, Convention => C, External_Name => "dlclose";
   function DL_Error return CS.chars_ptr
   with Import, Convention => C, External_Name => "dlerror";
   type Count_Access is access function return Interfaces.C.unsigned with Convention => C;
   type At_Access is
     access function
       (Index           : Interfaces.C.unsigned;
        Interval, Event : access Interfaces.Unsigned_32;
        Duration        : access Interfaces.Integer_64) return Interfaces.C.unsigned
   with Convention => C;
   function To_Count is new Ada.Unchecked_Conversion (System.Address, Count_Access);
   function To_At is new Ada.Unchecked_Conversion (System.Address, At_Access);
   type Hold_Access is access function (Value : Interfaces.C.unsigned) return Interfaces.C.unsigned
   with Convention => C;
   function To_Hold is new Ada.Unchecked_Conversion (System.Address, Hold_Access);
   Provider      : constant String :=
     Ada.Environment_Variables.Value ("AMS_MEL_TEST_PROVIDER_DIR") & "/libmock_rf_provider.so";
   Path          : CS.chars_ptr := CS.New_String (Provider);
   Library       : constant System.Address := DL_Open (Path, 2);
   function Symbol (Name : String) return System.Address is
      Text   : CS.chars_ptr := CS.New_String (Name);
      Result : constant System.Address := DL_Sym (Library, Text);
   begin
      CS.Free (Text);
      if Result = System.Null_Address then
         raise Program_Error with "missing mock observer " & Name & ": " & CS.Value (DL_Error);
      end if;
      return Result;
   end Symbol;
   Calls         : constant Count_Access := To_Count (Symbol ("mock_rf_extension_count"));
   At_Index      : constant At_Access := To_At (Symbol ("mock_rf_extension_at"));
   Hold          : constant Hold_Access := To_Hold (Symbol ("mock_rf_extension_hold"));
   Wait_Callback : constant Count_Access := To_Count (Symbol ("mock_rf_extension_wait"));
   type Duration_Array is array (Positive range <>) of Interfaces.Integer_64;
   procedure Verify (Condition : Boolean; Location : String := GNAT.Source_Info.Source_Location) is
   begin
      if not Condition then
         raise Program_Error with "RF extension assertion at " & Location;
      end if;
   end Verify;
   function VA_Config return C2.Virtual_Aperture_Config is
      Config               : C2.Virtual_Aperture_Config :=
        C2.Create_Virtual_Aperture_Config (16#FEDC_BA98#, 16#8000_0001#, "definition/β.json");
      First, Second, Third : AMS.MEL.IR.UUID := [others => 0];
   begin
      C2.Append_Local_Function_Info (Config, "alpha");
      C2.Append_Local_Function_Info (Config, "µ-local");
      C2.Append_Local_Function_Info (Config, "");
      First (1) := 16#80#;
      First (2) := 16#FF#;
      Second (0) := 16#FF#;
      Third (15) := 16#80#;
      C2.Append_Capability_ID (Config, AMS.MEL.IR.Create_UCI_ID (First, "first"));
      C2.Append_Capability_ID (Config, AMS.MEL.IR.Create_UCI_ID (Second, ""));
      C2.Append_Capability_ID (Config, AMS.MEL.IR.Create_UCI_ID (Third, "µ-third"));
      return Config;
   end VA_Config;
   function Job_Config return C2.Job_Config is
      Group : C2.RX_Element_Group_Config :=
        C2.Create_RX_Element_Group ("rx/µ-main", 0.625, "products/β");
   begin
      C2.Append_Expected_Center_Frequency (Group, 1000000.25, 2000000.5);
      C2.Append_Expected_Center_Frequency (Group, 987654321.125, 987654322.875);
      C2.Append_Endpoint_ID (Group, 0);
      C2.Append_Endpoint_ID (Group, 16#8000_0000_0000_0000#);
      C2.Append_Endpoint_ID (Group, Interfaces.Unsigned_64'Last);
      return
         Config : C2.Job_Config :=
           C2.Create_Job_Config (16#FEDC_BA98#, 16#8000_0001#, Group, 16#7FFF_FFFE#, True)
      do
         C2.Append_Instance_Selection (Config, 0);
         C2.Append_Instance_Selection (Config, 42);
         C2.Append_Instance_Selection (Config, Interfaces.Unsigned_32'Last);
      end return;
   end Job_Config;
   procedure Snapshot (Object : C2.Job) is
   begin
      Verify
        (C2.Actual_Start_Seconds (Object) = -123456789
         and C2.Actual_Start_Femtoseconds (Object) = 999999999999999
         and C2.Total_Job_Duration_Femtoseconds (Object) = 7654321098765);
      Verify
        (C2.VA_Instance_ID (Object) = 42
         and C2.VA_Definition_ID (Object) = 16#ABCD_EF01#
         and C2.Job_Details_ID (Object) = 16#1020_3040#
         and C2.Job_Request_ID (Object) = 16#FEDC_BA98#
         and C2.Lookahead_Femtoseconds (Object) = -12345);
      Verify
        (C2.RX_Stream_ID_Count (Object) = 3
         and C2.RX_Stream_ID_At (Object, 1) = 0
         and C2.RX_Stream_ID_At (Object, 2) = 3
         and C2.RX_Stream_ID_At (Object, 3) = Interfaces.Unsigned_32'Last);
   end Snapshot;
   procedure Recorded
     (Index           : Interfaces.C.unsigned;
      Interval, Event : Interfaces.Unsigned_32;
      Duration        : Interfaces.Integer_64)
   is
      I, E : aliased Interfaces.Unsigned_32 := 0;
      D    : aliased Interfaces.Integer_64 := 0;
   begin
      Verify (Calls.all = Index + 1 and At_Index (Index, I'Access, E'Access, D'Access) = 1);
      Verify (I = Interval and E = Event and D = Duration);
   end Recorded;
   procedure Extend
     (Object   : in out C2.Job;
      Duration : Interfaces.Integer_64;
      Interval : Interfaces.Unsigned_32 := 16#FEDC_BA98#;
      Event    : Interfaces.Unsigned_32 := 16#8000_0001#)
   is
      Before : constant Interfaces.C.unsigned := Calls.all;
   begin
      C2.Extend_Job_Event (Object, Interval, Event, Duration);
      Recorded (Before, Interval, Event, Duration);
      Snapshot (Object);
   end Extend;
   procedure Payload (Event : Status.Status_Event; Sequence : Interfaces.Integer_64) is
   begin
      Verify
        (Status.Interval_ID (Event) = 16#FEDC_BA98#
         and Status.Completion (Event) = Status.Started
         and Status.Log_Count (Event) = 1);
      Verify
        (Status.Event_ID (Status.Log_At (Event, 1)) = 16#8000_0001#
         and Status.Trigger (Status.Log_At (Event, 1)) = Status.Event_Extended);
      Verify
        (Status.Time_Seconds (Status.Log_At (Event, 1)) = 1234567890 + Sequence
         and Status.Time_Fractional_Femtoseconds (Status.Log_At (Event, 1))
             = 987654321000 + Sequence);
   end Payload;
   procedure Run (Scenario : String; Feedback : Boolean := False) is
      Parent : C2.C2_MEL := C2.Open (Provider, Scenario);
      VR     : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, VA_Config);
   begin
      Verify (C2.Outcome (C2.Wait (VR, 3_000)) = C2.Created);
      declare
         VA : C2.Virtual_Aperture := C2.Claim (VR);
         JR : C2.Job_Request := C2.Submit_Job (VA, Job_Config);
      begin
         C2.Close (VR);
         Verify (C2.Outcome (C2.Wait (JR, 3_000)) = C2.Created);
         declare
            Object : C2.Job := C2.Claim (JR);
         begin
            C2.Close (JR);
            if Feedback then
               declare
                  Stream    : Status.Stream := Status.Open (Object, 2, 1, 0);
                  Interval  : C2.RX_Job_Interval_Config :=
                    C2.Create_RX_Job_Interval (16#FEDC_BA98#, 2_000_000_000);
                  Intervals : C2.RX_Job_Interval_List;
                  Held      : Status.Status_Event;
               begin
                  C2.Set_Interval_Status_Enable (Interval, C2.Always);
                  C2.Append_RX_Event
                    (Interval,
                     C2.Create_RX_Receive_Event
                       (16#8000_0001#,
                        "rx/µ-main",
                        0,
                        1_000_000_000,
                        100_000_000.0,
                        1_000_000.0,
                        Max_Extension_Femtoseconds => 500_000_000));
                  C2.Append_Job_Interval (Intervals, Interval);
                  C2.Add_RX_Job_Intervals (Object, Intervals);
                  C2.Flush_Job (Object);
                  C2.Finalize_Job (Object);
                  begin
                     declare
                        Unexpected : constant C2.Job_Status := C2.Wait_Job_Status (Object, 0);
                     begin
                        raise Program_Error with "missing pending finalize" & Unexpected'Image;
                     end;
                  exception
                     when C2.Timeout_Error =>
                        null;
                  end;
                  C2.Close (VA);
                  C2.Close (Parent);
                  declare
                     Before : constant Interfaces.C.unsigned := Calls.all;
                     Passed : Boolean;
                     task Worker is
                        entry Start;
                        entry Finished (Success : out Boolean);
                     end Worker;
                     task body Worker is
                        OK : Boolean := True;
                     begin
                        accept Start;
                        begin
                           C2.Extend_Job_Event (Object, 16#FEDC_BA98#, 16#8000_0001#, 123456789);
                        exception
                           when others =>
                              OK := False;
                        end;
                        accept Finished (Success : out Boolean) do
                           Success := OK;
                        end Finished;
                     end Worker;
                  begin
                     Verify (Hold (1) = 1);
                     Worker.Start;
                     Verify (Wait_Callback.all = 1);
                     Held := Status.Receive_Event (Stream, 0);
                     Payload (Held, 1);
                     Recorded (Before, 16#FEDC_BA98#, 16#8000_0001#, 123456789);
                     Verify (Hold (0) = 1);
                     Worker.Finished (Passed);
                     Verify (Passed);
                     Snapshot (Object);
                  end;
                  Extend (Object, 123456789);
                  Payload (Status.Receive_Event (Stream, 0), 2);
                  Payload (Held, 1);
                  C2.Cancel_Remaining_Job_Intervals (Object);
                  Extend (Object, 1);
                  Payload (Status.Receive_Event (Stream, 0), 3);
                  if Scenario = "c2:extension-reference" then
                     Status.Close (Stream);
                     Extend (Object, 1);
                     Payload (Held, 1);
                  end if;
                  declare
                     Cancelled : constant C2.Cancel_Result := C2.Cancel_Job (Object);
                  begin
                     Verify (C2.Cancelled (Cancelled));
                  end;
                  Verify (C2.Wait_Job_Status (Object, 3_000) = C2.Complete);
                  declare
                     Before : constant Interfaces.C.unsigned := Calls.all;
                  begin
                     begin
                        C2.Extend_Job_Event (Object, 0, 0, 0);
                        raise Program_Error with "missing Cancel rejection";
                     exception
                        when AMS.MEL.Provider_Error =>
                           Verify (Calls.all = Before);
                     end;
                  end;
                  Snapshot (Object);
                  C2.Close (Object);
                  if Status.Is_Open (Stream) then
                     begin
                        Held := Status.Receive_Event (Stream, 0);
                        raise Program_Error with "missing stream stop";
                     exception
                        when Status.Stream_Stopped =>
                           null;
                     end;
                     Status.Close (Stream);
                  end if;
                  Payload (Held, 1);
               end;
            elsif Scenario = "c2:job-ok" then
               for Duration of
                 Duration_Array'
                   [0, 1, -1, 123456789, Interfaces.Integer_64'First, Interfaces.Integer_64'Last]
               loop
                  Extend (Object, Duration);
               end loop;
               Extend (Object, 1, 0, Interfaces.Unsigned_32'Last);
               Extend (Object, -1, Interfaces.Unsigned_32'Last, 0);
               Extend (Object, 123456789);
               Extend (Object, 123456789);
               C2.Finalize_Job (Object);
               Verify (C2.Wait_Job_Status (Object, 3_000) = C2.Complete);
               Extend (Object, 1);
               C2.Cancel_Remaining_Job_Intervals (Object);
               Extend (Object, 1);
               C2.Close (VA);
               C2.Close (Parent);
               Extend (Object, 1);
            elsif Scenario = "c2:extension-feedback" then
               --  Active Job with no status registration: return needs no callback.
               C2.Finalize_Job (Object);
               Extend (Object, 1);
               declare
                  Result : constant C2.Cancel_Result := C2.Cancel_Job (Object);
               begin
                  Verify (C2.Cancelled (Result));
               end;
               Verify (C2.Wait_Job_Status (Object, 3_000) = C2.Complete);
            elsif Scenario = "c2:cancel-false" or Scenario = "c2:cancel-throw" then
               declare
                  Before : constant Interfaces.C.unsigned := Calls.all;
               begin
                  begin
                     declare
                        Result : constant C2.Cancel_Result := C2.Cancel_Job (Object);
                     begin
                        Verify (not C2.Cancelled (Result));
                     end;
                  exception
                     when AMS.MEL.Provider_Error =>
                        Verify (Scenario = "c2:cancel-throw");
                  end;
                  begin
                     C2.Extend_Job_Event (Object, 0, 0, 0);
                     raise Program_Error with "missing lifecycle rejection";
                  exception
                     when AMS.MEL.Provider_Error =>
                        Verify (Calls.all = Before);
                  end;
               end;
            else
               for Attempt in 1 .. 2 loop
                  declare
                     Before : constant Interfaces.C.unsigned := Calls.all;
                  begin
                     begin
                        C2.Extend_Job_Event
                          (Object, Interfaces.Unsigned_32'Last, 0, Interfaces.Integer_64'First);
                        raise Program_Error with "missing provider error";
                     exception
                        when Error : AMS.MEL.Provider_Error =>
                           Recorded
                             (Before, Interfaces.Unsigned_32'Last, 0, Interfaces.Integer_64'First);
                           if Scenario = "c2:extend-throw" then
                              declare
                                 Message : constant String :=
                                   Ada.Exceptions.Exception_Message (Error);
                              begin
                                 --  GNAT exception occurrences have a bounded message
                                 --  store (200 bytes on the tested runtimes), separate
                                 --  from the native 512-byte UTF-8-safe diagnostic.
                                 Verify (Message'Length in 2 .. 510 and Message'Length mod 2 = 0);
                                 for I in 0 .. Message'Length / 2 - 1 loop
                                    Verify (Message (2 * I + 1 .. 2 * I + 2) = "µ");
                                 end loop;
                              end;
                           end if;
                     end;
                     Snapshot (Object);
                  end;
               end loop;
               C2.Finalize_Job (Object);
               Verify (C2.Wait_Job_Status (Object, 3_000) = C2.Complete);
            end if;
            if C2.Is_Open (Object) then
               Snapshot (Object);
               C2.Close (Object);
            end if;
            declare
               Before : constant Interfaces.C.unsigned := Calls.all;
            begin
               begin
                  C2.Extend_Job_Event (Object, 0, 0, 0);
                  raise Program_Error with "missing closed owner rejection";
               exception
                  when AMS.MEL.Provider_Error =>
                     Verify (Calls.all = Before);
               end;
            end;
         end;
         C2.Close (VA);
         C2.Close (Parent);
      end;
   end Run;
begin
   CS.Free (Path);
   for Iteration in 1 .. 50 loop
      Run ("c2:job-ok");
      Run ("c2:extension-feedback");
      Run
        ((if Iteration mod 2 = 0 then "c2:extension-feedback" else "c2:extension-reference"), True);
   end loop;
   Run ("c2:extend-standard");
   Run ("c2:extend-unknown");
   Run ("c2:extend-alloc");
   Run ("c2:extend-throw");
   Run ("c2:cancel-false");
   Run ("c2:cancel-throw");
   Verify (DL_Close (Library) = 0);
   Ada.Text_IO.Put_Line ("PASS: safe Ada RF event extension active/feedback focused repeat 50/50");
end AMS_MEL_RF_Job_Extension;
