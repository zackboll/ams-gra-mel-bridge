private with Ada.Finalization;
private with AMS.MEL_C_API;
with AMS.MEL.IR.Channel;

--  Conditionally required IR MEL Scheduling channel (@RequiredIfIRSTTrackTasking).
--  IRMEL-ADA-011 implements only the Scheduling channel ownership/lifecycle
--  foundation: Open, explicit Enable, owned Capabilities, the weak inherited
--  common Channel view, and Close. None of the Scheduling-specific upstream
--  operations is implemented: the four task exec overloads (AbortTask,
--  IRSTScanTask, IRSTStareTask, IRSTTrackTask), the PrioritizedTrackListCmd
--  send and the six Scheduling metadata callbacks have no binding, and no task
--  is ever submitted or reported as scheduled.
--
--  The upstream channel is conditional on provider tasking support. A provider
--  that does not publish the Scheduling category makes Open raise a provider
--  error; this is an expected unsupported-provider result, not a failure of the
--  Session, which remains usable. Qualification is mock-provider only.

package AMS.MEL.IR.Scheduling is
   --  Safe Scheduling attachment and lifecycle only. Serialize public
   --  operations and Close on a shared typed owner externally.

   type Scheduling_Config is private;
   --  Owned channel/platform UCI identities and sensor component location for a
   --  Scheduling attachment; creating a value performs no provider operation.
   function Create_Config
     (Channel_ID : UCI_ID; Platform_ID : UCI_ID; Sensor_Location : Component_Location)
      return Scheduling_Config;
   --  Copy identities and sensor location without attaching or enabling.
   --  @param Channel_ID Requested Scheduling channel UCI identity and descriptive label.
   --  @param Platform_ID Platform UCI identity and descriptive label.
   --  @param Sensor_Location Sensor component offsets and location identity.
   --  @return Independent owned configuration for Scheduling Open.

   type Scheduling_Channel is limited private;
   --  Limited controlled owner of an attached provider Scheduling channel. It
   --  retains the native Session graph and provider library independently of
   --  the parent Ada Session. Admitted common requests keep their own native
   --  lifetime and can defer physical teardown after public closure.
   --  Finalization is non-raising.
   function Open (Parent : Session; Config : Scheduling_Config) return Scheduling_Channel;
   --  Require an open Session that publishes the Scheduling category, attach
   --  once with ChannelType Scheduling (value 3), and verify the concrete
   --  SchedulingChannel interface and its advertised category. Configuration
   --  text is validated natively. Failure raises a provider error and no owner
   --  escapes; an ownership-uncertain incompatible attachment stays retained.
   --  @param Parent Open Session whose native graph is retained by the child.
   --  @param Config Owned attachment values copied before provider use.
   --  @return Attached, not Enabled, Scheduling owner.
   function Is_Open (Channel : Scheduling_Channel) return Boolean;
   --  Test local handle presence, not lifecycle eligibility or provider support.
   --  @param Channel Scheduling owner to inspect without provider access.
   --  @return True for a present native handle, even if lifecycle has failed.
   procedure Enable (Channel : in out Scheduling_Channel);
   --  Explicitly call the inherited Channel enable on an Attached owner; an
   --  already Enabled owner is idempotent. Provider refusal or exception raises
   --  a provider error and marks failure; retrying Enable does not recover it.
   --  Enabling does not submit, queue or schedule any task.
   --  @param Channel Open Attached or already Enabled owner; serialize owner operations.
   function Capabilities
     (Channel : Scheduling_Channel) return AMS.MEL.IR.Channel.Channel_Capability;
   --  Copy provider capabilities while Attached or Enabled. Task_Schedule_Depth
   --  is reported verbatim; zero is a valid mode in which the application manages
   --  task priority and does not mean Scheduling is unsupported. A closed/failed
   --  lifecycle or conversion failure raises a provider error.
   --  @param Channel Eligible Scheduling owner, externally serialized against Close.
   --  @return Owned snapshot valid after channel, Session and provider closure.
   function As_Channel (Channel : Scheduling_Channel) return AMS.MEL.IR.Channel.View;
   --  Allocate a weak common view (KeepAlive, CommsTest, Capabilities) without
   --  changing lifecycle. Closing the view leaves this owner intact; closing
   --  this owner makes later view operations fail. Admitted common requests
   --  retain their own lifetime and use Scheduling-only request accounting.
   --  @param Channel Open Scheduling owner to reference weakly.
   --  @return Weak common view; its handle presence does not prove target existence.
   procedure Close (Channel : in out Scheduling_Channel);
   --  Stop public admission and close the owner without cancelling futures.
   --  There is no separate Disable: Close calls the inherited provider disable
   --  only if Enable was attempted, then detaches. Physical teardown waits for
   --  admitted common requests. A detach failure retains the graph and handle
   --  for retry; a disable failure after a successful detach still raises.
   --  @param Channel Scheduling owner to close; serialize against its public operations.

private
   type Scheduling_Config is record
      Channel, Platform : UCI_ID;
      Location          : Component_Location;
   end record;
   type Scheduling_Channel is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Scheduling_Handle := AMS.MEL_C_API.Null_Scheduling;
   end record;
   overriding
   procedure Finalize (Channel : in out Scheduling_Channel);
end AMS.MEL.IR.Scheduling;
