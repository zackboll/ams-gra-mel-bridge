private with Ada.Finalization;
private with AMS.MEL_C_API;
with AMS.MEL.IR.Channel;

--  Required StackedImage channel ownership and common communications, including
--  sensors with no stack acquisition capability. No buffers, image listeners,
--  stack callbacks or optional LOS3D metadata registration are created. Required
--  buffer registration/unregistration remains unbound; Disable is available only
--  as Close cleanup. Communications metadata is available through
--  AMS.MEL.IR.Channel.Metadata using As_Channel. This is not a stack receiver.
--  Missing discovery is an expected Provider_Error and does not poison Session.

package AMS.MEL.IR.Stacked_Image is
   --  Safe Stacked attachment and lifecycle only. Serialize public
   --  operations and Close on a shared typed owner externally.

   type Stacked_Config is private;
   --  Owned channel/platform UCI identities and sensor component location for a
   --  Stacked attachment; creating a value performs no provider operation.
   function Create_Config
     (Channel_ID : UCI_ID; Platform_ID : UCI_ID; Sensor_Location : Component_Location)
      return Stacked_Config;
   --  Copy identities and sensor location without attaching or enabling.
   --  @param Channel_ID Requested Stacked channel UCI identity and descriptive label.
   --  @param Platform_ID Platform UCI identity and descriptive label.
   --  @param Sensor_Location Sensor component offsets and location identity.
   --  @return Independent owned configuration for Stacked Open.

   type Stacked_Channel is limited private;
   --  Limited controlled owner of an attached provider Stacked channel. It
   --  retains the native Session graph and provider library independently of
   --  the parent Ada Session. Admitted common requests keep their own native
   --  lifetime and can defer physical teardown after public closure.
   --  Finalization is non-raising.
   function Open (Parent : Session; Config : Stacked_Config) return Stacked_Channel;
   --  Require an open Session that publishes the Stacked category, attach
   --  once with ChannelType StackedImage (value 6), and verify the concrete
   --  StackedImageChannel interface and its advertised category. Configuration
   --  text is validated natively. Failure raises a provider error and no owner
   --  escapes; an ownership-uncertain incompatible attachment stays retained.
   --  @param Parent Open Session whose native graph is retained by the child.
   --  @param Config Owned attachment values copied before provider use.
   --  @return Attached, not Enabled, Stacked owner.
   function Is_Open (Channel : Stacked_Channel) return Boolean;
   --  Test local handle presence, not lifecycle eligibility or provider support.
   --  @param Channel Stacked owner to inspect without provider access.
   --  @return True for a present native handle, even if lifecycle has failed.
   procedure Enable (Channel : in out Stacked_Channel);
   --  Explicitly call the inherited Channel enable on an Attached owner; an
   --  already Enabled owner is idempotent. Provider refusal or exception raises
   --  a provider error and marks failure; retrying Enable does not recover it.
   --  Enabling allocates no buffers and installs no listeners.
   --  @param Channel Open Attached or already Enabled owner; serialize owner operations.
   function Capabilities (Channel : Stacked_Channel) return AMS.MEL.IR.Channel.Channel_Capability;
   --  Copy the complete provider snapshot while Attached or Enabled, preserving
   --  zero layout fields and empty nested collections. No acquisition format is
   --  inferred. Closed/failed lifecycle or conversion errors raise Provider_Error.
   --  @param Channel Eligible Stacked owner, externally serialized against Close.
   --  @return Owned snapshot valid after channel, Session and provider closure.
   function As_Channel (Channel : Stacked_Channel) return AMS.MEL.IR.Channel.View;
   --  Allocate a weak common view (KeepAlive, CommsTest, Capabilities, Disable) without
   --  changing lifecycle. Closing the view leaves this owner intact; closing
   --  this owner makes later view operations fail. Admitted common requests
   --  retain their own lifetime and use Stacked-only request accounting.
   --  @param Channel Open Stacked owner to reference weakly.
   --  @return Weak common view; its handle presence does not prove target existence.
   procedure Close (Channel : in out Stacked_Channel);
   --  Stop public admission and close the owner without cancelling futures.
   --  Use Channel.Disable for non-terminal disable. Close calls provider disable
   --  only for an outstanding enable cycle, then detaches. Physical teardown waits for
   --  admitted common requests. A detach failure retains the graph and handle
   --  for retry; a disable failure after a successful detach still raises.
   --  @param Channel Stacked owner to close; serialize against its public operations.

private
   type Stacked_Config is record
      Channel, Platform : UCI_ID;
      Location          : Component_Location;
   end record;
   type Stacked_Channel is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Stacked_Handle := AMS.MEL_C_API.Null_Stacked;
   end record;
   overriding
   procedure Finalize (Channel : in out Stacked_Channel);
end AMS.MEL.IR.Stacked_Image;
