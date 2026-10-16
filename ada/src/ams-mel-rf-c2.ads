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
   type Pulse_Threshold_Reference is (DBQ, DB_Above_Noise, DB_Below_Saturation);
   for Pulse_Threshold_Reference use (DBQ => 0, DB_Above_Noise => 1, DB_Below_Saturation => 2);
   type Pulse_Time_Tag_Threshold is (At_50_Percent, At_90_Percent);
   for Pulse_Time_Tag_Threshold use (At_50_Percent => 0, At_90_Percent => 1);
   type Pulse_Detection_Settings is private;
   function Create_Pulse_Detection_Settings
     (Reference                                    : Pulse_Threshold_Reference;
      Leading_M, Leading_N, Trailing_M, Trailing_N : Interfaces.Unsigned_8;
      Min_Pulse_Width_Femtoseconds                 : Interfaces.Integer_64;
      Time_Tag_Threshold                           : Pulse_Time_Tag_Threshold)
      return Pulse_Detection_Settings;
   procedure Append_Pulse_Detection_Threshold
     (Settings : in out Pulse_Detection_Settings; Leading_Edge_DB, Trailing_Edge_DB : Long_Float);
   type RX_Receive_Event_Config is private;
   type TX_Transmit_Event_Config is private;
   type Modulation_Config is private;
   type Stream_Sample_Format is (Stream_Real, Stream_Complex_Cartesian, Stream_Complex_Polar);
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
   for Stream_Repeat_Mode use (Stream_Repeat_From_Top => 0, Stream_Continue_From_Previous => 1);
   --  Scalar descriptor only: numeric address, not a pointer. No endpoint or
   --  backing memory is acquired, checked, owned or transferred by this API.
   function Waveform_Stream
     (Endpoint_ID, Start_Address, Num_Bytes : Interfaces.Unsigned_64;
      Sample_Format                         : Stream_Sample_Format := Stream_Complex_Cartesian;
      Item_Format                           : Stream_Item_Format := Stream_Fixed_Point_Signed;
      Repeat_Mode                           : Stream_Repeat_Mode := Stream_Repeat_From_Top)
      return Modulation_Config;
   procedure Replace_Waveform_Stream
     (Config                                : in out Modulation_Config;
      Endpoint_ID, Start_Address, Num_Bytes : Interfaces.Unsigned_64;
      Sample_Format                         : Stream_Sample_Format;
      Item_Format                           : Stream_Item_Format;
      Repeat_Mode                           : Stream_Repeat_Mode);
   type Waveform_Sample_I8 is record
      Real, Imag : Interfaces.Integer_8;
   end record;
   type Waveform_Sample_I16 is record
      Real, Imag : Interfaces.Integer_16;
   end record;
   type Waveform_Sample_I8_Array is array (Positive range <>) of Waveform_Sample_I8;
   type Waveform_Sample_I16_Array is array (Positive range <>) of Waveform_Sample_I16;
   --  Owned caller values. Each Add copies into fresh native storage; native
   --  copies of one submitted buffer share that storage. No timing/execution
   --  guarantee is implied, including for empty Cartesian buffers.
   function Waveform_Buffer_I8 (Samples : Waveform_Sample_I8_Array) return Modulation_Config;
   function Waveform_Buffer_I16 (Samples : Waveform_Sample_I16_Array) return Modulation_Config;
   procedure Append_Waveform_Sample_I8
     (Config : in out Modulation_Config; Sample : Waveform_Sample_I8);
   procedure Append_Waveform_Sample_I16
     (Config : in out Modulation_Config; Sample : Waveform_Sample_I16);
   procedure Replace_Waveform_Samples_I8
     (Config : in out Modulation_Config; Samples : Waveform_Sample_I8_Array);
   procedure Replace_Waveform_Samples_I16
     (Config : in out Modulation_Config; Samples : Waveform_Sample_I16_Array);
   type Modulation_Table is private;
   --  Owned configuration values only, not executable waveform validation.
   --  Degrees stay degrees; false update flags retain their numeric values.
   subtype Waveform_Profile_Index is Interfaces.Integer_32 range 0 .. Interfaces.Integer_32'Last;
   type Waveform_Profile is private;
   type Waveform_Profile_Table is private;
   type Event_Waveform_Pattern is private;
   type Waveform_Pattern_List is private;
   function Create_Waveform_Profile
     (Chip_Duration_Femtoseconds : Interfaces.Integer_64 := 0) return Waveform_Profile;
   procedure Set_Chip_Duration_Femtoseconds
     (Profile : in out Waveform_Profile; Value : Interfaces.Integer_64);
   procedure Set_Phase_Degrees
     (Profile : in out Waveform_Profile; Value : Long_Float; Update : Boolean);
   procedure Set_Delta_Phase_Degrees
     (Profile : in out Waveform_Profile; Value : Long_Float; Update : Boolean);
   procedure Set_Frequency_Offset_Hz
     (Profile : in out Waveform_Profile; Value : Long_Float; Update : Boolean);
   procedure Set_Delta_Frequency_Hz
     (Profile : in out Waveform_Profile; Value : Long_Float; Update : Boolean);
   procedure Set_Frequency_Slope_Hz_Sec
     (Profile : in out Waveform_Profile; Value : Long_Float; Update : Boolean);
   procedure Set_Amplitude_Factor
     (Profile : in out Waveform_Profile; Value : Long_Float; Update : Boolean);
   procedure Append_Profile
     (Table   : in out Waveform_Profile_Table;
      Profile : Waveform_Profile;
      Index   : out Waveform_Profile_Index);
   procedure Append_Step
     (Pattern             : in out Event_Waveform_Pattern;
      Profile_Index       : Waveform_Profile_Index;
      Num_Chip_Iterations : Interfaces.Integer_32 := 1);
   procedure Append_Pattern
     (Patterns : in out Waveform_Pattern_List; Pattern : Event_Waveform_Pattern);
   function Custom_Waveform
     (Profiles : Waveform_Profile_Table; Patterns : Waveform_Pattern_List) return Modulation_Config;
   procedure Replace_Custom_Waveform_Profiles
     (Config : in out Modulation_Config; Profiles : Waveform_Profile_Table);
   procedure Replace_Custom_Waveform_Patterns
     (Config : in out Modulation_Config; Patterns : Waveform_Pattern_List);
   type Phase_Chip_Array is array (Positive range <>) of Long_Float;
   subtype Modulation_Index is
     Interfaces.Integer_32 range Interfaces."-" (1) .. Interfaces.Integer_32'Last;
   No_Modulation                       : constant Modulation_Index := Interfaces."-" (1);
   function Tone return Modulation_Config;
   type Modulation_Boolean_Array is array (Positive range <>) of Boolean;
   function On_Off_Keying (Pulse_Enabled : Modulation_Boolean_Array) return Modulation_Config;
   function CPFSK_Pulse
     (Chip_Data                                               : Modulation_Boolean_Array;
      Pulse_Duration_Femtoseconds, Chip_Duration_Femtoseconds : Interfaces.Integer_64;
      Frequency_Offset_Hz                                     : Long_Float)
      return Modulation_Config;
   procedure Append_OOK_Pulse_Enable (Config : in out Modulation_Config; Enabled : Boolean);
   procedure Replace_OOK_Pulse_Enables
     (Config : in out Modulation_Config; Pulse_Enabled : Modulation_Boolean_Array);
   procedure Append_CPFSK_Chip (Config : in out Modulation_Config; Chip : Boolean);
   procedure Replace_CPFSK_Chips
     (Config : in out Modulation_Config; Chip_Data : Modulation_Boolean_Array);
   function Phase_Code
     (Chip_Duration_Femtoseconds : Interfaces.Integer_64; Phases_Rad : Phase_Chip_Array)
      return Modulation_Config;
   function Linear_Frequency_Modulation (Frequency_Extent_Hz : Long_Float) return Modulation_Config;
   procedure Append_Phase_Chip (Config : in out Modulation_Config; Phase_Rad : Long_Float);
   procedure Replace_Phase_Chips (Config : in out Modulation_Config; Phases_Rad : Phase_Chip_Array);
   --  Returned index is zero-based, independent of Ada container indexing.
   procedure Append_Modulation
     (Table : in out Modulation_Table; Config : Modulation_Config; Index : out Modulation_Index);
   procedure Append_Modulation_Reference
     (Event : in out TX_Transmit_Event_Config; Index : Modulation_Index);
   procedure Clear_Modulation_References (Event : in out TX_Transmit_Event_Config);
   procedure Set_Empty_Modulation_References (Event : in out TX_Transmit_Event_Config);
   function Create_TX_Transmit_Event
     (Event_ID                                  : Interfaces.Unsigned_32;
      Element_Group_Label                       : String;
      Start_Femtoseconds, Duration_Femtoseconds : Interfaces.Integer_64;
      Center_Frequency_Hz                       : Long_Float;
      TX_Attenuation_DB                         : Long_Float := 0.0;
      Rise_Duration_Femtoseconds                : Interfaces.Integer_64 := 0;
      Fall_Duration_Femtoseconds                : Interfaces.Integer_64 := 0)
      return TX_Transmit_Event_Config;
   procedure Set_Event_Stabilization_Point_Index
     (Event : in out TX_Transmit_Event_Config; Index : Interfaces.Unsigned_64);
   procedure Append_Applicable_TX_Element_Group
     (Event : in out TX_Transmit_Event_Config; Element_Group_Index : Interfaces.Unsigned_64);
   procedure Set_TX_Attenuation_DB (Event : in out TX_Transmit_Event_Config; Value : Long_Float);
   procedure Set_TX_Edge_Durations
     (Event                                : in out TX_Transmit_Event_Config;
      Rise_Femtoseconds, Fall_Femtoseconds : Interfaces.Integer_64);
   procedure Set_Pulse_Detection_Settings
     (Event : in out RX_Receive_Event_Config; Settings : Pulse_Detection_Settings);
   procedure Clear_Pulse_Detection_Settings (Event : in out RX_Receive_Event_Config);
   type Execution_Type is (Normal, Conditional);
   for Execution_Type use (Normal => 0, Conditional => 1);
   type Event_Termination_Type is (Inhibit_Event, Cancel_Event);
   for Event_Termination_Type use (Inhibit_Event => 0, Cancel_Event => 1);
   --  Ordered copied Stokes values, no normalization or finite checks. A third
   --  append raises Constraint_Error. Provider semantic validation remains final.
   procedure Append_Polarization
     (Event : in out RX_Receive_Event_Config; S0, S1, S2, S3 : Long_Float);
   procedure Set_Polarization_Beam_Steer_Correction
     (Event : in out RX_Receive_Event_Config; Enabled : Boolean);
   procedure Set_Phase_Offset_Radians (Event : in out RX_Receive_Event_Config; Value : Long_Float);
   procedure Set_Event_Execution_Type
     (Event : in out RX_Receive_Event_Config; Value : Execution_Type);
   procedure Set_Event_Termination_Type
     (Event : in out RX_Receive_Event_Config; Value : Event_Termination_Type);
   procedure Set_Allow_Delay_Start (Event : in out RX_Receive_Event_Config; Enabled : Boolean);
   procedure Set_Iteration_Hold_Count
     (Event : in out RX_Receive_Event_Config; Count : Interfaces.Unsigned_64);
   procedure Set_Iteration_Termination_Count
     (Event : in out RX_Receive_Event_Config; Count : Interfaces.Unsigned_64);
   procedure Set_Channelization_Enabled (Event : in out RX_Receive_Event_Config; Enabled : Boolean);
   procedure Append_Polarization
     (Event : in out TX_Transmit_Event_Config; S0, S1, S2, S3 : Long_Float);
   procedure Set_Polarization_Beam_Steer_Correction
     (Event : in out TX_Transmit_Event_Config; Enabled : Boolean);
   procedure Set_Phase_Offset_Radians (Event : in out TX_Transmit_Event_Config; Value : Long_Float);
   procedure Set_Event_Execution_Type
     (Event : in out TX_Transmit_Event_Config; Value : Execution_Type);
   procedure Set_Event_Termination_Type
     (Event : in out TX_Transmit_Event_Config; Value : Event_Termination_Type);
   procedure Set_Allow_Delay_Start (Event : in out TX_Transmit_Event_Config; Enabled : Boolean);
   procedure Set_Iteration_Hold_Count
     (Event : in out TX_Transmit_Event_Config; Count : Interfaces.Unsigned_64);
   procedure Set_Iteration_Termination_Count
     (Event : in out TX_Transmit_Event_Config; Count : Interfaces.Unsigned_64);
   procedure Set_Channelization_Enabled
     (Event : in out TX_Transmit_Event_Config; Enabled : Boolean);
   --  Portable provider indices, not labels. No local count/range relationship
   --  is inferred. Applicable group order and duplicates are preserved.
   procedure Set_Stab_Point_Index
     (Event : in out RX_Receive_Event_Config; Index : Interfaces.Unsigned_64);
   procedure Append_Applicable_RX_Element_Group
     (Event : in out RX_Receive_Event_Config; Element_Group_Index : Interfaces.Unsigned_64);
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
   type Local_Function_Command is private;
   function Create_Local_Function_Command
     (Local_Function_Type_ID : Interfaces.Unsigned_32; Instance : Interfaces.Unsigned_64)
      return Local_Function_Command;
   procedure Append_Local_Function_Write
     (Command : in out Local_Function_Command; Address, Value : Interfaces.Unsigned_64);
   type RX_Job_Interval_Config is private;
   --  Owned value configuration only. Numeric addresses are not access values;
   --  application/provider remain responsible for endpoint matching, registered
   --  writable storage and its execution lifetime. No memory-safety guarantee.
   type Product_Stream_Params is private;
   --  Exact copied labels; last composite-key occurrence wins at submission.
   --  No endpoint creation, DataPipe association or applicable-label setup.
   type Endpoint_Connections is private;
   function Create_Endpoint_Connections return Endpoint_Connections;
   procedure Append_Endpoint_Connection
     (Connections                          : in out Endpoint_Connections;
      Element_Group_Label, Data_Pipe_Label : String;
      Endpoint_ID                          : Interfaces.Unsigned_64);
   procedure Set_Interval_Endpoint_Connections
     (Interval : in out RX_Job_Interval_Config; Connections : Endpoint_Connections);
   procedure Clear_Interval_Endpoint_Connections (Interval : in out RX_Job_Interval_Config);
   function Create_Product_Stream_Params return Product_Stream_Params;
   procedure Append_Product_Stream_RX_Group
     (Params : in out Product_Stream_Params; Index : Interfaces.Unsigned_64);
   procedure Append_Product_Stream_Endpoint
     (Params                                : in out Product_Stream_Params;
      Endpoint_ID, Start_Address, Max_Bytes : Interfaces.Unsigned_64);
   procedure Set_Interval_Product_Stream_Params
     (Interval : in out RX_Job_Interval_Config; Params : Product_Stream_Params);
   procedure Clear_Interval_Product_Stream_Params (Interval : in out RX_Job_Interval_Config);
   procedure Append_Interval_Local_Function_Command
     (Interval : in out RX_Job_Interval_Config; Command : Local_Function_Command);
   procedure Set_Interval_TX_Power_Mode_ID
     (Interval : in out RX_Job_Interval_Config; Value : Interfaces.Unsigned_32);
   procedure Set_Interval_Activity_ID
     (Interval : in out RX_Job_Interval_Config; Value : Byte_Array);
   -- Copied binary values; absent and explicitly present empty are distinct.
   procedure Set_Interval_User_Context
     (Interval : in out RX_Job_Interval_Config; Bytes : Byte_Array);
   procedure Append_Interval_User_Context_Byte
     (Interval : in out RX_Job_Interval_Config; Value : Interfaces.Unsigned_8);
   procedure Set_Empty_Interval_User_Context (Interval : in out RX_Job_Interval_Config);
   procedure Clear_Interval_User_Context (Interval : in out RX_Job_Interval_Config);
   procedure Set_Interval_Execution_Type
     (Interval : in out RX_Job_Interval_Config; Value : Execution_Type);
   procedure Append_Stab_Point (Interval : in out RX_Job_Interval_Config; Value : Pointing);
   type Interval_Status_Enable is (Never, Always, On_Exception);
   for Interval_Status_Enable use (Never => 0, Always => 1, On_Exception => 2);
   procedure Set_Interval_Status_Enable
     (Interval : in out RX_Job_Interval_Config; Mode : Interval_Status_Enable);
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
   procedure Append_RX_Event
     (Interval : in out RX_Job_Interval_Config; Event : RX_Receive_Event_Config);
   procedure Append_TX_Event
     (Interval : in out RX_Job_Interval_Config; Event : TX_Transmit_Event_Config);
   procedure Set_Modulations (Interval : in out RX_Job_Interval_Config; Table : Modulation_Table);
   procedure Clear_Modulations (Interval : in out RX_Job_Interval_Config);
   procedure Set_Empty_Modulations (Interval : in out RX_Job_Interval_Config);
   --  Set empty expresses presence; Clear restores absence.
   procedure Set_Empty_TX_Events (Interval : in out RX_Job_Interval_Config);
   procedure Clear_TX_Events (Interval : in out RX_Job_Interval_Config);
   type RX_Job_Interval_List is private;
   procedure Append_Job_Interval
     (Intervals : in out RX_Job_Interval_List; Interval : RX_Job_Interval_Config);
   function Job_Interval_Count (Intervals : RX_Job_Interval_List) return Natural;
   --  Synchronous, externally serialized with same-Job calls and Close.
   --  Add/Flush reject after Finalize or full Cancel has been attempted.
   --  Cancel_Remaining is repeatable, including during/after Finalize, until
   --  full Cancel has been attempted. Failures raise Provider_Error; no retry.
   procedure Add_RX_Job_Intervals (Object : in out Job; Intervals : RX_Job_Interval_List);
   procedure Add_Job_Intervals (Object : in out Job; Intervals : RX_Job_Interval_List)
   renames Add_RX_Job_Intervals;
   procedure Flush_Job (Object : in out Job);
   procedure Cancel_Remaining_Job_Intervals (Object : in out Job);
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
