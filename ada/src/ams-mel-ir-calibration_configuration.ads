with Ada.Containers.Indefinite_Vectors;
with Ada.Containers.Vectors;
with AMS.MEL.IR.Channel;
private with Ada.Strings.Unbounded;

package AMS.MEL.IR.Calibration_Configuration is
   --  Complete owned values for the pinned optional CalibrationConfiguration.
   --  No Request_ID correlation, calibration execution or CalibrationStatus support is asserted.
   Not_Supported : exception;
   --  Explicit optional callback registration NotSupported; not a request rejection.
   subtype Identifier is AMS.MEL.IR.Channel.Command_ID;
   --  Unsigned 32-bit identifier preserving every bit without inferred correlation.
   type Configuration_Command is record
      Command_ID : Identifier := 0;
      Request_ID : Identifier := 0;
   end record;
   --  Independent unsigned 32-bit fields, including zero and UINT32_MAX.
   --  @field Command_ID Upstream command identifier, passed unconverted.
   --  @field Request_ID Independent upstream request identifier, passed unconverted.
   type Accepted_Interface is (Not_Set, Subsystem_Calibration_Command, Subsystem_State_Command);
   --  Exactly pinned values 0, 1, 2. MaxExclusive and future values are rejected, never remapped.
   --  @enum Not_Set Upstream NotSet (0), a meaningful unset interface.
   --  @enum Subsystem_Calibration_Command Upstream SubsystemCalibrationCommand (1).
   --  @enum Subsystem_State_Command Upstream SubsystemStateCommand (2).
   for Accepted_Interface use
     (Not_Set => 0, Subsystem_Calibration_Command => 1, Subsystem_State_Command => 2);
   package Name_Vectors is new Ada.Containers.Indefinite_Vectors (Positive, String);
   --  Owned ordered strings; empty names and independent name counts are supported.
   package ID_Vectors is new Ada.Containers.Vectors (Positive, UCI_ID);
   --  Owned ordered identifiers retaining every UUID byte and complete label.
   type Subsystem_Calibration is record
      Calibration_ID  : UCI_ID;
      Interface_Value : Accepted_Interface := Not_Set;
      Item_Names      : Name_Vectors.Vector;
      Component_IDs   : ID_Vectors.Vector;
      Capability_IDs  : ID_Vectors.Vector;
   end record;
   --  Fully owned entry. Each nested vector has independent length and order.
   --  UCI_ID retains every UUID byte and its complete descriptive label.
   --  @field Calibration_ID Complete calibration identity, including descriptive label.
   --  @field Interface_Value Exact accepted-interface alternative, never a boundary sentinel.
   --  @field Item_Names Ordered non-component/non-capability names, independent of ID vectors.
   --  @field Component_IDs Ordered subsystem component identifiers, not zipped with other vectors.
   --  @field Capability_IDs Ordered capability identifiers, not zipped with other vectors.
   type Calibration_Collection is private;
   --  Owned ordered collection; ordinary assignment creates independent Ada values.
   function Length (Value : Calibration_Collection) return Natural;
   --  Number of entries, including zero for a successful empty configuration.
   --  @param Value Owned collection to inspect.
   --  @return Number of ordered calibration entries.
   function Element (Value : Calibration_Collection; Index : Positive) return Subsystem_Calibration;
   --  Copy an entry (one-based); invalid indices raise Constraint_Error.
   --  @param Value Owned collection to read.
   --  @param Index One-based entry position, not a calibration identifier.
   --  @return Independent owned entry including all three nested collections.
   procedure Append (Value : in out Calibration_Collection; Entry_Value : Subsystem_Calibration);
   --  Append a complete owned copy, preserving all nested collection ordering.
   --  @param Value Destination collection.
   --  @param Entry_Value Complete entry copied to the end.
   subtype Outcome is AMS.MEL.IR.Channel.Outcome;
   --  Success, including an empty vector, or explicit MEL rejection.
   use type Outcome;
   subtype Error_Code is AMS.MEL.IR.Channel.Error_Code;
   --  Canonical known MEL error categories; unknown provider codes are errors.
   type Configuration_Result is private;
   --  Success (possibly empty) or explicit MEL rejection. Other failures raise Provider_Error.
   function Status (Result : Configuration_Result) return Outcome;
   --  Inspect terminal category without provider access.
   --  @param Result Owned terminal result.
   --  @return Success or Rejected.
   function Configuration (Result : Configuration_Result) return Calibration_Collection
   with Pre => Status (Result) = AMS.MEL.IR.Channel.Success;
   --  Return a deep Ada copy, independent of native request, C2 and Session lifetimes.
   --  @param Result Successful result, including an empty configuration.
   --  @return Complete owned calibration collection.
   function Rejection_Code (Result : Configuration_Result) return Error_Code
   with Pre => Status (Result) = AMS.MEL.IR.Channel.Rejected;
   --  Read an explicit MEL rejection category, not a callback registration result.
   --  @param Result Rejected terminal result.
   --  @return Known mapped MEL error category.
   function Description (Result : Configuration_Result) return String
   with Pre => Status (Result) = AMS.MEL.IR.Channel.Rejected;
   --  Copy the complete cached provider rejection text, including long descriptions.
   --  @param Result Rejected terminal result.
   --  @return Independent description string.
   type Configuration_Request is
     new AMS.MEL.IR.Calibration_Configuration_Request_Owner with private;
   --  Limited controlled observer. Pending work retains C2/Session/provider independently.
   function Is_Open (Request : Configuration_Request) return Boolean;
   --  Test local handle presence, not completion or provider availability.
   --  @param Request Request owner to inspect.
   --  @return True while a public observer is present.
   function Wait
     (Request : Configuration_Request; Timeout_Milliseconds : Natural) return Configuration_Result;
   --  Finite wait; Timeout_Error is not cancellation. Repeated Wait copies the cached result.
   --  Provider/future exceptions, invalid data, null success and cleanup failures raise Provider_Error.
   --  @param Request Open request owner; serialize against Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   --  @return Owned success configuration or explicit MEL rejection.
   --  @exception Timeout_Error The request remains pending.
   --  @exception Provider_Error Closed request, native failure, malformed provider result or deferred cleanup failure.
   procedure Close (Request : in out Configuration_Request);
   --  Idempotent observer closure, not cancellation. Serialize against Wait.
   --  @param Request Observer to release; pending work continues independently.
   --  @exception Provider_Error Native observer closure failed.
private
   package US renames Ada.Strings.Unbounded;
   package Calibration_Vectors is new Ada.Containers.Vectors (Positive, Subsystem_Calibration);
   type Calibration_Collection is record
      Entries : Calibration_Vectors.Vector;
   end record;
   type Configuration_Request is new AMS.MEL.IR.Calibration_Configuration_Request_Owner with record
      null;
   end record;
   type Configuration_Result is record
      Result_Status        : Outcome := AMS.MEL.IR.Channel.Success;
      Result_Configuration : Calibration_Collection;
      Result_Code          : Error_Code := AMS.MEL.IR.Channel.None;
      Result_Text          : US.Unbounded_String;
   end record;
end AMS.MEL.IR.Calibration_Configuration;
