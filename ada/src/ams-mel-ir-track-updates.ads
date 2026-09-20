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
   --  Upstream TrackStatus, exactly Create = 0, Update = 1, Predict = 2, and
   --  Delete = 3. Upstream declares no MaxExclusive value.
   type Track_Status is (Create, Update, Predict, Delete);
   for Track_Status use (Create => 0, Update => 1, Predict => 2, Delete => 3);
   for Track_Status'Size use 32;

   --  Owned Track-update-facing ECEF value. Deliberately independent of
   --  AMS.MEL.IR.Image so this package adds no Image dependency.
   type Directional is record
      X : Long_Float;
      Y : Long_Float;
      Z : Long_Float;
   end record;

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

   --  Upstream CommandState, with the published numeric representations
   --  already used by the C ABI. This package deliberately does not depend on
   --  AMS.MEL.IR.C2.Metadata merely to reuse its enums; a cross-package
   --  neutralization refactor is out of scope here.
   type Command_State is (Not_Set, Received, Accepted, Rejected, Cancelled);
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
   function Command_ID (Status : Command_Status) return Interfaces.Unsigned_32;
   function State (Status : Command_Status) return Command_State;
   function Reason (Status : Command_Status) return Cannot_Comply;
   function Reason_Description (Status : Command_Status) return String;

   type Update_Outcome is (Success, Rejected);
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

   --  A successful CommandStatus whose own State is Rejected is still a
   --  Success outcome: CommandStatus rejection is not an ErrorOr rejection.
   type Update_Result is private;
   function Status (Result : Update_Result) return Update_Outcome;
   function Command (Result : Update_Result) return Command_Status
   with Pre => Status (Result) = Success;
   function Rejection_Code (Result : Update_Result) return Update_Error_Code
   with Pre => Status (Result) = Rejected;
   function Description (Result : Update_Result) return String
   with Pre => Status (Result) = Rejected;

   --  Submission requires an enabled Track_Channel.
   type Update_Request is limited private;
   function Submit (Channel : Track_Channel; Value : Track_Data_Update) return Update_Request;
   function Is_Open (Request : Update_Request) return Boolean;
   --  Timeout_Error is inherited from AMS.MEL.IR. Timeout never cancels or
   --  consumes the request. Wait may be repeated and a later Wait returns the
   --  identical cached terminal result. Close must not race Wait on the same
   --  Update_Request.
   function Wait (Request : Update_Request; Timeout_Milliseconds : Natural) return Update_Result;
   procedure Close (Request : in out Update_Request);

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
