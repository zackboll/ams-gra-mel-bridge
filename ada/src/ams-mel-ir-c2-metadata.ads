private with Ada.Containers.Vectors;
private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with AMS.MEL.IR.Channel;

package AMS.MEL.IR.C2.Metadata is
   --  Owned inbound C2 command/BIT metadata and optional communications reports.
   --  Command status is independent of request Wait: accepting a command is not
   --  physical completion and the bridge supplies no additional correlation
   --  guarantee. All kinds share a bounded drop-incoming queue ordered by
   --  successful enqueue, not timestamps. Serialize stream operations and Close.

   type Metadata_Stream is limited private;
   --  Limited controlled consumer of a native metadata queue. Provider callbacks
   --  retain queue state independently; the stream has only a weak channel link.
   --  Closing cannot unregister callbacks because upstream provides no unregister.
   function Open
     (Channel : Control_Channel; Queue_Capacity : Positive := 16) return Metadata_Stream;
   --  Attempt one-shot registration in order: BIT configuration, command status,
   --  BIT status. No capability-advertisement gate or automatic communications
   --  registration is used here. Native code rejects a closed channel or prior
   --  metadata attempt; unlike sends it does not require Enabled. Partial failure
   --  leaves installed callbacks safely retaining inactive/failed queue state,
   --  marks the attempt consumed and raises a provider error; reopening is not
   --  a registration rollback. No partially open Ada stream escapes.
   --  @param Channel Open typed C2 owner on which registration has not been attempted.
   --  @param Queue_Capacity Positive maximum queued events, default sixteen.
   --  @return Owned consumer for the three base callback families.
   function Is_Open (Stream : Metadata_Stream) return Boolean;
   --  Test local stream-handle presence, not channel existence or queue health.
   --  @param Stream Metadata consumer to inspect.
   --  @return True for a present public handle, including an inactive/failed queue.

   procedure Enable_Comms_Test_Events (Stream : in out Metadata_Stream);
   --  Separately attempt communications-test callback registration once per C2
   --  channel while Attached or Enabled. An expired channel, repeated attempt,
   --  provider refusal or exception raises a provider error. The attempt flag is
   --  set before registration, so failure is not retryable. No capability gate
   --  is applied and no other callback registration is undone. Native code checks
   --  channel lifecycle, not an additional queue-active condition; this operation
   --  does not revive an inactive consumer. A closed stream cannot be used.
   --  @param Stream Open stream whose channel admits this first registration attempt.
   type Metadata_Kind is
     (Command_Status_Event, BIT_Configuration_Event, BIT_Status_Event, Channel_Comms_Test_Event);
   --  Inbound event alternatives sharing the same queue and owned-copy policy.
   --  @enum Command_Status_Event Reported command state and inability reason.
   --  @enum BIT_Configuration_Event Supported BIT definitions, items and component references.
   --  @enum BIT_Status_Event Active/completed BITs and fault reports.
   --  @enum Channel_Comms_Test_Event Communications-test report from separately enabled registration.
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
   type Cannot_Comply is
     (Not_Set,
      Constraint_Attempts,
      Constraint_Endurance,
      Constraint_Classification,
      Constraint_FOR_FOV_Limit,
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
   --  @enum Constraint_FOR_FOV_Limit Field-of-regard or field-of-view limitation, value 4.
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
      Constraint_FOR_FOV_Limit  => 4,
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
   type BIT_Control_Interface is
     (Not_Set, Subsystem_BIT_Command, Subsystem_State_Command, Subsystem_Initiated);
   --  Preserved interface categories, native values 0..3. Pinned upstream names
   --  and adjacent descriptions for values two and three contradict each other;
   --  their interpretation remains unresolved rather than silently swapped.
   --  @enum Not_Set Interface not set, value 0.
   --  @enum Subsystem_BIT_Command Initiated/manual BIT through the BIT-command interface without changing overall state, value 1.
   --  @enum Subsystem_State_Command Source name denotes state-command but adjacent prose describes self-initiation; unresolved, value 2.
   --  @enum Subsystem_Initiated Source name denotes subsystem initiation but adjacent prose describes state-command BIT; unresolved, value 3.
   for BIT_Control_Interface use
     (Not_Set                 => 0,
      Subsystem_BIT_Command   => 1,
      Subsystem_State_Command => 2,
      Subsystem_Initiated     => 3);
   for BIT_Control_Interface'Size use 32;
   type BIT_Result is (Not_Set, Pass, Fail, Interrupted, Not_Tested);
   --  Reported BIT outcome, native values 0..4, not the asynchronous Return payload.
   --  @enum Not_Set Result not set, value 0.
   --  @enum Pass Reported passing test, value 1.
   --  @enum Fail Reported failing test, value 2.
   --  @enum Interrupted Reported interrupted test, value 3.
   --  @enum Not_Tested Reported untested item, value 4.
   for BIT_Result use (Not_Set => 0, Pass => 1, Fail => 2, Interrupted => 3, Not_Tested => 4);
   for BIT_Result'Size use 32;
   type Fault_Severity is (Not_Set, Nominal, Caution, Warning, Failed);
   --  Provider fault severity categories; bridge does not calculate thresholds.
   --  @enum Not_Set Severity not set, value 0.
   --  @enum Nominal Maintenance-directed fault without current mission impact, value 1.
   --  @enum Caution Performance still within characteristics but potentially nonoptimal; no component-state effect, value 2.
   --  @enum Warning Definite problem with component/capability still usable, possibly degraded, value 3.
   --  @enum Failed Component failure that marks it faulted and may preclude capability use, value 4.
   for Fault_Severity use (Not_Set => 0, Nominal => 1, Caution => 2, Warning => 3, Failed => 4);
   for Fault_Severity'Size use 32;
   type Fault_State is (Not_Set, Set, Cleared, Unknown);
   --  Reported fault condition, not a command to modify a fault.
   --  @enum Not_Set Fault state not set, value 0.
   --  @enum Set Fault reported set, value 1.
   --  @enum Cleared Fault reported cleared, value 2.
   --  @enum Unknown Fault condition unknown, value 3.
   for Fault_State use (Not_Set => 0, Set => 1, Cleared => 2, Unknown => 3);
   for Fault_State'Size use 32;

   type Command_Status is private;
   --  Owned reported command identity/state/reason/text, independent of future
   --  results. Accepted status does not prove physical command execution ended.
   function Command_ID (Value : Command_Status) return C2.Command_ID;
   --  Read the provider's command correlation identifier.
   --  @param Value Owned command-status snapshot to inspect.
   --  @return Reported unsigned 32-bit command ID, without additional correlation guarantees.
   function State (Value : Command_Status) return Command_State;
   --  Read the reported command messaging state, not an asynchronous outcome.
   --  @param Value Owned command-status snapshot to inspect.
   --  @return Preserved command-state category.
   function Reason (Value : Command_Status) return Cannot_Comply;
   --  Read the reported inability category; zero represents an unset reason.
   --  @param Value Owned command-status snapshot to inspect.
   --  @return Reported reason, without bridge policy interpretation.
   function Reason_Description (Value : Command_Status) return String;
   --  Copy the provider's human-readable inability description.
   --  @param Value Owned command-status snapshot to inspect.
   --  @return Owned UTF-8 text, possibly empty, not a borrowed provider pointer.

   type BIT_Type is private;
   --  Owned supported-BIT definition with structured UCI identity, interface,
   --  expected duration and ordered item/component collections. Its UCI identity
   --  is not automatically interchangeable with the numeric BIT command selector.
   function BIT_ID (Value : BIT_Type) return UCI_ID;
   --  Copy a supported test's structured identity and label.
   --  @param Value Supported-BIT definition to inspect.
   --  @return UCI identity, not a numeric command BIT selector conversion.
   function Accepted_Interface (Value : BIT_Type) return BIT_Control_Interface;
   --  Read the interface report without resolving upstream's value-two/three
   --  name/prose contradiction or claiming availability of another command API.
   --  @param Value Supported-BIT definition to inspect.
   --  @return Preserved interface category.
   function Expected_Duration_NS (Value : BIT_Type) return Long_Long_Integer;
   --  Read expected test duration in signed nanoseconds. Upstream permits an
   --  unknown/optional duration; the scalar representation adds no presence flag.
   --  @param Value Supported-BIT definition to inspect.
   --  @return Copied duration count without normalization.
   function BIT_Item_Name_Count (Value : BIT_Type) return Natural;
   --  Count copied item names in provider vector order, retaining duplicates.
   --  @param Value Supported-BIT definition to inspect.
   --  @return Number of names available at positive one-based positions.
   function BIT_Item_Name_At (Value : BIT_Type; Index : Positive) return String;
   --  Copy an item name; a position beyond vector length raises a constraint error.
   --  @param Value Supported-BIT definition to inspect.
   --  @param Index One-based item-name position, not an item identifier.
   --  @return Owned UTF-8 item name at that position.
   function Subsystem_Component_Count (Value : BIT_Type) return Natural;
   --  Count copied subsystem component references in provider order.
   --  @param Value Supported-BIT definition to inspect.
   --  @return Number of component UCI identities, retaining duplicates.
   function Subsystem_Component_At (Value : BIT_Type; Index : Positive) return UCI_ID;
   --  Copy a component identity; a position beyond length raises a constraint error.
   --  @param Value Supported-BIT definition to inspect.
   --  @param Index One-based component position, not a component ID.
   --  @return Owned component UCI identity and descriptive label.

   type Active_BIT is private;
   --  Owned active-test report with UCI identity, estimated time and completion
   --  fraction. Provider estimates are preserved, not calculated by the bridge.
   function BIT_ID (Value : Active_BIT) return UCI_ID;
   --  Copy the active test's structured identity and label.
   --  @param Value Active-test report to inspect.
   --  @return Owned UCI identity, not an inferred numeric command selector.
   function Estimated_Completion_Time_NS (Value : Active_BIT) return Long_Long_Integer;
   --  Read the estimated completion time, described upstream as nanoseconds
   --  UTC with optional availability; this scalar adds no presence discriminator.
   --  @param Value Active-test report to inspect.
   --  @return Signed nanosecond count, without inventing a more specific epoch.
   function Estimated_Percent_Complete (Value : Active_BIT) return Long_Float;
   --  Read the provider's completion estimate: upstream defines 1.0 as 100%
   --  and permits values greater than 100%; no clamp is performed.
   --  @param Value Active-test report to inspect.
   --  @return Copied completion fraction, not a bridge-derived progress estimate.
   type Completed_BIT_Item is private;
   --  Owned completed item name, reported BIT outcome and failure reason text.
   function BIT_Item_Name (Value : Completed_BIT_Item) return String;
   --  Copy the completed item's name, not a generated command selector.
   --  @param Value Completed-item snapshot to inspect.
   --  @return Owned UTF-8 name.
   function Result (Value : Completed_BIT_Item) return BIT_Result;
   --  Read the reported item outcome, not an asynchronous request outcome.
   --  @param Value Completed-item snapshot to inspect.
   --  @return Preserved BIT result category.
   function Fail_Reason (Value : Completed_BIT_Item) return String;
   --  Copy the provider's item failure reason, possibly empty.
   --  @param Value Completed-item snapshot to inspect.
   --  @return Owned UTF-8 explanation without bridge interpretation.
   type Completed_BIT is private;
   --  Owned completed-test report with UCI identity, end timestamp, outcome,
   --  failure text and ordered item results.
   function BIT_ID (Value : Completed_BIT) return UCI_ID;
   --  Copy the completed test's structured identity.
   --  @param Value Completed-test snapshot to inspect.
   --  @return Owned UCI identity and label.
   function Time_Tag_NS (Value : Completed_BIT) return Long_Long_Integer;
   --  Read when the BIT ended, described upstream as nanoseconds UTC.
   --  @param Value Completed-test snapshot to inspect.
   --  @return Signed nanosecond timestamp, without a more specific epoch claim.
   function Result (Value : Completed_BIT) return BIT_Result;
   --  Read the completed test outcome, not a command's Return payload.
   --  @param Value Completed-test snapshot to inspect.
   --  @return Provider-reported BIT result category.
   function Fail_Reason (Value : Completed_BIT) return String;
   --  Copy the provider's test failure explanation, possibly empty.
   --  @param Value Completed-test snapshot to inspect.
   --  @return Owned UTF-8 explanation.
   function BIT_Item_Count (Value : Completed_BIT) return Natural;
   --  Count completed item reports in copied provider order, retaining duplicates.
   --  @param Value Completed-test snapshot to inspect.
   --  @return Number of item reports available by positive one-based position.
   function BIT_Item_At (Value : Completed_BIT; Index : Positive) return Completed_BIT_Item;
   --  Copy one item result; an index beyond length raises a constraint error.
   --  @param Value Completed-test snapshot to inspect.
   --  @param Index One-based item-report position, not an item identifier.
   --  @return Independent completed-item snapshot.

   type Fault_Data is private;
   --  Owned fault-associated key/value/format/units text. The bridge preserves
   --  descriptions without parsing values or inferring physical units.
   function Key (Value : Fault_Data) return String;
   --  Copy the associated-data key.
   --  @param Value Owned fault data to inspect.
   --  @return Provider-supplied key text.
   function Data_Value (Value : Fault_Data) return String;
   --  Copy the associated-data value without numeric interpretation.
   --  @param Value Owned fault data to inspect.
   --  @return Provider-supplied value text.
   function Format (Value : Fault_Data) return String;
   --  Copy the provider's format description, not a bridge parsing directive.
   --  @param Value Owned fault data to inspect.
   --  @return Format text preserved from the provider.
   function Units (Value : Fault_Data) return String;
   --  Copy the associated-data units description without unit conversion.
   --  @param Value Owned fault data to inspect.
   --  @return Provider-supplied units text.
   type Fault_Ambiguity_Group is private;
   --  Owned ordered diagnostic-test and component UCI references for a fault
   --  ambiguity group; the bridge performs no diagnosis or group resolution.
   function Diagnostic_Test_Count (Value : Fault_Ambiguity_Group) return Natural;
   --  Count diagnostic-test references in stored provider order.
   --  @param Value Owned ambiguity group to inspect.
   --  @return Number of test UCI identities, retaining duplicates.
   function Diagnostic_Test_At (Value : Fault_Ambiguity_Group; Index : Positive) return UCI_ID;
   --  Copy a diagnostic identity; index beyond length raises a constraint error.
   --  @param Value Owned ambiguity group to inspect.
   --  @param Index One-based diagnostic-test position, not a test ID.
   --  @return Owned diagnostic-test UCI identity and label.
   function Component_Count (Value : Fault_Ambiguity_Group) return Natural;
   --  Count component references in the ambiguity group's stored order.
   --  @param Value Owned ambiguity group to inspect.
   --  @return Number of component identities, retaining duplicates.
   function Component_At (Value : Fault_Ambiguity_Group; Index : Positive) return UCI_ID;
   --  Copy a component identity; index beyond length raises a constraint error.
   --  @param Value Owned ambiguity group to inspect.
   --  @param Index One-based component position, not a UCI identifier.
   --  @return Independent component identity and label.
   type Fault is private;
   --  Owned fault report with severity/state, detection time, text, associated
   --  data, components and ambiguity groups, preserving provider vector order.
   function Fault_ID (Value : Fault) return UCI_ID;
   --  Copy the fault's structured UCI identity and label.
   --  @param Value Owned fault report to inspect.
   --  @return Independent fault identity.
   function Severity (Value : Fault) return Fault_Severity;
   --  Read reported severity without calculating fault thresholds.
   --  @param Value Owned fault report to inspect.
   --  @return Preserved severity category.
   function State (Value : Fault) return Fault_State;
   --  Read the reported fault condition, not a state-changing operation.
   --  @param Value Owned fault report to inspect.
   --  @return Preserved set/cleared/unknown/unset category.
   function Detection_Time_NS (Value : Fault) return Long_Long_Integer;
   --  Read the detection-time nanosecond count; no clock epoch is inferred
   --  from the underlying chrono duration representation.
   --  @param Value Owned fault report to inspect.
   --  @return Signed nanosecond count preserved from the provider.
   function Fault_Code (Value : Fault) return String;
   --  Copy the fault code/name without equating it with the fault UCI identity.
   --  @param Value Owned fault report to inspect.
   --  @return Owned provider fault-code text.
   function Fault_Description (Value : Fault) return String;
   --  Copy the human-readable fault description.
   --  @param Value Owned fault report to inspect.
   --  @return Owned provider description text.
   function Fault_Data_Count (Value : Fault) return Natural;
   --  Count associated-data entries in copied provider order.
   --  @param Value Owned fault report to inspect.
   --  @return Number of data entries, retaining duplicates.
   function Fault_Data_At (Value : Fault; Index : Positive) return Fault_Data;
   --  Copy associated data; an index beyond length raises a constraint error.
   --  @param Value Owned fault report to inspect.
   --  @param Index One-based data-entry position.
   --  @return Independent key/value/format/units snapshot.
   function Component_Count (Value : Fault) return Natural;
   --  Count associated component identities in copied provider order.
   --  @param Value Owned fault report to inspect.
   --  @return Number of component references, retaining duplicates.
   function Component_At (Value : Fault; Index : Positive) return UCI_ID;
   --  Copy a component identity; index beyond length raises a constraint error.
   --  @param Value Owned fault report to inspect.
   --  @param Index One-based associated-component position.
   --  @return Owned component UCI identity and label.
   function Ambiguity_Group_Count (Value : Fault) return Natural;
   --  Count copied ambiguity groups without resolving their diagnostic meaning.
   --  @param Value Owned fault report to inspect.
   --  @return Number of groups in provider order.
   function Ambiguity_Group_At (Value : Fault; Index : Positive) return Fault_Ambiguity_Group;
   --  Copy one group; an index beyond length raises a constraint error.
   --  @param Value Owned fault report to inspect.
   --  @param Index One-based ambiguity-group position.
   --  @return Independent diagnostic-test/component reference group.

   type Metadata_Event is private;
   --  Wholly Ada-owned event snapshot. Kind selects the valid accessor family;
   --  this private representation is not a public discriminated record. Native
   --  event storage is disposed even if Ada copying raises; no borrowed provider
   --  references survive in returned values.
   function Receive
     (Stream : Metadata_Stream; Timeout_Milliseconds : Natural := 0) return Metadata_Event;
   --  Pop the oldest successfully enqueued snapshot. Already queued events
   --  drain before stopped/failed state is reported. Empty active queue timeout
   --  raises the inherited timeout exception; stopped/inactive queue raises
   --  the inherited stopped-stream exception; native/callback terminal failure raises a provider error.
   --  Conversion validates enums and releases native event storage on exceptions.
   --  @param Stream Open queue consumer, externally serialized against Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; default zero polls.
   --  @return Owned event of any enabled kind, independent of request Wait results.
   function Kind (Event : Metadata_Event) return Metadata_Kind;
   --  Read the snapshot's event selector without provider access.
   --  @param Event Owned event to inspect.
   --  @return Accessor family applicable to this event.
   function Command (Event : Metadata_Event) return Command_Status
   with Pre => Kind (Event) = Command_Status_Event;
   --  Copy command status under the declared command-event precondition. The
   --  body returns its stored field without an independent wrong-kind check.
   --  @param Event Command-status event, not a request-completion result.
   --  @return Owned reported command state, reason and diagnostic.
   function BIT_Type_Count (Event : Metadata_Event) return Natural
   with Pre => Kind (Event) = BIT_Configuration_Event;
   --  Count supported tests under the declared configuration-event precondition;
   --  the body does not independently check kind.
   --  @param Event BIT-configuration event to inspect.
   --  @return Number of supported-test definitions in provider order.
   function BIT_Type_At (Event : Metadata_Event; Index : Positive) return BIT_Type
   with Pre => Kind (Event) = BIT_Configuration_Event;
   --  Copy a supported test under the declared configuration-event precondition.
   --  The body adds no kind check; index beyond length raises a constraint error.
   --  @param Event BIT-configuration event to inspect.
   --  @param Index One-based supported-test position, not a BIT ID.
   --  @return Owned supported-test definition.
   function Active_BIT_Count (Event : Metadata_Event) return Natural
   with Pre => Kind (Event) = BIT_Status_Event;
   --  Count active tests under the declared BIT-status precondition; no separate
   --  body kind check is performed.
   --  @param Event BIT-status event to inspect.
   --  @return Number of active reports in provider order.
   function Active_BIT_At (Event : Metadata_Event; Index : Positive) return Active_BIT
   with Pre => Kind (Event) = BIT_Status_Event;
   --  Copy an active test under the declared BIT-status precondition. The body
   --  adds no kind check; index beyond length raises a constraint error.
   --  @param Event BIT-status event to inspect.
   --  @param Index One-based active-test position, retaining provider order.
   --  @return Owned active-test report.
   function Completed_BIT_Count (Event : Metadata_Event) return Natural
   with Pre => Kind (Event) = BIT_Status_Event;
   --  Count completed tests under the declared BIT-status precondition; the body
   --  returns stored length without independently checking kind.
   --  @param Event BIT-status event to inspect.
   --  @return Number of completed-test reports in provider order.
   function Completed_BIT_At (Event : Metadata_Event; Index : Positive) return Completed_BIT
   with Pre => Kind (Event) = BIT_Status_Event;
   --  Copy a completed test under the declared BIT-status precondition. The body
   --  adds no kind check; index beyond length raises a constraint error.
   --  @param Event BIT-status event to inspect.
   --  @param Index One-based completed-test position.
   --  @return Independent completed-test report and nested item values.
   function Fault_Count (Event : Metadata_Event) return Natural
   with Pre => Kind (Event) = BIT_Status_Event;
   --  Count fault reports under the declared BIT-status precondition; no separate
   --  body kind check is performed.
   --  @param Event BIT-status event to inspect.
   --  @return Number of copied faults, retaining provider order and duplicates.
   function Fault_At (Event : Metadata_Event; Index : Positive) return Fault
   with Pre => Kind (Event) = BIT_Status_Event;
   --  Copy a fault under the declared BIT-status precondition. The body does
   --  not check kind; an index beyond length raises a constraint error.
   --  @param Event BIT-status event to inspect.
   --  @param Index One-based fault-report position, not a fault identifier.
   --  @return Owned fault and all nested associated-data/reference collections.
   function Comms_Test (Event : Metadata_Event) return IR.Channel.Comms_Test_Report
   with Pre => Kind (Event) = Channel_Comms_Test_Event;
   --  Copy a separately registered communications report under its declared
   --  event-kind precondition. No independent body kind check is performed.
   --  @param Event Communications-test metadata event, not a Comms future result.
   --  @return Owned shared Channel report containing command/request identifiers.

   type Metadata_Counters is record
      Events_Received, Events_Dropped_Queue_Full, Malformed_Or_Unsupported : Counter;
   end record;
   --  Native unsigned counters saturate at their maximum and do not wrap. A
   --  callback increments received before checking whether the queue is active.
   --  @field Events_Received Callback entries observed, including entries not ultimately enqueued.
   --  @field Events_Dropped_Queue_Full Valid converted incoming events dropped because the bounded queue was full.
   --  @field Malformed_Or_Unsupported Null/malformed/unsupported converted events dropped non-terminally; callback exceptions instead fail the stream.
   function Counters (Stream : Metadata_Stream) return Metadata_Counters;
   --  Read a synchronized counter snapshot; a closed handle or native failure
   --  raises a provider error. Counts need not sum to received when inactive
   --  callbacks or terminal conversion failures occurred.
   --  @param Stream Open metadata queue consumer.
   --  @return Owned saturating counter values observed at query time.
   procedure Close (Stream : in out Metadata_Stream);
   --  Idempotently consume the public handle, mark an active queue inactive and
   --  wake waiters. This does not unregister callbacks, close C2, undo one-shot
   --  attempts or promise callback quiescence. Captured native state survives
   --  as needed; finalization closes without raising. Native failure raises a
   --  provider error. Serialize against all operations on this same stream.
   --  @param Stream Metadata consumer to close, leaving copied events valid.

private
   package US renames Ada.Strings.Unbounded;
   use type US.Unbounded_String;
   package String_Vectors is new Ada.Containers.Vectors (Positive, US.Unbounded_String);
   package ID_Vectors is new Ada.Containers.Vectors (Positive, UCI_ID);
   type Command_Status is record
      ID          : C2.Command_ID := 0;
      Status      : Command_State := Not_Set;
      Why         : Cannot_Comply := Not_Set;
      Description : US.Unbounded_String;
   end record;
   type BIT_Type is record
      ID              : UCI_ID;
      Interface_Value : BIT_Control_Interface := Not_Set;
      Duration        : Long_Long_Integer := 0;
      Names           : String_Vectors.Vector;
      Components      : ID_Vectors.Vector;
   end record;
   package BIT_Type_Vectors is new Ada.Containers.Vectors (Positive, BIT_Type);
   type Active_BIT is record
      ID         : UCI_ID;
      Completion : Long_Long_Integer := 0;
      Percent    : Long_Float := 0.0;
   end record;
   package Active_Vectors is new Ada.Containers.Vectors (Positive, Active_BIT);
   type Completed_BIT_Item is record
      Name, Why : US.Unbounded_String;
      Value     : BIT_Result := Not_Set;
   end record;
   package Item_Vectors is new Ada.Containers.Vectors (Positive, Completed_BIT_Item);
   type Completed_BIT is record
      ID    : UCI_ID;
      Time  : Long_Long_Integer := 0;
      Value : BIT_Result := Not_Set;
      Why   : US.Unbounded_String;
      Items : Item_Vectors.Vector;
   end record;
   package Completed_Vectors is new Ada.Containers.Vectors (Positive, Completed_BIT);
   type Fault_Data is record
      K, V, F, U : US.Unbounded_String;
   end record;
   package Data_Vectors is new Ada.Containers.Vectors (Positive, Fault_Data);
   type Fault_Ambiguity_Group is record
      Tests, Components : ID_Vectors.Vector;
   end record;
   package Group_Vectors is new Ada.Containers.Vectors (Positive, Fault_Ambiguity_Group);
   type Fault is record
      ID                : UCI_ID;
      Severity_Value    : Fault_Severity := Not_Set;
      State_Value       : Fault_State := Not_Set;
      Time              : Long_Long_Integer := 0;
      Code, Description : US.Unbounded_String;
      Data              : Data_Vectors.Vector;
      Components        : ID_Vectors.Vector;
      Groups            : Group_Vectors.Vector;
   end record;
   package Fault_Vectors is new Ada.Containers.Vectors (Positive, Fault);
   type Metadata_Event is record
      Event_Kind : Metadata_Kind := Command_Status_Event;
      Status     : Command_Status;
      BIT_Types  : BIT_Type_Vectors.Vector;
      Active     : Active_Vectors.Vector;
      Completed  : Completed_Vectors.Vector;
      Faults     : Fault_Vectors.Vector;
      Comms      : IR.Channel.Comms_Test_Report := (0, 0);
   end record;
   type Metadata_Stream is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Metadata_Handle := AMS.MEL_C_API.Null_Metadata;
   end record;
   overriding
   procedure Finalize (Stream : in out Metadata_Stream);
end AMS.MEL.IR.C2.Metadata;
