private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with Interfaces;

--  Conditionally required IR MEL TrackChannel::send (TrackDataUpdate)
--  (@RequiredIfTrackUpdate). This is a distinct upstream condition from
--  @RequiredIfTrack itself, so it lives in its own child package and the Track
--  parent package stays focused on channel ownership and the IRSTTrackReport
--  value types.
--
--  Deliberately absent: SystemTrackDataResponse, CandidateObjectMessage,
--  CandidateObjectPreProcMessage, and RequestSystemTrackData.
--
--  Submission requires an enabled Track_Channel. Close is not cancellation: a
--  pending request keeps the underlying provider future and Track channel alive
--  independently of the public Track_Channel and Session owners.

package AMS.MEL.IR.Track.Updates is
   --  Outbound TrackDataUpdate under the upstream track-update condition, which
   --  is distinct from basic Track support. Requires Enabled Track. Positive
   --  execution is mock-qualified only; pinned Squall cannot attach Track.
   --  Bridge copies kinematics/covariance, performing no estimation, coordinate
   --  transformation, normalization or operational-incorporation verification.

   --  Upstream TrackStatus, exactly Create = 0, Update = 1, Predict = 2, and
   --  Delete = 3. Upstream declares no MaxExclusive value.
   type Track_Status is (Create, Update, Predict, Delete);
   --  Source-defined status of an outgoing track update, values 0..3.
   --  @enum Create Track-created category, value 0.
   --  @enum Update Track-updated category, value 1.
   --  @enum Predict Track-predicted category, value 2; the bridge does not predict it.
   --  @enum Delete Track-deleted category, value 3; submission does not prove deletion occurred.
   for Track_Status use (Create => 0, Update => 1, Predict => 2, Delete => 3);
   for Track_Status'Size use 32;

   --  Owned Track-update-facing ECEF value. Deliberately independent of
   --  AMS.MEL.IR.Image so this package adds no Image dependency.
   type Directional is record
      X : Long_Float;
      Y : Long_Float;
      Z : Long_Float;
   end record;
   --  Owned ECEF xyz components used separately for position and velocity.
   --  Pinned TrackDataUpdate specifies ECEF but not stronger component units;
   --  the bridge does not convert coordinates or infer a unit convention.
   --  @field X ECEF x component, copied through native double representation.
   --  @field Y ECEF y component, copied through native double representation.
   --  @field Z ECEF z component, copied through native double representation.

   --  Every published TrackDataUpdate covariance term, exactly 21 values,
   --  corresponding one-to-one with the C covariance record.
   type Track_Covariance is record
      XX   : Long_Float;
      XY   : Long_Float;
      XZ   : Long_Float;
      X_VX : Long_Float;
      X_VY : Long_Float;
      X_VZ : Long_Float;

      YY   : Long_Float;
      YZ   : Long_Float;
      Y_VX : Long_Float;
      Y_VY : Long_Float;
      Y_VZ : Long_Float;

      ZZ   : Long_Float;
      Z_VX : Long_Float;
      Z_VY : Long_Float;
      Z_VZ : Long_Float;

      VX_VX : Long_Float;
      VX_VY : Long_Float;
      VX_VZ : Long_Float;

      VY_VY : Long_Float;
      VY_VZ : Long_Float;

      VZ_VZ : Long_Float;
   end record;
   --  All 21 individually named upstream Track covariance scalars. No implicit
   --  packed-matrix convention, units, symmetry expansion or transformation is
   --  invented. This differs from Image navigation's eighteen covariance terms.
   --  @field XX Upstream trackCovarianceXX scalar for the X/X pair.
   --  @field XY Upstream trackCovarianceXY scalar for the X/Y pair.
   --  @field XZ Upstream trackCovarianceXZ scalar for the X/Z pair.
   --  @field X_VX Upstream trackCovarianceXVx scalar for the X/velocity-X pair.
   --  @field X_VY Upstream trackCovarianceXVy scalar for the X/velocity-Y pair.
   --  @field X_VZ Upstream trackCovarianceXVz scalar for the X/velocity-Z pair.
   --  @field YY Upstream trackCovarianceYY scalar for the Y/Y pair.
   --  @field YZ Upstream trackCovarianceYZ scalar for the Y/Z pair.
   --  @field Y_VX Upstream trackCovarianceYVx scalar for the Y/velocity-X pair.
   --  @field Y_VY Upstream trackCovarianceYVy scalar for the Y/velocity-Y pair.
   --  @field Y_VZ Upstream trackCovarianceYVz scalar for the Y/velocity-Z pair.
   --  @field ZZ Upstream trackCovarianceZZ scalar for the Z/Z pair.
   --  @field Z_VX Upstream trackCovarianceZVx scalar for the Z/velocity-X pair.
   --  @field Z_VY Upstream trackCovarianceZVy scalar for the Z/velocity-Y pair.
   --  @field Z_VZ Upstream trackCovarianceZVz scalar for the Z/velocity-Z pair.
   --  @field VX_VX Upstream trackCovarianceVxVx scalar for the velocity-X/velocity-X pair.
   --  @field VX_VY Upstream trackCovarianceVxVy scalar for the velocity-X/velocity-Y pair.
   --  @field VX_VZ Upstream trackCovarianceVxVz scalar for the velocity-X/velocity-Z pair.
   --  @field VY_VY Upstream trackCovarianceVyVy scalar for the velocity-Y/velocity-Y pair.
   --  @field VY_VZ Upstream trackCovarianceVyVz scalar for the velocity-Y/velocity-Z pair.
   --  @field VZ_VZ Upstream trackCovarianceVzVz scalar for the velocity-Z/velocity-Z pair.

   --  Complete TrackDataUpdate. Every upstream field is represented exactly
   --  once. The two times stay in upstream epoch seconds and no value is
   --  clamped or normalized, because the upstream setters perform no such
   --  validation.
   type Track_Data_Update is record
      Platform_ID : Interfaces.Unsigned_32;

      Capability_UUID : UCI_ID;
      Activity_UUID   : UCI_ID;

      Track_ID : Interfaces.Unsigned_32;

      Entity_UUID : UCI_ID;

      Status : Track_Status;

      Time_Of_Validity_Seconds    : Long_Float;
      Time_Of_Last_Update_Seconds : Long_Float;

      Position_ECEF : Directional;
      Velocity_ECEF : Directional;

      Covariance : Track_Covariance;

      Maneuver_Probability : Long_Float;
      Track_Quality        : Long_Float;
   end record;
   --  Complete outbound update with floating epoch-second times, not nanosecond
   --  timestamps. All scalars cross as native double without clamping. UCI
   --  labels are copied after UTF-8/no-NUL validation before Submit returns.
   --  @field Platform_ID Unsigned integer platform identifier, distinct from structured UCI identities.
   --  @field Capability_UUID Structured capability UCI identity and descriptive label.
   --  @field Activity_UUID Structured activity UCI identity and descriptive label.
   --  @field Track_ID Unsigned integer track identifier, all 32 bits preserved.
   --  @field Entity_UUID Structured entity UCI identity and descriptive label.
   --  @field Status Track create/update/predict/delete category; no bridge tracker action is performed.
   --  @field Time_Of_Validity_Seconds Floating-point time of validity in epoch seconds; a more specific epoch is not established.
   --  @field Time_Of_Last_Update_Seconds Floating-point last-update time in epoch seconds, not nanoseconds.
   --  @field Position_ECEF Provider/application ECEF position components, copied without transformation.
   --  @field Velocity_ECEF Provider/application ECEF velocity components, copied without estimation or unit conversion.
   --  @field Covariance Twenty-one individually preserved Track covariance values, not Image navigation covariance.
   --  @field Maneuver_Probability Upstream probability zero-to-one: zero unlikely, one likely maneuvering; not range-enforced here.
   --  @field Track_Quality Upstream allowed quality range zero through fifteen; copied without clamping or calibration.

   --  Upstream CommandState, with the published numeric representations
   --  already used by the C ABI. This package deliberately does not depend on
   --  AMS.MEL.IR.C2.Metadata merely to reuse its enums; a cross-package
   --  neutralization refactor is out of scope here.
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
   --  Owned CommandStatus payload from a successful future. A payload whose own
   --  command state is Rejected is still a successful future result, unlike an
   --  explicit ErrorOr rejection. No physical incorporation guarantee follows.
   function Command_ID (Status : Command_Status) return Interfaces.Unsigned_32;
   --  Read the returned command correlation identifier.
   --  @param Status Owned command-status payload to inspect.
   --  @return Provider-reported unsigned command identifier.
   function State (Status : Command_Status) return Command_State;
   --  Read reported command messaging state, not physical execution progress.
   --  @param Status Owned command-status payload to inspect.
   --  @return Preserved state, including command Rejected within future Success.
   function Reason (Status : Command_Status) return Cannot_Comply;
   --  Read the reported inability reason without policy interpretation.
   --  @param Status Owned command-status payload to inspect.
   --  @return Source-mapped reason; zero denotes an unset optional reason.
   function Reason_Description (Status : Command_Status) return String;
   --  Copy reason text validated natively as UTF-8 without embedded NUL.
   --  @param Status Owned command-status payload to inspect.
   --  @return Independent text copied from immutable native cached storage.

   type Update_Outcome is (Success, Rejected);
   --  Terminal future category, distinct from submission or physical acceptance.
   --  @enum Success Future produced a valid CommandStatus; its own state can still be Rejected.
   --  @enum Rejected Future produced an explicit upstream ErrorOr error.
   type Update_Error_Code is
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
   type Update_Result is private;
   --  Owned terminal CommandStatus or explicit rejection. A successful outcome
   --  does not prove operational incorporation of kinematics by the provider.
   function Status (Result : Update_Result) return Update_Outcome;
   --  Read the stored future category without contacting the provider.
   --  @param Result Owned update result to inspect.
   --  @return Payload success or explicit upstream rejection.
   function Command (Result : Update_Result) return Command_Status
   with Pre => Status (Result) = Success;
   --  Copy status under the declared Success precondition; the expression body
   --  performs no independent outcome check when assertions are disabled.
   --  @param Result Successful update result.
   --  @return Owned CommandStatus; its command state is separate from future outcome.
   function Rejection_Code (Result : Update_Result) return Update_Error_Code
   with Pre => Status (Result) = Rejected;
   --  Read the error category under the declared Rejected precondition; no
   --  independent body outcome check is performed.
   --  @param Result Explicitly rejected update result.
   --  @return Mapped upstream error code, not Session admission exhaustion.
   function Description (Result : Update_Result) return String
   with Pre => Status (Result) = Rejected;
   --  Copy rejection text under the declared Rejected precondition; the body
   --  simply returns its stored string without independently checking outcome.
   --  @param Result Explicitly rejected update result.
   --  @return Complete owned diagnostic, with consistent cached retry for long text.

   --  Submission requires an enabled Track_Channel.
   type Update_Request is limited private;
   --  Limited controlled update observer, distinct from system-response/common
   --  Ada owners. Native pending work independently retains the Track graph and
   --  Session admission permit. Finalization is non-raising; closure is not
   --  cancellation or direct release of a still-pending admission permit.
   function Submit (Channel : Track_Channel; Value : Track_Data_Update) return Update_Request;
   --  Copy and send the update while Track is Enabled. Native lifecycle claim
   --  precedes unlocked provider send; no kinematic/score/covariance estimation
   --  or physical consistency validation is promised. Session admission refusal
   --  raises the resource-exhaustion exception; closed/ineligible channels,
   --  invalid text and other native/provider failures raise a provider error.
   --  @param Channel Enabled Track owner; externally serialize owner operations.
   --  @param Value Complete update copied before Submit returns, including identity labels.
   --  @return Admitted observer, not confirmation of provider acceptance/incorporation.
   function Is_Open (Request : Update_Request) return Boolean;
   --  Inspect observer-handle presence without querying future completion.
   --  @param Request Update observer to inspect.
   --  @return True when its native public handle is present, pending or terminal.
   function Wait (Request : Update_Request; Timeout_Milliseconds : Natural) return Update_Result;
   --  Wait for a terminal result, cached for repeat waits. Timeout raises the
   --  inherited timeout exception without consuming/cancelling. Explicit ErrorOr
   --  rejection is a result; provider/native/malformed-output failure raises a
   --  provider error. Deferred Track cleanup failure also fails the result closed
   --  and safely retains the graph rather than unloading uncertain provider code.
   --  @param Request Open update observer; must not race Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   --  @return Owned cached CommandStatus or explicit rejection.
   procedure Close (Request : in out Update_Request);
   --  Idempotently close only the public observer; the Ada body ignores native
   --  close status. Provider work is not cancelled. Updates, system responses
   --  and common requests share Track accounting; only final pending completion
   --  permits deferred disable/detach, provider destruction and callback drain.
   --  Other pending work or unsafe-cleanup retention can keep the graph alive.
   --  @param Request Observer to clear, externally serialized against Wait.

private
   package US renames Ada.Strings.Unbounded;

   type Command_Status is record
      Status_Command_ID : Interfaces.Unsigned_32 := 0;
      Status_State      : Command_State := Not_Set;
      Status_Reason     : Cannot_Comply := Not_Set;
      Status_Text       : US.Unbounded_String;
   end record;

   type Update_Result is record
      Result_Status  : Update_Outcome := Success;
      Result_Command : Command_Status;
      Result_Code    : Update_Error_Code := None;
      Result_Text    : US.Unbounded_String;
   end record;

   type Update_Request_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Track_Update_Request_Handle :=
        AMS.MEL_C_API.Null_Track_Update_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Update_Request_Owner);
   type Update_Request is limited record
      Owner : Update_Request_Owner;
   end record;
end AMS.MEL.IR.Track.Updates;
