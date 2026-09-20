with Ada.Directories;
with Ada.Environment_Variables;
with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Track.Updates;
with GNAT.OS_Lib;
with Interfaces;

--  Exercises exactly the @RequiredIfTrackUpdate TrackChannel::send
--  (TrackDataUpdate) surface. SystemTrackDataResponse, CandidateObjectMessage,
--  CandidateObjectPreProcMessage, and RequestSystemTrackData remain
--  unimplemented and are therefore not exercised here.

package body AMS_MEL_IR_Track_Update_Tests is
   package Trk renames AMS.MEL.IR.Track;
   package Upd renames AMS.MEL.IR.Track.Updates;
   use type Upd.Update_Outcome;
   use type Upd.Update_Error_Code;
   use type Upd.Command_State;
   use type Upd.Cannot_Comply;
   use type Interfaces.Unsigned_32;
   use type GNAT.OS_Lib.File_Descriptor;
   use type GNAT.OS_Lib.String_Access;

   Zero       : constant AMS.MEL.IR.UUID := [others => 0];
   Channel_ID : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "IR track channel");
   Platform   : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "track platform");
   Location   : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (1.25, -2.5, 3.75, "station-1", "mock-aircraft");
   Config     : constant Trk.Track_Config := Trk.Create_Config (Channel_ID, Platform, Location);

   function Seeded (Seed : Interfaces.Unsigned_8) return AMS.MEL.IR.UUID is
      Result : AMS.MEL.IR.UUID;
      use type Interfaces.Unsigned_8;
   begin
      for Index in Result'Range loop
         Result (Index) := Seed + Interfaces.Unsigned_8 ((Index - Result'First) * 3 mod 256);
      end loop;
      return Result;
   end Seeded;

   --  The one distinctive update the mock verifies field by field, including
   --  all 21 covariance terms.
   Rich_Update : constant Upd.Track_Data_Update :=
     (Platform_ID                 => 16#F123_4567#,
      Capability_UUID             =>
        AMS.MEL.IR.Create_UCI_ID
          (Seeded (16#10#), "capability-" & Character'Val (16#CE#) & Character'Val (16#B1#)),
      Activity_UUID               =>
        AMS.MEL.IR.Create_UCI_ID
          (Seeded (16#40#), "activity-" & Character'Val (16#CE#) & Character'Val (16#B2#)),
      Track_ID                    => 16#E234_5678#,
      Entity_UUID                 =>
        AMS.MEL.IR.Create_UCI_ID
          (Seeded (16#70#),
           "entity-" & Character'Val (16#E2#) & Character'Val (16#82#) & Character'Val (16#AC#)),
      Status                      => Upd.Predict,
      Time_Of_Validity_Seconds    => -12_345.25,
      Time_Of_Last_Update_Seconds => 1_700_000_000.875,
      Position_ECEF               => (X => -1.25, Y => 2.5, Z => -3.75),
      Velocity_ECEF               => (X => 4.125, Y => -5.25, Z => 6.5),
      Covariance                  =>
        (XX    => 1.01,
         XY    => 2.02,
         XZ    => 3.03,
         X_VX  => 4.04,
         X_VY  => 5.05,
         X_VZ  => 6.06,
         YY    => 7.07,
         YZ    => 8.08,
         Y_VX  => 9.09,
         Y_VY  => 10.10,
         Y_VZ  => 11.11,
         ZZ    => 12.12,
         Z_VX  => 13.13,
         Z_VY  => 14.14,
         Z_VZ  => 15.15,
         VX_VX => 16.16,
         VX_VY => 17.17,
         VX_VZ => 18.18,
         VY_VY => 19.19,
         VY_VZ => 20.20,
         VZ_VZ => 21.21),
      Maneuver_Probability        => 0.625,
      Track_Quality               => 12.75);

   --  Deterministic pending-completion control. The mock background completion
   --  waits for this file to exist, so the test never relies on a sleep for
   --  correctness. Reserve returns a unique path that does not yet exist.
   --  Reserve also publishes the path to the mock through the environment.
   function Reserve_Barrier return String is
      Descriptor : GNAT.OS_Lib.File_Descriptor;
      Name       : GNAT.OS_Lib.String_Access;
      Closed     : Boolean;
      Deleted    : Boolean;
   begin
      GNAT.OS_Lib.Create_Temp_File (Descriptor, Name);
      if Descriptor = GNAT.OS_Lib.Invalid_FD or else Name = null then
         raise Program_Error with "could not reserve a Track update barrier path";
      end if;
      GNAT.OS_Lib.Close (Descriptor, Closed);
      GNAT.OS_Lib.Delete_File (Name.all, Deleted);
      if not Closed or else not Deleted then
         raise Program_Error with "could not prepare the Track update barrier path";
      end if;
      return Result : constant String := Name.all do
         Ada.Environment_Variables.Set ("AMS_MEL_TEST_TRACK_UPDATE_BARRIER", Result);
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
      Ada.Environment_Variables.Clear ("AMS_MEL_TEST_TRACK_UPDATE_BARRIER");
   end Discard;

   Accepted_Description : constant String :=
     "Track update accepted " & Character'Val (16#C2#) & Character'Val (16#B5#);

   function Rich_Status_Matches (Value : Upd.Command_Status) return Boolean
   is (Upd.Command_ID (Value) = 16#F0E1_D2C3#
       and then Upd.State (Value) = Upd.Accepted
       and then Upd.Reason (Value) = Upd.Not_Set
       and then Upd.Reason_Description (Value) = Accepted_Description);

   --  Complete input fidelity: the mock verifies every field, including all 21
   --  covariance terms, and throws otherwise, so a successful rich
   --  CommandStatus proves the whole mapping. The terminal result is cached.
   procedure Test_Rich_Submit (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-update-rich");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
   begin
      Trk.Enable (Channel);
      declare
         Request : Upd.Update_Request := Upd.Submit (Channel, Rich_Update);
         First   : constant Upd.Update_Result := Upd.Wait (Request, 2_000);
         Second  : constant Upd.Update_Result := Upd.Wait (Request, 0);
      begin
         if not Upd.Is_Open (Request)
           or else Upd.Status (First) /= Upd.Success
           or else Upd.Status (Second) /= Upd.Success
           or else not Rich_Status_Matches (Upd.Command (First))
           or else not Rich_Status_Matches (Upd.Command (Second))
         then
            raise Program_Error with "Ada Track update rich submit/fidelity failed";
         end if;
         Upd.Close (Request);
      end;
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Rich_Submit;

   --  Submission requires an enabled Track channel.
   procedure Test_Submit_Before_Enable (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-update-any");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
   begin
      begin
         declare
            Ignored : Upd.Update_Request := Upd.Submit (Channel, Rich_Update);
         begin
            Upd.Close (Ignored);
         end;
         raise Program_Error with "Ada Track update submit before Enable was accepted";
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
      Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-update-status-rejected");
      Channel  : Trk.Track_Channel := Trk.Open (Parent, Config);
      Expected : constant String :=
        "Track update parameters rejected " & Character'Val (16#C2#) & Character'Val (16#B5#);
   begin
      Trk.Enable (Channel);
      declare
         Request : Upd.Update_Request := Upd.Submit (Channel, Rich_Update);
         Result  : constant Upd.Update_Result := Upd.Wait (Request, 2_000);
      begin
         if Upd.Status (Result) /= Upd.Success
           or else Upd.State (Upd.Command (Result)) /= Upd.Rejected
           or else Upd.Reason (Upd.Command (Result)) /= Upd.Invalid_Input_Parameter
           or else Upd.Reason_Description (Upd.Command (Result)) /= Expected
         then
            raise Program_Error with "Ada Track update status-rejection distinction failed";
         end if;
         Upd.Close (Request);
      end;
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Status_Rejected_Is_Success;

   --  ErrorOr rejection: the complete long UTF-8 diagnostic is recovered from
   --  the cached terminal request.
   procedure Test_Rejection (Provider_Path : String) is
      Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-update-reject");
      Channel  : Trk.Track_Channel := Trk.Open (Parent, Config);
      Expected : constant String :=
        [1 .. 510 => 'x']
        & Character'Val (16#E2#)
        & Character'Val (16#82#)
        & Character'Val (16#AC#)
        & " Track update rejected "
        & Character'Val (16#C2#)
        & Character'Val (16#B5#);
   begin
      Trk.Enable (Channel);
      declare
         Request : Upd.Update_Request := Upd.Submit (Channel, Rich_Update);
         Result  : constant Upd.Update_Result := Upd.Wait (Request, 2_000);
      begin
         if Upd.Status (Result) /= Upd.Rejected
           or else Upd.Rejection_Code (Result) /= Upd.Invalid_Parameters
           or else Upd.Description (Result) /= Expected
         then
            raise Program_Error with "Ada Track update long rejection recovery failed";
         end if;
         Upd.Close (Request);
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
         Request : Upd.Update_Request := Upd.Submit (Channel, Rich_Update);
      begin
         begin
            declare
               Ignored : constant Upd.Update_Result := Upd.Wait (Request, 2_000);
            begin
               null;
            end;
            raise Program_Error with "Ada Track update malformed outcome was not rejected";
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Upd.Close (Request);
      end;
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Unknown_Outcome;

   --  A throwing provider send() is reported from Submit itself; no request
   --  owner escapes and the Track channel still closes synchronously.
   procedure Test_Send_Throw (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-update-send-throw");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
   begin
      Trk.Enable (Channel);
      begin
         declare
            Ignored : Upd.Update_Request := Upd.Submit (Channel, Rich_Update);
         begin
            Upd.Close (Ignored);
         end;
         raise Program_Error with "Ada Track update send exception was not reported";
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
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-update-pending");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
   begin
      Trk.Enable (Channel);
      declare
         Request : Upd.Update_Request := Upd.Submit (Channel, Rich_Update);
      begin
         begin
            declare
               Ignored : constant Upd.Update_Result := Upd.Wait (Request, 0);
            begin
               null;
            end;
            raise Program_Error with "Ada Track update Wait(0) did not time out";
         exception
            when AMS.MEL.IR.Timeout_Error =>
               null;
         end;
         Release (Barrier);
         declare
            First  : constant Upd.Update_Result := Upd.Wait (Request, 5_000);
            Second : constant Upd.Update_Result := Upd.Wait (Request, 0);
         begin
            if Upd.Status (First) /= Upd.Success
              or else not Rich_Status_Matches (Upd.Command (First))
              or else not Rich_Status_Matches (Upd.Command (Second))
            then
               raise Program_Error with "Ada Track update cached Wait mismatch";
            end if;
         end;
         Upd.Close (Request);
      end;
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Timeout_Then_Cached;

   --  Session and Track close while an update is still pending. Close is not
   --  cancellation: physical provider teardown is deferred behind the request,
   --  which still completes and whose result stays readable afterwards.
   procedure Test_Parent_First_Close (Provider_Path : String; Barrier : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-update-pending");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
   begin
      Trk.Enable (Channel);
      declare
         Request : Upd.Update_Request := Upd.Submit (Channel, Rich_Update);
      begin
         AMS.MEL.Close (Parent);
         Trk.Close (Channel);
         if Trk.Is_Open (Channel) or else AMS.MEL.Is_Open (Parent) then
            raise Program_Error with "Ada Track update parent-first close left an owner open";
         end if;
         Release (Barrier);
         declare
            Result : constant Upd.Update_Result := Upd.Wait (Request, 5_000);
         begin
            if Upd.Status (Result) /= Upd.Success
              or else not Rich_Status_Matches (Upd.Command (Result))
            then
               raise Program_Error with "Ada Track update survived-parent result mismatch";
            end if;
         end;
         Upd.Close (Request);
      end;
   end Test_Parent_First_Close;

   --  Both published mappings are contiguous from zero, so proving
   --  'Enum_Rep = 'Pos for every literal locks the whole representation, not
   --  merely the one reason value the provider scenarios happen to use. The
   --  explicit 32-bit size is checked alongside it.
   procedure Test_Enum_Representations is
   begin
      for State in Upd.Command_State loop
         if Upd.Command_State'Enum_Rep (State) /= Upd.Command_State'Pos (State) then
            raise Program_Error
              with
                "Ada Track update Command_State representation mismatch for "
                & Upd.Command_State'Image (State);
         end if;
      end loop;
      for Reason in Upd.Cannot_Comply loop
         if Upd.Cannot_Comply'Enum_Rep (Reason) /= Upd.Cannot_Comply'Pos (Reason) then
            raise Program_Error
              with
                "Ada Track update Cannot_Comply representation mismatch for "
                & Upd.Cannot_Comply'Image (Reason);
         end if;
      end loop;
      if Upd.Command_State'Size /= 32 or else Upd.Cannot_Comply'Size /= 32 then
         raise Program_Error with "Ada Track update enum size is not 32 bits";
      end if;
   end Test_Enum_Representations;

   procedure Run (Provider_Path : String) is
   begin
      Test_Enum_Representations;
      Test_Rich_Submit (Provider_Path);
      Test_Submit_Before_Enable (Provider_Path);
      Test_Status_Rejected_Is_Success (Provider_Path);
      Test_Rejection (Provider_Path);
      Test_Unknown_Outcome (Provider_Path, "track-update-null-status");
      Test_Unknown_Outcome (Provider_Path, "track-update-bad-state");
      Test_Unknown_Outcome (Provider_Path, "track-update-bad-reason");
      Test_Unknown_Outcome (Provider_Path, "track-update-bad-description");
      Test_Unknown_Outcome (Provider_Path, "track-update-unknown-error");
      Test_Unknown_Outcome (Provider_Path, "track-update-future-throw");
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
      Ada.Text_IO.Put_Line ("PASS: Ada IR Track TrackDataUpdate contract");
   end Run;
end AMS_MEL_IR_Track_Update_Tests;
