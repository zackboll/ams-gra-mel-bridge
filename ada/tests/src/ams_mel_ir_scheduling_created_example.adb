with Ada.Command_Line;
with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Scheduling;
with AMS.MEL.IR.Scheduling.Metadata;
with Interfaces;

procedure AMS_MEL_IR_Scheduling_Created_Example is
   package IR renames AMS.MEL.IR;
   package Sch renames IR.Scheduling;
   package Metadata renames Sch.Metadata;
   use type Interfaces.Unsigned_32;
   Parent  : AMS.MEL.Session := AMS.MEL.Open (Ada.Command_Line.Argument (1), "sched-created-sync");
   Channel : Sch.Scheduling_Channel :=
     Sch.Open
       (Parent,
        Sch.Create_Config
          (IR.Create_UCI_ID ([others => 0], "schedule-created example"),
           IR.Create_UCI_ID ([others => 1], "platform"),
           IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock")));
   Stream  : Metadata.Schedule_Created_Stream := Metadata.Open_Schedule_Created (Channel, 3);
   Report  : Metadata.Schedule_Created_Report;
begin
   for J in 1 .. 3 loop
      Report := Metadata.Receive_Schedule_Created (Stream, Timeout_Milliseconds => 1_000);
   end loop;
   Metadata.Close (Stream);
   Sch.Close (Channel);
   AMS.MEL.Close (Parent);
   if Report.Activity_Count /= 0 or else Metadata.Length (Report.Activities) /= 300 then
      raise Program_Error with "mock returned an unexpected schedule";
   end if;
   Ada.Text_IO.Put_Line
     ("Mock schedule reports Activity_Count"
      & Report.Activity_Count'Image
      & " and vector length"
      & Metadata.Length (Report.Activities)'Image
      & "."
      & " Reception does not imply execution.");
end AMS_MEL_IR_Scheduling_Created_Example;
