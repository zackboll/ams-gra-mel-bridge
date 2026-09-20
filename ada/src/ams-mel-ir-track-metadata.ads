private with Ada.Finalization;
with Interfaces;
private with AMS.MEL_C_API;

--  Bounded owned polling of the conditionally required IRSTTrackReport
--  metadata callback (@RequiredIfTrack). The provider callback never invokes
--  Ada: it validates and copies a complete report into a bounded native
--  DROP-INCOMING queue that this package polls. Receive returns a wholly
--  Ada-owned value; no native event handle is ever exposed.
--
--  Registration lifetime: upstream has no unregister operation, so
--  registration is one-shot per Track channel and the callback state belongs
--  to the Track channel rather than to Metadata_Channel. Close only
--  deactivates public consumption; provider channel destruction remains the
--  callback-quiescence boundary.
--
--  The @Optional RequestSystemTrackData request is also delivered here. It is
--  an inbound request from the provider, not a send: the published
--  TrackChannel declares it only as a registerMetadataCallback overload and
--  declares no send(RequestSystemTrackData). Both kinds share the one bounded
--  queue, capacity, and counter set, and arrive in strict FIFO order.
--
--  Not implemented here, matching the native facade: TrackDataUpdate,
--  SystemTrackDataResponse, CandidateObjectMessage, and
--  CandidateObjectPreProcMessage.

package AMS.MEL.IR.Track.Metadata is
   type Metadata_Channel is limited private;
   function Open (Channel : Track_Channel; Queue_Capacity : Positive := 16) return Metadata_Channel;
   function Is_Open (Stream : Metadata_Channel) return Boolean;

   --  Complete RequestSystemTrackData. Every value is copied verbatim from the
   --  published upstream getters; upstream defines no enum and no constrained
   --  field, so every value the provider supplies is well formed.
   type Request_System_Track_Data is record
      System_Time_NS : Long_Long_Integer;
      Command_ID     : Interfaces.Unsigned_32;
      Request_ID     : Interfaces.Unsigned_32;
      Track_ID       : Interfaces.Unsigned_32;
   end record;

   --  The implemented Track metadata event kinds. Receive_Event fails closed on
   --  any other native kind.
   type Metadata_Kind is (IRST_Track_Report_Event, Request_System_Track_Data_Event);
   type Metadata_Event (Kind : Metadata_Kind := IRST_Track_Report_Event) is record
      case Kind is
         when IRST_Track_Report_Event =>
            Report : IRST_Track_Report;

         when Request_System_Track_Data_Event =>
            Request : Request_System_Track_Data;
      end case;
   end record;

   --  Receives the next event of either kind. A positive timeout blocks until
   --  an event arrives, the stream stops, or the timeout expires; a zero
   --  timeout is a nonblocking poll. No Ada thread spins: the native adapter
   --  waits on a condition variable.
   --  Timeout_Error is inherited from AMS.MEL.IR.
   function Receive_Event
     (Stream : Metadata_Channel; Timeout_Milliseconds : Natural := 0) return Metadata_Event;

   --  Report-only convenience retained for existing callers. It raises
   --  Provider_Error if the next event is a RequestSystemTrackData, so an
   --  application that registers interest in both kinds should use
   --  Receive_Event. Timeout_Error is inherited from AMS.MEL.IR.
   function Receive
     (Stream : Metadata_Channel; Timeout_Milliseconds : Natural := 0) return IRST_Track_Report;

   type Metadata_Counters is record
      Events_Received, Events_Dropped_Queue_Full, Malformed_Or_Unsupported : Counter;
   end record;
   function Counters (Stream : Metadata_Channel) return Metadata_Counters;
   procedure Close (Stream : in out Metadata_Channel);

private
   type Metadata_Channel is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Track_Metadata_Handle := AMS.MEL_C_API.Null_Track_Metadata;
   end record;
   overriding
   procedure Finalize (Stream : in out Metadata_Channel);
end AMS.MEL.IR.Track.Metadata;
