with Ada.Environment_Variables;
with Ada.Directories;
with Ada.Exceptions;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.Erase;
with AMS.MEL.IR.Erase.Testing;
with AMS.MEL.IR.Channel;

--  IRMEL-ADA-007 safe Ada EraseCommand contract, deterministic mock only.

package body AMS_MEL_IR_Erase_Tests is
   package CP renames AMS.MEL.IR.Erase;
   package C2 renames AMS.MEL.IR.C2;
   package Ch renames AMS.MEL.IR.Channel;
   use type CP.Erase_Type;
   use type CP.Command_ID;
   use type CP.Outcome;
   use type CP.Error_Code;

   Zero       : constant AMS.MEL.IR.UUID := [others => 0];
   Channel_ID : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Zero, "Ada eraseion channel");
   Platform   : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Zero, "Ada eraseion platform");
   Location   : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock");
   C2_Config  : constant C2.Control_Config := C2.Create_Config (Channel_ID, Platform, Location);

   procedure Require (Value : Boolean; Text : String) is
   begin
      if not Value then
         raise Program_Error with "Ada EraseCommand: " & Text;
      end if;
   end Require;

   function Command (ID : CP.Command_ID; Operation : CP.Erase_Type) return CP.Erase_Command
   is (CP.Create_Command (ID, Operation));

   procedure Check_Delivered (Provider_Path : String; Value : CP.Erase_Command) is
      package IC renames Interfaces.C;
      use type IC.int;
      use type System.Address;
      use type Interfaces.Unsigned_64;
      function Dlopen (Path : IC.char_array; Flags : IC.int) return System.Address
      with Import, Convention => C, External_Name => "dlopen";
      function Dlsym (Handle : System.Address; Name : IC.char_array) return System.Address
      with Import, Convention => C, External_Name => "dlsym";
      function Dlclose (Handle : System.Address) return IC.int
      with Import, Convention => C, External_Name => "dlclose";
      type Fields is array (0 .. 1) of Interfaces.Unsigned_64 with Convention => C;
      type Last_Function is
        access function (Values : access Fields; Sends : access IC.unsigned) return IC.int
      with Convention => C;
      function To_Last is new Ada.Unchecked_Conversion (System.Address, Last_Function);
      Pin    : constant System.Address := Dlopen (IC.To_C (Provider_Path), 2);
      Last   : constant Last_Function := To_Last (Dlsym (Pin, IC.To_C ("mock_erase_last_command")));
      Actual : aliased Fields := [others => 0];
   begin
      Require (Pin /= System.Null_Address and then Last /= null, "mock command observer");
      Require (Last (Actual'Access, null) = 1, "mock received command");
      Require (Actual (0) = Interfaces.Unsigned_64 (CP.ID (Value)), "delivered command ID");
      Require (Actual (1) = CP.Erase_Type'Pos (CP.Operation (Value)), "delivered erase type");
      Require (Dlclose (Pin) = 0, "command observer unpin");
   end Check_Delivered;

   procedure Check_Echo (Request : CP.Erase_Request; Operation : CP.Erase_Type) is
      First  : constant CP.Erase_Result := CP.Wait (Request, 5_000);
      Second : constant CP.Erase_Result := CP.Wait (Request, 0);
   begin
      Require (CP.Status (First) = Ch.Success and then CP.Status (Second) = Ch.Success, "status");
      Require (CP.Response (First) = Operation and then CP.Response (Second) = Operation, "type");
   end Check_Echo;

   procedure Test_Fidelity (Provider_Path : String) is
      type IDs is array (Positive range <>) of CP.Command_ID;
      Values : constant IDs := [0, 16#FFFF_FFFF#, 16#8000_0001#, 16#FEDC_BA98#];
   begin
      for Operation in CP.Erase_Type loop
         declare
            Parent  : constant AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "erase-echo");
            Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
         begin
            C2.Enable (Channel);
            for Value of Values loop
               declare
                  C       : constant CP.Erase_Command := Command (Value, Operation);
                  Request : CP.Erase_Request := C2.Submit_Erase_Command (Channel, C);
               begin
                  Require
                    (CP.ID (C) = Value and then CP.Operation (C) = Operation, "command value");
                  Check_Delivered (Provider_Path, C);
                  Check_Echo (Request, Operation);
                  CP.Close (Request);
                  CP.Close (Request);
                  Require (not CP.Is_Open (Request), "closed");
               end;
            end loop;
         end;
         declare
            Scenario : constant String :=
              "erase-response-"
              & Character'Val (Character'Pos ('0') + CP.Erase_Type'Pos (Operation));
            Parent   : constant AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
            Channel  : C2.Control_Channel := C2.Open (Parent, C2_Config);
         begin
            C2.Enable (Channel);
            declare
               Different : constant CP.Erase_Type :=
                 CP.Erase_Type'Val ((CP.Erase_Type'Pos (Operation) + 1) mod 4);
               Request   : constant CP.Erase_Request :=
                 C2.Submit_Erase_Command (Channel, Command (16#FFFF_FFFF#, Different));
            begin
               Check_Echo (Request, Operation);
            end;
         end;
      end loop;
   end Test_Fidelity;

   procedure Test_Outcomes (Provider_Path : String) is
      procedure Expect
        (Scenario : String; Code : CP.Error_Code; Text : String; Raises : Boolean := False)
      is
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      begin
         C2.Enable (Channel);
         declare
            Request : CP.Erase_Request :=
              C2.Submit_Erase_Command (Channel, Command (9, CP.Disable));
         begin
            if Raises then
               begin
                  declare
                     Ignored : constant CP.Erase_Result := CP.Wait (Request, 5_000);
                  begin
                     raise Program_Error with "Ada EraseCommand failure did not raise";
                  end;
               exception
                  when Error : AMS.MEL.Provider_Error =>
                     Require (Ada.Exceptions.Exception_Message (Error) = Text, "failure text");
               end;
            else
               declare
                  Result : constant CP.Erase_Result := CP.Wait (Request, 5_000);
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
      Expect ("no-erase", Ch.Unsupported, "");
      Expect ("erase-reject", Ch.Invalid_Parameters, "erase rejected");
      Expect ("erase-reject-long", Ch.Insufficient_Resources, Long);
      Expect
        ("erase-reject-nul",
         Ch.None,
         "provider rejection description was invalid UTF-8 or contained NUL",
         True);
      Expect
        ("erase-reject-utf8",
         Ch.None,
         "provider rejection description was invalid UTF-8 or contained NUL",
         True);
      Expect ("erase-null", Ch.None, "provider returned null successful EraseCommandType", True);
      Expect
        ("erase-response-99", Ch.None, "provider returned unknown EraseCommandType value", True);
      Expect
        ("erase-response-4", Ch.None, "provider returned unknown EraseCommandType value", True);
      Expect
        ("erase-response-4294967295",
         Ch.None,
         "provider returned unknown EraseCommandType value",
         True);
      Expect ("erase-unknown-error", Ch.None, "provider returned unknown MEL error code", True);
      Expect ("erase-future-throw", Ch.None, "mock erase future exception", True);
   end Test_Outcomes;

   --  Enabled lifecycle requirement, send exception and closed channel raise Provider_Error.
   procedure Test_Submission_Errors (Provider_Path : String) is
   begin
      declare
         Parent  : constant AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "erase-invalid-future");
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      begin
         C2.Enable (Channel);
         begin
            declare
               Unexpected : CP.Erase_Request :=
                 C2.Submit_Erase_Command (Channel, Command (0, CP.Disable));
            begin
               CP.Close (Unexpected);
               raise Program_Error with "invalid erase future accepted";
            end;
         exception
            when Error : AMS.MEL.Provider_Error =>
               Require
                 (Ada.Exceptions.Exception_Message (Error)
                  = "provider returned an invalid EraseCommand future",
                  "invalid future");
         end;
      end;
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "erase-echo");
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      begin
         begin
            declare
               Ignored : CP.Erase_Request :=
                 C2.Submit_Erase_Command (Channel, Command (1, CP.Disable));
            begin
               raise Program_Error with "Ada EraseCommand accepted before Enable";
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
               Ignored : CP.Erase_Request :=
                 C2.Submit_Erase_Command (Channel, Command (1, CP.Disable));
            begin
               raise Program_Error with "Ada EraseCommand accepted on closed channel";
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         AMS.MEL.Close (Parent);
      end;
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "erase-send-throw");
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      begin
         C2.Enable (Channel);
         declare
            Ignored : CP.Erase_Request :=
              C2.Submit_Erase_Command (Channel, Command (1, CP.Disable));
         begin
            raise Program_Error with "Ada EraseCommand send exception not raised";
         end;
      exception
         when Error : AMS.MEL.Provider_Error =>
            Require
              (Ada.Exceptions.Exception_Message (Error) = "mock erase send exception", "send");
            C2.Close (Channel);
            AMS.MEL.Close (Parent);
      end;
   end Test_Submission_Errors;

   --  Held completion through the deterministic provider promise gate: timeout does not
   --  consume, Session admission refuses a second pending request, and request/C2/Session
   --  closure before completion neither cancels nor loses the result.
   procedure Quiesce is
      package IC renames Interfaces.C;
      use type IC.int;
      type Counts is array (0 .. 3) of Interfaces.Unsigned_64 with Convention => C;
      function Dlsym (Handle : System.Address; Name : IC.char_array) return System.Address
      with Import, Convention => C, External_Name => "dlsym";
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
      Boundary : constant Boundary_Function :=
        To_Boundary (Dlsym (System.Null_Address, IC.To_C ("ams_mel_test_completion_boundary")));
      Owner    : constant Owner_Function :=
        To_Owner (Dlsym (System.Null_Address, IC.To_C ("ams_mel_test_completion_owner")));
      Values   : aliased Counts := [others => 0];
      Observed : aliased Interfaces.Unsigned_64 := 0;
   begin
      Require (Boundary /= null and then Owner /= null, "test-only probe binding");
      Require (Boundary (10, 3, 0, Values'Access) = 1, "snapshot");
      Require (Owner (10, Values (2), Observed'Access) = 1, "Erase owner quiescence");
   end Quiesce;

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
      Pin        : constant System.Address := Dlopen (IC.To_C (Provider_Path), 2);
      Release    : constant Release_Function :=
        To_Release (Dlsym (Pin, IC.To_C ("mock_completion_release_family")));
      Erase_Send : constant IC.unsigned := 11;

      procedure Expect_Timeout (Request : CP.Erase_Request) is
      begin
         declare
            Ignored : constant CP.Erase_Result := CP.Wait (Request, 0);
         begin
            raise Program_Error with "Ada EraseCommand Wait(0) did not time out";
         end;
      exception
         when AMS.MEL.IR.Timeout_Error =>
            null;
      end Expect_Timeout;
   begin
      Require (Pin /= System.Null_Address and then Release /= null, "provider gate");
      declare
         Parent  : AMS.MEL.Session :=
           AMS.MEL.Open_With_Options
             (Provider_Path, "erase-held", Options => (Max_Async_Requests => 1));
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
         Other   : constant AMS.MEL.Session :=
           AMS.MEL.Open_With_Options
             (Provider_Path, "erase-held", Options => (Max_Async_Requests => 1));
         Second  : C2.Control_Channel := C2.Open (Other, C2_Config);
      begin
         C2.Enable (Channel);
         C2.Enable (Second);
         declare
            Request : CP.Erase_Request :=
              C2.Submit_Erase_Command (Channel, Command (21, CP.Disable));
         begin
            Expect_Timeout (Request);
            begin
               declare
                  Refused : CP.Erase_Request :=
                    C2.Submit_Erase_Command (Channel, Command (22, CP.Disable));
               begin
                  CP.Close (Refused);
                  raise Program_Error with "Ada EraseCommand admission not enforced";
               end;
            exception
               when Error : AMS.MEL.Resource_Exhausted =>
                  Require
                    (Ada.Exceptions.Exception_Message (Error) = "async request limit reached",
                     "admission");
            end;
            Expect_Timeout (Request);
            declare
               Independent : constant CP.Erase_Request :=
                 C2.Submit_Erase_Command (Second, Command (31, CP.Disable));
            begin
               Expect_Timeout (Independent);
               Require (Release (Erase_Send, 2) = 1, "release independent Sessions");
               Check_Echo (Independent, CP.Disable);
            end;
            Check_Echo (Request, CP.Disable);
            CP.Close (Request);
            Quiesce;
         end;
         declare
            Observer : CP.Erase_Request :=
              C2.Submit_Erase_Command (Channel, Command (23, CP.Disable));
         begin
            C2.Close (Channel);
            AMS.MEL.Close (Parent);
            Expect_Timeout (Observer);
            Require (Release (Erase_Send, 1) = 1, "pending release");
            Check_Echo (Observer, CP.Disable);
            CP.Close (Observer);
         end;
      end;
      --  Public request Close before completion is not cancellation; finalization is silent.
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "erase-held");
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      begin
         C2.Enable (Channel);
         declare
            Closed : CP.Erase_Request :=
              C2.Submit_Erase_Command (Channel, Command (24, CP.Disable));
         begin
            CP.Close (Closed);
         end;
         declare
            Finalized : constant CP.Erase_Request :=
              C2.Submit_Erase_Command (Channel, Command (25, CP.Disable));
         begin
            Require (CP.Is_Open (Finalized), "finalized request open");
         end;
         C2.Close (Channel);
         AMS.MEL.Close (Parent);
         Require (Release (Erase_Send, 2) = 1, "closed release");
      end;
      Quiesce;
      Require (Dlclose (Pin) = 0, "provider unpin");
   end Test_Held;

   --  Opt-in C2 EraseCommandResp owner: synchronous registration delivery, all three
   --  alternatives in arrival order, boundaries, counters, one-shot, stopped state after C2
   --  Close, and values independent of owner/C2/Session.
   --  Isolated processes intentionally retain uncertain provider graphs. Never
   --  execute these failpoints in the full smoke process or retry the send.
   procedure Run_Isolated (Provider_Path : String; Mode : String) is
      package IC renames Interfaces.C;
      use type IC.int;
      use type System.Address;
      function Dlopen (Path : IC.char_array; Flags : IC.int) return System.Address
      with Import, Convention => C, External_Name => "dlopen";
      function Dlsym (Handle : System.Address; Name : IC.char_array) return System.Address
      with Import, Convention => C, External_Name => "dlsym";
      type Release_Function is access function (Family, Amount : IC.unsigned) return IC.int
      with Convention => C;
      function To_Release is new Ada.Unchecked_Conversion (System.Address, Release_Function);
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "erase-held");
      Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      Pin     : constant System.Address := Dlopen (IC.To_C (Provider_Path), 2);
      Release : constant Release_Function :=
        To_Release (Dlsym (Pin, IC.To_C ("mock_completion_release_family")));
      Log     : constant String := Ada.Environment_Variables.Value ("AMS_MEL_TEST_LIFETIME_LOG");
      function Occurrences (Event : String) return Natural is
         File  : Ada.Text_IO.File_Type;
         Count : Natural := 0;
      begin
         Ada.Text_IO.Open (File, Ada.Text_IO.In_File, Log);
         while not Ada.Text_IO.End_Of_File (File) loop
            if Ada.Text_IO.Get_Line (File) = Event then
               Count := Count + 1;
            end if;
         end loop;
         Ada.Text_IO.Close (File);
         return Count;
      end Occurrences;
   begin
      Require (Pin /= System.Null_Address and then Release /= null, "isolated mock pin");
      C2.Enable (Channel);
      if Mode = "allocation" or else Mode = "worker-launch" then
         Ada.Environment_Variables.Set ("AMS_MEL_TEST_ERASE_POST_SEND_FAILURE", Mode);
         begin
            declare
               Unexpected : CP.Erase_Request :=
                 C2.Submit_Erase_Command (Channel, Command (16#8000_0001#, CP.Disable));
            begin
               CP.Close (Unexpected);
               raise Program_Error with "post-send failure accepted";
            end;
         exception
            when Error : AMS.MEL.Provider_Error =>
               Require
                 (Ada.Exceptions.Exception_Message (Error)
                  = (if Mode = "allocation"
                     then "facade allocation failed after provider send"
                     else "facade worker launch failed"),
                  "post-send diagnostic");
         end;
         Ada.Environment_Variables.Clear ("AMS_MEL_TEST_ERASE_POST_SEND_FAILURE");
         C2.Close (Channel);
         AMS.MEL.Close (Parent);
         Require (Occurrences ("c2_erase_sent") = 1, "exactly one provider send");
         Require (Occurrences ("c2_channel_destroyed") = 0, "pending channel retained");
         Require (Occurrences ("control_destroyed") = 0, "pending Session retained");
      elsif Mode = "finish-failure" then
         Ada.Environment_Variables.Set ("AMS_MEL_TEST_C2_FINISH_FAILURE", "exception");
         declare
            Request : CP.Erase_Request :=
              C2.Submit_Erase_Command (Channel, Command (0, CP.CDE_AND_Zeroize));
         begin
            C2.Close (Channel);
            AMS.MEL.Close (Parent);
            Require (Release (11, 1) = 1, "release cleanup failure");
            for Attempt in 1 .. 2 loop
               begin
                  declare
                     Ignored : constant CP.Erase_Result := CP.Wait (Request, 5_000);
                  begin
                     raise Program_Error with "cleanup failure returned success";
                  end;
               exception
                  when Error : AMS.MEL.Provider_Error =>
                     Require
                       (Ada.Exceptions.Exception_Message (Error) = "deferred C2 cleanup failed",
                        "cleanup failure cached");
               end;
            end loop;
            CP.Close (Request);
         end;
         Require (Occurrences ("c2_erase_sent") = 1, "cleanup never replays command");
      else
         raise Program_Error with "unknown isolated erase test mode";
      end if;
      Require (Occurrences ("library_unloaded") = 0, "uncertain graph never unloaded");
      --  Keep the provider pin through process exit, matching native isolated tests.
      Ada.Environment_Variables.Clear ("AMS_MEL_TEST_LIFETIME_LOG");
      Ada.Directories.Delete_File (Log);
      Ada.Text_IO.Put_Line ("PASS: Ada isolated erase " & Mode);
   end Run_Isolated;

   procedure Run (Provider_Path : String) is
   begin
      CP.Testing.Check_Representation;
      Test_Fidelity (Provider_Path);
      Test_Outcomes (Provider_Path);
      Test_Submission_Errors (Provider_Path);
      Test_Held (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada mock EraseCommand contract");
   end Run;
end AMS_MEL_IR_Erase_Tests;
