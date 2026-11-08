pragma Validity_Checks ("F");
--  Only optional floating validity checks are disabled in this fixture, so the
--  intentional IEEE signed-zero and infinity inputs can be constructed and
--  copied; all other validity and range checks remain active.

with Ada.Directories;
with Ada.Environment_Variables;
with Ada.Exceptions;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with Interfaces;
with Interfaces.C;
with System;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.C2.System_Data;
with AMS.MEL.IR.C2.System_Data.Testing;
with AMS.MEL.IR.Track;
with AMS.MEL.IR.Track.System_Data;

--  IRMEL-ADA-008 safe Ada C2 SystemTrackDataResponse contract, deterministic
--  mock only. The Track SystemTrackDataResponse tests are separate and unchanged.

package body AMS_MEL_IR_C2_System_Data_Tests is
   package C2 renames AMS.MEL.IR.C2;
   package CS renames AMS.MEL.IR.C2.System_Data;
   package Trk renames AMS.MEL.IR.Track;
   package TS renames AMS.MEL.IR.Track.System_Data;
   package IC renames Interfaces.C;
   use type CS.Response_Outcome;
   use type CS.Response_Error_Code;
   use type CS.Command_State;
   use type CS.Cannot_Comply;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Unsigned_64;
   use type IC.int;
   use type System.Address;

   Zero       : constant AMS.MEL.IR.UUID := [others => 0];
   Channel_ID : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "Ada C2 channel");
   Platform   : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "Ada platform");
   Location   : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock");
   C2_Config  : constant C2.Control_Config := C2.Create_Config (Channel_ID, Platform, Location);
   Track_ID   : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "Ada track");
   Track_Cfg  : constant Trk.Track_Config := Trk.Create_Config (Track_ID, Platform, Location);

   C2_Response_Probe : constant IC.unsigned := 11;
   Track_Probe       : constant IC.unsigned := 5;
   C2_Response_Send  : constant IC.unsigned := 12;
   Track_Send        : constant IC.unsigned := 5;

   procedure Require (Value : Boolean; Text : String) is
   begin
      if not Value then
         raise Program_Error with "Ada C2 SystemTrackDataResponse: " & Text;
      end if;
   end Require;

   Rich : constant CS.System_Track_Data_Response :=
     (System_Time_NS       => -8_765_432_109_876,
      Command_ID           => 16#F123_4567#,
      Request_ID           => 16#E234_5678#,
      Track_ID             => 16#D345_6789#,
      Range_M              => 123_456.75,
      Range_Rate_MPS       => -456.125,
      Range_Error_M        => 12.5,
      Range_Rate_Error_MPS => -0.875,
      Az_El_Valid          => True,
      Inertial_Az_El       => (Azimuth_Rad => -1.25, Elevation_Rad => 0.625),
      Az_El_Error          => (Azimuth_Rad => 0.03125, Elevation_Rad => -0.015625),
      Range_Valid          => True);

   Accepted_Text : constant String :=
     "C2 system response accepted " & Character'Val (16#C2#) & Character'Val (16#B5#);

   function Dlopen (Path : IC.char_array; Flags : IC.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function Dlsym (Handle : System.Address; Name : IC.char_array) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   function Dlclose (Handle : System.Address) return IC.int
   with Import, Convention => C, External_Name => "dlclose";

   type Fields is array (0 .. 13) of Interfaces.Unsigned_64 with Convention => C;
   type Last_Function is
     access function (Values : access Fields; Sends : access IC.unsigned) return IC.int
   with Convention => C;
   type Release_Function is access function (Family, Amount : IC.unsigned) return IC.int
   with Convention => C;
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
   function To_Last is new Ada.Unchecked_Conversion (System.Address, Last_Function);
   function To_Release is new Ada.Unchecked_Conversion (System.Address, Release_Function);
   function To_Boundary is new Ada.Unchecked_Conversion (System.Address, Boundary_Function);
   function To_Owner is new Ada.Unchecked_Conversion (System.Address, Owner_Function);
   function Bits is new Ada.Unchecked_Conversion (Long_Float, Interfaces.Unsigned_64);
   function From_Bits is new Ada.Unchecked_Conversion (Interfaces.Unsigned_64, Long_Float);

   --  Exact delivered fields observed in the mock. Doubles are compared as
   --  IEEE bit patterns, so signed zero and infinities are distinguished.
   procedure Check_Delivered (Provider_Path : String; Value : CS.System_Track_Data_Response) is
      Pin  : constant System.Address := Dlopen (IC.To_C (Provider_Path), 2);
      Last : constant Last_Function :=
        To_Last (Dlsym (Pin, IC.To_C ("mock_c2_system_response_last")));
      F    : aliased Fields := [others => 0];
      function Flag (B : Boolean) return Interfaces.Unsigned_64
      is (if B then 1 else 0);
   begin
      Require (Pin /= System.Null_Address and then Last /= null, "mock observer");
      Require (Last (F'Access, null) = 1, "mock received response");
      Require (F (0) = Interfaces.Unsigned_64'Mod (Value.System_Time_NS), "delivered system time");
      Require (F (1) = Interfaces.Unsigned_64 (Value.Command_ID), "delivered command ID");
      Require (F (2) = Interfaces.Unsigned_64 (Value.Request_ID), "delivered request ID");
      Require (F (3) = Interfaces.Unsigned_64 (Value.Track_ID), "delivered track ID");
      Require (F (4) = Bits (Value.Range_M), "delivered range");
      Require (F (5) = Bits (Value.Range_Rate_MPS), "delivered range rate");
      Require (F (6) = Bits (Value.Range_Error_M), "delivered range error");
      Require (F (7) = Bits (Value.Range_Rate_Error_MPS), "delivered range-rate error");
      Require (F (8) = Flag (Value.Az_El_Valid), "delivered AzElValid");
      Require (F (9) = Flag (Value.Range_Valid), "delivered rangeValid");
      Require (F (10) = Bits (Value.Inertial_Az_El.Azimuth_Rad), "delivered inertial azimuth");
      Require (F (11) = Bits (Value.Inertial_Az_El.Elevation_Rad), "delivered inertial elevation");
      Require (F (12) = Bits (Value.Az_El_Error.Azimuth_Rad), "delivered azimuth error");
      Require (F (13) = Bits (Value.Az_El_Error.Elevation_Rad), "delivered elevation error");
      Require (Dlclose (Pin) = 0, "observer unpin");
   end Check_Delivered;

   function Sends (Provider_Path : String) return Natural is
      Pin     : constant System.Address := Dlopen (IC.To_C (Provider_Path), 2);
      Last    : constant Last_Function :=
        To_Last (Dlsym (Pin, IC.To_C ("mock_c2_system_response_last")));
      Count   : aliased IC.unsigned := 0;
      Ignored : IC.int;
   begin
      Require (Pin /= System.Null_Address and then Last /= null, "mock send counter");
      Ignored := Last (null, Count'Access);
      Require (Dlclose (Pin) = 0, "counter unpin");
      return Natural (Count);
   end Sends;

   function Release (Provider_Path : String; Family, Amount : IC.unsigned) return IC.int is
      Pin     : constant System.Address := Dlopen (IC.To_C (Provider_Path), 2);
      Release : constant Release_Function :=
        To_Release (Dlsym (Pin, IC.To_C ("mock_completion_release_family")));
      Result  : IC.int;
   begin
      Require (Pin /= System.Null_Address and then Release /= null, "provider gate");
      Result := Release (Family, Amount);
      Require (Dlclose (Pin) = 0, "gate unpin");
      return Result;
   end Release;

   --  Completion-owner probe: quiesce every earlier owner of a family and
   --  return created/destroyed counts. Bound in the test facade only.
   function Probe (Family : IC.unsigned; Index : Natural) return Interfaces.Unsigned_64 is
      Boundary : constant Boundary_Function :=
        To_Boundary (Dlsym (System.Null_Address, IC.To_C ("ams_mel_test_completion_boundary")));
      Values   : aliased Counts := [others => 0];
   begin
      Require (Boundary /= null, "test-only probe binding");
      Require (Boundary (Family, 3, 0, Values'Access) = 1, "probe snapshot");
      return Values (Index);
   end Probe;

   procedure Await_Owners (Family : IC.unsigned; Target : Interfaces.Unsigned_64) is
      Owner    : constant Owner_Function :=
        To_Owner (Dlsym (System.Null_Address, IC.To_C ("ams_mel_test_completion_owner")));
      Observed : aliased Interfaces.Unsigned_64 := 0;
   begin
      Require (Owner /= null, "test-only owner binding");
      Require (Owner (Family, Target, Observed'Access) = 1, "owner quiescence");
      Require (Observed = Target, "exact owner count");
   end Await_Owners;

   function Quiesce (Family : IC.unsigned) return Interfaces.Unsigned_64 is
      Created : constant Interfaces.Unsigned_64 := Probe (Family, 2);
   begin
      Await_Owners (Family, Created);
      return Created;
   end Quiesce;

   procedure Expect_Timeout (Request : CS.Response_Request) is
   begin
      declare
         Ignored : constant CS.Response_Result := CS.Wait (Request, 0);
      begin
         raise Program_Error with "C2 SystemTrackDataResponse Wait(0) did not time out";
      end;
   exception
      when AMS.MEL.IR.Timeout_Error =>
         null;
   end Expect_Timeout;

   procedure Check_Accepted (Result : CS.Response_Result; ID : Interfaces.Unsigned_32) is
   begin
      Require (CS.Status (Result) = TS.Success, "success outcome");
      Require (CS.Command_ID (CS.Command (Result)) = ID, "echoed command ID");
      Require (CS.State (CS.Command (Result)) = TS.Accepted, "accepted state");
      Require (CS.Reason (CS.Command (Result)) = TS.Not_Set, "no reason");
      Require (CS.Reason_Description (CS.Command (Result)) = Accepted_Text, "reason text");
   end Check_Accepted;

   --  First terminal Wait, then a cached repeat that must be identical.
   procedure Check_Accepted (Request : CS.Response_Request; ID : Interfaces.Unsigned_32) is
   begin
      Check_Accepted (CS.Wait (Request, 5_000), ID);
      Check_Accepted (CS.Wait (Request, 0), ID);
   end Check_Accepted;

   --  Input fidelity: identifier and timestamp boundaries, all four Boolean
   --  combinations with inactive-but-populated fields, signed zero, infinities
   --  and a subnormal value, each delivered bit-exactly and echoed.
   procedure Test_Fidelity (Provider_Path : String) is
      Parent     : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-sys-echo");
      Channel    : C2.Control_Channel := C2.Open (Parent, C2_Config);
      type Times is array (Positive range <>) of Long_Long_Integer;
      type U32s is array (Positive range <>) of Interfaces.Unsigned_32;
      Time_Cases : constant Times := [Long_Long_Integer'First, -1, 0, 1, Long_Long_Integer'Last];
      ID_Cases   : constant U32s := [0, 16#FFFF_FFFF#, 16#8000_0001#];
      Neg_Zero   : constant Long_Float := From_Bits (16#8000_0000_0000_0000#);
      Pos_Inf    : constant Long_Float := From_Bits (16#7FF0_0000_0000_0000#);
      Neg_Inf    : constant Long_Float := From_Bits (16#FFF0_0000_0000_0000#);
      Subnormal  : constant Long_Float := From_Bits (16#0000_0000_0000_0001#);
      type Float_Array is array (Positive range <>) of Long_Float;
      Non_Finite : constant Float_Array := [Pos_Inf, Neg_Inf, From_Bits (16#7FF8_0000_0000_0000#)];

      procedure Round_Trip (Value : CS.System_Track_Data_Response) is
         Before  : constant Natural := Sends (Provider_Path);
         Request : CS.Response_Request := CS.Submit_System_Track_Data_Response (Channel, Value);
      begin
         Require (Sends (Provider_Path) = Before + 1, "exactly one provider send");
         Check_Delivered (Provider_Path, Value);
         Check_Accepted (Request, Value.Command_ID);
         Require (Sends (Provider_Path) = Before + 1, "cached wait never resends");
         CS.Close (Request);
         CS.Close (Request);
         Require (not CS.Is_Open (Request), "closed");
      end Round_Trip;
   begin
      C2.Enable (Channel);
      Round_Trip (Rich);
      for T of Time_Cases loop
         declare
            V : CS.System_Track_Data_Response := Rich;
         begin
            V.System_Time_NS := T;
            Round_Trip (V);
         end;
      end loop;
      for A of ID_Cases loop
         for B of ID_Cases loop
            declare
               V : CS.System_Track_Data_Response := Rich;
            begin
               V.Command_ID := A;
               V.Request_ID := B;
               V.Track_ID := A xor B;
               Round_Trip (V);
            end;
         end loop;
      end loop;
      for Az_Valid in Boolean loop
         for Range_Valid in Boolean loop
            declare
               V : CS.System_Track_Data_Response := Rich;
            begin
               V.Az_El_Valid := Az_Valid;
               V.Range_Valid := Range_Valid;
               Round_Trip (V);
            end;
         end loop;
      end loop;
      declare
         V : CS.System_Track_Data_Response := Rich;
      begin
         --  Signed zero, subnormal and extreme finite values survive bit-exactly.
         V.Range_M := Neg_Zero;
         V.Range_Rate_MPS := Long_Float'Last;
         V.Range_Error_M := Long_Float'First;
         V.Range_Rate_Error_MPS := Subnormal;
         V.Inertial_Az_El := (Azimuth_Rad => 0.0, Elevation_Rad => Neg_Zero);
         V.Az_El_Error := (Azimuth_Rad => -Subnormal, Elevation_Rad => Neg_Zero);
         V.Range_Valid := False;
         V.Az_El_Valid := False;
         Round_Trip (V);
      end;
      --  Existing Track conversion contract: the validity-checked Ada library
      --  refuses non-finite Long_Float input with Constraint_Error before any
      --  admission or provider send (the C ABI itself carries non-finite values).
      --  The Track overload shares this exact conversion; parity is checked below.
      for Index in Non_Finite'Range loop
         declare
            V      : CS.System_Track_Data_Response := Rich;
            Before : constant Natural := Sends (Provider_Path);
         begin
            V.Az_El_Error.Elevation_Rad := Non_Finite (Index);
            begin
               declare
                  Unexpected : CS.Response_Request :=
                    CS.Submit_System_Track_Data_Response (Channel, V);
               begin
                  CS.Close (Unexpected);
                  raise Program_Error with "non-finite Ada input was submitted";
               end;
            exception
               when Constraint_Error =>
                  null;
            end;
            Require (Sends (Provider_Path) = Before, "non-finite input never sent");
         end;
      end loop;
      declare
         Track : Trk.Track_Channel := Trk.Open (Parent, Track_Cfg);
         V     : CS.System_Track_Data_Response := Rich;
      begin
         Trk.Enable (Track);
         V.Range_Rate_MPS := Pos_Inf;
         begin
            declare
               Unexpected : TS.Response_Request := TS.Submit (Track, V);
            begin
               TS.Close (Unexpected);
               raise Program_Error with "Track accepted non-finite Ada input";
            end;
         exception
            when Constraint_Error =>
               null;
         end;
         Trk.Close (Track);
      end;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Fidelity;

   --  Response fidelity through the derived mock: all five CommandState and
   --  all forty-seven CannotComply alternatives, CommandStatus Rejected as a
   --  Success outcome, and complete long reason text through cached Wait.
   procedure Test_Response_Fidelity (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-sys-derived");
      Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      Derived : constant String := "C2 derived " & Character'Val (16#C2#) & Character'Val (16#B5#);
      function Submit (State, Reason : Natural; Code : Long_Float) return CS.Response_Result is
         V : CS.System_Track_Data_Response := Rich;
      begin
         V.Request_ID := Interfaces.Unsigned_32 (State);
         V.Track_ID := Interfaces.Unsigned_32 (Reason);
         V.Range_M := Code;
         V.Command_ID := Interfaces.Unsigned_32 (State * 100 + Reason);
         declare
            Request : CS.Response_Request := CS.Submit_System_Track_Data_Response (Channel, V);
            Result  : constant CS.Response_Result := CS.Wait (Request, 5_000);
         begin
            --  The owned result survives explicit observer closure.
            CS.Close (Request);
            return Result;
         end;
      end Submit;
   begin
      C2.Enable (Channel);
      for State in CS.Command_State loop
         declare
            R : constant CS.Response_Result := Submit (CS.Command_State'Enum_Rep (State), 0, 0.0);
         begin
            Require (CS.Status (R) = TS.Success, "state alternative is Success");
            Require (CS.State (CS.Command (R)) = State, "exact CommandState");
            Require (CS.Reason_Description (CS.Command (R)) = Derived, "derived text");
         end;
      end loop;
      for Reason in CS.Cannot_Comply loop
         declare
            R : constant CS.Response_Result := Submit (3, CS.Cannot_Comply'Enum_Rep (Reason), 0.0);
         begin
            Require (CS.Status (R) = TS.Success, "CommandStatus Rejected is Success");
            Require (CS.State (CS.Command (R)) = TS.Rejected, "Rejected state preserved");
            Require (CS.Reason (CS.Command (R)) = Reason, "exact CannotComply");
            Require
              (CS.Command_ID (CS.Command (R))
               = Interfaces.Unsigned_32 (300 + CS.Cannot_Comply'Enum_Rep (Reason)),
               "command ID");
         end;
      end loop;
      declare
         R    : constant CS.Response_Result := Submit (3, 44, 1.0);
         Text : constant String := CS.Reason_Description (CS.Command (R));
      begin
         Require (Text'Length = 613, "complete long reason text");
         Require (Text (1 .. 510) = [1 .. 510 => 'x'], "long reason prefix");
         Require (Text (514 .. 613) = [514 .. 613 => 'y'], "long reason suffix");
      end;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Response_Fidelity;

   --  ErrorOr rejection versus failure categories, each cached through Wait.
   procedure Test_Outcomes (Provider_Path : String) is
      procedure Expect
        (Scenario : String; Code : CS.Response_Error_Code; Text : String; Raises : Boolean := False)
      is
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      begin
         C2.Enable (Channel);
         declare
            Request : CS.Response_Request := CS.Submit_System_Track_Data_Response (Channel, Rich);
         begin
            for Attempt in 1 .. 2 loop
               if Raises then
                  begin
                     declare
                        Ignored : constant CS.Response_Result := CS.Wait (Request, 5_000);
                     begin
                        raise Program_Error with "C2 failure did not raise: " & Scenario;
                     end;
                  exception
                     when Error : AMS.MEL.Provider_Error =>
                        Require (Ada.Exceptions.Exception_Message (Error) = Text, "failure text");
                  end;
               else
                  declare
                     Result : constant CS.Response_Result := CS.Wait (Request, 5_000);
                  begin
                     Require (CS.Status (Result) = TS.Rejected, "ErrorOr Rejected outcome");
                     Require (CS.Rejection_Code (Result) = Code, "rejection code");
                     Require (CS.Description (Result) = Text, "rejection text");
                  end;
               end if;
            end loop;
            CS.Close (Request);
         end;
         C2.Close (Channel);
         AMS.MEL.Close (Parent);
      end Expect;
      Long : String (1 .. 613) := [others => 'x'];
      Bad  : constant String := "provider rejection description was invalid UTF-8 or contained NUL";
   begin
      Long (511) := Character'Val (16#E2#);
      Long (512) := Character'Val (16#82#);
      Long (513) := Character'Val (16#AC#);
      Long (514 .. Long'Last) := [others => 'y'];
      Expect ("no-c2-system-response", TS.Unsupported, "");
      Expect
        ("c2-sys-reject",
         TS.Invalid_Parameters,
         "C2 system response rejected " & Character'Val (16#C2#) & Character'Val (16#B5#));
      Expect ("c2-sys-reject-long", TS.Insufficient_Resources, Long);
      Expect ("c2-sys-reject-utf8", TS.None, Bad, True);
      Expect ("c2-sys-reject-nul", TS.None, Bad, True);
      Expect ("c2-sys-unknown-error", TS.None, "provider returned unknown MEL error code", True);
      Expect ("c2-sys-null", TS.None, "provider returned null successful CommandStatus", True);
      Expect ("c2-sys-future-throw", TS.None, "mock C2 system response future exception", True);
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-sys-derived");
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
         V       : CS.System_Track_Data_Response := Rich;
      begin
         C2.Enable (Channel);
         V.Request_ID := 5;
         declare
            Request : constant CS.Response_Request :=
              CS.Submit_System_Track_Data_Response (Channel, V);
         begin
            declare
               Ignored : constant CS.Response_Result := CS.Wait (Request, 5_000);
            begin
               raise Program_Error with "unknown CommandState accepted";
            end;
         exception
            when Error : AMS.MEL.Provider_Error =>
               Require
                 (Ada.Exceptions.Exception_Message (Error)
                  = "provider returned malformed C2 CommandStatus",
                  "unknown CommandState");
         end;
         C2.Close (Channel);
         AMS.MEL.Close (Parent);
      end;
   end Test_Outcomes;

   procedure Expect_Submit_Error
     (Channel : C2.Control_Channel; Value : CS.System_Track_Data_Response; Text : String) is
   begin
      declare
         Unexpected : CS.Response_Request := CS.Submit_System_Track_Data_Response (Channel, Value);
      begin
         CS.Close (Unexpected);
         raise Program_Error with "C2 submission unexpectedly accepted: " & Text;
      end;
   exception
      when Error : AMS.MEL.Provider_Error =>
         Require
           (Text = "" or else Ada.Exceptions.Exception_Message (Error) = Text, "submit error");
   end Expect_Submit_Error;

   --  Attached-only refusal without a provider call, closed C2, send exception
   --  and invalid future all raise Provider_Error and leave nothing pending.
   procedure Test_Submission_Errors (Provider_Path : String) is
   begin
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-sys-echo");
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
         Before  : constant Natural := Sends (Provider_Path);
      begin
         Expect_Submit_Error (Channel, Rich, "C2 channel is not enabled");
         Require (Sends (Provider_Path) = Before, "Attached-only never sends");
         C2.Close (Channel);
         Expect_Submit_Error (Channel, Rich, "IR C2 channel is closed");
         Require (Sends (Provider_Path) = Before, "closed never sends");
         AMS.MEL.Close (Parent);
      end;
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-sys-send-throw");
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      begin
         C2.Enable (Channel);
         Expect_Submit_Error (Channel, Rich, "mock C2 system response send exception");
         C2.Close (Channel);
         AMS.MEL.Close (Parent);
      end;
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-sys-invalid-future");
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      begin
         C2.Enable (Channel);
         Expect_Submit_Error
           (Channel, Rich, "provider returned an invalid C2 SystemTrackDataResponse future");
         C2.Close (Channel);
         AMS.MEL.Close (Parent);
      end;
   end Test_Submission_Errors;

   --  Held completion through the deterministic provider promise gate.
   procedure Test_Held (Provider_Path : String) is
      Pin  : constant System.Address := Dlopen (IC.To_C (Provider_Path), 2);
      Base : Interfaces.Unsigned_64;
   begin
      Require (Pin /= System.Null_Address, "provider pin");
      --  Timeout then success, Session admission exhaustion, a second Session
      --  unaffected, and request Close not releasing the permit.
      declare
         Parent  : AMS.MEL.Session :=
           AMS.MEL.Open_With_Options
             (Provider_Path, "c2-sys-held", Options => (Max_Async_Requests => 1));
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
         Other   : AMS.MEL.Session :=
           AMS.MEL.Open_With_Options
             (Provider_Path, "c2-sys-held", Options => (Max_Async_Requests => 1));
         Second  : C2.Control_Channel := C2.Open (Other, C2_Config);
      begin
         C2.Enable (Channel);
         C2.Enable (Second);
         Base := Quiesce (C2_Response_Probe);
         declare
            Request : CS.Response_Request := CS.Submit_System_Track_Data_Response (Channel, Rich);
         begin
            Expect_Timeout (Request);
            begin
               declare
                  Refused : CS.Response_Request :=
                    CS.Submit_System_Track_Data_Response (Channel, Rich);
               begin
                  CS.Close (Refused);
                  raise Program_Error with "C2 admission not enforced";
               end;
            exception
               when Error : AMS.MEL.Resource_Exhausted =>
                  Require
                    (Ada.Exceptions.Exception_Message (Error) = "async request limit reached",
                     "admission");
            end;
            Expect_Timeout (Request);
            declare
               Independent : constant CS.Response_Request :=
                 CS.Submit_System_Track_Data_Response (Second, Rich);
            begin
               Expect_Timeout (Independent);
               Require (Release (Provider_Path, C2_Response_Send, 2) = 1, "release Sessions");
               Check_Accepted (Independent, Rich.Command_ID);
            end;
            Check_Accepted (Request, Rich.Command_ID);
            CS.Close (Request);
         end;
         Await_Owners (C2_Response_Probe, Base + 3);
         C2.Close (Second);
         AMS.MEL.Close (Other);
         --  C2 and Session Close with a pending observer: cached result survives.
         declare
            Observer : CS.Response_Request := CS.Submit_System_Track_Data_Response (Channel, Rich);
         begin
            C2.Close (Channel);
            AMS.MEL.Close (Parent);
            Expect_Timeout (Observer);
            Require (Release (Provider_Path, C2_Response_Send, 1) = 1, "pending release");
            Check_Accepted (Observer, Rich.Command_ID);
            CS.Close (Observer);
         end;
      end;
      --  Request Close before completion is not cancellation; finalization is silent.
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-sys-held");
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      begin
         C2.Enable (Channel);
         declare
            Closed : CS.Response_Request := CS.Submit_System_Track_Data_Response (Channel, Rich);
         begin
            CS.Close (Closed);
         end;
         declare
            Finalized : constant CS.Response_Request :=
              CS.Submit_System_Track_Data_Response (Channel, Rich);
         begin
            Require (CS.Is_Open (Finalized), "finalized request open");
         end;
         C2.Close (Channel);
         AMS.MEL.Close (Parent);
         Require (Release (Provider_Path, C2_Response_Send, 2) = 1, "closed release");
      end;
      --  Every owner since Base is gone: request, refused, independent, observer,
      --  closed and finalized (a refused submission still creates one owner).
      Await_Owners (C2_Response_Probe, Base + 6);
      Require (Dlclose (Pin) = 0, "provider unpin");
   end Test_Held;

   --  Concurrent pending C2 and Track SystemTrackDataResponse requests in one
   --  Session: each overload is accounted only in its own completion family
   --  and released only by its own provider gate.
   procedure Test_Mixed (Provider_Path : String) is
      Parent     : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-sys-mixed");
      Channel    : C2.Control_Channel := C2.Open (Parent, C2_Config);
      Track      : Trk.Track_Channel := Trk.Open (Parent, Track_Cfg);
      C2_Base    : constant Interfaces.Unsigned_64 := Quiesce (C2_Response_Probe);
      Track_Base : constant Interfaces.Unsigned_64 := Quiesce (Track_Probe);
      Before     : constant Natural := Sends (Provider_Path);
   begin
      C2.Enable (Channel);
      Trk.Enable (Track);
      declare
         C2_Request    : CS.Response_Request :=
           CS.Submit_System_Track_Data_Response (Channel, Rich);
         Track_Request : TS.Response_Request := TS.Submit (Track, Rich);
      begin
         Require (Sends (Provider_Path) = Before + 1, "only the C2 overload reached C2 send");
         Require (Probe (C2_Response_Probe, 2) = C2_Base + 1, "one C2 completion owner");
         Require (Probe (Track_Probe, 2) = Track_Base + 1, "one Track completion owner");
         Require (Release (Provider_Path, Track_Send, 1) = 1, "release Track only");
         declare
            Result : constant TS.Response_Result := TS.Wait (Track_Request, 5_000);
         begin
            Require (TS.Status (Result) = TS.Success, "Track success");
            Require (TS.Command_ID (TS.Command (Result)) = 16#A1B2_C3D4#, "Track status");
         end;
         Await_Owners (Track_Probe, Track_Base + 1);
         Expect_Timeout (C2_Request);
         Require (Probe (C2_Response_Probe, 3) = C2_Base, "C2 still pending");
         Trk.Close (Track);
         C2.Close (Channel);
         Require (Release (Provider_Path, C2_Response_Send, 1) = 1, "release C2 only");
         Check_Accepted (C2_Request, Rich.Command_ID);
         Await_Owners (C2_Response_Probe, C2_Base + 1);
         CS.Close (C2_Request);
         TS.Close (Track_Request);
      end;
      AMS.MEL.Close (Parent);
   end Test_Mixed;

   --  Isolated processes intentionally retain uncertain provider graphs. Never
   --  execute these failpoints in the full smoke process or retry the send.
   procedure Run_Isolated (Provider_Path : String; Mode : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-sys-held");
      Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      Pin     : constant System.Address := Dlopen (IC.To_C (Provider_Path), 2);
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
      Require (Pin /= System.Null_Address, "isolated mock pin");
      C2.Enable (Channel);
      if Mode = "allocation" or else Mode = "worker-launch" then
         Ada.Environment_Variables.Set ("AMS_MEL_TEST_C2_SYSTEM_RESPONSE_POST_SEND_FAILURE", Mode);
         Expect_Submit_Error
           (Channel,
            Rich,
            (if Mode = "allocation"
             then "facade allocation failed after provider send"
             else "facade worker launch failed"));
         Ada.Environment_Variables.Clear ("AMS_MEL_TEST_C2_SYSTEM_RESPONSE_POST_SEND_FAILURE");
         C2.Close (Channel);
         AMS.MEL.Close (Parent);
         Require (Occurrences ("c2_system_track_data_response_sent") = 1, "one provider send");
         Require (Occurrences ("c2_channel_destroyed") = 0, "pending channel retained");
         Require (Occurrences ("control_destroyed") = 0, "pending Session retained");
      elsif Mode = "finish-failure" then
         Ada.Environment_Variables.Set ("AMS_MEL_TEST_C2_FINISH_FAILURE", "exception");
         declare
            Request : CS.Response_Request := CS.Submit_System_Track_Data_Response (Channel, Rich);
         begin
            C2.Close (Channel);
            AMS.MEL.Close (Parent);
            Require (Release (Provider_Path, C2_Response_Send, 1) = 1, "release cleanup failure");
            for Attempt in 1 .. 2 loop
               begin
                  declare
                     Ignored : constant CS.Response_Result := CS.Wait (Request, 5_000);
                  begin
                     raise Program_Error with "cleanup failure returned a result";
                  end;
               exception
                  when Error : AMS.MEL.Provider_Error =>
                     Require
                       (Ada.Exceptions.Exception_Message (Error) = "deferred C2 cleanup failed",
                        "cleanup failure cached");
               end;
            end loop;
            CS.Close (Request);
         end;
         Require (Occurrences ("c2_system_track_data_response_sent") = 1, "never replayed");
      else
         raise Program_Error with "unknown isolated C2 system response test mode";
      end if;
      Require (Occurrences ("library_unloaded") = 0, "uncertain graph never unloaded");
      --  Keep the provider pin through process exit, matching native isolated tests.
      Ada.Environment_Variables.Clear ("AMS_MEL_TEST_LIFETIME_LOG");
      Ada.Directories.Delete_File (Log);
      Ada.Text_IO.Put_Line ("PASS: Ada isolated C2 SystemTrackDataResponse " & Mode);
   end Run_Isolated;

   procedure Run (Provider_Path : String) is
   begin
      CS.Testing.Check_Representation;
      Test_Fidelity (Provider_Path);
      Test_Response_Fidelity (Provider_Path);
      Test_Outcomes (Provider_Path);
      Test_Submission_Errors (Provider_Path);
      Test_Held (Provider_Path);
      Test_Mixed (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada mock C2 SystemTrackDataResponse contract");
   end Run;
end AMS_MEL_IR_C2_System_Data_Tests;
