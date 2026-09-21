with Ada.Containers.Vectors;
private with Ada.Finalization;
private with Ada.Strings.Unbounded;
with AMS.MEL.IR.Channel;
with AMS.MEL.Status;
with Interfaces;
private with AMS.MEL_C_API;
private with System;

package AMS.MEL.IR.Image is
   use type Interfaces.Unsigned_8;
   type Full_Image_Type is (Staring, Scanning, Reserved13);
   type Image_Flag is (Scan_First, Scan_Last, Stare_Snapshot, Stare_Rolling);
   type Directional is record
      X, Y, Z : Long_Float;
   end record;
   type Quaternion is record
      X, Y, Z, W : Long_Float;
   end record;
   type Nav_Error is record
      X, Y, Z, W : Long_Float;
   end record;
   type Uncertainty is record
      Sensor_Uncertainties, Platform_Uncertainties : Interfaces.Unsigned_32;
   end record;
   type Contributing_Sensor is record
      Location  : AMS.MEL.IR.Component_Location;
      Sensor_ID : Interfaces.Unsigned_32;
   end record;
   type Orientation_Kind is (Euler_Orientation, Quaternion_Orientation);
   type Orientation (Kind : Orientation_Kind := Euler_Orientation) is record
      case Kind is
         when Euler_Orientation =>
            Euler_Value : AMS.MEL.Status.Euler;

         when Quaternion_Orientation =>
            Quaternion_Value : Quaternion;
      end case;
   end record;
   type Sensor_Inertial_State is record
      System_Time_NS                   : Long_Long_Integer;
      Q_XYZW, Q_ECEF_XYZW              : Quaternion;
      Sensor_Position, Sensor_Velocity : Directional;
      Uncertainty                      : Image.Uncertainty;
   end record;
   type Coordinate_System is (LLA, ECEF, NED_Platform, NED_Sensor);
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

   type Full_Frame is private;
   function Capabilities
     (Object : AMS.MEL.IR.Image_Stream) return AMS.MEL.IR.Channel.Channel_Capability;
   --  Owned-copy compatibility path. Receive copies the complete native frame
   --  snapshot, including the whole pixel payload, into Ada-owned storage and
   --  releases the native snapshot before returning. The result therefore has
   --  no native lifetime dependency and outlives the stream, the Session, and
   --  provider unload. Use Acquire_Frame instead for the high-rate data plane.
   function Receive
     (Object : AMS.MEL.IR.Image_Stream; Timeout_Milliseconds : Natural := 0) return Full_Frame;
   function System_Time_NS (Value : Full_Frame) return Long_Long_Integer;
   function Integration_Time_NS (Value : Full_Frame) return Long_Long_Integer;
   function Width (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Height (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Bits_Per_Pixel (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Number_Of_Bands (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Horizontal_FOV_Rad (Value : Full_Frame) return Long_Float;
   function Vertical_FOV_Rad (Value : Full_Frame) return Long_Float;
   function Contributing_Sensor_Value (Value : Full_Frame) return Contributing_Sensor;
   function Pixel_Format (Value : Full_Frame) return AMS.MEL.IR.Channel.Pixel_Format;
   function Frame_ID (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Subframe_ID (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Subframe_Total (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Image_Type (Value : Full_Frame) return Full_Image_Type;
   function Image_Flip (Value : Full_Frame) return AMS.MEL.IR.Image_Flip;
   function Image_Flag_Count (Value : Full_Frame) return Natural;
   function Image_Flag_At (Value : Full_Frame; Index : Positive) return Image_Flag;
   function Dither_Row (Value : Full_Frame) return Long_Float;
   function Dither_Column (Value : Full_Frame) return Long_Float;
   function Row_Offset (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Column_Offset (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Sensor_Inertial_State_Count (Value : Full_Frame) return Natural;
   function Sensor_Inertial_State_At
     (Value : Full_Frame; Index : Positive) return Sensor_Inertial_State;
   function Sensor_Nav_State_Count (Value : Full_Frame) return Natural;
   function Sensor_Nav_State_At (Value : Full_Frame; Index : Positive) return Sensor_Nav_State;
   function Band_Index (Value : Full_Frame) return AMS.MEL.IR.Byte;
   function Pixel_Count (Value : Full_Frame) return Natural;
   --  Owned copy. The result is independent Ada storage.
   function Pixels (Value : Full_Frame) return AMS.MEL.IR.Pixel_Array;

   ---------------------------------------------------------------------------
   --  High-rate borrowed data plane (Task 030A)
   --
   --  This is the bulk-data ownership model the repository intends to reuse
   --  for other high-bandwidth interfaces (RF I/Q, real samples, VITA packet
   --  buffers, PDW buffers, waveform sources, Stacked Image):
   --
   --      native backing owner -> opaque C handle -> limited Ada owner
   --                           -> temporary borrowed Ada view
   --
   --  Frame_Lease is the IR-specific spelling of the limited Ada owner. One
   --  live lease owns exactly one native frame snapshot. The lease is limited,
   --  so it cannot be copied, and finalization closes the native snapshot
   --  exactly once. Close is idempotent and finalization after Close is
   --  harmless.
   --
   --  Naming rule: With_* borrows without a bulk copy; Copy_* produces
   --  independently owned Ada storage.
   --
   --  Task 030B: the bridge performs ZERO bulk payload copies from the MEL
   --  provider buffer into Ada. The borrowed view aliases the provider's own
   --  irmel::Buffer image memory:
   --
   --      Buffer::getImageAddress ()
   --          = native snapshot pixels.data
   --          = With_Pixels first-element address
   --
   --  This is a bridge claim only. Squall itself still receives UDP data and
   --  copies it into the registered MEL host buffer; that copy is outside
   --  this bridge and is not removed here.
   --
   --  Consequences a caller should understand:
   --
   --  * A live lease keeps one provider buffer checked out and therefore
   --    unavailable for provider reuse. With Buffer_Count = N, at most N
   --    provider buffers can be checked out at once unless the provider has
   --    another independent pool, so a slow consumer causes real
   --    provider-level backpressure or provider-side frame drops. That is
   --    intentional and is not hidden by silently copying. Use Receive or
   --    Copy_Pixels when independent ownership is preferred.
   --  * A live lease also defers physical provider teardown. Stop, Close and
   --    Session close all complete logically at once and never block on an
   --    application-held lease, but the provider channel and library are not
   --    actually released until the last lease is closed or finalized.
   type Frame_Lease is limited private;

   --  Dequeues one native frame snapshot and takes ownership of it. O(1) with
   --  respect to pixel count after dequeue: only small metadata is copied, and
   --  the pixel payload is retained by pointer and length. Raises
   --  Timeout_Error, Stream_Stopped, or Provider_Error exactly as Receive
   --  does. A failed acquisition leaks no native snapshot. At most one task
   --  may acquire or receive on a given Image_Stream at a time; leases already
   --  removed from the queue may be processed concurrently.
   function Acquire_Frame
     (Object : AMS.MEL.IR.Image_Stream; Timeout_Milliseconds : Natural := 0) return Frame_Lease;
   function Is_Open (Frame : Frame_Lease) return Boolean;

   --  Idempotent. Returns the retained provider buffer to the provider via
   --  the published irmel::Buffer::release () operation and releases the
   --  native snapshot; the borrowed pixel storage must not be used
   --  afterwards. Closing an already-closed lease succeeds and does nothing.
   --
   --  Since Task 030B this is a provider call and can therefore FAIL. When
   --  the provider refuses or throws on release, buffer ownership is
   --  uncertain, so Close raises Provider_Error rather than falsely claiming
   --  the buffer was safely returned, and the bridge deliberately retains the
   --  provider graph, the Buffer object and its host storage for the lifetime
   --  of the process instead of freeing memory whose ownership is unknown.
   --  That retention is an intentional leak on uncertain ownership, chosen
   --  over a possible use-after-free or use-after-unload.
   --
   --  Automatic finalization remains non-raising: a finalization that meets
   --  the same failure preserves memory safety and retains the graph, but
   --  reports nothing. Call Close explicitly when the outcome matters.
   procedure Close (Frame : in out Frame_Lease);

   --  Number of borrowable pixel bytes. O(1).
   function Pixel_Count (Frame : Frame_Lease) return Natural;

   --  Borrowed, read-only, O(1) view of the native snapshot payload.
   --
   --  The pixel array aliases storage owned by Frame_Lease and must not be
   --  retained beyond the dynamic extent of With_Pixels.
   --
   --  No payload-sized allocation, no per-byte loop, no container population,
   --  and no memcpy from the native snapshot occurs. An exception raised by
   --  Process propagates normally and leaves the lease valid. A null payload
   --  pointer with a nonzero size, or a size that cannot be represented as an
   --  Ada index, raises Provider_Error rather than touching memory.
   procedure With_Pixels
     (Frame : Frame_Lease; Process : not null access procedure (Pixels : AMS.MEL.IR.Pixel_Array))
   with Pre => Is_Open (Frame);

   --  Explicit owned copy: allocates and copies Pixel_Count (Frame) bytes into
   --  independent Ada storage that remains valid after the lease is closed or
   --  finalized. This is deliberately not named Pixels.
   function Copy_Pixels (Frame : Frame_Lease) return AMS.MEL.IR.Pixel_Array
   with Pre => Is_Open (Frame);

   --  Lease metadata. Small metadata is copied into Ada fields at acquisition,
   --  so every accessor stays valid for the whole lease lifetime and is
   --  unaffected by later frames or by native teardown.
   function System_Time_NS (Frame : Frame_Lease) return Long_Long_Integer;
   function Integration_Time_NS (Frame : Frame_Lease) return Long_Long_Integer;
   function Width (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   function Height (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   function Bits_Per_Pixel (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   function Number_Of_Bands (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   function Horizontal_FOV_Rad (Frame : Frame_Lease) return Long_Float;
   function Vertical_FOV_Rad (Frame : Frame_Lease) return Long_Float;
   function Contributing_Sensor_Value (Frame : Frame_Lease) return Contributing_Sensor;
   function Pixel_Format (Frame : Frame_Lease) return AMS.MEL.IR.Channel.Pixel_Format;
   function Frame_ID (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   function Subframe_ID (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   function Subframe_Total (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   function Image_Type (Frame : Frame_Lease) return Full_Image_Type;
   function Image_Flip (Frame : Frame_Lease) return AMS.MEL.IR.Image_Flip;
   function Image_Flag_Count (Frame : Frame_Lease) return Natural;
   function Image_Flag_At (Frame : Frame_Lease; Index : Positive) return Image_Flag;
   function Dither_Row (Frame : Frame_Lease) return Long_Float;
   function Dither_Column (Frame : Frame_Lease) return Long_Float;
   function Row_Offset (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   function Column_Offset (Frame : Frame_Lease) return Interfaces.Unsigned_32;
   function Sensor_Inertial_State_Count (Frame : Frame_Lease) return Natural;
   function Sensor_Inertial_State_At
     (Frame : Frame_Lease; Index : Positive) return Sensor_Inertial_State;
   function Sensor_Nav_State_Count (Frame : Frame_Lease) return Natural;
   function Sensor_Nav_State_At (Frame : Frame_Lease; Index : Positive) return Sensor_Nav_State;
   function Band_Index (Frame : Frame_Lease) return AMS.MEL.IR.Byte;

   --  Complete published mel::PositionSolutionState. MaxExclusive is not a
   --  valid safe value.
   type Position_Solution_State is (Not_Set, Aligning, Free_Inertial, GPS, Blended);
   for Position_Solution_State use
     (Not_Set => 0, Aligning => 1, Free_Inertial => 2, GPS => 3, Blended => 4);

   type North_East_Down is record
      North, East, Down : Long_Float;
   end record;

   type Attitude_Rate is record
      Value          : AMS.MEL.Status.Euler;
      System_Time_NS : Long_Long_Integer;
   end record;

   --  Complete published mel::PositionVelocityCovariance. All 18 terms are
   --  named explicitly to avoid matrix-order mistakes.
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

   --  Complete published mel::NavigationReport.
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

   --  Canonical safe Navigation_Response; see AMS.MEL.IR.Image.Metadata for
   --  the source-compatible subtype used by the metadata callback path.
   type Navigation_Response is record
      System_Time_NS : Long_Long_Integer;
      Command_ID     : Interfaces.Unsigned_32;
      Request_ID     : Interfaces.Unsigned_32;
   end record;

   type Navigation_Outcome is (Success, Rejected);
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
   type Navigation_Result is private;
   function Status (Result : Navigation_Result) return Navigation_Outcome;
   function Response (Result : Navigation_Result) return Navigation_Response
   with Pre => Status (Result) = Success;
   function Rejection_Code (Result : Navigation_Result) return Navigation_Error_Code
   with Pre => Status (Result) = Rejected;
   function Description (Result : Navigation_Result) return String
   with Pre => Status (Result) = Rejected;

   --  Navigation submission is valid while the stream is logically Attached
   --  or Running; a prior Start is not required. Close is not cancellation:
   --  a pending request keeps the underlying provider future/channel alive
   --  independently of the public Image_Stream and Session owners.
   type Navigation_Request is limited private;
   function Submit_Navigation_Report
     (Object : AMS.MEL.IR.Image_Stream; Report : Navigation_Report) return Navigation_Request;
   function Is_Open (Request : Navigation_Request) return Boolean;
   --  Timeout_Error is inherited from AMS.MEL.IR. Timeout never cancels or
   --  consumes the request. Wait may be repeated and a later Wait may return
   --  its cached terminal result. Close must not race Wait on the same
   --  Navigation_Request.
   function Wait
     (Request : Navigation_Request; Timeout_Milliseconds : Natural) return Navigation_Result;
   procedure Close (Request : in out Navigation_Request);
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
