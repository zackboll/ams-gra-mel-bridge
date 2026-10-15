private with Ada.Finalization;
private with AMS.MEL_C_API;
with AMS.MEL.IR.Channel;

package AMS.MEL.IR.Health_Status is
   --  Typed HealthAndStatus attachment, explicit enablement and common-channel integration. The
   --  child metadata package binds six health/status callback families using the canonical
   --  AMS.MEL.Status data types. This package does not duplicate inherited common channel
   --  services.

   type Health_Config is private;
   --  Owned configuration for attaching a typed HealthAndStatus channel; constructing it does not
   --  contact a provider.
   function Create_Config
     (Channel_ID : UCI_ID; Platform_ID : UCI_ID; Sensor_Location : Component_Location)
      return Health_Config;
   --  Builds an independent health-channel attachment configuration without enabling or
   --  registering metadata.
   --  @param Channel_ID Channel identity to pass to the provider.
   --  @param Platform_ID Platform identity to pass to the provider.
   --  @param Sensor_Location Component location with meter offsets and location identity.
   --  @return Owned configuration retaining all supplied identity and location values.

   type Health_Channel is limited private;
   --  Limited typed owner, initially closed. Open attaches; Enable is explicit. Native state
   --  retains the Session/provider while this child needs it. Closing the public Session does not
   --  invalidate an existing channel. A weak common view does not keep this typed owner alive.
   --  Finalization attempts non-raising cleanup; failed detach can deliberately retain native
   --  resources for safety. Serialize lifecycle operations on the same Ada owner.
   function Open (Parent : Session; Config : Health_Config) return Health_Channel;
   --  Attaches a HealthAndStatus channel, verifies its dynamic type and advertised channel
   --  capability, and publishes an owner only on success. Does not enable it. Metadata
   --  registration and capability queries are allowed while attached, before Enable. Native
   --  validation requires UTF-8 identity/location text without embedded NUL. Failed
   --  attachment/type validation attempts cleanup; uncertain detach retains the provider graph.
   --  @param Parent Open IR Session whose provider supplies the child channel.
   --  @param Config Complete attachment identity and sensor location.
   --  @return Attached, not yet enabled, limited health-channel owner.
   --  @exception Provider_Error Closed Session, invalid configuration text, provider
   --  attachment/capability/type failure or native failure.
   function Is_Open (Channel : Health_Channel) return Boolean;
   --  Tests only local typed-channel handle ownership, not whether the channel is enabled or
   --  provider operations will succeed.
   --  @param Channel Typed owner to inspect.
   --  @return True when its local native handle is nonnull.
   procedure Enable (Channel : in out Health_Channel);
   --  Explicitly enables the attached provider channel. Repeated Enable after success is a no-op;
   --  failure marks native lifecycle failed and requires cleanup rather than implying enabled
   --  service. Metadata may be registered before Enable.
   --  @param Channel Attached owner to enable, or already enabled owner.
   --  @exception Provider_Error Closed/failed owner, provider rejection/exception or native
   --  failure.
   function Capabilities (Channel : Health_Channel) return IR.Channel.Channel_Capability;
   --  Copies common capabilities while the channel is attached or enabled. Returned arrays and
   --  text are Ada-owned and do not retain the typed owner.
   --  @param Channel Attached or enabled health channel.
   --  @return Independent common channel capability snapshot.
   --  @exception Provider_Error Invalid lifecycle, provider capability failure or native
   --  conversion failure.
   function As_Channel (Channel : Health_Channel) return IR.Channel.View;
   --  Creates a weak common Channel view while attached or enabled. Inherited KeepAlive, CommsTest
   --  and common capability services use common Channel support, not duplicated health methods.
   --  This view neither owns nor keeps the typed Health Channel alive; closing it does not close
   --  that owner.
   --  @param Channel Live attached or enabled typed channel.
   --  @return New weak common channel view.
   --  @exception Provider_Error Closed/failed owner or native view creation failure.
   procedure Close (Channel : in out Health_Channel);
   --  Idempotently closes the typed public owner, deactivating metadata consumption. Native
   --  cleanup disables if enable was attempted and detaches before destroying the provider
   --  channel, then drains bridge callbacks. Pending inherited common requests can defer physical
   --  cleanup after logical Close. A synchronous detach failure retains ownership for retry;
   --  uncertain deferred failure deliberately retains the graph. Finalization attempts cleanup
   --  without raising and does not guarantee immediate unload. Closing a common view is not a
   --  substitute for closing this owner.
   --  @param Channel Typed owner to close; do not race other operations on this owner.
   --  @exception Provider_Error Disable/detach or native cleanup failure; a retained handle may
   --  require explicit cleanup retry.

private
   type Health_Config is record
      Channel, Platform : UCI_ID;
      Location          : Component_Location;
   end record;
   type Health_Channel is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Health_Handle := AMS.MEL_C_API.Null_Health;
   end record;
   overriding
   procedure Finalize (Channel : in out Health_Channel);
end AMS.MEL.IR.Health_Status;
