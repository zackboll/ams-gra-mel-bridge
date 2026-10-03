with Ada.Command_Line;
with Ada.Environment_Variables;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with AMS.MEL.RF.C2;
with AMS.MEL.RF.C2.Interval_Status;
with AMS.MEL.IR;
with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;

procedure AMS_MEL_RF_Interval_Status is
   package C2 renames AMS.MEL.RF.C2;
   package Status renames C2.Interval_Status;
   package CS renames Interfaces.C.Strings;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Unsigned_64;
   use type Interfaces.Unsigned_8;
   use type Interfaces.Integer_64;
   use type Interfaces.C.unsigned;
   use type Interfaces.C.int;
   use type C2.Request_Outcome;
   use type Status.Completion_Status;
   use type Status.Log_Trigger;
   use type System.Address;
   function DL_Open (Path : CS.chars_ptr; Flags : Interfaces.C.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function DL_Sym (Handle : System.Address; Name : CS.chars_ptr) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   function DL_Close (Handle : System.Address) return Interfaces.C.int
   with Import, Convention => C, External_Name => "dlclose";
   type Emit_Access is
     access function
       (Completion, Trigger, Kind, ID : Interfaces.C.unsigned) return Interfaces.C.unsigned
   with Convention => C;
   function To_Emit is new Ada.Unchecked_Conversion (System.Address, Emit_Access);
   type Mode_Access is access function (Index : Interfaces.C.unsigned) return Interfaces.C.unsigned
   with Convention => C;
   function To_Mode is new Ada.Unchecked_Conversion (System.Address, Mode_Access);
   procedure Verify (Condition : Boolean) is
   begin
      if not Condition then
         raise Program_Error with "RF interval status assertion";
      end if;
   end Verify;
   Provider  : constant String :=
     Ada.Environment_Variables.Value ("AMS_MEL_TEST_PROVIDER_DIR") & "/libmock_rf_provider.so";
   Path      : CS.chars_ptr := CS.New_String (Provider);
   Name      : CS.chars_ptr := CS.New_String ("mock_rf_status_emit");
   Library   : constant System.Address := DL_Open (Path, 2);
   Emit      : constant Emit_Access := To_Emit (DL_Sym (Library, Name));
   Mode_Name : CS.chars_ptr := CS.New_String ("mock_rf_status_mode");
   Mode_At   : constant Mode_Access := To_Mode (DL_Sym (Library, Mode_Name));
   procedure Send (ID : Interfaces.C.unsigned := 16#FEDC_BA98#) is
   begin
      Verify (Emit (2, 7, 0, ID) = 1);
   end Send;
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
   procedure Rich (Event : Status.Status_Event) is
   begin
      Verify (Status.Interval_ID (Event) = 16#FEDC_BA98#);
      Verify (Status.Completion (Event) = Status.Failed_Interrupted);
      Verify (Status.Log_Count (Event) = 3 and Status.Activity_ID_Length (Event) = 37);
      Verify (Status.Event_ID (Status.Log_At (Event, 1)) = 0);
      Verify (Status.Event_ID (Status.Log_At (Event, 2)) = 17);
      Verify (Status.Event_ID (Status.Log_At (Event, 3)) = Interfaces.Unsigned_32'Last);
      Verify (Status.Trigger (Status.Log_At (Event, 3)) = Status.Event_Type_Not_Supported);
      Verify (Status.Time_Seconds (Status.Log_At (Event, 1)) = -124);
      Verify (Status.Time_Fractional_Femtoseconds (Status.Log_At (Event, 1)) = 112);
      Verify (Status.Time_Seconds (Status.Log_At (Event, 2)) = -125);
      Verify (Status.Time_Fractional_Femtoseconds (Status.Log_At (Event, 2)) = 113);
      Verify (Status.Time_Seconds (Status.Log_At (Event, 3)) = -123);
      Verify (Status.Time_Fractional_Femtoseconds (Status.Log_At (Event, 3)) = 111);
      Verify (Status.Trigger (Status.Log_At (Event, 1)) = Status.Event_Extended);
      Verify (Status.Trigger (Status.Log_At (Event, 2)) = Status.Event_Triggered);
      for I in 1 .. 37 loop
         Verify
           (Status.Activity_ID_Byte (Event, I)
            = (if I = 2 then 16#80# elsif I = 3 then 16#FF# else Interfaces.Unsigned_8 (I - 1)));
      end loop;
   end Rich;
begin
   Verify (Ada.Command_Line.Argument_Count = 0);
   Verify (Library /= System.Null_Address);
   CS.Free (Path);
   CS.Free (Name);
   CS.Free (Mode_Name);
   for Run in 1 .. 50 loop
      declare
         Parent : C2.C2_MEL := C2.Open (Provider, "c2:status-reference");
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
               Object    : C2.Job := C2.Claim (JR);
               Stream    : Status.Stream := Status.Open (Object, 2, 3, 37);
               Held      : Status.Status_Event;
               Intervals : C2.RX_Job_Interval_List;
               Interval  : C2.RX_Job_Interval_Config := C2.Create_RX_Job_Interval (1, -1);
            begin
               C2.Close (JR);
               C2.Close (VA);
               C2.Close (Parent);
               for Mode in C2.Interval_Status_Enable loop
                  C2.Set_Interval_Status_Enable (Interval, Mode);
                  C2.Append_Job_Interval (Intervals, Interval);
               end loop;
               C2.Add_RX_Job_Intervals (Object, Intervals);
               Verify (Mode_At (0) = 0 and Mode_At (1) = 1 and Mode_At (2) = 2);
               Send;
               Held := Status.Receive_Event (Stream, 0);
               Rich (Held);
               for Value in Status.Completion_Status loop
                  Verify
                    (Emit
                       (Interfaces.C.unsigned (Status.Completion_Status'Enum_Rep (Value)), 0, 1, 0)
                     = 1);
                  declare
                     Event : constant Status.Status_Event := Status.Receive_Event (Stream, 0);
                  begin
                     Verify (Status.Completion (Event) = Value);
                     Verify
                       (Status.Activity_ID_Length (Event) = 0 and Status.Log_Count (Event) = 0);
                  end;
               end loop;
               for Value in Status.Log_Trigger loop
                  Verify
                    (Emit (2, Interfaces.C.unsigned (Status.Log_Trigger'Enum_Rep (Value)), 0, 0)
                     = 1);
                  declare
                     Event : constant Status.Status_Event := Status.Receive_Event (Stream, 0);
                  begin
                     Verify (Status.Trigger (Status.Log_At (Event, 3)) = Value);
                  end;
               end loop;
               Verify (Emit (2, 7, 4, 0) = 1);
               declare
                  Event : constant Status.Status_Event := Status.Receive_Event (Stream, 0);
               begin
                  Verify (Status.Time_Seconds (Status.Log_At (Event, 1)) = 0);
                  Verify (Status.Time_Fractional_Femtoseconds (Status.Log_At (Event, 1)) = -112);
               end;
               Send (101);
               Send (102);
               Send (103);
               Verify (Status.Interval_ID (Status.Receive_Event (Stream, 0)) = 101);
               Verify (Status.Interval_ID (Status.Receive_Event (Stream, 0)) = 102);
               Verify (Status.Statistics (Stream).Queue_Full_Drops = 1);
               begin
                  Held := Status.Receive_Event (Stream, 1);
                  raise Program_Error with "missing timeout";
               exception
                  when C2.Timeout_Error =>
                     null;
               end;
               C2.Close (Object);
               Verify (Status.Is_Open (Stream));
               begin
                  Held := Status.Receive_Event (Stream, 0);
                  raise Program_Error with "missing stream stop";
               exception
                  when Status.Stream_Stopped =>
                     null;
               end;
               Send;
               Verify (Status.Statistics (Stream).Callbacks_After_Close = 1);
               Status.Close (Stream);
               Send;
               Rich (Held);
            end;
         end;
      end;
   end loop;
   Verify (DL_Close (Library) = 0);
   Ada.Text_IO.Put_Line ("PASS: safe Ada RF interval status focused repeat 50/50");
end AMS_MEL_RF_Interval_Status;
