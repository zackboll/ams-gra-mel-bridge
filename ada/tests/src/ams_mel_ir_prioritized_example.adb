with Ada.Command_Line;
with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Scheduling;
with AMS.MEL.IR.Scheduling.Commands;
with Interfaces;

procedure AMS_MEL_IR_Prioritized_Example is
   package Sch renames AMS.MEL.IR.Scheduling;
   package Cmd renames AMS.MEL.IR.Scheduling.Commands;
   use type AMS.MEL.IR.Channel.Outcome;
   use type Cmd.Track_List_Response;
   use type Interfaces.Integer_32;
   Parent  : AMS.MEL.Session :=
     AMS.MEL.Open (Ada.Command_Line.Argument (1), "sched-prioritized-accepted");
   Channel : Sch.Scheduling_Channel :=
     Sch.Open
       (Parent,
        Sch.Create_Config
          (AMS.MEL.IR.Create_UCI_ID ([others => 0], "example"),
           AMS.MEL.IR.Create_UCI_ID ([others => 1], "platform"),
           AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock")));
   Command : Cmd.Prioritized_Track_List_Cmd (Length => 2);
begin
   Command.Track_Count := 7;
   Command.Tracks (1).Track_ID := -7;
   Command.Tracks (2).Track_ID := -7;
   Command.Tracks (2).Position := (13.0, -0.0, 0.25, -3.5);
   Sch.Enable (Channel);
   declare
      Request : Cmd.Track_List_Request := Cmd.Send_Prioritized_Tracks (Channel, Command);
   begin
      Sch.Close (Channel);
      AMS.MEL.Close (Parent);
      declare
         Receipt : constant Cmd.Track_List_Result := Cmd.Wait (Request, 5_000);
      begin
         Cmd.Close (Request);
         if Cmd.Status (Receipt) /= AMS.MEL.IR.Channel.Success
           or else Cmd.Value (Receipt) /= Cmd.Accepted
         then
            raise Program_Error with "mock did not return the expected receipt";
         end if;
      end;
   end;
   Ada.Text_IO.Put_Line ("Mock Accepted receipt observed; no physical tracking is implied.");
end AMS_MEL_IR_Prioritized_Example;
