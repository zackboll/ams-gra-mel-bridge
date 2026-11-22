with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR.Scheduling.Metadata;
with Interfaces;

package body AMS_MEL_IR_Scheduling_Events_Tests is
   package IR renames AMS.MEL.IR;
   package Sch renames IR.Scheduling;
   package M renames Sch.Metadata;
   use type Interfaces.Integer_64;
   use type M.Metadata_Counters;
   use type M.Task_Event_Type;

   Config : constant Sch.Scheduling_Config :=
     Sch.Create_Config
       (IR.Create_UCI_ID ([others => 0], "task events metadata"),
        IR.Create_UCI_ID ([others => 1], "platform"),
        IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock"));

   procedure Require (Condition : Boolean; Message : String) is
   begin
      if not Condition then
         raise Program_Error with "TaskEvents: " & Message;
      end if;
   end Require;

   procedure Payload (Provider_Path : String) is
      Max      : constant Interfaces.Unsigned_32 := Interfaces.Unsigned_32'Last;
      type Values is array (M.Task_Event_Type) of M.Task_Event;
      Expected : constant Values :=
        [M.Not_Set        => (M.Not_Set, 0, 0, Interfaces.Integer_64'First, 0),
         M.Slew_Start     => (M.Slew_Start, Max, 7, Interfaces.Integer_64'Last, Max),
         M.Scan_Start     => (M.Scan_Start, 38, Max, -6, 0),
         M.Break_Scan     => (M.Break_Scan, 57, 21, -9, Max),
         M.Reserved_11    => (M.Reserved_11, 76, 28, -12, 0),
         M.Reserved_12    => (M.Reserved_12, 95, 35, -15, Max),
         M.Accel_Start    => (M.Accel_Start, 114, 42, -18, 0),
         M.Accel_Complete => (M.Accel_Complete, 133, 49, -21, Max)];
      Actual   : Values;
   begin
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-events-sync");
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
         Stream  : M.Task_Events_Stream := M.Open_Task_Events (Channel, 8);
      begin
         Require (M.Task_Events_Counters (Stream) = (11, 0, 3), "synchronous counters");
         Sch.Close (Channel);
         AMS.MEL.Close (Parent);
         for Kind in Actual'Range loop
            Require
              (M.Task_Event_Type'Enum_Rep (Kind) = M.Task_Event_Type'Pos (Kind),
               "EventType representation");
            Actual (Kind) := M.Receive_Task_Events (Stream);
         end loop;
         begin
            Require (M.Receive_Task_Events (Stream).Event_Type = M.Not_Set, "expected stop");
            raise Program_Error with "received after draining a stopped stream";
         exception
            when IR.Stream_Stopped =>
               null;
         end;
         M.Close (Stream);
         M.Close (Stream);
      end;
      Require (Actual = Expected, "all five fields and all eight alternatives after owner closure");
   end Payload;

   procedure Refusal (Provider_Path : String) is
      Parent  : constant AMS.MEL.Session :=
        AMS.MEL.Open (Provider_Path, "sched-events-not-supported");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
   begin
      for Attempt in 1 .. 2 loop
         begin
            declare
               Stream : M.Task_Events_Stream := M.Open_Task_Events (Channel, 1);
            begin
               M.Close (Stream);
               raise Program_Error with "unexpected registration success";
            end;
         exception
            when M.Not_Supported =>
               Require (Attempt = 1, "NotSupported consumes the attempt");
            when AMS.MEL.Provider_Error =>
               Require (Attempt = 2, "expected NotSupported first");
         end;
         Sch.Enable (Channel);
      end loop;
   end Refusal;

   procedure Poll (Provider_Path : String) is
      Parent  : constant AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-events-wake");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
   begin
      Sch.Enable (Channel);
      declare
         Stream : constant M.Task_Events_Stream := M.Open_Task_Events (Channel, 1);
      begin
         Require (M.Receive_Task_Events (Stream, 2).Event_Type = M.Not_Set, "expected timeout");
         raise Program_Error with "empty active stream published a value";
      exception
         when IR.Timeout_Error =>
            null;
      end;
   end Poll;

   procedure Run (Provider_Path : String) is
   begin
      Payload (Provider_Path);
      Refusal (Provider_Path);
      Poll (Provider_Path);
      Ada.Text_IO.Put_Line ("Scheduling TaskEvents Ada tests passed");
   end Run;
end AMS_MEL_IR_Scheduling_Events_Tests;
