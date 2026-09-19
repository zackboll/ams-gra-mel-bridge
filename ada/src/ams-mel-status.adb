package body AMS.MEL.Status is
   procedure Append (Values : in out BIT_Type_List; Value : BIT_Type) is
   begin
      Values.Values.Append (Value);
   end Append;
   procedure Append (Values : in out Active_BIT_List; Value : Active_BIT) is
   begin
      Values.Values.Append (Value);
   end Append;
   procedure Append (Values : in out Completed_BIT_Item_List; Value : Completed_BIT_Item) is
   begin
      Values.Values.Append (Value);
   end Append;
   procedure Append (Values : in out Completed_BIT_List; Value : Completed_BIT) is
   begin
      Values.Values.Append (Value);
   end Append;
   procedure Append (Values : in out Fault_Data_List; Value : Fault_Data) is
   begin
      Values.Values.Append (Value);
   end Append;
   procedure Append (Values : in out Fault_Ambiguity_Group_List; Value : Fault_Ambiguity_Group) is
   begin
      Values.Values.Append (Value);
   end Append;
   procedure Append (Values : in out Fault_List; Value : Fault) is
   begin
      Values.Values.Append (Value);
   end Append;
   procedure Append (Values : in out MFA_Component_List; Value : MFA_Component) is
   begin
      Values.Values.Append (Value);
   end Append;
   procedure Append (Values : in out Name_Value_Pair_List; Value : Name_Value_Pair) is
   begin
      Values.Values.Append (Value);
   end Append;
   procedure Append (Values : in out Security_Artifact_List; Value : Security_Artifact) is
   begin
      Values.Values.Append (Value);
   end Append;
   function BIT_ID (Value : BIT_Type) return IR.UCI_ID
   is (Value.ID);
   function Accepted_Interface (Value : BIT_Type) return BIT_Control_Interface
   is (Value.Interface_Value);
   function Expected_Duration_NS (Value : BIT_Type) return Long_Long_Integer
   is (Value.Duration);
   function BIT_Item_Name_Count (Value : BIT_Type) return Natural
   is (Natural (Value.Names.Length));
   function BIT_Item_Name_At (Value : BIT_Type; Index : Positive) return String
   is (US.To_String (Value.Names (Index)));
   function Subsystem_Component_Count (Value : BIT_Type) return Natural
   is (Natural (Value.Components.Length));
   function Subsystem_Component_At (Value : BIT_Type; Index : Positive) return IR.UCI_ID
   is (Value.Components (Index));
   function BIT_ID (Value : Active_BIT) return IR.UCI_ID
   is (Value.ID);
   function Estimated_Completion_Time_NS (Value : Active_BIT) return Long_Long_Integer
   is (Value.Completion);
   function Estimated_Percent_Complete (Value : Active_BIT) return Long_Float
   is (Value.Percent);
   function BIT_Item_Name (Value : Completed_BIT_Item) return String
   is (US.To_String (Value.Name));
   function Result (Value : Completed_BIT_Item) return BIT_Result
   is (Value.Value);
   function Fail_Reason (Value : Completed_BIT_Item) return String
   is (US.To_String (Value.Why));
   function BIT_ID (Value : Completed_BIT) return IR.UCI_ID
   is (Value.ID);
   function Time_Tag_NS (Value : Completed_BIT) return Long_Long_Integer
   is (Value.Time);
   function Result (Value : Completed_BIT) return BIT_Result
   is (Value.Value);
   function Fail_Reason (Value : Completed_BIT) return String
   is (US.To_String (Value.Why));
   function BIT_Item_Count (Value : Completed_BIT) return Natural
   is (Natural (Value.Items.Length));
   function BIT_Item_At (Value : Completed_BIT; Index : Positive) return Completed_BIT_Item
   is (Value.Items (Index));
   function Key (Value : Fault_Data) return String
   is (US.To_String (Value.K));
   function Data_Value (Value : Fault_Data) return String
   is (US.To_String (Value.V));
   function Format (Value : Fault_Data) return String
   is (US.To_String (Value.F));
   function Units (Value : Fault_Data) return String
   is (US.To_String (Value.U));
   function Diagnostic_Test_Count (Value : Fault_Ambiguity_Group) return Natural
   is (Natural (Value.Tests.Length));
   function Diagnostic_Test_At (Value : Fault_Ambiguity_Group; Index : Positive) return IR.UCI_ID
   is (Value.Tests (Index));
   function Component_Count (Value : Fault_Ambiguity_Group) return Natural
   is (Natural (Value.Components.Length));
   function Component_At (Value : Fault_Ambiguity_Group; Index : Positive) return IR.UCI_ID
   is (Value.Components (Index));
   function Fault_ID (Value : Fault) return IR.UCI_ID
   is (Value.ID);
   function Severity (Value : Fault) return Fault_Severity
   is (Value.Severity_Value);
   function State (Value : Fault) return Fault_State
   is (Value.State_Value);
   function Detection_Time_NS (Value : Fault) return Long_Long_Integer
   is (Value.Time);
   function Fault_Code (Value : Fault) return String
   is (US.To_String (Value.Code));
   function Fault_Description (Value : Fault) return String
   is (US.To_String (Value.Description));
   function Fault_Data_Count (Value : Fault) return Natural
   is (Natural (Value.Data.Length));
   function Fault_Data_At (Value : Fault; Index : Positive) return Fault_Data
   is (Value.Data (Index));
   function Component_Count (Value : Fault) return Natural
   is (Natural (Value.Components.Length));
   function Component_At (Value : Fault; Index : Positive) return IR.UCI_ID
   is (Value.Components (Index));
   function Ambiguity_Group_Count (Value : Fault) return Natural
   is (Natural (Value.Groups.Length));
   function Ambiguity_Group_At (Value : Fault; Index : Positive) return Fault_Ambiguity_Group
   is (Value.Groups (Index));
   function Create_BIT_Type
     (ID              : IR.UCI_ID;
      Interface_Value : BIT_Control_Interface;
      Duration        : Long_Long_Integer;
      Names           : String;
      Components      : UCI_ID_Vectors.Vector) return BIT_Type
   is
      R : BIT_Type :=
        (ID              => ID,
         Interface_Value => Interface_Value,
         Duration        => Duration,
         Names           => <>,
         Components      => Components);
   begin
      if Names'Length > 0 then
         R.Names.Append (US.To_Unbounded_String (Names));
      end if;
      return R;
   end Create_BIT_Type;
   procedure Append_BIT_Item_Name (Value : in out BIT_Type; Name : String) is
   begin
      Value.Names.Append (US.To_Unbounded_String (Name));
   end;
   function Create_Active_BIT
     (ID : IR.UCI_ID; Completion : Long_Long_Integer; Percent : Long_Float) return Active_BIT
   is ((ID, Completion, Percent));
   function Create_Completed_BIT_Item
     (Name : String; Value : BIT_Result; Reason : String) return Completed_BIT_Item
   is ((US.To_Unbounded_String (Name), US.To_Unbounded_String (Reason), Value));
   function Create_Completed_BIT
     (ID     : IR.UCI_ID;
      Time   : Long_Long_Integer;
      Value  : BIT_Result;
      Reason : String;
      Items  : Completed_BIT_Item_List) return Completed_BIT
   is ((ID, Time, Value, US.To_Unbounded_String (Reason), Items.Values));
   function Create_Fault_Data (K, V, F, U : String) return Fault_Data
   is ((US.To_Unbounded_String (K),
        US.To_Unbounded_String (V),
        US.To_Unbounded_String (F),
        US.To_Unbounded_String (U)));
   function Create_Ambiguity_Group
     (Tests, Components : UCI_ID_Vectors.Vector) return Fault_Ambiguity_Group
   is ((Tests, Components));
   function Create_Fault
     (ID                : IR.UCI_ID;
      Severity_Value    : Fault_Severity;
      State_Value       : Fault_State;
      Time              : Long_Long_Integer;
      Code, Description : String;
      Data              : Fault_Data_List;
      Components        : UCI_ID_Vectors.Vector;
      Groups            : Fault_Ambiguity_Group_List) return Fault
   is ((ID,
        Severity_Value,
        State_Value,
        Time,
        US.To_Unbounded_String (Code),
        US.To_Unbounded_String (Description),
        Data.Values,
        Components,
        Groups.Values));
   function Create_BIT_Status
     (Active : Active_BIT_List; Completed : Completed_BIT_List; Faults : Fault_List)
      return BIT_Status
   is ((Active.Values, Completed.Values, Faults.Values));
   function Active_BIT_Count (Value : BIT_Status) return Natural
   is (Natural (Value.Active.Length));
   function Active_BIT_At (Value : BIT_Status; Index : Positive) return Active_BIT
   is (Value.Active (Index));
   function Completed_BIT_Count (Value : BIT_Status) return Natural
   is (Natural (Value.Completed.Length));
   function Completed_BIT_At (Value : BIT_Status; Index : Positive) return Completed_BIT
   is (Value.Completed (Index));
   function Fault_Count (Value : BIT_Status) return Natural
   is (Natural (Value.Faults.Length));
   function Fault_At (Value : BIT_Status; Index : Positive) return Fault
   is (Value.Faults (Index));
   function Create_Foreign_Key (Key, System_Name : String) return Foreign_Key
   is ((US.To_Unbounded_String (Key), US.To_Unbounded_String (System_Name)));
   function Key (Value : Foreign_Key) return String
   is (US.To_String (Value.K));
   function System_Name (Value : Foreign_Key) return String
   is (US.To_String (Value.S));
   function Create_Installation_Details
     (Location : IR.Component_Location; Orientation, Boresight : Euler) return Installation_Details
   is ((Location, Orientation, Boresight));
   function Location (Value : Installation_Details) return IR.Component_Location
   is (Value.L);
   function Orientation (Value : Installation_Details) return Euler
   is (Value.O);
   function Boresight (Value : Installation_Details) return Euler
   is (Value.B);
   function Create_MFA_Component
     (ID                       : IR.UCI_ID;
      State                    : Component_State;
      Temperature_C            : Long_Float;
      Temperature_State_Value  : Temperature_State;
      Installation_Location_ID : Foreign_Key;
      Details                  : Installation_Details) return MFA_Component
   is ((ID, State, Temperature_C, Temperature_State_Value, Installation_Location_ID, Details));
   function Component_ID (Value : MFA_Component) return IR.UCI_ID
   is (Value.ID);
   function State (Value : MFA_Component) return Component_State
   is (Value.State_Value);
   function Temperature_C (Value : MFA_Component) return Long_Float
   is (Value.Temp);
   function Temperature_Status (Value : MFA_Component) return Temperature_State
   is (Value.Temp_State);
   function Installation_Location_ID (Value : MFA_Component) return Foreign_Key
   is (Value.Location_ID);
   function Installation (Value : MFA_Component) return Installation_Details
   is (Value.Details);
   function Create_About
     (Model, Serial_Number, Software_Version, Bootloader_Software_Version, Hardware_Version :
        String) return About
   is ((US.To_Unbounded_String (Model),
        US.To_Unbounded_String (Serial_Number),
        US.To_Unbounded_String (Software_Version),
        US.To_Unbounded_String (Bootloader_Software_Version),
        US.To_Unbounded_String (Hardware_Version)));
   function Model (Value : About) return String
   is (US.To_String (Value.M));
   function Serial_Number (Value : About) return String
   is (US.To_String (Value.S));
   function Software_Version (Value : About) return String
   is (US.To_String (Value.SW));
   function Bootloader_Software_Version (Value : About) return String
   is (US.To_String (Value.B));
   function Hardware_Version (Value : About) return String
   is (US.To_String (Value.H));
   function Create_MFA_Status
     (State                               : MFA_State;
      State_Description, Mode_Description : String;
      Transition                          : State_Transition_Status;
      About_Value                         : About;
      Components                          : MFA_Component_List) return MFA_Status
   is ((State,
        US.To_Unbounded_String (State_Description),
        US.To_Unbounded_String (Mode_Description),
        Transition,
        About_Value,
        Components.Values));
   function State (Value : MFA_Status) return MFA_State
   is (Value.State_Value);
   function State_Description (Value : MFA_Status) return String
   is (US.To_String (Value.State_Text));
   function Mode_Description (Value : MFA_Status) return String
   is (US.To_String (Value.Mode_Text));
   function Transition_Status (Value : MFA_Status) return State_Transition_Status
   is (Value.Transition);
   function About_Value (Value : MFA_Status) return About
   is (Value.About_Data);
   function Component_Count (Value : MFA_Status) return Natural
   is (Natural (Value.Components.Length));
   function Component_At (Value : MFA_Status; Index : Positive) return MFA_Component
   is (Value.Components (Index));
   function Create_Name_Value_Pair (Name, Value : String) return Name_Value_Pair
   is ((US.To_Unbounded_String (Name), US.To_Unbounded_String (Value)));
   function Name (Value : Name_Value_Pair) return String
   is (US.To_String (Value.N));
   function Pair_Value (Value : Name_Value_Pair) return String
   is (US.To_String (Value.V));
   function Pair_Count (Values : Name_Value_Pair_List) return Natural
   is (Natural (Values.Values.Length));
   function Pair_At (Values : Name_Value_Pair_List; Index : Positive) return Name_Value_Pair
   is (Values.Values (Index));
   function Create_Security_Artifact
     (Component_ID, Associated_ID : IR.UCI_ID) return Security_Artifact
   is ((Component_ID, Associated_ID));
   function Component_ID (Value : Security_Artifact) return IR.UCI_ID
   is (Value.Component);
   function Associated_ID (Value : Security_Artifact) return IR.UCI_ID
   is (Value.Associated);
   function Create_Security_Event
     (Kind                             : Security_Event_Kind;
      Category                         : Natural;
      Details                          : String;
      Subsystem_ID, Service_ID, MDF_ID : IR.UCI_ID) return Security_Event
   is ((Kind, Category, US.To_Unbounded_String (Details), Subsystem_ID, Service_ID, MDF_ID));
   function Kind (Value : Security_Event) return Security_Event_Kind
   is (Value.Event_Kind);
   function Category (Value : Security_Event) return Natural
   is (Value.Category_Value);
   function Details (Value : Security_Event) return String
   is (US.To_String (Value.Text));
   function Subsystem_ID (Value : Security_Event) return IR.UCI_ID
   is (Value.Subsystem);
   function Service_ID (Value : Security_Event) return IR.UCI_ID
   is (Value.Service);
   function MDF_ID (Value : Security_Event) return IR.UCI_ID
   is (Value.MDF);
   function Create_Security_Audit_Record
     (Event_ID     : IR.UCI_ID;
      Timestamp_NS : Long_Long_Integer;
      Subsystem_ID : IR.UCI_ID;
      Artifacts    : Security_Artifact_List;
      Event        : Security_Event;
      Outcome      : Security_Outcome;
      Severity     : Security_Severity) return Security_Audit_Record
   is ((Event_ID, Timestamp_NS, Subsystem_ID, Artifacts.Values, Event, Outcome, Severity));
   function Event_ID (Value : Security_Audit_Record) return IR.UCI_ID
   is (Value.ID);
   function Timestamp_NS (Value : Security_Audit_Record) return Long_Long_Integer
   is (Value.Time);
   function Subsystem_ID (Value : Security_Audit_Record) return IR.UCI_ID
   is (Value.Subsystem);
   function Artifact_Count (Value : Security_Audit_Record) return Natural
   is (Natural (Value.Artifacts.Length));
   function Artifact_At (Value : Security_Audit_Record; Index : Positive) return Security_Artifact
   is (Value.Artifacts (Index));
   function Event (Value : Security_Audit_Record) return Security_Event
   is (Value.Event_Data);
   function Outcome (Value : Security_Audit_Record) return Security_Outcome
   is (Value.Outcome_Value);
   function Severity (Value : Security_Audit_Record) return Security_Severity
   is (Value.Severity_Value);
end AMS.MEL.Status;
