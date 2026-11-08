with Ada.Exceptions;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.C2.Metadata;
with AMS.MEL.IR.Camera_Protection;
with AMS.MEL.IR.Camera_Protection.Testing;
with AMS.MEL.IR.Channel;

--  IRMEL-ADA-004 safe Ada contract for C2.Submit_Camera_Protection and the opt-in
--  C2.Metadata camera-protection owner against the deterministic mock provider.

package body AMS_MEL_IR_Camera_Protection_Tests is
   package CP renames AMS.MEL.IR.Camera_Protection;
   package C2 renames AMS.MEL.IR.C2;
   package Metadata renames AMS.MEL.IR.C2.Metadata;
   package Ch renames AMS.MEL.IR.Channel;
   use type CP.Protection_Response;
   use type CP.Protection_Status;
   use type CP.Outcome;
   use type CP.Error_Code;
   use type CP.Nanoseconds;
   use type AMS.MEL.IR.Counter;

   type Command_Array is array (Positive range <>) of CP.Protection_Command;

   Zero       : constant AMS.MEL.IR.UUID := [others => 0];
   Channel_ID : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Zero, "Ada camera protection channel");
   Platform   : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Zero, "Ada camera protection platform");
   Location   : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock");
   C2_Config  : constant C2.Control_Config := C2.Create_Config (Channel_ID, Platform, Location);

   procedure Require (Value : Boolean; Text : String) is
   begin
      if not Value then
         raise Program_Error with "Ada CameraProtectCmd: " & Text;
      end if;
   end Require;

   function Command
     (ID : CP.Command_ID; Time : CP.Nanoseconds; Protect : Boolean) return CP.Protection_Command
   is ((ID => ID, Response_Time_NS => Time, Protect => Protect));

   --  The mock echoes ID and Response_Time; Status is selected by the scenario.
   procedure Check_Echo
     (Request : CP.Protection_Request; Value : CP.Protection_Command; Status : CP.Protection_Status)
   is
      First  : constant CP.Protection_Result := CP.Wait (Request, 5_000);
      Second : constant CP.Protection_Result := CP.Wait (Request, 0);
      Want   : constant CP.Protection_Response :=
        (ID => Value.ID, Response_Time_NS => Value.Response_Time_NS, Status => Status);
   begin
      Require (CP.Status (First) = Ch.Success and then CP.Status (Second) = Ch.Success, "status");
      Require (CP.Response (First) = Want and then CP.Response (Second) = Want, "response");
   end Check_Echo;

   --  Both ProtectFlag values, unsigned/signed boundaries and all three CmdResp alternatives.
   procedure Test_Fidelity
     (Provider_Path : String; Scenario : String; Status : CP.Protection_Status)
   is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
   begin
      C2.Enable (Channel);
      for Value of
        Command_Array'
          [Command (16#FFFF_FFFF#, CP.Nanoseconds'First, True),
           Command (0, CP.Nanoseconds'Last, False),
           Command (16#8000_0001#, -1, True),
           Command (7, 0, False)]
      loop
         declare
            Request : CP.Protection_Request := C2.Submit_Camera_Protection (Channel, Value);
         begin
            Require (CP.Is_Open (Request), "request open");
            Check_Echo (Request, Value, Status);
            CP.Close (Request);
            Require (not CP.Is_Open (Request), "request closed");
            CP.Close (Request);
         end;
      end loop;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Fidelity;

   --  MEL rejection is distinct from Protection_Unsupported; failure categories raise.
   procedure Test_Outcomes (Provider_Path : String) is
      procedure Expect
        (Scenario : String; Code : CP.Error_Code; Text : String; Raises : Boolean := False)
      is
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      begin
         C2.Enable (Channel);
         declare
            Request : CP.Protection_Request :=
              C2.Submit_Camera_Protection (Channel, Command (9, 9, True));
         begin
            if Raises then
               begin
                  declare
                     Ignored : constant CP.Protection_Result := CP.Wait (Request, 5_000);
                  begin
                     raise Program_Error with "Ada CameraProtectCmd failure did not raise";
                  end;
               exception
                  when Error : AMS.MEL.Provider_Error =>
                     Require (Ada.Exceptions.Exception_Message (Error) = Text, "failure text");
               end;
            else
               declare
                  Result : constant CP.Protection_Result := CP.Wait (Request, 5_000);
               begin
                  Require (CP.Status (Result) = Ch.Rejected, "rejected status");
                  Require (CP.Rejection_Code (Result) = Code, "rejection code");
                  Require (CP.Description (Result) = Text, "rejection text");
               end;
            end if;
            CP.Close (Request);
         end;
         C2.Close (Channel);
         AMS.MEL.Close (Parent);
      end Expect;
      Long : String (1 .. 613) := [others => 'x'];
   begin
      Long (511) := Character'Val (16#E2#);
      Long (512) := Character'Val (16#82#);
      Long (513) := Character'Val (16#AC#);
      Long (514 .. Long'Last) := [others => 'y'];
      Expect ("no-camera-protect", Ch.Unsupported, "");
      Expect ("protect-reject", Ch.Invalid_Parameters, "camera protect rejected");
      Expect ("protect-reject-long", Ch.Insufficient_Resources, Long);
      Expect
        ("protect-null", Ch.None, "provider returned null successful CameraProtectCmdResp", True);
      Expect
        ("protect-unknown-response",
         Ch.None,
         "provider returned unknown CameraProtectCmdResp CmdResp value",
         True);
      Expect ("protect-unknown-error", Ch.None, "provider returned unknown MEL error code", True);
      Expect ("protect-future-throw", Ch.None, "mock camera protect future exception", True);
   end Test_Outcomes;

   --  Enabled lifecycle requirement, send exception and closed channel raise Provider_Error.
   procedure Test_Submission_Errors (Provider_Path : String) is
   begin
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "protect-echo");
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      begin
         begin
            declare
               Ignored : CP.Protection_Request :=
                 C2.Submit_Camera_Protection (Channel, Command (1, 1, True));
            begin
               raise Program_Error with "Ada CameraProtectCmd accepted before Enable";
            end;
         exception
            when Error : AMS.MEL.Provider_Error =>
               Require
                 (Ada.Exceptions.Exception_Message (Error) = "C2 channel is not enabled",
                  "enabled");
         end;
         C2.Close (Channel);
         begin
            declare
               Ignored : CP.Protection_Request :=
                 C2.Submit_Camera_Protection (Channel, Command (1, 1, True));
            begin
               raise Program_Error with "Ada CameraProtectCmd accepted on closed channel";
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         AMS.MEL.Close (Parent);
      end;
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "protect-send-throw");
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      begin
         C2.Enable (Channel);
         declare
            Ignored : CP.Protection_Request :=
              C2.Submit_Camera_Protection (Channel, Command (1, 1, False));
         begin
            raise Program_Error with "Ada CameraProtectCmd send exception not raised";
         end;
      exception
         when Error : AMS.MEL.Provider_Error =>
            Require
              (Ada.Exceptions.Exception_Message (Error) = "mock camera protect send exception",
               "send");
            C2.Close (Channel);
            AMS.MEL.Close (Parent);
      end;
   end Test_Submission_Errors;

   --  Held completion through the deterministic provider promise gate: timeout does not
   --  consume, Session admission refuses a second pending request, and request/C2/Session
   --  closure before completion neither cancels nor loses the result.
   procedure Test_Held (Provider_Path : String) is
      package IC renames Interfaces.C;
      use type IC.int;
      use type System.Address;
      function Dlopen (Path : IC.char_array; Flags : IC.int) return System.Address
      with Import, Convention => C, External_Name => "dlopen";
      function Dlsym (Handle : System.Address; Name : IC.char_array) return System.Address
      with Import, Convention => C, External_Name => "dlsym";
      function Dlclose (Handle : System.Address) return IC.int
      with Import, Convention => C, External_Name => "dlclose";
      type Release_Function is access function (Family, Amount : IC.unsigned) return IC.int
      with Convention => C;
      function To_Release is new Ada.Unchecked_Conversion (System.Address, Release_Function);
      Pin          : constant System.Address := Dlopen (IC.To_C (Provider_Path), 2);
      Release      : constant Release_Function :=
        To_Release (Dlsym (Pin, IC.To_C ("mock_completion_release_family")));
      Protect_Send : constant IC.unsigned := 8;

      --  Test-only completion-owner probe, CameraProtect family 7. FinalOwner is the first
      --  CameraProtectWorkerInput member and is destroyed after its CompletionPermit, so an
      --  observed owner destruction proves the Session admission slot was released.
      --  A terminal Wait only proves the result was published, not that the detached
      --  worker has dropped its permit.
      use type Interfaces.Unsigned_64;
      Protect_Probe : constant IC.unsigned := 7;
      type Counts is array (0 .. 3) of Interfaces.Unsigned_64 with Convention => C;
      type Boundary_Function is
        access function
          (Family, Op : IC.unsigned; Target : Interfaces.Unsigned_64; Values : access Counts)
           return IC.int
      with Convention => C;
      type Owner_Function is
        access function
          (Family   : IC.unsigned;
           Target   : Interfaces.Unsigned_64;
           Observed : access Interfaces.Unsigned_64) return IC.int
      with Convention => C;
      function To_Boundary is new Ada.Unchecked_Conversion (System.Address, Boundary_Function);
      function To_Owner is new Ada.Unchecked_Conversion (System.Address, Owner_Function);
      Boundary      : constant Boundary_Function :=
        To_Boundary (Dlsym (System.Null_Address, IC.To_C ("ams_mel_test_completion_boundary")));
      Owner         : constant Owner_Function :=
        To_Owner (Dlsym (System.Null_Address, IC.To_C ("ams_mel_test_completion_owner")));
      Base          : Interfaces.Unsigned_64;

      procedure Await_Owners (Target : Interfaces.Unsigned_64) is
         Observed : aliased Interfaces.Unsigned_64 := 0;
      begin
         Require
           (Owner (Protect_Probe, Target, Observed'Access) = 1, "CameraProtect owner quiescence");
         Require (Observed = Target, "exact CameraProtect owner count");
      end Await_Owners;

      --  Quiesce every earlier CameraProtect owner and return the created count.
      function Quiesce return Interfaces.Unsigned_64 is
         Values : aliased Counts := [others => 0];
      begin
         Require
           (Boundary (Protect_Probe, 3, 0, Values'Access) = 1, "CameraProtect probe snapshot");
         Await_Owners (Values (2));
         return Values (2);
      end Quiesce;

      procedure Expect_Timeout (Request : CP.Protection_Request) is
      begin
         declare
            Ignored : constant CP.Protection_Result := CP.Wait (Request, 0);
         begin
            raise Program_Error with "Ada CameraProtectCmd Wait(0) did not time out";
         end;
      exception
         when AMS.MEL.IR.Timeout_Error =>
            null;
      end Expect_Timeout;
   begin
      Require (Pin /= System.Null_Address and then Release /= null, "provider gate");
      Require (Boundary /= null and then Owner /= null, "test-only probe binding");
      declare
         Parent  : AMS.MEL.Session :=
           AMS.MEL.Open_With_Options
             (Provider_Path, "protect-held", Options => (Max_Async_Requests => 1));
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      begin
         C2.Enable (Channel);
         Base := Quiesce;
         declare
            Request : CP.Protection_Request :=
              C2.Submit_Camera_Protection (Channel, Command (21, 210, True));
         begin
            Expect_Timeout (Request);
            begin
               declare
                  Refused : CP.Protection_Request :=
                    C2.Submit_Camera_Protection (Channel, Command (22, 220, False));
               begin
                  CP.Close (Refused);
                  raise Program_Error with "Ada CameraProtectCmd admission not enforced";
               end;
            exception
               when Error : AMS.MEL.Resource_Exhausted =>
                  Require
                    (Ada.Exceptions.Exception_Message (Error) = "async request limit reached",
                     "admission");
            end;
            Expect_Timeout (Request);
            Require (Release (Protect_Send, 1) = 1, "release");
            Check_Echo (Request, Command (21, 210, True), CP.Protection_OK);
            CP.Close (Request);
         end;
         --  Request 21 and the refused 22 (a refused submission still creates one owner)
         --  are both destroyed, so the one-slot Session permit is free before reuse.
         Await_Owners (Base + 2);
         declare
            Observer : CP.Protection_Request :=
              C2.Submit_Camera_Protection (Channel, Command (23, -230, False));
         begin
            C2.Close (Channel);
            AMS.MEL.Close (Parent);
            Expect_Timeout (Observer);
            Require (Release (Protect_Send, 1) = 1, "pending release");
            Check_Echo (Observer, Command (23, -230, False), CP.Protection_OK);
            CP.Close (Observer);
         end;
      end;
      --  Public request Close before completion is not cancellation; finalization is silent.
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "protect-held");
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      begin
         C2.Enable (Channel);
         declare
            Closed : CP.Protection_Request :=
              C2.Submit_Camera_Protection (Channel, Command (24, 24, True));
         begin
            CP.Close (Closed);
         end;
         declare
            Finalized : constant CP.Protection_Request :=
              C2.Submit_Camera_Protection (Channel, Command (25, 25, True));
         begin
            Require (CP.Is_Open (Finalized), "finalized request open");
         end;
         C2.Close (Channel);
         AMS.MEL.Close (Parent);
         Require (Release (Protect_Send, 2) = 1, "closed release");
      end;
      Require (Dlclose (Pin) = 0, "provider unpin");
   end Test_Held;

   --  Opt-in C2 CameraProtectCmdResp owner: synchronous registration delivery, all three
   --  alternatives in arrival order, boundaries, counters, one-shot, stopped state after C2
   --  Close, and values independent of owner/C2/Session.
   procedure Test_Callback (Provider_Path : String) is
      Kept : CP.Protection_Response;
   begin
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "protect-callback-rich");
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
         Owner   : Metadata.Camera_Protection_Stream :=
           Metadata.Open_Camera_Protection (Channel, 4);
         Counts  : constant Metadata.Metadata_Counters :=
           Metadata.Camera_Protection_Counters (Owner);
      begin
         Require (Metadata.Is_Open (Owner), "owner open");
         Require
           (Counts.Events_Received = 3
            and then Counts.Events_Dropped_Queue_Full = 0
            and then Counts.Malformed_Or_Unsupported = 0,
            "callback counters");
         Require
           (Metadata.Receive_Camera_Protection (Owner)
            = (ID => 1, Response_Time_NS => CP.Nanoseconds'First, Status => CP.Protection_OK),
            "first response");
         Require
           (Metadata.Receive_Camera_Protection (Owner)
            = (ID               => 16#FFFF_FFFF#,
               Response_Time_NS => CP.Nanoseconds'Last,
               Status           => CP.Protection_Unavailable),
            "second response");
         Kept := Metadata.Receive_Camera_Protection (Owner);
         begin
            declare
               Ignored : constant CP.Protection_Response :=
                 Metadata.Receive_Camera_Protection (Owner, 1);
            begin
               raise Program_Error with "Ada CameraProtectCmdResp empty queue did not time out";
            end;
         exception
            when AMS.MEL.IR.Timeout_Error =>
               null;
         end;
         begin
            declare
               Again : Metadata.Camera_Protection_Stream :=
                 Metadata.Open_Camera_Protection (Channel, 4);
            begin
               Metadata.Close (Again);
               raise Program_Error with "Ada CameraProtectCmdResp registered twice";
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         C2.Close (Channel);
         begin
            declare
               Ignored : constant CP.Protection_Response :=
                 Metadata.Receive_Camera_Protection (Owner, 1_000);
            begin
               raise Program_Error with "Ada CameraProtectCmdResp did not stop";
            end;
         exception
            when AMS.MEL.IR.Stream_Stopped =>
               null;
         end;
         Metadata.Close (Owner);
         Require (not Metadata.Is_Open (Owner), "owner closed");
         Metadata.Close (Owner);
         AMS.MEL.Close (Parent);
      end;
      Require
        (Kept = (ID => 0, Response_Time_NS => -1, Status => CP.Protection_Unsupported),
         "response independent of owners");
   end Test_Callback;

   --  Registration Fail and a registration exception both raise Provider_Error, never
   --  Not_Supported, and consume the one-shot attempt.
   procedure Expect_Registration_Failure (Provider_Path : String; Scenario : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
   begin
      begin
         declare
            Ignored : Metadata.Camera_Protection_Stream :=
              Metadata.Open_Camera_Protection (Channel, 2);
         begin
            raise Program_Error with "Ada CameraProtectCmdResp failure not reported";
         end;
      exception
         when CP.Not_Supported =>
            raise Program_Error with "Ada CameraProtectCmdResp failure reported NotSupported";
         when AMS.MEL.Provider_Error =>
            null;
      end;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Expect_Registration_Failure;

   --  Without camera self-protection the opt-in owner reports Not_Supported while the existing
   --  C2 metadata owner and channel lifecycle keep working.
   procedure Test_Callback_Not_Supported (Provider_Path : String) is
      Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "metadata-rich");
      Channel  : C2.Control_Channel := C2.Open (Parent, C2_Config);
      Existing : Metadata.Metadata_Stream := Metadata.Open (Channel, 8);
   begin
      begin
         declare
            Ignored : Metadata.Camera_Protection_Stream :=
              Metadata.Open_Camera_Protection (Channel, 2);
         begin
            raise Program_Error with "Ada CameraProtectCmdResp NotSupported not reported";
         end;
      exception
         when CP.Not_Supported =>
            null;
      end;
      declare
         Ignored : constant Metadata.Metadata_Event := Metadata.Receive (Existing, 1_000);
      begin
         null;
      end;
      Metadata.Close (Existing);
      C2.Enable (Channel);
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Callback_Not_Supported;

   --  Synchronous delivery inside send; metadata is independent of the request future.
   procedure Test_Callback_Synchronous (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "protect-sync");
      Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      Owner   : Metadata.Camera_Protection_Stream := Metadata.Open_Camera_Protection (Channel, 2);
   begin
      C2.Enable (Channel);
      declare
         Request : CP.Protection_Request :=
           C2.Submit_Camera_Protection (Channel, Command (16#5151#, 31, True));
      begin
         Require
           (Metadata.Receive_Camera_Protection (Owner)
            = (ID => 16#5151#, Response_Time_NS => -77, Status => CP.Protection_Unavailable),
            "synchronous callback");
         Check_Echo (Request, Command (16#5151#, 31, True), CP.Protection_OK);
         CP.Close (Request);
      end;
      Metadata.Close (Owner);
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Callback_Synchronous;

   procedure Run (Provider_Path : String) is
   begin
      CP.Testing.Check_Representation;
      Test_Fidelity (Provider_Path, "protect-echo", CP.Protection_OK);
      Test_Fidelity (Provider_Path, "protect-unavailable", CP.Protection_Unavailable);
      Test_Fidelity (Provider_Path, "protect-unsupported", CP.Protection_Unsupported);
      Test_Outcomes (Provider_Path);
      Test_Submission_Errors (Provider_Path);
      Test_Held (Provider_Path);
      Test_Callback (Provider_Path);
      Test_Callback_Not_Supported (Provider_Path);
      Expect_Registration_Failure (Provider_Path, "protect-register-fail");
      Expect_Registration_Failure (Provider_Path, "protect-register-throw");
      Test_Callback_Synchronous (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada IR C2 CameraProtectCmd and CameraProtectCmdResp contract");
   end Run;
end AMS_MEL_IR_Camera_Protection_Tests;
