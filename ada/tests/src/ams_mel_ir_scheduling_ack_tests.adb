with Ada.Exceptions;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Scheduling;
with AMS.MEL.IR.Scheduling.Metadata;
with AMS.MEL.IR.Scheduling.Metadata.Testing;
with Interfaces;
with Interfaces.C;
with System;

package body AMS_MEL_IR_Scheduling_Ack_Tests is
   package Sch renames AMS.MEL.IR.Scheduling;
   package M renames AMS.MEL.IR.Scheduling.Metadata;
   package V renames AMS.MEL.IR.Channel;
   package IC renames Interfaces.C;
   use type Interfaces.Unsigned_32;
   use type V.Outcome;
   use type AMS.MEL.IR.Counter;
   use type IC.int;
   use type IC.unsigned;
   use type System.Address;

   Zero                          : constant AMS.MEL.IR.UUID := [others => 0];
   Config                        : constant Sch.Scheduling_Config :=
     Sch.Create_Config
       (AMS.MEL.IR.Create_UCI_ID (Zero, "ack channel"),
        AMS.MEL.IR.Create_UCI_ID (Zero, "platform"),
        AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock"));
   function Dlopen (Path : IC.char_array; Flags : IC.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function Dlsym (Handle : System.Address; Name : IC.char_array) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   function Dlclose (Handle : System.Address) return IC.int
   with Import, Convention => C, External_Name => "dlclose";
   type Deliver_Function is
     access function (Command, Request : Interfaces.Unsigned_32; Mode : IC.unsigned) return IC.int
   with Convention => C;
   type Calls_Function is access function return IC.unsigned with Convention => C;
   type Probe_Function is access function (Operation : IC.unsigned; Value : IC.size_t) return IC.int
   with Convention => C;
   function To_Deliver is new Ada.Unchecked_Conversion (System.Address, Deliver_Function);
   function To_Calls is new Ada.Unchecked_Conversion (System.Address, Calls_Function);
   function To_Probe is new Ada.Unchecked_Conversion (System.Address, Probe_Function);
   Deliver                       : Deliver_Function;
   Registrations, Specific_Calls : Calls_Function;

   procedure Require (Condition : Boolean; Message : String) is
   begin
      if not Condition then
         raise Program_Error with "Ada ExecuteTaskAck: " & Message;
      end if;
   end Require;

   function Probe (Operation : Natural; Value : Natural := 0) return Integer is
      Call : constant Probe_Function :=
        To_Probe
          (Dlsym (System.Null_Address, IC.To_C ("ams_mel_test_scheduling_execute_task_ack_probe")));
   begin
      Require (Call /= null, "test-facade probe");
      return Integer (Call (IC.unsigned (Operation), IC.size_t (Value)));
   end Probe;

   procedure Send (Command, Request : Interfaces.Unsigned_32; Null_Payload : Boolean := False) is
   begin
      Require
        (Deliver (Command, Request, (if Null_Payload then 1 else 0)) = 1,
         "manual asynchronous provider delivery");
   end Send;

   procedure Check (Value : M.Execute_Task_Ack; Command, Request : Interfaces.Unsigned_32) is
   begin
      Require (Value.Command_ID = Command, "full-width command identifier");
      Require (Value.Request_ID = Request, "independent full-width request identifier");
   end Check;

   procedure Next (Stream : M.Execute_Task_Ack_Stream; Command, Request : Interfaces.Unsigned_32) is
   begin
      Check (M.Receive_Execute_Task_Ack (Stream), Command, Request);
   end Next;

   procedure Counters (Stream : M.Execute_Task_Ack_Stream; Received, Dropped, Malformed : Natural)
   is
      C : constant M.Metadata_Counters := M.Execute_Task_Ack_Counters (Stream);
   begin
      Require (C.Events_Received = AMS.MEL.IR.Counter (Received), "received counter");
      Require (C.Events_Dropped_Queue_Full = AMS.MEL.IR.Counter (Dropped), "full counter");
      Require (C.Malformed_Or_Unsupported = AMS.MEL.IR.Counter (Malformed), "malformed counter");
   end Counters;

   procedure Expect_Empty (Stream : M.Execute_Task_Ack_Stream; Stopped : Boolean := False) is
   begin
      declare
         Value : constant M.Execute_Task_Ack := M.Receive_Execute_Task_Ack (Stream, 1);
      begin
         raise Program_Error with "empty queue returned value" & Value.Command_ID'Image;
      end;
   exception
      when AMS.MEL.IR.Timeout_Error =>
         Require (not Stopped, "expected stopped, not timeout");
      when AMS.MEL.IR.Stream_Stopped =>
         Require (Stopped, "expected timeout, not stopped");
   end Expect_Empty;

   procedure One_Shot (Channel : Sch.Scheduling_Channel) is
      Before : constant IC.unsigned := Registrations.all;
   begin
      begin
         declare
            Again : M.Execute_Task_Ack_Stream := M.Open_Execute_Task_Ack (Channel, 2);
         begin
            M.Close (Again);
            raise Program_Error with "second registration accepted";
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      Require (Registrations.all = Before, "one-shot refused before provider");
   end One_Shot;

   procedure Fidelity (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-ack-sync");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      Stream  : M.Execute_Task_Ack_Stream := M.Open_Execute_Task_Ack (Channel, 16);
      Saved   : M.Execute_Task_Ack;
   begin
      One_Shot (Channel);
      Counters (Stream, 4, 0, 0);
      Next (Stream, 0, 0);
      Next (Stream, 16#FFFF_FFFF#, 16#FFFF_FFFF#);
      Next (Stream, 16#8000_0000#, 16#8000_0001#);
      Next (Stream, 7, 16#FFFF_FFF0#);
      Sch.Enable (Channel);
      --  The mock overwrites/destroys each payload immediately after callback return.
      Send (16#1234_5678#, 16#9ABC_DEF0#);
      Send (16#1234_5678#, 16#9ABC_DEF0#);
      Saved := M.Receive_Execute_Task_Ack (Stream);
      Next (Stream, 16#1234_5678#, 16#9ABC_DEF0#);
      Send (16#FFFF_0001#, 16#0001_FFFE#);
      AMS.MEL.Close (Parent);
      Sch.Close (Channel);
      Next (Stream, 16#FFFF_0001#, 16#0001_FFFE#);
      Expect_Empty (Stream, Stopped => True);
      M.Close (Stream);
      M.Close (Stream);
      Check (Saved, 16#1234_5678#, 16#9ABC_DEF0#);
   end Fidelity;

   procedure Registration_Failure
     (Provider_Path, Scenario, Message : String; Unsupported : Boolean := False)
   is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      Before  : constant IC.unsigned := Registrations.all;
   begin
      begin
         declare
            Stream : M.Execute_Task_Ack_Stream := M.Open_Execute_Task_Ack (Channel, 4);
         begin
            M.Close (Stream);
            raise Program_Error with "registration unexpectedly succeeded";
         end;
      exception
         when E : M.Not_Supported =>
            Require (Unsupported, "distinct NotSupported exception");
            Require (Ada.Exceptions.Exception_Message (E) = Message, "unsupported diagnostic");
         when E : AMS.MEL.Provider_Error =>
            Require (not Unsupported, "NotSupported incorrectly reported as Provider_Error");
            Require (Ada.Exceptions.Exception_Message (E) = Message, "registration diagnostic");
      end;
      Require (Registrations.all = Before + 1, "one provider attempt after refusal");
      One_Shot (Channel);
      Sch.Enable (Channel);
      Sch.Close (Channel);
      AMS.MEL.Close (Parent);
   end Registration_Failure;

   procedure Queue_Recovery (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-ack-basic");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      Copies  : Integer;
   begin
      Sch.Enable (Channel);
      declare
         Stream : constant M.Execute_Task_Ack_Stream := M.Open_Execute_Task_Ack (Channel, 1);
      begin
         Send (16#8000_0001#, 16#F000_0002#);
         Copies := Probe (4);
         Require (Probe (5, 1) = 1, "arm allocation failure");
         Send (3, 4);
         Send (0, 0, Null_Payload => True);
         Counters (Stream, 3, 1, 1);
         Require (Probe (4) = Copies and then Probe (6) = 1, "full queue avoids allocation");
         Next (Stream, 16#8000_0001#, 16#F000_0002#);
         Send (5, 6);
         Counters (Stream, 4, 1, 2);
         Expect_Empty (Stream);
         Send (5, 6);
         Next (Stream, 5, 6);
         Counters (Stream, 5, 1, 2);
      --  Non-raising finalization deactivates consumption without unregistering.
      end;
      One_Shot (Channel);
      Send (7, 8);
      Sch.Close (Channel);
      AMS.MEL.Close (Parent);
   end Queue_Recovery;

   procedure Common_Independence (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-ada-common");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      View    : V.View := Sch.As_Channel (Channel);
      Before  : constant IC.unsigned := Registrations.all;
   begin
      Sch.Enable (Channel);
      declare
         Stream : M.Execute_Task_Ack_Stream := M.Open_Execute_Task_Ack (Channel, 2);
      begin
         M.Close (Stream);
         raise Program_Error with "non-ack provider accepted registration";
      end;
   exception
      when M.Not_Supported =>
         Require (Registrations.all = Before + 1, "only explicit callback attempt");
         declare
            Request : V.Return_Request := V.Send_Keep_Alive (View);
            Result  : constant V.Return_Result := V.Wait (Request, 15_000);
         begin
            Require (V.Status (Result) = V.Success, "common request unaffected by refusal");
            V.Close (Request);
         end;
         V.Close (View);
         Sch.Close (Channel);
         AMS.MEL.Close (Parent);
   end Common_Independence;

   procedure Detach_Failure (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-ack-detach-fail");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      Stream  : M.Execute_Task_Ack_Stream := M.Open_Execute_Task_Ack (Channel, 4);
   begin
      begin
         Sch.Close (Channel);
         raise Program_Error with "detach failure accepted";
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      Require (Sch.Is_Open (Channel), "uncertain provider graph retained");
      Next (Stream, 16#1111_1111#, 16#2222_2222#);
      Next (Stream, 16#3333_3333#, 16#4444_4444#);
      begin
         Next (Stream, 0, 0);
         raise Program_Error with "missing terminal cleanup failure";
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      AMS.MEL.Close (Parent);
      Sch.Close (Channel);
      M.Close (Stream);
   end Detach_Failure;

   procedure Run (Provider_Path : String) is
      Pin    : constant System.Address := Dlopen (IC.To_C (Provider_Path), 2);
      Before : IC.unsigned;
   begin
      Require (Pin /= System.Null_Address, "provider pin");
      Deliver := To_Deliver (Dlsym (Pin, IC.To_C ("mock_scheduling_ack_deliver")));
      Registrations := To_Calls (Dlsym (Pin, IC.To_C ("mock_scheduling_ack_registrations")));
      Specific_Calls := To_Calls (Dlsym (Pin, IC.To_C ("mock_scheduling_specific_calls")));
      Require
        (Deliver /= null and then Registrations /= null and then Specific_Calls /= null,
         "provider test hooks");
      Before := Specific_Calls.all;
      M.Testing.Check_Representation;
      Fidelity (Provider_Path);
      Registration_Failure
        (Provider_Path, "sched-basic", "provider does not support ExecuteTaskAck callback", True);
      Registration_Failure
        (Provider_Path, "sched-ack-register-fail", "ExecuteTaskAck callback registration failed");
      Registration_Failure
        (Provider_Path,
         "sched-ack-register-not-implemented",
         "ExecuteTaskAck callback registration failed");
      Registration_Failure
        (Provider_Path,
         "sched-ack-register-unknown",
         "provider returned an unknown ExecuteTaskAck registration result");
      Registration_Failure
        (Provider_Path,
         "sched-ack-register-throw",
         "provider exception during ExecuteTaskAck registration");
      Queue_Recovery (Provider_Path);
      Common_Independence (Provider_Path);
      Detach_Failure (Provider_Path);
      Require (Specific_Calls.all = Before, "ten unsupported Scheduling methods uninvoked");
      Require (Dlclose (Pin) = 0, "provider unpin");
      Ada.Text_IO.Put_Line ("PASS: Ada Scheduling ExecuteTaskAck contract");
   end Run;
end AMS_MEL_IR_Scheduling_Ack_Tests;
