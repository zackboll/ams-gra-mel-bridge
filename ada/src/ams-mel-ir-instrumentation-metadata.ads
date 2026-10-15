private with Ada.Finalization;
private with AMS.MEL_C_API;

package AMS.MEL.IR.Instrumentation.Metadata is
   --  Bounded owned polling of instrumentation report callbacks. Native callbacks validate and
   --  copy the four-field report into a drop-incoming queue without calling Ada. Receive returns
   --  independent Ada values. Upstream has no callback unregister operation; public consumption
   --  closure is distinct from physical provider-channel destruction.

   type Metadata_Channel is limited private;
   --  Limited public metadata-consumption owner. Callback state belongs to the typed
   --  instrumentation channel, not this owner; finalization attempts non-raising consumption
   --  closure. Serialize operations on the same Ada object.
   function Open
     (Channel : Instrumentation_Channel; Queue_Capacity : Positive := 16) return Metadata_Channel;
   --  Registers the conditionally required report callback once per attached or enabled channel,
   --  including failed attempts. Provider registration can invoke the callback synchronously. A
   --  failed registration publishes no metadata owner but retains inactive callback state as
   --  required for safety; upstream provides no unregister operation. Queue ordering follows
   --  successful enqueue, not timestamps or concurrent callback entry order.
   --  @param Channel Attached or enabled typed instrumentation owner.
   --  @param Queue_Capacity Maximum queued reports; defaults to sixteen.
   --  @return Limited public polling owner.
   --  @exception Provider_Error Closed/ineligible channel, repeated registration, provider
   --  registration failure/exception or native failure.
   function Is_Open (Stream : Metadata_Channel) return Boolean;
   --  Tests local metadata handle ownership, not callback activity or channel enablement.
   --  @param Stream Public metadata owner to inspect.
   --  @return True when its local native handle is nonnull.

   function Receive
     (Stream : Metadata_Channel; Timeout_Milliseconds : Natural := 0) return Instrumentation_Report;
   --  Dequeues a complete native copied report, copies it into an Ada-owned record and closes the
   --  native temporary before return. No provider pointer escapes. Queued reports can drain after
   --  logical typed-channel closure or callback failure; an empty inactive queue raises Stream
   --  Stopped, and an empty failed queue raises Provider Error.
   --  @param Stream Open polling owner; serialize Receive and Close on this object.
   --  @param Timeout_Milliseconds Maximum queue wait in milliseconds; zero polls.
   --  @return Independent report surviving metadata, channel and Session closure.
   --  @exception Timeout_Error Active queue remained empty until the wait expired.
   --  @exception Stream_Stopped Inactive or stopped queue is empty.
   --  @exception Provider_Error Closed polling owner, failed callback state, invalid priority or
   --  native failure.

   type Metadata_Counters is record
      Events_Received, Events_Dropped_Queue_Full, Malformed_Or_Unsupported : Counter;
   end record;
   --  Saturating native callback counters, not counts of successful Ada receives.
   --  @field Events_Received Observed callback entries, including entries after consumption
   --  becomes inactive.
   --  @field Events_Dropped_Queue_Full Valid incoming reports dropped when the bounded queue is
   --  full; queued reports are not evicted.
   --  @field Malformed_Or_Unsupported Null reports or unknown priority values rejected by callback
   --  validation; copying/allocation exceptions instead fail the metadata stream.
   function Counters (Stream : Metadata_Channel) return Metadata_Counters;
   --  Copies current callback counters without consuming reports.
   --  @param Stream Open public polling owner.
   --  @return Independent snapshot of saturating counters.
   --  @exception Provider_Error Closed owner or native counter query failure.
   procedure Close (Stream : in out Metadata_Channel);
   --  Idempotently deactivates public consumption and wakes native waiters. It does not unregister
   --  the provider callback, destroy the typed channel or promise immediate callback quiescence.
   --  Callback state remains until actual provider-channel destruction and bridge callback
   --  draining, which can be deferred by pending requests. Finalization attempts the same public
   --  consumption closure without raising.
   --  @param Stream Public metadata owner to close; serialize against other uses.
   --  @exception Provider_Error Native metadata cleanup failure.

private
   type Metadata_Channel is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Instrumentation_Metadata_Handle :=
        AMS.MEL_C_API.Null_Instrumentation_Metadata;
   end record;
   overriding
   procedure Finalize (Stream : in out Metadata_Channel);
end AMS.MEL.IR.Instrumentation.Metadata;
