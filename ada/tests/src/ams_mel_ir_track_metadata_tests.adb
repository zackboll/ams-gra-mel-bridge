with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Track;
with AMS.MEL.IR.Track.Metadata;
with Interfaces;

--  Task 029B2 covers exactly the @RequiredIfTrack IRSTTrackReport callback.
--  TrackDataUpdate, SystemTrackDataResponse, CandidateObjectMessage,
--  CandidateObjectPreProcMessage, and RequestSystemTrackData remain
--  unimplemented and are therefore not exercised here.

package body AMS_MEL_IR_Track_Metadata_Tests is
   package Trk renames AMS.MEL.IR.Track;
   package Meta renames AMS.MEL.IR.Track.Metadata;
   use type Interfaces.Unsigned_32;
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
      Ada.Text_IO.Put_Line ("PASS: Ada IR Track IRSTTrackReport metadata contract");
   end Run;
end AMS_MEL_IR_Track_Metadata_Tests;
