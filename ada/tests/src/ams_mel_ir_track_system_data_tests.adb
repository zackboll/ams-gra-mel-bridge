with Ada.Directories;
with Ada.Environment_Variables;
with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Track.System_Data;
with GNAT.OS_Lib;
with Interfaces;

--  Exercises exactly the @Optional TrackChannel::send (SystemTrackDataResponse)
--  surface. The RequestSystemTrackData, CandidateObjectMessage, and
--  CandidateObjectPreProcMessage callbacks remain unimplemented and are
--  therefore not exercised here.

package body AMS_MEL_IR_Track_System_Data_Tests is
   package Trk renames AMS.MEL.IR.Track;
   package Sys renames AMS.MEL.IR.Track.System_Data;
   use type Sys.Response_Outcome;
   use type Sys.Response_Error_Code;
   use type Sys.Command_State;
   use type Sys.Cannot_Comply;
   use type Interfaces.Unsigned_32;
   use type GNAT.OS_Lib.File_Descriptor;
   use type GNAT.OS_Lib.String_Access;

   Zero       : constant AMS.MEL.IR.UUID := [others => 0];
   Channel_ID : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "IR track channel");
   Platform   : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "track platform");
   Location   : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (1.25, -2.5, 3.75, "station-1", "mock-aircraft");
   Config     : constant Trk.Track_Config := Trk.Create_Config (Channel_ID, Platform, Location);

   --  The one distinctive response the mock verifies field by field, including
   --  both AzEl pairs and both published bool values.
   Rich_Response : constant Sys.System_Track_Data_Response :=
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

   --  Deterministic pending-completion control. The mock background completion
   --  waits for this file to exist, so the test never relies on a sleep for
   --  correctness. Reserve returns a unique path that does not yet exist and
   --  publishes it to the mock through the environment.
   function Reserve_Barrier return String is
      Descriptor : GNAT.OS_Lib.File_Descriptor;
      Name       : GNAT.OS_Lib.String_Access;
      Closed     : Boolean;
      Deleted    : Boolean;
   begin
      GNAT.OS_Lib.Create_Temp_File (Descriptor, Name);
      if Descriptor = GNAT.OS_Lib.Invalid_FD or else Name = null then
         raise Program_Error with "could not reserve a Track response barrier path";
      end if;
      GNAT.OS_Lib.Close (Descriptor, Closed);
      GNAT.OS_Lib.Delete_File (Name.all, Deleted);
      if not Closed or else not Deleted then
         raise Program_Error with "could not prepare the Track response barrier path";
      end if;
      return Result : constant String := Name.all do
         Ada.Environment_Variables.Set ("AMS_MEL_TEST_TRACK_RESPONSE_BARRIER", Result);
         GNAT.OS_Lib.Free (Name);
      end return;
   end Reserve_Barrier;

   procedure Release (Barrier : String) is
      Stream : Ada.Text_IO.File_Type;
   begin
      Ada.Text_IO.Create (Stream, Ada.Text_IO.Out_File, Barrier);
      Ada.Text_IO.Close (Stream);
   end Release;

   procedure Discard (Barrier : String) is
   begin
      if Ada.Directories.Exists (Barrier) then
         Ada.Directories.Delete_File (Barrier);
      end if;
      Ada.Environment_Variables.Clear ("AMS_MEL_TEST_TRACK_RESPONSE_BARRIER");
   end Discard;

   Accepted_Description : constant String :=
     "Track system response accepted " & Character'Val (16#C2#) & Character'Val (16#B5#);

   function Rich_Status_Matches (Value : Sys.Command_Status) return Boolean
   is (Sys.Command_ID (Value) = 16#A1B2_C3D4#
       and then Sys.State (Value) = Sys.Accepted
       and then Sys.Reason (Value) = Sys.Not_Set
       and then Sys.Reason_Description (Value) = Accepted_Description);

   --  Complete input fidelity: the mock verifies every published getter and
   --  throws otherwise, so a successful rich CommandStatus proves the whole
   --  mapping, including signed nanoseconds, both AzEl pairs, and both bools.
   --  The terminal result is cached.
   procedure Test_Rich_Submit (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-response-rich");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
   begin
      Trk.Enable (Channel);
      declare
         Request : Sys.Response_Request := Sys.Submit (Channel, Rich_Response);
         First   : constant Sys.Response_Result := Sys.Wait (Request, 2_000);
         Second  : constant Sys.Response_Result := Sys.Wait (Request, 0);
      begin
         if not Sys.Is_Open (Request)
           or else Sys.Status (First) /= Sys.Success
           or else Sys.Status (Second) /= Sys.Success
           or else not Rich_Status_Matches (Sys.Command (First))
           or else not Rich_Status_Matches (Sys.Command (Second))
         then
            raise Program_Error with "Ada Track response rich submit/fidelity failed";
         end if;
         Sys.Close (Request);
      end;
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Rich_Submit;

   --  Boolean coverage: both published bool values reach the provider as False
   --  while every numeric value stays identical.
   procedure Test_False_Flags (Provider_Path : String) is
      Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-response-flags-false");
      Channel  : Trk.Track_Channel := Trk.Open (Parent, Config);
      Response : Sys.System_Track_Data_Response := Rich_Response;
   begin
      Response.Az_El_Valid := False;
      Response.Range_Valid := False;
      Trk.Enable (Channel);
      declare
         Request : Sys.Response_Request := Sys.Submit (Channel, Response);
         Result  : constant Sys.Response_Result := Sys.Wait (Request, 2_000);
      begin
         if Sys.Status (Result) /= Sys.Success
           or else not Rich_Status_Matches (Sys.Command (Result))
         then
            raise Program_Error with "Ada Track response false-flag submit failed";
         end if;
         Sys.Close (Request);
      end;
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_False_Flags;

   --  Submission requires an enabled Track channel.
   procedure Test_Submit_Before_Enable (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-response-any");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
   begin
      begin
         declare
            Ignored : Sys.Response_Request := Sys.Submit (Channel, Rich_Response);
         begin
            Sys.Close (Ignored);
         end;
         raise Program_Error with "Ada Track response submit before Enable was accepted";
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Submit_Before_Enable;

   --  A successful future whose CommandStatus state is itself Rejected stays a
   --  Success outcome; it is not an ErrorOr rejection.
   procedure Test_Status_Rejected_Is_Success (Provider_Path : String) is
      Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-response-status-rejected");
      Channel  : Trk.Track_Channel := Trk.Open (Parent, Config);
      Expected : constant String :=
        "Track system response rejected " & Character'Val (16#C2#) & Character'Val (16#B5#);
   begin
      Trk.Enable (Channel);
      declare
         Request : Sys.Response_Request := Sys.Submit (Channel, Rich_Response);
         Result  : constant Sys.Response_Result := Sys.Wait (Request, 2_000);
      begin
         if Sys.Status (Result) /= Sys.Success
           or else Sys.State (Sys.Command (Result)) /= Sys.Rejected
           or else Sys.Reason (Sys.Command (Result)) /= Sys.Invalid_Input_Parameter
           or else Sys.Reason_Description (Sys.Command (Result)) /= Expected
         then
            raise Program_Error with "Ada Track response status-rejection distinction failed";
         end if;
         Sys.Close (Request);
      end;
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Status_Rejected_Is_Success;

   --  ErrorOr rejection: the complete long UTF-8 diagnostic is recovered from
   --  the cached terminal request.
   procedure Test_Rejection (Provider_Path : String) is
      Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-response-reject");
      Channel  : Trk.Track_Channel := Trk.Open (Parent, Config);
      Expected : constant String :=
        [1 .. 510 => 'x']
        & Character'Val (16#E2#)
        & Character'Val (16#82#)
        & Character'Val (16#AC#)
        & " Track system response rejected "
        & Character'Val (16#C2#)
        & Character'Val (16#B5#);
   begin
      Trk.Enable (Channel);
      declare
         Request : Sys.Response_Request := Sys.Submit (Channel, Rich_Response);
         Result  : constant Sys.Response_Result := Sys.Wait (Request, 2_000);
      begin
         if Sys.Status (Result) /= Sys.Rejected
           or else Sys.Rejection_Code (Result) /= Sys.Invalid_Parameters
           or else Sys.Description (Result) /= Expected
         then
            raise Program_Error with "Ada Track response long rejection recovery failed";
         end if;
         Sys.Close (Request);
      end;
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Rejection;

   --  Every malformed or failing provider outcome fails closed.
   procedure Test_Unknown_Outcome (Provider_Path : String; Scenario : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
   begin
      Trk.Enable (Channel);
      declare
         Request : Sys.Response_Request := Sys.Submit (Channel, Rich_Response);
      begin
         begin
            declare
               Ignored : constant Sys.Response_Result := Sys.Wait (Request, 2_000);
            begin
               null;
            end;
            raise Program_Error with "Ada Track response malformed outcome was not rejected";
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Sys.Close (Request);
      end;
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Unknown_Outcome;

   --  A throwing provider send() is reported from Submit itself; no request
   --  owner escapes and the Track channel still closes synchronously.
   procedure Test_Send_Throw (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-response-send-throw");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
   begin
      Trk.Enable (Channel);
      begin
         declare
            Ignored : Sys.Response_Request := Sys.Submit (Channel, Rich_Response);
         begin
            Sys.Close (Ignored);
         end;
         raise Program_Error with "Ada Track response send exception was not reported";
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Send_Throw;

   --  Timeout is only "not ready yet": it never cancels or consumes the
   --  request, and a later Wait returns the identical cached terminal result.
   --  The mock completion is released through a test-controlled barrier file.
   procedure Test_Timeout_Then_Cached (Provider_Path : String; Barrier : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-response-pending");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
   begin
      Trk.Enable (Channel);
      declare
         Request : Sys.Response_Request := Sys.Submit (Channel, Rich_Response);
      begin
         begin
            declare
               Ignored : constant Sys.Response_Result := Sys.Wait (Request, 0);
            begin
               null;
            end;
            raise Program_Error with "Ada Track response Wait(0) did not time out";
         exception
            when AMS.MEL.IR.Timeout_Error =>
               null;
         end;
         Release (Barrier);
         declare
            First  : constant Sys.Response_Result := Sys.Wait (Request, 5_000);
            Second : constant Sys.Response_Result := Sys.Wait (Request, 0);
         begin
            if Sys.Status (First) /= Sys.Success
              or else not Rich_Status_Matches (Sys.Command (First))
              or else not Rich_Status_Matches (Sys.Command (Second))
            then
               raise Program_Error with "Ada Track response cached Wait mismatch";
            end if;
         end;
         Sys.Close (Request);
      end;
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Timeout_Then_Cached;

   --  Session and Track close while a response is still pending. Close is not
   --  cancellation: physical provider teardown is deferred behind the request,
   --  which still completes and whose result stays readable afterwards.
   procedure Test_Parent_First_Close (Provider_Path : String; Barrier : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-response-pending");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
   begin
      Trk.Enable (Channel);
      declare
         Request : Sys.Response_Request := Sys.Submit (Channel, Rich_Response);
      begin
         AMS.MEL.Close (Parent);
         Trk.Close (Channel);
         if Trk.Is_Open (Channel) or else AMS.MEL.Is_Open (Parent) then
            raise Program_Error with "Ada Track response parent-first close left an owner open";
         end if;
         Release (Barrier);
         declare
            Result : constant Sys.Response_Result := Sys.Wait (Request, 5_000);
         begin
            if Sys.Status (Result) /= Sys.Success
              or else not Rich_Status_Matches (Sys.Command (Result))
            then
               raise Program_Error with "Ada Track response survived-parent result mismatch";
            end if;
         end;
         Sys.Close (Request);
      end;
   end Test_Parent_First_Close;

   --  Both published mappings are contiguous from zero, so proving
   --  'Enum_Rep = 'Pos for every literal locks the whole representation, not
   --  merely the one reason value the provider scenarios happen to use. The
   --  explicit 32-bit size is checked alongside it.
   procedure Test_Enum_Representations is
   begin
      for State in Sys.Command_State loop
         if Sys.Command_State'Enum_Rep (State) /= Sys.Command_State'Pos (State) then
            raise Program_Error
              with
                "Ada Track response Command_State representation mismatch for "
                & Sys.Command_State'Image (State);
         end if;
      end loop;
      for Reason in Sys.Cannot_Comply loop
         if Sys.Cannot_Comply'Enum_Rep (Reason) /= Sys.Cannot_Comply'Pos (Reason) then
            raise Program_Error
              with
                "Ada Track response Cannot_Comply representation mismatch for "
                & Sys.Cannot_Comply'Image (Reason);
         end if;
      end loop;
      if Sys.Command_State'Size /= 32 or else Sys.Cannot_Comply'Size /= 32 then
         raise Program_Error with "Ada Track response enum size is not 32 bits";
      end if;
   end Test_Enum_Representations;

   procedure Run (Provider_Path : String) is
   begin
      Test_Enum_Representations;
      Test_Rich_Submit (Provider_Path);
      Test_False_Flags (Provider_Path);
      Test_Submit_Before_Enable (Provider_Path);
      Test_Status_Rejected_Is_Success (Provider_Path);
      Test_Rejection (Provider_Path);
      Test_Unknown_Outcome (Provider_Path, "track-response-null-status");
      Test_Unknown_Outcome (Provider_Path, "track-response-bad-state");
      Test_Unknown_Outcome (Provider_Path, "track-response-bad-reason");
      Test_Unknown_Outcome (Provider_Path, "track-response-bad-description");
      Test_Unknown_Outcome (Provider_Path, "track-response-unknown-error");
      Test_Unknown_Outcome (Provider_Path, "track-response-future-throw");
      Test_Send_Throw (Provider_Path);
      declare
         Barrier : constant String := Reserve_Barrier;
      begin
         Test_Timeout_Then_Cached (Provider_Path, Barrier);
         Discard (Barrier);
      end;
      declare
         Barrier : constant String := Reserve_Barrier;
      begin
         Test_Parent_First_Close (Provider_Path, Barrier);
         Discard (Barrier);
      end;
      Ada.Text_IO.Put_Line ("PASS: Ada IR Track SystemTrackDataResponse contract");
   end Run;
end AMS_MEL_IR_Track_System_Data_Tests;
