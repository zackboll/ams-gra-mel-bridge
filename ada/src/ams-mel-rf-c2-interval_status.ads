private with Ada.Finalization;
private with Ada.Containers.Vectors;
private with AMS.MEL_C_API;
with Interfaces;

package AMS.MEL.RF.C2.Interval_Status is
   --  Observe provider JobIntervalStatus callbacks without owning a JobDetail
   --  or any provider-resource graph. The callback receives status by value;
   --  its exact callable holder, bridge state and DSO pin remain for process
   --  lifetime because upstream provides no unregister or quiescence contract.
   --  Registration success is not evidence of callback delivery or scheduling.
   --  Serialize operations on an owner with that owner's destruction.

   --  Observer only. No provider-resource ownership or callback into Ada.
   type Stream is tagged limited private;
   --  Limited observer with explicit Close and non-raising controlled cleanup.
   --  Neither cleanup path calls provider unregister or waits for quiescence.
   function Open
     (Object                                       : Job;
      Queue_Capacity                               : Positive;
      Max_Event_Log_Entries, Max_Activity_ID_Bytes : Natural) return Stream;
   --  Attempt registration once on the Job, before Finalize or full Cancel.
   --  A synchronous callback during registration is supported. An exposed
   --  registration attempt is consumed even if the provider throws.
   --  The fixed FIFO drops incoming observations when full; malformed enums,
   --  oversize payloads and allocation failures discard whole observations.
   --  Limits bound queued storage, not provider argument construction,
   --  concurrent callback temporaries or application-held Status Event copies.
   --  Native option/lifecycle/registration failures raise Provider Error.
   --  @param Object Live Job whose status is observed, not retained by Stream.
   --  @param Queue_Capacity Maximum number of queued observations; positive.
   --  @param Max_Event_Log_Entries Maximum entries per observation; zero allows
   --    only an empty log.
   --  @param Max_Activity_ID_Bytes Maximum activity bytes per observation; zero
   --    allows only an empty activity ID.
   --  @return Open observer; no delivery or physical RF activity is promised.
   function Is_Open (Object : Stream) return Boolean;
   --  Test local handle ownership, not provider liveness or pending callbacks.
   --  @param Object Observer to inspect.
   --  @return True while the local native handle exists, even if stopped.
   procedure Close (Object : in out Stream);
   --  Stop locally, discard the queue and release the handle; idempotent.
   --  Job Close also stops reception. Neither operation cancels the Job.
   --  @param Object Observer to close; must not race its destruction or use.
   Stream_Stopped : exception;
   --  Receive observed stopped reception, distinct from Timeout Error and from
   --  a provider-reported failure contained in a Completion Status value.
   type Completion_Status is
     (None,
      Ready_For_Next_Job_Interval,
      Failed_Interrupted,
      Failed_Invalid_Tx_Event_Spatial_Data,
      Failed_Invalid_Tx_Event_Signal_Data,
      Failed_Invalid_Tx_Event_Temporal_Data,
      Failed_Invalid_Tx_Event_Identifier_Data,
      Failed_Invalid_Rx_Event_Spatial_Data,
      Failed_Invalid_Rx_Event_Signal_Data,
      Failed_Invalid_Rx_Event_Temporal_Data,
      Failed_Invalid_Rx_Event_Identifier_Data,
      Failed_Invalid_Sequence_Temporal_Data,
      Failed_Invalid_Job_Interval_Spatial_Data,
      Failed_Invalid_Job_Interval_Event_Signal_Data,
      Failed_Invalid_Job_Interval_Event_Temporal_Data,
      Failed_Invalid_Job_Interval_Event_Identifier_Data,
      Failed_Invalid_Job_Temporal_Data,
      Failed_Invalid_Job_Identifier_Data,
      Completed,
      Cancelled,
      Late_Controls,
      Invalid_Controls,
      Antenna_Fov_Error,
      Transmit_Rf_Inhibited,
      Started);
   --  Provider-reported interval state, not independently verified RF behavior.
   --  Known failure states are normal data, not Provider Error exceptions.
   --  @enum None No completion state reported.
   --  @enum Ready_For_Next_Job_Interval Provider reports readiness for the next interval.
   --  @enum Failed_Interrupted Provider reports interruption failure.
   --  @enum Failed_Invalid_Tx_Event_Spatial_Data Invalid transmit-event spatial data reported.
   --  @enum Failed_Invalid_Tx_Event_Signal_Data Invalid transmit-event signal data reported.
   --  @enum Failed_Invalid_Tx_Event_Temporal_Data Invalid transmit-event temporal data reported.
   --  @enum Failed_Invalid_Tx_Event_Identifier_Data Invalid transmit-event identifier data reported.
   --  @enum Failed_Invalid_Rx_Event_Spatial_Data Invalid receive-event spatial data reported.
   --  @enum Failed_Invalid_Rx_Event_Signal_Data Invalid receive-event signal data reported.
   --  @enum Failed_Invalid_Rx_Event_Temporal_Data Invalid receive-event temporal data reported.
   --  @enum Failed_Invalid_Rx_Event_Identifier_Data Invalid receive-event identifier data reported.
   --  @enum Failed_Invalid_Sequence_Temporal_Data Invalid sequence temporal data reported.
   --  @enum Failed_Invalid_Job_Interval_Spatial_Data Invalid interval spatial data reported.
   --  @enum Failed_Invalid_Job_Interval_Event_Signal_Data Invalid interval event signal data reported.
   --  @enum Failed_Invalid_Job_Interval_Event_Temporal_Data Invalid interval event temporal data reported.
   --  @enum Failed_Invalid_Job_Interval_Event_Identifier_Data Invalid interval event identifier data reported.
   --  @enum Failed_Invalid_Job_Temporal_Data Invalid Job temporal data reported.
   --  @enum Failed_Invalid_Job_Identifier_Data Invalid Job identifier data reported.
   --  @enum Completed Provider reports interval completion; not bridge proof of emission.
   --  @enum Cancelled Provider reports cancellation, not a timeout result.
   --  @enum Late_Controls Provider reports late controls.
   --  @enum Invalid_Controls Provider reports invalid controls.
   --  @enum Antenna_Fov_Error Provider reports an antenna field-of-view error.
   --  @enum Transmit_Rf_Inhibited Provider reports RF transmission inhibited.
   --  @enum Started Provider reports interval start, not bridge scheduling confirmation.
   for Completion_Status use
     (None                                              => 0,
      Ready_For_Next_Job_Interval                       => 1,
      Failed_Interrupted                                => 2,
      Failed_Invalid_Tx_Event_Spatial_Data              => 3,
      Failed_Invalid_Tx_Event_Signal_Data               => 4,
      Failed_Invalid_Tx_Event_Temporal_Data             => 5,
      Failed_Invalid_Tx_Event_Identifier_Data           => 6,
      Failed_Invalid_Rx_Event_Spatial_Data              => 7,
      Failed_Invalid_Rx_Event_Signal_Data               => 8,
      Failed_Invalid_Rx_Event_Temporal_Data             => 9,
      Failed_Invalid_Rx_Event_Identifier_Data           => 10,
      Failed_Invalid_Sequence_Temporal_Data             => 11,
      Failed_Invalid_Job_Interval_Spatial_Data          => 12,
      Failed_Invalid_Job_Interval_Event_Signal_Data     => 13,
      Failed_Invalid_Job_Interval_Event_Temporal_Data   => 14,
      Failed_Invalid_Job_Interval_Event_Identifier_Data => 15,
      Failed_Invalid_Job_Temporal_Data                  => 16,
      Failed_Invalid_Job_Identifier_Data                => 17,
      Completed                                         => 18,
      Cancelled                                         => 19,
      Late_Controls                                     => 20,
      Invalid_Controls                                  => 21,
      Antenna_Fov_Error                                 => 22,
      Transmit_Rf_Inhibited                             => 23,
      Started                                           => 24);
   type Log_Trigger is
     (None,
      Event_Extended,
      Event_Triggered,
      Event_Resumed,
      Event_Cancelled,
      Event_Inhibited,
      Event_Delayed_Start,
      Event_Type_Not_Supported);
   --  Provider reason for a conditional event log entry.
   --  @enum None No trigger reason reported.
   --  @enum Event_Extended Provider reports event extension.
   --  @enum Event_Triggered Provider reports event triggering.
   --  @enum Event_Resumed Provider reports event resumption.
   --  @enum Event_Cancelled Provider reports event cancellation.
   --  @enum Event_Inhibited Provider reports event inhibition.
   --  @enum Event_Delayed_Start Provider reports delayed event start.
   --  @enum Event_Type_Not_Supported Provider reports unsupported event type.
   for Log_Trigger use
     (None                     => 0,
      Event_Extended           => 1,
      Event_Triggered          => 2,
      Event_Resumed            => 3,
      Event_Cancelled          => 4,
      Event_Inhibited          => 5,
      Event_Delayed_Start      => 6,
      Event_Type_Not_Supported => 7);
   --  Ordinary Ada-owned value, independent of all native owners.
   type Status_Event is private;
   --  Ada-owned status, log and arbitrary binary activity bytes. Copies survive
   --  Stream and Job closure and retain no native/provider owner.
   type Event_Log_Entry is private;
   --  Copied event-ID map entry with trigger and separate signed time fields.
   function Receive_Event (Object : Stream; Timeout_Milliseconds : Natural) return Status_Event;
   --  Consume the next FIFO observation and copy it into Ada storage.
   --  Stop takes priority over queued data. Timeout Error means no observation
   --  arrived within the wait, not cancellation; Stream Stopped means stopped.
   --  Native failures or invalid native views raise Provider Error. Unknown
   --  callback enums are dropped natively, not returned as partial events.
   --  @param Object Open local observer; may already be logically stopped.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   --  @return Independent copied observation, including normal failure statuses.
   function Interval_ID (Event : Status_Event) return Interfaces.Unsigned_32;
   --  Read the provider interval identity without a live query.
   --  @param Event Copied observation.
   --  @return All 32 bits of the reported interval ID.
   function Completion (Event : Status_Event) return Completion_Status;
   --  Read the copied provider state without interpreting hardware behavior.
   --  @param Event Copied observation.
   --  @return Reported completion status, including normal failure data.

   --  Ascending event-ID key order, not time order. No timestamp normalization.
   function Log_Count (Event : Status_Event) return Natural;
   --  Count copied entries, including a successful empty log.
   --  @param Event Copied observation.
   --  @return Number of log entries.
   function Log_At (Event : Status_Event; Index : Positive) return Event_Log_Entry;
   --  Read ascending event-ID map-key order, not chronological order.
   --  @param Event Copied observation.
   --  @param Index One-based position; out of range raises Constraint Error.
   --  @return Independent copied log entry.
   function Event_ID (Entry_Value : Event_Log_Entry) return Interfaces.Unsigned_32;
   --  Read the original event map key.
   --  @param Entry_Value Copied log entry.
   --  @return Provider event ID with all 32 bits preserved.
   function Trigger (Entry_Value : Event_Log_Entry) return Log_Trigger;
   --  Read the reported conditional-event reason.
   --  @param Entry_Value Copied log entry.
   --  @return Provider log trigger, not independent execution confirmation.
   function Time_Seconds (Entry_Value : Event_Log_Entry) return Interfaces.Integer_64;
   --  Read integral UTC seconds separately from the fractional field.
   --  @param Entry_Value Copied log entry.
   --  @return Signed native seconds count without conversion or normalization.
   function Time_Fractional_Femtoseconds
     (Entry_Value : Event_Log_Entry) return Interfaces.Integer_64;
   --  Read the native fractional UTC field without recombining the timestamp.
   --  @param Entry_Value Copied log entry.
   --  @return Signed femtosecond count, not normalized or converted to Duration.

   --  Arbitrary binary length and bytes, never String/UUID.
   function Activity_ID_Length (Event : Status_Event) return Natural;
   --  Measure arbitrary binary activity data, not String, UTF-8 or UUID text.
   --  @param Event Copied observation.
   --  @return Byte length; zero is valid.
   function Activity_ID_Byte (Event : Status_Event; Index : Positive) return Interfaces.Unsigned_8;
   --  Read one activity byte, including NUL and high-bit values unchanged.
   --  @param Event Copied observation.
   --  @param Index One-based byte position; out of range raises Constraint Error.
   --  @return Original unsigned byte.
   type Counters is record
      Callback_Entries,
      Events_Queued,
      Events_Delivered,
      Queue_Full_Drops,
      Malformed_Drops,
      Oversize_Drops,
      Allocation_Failures,
      Callbacks_After_Close : Interfaces.Unsigned_64;
   end record;
   --  Consistent observer counters, each saturating at Unsigned 64'Last.
   --  Stop takes priority over a concurrent callback's decode/drop outcome.
   --  @field Callback_Entries All entries into the bridge callback body.
   --  @field Events_Queued Observations successfully published to the FIFO.
   --  @field Events_Delivered Observations removed by successful native receives.
   --  @field Queue_Full_Drops Incoming valid observations discarded at full capacity.
   --  @field Malformed_Drops Observations rejected for unknown completion or trigger enums.
   --  @field Oversize_Drops Observations exceeding configured log or activity limits.
   --  @field Allocation_Failures Observations discarded after payload-copy allocation failure.
   --  @field Callbacks_After_Close Callbacks discarded because reception was stopped.
   function Statistics (Object : Stream) return Counters;
   --  Take a nonconsuming native counter snapshot; no provider query.
   --  @param Object Observer with an open local handle, possibly stopped.
   --  @return Consistent saturating counters, not an exact total beyond saturation.
private
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (None) /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_None),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Ready_For_Next_Job_Interval)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Ready_For_Next_Job_Interval),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Failed_Interrupted)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Failed_Interrupted),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Failed_Invalid_Tx_Event_Spatial_Data)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Failed_Invalid_Tx_Event_Spatial_Data),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Failed_Invalid_Tx_Event_Signal_Data)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Failed_Invalid_Tx_Event_Signal_Data),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Failed_Invalid_Tx_Event_Temporal_Data)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Failed_Invalid_Tx_Event_Temporal_Data),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Failed_Invalid_Tx_Event_Identifier_Data)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Failed_Invalid_Tx_Event_Identifier_Data),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Failed_Invalid_Rx_Event_Spatial_Data)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Failed_Invalid_Rx_Event_Spatial_Data),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Failed_Invalid_Rx_Event_Signal_Data)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Failed_Invalid_Rx_Event_Signal_Data),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Failed_Invalid_Rx_Event_Temporal_Data)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Failed_Invalid_Rx_Event_Temporal_Data),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Failed_Invalid_Rx_Event_Identifier_Data)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Failed_Invalid_Rx_Event_Identifier_Data),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Failed_Invalid_Sequence_Temporal_Data)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Failed_Invalid_Sequence_Temporal_Data),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Failed_Invalid_Job_Interval_Spatial_Data)
          /= Integer
               (AMS.MEL_C_API.Rf_Interval_Completion_Failed_Invalid_Job_Interval_Spatial_Data),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Failed_Invalid_Job_Interval_Event_Signal_Data)
          /= Integer
               (AMS.MEL_C_API.Rf_Interval_Completion_Failed_Invalid_Job_Interval_Event_Signal_Data),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Failed_Invalid_Job_Interval_Event_Temporal_Data)
          /= Integer
               (AMS
                  .MEL_C_API
                  .Rf_Interval_Completion_Failed_Invalid_Job_Interval_Event_Temporal_Data),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Failed_Invalid_Job_Interval_Event_Identifier_Data)
          /= Integer
               (AMS
                  .MEL_C_API
                  .Rf_Interval_Completion_Failed_Invalid_Job_Interval_Event_Identifier_Data),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Failed_Invalid_Job_Temporal_Data)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Failed_Invalid_Job_Temporal_Data),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Failed_Invalid_Job_Identifier_Data)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Failed_Invalid_Job_Identifier_Data),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Completed)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Completed),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Cancelled)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Cancelled),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Late_Controls)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Late_Controls),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Invalid_Controls)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Invalid_Controls),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Antenna_Fov_Error)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Antenna_Fov_Error),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Transmit_Rf_Inhibited)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Transmit_Rf_Inhibited),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Completion_Status'Enum_Rep (Started)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Completion_Started),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Log_Trigger'Enum_Rep (None) /= Integer (AMS.MEL_C_API.Rf_Log_Trigger_None),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Log_Trigger'Enum_Rep (Event_Extended)
          /= Integer (AMS.MEL_C_API.Rf_Log_Trigger_Event_Extended),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Log_Trigger'Enum_Rep (Event_Triggered)
          /= Integer (AMS.MEL_C_API.Rf_Log_Trigger_Event_Triggered),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Log_Trigger'Enum_Rep (Event_Resumed)
          /= Integer (AMS.MEL_C_API.Rf_Log_Trigger_Event_Resumed),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Log_Trigger'Enum_Rep (Event_Cancelled)
          /= Integer (AMS.MEL_C_API.Rf_Log_Trigger_Event_Cancelled),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Log_Trigger'Enum_Rep (Event_Inhibited)
          /= Integer (AMS.MEL_C_API.Rf_Log_Trigger_Event_Inhibited),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Log_Trigger'Enum_Rep (Event_Delayed_Start)
          /= Integer (AMS.MEL_C_API.Rf_Log_Trigger_Event_Delayed_Start),
        "RF status enum representation mismatch");
   pragma
     Compile_Time_Error
       (Log_Trigger'Enum_Rep (Event_Type_Not_Supported)
          /= Integer (AMS.MEL_C_API.Rf_Log_Trigger_Event_Type_Not_Supported),
        "RF status enum representation mismatch");
   type Stream is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_Interval_Status_Handle :=
        AMS.MEL_C_API.Null_RF_Interval_Status;
   end record;
   overriding
   procedure Finalize (Object : in out Stream);
   type Event_Log_Entry is record
      ID                : Interfaces.Unsigned_32;
      Reason            : Log_Trigger;
      Seconds, Fraction : Interfaces.Integer_64;
   end record;
   package Log_Vectors is new Ada.Containers.Vectors (Positive, Event_Log_Entry);
   package Byte_Vectors is new
     Ada.Containers.Vectors (Positive, Interfaces.Unsigned_8, Interfaces."=");
   type Status_Event is record
      ID       : Interfaces.Unsigned_32 := 0;
      State    : Completion_Status := None;
      Logs     : Log_Vectors.Vector;
      Activity : Byte_Vectors.Vector;
   end record;
end AMS.MEL.RF.C2.Interval_Status;
