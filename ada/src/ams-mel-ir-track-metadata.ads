private with Ada.Containers.Vectors;
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
--  The @RequiredIfDetectCandidateObjects CandidateObjectMessage is also
--  delivered here. Upstream declares no send(CandidateObjectMessage) and no
--  RequestFor<CandidateObjectMessage>, so it too is inbound callback metadata
--  rather than an asynchronous request. It is registered only when the channel
--  advertises ChannelMetadataCapabilityType::CandidateObjectMessage; when the
--  capability IS advertised, a refused registration fails Open closed, because
--  the channel promised the type. All three kinds share the one bounded queue,
--  capacity, and counter set and arrive in strict FIFO order.
--
--  The @Optional CandidateObjectPreProcMessage is also delivered here.
--  Upstream declares no send(CandidateObjectPreProcMessage) and no
--  RequestFor<CandidateObjectPreProcMessage>, so it too is inbound callback
--  metadata. Because the callback itself is @Optional, a provider that answers
--  NotSupported simply loses this one kind and Open still succeeds; any other
--  non-Success answer fails Open closed. All four kinds share the one bounded
--  queue, capacity, and counter set and arrive in strict FIFO order.
--
--  Every published TrackChannel-specific metadata callback is now represented.
--  TrackDataUpdate and SystemTrackDataResponse are outbound sends and live in
--  their own packages.

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

   --  Owned Track-facing value types for CandidateObjectMessage. They are
   --  deliberately independent of AMS.MEL.IR.Image so this package adds no
   --  Image dependency; the C ABI still reuses the one canonical native
   --  layout underneath.
   type Row_Column is record
      Row    : Long_Float;
      Column : Long_Float;
   end record;
   type Directional is record
      X, Y, Z : Long_Float;
   end record;
   type Quaternion is record
      X, Y, Z, W : Long_Float;
   end record;
   type Uncertainty is record
      Sensor_Uncertainties   : Interfaces.Unsigned_32;
      Platform_Uncertainties : Interfaces.Unsigned_32;
   end record;

   --  Complete SensorInertialState. No quaternion is normalized and no vector
   --  is renormalized: upstream performs no such validation.
   type Sensor_Inertial_State is record
      System_Time_NS  : Long_Long_Integer;
      Q_XYZW          : Quaternion;
      Q_ECEF_XYZW     : Quaternion;
      Sensor_Position : Directional;
      Sensor_Velocity : Directional;
      Uncertainties   : Uncertainty;
   end record;

   --  Upstream HotRegionTypeEnum. It declares no MaxExclusive value, so any
   --  provider representation outside this range is malformed and is rejected
   --  natively before it can ever reach Ada.
   type Hot_Region_Type is (Invalid, Flare, Solar, Mask);
   for Hot_Region_Type use (Invalid => 0, Flare => 1, Solar => 2, Mask => 3);

   --  Complete HotRegion. Every published getter is represented exactly once
   --  and the geometry keeps its upstream 16-bit width.
   type Hot_Region is record
      Kind   : Hot_Region_Type;
      Size   : Interfaces.Unsigned_16;
      Top    : Interfaces.Unsigned_16;
      Left   : Interfaces.Unsigned_16;
      Right  : Interfaces.Unsigned_16;
      Bottom : Interfaces.Unsigned_16;
   end record;

   --  Complete CandidateObjectHeader. CFAR is upstream binary32 and is kept as
   --  Float; it is deliberately not widened. The validity bitfield is carried
   --  verbatim and is deliberately not decoded.
   type Candidate_Object_Header is record
      Number_Of_COs          : Interfaces.Unsigned_16;
      Stack_Frame_Index      : Interfaces.Unsigned_16;
      CFAR                   : Float;
      Validity_Flag_Bitfield : Interfaces.Unsigned_16;
      TOV_UTC_NS             : Long_Long_Integer;
   end record;

   --  Complete CandidateObject. No floating-point value is clamped or
   --  normalized.
   type Candidate_Object is record
      System_Time_NS               : Long_Long_Integer;
      Detection_Category           : Interfaces.Unsigned_32;
      Sensor_Index                 : Interfaces.Unsigned_32;
      Subpixel                     : Row_Column;
      Intensity                    : Long_Float;
      Sensor_Relative_Unit         : Directional;
      Signal_To_Interference_Ratio : Long_Float;
      Signal_To_Noise_Ratio        : Long_Float;
   end record;

   --  Wholly Ada-owned CandidateObjectMessage. The variable-size hot-region
   --  and candidate collections are copied out of the native event before that
   --  event is closed, so no native pointer, native span, or provider storage
   --  is ever exposed.
   --
   --  Exactly numberOfCOs candidate objects are present: the upstream fixed
   --  900-entry array is exposed only through its meaningful prefix.
   type Candidate_Object_Message is private;
   function Header (Message : Candidate_Object_Message) return Candidate_Object_Header;
   function Inertial_State (Message : Candidate_Object_Message) return Sensor_Inertial_State;
   function Hot_Region_Count (Message : Candidate_Object_Message) return Natural;
   function Hot_Region_At (Message : Candidate_Object_Message; Index : Positive) return Hot_Region;
   function Candidate_Object_Count (Message : Candidate_Object_Message) return Natural;
   function Candidate_Object_At
     (Message : Candidate_Object_Message; Index : Positive) return Candidate_Object;

   --  Upstream candidateObjectWithBackground is exactly a 3 by 3 patch of
   --  int16_t intensities around the candidate object. This is an owned Ada
   --  2-D value; the flat row-major C array is mapped to it explicitly.
   subtype Background_Index is Positive range 1 .. 3;
   type Candidate_Background is array (Background_Index, Background_Index) of Interfaces.Integer_16;

   --  Complete CandidateObjectPreProc. Every published getter is represented
   --  exactly once. No floating-point value is clamped or normalized:
   --  Candidate_Object_Quality is documented upstream as "0 to 1" but the
   --  published setter enforces nothing, and the sensor-relative unit vector
   --  is not renormalized. Detection_Category is an upstream bitfield and is
   --  deliberately not decoded. Each entry carries its OWN nested inertial
   --  state, distinct from the message-level one.
   type Candidate_Object_PreProc is record
      System_Time_NS                   : Long_Long_Integer;
      Detection_Category               : Interfaces.Unsigned_32;
      Sensor_Index                     : Interfaces.Unsigned_32;
      Subpixel                         : Row_Column;
      Intensity                        : Long_Float;
      Sensor_Relative_Unit             : Directional;
      Signal_To_Interference_Ratio     : Long_Float;
      Signal_To_Noise_Ratio            : Long_Float;
      Candidate_Object_With_Background : Candidate_Background;
      Clutter                          : Long_Float;
      Candidate_Object_Quality         : Long_Float;
      Sir_Delta                        : Long_Float;
      Inertial_State                   : Sensor_Inertial_State;
      Edge                             : Boolean;
      Az_Sigma                         : Long_Float;
      El_Sigma                         : Long_Float;
      Background_Normalizer            : Long_Float;
   end record;

   --  Wholly Ada-owned CandidateObjectPreProcMessage. The variable-size
   --  hot-region and PreProc collections are copied out of the native event
   --  before that event is closed, so no native pointer, native span, or
   --  provider storage is ever exposed.
   --
   --  Unlike Candidate_Object_Message, the upstream container here is a
   --  std::vector that already carries its own size, and the pinned headers
   --  publish NO invariant requiring Number_Of_COs to equal that size. Both
   --  values are therefore preserved verbatim: Header reports the provider's
   --  Number_Of_COs and Candidate_Object_PreProc_Count reports the actual
   --  vector length. A mismatch is not an error here.
   type Candidate_Object_PreProc_Message is private;
   function Header (Message : Candidate_Object_PreProc_Message) return Candidate_Object_Header;
   function Inertial_State
     (Message : Candidate_Object_PreProc_Message) return Sensor_Inertial_State;
   function Hot_Region_Count (Message : Candidate_Object_PreProc_Message) return Natural;
   function Hot_Region_At
     (Message : Candidate_Object_PreProc_Message; Index : Positive) return Hot_Region;
   function Candidate_Object_PreProc_Count
     (Message : Candidate_Object_PreProc_Message) return Natural;
   function Candidate_Object_PreProc_At
     (Message : Candidate_Object_PreProc_Message; Index : Positive) return Candidate_Object_PreProc;

   --  The implemented Track metadata event kinds. Receive_Event fails closed on
   --  any other native kind.
   type Metadata_Kind is
     (IRST_Track_Report_Event,
      Request_System_Track_Data_Event,
      Candidate_Object_Message_Event,
      Candidate_Object_PreProc_Message_Event);
   type Metadata_Event (Kind : Metadata_Kind := IRST_Track_Report_Event) is record
      case Kind is
         when IRST_Track_Report_Event =>
            Report : IRST_Track_Report;

         when Request_System_Track_Data_Event =>
            Request : Request_System_Track_Data;

         when Candidate_Object_Message_Event =>
            Candidates : Candidate_Object_Message;

         when Candidate_Object_PreProc_Message_Event =>
            Candidate_PreProcs : Candidate_Object_PreProc_Message;
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
   --  Provider_Error if the next event is any non-report Track metadata kind,
   --  so an application that can receive more than one kind should use
   --  Receive_Event. Timeout_Error is inherited from AMS.MEL.IR.
   function Receive
     (Stream : Metadata_Channel; Timeout_Milliseconds : Natural := 0) return IRST_Track_Report;

   type Metadata_Counters is record
      Events_Received, Events_Dropped_Queue_Full, Malformed_Or_Unsupported : Counter;
   end record;
   function Counters (Stream : Metadata_Channel) return Metadata_Counters;
   procedure Close (Stream : in out Metadata_Channel);

private
   package Hot_Region_Vectors is new Ada.Containers.Vectors (Positive, Hot_Region);
   package Candidate_Object_Vectors is new Ada.Containers.Vectors (Positive, Candidate_Object);

   --  Wholly Ada-owned storage. Both vectors are filled from the native event
   --  before that event is closed.
   type Candidate_Object_Message is record
      Header_Value      : Candidate_Object_Header;
      Inertial          : Sensor_Inertial_State;
      Regions           : Hot_Region_Vectors.Vector;
      Candidate_Objects : Candidate_Object_Vectors.Vector;
   end record;

   package Candidate_Object_PreProc_Vectors is new
     Ada.Containers.Vectors (Positive, Candidate_Object_PreProc);

   --  Wholly Ada-owned storage. Both vectors are filled from the native event
   --  before that event is closed. Header_Value.Number_Of_COs is the
   --  provider's verbatim count and is independent of Candidate_PreProcs
   --  length.
   type Candidate_Object_PreProc_Message is record
      Header_Value       : Candidate_Object_Header;
      Inertial           : Sensor_Inertial_State;
      Regions            : Hot_Region_Vectors.Vector;
      Candidate_PreProcs : Candidate_Object_PreProc_Vectors.Vector;
   end record;

   type Metadata_Channel is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Track_Metadata_Handle := AMS.MEL_C_API.Null_Track_Metadata;
   end record;
   overriding
   procedure Finalize (Stream : in out Metadata_Channel);
end AMS.MEL.IR.Track.Metadata;
