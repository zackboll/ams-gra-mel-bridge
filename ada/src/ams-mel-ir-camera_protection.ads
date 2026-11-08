with AMS.MEL.IR.Channel;
private with Ada.Strings.Unbounded;

package AMS.MEL.IR.Camera_Protection is
   --  Canonical owned values and request result for the pinned upstream CameraProtectCmd, CmdResp
   --  and CameraProtectCmdResp (@RequiredIfCameraProtect). One definition is shared by
   --  C2.Submit_Camera_Protection and the opt-in C2.Metadata camera-protection owner. No response
   --  deadline, camera behaviour or automatic retry is introduced by the bridge.

   Not_Supported : exception;
   --  The provider answered NotSupported for the C2 CameraProtectCmdResp registration. This is
   --  distinct from a registration failure, a provider exception, an asynchronous MEL rejection
   --  and the Protection_Unsupported response value; existing C2 owners are unaffected.

   subtype Command_ID is AMS.MEL.IR.Channel.Command_ID;
   --  Canonical unsigned 32-bit command identifier; all bits are preserved.

   type Nanoseconds is range -(2**63) .. (2**63) - 1 with Size => 64;
   --  Signed 64-bit nanosecond count; no clock domain, epoch or deadline is asserted by the bridge.

   type Protection_Status is (Protection_OK, Protection_Unavailable, Protection_Unsupported);
   --  Exactly the three pinned CmdResp alternatives. Every value is a successful provider
   --  response; none is a MEL rejection or a registration NotSupported result. Unknown native
   --  values are never remapped to one of these and raise Provider_Error instead.
   --  @enum Protection_OK Upstream CmdResp::OK, value 0: the MFA reports protection capability.
   --  @enum Protection_Unavailable Upstream CmdResp::Unavailable, value 1: protection is
   --  currently unavailable.
   --  @enum Protection_Unsupported Upstream CmdResp::Unsupported, value 2: protection is not
   --  supported by the MFA.

   type Protection_Command is record
      ID               : Command_ID := 0;
      Response_Time_NS : Nanoseconds := 0;
      Protect          : Boolean := False;
   end record;
   --  Owned copy of all three CameraProtectCmd fields; defaults equal the upstream defaults.
   --  @field ID Unique command identifier supplied by the caller.
   --  @field Response_Time_NS Response time for this command, signed nanoseconds, passed
   --  unconverted; the bridge enforces no deadline.
   --  @field Protect True enables camera self-protection; False disables it.

   type Protection_Response is record
      ID               : Command_ID := 0;
      Response_Time_NS : Nanoseconds := 0;
      Status           : Protection_Status := Protection_OK;
   end record;
   --  Owned copy of all three CameraProtectCmdResp fields, independent of native owners.
   --  @field ID Command identifier reported by the MFA.
   --  @field Response_Time_NS Response time reported by the MFA, signed nanoseconds.
   --  @field Status Exact CmdResp alternative reporting the MFA camera self-protection capability.

   subtype Outcome is AMS.MEL.IR.Channel.Outcome;
   use type Outcome;
   --  Success (a response with any of the three Status values) or explicit MEL rejection.
   subtype Error_Code is AMS.MEL.IR.Channel.Error_Code;
   --  Known MEL rejection categories; unknown codes raise Provider_Error instead.

   type Protection_Result is private;
   --  Owned terminal request result: a response or an explicit rejection. Provider exceptions,
   --  null successful responses, unknown CmdResp values, unknown error codes and deferred
   --  cleanup failure raise exceptions rather than becoming results.
   function Status (Result : Protection_Result) return Outcome;
   --  Read the terminal category without provider access.
   --  @param Result Owned terminal result.
   --  @return Success or Rejected.
   function Response (Result : Protection_Result) return Protection_Response
   with Pre => Status (Result) = AMS.MEL.IR.Channel.Success;
   --  Read the complete response of a successful result.
   --  @param Result Successful terminal result.
   --  @return Owned response, whose Status may be Protection_Unavailable or Protection_Unsupported.
   function Rejection_Code (Result : Protection_Result) return Error_Code
   with Pre => Status (Result) = AMS.MEL.IR.Channel.Rejected;
   --  Read the MEL rejection category of a rejected result.
   --  @param Result Rejected terminal result.
   --  @return Mapped upstream error code.
   function Description (Result : Protection_Result) return String
   with Pre => Status (Result) = AMS.MEL.IR.Channel.Rejected;
   --  Copy the complete provider rejection description.
   --  @param Result Rejected terminal result.
   --  @return Owned description, retried from the native cache when long.

   type Protection_Request is new AMS.MEL.IR.Camera_Protection_Request_Owner with private;
   --  Limited request owner returned by C2.Submit_Camera_Protection. Pending native work retains
   --  its C2 channel, Session and provider library after request/channel/Session closure;
   --  finalization is non-raising and Close never cancels provider work.

   function Is_Open (Request : Protection_Request) return Boolean;
   --  Test local handle presence without polling completion.
   --  @param Request Request owner to inspect.
   --  @return True while a native request handle is held.
   function Wait
     (Request : Protection_Request; Timeout_Milliseconds : Natural) return Protection_Result;
   --  Wait finitely for the terminal result. Timeout raises Timeout_Error and neither consumes
   --  nor cancels the request; repeated terminal waits return the cached result.
   --  @param Request Open request; must not race Close.
   --  @param Timeout_Milliseconds Maximum wait; zero polls.
   --  @return Owned terminal response or explicit rejection.
   --  @exception Timeout_Error The request is still pending.
   --  @exception Provider_Error Closed owner, provider/future exception, null response, unknown
   --  CmdResp value, unknown error code, deferred cleanup failure or native failure.
   procedure Close (Request : in out Protection_Request);
   --  Idempotently release only the public observer; pending work continues.
   --  @param Request Request owner; serialize against Wait.
   --  @exception Provider_Error Native close failed.

private
   package US renames Ada.Strings.Unbounded;
   type Protection_Request is new AMS.MEL.IR.Camera_Protection_Request_Owner with record
      null;
   end record;

   type Protection_Result is record
      Result_Status   : Outcome := AMS.MEL.IR.Channel.Success;
      Result_Response : Protection_Response;
      Result_Code     : Error_Code := AMS.MEL.IR.Channel.None;
      Result_Text     : US.Unbounded_String;
   end record;

end AMS.MEL.IR.Camera_Protection;
