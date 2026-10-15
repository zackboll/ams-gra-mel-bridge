with Ada.Containers.Vectors;
private with Ada.Finalization;
with Interfaces;
with AMS.MEL.Status;
private with AMS.MEL_C_API;

package AMS.MEL.IR.Image.Metadata is
   --  Owned polling of four image metadata callback families: bad pixels, line-of-sight report,
   --  line-of-sight Euler and navigation response. Optional LineOfSightQuaternion,
   --  OpticalDistortionMap and other upstream callbacks are not bound here. Payloads are copied at
   --  the native callback boundary and again into safe Ada values; no provider pointer is exposed.

   type Metadata_Kind is
     (Bad_Pixel_List_Event,
      Line_Of_Sight_Report_Event,
      Line_Of_Sight_Euler_Event,
      Navigation_Response_Event);
   --  The four image callback families bound by this package; other optional upstream metadata
   --  callbacks are not exposed.
   --  @enum Bad_Pixel_List_Event Owned bad-pixel defect list.
   --  @enum Line_Of_Sight_Report_Event Owned sensor-relative pointing report.
   --  @enum Line_Of_Sight_Euler_Event Owned aperture line-of-sight Euler data.
   --  @enum Navigation_Response_Event Owned navigation acknowledgement; distinct from
   --  request-handle completion.
   type Bad_Pixel_Reason is (Unknown);
   --  Supported defect reason subset; the pinned enum currently defines only Unknown.
   --  @enum Unknown Provider does not identify a specific reason for this bad pixel.
   type Bad_Pixel is record
      Row    : Interfaces.Unsigned_32;
      Column : Interfaces.Unsigned_32;
      Reason : Bad_Pixel_Reason;
   end record;
   --  One copied provider-reported bad pixel; coordinates are retained without renumbering or
   --  defect correction.
   --  @field Row Reported pixel row coordinate; the pinned header does not establish a
   --  coordinate-origin convention.
   --  @field Column Reported pixel column coordinate, without bridge renumbering.
   --  @field Reason Reported supported defect reason; unknown enum values cause event rejection.
   type Bad_Pixel_List is private;
   --  Owned bad-pixel collection with separate source-reported size/count. Actual copied vector
   --  length is authoritative for indexed access; reported counts are not used to truncate the
   --  vector.
   type Azimuth_Elevation is record
      Azimuth_Rad, Elevation_Rad : Long_Float;
   end record;
   --  Azimuth/elevation representation based on pinned AMS Math AzEl. Angle components are
   --  radians; the containing rate field must be distinguished from angles.
   --  @field Azimuth_Rad Azimuth component; radians for an angle. The pointing-rate field has no
   --  explicitly documented time denominator upstream.
   --  @field Elevation_Rad Elevation component; radians for an angle. The pointing-rate field has
   --  no explicitly documented time denominator upstream.
   type Line_Of_Sight_Report is record
      System_Time_NS         : Long_Long_Integer;
      Pointing_Angle         : Azimuth_Elevation;
      Pointing_Angle_Rates   : Azimuth_Elevation;
      At_Speed               : Boolean;
      In_Tolerance           : Boolean;
      Platform_Attitude      : AMS.MEL.Status.Euler;
      Validity_Flag_Bitfield : Interfaces.Unsigned_32;
      Image_Rotation_Rad     : Long_Float;
   end record;
   --  Owned sensor pointing report copied without transformations. Validity bits are preserved but
   --  their positions are not defined by the pinned header.
   --  @field System_Time_NS Provider system time in nanoseconds; clock and epoch are unspecified.
   --  @field Pointing_Angle Azimuth/elevation angles relative to the sensor, in radians.
   --  @field Pointing_Angle_Rates Provider pointing-rate components represented by AzEl; the
   --  pinned report does not explicitly establish rate units.
   --  @field At_Speed Provider indication that the MFA is at scan speed, subject to the validity
   --  field.
   --  @field In_Tolerance Provider indication that the MFA is within desired-position tolerance,
   --  subject to the validity field.
   --  @field Platform_Attitude Platform Euler attitude; consult the canonical Status Euler
   --  description and the validity field.
   --  @field Validity_Flag_Bitfield Raw validity value for at-speed, in-tolerance and
   --  platform-attitude fields; no bit positions are invented.
   --  @field Image_Rotation_Rad Provider image rotation in radians; no rotation is applied by the
   --  bridge.
   type Line_Of_Sight_Euler is record
      System_Time_NS : Long_Long_Integer;
      Attitude       : AMS.MEL.Status.Euler;
      Attitude_Rates : AMS.MEL.Status.Euler;
   end record;
   --  Owned aperture line-of-sight Euler angles and rates at their time of validity.
   --  @field System_Time_NS Line-of-sight time of validity in nanoseconds; no clock or epoch is
   --  specified.
   --  @field Attitude Euler angles describing aperture line of sight, in radians.
   --  @field Attitude_Rates Euler angle rates describing aperture line-of-sight rate, in radians
   --  per second.
   subtype Navigation_Response is AMS.MEL.IR.Image.Navigation_Response;
   --  Source-compatible subtype of the canonical Image Navigation Response, preserving response
   --  timestamp and both identifiers; see that type for units and identity semantics.
   function Reported_Size (Value : Bad_Pixel_List) return Interfaces.Unsigned_32;
   --  Provider-reported bad-pixel list size in bytes; not recomputed from copied entries.
   --  @param Value Owned bad-pixel list to inspect.
   --  @return Provider-reported bad-pixel list size in bytes; not recomputed from copied entries.
   function Reported_Count (Value : Bad_Pixel_List) return Interfaces.Unsigned_32;
   --  Provider-reported bad-pixel count; may differ from the copied collection length.
   --  @param Value Owned bad-pixel list to inspect.
   --  @return Provider-reported bad-pixel count; may differ from the copied collection length.
   function Pixel_Count (Value : Bad_Pixel_List) return Natural;
   --  Actual length of the copied bad-pixel collection, used for indexing.
   --  @param Value Owned bad-pixel list to inspect.
   --  @return Actual length of the copied bad-pixel collection, used for indexing.
   function Pixel_At (Value : Bad_Pixel_List; Index : Positive) return Bad_Pixel;
   --  Copied bad-pixel entry at the one-based sequence index.
   --  @param Value Owned bad-pixel list to inspect.
   --  @param Index One-based position; raises Constraint Error beyond the actual copied length.
   --  @return Copied bad-pixel entry at the one-based sequence index.
   type Metadata_Counters is record
      Events_Received           : AMS.MEL.IR.Counter;
      Events_Dropped_Queue_Full : AMS.MEL.IR.Counter;
      Malformed_Or_Unsupported  : AMS.MEL.IR.Counter;
   end record;
   --  Saturating unsigned callback counters shared across the four kinds; these count callback
   --  activity, not successful Ada Receive calls.
   --  @field Events_Received Callback entries observed, incremented even when consumption has
   --  become inactive.
   --  @field Events_Dropped_Queue_Full Valid copied incoming events discarded because the bounded
   --  queue is full; existing events are retained.
   --  @field Malformed_Or_Unsupported Null, unsupported or malformed events and copying/allocation
   --  failures rejected at the callback boundary.
   type Metadata_Stream is limited private;
   --  Limited owner of public metadata consumption. Callback state belongs to the image stream and
   --  can outlive this public owner. Close deactivates consumption, not provider callback
   --  unregistration; finalization attempts non-raising cleanup. Serialize operations on the same
   --  Ada owner.
   type Metadata_Event is private;
   --  Ada-owned tagged snapshot of one metadata event. Native temporary storage is closed before
   --  Receive returns; the value survives metadata stream, image stream and Session closure. No
   --  borrowed provider pointer escapes.
   function Open
     (Stream : AMS.MEL.IR.Image_Stream; Queue_Capacity : Positive) return Metadata_Stream;
   --  Registers all four supported callbacks on an attached or running image stream, once per
   --  image stream, including after registration failure. Prefer registration before Start to
   --  avoid missing early events. A failed or unsupported registration raises Provider Error and
   --  publishes no metadata owner; already registered callbacks retain inactive state until
   --  provider-channel destruction because upstream supplies no unregister operation. All kinds
   --  share one bounded drop-incoming queue, ordered by successful enqueue under its mutex, not by
   --  timestamps or concurrent callback entry order. Closing metadata does not close the image
   --  stream.
   --  @param Stream Owning image stream, not stopping, stopped or failed.
   --  @param Queue_Capacity Maximum number of queued events across all four kinds.
   --  @return Limited metadata-consumption owner.
   --  @exception Provider_Error Closed/ineligible stream, repeated registration attempt,
   --  registration rejection/exception or native failure.
   function Receive
     (Stream : Metadata_Stream; Timeout_Milliseconds : Natural := 0) return Metadata_Event;
   --  Dequeues one event and copies all active payload into Ada ownership before releasing its
   --  native temporary. Already queued events can drain after logical image Stop/Close; once
   --  empty, stopped consumption raises Stream Stopped. Provider callbacks never invoke Ada code.
   --  No automatic registration or optional callback binding is performed.
   --  @param Stream Open metadata-consumption owner; externally serialize access and Close.
   --  @param Timeout_Milliseconds Maximum queue wait in milliseconds; zero polls.
   --  @return Independent owned event of one of the four supported kinds.
   --  @exception Timeout_Error Active queue remained empty until the wait expired.
   --  @exception Stream_Stopped Inactive or stopped queue is empty.
   --  @exception Provider_Error Closed metadata owner, invalid converted event, or native
   --  operation failure.
   function Counters (Stream : Metadata_Stream) return Metadata_Counters;
   --  Copies the current native counters without consuming queued events. Values saturate at the
   --  maximum counter value.
   --  @param Stream Open public metadata owner.
   --  @return Owned snapshot of callback counters.
   --  @exception Provider_Error Closed owner or native query failure.
   procedure Close (Stream : in out Metadata_Stream);
   --  Idempotently closes public metadata consumption and wakes native waiters. Does not
   --  unregister provider callbacks, close the image stream, or guarantee immediate callback
   --  quiescence. Channel-owned callback state survives for safe later provider calls until
   --  channel destruction. Automatic finalization attempts cleanup without raising.
   --  @param Stream Metadata owner to close; serialize against Receive and other uses.
   --  @exception Provider_Error Native close failed.
   function Kind (Event : Metadata_Event) return Metadata_Kind;
   --  Returns the kind of this owned event.
   --  @param Event Event snapshot to inspect.
   --  @return Discriminator selecting the applicable payload accessor.
   function Bad_Pixel_List_Value (Event : Metadata_Event) return Bad_Pixel_List;
   --  Copies the bad-pixel list payload from an event of the corresponding kind. The body
   --  explicitly raises Provider Error for an incorrect kind; these accessors do not rely on
   --  declared preconditions.
   --  @param Event Owned event with the corresponding kind.
   --  @return Independent owned bad-pixel list value.
   --  @exception Provider_Error Event kind does not match this accessor.
   function Line_Of_Sight_Report_Value (Event : Metadata_Event) return Line_Of_Sight_Report;
   --  Copies the line-of-sight report payload from an event of the corresponding kind. The body
   --  explicitly raises Provider Error for an incorrect kind; these accessors do not rely on
   --  declared preconditions.
   --  @param Event Owned event with the corresponding kind.
   --  @return Independent owned line-of-sight report value.
   --  @exception Provider_Error Event kind does not match this accessor.
   function Line_Of_Sight_Euler_Value (Event : Metadata_Event) return Line_Of_Sight_Euler;
   --  Copies the line-of-sight Euler payload from an event of the corresponding kind. The body
   --  explicitly raises Provider Error for an incorrect kind; these accessors do not rely on
   --  declared preconditions.
   --  @param Event Owned event with the corresponding kind.
   --  @return Independent owned line-of-sight Euler value.
   --  @exception Provider_Error Event kind does not match this accessor.
   function Navigation_Response_Value (Event : Metadata_Event) return Navigation_Response;
   --  Copies the navigation response payload from an event of the corresponding kind. The body
   --  explicitly raises Provider Error for an incorrect kind; these accessors do not rely on
   --  declared preconditions.
   --  @param Event Owned event with the corresponding kind.
   --  @return Independent owned navigation response value.
   --  @exception Provider_Error Event kind does not match this accessor.
private
   package Pixel_Vectors is new Ada.Containers.Vectors (Positive, Bad_Pixel);
   type Bad_Pixel_List is record
      Size, Count : Interfaces.Unsigned_32;
      Pixels      : Pixel_Vectors.Vector;
   end record;
   type Metadata_Stream is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Image_Metadata_Handle := AMS.MEL_C_API.Null_Image_Metadata;
   end record;
   overriding
   procedure Finalize (Stream : in out Metadata_Stream);
   type Metadata_Event (Kind_Value : Metadata_Kind := Bad_Pixel_List_Event) is record
      case Kind_Value is
         when Bad_Pixel_List_Event =>
            Bad_Pixel_Value : Bad_Pixel_List;

         when Line_Of_Sight_Report_Event =>
            Report_Value : Line_Of_Sight_Report;

         when Line_Of_Sight_Euler_Event =>
            Euler_Value : Line_Of_Sight_Euler;

         when Navigation_Response_Event =>
            Navigation_Response_Value : Navigation_Response;
      end case;
   end record;
end AMS.MEL.IR.Image.Metadata;
