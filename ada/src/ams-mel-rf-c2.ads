private with Ada.Finalization;
private with Ada.Containers.Vectors;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with AMS.MEL.IR;
with Interfaces;

package AMS.MEL.RF.C2 is
   --  Consumer-side RF command ownership, asynchronous resource requests and
   --  copied configuration/snapshots over the published RF MEL interfaces.
   --  Provider reports and successful bridge calls do not establish physical
   --  aperture activation, scheduling or RF transmission. Externally serialize
   --  operations and closure on each owner; timeout is never cancellation.

   type C2_MEL is tagged limited private;
   --  Unique limited controlled owner of the independent RF command interface
   --  and its shared-library reference, not an Admin or DataMEL owner. Default
   --  initialization is closed. Externally serialize use, Close and finalization
   --  on each public owner. Explicit Close reports errors; controlled cleanup
   --  does not raise. Native child claims defer shutdown beyond parent closure.
   function Open (Library_Path : String; Configuration : String) return C2_MEL;
   --  Load the library and call only createC2MEL. Embedded NUL in either input
   --  raises a constraint error before temporary C-string allocation. Temporary
   --  strings are freed on exceptional exits too. Loading, symbol lookup, native
   --  allocation, null factory result and factory exceptions raise a provider
   --  error without returning an open owner. Configuration is not interpreted
   --  by the bridge; this entry point does not validate it as UTF-8.
   --  @param Library_Path Provider shared-library path for the native loader.
   --  @param Configuration Provider-defined factory text, passed synchronously.
   --  @return Independent owner retaining the provider object and library.
   function Is_Open (Object : C2_MEL) return Boolean;
   --  Inspect the local handle only, not provider health or physical RF state.
   --  @param Object Command-interface owner to inspect.
   --  @return True exactly when the public native handle is nonnull.
   procedure Close (Object : in out C2_MEL);
   --  Consume the public handle; repeated closure is harmless. With no child
   --  claims, invoke shutdown once, destroy C2, then drop its library reference.
   --  Otherwise defer shutdown until the final child releases its claim. New
   --  child admission stops at closure; existing children remain retained.
   --  A throwing shutdown raises a provider error on a synchronous cleanup path
   --  and permanently retains the complete graph without retry. Detached cleanup
   --  cannot report that error. Finalization uses non-raising cleanup instead.
   --  @param Object Owner to close; externally serialize its operations.
   Timeout_Error : exception;
   --  A bounded request or Job-status wait expired. Zero milliseconds polls.
   --  Timeout neither cancels work nor consumes a request, result or owner.
   type Virtual_Aperture_Config is private;
   --  Ada-owned request values: definition ID, priority, file information,
   --  ordered local-function strings and Common MEL capability IDs. Construction
   --  allocates no provider aperture and implies no physical activation.
   function Create_Virtual_Aperture_Config
     (VA_Definition_ID        : Interfaces.Unsigned_32;
      Priority                : Interfaces.Unsigned_32;
      VA_Definition_File_Info : String) return Virtual_Aperture_Config;
   --  Copy definition inputs; start with empty function and capability lists.
   --  Embedded NUL raises a constraint error; native submission additionally
   --  validates UTF-8. Empty file information is preserved.
   --  @param VA_Definition_ID Exact unsigned 32-bit provider definition ID.
   --  @param Priority Exact unsigned 32-bit request priority.
   --  @param VA_Definition_File_Info Provider-defined file information, not opened locally.
   --  @return Independently owned configuration value.
   procedure Append_Local_Function_Info (Config : in out Virtual_Aperture_Config; Value : String);
   --  Append a copied string in caller order, including empty strings. Embedded
   --  NUL raises a constraint error; UTF-8 is checked at native submission.
   --  @param Config Configuration whose local-function list is extended.
   --  @param Value Provider-defined local-function information.
   procedure Append_Capability_ID
     (Config : in out Virtual_Aperture_Config; Value : AMS.MEL.IR.UCI_ID);
   --  Append a copied Common MEL ID, preserving all UUID bytes, label and order.
   --  Embedded NUL in the label raises a constraint error. Submission checks UTF-8.
   --  @param Config Configuration whose capability list is extended.
   --  @param Value Capability identity; not a Job byte-vector identity.
   type Virtual_Aperture_Request is tagged limited private;
   --  Unique controlled asynchronous request owner. A native worker consumes the
   --  RequestFor future once and caches its outcome. One C2 child claim follows
   --  pending work, unclaimed success and the claimed aperture. Public parent
   --  closure does not invalidate that graph; a never-ready future can retain it
   --  indefinitely. Default initialization is closed; finalization abandons
   --  without raising, not by cancelling the provider request.
   function Submit_Virtual_Aperture
     (Parent : C2_MEL'Class; Config : Virtual_Aperture_Config) return Virtual_Aperture_Request;
   --  Validate and copy all request inputs before invoking requestVirtualAperture.
   --  Native text spans require UTF-8 without NUL. Invalid inputs, a closed parent,
   --  synchronous provider failure or invalid future raise a provider error and
   --  publish no request. Post-submission worker/publication failure retains the
   --  future and provider graph rather than assuming cancellation. Normal return
   --  means submission only, not successful allocation or physical activation.
   --  @param Parent Open independent C2 owner; serialize with parent closure.
   --  @param Config Copied request values; no caller storage is borrowed afterward.
   --  @return Open request for subsequent bounded Wait and unique Claim.
   function Is_Open (Request : Virtual_Aperture_Request) return Boolean;
   --  Inspect local request ownership, not readiness or Claim availability.
   --  @param Request Request owner to inspect.
   --  @return True exactly when its public native handle is nonnull.
   procedure Close (Request : in out Virtual_Aperture_Request);
   --  Nonblocking, idempotent abandonment, not cancellation. Pending work keeps
   --  its claim. An unclaimed successful aperture is destroyed by native cleanup
   --  before the claim releases; an already claimed aperture remains independent.
   --  @param Request Owner to consume; do not race Wait, Claim or Close on it.
   type Request_Outcome is (Created, Failed);
   --  Cached request result category, separate from bridge/provider exceptions.
   --  @enum Created A nonnull provider resource became available; Claim is separate.
   --  @enum Failed Provider failure data, not an exception from the bridge call.
   type Request_Error_Code is
     (None,
      Invalid_ID,
      Invalid_State,
      Invalid_Parameters,
      Insufficient_Permissions,
      Insufficient_Resources,
      Insufficient_Local_Resources,
      Insufficient_Remote_Resources,
      Unsupported);
   --  Common MEL request error codes, not native status or cancellation codes.
   --  @enum None No specific MEL error; also used for bridge-detected null success or unknown code failures.
   --  @enum Invalid_ID Provider reported an invalid identity.
   --  @enum Invalid_State Provider reported an invalid state.
   --  @enum Invalid_Parameters Provider reported invalid parameters.
   --  @enum Insufficient_Permissions Provider reported insufficient permissions.
   --  @enum Insufficient_Resources Provider reported insufficient resources.
   --  @enum Insufficient_Local_Resources Provider reported insufficient local resources.
   --  @enum Insufficient_Remote_Resources Provider reported insufficient remote resources.
   --  @enum Unsupported Provider reported an unsupported request.
   for Request_Error_Code use
     (None                          => 0,
      Invalid_ID                    => 1,
      Invalid_State                 => 2,
      Invalid_Parameters            => 3,
      Insufficient_Permissions      => 4,
      Insufficient_Resources        => 5,
      Insufficient_Local_Resources  => 6,
      Insufficient_Remote_Resources => 7,
      Unsupported                   => 8);
   type Virtual_Aperture_Result is private;
   --  Copied immutable terminal outcome, error code and diagnostic. Contains no
   --  resource owner; closing the request does not invalidate this value.
   function Outcome (Result : Virtual_Aperture_Result) return Request_Outcome;
   --  Read the cached request category, not a physical aperture status.
   --  @param Result Copied terminal request result.
   --  @return Creation or failure category.
   function Error_Code (Result : Virtual_Aperture_Result) return Request_Error_Code;
   --  Read the mapped MEL code; inspect outcome and description as well.
   --  @param Result Copied terminal request result.
   --  @return Stored error code, None on successful creation.
   function Description (Result : Virtual_Aperture_Result) return String;
   --  Copy the full cached diagnostic. Native malformed provider text is replaced
   --  by a fixed diagnostic; long failure descriptions are not silently truncated.
   --  @param Result Copied terminal request result.
   --  @return Owned diagnostic string, empty on successful creation.
   function Wait
     (Request : Virtual_Aperture_Request; Timeout_Milliseconds : Natural)
      return Virtual_Aperture_Result;
   --  Wait for cached completion, not aperture activation. Repeated terminal
   --  waits return the same data without consuming the future again. Timeout
   --  raises a timeout error and leaves work pending; provider rejection returns
   --  failure data, whereas future exceptions and native failures raise a provider
   --  error. A null successful provider pointer is failure data, not Created.
   --  @param Request Open request; serialize with its Close and Claim.
   --  @param Timeout_Milliseconds Maximum completion wait in milliseconds; zero polls.
   --  @return Copied terminal outcome, mapped code and complete diagnostic.
   type Virtual_Aperture is tagged limited private;
   --  Unique controlled owner obtained by Claim, retaining the provider aperture
   --  and C2 child claim. Owns copied ID/label snapshots independently in Ada.
   --  Default initialization is closed; automatic cleanup does not raise.
   function Claim (Request : Virtual_Aperture_Request'Class) return Virtual_Aperture;
   --  Transfer a successful request's resource exactly once. Pending, failed or
   --  previously claimed requests raise a provider error. Native getters build
   --  the complete snapshot before publication; a getter-started failure is cached
   --  without retrying those getters. Ada copies the snapshot and cleans the new
   --  owner if copying raises. Claim does not establish physical activation.
   --  @param Request Open successfully completed request; serialize its operations.
   --  @return Independent aperture owner with sorted IDs, copied labels and group flag.
   function Is_Open (Object : Virtual_Aperture) return Boolean;
   --  Inspect local ownership only, not operational state or provider health.
   --  @param Object Aperture owner to inspect.
   --  @return True exactly when the public native handle is nonnull.
   function VA_Instance_ID_Count (Object : Virtual_Aperture) return Natural;
   --  Read the Ada-owned Claim-time snapshot without querying the provider.
   --  @param Object Owner holding the copied snapshot, also readable after Close.
   --  @return Number of sorted instance IDs, zero for a default owner.
   function VA_Instance_ID_At
     (Object : Virtual_Aperture; Index : Positive) return Interfaces.Unsigned_32;
   --  Read an instance ID copied from the provider's sorted set at Claim.
   --  @param Object Owner holding the immutable copied snapshot.
   --  @param Index One-based position; a position beyond count raises a constraint error.
   --  @return Exact unsigned 32-bit instance ID at that position.
   function Element_Group_Label_Count (Object : Virtual_Aperture) return Natural;
   --  Read the copied Claim-time label count, not a live provider query.
   --  @param Object Owner holding the snapshot, also readable after Close.
   --  @return Number of labels in provider vector order.
   function Element_Group_Label_At (Object : Virtual_Aperture; Index : Positive) return String;
   --  Copy a validated UTF-8 label from Ada-owned storage; empty labels survive.
   --  @param Object Owner holding the immutable copied labels.
   --  @param Index One-based position; a position beyond count raises a constraint error.
   --  @return Label at the selected provider-vector position, not a sorted key.
   function Is_Single_Group (Object : Virtual_Aperture) return Boolean;
   --  Read the Claim-time provider flag, not a current configuration query.
   --  @param Object Owner holding the copied snapshot, also readable after Close.
   --  @return Copied single-group flag; False for a default owner.
   procedure Close (Object : in out Virtual_Aperture);
   --  Consume the handle idempotently, destroy its provider aperture reference,
   --  then release its C2 claim. Jobs retain their own provider reference and
   --  sibling claim. A final-child deferred shutdown exception raises a provider
   --  error after closure and retains the uncertain C2/library graph permanently.
   --  No shutdown retry occurs; finalization suppresses cleanup errors. Ada-owned
   --  snapshots are unchanged. This is resource release, not physical deactivation.
   --  @param Object Owner to close; serialize with submission and other owner calls.
   type RX_Element_Group_Config is private;
   --  TX JobRequest requirements only; this does not execute a TX interval.
   --  Power is exact provider-defined TxPowerLevel, not a TxPowerModeID.
   type TX_Element_Group_Config is private;
   --  Ada-owned TX Job requirements: label, duty, power level and frequency
   --  ranges. This value constructs no event and activates no transmitter.
   function Create_TX_Element_Group
     (Label               : String;
      TX_Power_Level      : Interfaces.Unsigned_32;
      Desired_Duty_Factor : Long_Float := 1.0) return TX_Element_Group_Config;
   --  Copy TX requirements without calling the provider. Embedded NUL or a
   --  nonfinite duty outside (0, 1] raises a constraint error. Submission checks
   --  label UTF-8 and the provider-created command's TX mode independently.
   --  @param Label Provider element-group label; empty text is allowed.
   --  @param TX_Power_Level Exact unsigned provider TxPowerLevel, not a power-mode ID.
   --  @param Desired_Duty_Factor Finite requested fraction, strictly positive and at most one.
   --  @return Owned TX requirements with an empty frequency list.
   procedure Append_Expected_Center_Frequency
     (Group : in out TX_Element_Group_Config; Min_Hz, Max_Hz : Long_Float);
   --  Append a copied TX frequency range in caller order. Nonfinite or reversed
   --  bounds raise a constraint error; no bridge positivity restriction applies.
   --  @param Group TX requirements to extend, not a running interval.
   --  @param Min_Hz Finite lower center-frequency bound in hertz.
   --  @param Max_Hz Finite upper center-frequency bound in hertz, at least the lower bound.
   function Create_RX_Element_Group
     (Label               : String;
      Desired_Duty_Factor : Long_Float := 1.0;
      Data_Pipe_Label     : String := "default") return RX_Element_Group_Config;
   --  Copy RX requirements, initially without frequencies, endpoints or expected
   --  pointings. Embedded NUL in either label, or nonfinite duty outside (0, 1],
   --  raises a constraint error. UTF-8 and command RX mode are checked at native
   --  submission. Constructing this value neither associates an actual endpoint
   --  nor starts receive activity.
   --  @param Label Provider element-group label; empty text is allowed.
   --  @param Desired_Duty_Factor Finite requested fraction, strictly positive and at most one.
   --  @param Data_Pipe_Label Default pipe label used by the two-argument endpoint append.
   --  @return Independently owned RX configuration value.
   procedure Append_Expected_Center_Frequency
     (Group : in out RX_Element_Group_Config; Min_Hz, Max_Hz : Long_Float);
   --  Append a copied RX frequency range in caller order. Nonfinite or reversed
   --  bounds raise a constraint error; no Squall-specific frequency rule applies.
   --  @param Group RX requirements to extend.
   --  @param Min_Hz Finite lower center-frequency bound in hertz.
   --  @param Max_Hz Finite upper center-frequency bound in hertz, at least the lower bound.
   procedure Append_Endpoint_ID
     (Group : in out RX_Element_Group_Config; ID : Interfaces.Unsigned_64);
   --  Append a copied endpoint identity to the configured default pipe.
   --  Pipe collections retain insertion order. Duplicate IDs within one
   --  collection raise a constraint error; empty collections are omitted.
   --  @param Group RX requirements; use the default pipe label saved at construction.
   --  @param ID Exact unsigned 64-bit endpoint ID to associate at submission.
   procedure Append_Endpoint_ID
     (Group           : in out RX_Element_Group_Config;
      Data_Pipe_Label : String;
      ID              : Interfaces.Unsigned_64);
   --  Append an endpoint to the named pipe's copied collection. Embedded NUL in
   --  the label or a duplicate ID within that pipe raises a constraint error.
   --  Existing pipe collections retain insertion order; IDs become a set during
   --  submission. The same ID in distinct pipes is not rejected locally. No live
   --  VA pipe lookup or endpoint allocation is performed here.
   --  @param Group RX requirements to extend.
   --  @param Data_Pipe_Label Pipe association label; native submission validates UTF-8.
   --  @param ID Exact unsigned 64-bit endpoint identity, including zero/high bits.
   type UTC_Time is private;
   --  Owned UTC integral seconds and canonical fractional femtoseconds. No clock
   --  query or conversion from another time scale occurs during construction.
   function Create_UTC_Time
     (Seconds, Fractional_Femtoseconds : Interfaces.Integer_64) return UTC_Time;
   --  Preserve the supplied pair without normalization. A fraction below zero
   --  or at least one quadrillion raises a constraint error; full signed seconds are valid.
   --  @param Seconds Exact integral UTC seconds in the upstream representation.
   --  @param Fractional_Femtoseconds Nonnegative fraction strictly below one quadrillion femtoseconds.
   --  @return Owned canonical UTC pair.
   function Seconds (Value : UTC_Time) return Interfaces.Integer_64;
   --  Read the stored integral part without rounding the fractional part.
   --  @param Value Owned UTC pair.
   --  @return Exact signed integral seconds.
   function Fractional_Femtoseconds (Value : UTC_Time) return Interfaces.Integer_64;
   --  Exact caller-selected coordinates: meters, meters/second and radians.
   --  No conversion, normalization, physical range or finite-value checks.
   --  @param Value Owned UTC pair.
   --  @return Stored nonnegative fraction strictly below one quadrillion femtoseconds.
   type Pointing_Kind is (ECEF, LLA, Platform_Relative, Face_Relative, Baseline_Relative);
   --  Active alternative of the published five-kind pointing variant.
   --  @enum ECEF Earth-centered Earth-fixed position and velocity with validity time.
   --  @enum LLA WGS-84 latitude/longitude/altitude and NED velocity with validity time.
   --  @enum Platform_Relative Azimuth/elevation relative to the platform, in radians.
   --  @enum Face_Relative Azimuth/elevation relative to the aperture face, in radians.
   --  @enum Baseline_Relative Conic angle relative to the aperture baseline, in radians.
   type Pointing is private;
   --  Owned caller-selected pointing value, not a coordinate transformation or
   --  capability query. Constructors preserve active coordinates and IEEE value
   --  classification, including signed zero, infinity and NaN; there is no NaN
   --  payload-bit promise. No clamping, wrapping, geodesy or finite/range check is
   --  performed on coordinates. Only ECEF/LLA UTC fractions are constrained.
   function Create_ECEF_Pointing
     (Location_X_M, Location_Y_M, Location_Z_M       : Long_Float;
      Velocity_X_MPS, Velocity_Y_MPS, Velocity_Z_MPS : Long_Float;
      Time_Of_Validity                               : UTC_Time) return Pointing;
   --  Construct exact ECEF position/velocity requirements without a frame change.
   --  @param Location_X_M Earth-fixed X position in meters.
   --  @param Location_Y_M Earth-fixed Y position in meters.
   --  @param Location_Z_M Earth-fixed Z position in meters.
   --  @param Velocity_X_MPS Earth-fixed X velocity in meters per second.
   --  @param Velocity_Y_MPS Earth-fixed Y velocity in meters per second.
   --  @param Velocity_Z_MPS Earth-fixed Z velocity in meters per second.
   --  @param Time_Of_Validity Canonical UTC pair copied into the value.
   --  @return Owned ECEF alternative, not a commanded physical pointing.
   function Create_LLA_Pointing
     (Latitude_Rad, Longitude_Rad, Altitude_M                  : Long_Float;
      Velocity_North_MPS, Velocity_East_MPS, Velocity_Down_MPS : Long_Float;
      Time_Of_Validity                                         : UTC_Time) return Pointing;
   --  Construct exact WGS-84 position and NED velocity without conversion to ECEF.
   --  Out-of-conventional-range coordinates are preserved, not normalized.
   --  @param Latitude_Rad Caller latitude in radians.
   --  @param Longitude_Rad Caller longitude in radians.
   --  @param Altitude_M Caller altitude in meters.
   --  @param Velocity_North_MPS North velocity in meters per second.
   --  @param Velocity_East_MPS East velocity in meters per second.
   --  @param Velocity_Down_MPS Down velocity in meters per second.
   --  @param Time_Of_Validity Canonical UTC pair copied into the value.
   --  @return Owned LLA alternative, not provider acceptance.
   function Create_Platform_Relative_Pointing
     (Azimuth_Rad, Elevation_Rad : Long_Float) return Pointing;
   --  Construct platform-relative angles exactly, without transformation.
   --  @param Azimuth_Rad Caller azimuth in radians; no wrapping or finite check.
   --  @param Elevation_Rad Caller elevation in radians; no clamping or finite check.
   --  @return Owned platform-relative alternative, without UTC payload.
   function Create_Face_Relative_Pointing (Azimuth_Rad, Elevation_Rad : Long_Float) return Pointing;
   --  Construct aperture-face-relative angles exactly, without transformation.
   --  @param Azimuth_Rad Caller azimuth in radians; no wrapping or finite check.
   --  @param Elevation_Rad Caller elevation in radians; no clamping or finite check.
   --  @return Owned face-relative alternative, without UTC payload.
   function Create_Baseline_Relative_Pointing (Conic_Rad : Long_Float) return Pointing;
   --  Construct the caller's baseline-relative angle without a frame conversion.
   --  @param Conic_Rad Caller baseline-relative conic angle in radians; no range check.
   --  @return Owned baseline-relative alternative, without UTC payload.
   function Kind (Value : Pointing) return Pointing_Kind;
   --  Read the active variant without inspecting inactive coordinates.
   --  @param Value Owned pointing value.
   --  @return Active alternative; no provider support claim is implied.
   type ECEF_Coordinates is record
      X, Y, Z : Long_Float;
   end record;
   --  Copied Earth-fixed triple: meters for position, meters per second for
   --  velocity, as selected by the accessor. No frame conversion is performed.
   --  @field X Exact X component in the accessor's units.
   --  @field Y Exact Y component in the accessor's units.
   --  @field Z Exact Z component in the accessor's units.
   type LLA_Coordinates is record
      Latitude_Rad, Longitude_Rad, Altitude_M : Long_Float;
   end record;
   --  Copied WGS-84 location; no coordinate normalization is performed.
   --  @field Latitude_Rad Latitude in radians.
   --  @field Longitude_Rad Longitude in radians.
   --  @field Altitude_M Altitude in meters.
   type NED_Velocity is record
      North_MPS, East_MPS, Down_MPS : Long_Float;
   end record;
   --  Copied north/east/down velocity; not converted to an Earth-fixed frame.
   --  @field North_MPS North component in meters per second.
   --  @field East_MPS East component in meters per second.
   --  @field Down_MPS Down component in meters per second.
   function ECEF_Position (Value : Pointing) return ECEF_Coordinates;
   --  Read active ECEF position; wrong kind raises a constraint error.
   --  @param Value ECEF pointing whose stored fields are inspected.
   --  @return Copied X/Y/Z position in meters.
   function ECEF_Velocity (Value : Pointing) return ECEF_Coordinates;
   --  Read active ECEF velocity; wrong kind raises a constraint error.
   --  @param Value ECEF pointing whose stored fields are inspected.
   --  @return Copied X/Y/Z velocity in meters per second.
   function LLA_Position (Value : Pointing) return LLA_Coordinates;
   --  Read active WGS-84 location; wrong kind raises a constraint error.
   --  @param Value LLA pointing whose stored fields are inspected.
   --  @return Copied latitude/longitude radians and altitude meters.
   function LLA_Velocity (Value : Pointing) return NED_Velocity;
   --  Read active NED velocity; wrong kind raises a constraint error.
   --  @param Value LLA pointing whose stored fields are inspected.
   --  @return Copied north/east/down velocity in meters per second.
   function Azimuth_Rad (Value : Pointing) return Long_Float;
   --  Read relative azimuth; other kinds raise a constraint error.
   --  @param Value Platform-relative or face-relative pointing.
   --  @return Stored azimuth in radians, without wrapping.
   function Elevation_Rad (Value : Pointing) return Long_Float;
   --  Read relative elevation; other kinds raise a constraint error.
   --  @param Value Platform-relative or face-relative pointing.
   --  @return Stored elevation in radians, without clamping.
   function Conic_Rad (Value : Pointing) return Long_Float;
   --  Read baseline-relative conic angle; other kinds raise a constraint error.
   --  @param Value Baseline-relative pointing.
   --  @return Stored conic angle in radians, without normalization.
   function Time_Of_Validity (Value : Pointing) return UTC_Time;
   --  Read absolute-pointing validity time; relative kinds raise a constraint error.
   --  @param Value ECEF or LLA pointing.
   --  @return Copied canonical UTC pair.
   procedure Append_Expected_Pointing (Group : in out RX_Element_Group_Config; Value : Pointing);
   --  Append a copied expected RX pointing, preserving order and duplicates.
   --  No provider setter runs until submission and no physical steering follows
   --  from this local operation. TX expected pointings are not exposed here.
   --  @param Group RX requirements whose pointing sequence is extended.
   --  @param Value Complete active pointing alternative to copy.
   type Byte_Array is array (Natural range <>) of Interfaces.Unsigned_8;
   --  Arbitrary identity bytes for Job configuration: length, order, embedded
   --  zeros and high bits are data. Empty values are allowed; not UTF-8 or a UUID.
   type Unsigned_32_Array is array (Natural range <>) of Interfaces.Unsigned_32;
   --  Unsigned ID sequence, including empty input. Power-mode submission converts
   --  it to a set, so duplicates collapse and caller order is not promised there.
   type Job_Config is private;
   --  Ada-owned JobRequest requirements with copied groups and values, not a
   --  provider Job or acceptance result. Constructors put one group first;
   --  appends preserve global RX/TX order and duplicate labels. Initial times,
   --  duration and lookahead are zero; capability/activity/power-mode IDs are
   --  each [0], instances empty and estimated pointing absent. Submission leaves
   --  upstream JIB controls, callbacks and context at pinned defaults. No local
   --  scheduling comparison or RX/TX activation is performed.
   procedure Set_Estimated_Stab_Point (Config : in out Job_Config; Value : Pointing);
   --  Copy and mark the optional estimated stabilization point present, replacing
   --  any earlier value. Does not transform coordinates or invoke a provider.
   --  @param Config Owned Job requirements to update.
   --  @param Value Complete active pointing alternative to copy.
   procedure Clear_Estimated_Stab_Point (Config : in out Job_Config);
   --  Mark estimated pointing absent; submission then omits its setter rather
   --  than reading or initializing the pinned default ECEF numeric storage.
   --  @param Config Owned Job requirements whose optional point is cleared.
   function Create_Job_Config
     (Request_ID, Priority       : Interfaces.Unsigned_32;
      Group                      : RX_Element_Group_Config;
      Precedence_Within_Priority : Interfaces.Unsigned_32 := 0;
      Interruptable              : Boolean := False) return Job_Config;
   --  Construct owned requirements with the RX group first and pinned defaults
   --  for other fields. This does not submit or schedule a Job.
   --  @param Request_ID Exact unsigned 32-bit request identity.
   --  @param Priority Exact unsigned 32-bit provider scheduling priority.
   --  @param Group RX requirements copied as the initial group.
   --  @param Precedence_Within_Priority Exact precedence among equal priorities.
   --  @param Interruptable True requests resume after higher-priority interruption; False requests termination.
   --  @return Independent mutable Job configuration value.
   procedure Append_Instance_Selection (Config : in out Job_Config; ID : Interfaces.Unsigned_32);
   --  Append an exact instance selection; retain order, duplicates and all bits.
   --  No provider membership check occurs in this local operation.
   --  @param Config Owned Job requirements to extend.
   --  @param ID Unsigned 32-bit VA instance selection value.
   function Create_Job_Config
     (Request_ID, Priority       : Interfaces.Unsigned_32;
      Group                      : TX_Element_Group_Config;
      Precedence_Within_Priority : Interfaces.Unsigned_32 := 0;
      Interruptable              : Boolean := False) return Job_Config;
   --  Append calls define one global order across RX/TX, including duplicates.
   --  @param Request_ID Exact unsigned 32-bit request identity.
   --  @param Priority Exact unsigned 32-bit provider scheduling priority.
   --  @param Group TX requirements copied as the initial group; no TX execution follows.
   --  @param Precedence_Within_Priority Exact precedence among equal priorities.
   --  @param Interruptable True requests resume after higher-priority interruption; False requests termination.
   --  @return Owned Job configuration with the same pinned defaults as the RX constructor.
   procedure Append_TX_Element_Group (Config : in out Job_Config; Group : TX_Element_Group_Config);
   --  Append a copy in global RX/TX order, retaining duplicate labels and modes.
   --  Later edits to the source group do not change the stored requirements.
   --  @param Config Owned Job configuration to extend.
   --  @param Group TX requirements to copy; no event or interval is created.
   procedure Append_RX_Element_Group (Config : in out Job_Config; Group : RX_Element_Group_Config);
   --  Append a copy in global RX/TX order, retaining duplicate labels and modes.
   --  Later edits to the source group do not change the stored requirements.
   --  @param Config Owned Job configuration to extend.
   --  @param Group RX requirements, including copied endpoints and pointings.
   procedure Set_Min_Start_Time (Config : in out Job_Config; Value : UTC_Time);
   --  Replace minimum-start requirements; no comparison with maximum completion
   --  or the current clock occurs. Provider scheduling semantics remain separate.
   --  @param Config Owned Job configuration to update.
   --  @param Value Canonical UTC pair copied exactly.
   procedure Set_Max_Complete_Time (Config : in out Job_Config; Value : UTC_Time);
   --  Replace maximum-completion requirements without a local ordering check.
   --  @param Config Owned Job configuration to update.
   --  @param Value Canonical UTC pair copied exactly.
   procedure Set_Duration_Femtoseconds (Config : in out Job_Config; Value : Interfaces.Integer_64);
   --  Replace requested duration without quantization, clamping or sign checks.
   --  Construction does not establish provider acceptance or effective duration.
   --  @param Config Owned Job configuration to update.
   --  @param Value Exact signed 64-bit femtoseconds, including both extremes and zero.
   procedure Set_Capability_ID (Config : in out Job_Config; Value : Byte_Array);
   --  Replace capability bytes using temporary storage before assignment, leaving
   --  the previous field intact if copying fails. No UUID-length rule applies.
   --  @param Config Owned Job configuration to update.
   --  @param Value Arbitrary copied bytes, including empty input, zeros and high bits.
   procedure Set_Activity_ID (Config : in out Job_Config; Value : Byte_Array);
   --  Replace activity bytes using temporary storage before assignment, leaving
   --  the previous field intact if copying fails. Not a string or Common MEL ID.
   --  @param Config Owned Job configuration to update.
   --  @param Value Arbitrary copied bytes, including empty input, zeros and high bits.
   procedure Set_TX_Power_Mode_IDs (Config : in out Job_Config; Value : Unsigned_32_Array);
   --  Replace copied IDs, building temporary storage before assignment. Native
   --  submission serializes them as a set: duplicates collapse; empty is valid.
   --  Power-mode IDs do not select TX events or guarantee TX activation.
   --  @param Config Owned Job configuration to update.
   --  @param Value Unsigned 32-bit IDs, distinct from each TX group's power level.
   procedure Set_Lookahead_Femtoseconds (Config : in out Job_Config; Value : Interfaces.Integer_64);
   --  Replace requested lookahead without clamping, quantization or scheduling checks.
   --  @param Config Owned Job configuration whose lookahead is replaced.
   --  @param Value Exact signed 64-bit femtoseconds; no sign check or quantization.
   type Job_Request is tagged limited private;
   --  Unique controlled asynchronous JobDetail request owner. Native work retains
   --  a strong provider VA and a sibling C2 child claim independently of public
   --  VA/C2 owners. One worker consumes the RequestFor future and caches terminal
   --  data. Default initialization is closed; non-raising finalization abandons,
   --  never cancels. Unresolved work may retain the provider/library indefinitely.
   function Submit_Job (VA : Virtual_Aperture'Class; Config : Job_Config) return Job_Request;
   --  Copy and validate requirements before provider entry, then create each
   --  command by label, require its selected RX/TX mode, populate requirements
   --  and call requestJob once. At least one total group is required; RX-only,
   --  TX-only and mixed requests are bridge inputs, not provider support claims.
   --  Labels must be UTF-8 without NUL. Validation/preparation failure publishes
   --  no request. A mode mismatch or synchronous native/provider failure raises
   --  a provider error; provider command mutations may precede a later exception,
   --  so this is not a provider rollback guarantee. Post-send worker/publication
   --  failure retains the future and graph. No physical RF activity is promised.
   --  @param VA Open aperture with C2 child admission still open; serialize submission with other owner calls and Close.
   --  @param Config Owned values copied synchronously, with no retained caller spans.
   --  @return Open asynchronous request, not a scheduling acceptance result.
   function Is_Open (Request : Job_Request) return Boolean;
   --  Inspect only local ownership, not readiness or claimability.
   --  @param Request Job request owner to inspect.
   --  @return True exactly when its public native handle is nonnull.
   procedure Close (Request : in out Job_Request);
   --  Nonblocking idempotent abandonment, not cancellation or Job destruction
   --  when pending. Native cleanup destroys an unclaimed successful JobDetail,
   --  then its VA reference, then releases the sibling C2 claim. A claimed Job
   --  remains independent. Serialize request Close, Claim and Wait.
   --  @param Request Public request owner to consume.
   type Job_Result is private;
   --  Copied immutable terminal request data, not a Job owner or execution status.
   --  Result values remain independent of request closure and Claim.
   function Outcome (Result : Job_Result) return Request_Outcome;
   --  Read request completion category, not Job execution or RF activity.
   --  @param Result Copied terminal Job request result.
   --  @return Created for a nonnull JobDetail, otherwise provider failure data.
   function Error_Code (Result : Job_Result) return Request_Error_Code;
   --  Read the mapped Common MEL code; also inspect category and description.
   --  @param Result Copied terminal Job request result.
   --  @return Stored code, None on successful creation.
   function Description (Result : Job_Result) return String;
   --  Copy the complete cached diagnostic; malformed provider text receives a
   --  native fallback. Long descriptions are retrieved without silent truncation.
   --  @param Result Copied terminal Job request result.
   --  @return Owned diagnostic string, empty on successful creation.
   function Wait (Request : Job_Request; Timeout_Milliseconds : Natural) return Job_Result;
   --  Await cached Job request completion, not Job execution. Repeated terminal
   --  waits return the same data. Timeout raises a timeout error without cancelling
   --  or consuming work. Provider rejection and null successful pointers return
   --  failure data; future exceptions and native failures raise a provider error.
   --  @param Request Open request; externally serialize its operations and Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   --  @return Copied terminal category, mapped code and complete diagnostic.
   type Job is limited private;
   --  Unique controlled JobDetail owner with a retained provider VA and sibling
   --  C2 child claim. Owns an Ada copy of the immutable Claim-time snapshot.
   --  Default initialization is closed; explicit closure reports cleanup errors,
   --  while finalization does not raise or implicitly finalize/cancel the Job.
   function Claim (Request : Job_Request'Class) return Job;
   --  Transfer successful Job ownership exactly once. Pending, failed or already
   --  claimed requests raise a provider error. Native getters build a complete
   --  immutable snapshot before publication; getter-started failure is cached,
   --  so no provider getter is retried. Ada copies all fields and cleans its new
   --  owner if copying raises. The snapshot includes only getRxStreamIDs(0), not
   --  all groups. Request and public VA/C2 may close without invalidating the Job.
   --  @param Request Open successfully completed request; serialize its operations.
   --  @return Independent Job owner and copied snapshot, not an RF execution guarantee.
   function Is_Open (Object : Job) return Boolean;
   --  Inspect the local public handle only, not completion, scheduling or RF state.
   --  @param Object Job owner to inspect.
   --  @return True exactly when its public native handle is nonnull.

   --  Matches RF MEL 762ce84: numeric_limits<Femtoseconds>::max() has
   --  count zero, unlike Femtoseconds::max(). This aliases an ordinary zero
   --  relative start; the scalar provider interface cannot distinguish intent.
   --  INT64_MAX is not the pinned continuation sentinel. No quantization or
   --  remapping is performed. Matching this value does not prove scheduling.
   Continue_From_Previous_Femtoseconds : constant Interfaces.Integer_64 := 0;
   --  Pinned RF MEL 762ce84 continuation count is zero, not INT64 MAX:
   --  numeric limits<Femtoseconds>::max() is unspecialized, unlike Femtoseconds::max().
   --  Zero also encodes an ordinary relative start; the scalar interface cannot
   --  distinguish intentions or guarantee continuation scheduling. No remapping.

   type Pulse_Threshold_Reference is (DBQ, DB_Above_Noise, DB_Below_Saturation);
   --  Reference for copied pulse thresholds, not a detector implementation.
   --  @enum DBQ Representation 0: detection threshold in dbq.
   --  @enum DB_Above_Noise Representation 1: threshold above thermal noise in dB.
   --  @enum DB_Below_Saturation Representation 2: threshold below receiver saturation in dB.

   for Pulse_Threshold_Reference use (DBQ => 0, DB_Above_Noise => 1, DB_Below_Saturation => 2);
   type Pulse_Time_Tag_Threshold is (At_50_Percent, At_90_Percent);
   --  Amplitude reference for the provider pulse time tag; no timing accuracy is promised.
   --  @enum At_50_Percent Representation 0: 50-percent amplitude time tag.
   --  @enum At_90_Percent Representation 1: 90-percent amplitude time tag.

   for Pulse_Time_Tag_Threshold use (At_50_Percent => 0, At_90_Percent => 1);
   type Pulse_Detection_Settings is private;
   --  Independent copyable settings and ordered threshold pairs owned by Ada.
   --  Default initialization is DBQ, leading/trailing M/N 0/0, minimum width
   --  zero femtoseconds, 50-percent time tag and no thresholds. Set on RX only;
   --  no detection algorithm, capability or pulse performance is established.

   function Create_Pulse_Detection_Settings
     (Reference                                    : Pulse_Threshold_Reference;
      Leading_M, Leading_N, Trailing_M, Trailing_N : Interfaces.Unsigned_8;
      Min_Pulse_Width_Femtoseconds                 : Interfaces.Integer_64;
      Time_Tag_Threshold                           : Pulse_Time_Tag_Threshold)
      return Pulse_Detection_Settings;
   --  Copy pulse settings with an initially empty threshold list. All uint8 M/N combinations and signed widths are accepted; no M/N relation or physical timing validation is imposed.
   --  @param Reference Threshold reference selector.
   --  @param Leading_M Leading-edge M, 0 through 255.
   --  @param Leading_N Leading-edge N, 0 through 255, independently of M.
   --  @param Trailing_M Trailing-edge M, 0 through 255.
   --  @param Trailing_N Trailing-edge N, 0 through 255, independently of M.
   --  @param Min_Pulse_Width_Femtoseconds Exact signed 64-bit minimum-width count in femtoseconds, including negative values.
   --  @param Time_Tag_Threshold Provider amplitude reference for the time tag.
   --  @return Owned settings without thresholds; no provider call.

   procedure Append_Pulse_Detection_Threshold
     (Settings : in out Pulse_Detection_Settings; Leading_Edge_DB, Trailing_Edge_DB : Long_Float);
   --  Append one copied leading/trailing pair; preserve order and duplicates. Long Float values convert to native double on Add without finite checks or normalization; signed zero, infinities and NaN classification are retained, not NaN payload bits.
   --  @param Settings Owned configuration to extend.
   --  @param Leading_Edge_DB Leading-edge threshold in dB relative to the selected reference.
   --  @param Trailing_Edge_DB Trailing-edge threshold in dB relative to the selected reference.

   type RX_Receive_Event_Config is private;
   --  Copyable Ada-owned RX event values, independent of subsequent edits to copies.
   --  Defaults are zero scalar fields, empty label/groups/polarization, Normal
   --  execution, Inhibit Event termination and False correction/delay/channelization.
   --  Stabilization index is always present and initially zero, not optional.
   --  Hold and termination counts default to zero. Add validates the whole batch
   --  before provider Add: UTF-8 labels without NUL, collection structure, enum
   --  domains and size t representability, not group membership or physical ranges.
   --  Floating values become native double without normalization or finite checks;
   --  IEEE signed zero, infinity and NaN classification survive, not NaN payload bits.
   --  Configuration is not execution, beam steering or cancellation. Provider
   --  capabilities and interpretation remain provider-defined.
   --  RX adds sample frequency, AGC counts and maximum extension; pulse settings
   --  are absent until Set, leaving upstream default settings when omitted.

   type TX_Transmit_Event_Config is private;
   --  Copyable Ada-owned TX event values, independent of subsequent edits to copies.
   --  Defaults are zero scalar fields, empty label/groups/polarization, Normal
   --  execution, Inhibit Event termination and False correction/delay/channelization.
   --  Stabilization index is always present and initially zero, not optional.
   --  Hold and termination counts default to zero. Add validates the whole batch
   --  before provider Add: UTF-8 labels without NUL, collection structure, enum
   --  domains and size t representability, not group membership or physical ranges.
   --  Floating values become native double without normalization or finite checks;
   --  IEEE signed zero, infinity and NaN classification survive, not NaN payload bits.
   --  Configuration is not execution, beam steering or cancellation. Provider
   --  capabilities and interpretation remain provider-defined.
   --  TX adds attenuation in dB and rise/fall durations, all initially zero.
   --  Modulation references are absent by default, retaining upstream {-1}.
   --  TX attenuation is neither TX Power Level nor an interval TX Power Mode ID;
   --  no RX sample frequency, AGC or pulse-detection configuration is exposed.

   type Modulation_Config is private;
   --  Ada-owned copyable tagged configuration, defaulting to Tone. Bridge kinds
   --  0/1/2/3/4/5/6/7 select Tone, PhaseCode, LinearFrequencyModulation,
   --  OnOffKeying, CpfskPulse, CustomWaveform, WaveformBuffer and WaveformStream.
   --  These tags are not C++ variant positions: the first seven select an inner
   --  CommonModulations alternative inside Modulation; Stream selects the outer
   --  WaveformStream alternative directly. Cached/extension alternatives are not exposed.
   --  Only the active payload is serialized/validated. Scalars and arrays are copied;
   --  native buffer copies share their fresh sample allocation, not caller storage.
   --  Native double conversion preserves signed zero, infinities and NaN classification,
   --  not NaN payload bits. No normalization, waveform synthesis, frequency-domain
   --  conversion or provider capability guarantee follows from construction.
   --  All active definitions, including unused ones, validate before provider Add.
   --  Presence belongs independently to interval tables and TX reference collections.

   type Stream_Sample_Format is (Stream_Real, Stream_Complex_Cartesian, Stream_Complex_Polar);
   --  Mapped waveform-stream sample shape, not JobDataFormat or buffer sample width.
   --  @enum Stream_Real Representation 0: real samples.
   --  @enum Stream_Complex_Cartesian Representation 1: Cartesian complex samples.
   --  @enum Stream_Complex_Polar Representation 2: polar complex samples.

   for Stream_Sample_Format use
     (Stream_Real => 0, Stream_Complex_Cartesian => 1, Stream_Complex_Polar => 2);
   type Stream_Item_Format is
     (Stream_Fixed_Point_Unsigned,
      Stream_Fixed_Point_Signed,
      Stream_VRT_Unsigned_Float,
      Stream_VRT_Signed_Float,
      Stream_Float32,
      Stream_Float64,
      Stream_Float16,
      Stream_Fixed_Point_Unsigned_Non_Normalized,
      Stream_Fixed_Point_Signed_Non_Normalized);
   --  Mapped stream numeric item descriptor only; selecting a format does not convert data or prove provider support.
   --  @enum Stream_Fixed_Point_Unsigned Representation 0: unsigned fixed point.
   --  @enum Stream_Fixed_Point_Signed Representation 1: signed fixed point.
   --  @enum Stream_VRT_Unsigned_Float Representation 2: VRT unsigned float.
   --  @enum Stream_VRT_Signed_Float Representation 3: VRT signed float.
   --  @enum Stream_Float32 Representation 4: 32-bit float.
   --  @enum Stream_Float64 Representation 5: 64-bit float.
   --  @enum Stream_Float16 Representation 6: 16-bit float.
   --  @enum Stream_Fixed_Point_Unsigned_Non_Normalized Representation 7: unsigned non-normalized fixed point.
   --  @enum Stream_Fixed_Point_Signed_Non_Normalized Representation 8: signed non-normalized fixed point.

   for Stream_Item_Format use
     (Stream_Fixed_Point_Unsigned                => 0,
      Stream_Fixed_Point_Signed                  => 1,
      Stream_VRT_Unsigned_Float                  => 2,
      Stream_VRT_Signed_Float                    => 3,
      Stream_Float32                             => 4,
      Stream_Float64                             => 5,
      Stream_Float16                             => 6,
      Stream_Fixed_Point_Unsigned_Non_Normalized => 7,
      Stream_Fixed_Point_Signed_Non_Normalized   => 8);
   type Stream_Repeat_Mode is (Stream_Repeat_From_Top, Stream_Continue_From_Previous);
   --  Copied provider read-location policy; no bridge address arithmetic or backing validation.
   --  @enum Stream_Repeat_From_Top Representation 0: restart from top each iteration; upstream requires at least Num Bytes backing.
   --  @enum Stream_Continue_From_Previous Representation 1: advance each iteration; upstream requires Num Bytes times repeat-count backing.

   for Stream_Repeat_Mode use (Stream_Repeat_From_Top => 0, Stream_Continue_From_Previous => 1);
   --  Scalar descriptor only: numeric address, not a pointer. No endpoint or
   --  backing memory is acquired, checked, owned or transferred by this API.
   function Waveform_Stream
     (Endpoint_ID, Start_Address, Num_Bytes : Interfaces.Unsigned_64;
      Sample_Format                         : Stream_Sample_Format := Stream_Complex_Cartesian;
      Item_Format                           : Stream_Item_Format := Stream_Fixed_Point_Signed;
      Repeat_Mode                           : Stream_Repeat_Mode := Stream_Repeat_From_Top)
      return Modulation_Config;
   --  Select kind 7, the outer native WaveformStream alternative, and copy six scalar descriptors. No endpoint acquisition, allocation, registration, mapping, dereference or transfer of backing memory occurs. Application/provider own memory extent, lifetime and capability obligations; zero values are structurally valid, not proof of an executable stream.
   --  @param Endpoint_ID Full unsigned 64-bit endpoint identity; no existence query.
   --  @param Start_Address Full unsigned 64-bit numeric descriptor, not an Ada access value or bridge-owned pointer.
   --  @param Num_Bytes Unsigned 64-bit bytes per iteration; must fit native size t at Add (checked Ada conversion/round trip). Zero is accepted.
   --  @param Sample_Format Sample shape, default ComplexCartesian in the constructor.
   --  @param Item_Format Numeric item descriptor, default FixedPointSigned in the constructor.
   --  @param Repeat_Mode Provider repeat policy, default RepeatFromTop in the constructor.
   --  @return Owned stream descriptor without any backing-storage owner.

   procedure Replace_Waveform_Stream
     (Config                                : in out Modulation_Config;
      Endpoint_ID, Start_Address, Num_Bytes : Interfaces.Unsigned_64;
      Sample_Format                         : Stream_Sample_Format;
      Item_Format                           : Stream_Item_Format;
      Repeat_Mode                           : Stream_Repeat_Mode);
   --  Replace all six descriptors through a new stream value; wrong kind raises Constraint Error without changing Config. Same numeric-only ownership and Add-time size t validation as the constructor; no data is transferred.
   --  @param Config Owned WaveformStream (kind 7) required.
   --  @param Endpoint_ID Full unsigned 64-bit endpoint identity; no existence query.
   --  @param Start_Address Full unsigned 64-bit numeric descriptor, not an Ada access value or bridge-owned pointer.
   --  @param Num_Bytes Unsigned 64-bit bytes per iteration; must fit native size t at Add (checked Ada conversion/round trip). Zero is accepted.
   --  @param Sample_Format Sample shape, default ComplexCartesian in the constructor.
   --  @param Item_Format Numeric item descriptor, default FixedPointSigned in the constructor.
   --  @param Repeat_Mode Provider repeat policy, default RepeatFromTop in the constructor.

   type Waveform_Sample_I8 is record
      Real, Imag : Interfaces.Integer_8;
   end record;
   --  One Cartesian complex pair with signed 8-bit components; no scaling or clipping.
   --  @field Real Real (I) component, full signed 8-bit integer domain.
   --  @field Imag Imaginary (Q) component, full signed 8-bit integer domain.

   type Waveform_Sample_I16 is record
      Real, Imag : Interfaces.Integer_16;
   end record;
   --  One Cartesian complex pair with signed 16-bit components; no scaling or clipping.
   --  @field Real Real (I) component, full signed 16-bit integer domain.
   --  @field Imag Imaginary (Q) component, full signed 16-bit integer domain.

   type Waveform_Sample_I8_Array is array (Positive range <>) of Waveform_Sample_I8;
   --  Ordered signed 8-bit Cartesian pairs with Positive indices and arbitrary lower bound.
   --  Null arrays are accepted; counts mean complex pairs, not bytes or individual components.

   type Waveform_Sample_I16_Array is array (Positive range <>) of Waveform_Sample_I16;
   --  Ordered signed 16-bit Cartesian pairs with Positive indices and arbitrary lower bound.
   --  Null arrays are accepted; counts mean complex pairs, not bytes or individual components.

   --  Owned caller values. Each Add copies into fresh native storage; native
   --  copies of one submitted buffer share that storage. No timing/execution
   --  guarantee is implied, including for empty Cartesian buffers.
   function Waveform_Buffer_I8 (Samples : Waveform_Sample_I8_Array) return Modulation_Config;
   --  Select kind 6 with signed 8-bit sample storage and fixed ComplexCartesian format. Copy pairs in caller index order. Each active definition per Add receives a fresh nonnull native shared vector, even when empty; native copies share that allocation, not Ada caller storage. Native vector max size and span arithmetic limit accepted sizes. No DMA, zero-copy lifetime, transfer or physical emission is promised.
   --  @param Samples Copied ordered complex pairs, including empty.
   --  @return Owned 8-bit WaveformBuffer configuration.

   function Waveform_Buffer_I16 (Samples : Waveform_Sample_I16_Array) return Modulation_Config;
   --  Select kind 6 with signed 16-bit sample storage and fixed ComplexCartesian format. Copy pairs in caller index order. Each active definition per Add receives a fresh nonnull native shared vector, even when empty; native copies share that allocation, not Ada caller storage. Native vector max size and span arithmetic limit accepted sizes. No DMA, zero-copy lifetime, transfer or physical emission is promised.
   --  @param Samples Copied ordered complex pairs, including empty.
   --  @return Owned 16-bit WaveformBuffer configuration.

   procedure Append_Waveform_Sample_I8
     (Config : in out Modulation_Config; Sample : Waveform_Sample_I8);
   --  Append one copied pair. Wrong kind or width raises Constraint Error before mutation; no normalization or hardware transfer.
   --  @param Config Owned kind-6 buffer with 8-bit components required.
   --  @param Sample Signed Cartesian pair to append in order.

   procedure Append_Waveform_Sample_I16
     (Config : in out Modulation_Config; Sample : Waveform_Sample_I16);
   --  Append one copied pair. Wrong kind or width raises Constraint Error before mutation; no normalization or hardware transfer.
   --  @param Config Owned kind-6 buffer with 16-bit components required.
   --  @param Sample Signed Cartesian pair to append in order.

   procedure Replace_Waveform_Samples_I8
     (Config : in out Modulation_Config; Samples : Waveform_Sample_I8_Array);
   --  Prepare a copied vector before assigning, preserving kind and 8-bit width. Wrong kind/width raises Constraint Error before mutation; temporary-allocation failure leaves the old vector unchanged. Empty replacement is valid. Add performs native vector bounds validation.
   --  @param Config Owned 8-bit WaveformBuffer required.
   --  @param Samples Complete replacement in caller index order; independently copied.

   procedure Replace_Waveform_Samples_I16
     (Config : in out Modulation_Config; Samples : Waveform_Sample_I16_Array);
   --  Prepare a copied vector before assigning, preserving kind and 16-bit width. Wrong kind/width raises Constraint Error before mutation; temporary-allocation failure leaves the old vector unchanged. Empty replacement is valid. Add performs native vector bounds validation.
   --  @param Config Owned 16-bit WaveformBuffer required.
   --  @param Samples Complete replacement in caller index order; independently copied.

   type Modulation_Table is private;
   --  Ordered owned definitions, initially empty; copies retain independent Ada values.
   --  References are zero-based and resolve only in the same interval active table.
   --  No prior Add, another interval or provider registry supplies missing entries.

   --  Owned configuration values only, not executable waveform validation.
   --  Degrees stay degrees; false update flags retain their numeric values.
   subtype Waveform_Profile_Index is Interfaces.Integer_32 range 0 .. Interfaces.Integer_32'Last;
   --  Nonnegative signed 32-bit profile index, 0 through Integer 32 Last.
   --  Must fit native int and refer to a profile in the same CustomWaveform at Add;
   --  there is no -1 sentinel for an inner waveform step.

   type Waveform_Profile is private;
   --  Owned profile values: zero femtoseconds, six zero double scalars and six False update flags by default.
   --  Phase/delta phase remain degrees, offsets/deltas Hz, slope the pinned HzSec
   --  quantity. False update flags retain their numeric fields. Signed zero and
   --  nonfinite classification survive native-double conversion, not NaN payload bits.
   --  No normalization, amplitude clamping, timing synthesis or update execution.

   type Waveform_Profile_Table is private;
   --  Ordered owned profile values, initially empty; append copies without deduplication.

   type Event_Waveform_Pattern is private;
   --  Owned ordered profile-index/iteration steps, initially empty; copies are independent.
   --  No chip duration is stored in a step: duration belongs to the selected profile.

   type Waveform_Pattern_List is private;
   --  Owned ordered event patterns, initially empty; repeated and empty patterns survive.
   --  The provider interprets pattern cycling; the bridge does not assign patterns to events.

   function Create_Waveform_Profile
     (Chip_Duration_Femtoseconds : Interfaces.Integer_64 := 0) return Waveform_Profile;
   --  Construct the duration with all six scalar values zero and update flags False.
   --  @param Chip_Duration_Femtoseconds Exact signed 64-bit femtoseconds, default zero; not quantized.
   --  @return Owned waveform profile.

   procedure Set_Chip_Duration_Femtoseconds
     (Profile : in out Waveform_Profile; Value : Interfaces.Integer_64);
   --  Replace the profile duration without quantization, arithmetic or positivity validation.
   --  @param Profile Owned profile.
   --  @param Value Exact signed 64-bit femtoseconds.

   procedure Set_Phase_Degrees
     (Profile : in out Waveform_Profile; Value : Long_Float; Update : Boolean);
   --  Replace the numeric value and its independent update flag; False does not erase Value. No wrapping, normalization, clamping or finite validation; copied native double retains signed zero and nonfinite classification.
   --  @param Profile Owned waveform profile.
   --  @param Value Phase in degrees.
   --  @param Update Provider request to apply this field when the profile is applied, not bridge execution.

   procedure Set_Delta_Phase_Degrees
     (Profile : in out Waveform_Profile; Value : Long_Float; Update : Boolean);
   --  Replace the numeric value and its independent update flag; False does not erase Value. No wrapping, normalization, clamping or finite validation; copied native double retains signed zero and nonfinite classification.
   --  @param Profile Owned waveform profile.
   --  @param Value Temporary phase addition in degrees.
   --  @param Update Provider request to apply this field when the profile is applied, not bridge execution.

   procedure Set_Frequency_Offset_Hz
     (Profile : in out Waveform_Profile; Value : Long_Float; Update : Boolean);
   --  Replace the numeric value and its independent update flag; False does not erase Value. No wrapping, normalization, clamping or finite validation; copied native double retains signed zero and nonfinite classification.
   --  @param Profile Owned waveform profile.
   --  @param Value Offset from center frequency in Hz.
   --  @param Update Provider request to apply this field when the profile is applied, not bridge execution.

   procedure Set_Delta_Frequency_Hz
     (Profile : in out Waveform_Profile; Value : Long_Float; Update : Boolean);
   --  Replace the numeric value and its independent update flag; False does not erase Value. No wrapping, normalization, clamping or finite validation; copied native double retains signed zero and nonfinite classification.
   --  @param Profile Owned waveform profile.
   --  @param Value Temporary frequency addition in Hz.
   --  @param Update Provider request to apply this field when the profile is applied, not bridge execution.

   procedure Set_Frequency_Slope_Hz_Sec
     (Profile : in out Waveform_Profile; Value : Long_Float; Update : Boolean);
   --  Replace the numeric value and its independent update flag; False does not erase Value. No wrapping, normalization, clamping or finite validation; copied native double retains signed zero and nonfinite classification.
   --  @param Profile Owned waveform profile.
   --  @param Value Frequency slope in the pinned HzSec quantity, forwarded verbatim.
   --  @param Update Provider request to apply this field when the profile is applied, not bridge execution.

   procedure Set_Amplitude_Factor
     (Profile : in out Waveform_Profile; Value : Long_Float; Update : Boolean);
   --  Replace the numeric value and its independent update flag; False does not erase Value. No wrapping, normalization, clamping or finite validation; copied native double retains signed zero and nonfinite classification.
   --  @param Profile Owned waveform profile.
   --  @param Value Amplitude-reduction factor, pinned intended range 0 through 1, not enforced by the bridge.
   --  @param Update Provider request to apply this field when the profile is applied, not bridge execution.

   procedure Append_Profile
     (Table   : in out Waveform_Profile_Table;
      Profile : Waveform_Profile;
      Index   : out Waveform_Profile_Index);
   --  Append an independent copied profile in order without merging identical values. Conversion of a full table length beyond the index domain raises Constraint Error before append.
   --  @param Table Owned profile table.
   --  @param Profile Complete value and update flags to copy.
   --  @param Index Returned zero-based position; not an Ada Positive index.

   procedure Append_Step
     (Pattern             : in out Event_Waveform_Pattern;
      Profile_Index       : Waveform_Profile_Index;
      Num_Chip_Iterations : Interfaces.Integer_32 := 1);
   --  Append one copied step. Add validates the index against this CustomWaveform final profile table even for zero/negative iteration counts; both integers must fit native int. No timing multiplication or repeat interpretation is performed.
   --  @param Pattern Owned ordered step list.
   --  @param Profile_Index Nonnegative zero-based index in the same definition.
   --  @param Num_Chip_Iterations Signed 32-bit iterations, default one; zero and negative values are copied.

   procedure Append_Pattern
     (Patterns : in out Waveform_Pattern_List; Pattern : Event_Waveform_Pattern);
   --  Append a complete independent pattern copy in order, retaining empty patterns and duplicates.
   --  @param Patterns Owned list.
   --  @param Pattern Ordered step values to copy.

   function Custom_Waveform
     (Profiles : Waveform_Profile_Table; Patterns : Waveform_Pattern_List) return Modulation_Config;
   --  Select kind 5 and copy both tables. Empty profiles are accepted only without actual steps. Add checks every local step reference and native integer domain before provider mutation; it does not sum chip timings, require equal event/pattern counts or generate samples.
   --  @param Profiles Ordered profile definitions.
   --  @param Patterns Ordered event patterns, including empty ones.
   --  @return Owned CustomWaveform configuration.

   procedure Replace_Custom_Waveform_Profiles
     (Config : in out Modulation_Config; Profiles : Waveform_Profile_Table);
   --  Copy the replacement before assignment; preserve the other collection. Wrong kind raises Constraint Error before mutation. Reference validity is deferred to Add against the final combined definition; no intermediate provider mutation occurs.
   --  @param Config Owned CustomWaveform (kind 5) required.
   --  @param Profiles Complete replacement values, independently copied in order, including empty.

   procedure Replace_Custom_Waveform_Patterns
     (Config : in out Modulation_Config; Patterns : Waveform_Pattern_List);
   --  Copy the replacement before assignment; preserve the other collection. Wrong kind raises Constraint Error before mutation. Reference validity is deferred to Add against the final combined definition; no intermediate provider mutation occurs.
   --  @param Config Owned CustomWaveform (kind 5) required.
   --  @param Patterns Complete replacement values, independently copied in order, including empty.

   type Phase_Chip_Array is array (Positive range <>) of Long_Float;
   --  Ordered phase radians in ascending caller index order, with arbitrary Positive lower bound.
   --  Null arrays are accepted; copied native doubles are not wrapped or normalized.

   subtype Modulation_Index is
     Interfaces.Integer_32 range Interfaces."-" (1) .. Interfaces.Integer_32'Last;
   --  Signed 32-bit reference domain -1 through Integer 32 Last.
   --  Minus one is no modulation; nonnegative values must fit native int and
   --  index the same interval active table at Add, not an Ada container position.

   No_Modulation : constant Modulation_Index := Interfaces."-" (1);
   --  Exact signed sentinel -1, not an explicit Tone definition or a missing table lookup.

   function Tone return Modulation_Config;
   --  Select bridge kind 0 and the empty native Tone alternative; no payload or synthesis.
   --  @return Owned Tone configuration, distinct from the -1 reference sentinel.

   type Modulation_Boolean_Array is array (Positive range <>) of Boolean;
   --  Ordered logical values in ascending Positive index order, including null arrays.
   --  Each Boolean is serialized separately as 0/1, then copied to native vector<bool>; not packed wire bits.

   function On_Off_Keying (Pulse_Enabled : Modulation_Boolean_Array) return Modulation_Config;
   --  Select kind 3 and copy ordered enables. At Add the list length must equal the interval sequence repeat count, even for an unused definition. One OOK entry cannot be referenced by two distinct TX events in that interval; repeated references from one event are allowed. No execution is performed.
   --  @param Pulse_Enabled Ordered pulse enables, including empty only with repeat count zero at Add.
   --  @return Owned OOK configuration.

   function CPFSK_Pulse
     (Chip_Data                                               : Modulation_Boolean_Array;
      Pulse_Duration_Femtoseconds, Chip_Duration_Femtoseconds : Interfaces.Integer_64;
      Frequency_Offset_Hz                                     : Long_Float)
      return Modulation_Config;
   --  Select kind 4, the CpfskPulse CommonModulations alternative despite upstream introductory CustomWaveform wording. Copy ordered chips without an invented chip-count/timing relationship or modulation algorithm.
   --  @param Chip_Data Copied logical chips, including empty.
   --  @param Pulse_Duration_Femtoseconds Exact signed 64-bit pulse duration in femtoseconds.
   --  @param Chip_Duration_Femtoseconds Exact signed 64-bit chip duration in femtoseconds.
   --  @param Frequency_Offset_Hz Copied native-double offset in Hz, without finite validation.
   --  @return Owned CPFSK configuration.

   procedure Append_OOK_Pulse_Enable (Config : in out Modulation_Config; Enabled : Boolean);
   --  Append a copied value in order. Wrong-kind mutation raises Constraint Error before changing Config. Final collection relationships are checked at Add, not here.
   --  @param Config Owned OnOffKeying configuration required.
   --  @param Enabled One logical pulse enable.

   procedure Replace_OOK_Pulse_Enables
     (Config : in out Modulation_Config; Pulse_Enabled : Modulation_Boolean_Array);
   --  Copy the entire replacement in caller index order before assigning; empty clears the active vector without changing the kind or other fields. Wrong kind raises Constraint Error before mutation. Allocation failure while preparing the temporary leaves the original vector unchanged.
   --  @param Config Owned OnOffKeying configuration required.
   --  @param Pulse_Enabled Replacement owned values, copied independently, including a null array.

   procedure Append_CPFSK_Chip (Config : in out Modulation_Config; Chip : Boolean);
   --  Append a copied value in order. Wrong-kind mutation raises Constraint Error before changing Config. Final collection relationships are checked at Add, not here.
   --  @param Config Owned CpfskPulse configuration required.
   --  @param Chip One logical chip.

   procedure Replace_CPFSK_Chips
     (Config : in out Modulation_Config; Chip_Data : Modulation_Boolean_Array);
   --  Copy the entire replacement in caller index order before assigning; empty clears the active vector without changing the kind or other fields. Wrong kind raises Constraint Error before mutation. Allocation failure while preparing the temporary leaves the original vector unchanged.
   --  @param Config Owned CpfskPulse configuration required.
   --  @param Chip_Data Replacement owned values, copied independently, including a null array.

   function Phase_Code
     (Chip_Duration_Femtoseconds : Interfaces.Integer_64; Phases_Rad : Phase_Chip_Array)
      return Modulation_Config;
   --  Select kind 1 with copied ordered phases, including empty. No duration/chip-count relationship is enforced; native doubles retain IEEE classification and signed zero, not NaN payload bits.
   --  @param Chip_Duration_Femtoseconds Exact signed 64-bit chip duration in femtoseconds, without quantization.
   --  @param Phases_Rad Copied phase radians in caller index order.
   --  @return Owned PhaseCode configuration.

   function Linear_Frequency_Modulation (Frequency_Extent_Hz : Long_Float) return Modulation_Config;
   --  Select kind 2. Pinned semantics describe start/end frequencies at center minus/plus half the extent; the bridge neither computes them nor synthesizes samples.
   --  @param Frequency_Extent_Hz Signed/nonfinite native-double extent in Hz; no finite/range check.
   --  @return Owned linear-frequency configuration.

   procedure Append_Phase_Chip (Config : in out Modulation_Config; Phase_Rad : Long_Float);
   --  Append a copied value in order. Wrong-kind mutation raises Constraint Error before changing Config. Final collection relationships are checked at Add, not here.
   --  @param Config Owned PhaseCode configuration required.
   --  @param Phase_Rad Phase radians; signed zero and nonfinite classification retained on native conversion.

   procedure Replace_Phase_Chips (Config : in out Modulation_Config; Phases_Rad : Phase_Chip_Array);
   --  Copy the entire replacement in caller index order before assigning; empty clears the active vector without changing the kind or other fields. Wrong kind raises Constraint Error before mutation. Allocation failure while preparing the temporary leaves the original vector unchanged.
   --  @param Config Owned PhaseCode configuration required.
   --  @param Phases_Rad Replacement owned values, copied independently, including a null array.

   --  Returned index is zero-based, independent of Ada container indexing.
   procedure Append_Modulation
     (Table : in out Modulation_Table; Config : Modulation_Config; Index : out Modulation_Index);
   --  Append a copied definition without deduplication; return its zero-based position. Index conversion beyond the signed domain raises Constraint Error before append. Native-int, active variant and same-interval reference validation occur at Add.
   --  @param Table Owned ordered definition table.
   --  @param Config Definition to copy; future edits do not affect the entry.
   --  @param Index Returned zero-based index, independent of Ada vector indexing.

   procedure Append_Modulation_Reference
     (Event : in out TX_Transmit_Event_Config; Index : Modulation_Index);
   --  Append an ordered signed reference and mark the TX collection present. Duplicates survive; Add resolves nonnegative indices only against the same interval final active table and checks native int. Minus one requests no modulation.
   --  @param Event Owned TX configuration.
   --  @param Index Reference in -1 through signed 32-bit maximum.

   procedure Clear_Modulation_References (Event : in out TX_Transmit_Event_Config);
   --  Clear the stored list and mark it absent, retaining native default {-1} during subsequent event construction; distinct from explicit empty.
   --  @param Event Owned TX configuration.

   procedure Set_Empty_Modulation_References (Event : in out TX_Transmit_Event_Config);
   --  Clear the stored list and mark it present-empty, so native setModIndex receives an empty vector rather than default {-1}.
   --  @param Event Owned TX configuration.

   function Create_TX_Transmit_Event
     (Event_ID                                  : Interfaces.Unsigned_32;
      Element_Group_Label                       : String;
      Start_Femtoseconds, Duration_Femtoseconds : Interfaces.Integer_64;
      Center_Frequency_Hz                       : Long_Float;
      TX_Attenuation_DB                         : Long_Float := 0.0;
      Rise_Duration_Femtoseconds                : Interfaces.Integer_64 := 0;
      Fall_Duration_Femtoseconds                : Interfaces.Integer_64 := 0)
      return TX_Transmit_Event_Config;
   --  Construct copied TX values without provider execution. Embedded NUL in the label raises Constraint Error; Add also checks UTF-8. Signed timings pass exactly without quantization, nonnegative or relationship checks; physical/provider constraints remain separate.
   --  @param Event_ID Exact unsigned 32-bit event identity; no uniqueness check.
   --  @param Element_Group_Label Copied provider lookup label, not a group index.
   --  @param Start_Femtoseconds Signed 64-bit start relative to the sequence iteration, in femtoseconds.
   --  @param Duration_Femtoseconds Signed 64-bit duration in femtoseconds.
   --  @param Center_Frequency_Hz Center frequency in Hz; copied as native double on Add.
   --  @param TX_Attenuation_DB Attenuation in dB, default zero; no power-mode capability query or step validation.
   --  @param Rise_Duration_Femtoseconds Signed leading-edge duration in femtoseconds, default zero.
   --  @param Fall_Duration_Femtoseconds Signed trailing-edge duration in femtoseconds, default zero.
   --  @return Independent TX configuration with the documented common defaults.

   procedure Set_Event_Stabilization_Point_Index
     (Event : in out TX_Transmit_Event_Config; Index : Interfaces.Unsigned_64);
   --  Replace the TX stabilization-point index. Zero-based provider vector index, always present. Add checks size t representability but not point membership or physical steering.
   --  @param Event Owned TX configuration to modify.
   --  @param Index Unsigned 64-bit provider index; default is zero.

   procedure Append_Applicable_TX_Element_Group
     (Event : in out TX_Transmit_Event_Config; Element_Group_Index : Interfaces.Unsigned_64);
   --  Append a copied TX group index, retaining order and duplicates. Empty is allowed. Add checks size t representability, not the Job group count or connectivity; these are provider indices, not labels or Ada positions.
   --  @param Event Owned TX configuration to extend.
   --  @param Element_Group_Index Unsigned 64-bit provider group index.

   procedure Set_TX_Attenuation_DB (Event : in out TX_Transmit_Event_Config; Value : Long_Float);
   --  Replace the TX attenuation value without clamping, quantization or capability queries. This is attenuation, not TX Power Level or TX Power Mode ID; native double preserves IEEE classification and signed zero, not NaN payload bits.
   --  @param Event Owned TX configuration.
   --  @param Value Attenuation in dB; nonfinite values are not rejected by the bridge.

   procedure Set_TX_Edge_Durations
     (Event                                : in out TX_Transmit_Event_Config;
      Rise_Femtoseconds, Fall_Femtoseconds : Interfaces.Integer_64);
   --  Replace copied TX edge durations without quantization or checking their sum against event duration. Pinned upstream describes zero as no attempted shaping and leaves support/shape to the MFA; the bridge does not implement shaping.
   --  @param Event Owned TX configuration.
   --  @param Rise_Femtoseconds Exact signed 64-bit leading-edge femtoseconds.
   --  @param Fall_Femtoseconds Exact signed 64-bit trailing-edge femtoseconds.

   procedure Set_Pulse_Detection_Settings
     (Event : in out RX_Receive_Event_Config; Settings : Pulse_Detection_Settings);
   --  Copy settings and mark RX pulse configuration present. Later edits to Settings do not change Event. Native Add constructs the active settings before provider mutation; support and detector behavior remain provider-defined.
   --  @param Event Owned RX configuration, not TX.
   --  @param Settings Complete owned value to copy, including ordered thresholds.

   procedure Clear_Pulse_Detection_Settings (Event : in out RX_Receive_Event_Config);
   --  Mark RX pulse settings absent; retained inactive Ada values are not submitted. Native construction leaves upstream default pulse settings rather than clearing a running detector.
   --  @param Event Owned RX configuration.

   type Execution_Type is (Normal, Conditional);
   --  Copied event/interval execution selector; the bridge implements no conditional trigger.
   --  @enum Normal Representation 0: normal execution request.
   --  @enum Conditional Representation 1: provider conditional-execution request.

   for Execution_Type use (Normal => 0, Conditional => 1);
   type Event_Termination_Type is (Inhibit_Event, Cancel_Event);
   --  Provider policy when a conditional event overlaps; not a bridge Cancel Job command.
   --  @enum Inhibit_Event Representation 0: upstream requests continued execution with outputs suppressed while the conditional event executes.
   --  @enum Cancel_Event Representation 1: upstream requests termination, or no execution if the conditional event precedes this event.

   for Event_Termination_Type use (Inhibit_Event => 0, Cancel_Event => 1);
   --  Ordered copied Stokes values, no normalization or finite checks. A third
   --  append raises Constraint_Error. Provider semantic validation remains final.
   procedure Append_Polarization
     (Event : in out RX_Receive_Event_Config; S0, S1, S2, S3 : Long_Float);
   --  RX event configuration only: Append one ordered copied four-double Stokes value in the array-face frame. Empty means default polarization; one/two values request single/dual polarization. A third append raises Constraint Error before mutation. No normalization, finite validation or physical beam steering is performed.
   --  @param Event Owned RX value; copies remain independent.
   --  @param S0 First Stokes component, native double on Add.
   --  @param S1 Second Stokes component.
   --  @param S2 Third Stokes component.
   --  @param S3 Fourth Stokes component.

   procedure Set_Polarization_Beam_Steer_Correction
     (Event : in out RX_Receive_Event_Config; Enabled : Boolean);
   --  RX event configuration only: Replace the copied beam-steering correction request, default False. Does not implement or confirm physical correction.
   --  @param Event Owned RX value; copies remain independent.
   --  @param Enabled Requested provider flag.

   procedure Set_Phase_Offset_Radians (Event : in out RX_Receive_Event_Config; Value : Long_Float);
   --  RX event configuration only: Replace the phase offset without wrapping or normalization. Native double conversion preserves signed zero and nonfinite classification, not NaN payload bits.
   --  @param Event Owned RX value; copies remain independent.
   --  @param Value Initial phase offset in radians, default zero.

   procedure Set_Event_Execution_Type
     (Event : in out RX_Receive_Event_Config; Value : Execution_Type);
   --  RX event configuration only: Replace the execution selector, default Normal; no conditional trigger is installed.
   --  @param Event Owned RX value; copies remain independent.
   --  @param Value Provider execution request.

   procedure Set_Event_Termination_Type
     (Event : in out RX_Receive_Event_Config; Value : Event_Termination_Type);
   --  RX event configuration only: Replace the conditional-overlap policy, default Inhibit Event; does not cancel a Job or execute an event.
   --  @param Event Owned RX value; copies remain independent.
   --  @param Value Provider termination policy.

   procedure Set_Allow_Delay_Start (Event : in out RX_Receive_Event_Config; Enabled : Boolean);
   --  RX event configuration only: Replace the delay-start permission, default False. Pinned semantics allow delay around a conditional event only as sequence time and MFA support permit; no bridge scheduling guarantee.
   --  @param Event Owned RX value; copies remain independent.
   --  @param Enabled Requested delay permission.

   procedure Set_Iteration_Hold_Count
     (Event : in out RX_Receive_Event_Config; Count : Interfaces.Unsigned_64);
   --  RX event configuration only: Replace the count of initial iterations to hold before event execution, default zero. Add checks provider size t, not its relationship to repeat count; no iterations are executed here.
   --  @param Event Owned RX value; copies remain independent.
   --  @param Count Unsigned 64-bit iteration count.

   procedure Set_Iteration_Termination_Count
     (Event : in out RX_Receive_Event_Config; Count : Interfaces.Unsigned_64);
   --  RX event configuration only: Replace the number of final sequence iterations in which the event is no longer executed, default zero. Add checks size t, not its relationship to repeat count.
   --  @param Event Owned RX value; copies remain independent.
   --  @param Count Unsigned 64-bit iteration count.

   procedure Set_Channelization_Enabled (Event : in out RX_Receive_Event_Config; Enabled : Boolean);
   --  RX event configuration only: Replace the channelization request, default False. No RX processing or TX channelizer is implemented or qualified by copying this flag.
   --  @param Event Owned RX value; copies remain independent.
   --  @param Enabled Requested provider channelization flag.

   procedure Append_Polarization
     (Event : in out TX_Transmit_Event_Config; S0, S1, S2, S3 : Long_Float);
   --  TX event configuration only: Append one ordered copied four-double Stokes value in the array-face frame. Empty means default polarization; one/two values request single/dual polarization. A third append raises Constraint Error before mutation. No normalization, finite validation or physical beam steering is performed.
   --  @param Event Owned TX value; copies remain independent.
   --  @param S0 First Stokes component, native double on Add.
   --  @param S1 Second Stokes component.
   --  @param S2 Third Stokes component.
   --  @param S3 Fourth Stokes component.

   procedure Set_Polarization_Beam_Steer_Correction
     (Event : in out TX_Transmit_Event_Config; Enabled : Boolean);
   --  TX event configuration only: Replace the copied beam-steering correction request, default False. Does not implement or confirm physical correction.
   --  @param Event Owned TX value; copies remain independent.
   --  @param Enabled Requested provider flag.

   procedure Set_Phase_Offset_Radians (Event : in out TX_Transmit_Event_Config; Value : Long_Float);
   --  TX event configuration only: Replace the phase offset without wrapping or normalization. Native double conversion preserves signed zero and nonfinite classification, not NaN payload bits.
   --  @param Event Owned TX value; copies remain independent.
   --  @param Value Initial phase offset in radians, default zero.

   procedure Set_Event_Execution_Type
     (Event : in out TX_Transmit_Event_Config; Value : Execution_Type);
   --  TX event configuration only: Replace the execution selector, default Normal; no conditional trigger is installed.
   --  @param Event Owned TX value; copies remain independent.
   --  @param Value Provider execution request.

   procedure Set_Event_Termination_Type
     (Event : in out TX_Transmit_Event_Config; Value : Event_Termination_Type);
   --  TX event configuration only: Replace the conditional-overlap policy, default Inhibit Event; does not cancel a Job or execute an event.
   --  @param Event Owned TX value; copies remain independent.
   --  @param Value Provider termination policy.

   procedure Set_Allow_Delay_Start (Event : in out TX_Transmit_Event_Config; Enabled : Boolean);
   --  TX event configuration only: Replace the delay-start permission, default False. Pinned semantics allow delay around a conditional event only as sequence time and MFA support permit; no bridge scheduling guarantee.
   --  @param Event Owned TX value; copies remain independent.
   --  @param Enabled Requested delay permission.

   procedure Set_Iteration_Hold_Count
     (Event : in out TX_Transmit_Event_Config; Count : Interfaces.Unsigned_64);
   --  TX event configuration only: Replace the count of initial iterations to hold before event execution, default zero. Add checks provider size t, not its relationship to repeat count; no iterations are executed here.
   --  @param Event Owned TX value; copies remain independent.
   --  @param Count Unsigned 64-bit iteration count.

   procedure Set_Iteration_Termination_Count
     (Event : in out TX_Transmit_Event_Config; Count : Interfaces.Unsigned_64);
   --  TX event configuration only: Replace the number of final sequence iterations in which the event is no longer executed, default zero. Add checks size t, not its relationship to repeat count.
   --  @param Event Owned TX value; copies remain independent.
   --  @param Count Unsigned 64-bit iteration count.

   procedure Set_Channelization_Enabled
     (Event : in out TX_Transmit_Event_Config; Enabled : Boolean);
   --  TX event configuration only: Replace the channelization request, default False. No RX processing or TX channelizer is implemented or qualified by copying this flag.
   --  @param Event Owned TX value; copies remain independent.
   --  @param Enabled Requested provider channelization flag.

   --  Portable provider indices, not labels. No local count/range relationship
   --  is inferred. Applicable group order and duplicates are preserved.
   procedure Set_Stab_Point_Index
     (Event : in out RX_Receive_Event_Config; Index : Interfaces.Unsigned_64);
   --  Replace the RX stabilization-point index. Zero-based provider vector index, not an Ada Positive position; always present. Add checks size t representability, not the interval point count or pointing capability.
   --  @param Event Owned RX configuration to modify.
   --  @param Index Unsigned 64-bit provider index; default is zero.

   procedure Append_Applicable_RX_Element_Group
     (Event : in out RX_Receive_Event_Config; Element_Group_Index : Interfaces.Unsigned_64);
   --  Append a copied RX group index, retaining order and duplicates. Empty is allowed. Add checks size t representability, not the Job group count or connectivity; these are provider indices, not labels or Ada positions.
   --  @param Event Owned RX configuration to extend.
   --  @param Element_Group_Index Unsigned 64-bit provider group index.

   function Create_RX_Receive_Event
     (Event_ID                    : Interfaces.Unsigned_32;
      Element_Group_Label         : String;
      Start_Femtoseconds          : Interfaces.Integer_64;
      Duration_Femtoseconds       : Interfaces.Integer_64;
      Center_Frequency_Hz         : Long_Float;
      Sample_Frequency_Hz         : Long_Float;
      AGC_Processing_Iterations   : Interfaces.Unsigned_64 := 0;
      Ignored_Post_AGC_Iterations : Interfaces.Unsigned_64 := 0;
      Max_Extension_Femtoseconds  : Interfaces.Integer_64 := 0) return RX_Receive_Event_Config;
   --  Construct copied RX values without provider execution. Embedded NUL in the label raises Constraint Error; Add also checks UTF-8. Signed timings pass exactly without quantization, nonnegative or relationship checks; physical/provider constraints remain separate.
   --  @param Event_ID Exact unsigned 32-bit event identity; no uniqueness check.
   --  @param Element_Group_Label Copied provider lookup label, not a group index.
   --  @param Start_Femtoseconds Signed 64-bit start relative to the sequence iteration, in femtoseconds.
   --  @param Duration_Femtoseconds Signed 64-bit duration in femtoseconds.
   --  @param Center_Frequency_Hz Center frequency in Hz; copied as native double on Add.
   --  @param Sample_Frequency_Hz RX sample frequency in Hz, without finite/range validation.
   --  @param AGC_Processing_Iterations AGC iteration count, default zero; must fit provider size t at Add.
   --  @param Ignored_Post_AGC_Iterations Ignored post-AGC count, default zero; must fit provider size t.
   --  @param Max_Extension_Femtoseconds Signed maximum local-function extension in femtoseconds, default zero (upstream disallows extension); not enforced by a bridge detector.
   --  @return Independent RX configuration with the documented common defaults.

   type Local_Function_Command is private;
   --  Owned local-function type/instance and ordered address/value writes.
   --  Defaults are type zero, instance zero and no writes. No function owner,
   --  hardware-register access, capability query or support confirmation is created.

   function Create_Local_Function_Command
     (Local_Function_Type_ID : Interfaces.Unsigned_32; Instance : Interfaces.Unsigned_64)
      return Local_Function_Command;
   --  Copy identifiers and start with an empty write list. Pinned upstream intends commands for functions associated with the selected VA at interval start; bridge submission does not establish function support or register effects.
   --  @param Local_Function_Type_ID Full unsigned 32-bit type/command identity, no capability lookup.
   --  @param Instance Unsigned 64-bit function instance identity, checked against native size t at Add.
   --  @return Owned command configuration.

   procedure Append_Local_Function_Write
     (Command : in out Local_Function_Command; Address, Value : Interfaces.Unsigned_64);
   --  Append an ordered copied address/value pair, retaining duplicates without merging, masking, endian conversion or hardware access.
   --  @param Command Owned command configuration.
   --  @param Address Full unsigned 64-bit offset in local-function address space, not a process pointer.
   --  @param Value Full unsigned 64-bit scalar write value.

   type RX_Job_Interval_Config is private;
   --  Copyable Ada-owned interval configuration, now supporting both ordered RX and TX
   --  events despite the historical RX name. No resource allocation or scheduling.
   --  Default collections are empty, status Never, execution Normal, power mode zero;
   --  TX events, modulations, endpoint connections, product parameters and user context
   --  each have independent absent/present-empty/present-nonempty configuration states.
   --  Absence skips the corresponding native setter; native defaults can equal an
   --  explicit empty value, so provider-observable distinction is not guaranteed.
   --  Set/append operations own copies; no manual Close or provider pointer escapes.
   --  Upstream sequence nonoverlap, switching time, event-end/sequence duration,
   --  quantization and hardware constraints remain application/provider obligations;
   --  the bridge does not implement a scheduler or infer these relationships.

   --  Owned value configuration only. Numeric addresses are not access values;
   --  application/provider remain responsible for endpoint matching, registered
   --  writable storage and its execution lifetime. No memory-safety guarantee.
   type Product_Stream_Params is private;
   --  Owned ordered RX group indices and endpoint descriptors, initially empty.
   --  Both vectors retain duplicates; groups-only/endpoints-only/empty are structurally
   --  accepted. Pinned endpoint IDs/order must match the relevant JobRequest endpoint
   --  vector; the bridge neither queries nor checks that relationship.
   --  Numeric addresses are not process pointers or owned registered memory.
   --  Application/provider remain responsible for valid writable storage, capacity
   --  and execution lifetime; this configuration does not guarantee memory safety.

   --  Exact copied labels; last composite-key occurrence wins at submission.
   --  No endpoint creation, DataPipe association or applicable-label setup.
   type Endpoint_Connections is private;
   --  Owned insertion-ordered label/label/endpoint triples, initially empty.
   --  At Add these become a nested unordered map: exact element-group label to
   --  data-pipe label to endpoint ID. Last occurrence of a composite label key wins;
   --  native unordered iteration is not insertion order. No group/pipe membership,
   --  connectivity, endpoint creation or provider association capability is validated.

   function Create_Endpoint_Connections return Endpoint_Connections;
   --  Construct an empty owned association value; setting it on an interval expresses present-empty, distinct from Clear.
   --  @return Empty copied-configuration collection.

   procedure Append_Endpoint_Connection
     (Connections                          : in out Endpoint_Connections;
      Element_Group_Label, Data_Pipe_Label : String;
      Endpoint_ID                          : Interfaces.Unsigned_64);
   --  Append copied exact labels and identity in order. Embedded NUL raises Constraint Error before append; native Add also validates UTF-8 and span structure. Empty labels, case and whitespace survive; empty pipe differs from literal default. Duplicate composite keys use last occurrence at native map construction.
   --  @param Connections Owned association values.
   --  @param Element_Group_Label Provider lookup label, not a numeric group index.
   --  @param Data_Pipe_Label Exact pipe lookup label, not a created DataPipe owner.
   --  @param Endpoint_ID Full unsigned 64-bit endpoint identity, no existence or connectivity query.

   procedure Set_Interval_Endpoint_Connections
     (Interval : in out RX_Job_Interval_Config; Connections : Endpoint_Connections);
   --  Copy complete connections and mark presence. Empty calls native setEndpoints with an empty map; source edits cannot affect the copied interval. This configures values, not physical routing or applicable-group labels.
   --  @param Interval Owned interval configuration.
   --  @param Connections Owned triples to copy; duplicate handling occurs at Add.

   procedure Clear_Interval_Endpoint_Connections (Interval : in out RX_Job_Interval_Config);
   --  Clear stored triples and restore absence; skip native setEndpoints on future construction, not a live disconnection operation.
   --  @param Interval Owned interval configuration.

   function Create_Product_Stream_Params return Product_Stream_Params;
   --  Create empty RX-group and endpoint vectors without provider or memory operations.
   --  @return Owned empty parameter value; interval presence is set separately.

   procedure Append_Product_Stream_RX_Group
     (Params : in out Product_Stream_Params; Index : Interfaces.Unsigned_64);
   --  Append a copied index in order, retaining duplicates. Add checks size t representability, not membership in JobRequest groups or endpoint matching.
   --  @param Params Owned stream parameters.
   --  @param Index Unsigned 64-bit provider RX-group index, not a label or Ada position.

   procedure Append_Product_Stream_Endpoint
     (Params                                : in out Product_Stream_Params;
      Endpoint_ID, Start_Address, Max_Bytes : Interfaces.Unsigned_64);
   --  Append an ordered descriptor copy, retaining duplicates. No dereference, mapping, registration, address-plus-size arithmetic or transfer; all three full unsigned 64-bit domains, including zero, are accepted.
   --  @param Params Owned stream parameters.
   --  @param Endpoint_ID Provider endpoint identity, not an owned endpoint.
   --  @param Start_Address Numeric starting virtual-address descriptor, not an Ada access value.
   --  @param Max_Bytes Unsigned 64-bit maximum destination bytes; unlike waveform-stream Num Bytes, not narrowed to size t.

   procedure Set_Interval_Product_Stream_Params
     (Interval : in out RX_Job_Interval_Config; Params : Product_Stream_Params);
   --  Copy the complete value and mark it present; an empty value explicitly calls the native setter with empty vectors. Later source edits do not affect Interval. No hardware destination is validated.
   --  @param Interval Owned interval configuration.
   --  @param Params Independent parameter value to copy.

   procedure Clear_Interval_Product_Stream_Params (Interval : in out RX_Job_Interval_Config);
   --  Mark product parameters absent; inactive retained Ada payload is not submitted and native construction retains upstream defaults. This does not clear live hardware memory.
   --  @param Interval Owned interval configuration.

   procedure Append_Interval_Local_Function_Command
     (Interval : in out RX_Job_Interval_Config; Command : Local_Function_Command);
   --  Append an independent command copy in order, including duplicate commands/writes. Full-batch construction precedes provider Add; no register write or execution is confirmed.
   --  @param Interval Owned interval configuration.
   --  @param Command Complete command value to copy.

   procedure Set_Interval_TX_Power_Mode_ID
     (Interval : in out RX_Job_Interval_Config; Value : Interfaces.Unsigned_32);
   --  Replace the copied mode identifier, default zero; no presence flag, capability query or mode/JobRequest relationship check. Distinct from TX Power Level and TX attenuation in dB; not a power-setting guarantee.
   --  @param Interval Owned interval configuration.
   --  @param Value Exact unsigned 32-bit provider mode identity.

   procedure Set_Interval_Activity_ID
     (Interval : in out RX_Job_Interval_Config; Value : Byte_Array);
   --  Replace activity bytes in caller order; arbitrary binary bytes, including NUL/high-bit and empty. No optional presence flag, UUID/UCI interpretation or context schema. This clears then appends, not the temporary-vector replacement guarantee of user context.
   --  @param Interval Owned interval configuration.
   --  @param Value Copied activity identity bytes, independent of user context.

   -- Copied binary values; absent and explicitly present empty are distinct.
   procedure Set_Interval_User_Context
     (Interval : in out RX_Job_Interval_Config; Bytes : Byte_Array);
   --  Prepare a copied arbitrary binary byte vector then move it into Interval and mark present, including empty. Temporary preparation failure leaves old context unchanged. Native Add stores exactly vector<uint8 t> in std::any, not a universal RF MEL context schema; provider must understand that concrete type. No text decoding or provider interpretation is guaranteed.
   --  @param Interval Owned interval; context independent of Activity ID.
   --  @param Bytes Replacement bytes in caller index order, including NUL/high-bit bytes and null arrays.

   procedure Append_Interval_User_Context_Byte
     (Interval : in out RX_Job_Interval_Config; Value : Interfaces.Unsigned_8);
   --  Append one arbitrary binary byte and mark context present after successful insertion. From absence starts a new list; from present-empty extends it. Native concrete std::any type is vector<uint8 t>; provider compatibility remains required.
   --  @param Interval Owned context configuration.
   --  @param Value Full unsigned byte, 0 through 255, copied without decoding.

   procedure Set_Empty_Interval_User_Context (Interval : in out RX_Job_Interval_Config);
   --  Clear bytes and mark present-empty; native Add calls the context setter with an empty owned vector<uint8 t> in std::any. Distinct from absence, not proof the provider interprets this type.
   --  @param Interval Owned interval configuration.

   procedure Clear_Interval_User_Context (Interval : in out RX_Job_Interval_Config);
   --  Clear bytes and mark absent, skipping the context setter. No typed upstream getter is called on absent or mismatched std::any data; the pinned getter is unsafe for those cases.
   --  @param Interval Owned interval configuration.

   procedure Set_Interval_Execution_Type
     (Interval : in out RX_Job_Interval_Config; Value : Execution_Type);
   --  Replace the copied interval execution selector, default Normal; independent of event selectors. Does not install conditional triggers or validate event/interval relationships.
   --  @param Interval Owned configuration.
   --  @param Value Provider execution request.

   procedure Append_Stab_Point (Interval : in out RX_Job_Interval_Config; Value : Pointing);
   --  Append a complete copied pointing value in order, preserving its active kind and exact units. Add validates active UTC fractions, not event-index membership or physical pointing; no coordinate conversion or beam steering.
   --  @param Interval Owned interval configuration.
   --  @param Value Pointing value to copy; later source changes do not affect the interval.

   type Interval_Status_Enable is (Never, Always, On_Exception);
   --  Provider reporting policy, not callback registration or proof of delivery.
   --  @enum Never Representation 0: do not request interval status; default, no bridge registration required for Add.
   --  @enum Always Representation 1: request status always; Add requires usable bridge registration.
   --  @enum On_Exception Representation 2: request exception status; Add requires usable bridge registration.

   for Interval_Status_Enable use (Never => 0, Always => 1, On_Exception => 2);
   procedure Set_Interval_Status_Enable
     (Interval : in out RX_Job_Interval_Config; Mode : Interval_Status_Enable);
   --  Replace the reporting policy. Always/OnException require successful usable bridge status registration before Add; neither setting nor successful Add guarantees callback delivery. Never needs no registration.
   --  @param Interval Owned interval configuration.
   --  @param Mode Provider reporting policy, default Never.

   function Create_RX_Job_Interval
     (Interval_ID                        : Interfaces.Unsigned_32;
      Sequence_Duration_Femtoseconds     : Interfaces.Integer_64;
      Job_Details_ID                     : Interfaces.Unsigned_32 := 0;
      Interval_Start_Femtoseconds        : Interfaces.Integer_64 :=
        Continue_From_Previous_Femtoseconds;
      Interval_Starting_Gap_Femtoseconds : Interfaces.Integer_64 := 0;
      Sequence_Repeat_Count              : Interfaces.Unsigned_64 := 1;
      Calibration_Duration_Femtoseconds  : Interfaces.Integer_64 := 0;
      Interval_Ending_Gap_Femtoseconds   : Interfaces.Integer_64 := 0;
      Phase_Coherence_With_Prior         : Boolean := False;
      Iterations_Per_Signal              : Interfaces.Unsigned_64 := 0;
      Max_Data_Rate_BPS                  : Long_Float := 0.0;
      Max_Sample_Rate_Hz                 : Long_Float := 0.0) return RX_Job_Interval_Config;
   --  Copy interval scalars with empty RX events, points, commands and activity; optional collections absent, Normal execution, power mode zero and status Never. All signed timings, including negative values, pass without quantization or relationship checks. Counts must fit provider size t at Add; no infinite-repeat meaning is assigned to zero. Native doubles retain signed zero and nonfinite classification without range validation, not NaN payload bits.
   --  @param Interval_ID Exact unsigned 32-bit interval identity; no uniqueness validation.
   --  @param Sequence_Duration_Femtoseconds Signed 64-bit sequence duration in femtoseconds.
   --  @param Job_Details_ID Unsigned 32-bit JobDetail identity, default zero; no equality check with the receiving Job.
   --  @param Interval_Start_Femtoseconds Signed femtoseconds relative to Job start; default pinned continuation zero, ambiguous with ordinary zero start.
   --  @param Interval_Starting_Gap_Femtoseconds Signed femtoseconds before the initial sequence, default zero.
   --  @param Sequence_Repeat_Count Unsigned 64-bit sequence repetitions, default one; size t representability checked at Add.
   --  @param Calibration_Duration_Femtoseconds Signed femtoseconds for calibration following sequence iterations, default zero.
   --  @param Interval_Ending_Gap_Femtoseconds Signed femtoseconds following calibration, default zero.
   --  @param Phase_Coherence_With_Prior Requested coherence with preceding interval, default False; not a bridge phase guarantee.
   --  @param Iterations_Per_Signal Unsigned repetition interval for product-availability signaling, default zero; upstream zero suppresses periodic signaling but describes a final-Job signal. No bridge delivery guarantee.
   --  @param Max_Data_Rate_BPS Per-channel maximum bits per second, native double, default zero.
   --  @param Max_Sample_Rate_Hz Per-channel maximum sample rate in Hz, native double, default zero.
   --  @return Owned interval configuration, not a scheduled or executing interval.

   procedure Append_RX_Event
     (Interval : in out RX_Job_Interval_Config; Event : RX_Receive_Event_Config);
   --  Append an independent complete RX event copy in order, retaining duplicates. Subsequent edits to the source event do not affect the interval; no provider submission occurs.
   --  @param Interval Owned interval configuration.
   --  @param Event Owned RX configuration to copy.

   procedure Append_TX_Event
     (Interval : in out RX_Job_Interval_Config; Event : TX_Transmit_Event_Config);
   --  Append an independent complete TX event copy in order, retaining duplicates. Subsequent edits to the source event do not affect the interval; no provider submission occurs. Marks TX events present, including their ordered modulation references.
   --  @param Interval Owned interval configuration.
   --  @param Event Owned TX configuration to copy.

   procedure Set_Modulations (Interval : in out RX_Job_Interval_Config; Table : Modulation_Table);
   --  Copy the ordered definition table and mark it present, including empty. Later source edits are independent. All active definitions and final same-interval TX references validate at Add; no provider registry or cross-interval lookup.
   --  @param Interval Owned interval configuration.
   --  @param Table Final definitions to copy, retaining order and duplicates.

   procedure Clear_Modulations (Interval : in out RX_Job_Interval_Config);
   --  Clear definitions and restore absence independently of TX events/references. Nonnegative references remaining in an active TX list then fail final Add validation; no automatic reference repair.
   --  @param Interval Owned interval configuration.

   procedure Set_Empty_Modulations (Interval : in out RX_Job_Interval_Config);
   --  Clear definitions and mark the table present-empty, invoking native setModulations with an empty vector. Nonnegative active references cannot resolve; -1 remains permitted.
   --  @param Interval Owned interval configuration.

   --  Set empty expresses presence; Clear restores absence.
   procedure Set_Empty_TX_Events (Interval : in out RX_Job_Interval_Config);
   --  Clear TX events and mark present-empty so native setTxEvents receives an empty vector. RX events and modulation table remain unchanged.
   --  @param Interval Owned interval configuration.

   procedure Clear_TX_Events (Interval : in out RX_Job_Interval_Config);
   --  Clear TX events and restore absence, skipping the native TX setter on subsequent construction. RX events and modulation definitions remain unchanged; no provider cancellation occurs.
   --  @param Interval Owned interval configuration.

   type RX_Job_Interval_List is private;
   --  Owned ordered interval values, initially empty; copies are independent.
   --  Append retains duplicates; Add submits in list order, not interval-ID sort order.

   procedure Append_Job_Interval
     (Intervals : in out RX_Job_Interval_List; Interval : RX_Job_Interval_Config);
   --  Append a complete independent interval copy in order, retaining duplicate IDs and values; no provider call.
   --  @param Intervals Owned interval list.
   --  @param Interval Configuration to copy, including nested values and presence flags.

   function Job_Interval_Count (Intervals : RX_Job_Interval_List) return Natural;
   --  Read only the local list length; no Job/provider query.
   --  @param Intervals Owned list to inspect.
   --  @return Number of stored interval values; Natural conversion is checked.

   --  Synchronous, externally serialized with same-Job calls and Close.
   --  Add/Flush reject after Finalize or full Cancel has been attempted.
   --  Cancel_Remaining is repeatable, including during/after Finalize, until
   --  full Cancel has been attempted. Failures raise Provider_Error; no retry.
   procedure Add_RX_Job_Intervals (Object : in out Job; Intervals : RX_Job_Interval_List);
   --  Synchronously serialize current safe values through native contract version 16, including mixed RX/TX. Historical v1-v15 contracts retain frozen layouts/domains: v16 uses the v8 base, TX-v3 and modulation-v5 plus context, not interchangeable array strides. Full-batch structural validation and local copied-value preparation precede one unlocked provider Add, including empty batches. Ordered intervals/events and duplicates survive. Borrowed transient Ada/C backing lasts through the call; native values own copies (native buffer copies share fresh sample vectors). Invalid final configuration raises Constraint Error before provider Add; lifecycle, registration and provider failures raise Provider Error. Add requires an open Job before any Finalize/full Cancel attempt, even if that attempt failed. Enabled reporting requires usable prior registration; no delivery guarantee. Externally serialize all same-Job calls/Close. No automatic retry: provider may mutate before throwing, and normal void return proves neither scheduling nor event execution/RF emission. Snapshot remains unchanged.
   --  @param Object Open Job with Add admission still available.
   --  @param Intervals Complete owned ordered batch; not consumed by submission.

   procedure Add_Job_Intervals (Object : in out Job; Intervals : RX_Job_Interval_List)
   renames Add_RX_Job_Intervals;
   --  Public rename of Add RX Job Intervals, not a separate native implementation. Identical version-16 serialization, ownership, validation, registration, lifecycle/error and no-retry contracts apply; this spelling makes mixed RX/TX scope explicit.
   --  @param Object Open Job, externally serialized with all same-Job calls/Close.
   --  @param Intervals Ordered copied configurations, including an empty batch.

   procedure Flush_Job (Object : in out Job);
   --  Synchronously call provider flush once per admitted invocation, requesting buffered intervals be sent. Allowed only before any Finalize/full Cancel attempt, even a failed attempt. Does not Finalize or alter copied snapshots. Provider failures raise Provider Error with no retry; mutation may precede a throw. Normal void return does not prove delivery, scheduling or RF execution. No status registration required.
   --  @param Object Open Job; externally serialize same-Job calls and Close.

   procedure Cancel_Remaining_Job_Intervals (Object : in out Job);
   --  Synchronously request removal of unexecuted intervals; distinct from full Cancel Job and does not consume that full-Cancel attempt. Repeatable before/during/after Finalize until any full Cancel attempt, including False or exception. No status registration required. Each admitted call invokes provider once, with no automatic retry; mutation may precede Provider Error. Normal void return proves neither effective cancellation nor notification; immutable snapshot is unchanged.
   --  @param Object Open Job; externally serialize same-Job calls and Close.

   --  Repeatable, including before/during/after Finalize, until full Cancel
   --  has been attempted (even False or an exception). No status stream is
   --  required. Normal return means only the provider's void method returned
   --  without throwing, not acceptance, effective extension or notification.
   --  Exact signed femtoseconds; no validation, arithmetic or quantization.
   --  Never automatically retries. Explicit retries after failure require
   --  provider-specific knowledge because mutation may precede an exception.
   --  Same-Job calls/Close are externally serialized; snapshot is unchanged.
   procedure Extend_Job_Event
     (Object                      : in out Job;
      Interval_ID                 : Interfaces.Unsigned_32;
      Event_ID                    : Interfaces.Unsigned_32;
      Added_Duration_Femtoseconds : Interfaces.Integer_64);
   --  Synchronously forward exact IDs and signed femtoseconds once per admitted call. Repeatable before/during/after Finalize until any full Cancel attempt, including False or exception. No lookup, cumulative budget accounting, duration arithmetic, quantization or automatic retry. Provider may mutate before Provider Error; explicit retries require provider-specific knowledge. Normal void return proves neither support, effective extension nor notification. Status registration is optional and snapshot unchanged; external same-Job/Close serialization required.
   --  @param Object Open Job not blocked by a full Cancel attempt.
   --  @param Interval_ID Full unsigned 32-bit interval identity, no bridge existence check.
   --  @param Event_ID Full unsigned 32-bit event identity, no bridge membership check.
   --  @param Added_Duration_Femtoseconds Exact signed 64-bit added duration; zero, negative and extrema pass unchanged, not an interval-start sentinel.

   type Job_Status is
     (None, In_Progress, Complete, Failed_Invalid_ID, Failed_Interrupted, Failed_Invalid_State);
   --  Provider status returned by the one Finalize future, not a live RF activity
   --  stream. Every known value is result data, including failure values. Even
   --  an unset or in-progress result is cached once that future is consumed.
   --  @enum None Provider reported no specific Job status.
   --  @enum In_Progress Provider reported an accepted Job in progress.
   --  @enum Complete Provider reported completion, not a bridge RF transmission guarantee.
   --  @enum Failed_Invalid_ID Provider reported failure due to invalid ID.
   --  @enum Failed_Interrupted Provider reported interruption failure.
   --  @enum Failed_Invalid_State Provider reported failure due to invalid state.
   for Job_Status use
     (None                 => 0,
      In_Progress          => 1,
      Complete             => 2,
      Failed_Invalid_ID    => 3,
      Failed_Interrupted   => 4,
      Failed_Invalid_State => 5);
   type Cancel_Error is (None);
   --  Pinned cancellation error domain; semantic success is a separate Boolean.
   --  @enum None Only known error code; does not by itself mean cancellation succeeded.
   for Cancel_Error use (None => 0);
   type Cancel_Result is private;
   --  Copied cached cancellation Boolean and error. False with None is a valid
   --  provider rejection; native/provider exceptions are raised, not fabricated
   --  as another Boolean result. Contains no Job or provider owner.
   function Cancelled (Result : Cancel_Result) return Boolean;
   --  Read the provider CancelStatus conversion to Boolean independently of error.
   --  @param Result Copied cancellation result.
   --  @return True for provider-reported cancellation success; False for semantic failure.
   function Error_Code (Result : Cancel_Result) return Cancel_Error;
   --  Read the validated cancellation error, not the request error-code domain.
   --  @param Result Copied cancellation result.
   --  @return None for the only supported pinned code, also possible with False.
   procedure Finalize_Job (Object : in out Job);
   --  Synchronously invoke provider finalize once, indicating that Job population
   --  is finished; hand its future to one native completion worker. Normal return
   --  means invocation/worker setup succeeded, not future completion or physical
   --  RF activation. Once invocation is attempted, repeated success returns
   --  normally and repeated synchronous failure raises the cached provider error
   --  without resending. An invalid future also raises a provider error. Bridge
   --  allocation failure before invocation does not consume the provider attempt.
   --  Cancel attempted first prohibits a new Finalize, even if Cancel returned
   --  False or raised; an already attempted Finalize remains repeatable from
   --  cache after Cancel. Future exceptions are reported by status Wait instead.
   --  Post-invocation worker failure retains future/provider/library state.
   --  @param Object Open Job; externally serialize all same-Job calls and Close.
   function Wait_Job_Status (Object : Job; Timeout_Milliseconds : Natural) return Job_Status;
   --  Await the Finalize future's cached completion, not a physical RF state.
   --  Waiting before a Finalize attempt raises a provider error. Timeout raises
   --  a timeout error without cancellation or ownership consumption. Synchronous
   --  Finalize failures, future exceptions, unknown statuses and native failures
   --  raise a provider error; the six known statuses, including failed statuses,
   --  return as data. Repeated completion waits return the cached status or error,
   --  even for an unset or in-progress value; no further provider polling occurs.
   --  @param Object Open Job retaining the future/provider graph despite parent closure.
   --  @param Timeout_Milliseconds Maximum completion wait in milliseconds; zero polls.
   --  @return Known status supplied by the single consumed Finalize future.
   function Cancel_Job (Object : in out Job) return Cancel_Result;
   --  Synchronously call cancelJob once and cache its Boolean/error separately.
   --  Repeats return the same result or raise the cached provider error without
   --  retry. Unknown error codes and provider/native exceptions raise a provider
   --  error, not False. A normal False/None result is permitted. The provider may
   --  complete the Finalize future during cancellation; this call is not that
   --  future wait and does not destroy the Job. A first Cancel attempt prohibits
   --  starting Finalize afterward regardless of cancellation success. No physical
   --  RF cessation or universal provider timing guarantee is inferred.
   --  @param Object Open Job; externally serialize all same-Job operations and Close.
   --  @return Copied provider-reported cancellation success and validated error code.
   procedure Close (Object : in out Job);
   --  Idempotently consume the public owner, without implicit Finalize or Cancel.
   --  With no pending finalize work, release JobDetail, then provider VA, then
   --  the sibling C2 claim; report synchronous deferred shutdown failure as a
   --  provider error after closure. With pending work, do not wait: its worker
   --  retains the graph and DSO until completion, potentially indefinitely. A
   --  throwing shutdown retains the uncertain C2/library graph without retry;
   --  detached cleanup cannot report it. Controlled finalization never raises.
   --  Ada-owned Claim-time snapshots remain unchanged and readable after Close.
   --  @param Object Job owner to consume; externally serialize with its other calls.
   function Actual_Start_Seconds (Object : Job) return Interfaces.Integer_64;
   --  Read the Claim-time provider actual-start report, not the current clock or
   --  proof of RF activity. Snapshot access does not require an open native handle.
   --  @param Object Job holding an Ada-owned snapshot, also readable after Close.
   --  @return Exact signed integral UTC seconds from the provider report.
   function Actual_Start_Femtoseconds (Object : Job) return Interfaces.Integer_64;
   --  Read the fractional part of the Claim-time provider actual-start report.
   --  @param Object Job holding the immutable Ada-owned snapshot.
   --  @return Fractional femtoseconds supplied by pinned UTCTime, separate from seconds.
   function Total_Job_Duration_Femtoseconds (Object : Job) return Interfaces.Integer_64;
   --  Read the Claim-time duration report without refreshing it after commands.
   --  This is a provider scheduling report, not measured RF emission time.
   --  @param Object Job holding the copied snapshot, also readable after Close.
   --  @return Exact signed duration count in femtoseconds.
   function VA_Instance_ID (Object : Job) return Interfaces.Unsigned_32;
   --  Read the assigned instance ID reported at Claim, not an array position.
   --  @param Object Job holding the copied immutable snapshot.
   --  @return Exact unsigned 32-bit VA instance identity.
   function VA_Definition_ID (Object : Job) return Interfaces.Unsigned_32;
   --  Read the definition ID reported at Claim, distinct from the instance ID.
   --  @param Object Job holding the copied immutable snapshot.
   --  @return Exact unsigned 32-bit VA definition identity.
   function Job_Details_ID (Object : Job) return Interfaces.Unsigned_32;
   --  Read the JobDetail identity reported at Claim, distinct from request ID.
   --  @param Object Job holding the copied immutable snapshot.
   --  @return Exact unsigned 32-bit Job details identity.
   function Job_Request_ID (Object : Job) return Interfaces.Unsigned_32;
   --  Read the request identity reported by JobDetail at Claim.
   --  @param Object Job holding the copied immutable snapshot.
   --  @return Exact unsigned 32-bit provider-reported Job request ID.
   function Lookahead_Femtoseconds (Object : Job) return Interfaces.Integer_64;
   --  Read the Claim-time next-JIB lookahead report, without running JIB controls
   --  or refreshing provider state. A report is not a scheduling guarantee.
   --  @param Object Job holding the copied immutable snapshot.
   --  @return Exact signed lookahead count in femtoseconds.
   function RX_Stream_ID_Count (Object : Job) return Natural;
   --  Read the copied getRxStreamIDs(0) count, not the total across all groups.
   --  @param Object Job holding the immutable snapshot, also readable after Close.
   --  @return Number of group-zero provider stream IDs, zero for a default owner.
   function RX_Stream_ID_At (Object : Job; Index : Positive) return Interfaces.Unsigned_32;
   --  Read a copied group-zero stream ID in provider vector order, not sorted.
   --  This identifies a reported stream, not proof that any samples arrived.
   --  @param Object Job holding the copied immutable stream list.
   --  @param Index One-based position; a position beyond count raises a constraint error.
   --  @return Exact unsigned 32-bit stream ID at that position.
private
   pragma
     Compile_Time_Error
       (Execution_Type'Enum_Rep (Normal) /= Integer (AMS.MEL_C_API.RF_Execution_Normal),
        "RF execution representation mismatch");
   pragma
     Compile_Time_Error
       (Execution_Type'Enum_Rep (Conditional) /= Integer (AMS.MEL_C_API.RF_Execution_Conditional),
        "RF execution representation mismatch");
   pragma
     Compile_Time_Error
       (Event_Termination_Type'Enum_Rep (Inhibit_Event)
          /= Integer (AMS.MEL_C_API.RF_Event_Termination_Inhibit),
        "RF execution representation mismatch");
   pragma
     Compile_Time_Error
       (Event_Termination_Type'Enum_Rep (Cancel_Event)
          /= Integer (AMS.MEL_C_API.RF_Event_Termination_Cancel),
        "RF execution representation mismatch");
   pragma
     Compile_Time_Error
       (Interval_Status_Enable'Enum_Rep (Never) /= Integer (AMS.MEL_C_API.Rf_Interval_Status_Never),
        "RF reporting enum representation mismatch");
   pragma
     Compile_Time_Error
       (Interval_Status_Enable'Enum_Rep (Always)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Status_Always),
        "RF reporting enum representation mismatch");
   pragma
     Compile_Time_Error
       (Interval_Status_Enable'Enum_Rep (On_Exception)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Status_On_Exception),
        "RF reporting enum representation mismatch");
   pragma
     Compile_Time_Error
       (Job_Status'Enum_Rep (Failed_Invalid_State)
          /= Integer (AMS.MEL_C_API.RF_Job_Status_Failed_Invalid_State),
        "RF Job status representation mismatch");
   pragma
     Compile_Time_Error
       (Cancel_Error'Enum_Rep (None) /= Integer (AMS.MEL_C_API.RF_Cancel_Error_None),
        "RF CancelError representation mismatch");
   type UTC_Time is record
      Seconds, Fraction : Interfaces.Integer_64 := 0;
   end record;
   type Pointing_Components is array (Positive range 1 .. 6) of Long_Float;
   type Pointing (Kind : Pointing_Kind := Face_Relative) is record
      Components : Pointing_Components := [others => 0.0];
      Time       : UTC_Time;
   end record;
   package Pointing_Vectors is new Ada.Containers.Vectors (Positive, Pointing);
   package RX_Index_Vectors is new
     Ada.Containers.Vectors (Positive, Interfaces.Unsigned_64, Interfaces."=");
   type Stokes_Components is array (Positive range 1 .. 4) of Long_Float;
   package Stokes_Vectors is new Ada.Containers.Vectors (Positive, Stokes_Components);
   package Byte_Vectors is new
     Ada.Containers.Vectors (Positive, Interfaces.Unsigned_8, Interfaces."=");
   type Pulse_Threshold is record
      Leading_Edge_DB, Trailing_Edge_DB : Long_Float;
   end record;
   package Pulse_Threshold_Vectors is new Ada.Containers.Vectors (Positive, Pulse_Threshold);
   type Pulse_Detection_Settings is record
      Reference                                    : Pulse_Threshold_Reference := DBQ;
      Leading_M, Leading_N, Trailing_M, Trailing_N : Interfaces.Unsigned_8 := 0;
      Min_Pulse_Width_Femtoseconds                 : Interfaces.Integer_64 := 0;
      Time_Tag_Threshold                           : Pulse_Time_Tag_Threshold := At_50_Percent;
      Thresholds                                   : Pulse_Threshold_Vectors.Vector;
   end record;
   type RX_Receive_Event_Config is record
      Event_ID                                               : Interfaces.Unsigned_32;
      Label                                                  :
        Ada.Strings.Unbounded.Unbounded_String;
      Start_Femtoseconds, Duration_Femtoseconds              : Interfaces.Integer_64;
      Center_Frequency_Hz, Sample_Frequency_Hz               : Long_Float;
      AGC_Processing_Iterations, Ignored_Post_AGC_Iterations : Interfaces.Unsigned_64;
      Max_Extension_Femtoseconds                             : Interfaces.Integer_64;
      Stab_Point_Index                                       : Interfaces.Unsigned_64 := 0;
      Applicable_RX_Element_Groups                           : RX_Index_Vectors.Vector;
      Polarization                                           : Stokes_Vectors.Vector;
      Beam_Correction                                        : Boolean := False;
      Phase_Offset                                           : Long_Float := 0.0;
      Execution                                              : Execution_Type := Normal;
      Termination                                            : Event_Termination_Type :=
        Inhibit_Event;
      Allow_Delay                                            : Boolean := False;
      Hold_Count, Termination_Count                          : Interfaces.Unsigned_64 := 0;
      Channelization                                         : Boolean := False;
      Has_Pulse_Settings                                     : Boolean := False;
      Pulse_Settings                                         : Pulse_Detection_Settings;
   end record;
   package RX_Event_Vectors is new Ada.Containers.Vectors (Positive, RX_Receive_Event_Config);
   package Phase_Chip_Vectors is new Ada.Containers.Vectors (Positive, Long_Float);
   package Modulation_Boolean_Vectors is new Ada.Containers.Vectors (Positive, Boolean);
   package Modulation_Index_Vectors is new
     Ada.Containers.Vectors (Positive, Interfaces.Integer_32, Interfaces."=");
   type Waveform_Profile is record
      Duration                : Interfaces.Integer_64 := 0;
      Update_Phase,
      Update_Delta_Phase,
      Update_Frequency_Offset,
      Update_Delta_Frequency,
      Update_Frequency_Slope,
      Update_Amplitude_Factor : Boolean := False;
      Phase_Degrees,
      Delta_Phase_Degrees,
      Frequency_Offset_Hz,
      Delta_Frequency_Hz,
      Frequency_Slope_Hz_Sec,
      Amplitude_Factor        : Long_Float := 0.0;
   end record;
   package Waveform_Profile_Vectors is new Ada.Containers.Vectors (Positive, Waveform_Profile);
   type Waveform_Profile_Table is record
      Values : Waveform_Profile_Vectors.Vector;
   end record;
   type Waveform_Step is record
      Iterations : Interfaces.Integer_32;
      Index      : Waveform_Profile_Index;
   end record;
   package Waveform_Step_Vectors is new Ada.Containers.Vectors (Positive, Waveform_Step);
   type Event_Waveform_Pattern is record
      Steps : Waveform_Step_Vectors.Vector;
   end record;
   package Waveform_Pattern_Vectors is new
     Ada.Containers.Vectors (Positive, Event_Waveform_Pattern);
   type Waveform_Pattern_List is record
      Values : Waveform_Pattern_Vectors.Vector;
   end record;
   package Waveform_I8_Vectors is new Ada.Containers.Vectors (Positive, Waveform_Sample_I8);
   package Waveform_I16_Vectors is new Ada.Containers.Vectors (Positive, Waveform_Sample_I16);
   type Modulation_Config is record
      Kind                                                       : Natural range 0 .. 7 := 0;
      Stream_Endpoint_ID, Stream_Start_Address, Stream_Num_Bytes : Interfaces.Unsigned_64 := 0;
      Stream_Sample                                              : Stream_Sample_Format :=
        Stream_Complex_Cartesian;
      Stream_Item                                                : Stream_Item_Format :=
        Stream_Fixed_Point_Signed;
      Stream_Repeat                                              : Stream_Repeat_Mode :=
        Stream_Repeat_From_Top;
      Sample_Width                                               : Natural range 8 .. 16 := 8;
      Samples_I8                                                 : Waveform_I8_Vectors.Vector;
      Samples_I16                                                : Waveform_I16_Vectors.Vector;
      Duration                                                   : Interfaces.Integer_64 := 0;
      Phases                                                     : Phase_Chip_Vectors.Vector;
      Extent                                                     : Long_Float := 0.0;
      Binary_Data                                                :
        Modulation_Boolean_Vectors.Vector;
      Pulse_Duration                                             : Interfaces.Integer_64 := 0;
      Frequency_Offset                                           : Long_Float := 0.0;
      Profiles                                                   : Waveform_Profile_Table;
      Patterns                                                   : Waveform_Pattern_List;
   end record;
   package Modulation_Vectors is new Ada.Containers.Vectors (Positive, Modulation_Config);
   type Modulation_Table is record
      Values : Modulation_Vectors.Vector;
   end record;
   type TX_Transmit_Event_Config is record
      Event_ID                                               : Interfaces.Unsigned_32;
      Label                                                  :
        Ada.Strings.Unbounded.Unbounded_String;
      Start_Femtoseconds, Duration_Femtoseconds              : Interfaces.Integer_64;
      Center_Frequency_Hz, TX_Attenuation_DB                 : Long_Float;
      Rise_Duration_Femtoseconds, Fall_Duration_Femtoseconds : Interfaces.Integer_64;
      Stab_Point_Index                                       : Interfaces.Unsigned_64 := 0;
      Groups                                                 : RX_Index_Vectors.Vector;
      Has_Modulation_Indices                                 : Boolean := False;
      Modulation_Indices                                     : Modulation_Index_Vectors.Vector;
      Polarization                                           : Stokes_Vectors.Vector;
      Beam_Correction                                        : Boolean := False;
      Phase_Offset                                           : Long_Float := 0.0;
      Execution                                              : Execution_Type := Normal;
      Termination                                            : Event_Termination_Type :=
        Inhibit_Event;
      Allow_Delay                                            : Boolean := False;
      Hold_Count, Termination_Count                          : Interfaces.Unsigned_64 := 0;
      Channelization                                         : Boolean := False;

   end record;
   package TX_Event_Vectors is new Ada.Containers.Vectors (Positive, TX_Transmit_Event_Config);
   type Local_Function_Write is record
      Address, Value : Interfaces.Unsigned_64;
   end record;
   package Local_Function_Write_Vectors is new
     Ada.Containers.Vectors (Positive, Local_Function_Write);
   type Local_Function_Command is record
      Type_ID  : Interfaces.Unsigned_32;
      Instance : Interfaces.Unsigned_64;
      Writes   : Local_Function_Write_Vectors.Vector;
   end record;
   package Local_Function_Command_Vectors is new
     Ada.Containers.Vectors (Positive, Local_Function_Command);
   type Product_Stream_Endpoint is record
      Endpoint_ID, Start_Address, Max_Bytes : Interfaces.Unsigned_64;
   end record;
   package Product_Stream_Endpoint_Vectors is new
     Ada.Containers.Vectors (Positive, Product_Stream_Endpoint);
   type Product_Stream_Params is record
      RX_Groups : RX_Index_Vectors.Vector;
      Endpoints : Product_Stream_Endpoint_Vectors.Vector;
   end record;
   type Endpoint_Connection is record
      Group_Label, Pipe_Label : Ada.Strings.Unbounded.Unbounded_String;
      ID                      : Interfaces.Unsigned_64;
   end record;
   package Endpoint_Connection_Vectors is new
     Ada.Containers.Vectors (Positive, Endpoint_Connection);
   type Endpoint_Connections is record
      Values : Endpoint_Connection_Vectors.Vector;
   end record;
   type RX_Job_Interval_Config is record
      Interval_ID, Job_Details_ID                  : Interfaces.Unsigned_32;
      Interval_Start_Femtoseconds,
      Interval_Starting_Gap_Femtoseconds,
      Sequence_Duration_Femtoseconds,
      Calibration_Duration_Femtoseconds,
      Interval_Ending_Gap_Femtoseconds             : Interfaces.Integer_64;
      Sequence_Repeat_Count, Iterations_Per_Signal : Interfaces.Unsigned_64;
      Phase_Coherence_With_Prior                   : Boolean;
      Max_Data_Rate_BPS, Max_Sample_Rate_Hz        : Long_Float;
      Commands                                     : Local_Function_Command_Vectors.Vector;
      Events                                       : RX_Event_Vectors.Vector;
      Status_Enable                                : Interval_Status_Enable := Never;
      Stab_Points                                  : Pointing_Vectors.Vector;
      TX_Power_Mode                                : Interfaces.Unsigned_32 := 0;
      Activity                                     : Byte_Vectors.Vector;
      User_Context                                 : Byte_Vectors.Vector;
      Has_User_Context                             : Boolean := False;
      Execution                                    : Execution_Type := Normal;
      Has_Product_Stream_Params                    : Boolean := False;
      Product_Stream                               : Product_Stream_Params;
      Has_Endpoint_Connections                     : Boolean := False;
      Connections                                  : Endpoint_Connections;
      Has_TX_Events                                : Boolean := False;
      TX_Events                                    : TX_Event_Vectors.Vector;
      Has_Modulations                              : Boolean := False;
      Modulations                                  : Modulation_Table;
   end record;
   package RX_Interval_Vectors is new Ada.Containers.Vectors (Positive, RX_Job_Interval_Config);
   type RX_Job_Interval_List is record
      Values : RX_Interval_Vectors.Vector;
   end record;
   type Cancel_Result is record
      Was_Cancelled : Boolean := False;
      Code          : Cancel_Error := None;
   end record;
   package Text_Vectors is new
     Ada.Containers.Vectors
       (Positive,
        Ada.Strings.Unbounded.Unbounded_String,
        Ada.Strings.Unbounded."=");
   package UCI_Vectors is new Ada.Containers.Vectors (Positive, AMS.MEL.IR.UCI_ID, AMS.MEL.IR."=");
   package ID_Vectors is new
     Ada.Containers.Vectors (Positive, Interfaces.Unsigned_32, Interfaces."=");
   package Endpoint_Vectors is new
     Ada.Containers.Vectors (Positive, Interfaces.Unsigned_64, Interfaces."=");
   type Frequency_Range is record
      Min_Hz, Max_Hz : Long_Float;
   end record;
   package Frequency_Vectors is new Ada.Containers.Vectors (Positive, Frequency_Range);
   type Pipe_Config is record
      Label     : Ada.Strings.Unbounded.Unbounded_String;
      Endpoints : Endpoint_Vectors.Vector;
   end record;
   package Pipe_Vectors is new Ada.Containers.Vectors (Positive, Pipe_Config);
   type RX_Element_Group_Config is record
      Label, Pipe : Ada.Strings.Unbounded.Unbounded_String;
      Duty        : Long_Float;
      Frequencies : Frequency_Vectors.Vector;
      Pipes       : Pipe_Vectors.Vector;
      Pointings   : Pointing_Vectors.Vector;
   end record;
   type TX_Element_Group_Config is record
      Label       : Ada.Strings.Unbounded.Unbounded_String;
      Power       : Interfaces.Unsigned_32;
      Duty        : Long_Float;
      Frequencies : Frequency_Vectors.Vector;
   end record;
   type Group_Mode is (RX, TX);
   type Job_Group (Mode : Group_Mode := RX) is record
      case Mode is
         when RX =>
            Receive_Group : RX_Element_Group_Config;

         when TX =>
            Transmit_Group : TX_Element_Group_Config;
      end case;
   end record;
   package Group_Vectors is new Ada.Containers.Vectors (Positive, Job_Group);
   type Job_Config is record
      ID, Priority, Precedence : Interfaces.Unsigned_32;
      Interruptable            : Boolean;
      Groups                   : Group_Vectors.Vector;
      Instances                : ID_Vectors.Vector;
      Min_Start, Max_Complete  : UTC_Time;
      Duration, Lookahead      : Interfaces.Integer_64 := 0;
      Capability, Activity     : Byte_Vectors.Vector;
      Power_Modes              : ID_Vectors.Vector;
      Has_Estimated_Point      : Boolean := False;
      Estimated_Point          : Pointing;
   end record;
   type Job_Result is record
      State : Request_Outcome := Created;
      Code  : Request_Error_Code := None;
      Text  : Ada.Strings.Unbounded.Unbounded_String;
   end record;
   type Job_Request is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_Job_Request_Handle := AMS.MEL_C_API.Null_RF_Job_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Job_Request);
   type Job is new Ada.Finalization.Limited_Controlled with record
      Handle                                                   :
        aliased AMS.MEL_C_API.RF_Job_Handle := AMS.MEL_C_API.Null_RF_Job;
      Start_Seconds, Start_Femtoseconds, Duration_Femtoseconds : Interfaces.Integer_64 := 0;
      Instance_ID, Definition_ID, Details_ID, Request_ID       : Interfaces.Unsigned_32 := 0;
      Lookahead                                                : Interfaces.Integer_64 := 0;
      Streams                                                  : ID_Vectors.Vector;
   end record;
   overriding
   procedure Finalize (Object : in out Job);
   type Virtual_Aperture_Config is record
      ID, Priority : Interfaces.Unsigned_32;
      File_Info    : Ada.Strings.Unbounded.Unbounded_String;
      Local        : Text_Vectors.Vector;
      Capabilities : UCI_Vectors.Vector;
   end record;
   type Virtual_Aperture_Result is record
      State : Request_Outcome := Created;
      Code  : Request_Error_Code := None;
      Text  : Ada.Strings.Unbounded.Unbounded_String;
   end record;
   type Virtual_Aperture_Request is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_VA_Request_Handle := AMS.MEL_C_API.Null_RF_VA_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Virtual_Aperture_Request);
   type Virtual_Aperture is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_VA_Handle := AMS.MEL_C_API.Null_RF_VA;
      IDs    : ID_Vectors.Vector;
      Labels : Text_Vectors.Vector;
      Single : Boolean := False;
   end record;
   overriding
   procedure Finalize (Object : in out Virtual_Aperture);
   type C2_MEL is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_C2_Handle := AMS.MEL_C_API.Null_RF_C2;
   end record;
   overriding
   procedure Finalize (Object : in out C2_MEL);
end AMS.MEL.RF.C2;
