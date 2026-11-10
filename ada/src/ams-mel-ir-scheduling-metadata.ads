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

   --  IRMEL-ADA-013: opt-in reception of the pinned @RequiredIfIRSTTrackTasking
   --  TaskExecutingRep callback, independent of ExecuteTaskAck. A report describes what the
   --  provider says is executing; it is not a task request and no task is executed here.

   type Task_Kind is
     (No_Task,
      Abort_Task,
      IRST_Camera_Calibration,
      Camera_Calibration,
      IRST_Boresight,
      Idle,
      IRST_Scan,
      IRST_Position,
      IRST_Single_Target_Track,
      IRST_Multiple_Target_Track,
      IRST_Update,
      Reserved_1,
      Reserved_2,
      Reserved_3,
      Reserved_4,
      Reserved_5,
      Reserved_6);
   --  Every defined pinned upstream Task value, in numeric order 0..16. Reserved_1 through
   --  Reserved_6 are preserved exactly as reported but carry no operational meaning and are not
   --  supported or executable task kinds. The NumTaskTypes count sentinel (17) and every larger
   --  value are not representable: such a report is rejected whole.
   --  @enum No_Task Upstream NoTask, value 0.
   --  @enum Abort_Task Upstream AbortTask, value 1; a reported kind, not an abort request.
   --  @enum IRST_Camera_Calibration Upstream IRSTCamCal, value 2.
   --  @enum Camera_Calibration Upstream CamCal, value 3.
   --  @enum IRST_Boresight Upstream IRSTBoresight, value 4.
   --  @enum Idle Upstream Idle, value 5.
   --  @enum IRST_Scan Upstream IRSTScan, value 6.
   --  @enum IRST_Position Upstream IRSTPosition, value 7.
   --  @enum IRST_Single_Target_Track Upstream IRSTSTT, value 8.
   --  @enum IRST_Multiple_Target_Track Upstream IRSTMTT, value 9.
   --  @enum IRST_Update Upstream IRSTUpdate, value 10.
   --  @enum Reserved_1 Upstream Reserved1, value 11; preserved, non-operational.
   --  @enum Reserved_2 Upstream Reserved2, value 12; preserved, non-operational.
   --  @enum Reserved_3 Upstream Reserved3, value 13; preserved, non-operational.
   --  @enum Reserved_4 Upstream Reserved4, value 14; preserved, non-operational.
   --  @enum Reserved_5 Upstream Reserved5, value 15; preserved, non-operational.
   --  @enum Reserved_6 Upstream Reserved6, value 16; preserved, non-operational.
   for Task_Kind use
     (No_Task                    => 0,
      Abort_Task                 => 1,
      IRST_Camera_Calibration    => 2,
      Camera_Calibration         => 3,
      IRST_Boresight             => 4,
      Idle                       => 5,
      IRST_Scan                  => 6,
      IRST_Position              => 7,
      IRST_Single_Target_Track   => 8,
      IRST_Multiple_Target_Track => 9,
      IRST_Update                => 10,
      Reserved_1                 => 11,
      Reserved_2                 => 12,
      Reserved_3                 => 13,
      Reserved_4                 => 14,
      Reserved_5                 => 15,
      Reserved_6                 => 16);

   subtype Operational_Task_Kind is Task_Kind range No_Task .. IRST_Update;
   --  The eleven meaningful pinned Task values 0..10. A reserved value is never a member.

   function Is_Operational (Kind : Task_Kind) return Boolean
   is (Kind in Operational_Task_Kind);
   --  Whether a reported task kind is one of the meaningful pinned values.
   --  @param Kind Reported current or previous task kind.
   --  @return False exactly for Reserved_1 .. Reserved_6.

   type Task_Completion_Status is (Status_No_Task, Complete, Timeout, Abort_Status, Skipped);
   --  Pinned upstream TaskCompStatus, values 0..4, a type separate from Task_Kind. In a report
   --  it describes the PREVIOUS task only, never the current task. Other values are rejected.
   --  @enum Status_No_Task Upstream NoTask, value 0; prefixed to avoid the Task_Kind literal.
   --  @enum Complete Upstream Complete, value 1.
   --  @enum Timeout Upstream Timeout, value 2; a reported outcome, not a bridge timeout.
   --  @enum Abort_Status Upstream Abort, value 3; renamed because abort is reserved in Ada.
   --  @enum Skipped Upstream Skipped, value 4.
   for Task_Completion_Status use
     (Status_No_Task => 0, Complete => 1, Timeout => 2, Abort_Status => 3, Skipped => 4);

   type Task_Executing_Report is record
      Command_ID            : Interfaces.Unsigned_32 := 0;
      Activity_ID           : Interfaces.Unsigned_32 := 0;
      Request_ID            : Interfaces.Unsigned_32 := 0;
      Current_Task          : Task_Kind := No_Task;
      Estimated_Duration_NS : Interfaces.Integer_64 := 0;
      Track_ID              : Interfaces.Integer_32 := 0;
      Previous_Activity_ID  : Interfaces.Unsigned_32 := 0;
      Previous_Request_ID   : Interfaces.Unsigned_32 := 0;
      Previous_Task         : Task_Kind := No_Task;
      Previous_Task_Status  : Task_Completion_Status := Status_No_Task;
   end record;
   --  Complete Ada-owned report of all ten upstream getters, independent of the native event and
   --  provider graph. Defaults equal the upstream defaults. Current and previous values are
   --  independent; no relationship between them is inferred or enforced.
   --  @field Command_ID All 32 bits of upstream getCommandID().
   --  @field Activity_ID All 32 bits of upstream getActivityId() for the current task.
   --  @field Request_ID All 32 bits of upstream getReqId() for the current task.
   --  @field Current_Task Upstream getTask(), the reported currently executing task kind.
   --  @field Estimated_Duration_NS Exact signed upstream getEstimatedDuration().count() in
   --  nanoseconds; never converted to seconds, Duration or a time point. Negative is preserved.
   --  @field Track_ID Signed upstream getTrackId(); negative and zero values are preserved.
   --  @field Previous_Activity_ID All 32 bits of upstream getPrevActivityId().
   --  @field Previous_Request_ID All 32 bits of upstream getPrevReqId().
   --  @field Previous_Task Upstream getPrevTask(), the reported previous task kind.
   --  @field Previous_Task_Status Upstream getPrevTaskStatus(): the previous task's reported
   --  completion status, not a status of Current_Task.

   type Task_Executing_Stream is limited private;
   --  Limited controlled TaskExecutingRep consumption owner, independent of any
   --  Execute_Task_Ack_Stream on the same channel. Explicit Close is preferred; finalization
   --  attempts non-raising cleanup. Serialize operations and Close on one stream externally.

   function Open_Task_Executing
     (Channel : Scheduling_Channel; Queue_Capacity : Positive) return Task_Executing_Stream;
   --  Register exactly the TaskExecutingRep overload once per Attached or Enabled Scheduling
   --  channel, including failed attempts. Opening never registers ExecuteTaskAck, and opening an
   --  ExecuteTaskAck stream never registers this callback; either order is valid. Registration
   --  runs without bridge locks and may deliver synchronously. No unregister exists.
   --  @param Channel Attached or Enabled Scheduling owner, serialized against its closure.
   --  @param Queue_Capacity Maximum number of reports in the bounded drop-incoming queue.
   --  @return Independent limited metadata consumption owner.
   --  @exception Not_Supported Provider answered NotSupported.
   --  @exception Provider_Error Ineligible/closed owner, second attempt, other refusal, provider
   --  exception or native allocation failure.

   function Receive_Task_Executing
     (Stream : Task_Executing_Stream; Timeout_Milliseconds : Natural := 0)
      return Task_Executing_Report;
   --  Dequeue in successful-enqueue order, convert all ten fields with checked enumerations and
   --  release the native event before returning, including on conversion failure. Reports with
   --  an unknown Task or TaskCompStatus never reach the queue. Queued reports drain after
   --  Scheduling Close; terminal cleanup failure is reported only after that queue drains.
   --  @param Stream Open metadata consumption owner.
   --  @param Timeout_Milliseconds Maximum queue wait in milliseconds; zero polls, not cancellation.
   --  @return Fully Ada-owned report, valid after all owners and the provider close.
   --  @exception Timeout_Error Active queue remained empty until the wait expired.
   --  @exception Stream_Stopped Inactive or stopped queue is empty.
   --  @exception Provider_Error Closed stream, failed provider cleanup, out-of-domain native
   --  enumeration or native failure.

   function Task_Executing_Counters (Stream : Task_Executing_Stream) return Metadata_Counters;
   --  Copy this family's saturating counters without consuming reports. Malformed counts null
   --  payloads, unknown Task/TaskCompStatus values (even while full) and allocation failures.
   --  @param Stream Open consumption owner.
   --  @return Received, queue-full dropped and malformed counters.
   --  @exception Provider_Error Closed stream or native query failure.

   procedure Close (Stream : in out Task_Executing_Stream);
   --  Idempotently deactivate TaskExecutingRep consumption only, without unregistering,
   --  affecting an Execute_Task_Ack_Stream, closing the Scheduling owner or waiting for callback
   --  quiescence. Already returned Ada values remain owned.
   --  @param Stream Metadata owner to close.
   --  @exception Provider_Error Native closure failed.
private
   type Execute_Task_Ack_Stream is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Scheduling_Ack_Metadata_Handle :=
        AMS.MEL_C_API.Null_Scheduling_Ack_Metadata;
   end record;
   overriding
   procedure Finalize (Stream : in out Execute_Task_Ack_Stream);
   type Task_Executing_Stream is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Scheduling_Rep_Metadata_Handle :=
        AMS.MEL_C_API.Null_Scheduling_Rep_Metadata;
   end record;
   overriding
   procedure Finalize (Stream : in out Task_Executing_Stream);
   function To_Report (Raw : AMS.MEL_C_API.IR_Task_Executing_Rep_V1) return Task_Executing_Report;
   --  Checked conversion of every native field; raises Provider_Error for any out-of-domain
   --  Task or TaskCompStatus instead of reinterpreting it.
end AMS.MEL.IR.Scheduling.Metadata;
