with Ada.Command_Line;
with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Scheduling;
with AMS.MEL.IR.Scheduling.Metadata;
with Interfaces;

procedure AMS_MEL_IR_Scheduling_Update_Example is
   package IR renames AMS.MEL.IR;
   package Sch renames IR.Scheduling;
   package Metadata renames Sch.Metadata;
   use type Interfaces.Unsigned_32;
   use type Metadata.Track_List_Receipt;
   Parent  : AMS.MEL.Session := AMS.MEL.Open (Ada.Command_Line.Argument (1), "sched-update-sync");
   Channel : Sch.Scheduling_Channel :=
     Sch.Open
       (Parent,
        Sch.Create_Config
          (IR.Create_UCI_ID ([others => 0], "track list example"),
           IR.Create_UCI_ID ([others => 1], "platform"),
           IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock")));
   Stream  : Metadata.Update_Track_List_Stream := Metadata.Open_Update_Track_List (Channel, 4);
   Report  : Metadata.Update_Track_List_Response;
begin
   for J in 1 .. 4 loop
      Report := Metadata.Receive_Update_Track_List (Stream, Timeout_Milliseconds => 1_000);
   end loop;
   Metadata.Close (Stream);
   Sch.Close (Channel);
   AMS.MEL.Close (Parent);
   if Report.Command_ID /= Interfaces.Unsigned_32'Last or else Report.Response /= Metadata.Accepted
   then
      raise Program_Error with "unexpected mock receipt";
   end if;
   Ada.Text_IO.Put_Line
     ("Owned metadata after close: Command_ID"
      & Report.Command_ID'Image
      & ", "
      & Report.Response'Image
      & ". This is not evidence of physical tracking.");
end AMS_MEL_IR_Scheduling_Update_Example;
