private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with AMS.MEL.IR.Channel;
with Interfaces;

--  Conditionally required IR MEL Track channel family (@RequiredIfTrack).
--  This package implements the Track channel ownership/lifecycle foundation
--  (Open, Enable, Capabilities, Close) and the safe value types for the
--  @RequiredIfTrack IRSTTrackReport callback. The bounded owned polling API
--  for that callback lives in the child package AMS.MEL.IR.Track.Metadata.
--
--  Current status: the @RequiredIfTrackUpdate TrackDataUpdate send, the
--  @Optional SystemTrackDataResponse send, the @Optional
--  RequestSystemTrackData inbound request, and the
--  @RequiredIfDetectCandidateObjects CandidateObjectMessage inbound metadata
--  are all implemented in the child packages.
--
--  Deliberately absent in this release: the @Optional
--  CandidateObjectPreProcMessage callback. The Track API as a whole is
--  therefore NOT complete.
--
--  Facade lifecycle policy: Open attaches the upstream channel. Capabilities
--  is valid while attached or enabled. Enable explicitly calls upstream
--  Channel::enable and is idempotent once enabled. Close disables when Enable
--  was attempted, detaches, and releases the provider/session graph; a Track
--  channel keeps that graph alive independently of its parent Session.

package AMS.MEL.IR.Track is
   type Track_Config is private;
   function Create_Config
     (Channel_ID : UCI_ID; Platform_ID : UCI_ID; Sensor_Location : Component_Location)
      return Track_Config;

   type Track_Channel is limited private;
   function Open (Parent : Session; Config : Track_Config) return Track_Channel;
   function Is_Open (Channel : Track_Channel) return Boolean;
   procedure Enable (Channel : in out Track_Channel);
   function Capabilities (Channel : Track_Channel) return IR.Channel.Channel_Capability;
   procedure Close (Channel : in out Track_Channel);

   --  Upstream IrstTrackState. The Idle literal is prefixed because
   --  IrstTrackMode declares an Idle of its own in the same scope.
   type IRST_Track_State is (State_Idle, Detected, Coast, Dropped);
   for IRST_Track_State use (State_Idle => 0, Detected => 1, Coast => 2, Dropped => 3);

   --  Upstream IrstTrackMode.
   type IRST_Track_Mode is (Mode_Idle, Scan, Stare);
   for IRST_Track_Mode use (Mode_Idle => 0, Scan => 1, Stare => 2);

   --  Owned Track-facing NED value. Deliberately independent of
   --  AMS.MEL.IR.Image so this package adds no Image dependency.
   type North_East_Down is record
      North : Long_Float;
      East  : Long_Float;
      Down  : Long_Float;
   end record;

   --  Complete IRSTTrackReport. Every upstream getter is represented exactly
   --  once and no floating-point value is clamped or normalized.
   type IRST_Track_Report is record
      System_Time_NS : Long_Long_Integer;
      Activity_ID    : Interfaces.Unsigned_32;

      Measured_NED       : North_East_Down;
      Measured_Intensity : Long_Float;
      Measured_SNR       : Long_Float;

      Filtered_NED       : North_East_Down;
      Filtered_Intensity : Long_Float;
      Filtered_SNR       : Long_Float;

      Range_M            : Long_Float;
      Range_Error_M      : Long_Float;
      Spatial_Extent_Rad : Long_Float;
      Track_Quality      : Long_Float;
      Clutter            : Long_Float;

      Age_NS : Long_Long_Integer;

      State : IRST_Track_State;
      Mode  : IRST_Track_Mode;
   end record;

private
   package US renames Ada.Strings.Unbounded;
   type Track_Config is record
      Channel, Platform : UCI_ID;
      Location          : Component_Location;
   end record;
   type Track_Channel is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Track_Handle := AMS.MEL_C_API.Null_Track;
   end record;
   overriding
   procedure Finalize (Channel : in out Track_Channel);
end AMS.MEL.IR.Track;
