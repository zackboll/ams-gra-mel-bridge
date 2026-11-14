with Ada.Exceptions;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Scheduling;
with AMS.MEL.IR.Scheduling.Metadata;
with AMS.MEL.IR.Scheduling.Metadata.Testing;
with Interfaces;
with Interfaces.C;
with System;

--  IRMEL-ADA-013 safe Ada TaskExecutingRep contract against the separate mock provider.

package body AMS_MEL_IR_Scheduling_Task_Executing_Tests is
   package Sch renames AMS.MEL.IR.Scheduling;
   package M renames AMS.MEL.IR.Scheduling.Metadata;
   package T renames AMS.MEL.IR.Scheduling.Metadata.Testing;
   package IC renames Interfaces.C;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Integer_32;
   use type Interfaces.Integer_64;
   use type M.Task_Kind;
   use type M.Task_Completion_Status;
   use type AMS.MEL.IR.Counter;
   use type IC.int;
   use type IC.unsigned;
   use type System.Address;

   Zero                                       : constant AMS.MEL.IR.UUID := [others => 0];
   Config                                     : constant Sch.Scheduling_Config :=
     Sch.Create_Config
       (AMS.MEL.IR.Create_UCI_ID (Zero, "task executing channel"),
        AMS.MEL.IR.Create_UCI_ID (Zero, "platform"),
        AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock"));
   function Dlopen (Path : IC.char_array; Flags : IC.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function Dlsym (Handle : System.Address; Name : IC.char_array) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   function Dlclose (Handle : System.Address) return IC.int
   with Import, Convention => C, External_Name => "dlclose";
   type Raw_Access is access constant T.Raw_Task_Executing;
   type Deliver_Function is access function (Value : Raw_Access; Mode : IC.unsigned) return IC.int
   with Convention => C;
   type Ack_Deliver_Function is
     access function (Command, Request : Interfaces.Unsigned_32; Mode : IC.unsigned) return IC.int
   with Convention => C;
   type Calls_Function is access function return IC.unsigned with Convention => C;
   type Probe_Function is access function (Operation : IC.unsigned; Value : IC.size_t) return IC.int
   with Convention => C;
   function To_Deliver is new Ada.Unchecked_Conversion (System.Address, Deliver_Function);
   function To_Ack_Deliver is new Ada.Unchecked_Conversion (System.Address, Ack_Deliver_Function);
   function To_Calls is new Ada.Unchecked_Conversion (System.Address, Calls_Function);
   function To_Probe is new Ada.Unchecked_Conversion (System.Address, Probe_Function);
   Deliver                                    : Deliver_Function;
   Ack_Deliver                                : Ack_Deliver_Function;
   Rep_Registrations, Ack_Registrations, Spec : Calls_Function;

   procedure Require (Condition : Boolean; Message : String) is
   begin
      if not Condition then
         raise Program_Error with "Ada TaskExecutingRep: " & Message;
      end if;
   end Require;

   function Probe (Operation : Natural; Value : Natural := 0) return Integer is
      Call : constant Probe_Function :=
        To_Probe
          (Dlsym (System.Null_Address, IC.To_C ("ams_mel_test_scheduling_task_executing_probe")));
   begin
      Require (Call /= null, "test-facade probe");
      return Integer (Call (IC.unsigned (Operation), IC.size_t (Value)));
   end Probe;

   procedure Send (Value : T.Raw_Task_Executing) is
      Held : aliased constant T.Raw_Task_Executing := Value;
   begin
      Require (Deliver (Held'Unchecked_Access, 0) = 1, "manual provider delivery");
   end Send;

   procedure Send_Null is
   begin
      Require (Deliver (null, 1) = 1, "manual null provider delivery");
   end Send_Null;

   --  Ten independent field comparisons, each with its own diagnostic.
   procedure Check (Value : M.Task_Executing_Report; Raw : T.Raw_Task_Executing) is
   begin
      Require (Value.Command_ID = Raw.Command_ID, "command identifier");
      Require (Value.Activity_ID = Raw.Activity_ID, "activity identifier");
      Require (Value.Request_ID = Raw.Request_ID, "request identifier");
      Require (M.Task_Kind'Enum_Rep (Value.Current_Task) = Raw.Task_Value, "current task");
      Require (Value.Estimated_Duration_NS = Raw.Estimated_Duration_NS, "signed duration");
      Require (Value.Track_ID = Raw.Track_ID, "signed track identifier");
      Require (Value.Previous_Activity_ID = Raw.Previous_Activity_ID, "previous activity");
      Require (Value.Previous_Request_ID = Raw.Previous_Request_ID, "previous request");
      Require (M.Task_Kind'Enum_Rep (Value.Previous_Task) = Raw.Previous_Task, "previous task");
      Require
        (M.Task_Completion_Status'Enum_Rep (Value.Previous_Task_Status) = Raw.Previous_Task_Status,
         "previous task status");
   end Check;

   procedure Next (Stream : M.Task_Executing_Stream; Raw : T.Raw_Task_Executing) is
   begin
      Check (M.Receive_Task_Executing (Stream), Raw);
   end Next;

   procedure Counters (Stream : M.Task_Executing_Stream; Received, Dropped, Malformed : Natural) is
      C : constant M.Metadata_Counters := M.Task_Executing_Counters (Stream);
   begin
      Require (C.Events_Received = AMS.MEL.IR.Counter (Received), "received counter");
      Require (C.Events_Dropped_Queue_Full = AMS.MEL.IR.Counter (Dropped), "full counter");
      Require (C.Malformed_Or_Unsupported = AMS.MEL.IR.Counter (Malformed), "malformed counter");
   end Counters;

   procedure Expect_Empty (Stream : M.Task_Executing_Stream; Stopped : Boolean := False) is
   begin
      declare
         Value : constant M.Task_Executing_Report := M.Receive_Task_Executing (Stream, 1);
      begin
         raise Program_Error with "empty queue returned value" & Value.Command_ID'Image;
      end;
   exception
      when AMS.MEL.IR.Timeout_Error =>
         Require (not Stopped, "expected stopped, not timeout");
      when AMS.MEL.IR.Stream_Stopped =>
         Require (Stopped, "expected timeout, not stopped");
   end Expect_Empty;

   function Raw
     (Command, Activity, Request, Current : Interfaces.Unsigned_32;
      Duration                            : Interfaces.Integer_64;
      Track                               : Interfaces.Integer_32;
      Previous_Activity, Previous_Request : Interfaces.Unsigned_32;
      Previous, Status                    : Interfaces.Unsigned_32) return T.Raw_Task_Executing
   is ((Command_ID            => Command,
        Activity_ID           => Activity,
        Request_ID            => Request,
        Task_Value            => Current,
        Estimated_Duration_NS => Duration,
        Track_ID              => Track,
        Previous_Activity_ID  => Previous_Activity,
        Previous_Request_ID   => Previous_Request,
        Previous_Task         => Previous,
        Previous_Task_Status  => Status));

   --  The mock's synchronous registration delivery, in arrival order.
   Sync : constant array (1 .. 4) of T.Raw_Task_Executing :=
     [Raw (0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
      Raw
        (16#FFFF_FFFF#,
         16#8000_0000#,
         16#7FFF_FFFF#,
         10,
         Interfaces.Integer_64'Last,
         Interfaces.Integer_32'Last,
         16#FFFF_FFFE#,
         16#8000_0001#,
         16,
         4),
      Raw
        (16#8000_0001#,
         16#FFFF_FFFE#,
         0,
         6,
         Interfaces.Integer_64'First,
         Interfaces.Integer_32'First,
         1,
         16#FFFF_FFFF#,
         11,
         1),
      Raw (7, 8, 9, 3, -1, -1, 10, 11, 2, 3)];

   procedure One_Shot (Channel : Sch.Scheduling_Channel) is
      Before : constant IC.unsigned := Rep_Registrations.all;
   begin
      begin
         declare
            Again : M.Task_Executing_Stream := M.Open_Task_Executing (Channel, 2);
         begin
            M.Close (Again);
            raise Program_Error with "second registration accepted";
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      Require (Rep_Registrations.all = Before, "one-shot refused before provider");
   end One_Shot;

   --  Synchronous registration delivery, every meaningful and reserved Task value in both the
   --  current and previous task, every TaskCompStatus, signed extremes, high-bit identifiers,
   --  repeated reports, payload overwrite and post-closure ownership.
   procedure Fidelity (Provider_Path : String) is
      Acks    : constant IC.unsigned := Ack_Registrations.all;
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-rep-sync");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      Stream  : M.Task_Executing_Stream := M.Open_Task_Executing (Channel, 32);
      Saved   : M.Task_Executing_Report;
      Value   : T.Raw_Task_Executing;
   begin
      One_Shot (Channel);
      Require (Ack_Registrations.all = Acks, "TaskExecutingRep never registers ExecuteTaskAck");
      Counters (Stream, 4, 0, 0);
      for Item of Sync loop
         Next (Stream, Item);
      end loop;
      Sch.Enable (Channel);
      for Kind in Interfaces.Unsigned_32 range 0 .. 16 loop
         Value :=
           Raw
             (Kind,
              not Kind,
              16#8000_0000# or Kind,
              Kind,
              -(Interfaces.Integer_64 (Kind) * 1_000_000),
              Interfaces.Integer_32 (Kind) - 8,
              Kind * 3,
              16#FFFF_0000# or Kind,
              16 - Kind,
              Kind mod 5);
         Send (Value);
         Next (Stream, Value);
      end loop;
      --  The mock overwrites/destroys each payload immediately after callback return.
      Value := Raw (16#1234_5678#, 1, 2, 9, 0, 0, 3, 4, 8, 2);
      Send (Value);
      Send (Value);
      Saved := M.Receive_Task_Executing (Stream);
      Next (Stream, Value);
      Require (Saved.Current_Task = M.IRST_Multiple_Target_Track, "named current task");
      Require (Saved.Previous_Task = M.IRST_Single_Target_Track, "named previous task");
      Require (Saved.Previous_Task_Status = M.Timeout, "previous status is not current status");
      Require (M.Is_Operational (Saved.Current_Task), "operational task");
      Require (not M.Is_Operational (M.Reserved_1), "reserved is not operational");
      Value := Raw (16#FFFF_0001#, 5, 6, 0, 0, 0, 7, 8, 0, 0);
      Send (Value);
      AMS.MEL.Close (Parent);
      Sch.Close (Channel);
      Next (Stream, Value);
      Expect_Empty (Stream, Stopped => True);
      M.Close (Stream);
      M.Close (Stream);
      Check (Saved, Raw (16#1234_5678#, 1, 2, 9, 0, 0, 3, 4, 8, 2));
   end Fidelity;

   --  The private checked conversion: every reserved value survives verbatim; the sentinel 17,
   --  any larger Task and any TaskCompStatus above 4 raise Provider_Error, never reinterpreted.
   type Raw_Values is array (Positive range <>) of Interfaces.Unsigned_32;
   Bad_Tasks    : constant Raw_Values := [17, 18, 16#8000_0000#, 16#FFFF_FFFF#];
   Bad_Statuses : constant Raw_Values := [5, 99, 16#FFFF_FFFF#];

   procedure Conversion is
      Base  : constant T.Raw_Task_Executing := Raw (1, 2, 3, 4, -5, -6, 7, 8, 9, 0);
      Value : T.Raw_Task_Executing;
      procedure Reject (Bad : T.Raw_Task_Executing; Text : String) is
      begin
         declare
            Ignored : constant M.Task_Executing_Report := T.Convert (Bad);
         begin
            raise Program_Error with "accepted " & Text;
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end Reject;
   begin
      for Kind in Interfaces.Unsigned_32 range 11 .. 16 loop
         Value := Base;
         Value.Task_Value := Kind;
         Value.Previous_Task := 27 - Kind;
         declare
            Result : constant M.Task_Executing_Report := T.Convert (Value);
         begin
            Check (Result, Value);
            Require (not M.Is_Operational (Result.Current_Task), "reserved current");
            Require (not M.Is_Operational (Result.Previous_Task), "reserved previous");
         end;
      end loop;
      Require (T.Convert (Base).Current_Task = M.IRST_Boresight, "value 4 is IRSTBoresight");
      for Bad of Bad_Tasks loop
         Value := Base;
         Value.Task_Value := Bad;
         Reject (Value, "current task");
         Value := Base;
         Value.Previous_Task := Bad;
         Reject (Value, "previous task");
      end loop;
      for Bad of Bad_Statuses loop
         Value := Base;
         Value.Previous_Task_Status := Bad;
         Reject (Value, "previous task status");
      end loop;
   end Conversion;

   --  Unknown enumerations reject the whole report as malformed (even while full) without
   --  allocation; capacity, allocation failure and recovery.
   procedure Queue_Recovery (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-rep-basic");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      Copies  : Integer;
      First   : constant T.Raw_Task_Executing := Raw (16#8000_0001#, 1, 2, 3, 4, 5, 6, 7, 8, 1);
      Bad     : T.Raw_Task_Executing := First;
   begin
      Sch.Enable (Channel);
      declare
         Stream : constant M.Task_Executing_Stream := M.Open_Task_Executing (Channel, 1);
      begin
         Send (First);
         Copies := Probe (4);
         Require (Probe (5, 1) = 1, "arm allocation failure");
         Send (Raw (3, 4, 5, 6, 7, 8, 9, 10, 1, 2));
         Send_Null;
         Bad.Task_Value := 17;
         Send (Bad);
         Bad := First;
         Bad.Previous_Task := 17;
         Send (Bad);
         Bad := First;
         Bad.Previous_Task_Status := 5;
         Send (Bad);
         Counters (Stream, 6, 1, 4);
         Require (Probe (4) = Copies and then Probe (6) = 1, "no allocation while full/invalid");
         Next (Stream, First);
         Send (Raw (5, 6, 7, 8, 9, 10, 11, 12, 13, 4));
         Counters (Stream, 7, 1, 5);
         Expect_Empty (Stream);
         Send (Raw (5, 6, 7, 8, 9, 10, 11, 12, 13, 4));
         Next (Stream, Raw (5, 6, 7, 8, 9, 10, 11, 12, 13, 4));
         Counters (Stream, 8, 1, 5);
      --  Non-raising finalization deactivates consumption without unregistering.
      end;
      One_Shot (Channel);
      Send (First);
      Sch.Close (Channel);
      AMS.MEL.Close (Parent);
   end Queue_Recovery;

   procedure Registration_Failure
     (Provider_Path, Scenario, Message : String; Unsupported : Boolean := False)
   is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      Before  : constant IC.unsigned := Rep_Registrations.all;
   begin
      begin
         declare
            Stream : M.Task_Executing_Stream := M.Open_Task_Executing (Channel, 4);
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
      Require (Rep_Registrations.all = Before + 1, "one provider attempt after refusal");
      One_Shot (Channel);
      Sch.Enable (Channel);
      Sch.Close (Channel);
      AMS.MEL.Close (Parent);
   end Registration_Failure;

   --  Both families on one Scheduling channel, in either registration order, with separate
   --  one-shots, queues, counters and independently closable streams.
   procedure Exercise
     (Channel                : Sch.Scheduling_Channel;
      Report                 : in out M.Task_Executing_Stream;
      Ack                    : in out M.Execute_Task_Ack_Stream;
      Rep_Before, Ack_Before : IC.unsigned)
   is
      Value : constant T.Raw_Task_Executing := Raw (21, 22, 23, 5, -24, -25, 26, 27, 12, 3);
   begin
      Require (Rep_Registrations.all = Rep_Before + 1, "one report registration");
      Require (Ack_Registrations.all = Ack_Before + 1, "one ack registration");
      One_Shot (Channel);
      Send (Value);
      Require (Ack_Deliver (31, 32, 0) = 1, "ack delivery");
      Counters (Report, 1, 0, 0);
      Require (M.Execute_Task_Ack_Counters (Ack).Events_Received = 1, "ack counter is independent");
      Next (Report, Value);
      Expect_Empty (Report);
      M.Close (Report);
      Send (Value);
      declare
         Received : constant M.Execute_Task_Ack := M.Receive_Execute_Task_Ack (Ack);
      begin
         Require (Received.Command_ID = 31 and then Received.Request_ID = 32, "ack fidelity");
      end;
      Require (Ack_Deliver (33, 34, 0) = 1, "ack after report close");
      Require
        (M.Receive_Execute_Task_Ack (Ack).Command_ID = 33, "ack survives report owner closure");
      M.Close (Ack);
   end Exercise;

   procedure Both_Families (Provider_Path : String; Report_First : Boolean) is
      Parent     : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-both-basic");
      Channel    : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      Rep_Before : constant IC.unsigned := Rep_Registrations.all;
      Ack_Before : constant IC.unsigned := Ack_Registrations.all;
   begin
      if Report_First then
         declare
            Report : M.Task_Executing_Stream := M.Open_Task_Executing (Channel, 4);
         begin
            Require (Ack_Registrations.all = Ack_Before, "report open implied ExecuteTaskAck");
            declare
               Ack : M.Execute_Task_Ack_Stream := M.Open_Execute_Task_Ack (Channel, 4);
            begin
               Exercise (Channel, Report, Ack, Rep_Before, Ack_Before);
            end;
         end;
      else
         declare
            Ack : M.Execute_Task_Ack_Stream := M.Open_Execute_Task_Ack (Channel, 4);
         begin
            Require (Rep_Registrations.all = Rep_Before, "ack open implied TaskExecutingRep");
            declare
               Report : M.Task_Executing_Stream := M.Open_Task_Executing (Channel, 4);
            begin
               Exercise (Channel, Report, Ack, Rep_Before, Ack_Before);
            end;
         end;
      end if;
      Sch.Close (Channel);
      AMS.MEL.Close (Parent);
   end Both_Families;

   --  Failed detach retains the provider graph; queued reports drain before the terminal
   --  provider failure.
   procedure Detach_Failure (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-rep-detach-fail");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      Stream  : M.Task_Executing_Stream := M.Open_Task_Executing (Channel, 4);
   begin
      begin
         Sch.Close (Channel);
         raise Program_Error with "detach failure accepted";
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      Require (Sch.Is_Open (Channel), "uncertain provider graph retained");
      Next (Stream, Raw (16#1111_1111#, 1, 2, 1, 3, 4, 5, 6, 7, 2));
      Next (Stream, Raw (16#3333_3333#, 8, 9, 2, 10, 11, 12, 13, 14, 3));
      begin
         Next (Stream, Raw (0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
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
      Deliver := To_Deliver (Dlsym (Pin, IC.To_C ("mock_scheduling_rep_deliver")));
      Ack_Deliver := To_Ack_Deliver (Dlsym (Pin, IC.To_C ("mock_scheduling_ack_deliver")));
      Rep_Registrations := To_Calls (Dlsym (Pin, IC.To_C ("mock_scheduling_rep_registrations")));
      Ack_Registrations := To_Calls (Dlsym (Pin, IC.To_C ("mock_scheduling_ack_registrations")));
      Spec := To_Calls (Dlsym (Pin, IC.To_C ("mock_scheduling_specific_calls")));
      Require
        (Deliver /= null
         and then Ack_Deliver /= null
         and then Rep_Registrations /= null
         and then Ack_Registrations /= null
         and then Spec /= null,
         "provider test hooks");
      Before := Spec.all;
      T.Check_Task_Executing_Representation;
      Conversion;
      declare
         Ack_Start : constant IC.unsigned := Ack_Registrations.all;
      begin
         Fidelity (Provider_Path);
         Require (Ack_Registrations.all = Ack_Start, "no implicit ExecuteTaskAck registration");
      end;
      Registration_Failure
        (Provider_Path,
         "sched-ack-basic",
         "provider does not support TaskExecutingRep callback",
         True);
      Registration_Failure
        (Provider_Path, "sched-rep-register-fail", "TaskExecutingRep callback registration failed");
      Registration_Failure
        (Provider_Path,
         "sched-rep-register-not-implemented",
         "TaskExecutingRep callback registration failed");
      Registration_Failure
        (Provider_Path,
         "sched-rep-register-unknown",
         "provider returned an unknown TaskExecutingRep registration result");
      Registration_Failure
        (Provider_Path,
         "sched-rep-register-throw",
         "provider exception during TaskExecutingRep registration");
      Queue_Recovery (Provider_Path);
      Both_Families (Provider_Path, Report_First => True);
      Both_Families (Provider_Path, Report_First => False);
      Detach_Failure (Provider_Path);
      Require (Spec.all = Before, "nine unsupported Scheduling methods uninvoked");
      Require (Dlclose (Pin) = 0, "provider unpin");
      Ada.Text_IO.Put_Line ("PASS: Ada Scheduling TaskExecutingRep contract");
   end Run;
end AMS_MEL_IR_Scheduling_Task_Executing_Tests;
