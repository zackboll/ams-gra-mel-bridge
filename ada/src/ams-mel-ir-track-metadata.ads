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
   --  Four inbound callback families, not outbound RequestFor sends. Required
   --  Track reports, advertised candidate messages and two optional callbacks
   --  share one bounded drop-incoming FIFO ordered by successful enqueue, not
   --  provider timestamps. Native callbacks never invoke Ada. Positive behavior
   --  is mock-qualified; pinned Squall cannot attach this Track channel.

   type Metadata_Channel is limited private;
   --  Limited controlled owner of queue consumption, not the Track/Session or
   --  provider graph. Callback state survives public closure as required;
   --  provider-channel destruction remains the quiescence boundary.
   function Open (Channel : Track_Channel; Queue_Capacity : Positive := 16) return Metadata_Channel;
   --  Register once while Attached or Enabled, without the Track lifecycle lock
   --  held across provider registration. Order: required Track report; candidate
   --  message only if advertised at Track Open; optional system-data request;
   --  optional candidate PreProc. Required report refusal and advertised candidate
   --  refusal fail, including NotSupported. Optional callbacks accept NotSupported
   --  non-fatally but other non-Success answers fail. Partial failure consumes
   --  the attempt and retains inactive/failed callback state without unregister;
   --  no public consumer escapes. Reopening after Close does not retry registration.
   --  @param Channel Open Attached/Enabled Track owner; serialize owner operations.
   --  @param Queue_Capacity Positive shared queue bound, default sixteen events.
   --  @return Owned queue consumer; registration failure raises a provider error.
   function Is_Open (Stream : Metadata_Channel) return Boolean;
   --  Test local handle presence, not channel existence or callback/queue health.
   --  @param Stream Metadata consumer to inspect.
   --  @return True when its public native handle is present, even if stopped/failed.

   --  Complete RequestSystemTrackData. Every value is copied verbatim from the
   --  published upstream getters; upstream defines no enum and no constrained
   --  field, so every value the provider supplies is well formed.
   type Request_System_Track_Data is record
      System_Time_NS : Long_Long_Integer;
      Command_ID     : Interfaces.Unsigned_32;
      Request_ID     : Interfaces.Unsigned_32;
      Track_ID       : Interfaces.Unsigned_32;
   end record;
   --  Owned inbound request for system Track data, not an outbound submission
   --  or an asynchronous request owner. No response is sent automatically.
   --  @field System_Time_NS Signed system-time nanosecond count; epoch unspecified.
   --  @field Command_ID Provider command correlation identifier, all 32 bits preserved.
   --  @field Request_ID Provider system-data request identifier, all 32 bits preserved.
   --  @field Track_ID Provider numeric track identifier, not a UCI identity.

   --  Owned Track-facing value types for CandidateObjectMessage. They are
   --  deliberately independent of AMS.MEL.IR.Image so this package adds no
   --  Image dependency; the C ABI still reuses the one canonical native
   --  layout underneath.
   type Row_Column is record
      Row    : Long_Float;
      Column : Long_Float;
   end record;
   --  Subpixel coordinates copied from upstream RowCol, whose x is column and
   --  y is row. No pixel-origin convention or rounding is inferred.
   --  @field Row Subpixel row coordinate, preserving the provider value.
   --  @field Column Subpixel column coordinate, preserving the provider value.
   type Directional is record
      X, Y, Z : Long_Float;
   end record;
   --  Three copied directional components; units and frame belong to the field
   --  using this type, not to the type itself. No normalization is performed.
   --  @field X First directional component.
   --  @field Y Second directional component.
   --  @field Z Third directional component.
   type Quaternion is record
      X, Y, Z, W : Long_Float;
   end record;
   --  Copied quaternion components in upstream XYZW order, without normalization
   --  or an inferred rotation convention beyond the containing field's meaning.
   --  @field X Quaternion x component.
   --  @field Y Quaternion y component.
   --  @field Z Quaternion z component.
   --  @field W Quaternion w component, retained separately from xyz.
   type Uncertainty is record
      Sensor_Uncertainties   : Interfaces.Unsigned_32;
      Platform_Uncertainties : Interfaces.Unsigned_32;
   end record;
   --  Opaque upstream navigation-uncertainty values, copied without decoding
   --  bit meanings or assigning statistical confidence units.
   --  @field Sensor_Uncertainties Unsigned sensor uncertainty representation, all bits preserved.
   --  @field Platform_Uncertainties Unsigned platform uncertainty representation, all bits preserved.

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
   --  Owned sensor navigation snapshot. Quaternions and vectors are copied, not
   --  normalized, transformed or estimated by this bridge.
   --  @field System_Time_NS Signed system-time nanosecond count; epoch unspecified.
   --  @field Q_XYZW Quaternion from sensor to local north/east/down frame, XYZW order.
   --  @field Q_ECEF_XYZW Quaternion from local north/east/down to ECEF, XYZW order.
   --  @field Sensor_Position Sensor position in ECEF, meters.
   --  @field Sensor_Velocity Sensor velocity in ECEF, meters per second.
   --  @field Uncertainties Opaque sensor/platform navigation uncertainty values.

   --  Upstream HotRegionTypeEnum. It declares no MaxExclusive value, so any
   --  provider representation outside this range is malformed and is rejected
   --  natively before it can ever reach Ada.
   type Hot_Region_Type is (Invalid, Flare, Solar, Mask);
   --  Pinned optional hot-region categories; values outside 0..3 are malformed
   --  natively, not silently interpreted as a known category.
   --  @enum Invalid Invalid/default hot-region category, value 0.
   --  @enum Flare Flare category, value 1, without an inferred detection policy.
   --  @enum Solar Solar category, value 2.
   --  @enum Mask Mask category, value 3.
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
   --  Copied geometry for an extended high-intensity object. Upstream defines
   --  boundary positions as zero-based but does not establish inclusive/exclusive
   --  edge arithmetic here; no geometry consistency check is performed.
   --  @field Kind Provider hot-region category.
   --  @field Size Number of pixels making up the region, unsigned 16-bit.
   --  @field Top Top pixel position, zero-based unsigned 16-bit.
   --  @field Left Left pixel position, zero-based unsigned 16-bit.
   --  @field Right Right pixel position, zero-based unsigned 16-bit.
   --  @field Bottom Bottom pixel position, zero-based unsigned 16-bit.

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
   --  Owned fixed header fields; variable hot regions are accessed through the
   --  message. Count meaning differs between fixed-array and PreProc messages.
   --  @field Number_Of_COs Declared unsigned 16-bit candidate count; fixed-array messages use this prefix, PreProc vectors have an independent length.
   --  @field Stack_Frame_Index Zero-based current frame index within a stack sequence.
   --  @field CFAR Upstream binary32 average per-pixel false alarm rate, FA/pixel; retained as Float without widening or algorithm inference.
   --  @field Validity_Flag_Bitfield Opaque unsigned 16-bit validity for stack index/CFAR; bit definitions are not invented.
   --  @field TOV_UTC_NS Signed UTC-named time-of-validity nanosecond count; no more specific epoch is established.

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
   --  Complete owned candidate detection, without numeric normalization.
   --  @field System_Time_NS Time of validity in signed nanoseconds, without an established epoch.
   --  @field Detection_Category Bitfield identifying waveband temporal depth; copied without invented bit definitions.
   --  @field Sensor_Index Unsigned index indicating sensor position, not an Ada vector index.
   --  @field Subpixel Subpixel column/row location, with no inferred pixel origin.
   --  @field Intensity Merged candidate intensity; units/calibration unspecified.
   --  @field Sensor_Relative_Unit Detection line-of-sight unit-vector report in sensor-relative xyz; not renormalized.
   --  @field Signal_To_Interference_Ratio SIR scalar; no linear-versus-dB convention established.
   --  @field Signal_To_Noise_Ratio SNR scalar; no scale conversion or normalization performed.

   --  Wholly Ada-owned CandidateObjectMessage. The variable-size hot-region
   --  and candidate collections are copied out of the native event before that
   --  event is closed, so no native pointer, native span, or provider storage
   --  is ever exposed.
   --
   --  Exactly numberOfCOs candidate objects are present: the upstream fixed
   --  900-entry array is exposed only through its meaningful prefix.
   type Candidate_Object_Message is private;
   --  Owned header, navigation, hot regions and candidate prefix. Upstream fixed
   --  capacity is 900; declared count greater than capacity is malformed. Only
   --  the meaningful prefix is copied, in native order, retaining duplicates.
   function Header (Message : Candidate_Object_Message) return Candidate_Object_Header;
   --  Copy the fixed candidate-message header.
   --  @param Message Owned fixed-array candidate message to inspect.
   --  @return Owned header whose count matches the copied meaningful prefix.
   function Inertial_State (Message : Candidate_Object_Message) return Sensor_Inertial_State;
   --  Copy the message-level navigation snapshot, not a borrowed provider object.
   --  @param Message Owned fixed-array candidate message to inspect.
   --  @return Independent sensor inertial state.
   function Hot_Region_Count (Message : Candidate_Object_Message) return Natural;
   --  Count the complete copied hot-region vector in provider order.
   --  @param Message Owned fixed-array candidate message to inspect.
   --  @return Number of hot regions, independent of candidate count.
   function Hot_Region_At (Message : Candidate_Object_Message; Index : Positive) return Hot_Region;
   --  Copy one region; index beyond length raises a constraint error.
   --  @param Message Owned fixed-array candidate message to inspect.
   --  @param Index One-based copied-region position, not a pixel coordinate.
   --  @return Independent hot-region value.
   function Candidate_Object_Count (Message : Candidate_Object_Message) return Natural;
   --  Count the copied meaningful prefix, not the fixed capacity of 900.
   --  @param Message Owned fixed-array candidate message to inspect.
   --  @return Actual copied candidate count established by the header.
   function Candidate_Object_At
     (Message : Candidate_Object_Message; Index : Positive) return Candidate_Object;
   --  Copy one candidate; index beyond copied prefix raises a constraint error.
   --  @param Message Owned fixed-array candidate message to inspect.
   --  @param Index One-based candidate position, not the upstream sensor index.
   --  @return Independent complete candidate detection.

   --  Upstream candidateObjectWithBackground is exactly a 3 by 3 patch of
   --  int16_t intensities around the candidate object. This is an owned Ada
   --  2-D value; the flat row-major C array is mapped to it explicitly.
   subtype Background_Index is Positive range 1 .. 3;
   --  One-based row/column index into the exact three-by-three background patch,
   --  not a provider image coordinate.
   type Candidate_Background is array (Background_Index, Background_Index) of Interfaces.Integer_16;
   --  Owned 3x3 signed 16-bit intensity patch around a candidate. Native row-major
   --  nine-sample storage is mapped explicitly to Ada row/column indices; signed
   --  values survive without unsigned conversion or intensity rescaling.

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
   --  Complete owned preprocessed detection. No statistical estimation,
   --  confidence calibration, vector normalization or score clamping is done.
   --  @field System_Time_NS Time of validity in signed nanoseconds; epoch unspecified.
   --  @field Detection_Category Opaque waveband temporal-depth bitfield, all 32 bits preserved.
   --  @field Sensor_Index Unsigned sensor-position index, not an Ada vector position.
   --  @field Subpixel Subpixel column/row location with no invented pixel origin.
   --  @field Intensity Merged candidate intensity with unspecified physical units.
   --  @field Sensor_Relative_Unit Sensor-relative line-of-sight xyz unit-vector report, not renormalized.
   --  @field Signal_To_Interference_Ratio SIR scalar without an established dB/linear convention.
   --  @field Signal_To_Noise_Ratio SNR scalar without scale conversion.
   --  @field Candidate_Object_With_Background Exact signed 16-bit 3x3 intensity patch around this candidate.
   --  @field Clutter Provider clutter scalar, with no established numeric range.
   --  @field Candidate_Object_Quality Upstream documents zero to one, but setters/bridge do not enforce or clamp it.
   --  @field Sir_Delta Signal intensity from threshold, reporting candidate sensitivity; units unspecified.
   --  @field Inertial_State This entry's own navigation snapshot, distinct from the message-level state.
   --  @field Edge True when detection is near an obscuration, as reported by the provider.
   --  @field Az_Sigma Azimuth-measurement uncertainty scalar; unit/confidence convention unspecified.
   --  @field El_Sigma Elevation-measurement uncertainty scalar; unit/confidence convention unspecified.
   --  @field Background_Normalizer Background-noise statistic used for SIR, without bridge calculation.

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
   --  Owned header/navigation/hot regions and independently sized PreProc vector.
   --  Header count need not equal vector length; both values survive unchanged.
   --  Native vector order and duplicates are preserved, without a 900-entry cap.
   function Header (Message : Candidate_Object_PreProc_Message) return Candidate_Object_Header;
   --  Copy the provider header without replacing its reported candidate count.
   --  @param Message Owned PreProc message to inspect.
   --  @return Header whose declared count is independent of actual vector length.
   function Inertial_State
     (Message : Candidate_Object_PreProc_Message) return Sensor_Inertial_State;
   --  Copy message-level navigation, distinct from per-entry inertial snapshots.
   --  @param Message Owned PreProc message to inspect.
   --  @return Independent message-level sensor inertial state.
   function Hot_Region_Count (Message : Candidate_Object_PreProc_Message) return Natural;
   --  Count the complete copied hot-region vector in provider order.
   --  @param Message Owned PreProc message to inspect.
   --  @return Hot-region count, independent of candidate/header counts.
   function Hot_Region_At
     (Message : Candidate_Object_PreProc_Message; Index : Positive) return Hot_Region;
   --  Copy a region; index beyond vector length raises a constraint error.
   --  @param Message Owned PreProc message to inspect.
   --  @param Index One-based hot-region position, not a pixel coordinate.
   --  @return Independent hot-region value.
   function Candidate_Object_PreProc_Count
     (Message : Candidate_Object_PreProc_Message) return Natural;
   --  Count the actual copied PreProc vector, not the header's candidate count.
   --  @param Message Owned PreProc message to inspect.
   --  @return Actual vector length, even when header count differs.
   function Candidate_Object_PreProc_At
     (Message : Candidate_Object_PreProc_Message; Index : Positive) return Candidate_Object_PreProc;
   --  Copy a PreProc entry; index beyond vector length raises a constraint error.
   --  @param Message Owned PreProc message to inspect.
   --  @param Index One-based vector position in provider order.
   --  @return Independent preprocessed detection including its patch and navigation.

   --  The implemented Track metadata event kinds. Receive_Event fails closed on
   --  any other native kind.
   type Metadata_Kind is
     (IRST_Track_Report_Event,
      Request_System_Track_Data_Event,
      Candidate_Object_Message_Event,
      Candidate_Object_PreProc_Message_Event);
   --  Supported inbound callback alternatives, not outgoing send categories.
   --  @enum IRST_Track_Report_Event Conditionally required Track report callback.
   --  @enum Request_System_Track_Data_Event Optional inbound request for system data; no automatic response.
   --  @enum Candidate_Object_Message_Event Advertised candidate-message callback, required when its detection condition applies.
   --  @enum Candidate_Object_PreProc_Message_Event Optional inbound PreProc vector message.
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
   --  Discriminated wholly owned event; selecting an inactive variant component
   --  is governed by Ada discriminant checks, not a provider pointer lifetime.
   --  @field Kind Active inbound callback alternative.
   --  @field Report Complete owned IRST report for the report alternative.
   --  @field Request Owned inbound system-data request, not an outbound request handle.
   --  @field Candidates Owned fixed-array candidate prefix and associated metadata.
   --  @field Candidate_PreProcs Owned independently sized PreProc vector and associated metadata.

   --  Receives the next event of either kind. A positive timeout blocks until
   --  an event arrives, the stream stops, or the timeout expires; a zero
   --  timeout is a nonblocking poll. No Ada thread spins: the native adapter
   --  waits on a condition variable.
   --  Timeout_Error is inherited from AMS.MEL.IR.
   function Receive_Event
     (Stream : Metadata_Channel; Timeout_Milliseconds : Natural := 0) return Metadata_Event;
   --  Pop the oldest successfully enqueued event of any supported kind. Queued
   --  values drain before stopped/failed state is reported. Empty active timeout
   --  raises the inherited timeout exception; stopped/inactive raises the
   --  inherited stopped-stream exception; native/callback/conversion failure
   --  raises a provider error. Native storage is released on copying exceptions.
   --  @param Stream Open metadata consumer, externally serialized against Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; default zero polls.
   --  @return Wholly Ada-owned discriminated event with no surviving provider references.

   --  Report-only convenience retained for existing callers. It raises
   --  Provider_Error if the next event is any non-report Track metadata kind,
   --  so an application that can receive more than one kind should use
   --  Receive_Event. Timeout_Error is inherited from AMS.MEL.IR.
   function Receive
     (Stream : Metadata_Channel; Timeout_Milliseconds : Natural := 0) return IRST_Track_Report;
   --  Report-only convenience that consumes the next event. A non-report event
   --  raises a provider error after consumption; it is not skipped or requeued.
   --  Use the general event receiver for mixed kinds. Timeout/stopped/native
   --  failure behavior is the same as that receiver.
   --  @param Stream Open metadata consumer, externally serialized against Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; default zero polls.
   --  @return Owned report only when the consumed event has report kind.

   type Metadata_Counters is record
      Events_Received, Events_Dropped_Queue_Full, Malformed_Or_Unsupported : Counter;
   end record;
   --  Shared unsigned counters saturate without wrapping. Callback receipt is
   --  counted before checking active consumption; totals need not sum to received.
   --  @field Events_Received Observed callback entries for all four supported families.
   --  @field Events_Dropped_Queue_Full Valid incoming events discarded when the shared queue is full.
   --  @field Malformed_Or_Unsupported Null/malformed/unsupported payload drops; callback exceptions instead fail the stream.
   function Counters (Stream : Metadata_Channel) return Metadata_Counters;
   --  Read synchronized queue counters; closed handle/native failure raises a
   --  provider error. Query does not reset counts or poll provider metadata.
   --  @param Stream Open metadata consumer.
   --  @return Owned saturating counts observed at query time.
   procedure Close (Stream : in out Metadata_Channel);
   --  Idempotently stop public consumption, wake waiters and clear the handle.
   --  Does not unregister callbacks, close Track or prove callback quiescence;
   --  captured state remains safe until provider-channel destruction/drain.
   --  Native failure raises a provider error; finalization is non-raising.
   --  @param Stream Consumer to close, serialized against its public operations.

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
