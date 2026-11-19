with AMS.MEL.IR.C2;
with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Scheduling;
with AMS.MEL.IR.Scheduling.Commands;
with AMS.MEL.IR.Scheduling.Metadata;
with Interfaces;
with AMS_MEL_Scan_Fixture;

package body AMS_MEL_IR_Scheduling_Scan_Tests is
   package Sch renames AMS.MEL.IR.Scheduling;
   package Cmd renames AMS.MEL.IR.Scheduling.Commands;
   package Meta renames AMS.MEL.IR.Scheduling.Metadata;
   package V renames AMS.MEL.IR.Channel;
   use type V.Outcome;
   use type V.Command_Return;
   use type V.Error_Code;
   use type Interfaces.Unsigned_32;
   use type Cmd.Task_List_Action;

   procedure Require (Condition : Boolean; Message : String) is
   begin
      if not Condition then
         raise Program_Error with "Ada ScanTask: " & Message;
      end if;
   end Require;

   Config : constant Sch.Scheduling_Config :=
     Sch.Create_Config
       (AMS.MEL.IR.Create_UCI_ID ([others => 0], "scan-channel"),
        AMS.MEL.IR.Create_UCI_ID ([others => 1], "platform"),
        AMS.MEL.IR.Create_Component_Location (1.25, -2.5, 3.75, "station", "mock"));
   Info   : constant Cmd.Task_Info :=
     (System_Time_NS             => Interfaces.Integer_64'First,
      Request_ID                 => 16#F123_4567#,
      Action                     => Cmd.Normal,
      Precedence_Within_Priority => Interfaces.Unsigned_32'Last,
      Priority                   => 16#8000_0001#);

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
      Info       => Info);

   procedure Run (Provider_Path : String) is
      procedure Completed (Scenario : String; Expected : V.Command_Return) is
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      begin
         --  The binding exists even before enable; eligibility is a distinct failure.
         begin
            declare
               Request : Cmd.Scan_Request := Cmd.Exec_Scan (Channel, Task_Value);
            begin
               Cmd.Close (Request);
               raise Program_Error with "Attached owner admitted ScanTask";
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Sch.Enable (Channel);
         for Action in Cmd.Task_List_Action loop
            declare
               Value : Cmd.Scan_Task := Task_Value;
            begin
               Value.Info.Action := Action;
               Value.Parameters.Degradation :=
                 AMS.MEL.IR.C2.Degradation_Method'Val (Cmd.Task_List_Action'Pos (Action));
               Value.Parameters.Center_Frame_Reference_EL :=
                 AMS.MEL.IR.C2.Coordinate_Frame_Reference'Val
                   (Cmd.Task_List_Action'Pos (Action) mod 2);
               Value.Parameters.Center_Frame_Reference_AZ :=
                 AMS.MEL.IR.C2.Coordinate_Frame_Reference'Val
                   (Cmd.Task_List_Action'Pos (Action) / 2);
               Value.Parameters.Elevation_Defined_With_Range_And_Altitude :=
                 Action = Cmd.Normal or else Action = Cmd.Immediate;
               if Action /= Cmd.Normal then
                  Value.Parameters.Center_Azimuth_Rad := 2.34567890123456;
                  Value.Parameters.Center_Elevation_Rad := -0.0;
                  Value.Parameters.Scan_Width_Rad := -0.0;
                  Value.Parameters.Scan_Height_Rad := -0.0;
                  Value.Parameters.Scan_Rate_Rad_Per_Second := -0.0;
                  Value.Parameters.Preferred_Revisit_Interval_Seconds := -0.0;
                  Value.Parameters.Required_Revisit_Interval_Seconds := -0.0;
                  Value.Parameters.Continuous_Scan := 0;
                  Value.Parameters.Returning := AMS.MEL.IR.C2.Scan_Value'Last;
                  Value.Parameters.Agile_Scan := 16#8000_0000#;
                  Value.Info.System_Time_NS := Interfaces.Integer_64'Last;
                  Value.Command_ID := 0;
               end if;
               declare
                  Request : Cmd.Scan_Request := Cmd.Exec_Scan (Channel, Value);
                  Result  : constant Cmd.Scan_Result := Cmd.Wait (Request, 5_000);
                  Cached  : constant Cmd.Scan_Result := Cmd.Wait (Request, 0);
               begin
                  AMS_MEL_Scan_Fixture.Verify (Value);
                  Require (Cmd.Status (Result) = V.Success, "valid Return outcome");
                  Require (Cmd.Value (Result) = Expected, "actual Return payload");
                  Require (Cmd.Value (Cached) = Expected, "cached Return payload");
                  Cmd.Close (Request);
               end;
            end;
         end loop;
         --  Automatic observer finalization also remains non-raising.
         declare
            Request : constant Cmd.Scan_Request := Cmd.Exec_Scan (Channel, Task_Value);
            Result  : constant Cmd.Scan_Result := Cmd.Wait (Request, 5_000);
         begin
            Require (Cmd.Value (Result) = Expected, "finalized observer");
         end;
         Sch.Close (Channel);
         AMS.MEL.Close (Parent);
      end Completed;

      procedure Rejected (Scenario : String; Expected : V.Error_Code) is
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      begin
         Sch.Enable (Channel);
         declare
            Request : Cmd.Scan_Request := Cmd.Exec_Scan (Channel, Task_Value);
            Result  : constant Cmd.Scan_Result := Cmd.Wait (Request, 5_000);
            Cached  : constant Cmd.Scan_Result := Cmd.Wait (Request, 0);
         begin
            Require (Cmd.Status (Result) = V.Rejected, "ErrorOr rejection");
            Require (Cmd.Rejection_Code (Result) = Expected, "exact MEL rejection code");
            Require (Cmd.Description (Result)'Length = 1_044, "complete long description");
            Require (Cmd.Description (Result) = Cmd.Description (Cached), "cached rejection");
            Cmd.Close (Request);
         end;
         Sch.Close (Channel);
         AMS.MEL.Close (Parent);
      end Rejected;

      procedure Failed (Scenario : String) is
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      begin
         Sch.Enable (Channel);
         begin
            declare
               Request : constant Cmd.Scan_Request := Cmd.Exec_Scan (Channel, Task_Value);
               Result  : constant Cmd.Scan_Result := Cmd.Wait (Request, 5_000);
            begin
               Require (Cmd.Status (Result) = V.Success, "unexpected failed result");
               raise Program_Error with "malformed provider path reported success";
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Sch.Close (Channel);
         AMS.MEL.Close (Parent);
         AMS_MEL_Scan_Fixture.Drain;
      end Failed;

      procedure Pending is
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-scan-delayed");
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
         Ack     : Meta.Execute_Task_Ack_Stream := Meta.Open_Execute_Task_Ack (Channel, 2);
         Rep     : Meta.Task_Executing_Stream := Meta.Open_Task_Executing (Channel, 2);
      begin
         Sch.Enable (Channel);
         declare
            Request : Cmd.Scan_Request := Cmd.Exec_Scan (Channel, Task_Value);
         begin
            begin
               declare
                  Result : constant Cmd.Scan_Result := Cmd.Wait (Request, 0);
               begin
                  Require (Cmd.Status (Result) = V.Success, "unexpected pending result");
                  raise Program_Error with "pending ScanTask did not time out";
               end;
            exception
               when AMS.MEL.IR.Timeout_Error =>
                  null;
            end;
            begin
               declare
                  Event : constant Meta.Execute_Task_Ack := Meta.Receive_Execute_Task_Ack (Ack, 0);
               begin
                  Require (Event.Request_ID = Info.Request_ID, "unexpected ack");
                  raise Program_Error with "future fabricated acknowledgement";
               end;
            exception
               when AMS.MEL.IR.Timeout_Error =>
                  null;
            end;
            begin
               declare
                  Event : constant Meta.Task_Executing_Report :=
                    Meta.Receive_Task_Executing (Rep, 0);
               begin
                  Require (Event.Request_ID = Info.Request_ID, "unexpected report");
                  raise Program_Error with "future fabricated executing report";
               end;
            exception
               when AMS.MEL.IR.Timeout_Error =>
                  null;
            end;
            AMS_MEL_Scan_Fixture.Release_Pending (1);
            declare
               Result : constant Cmd.Scan_Result := Cmd.Wait (Request, 5_000);
            begin
               Require (Cmd.Value (Result) = V.Return_Success, "timeout did not cancel");
            end;
            begin
               declare
                  Event : constant Meta.Execute_Task_Ack := Meta.Receive_Execute_Task_Ack (Ack, 0);
               begin
                  Require (Event.Request_ID = Info.Request_ID, "unexpected completed ack");
                  raise Program_Error with "completed future fabricated acknowledgement";
               end;
            exception
               when AMS.MEL.IR.Timeout_Error =>
                  null;
            end;
            Cmd.Close (Request);
         end;
         --  A second outstanding observer is retired by automatic finalization.
         declare
            Request : Cmd.Scan_Request := Cmd.Exec_Scan (Channel, Task_Value);
         begin
            Cmd.Close (Request);
         end;
         declare
            Request : constant Cmd.Scan_Request := Cmd.Exec_Scan (Channel, Task_Value);
         begin
            --  Leave an open pending observer to non-raising finalization.
            Require (Cmd.Status (Cmd.Wait (Request, 0)) = V.Success, "unexpected completion");
         exception
            when AMS.MEL.IR.Timeout_Error =>
               null;
         end;
         Sch.Close (Channel);
         AMS.MEL.Close (Parent);
         AMS_MEL_Scan_Fixture.Release_Pending (2);
         AMS_MEL_Scan_Fixture.Drain;
         Meta.Close (Ack);
         Meta.Close (Rep);
      end Pending;

      procedure Retained is
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-scan-delayed");
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      begin
         Sch.Enable (Channel);
         declare
            Request : Cmd.Scan_Request := Cmd.Exec_Scan (Channel, Task_Value);
         begin
            Sch.Close (Channel);
            AMS.MEL.Close (Parent);
            AMS_MEL_Scan_Fixture.Release_Pending (1);
            declare
               Result : constant Cmd.Scan_Result := Cmd.Wait (Request, 5_000);
            begin
               Require (Cmd.Value (Result) = V.Return_Success, "request outlived both owners");
            end;
            Cmd.Close (Request);
         end;
         AMS_MEL_Scan_Fixture.Drain;
      end Retained;
   begin
      AMS_MEL_Scan_Fixture.Open (Provider_Path);
      Completed ("sched-scan-success", V.Return_Success);
      Completed ("sched-scan-fail", V.Fail);
      Completed ("sched-scan-not-supported", V.Not_Supported);
      Completed ("sched-scan-not-implemented", V.Not_Implemented);
      Rejected ("sched-scan-unsupported", V.Unsupported);
      Rejected ("sched-scan-invalid-state", V.Invalid_State);
      Failed ("sched-scan-exec-throw");
      Failed ("sched-scan-future-throw");
      Failed ("sched-scan-invalid-future");
      Failed ("sched-scan-null");
      Failed ("sched-scan-malformed");
      Failed ("sched-scan-invalid-return");
      Pending;
      Retained;
      declare
         Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-not-advertised");
      begin
         begin
            declare
               Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
            begin
               Sch.Close (Channel);
               raise Program_Error with "unsupported Scheduling unexpectedly attached";
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Require (AMS.MEL.Is_Open (Parent), "unsupported capability must not fail Session");
         AMS.MEL.Close (Parent);
      end;
      AMS_MEL_Scan_Fixture.Close;
      Ada.Text_IO.Put_Line ("PASS safe Ada Scheduling ScanTask public interface");
   end Run;
end AMS_MEL_IR_Scheduling_Scan_Tests;
