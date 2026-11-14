private with Ada.Finalization;
private with AMS.MEL_C_API;

package AMS.MEL.IR.Channel.Metadata is
   --  Required ChannelCommsTestRep reception for Health, Image and StackedImage.
   --  C2 uses C2.Metadata.Enable_Comms_Test_Events and its existing shared queue.
   --  No callback is inferred from a request future. Serialize operations and
   --  Close on the same stream. No Ada code executes on a provider thread.
   type Comms_Metadata_Stream is limited private;

   function Open (Channel : View; Queue_Capacity : Positive := 16) return Comms_Metadata_Stream;
   --  Explicit one-shot registration while Health/Stacked is Attached or Enabled,
   --  or Image is Attached/Running, including a control-only Image attachment.
   --  No layout or capability-advertisement prerequisite. Failure consumes the
   --  typed channel's attempt even through another weak View. Synchronous
   --  delivery is supported. Refusal/exception raises Provider_Error; no owner
   --  escapes. Partially installed callbacks retain inactive queue state.
   function Is_Open (Stream : Comms_Metadata_Stream) return Boolean;
   --  Local handle presence only; does not prove the typed channel is active.
   function Receive
     (Stream : Comms_Metadata_Stream; Timeout_Milliseconds : Natural := 0) return Comms_Test_Report;
   --  Copies one complete command/request ID pair, preserving all 32 bits.
   --  FIFO drops incoming events when full. Zero polls, finite timeout raises
   --  Timeout_Error without cancellation. Queued copies drain after typed
   --  closure, then Stream_Stopped (or Provider_Error for cleanup failure).
   --  Channel association is this stream, never a numeric request selector.
   type Metadata_Counters is record
      Events_Received           : Counter;
      Events_Dropped_Queue_Full : Counter;
      Malformed_Or_Unsupported  : Counter;
   end record;
   function Counters (Stream : Comms_Metadata_Stream) return Metadata_Counters;
   --  Saturating diagnostics, including null reports; does not consume events.
   procedure Close (Stream : in out Comms_Metadata_Stream);
   --  Idempotently deactivate/release consumption, not unregister the callback.
   --  Does not retain or close the typed channel, or promise disable quiescence.
   --  Prefer explicit Close; finalization attempts non-raising cleanup.
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
