private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with AMS.MEL.IR.Track.System_Data;
with Interfaces;

--  Optional IR MEL C2Channel::send (SystemTrackDataResponse) (@Optional),
--  IRMEL-ADA-008. This is the C2 overload, independent of the TrackChannel
--  overload in AMS.MEL.IR.Track.System_Data: it requires an Enabled C2 channel,
--  uses C2 lifecycle and C2 request accounting, and never touches a Track
--  channel. Only the owned input value and the CommandStatus value types are
--  shared with the Track package; request and result owners are distinct.

package AMS.MEL.IR.C2.System_Data is
   --  Optional outbound C2 system Track data response. Positive qualification is
   --  mock-only. A successful future means only that the provider returned a
   --  CommandStatus; it does not establish operational incorporation of the data.

   subtype System_Track_Data_Response is AMS.MEL.IR.Track.System_Data.System_Track_Data_Response;
   --  The one public owned input value shared with the Track overload. All
   --  twelve fields are copied exactly; validity flags never zero other fields.
   subtype Command_State is AMS.MEL.IR.Track.System_Data.Command_State;
   --  Shared CommandState values 0..4; no second enumeration mapping exists.
   subtype Cannot_Comply is AMS.MEL.IR.Track.System_Data.Cannot_Comply;
   --  Shared CannotComply values 0..46; no second enumeration mapping exists.
   subtype Response_Outcome is AMS.MEL.IR.Track.System_Data.Response_Outcome;
   --  Success for a returned CommandStatus (whatever its own state), Rejected
   --  only for an upstream ErrorOr error.
   subtype Response_Error_Code is AMS.MEL.IR.Track.System_Data.Response_Error_Code;
   --  Known upstream ErrorOr rejection categories, values 0..8.
   use type Response_Outcome;

   type Command_Status is private;
   --  Owned CommandStatus returned by a successful C2 future, copied during
   --  Wait so no native pointer escapes. Its own Rejected state is still a
   --  Success future outcome. Reuses the shared enumeration types above.
   function Command_ID (Status : Command_Status) return Interfaces.Unsigned_32;
   --  @param Status Owned command status.
   --  @return Provider-reported unsigned command identifier, all 32 bits.
   function State (Status : Command_Status) return Command_State;
   --  @param Status Owned command status.
   --  @return Reported command messaging state, not physical completion.
   function Reason (Status : Command_Status) return Cannot_Comply;
   --  @param Status Owned command status.
   --  @return Reported inability category; Not_Set denotes no reason.
   function Reason_Description (Status : Command_Status) return String;
   --  @param Status Owned command status.
   --  @return Complete reason text, validated natively as UTF-8 without NUL.

   type Response_Result is private;
   --  Owned terminal C2 result: a successful CommandStatus or an explicit
   --  ErrorOr rejection with its complete diagnostic. Provider exceptions,
   --  null/malformed payloads, unknown codes and cleanup failures raise
   --  Provider_Error instead. Distinct from the Track overload's result type.
   function Status (Result : Response_Result) return Response_Outcome;
   --  @param Result Owned terminal result.
   --  @return Success or Rejected.
   function Command (Result : Response_Result) return Command_Status
   with Pre => Status (Result) = AMS.MEL.IR.Track.System_Data.Success;
   --  @param Result Successful result.
   --  @return Owned CommandStatus; its own state may be Rejected.
   function Rejection_Code (Result : Response_Result) return Response_Error_Code
   with Pre => Status (Result) = AMS.MEL.IR.Track.System_Data.Rejected;
   --  @param Result Explicitly rejected result.
   --  @return Known upstream error code, not bridge admission exhaustion.
   function Description (Result : Response_Result) return String
   with Pre => Status (Result) = AMS.MEL.IR.Track.System_Data.Rejected;
   --  @param Result Explicitly rejected result.
   --  @return Complete owned diagnostic, including long descriptions.

   type Response_Request is limited private;
   --  Limited controlled C2 observer, distinct from the Track response owner.
   --  Finalization closes without raising. Closing never cancels provider work;
   --  pending native work retains the C2/Session/provider graph independently.

   function Submit_System_Track_Data_Response
     (Channel : Control_Channel; Value : System_Track_Data_Response) return Response_Request;
   --  Copy and submit the optional response exactly once through the C2
   --  overload while C2 is Enabled, without a bridge lock or retry.
   --  @param Channel Enabled C2 owner; externally serialize its operations.
   --  @param Value Complete response copied before submission returns.
   --  @return Admitted observer, not provider acceptance or incorporation.
   --  @exception Provider_Error Closed/Attached-only C2, refusal or send failure.
   --  @exception Resource_Exhausted This Session has no free completion permit.
   function Is_Open (Request : Response_Request) return Boolean;
   --  @param Request Observer to inspect.
   --  @return Local handle presence, not completion state.
   function Wait
     (Request : Response_Request; Timeout_Milliseconds : Natural) return Response_Result;
   --  Cached terminal wait; timeout never cancels, consumes or resends.
   --  @param Request Open observer; serialize against Close.
   --  @param Timeout_Milliseconds Finite wait; zero polls.
   --  @return Owned CommandStatus success or explicit ErrorOr rejection.
   --  @exception Timeout_Error Still pending.
   --  @exception Provider_Error Closed observer, provider exception, null or
   --  malformed CommandStatus, unknown code, native or deferred cleanup failure.
   procedure Close (Request : in out Response_Request);
   --  Idempotently close the observer only; never cancel provider execution.
   --  @param Request Observer; serialize against Wait.
   --  @exception Provider_Error Native close failed.
private
   package US renames Ada.Strings.Unbounded;
   type Command_Status is record
      Status_Command_ID : Interfaces.Unsigned_32 := 0;
      Status_State      : Command_State := AMS.MEL.IR.Track.System_Data.Not_Set;
      Status_Reason     : Cannot_Comply := AMS.MEL.IR.Track.System_Data.Not_Set;
      Status_Text       : US.Unbounded_String;
   end record;
   type Response_Result is record
      Result_Status  : Response_Outcome := AMS.MEL.IR.Track.System_Data.Success;
      Result_Command : Command_Status;
      Result_Code    : Response_Error_Code := AMS.MEL.IR.Track.System_Data.None;
      Result_Text    : US.Unbounded_String;
   end record;
   type Response_Request_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.C2_System_Response_Request_Handle :=
        AMS.MEL_C_API.Null_C2_System_Response_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Response_Request_Owner);
   type Response_Request is limited record
      Owner : Response_Request_Owner;
   end record;
end AMS.MEL.IR.C2.System_Data;
