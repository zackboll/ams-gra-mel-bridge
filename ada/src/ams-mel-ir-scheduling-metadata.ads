with Interfaces;
private with Ada.Finalization;
private with AMS.MEL_C_API;

package AMS.MEL.IR.Scheduling.Metadata is
   --  Opt-in reception of the pinned @RequiredIfIRSTTrackTasking ExecuteTaskAck callback.
   --  Reception and provider task execution are separate events. An acknowledgement means only
   --  that the provider delivered this value: it proves neither completion nor success. No
   --  Scheduling exec/send, local request matching, identifier reordering or uniqueness rule is
   --  implemented. Qualification uses the separate mock provider only.

   Not_Supported : exception;
   --  The provider answered NotSupported for the conditional ExecuteTaskAck registration.
   --  The Scheduling owner and its inherited common Channel services are unaffected.

   type Execute_Task_Ack is record
      Command_ID : Interfaces.Unsigned_32 := 0;
      Request_ID : Interfaces.Unsigned_32 := 0;
   end record;
   --  Complete Ada-owned acknowledgement, independent of the native event and provider graph.
   --  @field Command_ID All 32 bits of upstream getCommandID(), including zero and high bits.
   --  @field Request_ID All 32 bits of upstream getReqId(), independent of Command_ID. Zero,
   --  unequal and duplicate identifiers are all valid; this is not a completion status.

   type Metadata_Counters is record
      Events_Received           : AMS.MEL.IR.Counter;
      Events_Dropped_Queue_Full : AMS.MEL.IR.Counter;
      Malformed_Or_Unsupported  : AMS.MEL.IR.Counter;
   end record;
   --  Saturating callback counters, not successful Ada Receive calls.
   --  @field Events_Received Callback entries, including inactive callbacks and rejected events.
   --  @field Events_Dropped_Queue_Full Valid incoming events dropped without snapshot allocation
   --  because the bounded queue is full. Already queued events are retained.
   --  @field Malformed_Or_Unsupported Null payloads (even while full) and callback allocation or
   --  copy failures. A failed event is rejected atomically; later reception can recover.

   type Execute_Task_Ack_Stream is limited private;
   --  Limited controlled consumption owner. Explicit Close is preferred; finalization attempts
   --  non-raising cleanup. Serialize operations and Close on the same stream externally. A
   --  Scheduling Close may run while Receive waits: it stops consumption and wakes the receiver.

   function Open_Execute_Task_Ack
     (Channel : Scheduling_Channel; Queue_Capacity : Positive) return Execute_Task_Ack_Stream;
   --  Register exactly the ExecuteTaskAck overload once per Attached or Enabled Scheduling
   --  channel, including failed attempts. No foundation operation implicitly registers it.
   --  Registration runs without bridge locks and may deliver synchronously. No unregister
   --  exists; after failure retained callback state is inactive. No task is submitted.
   --  @param Channel Attached or Enabled Scheduling owner, serialized against its closure.
   --  @param Queue_Capacity Maximum number of events in the bounded drop-incoming queue.
   --  @return Independent limited metadata consumption owner.
   --  @exception Not_Supported Provider answered NotSupported.
   --  @exception Provider_Error Ineligible/closed owner, second attempt, other refusal, provider
   --  exception or native allocation failure.

   function Receive_Execute_Task_Ack
     (Stream : Execute_Task_Ack_Stream; Timeout_Milliseconds : Natural := 0)
      return Execute_Task_Ack;
   --  Dequeue in successful-enqueue order; copy both identifiers and release the native event
   --  before returning, including on conversion failure. Queued events drain after Scheduling
   --  Close, even while common requests defer physical teardown; terminal cleanup failure is
   --  reported only after that queue drains. Reception is not execution completion or success.
   --  @param Stream Open metadata consumption owner.
   --  @param Timeout_Milliseconds Maximum queue wait in milliseconds; zero polls, not cancellation.
   --  @return Fully Ada-owned acknowledgement, valid after all owners and the provider close.
   --  @exception Timeout_Error Active queue remained empty until the wait expired.
   --  @exception Stream_Stopped Inactive or stopped queue is empty.
   --  @exception Provider_Error Closed stream, failed provider cleanup or native failure.

   function Execute_Task_Ack_Counters (Stream : Execute_Task_Ack_Stream) return Metadata_Counters;
   --  Copy saturating counters without consuming events.
   --  @param Stream Open consumption owner.
   --  @return Received, queue-full dropped and malformed counters.
   --  @exception Provider_Error Closed stream or native query failure.

   procedure Close (Stream : in out Execute_Task_Ack_Stream);
   --  Idempotently deactivate consumption, without unregistering, closing the Scheduling owner
   --  or waiting for callback quiescence. Already returned Ada values remain owned. Physical
   --  teardown retains the provider graph until in-flight bridge callbacks have drained.
   --  @param Stream Metadata owner to close.
   --  @exception Provider_Error Native closure failed.
private
   type Execute_Task_Ack_Stream is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Scheduling_Ack_Metadata_Handle :=
        AMS.MEL_C_API.Null_Scheduling_Ack_Metadata;
   end record;
   overriding
   procedure Finalize (Stream : in out Execute_Task_Ack_Stream);
end AMS.MEL.IR.Scheduling.Metadata;
