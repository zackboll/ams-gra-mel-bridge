with AMS.MEL.IR.C2;
with Ada.Command_Line;
with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Scheduling;
with AMS.MEL.IR.Scheduling.Commands;
with Interfaces;

procedure AMS_MEL_IR_Scheduling_Scan_Example is
   package Sch renames AMS.MEL.IR.Scheduling;
   package Cmd renames AMS.MEL.IR.Scheduling.Commands;
   use type AMS.MEL.IR.Channel.Outcome;
   use type AMS.MEL.IR.Channel.Command_Return;
   use type Interfaces.Integer_64;
   Parent     : AMS.MEL.Session :=
     AMS.MEL.Open (Ada.Command_Line.Argument (1), "sched-scan-success");
   Config     : constant Sch.Scheduling_Config :=
     Sch.Create_Config
       (AMS.MEL.IR.Create_UCI_ID ([others => 0], "scan-example"),
        AMS.MEL.IR.Create_UCI_ID ([others => 1], "platform"),
        AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock"));
   Channel    : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
   Task_Value : constant Cmd.Scan_Task :=
     (Command_ID => Interfaces.Unsigned_32'Last,
      Parameters =>
        (Elevation_Defined_With_Range_And_Altitude => True,
         Center_Azimuth_Rad                        => -0.0,
         Center_Elevation_Rad                      => 1.23456789012345,
         Center_Frame_Reference_EL                 => AMS.MEL.IR.C2.Inertial,
         Center_Frame_Reference_AZ                 => AMS.MEL.IR.C2.Aircraft,
         Scan_Width_Rad                            => -2.34567890123456,
         Scan_Height_Rad                           => 0.0,
         Continuous_Scan                           => AMS.MEL.IR.C2.Scan_Value'Last,
         Returning                                 => 16#8000_0000#,
         Agile_Scan                                => 2,
         Scan_ID                                   => 16#FEDC_BA98#,
         Scan_Rate_Rad_Per_Second                  => -3.45678901234567,
         Preferred_Revisit_Interval_Seconds        => 4.56789012345678,
         Required_Revisit_Interval_Seconds         => -5.67890123456789,
         Max_Range_Of_Interest_M                   => AMS.MEL.IR.C2.Scan_Value'Last,
         Min_Range_Of_Interest_M                   => 16#8000_0001#,
         Elevation_Scan_Center_Altitude_M          => 16#8765_4321#,
         Elevation_Scan_Center_Range_M             => 16#F123_4567#,
         Degradation                               => AMS.MEL.IR.C2.Capacity_Degradation),
      Info       =>
        (System_Time_NS             => -1,
         Request_ID                 => 16#8000_0000#,
         Action                     => Cmd.Normal,
         Precedence_Within_Priority => 2,
         Priority                   => 3));
begin
   Sch.Enable (Channel);
   declare
      Request : Cmd.Scan_Request := Cmd.Exec_Scan (Channel, Task_Value);
      Result  : constant Cmd.Scan_Result := Cmd.Wait (Request, 5_000);
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
     ("Mock Return::Success observed; no physical scanning or acquisition is claimed.");
end AMS_MEL_IR_Scheduling_Scan_Example;
