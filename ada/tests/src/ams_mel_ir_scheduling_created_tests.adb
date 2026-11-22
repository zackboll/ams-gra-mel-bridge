with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Scheduling;
with AMS.MEL.IR.Scheduling.Metadata;
with Interfaces;

package body AMS_MEL_IR_Scheduling_Created_Tests is
   package IR renames AMS.MEL.IR;
   package Sch renames IR.Scheduling;
   package M renames Sch.Metadata;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Unsigned_64;
   use type Interfaces.Integer_32;
   use type Interfaces.Integer_64;
   use type IR.Counter;

   Config : constant Sch.Scheduling_Config :=
     Sch.Create_Config
       (IR.Create_UCI_ID ([others => 0], "sched created"),
        IR.Create_UCI_ID ([others => 1], "platform"),
        IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock"));

   procedure Require (Condition : Boolean; Message : String) is
   begin
      if not Condition then
         raise Program_Error with "Schedule Created: " & Message;
      end if;
   end Require;

   type Position_Bits is array (Positive range 1 .. 4) of Interfaces.Unsigned_64;
   function Bits is new Ada.Unchecked_Conversion (M.Range_Az_El, Position_Bits);
   Patterns : constant array (Natural range 0 .. 7) of Interfaces.Unsigned_64 :=
     [16#8000_0000_0000_0000#,
      16#7FF0_0000_0000_0000#,
      16#FFF0_0000_0000_0000#,
      16#7FF8_0000_0000_0ABC#,
      16#7FF0_0000_0000_0DEF#,
      1,
      16#3FF0_0000_0000_0001#,
      0];

   procedure Check_Activities (Report : M.Schedule_Created_Report; Expected_Length : Natural) is
   begin
      Require (M.Length (Report.Activities) = Expected_Length, "vector size");
      for I in 1 .. Expected_Length loop
         declare
            A : constant M.Activity_Record := M.Element (Report.Activities, I);
            K : constant Natural := I - 1;
            P : constant Position_Bits := Bits (A.Position);
         begin
            Require
              (A.Activity_ID
               = (case K mod 3 is
                    when 0      => Interfaces.Unsigned_32'Last,
                    when 1      => 0,
                    when others => 16#8000_0004#),
               "activity ID");
            Require (M.Task_Kind'Enum_Rep (A.Task_ID) = K mod 17, "Task ordering/domain");
            Require
              (M.Is_Operational (A.Task_ID) = (K mod 17 <= 10), "reserved is not operational");
            Require
              (A.Track_ID
               = (if K mod 2 = 0 then Interfaces.Integer_32'First else Interfaces.Integer_32'Last),
               "track ID");
            Require
              (A.Duration_NS
               = (if K mod 2 = 0 then Interfaces.Integer_64'First else Interfaces.Integer_64'Last),
               "duration");
            Require (P (1) = Patterns (K mod 8), "range bits");
            Require (P (2) = Patterns ((K + 1) mod 8), "azimuth bits");
            Require (P (3) = Patterns ((K + 2) mod 8), "elevation bits");
            Require (P (4) = Patterns ((K + 3) mod 8), "range rate bits");
         end;
      end loop;
   end Check_Activities;

   procedure Is_Empty (Stream : M.Schedule_Created_Stream; Stopped : Boolean) is
   begin
      declare
         Unexpected : constant M.Schedule_Created_Report := M.Receive_Schedule_Created (Stream);
      begin
         raise Program_Error with "unexpected report" & Unexpected.Command_ID'Image;
      end;
   exception
      when IR.Timeout_Error =>
         Require (not Stopped, "timeout only while active");
      when IR.Stream_Stopped =>
         Require (Stopped, "terminal stop after drain");
   end Is_Empty;

   procedure Payload (Provider_Path : String) is
      Empty, Single, Large : M.Schedule_Created_Report;
   begin
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-created-sync");
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
         Stream  : M.Schedule_Created_Stream := M.Open_Schedule_Created (Channel, 3);
      begin
         Sch.Close (Channel);
         AMS.MEL.Close (Parent);
         Empty := M.Receive_Schedule_Created (Stream);
         Single := M.Receive_Schedule_Created (Stream);
         Large := M.Receive_Schedule_Created (Stream);
         Is_Empty (Stream, Stopped => True);
         declare
            C : constant M.Metadata_Counters := M.Schedule_Created_Counters (Stream);
         begin
            Require
              (C.Events_Received = 4
               and C.Events_Dropped_Queue_Full = 0
               and C.Malformed_Or_Unsupported = 0,
               "synchronous and late counters");
         end;
         M.Close (Stream);
         M.Close (Stream);
      end;
      Require
        (Empty.Command_ID = 0
         and Empty.Scan_ID = Interfaces.Unsigned_32'Last
         and Empty.Activity_Count = Interfaces.Unsigned_32'Last
         and Empty.Estimated_Duration_NS = Interfaces.Integer_64'Last,
         "empty scalars");
      Require
        (Single.Command_ID = 16#8000_0001#
         and Single.Scan_ID = 0
         and Single.Activity_Count = 7
         and Single.Estimated_Duration_NS = -1,
         "single scalars");
      Require
        (Large.Command_ID = Interfaces.Unsigned_32'Last
         and Large.Scan_ID = 16#8000_0002#
         and Large.Activity_Count = 0
         and Large.Estimated_Duration_NS = Interfaces.Integer_64'First,
         "large scalars");
      Check_Activities (Empty, 0);
      Check_Activities (Single, 1);
      Check_Activities (Large, 300);
      declare
         Copy : constant M.Schedule_Created_Report := Large;
      begin
         Large := Empty;
         Check_Activities (Copy, 300);
      end;
   end Payload;

   procedure Refused (Provider_Path, Scenario : String; Unsupported : Boolean) is
      Parent  : constant AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : constant Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      procedure Attempt (Expect_Not_Supported : Boolean) is
      begin
         declare
            Stream : M.Schedule_Created_Stream := M.Open_Schedule_Created (Channel, 2);
         begin
            M.Close (Stream);
            raise Program_Error with "unexpected registration success";
         end;
      exception
         when M.Not_Supported =>
            Require (Expect_Not_Supported, "unexpected Not_Supported");
         when AMS.MEL.Provider_Error =>
            Require (not Expect_Not_Supported, "expected Not_Supported");
      end Attempt;
   begin
      Attempt (Unsupported);
      Attempt (False);
   end Refused;

   procedure All_Families (Provider_Path : String) is
      Parent  : constant AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-both-sync");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
   begin
      Sch.Enable (Channel);
      declare
         Created : M.Schedule_Created_Stream := M.Open_Schedule_Created (Channel, 1);
         Ack     : constant M.Execute_Task_Ack_Stream := M.Open_Execute_Task_Ack (Channel, 4);
         Rep     : constant M.Task_Executing_Stream := M.Open_Task_Executing (Channel, 4);
         C       : constant M.Metadata_Counters := M.Schedule_Created_Counters (Created);
         Empty   : constant M.Schedule_Created_Report := M.Receive_Schedule_Created (Created);
      begin
         Require
           (C.Events_Received = 3
            and C.Events_Dropped_Queue_Full = 2
            and C.Malformed_Or_Unsupported = 0,
            "independent drop-incoming");
         Check_Activities (Empty, 0);
         Is_Empty (Created, Stopped => False);
         M.Close (Created);
         declare
            A : constant M.Execute_Task_Ack := M.Receive_Execute_Task_Ack (Ack);
            R : constant M.Task_Executing_Report := M.Receive_Task_Executing (Rep);
         begin
            Require (A.Command_ID = 0 and R.Command_ID = 0, "other queues unaffected by Close");
         end;
      --  Ack and Rep finalize here; Scheduling and Session finalize outside.
      end;
   end All_Families;

   procedure Run (Provider_Path : String) is
   begin
      Payload (Provider_Path);
      Refused (Provider_Path, "sched-created-not-supported", Unsupported => True);
      Refused (Provider_Path, "sched-created-fail", Unsupported => False);
      Refused (Provider_Path, "sched-created-unknown", Unsupported => False);
      Refused (Provider_Path, "sched-created-throw", Unsupported => False);
      All_Families (Provider_Path);
      declare
         Parent  : constant AMS.MEL.Session :=
           AMS.MEL.Open (Provider_Path, "sched-created-malformed");
         Channel : constant Sch.Scheduling_Channel := Sch.Open (Parent, Config);
         Stream  : constant M.Schedule_Created_Stream := M.Open_Schedule_Created (Channel, 1);
         C       : constant M.Metadata_Counters := M.Schedule_Created_Counters (Stream);
         Report  : constant M.Schedule_Created_Report := M.Receive_Schedule_Created (Stream, 100);
      begin
         Require
           (C.Events_Received = 5
            and C.Events_Dropped_Queue_Full = 1
            and C.Malformed_Or_Unsupported = 3,
            "malformed takes precedence over full");
         Require (Report.Activity_Count = 7, "the earlier report was retained atomically");
         Check_Activities (Report, 1);
         Is_Empty (Stream, Stopped => False);
      --  Exercise implicit finalization with all three owners still open.
      end;
      Ada.Text_IO.Put_Line ("Ada Scheduling SchedCreatedRep checks passed");
   end Run;
end AMS_MEL_IR_Scheduling_Created_Tests;
