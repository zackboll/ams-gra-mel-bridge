private with Ada.Finalization;
private with AMS.MEL_C_API;

package AMS.MEL.IR.Channel.Metadata is
   --  Required ChannelCommsTestRep reception for Health, Image and StackedImage.
   --  C2 uses C2.Metadata.Enable_Comms_Test_Events and its existing shared queue.
   --  No callback is inferred from a request future. Serialize operations and
   --  Close on the same stream. No Ada code executes on a provider thread.

   type Comms_Metadata_Stream is limited private;
   --  Limited consumer of a native queue of copied communications-test reports.
   --  The stream has only a weak link to the typed channel; installed callbacks
   --  retain queue state independently of this owner. Explicit Close is preferred;
   --  finalization attempts non-raising cleanup. Serialize operations on the same
   --  stream. Callback delivery is independent of request-future completion.

   function Open (Channel : View; Queue_Capacity : Positive := 16) return Comms_Metadata_Stream;
   --  Explicit one-shot registration while Health/Stacked is Attached or Enabled,
   --  or Image is Attached/Running, including a control-only Image attachment.
   --  No layout or capability-advertisement prerequisite. Failure consumes the
   --  typed channel's attempt even through another weak View. Synchronous
   --  delivery is supported. Refusal/exception raises Provider_Error; no owner
   --  escapes. Partially installed callbacks retain inactive queue state.
   --  @param Channel Weak view of an eligible Health, Image or StackedImage owner
   --  whose communications-metadata registration has not been attempted.
   --  @param Queue_Capacity Positive maximum number of queued reports, default sixteen.
   --  @return Limited consumer of the channel's communications-test callback reports.
   --  @exception Provider_Error Closed, expired or ineligible channel, repeated
   --  registration attempt, provider refusal or exception, or native open failure.
   function Is_Open (Stream : Comms_Metadata_Stream) return Boolean;
   --  Local handle presence only; does not prove the typed channel is active.
   --  @param Stream Communications-metadata consumer to inspect.
   --  @return True when a local handle is present, including for a stopped queue.
   function Receive
     (Stream : Comms_Metadata_Stream; Timeout_Milliseconds : Natural := 0) return Comms_Test_Report;
   --  Copies one complete command/request ID pair, preserving all 32 bits.
   --  FIFO drops incoming events when full. Zero polls, finite timeout raises
   --  Timeout_Error without cancellation. Queued copies drain after typed
   --  closure, then Stream_Stopped (or Provider_Error for cleanup failure).
   --  Channel association is this stream, never a numeric request selector.
   --  @param Stream Open communications-metadata consumer.
   --  @param Timeout_Milliseconds Maximum queue wait in milliseconds; zero polls.
   --  @return Oldest queued report, copied independently of native owners and futures.
   --  @exception Timeout_Error Active queue remained empty until the wait expired,
   --  including an empty poll. Registration and requests are not cancelled.
   --  @exception Stream_Stopped Inactive or stopped queue is empty after queued
   --  reports have drained.
   --  @exception Provider_Error Closed stream, callback or typed-channel cleanup
   --  failure after queued reports drain, or native receive failure.
   type Metadata_Counters is record
      Events_Received           : Counter;
      Events_Dropped_Queue_Full : Counter;
      Malformed_Or_Unsupported  : Counter;
   end record;
   --  Snapshot of saturating callback diagnostics; each count stops at Counter'Last.
   --  @field Events_Received Callback invocations, including null reports, full-queue
   --  drops and reports delivered after admission stops; not a dequeue count.
   --  @field Events_Dropped_Queue_Full Incoming valid reports dropped because the
   --  active FIFO was full; previously accepted reports are preserved.
   --  @field Malformed_Or_Unsupported Null callback reports rejected without enqueueing.
   function Counters (Stream : Comms_Metadata_Stream) return Metadata_Counters;
   --  Saturating diagnostics, including null reports; does not consume events.
   --  @param Stream Open communications-metadata consumer, possibly stopped.
   --  @return Snapshot of received, queue-full dropped and malformed report counts.
   --  @exception Provider_Error Closed stream or native counter-query failure.
   procedure Close (Stream : in out Comms_Metadata_Stream);
   --  Idempotently deactivate/release consumption, not unregister the callback.
   --  Does not retain or close the typed channel, or promise disable quiescence.
   --  Prefer explicit Close; finalization attempts non-raising cleanup.
   --  @param Stream Consumer to deactivate and release; already closed is harmless.
   --  @exception Provider_Error Native close failure.
private
   type Stream_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Comms_Metadata_Handle := AMS.MEL_C_API.Null_Comms_Metadata;
   end record;
   overriding
   procedure Finalize (Owner : in out Stream_Owner);
   type Comms_Metadata_Stream is limited record
      Owner : Stream_Owner;
   end record;
end AMS.MEL.IR.Channel.Metadata;
