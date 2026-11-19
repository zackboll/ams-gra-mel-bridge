with Ada.Command_Line;
with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Scheduling;
with AMS.MEL.IR.Scheduling.Commands;
with Interfaces;

procedure AMS_MEL_IR_Scheduling_Track_Example is
   package Sch renames AMS.MEL.IR.Scheduling;
   package Cmd renames AMS.MEL.IR.Scheduling.Commands;
   use type AMS.MEL.IR.Channel.Outcome;
   use type AMS.MEL.IR.Channel.Command_Return;
   use type Interfaces.Integer_64;
   Parent     : AMS.MEL.Session :=
     AMS.MEL.Open (Ada.Command_Line.Argument (1), "sched-track-success");
   Config     : constant Sch.Scheduling_Config :=
     Sch.Create_Config
       (AMS.MEL.IR.Create_UCI_ID ([others => 0], "track-example"),
        AMS.MEL.IR.Create_UCI_ID ([others => 1], "platform"),
        AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock"));
   Channel    : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
   Task_Value : constant Cmd.Track_Task :=
     (Command_ID => Interfaces.Unsigned_32'Last,
      Parameters =>
        (Duration_NS => -1,
         Count       => 1,
         Targets     =>
           [1      =>
              (Target_ID                     => Interfaces.Integer_32'First,
               Location_Azimuth_Rad          => -0.0,
               Location_Elevation_Rad        => 1.25,
               Rate_Azimuth_Rad_Per_Second   => 2.5,
               Rate_Elevation_Rad_Per_Second => -3.75),
            others => <>]),
      Info       =>
        (System_Time_NS             => -1,
         Request_ID                 => 16#8000_0000#,
         Action                     => Cmd.Normal,
         Precedence_Within_Priority => 2,
         Priority                   => 3));
begin
   Sch.Enable (Channel);
   declare
      Request : Cmd.Track_Request := Cmd.Exec_Track (Channel, Task_Value);
      Result  : constant Cmd.Track_Result := Cmd.Wait (Request, 5_000);
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
     ("Mock Return::Success observed; no physical trackning or acquisition is claimed.");
end AMS_MEL_IR_Scheduling_Track_Example;
