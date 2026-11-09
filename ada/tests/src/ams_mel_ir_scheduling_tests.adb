with Ada.Exceptions;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Scheduling;
with Interfaces;
with Interfaces.C;
with System;

--  IRMEL-ADA-011: safe Ada SchedulingChannel foundation contract. Only Open,
--  Enable, Capabilities, As_Channel and Close exist; no Scheduling-specific
--  exec, send or metadata registration is exercised, and the mock's counter of
--  such calls is required to stay zero throughout.

package body AMS_MEL_IR_Scheduling_Tests is
   package Sch renames AMS.MEL.IR.Scheduling;
   package V renames AMS.MEL.IR.Channel;
   package IC renames Interfaces.C;
   use type V.Channel_Type;
   use type V.Metadata_Capability;
   use type V.Outcome;
   use type V.Command_Return;
   use type V.Command_ID;
   use type V.Comms_Request_ID;
   use type Interfaces.Unsigned_8;
   use type Interfaces.Unsigned_32;
   use type V.Comms_Test_Report;
   use type IC.unsigned;
   use type System.Address;

   High_Channel : constant V.Comms_Channel_ID := 16#F000_0002#;
   High_Command : constant V.Command_ID := 16#8000_0001#;
   High_Request : constant V.Comms_Request_ID := 16#E000_0003#;

   function Make_UUID (Base : Interfaces.Unsigned_8) return AMS.MEL.IR.UUID is
      Result : AMS.MEL.IR.UUID;
   begin
      for I in Result'Range loop
         Result (I) := Base + Interfaces.Unsigned_8 (I - Result'First);
      end loop;
      return Result;
   end Make_UUID;

   --  Exactly the identities and location the "sched-config" mock verifies.
   Channel_ID : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID
       (Make_UUID (16#A0#),
        --  Exact UTF-8 bytes of U+03BC, as the C11 test and the mock expect.
        "IR scheduling " & Character'Val (16#CE#) & Character'Val (16#BC#) & " channel");
   Platform   : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Make_UUID (16#F0#), "test platform");
   Location   : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (1.25, -2.5, 3.75, "station-1", "mock-aircraft");
   Config     : constant Sch.Scheduling_Config :=
     Sch.Create_Config (Channel_ID, Platform, Location);

   procedure Require (Condition : Boolean; Message : String) is
   begin
      if not Condition then
         raise Program_Error with "Ada Scheduling: " & Message;
      end if;
   end Require;

   --  Negative control: the mock's process-wide count of Scheduling-specific
   --  exec/send/registration calls. The counter lives in provider code that
   --  unloads with each Session, so Run pins the mock for its whole duration.
   function Dlopen (Path : IC.char_array; Flags : IC.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function Dlsym (Handle : System.Address; Name : IC.char_array) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   function Dlclose (Handle : System.Address) return IC.int
   with Import, Convention => C, External_Name => "dlclose";
   type Calls_Function is access function return IC.unsigned with Convention => C;
   function To_Calls is new Ada.Unchecked_Conversion (System.Address, Calls_Function);

   procedure Check_Capability (Value : V.Channel_Capability; Depth : Interfaces.Unsigned_32) is
   begin
      Require (V.Channel_Type_Count (Value) = 2, "channel type count");
      Require (V.Channel_Type_At (Value, 1) = V.Scheduling, "Scheduling channel type");
      Require (V.Channel_Type_At (Value, 1)'Enum_Rep = 3, "Scheduling numeric value");
      Require (V.Channel_Type_At (Value, 2) = V.Reserved_1, "second channel type");
      Require (V.Task_Schedule_Depth (Value) = Depth, "TaskScheduleDepth");
      Require (V.Metadata_Capability_Count (Value) = 7, "metadata count");
      Require
        (V.Metadata_Capability_At (Value, 1) = V.Task_Executing_Rep
         and then V.Metadata_Capability_At (Value, 2) = V.Execute_Task_Ack
         and then V.Metadata_Capability_At (Value, 3) = V.Sched_Created_Rep
         and then V.Metadata_Capability_At (Value, 4) = V.Channel_Comms_Test_Rep
         and then V.Metadata_Capability_At (Value, 5) = V.Update_Track_List_Response
         and then V.Metadata_Capability_At (Value, 6) = V.Task_Events
         and then V.Metadata_Capability_At (Value, 7) = V.Scan_Performance_Report,
         "metadata capabilities");
      Require
        (V.Width (Value) = 1920 and then V.Height (Value) = 1080 and then V.Bit_Depth (Value) = 12,
         "shared capability scalars");
      Require
        (AMS.MEL.IR.Descriptive_Label (V.Channel_ID (Value)) = "channel-α",
         "capability channel label");
      Require
        (V.Image_Band_Count (Value) = 2 and then V.Nav_Frame_Count (Value) = 2,
         "capability collections");
   end Check_Capability;

   --  Complete Config propagation (verified by the mock), Attached capability,
   --  explicit and idempotent Enable, Enabled capability, idempotent Close,
   --  and a snapshot that outlives owner, Session and provider unload.
   procedure Test_Open_Enable_Close (Provider_Path : String) is
      Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-config");
      Channel  : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      Attached : constant V.Channel_Capability := Sch.Capabilities (Channel);
   begin
      Require (Sch.Is_Open (Channel), "Open");
      Check_Capability (Attached, 23);
      Sch.Enable (Channel);
      Sch.Enable (Channel);
      declare
         Enabled : constant V.Channel_Capability := Sch.Capabilities (Channel);
      begin
         Check_Capability (Enabled, 23);
         Sch.Close (Channel);
         Require (not Sch.Is_Open (Channel), "Close did not clear the owner");
         Sch.Close (Channel);
         AMS.MEL.Close (Parent);
         Check_Capability (Enabled, 23);
      end;
      Check_Capability (Attached, 23);
   end Test_Open_Enable_Close;

   --  TaskScheduleDepth zero is a valid application-managed mode, not absence.
   procedure Test_Depth (Provider_Path : String; Scenario : String; Depth : Interfaces.Unsigned_32)
   is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
   begin
      Check_Capability (Sch.Capabilities (Channel), Depth);
      Sch.Enable (Channel);
      Check_Capability (Sch.Capabilities (Channel), Depth);
      Sch.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Depth;

   procedure Test_Parent_First (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-basic");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
   begin
      Sch.Enable (Channel);
      AMS.MEL.Close (Parent);
      Require (not AMS.MEL.Is_Open (Parent), "parent remained open");
      Check_Capability (Sch.Capabilities (Channel), 23);
      Sch.Close (Channel);
   end Test_Parent_First;

   --  Scope-only finalization closes an un-closed owner without raising.
   procedure Test_Finalization (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-basic");
   begin
      declare
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      begin
         Sch.Enable (Channel);
      end;
      AMS.MEL.Close (Parent);
   end Test_Finalization;

   procedure Test_Enable_Failure (Provider_Path : String; Scenario : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
   begin
      for Attempt in 1 .. 2 loop
         begin
            Sch.Enable (Channel);
            raise Program_Error with "Ada Scheduling enable failure accepted" & Attempt'Image;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
      end loop;
      begin
         declare
            Unexpected : constant V.Channel_Capability := Sch.Capabilities (Channel);
         begin
            raise Program_Error
              with "failed Scheduling capability returned" & V.Width (Unexpected)'Image;
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      Sch.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Enable_Failure;

   --  Disable failure after a proven detach consumes the handle but raises.
   procedure Test_Disable_Failure (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-disable-fail");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
   begin
      Sch.Enable (Channel);
      begin
         Sch.Close (Channel);
         raise Program_Error with "Ada Scheduling disable failure accepted";
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      Require (not Sch.Is_Open (Channel), "disable failure retained the handle");
      AMS.MEL.Close (Parent);
   end Test_Disable_Failure;

   --  Detach failure retains the owner; a later Close retries successfully.
   procedure Test_Detach_Failure (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-detach-fail");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
   begin
      begin
         Sch.Close (Channel);
         raise Program_Error with "Ada Scheduling detach failure accepted";
      exception
         when Error : AMS.MEL.Provider_Error =>
            Require
              (Ada.Exceptions.Exception_Message (Error)
               = "Scheduling detach failed; provider state retained",
               "detach diagnostic");
      end;
      Require (Sch.Is_Open (Channel), "detach failure dropped the handle");
      AMS.MEL.Close (Parent);
      Sch.Close (Channel);
      Require (not Sch.Is_Open (Channel), "retry Close");
   end Test_Detach_Failure;

   procedure Test_Open_Failure (Provider_Path : String; Scenario : String; Text : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
   begin
      begin
         declare
            Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
         begin
            Sch.Close (Channel);
         end;
         raise Program_Error with "Ada Scheduling open failure accepted: " & Scenario;
      exception
         when Error : AMS.MEL.Provider_Error =>
            Require (Ada.Exceptions.Exception_Message (Error) = Text, Scenario & " diagnostic");
      end;
      --  Conditional absence is not fatal: the Session stays open and usable.
      Require (AMS.MEL.Is_Open (Parent), Scenario & " closed the Session");
      AMS.MEL.Close (Parent);
   end Test_Open_Failure;

   --  Weak view: typed and weak capabilities agree, KeepAlive and CommsTest
   --  (with cached repeated Wait) succeed while Attached and Enabled, view
   --  Close leaves the typed owner usable, and after typed Close an existing
   --  view refuses every operation.
   procedure Test_Common_Channel (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-ada-common");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      View    : V.View := Sch.As_Channel (Channel);
   begin
      for Enabled in Boolean loop
         if Enabled then
            Sch.Enable (Channel);
         end if;
         Check_Capability (V.Capabilities (View), 23);
         Check_Capability (Sch.Capabilities (Channel), 23);
         declare
            Request : V.Return_Request := V.Send_Keep_Alive (View);
            Result  : constant V.Return_Result := V.Wait (Request, 5_000);
         begin
            Require
              (V.Status (Result) = V.Success
               and then V.Value (Result) = V.Return_Success
               and then V.Value (V.Wait (Request, 0)) = V.Return_Success,
               "KeepAlive");
            V.Close (Request);
         end;
         declare
            Request : V.Comms_Request :=
              V.Submit_Comms_Test (View, High_Channel, High_Command, High_Request);
            Report  : constant V.Comms_Test_Report := V.Report (V.Wait (Request, 5_000));
         begin
            Require
              (Report.Command_ID = High_Command
               and then Report.Request_ID = High_Request
               and then V.Report (V.Wait (Request, 0)) = Report,
               "CommsTest");
            V.Close (Request);
         end;
      end loop;
      V.Close (View);
      V.Close (View);
      Require (not V.Is_Open (View) and then Sch.Is_Open (Channel), "view Close");
      Check_Capability (Sch.Capabilities (Channel), 23);
      declare
         Later : V.View := Sch.As_Channel (Channel);
      begin
         Sch.Close (Channel);
         begin
            declare
               Unexpected : V.Return_Request := V.Send_Keep_Alive (Later);
            begin
               V.Close (Unexpected);
               raise Program_Error with "expired Scheduling view accepted KeepAlive";
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         begin
            declare
               Unexpected : constant V.Channel_Capability := V.Capabilities (Later);
            begin
               raise Program_Error
                 with "expired Scheduling view returned capabilities" & V.Width (Unexpected)'Image;
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         V.Close (Later);
      end;
      --  As_Channel on a closed owner raises.
      begin
         declare
            Unexpected : constant V.View := Sch.As_Channel (Channel);
         begin
            raise Program_Error
              with "As_Channel on closed Scheduling owner" & V.Is_Open (Unexpected)'Image;
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      AMS.MEL.Close (Parent);
   end Test_Common_Channel;

   --  Operations on a closed owner raise rather than reaching the provider.
   procedure Test_Closed_Owner (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-basic");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
   begin
      Sch.Close (Channel);
      begin
         Sch.Enable (Channel);
         raise Program_Error with "Enable on closed Scheduling owner accepted";
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      AMS.MEL.Close (Parent);
      begin
         declare
            Orphan : constant Sch.Scheduling_Channel := Sch.Open (Parent, Config);
         begin
            raise Program_Error with "Open on closed Session accepted" & Sch.Is_Open (Orphan)'Image;
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
   end Test_Closed_Owner;

   procedure Run (Provider_Path : String) is
      use type IC.int;
      --  RTLD_NOW. The pin keeps the mock (and its counter) loaded across all
      --  scenarios; it is released before Run returns.
      Pin    : constant System.Address := Dlopen (IC.To_C (Provider_Path), 2);
      Calls  : Calls_Function;
      Before : IC.unsigned;
   begin
      Require (Pin /= System.Null_Address, "mock pin");
      Calls := To_Calls (Dlsym (Pin, IC.To_C ("mock_scheduling_specific_calls")));
      Require (Calls /= null, "mock negative-control probe");
      Before := Calls.all;
      Test_Open_Enable_Close (Provider_Path);
      Test_Depth (Provider_Path, "sched-depth-zero", 0);
      Test_Depth (Provider_Path, "sched-depth-max", Interfaces.Unsigned_32'Last);
      Test_Parent_First (Provider_Path);
      Test_Finalization (Provider_Path);
      Test_Enable_Failure (Provider_Path, "sched-enable-fail");
      Test_Enable_Failure (Provider_Path, "sched-enable-throw");
      Test_Disable_Failure (Provider_Path);
      Test_Detach_Failure (Provider_Path);
      Test_Open_Failure
        (Provider_Path, "sched-not-advertised", "provider does not advertise Scheduling");
      Test_Open_Failure (Provider_Path, "sched-attach-null", "attachChannel returned null");
      Test_Open_Failure
        (Provider_Path, "sched-wrong-concrete", "attached channel is not a SchedulingChannel");
      Test_Open_Failure
        (Provider_Path, "sched-capability-wrong", "Scheduling channel capability omits Scheduling");
      Test_Open_Failure
        (Provider_Path, "sched-attach-throw", "provider exception during Scheduling open");
      Test_Common_Channel (Provider_Path);
      Test_Closed_Owner (Provider_Path);
      --  Negative control: no Scheduling-specific exec, send or registration.
      Require (Calls.all = Before, "Scheduling-specific call observed");
      Require (Dlclose (Pin) = 0, "mock pin release");
      Ada.Text_IO.Put_Line ("PASS: Ada IR Scheduling channel foundation contract");
   end Run;
end AMS_MEL_IR_Scheduling_Tests;
