private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with AMS.MEL.IR.Channel;
with Interfaces;

package AMS.MEL.IR.Instrumentation is
   --  Conditionally required IR MEL instrumentation support (upstream RequiredIfInstrumentation):
   --  typed attachment, recording-level commands and owned completion reports. The metadata child
   --  polls copied reports. Inherited common channel services are accessed through common Channel
   --  support. Only the published Normal and Debug recording levels are bound.

   type Instrumentation_Config is private;
   --  Owned attachment configuration for the conditionally required instrumentation channel;
   --  constructing it performs no provider work.
   function Create_Config
     (Channel_ID : UCI_ID; Platform_ID : UCI_ID; Sensor_Location : Component_Location)
      return Instrumentation_Config;
   --  Builds a complete typed instrumentation attachment configuration without enabling or
   --  registering callbacks.
   --  @param Channel_ID Channel identity supplied to the provider.
   --  @param Platform_ID Platform identity supplied to the provider.
   --  @param Sensor_Location Component location with meter offsets and location identity strings.
   --  @return Independent configuration retaining all supplied values.

   type Instrumentation_Channel is limited private;
   --  Limited typed owner, initially closed. Open attaches without enabling. Native child state
   --  retains Session/provider lifetime after public Session closure. A common weak view does not
   --  keep this owner alive. Finalization attempts non-raising cleanup and can retain resources
   --  when detach ownership is uncertain. Externally serialize operations on a shared Ada owner.
   function Open (Parent : Session; Config : Instrumentation_Config) return Instrumentation_Channel;
   --  Attaches and verifies an InstrumentationChannel and its advertised instrumentation
   --  capability; no owner escapes on failure. This interface is conditionally required only for
   --  providers supporting instrumentation. Capabilities, common services and metadata
   --  registration are available while attached; instrumentation Submit requires explicit Enable.
   --  Native configuration text must be UTF-8 without embedded NUL. Failure cleanup retains the
   --  native graph if detach is uncertain.
   --  @param Parent Open IR Session supplying the child provider channel.
   --  @param Config Complete typed attachment configuration.
   --  @return Attached, not yet enabled, limited channel owner.
   --  @exception Provider_Error Closed Session, invalid text, provider attach/type/capability
   --  failure or native failure.
   function Is_Open (Channel : Instrumentation_Channel) return Boolean;
   --  Tests local native handle ownership, not readiness, enablement or provider health.
   --  @param Channel Typed channel owner to inspect.
   --  @return True when the local typed-channel handle is nonnull.
   procedure Enable (Channel : in out Instrumentation_Channel);
   --  Explicitly enables the provider channel; repeated successful Enable is a no-op. Failure
   --  marks native lifecycle failed rather than permitting instrumentation submission.
   --  @param Channel Attached owner to enable, or already enabled owner.
   --  @exception Provider_Error Closed/failed channel, provider rejection/exception or native
   --  failure.
   function Capabilities (Channel : Instrumentation_Channel) return IR.Channel.Channel_Capability;
   --  Copies common capability data while the channel is attached or enabled. Returned text and
   --  collections have independent Ada ownership.
   --  @param Channel Attached or enabled instrumentation channel.
   --  @return Owned common channel capability snapshot.
   --  @exception Provider_Error Invalid lifecycle or provider/native query or conversion failure.
   function As_Channel (Channel : Instrumentation_Channel) return IR.Channel.View;
   --  Creates a weak common Channel view while attached or enabled. Inherited KeepAlive, CommsTest
   --  and capabilities are accessed through common Channel support, not duplicated instrumentation
   --  methods. The view does not keep the typed owner alive, and closing the view does not close
   --  it.
   --  @param Channel Attached or enabled typed owner.
   --  @return New weak common channel view.
   --  @exception Provider_Error Closed/failed owner or native view creation failure.
   procedure Close (Channel : in out Instrumentation_Channel);
   --  Idempotently closes the typed public owner and deactivates metadata consumption. Pending
   --  requests defer physical disable/detach until completion. Cleanup disables if enable was
   --  attempted, detaches and destroys the provider channel before draining bridge callbacks.
   --  Synchronous detach failure retains the owner for retry; uncertain orphaned cleanup retains
   --  the provider graph. Automatic finalization attempts non-raising cleanup, not guaranteed
   --  immediate unload.
   --  @param Channel Typed owner to close; serialize against other uses.
   --  @exception Provider_Error Disable/detach or native cleanup failure; handle may remain for
   --  retry.

   type Priority is (Normal, Debug);
   --  Exactly the two upstream instrumentation data-recording levels: Normal = 0 and Debug = 1. No
   --  additional levels or sentinel are defined.
   --  @enum Normal Normal instrumentation recording level, upstream numeric value zero.
   --  @enum Debug Debug instrumentation recording level, upstream numeric value one.
   for Priority use (Normal => 0, Debug => 1);

   type Instrumentation_Level_Command is record
      Command_ID : Interfaces.Unsigned_32;
      Priority   : Instrumentation.Priority;
   end record;
   --  Supplied command controlling how much instrumentation data the MFA records. Sending it does
   --  not itself prove acceptance or completion.
   --  @field Command_ID Caller-supplied command identifier, preserving all 32 bits.
   --  @field Priority Requested Normal or Debug instrumentation recording level.

   type Instrumentation_Report is record
      Command_ID   : Interfaces.Unsigned_32;
      Size         : Interfaces.Unsigned_32;
      Timestamp_NS : Long_Long_Integer;
      Priority     : Instrumentation.Priority;
   end record;
   --  Canonical owned report used both for asynchronous command completion and metadata callbacks.
   --  The pinned declaration contains only these four fields, not a variable instrumentation-data
   --  payload.
   --  @field Command_ID Provider report command identifier, preserving all 32 bits.
   --  @field Size Raw provider size field; the pinned header does not specify its units or
   --  relation to any payload length.
   --  @field Timestamp_NS Provider timestamp in nanoseconds; clock and epoch are unspecified.
   --  @field Priority Provider-reported Normal or Debug instrumentation level; unknown values fail
   --  conversion.

   type Instrumentation_Outcome is (Success, Rejected);
   --  Terminal provider completion category, distinct from exceptions and native failures.
   --  @enum Success Provider returned a valid nonnull instrumentation report.
   --  @enum Rejected Provider completed with an explicit MEL error instead of a report.
   type Instrumentation_Error_Code is
     (None,
      Invalid_ID,
      Invalid_State,
      Invalid_Parameters,
      Insufficient_Permissions,
      Insufficient_Resources,
      Insufficient_Local_Resources,
      Insufficient_Remote_Resources,
      Unsupported);
   --  Known Common MEL rejection reasons mapped individually; not a numeric severity scale.
   --  Unknown provider codes are failures, not invented rejection categories.
   --  @enum None No specific reason supplied.
   --  @enum Invalid_ID Provider reports an invalid identifier.
   --  @enum Invalid_State Provider reports an invalid state.
   --  @enum Invalid_Parameters Provider reports invalid parameters.
   --  @enum Insufficient_Permissions Provider reports insufficient permissions.
   --  @enum Insufficient_Resources Provider reports insufficient resources without a local/remote
   --  distinction.
   --  @enum Insufficient_Local_Resources Provider reports insufficient local resources.
   --  @enum Insufficient_Remote_Resources Provider reports insufficient remote resources.
   --  @enum Unsupported Provider reports the request is unsupported.
   type Instrumentation_Result is private;
   --  Ada-owned terminal report or explicit rejection. Provider exceptions, null successful
   --  results, unknown priority/error values and native failures raise Provider Error rather than
   --  becoming a normal Rejected result. Values survive request closure.
   function Status (Result : Instrumentation_Result) return Instrumentation_Outcome;
   --  Returns the terminal outcome stored in the result.
   --  @param Result Owned terminal instrumentation result.
   --  @return Success or explicit provider rejection.
   function Report (Result : Instrumentation_Result) return Instrumentation_Report
   with Pre => Status (Result) = Success;
   --  Accesses the report of a terminal result. Requires Status equal to Success; enabled
   --  precondition checking raises Assertion Error on violation. The accessor body does not add an
   --  outcome check.
   --  @param Result Owned terminal result satisfying the declared outcome precondition.
   --  @return Independent four-field instrumentation report.
   function Rejection_Code (Result : Instrumentation_Result) return Instrumentation_Error_Code
   with Pre => Status (Result) = Rejected;
   --  Accesses the rejection detail of a terminal result. Requires Status equal to Rejected;
   --  enabled precondition checking raises Assertion Error on violation. The accessor body does
   --  not add an outcome check.
   --  @param Result Owned terminal result satisfying the declared outcome precondition.
   --  @return Mapped provider rejection reason.
   function Description (Result : Instrumentation_Result) return String
   with Pre => Status (Result) = Rejected;
   --  Accesses the rejection detail of a terminal result. Requires Status equal to Rejected;
   --  enabled precondition checking raises Assertion Error on violation. The accessor body does
   --  not add an outcome check.
   --  @param Result Owned terminal result satisfying the declared outcome precondition.
   --  @return Independent Ada string containing the complete rejection description.

   type Instrumentation_Request is limited private;
   --  Limited public asynchronous-request owner. A private completion worker consumes the provider
   --  future once and caches the result while retaining the native channel, Session and provider
   --  code. Public request Close and finalization do not cancel provider work; a never-completing
   --  future can retain resources indefinitely. Finalization is non-raising.
   function Submit
     (Channel : Instrumentation_Channel; Command : Instrumentation_Level_Command)
      return Instrumentation_Request;
   --  Submits the supplied command only while the typed channel is enabled; does not implicitly
   --  Enable. Copies command values synchronously and returns a request handle, not proof of
   --  acceptance or completion. The native provider send runs outside the lifecycle lock so
   --  synchronous metadata callbacks can proceed. A pending request retains provider resources
   --  independently after public channel and Session closure.
   --  @param Channel Enabled typed instrumentation owner; serialize application lifecycle
   --  operations.
   --  @param Command Command identifier and one of the two supported priority levels.
   --  @return Open limited request handle for later Wait.
   --  @exception Provider_Error Closed/not-enabled channel, provider send exception or native
   --  failure.
   --  @exception Resource_Exhausted Shared asynchronous-request admission limit reached.
   function Is_Open (Request : Instrumentation_Request) return Boolean;
   --  Tests public request handle ownership, not future readiness.
   --  @param Request Request owner to inspect.
   --  @return True while the local native request handle is nonnull.
   function Wait
     (Request : Instrumentation_Request; Timeout_Milliseconds : Natural)
      return Instrumentation_Result;
   --  Waits for a terminal result without consuming the request. Timeout is not cancellation;
   --  later Wait may complete, and repeated terminal waits return the identical cached result.
   --  Explicit provider rejection is returned separately from provider exceptions or native
   --  failures. Rejection text is recovered completely by a checked cached retry if necessary;
   --  invalid UTF-8/NUL text receives a native fallback. Close must not race Wait on the same
   --  request.
   --  @param Request Open public instrumentation request.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   --  @return Owned report or explicit rejection.
   --  @exception Timeout_Error Future has not completed within the wait; request remains open.
   --  @exception Provider_Error Closed request, provider exception, invalid terminal result, or
   --  native/deferred-cleanup failure.
   procedure Close (Request : in out Instrumentation_Request);
   --  Idempotently closes the public request handle without terminating provider work. Pending
   --  completion retains the native graph independently of public owners. Finalization attempts
   --  this cleanup without raising; the Ada wrapper ignores native close status. Do not race Wait
   --  on this same owner.
   --  @param Request Public request owner to close.

private
   package US renames Ada.Strings.Unbounded;
   type Instrumentation_Config is record
      Channel, Platform : UCI_ID;
      Location          : Component_Location;
   end record;
   type Instrumentation_Channel is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Instrumentation_Handle := AMS.MEL_C_API.Null_Instrumentation;
   end record;
   overriding
   procedure Finalize (Channel : in out Instrumentation_Channel);

   type Instrumentation_Result is record
      Result_Status : Instrumentation_Outcome := Success;
      Result_Report : Instrumentation_Report := (0, 0, 0, Normal);
      Result_Code   : Instrumentation_Error_Code := None;
      Result_Text   : US.Unbounded_String;
   end record;
   type Instrumentation_Request_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Instrumentation_Request_Handle :=
        AMS.MEL_C_API.Null_Instrumentation_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Instrumentation_Request_Owner);
   type Instrumentation_Request is limited record
      Owner : Instrumentation_Request_Owner;
   end record;
end AMS.MEL.IR.Instrumentation;
