private with Ada.Finalization;
private with Ada.Containers.Vectors;
private with AMS.MEL_C_API;
with Interfaces;

package AMS.MEL.RF.C2.Interval_Status is
   --  Observer only. No provider-resource ownership or callback into Ada.
   type Stream is tagged limited private;
   function Open
     (Object                                       : Job;
      Queue_Capacity                               : Positive;
      Max_Event_Log_Entries, Max_Activity_ID_Bytes : Natural) return Stream;
   function Is_Open (Object : Stream) return Boolean;
   procedure Close (Object : in out Stream);
   Stream_Stopped : exception;
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
   type Event_Log_Entry is private;
   function Receive_Event (Object : Stream; Timeout_Milliseconds : Natural) return Status_Event;
   function Interval_ID (Event : Status_Event) return Interfaces.Unsigned_32;
   function Completion (Event : Status_Event) return Completion_Status;
   --  Ascending event-ID key order, not time order. No timestamp normalization.
   function Log_Count (Event : Status_Event) return Natural;
   function Log_At (Event : Status_Event; Index : Positive) return Event_Log_Entry;
   function Event_ID (Entry_Value : Event_Log_Entry) return Interfaces.Unsigned_32;
   function Trigger (Entry_Value : Event_Log_Entry) return Log_Trigger;
   function Time_Seconds (Entry_Value : Event_Log_Entry) return Interfaces.Integer_64;
   function Time_Fractional_Femtoseconds
     (Entry_Value : Event_Log_Entry) return Interfaces.Integer_64;
   --  Arbitrary binary length and bytes, never String/UUID.
   function Activity_ID_Length (Event : Status_Event) return Natural;
   function Activity_ID_Byte (Event : Status_Event; Index : Positive) return Interfaces.Unsigned_8;
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
   function Statistics (Object : Stream) return Counters;
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
