private with Ada.Containers.Vectors;
private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with AMS.MEL.Status;
with Interfaces;

package AMS.MEL.IR.Health_Status.Metadata is
   --  Owned polling of six HealthAndStatus metadata families and complete subsystem-health data.
   --  Shared status, BIT/fault and security types are authoritative in AMS.MEL.Status; the public
   --  Status renaming refers to that canonical package. GNATdoc 26 omits the renaming section,
   --  which remains an explicitly outstanding renderer exception. No optional NUC temperature
   --  callback is bound here.

   package Status renames AMS.MEL.Status;

   type Metadata_Stream is limited private;
   --  Limited owner of public health metadata consumption. Channel-owned native callback state
   --  survives public metadata Close; finalization attempts non-raising cleanup. Serialize
   --  operations on the same Ada owner.
   function Open (Channel : Health_Channel; Queue_Capacity : Positive := 16) return Metadata_Stream;
   --  Registers the six bound health/status callbacks on an attached or enabled channel, once per
   --  channel including a failed attempt. Registration may synchronously produce events. Failure
   --  publishes no metadata owner; partial registrations retain inactive state because upstream
   --  has no unregister operation. All kinds share one bounded drop-incoming queue ordered by
   --  successful enqueue, not timestamps or simultaneous callback entry order.
   --  @param Channel Attached or enabled owning typed health channel.
   --  @param Queue_Capacity Maximum queued events across all six kinds; defaults to sixteen.
   --  @return Limited owner of public metadata polling.
   --  @exception Provider_Error Invalid/closed channel, repeated registration attempt, provider
   --  registration failure/exception or native failure.
   function Is_Open (Stream : Metadata_Stream) return Boolean;
   --  Tests local public metadata-handle ownership, not callback activity or provider health.
   --  @param Stream Metadata owner to inspect.
   --  @return True while its local native handle is nonnull.

   type Failure_Level is (NA, Critical, Major, Parametric, Informational, Available, Not_Present);
   --  Provider failure-state category, not a numeric severity scale or bitmask. Descriptions do
   --  not invent thresholds absent from the pinned interface.
   --  @enum NA Upstream NA category; no further meaning is specified in the pinned enum.
   --  @enum Critical Upstream critical failure, explicitly annotated as no heartbeat message.
   --  @enum Major Provider major failure category; no threshold is specified upstream.
   --  @enum Parametric Provider parametric failure category; no threshold is specified upstream.
   --  @enum Informational Provider informational category.
   --  @enum Available No failure, as explicitly described upstream.
   --  @enum Not_Present Subsystem is not physically or simulated-ly installed.
   type CSCI_Mode is
     (Unknown,
      Unused,
      Initialization,
      Maintenance,
      Idle,
      Operational,
      VSA,
      Quick_Look,
      Track,
      Imaging,
      Noise);
   --  Reported subsystem software configuration mode, preserved without commanding a mode
   --  transition. The pinned enum does not define operational criteria for these modes.
   --  @enum Unknown Unknown reported mode.
   --  @enum Unused Unused mode.
   --  @enum Initialization Initialization mode.
   --  @enum Maintenance Maintenance mode.
   --  @enum Idle Idle mode.
   --  @enum Operational Operational mode.
   --  @enum VSA Upstream VSA mode; the acronym is not expanded by the pinned declaration.
   --  @enum Quick_Look Quick-look mode.
   --  @enum Track Track mode.
   --  @enum Imaging Imaging mode.
   --  @enum Noise Noise mode.
   type Subsystem_Dependency is private;
   --  Owned dependent-subsystem identity, criticality and failure category copied from
   --  SubsystemDepInfo. No severity scale is inferred from criticality.
   function Subsystem_ID (Value : Subsystem_Dependency) return Interfaces.Unsigned_32;
   --  Provider subsystem identifier, preserving all 32 bits.
   --  @param Value Owned dependency to inspect.
   --  @return Provider subsystem identifier, preserving all 32 bits.
   function Criticality (Value : Subsystem_Dependency) return Interfaces.Unsigned_32;
   --  Raw provider criticality value; no numeric severity scale or bitmask is established
   --  upstream.
   --  @param Value Owned dependency to inspect.
   --  @return Raw provider criticality value; no numeric severity scale or bitmask is established
   --  upstream.
   function Failure (Value : Subsystem_Dependency) return Failure_Level;
   --  Reported failure category without bridge reclassification.
   --  @param Value Owned dependency to inspect.
   --  @return Reported failure category without bridge reclassification.
   type Version is record
      Source, Major_Revision, Minor_Revision, Engineering_Revision : Interfaces.Unsigned_32;
   end record;
   --  Four unsigned subsystem software-version components. This is neither the bridge ABI version
   --  nor provider API/library version; no packed encoding or source code scheme is imposed.
   --  @field Source Raw upstream version source component; its encoding is not specified.
   --  @field Major_Revision Reported major revision component.
   --  @field Minor_Revision Reported minor revision component.
   --  @field Engineering_Revision Reported engineering revision component.
   type Subsystem_CSCI is private;
   --  Owned subsystem computer-software configuration-item data, including independent name text,
   --  version, criticality, failure, BIT change counter and connection indication.
   function CSCI_Name (Value : Subsystem_CSCI) return String;
   --  Independent Ada string containing the validated provider software-item name.
   --  @param Value Owned software configuration item to inspect.
   --  @return Independent Ada string containing the validated provider software-item name.
   function Mode (Value : Subsystem_CSCI) return CSCI_Mode;
   --  Reported software configuration mode; no transition is performed.
   --  @param Value Owned software configuration item to inspect.
   --  @return Reported software configuration mode; no transition is performed.
   function Version_Value (Value : Subsystem_CSCI) return Version;
   --  Copied four-component subsystem software version.
   --  @param Value Owned software configuration item to inspect.
   --  @return Copied four-component subsystem software version.
   function Criticality (Value : Subsystem_CSCI) return Interfaces.Unsigned_32;
   --  Raw provider criticality value; no numeric severity scale or bitmask is established
   --  upstream.
   --  @param Value Owned software configuration item to inspect.
   --  @return Raw provider criticality value; no numeric severity scale or bitmask is established
   --  upstream.
   function Failure (Value : Subsystem_CSCI) return Failure_Level;
   --  Reported failure category without bridge reclassification.
   --  @param Value Owned software configuration item to inspect.
   --  @return Reported failure category without bridge reclassification.
   function BIT_Report (Value : Subsystem_CSCI) return Interfaces.Unsigned_32;
   --  Provider counter incremented when BIT status changes, enabling a pull pattern; not an actual
   --  BIT report payload.
   --  @param Value Owned software configuration item to inspect.
   --  @return Provider counter incremented when BIT status changes, enabling a pull pattern; not
   --  an actual BIT report payload.
   function Connection_Established (Value : Subsystem_CSCI) return Boolean;
   --  Provider connection-established indication; no connectivity probe is performed.
   --  @param Value Owned software configuration item to inspect.
   --  @return Provider connection-established indication; no connectivity probe is performed.
   type Subsystem_Status is private;
   --  Owned subsystem-status snapshot with separately preserved reported counts and copied
   --  dependency/CSCI sequences. Actual lengths, not reported counts, bound indexed access. Order
   --  and duplicates are preserved.
   function Subsystem_ID (Value : Subsystem_Status) return Interfaces.Unsigned_32;
   --  Provider subsystem identifier, preserving all 32 bits.
   --  @param Value Owned subsystem status to inspect.
   --  @return Provider subsystem identifier, preserving all 32 bits.
   function Criticality (Value : Subsystem_Status) return Interfaces.Unsigned_32;
   --  Raw provider criticality value; no numeric severity scale or bitmask is established
   --  upstream.
   --  @param Value Owned subsystem status to inspect.
   --  @return Raw provider criticality value; no numeric severity scale or bitmask is established
   --  upstream.
   function Status_Sequence_Number (Value : Subsystem_Status) return Interfaces.Unsigned_32;
   --  Raw provider status sequence number; no monotonicity or wrap policy is imposed.
   --  @param Value Owned subsystem status to inspect.
   --  @return Raw provider status sequence number; no monotonicity or wrap policy is imposed.
   function Failure (Value : Subsystem_Status) return Failure_Level;
   --  Reported failure category without bridge reclassification.
   --  @param Value Owned subsystem status to inspect.
   --  @return Reported failure category without bridge reclassification.
   function Reported_Subsystem_Count (Value : Subsystem_Status) return Interfaces.Unsigned_32;
   --  Provider-reported dependency count, not the copied vector length.
   --  @param Value Owned subsystem status to inspect.
   --  @return Provider-reported dependency count, not the copied vector length.
   function Subsystem_Count (Value : Subsystem_Status) return Natural;
   --  Actual copied dependency-sequence length.
   --  @param Value Owned subsystem status to inspect.
   --  @return Actual copied dependency-sequence length.
   function Subsystem_At (Value : Subsystem_Status; Index : Positive) return Subsystem_Dependency;
   --  Dependency at the one-based copied-sequence index.
   --  @param Value Owned subsystem status whose dependency sequence is inspected.
   --  @param Index One-based position; raises Constraint Error beyond the actual copied length.
   --  @return Dependency at the one-based copied-sequence index.
   function Reported_CSCI_Count (Value : Subsystem_Status) return Interfaces.Unsigned_32;
   --  Provider-reported CSCI count, not the copied vector length.
   --  @param Value Owned subsystem status to inspect.
   --  @return Provider-reported CSCI count, not the copied vector length.
   function CSCI_Count (Value : Subsystem_Status) return Natural;
   --  Actual copied CSCI-sequence length.
   --  @param Value Owned subsystem status to inspect.
   --  @return Actual copied CSCI-sequence length.
   function CSCI_At (Value : Subsystem_Status; Index : Positive) return Subsystem_CSCI;
   --  Software configuration item at the one-based copied-sequence index.
   --  @param Value Owned subsystem status to inspect.
   --  @param Index One-based position; raises Constraint Error beyond the actual copied length.
   --  @return Software configuration item at the one-based copied-sequence index.

   type Metadata_Kind is
     (MFA_Status_Event,
      BIT_Status_Event,
      Subsystem_Status_Event,
      Discrete_Status_Event,
      Security_Audit_Event,
      MFA_Status_Detailed_Event);
   --  Discriminator for six bound health callback families. Canonical Status types define shared
   --  status/BIT/fault/security semantics; this package does not redefine them.
   --  @enum MFA_Status_Event Canonical Status MFA status snapshot.
   --  @enum BIT_Status_Event Canonical Status BIT status, completed/active tests and fault
   --  information.
   --  @enum Subsystem_Status_Event Subsystem dependencies and software configuration items.
   --  @enum Discrete_Status_Event Canonical Status discrete name/value sequence.
   --  @enum Security_Audit_Event Canonical Status security-audit record and its active event
   --  variant.
   --  @enum MFA_Status_Detailed_Event Canonical Status detailed MFA name/value sequence.
   type Metadata_Event (Event_Kind : Metadata_Kind := MFA_Status_Event) is private;
   --  Owned discriminated event copied from native metadata. All nested collections and strings
   --  are Ada-owned and survive metadata/channel/Session closure. Call only the accessor
   --  corresponding to the discriminant; see each declared precondition.
   --  @field Event_Kind Selects the active payload; defaults to the MFA status alternative.
   function Kind (Event : Metadata_Event) return Metadata_Kind;
   --  Returns the public event discriminant without interpreting its payload.
   --  @param Event Owned event to inspect.
   --  @return Active metadata kind.
   function MFA_Status_Value (Event : Metadata_Event) return Status.MFA_Status
   with Pre => Kind (Event) = MFA_Status_Event;
   --  Returns an owned payload for the MFA Status Event kind. Requires Kind equal to MFA Status
   --  Event. Enabled precondition checks raise Assertion Error on violation; the body directly
   --  selects the discriminated component, so a wrong-kind component selection is also subject to
   --  Ada discriminant checking (Constraint Error), not a bridge Provider Error conversion.
   --  Consult canonical AMS.MEL.Status for shared data semantics.
   --  @param Event Owned event whose kind satisfies the declared precondition.
   --  @return Independent payload value of the corresponding event kind.
   function BIT_Status_Value (Event : Metadata_Event) return Status.BIT_Status
   with Pre => Kind (Event) = BIT_Status_Event;
   --  Returns an owned payload for the BIT Status Event kind. Requires Kind equal to BIT Status
   --  Event. Enabled precondition checks raise Assertion Error on violation; the body directly
   --  selects the discriminated component, so a wrong-kind component selection is also subject to
   --  Ada discriminant checking (Constraint Error), not a bridge Provider Error conversion.
   --  Consult canonical AMS.MEL.Status for shared data semantics.
   --  @param Event Owned event whose kind satisfies the declared precondition.
   --  @return Independent payload value of the corresponding event kind.
   function Subsystem_Status_Value (Event : Metadata_Event) return Subsystem_Status
   with Pre => Kind (Event) = Subsystem_Status_Event;
   --  Returns an owned payload for the Subsystem Status Event kind. Requires Kind equal to
   --  Subsystem Status Event. Enabled precondition checks raise Assertion Error on violation; the
   --  body directly selects the discriminated component, so a wrong-kind component selection is
   --  also subject to Ada discriminant checking (Constraint Error), not a bridge Provider Error
   --  conversion. Consult canonical AMS.MEL.Status for shared data semantics.
   --  @param Event Owned event whose kind satisfies the declared precondition.
   --  @return Independent payload value of the corresponding event kind.
   function Discrete_Status_Value (Event : Metadata_Event) return Status.Discrete_Status
   with Pre => Kind (Event) = Discrete_Status_Event;
   --  Returns an owned payload for the Discrete Status Event kind. Requires Kind equal to Discrete
   --  Status Event. Enabled precondition checks raise Assertion Error on violation; the body
   --  directly selects the discriminated component, so a wrong-kind component selection is also
   --  subject to Ada discriminant checking (Constraint Error), not a bridge Provider Error
   --  conversion. Consult canonical AMS.MEL.Status for shared data semantics.
   --  @param Event Owned event whose kind satisfies the declared precondition.
   --  @return Independent payload value of the corresponding event kind.
   function Security_Audit_Value (Event : Metadata_Event) return Status.Security_Audit_Record
   with Pre => Kind (Event) = Security_Audit_Event;
   --  Returns an owned payload for the Security Audit Event kind. Requires Kind equal to Security
   --  Audit Event. Enabled precondition checks raise Assertion Error on violation; the body
   --  directly selects the discriminated component, so a wrong-kind component selection is also
   --  subject to Ada discriminant checking (Constraint Error), not a bridge Provider Error
   --  conversion. Consult canonical AMS.MEL.Status for shared data semantics.
   --  @param Event Owned event whose kind satisfies the declared precondition.
   --  @return Independent payload value of the corresponding event kind.
   function MFA_Status_Detailed_Value (Event : Metadata_Event) return Status.MFA_Status_Detailed
   with Pre => Kind (Event) = MFA_Status_Detailed_Event;
   --  Returns an owned payload for the MFA Status Detailed Event kind. Requires Kind equal to MFA
   --  Status Detailed Event. Enabled precondition checks raise Assertion Error on violation; the
   --  body directly selects the discriminated component, so a wrong-kind component selection is
   --  also subject to Ada discriminant checking (Constraint Error), not a bridge Provider Error
   --  conversion. Consult canonical AMS.MEL.Status for shared data semantics.
   --  @param Event Owned event whose kind satisfies the declared precondition.
   --  @return Independent payload value of the corresponding event kind.

   function Receive
     (Stream : Metadata_Stream; Timeout_Milliseconds : Natural := 0) return Metadata_Event;
   --  Dequeues one native event, deeply copies its active payload to Ada and closes the temporary
   --  native event before returning. Existing queued events can drain after channel closure or
   --  callback failure; an empty inactive queue raises Stream Stopped, while an empty failed queue
   --  raises Provider Error. Provider callbacks never call Ada. Malformed records are discarded;
   --  exceptions during callback copying/allocation mark the metadata stream failed rather than
   --  being silently counted as a malformed record.
   --  @param Stream Open public metadata owner; externally serialize use and Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   --  @return Ada-owned discriminated event with independent nested data.
   --  @exception Timeout_Error Active queue remained empty until the wait expired.
   --  @exception Stream_Stopped Inactive or stopped queue is empty.
   --  @exception Provider_Error Closed owner, failed callback state, invalid conversion or native
   --  failure.
   type Metadata_Counters is record
      Events_Received, Events_Dropped_Queue_Full, Malformed_Or_Unsupported : Counter;
   end record;
   --  Saturating native callback counters across all six kinds; not counts of successful Ada
   --  receives.
   --  @field Events_Received Observed callback entries, including entries after consumption
   --  becomes inactive.
   --  @field Events_Dropped_Queue_Full Valid copied incoming events dropped at queue capacity;
   --  queued events are not evicted.
   --  @field Malformed_Or_Unsupported Null, unknown-enum, invalid-text or otherwise unsupported
   --  events rejected by conversion. Callback exceptions instead mark the stream failed.
   function Counters (Stream : Metadata_Stream) return Metadata_Counters;
   --  Copies the current callback counter snapshot without dequeuing events.
   --  @param Stream Open metadata-consumption owner.
   --  @return Independent saturating counter values.
   --  @exception Provider_Error Closed owner or native query failure.
   procedure Close (Stream : in out Metadata_Stream);
   --  Idempotently closes public metadata consumption and wakes native waiters. It does not
   --  unregister callbacks or close the typed health channel. Callback state remains with the
   --  provider channel; channel destruction followed by bridge callback draining is the cleanup
   --  boundary, not public metadata Close. Finalization attempts cleanup without raising.
   --  @param Stream Public metadata owner to close; serialize against other uses.
   --  @exception Provider_Error Native metadata cleanup failure.

private
   package US renames Ada.Strings.Unbounded;
   type Subsystem_Dependency is record
      ID, Importance : Interfaces.Unsigned_32;
      Failure_Value  : Failure_Level := NA;
   end record;
   package Dependency_Vectors is new Ada.Containers.Vectors (Positive, Subsystem_Dependency);
   type Subsystem_CSCI is record
      Name          : US.Unbounded_String;
      Mode_Value    : CSCI_Mode := Unknown;
      Version_Data  : Version := (others => 0);
      Importance    : Interfaces.Unsigned_32 := 0;
      Failure_Value : Failure_Level := NA;
      Report        : Interfaces.Unsigned_32 := 0;
      Connected     : Boolean := False;
   end record;
   package CSCI_Vectors is new Ada.Containers.Vectors (Positive, Subsystem_CSCI);
   type Subsystem_Status is record
      ID, Importance, Sequence : Interfaces.Unsigned_32 := 0;
      Failure_Value            : Failure_Level := NA;
      Reported_Subsystems      : Interfaces.Unsigned_32 := 0;
      Subsystems               : Dependency_Vectors.Vector;
      Reported_CSCI            : Interfaces.Unsigned_32 := 0;
      CSCIs                    : CSCI_Vectors.Vector;
   end record;
   type Metadata_Event (Event_Kind : Metadata_Kind := MFA_Status_Event) is record
      case Event_Kind is
         when MFA_Status_Event =>
            MFA : Status.MFA_Status;

         when BIT_Status_Event =>
            BIT : Status.BIT_Status;

         when Subsystem_Status_Event =>
            Subsystem : Subsystem_Status;

         when Discrete_Status_Event =>
            Discrete : Status.Discrete_Status;

         when Security_Audit_Event =>
            Security : Status.Security_Audit_Record;

         when MFA_Status_Detailed_Event =>
            Detailed : Status.MFA_Status_Detailed;
      end case;
   end record;
   type Metadata_Stream is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Health_Metadata_Handle := AMS.MEL_C_API.Null_Health_Metadata;
   end record;
   overriding
   procedure Finalize (Stream : in out Metadata_Stream);
end AMS.MEL.IR.Health_Status.Metadata;
