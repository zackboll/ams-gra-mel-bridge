with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Track;
with AMS.MEL.IR.Track.Metadata;
with Interfaces;

--  Task 029B2 covers the @RequiredIfTrack IRSTTrackReport callback, Task 029E
--  adds the @Optional RequestSystemTrackData request, and Task 029F adds the
--  @RequiredIfDetectCandidateObjects CandidateObjectMessage. All three share
--  the same bounded queue, capacity, and counter set.
--  CandidateObjectPreProcMessage remains unimplemented and is therefore not
--  exercised here.

package body AMS_MEL_IR_Track_Metadata_Tests is
   package Trk renames AMS.MEL.IR.Track;
   package Meta renames AMS.MEL.IR.Track.Metadata;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Unsigned_16;
   use type Meta.Metadata_Kind;
   use type Meta.Hot_Region_Type;
   use type Trk.IRST_Track_State;
   use type Trk.IRST_Track_Mode;
   use type AMS.MEL.IR.Counter;

   Zero       : constant AMS.MEL.IR.UUID := [others => 0];
   Channel_ID : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "IR track channel");
   Platform   : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "track platform");
   Location   : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (1.25, -2.5, 3.75, "station-1", "mock-aircraft");
   Config     : constant Trk.Track_Config := Trk.Create_Config (Channel_ID, Platform, Location);

   --  Exact all-field fidelity of the distinctive rich IRSTTrackReport.
   procedure Check_Rich (Report : Trk.IRST_Track_Report; Context : String) is
   begin
      if Report.System_Time_NS /= -1_234_567_890_123 or else Report.Activity_ID /= 16#F1234567# then
         raise Program_Error with "Ada Track report identity mismatch in " & Context;
      end if;
      if Report.Measured_NED.North /= -1.25
        or else Report.Measured_NED.East /= 2.5
        or else Report.Measured_NED.Down /= -3.75
        or else Report.Measured_Intensity /= 4.125
        or else Report.Measured_SNR /= -5.25
      then
         raise Program_Error with "Ada Track measured values mismatch in " & Context;
      end if;
      if Report.Filtered_NED.North /= 6.5
        or else Report.Filtered_NED.East /= -7.75
        or else Report.Filtered_NED.Down /= 8.875
        or else Report.Filtered_Intensity /= -9.125
        or else Report.Filtered_SNR /= 10.25
      then
         raise Program_Error with "Ada Track filtered values mismatch in " & Context;
      end if;
      if Report.Range_M /= 123_456.75
        or else Report.Range_Error_M /= 654.5
        or else Report.Spatial_Extent_Rad /= 0.0125
        or else Report.Track_Quality /= 0.875
        or else Report.Clutter /= -0.5
      then
         raise Program_Error with "Ada Track scalar values mismatch in " & Context;
      end if;
      if Report.Age_NS /= 9_876_543_210
        or else Report.State /= Trk.Coast
        or else Report.Mode /= Trk.Stare
      then
         raise Program_Error with "Ada Track age/state/mode mismatch in " & Context;
      end if;
   end Check_Rich;

   procedure Check_Counters
     (Values                       : Meta.Metadata_Counters;
      Received, Dropped, Malformed : AMS.MEL.IR.Counter;
      Context                      : String) is
   begin
      if Values.Events_Received /= Received
        or else Values.Events_Dropped_Queue_Full /= Dropped
        or else Values.Malformed_Or_Unsupported /= Malformed
      then
         raise Program_Error with "Ada Track metadata counter mismatch in " & Context;
      end if;
   end Check_Counters;

   --  The mock emits the rich report synchronously from inside
   --  registerMetadataCallback while the channel is only attached.
   procedure Test_Attached_Registration (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-report");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
      Stream  : Meta.Metadata_Channel := Meta.Open (Channel, 4);
   begin
      if not Meta.Is_Open (Stream) then
         raise Program_Error with "Ada Track metadata did not open";
      end if;
      Check_Counters (Meta.Counters (Stream), 1, 0, 0, "attached registration");
      Check_Rich (Meta.Receive (Stream), "attached registration");
      --  A zero-timeout poll of an empty active queue times out.
      begin
         declare
            Ignored : constant Trk.IRST_Track_Report := Meta.Receive (Stream);
         begin
            raise Program_Error with "Ada Track metadata poll unexpectedly returned";
         end;
      exception
         when AMS.MEL.IR.Timeout_Error =>
            null;
      end;
      Meta.Close (Stream);
      --  Close is idempotent.
      Meta.Close (Stream);
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Attached_Registration;

   --  Registration is equally valid while enabled and remains one-shot.
   procedure Test_Enabled_Registration (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-report");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
   begin
      Trk.Enable (Channel);
      declare
         Stream : Meta.Metadata_Channel := Meta.Open (Channel, 4);
      begin
         Check_Rich (Meta.Receive (Stream), "enabled registration");
         --  Upstream has no unregister, so a second Open is refused.
         begin
            declare
               Second : Meta.Metadata_Channel := Meta.Open (Channel, 4);
            begin
               Meta.Close (Second);
               raise Program_Error with "Ada Track second registration was accepted";
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Meta.Close (Stream);
      end;
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Enabled_Registration;

   --  A null payload, an unknown state, and an unknown mode are each malformed
   --  independently: counted, never queued, and never poisoning the stream.
   procedure Test_Malformed (Provider_Path : String; Scenario : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
      Stream  : Meta.Metadata_Channel := Meta.Open (Channel, 4);
   begin
      Check_Counters (Meta.Counters (Stream), 1, 0, 1, Scenario);
      begin
         declare
            Ignored : constant Trk.IRST_Track_Report := Meta.Receive (Stream);
         begin
            raise Program_Error with "Ada Track malformed report was queued in " & Scenario;
         end;
      exception
         when AMS.MEL.IR.Timeout_Error =>
            null;
      end;
      Meta.Close (Stream);
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Malformed;

   --  Six reports into a capacity-2 queue: DROP-INCOMING retains the first two
   --  in arrival order and drops the last four.
   procedure Test_Overflow_FIFO (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-report-overflow");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
      Stream  : Meta.Metadata_Channel := Meta.Open (Channel, 2);
   begin
      Check_Counters (Meta.Counters (Stream), 6, 4, 0, "overflow");
      declare
         First  : constant Trk.IRST_Track_Report := Meta.Receive (Stream);
         Second : constant Trk.IRST_Track_Report := Meta.Receive (Stream);
      begin
         if First.Activity_ID /= 0 or else Second.Activity_ID /= 1 then
            raise Program_Error with "Ada Track overflow did not retain the first two in order";
         end if;
      end;
      begin
         declare
            Ignored : constant Trk.IRST_Track_Report := Meta.Receive (Stream);
         begin
            raise Program_Error with "Ada Track overflow queue held more than two";
         end;
      exception
         when AMS.MEL.IR.Timeout_Error =>
            null;
      end;
      Meta.Close (Stream);
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Overflow_FIFO;

   --  Closing the Track channel while metadata is open stops public
   --  consumption after any already queued report has drained.
   procedure Test_Track_Close_With_Metadata (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-report");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
      Stream  : Meta.Metadata_Channel := Meta.Open (Channel, 4);
   begin
      Trk.Close (Channel);
      Check_Rich (Meta.Receive (Stream), "Track close with metadata");
      begin
         declare
            Ignored : constant Trk.IRST_Track_Report := Meta.Receive (Stream);
         begin
            raise Program_Error with "Ada Track metadata continued after Track close";
         end;
      exception
         when AMS.MEL.IR.Stream_Stopped =>
            null;
      end;
      Check_Counters (Meta.Counters (Stream), 1, 0, 0, "Track close with metadata");
      Meta.Close (Stream);
      AMS.MEL.Close (Parent);
   end Test_Track_Close_With_Metadata;

   --  Session parent-first close: the Track owner keeps the graph alive, so
   --  the metadata stays usable until the Track owner releases it.
   procedure Test_Parent_First_Close (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-report");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
      Stream  : Meta.Metadata_Channel := Meta.Open (Channel, 4);
   begin
      AMS.MEL.Close (Parent);
      if AMS.MEL.Is_Open (Parent) then
         raise Program_Error with "Ada Track metadata parent session remained open";
      end if;
      Check_Rich (Meta.Receive (Stream), "parent-first close");
      Meta.Close (Stream);
      Trk.Close (Channel);
   end Test_Parent_First_Close;

   --  Finalization closes an un-closed metadata stream and Track channel
   --  without raising.
   procedure Test_Finalization (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-report");
   begin
      declare
         Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
         --  Neither owner is closed explicitly: finalization must release the
         --  metadata stream and then the Track channel without raising.
         Stream  : constant Meta.Metadata_Channel := Meta.Open (Channel, 4);
      begin
         Trk.Enable (Channel);
         Check_Rich (Meta.Receive (Stream), "finalization");
      end;
      AMS.MEL.Close (Parent);
   end Test_Finalization;

   --  A non-Success registration and a throwing registration both refuse to
   --  produce a stream, and both still consume the single permitted attempt.
   procedure Test_Registration_Failure (Provider_Path : String; Scenario : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
   begin
      begin
         declare
            Stream : Meta.Metadata_Channel := Meta.Open (Channel, 4);
         begin
            Meta.Close (Stream);
            raise Program_Error with "Ada Track registration failure was accepted";
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      begin
         declare
            Retry : Meta.Metadata_Channel := Meta.Open (Channel, 4);
         begin
            Meta.Close (Retry);
            raise Program_Error with "Ada Track retried a consumed registration";
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Registration_Failure;

   --  The @Optional RequestSystemTrackData arrives through the same bounded
   --  queue. Every field is checked against the distinctive rich request; the
   --  negative system time proves the signed nanosecond carrier survives.
   procedure Check_Rich_Request (Request : Meta.Request_System_Track_Data; Context : String) is
   begin
      if Request.System_Time_NS /= -8_765_432_109_876
        or else Request.Command_ID /= 16#C1234567#
        or else Request.Request_ID /= 16#D2345678#
        or else Request.Track_ID /= 16#E3456789#
      then
         raise Program_Error with "Ada Track request value mismatch in " & Context;
      end if;
   end Check_Rich_Request;

   procedure Test_Request_System_Track_Data (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-request-rich");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
      Stream  : Meta.Metadata_Channel := Meta.Open (Channel, 4);
   begin
      declare
         Event : constant Meta.Metadata_Event := Meta.Receive_Event (Stream);
      begin
         if Event.Kind /= Meta.Request_System_Track_Data_Event then
            raise Program_Error with "Ada Track returned the wrong metadata kind";
         end if;
         Check_Rich_Request (Event.Request, "request reception");
      end;
      --  A zero timeout on a drained active queue is a nonblocking poll.
      begin
         declare
            Ignored : constant Meta.Metadata_Event := Meta.Receive_Event (Stream);
         begin
            raise Program_Error with "Ada Track metadata returned an unexpected event";
         end;
      exception
         when AMS.MEL.IR.Timeout_Error =>
            null;
      end;
      Check_Counters (Meta.Counters (Stream), 1, 0, 0, "request reception");
      Meta.Close (Stream);
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Request_System_Track_Data;

   --  Both kinds share the one queue and must arrive in strict FIFO order,
   --  each carrying only its own payload.
   procedure Test_Mixed_Kinds (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-metadata-mixed");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
      Stream  : Meta.Metadata_Channel := Meta.Open (Channel, 8);
   begin
      declare
         First  : constant Meta.Metadata_Event := Meta.Receive_Event (Stream);
         Second : constant Meta.Metadata_Event := Meta.Receive_Event (Stream);
         Third  : constant Meta.Metadata_Event := Meta.Receive_Event (Stream);
      begin
         if First.Kind /= Meta.Request_System_Track_Data_Event
           or else Second.Kind /= Meta.IRST_Track_Report_Event
           or else Third.Kind /= Meta.Request_System_Track_Data_Event
         then
            raise Program_Error with "Ada Track mixed metadata order mismatch";
         end if;
         Check_Rich_Request (First.Request, "mixed first");
         Check_Rich (Second.Report, "mixed second");
         Check_Rich_Request (Third.Request, "mixed third");
      end;
      Check_Counters (Meta.Counters (Stream), 3, 0, 0, "mixed kinds");
      Meta.Close (Stream);
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Mixed_Kinds;

   --  A null RequestSystemTrackData is malformed: it is counted and nothing is
   --  queued, exactly as for the required report kind.
   procedure Test_Request_Null (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-request-null");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
      Stream  : Meta.Metadata_Channel := Meta.Open (Channel, 4);
   begin
      begin
         declare
            Ignored : constant Meta.Metadata_Event := Meta.Receive_Event (Stream);
         begin
            raise Program_Error with "Ada Track queued a null request";
         end;
      exception
         when AMS.MEL.IR.Timeout_Error =>
            null;
      end;
      Check_Counters (Meta.Counters (Stream), 1, 0, 1, "null request");
      Meta.Close (Stream);
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Request_Null;

   --  An @Optional callback the provider refuses must leave the
   --  @RequiredIfTrack report callback fully working.
   procedure Test_Optional_Refusal (Provider_Path : String) is
      Parent  : AMS.MEL.Session :=
        AMS.MEL.Open (Provider_Path, "track-request-register-not-supported");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
      Stream  : Meta.Metadata_Channel := Meta.Open (Channel, 4);
   begin
      Check_Rich (Meta.Receive (Stream), "optional refusal");
      Check_Counters (Meta.Counters (Stream), 1, 0, 0, "optional refusal");
      Meta.Close (Stream);
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Optional_Refusal;

   --  Exact all-field fidelity of the distinctive rich CandidateObjectMessage,
   --  wholly Ada owned. Every header field, every hot region, the complete
   --  inertial state, and every exposed candidate object is checked, and the
   --  exposed prefix length is checked to be exactly numberOfCOs.
   procedure Check_Rich_Candidate (Message : Meta.Candidate_Object_Message; Context : String) is
      H : constant Meta.Candidate_Object_Header := Meta.Header (Message);
      S : constant Meta.Sensor_Inertial_State := Meta.Inertial_State (Message);
   begin
      if H.Number_Of_COs /= 3
        or else H.Stack_Frame_Index /= 16#BEEF#
        or else H.CFAR /= 1.5309e-7
        or else H.Validity_Flag_Bitfield /= 16#A5C3#
        or else H.TOV_UTC_NS /= -4_433_221_100_998_877
      then
         raise Program_Error with "Ada candidate header mismatch in " & Context;
      end if;

      if Meta.Hot_Region_Count (Message) /= 3 then
         raise Program_Error with "Ada hot region count mismatch in " & Context;
      end if;
      declare
         R1 : constant Meta.Hot_Region := Meta.Hot_Region_At (Message, 1);
         R2 : constant Meta.Hot_Region := Meta.Hot_Region_At (Message, 2);
         R3 : constant Meta.Hot_Region := Meta.Hot_Region_At (Message, 3);
      begin
         if R1.Kind /= Meta.Flare
           or else R1.Size /= 1111
           or else R1.Top /= 2222
           or else R1.Left /= 3333
           or else R1.Right /= 4444
           or else R1.Bottom /= 5555
         then
            raise Program_Error with "Ada hot region 1 mismatch in " & Context;
         end if;
         if R2.Kind /= Meta.Solar
           or else R2.Size /= 6666
           or else R2.Top /= 7777
           or else R2.Left /= 8888
           or else R2.Right /= 9999
           or else R2.Bottom /= 10111
         then
            raise Program_Error with "Ada hot region 2 mismatch in " & Context;
         end if;
         if R3.Kind /= Meta.Mask
           or else R3.Size /= 12222
           or else R3.Top /= 13333
           or else R3.Left /= 14444
           or else R3.Right /= 15555
           or else R3.Bottom /= 16666
         then
            raise Program_Error with "Ada hot region 3 mismatch in " & Context;
         end if;
      end;

      if S.System_Time_NS /= -1_122_334_455_667_788
        or else S.Q_XYZW.X /= 0.125
        or else S.Q_XYZW.Y /= -0.25
        or else S.Q_XYZW.Z /= 0.375
        or else S.Q_XYZW.W /= -0.5
        or else S.Q_ECEF_XYZW.X /= -0.625
        or else S.Q_ECEF_XYZW.Y /= 0.75
        or else S.Q_ECEF_XYZW.Z /= -0.875
        or else S.Q_ECEF_XYZW.W /= 1.125
        or else S.Sensor_Position.X /= 1_234_567.25
        or else S.Sensor_Position.Y /= -2_345_678.5
        or else S.Sensor_Position.Z /= 3_456_789.75
        or else S.Sensor_Velocity.X /= -11.125
        or else S.Sensor_Velocity.Y /= 22.25
        or else S.Sensor_Velocity.Z /= -33.375
        or else S.Uncertainties.Sensor_Uncertainties /= 16#C0FFEE01#
        or else S.Uncertainties.Platform_Uncertainties /= 16#DEADBE02#
      then
         raise Program_Error with "Ada inertial state mismatch in " & Context;
      end if;

      --  Exactly the meaningful prefix is present: the sentinel in upstream
      --  slot 3 is never exposed.
      if Meta.Candidate_Object_Count (Message) /= 3 then
         raise Program_Error with "Ada candidate count mismatch in " & Context;
      end if;
      for Index in 1 .. 3 loop
         declare
            O      : constant Meta.Candidate_Object := Meta.Candidate_Object_At (Message, Index);
            Offset : constant Long_Float := Long_Float (Index - 1);
         begin
            if O.System_Time_NS /= -1_000_000_000_000 - Long_Long_Integer (Index - 1) * 7
              or else O.Detection_Category /= 16#11110000# + Interfaces.Unsigned_32 (Index - 1)
              or else O.Sensor_Index /= 16#22220000# + Interfaces.Unsigned_32 (Index - 1)
              or else O.Subpixel.Row /= 100.5 + Offset
              or else O.Subpixel.Column /= 200.25 + Offset
              or else O.Intensity /= 3000.125 + Offset
              or else O.Sensor_Relative_Unit.X /= 0.1 + Offset
              or else O.Sensor_Relative_Unit.Y /= -0.2 - Offset
              or else O.Sensor_Relative_Unit.Z /= 0.3 + Offset
              or else O.Signal_To_Interference_Ratio /= 40.5 + Offset
              or else O.Signal_To_Noise_Ratio /= -50.75 - Offset
            then
               raise Program_Error with "Ada candidate object mismatch in " & Context;
            end if;
         end;
      end loop;
   end Check_Rich_Candidate;

   --  Positive CandidateObjectMessage delivery, complete and wholly Ada owned
   --  after Receive_Event returns.
   procedure Test_Candidate_Message (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-candidate-rich");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
      Stream  : Meta.Metadata_Channel := Meta.Open (Channel, 4);
   begin
      Check_Counters (Meta.Counters (Stream), 1, 0, 0, "candidate message");
      declare
         Event : constant Meta.Metadata_Event := Meta.Receive_Event (Stream);
      begin
         if Event.Kind /= Meta.Candidate_Object_Message_Event then
            raise Program_Error with "Ada Track metadata kind is not candidate";
         end if;
         Check_Rich_Candidate (Event.Candidates, "candidate message");
      end;
      Meta.Close (Stream);
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Candidate_Message;

   --  Each malformed candidate payload is counted and enqueues nothing.
   procedure Test_Candidate_Malformed (Provider_Path : String; Scenario : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
      Stream  : Meta.Metadata_Channel := Meta.Open (Channel, 4);
   begin
      Check_Counters (Meta.Counters (Stream), 1, 0, 1, Scenario);
      begin
         declare
            Ignored : constant Meta.Metadata_Event := Meta.Receive_Event (Stream);
         begin
            raise Program_Error with "Ada candidate malformed unexpectedly queued";
         end;
      exception
         when AMS.MEL.IR.Timeout_Error =>
            null;
      end;
      Meta.Close (Stream);
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Candidate_Malformed;

   --  An advertised capability whose registration is refused must fail Open
   --  closed, and no metadata owner may escape.
   procedure Test_Candidate_Registration_Failure (Provider_Path : String; Scenario : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
   begin
      declare
         Ignored : Meta.Metadata_Channel := Meta.Open (Channel, 4);
      begin
         raise Program_Error with "Ada candidate registration unexpectedly succeeded";
      end;
   exception
      when AMS.MEL.Provider_Error =>
         Trk.Close (Channel);
         AMS.MEL.Close (Parent);
   end Test_Candidate_Registration_Failure;

   --  Deterministic three-kind FIFO across the one shared queue.
   procedure Test_Candidate_Mixed_FIFO (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-candidate-mixed");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
      Stream  : Meta.Metadata_Channel := Meta.Open (Channel, 8);
   begin
      Check_Counters (Meta.Counters (Stream), 3, 0, 0, "candidate mixed FIFO");
      declare
         First  : constant Meta.Metadata_Event := Meta.Receive_Event (Stream);
         Second : constant Meta.Metadata_Event := Meta.Receive_Event (Stream);
         Third  : constant Meta.Metadata_Event := Meta.Receive_Event (Stream);
      begin
         if First.Kind /= Meta.IRST_Track_Report_Event
           or else Second.Kind /= Meta.Candidate_Object_Message_Event
           or else Third.Kind /= Meta.Request_System_Track_Data_Event
         then
            raise Program_Error with "Ada Track mixed FIFO order mismatch";
         end if;
         Check_Rich (First.Report, "candidate mixed FIFO");
         Check_Rich_Candidate (Second.Candidates, "candidate mixed FIFO");
         if Third.Request.Command_ID /= 16#C1234567# then
            raise Program_Error with "Ada Track mixed FIFO request mismatch";
         end if;
      end;
      Meta.Close (Stream);
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Candidate_Mixed_FIFO;

   procedure Run (Provider_Path : String) is
   begin
      Test_Attached_Registration (Provider_Path);
      Test_Enabled_Registration (Provider_Path);
      Test_Malformed (Provider_Path, "track-report-null");
      Test_Malformed (Provider_Path, "track-report-bad-state");
      Test_Malformed (Provider_Path, "track-report-bad-mode");
      Test_Overflow_FIFO (Provider_Path);
      Test_Track_Close_With_Metadata (Provider_Path);
      Test_Parent_First_Close (Provider_Path);
      Test_Finalization (Provider_Path);
      Test_Registration_Failure (Provider_Path, "track-report-register-fail");
      Test_Registration_Failure (Provider_Path, "track-report-register-throw");
      Test_Request_System_Track_Data (Provider_Path);
      Test_Mixed_Kinds (Provider_Path);
      Test_Request_Null (Provider_Path);
      Test_Optional_Refusal (Provider_Path);
      --  @RequiredIfDetectCandidateObjects CandidateObjectMessage.
      Test_Candidate_Message (Provider_Path);
      Test_Candidate_Malformed (Provider_Path, "track-candidate-null");
      Test_Candidate_Malformed (Provider_Path, "track-candidate-too-many");
      Test_Candidate_Malformed (Provider_Path, "track-candidate-bad-region");
      Test_Candidate_Registration_Failure (Provider_Path, "track-candidate-register-not-supported");
      Test_Candidate_Registration_Failure (Provider_Path, "track-candidate-register-fail");
      Test_Candidate_Registration_Failure (Provider_Path, "track-candidate-register-throw");
      Test_Candidate_Mixed_FIFO (Provider_Path);
      Ada.Text_IO.Put_Line
        ("PASS: Ada IR Track IRSTTrackReport and RequestSystemTrackData metadata contract");
   end Run;
end AMS_MEL_IR_Track_Metadata_Tests;
