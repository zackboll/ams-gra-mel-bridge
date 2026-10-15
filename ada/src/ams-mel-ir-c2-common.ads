private with Ada.Finalization;
private with Ada.Strings.Unbounded;
with AMS.MEL.IR.Channel;
private with AMS.MEL_C_API;

package AMS.MEL.IR.C2.Common is
   --  Historical C2-only access to inherited common Channel operations. The
   --  canonical common facade supports all five typed families through weak
   --  views; this compatibility child takes a typed C2 owner directly. Channel
   --  selectors and reports are shared with the common package, but the C2
   --  Return owner/result and this child's Comms owner/result remain distinct
   --  Ada types. Handles cannot be substituted across these APIs. These are
   --  wrappers of existing upstream Channel services, not additional services.
   --  Serialize typed-owner operations externally and do not race request Close
   --  against Wait. Native pending work independently retains provider lifetime.

   function Send_Keep_Alive (Channel : Control_Channel) return Return_Request;
   --  Submit inherited keep-alive while C2 is Attached or Enabled, without
   --  implicitly enabling. Uses the shared native Return completion engine and
   --  Session admission pool. Exhaustion raises the resource-exhaustion exception;
   --  other native failures raise a provider error. Submission is not completion.
   --  @param Channel Open eligible C2 owner; serialize its operations externally.
   --  @return C2 Return observer; use the parent package's Wait/Close and inspect payload separately from outcome.
   type Comms_Request is limited private;
   --  Legacy limited controlled communications-test observer, distinct from
   --  the canonical common request type despite sharing the native engine.
   --  Finalization closes without raising. Observer closure is not cancellation
   --  or direct release of a permit still held by pending native work.
   function Submit_Comms_Test
     (Channel    : Control_Channel;
      Channel_ID : IR.Channel.Comms_Channel_ID;
      Command_ID : C2.Command_ID;
      Request_ID : IR.Channel.Comms_Request_ID) return Comms_Request;
   --  Submit inherited communications test while Attached or Enabled. The
   --  provider send runs after an accounted native lifetime claim. Session
   --  admission refusal raises the resource-exhaustion exception; closed/failed
   --  channels and other native failures raise a provider error.
   --  @param Channel Open eligible C2 owner; externally serialize owner operations.
   --  @param Channel_ID Unsigned communications-test selector, not a channel UCI identity.
   --  @param Command_ID Canonical command identifier through its C2 compatibility subtype.
   --  @param Request_ID Unsigned communications-test correlation identifier.
   --  @return Legacy observer of an admitted future, not evidence of provider acceptance.
   type Comms_Result is private;
   --  Owned legacy communications-test terminal report or explicit rejection.
   --  Uses the parent's outcome/error types and the shared Channel report type,
   --  not the canonical common facade's distinct Ada result owner.
   function Status (Result : Comms_Result) return Outcome;
   --  Read the cached result category without provider access.
   --  @param Result Legacy communications-test result to inspect.
   --  @return Success for a report or Rejected for an explicit upstream ErrorOr error.
   function Report (Result : Comms_Result) return IR.Channel.Comms_Test_Report
   with Pre => Status (Result) = Success;
   --  Copy the report under the declared Success precondition. The expression
   --  body adds no independent outcome check when assertions are disabled.
   --  @param Result Successful legacy result.
   --  @return Shared Channel report containing provider-returned command/request IDs.
   function Rejection_Code (Result : Comms_Result) return Error_Code
   with Pre => Status (Result) = Rejected;
   --  Read the rejection category under the declared Rejected precondition;
   --  there is no independent body check.
   --  @param Result Rejected legacy communications-test result.
   --  @return C2 error category mapped from the upstream rejection, not bridge admission failure.
   function Description (Result : Comms_Result) return String
   with Pre => Status (Result) = Rejected;
   --  Copy stored text under the declared Rejected precondition; the body does
   --  not independently check outcome. The legacy diagnostic helper substitutes
   --  a fixed native-operation-failure message when the diagnostic is empty.
   --  @param Result Rejected legacy communications-test result.
   --  @return Owned diagnostic, using cached retry for text beyond the initial buffer.
   function Wait (Request : Comms_Request; Timeout_Milliseconds : Natural) return Comms_Result;
   --  Await a terminal report/rejection through the shared native Comms engine.
   --  Results are cached for repeat waits. Timeout raises the inherited timeout
   --  exception without cancellation or consumption; other native failures raise
   --  a provider error. Long rejection text is retried and consistency checked.
   --  Unlike the canonical Ada wrapper, this legacy body directly converts the
   --  native rejection number; native enum mapping still rejects unknown codes.
   --  @param Request Open legacy observer; must not race Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   --  @return Owned legacy result, independent of later metadata callbacks.
   procedure Close (Request : in out Comms_Request);
   --  Idempotently clear the public observer without cancelling its future.
   --  Pending native accounting and provider retention survive independently;
   --  unexpected native failure raises a provider error.
   --  @param Request Legacy observer to close, externally serialized against Wait.
   function Capabilities (Channel : Control_Channel) return IR.Channel.Channel_Capability;
   --  Query C2 capabilities while Attached or Enabled and copy them into the
   --  same owned value type as canonical common queries. Failed/closed lifecycle
   --  or native conversion failure raises a provider error. Metadata reports
   --  do not register callbacks or establish implemented Ada service support.
   --  @param Channel Open C2 owner eligible for a capability query.
   --  @return Independent snapshot valid after channel, Session and provider closure.
private
   package US renames Ada.Strings.Unbounded;
   type Comms_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Comms_Request_Handle := AMS.MEL_C_API.Null_Comms_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Comms_Owner);
   type Comms_Request is limited record
      Owner : Comms_Owner;
   end record;
   type Comms_Result is record
      Result_Status : Outcome := Success;
      Result_Report : IR.Channel.Comms_Test_Report := (0, 0);
      Result_Code   : Error_Code := None;
      Result_Text   : US.Unbounded_String;
   end record;
end AMS.MEL.IR.C2.Common;
