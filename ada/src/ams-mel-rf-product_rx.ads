private with Ada.Containers.Vectors;
private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with AMS.MEL.RF.C2;
with Interfaces;
private with System;

package AMS.MEL.RF.Product_Rx is
   --  Asynchronous RF receive creation and bounded delivery of copied callback products.
   --  The eight supported integer/VITA formats use native v9 metadata; no DSP, RDMA
   --  setup, physical reception or provider-wide callback quiescence is promised.
   --  Limited owners require explicit Close, with non-raising finalization fallback.

   Timeout_Error, Stream_Stopped : exception;
   --  Wait or Receive reached its timeout without completion or an available product. Timeout does
   --  not cancel creation or close reception. Stream Stopped instead reports logically stopped
   --  reception.

   type Config is private;
   --  Copyable request settings, not a provider resource. Formats are Direct INT8 (0), Direct
   --  INT16 (1), Complex INT8 (2), Complex INT16 (3), and AMS VITA Small/Medium/Large/Extra Large
   --  (4 through 7). Constructors only store settings; Submit checks the native domain, positive
   --  queue/maximum and region representability before provider entry. No PDW or Local Function
   --  receive alternative is supported.

   function Create_Config
     (Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Positive;
      Max_Samples_Per_Event : Positive) return Config;
   --  Store settings selecting Complex INT16 (3); no provider call or allocation of a receive
   --  region occurs here. Zero region size is forwarded if representable; the bridge imposes no
   --  positive-region or sample-storage-size relationship.
   --  @param Region_Size_Bytes Requested provider region size in bytes, unchanged; Submit requires
   --  it to fit native size t.
   --  @param Queue_Capacity Positive maximum number of queued copied products; a full queue drops
   --  incoming products, not older entries.
   --  @param Max_Samples_Per_Event Positive maximum sample element count per callback; one complex
   --  pair or one direct scalar is one element.
   --  @return Copied configuration for later Submit.

   type Create_Request is tagged limited private;
   --  Unique limited controlled owner of an asynchronous creation completion. Default
   --  initialization is closed. Native ownership retains DataMEL and its library across pending
   --  future, cached success and unique endpoint Claim; no reference to the parent Ada object is
   --  needed. Close abandons ownership, not provider work. Finalization attempts closure without
   --  raising.

   function Create_Config_Complex_I8
     (Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Positive;
      Max_Samples_Per_Event : Positive) return Config;
   --  Store settings selecting Complex INT8 (2); no provider call or allocation of a receive
   --  region occurs here. Zero region size is forwarded if representable; the bridge imposes no
   --  positive-region or sample-storage-size relationship.
   --  @param Region_Size_Bytes Requested provider region size in bytes, unchanged; Submit requires
   --  it to fit native size t.
   --  @param Queue_Capacity Positive maximum number of queued copied products; a full queue drops
   --  incoming products, not older entries.
   --  @param Max_Samples_Per_Event Positive maximum sample element count per callback; one complex
   --  pair or one direct scalar is one element.
   --  @return Copied configuration for later Submit.

   function Submit (Parent : Data_MEL; Value : Config) return Create_Request;
   --  Validate and submit one asynchronous provider creation, using native metadata profile v9 for
   --  every supported format 0 through 7. Profile 9 is not a JobDataFormat value. Earlier native
   --  submit domains remain frozen: v1 accepts 3, v2 accepts 2..3, v3 accepts 0..3, v4/v5/v6
   --  accept 0..4/5/6, and v7/v8 accept 0..7. Closed parent, invalid configuration, synchronous
   --  provider exceptions, invalid future or worker setup failure raise Provider Error rather than
   --  returning a normal rejection. The worker alone consumes the future. Externally serialize
   --  Submit against parent Close.
   --  @param Parent Open DataMEL; its native child claim defers parent shutdown even if the public
   --  parent closes first.
   --  @param Value Stored region, queue, maximum and exact requested format; no capability
   --  auto-gate or implicit conversion.
   --  @return Open request; successful submission does not mean endpoint creation has completed or
   --  RF samples are arriving.

   function Create_Config_Direct_I8
     (Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Positive;
      Max_Samples_Per_Event : Positive) return Config;
   --  Store settings selecting Direct INT8 (0); no provider call or allocation of a receive region
   --  occurs here. Zero region size is forwarded if representable; the bridge imposes no
   --  positive-region or sample-storage-size relationship.
   --  @param Region_Size_Bytes Requested provider region size in bytes, unchanged; Submit requires
   --  it to fit native size t.
   --  @param Queue_Capacity Positive maximum number of queued copied products; a full queue drops
   --  incoming products, not older entries.
   --  @param Max_Samples_Per_Event Positive maximum sample element count per callback; one complex
   --  pair or one direct scalar is one element.
   --  @return Copied configuration for later Submit.

   function Create_Config_Direct_I16
     (Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Positive;
      Max_Samples_Per_Event : Positive) return Config;
   --  Store settings selecting Direct INT16 (1); no provider call or allocation of a receive
   --  region occurs here. Zero region size is forwarded if representable; the bridge imposes no
   --  positive-region or sample-storage-size relationship.
   --  @param Region_Size_Bytes Requested provider region size in bytes, unchanged; Submit requires
   --  it to fit native size t.
   --  @param Queue_Capacity Positive maximum number of queued copied products; a full queue drops
   --  incoming products, not older entries.
   --  @param Max_Samples_Per_Event Positive maximum sample element count per callback; one complex
   --  pair or one direct scalar is one element.
   --  @return Copied configuration for later Submit.

   function Is_Open (Request : Create_Request) return Boolean;
   --  Inspect local request ownership only, without querying future readiness.
   --  @param Request Request whose local handle is inspected.
   --  @return True while the request handle is owned, including after completion or Claim.

   function Create_Config_AMS_VITA_Small
     (Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Positive;
      Max_Samples_Per_Event : Positive) return Config;
   --  Store settings selecting AMS VITA Small (4); no provider call or allocation of a receive
   --  region occurs here. Zero region size is forwarded if representable; the bridge imposes no
   --  positive-region or sample-storage-size relationship.
   --  @param Region_Size_Bytes Requested provider region size in bytes, unchanged; Submit requires
   --  it to fit native size t.
   --  @param Queue_Capacity Positive maximum number of queued copied products; a full queue drops
   --  incoming products, not older entries.
   --  @param Max_Samples_Per_Event Positive maximum packet-object count per callback, not bytes or
   --  decoded IQ samples.
   --  @return Copied configuration for later Submit.

   function Create_Config_AMS_VITA_Medium
     (Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Positive;
      Max_Samples_Per_Event : Positive) return Config;
   --  Store settings selecting AMS VITA Medium (5); no provider call or allocation of a receive
   --  region occurs here. Zero region size is forwarded if representable; the bridge imposes no
   --  positive-region or sample-storage-size relationship.
   --  @param Region_Size_Bytes Requested provider region size in bytes, unchanged; Submit requires
   --  it to fit native size t.
   --  @param Queue_Capacity Positive maximum number of queued copied products; a full queue drops
   --  incoming products, not older entries.
   --  @param Max_Samples_Per_Event Positive maximum packet-object count per callback, not bytes or
   --  decoded IQ samples.
   --  @return Copied configuration for later Submit.

   function Create_Config_AMS_VITA_Large
     (Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Positive;
      Max_Samples_Per_Event : Positive) return Config;
   --  Store settings selecting AMS VITA Large (6); no provider call or allocation of a receive
   --  region occurs here. Zero region size is forwarded if representable; the bridge imposes no
   --  positive-region or sample-storage-size relationship.
   --  @param Region_Size_Bytes Requested provider region size in bytes, unchanged; Submit requires
   --  it to fit native size t.
   --  @param Queue_Capacity Positive maximum number of queued copied products; a full queue drops
   --  incoming products, not older entries.
   --  @param Max_Samples_Per_Event Positive maximum packet-object count per callback, not bytes or
   --  decoded IQ samples.
   --  @return Copied configuration for later Submit.

   function Create_Config_AMS_VITA_Extra_Large
     (Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Positive;
      Max_Samples_Per_Event : Positive) return Config;
   --  Store settings selecting AMS VITA Extra Large (7); no provider call or allocation of a
   --  receive region occurs here. Zero region size is forwarded if representable; the bridge
   --  imposes no positive-region or sample-storage-size relationship.
   --  @param Region_Size_Bytes Requested provider region size in bytes, unchanged; Submit requires
   --  it to fit native size t.
   --  @param Queue_Capacity Positive maximum number of queued copied products; a full queue drops
   --  incoming products, not older entries.
   --  @param Max_Samples_Per_Event Positive maximum packet-object count per callback, not bytes or
   --  decoded IQ samples.
   --  @return Copied configuration for later Submit.

   procedure Close (Request : in out Create_Request);
   --  Idempotently abandon the public request without cancelling the future. A pending worker
   --  retains the provider graph until settlement; an abandoned successful unclaimed endpoint is
   --  destroyed without callback registration. A claimed endpoint remains independently owned.
   --  Explicit closure reports native failures; automatic finalization is non-raising.
   --  @param Request Owner consumed by closure; do not race its destruction with Wait or Claim.

   type Create_Outcome is (Created, Failed);
   --  Normal terminal creation outcome, distinct from timeout and bridge/provider exceptions.
   --  @enum Created Endpoint creation succeeded and its requested format matched; Claim remains
   --  necessary. No RF delivery is proved.
   --  @enum Failed Provider rejection or malformed terminal creation result; inspect code and
   --  description.

   type Create_Error_Code is
     (None,
      Invalid_ID,
      Invalid_State,
      Invalid_Parameters,
      Insufficient_Permissions,
      Insufficient_Resources,
      Insufficient_Local_Resources,
      Insufficient_Remote_Resources,
      Unsupported);
   --  Copied Common MEL rejection code, not an exception status. A malformed result can use None
   --  with a diagnostic; the code alone is not proof of successful creation.
   --  @enum None No specific rejection code was supplied, including malformed-result diagnostics.
   --  @enum Invalid_ID Provider reports an invalid identifier.
   --  @enum Invalid_State Provider reports creation is invalid in its state.
   --  @enum Invalid_Parameters Provider rejects request parameters.
   --  @enum Insufficient_Permissions Provider reports insufficient authorization.
   --  @enum Insufficient_Resources Provider reports insufficient resources without a locality
   --  distinction.
   --  @enum Insufficient_Local_Resources Provider reports insufficient local resources.
   --  @enum Insufficient_Remote_Resources Provider reports insufficient remote resources.
   --  @enum Unsupported Provider reports the operation is unsupported.

   for Create_Error_Code use
     (None                          => 0,
      Invalid_ID                    => 1,
      Invalid_State                 => 2,
      Invalid_Parameters            => 3,
      Insufficient_Permissions      => 4,
      Insufficient_Resources        => 5,
      Insufficient_Local_Resources  => 6,
      Insufficient_Remote_Resources => 7,
      Unsupported                   => 8);
   for Create_Error_Code'Size use 32;
   type Create_Result is private;
   --  Ordinary copied terminal result, independent of request lifetime. Created and Failed are
   --  normal outcomes; timeout and provider/bridge exceptions are raised instead. Failure text is
   --  copied, with a fallback for invalid provider UTF-8; unknown rejection codes and
   --  null/mismatched endpoints are diagnosed rather than accepted.

   function Outcome (Result : Create_Result) return Create_Outcome;
   --  Read the copied terminal discriminator without provider entry.
   --  @param Result Previously returned creation result.
   --  @return Created or Failed, not a live readiness query.

   function Error_Code (Result : Create_Result) return Create_Error_Code
   with Pre => Outcome (Result) = Failed;
   --  Read a failed result only: the declared precondition requires Outcome (Result) = Failed.
   --  This accessor is not valid for a Created result.
   --  @param Result Failed terminal result satisfying the precondition.
   --  @return Copied Common MEL rejection code, possibly None for a malformed provider result.

   function Description (Result : Create_Result) return String
   with Pre => Outcome (Result) = Failed;
   --  Read failure text only: the declared precondition requires Outcome (Result) = Failed. The
   --  returned Ada string is independent of native diagnostic storage.
   --  @param Result Failed terminal result satisfying the precondition.
   --  @return Copied rejection or malformed-result diagnostic; native invalid UTF-8 receives
   --  fallback text.

   function Wait (Request : Create_Request; Timeout_Milliseconds : Natural) return Create_Result;
   --  Wait for the cached terminal result. Native future get occurs once on a worker; repeated
   --  waits read the same completion, including after Claim. Timeout raises Timeout Error without
   --  cancellation or request consumption. Future/getter exceptions and bridge failures raise
   --  Provider Error, not Failed. A future that never completes can retain DataMEL and its library
   --  indefinitely.
   --  @param Request Open creation request; serialize against request Close/finalization.
   --  @param Timeout_Milliseconds Maximum native wait in milliseconds; zero polls current
   --  completion.
   --  @return Independent copied Created or Failed result; creation success does not establish
   --  sample delivery.

   type Endpoint is tagged limited private;
   --  Unique limited controlled owner of a claimed provider receive endpoint and the transferred
   --  DataMEL child claim. Default initialization is closed. Endpoint ID and assigned format are
   --  cached after successful provider creation, not queried live. Explicit Close reports
   --  failures; finalization attempts non-raising cleanup. Keep operations externally serialized
   --  against owner destruction.

   function Claim (Request : Create_Request'Class) return Endpoint;
   --  Transfer successful unclaimed creation to exactly one endpoint and register its data-ready
   --  callback. Pending Claim raises Timeout Error; failed, already-claimed or registration-failed
   --  requests raise Provider Error. A registration failure is cached and is not retried. Callback
   --  state is ready before registration, allowing synchronous callbacks. The exact registered
   --  std::function lvalue, callback state and provider DSO remain permanently retained: providers
   --  may keep a reference or move the callable. This shell owns neither DataMEL nor endpoint nor
   --  event payloads.
   --  @param Request Open successful request; only one Claim may succeed.
   --  @return Unique open endpoint with the requested format verified against the assigned format;
   --  not proof of reception.

   function Is_Open (Object : Endpoint) return Boolean;
   --  Inspect local endpoint ownership without checking provider health or callback delivery.
   --  @param Object Endpoint whose local ownership is inspected.
   --  @return True while a native endpoint handle is owned.

   function Endpoint_ID (Object : Endpoint) return Interfaces.Unsigned_64;
   --  Read the creation-time cached provider identity; this is not an array index or live query.
   --  @param Object Endpoint value containing the cached ID.
   --  @return Exact unsigned 64-bit endpoint ID, retained as a cached value after Close.

   function Assigned_Data_Format (Object : Endpoint) return Job_Data_Format;
   --  Read the cached assigned provider format. Creation checks equality with the requested
   --  supported format before callback registration.
   --  @param Object Endpoint containing the cached format, which remains stored after Close.
   --  @return One of formats 0 through 7 after successful Claim; no conversion or broader upstream
   --  support is implied.

   procedure Close (Object : in out Endpoint);
   --  Idempotently consume the endpoint owner. Native logical closure discards queued products and
   --  wakes receivers before draining admitted bridge callback bodies; only then is the provider
   --  endpoint destroyed and its parent claim released. This drain is not provider-wide callback
   --  quiescence. Late callbacks take the stopped path before payload access. The permanent
   --  callable/state/DSO shell is not freed at Close. Failure to establish safe drain retains
   --  endpoint and claim; throwing parent shutdown permanently retains its provider graph.
   --  Delivered Events remain usable.
   --  @param Object Endpoint owner to close; externally serialize wrapper destruction against
   --  other operations. Explicit failures raise Provider Error; finalization does not propagate
   --  them.

   type Counter is mod 2**64 with Size => 64;
   --  Unsigned 64-bit diagnostic count. Native increments saturate at the maximum rather than
   --  wrapping, despite the modular Ada representation.

   type Counters is record
      Callbacks_Received          : Counter;
      Products_Queued             : Counter;
      Products_Dropped_Queue_Full : Counter;
      Malformed_Or_Unsupported    : Counter;
      Allocation_Failures         : Counter;
      Callbacks_After_Close       : Counter;
   end record;
   --  Consistent copied native queue/callback counters; these are bridge observations, not RF
   --  performance measurements.
   --  @field Callbacks_Received All bridge callback entries, including late callbacks after
   --  logical closure.
   --  @field Products_Queued Complete products successfully appended to the queue; cumulative, not
   --  current queue depth.
   --  @field Products_Dropped_Queue_Full Incoming complete products discarded because the bounded
   --  queue was full; older queued products remain.
   --  @field Malformed_Or_Unsupported Callbacks rejected for invalid metadata, wrong alternatives,
   --  oversize counts or unsupported content; no partial product is queued.
   --  @field Allocation_Failures Callback copy or queue-allocation failures; no partial event is
   --  published.
   --  @field Callbacks_After_Close Entries observed after logical Close, or admitted copies that
   --  lost publication to Close.

   function Statistics (Object : Endpoint) return Counters;
   --  Copy all counters under the native callback-state lock without consuming products. Every
   --  counter saturates; queue-full drops are distinct from malformed products and allocation
   --  failures. Closed/invalid native ownership raises Provider Error.
   --  @param Object Open endpoint whose bridge counters are sampled.
   --  @return Independent diagnostic counter snapshot.

   type Complex_I16 is record
      Real : Interfaces.Integer_16;
      Imag : Interfaces.Integer_16;
   end record
   --  C-compatible Cartesian I/Q value with two signed 16-bit integer components. Native
   --  MELComplex values are copied component-wise; no upstream C++ object-layout compatibility,
   --  sample scaling or normalization is assumed.
   --  @field Real Signed real (in-phase) component, copied without DSP conversion.
   --  @field Imag Signed imaginary (quadrature) component, copied without DSP conversion.
   with Convention => C;
   --  C-compatible Cartesian I/Q value with two signed 16-bit integer components. Native
   --  MELComplex values are copied component-wise; no upstream C++ object-layout compatibility,
   --  sample scaling or normalization is assumed.
   --  @field Real Signed real (in-phase) component, copied without DSP conversion.
   --  @field Imag Signed imaginary (quadrature) component, copied without DSP conversion.

   type Complex_I16_Array is array (Natural range <>) of Complex_I16 with Convention => C;
   --  C-convention array of signed 16-bit Cartesian pairs; Length counts pairs, not bytes.
   --  Borrowed nonempty sample arrays use indices 0 through count minus one; explicit copies use 1
   --  through count. Empty arrays have a null range.

   type Complex_I8 is record
      Real : Interfaces.Integer_8;
      Imag : Interfaces.Integer_8;
   end record
   --  C-compatible Cartesian I/Q value with two signed 8-bit integer components. Native MELComplex
   --  values are copied component-wise; no upstream C++ object-layout compatibility, sample
   --  scaling or normalization is assumed.
   --  @field Real Signed real (in-phase) component, copied without DSP conversion.
   --  @field Imag Signed imaginary (quadrature) component, copied without DSP conversion.
   with Convention => C;
   --  C-compatible Cartesian I/Q value with two signed 8-bit integer components. Native MELComplex
   --  values are copied component-wise; no upstream C++ object-layout compatibility, sample
   --  scaling or normalization is assumed.
   --  @field Real Signed real (in-phase) component, copied without DSP conversion.
   --  @field Imag Signed imaginary (quadrature) component, copied without DSP conversion.

   type Complex_I8_Array is array (Natural range <>) of Complex_I8 with Convention => C;
   --  C-convention array of signed 8-bit Cartesian pairs; Length counts pairs, not bytes. Borrowed
   --  nonempty sample arrays use indices 0 through count minus one; explicit copies use 1 through
   --  count. Empty arrays have a null range.

   type Direct_I8_Array is array (Natural range <>) of Interfaces.Integer_8 with Convention => C;
   --  C-convention array of signed 8-bit scalar values, not I/Q pairs. Length counts elements, not
   --  bytes. Borrowed nonempty arrays are zero-based; explicit copies are one-based. No
   --  resampling, normalization or decoding occurs.

   type Direct_I16_Array is array (Natural range <>) of Interfaces.Integer_16 with Convention => C;
   --  C-convention array of signed 16-bit scalar values, not I/Q pairs. Length counts elements,
   --  not bytes. Borrowed nonempty arrays are zero-based; explicit copies are one-based. No
   --  resampling, normalization or decoding occurs.

   type Product_Metadata is private;
   --  Ordinary copied observation metadata with Ada-owned ordered vectors; no provider object,
   --  live query or command is retained. Copies survive Event, Endpoint and DataMEL closure. Safe
   --  Ada Submit and Receive use native v9 for all formats 0..7, adding stabilization to v8
   --  association semantics; these profile numbers are not new formats. Older native profiles
   --  remain frozen and reject nonempty metadata they cannot expose. User-defined std::any content
   --  and full ReceiveEvent objects remain unsupported and reject callbacks rather than being
   --  silently dropped. Stored identifiers and pointings do not prove physical RF provenance or
   --  commanded-event validity.

   function MEL_Protocol_Version_ID (Value : Product_Metadata) return Interfaces.Unsigned_32;
   --  Read the copied provider-reported MEL protocol/API version identifier. This observation is
   --  not a live provider lookup, array position or validation against a commanded Job.
   --  @param Value Independent Ada-owned product metadata.
   --  @return Exact unsigned 32-bit observed identifier, including zero or any high bits supplied.

   function VA_Definition_ID (Value : Product_Metadata) return Interfaces.Unsigned_32;
   --  Read the copied provider VA definition identity. This observation is not a live provider
   --  lookup, array position or validation against a commanded Job.
   --  @param Value Independent Ada-owned product metadata.
   --  @return Exact unsigned 32-bit observed identifier, including zero or any high bits supplied.

   function VA_Instance_ID (Value : Product_Metadata) return Interfaces.Unsigned_32;
   --  Read the copied provider VA instance identity. This observation is not a live provider
   --  lookup, array position or validation against a commanded Job.
   --  @param Value Independent Ada-owned product metadata.
   --  @return Exact unsigned 32-bit observed identifier, including zero or any high bits supplied.

   function Job_Details_ID (Value : Product_Metadata) return Interfaces.Unsigned_32;
   --  Read the copied provider JobDetails identity. This observation is not a live provider
   --  lookup, array position or validation against a commanded Job.
   --  @param Value Independent Ada-owned product metadata.
   --  @return Exact unsigned 32-bit observed identifier, including zero or any high bits supplied.

   function Job_Interval_ID (Value : Product_Metadata) return Interfaces.Unsigned_32;
   --  Read the copied provider JobInterval identity. This observation is not a live provider
   --  lookup, array position or validation against a commanded Job.
   --  @param Value Independent Ada-owned product metadata.
   --  @return Exact unsigned 32-bit observed identifier, including zero or any high bits supplied.

   function LF_Type_ID (Value : Product_Metadata) return Interfaces.Unsigned_32;
   --  Read the copied provider Local Function type identity. This observation is not a live
   --  provider lookup, array position or validation against a commanded Job.
   --  @param Value Independent Ada-owned product metadata.
   --  @return Exact unsigned 32-bit observed identifier, including zero or any high bits supplied.

   function LF_Instance_ID (Value : Product_Metadata) return Interfaces.Unsigned_32;
   --  Read the copied provider Local Function instance identity. This observation is not a live
   --  provider lookup, array position or validation against a commanded Job.
   --  @param Value Independent Ada-owned product metadata.
   --  @return Exact unsigned 32-bit observed identifier, including zero or any high bits supplied.

   function Phase_Coherence_With_Prior (Value : Product_Metadata) return Boolean;
   --  Read the provider-reported coherence flag for this collection relative to the prior
   --  interval. The bridge copies the Boolean without measuring phase or establishing physical
   --  coherence.
   --  @param Value Ada-owned metadata observation.
   --  @return Copied provider coherence observation.

   function First_Rx_Event_Start_Seconds (Value : Product_Metadata) return Interfaces.Integer_64;
   --  Read integral UTC seconds copied verbatim from the first receive-event start. This is
   --  provider metadata, not a bridge clock reading or hardware timestamp accuracy guarantee.
   --  @param Value Copied metadata holding the observed start time.
   --  @return Exact signed 64-bit seconds; interpreted together with the fractional femtoseconds
   --  accessor.

   function First_Rx_Event_Start_Femtoseconds
     (Value : Product_Metadata) return Interfaces.Integer_64;
   --  Read the fractional femtosecond count copied verbatim from provider start-time metadata,
   --  without nanosecond conversion or normalization. Unlike active stabilization UTC fields, this
   --  legacy field has no added bridge canonical-fraction validation; do not infer such validation
   --  from the v9 profile.
   --  @param Value Copied metadata holding the provider time representation.
   --  @return Exact signed 64-bit fractional femtosecond count alongside the integral seconds.

   function Rx_Stream_ID_Count (Value : Product_Metadata) return Natural;
   --  Count copied stream identifiers, preserving provider vector order and repetitions. The VA
   --  defines their meaning; this count is not required to equal sample or packet count.
   --  @param Value Ada-owned metadata containing the copied stream-ID vector.
   --  @return Stored stream-ID vector length, possibly zero.

   function Rx_Stream_ID_At
     (Value : Product_Metadata; Index : Positive) return Interfaces.Unsigned_32;
   --  Read a stream identity by Ada position, preserving provider order and duplicates. Stream IDs
   --  are opaque provider values, not array positions; no normalization or relationship validation
   --  is performed.
   --  @param Value Copied metadata; access is independent of Event lifetime.
   --  @param Index Positive one-based vector position; beyond the stored count raises
   --  Constraint Error.
   --  @return Exact unsigned 32-bit stream identity at the selected position.

   type Event is limited private;
   --  Unique limited controlled owner of one immutable native copied callback product, plus
   --  ordinary Ada metadata copies. Default initialization is closed. Provider buffers are read
   --  and copied before callback return and are never retained as borrowed event data. A delivered
   --  event survives endpoint and DataMEL closure without retaining their provider objects.
   --  Explicit Close releases the event; automatic finalization is non-raising. Serialize access
   --  against Event Close/finalization; borrowed views are not unrestricted cross-task references.

   function Receive (Object : Endpoint'Class; Timeout_Milliseconds : Natural := 0) return Event;
   --  Transfer the oldest complete queued event to one owner, copying checked metadata to Ada
   --  without a second bulk payload copy. Queue capacity is bounded and drops incoming products
   --  when full. Malformed or unsupported callbacks and copy-allocation failures publish nothing
   --  and are counted separately. Native stop raises Stream Stopped in preference to queued
   --  delivery; no ready product raises Timeout Error without stopping reception. Invalid native
   --  views raise Provider Error and the partially constructed owner is finalized.
   --  @param Object Open endpoint; serialize public wrapper destruction against operations.
   --  @param Timeout_Milliseconds Native wait in milliseconds; default zero polls without blocking
   --  for a product.
   --  @return Unique copied event; sample or packet data remain owned until Event Close,
   --  independently of provider buffer reuse.

   function Is_Open (Value : Event) return Boolean;
   --  Inspect ownership of the copied native product, not provider endpoint state.
   --  @param Value Event whose ownership is inspected.
   --  @return True while the event owns its native handle.

   function Endpoint_ID (Value : Event) return Interfaces.Unsigned_64;
   --  Read the endpoint identity copied into the event at delivery; not a current provider lookup.
   --  @param Value Event containing the copied endpoint ID.
   --  @return Exact unsigned 64-bit originating endpoint identity; cached after Event Close.

   function Data_Format (Value : Event) return Job_Data_Format;
   --  Read the selected format copied into the event. Inactive native payload spans are checked
   --  empty; an empty active span does not change the selected format.
   --  @param Value Event containing the copied format discriminator.
   --  @return Format 0 through 7; stored after Close, not evidence that samples are still
   --  available.

   function Metadata (Value : Event) return Product_Metadata;
   --  Copy the Ada-owned metadata observation without any provider call. The returned metadata and
   --  its vectors survive Event, Endpoint and DataMEL closure.
   --  @param Value Event containing the already copied metadata.
   --  @return Independent ordinary Ada metadata value, not a live provider snapshot query.

   function Sample_Count (Value : Event) return Natural;
   --  Read the stored element count, not a byte count. Complex formats count Cartesian pairs,
   --  direct formats count signed scalars, and VITA formats count packet objects, not decoded
   --  samples or payload bytes. Event Close resets this count to zero.
   --  @param Value Event whose stored payload count is read.
   --  @return Number of active payload elements stored in the event.

   --  Borrows the native event payload: no additional bulk copy. Valid only
   --  during Process, while the Event remains open and is not concurrently closed.
   procedure With_Samples
     (Value : Event; Process : not null access procedure (Samples : Complex_I16_Array));
   --  Borrow event-owned signed 16-bit Cartesian pairs for Complex INT16 with no additional bulk
   --  copy. The open-event check precedes the exact-format check; wrong format raises
   --  Constraint Error even when count is zero, and a closed event raises Provider Error. A
   --  correctly selected empty event invokes Process with a null array. Nonempty borrowed arrays
   --  are zero-based. No decoding, normalization or scaling occurs. Externally serialize with
   --  Event Close and finalization; do not retain addresses or views beyond Process or close the
   --  event inside Process.
   --  @param Value Open Complex INT16 event owning the immutable buffer.
   --  @param Process Synchronous application procedure receiving the borrowed array only for this
   --  call; application exceptions propagate normally.

   --  Explicit Ada-owned payload copy, unlike With_Samples.
   function Copy_Samples (Value : Event) return Complex_I16_Array;
   --  Copy signed 16-bit Cartesian pairs from a selected Complex INT16 event into independent Ada
   --  storage. Uses the same open-event and exact-format checks as borrowed access: wrong format
   --  raises Constraint Error even for empty events; a closed event raises Provider Error.
   --  Serialize copying against Event Close. No conversion, resampling or DSP is performed.
   --  @param Value Open event of exactly Complex INT16 format.
   --  @return One-based Ada array (null range if empty) whose values survive Event closure.

   --  Width-specific access raises Constraint_Error for the wrong format,
   --  even for an empty event. Borrowing requires external serialization
   --  with Close, just like With_Samples; copies are independent Ada owners.
   procedure With_Samples_I8
     (Value : Event; Process : not null access procedure (Samples : Complex_I8_Array));
   --  Borrow event-owned signed 8-bit Cartesian pairs for Complex INT8 with no additional bulk
   --  copy. The open-event check precedes the exact-format check; wrong format raises
   --  Constraint Error even when count is zero, and a closed event raises Provider Error. A
   --  correctly selected empty event invokes Process with a null array. Nonempty borrowed arrays
   --  are zero-based. No decoding, normalization or scaling occurs. Externally serialize with
   --  Event Close and finalization; do not retain addresses or views beyond Process or close the
   --  event inside Process.
   --  @param Value Open Complex INT8 event owning the immutable buffer.
   --  @param Process Synchronous application procedure receiving the borrowed array only for this
   --  call; application exceptions propagate normally.

   function Copy_Samples_I8 (Value : Event) return Complex_I8_Array;
   --  Copy signed 8-bit Cartesian pairs from a selected Complex INT8 event into independent Ada
   --  storage. Uses the same open-event and exact-format checks as borrowed access: wrong format
   --  raises Constraint Error even for empty events; a closed event raises Provider Error.
   --  Serialize copying against Event Close. No conversion, resampling or DSP is performed.
   --  @param Value Open event of exactly Complex INT8 format.
   --  @return One-based Ada array (null range if empty) whose values survive Event closure.

   --  Scalar samples, never I/Q pairs. Wrong-format access raises
   --  Constraint_Error before sample access, including empty events.
   --  Borrowing requires an open event externally serialized with Close.
   procedure With_Direct_Samples_I8
     (Value : Event; Process : not null access procedure (Samples : Direct_I8_Array));
   --  Borrow event-owned signed 8-bit scalar samples for Direct INT8 with no additional bulk copy.
   --  The open-event check precedes the exact-format check; wrong format raises Constraint Error
   --  even when count is zero, and a closed event raises Provider Error. A correctly selected
   --  empty event invokes Process with a null array. Nonempty borrowed arrays are zero-based. No
   --  decoding, normalization or scaling occurs. Externally serialize with Event Close and
   --  finalization; do not retain addresses or views beyond Process or close the event inside
   --  Process.
   --  @param Value Open Direct INT8 event owning the immutable buffer.
   --  @param Process Synchronous application procedure receiving the borrowed array only for this
   --  call; application exceptions propagate normally.

   function Copy_Direct_Samples_I8 (Value : Event) return Direct_I8_Array;
   --  Copy signed 8-bit scalar samples from a selected Direct INT8 event into independent Ada
   --  storage. Uses the same open-event and exact-format checks as borrowed access: wrong format
   --  raises Constraint Error even for empty events; a closed event raises Provider Error.
   --  Serialize copying against Event Close. No conversion, resampling or DSP is performed.
   --  @param Value Open event of exactly Direct INT8 format.
   --  @return One-based Ada array (null range if empty) whose values survive Event closure.

   procedure With_Direct_Samples_I16
     (Value : Event; Process : not null access procedure (Samples : Direct_I16_Array));
   --  Borrow event-owned signed 16-bit scalar samples for Direct INT16 with no additional bulk
   --  copy. The open-event check precedes the exact-format check; wrong format raises
   --  Constraint Error even when count is zero, and a closed event raises Provider Error. A
   --  correctly selected empty event invokes Process with a null array. Nonempty borrowed arrays
   --  are zero-based. No decoding, normalization or scaling occurs. Externally serialize with
   --  Event Close and finalization; do not retain addresses or views beyond Process or close the
   --  event inside Process.
   --  @param Value Open Direct INT16 event owning the immutable buffer.
   --  @param Process Synchronous application procedure receiving the borrowed array only for this
   --  call; application exceptions propagate normally.

   function Copy_Direct_Samples_I16 (Value : Event) return Direct_I16_Array;
   --  Copy signed 16-bit scalar samples from a selected Direct INT16 event into independent Ada
   --  storage. Uses the same open-event and exact-format checks as borrowed access: wrong format
   --  raises Constraint Error even for empty events; a closed event raises Provider Error.
   --  Serialize copying against Event Close. No conversion, resampling or DSP is performed.
   --  @param Value Open event of exactly Direct INT16 format.
   --  @return One-based Ada array (null range if empty) whose values survive Event closure.

   procedure Close (Value : in out Event);
   --  Idempotently release the sole native copied event and clear borrowed sample/packet addresses
   --  and count. Cached ID, format and Ada metadata remain stored; previously returned metadata
   --  and explicit payload copies remain independent. Externally serialize all event access with
   --  closure. Automatic finalization attempts the same release without propagating exceptions.
   --  @param Value Event owner consumed; no provider sample buffer or endpoint is destroyed here.

   type Packet_Byte_Array is array (Natural range <>) of Interfaces.Unsigned_8 with Convention => C;
   --  C-convention unsigned octets containing validated serialized AMS VITA packet bytes,
   --  including prologue, payload and trailer. Bytes remain in original packet/network order;
   --  there is no host byte-order conversion, IQ decoding or claim of complete general VITA
   --  semantic conformance. Borrowed and copied packet arrays are one-based and contain actual
   --  declared length, not unused storage tail.

   --  Packet count, not IQ/sample byte count. Wrong-format access raises
   --  Constraint_Error, including empty events. Borrowing requires external
   --  serialization with Close; explicit copies survive Event Close.
   function VITA_Small_Packet_Count (Value : Event) return Natural;
   --  Count AMS VITA Small packet objects, not payload bytes or IQ samples. The selected native
   --  packet class has 512-byte storage capacity and at most 480 payload bytes. The pinned AMS
   --  VITA view must validate each packet before its declared size is read: accepted serialized
   --  length is 32 through 512 bytes in multiples of four. Any malformed packet rejects the entire
   --  callback, including earlier valid packets, with no partial event publication. Wrong format
   --  raises Constraint Error even on an empty event; closed event raises Provider Error.
   --  @param Value Open event selected as AMS VITA Small; no provider call or decoding.
   --  @return Packet-object count, including zero for a correctly selected empty event.

   procedure With_VITA_Small_Packet
     (Value   : Event;
      Index   : Positive;
      Process : not null access procedure (Bytes : Packet_Byte_Array));
   --  Borrow exactly one validated AMS VITA Small packet from immutable event-owned copies. The
   --  native pinned packet view, not fixed storage capacity, determines the original serialized
   --  byte length (32..512, multiple of four). No storage tail, byte swapping or sample decoding
   --  is supplied. The event remains usable after callback return and endpoint/DataMEL closure.
   --  Wrong format raises Constraint Error even for empty events; out-of-range Index raises
   --  Constraint Error and a closed event raises Provider Error. Externally serialize with Event
   --  Close/finalization and do not close the event in Process.
   --  @param Value Open AMS VITA Small event owning the packet bytes.
   --  @param Index One-based packet-object position; must not exceed the packet count. An empty
   --  event has no valid index.
   --  @param Process Synchronous procedure receiving a one-based raw byte array only during this
   --  call. Do not retain addresses or borrow across tasks; application exceptions propagate.

   function Copy_VITA_Small_Packet (Value : Event; Index : Positive) return Packet_Byte_Array;
   --  Return an independent copy of one AMS VITA Small packet, preserving all validated serialized
   --  bytes in original order, not unused capacity. Uses the same selected-format, open-event and
   --  one-based index checks as borrowed packet access. Wrong format or invalid index raises
   --  Constraint Error, including empty events; closed event raises Provider Error. Serialize
   --  copying against Event Close. Mock/header byte fidelity does not establish physical reception
   --  or positive Squall VITA delivery.
   --  @param Value Open AMS VITA Small event.
   --  @param Index One-based packet-object index, never a byte offset or provider identifier.
   --  @return One-based Ada byte array of actual validated packet length, 32..512 bytes; survives
   --  Event closure.

   function VITA_Medium_Packet_Count (Value : Event) return Natural;
   --  Count AMS VITA Medium packet objects, not payload bytes or IQ samples. The selected native
   --  packet class has 1024-byte storage capacity and at most 992 payload bytes. The pinned AMS
   --  VITA view must validate each packet before its declared size is read: accepted serialized
   --  length is 32 through 1024 bytes in multiples of four. Any malformed packet rejects the
   --  entire callback, including earlier valid packets, with no partial event publication. Wrong
   --  format raises Constraint Error even on an empty event; closed event raises Provider Error.
   --  @param Value Open event selected as AMS VITA Medium; no provider call or decoding.
   --  @return Packet-object count, including zero for a correctly selected empty event.

   procedure With_VITA_Medium_Packet
     (Value   : Event;
      Index   : Positive;
      Process : not null access procedure (Bytes : Packet_Byte_Array));
   --  Borrow exactly one validated AMS VITA Medium packet from immutable event-owned copies. The
   --  native pinned packet view, not fixed storage capacity, determines the original serialized
   --  byte length (32..1024, multiple of four). No storage tail, byte swapping or sample decoding
   --  is supplied. The event remains usable after callback return and endpoint/DataMEL closure.
   --  Wrong format raises Constraint Error even for empty events; out-of-range Index raises
   --  Constraint Error and a closed event raises Provider Error. Externally serialize with Event
   --  Close/finalization and do not close the event in Process.
   --  @param Value Open AMS VITA Medium event owning the packet bytes.
   --  @param Index One-based packet-object position; must not exceed the packet count. An empty
   --  event has no valid index.
   --  @param Process Synchronous procedure receiving a one-based raw byte array only during this
   --  call. Do not retain addresses or borrow across tasks; application exceptions propagate.

   function Copy_VITA_Medium_Packet (Value : Event; Index : Positive) return Packet_Byte_Array;
   --  Return an independent copy of one AMS VITA Medium packet, preserving all validated
   --  serialized bytes in original order, not unused capacity. Uses the same selected-format,
   --  open-event and one-based index checks as borrowed packet access. Wrong format or invalid
   --  index raises Constraint Error, including empty events; closed event raises Provider Error.
   --  Serialize copying against Event Close. Mock/header byte fidelity does not establish physical
   --  reception or positive Squall VITA delivery.
   --  @param Value Open AMS VITA Medium event.
   --  @param Index One-based packet-object index, never a byte offset or provider identifier.
   --  @return One-based Ada byte array of actual validated packet length, 32..1024 bytes; survives
   --  Event closure.

   function VITA_Large_Packet_Count (Value : Event) return Natural;
   --  Count AMS VITA Large packet objects, not payload bytes or IQ samples. The selected native
   --  packet class has 8192-byte storage capacity and at most 8160 payload bytes. The pinned AMS
   --  VITA view must validate each packet before its declared size is read: accepted serialized
   --  length is 32 through 8192 bytes in multiples of four. Any malformed packet rejects the
   --  entire callback, including earlier valid packets, with no partial event publication. Wrong
   --  format raises Constraint Error even on an empty event; closed event raises Provider Error.
   --  @param Value Open event selected as AMS VITA Large; no provider call or decoding.
   --  @return Packet-object count, including zero for a correctly selected empty event.

   procedure With_VITA_Large_Packet
     (Value   : Event;
      Index   : Positive;
      Process : not null access procedure (Bytes : Packet_Byte_Array));
   --  Borrow exactly one validated AMS VITA Large packet from immutable event-owned copies. The
   --  native pinned packet view, not fixed storage capacity, determines the original serialized
   --  byte length (32..8192, multiple of four). No storage tail, byte swapping or sample decoding
   --  is supplied. The event remains usable after callback return and endpoint/DataMEL closure.
   --  Wrong format raises Constraint Error even for empty events; out-of-range Index raises
   --  Constraint Error and a closed event raises Provider Error. Externally serialize with Event
   --  Close/finalization and do not close the event in Process.
   --  @param Value Open AMS VITA Large event owning the packet bytes.
   --  @param Index One-based packet-object position; must not exceed the packet count. An empty
   --  event has no valid index.
   --  @param Process Synchronous procedure receiving a one-based raw byte array only during this
   --  call. Do not retain addresses or borrow across tasks; application exceptions propagate.

   function Copy_VITA_Large_Packet (Value : Event; Index : Positive) return Packet_Byte_Array;
   --  Return an independent copy of one AMS VITA Large packet, preserving all validated serialized
   --  bytes in original order, not unused capacity. Uses the same selected-format, open-event and
   --  one-based index checks as borrowed packet access. Wrong format or invalid index raises
   --  Constraint Error, including empty events; closed event raises Provider Error. Serialize
   --  copying against Event Close. Mock/header byte fidelity does not establish physical reception
   --  or positive Squall VITA delivery.
   --  @param Value Open AMS VITA Large event.
   --  @param Index One-based packet-object index, never a byte offset or provider identifier.
   --  @return One-based Ada byte array of actual validated packet length, 32..8192 bytes; survives
   --  Event closure.

   function VITA_Extra_Large_Packet_Count (Value : Event) return Natural;
   --  Count AMS VITA Extra Large packet objects, not payload bytes or IQ samples. The selected
   --  native packet class has 32768-byte storage capacity and at most 32736 payload bytes. The
   --  pinned AMS VITA view must validate each packet before its declared size is read: accepted
   --  serialized length is 32 through 32768 bytes in multiples of four. Any malformed packet
   --  rejects the entire callback, including earlier valid packets, with no partial event
   --  publication. Wrong format raises Constraint Error even on an empty event; closed event
   --  raises Provider Error.
   --  @param Value Open event selected as AMS VITA Extra Large; no provider call or decoding.
   --  @return Packet-object count, including zero for a correctly selected empty event.

   procedure With_VITA_Extra_Large_Packet
     (Value   : Event;
      Index   : Positive;
      Process : not null access procedure (Bytes : Packet_Byte_Array));
   --  Borrow exactly one validated AMS VITA Extra Large packet from immutable event-owned copies.
   --  The native pinned packet view, not fixed storage capacity, determines the original
   --  serialized byte length (32..32768, multiple of four). No storage tail, byte swapping or
   --  sample decoding is supplied. The event remains usable after callback return and
   --  endpoint/DataMEL closure. Wrong format raises Constraint Error even for empty events;
   --  out-of-range Index raises Constraint Error and a closed event raises Provider Error.
   --  Externally serialize with Event Close/finalization and do not close the event in Process.
   --  @param Value Open AMS VITA Extra Large event owning the packet bytes.
   --  @param Index One-based packet-object position; must not exceed the packet count. An empty
   --  event has no valid index.
   --  @param Process Synchronous procedure receiving a one-based raw byte array only during this
   --  call. Do not retain addresses or borrow across tasks; application exceptions propagate.

   function Copy_VITA_Extra_Large_Packet (Value : Event; Index : Positive) return Packet_Byte_Array;
   --  Return an independent copy of one AMS VITA Extra Large packet, preserving all validated
   --  serialized bytes in original order, not unused capacity. Uses the same selected-format,
   --  open-event and one-based index checks as borrowed packet access. Wrong format or invalid
   --  index raises Constraint Error, including empty events; closed event raises Provider Error.
   --  Serialize copying against Event Close. Mock/header byte fidelity does not establish physical
   --  reception or positive Squall VITA delivery.
   --  @param Value Open AMS VITA Extra Large event.
   --  @param Index One-based packet-object index, never a byte offset or provider identifier.
   --  @return One-based Ada byte array of actual validated packet length, 32..32768 bytes;
   --  survives Event closure.

   function Receive_Event_Associations_Present (Value : Product_Metadata) return Boolean;
   --  Distinguish absent optional associations from present-empty and present-nonempty values. For
   --  v9 VITA events a present vector must match packet-object count, so present-empty is valid
   --  only for zero packets; absence remains allowed at any count. Direct/complex events permit
   --  absent or present-empty only.
   --  @param Value Copied association presence and IDs; no provider query.
   --  @return True when the provider optional had a value, even if its copied vector is empty.

   function Receive_Event_Association_Count (Value : Product_Metadata) return Natural;
   --  Count copied association IDs, not ReceiveEvent objects. Zero alone cannot distinguish
   --  absence from present-empty. VITA present cardinality equals packet-object count;
   --  Direct/Complex nonempty associations reject the whole callback.
   --  @param Value Metadata with independent Ada association storage.
   --  @return Number of stored association IDs, preserving order and duplicates.

   function Receive_Event_Association_ID_At
     (Value : Product_Metadata; Index : Positive) return Interfaces.Unsigned_32;
   --  Read an observed association ID at the corresponding one-based VITA packet position when
   --  present. IDs may repeat and preserve all bits. The bridge does not map full ReceiveEvent
   --  objects or validate that IDs refer to actual observations or commanded events.
   --  @param Value Copied metadata independent of native Event lifetime.
   --  @param Index Positive one-based stored association position; beyond count raises
   --  Constraint Error, including absent/empty vectors.
   --  @return Exact unsigned 32-bit opaque observed event identity.

   --  Ada-owned executed-collection metadata, independent of native Event lifetime.
   function Stabilization_Point_Count (Value : Product_Metadata) return Natural;
   --  Count copied executed-collection stabilization points. These are provider observations, not
   --  newly commanded Job pointing or the requested estimated stabilization point. No equality
   --  with sample, packet or association count is imposed.
   --  @param Value Ada-owned metadata containing copied pointings.
   --  @return Length of the ordered nonoptional stabilization vector, including zero.

   function Stabilization_Point_At
     (Value : Product_Metadata; Index : Positive) return AMS.MEL.RF.C2.Pointing;
   --  Copy an executed-collection pointing using the existing five-variant RF C2 model: ECEF, LLA,
   --  platform-relative, face-relative or baseline-relative. Active ECEF/LLA UTC fractions must be
   --  canonical (0 through 999999999999999 femtoseconds); malformed values reject the callback and
   --  invalid native views raise Provider Error. Coordinates, velocities, radians and time fields
   --  are preserved without frame conversion, clamping, normalization or physical-validity
   --  computation. Order and repeated pointings survive; full ReceiveEvent stabilization
   --  references remain unmapped.
   --  @param Value Copied executed-collection metadata, not a live provider query.
   --  @param Index Positive one-based stabilization position; beyond count raises
   --  Constraint Error.
   --  @return Independent C2.Pointing value with its active variant preserved; survives all
   --  receive-owner closure.

private
   use type Interfaces.Unsigned_32;
   pragma
     Compile_Time_Error
       (Complex_I16'Size /= AMS.MEL_C_API.RF_Complex_I16_V1'Size,
        "Complex_I16 size does not match the C ABI");
   package ID_Vectors is new Ada.Containers.Vectors (Positive, Interfaces.Unsigned_32);
   package Stabilization_Vectors is new
     Ada.Containers.Vectors (Positive, AMS.MEL.RF.C2.Pointing, AMS.MEL.RF.C2."=");
   procedure Copy_Stabilization
     (Span : AMS.MEL_C_API.RF_Pointing_Span_V1; Target : in out Stabilization_Vectors.Vector);
   type Config is record
      Region         : Interfaces.Unsigned_64;
      Queue, Maximum : Positive;
      Format         : Job_Data_Format := Complex_INT16;
   end record;
   type Create_Request is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_Product_Rx_Request_Handle :=
        AMS.MEL_C_API.Null_RF_Product_Rx_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Create_Request);
   type Create_Result is record
      State : Create_Outcome := Created;
      Code  : Create_Error_Code := None;
      Text  : Ada.Strings.Unbounded.Unbounded_String;
   end record;
   type Endpoint is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_Product_Rx_Handle := AMS.MEL_C_API.Null_RF_Product_Rx;
      ID     : Interfaces.Unsigned_64 := 0;
      Format : Job_Data_Format := Complex_INT16;
   end record;
   overriding
   procedure Finalize (Object : in out Endpoint);
   type Product_Metadata is record
      Protocol, Definition, Instance, Details, Interval, LF_Type, LF_Instance :
        Interfaces.Unsigned_32 := 0;
      Coherent                                                                : Boolean := False;
      Seconds, Femtoseconds                                                   :
        Interfaces.Integer_64 := 0;
      Stream_IDs                                                              : ID_Vectors.Vector;
      Associations_Present                                                    : Boolean := False;
      Association_IDs                                                         : ID_Vectors.Vector;
      Stabilization                                                           :
        Stabilization_Vectors.Vector;
   end record;
   type Event is new Ada.Finalization.Limited_Controlled with record
      Handle         : aliased AMS.MEL_C_API.RF_Product_Rx_Event_Handle :=
        AMS.MEL_C_API.Null_RF_Product_Rx_Event;
      ID             : Interfaces.Unsigned_64 := 0;
      Format         : Job_Data_Format := Complex_INT16;
      Info           : Product_Metadata;
      Sample_Address : System.Address := System.Null_Address;
      Packet_Address : System.Address := System.Null_Address;
      Count          : Natural := 0;
   end record;
   overriding
   procedure Finalize (Value : in out Event);
end AMS.MEL.RF.Product_Rx;
