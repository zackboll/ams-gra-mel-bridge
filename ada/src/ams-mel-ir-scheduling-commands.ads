with AMS.MEL.IR.C2;
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

   type Scan_Task is record
      Command_ID : Interfaces.Unsigned_32 := 0;
      Parameters : AMS.MEL.IR.C2.Scan_Parameters;
      Info       : Task_Info;
   end record;
   --  Complete owned IRSTScanTask; success is acceptance, not physical scanning.
   --  @field Command_ID Complete unsigned identifier without inferred correlation.
   --  @field Parameters Existing complete C2 scan values, without normalization.
   --  @field Info Existing general TaskInfo without action rewriting.

   type Track_Target is record
      Target_ID                     : Interfaces.Integer_32 := 0;
      Location_Azimuth_Rad          : Long_Float := 0.0;
      Location_Elevation_Rad        : Long_Float := 0.0;
      Rate_Azimuth_Rad_Per_Second   : Long_Float := 0.0;
      Rate_Elevation_Rad_Per_Second : Long_Float := 0.0;
   end record;
   --  Complete TrackTaskData, without association or frame conversion.
   --  @field Target_ID Exact signed 32-bit identifier, including negative values.
   --  @field Location_Azimuth_Rad Binary64 location azimuth, preserving signed zero.
   --  @field Location_Elevation_Rad Independent binary64 location elevation.
   --  @field Rate_Azimuth_Rad_Per_Second Independent binary64 azimuth rate.
   --  @field Rate_Elevation_Rad_Per_Second Independent binary64 elevation rate.

   subtype Track_Target_Index is Positive range 1 .. 255;
   --  One-based index of every stored upstream array entry.
   type Track_Target_Array is array (Track_Target_Index) of Track_Target;
   --  Owned fixed collection; entries beyond Count are copied and verified too.

   type Track_Parameters is record
      Duration_NS : Interfaces.Integer_64 := 0;
      Count       : Interfaces.Unsigned_32 := 0;
      Targets     : Track_Target_Array;
   end record;
   --  Complete TrackTaskParam; Count does not shorten the stored collection.
   --  @field Duration_NS Exact signed nanoseconds without epoch or unit conversion.
   --  @field Count Active count, rejected before dispatch if greater than 255.
   --  @field Targets All 255 entries in order, including duplicates and inactive entries.

   type Track_Task is record
      Command_ID : Interfaces.Unsigned_32 := 0;
      Parameters : Track_Parameters;
      Info       : Task_Info;
   end record;
   --  Complete owned IRSTTrackTask; acceptance is not physical tracking success.
   --  @field Command_ID Complete unsigned 32-bit command identifier.
   --  @field Parameters Complete parameters with the full fixed array.
   --  @field Info Existing five-field TaskInfo, including all four actions.

   type Track_Request is limited private;
   --  Limited-controlled Return observer sharing Wait, Close and non-raising
   --  finalization with Abort_Request. Timeout and Close do not cancel work.
   type Track_Result is private;
   --  Owned cached Return or MEL rejection; uses Status, Value, Rejection_Code
   --  and Description below. No acknowledgement or execution report is inferred.

   function Exec_Track (Channel : Scheduling_Channel; Value : Track_Task) return Track_Request;
   --  Submit exactly SchedulingChannel::exec(IRSTTrackTask), without registering
   --  metadata. Admission, completion and physical execution are distinct facts.
   --  @param Channel Live Enabled owner, serialized against Close until admission.
   --  @param Value Complete owned values copied and verified before provider entry.
   --  @return Limited observer; Wait reports actual Return or explicit MEL rejection.
   --  @exception Provider_Error Ineligible owner, native failure or provider exec exception.
   --  @exception Resource_Exhausted Session completion admission limit reached.

   function Wait (Request : Track_Request; Timeout_Milliseconds : Natural) return Track_Result;
   --  Observe the shared cached Return engine; zero timeout polls without cancellation.
   --  @param Request Open observer, serialized against Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   --  @return Actual Return or complete MEL rejection, never physical execution proof.
   --  @exception Timeout_Error Still pending; request remains open and unconsumed.
   --  @exception Provider_Error Closed observer, malformed payload or future exception.
   function Status (Result : Track_Result) return AMS.MEL.IR.Channel.Outcome;
   --  Inspect the terminal outcome, independently of the Return payload.
   --  @param Result Owned cached result.
   --  @return Success for a valid Return, Rejected for an explicit MEL rejection.
   function Value (Result : Track_Result) return AMS.MEL.IR.Channel.Command_Return;
   --  Inspect the actual provider Return value.
   --  @param Result Successful terminal result.
   --  @return Exact Return including Fail, Not_Supported and Not_Implemented.
   --  @exception Provider_Error Result is Rejected.
   function Rejection_Code (Result : Track_Result) return AMS.MEL.IR.Channel.Error_Code;
   --  Inspect an explicit MEL rejection, distinct from a Return payload.
   --  @param Result Rejected terminal result.
   --  @return Exact MEL error code.
   --  @exception Provider_Error Result is Success.
   function Description (Result : Track_Result) return String;
   --  Copy the complete owned MEL rejection description.
   --  @param Result Rejected terminal result.
   --  @return Full provider rejection text, including long descriptions.
   --  @exception Provider_Error Result is Success.
   procedure Close (Request : in out Track_Request);
   --  Idempotently close the observer without cancelling admitted provider work.
   --  @param Request Observer to close, serialized against Wait.
   --  @exception Provider_Error Native observer closure failed.

   type Scan_Request is limited private;
   --  Limited-controlled Return observer sharing Wait, Close and non-raising
   --  finalization with Abort_Request. Timeout and Close do not cancel work.
   type Scan_Result is private;
   --  Owned cached Return or MEL rejection; uses Status, Value, Rejection_Code
   --  and Description below. No acknowledgement or execution report is inferred.

   function Exec_Scan (Channel : Scheduling_Channel; Value : Scan_Task) return Scan_Request;
   --  Submit exactly SchedulingChannel::exec(IRSTScanTask), without registering
   --  metadata. Admission, completion and physical execution are distinct facts.
   --  @param Channel Live Enabled owner, serialized against Close until admission.
   --  @param Value Complete owned values copied and verified before provider entry.
   --  @return Limited observer; Wait reports actual Return or explicit MEL rejection.
   --  @exception Provider_Error Ineligible owner, native failure or provider exec exception.
   --  @exception Resource_Exhausted Session completion admission limit reached.

   function Wait (Request : Scan_Request; Timeout_Milliseconds : Natural) return Scan_Result;
   --  Observe the shared cached Return engine; zero timeout polls without cancellation.
   --  @param Request Open observer, serialized against Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   --  @return Actual Return or complete MEL rejection, never physical execution proof.
   --  @exception Timeout_Error Still pending; request remains open and unconsumed.
   --  @exception Provider_Error Closed observer, malformed payload or future exception.
   function Status (Result : Scan_Result) return AMS.MEL.IR.Channel.Outcome;
   --  Inspect the terminal outcome, independently of the Return payload.
   --  @param Result Owned cached result.
   --  @return Success for a valid Return, Rejected for an explicit MEL rejection.
   function Value (Result : Scan_Result) return AMS.MEL.IR.Channel.Command_Return;
   --  Inspect the actual provider Return value.
   --  @param Result Successful terminal result.
   --  @return Exact Return including Fail, Not_Supported and Not_Implemented.
   --  @exception Provider_Error Result is Rejected.
   function Rejection_Code (Result : Scan_Result) return AMS.MEL.IR.Channel.Error_Code;
   --  Inspect an explicit MEL rejection, distinct from a Return payload.
   --  @param Result Rejected terminal result.
   --  @return Exact MEL error code.
   --  @exception Provider_Error Result is Success.
   function Description (Result : Scan_Result) return String;
   --  Copy the complete owned MEL rejection description.
   --  @param Result Rejected terminal result.
   --  @return Full provider rejection text, including long descriptions.
   --  @exception Provider_Error Result is Success.
   procedure Close (Request : in out Scan_Request);
   --  Idempotently close the observer without cancelling admitted provider work.
   --  @param Request Observer to close, serialized against Wait.
   --  @exception Provider_Error Native observer closure failed.

   type Stare_Request is limited private;
   --  Limited-controlled Return observer sharing Wait, Close and non-raising
   --  finalization with Abort_Request. Timeout and Close do not cancel work.
   type Stare_Result is private;
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

   function Wait (Request : Stare_Request; Timeout_Milliseconds : Natural) return Stare_Result;
   --  Observe the shared cached Return engine; zero timeout polls without cancellation.
   --  @param Request Open observer, serialized against Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   --  @return Actual Return or complete MEL rejection, never physical execution proof.
   --  @exception Timeout_Error Still pending; request remains open and unconsumed.
   --  @exception Provider_Error Closed observer, malformed payload or future exception.
   function Status (Result : Stare_Result) return AMS.MEL.IR.Channel.Outcome;
   --  Inspect the terminal outcome, independently of the Return payload.
   --  @param Result Owned cached result.
   --  @return Success for a valid Return, Rejected for an explicit MEL rejection.
   function Value (Result : Stare_Result) return AMS.MEL.IR.Channel.Command_Return;
   --  Inspect the actual provider Return value.
   --  @param Result Successful terminal result.
   --  @return Exact Return including Fail, Not_Supported and Not_Implemented.
   --  @exception Provider_Error Result is Rejected.
   function Rejection_Code (Result : Stare_Result) return AMS.MEL.IR.Channel.Error_Code;
   --  Inspect an explicit MEL rejection, distinct from a Return payload.
   --  @param Result Rejected terminal result.
   --  @return Exact MEL error code.
   --  @exception Provider_Error Result is Success.
   function Description (Result : Stare_Result) return String;
   --  Copy the complete owned MEL rejection description.
   --  @param Result Rejected terminal result.
   --  @return Full provider rejection text, including long descriptions.
   --  @exception Provider_Error Result is Success.
   procedure Close (Request : in out Stare_Request);
   --  Idempotently close the observer without cancelling admitted provider work.
   --  @param Request Observer to close, serialized against Wait.
   --  @exception Provider_Error Native observer closure failed.

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
   type Stare_Request is limited record
      Owner : Abort_Request;
   end record;
   type Stare_Result is record
      Owner : Abort_Result;
   end record;
   type Scan_Request is limited record
      Owner : Abort_Request;
   end record;
   type Scan_Result is record
      Owner : Abort_Result;
   end record;
   type Track_Request is limited record
      Owner : Abort_Request;
   end record;
   type Track_Result is record
      Owner : Abort_Result;
   end record;
end AMS.MEL.IR.Scheduling.Commands;
