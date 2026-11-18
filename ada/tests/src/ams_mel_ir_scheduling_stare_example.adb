with Ada.Command_Line;
with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Scheduling;
with AMS.MEL.IR.Scheduling.Commands;
with Interfaces;

procedure AMS_MEL_IR_Scheduling_Stare_Example is
   package Sch renames AMS.MEL.IR.Scheduling;
   package Cmd renames AMS.MEL.IR.Scheduling.Commands;
   use type AMS.MEL.IR.Channel.Outcome;
   use type AMS.MEL.IR.Channel.Command_Return;
   use type Interfaces.Integer_64;
   Parent     : AMS.MEL.Session :=
     AMS.MEL.Open (Ada.Command_Line.Argument (1), "sched-stare-success");
   Config     : constant Sch.Scheduling_Config :=
     Sch.Create_Config
       (AMS.MEL.IR.Create_UCI_ID ([others => 0], "stare-example"),
        AMS.MEL.IR.Create_UCI_ID ([others => 1], "platform"),
        AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock"));
   Channel    : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
   Task_Value : constant Cmd.Stare_Task :=
     (Command_ID => Interfaces.Unsigned_32'Last,
      Parameters =>
        (Center_Azimuth_Rad   => 0.25,
         Center_Elevation_Rad => -0.125,
         Elevation_Frame      => Cmd.Inertial,
         Azimuth_Frame        => Cmd.Aircraft,
         Duration_NS          => 1_000_000),
      Info       =>
        (System_Time_NS             => -1,
         Request_ID                 => 16#8000_0000#,
         Action                     => Cmd.Normal,
         Precedence_Within_Priority => 2,
         Priority                   => 3));
begin
   Sch.Enable (Channel);
   declare
      Request : Cmd.Stare_Request := Cmd.Exec_Stare (Channel, Task_Value);
      Result  : constant Cmd.Stare_Result := Cmd.Wait (Request, 5_000);
   begin
      if Cmd.Status (Result) /= AMS.MEL.IR.Channel.Success
        or else Cmd.Value (Result) /= AMS.MEL.IR.Channel.Return_Success
      then
         raise Program_Error with "mock did not return Success";
      end if;
      Cmd.Close (Request);
   end;
   Sch.Close (Channel);
   AMS.MEL.Close (Parent);
   Ada.Text_IO.Put_Line
     ("Mock Return::Success observed; no physical staring or acquisition is claimed.");
end AMS_MEL_IR_Scheduling_Stare_Example;
