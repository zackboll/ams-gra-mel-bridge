with Ada.Command_Line;
with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.Scheduling.Metadata;

procedure AMS_MEL_IR_Scheduling_Performance_Example is
   package IR renames AMS.MEL.IR;
   package Sch renames IR.Scheduling;
   package Metadata renames Sch.Metadata;
   use type IR.C2.Scan_Value;

   Parent  : AMS.MEL.Session :=
     AMS.MEL.Open (Ada.Command_Line.Argument (1), "sched-performance-sync");
   Channel : Sch.Scheduling_Channel :=
     Sch.Open
       (Parent,
        Sch.Create_Config
          (IR.Create_UCI_ID ([others => 0], "scan performance example"),
           IR.Create_UCI_ID ([others => 1], "platform"),
           IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock")));
   Stream  : Metadata.Scan_Performance_Stream := Metadata.Open_Scan_Performance (Channel, 14);
   Report  : constant Metadata.Scan_Performance_Report :=
     Metadata.Receive_Scan_Performance (Stream, Timeout_Milliseconds => 1_000);
begin
   Metadata.Close (Stream);
   Sch.Close (Channel);
   AMS.MEL.Close (Parent);
   if Report.Commanded.Scan_ID /= 17 or else Report.Achieved.Scan_ID /= 16#f000_0003# then
      raise Program_Error with "unexpected mock report";
   end if;
   Ada.Text_IO.Put_Line
     ("Owned scan parameters after provider closure: commanded ID"
      & Report.Commanded.Scan_ID'Image
      & ", achieved ID"
      & Report.Achieved.Scan_ID'Image);
end AMS_MEL_IR_Scheduling_Performance_Example;
