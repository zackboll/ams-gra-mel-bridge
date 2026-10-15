private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with Interfaces;

--  Optional IR MEL TrackChannel::send (SystemTrackDataResponse) (@Optional).
--  This is neither @RequiredIfTrack nor @RequiredIfTrackUpdate: it is an
--  optional upstream Track operation, so it lives in its own child package and
--  neither the Track parent package nor AMS.MEL.IR.Track.Updates is disturbed.
--
--  Inbound system-data requests and candidate callbacks live in Track.Metadata,
--  not in this outbound package.
--
--  Submission requires an enabled Track_Channel. Close is not cancellation: a
--  pending request keeps the underlying provider future and Track channel alive
--  independently of the public Track_Channel and Session owners. Requests of
--  this family share one pending-request accounting domain with
--  AMS.MEL.IR.Track.Updates requests, so physical Track teardown is deferred
--  until every pending request of both families has completed.

package AMS.MEL.IR.Track.System_Data is
   --  Optional outbound system Track data response, distinct from inbound
   --  system-data-request metadata. Requires Enabled Track; no automatic response
   --  or physical incorporation guarantee is supplied. Positive qualification is
   --  mock-only; pinned Squall cannot attach the Track channel. Request lifetime
   --  and admission are shared natively with updates and common Channel requests.

   --  The one canonical Track-facing azimuth/elevation value. Both components
   --  are radians and neither is clamped or normalized.
   type Azimuth_Elevation is record
      Azimuth_Rad   : Long_Float;
      Elevation_Rad : Long_Float;
   end record;
   --  Owned upstream AzEl pair in radians, without clamping or normalization.
   --  @field Azimuth_Rad Azimuth component in radians.
   --  @field Elevation_Rad Elevation component in radians.

   --  Complete SystemTrackDataResponse. Every upstream field is represented
   --  exactly once. The system time stays signed nanoseconds, the ranges and
   --  rates stay in upstream meters and meters/second, and no value is clamped
   --  or normalized, because the upstream setters perform no such validation.
   type System_Track_Data_Response is record
      System_Time_NS : Long_Long_Integer;

      Command_ID : Interfaces.Unsigned_32;
      Request_ID : Interfaces.Unsigned_32;
      Track_ID   : Interfaces.Unsigned_32;

      Range_M              : Long_Float;
      Range_Rate_MPS       : Long_Float;
      Range_Error_M        : Long_Float;
      Range_Rate_Error_MPS : Long_Float;

      Az_El_Valid : Boolean;

      Inertial_Az_El : Azimuth_Elevation;
      Az_El_Error    : Azimuth_Elevation;

      Range_Valid : Boolean;
   end record;
   --  Complete outbound payload. Validity flags are provider data, not bridge
   --  commands to zero inactive fields. All values are copied through native
   --  double/scalar fields without physical consistency checks or normalization.
   --  @field System_Time_NS Signed system-time nanosecond count; epoch unspecified.
   --  @field Command_ID Unsigned command correlation identifier, all 32 bits retained.
   --  @field Request_ID Unsigned system-data request correlation identifier.
   --  @field Track_ID Unsigned numeric track identifier, not a UCI identity.
   --  @field Range_M Range in meters, copied even if range validity is false.
   --  @field Range_Rate_MPS Range rate in meters per second, without sign normalization.
   --  @field Range_Error_M Range error in meters, without clamping.
   --  @field Range_Rate_Error_MPS Range-rate error in meters per second.
   --  @field Az_El_Valid Provider angular-validity flag; false does not zero either angular pair.
   --  @field Inertial_Az_El Inertial azimuth/elevation in radians, copied without transformation.
   --  @field Az_El_Error Angular errors in radians; no statistical confidence convention inferred.
   --  @field Range_Valid Provider range-validity flag; false does not remove or zero range fields.

   --  Upstream CommandState, with the published numeric representations
   --  already used by the C ABI. This package deliberately does not depend on
   --  AMS.MEL.IR.C2.Metadata merely to reuse its enums; a cross-package
   --  neutralization refactor is out of scope here. Every representation is
   --  pinned explicitly rather than left to declaration order.
   type Command_State is (Not_Set, Received, Accepted, Rejected, Cancelled);
   --  Reported command messaging state, native values 0..4, not a Wait outcome.
   --  @enum Not_Set Command state not set, value 0.
   --  @enum Received Command received and being assessed; updates can modify it, value 1.
   --  @enum Accepted Command accepted for later activity/status reporting; terminal messaging state, value 2, not physical completion.
   --  @enum Rejected Reported rejected command state, value 3.
   --  @enum Cancelled Delete honored before resulting activity/functionality started, value 4.
   for Command_State use
     (Not_Set => 0, Received => 1, Accepted => 2, Rejected => 3, Cancelled => 4);
   for Command_State'Size use 32;

   --  Upstream CannotComply, with the published numeric representations.
   --  Every literal is pinned explicitly rather than left to declaration
   --  order, so the mapping matches AMS.MEL.IR.C2.Metadata exactly.
   type Cannot_Comply is
     (Not_Set,
      Constraint_Attempts,
      Constraint_Endurance,
      Constraint_Classification,
      Constraint_For_FOV_Limit,
      Constraint_Gating,
      Constraint_Maneuver_Limit,
      Constraint_Op,
      Constraint_Occlusion,
      Capability_Range,
      Capability_Performance,
      Constraint_RF,
      Constraint_Route,
      Constraint_Safety,
      Constraint_Target_Angle,
      Constraint_Time,
      Constraint_System,
      Infeasible_Route,
      Mission_Event,
      State_Or_Settings,
      State_Or_Settings_Change,
      System_Unavailable,
      System_Fault,
      System_Conflict,
      Subsystem_Unavailable,
      Subsystem_Fault,
      Capability_Fault,
      Capability_Precedence,
      Capability_Unavailable,
      Insufficient_Resources,
      Ranking,
      Weather,
      Ineligible_Control_Source,
      Dependency_Predecessor,
      Dependency_All_Or_Nothing,
      Dependency_Either_Or,
      Init_Criteria_Not_Met,
      Unknown_ID,
      Invalid_Input_Parameter,
      Input_Other,
      MDF_Activation_Error,
      Multiple,
      Cancelled,
      Other,
      Unknown,
      Aborted,
      Alignment_Maneuver);
   --  Source-defined inability categories with exact native mapping 0..46.
   --  Meanings come from pinned CommonIR MEL; unspecified categories add no
   --  bridge policy. This Ada type is distinct from child-package duplicates.
   --  @enum Not_Set Optional reason not set, value 0.
   --  @enum Constraint_Attempts Allowed attempts exhausted before desired result, value 1.
   --  @enum Constraint_Endurance Insufficient endurance such as fuel or battery, value 2.
   --  @enum Constraint_Classification Inconsistent classification of action or associated elements, value 3.
   --  @enum Constraint_For_FOV_Limit Field-of-regard or field-of-view limitation, value 4.
   --  @enum Constraint_Gating Command gating constraint not met, value 5.
   --  @enum Constraint_Maneuver_Limit Maneuver exceeds action limits, value 6.
   --  @enum Constraint_Op Constraint imposed by an operational zone, line or routing message, value 7.
   --  @enum Constraint_Occlusion Terrain occludes line of sight, value 8.
   --  @enum Capability_Range Associated capability range limitation, value 9.
   --  @enum Capability_Performance Source-defined capability-performance category without further pinned explanatory prose, value 10.
   --  @enum Constraint_RF Needed/commanded RF parameters conflict with available spectrum, value 11.
   --  @enum Constraint_Route Action not achievable on planned route or route cannot be generated, value 12.
   --  @enum Constraint_Safety Safety-related constraint, value 13.
   --  @enum Constraint_Target_Angle Target-angle constraint, value 14.
   --  @enum Constraint_Time Time-related constraint, value 15.
   --  @enum Constraint_System Allocation constrained to a system that cannot complete the action, value 16.
   --  @enum Infeasible_Route Route to achieve the action could not be generated, value 17.
   --  @enum Mission_Event General mission event such as contingency or allocation change, value 18.
   --  @enum State_Or_Settings Current/future capability or subsystem state/settings, value 19.
   --  @enum State_Or_Settings_Change Change of capability or subsystem state/settings, value 20.
   --  @enum System_Unavailable Associated system unavailable at needed time, value 21.
   --  @enum System_Fault Associated system faulted, value 22.
   --  @enum System_Conflict Conflict with another system's plans or actions, value 23.
   --  @enum Subsystem_Unavailable Associated subsystem unavailable at needed time, value 24.
   --  @enum Subsystem_Fault Associated subsystem faulted, value 25.
   --  @enum Capability_Fault Associated capability faulted, value 26.
   --  @enum Capability_Precedence Capability precedence within subsystem too low, value 27.
   --  @enum Capability_Unavailable Associated capability unavailable at needed time, value 28.
   --  @enum Insufficient_Resources Insufficient subordinate resources although otherwise able to act, value 29.
   --  @enum Ranking Ranking insufficient to obtain or retain capability access, value 30.
   --  @enum Weather Weather effects prevent satisfactory action, value 31.
   --  @enum Ineligible_Control_Source Source ineligible or not granted needed control permission, value 32.
   --  @enum Dependency_Predecessor Required predecessor not allocated, unallocated or failed, value 33.
   --  @enum Dependency_All_Or_Nothing All-or-nothing dependency not satisfied, value 34.
   --  @enum Dependency_Either_Or Either-or dependency not satisfied, value 35.
   --  @enum Init_Criteria_Not_Met Initialization conditions unmet, value 36.
   --  @enum Unknown_ID Object for a command identifier not found, value 37.
   --  @enum Invalid_Input_Parameter Invalid action or associated input parameter, value 38.
   --  @enum Input_Other Other unspecified input problem, value 39.
   --  @enum MDF_Activation_Error MDF activation rejected for invalid identifiers or incompatible file set, value 40.
   --  @enum Multiple Multiple sibling reasons without a known single reason, value 41.
   --  @enum Cancelled Action cancelled, generally by its originating source, value 42.
   --  @enum Other Reason not yet incorporated into enumeration, value 43.
   --  @enum Unknown Reason unknown, value 44.
   --  @enum Aborted Action aborted due to mission/system constraints or priorities, distinct from cancellation/rejection/failure, value 45.
   --  @enum Alignment_Maneuver Source-defined alignment-maneuver category without further pinned explanatory prose, value 46.
   for Cannot_Comply use
     (Not_Set                   => 0,
      Constraint_Attempts       => 1,
      Constraint_Endurance      => 2,
      Constraint_Classification => 3,
      Constraint_For_FOV_Limit  => 4,
      Constraint_Gating         => 5,
      Constraint_Maneuver_Limit => 6,
      Constraint_Op             => 7,
      Constraint_Occlusion      => 8,
      Capability_Range          => 9,
      Capability_Performance    => 10,
      Constraint_RF             => 11,
      Constraint_Route          => 12,
      Constraint_Safety         => 13,
      Constraint_Target_Angle   => 14,
      Constraint_Time           => 15,
      Constraint_System         => 16,
      Infeasible_Route          => 17,
      Mission_Event             => 18,
      State_Or_Settings         => 19,
      State_Or_Settings_Change  => 20,
      System_Unavailable        => 21,
      System_Fault              => 22,
      System_Conflict           => 23,
      Subsystem_Unavailable     => 24,
      Subsystem_Fault           => 25,
      Capability_Fault          => 26,
      Capability_Precedence     => 27,
      Capability_Unavailable    => 28,
      Insufficient_Resources    => 29,
      Ranking                   => 30,
      Weather                   => 31,
      Ineligible_Control_Source => 32,
      Dependency_Predecessor    => 33,
      Dependency_All_Or_Nothing => 34,
      Dependency_Either_Or      => 35,
      Init_Criteria_Not_Met     => 36,
      Unknown_ID                => 37,
      Invalid_Input_Parameter   => 38,
      Input_Other               => 39,
      MDF_Activation_Error      => 40,
      Multiple                  => 41,
      Cancelled                 => 42,
      Other                     => 43,
      Unknown                   => 44,
      Aborted                   => 45,
      Alignment_Maneuver        => 46);
   for Cannot_Comply'Size use 32;

   --  Ada-owned CommandStatus. Reason_Description is copied into Ada-owned
   --  storage during Wait, so no C pointer escapes that call.
   type Command_Status is private;
   --  Owned CommandStatus payload returned by a successful provider future.
   --  Its own rejected command state remains a Success future outcome; only an
   --  upstream ErrorOr error produces a rejected response outcome.
   function Command_ID (Status : Command_Status) return Interfaces.Unsigned_32;
   --  Read the returned command correlation identifier.
   --  @param Status Owned command-status payload to inspect.
   --  @return Provider-reported unsigned command ID, not a bridge-generated sequence.
   function State (Status : Command_Status) return Command_State;
   --  Read reported command messaging state, not physical completion.
   --  @param Status Owned command-status payload to inspect.
   --  @return Preserved command state, including Rejected within a successful future.
   function Reason (Status : Command_Status) return Cannot_Comply;
   --  Read the inability category without adding bridge policy.
   --  @param Status Owned command-status payload to inspect.
   --  @return Source-mapped reason; the zero category denotes an unset optional reason.
   function Reason_Description (Status : Command_Status) return String;
   --  Copy the command-status reason, validated natively as UTF-8 without NUL.
   --  @param Status Owned command-status payload to inspect.
   --  @return Independent reason text copied from immutable native result storage.

   type Response_Outcome is (Success, Rejected);
   --  Terminal future category, not submission acceptance or physical execution.
   --  @enum Success Future returned a valid CommandStatus, whose own command state can still be Rejected.
   --  @enum Rejected Future returned an explicit upstream ErrorOr error.
   type Response_Error_Code is
     (None,
      Invalid_ID,
      Invalid_State,
      Invalid_Parameters,
      Insufficient_Permissions,
      Insufficient_Resources,
      Insufficient_Local_Resources,
      Insufficient_Remote_Resources,
      Unsupported);
   --  Known upstream rejection categories mapped to native values 0..8.
   --  Session admission refusal is a separate exception, not one of these results.
   --  @enum None No-error category, value 0, preserved if present in an upstream error.
   --  @enum Invalid_ID Invalid identifier category, value 1.
   --  @enum Invalid_State Invalid state category, value 2.
   --  @enum Invalid_Parameters Invalid parameters category, value 3.
   --  @enum Insufficient_Permissions Insufficient permissions category, value 4.
   --  @enum Insufficient_Resources Insufficient resources category, value 5.
   --  @enum Insufficient_Local_Resources Insufficient local resources category, value 6.
   --  @enum Insufficient_Remote_Resources Insufficient remote resources category, value 7.
   --  @enum Unsupported Unsupported request category, value 8.

   --  A successful CommandStatus whose own State is Rejected is still a
   --  Success outcome: CommandStatus rejection is not an ErrorOr rejection.
   type Response_Result is private;
   --  Owned terminal CommandStatus payload or explicit rejection. Neither
   --  successful submission nor successful future completion establishes that
   --  the provider operationally incorporated the response.
   function Status (Result : Response_Result) return Response_Outcome;
   --  Read the stored terminal future category without provider access.
   --  @param Result Owned response result to inspect.
   --  @return Payload success or explicit ErrorOr rejection.
   function Command (Result : Response_Result) return Command_Status
   with Pre => Status (Result) = Success;
   --  Copy the payload under the declared Success precondition. The expression
   --  body adds no independent outcome check when assertions are disabled.
   --  @param Result Successful response result.
   --  @return Owned CommandStatus; inspect its state separately from future outcome.
   function Rejection_Code (Result : Response_Result) return Response_Error_Code
   with Pre => Status (Result) = Rejected;
   --  Read the rejection category under the declared Rejected precondition;
   --  the body does not independently check the outcome.
   --  @param Result Explicitly rejected response result.
   --  @return Mapped upstream error code, not bridge admission exhaustion.
   function Description (Result : Response_Result) return String
   with Pre => Status (Result) = Rejected;
   --  Copy rejection text under the declared Rejected precondition; the body
   --  directly returns stored text without an independent outcome check.
   --  @param Result Explicitly rejected response result.
   --  @return Complete owned diagnostic, using consistent cached retry for long text.

   --  Submission requires an enabled Track_Channel.
   type Response_Request is limited private;
   --  Limited controlled response observer, distinct from update/common owners.
   --  A pending native worker independently retains Track/Session/provider graph
   --  and Session admission permit. Observer closure is not cancellation or direct
   --  pending-permit release; finalization closes without raising.
   function Submit
     (Channel : Track_Channel; Value : System_Track_Data_Response) return Response_Request;
   --  Copy and submit the optional response while Track is Enabled. Native
   --  accounting precedes unlocked provider send. Attached-only/failed/closed
   --  resources and native/provider failures raise a provider error; Session-wide
   --  admission refusal raises the resource-exhaustion exception. No field is
   --  zeroed based on validity flags and no physical validation is promised.
   --  @param Channel Enabled Track owner; externally serialize its public operations.
   --  @param Value Complete response payload copied before submission returns.
   --  @return Admitted observer, not provider acceptance or operational incorporation.
   function Is_Open (Request : Response_Request) return Boolean;
   --  Inspect public observer-handle presence without polling completion.
   --  @param Request Response observer to inspect.
   --  @return True for a present handle, whether pending or terminal.
   function Wait
     (Request : Response_Request; Timeout_Milliseconds : Natural) return Response_Result;
   --  Await or repeat a cached terminal result. Timeout raises the inherited
   --  timeout exception without consumption/cancellation. Explicit ErrorOr
   --  rejection returns a result; provider/native failure raises a provider error.
   --  Malformed status and deferred Track cleanup failure also raise that error;
   --  failed deferred cleanup safely retains resources instead of unloading code.
   --  @param Request Open response observer; must not race its Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   --  @return Owned cached terminal payload or explicit rejection.
   procedure Close (Request : in out Response_Request);
   --  Idempotently release only the public observer, without cancelling provider
   --  work. The Ada body ignores native close status. Pending update, response
   --  and common work share Track accounting; physical disable/detach waits for
   --  the final pending claim, then provider destruction and callback drain.
   --  @param Request Observer to clear, externally serialized against Wait.

private
   package US renames Ada.Strings.Unbounded;

   type Command_Status is record
      Status_Command_ID : Interfaces.Unsigned_32 := 0;
      Status_State      : Command_State := Not_Set;
      Status_Reason     : Cannot_Comply := Not_Set;
      Status_Text       : US.Unbounded_String;
   end record;

   type Response_Result is record
      Result_Status  : Response_Outcome := Success;
      Result_Command : Command_Status;
      Result_Code    : Response_Error_Code := None;
      Result_Text    : US.Unbounded_String;
   end record;

   type Response_Request_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Track_System_Response_Request_Handle :=
        AMS.MEL_C_API.Null_Track_System_Response_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Response_Request_Owner);
   type Response_Request is limited record
      Owner : Response_Request_Owner;
   end record;
end AMS.MEL.IR.Track.System_Data;
