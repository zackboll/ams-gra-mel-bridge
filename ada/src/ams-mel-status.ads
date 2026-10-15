with Ada.Containers.Vectors;
private with Ada.Strings.Unbounded;
with AMS.MEL.IR;

package AMS.MEL.Status is
   --  Ada-owned Common MEL status values for built-in tests, faults, component
   --  health and security audit data. Constructors copy their inputs; they do
   --  not query a provider or enforce a provider's state machine. Returned text
   --  and nested values are copies, not borrowed native storage. Collection
   --  access uses Positive indices starting at one; an index beyond the stored
   --  length fails an Ada constraint check. Appending preserves order and duplicates.

   use type AMS.MEL.IR.UCI_ID;
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
   --  Overall MFA/subsystem operating state, distinct from an individual
   --  component's capability state and from a transition-in-progress report.
   --  Not every provider supports every state. This enum reports or requests
   --  a state; storing a value here does not execute a state transition.
   --  @enum Not_Set No overall state has been set.
   --  @enum Unknown The subsystem's overall state is unknown.
   --  @enum Not_Installed The subsystem is not installed.
   --  @enum Off The subsystem is not powered up.
   --  @enum Pre_Initialization Bootstrap and initial operational-software load.
   --  @enum Initialization Initialization and mission-data load are in progress.
   --  @enum Standby Initialization completed; ready to transition to operation.
   --  @enum Operate Enabled capabilities may be performed.
   --  @enum Operate_Rx_Only Enabled receive-only capabilities may be performed.
   --  @enum Operate_Tx_Only Pinned transmit-only operating-state identifier.
   --  Its presence does not establish support by an IR or RF provider.
   --  @enum Maintenance Maintenance procedures are being or have been performed.
   --  @enum Calibration State-command calibration that interrupts other functions,
   --  distinct from calibration interleaved through a BIT command.
   --  @enum Initiated_BIT State-command built-in testing that interrupts other
   --  functions, distinct from BIT that does not require a state change.
   --  @enum Shutdown Shutdown procedure is being or has been performed; upstream
   --  states that the subsystem sends no further messages after completion.
   --  @enum Degraded The subsystem cannot perform all normal activities.

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

   type BIT_Control_Interface is
     (Not_Set, Subsystem_BIT_Command, Subsystem_State_Command, Subsystem_Initiated);
   --  Provider-reported BIT control-interface identifier, preserving the pinned
   --  numeric mapping. Values two and three have contradictory upstream prose;
   --  they must not be used to infer whether a BIT changes subsystem state.
   --  @enum Not_Set No control-interface value has been set.
   --  @enum Subsystem_BIT_Command Interface value one: manual/initiated BIT via
   --  a BIT command, described upstream as interleavable with operational work.
   --  @enum Subsystem_State_Command Interface value two, named state command.
   --  Pinned prose instead describes self-initiated BIT; behavior is unresolved.
   --  @enum Subsystem_Initiated Interface value three, named subsystem initiated.
   --  Pinned prose instead describes a state-command transition to initiated
   --  BIT; the bridge preserves the value without resolving that ambiguity.
   for BIT_Control_Interface use
     (Not_Set                 => 0,
      Subsystem_BIT_Command   => 1,
      Subsystem_State_Command => 2,
      Subsystem_Initiated     => 3);
   for BIT_Control_Interface'Size use 32;

   type BIT_Result is (Not_Set, Pass, Fail, Interrupted, Not_Tested);
   --  Completion result for a whole built-in test or an individual tested item.
   --  A result is status data, not a bridge submission/admission outcome.
   --  @enum Not_Set No test-result value has been set.
   --  @enum Pass The test completed successfully.
   --  @enum Fail The test failed; explanatory failure text may be provided.
   --  @enum Interrupted The test was interrupted and did not complete.
   --  @enum Not_Tested The test was not executed.
   for BIT_Result use (Not_Set => 0, Pass => 1, Fail => 2, Interrupted => 3, Not_Tested => 4);
   for BIT_Result'Size use 32;

   type Fault_Severity is (Not_Set, Nominal, Caution, Warning, Failed);
   --  Significance of a reported fault, independent of whether it is currently
   --  set or cleared. These meanings describe upstream reporting conventions;
   --  constructing a value does not change any component's state.
   --  @enum Not_Set No severity has been set.
   --  @enum Nominal Maintenance-relevant condition with no current mission
   --  impact, such as a service interval being reached.
   --  @enum Caution Condition worth operator attention that may worsen; the
   --  system still meets performance characteristics and component state is
   --  not affected by this severity alone.
   --  @enum Warning Definite problem with a still-usable component/capability;
   --  associated component state is degraded and performance may fall short.
   --  @enum Failed Component failure that may preclude capability use; upstream
   --  associates this severity with a faulted component state.
   for Fault_Severity use (Not_Set => 0, Nominal => 1, Caution => 2, Warning => 3, Failed => 4);
   for Fault_Severity'Size use 32;

   type Fault_State is (Not_Set, Set, Cleared, Unknown);
   --  Lifecycle state of a fault report, not its severity or a component state.
   --  @enum Not_Set No fault-state value has been set.
   --  @enum Set The fault has occurred.
   --  @enum Cleared The fault has been cleared.
   --  @enum Unknown The fault's state is unknown.
   for Fault_State use (Not_Set => 0, Set => 1, Cleared => 2, Unknown => 3);
   for Fault_State'Size use 32;

   type BIT_Type is private;
   --  Description of one supported built-in test: its identifier, accepted
   --  control interface, optional item/component lists and expected duration.

   type Active_BIT is private;
   --  Progress snapshot for an active built-in test. A default value has a
   --  default identifier, zero completion-time count and zero progress estimate.

   type Completed_BIT_Item is private;
   --  Result and optional failure explanation for a named logical or physical
   --  item within a completed built-in test.

   type Completed_BIT is private;
   --  Completed built-in test snapshot with overall result, end-time count and
   --  an ordered collection of per-item results.

   type Fault_Data is private;
   --  Data supporting fault isolation: a textual key/value with explicit format
   --  and units descriptions. The Ada value does not parse or convert the text.

   type Fault_Ambiguity_Group is private;
   --  Components to which a fault has been narrowed, but not fully isolated,
   --  together with diagnostic-test references for that group.

   type Fault is private;
   --  Fault report with independent severity/state, detection time, explanatory
   --  data, individually implicated components and unresolved ambiguity groups.

   function BIT_ID (Value : BIT_Type) return IR.UCI_ID;
   --  Read the supported test identifier used to initiate and report that BIT.
   --  @param Value Supported-test description.
   --  @return Copied BIT identifier, including its descriptive label.

   function Accepted_Interface (Value : BIT_Type) return BIT_Control_Interface;
   --  Read the single reported interface accepted by this built-in test.
   --  @param Value Supported-test description.
   --  @return Preserved control-interface identifier; see its enum ambiguity.

   function Expected_Duration_NS (Value : BIT_Type) return Long_Long_Integer;
   --  Read the expected test duration in nanoseconds, when reported.
   --  @param Value Supported-test description.
   --  @return Signed duration count, unmodified; the upstream default is zero
   --  and no separate presence indicator is exposed.

   function BIT_Item_Name_Count (Value : BIT_Type) return Natural;
   --  Count named non-component items tested by this BIT.
   --  @param Value Supported-test description.
   --  @return Number of stored item names; zero denotes an empty collection.

   function BIT_Item_Name_At (Value : BIT_Type; Index : Positive) return String;
   --  Copy one logical or physical tested-item name in stored order.
   --  @param Value Supported-test description owning the item names.
   --  @param Index One-based position, no greater than the item-name count.
   --  @return Independent Ada String for the selected name.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   function Subsystem_Component_Count (Value : BIT_Type) return Natural;
   --  Count components that this BIT tests at least partially.
   --  @param Value Supported-test description.
   --  @return Stored component-reference count, including duplicates.

   function Subsystem_Component_At (Value : BIT_Type; Index : Positive) return IR.UCI_ID;
   --  Copy a reference to a component tested at least partially by this BIT.
   --  @param Value Supported-test description owning the component references.
   --  @param Index One-based position within the stored component count.
   --  @return Component identifier at that position, with its label preserved.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   function BIT_ID (Value : Active_BIT) return IR.UCI_ID;
   --  Read the identifier of the test whose progress is being reported.
   --  @param Value Active-test progress snapshot.
   --  @return Copied identifier of the active BIT.

   function Estimated_Completion_Time_NS (Value : Active_BIT) return Long_Long_Integer;
   --  Read the estimated completion time, described upstream as nanoseconds UTC.
   --  The pinned declaration supplies no epoch or clock-conversion contract.
   --  @param Value Active-test progress snapshot.
   --  @return Signed time count, without epoch inference or conversion; zero is
   --  the upstream default and there is no separate optional-presence flag.

   function Estimated_Percent_Complete (Value : Active_BIT) return Long_Float;
   --  Read estimated progress as a fraction: 1.0 represents 100 percent.
   --  @param Value Active-test progress snapshot.
   --  @return Stored estimate, not a 0-to-100 number. Upstream permits values
   --  above 1.0; the Ada constructor does not clamp or range-check progress.

   function BIT_Item_Name (Value : Completed_BIT_Item) return String;
   --  Copy the name of a logical or physical item tested by the completed BIT.
   --  @param Value Per-item test-result snapshot.
   --  @return Independent Ada String containing the tested-item name.

   function Result (Value : Completed_BIT_Item) return BIT_Result;
   --  Read the test outcome for one item, not the overall BIT summary.
   --  @param Value Per-item test-result snapshot.
   --  @return Stored item result, including an unset or not-tested value.

   function Fail_Reason (Value : Completed_BIT_Item) return String;
   --  Copy the human-readable reason for this item's failure.
   --  @param Value Per-item test-result snapshot.
   --  @return Stored explanation, possibly empty. Upstream expects this text
   --  for a failed result; Ada does not enforce a result/text dependency.

   function BIT_ID (Value : Completed_BIT) return IR.UCI_ID;
   --  Read the identifier of the completed built-in test.
   --  @param Value Completed-test snapshot.
   --  @return Copied identifier of the completed BIT.

   function Time_Tag_NS (Value : Completed_BIT) return Long_Long_Integer;
   --  Read when the BIT ended, described upstream as nanoseconds UTC.
   --  @param Value Completed-test snapshot.
   --  @return Signed completion-time count, without epoch inference or clock
   --  conversion; the pinned declaration does not establish an epoch.

   function Result (Value : Completed_BIT) return BIT_Result;
   --  Read the overall BIT summary result, distinct from each item's result.
   --  @param Value Completed-test snapshot.
   --  @return Stored overall result; no aggregation is recomputed by Ada.

   function Fail_Reason (Value : Completed_BIT) return String;
   --  Copy the human-readable explanation of overall BIT failure.
   --  @param Value Completed-test snapshot.
   --  @return Stored explanation, possibly empty, without enforcing that the
   --  overall result indicates failure.

   function BIT_Item_Count (Value : Completed_BIT) return Natural;
   --  Count the per-item results included in this completed test.
   --  @param Value Completed-test snapshot.
   --  @return Stored item-result count, including any duplicate item names.

   function BIT_Item_At (Value : Completed_BIT; Index : Positive) return Completed_BIT_Item;
   --  Copy one per-item result from the completed test's ordered collection.
   --  @param Value Completed-test snapshot owning the item results.
   --  @param Index One-based position within the stored item count.
   --  @return Ada-owned item-result value independent of the source snapshot.
   --  @exception Constraint_Error Index exceeds the stored collection length.
   function Key (Value : Fault_Data) return String;
   --  Copy the key naming a datum used to explain or isolate a fault.
   --  @param Value Fault-isolation datum.
   --  @return Independent text naming the datum, without interpretation.

   function Data_Value (Value : Fault_Data) return String;
   --  Copy the textual value of a fault-isolation datum without parsing it.
   --  @param Value Fault-isolation datum with associated format and units.
   --  @return Independent Ada String preserving the stored data text.

   function Format (Value : Fault_Data) return String;
   --  Copy the description of how the fault datum's value is represented.
   --  @param Value Fault-isolation datum.
   --  @return Stored format text; no universal format vocabulary is imposed.

   function Units (Value : Fault_Data) return String;
   --  Copy the units reported for the fault datum's textual value.
   --  @param Value Fault-isolation datum.
   --  @return Stored units text; Ada performs no unit conversion.

   function Diagnostic_Test_Count (Value : Fault_Ambiguity_Group) return Natural;
   --  Count tests referenced for further isolation among ambiguous components.
   --  @param Value Unresolved fault-isolation group.
   --  @return Number of stored test references, including duplicates.

   function Diagnostic_Test_At (Value : Fault_Ambiguity_Group; Index : Positive) return IR.UCI_ID;
   --  Copy a diagnostic-test reference from an ambiguity group. Upstream implies
   --  no positional relationship to the group's separate component list.
   --  @param Value Group owning the diagnostic-test references.
   --  @param Index One-based position within the stored diagnostic-test count.
   --  @return Copied identifier of a test that may further isolate the fault.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   function Component_Count (Value : Fault_Ambiguity_Group) return Natural;
   --  Count components that may be related to this unresolved fault group.
   --  @param Value Ambiguity group, not a list of individually isolated faults.
   --  @return Number of stored candidate-component references.

   function Component_At (Value : Fault_Ambiguity_Group; Index : Positive) return IR.UCI_ID;
   --  Copy one candidate-component reference from an ambiguity group.
   --  @param Value Group owning the unresolved component references.
   --  @param Index One-based position within the stored component count.
   --  @return Copied candidate identifier; not paired by index with a test ID.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   function Fault_ID (Value : Fault) return IR.UCI_ID;
   --  Read the unique identifier assigned to this reported fault.
   --  @param Value Fault-report snapshot.
   --  @return Copied fault identifier with its descriptive label preserved.

   function Severity (Value : Fault) return Fault_Severity;
   --  Read the reported impact level, separately from the fault lifecycle state.
   --  @param Value Fault-report snapshot.
   --  @return Stored severity; no component state is recomputed or changed.

   function State (Value : Fault) return Fault_State;
   --  Read whether a fault is set, cleared, unknown or not yet specified.
   --  @param Value Fault-report snapshot.
   --  @return Stored fault lifecycle state, not the severity or MFA state.

   function Detection_Time_NS (Value : Fault) return Long_Long_Integer;
   --  Read the time of fault detection, not transmission of its report.
   --  @param Value Fault-report snapshot.
   --  @return Signed nanosecond count described upstream as UTC; the pinned
   --  declaration establishes no epoch and Ada performs no clock conversion.

   function Fault_Code (Value : Fault) return String;
   --  Copy the reported code or name of the fault.
   --  @param Value Fault-report snapshot.
   --  @return Independent fault-code text; no bridge code vocabulary is imposed.

   function Fault_Description (Value : Fault) return String;
   --  Copy the human-readable fault or cause description, which may differ from
   --  the descriptive label carried by the fault identifier.
   --  @param Value Fault-report snapshot.
   --  @return Independent Ada String containing the stored explanation.

   function Fault_Data_Count (Value : Fault) return Natural;
   --  Count additional data items used for fault isolation and false-alarm analysis.
   --  @param Value Fault-report snapshot.
   --  @return Stored data-item count; zero means no data items were included.

   function Fault_Data_At (Value : Fault; Index : Positive) return Fault_Data;
   --  Copy one fault-isolation datum, retaining its format and units text.
   --  @param Value Fault report owning the ordered additional-data collection.
   --  @param Index One-based position within the stored data-item count.
   --  @return Ada-owned copy of the selected key/value/format/units datum.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   function Component_Count (Value : Fault) return Natural;
   --  Count components individually determined to be related to the fault.
   --  @param Value Fault report, whose unresolved candidates are in separate groups.
   --  @return Stored individually implicated component-reference count.

   function Component_At (Value : Fault; Index : Positive) return IR.UCI_ID;
   --  Copy an individually implicated component reference, not an unresolved
   --  candidate from an ambiguity group.
   --  @param Value Fault report owning the component references.
   --  @param Index One-based position within the stored component count.
   --  @return Copied related-component identifier in stored order.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   function Ambiguity_Group_Count (Value : Fault) return Natural;
   --  Count unresolved candidate-component groups for this fault.
   --  @param Value Fault-report snapshot.
   --  @return Stored group count; separate groups can associate different tests
   --  with different subsets of candidate components.

   function Ambiguity_Group_At (Value : Fault; Index : Positive) return Fault_Ambiguity_Group;
   --  Copy one unresolved component/test group from the fault report.
   --  @param Value Fault report owning the ordered ambiguity groups.
   --  @param Index One-based position within the stored group count.
   --  @return Ada-owned group with independent component and test collections.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   type BIT_Type_List is private;
   --  Initially empty, ordered Ada-owned collection of supported-test values.

   type Active_BIT_List is private;
   --  Initially empty, ordered Ada-owned collection of active-test snapshots.

   type Completed_BIT_Item_List is private;
   --  Initially empty, ordered Ada-owned collection of per-item test results.

   type Completed_BIT_List is private;
   --  Initially empty, ordered Ada-owned collection of completed-test snapshots.

   type Fault_Data_List is private;
   --  Initially empty, ordered Ada-owned collection of fault-isolation data.

   package UCI_ID_Vectors is new Ada.Containers.Vectors (Positive, IR.UCI_ID);
   --  Standard Ada vector of copied UCI identifiers with Positive indexing.
   --  Nonempty vectors start at one; standard vector equality and mutation
   --  rules apply. No uniqueness constraint is imposed on the identifiers.

   type Fault_Ambiguity_Group_List is private;
   --  Initially empty, ordered Ada-owned collection of unresolved fault groups.

   type Fault_List is private;
   --  Initially empty, ordered Ada-owned collection of fault-report values.

   procedure Append (Values : in out BIT_Type_List; Value : BIT_Type);
   --  Copy a supported-test description onto the end of an ordered list.
   --  @param Values Destination list, extended without removing duplicates.
   --  @param Value Supported-test description to copy into Ada-owned storage.

   procedure Append (Values : in out Active_BIT_List; Value : Active_BIT);
   --  Copy an active-test snapshot onto the end of an ordered list.
   --  @param Values Destination list, extended without removing duplicates.
   --  @param Value Active-test progress snapshot to copy.

   procedure Append (Values : in out Completed_BIT_Item_List; Value : Completed_BIT_Item);
   --  Copy a per-item test result onto the end of an ordered list.
   --  @param Values Destination list, extended without removing duplicates.
   --  @param Value Named item result and failure explanation to copy.

   procedure Append (Values : in out Completed_BIT_List; Value : Completed_BIT);
   --  Copy a completed-test snapshot onto the end of an ordered list.
   --  @param Values Destination list, extended without removing duplicates.
   --  @param Value Completed-test value, including copied nested item results.

   procedure Append (Values : in out Fault_Data_List; Value : Fault_Data);
   --  Copy fault-isolation data onto the end of an ordered list.
   --  @param Values Destination list, extended without removing duplicates.
   --  @param Value Textual key/value, format and units to copy.

   procedure Append (Values : in out Fault_Ambiguity_Group_List; Value : Fault_Ambiguity_Group);
   --  Copy an unresolved component/test group onto an ordered list.
   --  @param Values Destination list, extended without removing duplicates.
   --  @param Value Ambiguity group with independently copied identifier vectors.

   procedure Append (Values : in out Fault_List; Value : Fault);
   --  Copy a fault report onto the end of an ordered list.
   --  @param Values Destination list, extended without removing duplicates.
   --  @param Value Fault report, including its nested data and groups, to copy.

   function Create_BIT_Type
     (ID              : IR.UCI_ID;
      Interface_Value : BIT_Control_Interface;
      Duration        : Long_Long_Integer;
      Names           : String;
      Components      : UCI_ID_Vectors.Vector) return BIT_Type;
   --  Construct a supported-test description without calling the provider.
   --  Strings and component references are copied; no semantic validation of
   --  duration or control-interface support is performed.
   --  @param ID Unique test identifier used by BIT initiation and status.
   --  @param Interface_Value Single accepted BIT interface identifier.
   --  @param Duration Expected test duration in nanoseconds, copied as signed.
   --  @param Names One initial item name if nonempty, otherwise no item names.
   --  This is not a delimiter-separated list and is not split or parsed.
   --  @param Components Ordered references to components tested at least partly;
   --  duplicates and identifier labels are retained.
   --  @return Ada-owned supported-test value with copied fields.

   procedure Append_BIT_Item_Name (Value : in out BIT_Type; Name : String);
   --  Append a copied item name, including an empty name, to a test description.
   --  @param Value Test description whose item-name collection is extended.
   --  @param Name One name to append verbatim; order and duplicates survive.

   function Create_Active_BIT
     (ID : IR.UCI_ID; Completion : Long_Long_Integer; Percent : Long_Float) return Active_BIT;
   --  Construct an active-test snapshot without validating the progress range.
   --  @param ID Identifier of the active built-in test.
   --  @param Completion Estimated completion-time count in nanoseconds UTC as
   --  described upstream; no epoch is established or converted here.
   --  @param Percent Progress fraction, where 1.0 means 100 percent. Values
   --  above 1.0 are permitted upstream; Ada copies without clamping.
   --  @return Ada-owned copy of the identifier and both progress scalars.

   function Create_Completed_BIT_Item
     (Name : String; Value : BIT_Result; Reason : String) return Completed_BIT_Item;
   --  Construct one tested-item result with copied descriptive text.
   --  @param Name Logical or physical item tested, singular or aggregated.
   --  @param Value Result for this item, not necessarily the overall BIT result.
   --  @param Reason Human-readable failure explanation, expected upstream when
   --  the item failed; Ada permits any text/result combination.
   --  @return Ada-owned item result with independently stored strings.

   function Create_Completed_BIT
     (ID     : IR.UCI_ID;
      Time   : Long_Long_Integer;
      Value  : BIT_Result;
      Reason : String;
      Items  : Completed_BIT_Item_List) return Completed_BIT;
   --  Construct a completed-test snapshot without recomputing the summary from
   --  individual results or validating their consistency with it.
   --  @param ID Identifier of the completed built-in test.
   --  @param Time Test end-time count in nanoseconds UTC, without epoch inference.
   --  @param Value Overall summary result supplied by the caller.
   --  @param Reason Human-readable overall failure explanation, copied even if
   --  the summary does not indicate failure.
   --  @param Items Per-item results copied in order, including duplicates.
   --  @return Ada-owned snapshot independent of the supplied item list.
   function Create_Fault_Data (K, V, F, U : String) return Fault_Data;
   --  Copy a fault-isolation datum without parsing, validating or converting text.
   --  @param K Name or key of the reported datum.
   --  @param V Textual data value associated with the key.
   --  @param F Description of the value's representation or format.
   --  @param U Units description associated with the value.
   --  @return Ada-owned datum preserving all four input strings.

   function Create_Ambiguity_Group
     (Tests, Components : UCI_ID_Vectors.Vector) return Fault_Ambiguity_Group;
   --  Copy unresolved candidate components and diagnostic-test references.
   --  @param Tests Ordered tests that may further isolate the fault. No direct
   --  positional relationship to the component vector is implied.
   --  @param Components Ordered components that may be related to the fault.
   --  @return Ada-owned group preserving both vectors' order and duplicates.

   function Create_Fault
     (ID                : IR.UCI_ID;
      Severity_Value    : Fault_Severity;
      State_Value       : Fault_State;
      Time              : Long_Long_Integer;
      Code, Description : String;
      Data              : Fault_Data_List;
      Components        : UCI_ID_Vectors.Vector;
      Groups            : Fault_Ambiguity_Group_List) return Fault;
   --  Construct a fault report by copying every field without provider access
   --  or consistency checks among severity, state, data and implicated components.
   --  @param ID Unique identifier for this fault report.
   --  @param Severity_Value Reported impact, independent of lifecycle state.
   --  @param State_Value Reported set/cleared/unknown state, independent of impact.
   --  @param Time Detection-time count in nanoseconds UTC, not report-transmit
   --  time; no epoch inference or clock conversion is performed.
   --  @param Code Reported code or name of the fault.
   --  @param Description Human-readable fault/cause explanation, which may
   --  differ from the identifier's descriptive label.
   --  @param Data Additional isolation data, copied in order with duplicates.
   --  @param Components Individually implicated component identifiers.
   --  @param Groups Candidate-component groups where isolation is unresolved.
   --  @return Ada-owned report independent of the supplied strings and lists.

   type BIT_Status is private;
   --  Ada-owned status snapshot combining active tests, completed tests and
   --  reported faults. Default construction leaves all three collections empty.

   function Create_BIT_Status
     (Active : Active_BIT_List; Completed : Completed_BIT_List; Faults : Fault_List)
      return BIT_Status;
   --  Copy the three independent status collections without querying a provider.
   --  @param Active Ordered active-test progress values to copy.
   --  @param Completed Ordered completed-test values and nested items to copy.
   --  @param Faults Ordered fault reports and their nested data/groups to copy.
   --  @return Ada-owned snapshot preserving collection order and duplicates.

   function Active_BIT_Count (Value : BIT_Status) return Natural;
   --  Count active-test entries in this snapshot, not live provider requests.
   --  @param Value Built-in-test status snapshot.
   --  @return Stored active-test count, zero for an empty collection.

   function Active_BIT_At (Value : BIT_Status; Index : Positive) return Active_BIT;
   --  Copy one active-test progress entry in snapshot order.
   --  @param Value Status snapshot owning the active-test collection.
   --  @param Index One-based position within the stored active-test count.
   --  @return Independent active-test value, not a live progress subscription.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   function Completed_BIT_Count (Value : BIT_Status) return Natural;
   --  Count completed-test entries in this snapshot.
   --  @param Value Built-in-test status snapshot.
   --  @return Stored completed-test count, including duplicates.

   function Completed_BIT_At (Value : BIT_Status; Index : Positive) return Completed_BIT;
   --  Copy a completed-test entry with its nested per-item results.
   --  @param Value Status snapshot owning the completed-test collection.
   --  @param Index One-based position within the stored completed-test count.
   --  @return Ada-owned completed-test snapshot independent of the source.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   function Fault_Count (Value : BIT_Status) return Natural;
   --  Count fault reports stored alongside the built-in-test results.
   --  @param Value Built-in-test status snapshot.
   --  @return Stored fault-report count, including duplicates.

   function Fault_At (Value : BIT_Status; Index : Positive) return Fault;
   --  Copy a fault report with its nested data and ambiguity groups.
   --  @param Value Status snapshot owning the fault-report collection.
   --  @param Index One-based position within the stored fault count.
   --  @return Ada-owned fault report independent of the source snapshot.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   type State_Transition_Status is (Not_Set, Not_Transitioning, Shutting_Down, Transitioning);
   --  Whether the subsystem is transitioning, distinct from its overall state.
   --  @enum Not_Set No transition-status value has been set.
   --  @enum Not_Transitioning No state transition is reported in progress.
   --  @enum Shutting_Down A transition to shutdown is actively in progress.
   --  @enum Transitioning A transition to a state other than shutdown is active.

   type Component_State is
     (Not_Set, Unknown, Not_Installed, Off, Initializing, Operational, Degraded, Disabled, Faulted);
   --  Capability-related state of one subsystem component, not overall MFA state.
   --  @enum Not_Set No component-state value has been set.
   --  @enum Unknown The component's state is unknown.
   --  @enum Not_Installed The component is not installed.
   --  @enum Off Installed but not powered on.
   --  @enum Initializing Hardware components or software elements are initializing.
   --  @enum Operational Able to perform advertised capabilities.
   --  @enum Degraded Degraded but still supports at least one capability.
   --  @enum Disabled Not enabled, or explicitly disabled, to perform capabilities.
   --  @enum Faulted Incapable of supporting any capability.

   type Temperature_State is
     (Not_Set, Under_Temp, Normal, Over_Temp_Warning, Over_Temp_Degraded, Over_Temp_Shutdown);
   --  Reported thermal operating condition, separate from the Celsius reading.
   --  Temperature thresholds are provider-defined; Ada does not derive this enum
   --  from a numeric reading or initiate thermal protection actions.
   --  @enum Not_Set No temperature-state value has been set.
   --  @enum Under_Temp Temperature is below the normal operating range.
   --  @enum Normal Temperature is within the normal operating range.
   --  @enum Over_Temp_Warning Above normal range without operational impact yet.
   --  @enum Over_Temp_Degraded High temperature has degraded component operation.
   --  @enum Over_Temp_Shutdown The component has shut down due to high temperature.
   type Euler is record
      Roll, Pitch, Yaw : Long_Float := 0.0;
   end record;
   --  Component orientation/boresight angles in radians, relative to the host
   --  system frame centered on its navigational center. Upstream specifies the
   --  sequence yaw, pitch, roll; record field order is not a rotation algorithm.
   --  Ada stores angles without normalization or coordinate transformation.
   --  @field Roll Roll angle in radians, default zero.
   --  @field Pitch Pitch angle in radians, default zero.
   --  @field Yaw Yaw angle in radians, default zero.

   type Foreign_Key is private;
   --  Key or identifier in another system/protocol/network, together with that
   --  system's name. This is copied text, not an automatically resolved link.

   function Create_Foreign_Key (Key, System_Name : String) return Foreign_Key;
   --  Copy an external identifier and its naming-system description.
   --  @param Key Identifier text in the external naming system.
   --  @param System_Name Name identifying that external system or protocol.
   --  @return Ada-owned key without lookup, parsing or uniqueness checks.

   function Key (Value : Foreign_Key) return String;
   --  Copy an external identifier's key text.
   --  @param Value External-system reference.
   --  @return Independent Ada String containing the stored key.

   function System_Name (Value : Foreign_Key) return String;
   --  Copy the name of the system in which the external key is meaningful.
   --  @param Value External-system reference.
   --  @return Independent naming-system text, without resolving the key.

   type Installation_Details is private;
   --  Copied physical installation location, orientation and boresight of a
   --  component. The two Euler values remain distinct; Ada does not combine them.

   function Create_Installation_Details
     (Location : IR.Component_Location; Orientation, Boresight : Euler) return Installation_Details;
   --  Copy component installation geometry without coordinate transformation.
   --  @param Location Host-system forward/right/down offsets in meters and
   --  logical mounting-location reference, represented by the IR location type.
   --  @param Orientation Component orientation in radians in the host frame.
   --  @param Boresight Component boresight in radians in the host frame.
   --  @return Ada-owned copy of all three installation values.

   function Location (Value : Installation_Details) return IR.Component_Location;
   --  Copy the component's host-relative installation location.
   --  @param Value Physical-installation snapshot.
   --  @return Ada-owned location with meter offsets and mounting reference.

   function Orientation (Value : Installation_Details) return Euler;
   --  Read the stored component orientation, not its boresight.
   --  @param Value Physical-installation snapshot.
   --  @return Copied roll/pitch/yaw angles in radians without normalization.

   function Boresight (Value : Installation_Details) return Euler;
   --  Read the stored boresight, distinct from component orientation.
   --  @param Value Physical-installation snapshot.
   --  @return Copied roll/pitch/yaw angles in radians without normalization.

   type MFA_Component is private;
   --  Ada-owned component status: identity, capability state, temperature and
   --  logical/physical installation data. Components may be physical or functional.

   function Create_MFA_Component
     (ID                       : IR.UCI_ID;
      State                    : Component_State;
      Temperature_C            : Long_Float;
      Temperature_State_Value  : Temperature_State;
      Installation_Location_ID : Foreign_Key;
      Details                  : Installation_Details) return MFA_Component;
   --  Copy a component status without deriving thermal state or checking that
   --  logical and physical location references agree.
   --  @param ID Identifier of the subsystem component being reported.
   --  @param State Capability-related component state, not overall MFA state.
   --  @param Temperature_C Component temperature in degrees Celsius.
   --  @param Temperature_State_Value Independently supplied thermal condition.
   --  @param Installation_Location_ID Logical location where the component is
   --  mounted on the subsystem, as an external-system key.
   --  @param Details Physical location, orientation and boresight to copy.
   --  @return Ada-owned component value independent of all input objects.

   function Component_ID (Value : MFA_Component) return IR.UCI_ID;
   --  Read the identity of the component whose status is reported.
   --  @param Value Component-status snapshot.
   --  @return Copied UCI identifier, preserving UUID bytes and descriptive label.

   function State (Value : MFA_Component) return Component_State;
   --  Read the component's reported capability state.
   --  @param Value Component-status snapshot.
   --  @return Stored component state, independent of overall MFA state.

   function Temperature_C (Value : MFA_Component) return Long_Float;
   --  Read the reported numeric temperature without unit conversion.
   --  @param Value Component-status snapshot.
   --  @return Stored temperature in degrees Celsius, not Kelvin or Fahrenheit.

   function Temperature_Status (Value : MFA_Component) return Temperature_State;
   --  Read the independently reported thermal operating condition.
   --  @param Value Component-status snapshot.
   --  @return Stored thermal state; Ada does not calculate it from temperature.

   function Installation_Location_ID (Value : MFA_Component) return Foreign_Key;
   --  Copy the logical location where this component mounts on the subsystem.
   --  @param Value Component-status snapshot.
   --  @return Ada-owned external location key, not an automatically resolved site.

   function Installation (Value : MFA_Component) return Installation_Details;
   --  Copy physical installation data independently of the component snapshot.
   --  @param Value Component-status snapshot.
   --  @return Ada-owned location, orientation and boresight values.

   type MFA_Component_List is private;
   --  Initially empty ordered collection of copied component-status values.

   procedure Append (Values : in out MFA_Component_List; Value : MFA_Component);
   --  Append a component snapshot without deduplicating by identifier.
   --  @param Values Destination ordered collection to extend.
   --  @param Value Component snapshot and nested installation values to copy.

   type About is private;
   --  Ada-owned model/serial/software/bootloader/hardware identification text,
   --  distinct from numeric provider API and library version information.

   function Create_About
     (Model, Serial_Number, Software_Version, Bootloader_Software_Version, Hardware_Version :
        String) return About;
   --  Copy identification strings without parsing version-number syntax.
   --  @param Model Coordinated model name, generally established at production.
   --  @param Serial_Number Identifier of an instance of that model.
   --  @param Software_Version Software/operational-flight-program version text.
   --  @param Bootloader_Software_Version Bootloader version text.
   --  @param Hardware_Version Hardware revision text for the model instance.
   --  @return Ada-owned identification snapshot preserving all five strings.

   function Model (Value : About) return String;
   --  Copy the model name reported for the subsystem/item.
   --  @param Value Identification/version-text snapshot.
   --  @return Independent model-name String, possibly empty.

   function Serial_Number (Value : About) return String;
   --  Copy the identifier assigned to an instance of the model.
   --  @param Value Identification/version-text snapshot.
   --  @return Independent serial-number text without numeric interpretation.

   function Software_Version (Value : About) return String;
   --  Copy reported software/operational-flight-program version text.
   --  @param Value Identification/version-text snapshot.
   --  @return Independent software version String, not a provider API number.

   function Bootloader_Software_Version (Value : About) return String;
   --  Copy reported bootloader software version text.
   --  @param Value Identification/version-text snapshot.
   --  @return Independent bootloader version String, without syntax conversion.

   function Hardware_Version (Value : About) return String;
   --  Copy the hardware revision text associated with the item/model.
   --  @param Value Identification/version-text snapshot.
   --  @return Independent hardware version String, without numeric interpretation.

   type MFA_Status is private;
   --  Ada-owned overall subsystem readiness/status with state/mode descriptions,
   --  transition progress, version text and subordinate component snapshots.

   function Create_MFA_Status
     (State                               : MFA_State;
      State_Description, Mode_Description : String;
      Transition                          : State_Transition_Status;
      About_Value                         : About;
      Components                          : MFA_Component_List) return MFA_Status;
   --  Copy subsystem status fields without validating the state machine or
   --  aggregating component states into the overall state.
   --  @param State Overall reported MFA/subsystem operating state.
   --  @param State_Description Supplemental human-readable state description.
   --  @param Mode_Description Supplemental human-readable mode description.
   --  @param Transition Reported transition progress, independent of State.
   --  @param About_Value Model/serial/software/hardware identification to copy.
   --  @param Components Ordered subordinate physical/functional components;
   --  duplicate component identifiers are not removed.
   --  @return Ada-owned status snapshot independent of the supplied collection.

   function State (Value : MFA_Status) return MFA_State;
   --  Read overall MFA/subsystem state, not the state of an individual component.
   --  @param Value Overall status snapshot.
   --  @return Stored overall state without deriving it from component values.

   function State_Description (Value : MFA_Status) return String;
   --  Copy supplemental human-readable state information.
   --  @param Value Overall status snapshot.
   --  @return Independent state-description String, possibly empty.

   function Mode_Description (Value : MFA_Status) return String;
   --  Copy supplemental human-readable operating-mode information.
   --  @param Value Overall status snapshot.
   --  @return Independent mode-description String, not an executable mode command.

   function Transition_Status (Value : MFA_Status) return State_Transition_Status;
   --  Read whether a state transition is reported in progress.
   --  @param Value Overall status snapshot.
   --  @return Stored transition status, distinct from the destination/state value.

   function About_Value (Value : MFA_Status) return About;
   --  Copy model, serial and software/hardware identification from status.
   --  @param Value Overall status snapshot.
   --  @return Independent Ada-owned identification/version-text snapshot.

   function Component_Count (Value : MFA_Status) return Natural;
   --  Count subordinate physical or functional components included in status.
   --  @param Value Overall status snapshot.
   --  @return Stored component count, including duplicate identifiers.

   function Component_At (Value : MFA_Status; Index : Positive) return MFA_Component;
   --  Copy one subordinate component status with nested installation data.
   --  @param Value Overall status snapshot owning the component collection.
   --  @param Index One-based position within the stored component count.
   --  @return Ada-owned component value independent of the overall snapshot.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   type Name_Value_Pair is private;
   --  Ada-owned textual attribute not represented by another status structure.
   --  Neither the attribute's vocabulary nor its value syntax is imposed here.

   function Create_Name_Value_Pair (Name, Value : String) return Name_Value_Pair;
   --  Copy an attribute name and textual value without parsing either string.
   --  @param Name Attribute name, copied verbatim, including an empty string.
   --  @param Value Attribute value, copied verbatim without numeric conversion.
   --  @return Ada-owned pair independent of both input strings.

   function Name (Value : Name_Value_Pair) return String;
   --  Copy the attribute name from a textual status pair.
   --  @param Value Ada-owned name/value pair.
   --  @return Independent Ada String naming the stored attribute.

   function Pair_Value (Value : Name_Value_Pair) return String;
   --  Copy the attribute's text without applying a schema or unit conversion.
   --  @param Value Ada-owned name/value pair.
   --  @return Independent Ada String containing the stored value text.

   type Name_Value_Pair_List is private;
   --  Initially empty ordered list of copied textual attributes. Repeated names
   --  are retained; this is not a map and append does not replace earlier pairs.

   procedure Append (Values : in out Name_Value_Pair_List; Value : Name_Value_Pair);
   --  Append a copied attribute pair, retaining repeated names and values.
   --  @param Values Destination list extended at its end.
   --  @param Value Attribute pair to copy into the destination's Ada storage.

   subtype Discrete_Status is Name_Value_Pair_List;
   --  Ordered subsystem-specific status attributes not covered by other messages.
   --  This is the same Ada list type, not a distinct validated status schema.

   subtype MFA_Status_Detailed is Name_Value_Pair_List;
   --  Ordered MFA resource/hardware metrics represented as textual attributes.
   --  Upstream expects providers to document their names and values; the bridge
   --  supplies no universal metric schema. This is the same Ada pair-list type.

   function Pair_Count (Values : Name_Value_Pair_List) return Natural;
   --  Count stored attribute pairs, including repeated names.
   --  @param Values Discrete/detailed status or another attribute-pair list.
   --  @return Number of pairs, zero for an empty list.

   function Pair_At (Values : Name_Value_Pair_List; Index : Positive) return Name_Value_Pair;
   --  Copy one attribute pair in list order, without lookup by name.
   --  @param Values List owning the attribute pairs.
   --  @param Index One-based position within the stored pair count.
   --  @return Independent Ada-owned attribute pair.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   type Security_Event_Kind is
     (None, Authentication, Integrity, File_Management, Key_Management, System, Sanitization);
   --  Selected security-event variant, independent of its kind-specific category.
   --  Providers need only report event kinds relevant to their design.
   --  @enum None No event subtype, corresponding to the upstream empty variant.
   --  @enum Authentication Authentication of subsystems, services, files or devices.
   --  @enum Integrity Integrity checking of files, bootloader or other objects.
   --  @enum File_Management Updates to configuration, operational software,
   --  mission data or other files.
   --  @enum Key_Management Key/certificate loading, generation, update or deletion.
   --  @enum System System startup, shutdown, reset or fault events.
   --  @enum Sanitization Zeroization, sanitization or classified-data erasure.

   type Security_Outcome is (Not_Set, Failure, Success);
   --  Outcome of the audited operation, independent of security significance.
   --  @enum Not_Set No outcome has been specified.
   --  @enum Failure The event or operation failed.
   --  @enum Success The event or operation succeeded.

   type Security_Severity is (Not_Set, Critical, Error, Informational, Warning);
   --  Significance of a security audit event, not a hardware fault severity.
   --  @enum Not_Set No security significance has been specified.
   --  @enum Critical Significant information/mechanisms compromised; correct
   --  security-policy operation can no longer be assumed; immediate action needed.
   --  @enum Error Less significant information/mechanisms compromised; action
   --  is required within a given time.
   --  @enum Informational Normal operational information requiring no action.
   --  @enum Warning A security event may occur if preventive action is not taken.

   type Security_Artifact is private;
   --  Copied component identifier and associated-item identifier for audit
   --  context, such as a file the component could not authenticate.

   function Create_Security_Artifact
     (Component_ID, Associated_ID : IR.UCI_ID) return Security_Artifact;
   --  Copy an audit artifact's component and associated-item references.
   --  @param Component_ID Identifier of the specific component.
   --  @param Associated_ID Identifier of an associated item, when needed.
   --  @return Ada-owned pair preserving both identifiers and their labels.

   function Component_ID (Value : Security_Artifact) return IR.UCI_ID;
   --  Copy the component reference from an audit artifact.
   --  @param Value Audit artifact linking a component to an associated item.
   --  @return Component UCI identifier, not the associated-item identifier.

   function Associated_ID (Value : Security_Artifact) return IR.UCI_ID;
   --  Copy the contextual item reference from an audit artifact.
   --  @param Value Audit artifact linking a component to an associated item.
   --  @return Associated-item identifier, possibly its default value if unused.

   type Security_Artifact_List is private;
   --  Initially empty ordered collection of copied security-audit artifacts.

   procedure Append (Values : in out Security_Artifact_List; Value : Security_Artifact);
   --  Copy an audit artifact onto the end of a list without deduplication.
   --  @param Values Destination ordered artifact collection.
   --  @param Value Component/associated-item pair to copy into the collection.

   type Security_Event is private;
   --  Ada-owned flattened security-event value: kind, category, details and
   --  subtype-specific identifiers. Authentication reports subsystem/service/MDF
   --  references; integrity and file management report MDF references; other
   --  kinds have none of these subtype references in the native source.
   --  The Ada constructor still stores all supplied fields without validation.

   function Create_Security_Event
     (Kind                             : Security_Event_Kind;
      Category                         : Natural;
      Details                          : String;
      Subsystem_ID, Service_ID, MDF_ID : IR.UCI_ID) return Security_Event;
   --  Copy a security-event value without validating category against kind or
   --  clearing identifiers not applicable to that kind. This does not publish
   --  an audit record or execute a security operation.
   --  @param Kind Selected event subtype; None represents no subtype.
   --  @param Category Kind-specific numeric category, not a universal enum.
   --  The constructor accepts any Natural; provider-received categories are
   --  separately validated by the native bridge.
   --  @param Details Human-readable subtype details copied without parsing.
   --  @param Subsystem_ID Authentication subsystem reference, when applicable.
   --  @param Service_ID Authentication service reference, when applicable.
   --  @param MDF_ID Mission-data-file reference for authentication, integrity
   --  or file-management events, when applicable.
   --  @return Ada-owned event preserving every supplied field, even an
   --  inconsistent kind/category/reference combination.

   function Kind (Value : Security_Event) return Security_Event_Kind;
   --  Read the selected security-event subtype.
   --  @param Value Flattened security-event snapshot.
   --  @return Stored kind; it determines interpretation of the category number.

   function Category (Value : Security_Event) return Natural;
   --  Read the kind-specific category number. Zero is unset for each concrete
   --  upstream subtype. Authentication values 1..7 mean subsystem, service,
   --  configuration-file, operational-software, mission-data-file, other-file
   --  and other-device authentication. Integrity 1..6 means configuration,
   --  bootloader, operational-software, mission-data-file, other-file and other
   --  integrity checks. File management 1..4 means configuration, operational-
   --  software, mission-data-file and other-file updates. Key management 1..6
   --  means key load, generation, update, deletion, certificate load and deletion.
   --  System 1..4 means startup, shutdown, reset and fault. Sanitization 1..3
   --  means zeroization, sanitization and classified-data erase.
   --  @param Value Event whose kind selects the numeric category vocabulary.
   --  @return Stored category without decoding; Ada-created values need not
   --  lie in a native subtype's supported category range.

   function Details (Value : Security_Event) return String;
   --  Copy subtype-specific audit details, without inferring an account-access
   --  meaning from the base class's generic member comment.
   --  @param Value Flattened security-event snapshot.
   --  @return Independent Ada String containing stored detail text.

   function Subsystem_ID (Value : Security_Event) return IR.UCI_ID;
   --  Copy the authentication subtype's subsystem reference when applicable.
   --  @param Value Event value; local construction can populate this for any kind.
   --  @return Stored identifier. Non-authentication native events leave this
   --  subtype field at its default; this is not the audit record's subsystem ID.

   function Service_ID (Value : Security_Event) return IR.UCI_ID;
   --  Copy the authentication subtype's service reference when applicable.
   --  @param Value Event value; local construction can populate this for any kind.
   --  @return Stored identifier; other native event kinds leave it at default.

   function MDF_ID (Value : Security_Event) return IR.UCI_ID;
   --  Copy the subtype's mission-data-file reference when applicable.
   --  @param Value Authentication, integrity or file-management event, or a
   --  locally constructed value whose fields were not validated by kind.
   --  @return Stored identifier; other native kinds leave this field at default.

   type Security_Audit_Record is private;
   --  Ada-owned audit snapshot with event identity/time, reporting subsystem,
   --  ordered artifacts, selected event data, outcome and security severity.

   function Create_Security_Audit_Record
     (Event_ID     : IR.UCI_ID;
      Timestamp_NS : Long_Long_Integer;
      Subsystem_ID : IR.UCI_ID;
      Artifacts    : Security_Artifact_List;
      Event        : Security_Event;
      Outcome      : Security_Outcome;
      Severity     : Security_Severity) return Security_Audit_Record;
   --  Copy an audit record without publishing it or validating cross-field
   --  consistency. Nested text, artifacts and identifiers are independently owned.
   --  @param Event_ID Identifier of the reported security event.
   --  @param Timestamp_NS Signed nanosecond count for when the event occurred.
   --  The pinned declaration specifies no epoch or clock scale.
   --  @param Subsystem_ID Reporting subsystem reference, distinct from any
   --  authentication-specific subsystem reference inside the event data.
   --  @param Artifacts Ordered component/associated-item context, copied with
   --  duplicates retained.
   --  @param Event Selected subtype/category/details and applicable references.
   --  @param Outcome Reported success/failure/unset outcome of the operation.
   --  @param Severity Security significance, independent of outcome.
   --  @return Ada-owned audit snapshot independent of the input artifact list.

   function Event_ID (Value : Security_Audit_Record) return IR.UCI_ID;
   --  Copy the identity assigned to the audited security event.
   --  @param Value Security-audit snapshot.
   --  @return Event identifier with its UUID bytes and descriptive label.

   function Timestamp_NS (Value : Security_Audit_Record) return Long_Long_Integer;
   --  Read when the security event occurred, without interpreting a clock epoch.
   --  @param Value Security-audit snapshot.
   --  @return Signed nanosecond count; unlike some BIT fields, the pinned audit
   --  declaration does not even specify UTC. No clock conversion is performed.

   function Subsystem_ID (Value : Security_Audit_Record) return IR.UCI_ID;
   --  Copy the audit record's reporting-subsystem reference.
   --  @param Value Security-audit snapshot.
   --  @return Subsystem identifier, distinct from authentication subtype context.

   function Artifact_Count (Value : Security_Audit_Record) return Natural;
   --  Count contextual component/associated-item pairs in the audit record.
   --  @param Value Security-audit snapshot.
   --  @return Stored artifact count, including duplicate pairs.

   function Artifact_At (Value : Security_Audit_Record; Index : Positive) return Security_Artifact;
   --  Copy one contextual artifact pair in audit-record order.
   --  @param Value Audit record owning the artifact collection.
   --  @param Index One-based position within the stored artifact count.
   --  @return Ada-owned artifact pair independent of the audit record.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   function Event (Value : Security_Audit_Record) return Security_Event;
   --  Copy the selected security-event data and applicable subtype references.
   --  @param Value Security-audit snapshot.
   --  @return Ada-owned event value with its kind, category and copied details.

   function Outcome (Value : Security_Audit_Record) return Security_Outcome;
   --  Read the audited operation's success/failure status.
   --  @param Value Security-audit snapshot.
   --  @return Stored outcome, independent of severity and event kind.

   function Severity (Value : Security_Audit_Record) return Security_Severity;
   --  Read the security significance of the event, not hardware fault severity.
   --  @param Value Security-audit snapshot.
   --  @return Stored security severity without deriving it from the outcome.

private
   package US renames Ada.Strings.Unbounded;
   use type US.Unbounded_String;
   package String_Vectors is new Ada.Containers.Vectors (Positive, US.Unbounded_String);
   type BIT_Type is record
      ID              : IR.UCI_ID;
      Interface_Value : BIT_Control_Interface := Not_Set;
      Duration        : Long_Long_Integer := 0;
      Names           : String_Vectors.Vector;
      Components      : UCI_ID_Vectors.Vector;
   end record;
   package BIT_Type_Vectors is new Ada.Containers.Vectors (Positive, BIT_Type);
   type Active_BIT is record
      ID         : IR.UCI_ID;
      Completion : Long_Long_Integer := 0;
      Percent    : Long_Float := 0.0;
   end record;
   package Active_BIT_Vectors is new Ada.Containers.Vectors (Positive, Active_BIT);
   type Completed_BIT_Item is record
      Name, Why : US.Unbounded_String;
      Value     : BIT_Result := Not_Set;
   end record;
   package Completed_BIT_Item_Vectors is new Ada.Containers.Vectors (Positive, Completed_BIT_Item);
   type Completed_BIT is record
      ID    : IR.UCI_ID;
      Time  : Long_Long_Integer := 0;
      Value : BIT_Result := Not_Set;
      Why   : US.Unbounded_String;
      Items : Completed_BIT_Item_Vectors.Vector;
   end record;
   package Completed_BIT_Vectors is new Ada.Containers.Vectors (Positive, Completed_BIT);
   type Fault_Data is record
      K, V, F, U : US.Unbounded_String;
   end record;
   package Fault_Data_Vectors is new Ada.Containers.Vectors (Positive, Fault_Data);
   type Fault_Ambiguity_Group is record
      Tests, Components : UCI_ID_Vectors.Vector;
   end record;
   package Fault_Ambiguity_Group_Vectors is new
     Ada.Containers.Vectors (Positive, Fault_Ambiguity_Group);
   type Fault is record
      ID                : IR.UCI_ID;
      Severity_Value    : Fault_Severity := Not_Set;
      State_Value       : Fault_State := Not_Set;
      Time              : Long_Long_Integer := 0;
      Code, Description : US.Unbounded_String;
      Data              : Fault_Data_Vectors.Vector;
      Components        : UCI_ID_Vectors.Vector;
      Groups            : Fault_Ambiguity_Group_Vectors.Vector;
   end record;
   package Fault_Vectors is new Ada.Containers.Vectors (Positive, Fault);
   type BIT_Type_List is record
      Values : BIT_Type_Vectors.Vector;
   end record;
   type Active_BIT_List is record
      Values : Active_BIT_Vectors.Vector;
   end record;
   type Completed_BIT_Item_List is record
      Values : Completed_BIT_Item_Vectors.Vector;
   end record;
   type Completed_BIT_List is record
      Values : Completed_BIT_Vectors.Vector;
   end record;
   type Fault_Data_List is record
      Values : Fault_Data_Vectors.Vector;
   end record;
   type Fault_Ambiguity_Group_List is record
      Values : Fault_Ambiguity_Group_Vectors.Vector;
   end record;
   type Fault_List is record
      Values : Fault_Vectors.Vector;
   end record;
   type BIT_Status is record
      Active    : Active_BIT_Vectors.Vector;
      Completed : Completed_BIT_Vectors.Vector;
      Faults    : Fault_Vectors.Vector;
   end record;
   type Foreign_Key is record
      K, S : US.Unbounded_String;
   end record;
   type Installation_Details is record
      L    : IR.Component_Location;
      O, B : Euler;
   end record;
   type MFA_Component is record
      ID          : IR.UCI_ID;
      State_Value : Component_State := Not_Set;
      Temp        : Long_Float := 0.0;
      Temp_State  : Temperature_State := Not_Set;
      Location_ID : Foreign_Key;
      Details     : Installation_Details;
   end record;
   package MFA_Component_Vectors is new Ada.Containers.Vectors (Positive, MFA_Component);
   type MFA_Component_List is record
      Values : MFA_Component_Vectors.Vector;
   end record;
   type About is record
      M, S, SW, B, H : US.Unbounded_String;
   end record;
   type MFA_Status is record
      State_Value           : MFA_State := Not_Set;
      State_Text, Mode_Text : US.Unbounded_String;
      Transition            : State_Transition_Status := Not_Set;
      About_Data            : About;
      Components            : MFA_Component_Vectors.Vector;
   end record;
   type Name_Value_Pair is record
      N, V : US.Unbounded_String;
   end record;
   package Name_Value_Pair_Vectors is new Ada.Containers.Vectors (Positive, Name_Value_Pair);
   type Name_Value_Pair_List is record
      Values : Name_Value_Pair_Vectors.Vector;
   end record;
   type Security_Artifact is record
      Component, Associated : IR.UCI_ID;
   end record;
   package Security_Artifact_Vectors is new Ada.Containers.Vectors (Positive, Security_Artifact);
   type Security_Artifact_List is record
      Values : Security_Artifact_Vectors.Vector;
   end record;
   type Security_Event is record
      Event_Kind              : Security_Event_Kind := None;
      Category_Value          : Natural := 0;
      Text                    : US.Unbounded_String;
      Subsystem, Service, MDF : IR.UCI_ID;
   end record;
   type Security_Audit_Record is record
      ID             : IR.UCI_ID;
      Time           : Long_Long_Integer := 0;
      Subsystem      : IR.UCI_ID;
      Artifacts      : Security_Artifact_Vectors.Vector;
      Event_Data     : Security_Event;
      Outcome_Value  : Security_Outcome := Not_Set;
      Severity_Value : Security_Severity := Not_Set;
   end record;
end AMS.MEL.Status;
