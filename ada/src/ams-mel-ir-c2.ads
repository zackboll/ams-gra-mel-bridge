private with Ada.Finalization;
private with Ada.Strings.Unbounded;
with Ada.Containers.Indefinite_Vectors;
with AMS.MEL.IR.Channel;
private with AMS.MEL_C_API;

package AMS.MEL.IR.C2 is
   --  Safe typed command-and-control channel and asynchronous command owners.
   --  Open attaches; Enable is explicit. Typed sends require Enabled, unlike
   --  inherited common operations which also admit Attached. Serialize owner
   --  operations externally. Native submission accounts lifetime under its lock
   --  and calls provider send unlocked; this is not permission to race public
   --  Ada owners. Future completion is not proof of a physical state transition
   --  or completed BIT; command-status metadata is a separate reporting path.

   type Control_Config is private;
   --  Owned channel/platform identities and sensor component location used to
   --  attach a command-and-control channel; constructing it opens no resources.
   function Create_Config
     (Channel_ID : UCI_ID; Platform_ID : UCI_ID; Sensor_Location : Component_Location)
      return Control_Config;
   --  Copy attachment configuration without provider access or text validation.
   --  @param Channel_ID Requested channel UCI identity and label.
   --  @param Platform_ID Platform UCI identity and label.
   --  @param Sensor_Location Sensor offsets and location identity to copy.
   --  @return Independent configuration value used by Open.

   type Control_Channel is limited private;
   --  Limited controlled typed owner retaining native Session/provider lifetime.
   --  Explicit Close is preferred; non-raising finalization is a fallback.
   --  Pending native requests independently retain the necessary provider graph.
   function Open (Parent : Session; Config : Control_Config) return Control_Channel;
   --  Require an open Session, published command-and-control capability and a
   --  successful typed attachment. Text is validated as UTF-8 by native code.
   --  The returned channel is Attached, not enabled; failure raises a provider
   --  error and no partially open Ada owner escapes.
   --  @param Parent Open provider Session whose native state is retained.
   --  @param Config Attachment identities and sensor location to copy.
   --  @return Attached typed owner; call Enable before typed command submission.
   function Is_Open (Channel : Control_Channel) return Boolean;
   --  Test local handle presence, not Enabled state or provider health.
   --  @param Channel Typed owner to inspect without a provider call.
   --  @return True for a present native handle, even when lifecycle has failed.
   procedure Enable (Channel : in out Control_Channel);
   --  Enable an Attached channel; already Enabled is idempotent. Provider
   --  refusal or exception makes the native lifecycle Failed and raises a
   --  provider error; a failed channel is not re-enabled by retrying this call.
   --  @param Channel Open Attached or already Enabled owner; serialize owner operations.

   --  Returns a new weak common Channel view of this open Control_Channel.
   --  Raises Provider_Error if the Control_Channel is closed; no open View
   --  escapes on failure. The View does not own or keep this owner alive, and
   --  closing the View does not close this owner. See AMS.MEL.IR.Channel.
   function As_Channel (Channel : Control_Channel) return AMS.MEL.IR.Channel.View;
   --  Allocate a weak common view without changing attachment or enabling.
   --  A closed owner raises a provider error; the view does not retain this
   --  owner and its closure does not close the typed channel.
   --  @param Channel Open typed owner whose resource is weakly referenced.
   --  @return Independent weak view; handle presence is not lifecycle eligibility.

   --  Source-compatible name for the canonical common command identifier.
   subtype Command_ID is AMS.MEL.IR.Channel.Command_ID;
   --  Compatibility subtype of the canonical common 32-bit command identifier;
   --  this is the same Ada type and needs no conversion between the two names.
   type BIT_ID is mod 2**32 with Size => 32;
   --  Numeric unsigned 32-bit BIT selector used by the upstream BIT command,
   --  not the structured UCI identity used by other interfaces.
   type BIT_ID_Array is array (Positive range <>) of BIT_ID;
   --  Caller-owned ordered sequence of numeric BIT selectors. Sends copy its
   --  complete range in order, retaining duplicates and non-one lower bounds.
   package Fault_Code_Vectors is new
     Ada.Containers.Indefinite_Vectors (Index_Type => Positive, Element_Type => String);
   --  Standard positive-index vector of owned fault-code strings. Fault-clear
   --  submission preserves order and duplicates and copies length-delimited
   --  UTF-8 text; this instance adds no generated first-party declarations.

   type MFA_State is
     (Not_Set,
      Unknown,
      Not_Installed,
      Off,
      Pre_Initialization,
      Initialization,
      Standby,
      Operate,
      Operate_Rx_Only,
      Operate_Tx_Only,
      Maintenance,
      Calibration,
      Initiated_BIT,
      Shutdown,
      Degraded);
   --  Requested MFA state with explicit native values 0..14. This is a distinct
   --  Ada type from the shared Status package's state type even though values
   --  correspond. Representation does not prove a provider supports a state.
   --  @enum Not_Set State not set, value 0.
   --  @enum Unknown Unknown state, value 1.
   --  @enum Not_Installed Not-installed state, value 2.
   --  @enum Off Powered-off state, value 3.
   --  @enum Pre_Initialization Pre-initialization state, value 4.
   --  @enum Initialization Initialization state, value 5.
   --  @enum Standby Standby state, value 6.
   --  @enum Operate Operational state, value 7.
   --  @enum Operate_Rx_Only Receive-only operational state, value 8.
   --  @enum Operate_Tx_Only Transmit-only operational state, value 9, present in the pinned declaration.
   --  @enum Maintenance Maintenance state, value 10.
   --  @enum Calibration Calibration state, value 11.
   --  @enum Initiated_BIT Initiated built-in-test state, value 12.
   --  @enum Shutdown Shutdown state, value 13.
   --  @enum Degraded Degraded state, value 14.
   for MFA_State use
     (Not_Set            => 0,
      Unknown            => 1,
      Not_Installed      => 2,
      Off                => 3,
      Pre_Initialization => 4,
      Initialization     => 5,
      Standby            => 6,
      Operate            => 7,
      Operate_Rx_Only    => 8,
      Operate_Tx_Only    => 9,
      Maintenance        => 10,
      Calibration        => 11,
      Initiated_BIT      => 12,
      Shutdown           => 13,
      Degraded           => 14);
   for MFA_State'Size use 32;

   type Coordinate_Frame_Reference is (Inertial, Aircraft);
   --  Reference selection for each scan-center angle; no transformation is
   --  performed by the bridge.
   --  @enum Inertial Inertial reference frame, value 0.
   --  @enum Aircraft Aircraft reference frame, value 1.
   for Coordinate_Frame_Reference use (Inertial => 0, Aircraft => 1);
   for Coordinate_Frame_Reference'Size use 32;
   type Degradation_Method is
     (Capacity_Degradation, Volume_Degradation, Range_Degradation, Revisit_Degradation);
   --  Requested method for handling overload of non-skippable tasking. These
   --  categories do not cause bridge-side scheduling or degradation algorithms.
   --  @enum Capacity_Degradation Capacity-degradation category, value 0; default method.
   --  @enum Volume_Degradation Volume-degradation category, value 1.
   --  @enum Range_Degradation Range-degradation category, value 2.
   --  @enum Revisit_Degradation Revisit-degradation category, value 3.
   for Degradation_Method use
     (Capacity_Degradation => 0,
      Volume_Degradation   => 1,
      Range_Degradation    => 2,
      Revisit_Degradation  => 3);
   for Degradation_Method'Size use 32;
   type Scan_Value is mod 2**32 with Size => 32;
   --  Unsigned 32-bit scan scalar/identifier. Scan-type controls retain the
   --  upstream integer representation; they are not constrained Ada Booleans.
   type Scan_Parameters is record
      Elevation_Defined_With_Range_And_Altitude : Boolean := False;
      Center_Azimuth_Rad                        : Long_Float := 0.0;
      Center_Elevation_Rad                      : Long_Float := 0.0;
      Center_Frame_Reference_EL                 : Coordinate_Frame_Reference := Inertial;
      Center_Frame_Reference_AZ                 : Coordinate_Frame_Reference := Aircraft;
      Scan_Width_Rad                            : Long_Float := 0.0;
      Scan_Height_Rad                           : Long_Float := 0.0;
      Continuous_Scan                           : Scan_Value := 0;
      Returning                                 : Scan_Value := 0;
      Agile_Scan                                : Scan_Value := 0;
      Scan_ID                                   : Scan_Value := 0;
      Scan_Rate_Rad_Per_Second                  : Long_Float := 0.0;
      Preferred_Revisit_Interval_Seconds        : Long_Float := 0.0;
      Required_Revisit_Interval_Seconds         : Long_Float := 0.0;
      Max_Range_Of_Interest_M                   : Scan_Value := 0;
      Min_Range_Of_Interest_M                   : Scan_Value := 0;
      Elevation_Scan_Center_Altitude_M          : Scan_Value := 0;
      Elevation_Scan_Center_Range_M             : Scan_Value := 0;
      Degradation                               : Degradation_Method := Capacity_Degradation;
   end record;
   --  Complete scan payload, associated by upstream ModeCmd with volume-scan
   --  scheduling. Native checks representation domains, not mode/scan semantic
   --  compatibility, ranges, finite values or physical feasibility. Floating
   --  inputs cross the native boundary as double; no normalization is performed.
   --  @field Elevation_Defined_With_Range_And_Altitude Select elevation defined by range/altitude rather than the angular center; upstream introductory wording is inconsistent.
   --  @field Center_Azimuth_Rad Center pointing azimuth in radians.
   --  @field Center_Elevation_Rad Center pointing elevation in radians.
   --  @field Center_Frame_Reference_EL Reference for elevation center; defaults to Inertial.
   --  @field Center_Frame_Reference_AZ Reference for azimuth center; defaults to Aircraft.
   --  @field Scan_Width_Rad Scan angular width in radians.
   --  @field Scan_Height_Rad Scan angular height in radians.
   --  @field Continuous_Scan Unsigned single-bar back-and-forth scan control; no binary-only domain is enforced.
   --  @field Returning Unsigned scan-direction disambiguation/feedback control used with continuous scan.
   --  @field Agile_Scan Unsigned control for prioritized subsection collection; no Boolean conversion occurs.
   --  @field Scan_ID Unsigned scan identifier, not assigned by the bridge.
   --  @field Scan_Rate_Rad_Per_Second Radians per second; negative decreases azimuth, positive increases it.
   --  @field Preferred_Revisit_Interval_Seconds Preferred target revisit interval in seconds.
   --  @field Required_Revisit_Interval_Seconds Required target revisit interval in seconds.
   --  @field Max_Range_Of_Interest_M Maximum range of interest in meters; zero means unset upstream, and set values may be ignored operationally.
   --  @field Min_Range_Of_Interest_M Minimum range of interest in meters; zero means unset upstream, not bridge-side validation.
   --  @field Elevation_Scan_Center_Altitude_M Altitude scalar for elevation stabilization with center azimuth and range; zero means unset. Ada names meters; pinned field prose supplies no altitude datum.
   --  @field Elevation_Scan_Center_Range_M Range scalar paired with altitude for elevation stabilization; zero means unset. No coordinate calculation is performed here.
   --  @field Degradation Requested overload-handling category, defaulting to capacity degradation.
   Default_Scan_Parameters : constant Scan_Parameters := (others => <>);
   --  Record defaults: False elevation selection; zero scalars, angles and
   --  intervals; Inertial elevation reference, Aircraft azimuth reference and
   --  capacity degradation. This value is not a provider validity guarantee.

   type Mode_Request is limited private;
   --  Limited controlled observer of a Mode future. Native pending work retains
   --  the provider graph independently of observer/channel/Session closure.
   --  Finalization is non-raising; explicit Close must not race Wait.
   type MFA_Mode is (Unused, Task_Sched, Scan_Volume_Sched, Scan_Bar_Sched);
   --  Upstream mode categories, without bridge validation of state/mode pairs.
   --  @enum Unused Mode used for states without internal modes, value 0.
   --  @enum Task_Sched Operational task-scheduling mode, value 1; upstream specifies general MFA support.
   --  @enum Scan_Volume_Sched Operational volume-scan scheduling mode, value 2.
   --  @enum Scan_Bar_Sched Operational bar-scan scheduling growth mode, value 3; not proof of provider support.
   function Submit_Mode
     (Channel         : Control_Channel;
      ID              : Command_ID;
      State           : MFA_State;
      Mode            : MFA_Mode;
      Scan_Parameters : AMS.MEL.IR.C2.Scan_Parameters := Default_Scan_Parameters)
      return Mode_Request;
   --  Submit a complete ModeCmd while Enabled. Inputs are copied before send;
   --  no state/mode/scan feasibility validation is promised. The native lifetime
   --  claim precedes unlocked provider send. Session admission exhaustion raises
   --  the resource-exhaustion exception; other failures raise a provider error.
   --  @param Channel Enabled typed owner; externally serialize owner operations.
   --  @param ID Command correlation identifier, preserved with all 32 bits.
   --  @param State Requested state, not proof of a completed transition.
   --  @param Mode Requested mode category; provider determines acceptance.
   --  @param Scan_Parameters Complete copied scan payload, defaulting to record defaults.
   --  @return Admitted request observer; terminal result and later metadata are separate.
   function Submit_Operate (Channel : Control_Channel; ID : Command_ID := 0) return Mode_Request;
   --  Convenience ModeCmd submission selecting Operate, task-scheduling mode
   --  and upstream default scan parameters. Requires Enabled and has the same
   --  admission/error/lifetime rules as full mode submission; not a new service.
   --  @param Channel Enabled typed owner; externally serialize owner operations.
   --  @param ID Command correlation identifier; defaults to zero.
   --  @return Mode request observer, not confirmation of physical operation.
   function Is_Open (Request : Mode_Request) return Boolean;
   --  Inspect Mode observer handle presence without polling provider completion.
   --  @param Request Mode observer to inspect.
   --  @return True for a present public handle, pending or terminal.

   type Return_Request is limited private;
   --  Limited controlled observer of a Return future, also used by legacy C2
   --  keep-alive. It is not the canonical common package's Ada request type.
   --  Closing/finalizing does not cancel native work or release a pending permit.
   function Submit_BIT_No_Op
     (Channel : Control_Channel; ID : Command_ID := 0) return Return_Request;
   --  Submit a BIT command with all three payload lists empty while Enabled.
   --  This is not a command to initiate an unspecified test. Admission exhaustion
   --  raises the resource-exhaustion exception; other failures raise a provider error.
   --  @param Channel Enabled typed owner; externally serialize owner operations.
   --  @param ID BIT command correlation identifier, default zero.
   --  @return Return request observer, whose successful payload may still be Fail.
   function Is_Open (Request : Return_Request) return Boolean;
   --  Test Return observer handle presence, not provider completion or success.
   --  @param Request Return observer to inspect.
   --  @return True for a present native observer handle.
   --  These safe operations enforce the upstream one-choice BIT rule. Each
   --  payload-specific operation requires a nonempty owned Ada collection.
   function Submit_BIT_Initiate
     (Channel : Control_Channel; IDs : BIT_ID_Array; ID : Command_ID := 0) return Return_Request
   with Pre => IDs'Length > 0;
   --  Submit only the initiate-ID list while Enabled, leaving cancel and fault
   --  lists empty. The declared nonempty precondition is not independently
   --  checked by the shared body if assertion checking is disabled. Native send
   --  copies IDs; admission exhaustion and other failures follow mode submission.
   --  @param Channel Enabled typed owner; serialize owner operations.
   --  @param IDs Nonempty ordered numeric BIT selectors; duplicate IDs are preserved.
   --  @param ID Command correlation identifier, default zero.
   --  @return Return observer; future completion does not prove BIT execution finished.
   function Submit_BIT_Cancel
     (Channel : Control_Channel; IDs : BIT_ID_Array; ID : Command_ID := 0) return Return_Request
   with Pre => IDs'Length > 0;
   --  Submit only the cancel-ID list while Enabled; initiate and fault lists
   --  remain empty. The declared nonempty precondition has no separate body
   --  check with assertions disabled. IDs are copied before provider send.
   --  @param Channel Enabled typed owner; serialize owner operations.
   --  @param IDs Nonempty ordered numeric BIT selectors to cancel, retaining duplicates.
   --  @param ID Command correlation identifier, default zero.
   --  @return Admitted Return observer, not evidence that cancellation physically completed.
   function Submit_BIT_Clear_Faults
     (Channel : Control_Channel; Fault_Codes : Fault_Code_Vectors.Vector; ID : Command_ID := 0)
      return Return_Request
   with Pre => not Fault_Codes.Is_Empty;
   --  Submit only fault-code text while Enabled, leaving both BIT-ID lists
   --  empty. The declared precondition requires a nonempty vector; the body's
   --  Positive length conversion also fails for empty input. Native code copies
   --  each length-delimited UTF-8 string but rejects embedded NUL bytes.
   --  Invalid UTF-8 or other native failures raise a provider error; Session
   --  admission exhaustion raises the resource-exhaustion exception.
   --  @param Channel Enabled typed owner; serialize owner operations.
   --  @param Fault_Codes Nonempty ordered vector of fault-code strings, retaining duplicates.
   --  @param ID Command correlation identifier, default zero.
   --  @return Return observer; payload success and physical fault clearance are distinct.
   type System_Time_Nanoseconds is range -(2**63) .. (2**63) - 1 with Size => 64;
   --  Signed 64-bit system-time count in nanoseconds. Neither the pinned
   --  ConfigSetCommand nor conversion establishes an epoch or clock domain.
   function Submit_Config_Set
     (Channel        : Control_Channel;
      ID             : Command_ID := 0;
      System_Time_NS : System_Time_Nanoseconds := 0;
      Config         : String := "") return Return_Request;
   --  Submit ConfigSetCommand while Enabled. Native code validates UTF-8 and
   --  copies the complete length-delimited text, rejecting embedded NUL bytes;
   --  it does not parse configuration syntax or establish timestamp semantics.
   --  Session admission exhaustion raises the resource-exhaustion exception;
   --  closed/ineligible channels and other native failures raise a provider error.
   --  @param Channel Enabled typed owner; externally serialize owner operations.
   --  @param ID Command correlation identifier, default zero.
   --  @param System_Time_NS Signed nanosecond count, default zero, with no asserted epoch.
   --  @param Config Opaque UTF-8 configuration text; empty text is permitted.
   --  @return Admitted Return observer, not confirmation that configuration is active.

   type Outcome is (Success, Rejected);
   --  Terminal future result category, not submission status or physical success.
   --  @enum Success A typed payload was returned; a Return payload may still be Fail.
   --  @enum Rejected Explicit upstream ErrorOr rejection, not a provider exception.
   type Error_Code is
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
   type Mode_Result is private;
   --  Owned cached Mode future result containing a mode or explicit rejection.
   --  This is not a command-status metadata event or a physical transition report.
   function Status (Result : Mode_Result) return Outcome;
   --  Read the stored Mode result category without provider access.
   --  @param Result Owned Mode result to inspect.
   --  @return Payload success or explicit rejection.
   function Mode (Result : Mode_Result) return MFA_Mode
   with Pre => Status (Result) = Success;
   --  Read the returned mode under the declared Success precondition. The body
   --  directly returns its field without an independent assertion-disabled check.
   --  @param Result Successful Mode result.
   --  @return Provider-returned mode, not a bridge-verified physical state.
   function Rejection_Code (Result : Mode_Result) return Error_Code
   with Pre => Status (Result) = Rejected;
   --  Read the error code under the declared Rejected precondition; the body
   --  performs no independent outcome check.
   --  @param Result Rejected Mode result.
   --  @return Explicit upstream rejection category.
   function Description (Result : Mode_Result) return String
   with Pre => Status (Result) = Rejected;
   --  Copy rejection text under the declared Rejected precondition. The body
   --  returns stored text without independently checking the outcome.
   --  @param Result Rejected Mode result.
   --  @return Complete owned diagnostic, with long native diagnostics retried from cache.
   type Command_Return is (Return_Success, Bad_Pointer, Fail, Not_Supported, Not_Implemented);
   --  Return payload categories under a successful future result, values 0..4.
   --  @enum Return_Success Upstream Return::Success, value 0.
   --  @enum Bad_Pointer Upstream Return::BadPointer, value 1.
   --  @enum Fail Upstream Return::Fail, value 2; not a bridge/provider exception.
   --  @enum Not_Supported Upstream Return::NotSupported, value 3.
   --  @enum Not_Implemented Upstream Return::NotImplemented, value 4, representing an unimplemented command.
   type Return_Result is private;
   --  Owned terminal Return payload or rejection; payload Fail still has a
   --  successful result category. Command-status callbacks remain independent.
   function Status (Result : Return_Result) return Outcome;
   --  Read the stored Return result category.
   --  @param Result Owned Return result to inspect.
   --  @return Payload success or explicit upstream rejection.
   function Value (Result : Return_Result) return Command_Return
   with Pre => Status (Result) = Success;
   --  Read the payload under the declared Success precondition. There is no
   --  independent outcome check in the expression-function body.
   --  @param Result Successful Return result.
   --  @return Provider Return value; inspect it separately from result status.
   function Rejection_Code (Result : Return_Result) return Error_Code
   with Pre => Status (Result) = Rejected;
   --  Read the rejection code under the declared Rejected precondition; the
   --  body does not independently check the outcome.
   --  @param Result Rejected Return result.
   --  @return Explicit upstream error category.
   function Description (Result : Return_Result) return String
   with Pre => Status (Result) = Rejected;
   --  Copy rejection text under the declared Rejected precondition; the body
   --  simply copies its stored string.
   --  @param Result Rejected Return result.
   --  @return Complete owned rejection diagnostic, not a truncated buffer prefix.

   --  Timeout_Error is inherited from AMS.MEL.IR. Timeout never cancels or
   --  consumes either request type; Wait may be repeated and a later Wait may
   --  return its cached terminal result. Close must not race Wait on the same
   --  Mode_Request or on the same Return_Request.
   function Wait (Request : Mode_Request; Timeout_Milliseconds : Natural) return Mode_Result;
   --  Await the future or return its cached terminal result. Timeout raises the
   --  inherited timeout exception without consumption or cancellation. Other
   --  native/provider failures raise a provider error; explicit rejection is a
   --  result. No guarantee of physical transition completion follows.
   --  @param Request Open Mode observer; must not race Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   --  @return Owned terminal mode or rejection, repeatable from the native cache.
   function Wait (Request : Return_Request; Timeout_Milliseconds : Natural) return Return_Result;
   --  Await a Return payload or explicit rejection, cached for repeated waits.
   --  Timeout raises the inherited timeout exception without cancellation;
   --  other native/provider failures raise a provider error. Success may contain
   --  Fail and does not establish physical completion of a command.
   --  @param Request Open Return observer; must not race Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   --  @return Owned terminal payload or explicit rejection.

   procedure Close (Request : in out Mode_Request);
   --  Idempotently release the public Mode observer. Pending native work retains
   --  its provider graph and admission permit until completion/safe retention;
   --  this call does not cancel it. Native failure raises a provider error.
   --  @param Request Observer to clear, externally serialized against Wait.
   procedure Close (Request : in out Return_Request);
   --  Idempotently close only the Return observer, not pending provider work.
   --  No immediate permit release or physical cancellation is promised.
   --  @param Request Observer to clear; must not race Wait; native failure raises a provider error.
   procedure Close (Channel : in out Control_Channel);
   --  Close public admission and consume the owner on success. Native disable/
   --  detach is deferred while Mode, Return or Comms requests remain pending.
   --  Synchronous detach failure raises a provider error and retains the handle
   --  for retry. Disable failure can raise after successful detach consumed the
   --  handle; an orphaned deferred failure safely retains resources.
   --  Finalization is non-raising. Closing does not cancel provider futures.
   --  @param Channel Owner to close; serialize against all its public operations.

private
   package US renames Ada.Strings.Unbounded;
   type Control_Config is record
      Channel  : UCI_ID;
      Platform : UCI_ID;
      Location : Component_Location;
   end record;
   type Control_Channel is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.C2_Handle := AMS.MEL_C_API.Null_C2;
   end record;
   overriding
   procedure Finalize (Channel : in out Control_Channel);
   type Request_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Mode_Request_Handle := AMS.MEL_C_API.Null_Mode_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Request_Owner);
   type Mode_Request is limited record
      Owner : Request_Owner;
   end record;
   type Return_Request_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Return_Request_Handle := AMS.MEL_C_API.Null_Return_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Return_Request_Owner);
   type Return_Request is limited record
      Owner : Return_Request_Owner;
   end record;
   type Mode_Result is record
      Result_Status : Outcome := Success;
      Result_Mode   : MFA_Mode := Unused;
      Result_Code   : Error_Code := None;
      Result_Text   : US.Unbounded_String;
   end record;
   type Return_Result is record
      Result_Status : Outcome := Success;
      Result_Value  : Command_Return := Return_Success;
      Result_Code   : Error_Code := None;
      Result_Text   : US.Unbounded_String;
   end record;
end AMS.MEL.IR.C2;
