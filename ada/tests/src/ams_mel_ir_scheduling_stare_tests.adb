with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Scheduling;
with AMS.MEL.IR.Scheduling.Commands;
with AMS.MEL.IR.Scheduling.Metadata;
with Interfaces;
with AMS_MEL_Stare_Fixture;

package body AMS_MEL_IR_Scheduling_Stare_Tests is
   package Sch renames AMS.MEL.IR.Scheduling;
   package Cmd renames AMS.MEL.IR.Scheduling.Commands;
   package Meta renames AMS.MEL.IR.Scheduling.Metadata;
   package V renames AMS.MEL.IR.Channel;
   use type V.Outcome;
   use type V.Command_Return;
   use type V.Error_Code;
   use type Interfaces.Unsigned_32;

   procedure Require (Condition : Boolean; Message : String) is
   begin
      if not Condition then
         raise Program_Error with "Ada StareTask: " & Message;
      end if;
   end Require;

   Config : constant Sch.Scheduling_Config :=
     Sch.Create_Config
       (AMS.MEL.IR.Create_UCI_ID ([others => 0], "stare-channel"),
        AMS.MEL.IR.Create_UCI_ID ([others => 1], "platform"),
        AMS.MEL.IR.Create_Component_Location (1.25, -2.5, 3.75, "station", "mock"));
   Info   : constant Cmd.Task_Info :=
     (System_Time_NS             => Interfaces.Integer_64'First,
      Request_ID                 => 16#F123_4567#,
      Action                     => Cmd.Normal,
      Precedence_Within_Priority => Interfaces.Unsigned_32'Last,
      Priority                   => 16#8000_0001#);

   Task_Value : constant Cmd.Stare_Task :=
     (Command_ID => Interfaces.Unsigned_32'Last,
      Parameters =>
        (Center_Azimuth_Rad   => -0.0,
         Center_Elevation_Rad => 1.23456789012345,
         Elevation_Frame      => Cmd.Inertial,
         Azimuth_Frame        => Cmd.Aircraft,
         Duration_NS          => Interfaces.Integer_64'First),
      Info       => Info);

   procedure Run (Provider_Path : String) is
      procedure Completed (Scenario : String; Expected : V.Command_Return) is
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      begin
         --  The binding exists even before enable; eligibility is a distinct failure.
         begin
            declare
               Request : Cmd.Stare_Request := Cmd.Exec_Stare (Channel, Task_Value);
            begin
               Cmd.Close (Request);
               raise Program_Error with "Attached owner admitted StareTask";
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Sch.Enable (Channel);
         for Action in Cmd.Task_List_Action loop
            declare
               Value : Cmd.Stare_Task := Task_Value;
            begin
               Value.Info.Action := Action;
               declare
                  Request : Cmd.Stare_Request := Cmd.Exec_Stare (Channel, Value);
                  Result  : constant Cmd.Stare_Result := Cmd.Wait (Request, 5_000);
                  Cached  : constant Cmd.Stare_Result := Cmd.Wait (Request, 0);
               begin
                  Require (Cmd.Status (Result) = V.Success, "valid Return outcome");
                  Require (Cmd.Value (Result) = Expected, "actual Return payload");
                  Require (Cmd.Value (Cached) = Expected, "cached Return payload");
                  Cmd.Close (Request);
               end;
            end;
         end loop;
         --  Automatic observer finalization also remains non-raising.
         declare
            Request : constant Cmd.Stare_Request := Cmd.Exec_Stare (Channel, Task_Value);
            Result  : constant Cmd.Stare_Result := Cmd.Wait (Request, 5_000);
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
            Request : Cmd.Stare_Request := Cmd.Exec_Stare (Channel, Task_Value);
            Result  : constant Cmd.Stare_Result := Cmd.Wait (Request, 5_000);
            Cached  : constant Cmd.Stare_Result := Cmd.Wait (Request, 0);
         begin
            Require (Cmd.Status (Result) = V.Rejected, "ErrorOr rejection");
            Require (Cmd.Rejection_Code (Result) = Expected, "exact MEL rejection code");
            Require (Cmd.Description (Result)'Length = 1_045, "complete long description");
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
               Request : constant Cmd.Stare_Request := Cmd.Exec_Stare (Channel, Task_Value);
               Result  : constant Cmd.Stare_Result := Cmd.Wait (Request, 5_000);
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
         AMS_MEL_Stare_Fixture.Drain;
      end Failed;

      procedure Pending is
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-stare-delayed");
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
         Ack     : Meta.Execute_Task_Ack_Stream := Meta.Open_Execute_Task_Ack (Channel, 2);
         Rep     : Meta.Task_Executing_Stream := Meta.Open_Task_Executing (Channel, 2);
      begin
         Sch.Enable (Channel);
         declare
            Request : Cmd.Stare_Request := Cmd.Exec_Stare (Channel, Task_Value);
         begin
            begin
               declare
                  Result : constant Cmd.Stare_Result := Cmd.Wait (Request, 0);
               begin
                  Require (Cmd.Status (Result) = V.Success, "unexpected pending result");
                  raise Program_Error with "pending StareTask did not time out";
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
            AMS_MEL_Stare_Fixture.Release_Pending (1);
            declare
               Result : constant Cmd.Stare_Result := Cmd.Wait (Request, 5_000);
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
            Request : Cmd.Stare_Request := Cmd.Exec_Stare (Channel, Task_Value);
         begin
            Cmd.Close (Request);
         end;
         declare
            Request : constant Cmd.Stare_Request := Cmd.Exec_Stare (Channel, Task_Value);
         begin
            --  Leave an open pending observer to non-raising finalization.
            Require (Cmd.Status (Cmd.Wait (Request, 0)) = V.Success, "unexpected completion");
         exception
            when AMS.MEL.IR.Timeout_Error =>
               null;
         end;
         Sch.Close (Channel);
         AMS.MEL.Close (Parent);
         AMS_MEL_Stare_Fixture.Release_Pending (2);
         AMS_MEL_Stare_Fixture.Drain;
         Meta.Close (Ack);
         Meta.Close (Rep);
      end Pending;

      procedure Retained is
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-stare-delayed");
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      begin
         Sch.Enable (Channel);
         declare
            Request : Cmd.Stare_Request := Cmd.Exec_Stare (Channel, Task_Value);
         begin
            Sch.Close (Channel);
            AMS.MEL.Close (Parent);
            AMS_MEL_Stare_Fixture.Release_Pending (1);
            declare
               Result : constant Cmd.Stare_Result := Cmd.Wait (Request, 5_000);
            begin
               Require (Cmd.Value (Result) = V.Return_Success, "request outlived both owners");
            end;
            Cmd.Close (Request);
         end;
         AMS_MEL_Stare_Fixture.Drain;
      end Retained;
   begin
      AMS_MEL_Stare_Fixture.Open (Provider_Path);
      Completed ("sched-stare-success", V.Return_Success);
      Completed ("sched-stare-fail", V.Fail);
      Completed ("sched-stare-not-supported", V.Not_Supported);
      Completed ("sched-stare-not-implemented", V.Not_Implemented);
      Rejected ("sched-stare-unsupported", V.Unsupported);
      Rejected ("sched-stare-invalid-state", V.Invalid_State);
      Failed ("sched-stare-exec-throw");
      Failed ("sched-stare-future-throw");
      Failed ("sched-stare-invalid-future");
      Failed ("sched-stare-null");
      Failed ("sched-stare-malformed");
      Failed ("sched-stare-invalid-return");
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
      AMS_MEL_Stare_Fixture.Close;
      Ada.Text_IO.Put_Line ("PASS safe Ada Scheduling StareTask public interface");
   end Run;
end AMS_MEL_IR_Scheduling_Stare_Tests;
