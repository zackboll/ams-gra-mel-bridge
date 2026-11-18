with Interfaces;
private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with AMS.MEL.IR.Channel;

package AMS.MEL.IR.Scheduling.Commands is
   --  Outgoing task requests, independent of Scheduling metadata. A future
   --  reports the provider's Return or rejection, not physical sensor activity.

   type Task_List_Action is (Normal, Next, Immediate, Abort_Action);
   --  Exact pinned TaskListAction alternatives; never implicitly rewritten.
   --  @enum Normal Published Normal action, numeric value 0.
   --  @enum Next Published Next action, numeric value 1.
   --  @enum Immediate Published Immediate action, numeric value 2.
   --  @enum Abort_Action Published Abort action, numeric value 3.

   type Task_Info is record
      System_Time_NS             : Interfaces.Integer_64 := 0;
      Request_ID                 : Interfaces.Unsigned_32 := 0;
      Action                     : Task_List_Action := Normal;
      Precedence_Within_Priority : Interfaces.Unsigned_32 := 0;
      Priority                   : Interfaces.Unsigned_32 := 0;
   end record;
   --  Complete value passed to the published upstream TaskInfo constructor path.
   --  @field System_Time_NS Exact signed nanoseconds, without epoch conversion.
   --  @field Request_ID All 32 bits, without correlation or uniqueness assumptions.
   --  @field Action Exact action, including Normal even inside AbortTask.
   --  @field Precedence_Within_Priority Exact unsigned value, without priority inversion.
   --  @field Priority Exact unsigned value, without normalization or range restrictions.

   type Coordinate_Frame_Reference is (Inertial, Aircraft);
   --  Published CoordFrameRef, independently selected for each center coordinate.
   --  @enum Inertial Published Inertial frame, numeric value 0.
   --  @enum Aircraft Published Aircraft frame, numeric value 1.

   type Stare_Parameters is record
      Center_Azimuth_Rad   : Long_Float := 0.0;
      Center_Elevation_Rad : Long_Float := 0.0;
      Elevation_Frame      : Coordinate_Frame_Reference := Inertial;
      Azimuth_Frame        : Coordinate_Frame_Reference := Aircraft;
      Duration_NS          : Interfaces.Integer_64 := 0;
   end record;
   --  Complete owned StareTaskParam; no pointing transformation or time conversion.
   --  @field Center_Azimuth_Rad IEEE-754 double azimuth in radians, including signed zero.
   --  @field Center_Elevation_Rad Independent IEEE-754 double elevation in radians.
   --  @field Elevation_Frame Frame for elevation only.
   --  @field Azimuth_Frame Independent frame for azimuth only.
   --  @field Duration_NS Exact signed 64-bit nanoseconds; no epoch or ordering inferred.

   type Stare_Task is record
      Command_ID : Interfaces.Unsigned_32 := 0;
      Parameters : Stare_Parameters;
      Info       : Task_Info;
   end record;
   --  Complete owned IRSTStareTask; no range, priority or correlation constraints.
   --  @field Command_ID Complete unsigned 32-bit identifier, including zero.
   --  @field Parameters Complete stare-specific parameters.
   --  @field Info Existing five-field general TaskInfo, without action rewriting.

   type Abort_Request is limited private;
   --  Limited controlled observer; finalization never raises. Closing an observer
   --  does not cancel admitted work. Serialize Wait and Close on this observer.
   type Abort_Result is private;
   --  Owned cached terminal result, independent of all provider owners.

   subtype Stare_Request is Abort_Request;
   --  Limited-controlled Return observer sharing Wait, Close and non-raising
   --  finalization with Abort_Request. Timeout and Close do not cancel work.
   subtype Stare_Result is Abort_Result;
   --  Owned cached Return or MEL rejection; uses Status, Value, Rejection_Code
   --  and Description below. No acknowledgement or execution report is inferred.

   function Exec_Stare (Channel : Scheduling_Channel; Value : Stare_Task) return Stare_Request;
   --  Submit exactly SchedulingChannel::exec(IRSTStareTask), without registering
   --  metadata. Admission, completion and physical execution are distinct facts.
   --  @param Channel Live Enabled owner, serialized against Close until admission.
   --  @param Value Complete owned values copied and verified before provider entry.
   --  @return Limited observer; Wait reports actual Return or explicit MEL rejection.
   --  @exception Provider_Error Ineligible owner, native failure or provider exec exception.
   --  @exception Resource_Exhausted Session completion admission limit reached.

   function Exec_Abort (Channel : Scheduling_Channel; Info : Task_Info) return Abort_Request;
   --  Submit exactly one real upstream exec outside the Scheduling lifecycle lock.
   --  Does not register or synthesize ExecuteTaskAck or TaskExecutingRep callbacks.
   --  @param Channel Live Enabled Scheduling owner, serialized against Close until admission.
   --  @param Info Complete values copied through all five upstream setters/getters.
   --  @return Observer of an independently retained asynchronous provider request.
   --  @exception Provider_Error Ineligible owner, native failure or provider exec exception.
   --  @exception Resource_Exhausted Session completion admission limit reached.

   function Wait (Request : Abort_Request; Timeout_Milliseconds : Natural) return Abort_Result;
   --  Finite wait; terminal results are cached and repeated waits are identical.
   --  @param Request Open observer, serialized against Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   --  @return Actual Return payload or complete MEL rejection, not physical execution proof.
   --  @exception Timeout_Error Still pending; does not consume or cancel the request.
   --  @exception Provider_Error Null/invalid payload, future exception or closed observer.

   function Status (Result : Abort_Result) return AMS.MEL.IR.Channel.Outcome;
   --  Inspect the terminal outcome.
   --  @param Result Owned terminal result.
   --  @return Success for any valid Return payload, Rejected for an ErrorOr rejection.
   function Value (Result : Abort_Result) return AMS.MEL.IR.Channel.Command_Return;
   --  Inspect the actual provider Return, including Fail or Not_Supported.
   --  @param Result Successful terminal result.
   --  @return Exact upstream Return payload.
   --  @exception Provider_Error Result is Rejected.
   function Rejection_Code (Result : Abort_Result) return AMS.MEL.IR.Channel.Error_Code;
   --  Inspect a provider-declared MEL rejection.
   --  @param Result Rejected terminal result.
   --  @return Exact MEL error code, distinct from a Return payload.
   --  @exception Provider_Error Result is Success.
   function Description (Result : Abort_Result) return String;
   --  Copy the complete rejection diagnostic, including text beyond the initial buffer.
   --  @param Result Rejected terminal result.
   --  @return Complete owned provider rejection description.
   --  @exception Provider_Error Result is Success.
   procedure Close (Request : in out Abort_Request);
   --  Idempotently close only the observer; pending native work retains its graph.
   --  @param Request Observer to close, serialized against Wait.
   --  @exception Provider_Error Native observer closure failed.
private
   package US renames Ada.Strings.Unbounded;
   type Abort_Request is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Return_Request_Handle := AMS.MEL_C_API.Null_Return_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Abort_Request);
   type Abort_Result is record
      Result_Status : AMS.MEL.IR.Channel.Outcome := AMS.MEL.IR.Channel.Success;
      Result_Value  : AMS.MEL.IR.Channel.Command_Return := AMS.MEL.IR.Channel.Return_Success;
      Result_Code   : AMS.MEL.IR.Channel.Error_Code := AMS.MEL.IR.Channel.None;
      Result_Text   : US.Unbounded_String;
   end record;
end AMS.MEL.IR.Scheduling.Commands;
