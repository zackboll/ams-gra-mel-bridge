with Ada.Containers.Vectors;
private with Ada.Finalization;
private with Ada.Strings.Unbounded;
with AMS.MEL.IR.Channel;
with AMS.MEL.Status;
with Interfaces;
private with AMS.MEL_C_API;
private with System;

package AMS.MEL.IR.Image is
   --  Complete-frame image access and asynchronous navigation over the consumer-side MEL bridge.
   --  Owned frames and explicit pixel copies have independent Ada storage; the lease path borrows
   --  provider host memory under an explicit limited owner. Only the documented Mono8 single-band
   --  receive subset is supported, not optional upstream image processing or pixel-format
   --  conversion.

   use type Interfaces.Unsigned_8;
   type Full_Image_Type is (Staring, Scanning, Reserved13);
   --  Provider image collection category; reserved values are preserved, not interpreted.
   --  @enum Staring Staring image collection.
   --  @enum Scanning Scanning image collection.
   --  @enum Reserved13 Reserved upstream image category; no additional behavior is assigned.
   type Image_Flag is (Scan_First, Scan_Last, Stare_Snapshot, Stare_Rolling);
   --  Ordered scan/stare annotations supplied by the provider; duplicates are preserved.
   --  @enum Scan_First First image chunk in a scan.
   --  @enum Scan_Last Last image chunk in a scan.
   --  @enum Stare_Snapshot Snapshot capture of a staring image.
   --  @enum Stare_Rolling Rolling-shutter capture of a staring image.
   type Directional is record
      X, Y, Z : Long_Float;
   end record;
   --  Three provider-supplied directional components. Units and reference frame belong to the
   --  containing field; the bridge performs no transformation.
   --  @field X X-axis component in the containing field reference frame.
   --  @field Y Y-axis component in the containing field reference frame.
   --  @field Z Z-axis component in the containing field reference frame.
   type Quaternion is record
      X, Y, Z, W : Long_Float;
   end record;
   --  Provider quaternion in X, Y, Z, W component order. The bridge neither normalizes the value
   --  nor checks numerical validity.
   --  @field X X component of the quaternion.
   --  @field Y Y component of the quaternion.
   --  @field Z Z component of the quaternion.
   --  @field W W component of the quaternion.
   type Nav_Error is record
      X, Y, Z, W : Long_Float;
   end record;
   --  Provider error values for a vector, Euler orientation or quaternion. No statistical
   --  confidence, covariance construction or universal component units are established by the
   --  pinned interface.
   --  @field X First error component of the associated representation.
   --  @field Y Second error component of the associated representation.
   --  @field Z Third error component of the associated representation.
   --  @field W Fourth component, used upstream for quaternion error only; preserved without
   --  interpretation.
   type Uncertainty is record
      Sensor_Uncertainties, Platform_Uncertainties : Interfaces.Unsigned_32;
   end record;
   --  Raw sensor and platform navigation uncertainty values. The pinned interface defines neither
   --  units nor bit meanings; the bridge preserves both unsigned values.
   --  @field Sensor_Uncertainties Sensor-reported uncertainty value, not decoded by this bridge.
   --  @field Platform_Uncertainties Platform-reported uncertainty value, not decoded by this
   --  bridge.
   type Contributing_Sensor is record
      Location  : AMS.MEL.IR.Component_Location;
      Sensor_ID : Interfaces.Unsigned_32;
   end record;
   --  Identity and physical location of the sensor contributing this image.
   --  @field Location Sensor component location, including meter offsets and owned location
   --  identity strings.
   --  @field Sensor_ID Provider sensor identifier; all 32 bits are preserved.
   type Orientation_Kind is (Euler_Orientation, Quaternion_Orientation);
   --  Selects the stored upstream orientation alternative independently for each orientation or
   --  derivative.
   --  @enum Euler_Orientation Euler representation is active.
   --  @enum Quaternion_Orientation Quaternion representation is active.
   type Orientation (Kind : Orientation_Kind := Euler_Orientation) is record
      case Kind is
         when Euler_Orientation =>
            Euler_Value : AMS.MEL.Status.Euler;

         when Quaternion_Orientation =>
            Quaternion_Value : Quaternion;
      end case;
   end record;
   --  Discriminated copy of one upstream orientation alternative. No conversion between Euler and
   --  quaternion representations is performed.
   --  @field Kind Selects the active record component; defaults to the Euler alternative.
   --  @field Euler_Value Euler value when the Euler alternative is selected; see the canonical
   --  Status Euler type.
   --  @field Quaternion_Value X, Y, Z, W value when the quaternion alternative is selected; no
   --  normalization is applied.
   type Sensor_Inertial_State is record
      System_Time_NS                   : Long_Long_Integer;
      Q_XYZW, Q_ECEF_XYZW              : Quaternion;
      Sensor_Position, Sensor_Velocity : Directional;
      Uncertainty                      : Image.Uncertainty;
   end record;
   --  Provider inertial state associated with image collection. FrameHeader describes one state
   --  for non-TDI imagery and a per-column sequence for TDI; the bridge preserves the actual
   --  sequence without imposing those lengths.
   --  @field System_Time_NS Provider system time in nanoseconds; no clock or epoch is assigned by
   --  the bridge.
   --  @field Q_XYZW Quaternion from sensor coordinates to local-level North/East/Down.
   --  @field Q_ECEF_XYZW Quaternion from North/East/Down to Earth-centered Earth-fixed
   --  coordinates.
   --  @field Sensor_Position Sensor position in ECEF coordinates, in meters.
   --  @field Sensor_Velocity Sensor velocity in ECEF coordinates, in meters per second.
   --  @field Uncertainty Uninterpreted sensor and platform navigation uncertainty values.
   type Coordinate_System is (LLA, ECEF, NED_Platform, NED_Sensor);
   --  Provider identifier for the reference frame of a sensor navigation state. This identifies
   --  the stored data; it does not request a bridge coordinate transformation.
   --  @enum LLA Latitude/longitude/altitude coordinate-system identifier; per-component units are
   --  not specified by SensorNavState.
   --  @enum ECEF Earth-centered Earth-fixed reference frame.
   --  @enum NED_Platform Platform North/East/Down reference-frame identifier.
   --  @enum NED_Sensor Sensor North/East/Down reference-frame identifier.
   type Sensor_Nav_State is record
      Position, Velocity, Acceleration                   : Directional;
      Position_Error, Velocity_Error, Acceleration_Error : Nav_Error;
      Orientation                                        : Image.Orientation;
      Orientation_Error                                  : Nav_Error;
      Orientation_Velocity                               : Image.Orientation;
      Orientation_Velocity_Error                         : Nav_Error;
      Orientation_Acceleration                           : Image.Orientation;
      Orientation_Acceleration_Error                     : Nav_Error;
      Coordinate_System                                  : Image.Coordinate_System;
   end record;
   --  Copied internal sensor navigation state at collection time. Components and orientation
   --  alternatives are preserved independently. The pinned SensorNavState header does not
   --  establish universal component or derivative units; the bridge performs no interpolation,
   --  coordinate transformation or error estimation.
   --  @field Position Reported position in the identified coordinate system.
   --  @field Velocity Reported velocity in the identified coordinate system.
   --  @field Acceleration Reported acceleration in the identified coordinate system.
   --  @field Position_Error Provider position error components, without statistical
   --  interpretation.
   --  @field Velocity_Error Provider velocity error components, without statistical
   --  interpretation.
   --  @field Acceleration_Error Provider acceleration error components, without statistical
   --  interpretation.
   --  @field Orientation Reported orientation with its own representation discriminant.
   --  @field Orientation_Error Provider error components for the reported orientation.
   --  @field Orientation_Velocity Reported orientation derivative with its own representation
   --  discriminant.
   --  @field Orientation_Velocity_Error Provider error components for orientation velocity.
   --  @field Orientation_Acceleration Reported second orientation derivative with its own
   --  representation discriminant.
   --  @field Orientation_Acceleration_Error Provider error components for orientation
   --  acceleration.
   --  @field Coordinate_System Identifies the reference frame of this state; no conversion is
   --  performed.

   type Full_Frame is private;
   --  Ada-owned complete frame snapshot: copied header, ordered flags, inertial/navigation
   --  sequences and pixel bytes. Unlike the smaller legacy IR Frame, this retains
   --  contributing-sensor identity and detailed navigation metadata. Returned snapshots survive
   --  stream, Session and provider closure.
   function Capabilities
     (Object : AMS.MEL.IR.Image_Stream) return AMS.MEL.IR.Channel.Channel_Capability;
   --  Copies the common channel capability snapshot while the image stream is open. No returned
   --  value retains the stream. Native failures are mapped through the IR submission exception
   --  policy.
   --  @param Object Image stream whose capability is queried.
   --  @return Ada-owned capability snapshot.
   function As_Channel (Object : AMS.MEL.IR.Image_Stream) return AMS.MEL.IR.Channel.View;
   --  Creates a weak common Channel view of the open stream. Raises Provider Error if closed or
   --  view creation fails. The view neither keeps the stream alive nor stops or closes it; use
   --  common Channel support for inherited services.
   --  @param Object Open image stream to view.
   --  @return New weak common channel view.
   function Receive
     (Object : AMS.MEL.IR.Image_Stream; Timeout_Milliseconds : Natural := 0) return Full_Frame;
   --  Receives a complete frame by copying header metadata and the entire pixel payload into
   --  Ada-owned storage, then closing the temporary native snapshot before return. The result
   --  survives stream, Session and provider closure. Pixels makes another independent copy. This
   --  exposes more metadata than legacy IR Receive, but performs no pixel decoding or conversion.
   --  Only Mono8 single-band nonempty dimensions and validated host-buffer spans are accepted;
   --  malformed provider frames are discarded by the native callback. At most one receive/acquire
   --  consumer may use a stream at once. Externally serialize lifecycle operations on a shared Ada
   --  owner. Timeout does not stop the stream. The private snapshot cleanup helper ignores native
   --  release status; explicit lease Close is the reporting path for release failures.
   --  @param Object Image stream from which to dequeue; Start admits image callbacks.
   --  @param Timeout_Milliseconds Maximum queue wait in milliseconds; zero polls without waiting.
   --  @return Complete independently owned frame and row-major payload bytes.
   --  @exception Timeout_Error No frame became available within the wait.
   --  @exception Stream_Stopped Stream has logically stopped with no frame available.
   --  @exception Provider_Error Native/provider failure, closed stream, or invalid converted
   --  metadata/span.
   function System_Time_NS (Value : Full_Frame) return Long_Long_Integer;
   --  Image system time in nanoseconds: start of integration for staring imagery, first-line time
   --  for scanning imagery. No clock or epoch is imposed.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Image system time in nanoseconds: start of integration for staring imagery,
   --  first-line time for scanning imagery. No clock or epoch is imposed.
   function Integration_Time_NS (Value : Full_Frame) return Long_Long_Integer;
   --  Provider integration duration in nanoseconds.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Provider integration duration in nanoseconds.
   function Width (Value : Full_Frame) return Interfaces.Unsigned_32;
   --  Image width in pixels.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Image width in pixels.
   function Height (Value : Full_Frame) return Interfaces.Unsigned_32;
   --  Image height in pixels.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Image height in pixels.
   function Bits_Per_Pixel (Value : Full_Frame) return Interfaces.Unsigned_32;
   --  Number of bits per pixel; currently accepted frames have eight.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Number of bits per pixel; currently accepted frames have eight.
   function Number_Of_Bands (Value : Full_Frame) return Interfaces.Unsigned_32;
   --  Raw FrameHeader number-of-bands value; current accepted frames require one. The upstream
   --  field prose also calls this a spectral-band indicator; no decoding is performed.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Raw FrameHeader number-of-bands value; current accepted frames require one. The
   --  upstream field prose also calls this a spectral-band indicator; no decoding is performed.
   function Horizontal_FOV_Rad (Value : Full_Frame) return Long_Float;
   --  Horizontal field of view in radians.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Horizontal field of view in radians.
   function Vertical_FOV_Rad (Value : Full_Frame) return Long_Float;
   --  Vertical field of view in radians.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Vertical field of view in radians.
   function Contributing_Sensor_Value (Value : Full_Frame) return Contributing_Sensor;
   --  Copied contributing-sensor identity and location.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Copied contributing-sensor identity and location.
   function Pixel_Format (Value : Full_Frame) return AMS.MEL.IR.Channel.Pixel_Format;
   --  Provider pixel format; the current receive path accepts Mono only and performs no decoding
   --  or conversion.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Provider pixel format; the current receive path accepts Mono only and performs no
   --  decoding or conversion.
   function Frame_ID (Value : Full_Frame) return Interfaces.Unsigned_32;
   --  Provider frame identifier, preserving all 32 bits.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Provider frame identifier, preserving all 32 bits.
   function Subframe_ID (Value : Full_Frame) return Interfaces.Unsigned_32;
   --  Provider subframe identifier, without bridge renumbering.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Provider subframe identifier, without bridge renumbering.
   function Subframe_Total (Value : Full_Frame) return Interfaces.Unsigned_32;
   --  Provider total number of subframes in a frame.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Provider total number of subframes in a frame.
   function Image_Type (Value : Full_Frame) return Full_Image_Type;
   --  Provider staring, scanning or reserved image category.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Provider staring, scanning or reserved image category.
   function Image_Flip (Value : Full_Frame) return AMS.MEL.IR.Image_Flip;
   --  Provider image-flip configuration; bytes are not flipped by this accessor.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Provider image-flip configuration; bytes are not flipped by this accessor.
   function Image_Flag_Count (Value : Full_Frame) return Natural;
   --  Actual number of copied image flags, including duplicates.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Actual number of copied image flags, including duplicates.
   function Image_Flag_At (Value : Full_Frame; Index : Positive) return Image_Flag;
   --  Image flag at the one-based index in provider order; duplicates are retained.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @param Index One-based sequence index; raises Constraint Error beyond the actual sequence
   --  length.
   --  @return Image flag at the one-based index in provider order; duplicates are retained.
   function Dither_Row (Value : Full_Frame) return Long_Float;
   --  Microscan row dither offset. A positive value moves an object formerly at row Y to Y minus
   --  this offset.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Microscan row dither offset. A positive value moves an object formerly at row Y to Y
   --  minus this offset.
   function Dither_Column (Value : Full_Frame) return Long_Float;
   --  Microscan column dither offset. A positive value moves an object formerly at column X to X
   --  minus this offset.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Microscan column dither offset. A positive value moves an object formerly at column
   --  X to X minus this offset.
   function Row_Offset (Value : Full_Frame) return Interfaces.Unsigned_32;
   --  Starting row offset of this frame in sensor coordinates, in rows.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Starting row offset of this frame in sensor coordinates, in rows.
   function Column_Offset (Value : Full_Frame) return Interfaces.Unsigned_32;
   --  Starting column offset of this frame in sensor coordinates, in columns.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Starting column offset of this frame in sensor coordinates, in columns.
   function Sensor_Inertial_State_Count (Value : Full_Frame) return Natural;
   --  Actual number of copied sensor inertial states.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Actual number of copied sensor inertial states.
   function Sensor_Inertial_State_At
     (Value : Full_Frame; Index : Positive) return Sensor_Inertial_State;
   --  Sensor inertial state at the one-based index in provider order.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @param Index One-based sequence index; raises Constraint Error beyond the actual sequence
   --  length.
   --  @return Sensor inertial state at the one-based index in provider order.
   function Sensor_Nav_State_Count (Value : Full_Frame) return Natural;
   --  Actual number of copied sensor navigation states.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Actual number of copied sensor navigation states.
   function Sensor_Nav_State_At (Value : Full_Frame; Index : Positive) return Sensor_Nav_State;
   --  Sensor navigation state at the one-based index in provider order.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @param Index One-based sequence index; raises Constraint Error beyond the actual sequence
   --  length.
   --  @return Sensor navigation state at the one-based index in provider order.
   function Band_Index (Value : Full_Frame) return AMS.MEL.IR.Byte;
   --  Provider byte-sized band-map index; interpreted using the channel capability image-band map.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Provider byte-sized band-map index; interpreted using the channel capability
   --  image-band map.
   function Pixel_Count (Value : Full_Frame) return Natural;
   --  Number of stored payload bytes, not a generic decoded pixel count. Current accepted Mono8
   --  frames contain width times height bytes.
   --  @param Value Ada-owned frame snapshot to inspect.
   --  @return Number of stored payload bytes, not a generic decoded pixel count. Current accepted
   --  Mono8 frames contain width times height bytes.
   function Pixels (Value : Full_Frame) return AMS.MEL.IR.Pixel_Array;
   --  Independent Ada-owned copy of the stored row-major payload bytes.
   --  @param Value Ada-owned frame snapshot to copy.
   --  @return Independent byte array indexed from one through the byte count.

   type Frame_Lease is limited private;
   --  Limited owner of one native snapshot retaining a provider-owned registered host buffer. The
   --  Frame Lease borrowed path performs no bulk image-payload copy from the MEL provider's
   --  registered host buffer into Ada. This is not end-to-end sensor zero-copy: pinned Squall
   --  copies UDP bytes into that host buffer; no DMA or kernel/network-stack zero-copy is claimed.
   --  Small metadata is copied at acquisition. A held lease keeps a provider buffer checked out;
   --  slow consumers cause backpressure or possible provider drops. Public stream/Session closure
   --  can be logical while physical teardown remains deferred behind leases, requests and release
   --  obligations. Closing the final child can trigger retained cleanup. Finalization is
   --  non-raising and can deliberately retain resources after uncertain release. Do not race
   --  lifecycle operations on a shared Ada object. Independent already-acquired leases may be
   --  processed separately; the stream still supports only one receive/acquire consumer.

   function Acquire_Frame
     (Object : AMS.MEL.IR.Image_Stream; Timeout_Milliseconds : Natural := 0) return Frame_Lease;
   --  Dequeues and owns one native snapshot, copying only small metadata and retaining the
   --  provider payload by address and length. Uses the same Mono8 single-band validated queue as
   --  owned Receive. At most one receive/acquire consumer may operate on a stream at once;
   --  independent acquired leases can be processed separately. Stream Stop/Close stops admission
   --  and can defer physical cleanup without invalidating held leases. Provider callbacks and
   --  navigation completion are synchronized internally, not permission for arbitrary concurrent
   --  operations on one Ada owner.
   --  @param Object Image stream to consume; Start admits image callbacks.
   --  @param Timeout_Milliseconds Queue wait in milliseconds; zero is a nonblocking poll.
   --  @return Limited owner of one checked-out provider buffer and copied metadata.
   --  @exception Timeout_Error No frame is available within the wait; does not stop the stream.
   --  @exception Stream_Stopped Stream is logically stopped and no frame is available.
   --  @exception Provider_Error Closed/failed stream, invalid converted metadata or payload span,
   --  or native failure.
   function Is_Open (Frame : Frame_Lease) return Boolean;
   --  Tests only whether this lease holds a native snapshot handle; does not query provider
   --  health.
   --  @param Frame Lease to inspect.
   --  @return True while the local snapshot handle is nonnull.

   procedure Close (Frame : in out Frame_Lease);
   --  Closes the lease and invalidates its borrowed payload, also on provider-release failure. An
   --  already-closed lease is a no-op. The retained buffer is returned through the published
   --  Buffer release operation when successful. Provider failure or exception makes ownership
   --  uncertain: the bridge deliberately retains the provider graph, exact buffer object and host
   --  memory to avoid unsafe reuse or unload. This is not successful buffer release; the failed
   --  release is not automatically retried. Explicit Close reports failure, whereas automatic
   --  finalization suppresses it and can retain resources for safety. Final-child closure may
   --  perform deferred channel cleanup. Do not race Close with a borrow, copy, or another
   --  operation on this same lease.
   --  @param Frame Lease consumed by this operation; its payload count becomes zero.
   --  @exception Provider_Error Native buffer release or deferred cleanup did not complete safely.

   function Pixel_Count (Frame : Frame_Lease) return Natural;
   --  Returns the borrowable payload byte count in constant time, not a generic decoded sample
   --  count. Explicit Close resets it to zero.
   --  @param Frame Lease whose stored byte count is inspected.
   --  @return Validated byte count captured at acquisition, or zero after explicit Close.

   procedure With_Pixels
     (Frame : Frame_Lease; Process : not null access procedure (Pixels : AMS.MEL.IR.Pixel_Array))
   with Pre => Is_Open (Frame);
   --  Synchronously invokes Process with a read-only-by-contract array aliasing native/provider
   --  storage, without a bulk payload copy. Array bounds are one through the byte count (one
   --  through zero if empty). The borrow ends when Process returns or raises: do not retain an
   --  address, slice, view or access object beyond that invocation. Keep the owning lease open
   --  throughout processing and do not race its Close. An exception from Process propagates
   --  without consuming the lease. Requires an open lease; enabled precondition checking raises
   --  Assertion Error on violation, while the body separately raises Provider Error for a closed
   --  handle or invalid payload address. No general thread safety or callback-quiescence guarantee
   --  is implied.
   --  @param Frame Open owning lease which must remain open throughout Process.
   --  @param Process Synchronous read-only processor; must not retain any reference to borrowed
   --  storage.

   function Copy_Pixels (Frame : Frame_Lease) return AMS.MEL.IR.Pixel_Array
   with Pre => Is_Open (Frame);
   --  Allocates independent Ada storage and copies exactly the lease payload byte count. Bounds
   --  are one through the payload byte count, including the empty one-through-zero case. The
   --  result survives lease, stream and Session closure. Requires an open lease and borrows internally through
   --  the synchronous pixel borrow; its closed-handle and invalid-address failure rules also apply.
   --  @param Frame Open lease whose bytes are copied; do not race Close.
   --  @return Independent row-major payload-byte array, with no decoding or conversion.

   function System_Time_NS (Frame : Frame_Lease) return Long_Long_Integer;
   --  Image system time in nanoseconds: start of integration for staring imagery, first-line time
   --  for scanning imagery. No clock or epoch is imposed. Reads the metadata copy made at
   --  acquisition, without dereferencing provider storage. Explicit lease Close leaves these
   --  metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Image system time in nanoseconds: start of integration for staring imagery,
   --  first-line time for scanning imagery. No clock or epoch is imposed.
   function Integration_Time_NS (Frame : Frame_Lease) return Long_Long_Integer;
   --  Provider integration duration in nanoseconds. Reads the metadata copy made at acquisition,
   --  without dereferencing provider storage. Explicit lease Close leaves these metadata fields
   --  intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Provider integration duration in nanoseconds.
   function Width (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   --  Image width in pixels. Reads the metadata copy made at acquisition, without dereferencing
   --  provider storage. Explicit lease Close leaves these metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Image width in pixels.
   function Height (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   --  Image height in pixels. Reads the metadata copy made at acquisition, without dereferencing
   --  provider storage. Explicit lease Close leaves these metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Image height in pixels.
   function Bits_Per_Pixel (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   --  Number of bits per pixel; currently accepted frames have eight. Reads the metadata copy made
   --  at acquisition, without dereferencing provider storage. Explicit lease Close leaves these
   --  metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Number of bits per pixel; currently accepted frames have eight.
   function Number_Of_Bands (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   --  Raw FrameHeader number-of-bands value; current accepted frames require one. The upstream
   --  field prose also calls this a spectral-band indicator; no decoding is performed. Reads the
   --  metadata copy made at acquisition, without dereferencing provider storage. Explicit lease
   --  Close leaves these metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Raw FrameHeader number-of-bands value; current accepted frames require one. The
   --  upstream field prose also calls this a spectral-band indicator; no decoding is performed.
   function Horizontal_FOV_Rad (Frame : Frame_Lease) return Long_Float;
   --  Horizontal field of view in radians. Reads the metadata copy made at acquisition, without
   --  dereferencing provider storage. Explicit lease Close leaves these metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Horizontal field of view in radians.
   function Vertical_FOV_Rad (Frame : Frame_Lease) return Long_Float;
   --  Vertical field of view in radians. Reads the metadata copy made at acquisition, without
   --  dereferencing provider storage. Explicit lease Close leaves these metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Vertical field of view in radians.
   function Contributing_Sensor_Value (Frame : Frame_Lease) return Contributing_Sensor;
   --  Copied contributing-sensor identity and location. Reads the metadata copy made at
   --  acquisition, without dereferencing provider storage. Explicit lease Close leaves these
   --  metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Copied contributing-sensor identity and location.
   function Pixel_Format (Frame : Frame_Lease) return AMS.MEL.IR.Channel.Pixel_Format;
   --  Provider pixel format; the current receive path accepts Mono only and performs no decoding
   --  or conversion. Reads the metadata copy made at acquisition, without dereferencing provider
   --  storage. Explicit lease Close leaves these metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Provider pixel format; the current receive path accepts Mono only and performs no
   --  decoding or conversion.
   function Frame_ID (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   --  Provider frame identifier, preserving all 32 bits. Reads the metadata copy made at
   --  acquisition, without dereferencing provider storage. Explicit lease Close leaves these
   --  metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Provider frame identifier, preserving all 32 bits.
   function Subframe_ID (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   --  Provider subframe identifier, without bridge renumbering. Reads the metadata copy made at
   --  acquisition, without dereferencing provider storage. Explicit lease Close leaves these
   --  metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Provider subframe identifier, without bridge renumbering.
   function Subframe_Total (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   --  Provider total number of subframes in a frame. Reads the metadata copy made at acquisition,
   --  without dereferencing provider storage. Explicit lease Close leaves these metadata fields
   --  intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Provider total number of subframes in a frame.
   function Image_Type (Frame : Frame_Lease) return Full_Image_Type;
   --  Provider staring, scanning or reserved image category. Reads the metadata copy made at
   --  acquisition, without dereferencing provider storage. Explicit lease Close leaves these
   --  metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Provider staring, scanning or reserved image category.
   function Image_Flip (Frame : Frame_Lease) return AMS.MEL.IR.Image_Flip;
   --  Provider image-flip configuration; bytes are not flipped by this accessor. Reads the
   --  metadata copy made at acquisition, without dereferencing provider storage. Explicit lease
   --  Close leaves these metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Provider image-flip configuration; bytes are not flipped by this accessor.
   function Image_Flag_Count (Frame : Frame_Lease) return Natural;
   --  Actual number of copied image flags, including duplicates. Reads the metadata copy made at
   --  acquisition, without dereferencing provider storage. Explicit lease Close leaves these
   --  metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Actual number of copied image flags, including duplicates.
   function Image_Flag_At (Frame : Frame_Lease; Index : Positive) return Image_Flag;
   --  Image flag at the one-based index in provider order; duplicates are retained. Reads the
   --  metadata copy made at acquisition, without dereferencing provider storage. Explicit lease
   --  Close leaves these metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @param Index One-based sequence position; raises Constraint Error beyond the copied length.
   --  @return Image flag at the one-based index in provider order; duplicates are retained.
   function Dither_Row (Frame : Frame_Lease) return Long_Float;
   --  Microscan row dither offset. A positive value moves an object formerly at row Y to Y minus
   --  this offset. Reads the metadata copy made at acquisition, without dereferencing provider
   --  storage. Explicit lease Close leaves these metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Microscan row dither offset. A positive value moves an object formerly at row Y to Y
   --  minus this offset.
   function Dither_Column (Frame : Frame_Lease) return Long_Float;
   --  Microscan column dither offset. A positive value moves an object formerly at column X to X
   --  minus this offset. Reads the metadata copy made at acquisition, without dereferencing
   --  provider storage. Explicit lease Close leaves these metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Microscan column dither offset. A positive value moves an object formerly at column
   --  X to X minus this offset.
   function Row_Offset (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   --  Starting row offset of this frame in sensor coordinates, in rows. Reads the metadata copy
   --  made at acquisition, without dereferencing provider storage. Explicit lease Close leaves
   --  these metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Starting row offset of this frame in sensor coordinates, in rows.
   function Column_Offset (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   --  Starting column offset of this frame in sensor coordinates, in columns. Reads the metadata
   --  copy made at acquisition, without dereferencing provider storage. Explicit lease Close
   --  leaves these metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Starting column offset of this frame in sensor coordinates, in columns.
   function Sensor_Inertial_State_Count (Frame : Frame_Lease) return Natural;
   --  Actual number of copied sensor inertial states. Reads the metadata copy made at acquisition,
   --  without dereferencing provider storage. Explicit lease Close leaves these metadata fields
   --  intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Actual number of copied sensor inertial states.
   function Sensor_Inertial_State_At
     (Frame : Frame_Lease; Index : Positive) return Sensor_Inertial_State;
   --  Sensor inertial state at the one-based index in provider order. Reads the metadata copy made
   --  at acquisition, without dereferencing provider storage. Explicit lease Close leaves these
   --  metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @param Index One-based sequence position; raises Constraint Error beyond the copied length.
   --  @return Sensor inertial state at the one-based index in provider order.
   function Sensor_Nav_State_Count (Frame : Frame_Lease) return Natural;
   --  Actual number of copied sensor navigation states. Reads the metadata copy made at
   --  acquisition, without dereferencing provider storage. Explicit lease Close leaves these
   --  metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Actual number of copied sensor navigation states.
   function Sensor_Nav_State_At (Frame : Frame_Lease; Index : Positive) return Sensor_Nav_State;
   --  Sensor navigation state at the one-based index in provider order. Reads the metadata copy
   --  made at acquisition, without dereferencing provider storage. Explicit lease Close leaves
   --  these metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @param Index One-based sequence position; raises Constraint Error beyond the copied length.
   --  @return Sensor navigation state at the one-based index in provider order.
   function Band_Index (Frame : Frame_Lease) return AMS.MEL.IR.Byte;
   --  Provider byte-sized band-map index; interpreted using the channel capability image-band map.
   --  Reads the metadata copy made at acquisition, without dereferencing provider storage.
   --  Explicit lease Close leaves these metadata fields intact.
   --  @param Frame Lease whose copied metadata is inspected.
   --  @return Provider byte-sized band-map index; interpreted using the channel capability
   --  image-band map.

   type Position_Solution_State is (Not_Set, Aligning, Free_Inertial, GPS, Blended);
   --  Aircraft position-solution state supplied in a navigation report. The upstream sentinel is
   --  not a safe Ada value; submission does not compute a position solution.
   --  @enum Not_Set Position-solution enumeration has not been set.
   --  @enum Aligning Reported aligning solution state; alignment criteria are provider-defined.
   --  @enum Free_Inertial Reported free-inertial solution state.
   --  @enum GPS Reported GPS solution state.
   --  @enum Blended Reported blended solution state; blending is not performed by this bridge.
   for Position_Solution_State use
     (Not_Set => 0, Aligning => 1, Free_Inertial => 2, GPS => 3, Blended => 4);

   type North_East_Down is record
      North, East, Down : Long_Float;
   end record;
   --  North/East/Down components whose units belong to the containing navigation-report field.
   --  @field North North component in the containing field units.
   --  @field East East component in the containing field units.
   --  @field Down Down component in the containing field units.

   type Attitude_Rate is record
      Value          : AMS.MEL.Status.Euler;
      System_Time_NS : Long_Long_Integer;
   end record;
   --  Euler attitude-rate values and their associated provider timestamp. The pinned header
   --  supplies an Euler representation but does not explicitly define rate units or a timestamp
   --  epoch.
   --  @field Value Euler components representing attitude rate, passed unchanged to the provider.
   --  @field System_Time_NS Associated attitude-rate time in nanoseconds; clock and epoch are
   --  unspecified.

   type Position_Velocity_Covariance is record
      Position_Position_Pn_Pn : Long_Float;
      Position_Position_Pn_Pe : Long_Float;
      Position_Position_Pn_Pd : Long_Float;
      Position_Position_Pe_Pe : Long_Float;
      Position_Position_Pe_Pd : Long_Float;
      Position_Position_Pd_Pd : Long_Float;
      Position_Velocity_Pn_Vn : Long_Float;
      Position_Velocity_Pn_Ve : Long_Float;
      Position_Velocity_Pn_Vd : Long_Float;
      Position_Velocity_Pe_Ve : Long_Float;
      Position_Velocity_Pe_Vd : Long_Float;
      Position_Velocity_Pd_Vd : Long_Float;
      Velocity_Velocity_Vn_Vn : Long_Float;
      Velocity_Velocity_Vn_Ve : Long_Float;
      Velocity_Velocity_Vn_Vd : Long_Float;
      Velocity_Velocity_Ve_Ve : Long_Float;
      Velocity_Velocity_Ve_Vd : Long_Float;
      Velocity_Velocity_Vd_Vd : Long_Float;
   end record;
   --  All 18 distinctly named upstream covariance terms, passed individually without bridge
   --  estimation, completion of a matrix or numerical validation. P denotes position and V
   --  velocity; n, e and d identify North, East and Down. The pinned covariance declaration does
   --  not explicitly state units.
   --  @field Position_Position_Pn_Pn Covariance term for north position and north position,
   --  preserving this distinct upstream entry.
   --  @field Position_Position_Pn_Pe Covariance term for north position and east position,
   --  preserving this distinct upstream entry.
   --  @field Position_Position_Pn_Pd Covariance term for north position and down position,
   --  preserving this distinct upstream entry.
   --  @field Position_Position_Pe_Pe Covariance term for east position and east position,
   --  preserving this distinct upstream entry.
   --  @field Position_Position_Pe_Pd Covariance term for east position and down position,
   --  preserving this distinct upstream entry.
   --  @field Position_Position_Pd_Pd Covariance term for down position and down position,
   --  preserving this distinct upstream entry.
   --  @field Position_Velocity_Pn_Vn Covariance term for north position and north velocity,
   --  preserving this distinct upstream entry.
   --  @field Position_Velocity_Pn_Ve Covariance term for north position and east velocity,
   --  preserving this distinct upstream entry.
   --  @field Position_Velocity_Pn_Vd Covariance term for north position and down velocity,
   --  preserving this distinct upstream entry.
   --  @field Position_Velocity_Pe_Ve Covariance term for east position and east velocity,
   --  preserving this distinct upstream entry.
   --  @field Position_Velocity_Pe_Vd Covariance term for east position and down velocity,
   --  preserving this distinct upstream entry.
   --  @field Position_Velocity_Pd_Vd Covariance term for down position and down velocity,
   --  preserving this distinct upstream entry.
   --  @field Velocity_Velocity_Vn_Vn Covariance term for north velocity and north velocity,
   --  preserving this distinct upstream entry.
   --  @field Velocity_Velocity_Vn_Ve Covariance term for north velocity and east velocity,
   --  preserving this distinct upstream entry.
   --  @field Velocity_Velocity_Vn_Vd Covariance term for north velocity and down velocity,
   --  preserving this distinct upstream entry.
   --  @field Velocity_Velocity_Ve_Ve Covariance term for east velocity and east velocity,
   --  preserving this distinct upstream entry.
   --  @field Velocity_Velocity_Ve_Vd Covariance term for east velocity and down velocity,
   --  preserving this distinct upstream entry.
   --  @field Velocity_Velocity_Vd_Vd Covariance term for down velocity and down velocity,
   --  preserving this distinct upstream entry.

   type Navigation_Report is record
      System_Time_NS               : Long_Long_Integer;
      State                        : Position_Solution_State;
      Latitude_Rad                 : Long_Float;
      Longitude_Rad                : Long_Float;
      Altitude_M                   : Long_Float;
      Attitude                     : AMS.MEL.Status.Euler;
      Attitude_Rate                : Image.Attitude_Rate;
      Speed                        : North_East_Down;
      Acceleration                 : North_East_Down;
      Wander_Angle_Rad             : Long_Float;
      Magnetic_Heading             : Long_Float;
      Altitude_MSL                 : Long_Float;
      Position_Velocity_Covariance : Image.Position_Velocity_Covariance;
   end record;
   --  Complete supplied Common MEL navigation report. Submission transmits these values; it does
   --  not estimate, normalize or transform kinematics. Angle, altitude and velocity units below
   --  follow the pinned header only.
   --  @field System_Time_NS System time for the report in nanoseconds; no clock or epoch is
   --  specified.
   --  @field State Reported aircraft position-solution state.
   --  @field Latitude_Rad Latitude in radians.
   --  @field Longitude_Rad Longitude in radians.
   --  @field Altitude_M Altitude in meters; no additional datum is imposed by the bridge.
   --  @field Attitude Euler attitude; see the canonical Status Euler definition.
   --  @field Attitude_Rate Euler attitude rate and its separate timestamp; rate units are not
   --  explicitly specified upstream.
   --  @field Speed North/East/Down speed in meters per second.
   --  @field Acceleration North/East/Down acceleration in meters per second squared.
   --  @field Wander_Angle_Rad Wander angle in radians.
   --  @field Magnetic_Heading Provider magnetic heading; the pinned declaration specifies no unit.
   --  @field Altitude_MSL Upstream altitudeMSL value, described as barometric altitude; no
   --  explicit unit is stated for this field.
   --  @field Position_Velocity_Covariance All 18 provided covariance values; the bridge does not
   --  estimate them.

   type Navigation_Response is record
      System_Time_NS : Long_Long_Integer;
      Command_ID     : Interfaces.Unsigned_32;
      Request_ID     : Interfaces.Unsigned_32;
   end record;
   --  Owned copy of the upstream navigation acknowledgement, shared by request results and the
   --  metadata response subtype.
   --  @field System_Time_NS Provider response system time in nanoseconds; no clock or epoch is
   --  specified.
   --  @field Command_ID Provider command identifier, preserving all 32 bits.
   --  @field Request_ID Provider request identifier, preserving all 32 bits; no identity is
   --  generated by this accessor.

   type Navigation_Outcome is (Success, Rejected);
   --  Terminal provider response category, distinct from transport/provider exceptions.
   --  @enum Success Provider completed with a nonnull navigation response.
   --  @enum Rejected Provider completed with an explicit MEL error; inspect rejection code and
   --  description.
   type Navigation_Error_Code is
     (None,
      Invalid_ID,
      Invalid_State,
      Invalid_Parameters,
      Insufficient_Permissions,
      Insufficient_Resources,
      Insufficient_Local_Resources,
      Insufficient_Remote_Resources,
      Unsupported);
   --  Explicit Common MEL rejection reason. The bridge maps known enumerators, not a numeric
   --  severity scale; unknown provider codes fail closed.
   --  @enum None No specific error reason supplied.
   --  @enum Invalid_ID Provider reports an invalid identifier.
   --  @enum Invalid_State Provider reports an invalid state.
   --  @enum Invalid_Parameters Provider reports invalid parameters.
   --  @enum Insufficient_Permissions Provider reports insufficient permissions.
   --  @enum Insufficient_Resources Provider reports insufficient resources without a local/remote
   --  distinction.
   --  @enum Insufficient_Local_Resources Provider reports insufficient local resources.
   --  @enum Insufficient_Remote_Resources Provider reports insufficient remote resources.
   --  @enum Unsupported Provider reports the request is unsupported.
   type Navigation_Result is private;
   --  Ada-owned terminal navigation response or explicit rejection. Provider exceptions, null
   --  successful responses, unknown error codes and native failures raise Provider Error rather
   --  than becoming Rejected. Values survive request closure.
   function Status (Result : Navigation_Result) return Navigation_Outcome;
   --  Returns the terminal response category stored in this result.
   --  @param Result Owned terminal navigation result.
   --  @return Success or explicit provider rejection.
   function Response (Result : Navigation_Result) return Navigation_Response
   with Pre => Status (Result) = Success;
   --  Reads the successful response from an owned result. Requires Status equal to Success;
   --  enabled precondition checks raise Assertion Error on violation. The accessor body does not
   --  add an outcome check.
   --  @param Result Terminal result with the required outcome.
   --  @return Copied navigation response.
   function Rejection_Code (Result : Navigation_Result) return Navigation_Error_Code
   with Pre => Status (Result) = Rejected;
   --  Reads the rejection detail from an owned result. Requires Status equal to Rejected; enabled
   --  precondition checks raise Assertion Error on violation. The accessor body does not add an
   --  outcome check.
   --  @param Result Terminal result with the required outcome.
   --  @return Mapped provider rejection reason.
   function Description (Result : Navigation_Result) return String
   with Pre => Status (Result) = Rejected;
   --  Reads the rejection detail from an owned result. Requires Status equal to Rejected; enabled
   --  precondition checks raise Assertion Error on violation. The accessor body does not add an
   --  outcome check.
   --  @param Result Terminal result with the required outcome.
   --  @return Independent Ada string containing the full rejection description.

   type Navigation_Request is limited private;
   --  Limited owner of a public asynchronous navigation request handle. A private completion
   --  worker consumes the provider future once and caches its terminal result. Pending work
   --  retains the underlying stream, Session and provider code after public Close. A future that
   --  never completes can retain those resources indefinitely. Finalization closes the public
   --  handle without raising; neither it nor explicit Close cancels provider work.
   function Submit_Navigation_Report
     (Object : AMS.MEL.IR.Image_Stream; Report : Navigation_Report) return Navigation_Request;
   --  Submits the supplied report while the stream is logically Attached or Running; Start is not
   --  required. Synchronously copies the report into the provider representation and returns a
   --  request, not proof of completion or acceptance. Provider send executes outside the frame
   --  callback lock so synchronous metadata callbacks can proceed. Application operations on the
   --  same stream owner require external serialization. Pending navigation requests and image
   --  leases independently delay physical cleanup after logical Stop/Close; corrected native
   --  teardown synchronization adopts actual cleanup outcomes.
   --  @param Object Attached or running image stream to which the report is sent.
   --  @param Report Provided navigation solution and all detailed kinematics; no solution is
   --  computed here.
   --  @return Open limited request handle; use Wait for the terminal response.
   --  @exception Provider_Error Invalid stream lifecycle, provider send failure or native
   --  submission failure.
   --  @exception Resource_Exhausted Shared asynchronous-request admission limit reached.
   function Is_Open (Request : Navigation_Request) return Boolean;
   --  Tests local navigation request handle ownership, not future readiness.
   --  @param Request Request owner to inspect.
   --  @return True while this public request handle is nonnull.
   function Wait
     (Request : Navigation_Request; Timeout_Milliseconds : Natural) return Navigation_Result;
   --  Waits for a cached terminal result without consuming the request. Timeout is not
   --  cancellation; later waits may obtain completion and repeated terminal waits return the
   --  cached result. Requires an open request in practice; closed handles and provider/native
   --  failures raise Provider Error. Public request Close must not race Wait. Cleanup failure
   --  after provider completion is reported as a failure rather than a false successful response.
   --  Rejection text is copied completely using a checked second cached read when needed; invalid
   --  UTF-8 or embedded NUL receives a native fallback description.
   --  @param Request Open navigation request to observe.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   --  @return Owned successful response or explicit provider rejection.
   --  @exception Timeout_Error Request has not completed within the wait; remains open and
   --  pending.
   --  @exception Provider_Error Closed request, provider exception, invalid result, or
   --  native/deferred-cleanup failure.
   procedure Close (Request : in out Navigation_Request);
   --  Idempotently closes the public request handle without cancelling provider work. Pending
   --  completion independently retains the native graph, possibly beyond stream and Session
   --  closure. Finalization applies the same public-handle cleanup without raising. The Ada
   --  wrapper ignores native close status. Do not race this operation with Wait on the same
   --  request.
   --  @param Request Public request owner to close.
private
   package Flag_Vectors is new Ada.Containers.Vectors (Positive, Image_Flag);
   package Inertial_Vectors is new Ada.Containers.Vectors (Positive, Sensor_Inertial_State);
   package Nav_Vectors is new Ada.Containers.Vectors (Positive, Sensor_Nav_State);
   package Pixel_Vectors is new Ada.Containers.Vectors (Positive, AMS.MEL.IR.Byte);
   type Full_Frame is record
      Time, Integration     : Long_Long_Integer;
      W, H, BPP, Bands      : Interfaces.Unsigned_32;
      HFOV, VFOV            : Long_Float;
      Sensor                : Contributing_Sensor;
      Format                : AMS.MEL.IR.Channel.Pixel_Format;
      ID, Sub_ID, Sub_Total : Interfaces.Unsigned_32;
      Kind                  : Full_Image_Type;
      Flip                  : AMS.MEL.IR.Image_Flip;
      Flags                 : Flag_Vectors.Vector;
      D_Row, D_Column       : Long_Float;
      Row, Column           : Interfaces.Unsigned_32;
      Inertial              : Inertial_Vectors.Vector;
      Nav                   : Nav_Vectors.Vector;
      Band                  : AMS.MEL.IR.Byte;
      Data                  : Pixel_Vectors.Vector;
   end record;

   --  Small metadata copied at acquisition; the bulk payload is not copied.
   type Lease_Metadata is record
      Time, Integration     : Long_Long_Integer := 0;
      W, H, BPP, Bands      : Interfaces.Unsigned_32 := 0;
      HFOV, VFOV            : Long_Float := 0.0;
      Sensor                : Contributing_Sensor;
      Format                : AMS.MEL.IR.Channel.Pixel_Format := AMS.MEL.IR.Channel.Mono;
      ID, Sub_ID, Sub_Total : Interfaces.Unsigned_32 := 0;
      Kind                  : Full_Image_Type := Staring;
      Flip                  : AMS.MEL.IR.Image_Flip := AMS.MEL.IR.No_Flip;
      Flags                 : Flag_Vectors.Vector;
      D_Row, D_Column       : Long_Float := 0.0;
      Row, Column           : Interfaces.Unsigned_32 := 0;
      Inertial              : Inertial_Vectors.Vector;
      Nav                   : Nav_Vectors.Vector;
      Band                  : AMS.MEL.IR.Byte := 0;
      Payload               : System.Address := System.Null_Address;
      Payload_Size          : Natural := 0;
   end record;

   --  The limited owner of exactly one native frame snapshot.
   type Frame_Lease_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Frame_Snapshot_Handle := AMS.MEL_C_API.Null_Frame_Snapshot;
      Data   : Lease_Metadata;
   end record;
   overriding
   procedure Finalize (Frame : in out Frame_Lease_Owner);
   type Frame_Lease is limited record
      Owner : Frame_Lease_Owner;
   end record;

   package US renames Ada.Strings.Unbounded;
   type Navigation_Result is record
      Result_Status   : Navigation_Outcome := Success;
      Result_Response : Navigation_Response := (0, 0, 0);
      Result_Code     : Navigation_Error_Code := None;
      Result_Text     : US.Unbounded_String;
   end record;
   type Navigation_Request_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Navigation_Request_Handle :=
        AMS.MEL_C_API.Null_Navigation_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Navigation_Request_Owner);
   type Navigation_Request is limited record
      Owner : Navigation_Request_Owner;
   end record;
end AMS.MEL.IR.Image;
