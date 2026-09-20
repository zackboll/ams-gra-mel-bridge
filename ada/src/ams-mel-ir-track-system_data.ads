private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with Interfaces;

--  Optional IR MEL TrackChannel::send (SystemTrackDataResponse) (@Optional).
--  This is neither @RequiredIfTrack nor @RequiredIfTrackUpdate: it is an
--  optional upstream Track operation, so it lives in its own child package and
--  neither the Track parent package nor AMS.MEL.IR.Track.Updates is disturbed.
--
--  This package is also the natural future home for the RequestSystemTrackData
--  callback, but that callback is deliberately NOT implemented here.
--
--  Deliberately absent: the RequestSystemTrackData callback, the
--  CandidateObjectMessage callback, and the CandidateObjectPreProcMessage
--  callback.
--
--  Submission requires an enabled Track_Channel. Close is not cancellation: a
--  pending request keeps the underlying provider future and Track channel alive
--  independently of the public Track_Channel and Session owners. Requests of
--  this family share one pending-request accounting domain with
--  AMS.MEL.IR.Track.Updates requests, so physical Track teardown is deferred
--  until every pending request of both families has completed.

package AMS.MEL.IR.Track.System_Data is
   --  The one canonical Track-facing azimuth/elevation value. Both components
   --  are radians and neither is clamped or normalized.
   type Azimuth_Elevation is record
      Azimuth_Rad   : Long_Float;
      Elevation_Rad : Long_Float;
   end record;

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

   --  Upstream CommandState, with the published numeric representations
   --  already used by the C ABI. This package deliberately does not depend on
   --  AMS.MEL.IR.C2.Metadata merely to reuse its enums; a cross-package
   --  neutralization refactor is out of scope here. Every representation is
   --  pinned explicitly rather than left to declaration order.
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

   type Response_Outcome is (Success, Rejected);
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

   --  A successful CommandStatus whose own State is Rejected is still a
   --  Success outcome: CommandStatus rejection is not an ErrorOr rejection.
   type Response_Result is private;
   function Status (Result : Response_Result) return Response_Outcome;
   function Command (Result : Response_Result) return Command_Status
   with Pre => Status (Result) = Success;
   function Rejection_Code (Result : Response_Result) return Response_Error_Code
   with Pre => Status (Result) = Rejected;
   function Description (Result : Response_Result) return String
   with Pre => Status (Result) = Rejected;

   --  Submission requires an enabled Track_Channel.
   type Response_Request is limited private;
   function Submit
     (Channel : Track_Channel; Value : System_Track_Data_Response) return Response_Request;
   function Is_Open (Request : Response_Request) return Boolean;
   --  Timeout_Error is inherited from AMS.MEL.IR. Timeout never cancels or
   --  consumes the request. Wait may be repeated and a later Wait returns the
   --  identical cached terminal result. Close must not race Wait on the same
   --  Response_Request.
   function Wait
     (Request : Response_Request; Timeout_Milliseconds : Natural) return Response_Result;
   procedure Close (Request : in out Response_Request);

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
