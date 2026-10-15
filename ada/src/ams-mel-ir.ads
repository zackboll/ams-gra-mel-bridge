with Interfaces;
private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;

package AMS.MEL.IR is
   --  IR foundation values and the legacy copied-frame image stream. This
   --  stream supports host-memory, single-band, eight-bit monochrome images;
   --  it performs no decoding, flipping, resampling or coordinate transforms.
   --  The richer image child package has separate full-header and frame-lease
   --  APIs; their capabilities must not be inferred for this legacy Frame.

   subtype Byte is Interfaces.Unsigned_8;
   --  Unsigned eight-bit octet used for UUID bytes and monochrome pixel storage.

   type UUID is array (Natural range 0 .. 15) of Byte;
   --  Exactly sixteen identifier bytes indexed zero through fifteen. These are
   --  binary identity bytes, not a UTF-8 label or a textual UUID encoding.

   type Pixel_Array is array (Natural range <>) of Byte;
   --  Unconstrained array of octets. Legacy received Frames own a one-based
   --  array of monochrome samples; borrowed views in child APIs have separate
   --  lifetime contracts and must not be treated as owned merely by this type.

   type UCI_ID is private;
   --  Ada-owned UUID and descriptive label. Predefined Ada equality compares
   --  both fields, including the label; matching UUID bytes alone need not make
   --  two Ada values equal. Pinned upstream equality also compares both fields.

   function Create_UCI_ID (Value : UUID; Descriptive_Label : String) return UCI_ID;
   --  Copy binary identity and descriptive text without parsing a textual UUID.
   --  @param Value All sixteen UUID bytes, preserved in array order.
   --  @param Descriptive_Label Human-readable UTF-8 bytes copied without
   --  transcoding; this constructor does not validate UTF-8.
   --  @return Ada-owned identifier independent of the supplied label storage.
   --  @exception Constraint_Error The label contains an embedded NUL.

   function UUID_Value (ID : UCI_ID) return UUID;
   --  Copy an identifier's binary UUID independently of its descriptive label.
   --  @param ID Ada-owned identifier.
   --  @return All sixteen UUID bytes in their original order.

   function Descriptive_Label (ID : UCI_ID) return String;
   --  Copy the identifier's human-readable descriptive label.
   --  @param ID Ada-owned identifier.
   --  @return Independent Ada String of label bytes without transcoding.

   type Component_Location is private;
   --  Host-system forward/right/down offsets in meters and an external key for
   --  the logical mounting location. No coordinate transformation is performed.

   function Create_Component_Location
     (Offset_X_M  : Long_Float;
      Offset_Y_M  : Long_Float;
      Offset_Z_M  : Long_Float;
      Key         : String;
      System_Name : String) return Component_Location;
   --  Copy component offsets and a logical mounting reference.
   --  @param Offset_X_M Forward offset from the host navigational center, meters.
   --  @param Offset_Y_M Rightward offset from the host navigational center, meters.
   --  @param Offset_Z_M Downward offset from the host navigational center, meters.
   --  @param Key Pylon, bay or other logical mounting-location identifier.
   --  @param System_Name External naming system for that key.
   --  @return Ada-owned location with copied text and unchanged offset scalars.
   --  @exception Constraint_Error Key or naming-system text contains embedded NUL.

   function Offset_X_M (Location : Component_Location) return Long_Float;
   --  Read the component's forward offset relative to the host navigational center.
   --  @param Location Component installation location.
   --  @return Stored forward displacement in meters, without conversion.

   function Offset_Y_M (Location : Component_Location) return Long_Float;
   --  Read the component's rightward offset relative to the host navigational center.
   --  @param Location Component installation location.
   --  @return Stored rightward displacement in meters, without conversion.

   function Offset_Z_M (Location : Component_Location) return Long_Float;
   --  Read the component's downward offset relative to the host navigational center.
   --  @param Location Component installation location.
   --  @return Stored downward displacement in meters, without conversion.

   function Key (Location : Component_Location) return String;
   --  Copy the logical mounting-location key, not the numeric offsets.
   --  @param Location Component installation location.
   --  @return Independent identifier text for the pylon, bay or logical location.

   function System_Name (Location : Component_Location) return String;
   --  Copy the naming system for the logical mounting-location key.
   --  @param Location Component installation location.
   --  @return Independent naming-system text, without resolving the key.

   type Image_Config is private;
   --  Ada-owned image-channel configuration copied into native storage on open.
   --  Buffer sizes/count and receive-queue capacity are bridge resource choices,
   --  not image transformations or promises of provider throughput.

   function Create_Image_Config
     (Channel_ID      : UCI_ID;
      Platform_ID     : UCI_ID;
      Sensor_Location : Component_Location;
      Buffer_Count    : Positive := 3;
      Buffer_Size     : Positive := 1_048_576;
      Queue_Capacity  : Positive := 4) return Image_Config;
   --  Assemble image configuration without loading or attaching a provider.
   --  Native capability, text and resource checks occur when the stream opens.
   --  @param Channel_ID Requested image-channel identity and descriptive label.
   --  @param Platform_ID Host platform identity and descriptive label.
   --  @param Sensor_Location Sensor mounting offsets and external location key.
   --  @param Buffer_Count Positive number of host buffers to register.
   --  @param Buffer_Size Positive storage capacity in bytes for each buffer.
   --  @param Queue_Capacity Positive bound on queued frames awaiting consumption.
   --  @return Ada-owned configuration preserving all supplied values.

   type Image_Stream is limited private;
   --  Limited controlled owner of a legacy native image stream. Default state
   --  is closed. Explicit Close reports errors; finalization provides non-raising
   --  cleanup fallback. Native ownership retains provider/session state as needed
   --  independently of the parent Ada Session object's lifetime.

   function Open_Image_Stream (Parent : Session; Config : Image_Config) return Image_Stream;
   --  Attach an image channel using the advertised Mono8 single-band capability.
   --  Opening does not start image reception; call Start explicitly.
   --  @param Parent Open IR Session; serialize this call against parent closure.
   --  @param Config Channel/platform/location and bounded host-storage settings.
   --  @return Limited owner of an attached, not yet started, image stream.
   --  @exception Provider_Error Parent is closed or native validation, capability
   --  selection, acquisition or attachment fails, including invalid UTF-8 text.

   function Is_Open (Object : Image_Stream) return Boolean;
   --  Inspect local stream-handle presence without calling the provider.
   --  @param Object Legacy image stream owner.
   --  @return True if a native handle is held, not a guarantee of enabled or
   --  healthy provider reception.

   procedure Start (Object : in out Image_Stream);
   --  Initialize/register host buffers and enable the attached image channel.
   --  @param Object Open stream to start; serialize lifecycle operations on it.
   --  @exception Provider_Error Native registration, enable or state check fails.

   type Image_Type is (Staring, Scanning);
   --  Provider-reported acquisition kind, not a requested image transformation.
   --  @enum Staring Image acquired by a staring sensor.
   --  @enum Scanning Image acquired by a scanning sensor.

   type Image_Flip is (No_Flip, Vertical, Horizontal, Both);
   --  Reported image flip configuration; the bridge does not flip the pixels.
   --  @enum No_Flip No flip is indicated.
   --  @enum Vertical A vertical flip is indicated.
   --  @enum Horizontal A horizontal flip is indicated.
   --  @enum Both Both vertical and horizontal flips are indicated.
   type Frame (Pixel_Count : Natural) is record
      System_Time_NS      : Long_Long_Integer;
      Integration_Time_NS : Long_Long_Integer;
      Width               : Interfaces.Unsigned_32;
      Height              : Interfaces.Unsigned_32;
      Bits_Per_Pixel      : Interfaces.Unsigned_32;
      Number_Of_Bands     : Interfaces.Unsigned_32;
      Horizontal_FOV_Rad  : Long_Float;
      Vertical_FOV_Rad    : Long_Float;
      Frame_ID            : Interfaces.Unsigned_32;
      Subframe_ID         : Interfaces.Unsigned_32;
      Subframe_Total      : Interfaces.Unsigned_32;
      Kind                : Image_Type;
      Flip                : Image_Flip;
      Image_Flags         : Interfaces.Unsigned_32;
      Dither_Row          : Long_Float;
      Dither_Column       : Long_Float;
      Row_Offset          : Interfaces.Unsigned_32;
      Column_Offset       : Interfaces.Unsigned_32;
      Band_Index          : Byte;
      Pixels              : Pixel_Array (1 .. Pixel_Count);
   end record;
   --  Ada-owned legacy Mono8 frame and selected scalar header metadata. Pixels
   --  are copied into this value and survive stream/session closure. This record
   --  does not expose the full header's contributing sensor or navigation arrays.
   --  @field Pixel_Count Number of pixel octets owned by this frame; successful
   --  reception supplies width times height for single-band eight-bit data.
   --  @field System_Time_NS Provider image time in nanoseconds: integration
   --  start for staring images, first-line time for scanning/TDI images. No
   --  epoch conversion is performed by this wrapper.
   --  @field Integration_Time_NS Image integration duration in nanoseconds.
   --  @field Width Image width in pixels.
   --  @field Height Image height in pixels.
   --  @field Bits_Per_Pixel Eight for the supported legacy receive profile.
   --  @field Number_Of_Bands One for the supported legacy receive profile.
   --  @field Horizontal_FOV_Rad Horizontal field of view in radians.
   --  @field Vertical_FOV_Rad Vertical field of view in radians.
   --  @field Frame_ID Provider-assigned frame identifier.
   --  @field Subframe_ID Provider-assigned subframe identifier.
   --  @field Subframe_Total Total number of subframes in a frame, as reported.
   --  @field Kind Reported staring/scanning acquisition kind.
   --  @field Flip Reported flip configuration; pixels are not transformed.
   --  @field Image_Flags Bit set representing scan-first, scan-last,
   --  stare-snapshot and stare-rolling flags in native enum bit positions 0..3.
   --  @field Dither_Row Microscan row displacement: a positive value moves an
   --  object formerly at row Y to Y minus this displacement.
   --  @field Dither_Column Microscan column displacement: a positive value moves
   --  an object formerly at column X to X minus this displacement.
   --  @field Row_Offset Start-of-frame offset in rows in sensor coordinates.
   --  @field Column_Offset Start-of-frame offset in columns in sensor coordinates.
   --  @field Band_Index Provider band-map index identifying bands used for image.
   --  @field Pixels Owned one-based monochrome octets, with no decoding applied.

   Timeout_Error : exception;
   --  Receive found no queued frame within its wait interval; not cancellation.

   Stream_Stopped : exception;
   --  Receive found no queued frame and the native stream is stopped.

   function Receive (Object : Image_Stream; Timeout_Milliseconds : Natural := 0) return Frame;
   --  Receive a copied legacy frame using native size-discovery then copy calls.
   --  At most one task may call Receive on this stream at a time. Queued frames
   --  precede stopped/failure reporting. Serialize closure against this call.
   --  @param Object Open legacy image stream whose next frame is consumed.
   --  @param Timeout_Milliseconds Wait duration in milliseconds; zero polls
   --  without waiting. A timeout neither stops the stream nor cancels provider work.
   --  @return Ada-owned pixels and selected scalar metadata for the next frame.
   --  @exception Timeout_Error No frame becomes available within the wait.
   --  @exception Stream_Stopped No queued frame remains and the stream is stopped.
   --  @exception Provider_Error Closed/failed stream, size discovery failure or
   --  a frame outside this wrapper's Mono8 single-band metadata profile.

   type Counter is mod 2**64 with Size => 64;
   --  Unsigned 64-bit native stream diagnostic count.

   type Stream_Counters is record
      Frames_Received           : Counter;
      Frames_Dropped_Queue_Full : Counter;
      Malformed_Or_Unsupported  : Counter;
   end record;
   --  Snapshot of callback/rejection diagnostics, not successful Ada reads.
   --  @field Frames_Received Incoming image callbacks counted before validation,
   --  including malformed data and queue-full drops.
   --  @field Frames_Dropped_Queue_Full Valid incoming frames rejected because
   --  the bounded receive queue has no space.
   --  @field Malformed_Or_Unsupported Malformed/unsupported callbacks and native
   --  callback failures rejected by the bridge, not timeout polls.

   function Counters (Object : Image_Stream) return Stream_Counters;
   --  Copy current native stream counters without consuming a frame.
   --  @param Object Open legacy image stream.
   --  @return Independent diagnostic-count snapshot.
   --  @exception Provider_Error Native query fails, including a closed owner.

   procedure Stop (Object : in out Image_Stream);
   --  Stop acceptance and disable the stream with native cleanup/lifetime rules.
   --  Previously queued frames remain receivable before stopped reporting.
   --  This is not cancellation of every outstanding provider operation.
   --  @param Object Open stream to stop; serialize lifecycle operations.
   --  @exception Provider_Error Native disable or cleanup/state check fails.

   procedure Close (Object : in out Image_Stream);
   --  Release the public stream owner through native detach/cleanup. Repeated
   --  closure of a closed owner is harmless. Retained frames/requests or uncertain
   --  cleanup can retain the provider graph rather than unloading unsafe code.
   --  @param Object Stream to close; do not race closure with Receive or other
   --  operations on this Ada owner.
   --  @exception Provider_Error Native cleanup fails; explicit closure allows
   --  the caller to observe the failure instead of relying on finalization.

   --  Controlled owner of exactly one native weak common Channel view
   --  (ams_mel_ir_channel). Applications use it only through the subtype
   --  AMS.MEL.IR.Channel.View; its operations live in AMS.MEL.IR.Channel and
   --  the typed-family As_Channel conversions. It never owns the typed family
   --  state, Session, provider Channel, Control, or provider library.
   type Channel_View is limited private;
   --  Limited controlled owner of a weak common-channel view, used through the
   --  channel child package's View subtype. It owns only the view handle and
   --  never retains the typed channel, Session, provider objects or library.
   --  Keeping a view does not keep the resource it references open. Typed-family
   --  conversions and common operations enforce that resource's lifetime/state.

private
   package US renames Ada.Strings.Unbounded;
   type UCI_ID is record
      Value : UUID;
      Label : US.Unbounded_String;
   end record;
   type Component_Location is record
      X, Y, Z      : Long_Float;
      Key_Value    : US.Unbounded_String;
      System_Value : US.Unbounded_String;
   end record;
   type Image_Config is record
      Channel  : UCI_ID;
      Platform : UCI_ID;
      Location : Component_Location;
      Buffers  : Positive;
      Size     : Positive;
      Queue    : Positive;
   end record;
   type Image_Stream is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Stream_Handle := AMS.MEL_C_API.Null_Stream;
   end record;
   overriding
   procedure Finalize (Object : in out Image_Stream);

   --  Visible only to descendants of AMS.MEL.IR (their private parts and
   --  bodies), which lets each typed family initialize the native view from
   --  its own private handle without any public raw-handle accessor.
   type Channel_View is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Channel_Handle := AMS.MEL_C_API.Null_Channel;
   end record;
   overriding
   procedure Finalize (Object : in out Channel_View);
end AMS.MEL.IR;
