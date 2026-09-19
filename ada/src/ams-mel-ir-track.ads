private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with AMS.MEL.IR.Channel;

--  Conditionally required IR MEL Track channel family (@RequiredIfTrack).
--  This package implements only the Track channel ownership/lifecycle
--  foundation: Open, Enable, Capabilities, and Close.
--
--  Deliberately absent in this release, and reserved for the Track report
--  slice: the IRSTTrackReport metadata callback, TrackDataUpdate,
--  SystemTrackDataResponse, CandidateObjectMessage,
--  CandidateObjectPreProcMessage, and RequestSystemTrackData. No Track
--  metadata package, Track enumerations, or NED type are declared here.
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
