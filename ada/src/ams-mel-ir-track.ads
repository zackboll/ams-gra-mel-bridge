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
--  RequestSystemTrackData inbound request, the
--  @RequiredIfDetectCandidateObjects CandidateObjectMessage inbound metadata,
--  and the @Optional CandidateObjectPreProcMessage inbound metadata are all
--  implemented in the child packages.
--
--  Every published TrackChannel-specific surface is therefore represented and
--  the safe Ada Track API is complete. Positive Track behavior remains
--  mock-only: pinned Squall cannot attach a Track channel through
--  Control::attachChannel.
--
--  Facade lifecycle policy: Open attaches the upstream channel. Capabilities
--  is valid while attached or enabled. Enable explicitly calls upstream
--  Channel::enable and is idempotent once enabled. Close disables when Enable
--  was attempted, detaches, and releases the provider/session graph; a Track
--  channel keeps that graph alive independently of its parent Session.

package AMS.MEL.IR.Track is
   --  Safe Track attachment, lifecycle and complete inbound IRST report value.
   --  Positive Track behavior is mock-qualified only. Pinned Squall's Control
   --  attachment has no Track case and returns null; its real-provider evidence
   --  is an expected-negative unsupported path, not successful Track execution.
   --  Child packages separate inbound metadata from the two outbound sends.
   --  Serialize public operations and Close on a shared typed owner externally.

   type Track_Config is private;
   --  Owned channel/platform UCI identities and sensor component location for
   --  an IRST Track attachment; creating a value performs no provider operation.
   function Create_Config
     (Channel_ID : UCI_ID; Platform_ID : UCI_ID; Sensor_Location : Component_Location)
      return Track_Config;
   --  Copy identities and sensor location without attaching or enabling.
   --  @param Channel_ID Requested Track channel UCI identity and descriptive label.
   --  @param Platform_ID Platform UCI identity and descriptive label.
   --  @param Sensor_Location Sensor component offsets and location identity.
   --  @return Independent owned configuration for Track Open.

   type Track_Channel is limited private;
   --  Limited controlled owner of an attached provider Track channel, retaining
   --  the native Session graph independently of the parent Ada Session. Pending
   --  updates, system responses and common requests share native request counts
   --  and can retain resources after public closure. Finalization is non-raising.
   function Open (Parent : Session; Config : Track_Config) return Track_Channel;
   --  Require an open Session and published Track capability, attach using the
   --  Track category and verify the concrete Track interface. Configuration text
   --  is validated natively. Failure raises a provider error; no owner escapes.
   --  Pinned Squall cannot attach this category. Positive behavior is mock-only.
   --  @param Parent Open Session whose native graph is retained by the child.
   --  @param Config Owned attachment values copied before provider use.
   --  @return Attached, not Enabled, Track owner.
   function Is_Open (Channel : Track_Channel) return Boolean;
   --  Test local handle presence, not lifecycle eligibility or provider support.
   --  @param Channel Track owner to inspect without provider access.
   --  @return True for a present native handle, even if lifecycle has failed.
   procedure Enable (Channel : in out Track_Channel);
   --  Explicitly enable an Attached channel; already Enabled is idempotent.
   --  Provider refusal/exception raises a provider error and marks failure;
   --  retrying Enable does not recover a failed lifecycle.
   --  @param Channel Open Attached or already Enabled owner; serialize owner operations.
   function Capabilities (Channel : Track_Channel) return IR.Channel.Channel_Capability;
   --  Copy provider capabilities while Attached or Enabled. A closed/failed
   --  lifecycle or native conversion failure raises a provider error. Querying
   --  does not enable services or register callbacks.
   --  @param Channel Eligible Track owner, externally serialized against Close.
   --  @return Owned snapshot valid after channel, Session and provider closure.
   function As_Channel (Channel : Track_Channel) return IR.Channel.View;
   --  Allocate a weak common view without changing lifecycle. Closed owner or
   --  native failure raises a provider error. View closure leaves this typed
   --  owner intact; admitted common requests retain their own lifetime claims.
   --  @param Channel Open Track owner to reference weakly.
   --  @return Weak common view; its handle presence does not prove target existence.
   procedure Close (Channel : in out Track_Channel);
   --  Stop public admission and close the owner without cancelling futures.
   --  Physical disable/detach waits for all pending update, system-response and
   --  common requests. Disable is attempted if enabling was attempted; detach
   --  failure retains the graph/handle for retry. Disable failure can raise a
   --  provider error even after successful detach consumed the handle. Provider
   --  channel destruction, then callback-count drain, establishes quiescence;
   --  disable return alone does not. Orphaned unsafe cleanup retains resources.
   --  @param Channel Track owner to close; serialize against its public operations.

   --  Upstream IrstTrackState. The Idle literal is prefixed because
   --  IrstTrackMode declares an Idle of its own in the same scope.
   type IRST_Track_State is (State_Idle, Detected, Coast, Dropped);
   --  Reported upstream track-state categories; no bridge tracker is implemented.
   --  @enum State_Idle Idle report state, native value 0; prefixed to avoid mode-name collision.
   --  @enum Detected Detected report state, native value 1.
   --  @enum Coast Coast report state, native value 2; no extrapolation algorithm is implied.
   --  @enum Dropped Dropped report state, native value 3.
   for IRST_Track_State use (State_Idle => 0, Detected => 1, Coast => 2, Dropped => 3);

   --  Upstream IrstTrackMode.
   type IRST_Track_Mode is (Mode_Idle, Scan, Stare);
   --  Reported upstream tracking-mode categories, not commands to change mode.
   --  @enum Mode_Idle Idle report mode, native value 0; prefixed to avoid state-name collision.
   --  @enum Scan Scan report mode, native value 1.
   --  @enum Stare Stare report mode, native value 2.
   for IRST_Track_Mode use (Mode_Idle => 0, Scan => 1, Stare => 2);

   --  Owned Track-facing NED value. Deliberately independent of
   --  AMS.MEL.IR.Image so this package adds no Image dependency.
   type North_East_Down is record
      North : Long_Float;
      East  : Long_Float;
      Down  : Long_Float;
   end record;
   --  Copied north/east/down components independent of Image package types.
   --  Pinned directional/report declarations establish no component units or
   --  normalization; do not treat this as an automatically transformed position.
   --  @field North Reported north component without bridge conversion.
   --  @field East Reported east component without bridge conversion.
   --  @field Down Reported down component without bridge conversion.

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
   --  Complete owned IRST report copied through native double fields without
   --  clamping, filtering, normalization or estimation. Pinned prose labels the
   --  filtered NED field as latest measurement too; the separate getter identity
   --  is retained rather than inventing a filtering algorithm. Positive report
   --  fidelity and callback behavior are mock-qualified, not Squall-qualified.
   --  @field System_Time_NS Signed system-time nanosecond count; epoch unspecified.
   --  @field Activity_ID Reported unsigned 32-bit activity identifier.
   --  @field Measured_NED Latest measurement north/east/down components; units unspecified by pinned field.
   --  @field Measured_Intensity Latest measurement intensity; calibration/units unspecified.
   --  @field Measured_SNR Measurement SNR scalar; no dB or linear convention established.
   --  @field Filtered_NED Separate filtered north/east/down report; no bridge filtering or unit inference.
   --  @field Filtered_Intensity Provider filtered-intensity scalar; units unspecified.
   --  @field Filtered_SNR Provider filtered SNR scalar; no scale conversion.
   --  @field Range_M Range to target in meters.
   --  @field Range_Error_M Range-error magnitude in meters, as the plus/minus error in the upstream example.
   --  @field Spatial_Extent_Rad Angular extent of target in radians.
   --  @field Track_Quality Provider quality scalar, documented upstream as zero to one; bridge does not enforce that range.
   --  @field Clutter Provider clutter scalar; no unit, score range or normalization established.
   --  @field Age_NS Signed age since first detection in nanoseconds.
   --  @field State Provider-reported track state, validated against the known enum domain.
   --  @field Mode Provider-reported track mode, validated against the known enum domain.

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
