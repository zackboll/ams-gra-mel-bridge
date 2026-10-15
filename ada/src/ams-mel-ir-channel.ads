with Ada.Containers.Vectors;
private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with Interfaces;

package AMS.MEL.IR.Channel is
   --  Shared channel capability snapshots and the weak common-channel facade.
   --  Capability values describe provider reports, not a promise that the Ada
   --  bridge implements every advertised operation. Reading metadata support
   --  does not register a callback. Snapshots own their strings and collections
   --  independently of the provider, Session and typed channel.

   type Command_ID is mod 2**32 with Size => 32;
   --  Canonical unsigned 32-bit command correlation identifier. The C2 command
   --  identifier is a compatibility subtype of this same type; all bits survive
   --  common request and report conversion.
   type Comms_Channel_ID is mod 2**32 with Size => 32;
   --  Unsigned 32-bit channel selector carried by a communications-test request;
   --  it is not the UCI identifier used in a channel capability.
   type Comms_Request_ID is mod 2**32 with Size => 32;
   --  Unsigned 32-bit communications-test correlation identifier, preserved in
   --  the corresponding report without assigning a bridge-generated sequence.
   type Comms_Test_Report is record
      Command_ID : AMS.MEL.IR.Channel.Command_ID;
      Request_ID : Comms_Request_ID;
   end record;
   --  Owned correlation values returned in a communications-test report.
   --  @field Command_ID Command identifier reported by the provider.
   --  @field Request_ID Communications-test request identifier reported by the provider.

   type Channel_Type is
     (IRST_Track,
      IRST_Image,
      Command_And_Control,
      Scheduling,
      Health_And_Status,
      Instrumentation,
      Stacked_Image,
      Reserved_1,
      Reserved_2);
   --  Published channel categories, in the pinned upstream numeric order 0..8.
   --  A category can be represented without its service being implemented here.
   --  @enum IRST_Track Infrared search-and-track channel category, value 0.
   --  @enum IRST_Image Infrared search-and-track image channel category, value 1.
   --  @enum Command_And_Control Command-and-control channel category, value 2.
   --  @enum Scheduling Task-scheduling channel category, value 3; not a generic Ada scheduling service.
   --  @enum Health_And_Status Health-and-status channel category, value 4.
   --  @enum Instrumentation Instrumentation channel category, value 5.
   --  @enum Stacked_Image Stacked-image channel category, value 6; representation does not add an image API.
   --  @enum Reserved_1 Reserved upstream category, value 7; no supported functionality is implied.
   --  @enum Reserved_2 Reserved upstream category, value 8; no supported functionality is implied.
   type Pixel_Format is (Mono, RGB, Bayer);
   --  Provider-reported image format category, not a complete byte layout.
   --  @enum Mono Monochrome category, upstream value 0.
   --  @enum RGB Red/green/blue category, upstream value 1; packing is not specified by this literal.
   --  @enum Bayer Bayer category, upstream value 2; the literal does not identify a mosaic pattern.
   type Sensor_Type is
     (Unspecified, Gimbal_Horizontal, Gimbal_Vertical, Gimbal_Rotation, Step_Stare);
   --  Published sensor-mechanization reports. Provider-specific values beyond
   --  the upstream exclusive sentinel are not represented by this Ada type.
   --  @enum Unspecified Unknown sensor capabilities, upstream value 0.
   --  @enum Gimbal_Horizontal Sensor can pivot about the horizontal axis, value 1.
   --  @enum Gimbal_Vertical Sensor can pivot about the vertical axis, value 2.
   --  @enum Gimbal_Rotation Sensor can rotate through 360 degrees, value 3.
   --  @enum Step_Stare Step-stare mechanization used for scanning, value 4.
   type Metadata_Capability is
     (Bad_Pixel_List,
      Optical_Distortion_Map,
      LF_Status,
      Line_Of_Sight_Report,
      Line_Of_Sight_Quaternion,
      Line_Of_Sight_Euler,
      MFA_Status,
      MFA_Status_Detailed,
      BIT_Configuration,
      Command_Status,
      BIT_Status,
      Candidate_Object_Message,
      Task_Executing_Rep,
      Subsystem_Status_Resp,
      Execute_Task_Ack,
      Sched_Created_Rep,
      IRST_Track_Report,
      Channel_Comms_Test_Rep,
      Camera_Command_Resp,
      Camera_Protect_Cmd_Resp,
      Instrumentation_Report,
      Navigation_Report_Resp,
      Request_System_Track_Data,
      Update_Track_List_Response,
      LOS_3D_Kinematics_Type,
      Candidate_Object_Preproc_Message,
      Task_Events,
      Scan_Performance_Report,
      Reserved_3,
      Reserved_5,
      Reserved_9,
      Reserved_10);
   --  Metadata callback categories in pinned upstream numeric order 0..31.
   --  Upstream expects an unadvertised callback registration to return
   --  NotSupported. Advertisement does not itself register the callback or
   --  establish that a corresponding safe Ada registration API exists.
   --  @enum Bad_Pixel_List BadPixelList callback category, value 0.
   --  @enum Optical_Distortion_Map OpticalDistortionMap callback category, value 1.
   --  @enum LF_Status LFStatus callback category, value 2.
   --  @enum Line_Of_Sight_Report LineOfSightReport callback category, value 3.
   --  @enum Line_Of_Sight_Quaternion LineOfSightQuaternion callback category, value 4.
   --  @enum Line_Of_Sight_Euler LineOfSightEuler callback category, value 5.
   --  @enum MFA_Status MFA status callback category, value 6.
   --  @enum MFA_Status_Detailed Detailed MFA status callback category, value 7.
   --  @enum BIT_Configuration Built-in-test configuration callback category, value 8.
   --  @enum Command_Status CommandStatus callback category, value 9.
   --  @enum BIT_Status Built-in-test status callback category, value 10.
   --  @enum Candidate_Object_Message CandidateObjectMessage callback category, value 11.
   --  @enum Task_Executing_Rep TaskExecutingRep callback category, value 12.
   --  @enum Subsystem_Status_Resp SubsystemStatusResp callback category, value 13.
   --  @enum Execute_Task_Ack ExecuteTaskAck callback category, value 14.
   --  @enum Sched_Created_Rep SchedCreatedRep callback category, value 15.
   --  @enum IRST_Track_Report IRSTTrackReport callback category, value 16.
   --  @enum Channel_Comms_Test_Rep ChannelCommsTestRep callback category, value 17.
   --  @enum Camera_Command_Resp CameraCommandResp callback category, value 18.
   --  @enum Camera_Protect_Cmd_Resp CameraProtectCmdResp callback category, value 19.
   --  @enum Instrumentation_Report InstrumentationReport callback category, value 20.
   --  @enum Navigation_Report_Resp NavigationReportResp callback category, value 21.
   --  @enum Request_System_Track_Data RequestSystemTrackData inbound callback category, value 22.
   --  @enum Update_Track_List_Response UpdateTrackListResponse callback category, value 23.
   --  @enum LOS_3D_Kinematics_Type Three-dimensional line-of-sight kinematics callback category, value 24.
   --  @enum Candidate_Object_Preproc_Message CandidateObjectPreProcMessage inbound callback category, value 25.
   --  @enum Task_Events TaskEvents callback category, value 26.
   --  @enum Scan_Performance_Report ScanPerformanceReport callback category, value 27.
   --  @enum Reserved_3 Reserved upstream category, value 28, not implemented functionality.
   --  @enum Reserved_5 Reserved upstream category, value 29, not implemented functionality.
   --  @enum Reserved_9 Reserved upstream category, value 30, not implemented functionality.
   --  @enum Reserved_10 Reserved upstream category, value 31, not implemented functionality.
   type Band_Type is
     (Invalid,
      Multiband,
      IR_Far,
      IR_Near,
      IR_Longwave,
      IR_Midwave,
      IR_Shortwave,
      Visible_White,
      Visible_Red,
      Visible_Green,
      Visible_Blue,
      UVA,
      UVB,
      UVC,
      UV_Vacuum);
   --  Spectral categories; numeric wavelength limits are supplied separately.
   --  @enum Invalid Invalid/default band category, upstream value 0.
   --  @enum Multiband Simultaneous frequency spectra, value 1.
   --  @enum IR_Far Far infrared category, value 2.
   --  @enum IR_Near Near infrared category, value 3.
   --  @enum IR_Longwave Longwave infrared category, value 4.
   --  @enum IR_Midwave Midwave infrared category, value 5.
   --  @enum IR_Shortwave Shortwave infrared category, value 6.
   --  @enum Visible_White White light in the visible spectrum, value 7.
   --  @enum Visible_Red Red light in the visible spectrum, value 8.
   --  @enum Visible_Green Green light in the visible spectrum, value 9.
   --  @enum Visible_Blue Blue light in the visible spectrum, value 10.
   --  @enum UVA Ultraviolet A category, value 11.
   --  @enum UVB Ultraviolet B category, value 12.
   --  @enum UVC Ultraviolet C category, value 13.
   --  @enum UV_Vacuum Vacuum ultraviolet category, value 14.
   type Coordinate_System_Type is (LLA, ECEF, NED_Platform, NED_Sensor);
   --  Navigation reference-frame categories, not a coordinate transformation.
   --  @enum LLA Latitude/longitude/altitude representation, upstream value 0.
   --  @enum ECEF Earth-centered, Earth-fixed frame, value 1.
   --  @enum NED_Platform North/east/down frame associated with the platform, value 2.
   --  @enum NED_Sensor North/east/down frame associated with the sensor, value 3.
   type Band_Info is record
      Kind                               : Band_Type;
      Min_Wavelength_M, Max_Wavelength_M : Long_Float;
   end record;
   --  Copied spectral-band description. No normalization or ordering check of
   --  the wavelength bounds is performed by the capability constructor.
   --  @field Kind Overall spectral category; does not supply numerical band limits.
   --  @field Min_Wavelength_M Minimum wavelength in vacuum, in meters.
   --  @field Max_Wavelength_M Maximum wavelength in vacuum, in meters.

   package Sensor_Type_Vectors is new Ada.Containers.Vectors (Positive, Sensor_Type);
   --  Standard positive-index vector of sensor reports. Provider vector order
   --  and duplicate entries survive snapshot conversion.
   package Channel_Type_Vectors is new Ada.Containers.Vectors (Positive, Channel_Type);
   --  Standard positive-index vector of channel categories. Provider vector
   --  order and duplicates survive snapshot conversion.
   package Metadata_Vectors is new Ada.Containers.Vectors (Positive, Metadata_Capability);
   --  Standard positive-index vector of metadata categories. Provider snapshots
   --  follow the native set's ordered, unique iteration; local vectors need not.
   package Band_Info_Vectors is new Ada.Containers.Vectors (Positive, Band_Info);
   --  Standard positive-index vector of band descriptions, preserving the
   --  provider's per-map-entry vector order and duplicates.
   type Image_Band is record
      Band_Index : Interfaces.Unsigned_32;
      Bands      : Band_Info_Vectors.Vector;
   end record;
   --  Owned entry of the provider's image-band map. Its key correlates frames
   --  with band information and need not equal the entry's Ada vector position.
   --  @field Band_Index Upstream map key, widened from unsigned 8 bits in provider snapshots.
   --  @field Bands Ordered band descriptions associated with this key.
   package Image_Band_Vectors is new Ada.Containers.Vectors (Positive, Image_Band);
   --  Standard positive-index vector of band-map entries. Provider snapshots
   --  follow ascending native map-key order, not necessarily contiguous keys.
   package Coordinate_Vectors is new Ada.Containers.Vectors (Positive, Coordinate_System_Type);
   --  Standard positive-index vector of navigation frames. Provider order
   --  corresponds to navigation states in frame headers; an empty list is valid.

   type Channel_Capability is private;
   --  Value-owned capability snapshot with copied identifiers, location and
   --  collections. Reports do not enable services, allocate image buffers or
   --  establish support for unimplemented Ada APIs. Provider set/map iteration
   --  becomes vector order; the local constructor preserves supplied vectors.
   function Create_Capability
     (Channel_ID                                                                    : UCI_ID;
      Height, Width, Bit_Depth, Row_Pitch, Buffer_Size, Image_Size, Number_Of_Bands :
        Interfaces.Unsigned_32;
      Format                                                                        : Pixel_Format;
      Sensor_Types                                                                  :
        Sensor_Type_Vectors.Vector;
      Platform_ID                                                                   : UCI_ID;
      Sensor_Location                                                               :
        Component_Location;
      Channel_Types                                                                 :
        Channel_Type_Vectors.Vector;
      Task_Schedule_Depth                                                           :
        Interfaces.Unsigned_32;
      ODC_Available, NUC_Available                                                  : Boolean;
      Metadata                                                                      :
        Metadata_Vectors.Vector;
      Image_Bands                                                                   :
        Image_Band_Vectors.Vector;
      Nav_Frames                                                                    :
        Coordinate_Vectors.Vector) return Channel_Capability;
   --  Construct a local owned value by copying every argument. This does not
   --  query a provider, validate spectral bounds or enforce native set/map
   --  uniqueness, ordering or the upstream 8-bit band-key range.
   --  @param Channel_ID Channel UCI identity, including its descriptive label.
   --  @param Height Reported image-buffer height.
   --  @param Width Reported image-buffer width.
   --  @param Bit_Depth Image dynamic-range bit depth, not necessarily storage width.
   --  @param Row_Pitch Image-buffer row pitch in bytes.
   --  @param Buffer_Size Buffer-size scalar; copied independently of image size and row pitch.
   --  @param Image_Size Image-size scalar; no product of dimensions is calculated.
   --  @param Number_Of_Bands Reported number of EOIR bands, independent of map-entry count.
   --  @param Format Reported pixel category, not a complete layout description.
   --  @param Sensor_Types Ordered sensor-mechanization reports, including duplicates.
   --  @param Platform_ID Platform UCI identity, including its descriptive label.
   --  @param Sensor_Location Sensor component-location value to copy.
   --  @param Channel_Types Ordered channel categories, including duplicates.
   --  @param Task_Schedule_Depth Reported queued-task capacity, not bridge scheduling state.
   --  @param ODC_Available Whether the provider reports an optical-distortion map is available.
   --  @param NUC_Available Whether the provider reports that it performs nonuniformity correction.
   --  @param Metadata Metadata advertisements; constructing this value registers no callbacks.
   --  @param Image_Bands Band-map entries in supplied order, without sorting or key validation.
   --  @param Nav_Frames Ordered navigation-frame reports; an empty vector is permitted.
   --  @return Independent capability value retaining all supplied fields.

   function Channel_ID (Value : Channel_Capability) return UCI_ID;
   --  Copy the reported channel identity and descriptive label.
   --  @param Value Owned capability snapshot to inspect.
   --  @return Channel UCI identifier, not a communications-test channel selector.
   function Height (Value : Channel_Capability) return Interfaces.Unsigned_32;
   --  Read the reported image-buffer height without deriving allocation size.
   --  @param Value Owned capability snapshot to inspect.
   --  @return Height scalar supplied by the provider or local constructor.
   function Width (Value : Channel_Capability) return Interfaces.Unsigned_32;
   --  Read the reported image-buffer width without deriving allocation size.
   --  @param Value Owned capability snapshot to inspect.
   --  @return Width scalar supplied by the provider or local constructor.
   function Bit_Depth (Value : Channel_Capability) return Interfaces.Unsigned_32;
   --  Read image dynamic-range bit depth, for example an ADC's 12-bit range.
   --  @param Value Owned capability snapshot to inspect.
   --  @return Reported bit depth; not necessarily the storage bits per pixel.
   function Row_Pitch (Value : Channel_Capability) return Interfaces.Unsigned_32;
   --  Read image-buffer row pitch independently of width and image size.
   --  @param Value Owned capability snapshot to inspect.
   --  @return Reported row pitch in bytes.
   function Buffer_Size (Value : Channel_Capability) return Interfaces.Unsigned_32;
   --  Read the distinct upstream buffer-size scalar. The pinned getter supplies
   --  no stronger unit or relationship to image size; no size is recomputed.
   --  @param Value Owned capability snapshot to inspect.
   --  @return Stored buffer-size value.
   function Image_Size (Value : Channel_Capability) return Interfaces.Unsigned_32;
   --  Read the distinct upstream image-size scalar. The pinned getter supplies
   --  no stronger unit or relationship to buffer size; no size is recomputed.
   --  @param Value Owned capability snapshot to inspect.
   --  @return Stored image-size value.
   function Number_Of_Bands (Value : Channel_Capability) return Interfaces.Unsigned_32;
   --  Read the reported number of different EOIR bands produced by the MFA.
   --  @param Value Owned capability snapshot to inspect.
   --  @return Reported band count, not the number of entries in the band map.
   function Format (Value : Channel_Capability) return Pixel_Format;
   --  Read the pixel category without specifying packing or a Bayer pattern.
   --  @param Value Owned capability snapshot to inspect.
   --  @return Reported monochrome, RGB or Bayer category.
   function Sensor_Type_Count (Value : Channel_Capability) return Natural;
   --  Count stored sensor reports, retaining duplicates from provider vectors.
   --  @param Value Owned capability snapshot to inspect.
   --  @return Number of entries available through positive positional indices.
   function Sensor_Type_At (Value : Channel_Capability; Index : Positive) return Sensor_Type;
   --  Read a sensor report in stored order; an index beyond length raises
   --  a constraint error through the underlying Ada vector.
   --  @param Value Owned capability snapshot to inspect.
   --  @param Index One-based vector position, not a sensor identifier.
   --  @return Sensor-mechanization report at the selected position.
   function Platform_ID (Value : Channel_Capability) return UCI_ID;
   --  Copy the reported platform identity and descriptive label.
   --  @param Value Owned capability snapshot to inspect.
   --  @return Stored platform UCI identifier.
   function Sensor_Location (Value : Channel_Capability) return Component_Location;
   --  Copy the sensor component location, retaining offsets and location ID.
   --  @param Value Owned capability snapshot to inspect.
   --  @return Independent component-location value; no coordinate conversion occurs.
   function Channel_Type_Count (Value : Channel_Capability) return Natural;
   --  Count reported channel categories in the copied provider vector.
   --  @param Value Owned capability snapshot to inspect.
   --  @return Number of entries, including any duplicates.
   function Channel_Type_At (Value : Channel_Capability; Index : Positive) return Channel_Type;
   --  Read a category without asserting that the bridge implements it.
   --  An index beyond vector length raises a constraint error.
   --  @param Value Owned capability snapshot to inspect.
   --  @param Index One-based position in stored provider order.
   --  @return Reported channel category at that position.
   function Task_Schedule_Depth (Value : Channel_Capability) return Interfaces.Unsigned_32;
   --  Read reported task-queue capacity. Upstream describes a positive value as
   --  tasks queued after the current task; zero lets an IMMEDIATE task replace
   --  the current task. This report adds no bridge scheduling implementation.
   --  @param Value Owned capability snapshot to inspect.
   --  @return Reported task-schedule depth.
   function ODC_Available (Value : Channel_Capability) return Boolean;
   --  Inspect the upstream optical-distortion-map availability report, not an
   --  implementation of optical-distortion correction in this bridge.
   --  @param Value Owned capability snapshot to inspect.
   --  @return True when the stored availability report is set.
   function NUC_Available (Value : Channel_Capability) return Boolean;
   --  Inspect the upstream report that the MFA performs nonuniformity
   --  correction; no correction algorithm is implemented by this accessor.
   --  @param Value Owned capability snapshot to inspect.
   --  @return True when the stored provider report is set.
   function Metadata_Capability_Count (Value : Channel_Capability) return Natural;
   --  Count stored metadata advertisements. Provider snapshots reflect native
   --  set iteration; local constructor input is not deduplicated.
   --  @param Value Owned capability snapshot to inspect.
   --  @return Number of stored metadata entries.
   function Metadata_Capability_At
     (Value : Channel_Capability; Index : Positive) return Metadata_Capability;
   --  Read a metadata advertisement, not a callback registration. An index
   --  beyond vector length raises a constraint error.
   --  @param Value Owned capability snapshot to inspect.
   --  @param Index One-based stored position, not the enum's numeric value.
   --  @return Metadata category at that position.
   function Has_Metadata_Capability
     (Value : Channel_Capability; Item : Metadata_Capability) return Boolean;
   --  Test stored-vector membership without contacting the provider. Upstream
   --  relates advertisement to registration support, but this test registers
   --  nothing and does not establish a corresponding implemented Ada API.
   --  @param Value Owned capability snapshot to inspect.
   --  @param Item Metadata callback category to seek.
   --  @return True if an equal category occurs in the stored vector.
   function Image_Band_Count (Value : Channel_Capability) return Natural;
   --  Count image-band map entries, not individual spectral bands.
   --  @param Value Owned capability snapshot to inspect.
   --  @return Number of stored map entries available by positive position.
   function Image_Band_Index_At
     (Value : Channel_Capability; Index : Positive) return Interfaces.Unsigned_32;
   --  Read the map key at a positional index. Provider map iteration is ordered
   --  by key; keys need not be contiguous or equal their vector positions.
   --  An index beyond vector length raises a constraint error.
   --  @param Value Owned capability snapshot to inspect.
   --  @param Index One-based map-entry position, not the desired map key.
   --  @return Stored band key, widened from upstream unsigned 8 bits for provider snapshots.
   function Image_Band_Info_Count (Value : Channel_Capability; Index : Positive) return Natural;
   --  Count spectral descriptions associated with one stored map entry.
   --  An out-of-range map-entry position raises a constraint error.
   --  @param Value Owned capability snapshot to inspect.
   --  @param Index One-based map-entry position, not the upstream map key.
   --  @return Length of the selected entry's band-description vector.
   function Image_Band_Info_At
     (Value : Channel_Capability; Band_Index, Info_Index : Positive) return Band_Info;
   --  Copy one spectral description using two positional indices. Either index
   --  exceeding the corresponding vector length raises a constraint error.
   --  @param Value Owned capability snapshot to inspect.
   --  @param Band_Index One-based map-entry position, despite the parameter name; not the upstream map key.
   --  @param Info_Index One-based position in that entry's ordered description vector.
   --  @return Owned category and vacuum wavelength bounds in meters, without normalization.
   function Nav_Frame_Count (Value : Channel_Capability) return Natural;
   --  Count advertised navigation frames; upstream permits an empty vector
   --  when internal sensor navigation data is not provided.
   --  @param Value Owned capability snapshot to inspect.
   --  @return Number of stored navigation-frame reports.
   function Nav_Frame_At
     (Value : Channel_Capability; Index : Positive) return Coordinate_System_Type;
   --  Read a frame category in provider vector order, which upstream relates
   --  to frame-header navigation-state order. No transformation is performed.
   --  An index beyond vector length raises a constraint error.
   --  @param Value Owned capability snapshot to inspect.
   --  @param Index One-based position in the navigation-frame vector.
   --  @return Reported coordinate-system category at that position.

   --  Safe common Channel facade (Task 032B2).
   --
   --  A View is created only by a typed family's As_Channel conversion:
   --  C2.As_Channel, Image.As_Channel, Health_Status.As_Channel,
   --  Instrumentation.As_Channel, or Track.As_Channel. It is a controlled
   --  owner of exactly one native weak common Channel view. It does NOT own
   --  the typed family state, Session, provider Channel, Control, or provider
   --  library: after the typed owner and its Session close, common operations
   --  on a still-open View raise Provider_Error, while Close still succeeds.
   --  Closing or finalizing a View never closes, enables, disables, or
   --  detaches the typed source owner and never cancels pending requests.
   --  Operations on one View and Close of that same View must be externally
   --  serialized; distinct Views need no serialization.
   subtype View is AMS.MEL.IR.Channel_View;
   --  Limited controlled owner of a weak native view obtained from a typed
   --  family's channel conversion. It retains neither typed resource, Session,
   --  provider Channel, Control nor library. A present handle can outlive its
   --  target; even a live target must pass its lifecycle check for operations.
   --  Closing this view never closes its typed owner or cancels requests.
   --  Serialize operations and closure of the same view externally; distinct
   --  views do not require serialization with one another.
   function Is_Open (Channel : View) return Boolean;
   --  Test local handle presence only; this neither locks the weak target nor
   --  proves that it still exists or admits operations.
   --  @param Channel Weak view to inspect without contacting the provider.
   --  @return True when the native view handle is present, even if its target expired.
   procedure Close (Channel : in out View);
   --  Idempotently destroy only the weak view and clear its handle. The typed
   --  owner and independently admitted requests are unaffected. Unexpected
   --  native failure raises a provider error; finalization is non-raising.
   --  @param Channel View to close; externally serialize against its operations.

   type Outcome is (Success, Rejected);
   --  Terminal upstream result category, not submission status or evidence of
   --  physical command execution. Native/provider failures raise exceptions.
   --  @enum Success A future produced its payload; a Return payload can still be Fail.
   --  @enum Rejected The future produced an explicit upstream ErrorOr error.
   type Error_Code is
     (None,
      Invalid_ID,
      Invalid_State,
      Invalid_Parameters,
      Insufficient_Permissions,
      Insufficient_Resources,
      Insufficient_Local_Resources,
      Insufficient_Remote_Resources,
      Unsupported);
   --  Explicit upstream rejection codes mapped to native values 0..8. These
   --  are distinct from bridge admission exhaustion and provider exceptions.
   --  @enum None Upstream no-error code, native value 0; preserved if reported in an error.
   --  @enum Invalid_ID Invalid identifier category, native value 1.
   --  @enum Invalid_State Invalid state category, native value 2.
   --  @enum Invalid_Parameters Invalid parameters category, native value 3.
   --  @enum Insufficient_Permissions Insufficient permissions category, native value 4.
   --  @enum Insufficient_Resources Insufficient resources category, native value 5.
   --  @enum Insufficient_Local_Resources Insufficient local resources category, native value 6.
   --  @enum Insufficient_Remote_Resources Insufficient remote resources category, native value 7.
   --  @enum Unsupported Unsupported request category, native value 8.
   type Command_Return is (Return_Success, Bad_Pointer, Fail, Not_Supported, Not_Implemented);
   --  Payload of a successfully completed Return future, mapped to values 0..4.
   --  Every literal is a payload under Success, not an explicit rejection.
   --  @enum Return_Success Upstream Return::Success, native value 0.
   --  @enum Bad_Pointer Upstream Return::BadPointer, native value 1.
   --  @enum Fail Upstream Return::Fail, native value 2; successful future completion, not command success.
   --  @enum Not_Supported Upstream Return::NotSupported, native value 3.
   --  @enum Not_Implemented Upstream Return::NotImplemented, native value 4.

   --  Submissions are valid while the family's common lifecycle admits
   --  requests (C2/Health/Instrumentation/Track: Attached or Enabled; Image:
   --  Attached or Running). Session admission refusal raises
   --  AMS.MEL.Resource_Exhausted; any other submission failure, including an
   --  expired or closed View, raises Provider_Error. An admitted request owns
   --  the native family graph independently of the View, typed owner, and
   --  Session. Timeout_Error is inherited from AMS.MEL.IR; timeout never
   --  cancels or consumes a request, and a later Wait returns the identical
   --  cached terminal result. Close must not race Wait on the same request.
   type Return_Request is limited private;
   --  Limited controlled observer of one admitted Return future. Pending work
   --  retains its native family graph independently of this owner, the view,
   --  typed owner and Session. Close is not cancellation or direct release of a
   --  pending admission permit. Finalization closes without raising. Serialize
   --  explicit Close against Wait on the same request.
   function Send_Keep_Alive (Channel : View) return Return_Request;
   --  Submit the inherited keep-alive operation, not a typed mode command.
   --  C2, Health, Instrumentation and Track admit it while Attached or Enabled;
   --  Image admits it while Attached or Running. Expired targets or ineligible
   --  states raise a provider error. Session-wide admission refusal raises
   --  the resource-exhaustion exception; other submission failures raise a provider error.
   --  Successful submission does not establish future or command success.
   --  @param Channel Open weak view of an eligible typed resource.
   --  @return Admitted request whose worker independently retains provider lifetime.
   function Is_Open (Request : Return_Request) return Boolean;
   --  Inspect public request-handle presence, not completion or provider state.
   --  @param Request Return request observer to inspect.
   --  @return True until explicit closure or finalization clears the handle.
   type Return_Result is private;
   --  Owned terminal outcome and either a Return payload or rejection details.
   --  A successful outcome may contain Fail; it does not certify physical work.
   function Status (Result : Return_Result) return Outcome;
   --  Read the stored terminal category without contacting the provider.
   --  @param Result Owned result from a Return request wait.
   --  @return Success for a payload or Rejected for an explicit upstream error.
   function Value (Result : Return_Result) return Command_Return
   with Pre => Status (Result) = Success;
   --  Read the Return payload. The declared precondition requires Success;
   --  the body only returns its stored field and adds no outcome check when
   --  assertion checking is disabled. Do not use it on a rejection.
   --  @param Result Successful Return result to inspect.
   --  @return Provider Return payload, which may be Fail or another non-success value.
   function Rejection_Code (Result : Return_Result) return Error_Code
   with Pre => Status (Result) = Rejected;
   --  Read an explicit rejection code. The declared precondition requires
   --  Rejected; the body does not independently enforce that condition.
   --  @param Result Rejected Return result to inspect.
   --  @return Preserved upstream error category, not a bridge failure status.
   function Description (Result : Return_Result) return String
   with Pre => Status (Result) = Rejected;
   --  Copy the rejection diagnostic. The declared precondition requires
   --  Rejected; the body itself simply copies stored text.
   --  @param Result Rejected Return result to inspect.
   --  @return Complete owned rejection text, not a truncated fixed-buffer prefix.
   function Wait (Request : Return_Request; Timeout_Milliseconds : Natural) return Return_Result;
   --  Await completion or return the cached terminal result on repeated waits.
   --  Timeout raises the inherited timeout exception without consuming or
   --  cancelling work. Other native/provider failures raise a provider error;
   --  explicit rejection is a result, not that exception. Completion releases
   --  pending accounting through the worker, not through observer closure.
   --  @param Request Open Return request; must not race its Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls for completion.
   --  @return Owned terminal payload or explicit rejection, cached repeatably.
   procedure Close (Request : in out Return_Request);
   --  Idempotently release the public observer, not the provider work. Pending
   --  work still retains its graph and admission permit until worker completion
   --  or safe-retention fallback. Unexpected native failure raises a provider error.
   --  @param Request Observer to close; externally serialize against Wait.

   type Comms_Request is limited private;
   --  Limited controlled observer of one admitted communications-test future.
   --  Independent native retention protects pending work after view, typed owner,
   --  Session or observer Close. It is distinct from the legacy C2 child owner.
   --  Finalization is non-raising; Close must not race Wait on this observer.
   function Submit_Comms_Test
     (Channel    : View;
      Channel_ID : Comms_Channel_ID;
      Command_ID : AMS.MEL.IR.Channel.Command_ID;
      Request_ID : Comms_Request_ID) return Comms_Request;
   --  Submit the inherited communications test while the typed family admits
   --  common operations: Attached/Enabled for C2, Health, Instrumentation and
   --  Track; Attached/Running for Image. Session admission exhaustion raises
   --  the resource-exhaustion exception. Closed/expired views, ineligible resources and other
   --  native failures raise a provider error. Submission is not completion.
   --  @param Channel Open weak view of an eligible typed channel.
   --  @param Channel_ID Unsigned communications-test channel selector, not a UCI identity.
   --  @param Command_ID Command correlation identifier copied with all 32 bits.
   --  @param Request_ID Communications-test correlation identifier copied with all 32 bits.
   --  @return Admitted observer; pending provider work independently retains lifetime.
   function Is_Open (Request : Comms_Request) return Boolean;
   --  Inspect observer-handle presence without polling the future.
   --  @param Request Communications-test observer to inspect.
   --  @return True for a present handle, whether pending or terminal.
   type Comms_Result is private;
   --  Owned communications-test report or explicit rejection. This future result
   --  is separate from independently registered communications-test metadata.
   function Status (Result : Comms_Result) return Outcome;
   --  Inspect the stored communications-test terminal category.
   --  @param Result Owned communications-test result to inspect.
   --  @return Success for a report or Rejected for an upstream ErrorOr error.
   function Report (Result : Comms_Result) return Comms_Test_Report
   with Pre => Status (Result) = Success;
   --  Copy the report. The declared precondition requires Success; the body
   --  returns its stored field without an independent outcome check.
   --  @param Result Successful communications-test result.
   --  @return Owned command/request correlation identifiers reported by the provider.
   function Rejection_Code (Result : Comms_Result) return Error_Code
   with Pre => Status (Result) = Rejected;
   --  Read the rejection category. The declared precondition requires Rejected;
   --  assertion-disabled execution has no independent body check.
   --  @param Result Rejected communications-test result.
   --  @return Explicit upstream error code, distinct from bridge admission failure.
   function Description (Result : Comms_Result) return String
   with Pre => Status (Result) = Rejected;
   --  Copy rejection text under the declared Rejected precondition. The body
   --  returns stored text without independently checking the outcome.
   --  @param Result Rejected communications-test result.
   --  @return Complete owned diagnostic, including text beyond the initial native buffer.
   function Wait (Request : Comms_Request; Timeout_Milliseconds : Natural) return Comms_Result;
   --  Wait for a report or explicit rejection; repeated terminal waits return
   --  the cached result. Timeout raises the inherited timeout exception and is
   --  not cancellation. Closed handles and other native/provider failures raise
   --  a provider error. No callback registration is performed by this wait.
   --  @param Request Open communications-test observer; must not race Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   --  @return Owned terminal report or rejection, independent of provider storage.
   procedure Close (Request : in out Comms_Request);
   --  Idempotently close the observer, without cancelling the future or directly
   --  freeing its pending admission permit. Independent work retention remains.
   --  Unexpected native failure raises a provider error; finalization is non-raising.
   --  @param Request Observer to close, externally serialized against Wait.

   --  Returns an owned snapshot that remains valid after View Close, typed
   --  owner Close, Session Close, and provider unload.
   function Capabilities (Channel : View) return Channel_Capability;
   --  Temporarily lock the weak target and apply its typed capability lifecycle
   --  check, then copy the report into Ada-owned storage. Closed/expired views
   --  and provider/native failures raise a provider error. Local handle presence
   --  alone proves neither target existence nor eligibility. Reading this report
   --  registers no callbacks and does not implement advertised services.
   --  @param Channel Open weak view whose typed resource admits capability queries.
   --  @return Owned snapshot valid after view, typed owner, Session and provider closure.
private
   package US renames Ada.Strings.Unbounded;
   type Return_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Return_Request_Handle := AMS.MEL_C_API.Null_Return_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Return_Owner);
   type Return_Request is limited record
      Owner : Return_Owner;
   end record;
   type Return_Result is record
      Result_Status : Outcome := Success;
      Result_Value  : Command_Return := Return_Success;
      Result_Code   : Error_Code := None;
      Result_Text   : US.Unbounded_String;
   end record;
   type Comms_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Comms_Request_Handle := AMS.MEL_C_API.Null_Comms_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Comms_Owner);
   type Comms_Request is limited record
      Owner : Comms_Owner;
   end record;
   type Comms_Result is record
      Result_Status : Outcome := Success;
      Result_Report : Comms_Test_Report := (0, 0);
      Result_Code   : Error_Code := None;
      Result_Text   : US.Unbounded_String;
   end record;
   type Channel_Capability is record
      ID, Platform                                            : UCI_ID;
      H, W, Depth, Pitch, Buffer, Image, Band_Count, Schedule : Interfaces.Unsigned_32;
      Pixel                                                   : Pixel_Format := Mono;
      Sensors                                                 : Sensor_Type_Vectors.Vector;
      Location                                                : Component_Location;
      Channels                                                : Channel_Type_Vectors.Vector;
      ODC, NUC                                                : Boolean := False;
      Metadata                                                : Metadata_Vectors.Vector;
      Bands                                                   : Image_Band_Vectors.Vector;
      Frames                                                  : Coordinate_Vectors.Vector;
   end record;
end AMS.MEL.IR.Channel;
