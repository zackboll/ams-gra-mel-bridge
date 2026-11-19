with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Scheduling;
with AMS.MEL.IR.Scheduling.Commands;
with AMS.MEL.IR.Scheduling.Metadata;
with Interfaces;
with AMS_MEL_Track_Fixture;

package body AMS_MEL_IR_Scheduling_Track_Tests is
   package Sch renames AMS.MEL.IR.Scheduling;
   package Cmd renames AMS.MEL.IR.Scheduling.Commands;
   package Meta renames AMS.MEL.IR.Scheduling.Metadata;
   package V renames AMS.MEL.IR.Channel;
   use type V.Outcome;
   use type V.Command_Return;
   use type V.Error_Code;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Integer_32;
   use type Interfaces.Integer_64;
   use type Cmd.Task_List_Action;

   procedure Require (Condition : Boolean; Message : String) is
   begin
      if not Condition then
         raise Program_Error with "Ada TrackTask: " & Message;
      end if;
   end Require;

   Config : constant Sch.Scheduling_Config :=
     Sch.Create_Config
       (AMS.MEL.IR.Create_UCI_ID ([others => 0], "track-channel"),
        AMS.MEL.IR.Create_UCI_ID ([others => 1], "platform"),
        AMS.MEL.IR.Create_Component_Location (1.25, -2.5, 3.75, "station", "mock"));
   Info   : constant Cmd.Task_Info :=
     (System_Time_NS             => Interfaces.Integer_64'First,
      Request_ID                 => 16#F123_4567#,
      Action                     => Cmd.Normal,
      Precedence_Within_Priority => Interfaces.Unsigned_32'Last,
      Priority                   => 16#8000_0001#);

   function Sample return Cmd.Track_Task is
      Result : Cmd.Track_Task;
   begin
      Result.Command_ID := Interfaces.Unsigned_32'Last;
      Result.Info := Info;
      Result.Parameters.Duration_NS := Interfaces.Integer_64'First;
      for Index in Result.Parameters.Targets'Range loop
         Result.Parameters.Targets (Index) :=
           (Target_ID                     =>
              (if Index = 255
               then Interfaces.Integer_32'First
               elsif Index = 254
               then Interfaces.Integer_32'Last
               else -Interfaces.Integer_32 (Index mod 17)),
            Location_Azimuth_Rad          =>
              (if Index mod 4 = 0 then -0.0 else Long_Float (Index) + 0.12345678901234),
            Location_Elevation_Rad        =>
              (if Index mod 4 = 1 then -0.0 else -Long_Float (Index) - 0.23456789012345),
            Rate_Azimuth_Rad_Per_Second   =>
              (if Index mod 4 = 2 then -0.0 else Long_Float (Index) + 0.34567890123456),
            Rate_Elevation_Rad_Per_Second =>
              (if Index mod 4 = 3 then -0.0 else -Long_Float (Index) - 0.45678901234567));
      end loop;
      return Result;
   end Sample;
   Task_Value : constant Cmd.Track_Task := Sample;

   procedure Run (Provider_Path : String) is
      procedure Completed (Scenario : String; Expected : V.Command_Return) is
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      begin
         --  The binding exists even before enable; eligibility is a distinct failure.
         begin
            declare
               Request : Cmd.Track_Request := Cmd.Exec_Track (Channel, Task_Value);
            begin
               Cmd.Close (Request);
               raise Program_Error with "Attached owner admitted TrackTask";
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Sch.Enable (Channel);
         for Action in Cmd.Task_List_Action loop
            declare
               Value : Cmd.Track_Task := Task_Value;
            begin
               Value.Info.Action := Action;
               case Action is
                  when Cmd.Normal       =>
                     Value.Parameters.Count := 0;

                  when Cmd.Next         =>
                     Value.Parameters.Count := 1;
                     Value.Parameters.Duration_NS := Interfaces.Integer_64'Last;
                     Value.Info.System_Time_NS := Interfaces.Integer_64'Last;
                     Value.Command_ID := 0;

                  when Cmd.Immediate    =>
                     Value.Parameters.Count := 254;
                     Value.Parameters.Duration_NS := 0;
                     Value.Command_ID := 16#8000_0000#;

                  when Cmd.Abort_Action =>
                     Value.Parameters.Count := 255;
                     Value.Parameters.Duration_NS := -1;
               end case;
               declare
                  Request : Cmd.Track_Request := Cmd.Exec_Track (Channel, Value);
                  Result  : constant Cmd.Track_Result := Cmd.Wait (Request, 5_000);
                  Cached  : constant Cmd.Track_Result := Cmd.Wait (Request, 0);
               begin
                  AMS_MEL_Track_Fixture.Verify (Value);
                  Require (Cmd.Status (Result) = V.Success, "valid Return outcome");
                  Require (Cmd.Value (Result) = Expected, "actual Return payload");
                  Require (Cmd.Value (Cached) = Expected, "cached Return payload");
                  Cmd.Close (Request);
               end;
            end;
         end loop;
         declare
            Invalid : Cmd.Track_Task := Task_Value;
         begin
            for Index in 1 .. 2 loop
               Invalid.Parameters.Count := (if Index = 1 then 256 else Interfaces.Unsigned_32'Last);
               begin
                  declare
                     Request : Cmd.Track_Request := Cmd.Exec_Track (Channel, Invalid);
                  begin
                     Cmd.Close (Request);
                     raise Program_Error with "invalid target count was dispatched";
                  end;
               exception
                  when AMS.MEL.Provider_Error =>
                     null;
               end;
            end loop;
         end;
         --  Automatic observer finalization also remains non-raising.
         declare
            Request : constant Cmd.Track_Request := Cmd.Exec_Track (Channel, Task_Value);
            Result  : constant Cmd.Track_Result := Cmd.Wait (Request, 5_000);
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
            Request : Cmd.Track_Request := Cmd.Exec_Track (Channel, Task_Value);
            Result  : constant Cmd.Track_Result := Cmd.Wait (Request, 5_000);
            Cached  : constant Cmd.Track_Result := Cmd.Wait (Request, 0);
         begin
            Require (Cmd.Status (Result) = V.Rejected, "ErrorOr rejection");
            Require (Cmd.Rejection_Code (Result) = Expected, "exact MEL rejection code");
            Require
              (Cmd.Description (Result) = "TrackTask rejection: " & String'(1 .. 1_024 => 'x'),
               "complete long description");
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
               Request : constant Cmd.Track_Request := Cmd.Exec_Track (Channel, Task_Value);
               Result  : constant Cmd.Track_Result := Cmd.Wait (Request, 5_000);
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
         AMS_MEL_Track_Fixture.Drain;
      end Failed;

      procedure Pending is
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-track-delayed");
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
         Ack     : Meta.Execute_Task_Ack_Stream := Meta.Open_Execute_Task_Ack (Channel, 2);
         Rep     : Meta.Task_Executing_Stream := Meta.Open_Task_Executing (Channel, 2);
      begin
         Sch.Enable (Channel);
         declare
            Request : Cmd.Track_Request := Cmd.Exec_Track (Channel, Task_Value);
         begin
            begin
               declare
                  Result : constant Cmd.Track_Result := Cmd.Wait (Request, 0);
               begin
                  Require (Cmd.Status (Result) = V.Success, "unexpected pending result");
                  raise Program_Error with "pending TrackTask did not time out";
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
            AMS_MEL_Track_Fixture.Release_Pending (1);
            declare
               Result : constant Cmd.Track_Result := Cmd.Wait (Request, 5_000);
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
            Request : Cmd.Track_Request := Cmd.Exec_Track (Channel, Task_Value);
         begin
            Cmd.Close (Request);
         end;
         declare
            Request : constant Cmd.Track_Request := Cmd.Exec_Track (Channel, Task_Value);
         begin
            --  Leave an open pending observer to non-raising finalization.
            Require (Cmd.Status (Cmd.Wait (Request, 0)) = V.Success, "unexpected completion");
         exception
            when AMS.MEL.IR.Timeout_Error =>
               null;
         end;
         Sch.Close (Channel);
         AMS.MEL.Close (Parent);
         AMS_MEL_Track_Fixture.Release_Pending (2);
         AMS_MEL_Track_Fixture.Drain;
         Meta.Close (Ack);
         Meta.Close (Rep);
      end Pending;

      procedure Admission_Limit is
         Parent  : AMS.MEL.Session :=
           AMS.MEL.Open_With_Options
             (Provider_Path, "sched-track-delayed", (Max_Async_Requests => 1));
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      begin
         Sch.Enable (Channel);
         declare
            Request : Cmd.Track_Request := Cmd.Exec_Track (Channel, Task_Value);
         begin
            begin
               declare
                  Refused : Cmd.Track_Request := Cmd.Exec_Track (Channel, Task_Value);
               begin
                  Cmd.Close (Refused);
                  raise Program_Error with "Session limit did not refuse TrackTask";
               end;
            exception
               when AMS.MEL.Resource_Exhausted =>
                  null;
            end;
            AMS_MEL_Track_Fixture.Release_Pending (1);
            Require
              (Cmd.Value (Cmd.Wait (Request, 5_000)) = V.Return_Success,
               "admitted request after refusal");
            Cmd.Close (Request);
         end;
         Sch.Close (Channel);
         AMS.MEL.Close (Parent);
         AMS_MEL_Track_Fixture.Drain;
      end Admission_Limit;

      procedure Retained is
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-track-delayed");
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      begin
         Sch.Enable (Channel);
         declare
            Request : Cmd.Track_Request := Cmd.Exec_Track (Channel, Task_Value);
         begin
            Sch.Close (Channel);
            AMS.MEL.Close (Parent);
            AMS_MEL_Track_Fixture.Release_Pending (1);
            declare
               Result : constant Cmd.Track_Result := Cmd.Wait (Request, 5_000);
            begin
               Require (Cmd.Value (Result) = V.Return_Success, "request outlived both owners");
            end;
            Cmd.Close (Request);
         end;
         AMS_MEL_Track_Fixture.Drain;
      end Retained;
   begin
      AMS_MEL_Track_Fixture.Open (Provider_Path);
      Completed ("sched-track-success", V.Return_Success);
      Completed ("sched-track-fail", V.Fail);
      Completed ("sched-track-not-supported", V.Not_Supported);
      Completed ("sched-track-not-implemented", V.Not_Implemented);
      Rejected ("sched-track-unsupported", V.Unsupported);
      Rejected ("sched-track-invalid-state", V.Invalid_State);
      Failed ("sched-track-exec-throw");
      Failed ("sched-track-future-throw");
      Failed ("sched-track-invalid-future");
      Failed ("sched-track-null");
      Failed ("sched-track-malformed");
      Failed ("sched-track-invalid-return");
      Pending;
      Admission_Limit;
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
      AMS_MEL_Track_Fixture.Close;
      Ada.Text_IO.Put_Line ("PASS safe Ada Scheduling TrackTask public interface");
   end Run;
end AMS_MEL_IR_Scheduling_Track_Tests;
