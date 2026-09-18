with Ada.Containers.Vectors;
private with Ada.Strings.Unbounded;
with AMS.MEL.IR;

package AMS.MEL.Status is
   use type AMS.MEL.IR.UCI_ID;
   type MFA_State is
     (Not_Set, Unknown, Not_Installed, Off, Pre_Initialization,
      Initialization, Standby, Operate, Operate_Rx_Only, Operate_Tx_Only,
      Maintenance, Calibration, Initiated_BIT, Shutdown, Degraded);
   for MFA_State use
     (Not_Set => 0, Unknown => 1, Not_Installed => 2, Off => 3,
      Pre_Initialization => 4, Initialization => 5, Standby => 6,
      Operate => 7, Operate_Rx_Only => 8, Operate_Tx_Only => 9,
      Maintenance => 10, Calibration => 11, Initiated_BIT => 12,
      Shutdown => 13, Degraded => 14);
   for MFA_State'Size use 32;

   type BIT_Control_Interface is (Not_Set, Subsystem_BIT_Command,
      Subsystem_State_Command, Subsystem_Initiated);
   for BIT_Control_Interface use (Not_Set => 0, Subsystem_BIT_Command => 1,
      Subsystem_State_Command => 2, Subsystem_Initiated => 3);
   for BIT_Control_Interface'Size use 32;
   type BIT_Result is (Not_Set, Pass, Fail, Interrupted, Not_Tested);
   for BIT_Result use (Not_Set => 0, Pass => 1, Fail => 2, Interrupted => 3,
      Not_Tested => 4);
   for BIT_Result'Size use 32;
   type Fault_Severity is (Not_Set, Nominal, Caution, Warning, Failed);
   for Fault_Severity use (Not_Set => 0, Nominal => 1, Caution => 2,
      Warning => 3, Failed => 4);
   for Fault_Severity'Size use 32;
   type Fault_State is (Not_Set, Set, Cleared, Unknown);
   for Fault_State use (Not_Set => 0, Set => 1, Cleared => 2, Unknown => 3);
   for Fault_State'Size use 32;

   type BIT_Type is private;
   type Active_BIT is private;
   type Completed_BIT_Item is private;
   type Completed_BIT is private;
   type Fault_Data is private;
   type Fault_Ambiguity_Group is private;
   type Fault is private;

   function BIT_ID (Value : BIT_Type) return IR.UCI_ID;
   function Accepted_Interface (Value : BIT_Type) return BIT_Control_Interface;
   function Expected_Duration_NS (Value : BIT_Type) return Long_Long_Integer;
   function BIT_Item_Name_Count (Value : BIT_Type) return Natural;
   function BIT_Item_Name_At (Value : BIT_Type; Index : Positive) return String;
   function Subsystem_Component_Count (Value : BIT_Type) return Natural;
   function Subsystem_Component_At (Value : BIT_Type; Index : Positive) return IR.UCI_ID;
   function BIT_ID (Value : Active_BIT) return IR.UCI_ID;
   function Estimated_Completion_Time_NS (Value : Active_BIT) return Long_Long_Integer;
   function Estimated_Percent_Complete (Value : Active_BIT) return Long_Float;
   function BIT_Item_Name (Value : Completed_BIT_Item) return String;
   function Result (Value : Completed_BIT_Item) return BIT_Result;
   function Fail_Reason (Value : Completed_BIT_Item) return String;
   function BIT_ID (Value : Completed_BIT) return IR.UCI_ID;
   function Time_Tag_NS (Value : Completed_BIT) return Long_Long_Integer;
   function Result (Value : Completed_BIT) return BIT_Result;
   function Fail_Reason (Value : Completed_BIT) return String;
   function BIT_Item_Count (Value : Completed_BIT) return Natural;
   function BIT_Item_At (Value : Completed_BIT; Index : Positive) return Completed_BIT_Item;
   function Key (Value : Fault_Data) return String;
   function Data_Value (Value : Fault_Data) return String;
   function Format (Value : Fault_Data) return String;
   function Units (Value : Fault_Data) return String;
   function Diagnostic_Test_Count (Value : Fault_Ambiguity_Group) return Natural;
   function Diagnostic_Test_At (Value : Fault_Ambiguity_Group; Index : Positive) return IR.UCI_ID;
   function Component_Count (Value : Fault_Ambiguity_Group) return Natural;
   function Component_At (Value : Fault_Ambiguity_Group; Index : Positive) return IR.UCI_ID;
   function Fault_ID (Value : Fault) return IR.UCI_ID;
   function Severity (Value : Fault) return Fault_Severity;
   function State (Value : Fault) return Fault_State;
   function Detection_Time_NS (Value : Fault) return Long_Long_Integer;
   function Fault_Code (Value : Fault) return String;
   function Fault_Description (Value : Fault) return String;
   function Fault_Data_Count (Value : Fault) return Natural;
   function Fault_Data_At (Value : Fault; Index : Positive) return Fault_Data;
   function Component_Count (Value : Fault) return Natural;
   function Component_At (Value : Fault; Index : Positive) return IR.UCI_ID;
   function Ambiguity_Group_Count (Value : Fault) return Natural;
   function Ambiguity_Group_At (Value : Fault; Index : Positive) return Fault_Ambiguity_Group;

   type BIT_Type_List is private;
   type Active_BIT_List is private;
   type Completed_BIT_Item_List is private;
   type Completed_BIT_List is private;
   type Fault_Data_List is private;
   package UCI_ID_Vectors is new Ada.Containers.Vectors (Positive, IR.UCI_ID);
   type Fault_Ambiguity_Group_List is private;
   type Fault_List is private;
   procedure Append (Values : in out BIT_Type_List; Value : BIT_Type);
   procedure Append (Values : in out Active_BIT_List; Value : Active_BIT);
   procedure Append (Values : in out Completed_BIT_Item_List; Value : Completed_BIT_Item);
   procedure Append (Values : in out Completed_BIT_List; Value : Completed_BIT);
   procedure Append (Values : in out Fault_Data_List; Value : Fault_Data);
   procedure Append (Values : in out Fault_Ambiguity_Group_List; Value : Fault_Ambiguity_Group);
   procedure Append (Values : in out Fault_List; Value : Fault);

   function Create_BIT_Type (ID : IR.UCI_ID; Interface_Value : BIT_Control_Interface;
      Duration : Long_Long_Integer; Names : String; Components : UCI_ID_Vectors.Vector)
      return BIT_Type;
   procedure Append_BIT_Item_Name (Value : in out BIT_Type; Name : String);
   function Create_Active_BIT (ID : IR.UCI_ID; Completion : Long_Long_Integer;
      Percent : Long_Float) return Active_BIT;
   function Create_Completed_BIT_Item (Name : String; Value : BIT_Result;
      Reason : String) return Completed_BIT_Item;
   function Create_Completed_BIT (ID : IR.UCI_ID; Time : Long_Long_Integer;
      Value : BIT_Result; Reason : String; Items : Completed_BIT_Item_List)
      return Completed_BIT;
   function Create_Fault_Data (K, V, F, U : String) return Fault_Data;
   function Create_Ambiguity_Group (Tests, Components : UCI_ID_Vectors.Vector)
      return Fault_Ambiguity_Group;
   function Create_Fault (ID : IR.UCI_ID; Severity_Value : Fault_Severity;
      State_Value : Fault_State; Time : Long_Long_Integer; Code, Description : String;
      Data : Fault_Data_List; Components : UCI_ID_Vectors.Vector;
      Groups : Fault_Ambiguity_Group_List) return Fault;

   type BIT_Status is private;
   function Create_BIT_Status (Active : Active_BIT_List;
      Completed : Completed_BIT_List; Faults : Fault_List)
      return BIT_Status;
   function Active_BIT_Count (Value : BIT_Status) return Natural;
   function Active_BIT_At (Value : BIT_Status; Index : Positive) return Active_BIT;
   function Completed_BIT_Count (Value : BIT_Status) return Natural;
   function Completed_BIT_At (Value : BIT_Status; Index : Positive) return Completed_BIT;
   function Fault_Count (Value : BIT_Status) return Natural;
   function Fault_At (Value : BIT_Status; Index : Positive) return Fault;

   type State_Transition_Status is (Not_Set, Not_Transitioning, Shutting_Down, Transitioning);
   type Component_State is (Not_Set, Unknown, Not_Installed, Off, Initializing,
      Operational, Degraded, Disabled, Faulted);
   type Temperature_State is (Not_Set, Under_Temp, Normal, Over_Temp_Warning,
      Over_Temp_Degraded, Over_Temp_Shutdown);
   type Euler is record Roll, Pitch, Yaw : Long_Float := 0.0; end record;
   type Foreign_Key is private;
   function Create_Foreign_Key (Key, System_Name : String) return Foreign_Key;
   function Key (Value : Foreign_Key) return String;
   function System_Name (Value : Foreign_Key) return String;
   type Installation_Details is private;
   function Create_Installation_Details (Location : IR.Component_Location;
      Orientation, Boresight : Euler) return Installation_Details;
   function Location (Value : Installation_Details) return IR.Component_Location;
   function Orientation (Value : Installation_Details) return Euler;
   function Boresight (Value : Installation_Details) return Euler;
   type MFA_Component is private;
   function Create_MFA_Component (ID : IR.UCI_ID; State : Component_State;
      Temperature_C : Long_Float; Temperature_State_Value : Temperature_State;
      Installation_Location_ID : Foreign_Key;
      Details : Installation_Details) return MFA_Component;
   function Component_ID (Value : MFA_Component) return IR.UCI_ID;
   function State (Value : MFA_Component) return Component_State;
   function Temperature_C (Value : MFA_Component) return Long_Float;
   function Temperature_Status (Value : MFA_Component) return Temperature_State;
   function Installation_Location_ID (Value : MFA_Component) return Foreign_Key;
   function Installation (Value : MFA_Component) return Installation_Details;
   type MFA_Component_List is private;
   procedure Append (Values : in out MFA_Component_List; Value : MFA_Component);
   type About is private;
   function Create_About (Model, Serial_Number, Software_Version,
      Bootloader_Software_Version, Hardware_Version : String) return About;
   function Model (Value : About) return String;
   function Serial_Number (Value : About) return String;
   function Software_Version (Value : About) return String;
   function Bootloader_Software_Version (Value : About) return String;
   function Hardware_Version (Value : About) return String;
   type MFA_Status is private;
   function Create_MFA_Status (State : MFA_State; State_Description,
      Mode_Description : String; Transition : State_Transition_Status;
      About_Value : About; Components : MFA_Component_List) return MFA_Status;
   function State (Value : MFA_Status) return MFA_State;
   function State_Description (Value : MFA_Status) return String;
   function Mode_Description (Value : MFA_Status) return String;
   function Transition_Status (Value : MFA_Status) return State_Transition_Status;
   function About_Value (Value : MFA_Status) return About;
   function Component_Count (Value : MFA_Status) return Natural;
   function Component_At (Value : MFA_Status; Index : Positive) return MFA_Component;

   type Name_Value_Pair is private;
   function Create_Name_Value_Pair (Name, Value : String) return Name_Value_Pair;
   function Name (Value : Name_Value_Pair) return String;
   function Pair_Value (Value : Name_Value_Pair) return String;
   type Name_Value_Pair_List is private;
   procedure Append (Values : in out Name_Value_Pair_List; Value : Name_Value_Pair);
   subtype Discrete_Status is Name_Value_Pair_List;
   subtype MFA_Status_Detailed is Name_Value_Pair_List;
   function Pair_Count (Values : Name_Value_Pair_List) return Natural;
   function Pair_At (Values : Name_Value_Pair_List; Index : Positive) return Name_Value_Pair;

   type Security_Event_Kind is (None, Authentication, Integrity, File_Management,
      Key_Management, System, Sanitization);
   type Security_Outcome is (Not_Set, Failure, Success);
   type Security_Severity is (Not_Set, Critical, Error, Informational, Warning);
   type Security_Artifact is private;
   function Create_Security_Artifact (Component_ID, Associated_ID : IR.UCI_ID)
      return Security_Artifact;
   function Component_ID (Value : Security_Artifact) return IR.UCI_ID;
   function Associated_ID (Value : Security_Artifact) return IR.UCI_ID;
   type Security_Artifact_List is private;
   procedure Append (Values : in out Security_Artifact_List; Value : Security_Artifact);
   type Security_Event is private;
   function Create_Security_Event (Kind : Security_Event_Kind; Category : Natural;
      Details : String; Subsystem_ID, Service_ID, MDF_ID : IR.UCI_ID) return Security_Event;
   function Kind (Value : Security_Event) return Security_Event_Kind;
   function Category (Value : Security_Event) return Natural;
   function Details (Value : Security_Event) return String;
   function Subsystem_ID (Value : Security_Event) return IR.UCI_ID;
   function Service_ID (Value : Security_Event) return IR.UCI_ID;
   function MDF_ID (Value : Security_Event) return IR.UCI_ID;
   type Security_Audit_Record is private;
   function Create_Security_Audit_Record (Event_ID : IR.UCI_ID;
      Timestamp_NS : Long_Long_Integer; Subsystem_ID : IR.UCI_ID;
      Artifacts : Security_Artifact_List; Event : Security_Event;
      Outcome : Security_Outcome; Severity : Security_Severity)
      return Security_Audit_Record;
   function Event_ID (Value : Security_Audit_Record) return IR.UCI_ID;
   function Timestamp_NS (Value : Security_Audit_Record) return Long_Long_Integer;
   function Subsystem_ID (Value : Security_Audit_Record) return IR.UCI_ID;
   function Artifact_Count (Value : Security_Audit_Record) return Natural;
   function Artifact_At (Value : Security_Audit_Record; Index : Positive) return Security_Artifact;
   function Event (Value : Security_Audit_Record) return Security_Event;
   function Outcome (Value : Security_Audit_Record) return Security_Outcome;
   function Severity (Value : Security_Audit_Record) return Security_Severity;

private
   package US renames Ada.Strings.Unbounded;
   use type US.Unbounded_String;
   package String_Vectors is new Ada.Containers.Vectors (Positive, US.Unbounded_String);
   type BIT_Type is record ID : IR.UCI_ID; Interface_Value : BIT_Control_Interface := Not_Set;
      Duration : Long_Long_Integer := 0; Names : String_Vectors.Vector; Components : UCI_ID_Vectors.Vector; end record;
   package BIT_Type_Vectors is new Ada.Containers.Vectors (Positive, BIT_Type);
   type Active_BIT is record ID : IR.UCI_ID; Completion : Long_Long_Integer := 0; Percent : Long_Float := 0.0; end record;
   package Active_BIT_Vectors is new Ada.Containers.Vectors (Positive, Active_BIT);
   type Completed_BIT_Item is record Name, Why : US.Unbounded_String; Value : BIT_Result := Not_Set; end record;
   package Completed_BIT_Item_Vectors is new Ada.Containers.Vectors (Positive, Completed_BIT_Item);
   type Completed_BIT is record ID : IR.UCI_ID; Time : Long_Long_Integer := 0; Value : BIT_Result := Not_Set; Why : US.Unbounded_String; Items : Completed_BIT_Item_Vectors.Vector; end record;
   package Completed_BIT_Vectors is new Ada.Containers.Vectors (Positive, Completed_BIT);
   type Fault_Data is record K,V,F,U : US.Unbounded_String; end record;
   package Fault_Data_Vectors is new Ada.Containers.Vectors (Positive, Fault_Data);
   type Fault_Ambiguity_Group is record Tests,Components : UCI_ID_Vectors.Vector; end record;
   package Fault_Ambiguity_Group_Vectors is new Ada.Containers.Vectors (Positive, Fault_Ambiguity_Group);
   type Fault is record ID : IR.UCI_ID; Severity_Value : Fault_Severity := Not_Set; State_Value : Fault_State := Not_Set; Time : Long_Long_Integer := 0; Code,Description : US.Unbounded_String; Data : Fault_Data_Vectors.Vector; Components : UCI_ID_Vectors.Vector; Groups : Fault_Ambiguity_Group_Vectors.Vector; end record;
   package Fault_Vectors is new Ada.Containers.Vectors (Positive, Fault);
   type BIT_Type_List is record Values : BIT_Type_Vectors.Vector; end record;
   type Active_BIT_List is record Values : Active_BIT_Vectors.Vector; end record;
   type Completed_BIT_Item_List is record Values : Completed_BIT_Item_Vectors.Vector; end record;
   type Completed_BIT_List is record Values : Completed_BIT_Vectors.Vector; end record;
   type Fault_Data_List is record Values : Fault_Data_Vectors.Vector; end record;
   type Fault_Ambiguity_Group_List is record Values : Fault_Ambiguity_Group_Vectors.Vector; end record;
   type Fault_List is record Values : Fault_Vectors.Vector; end record;
   type BIT_Status is record Active : Active_BIT_Vectors.Vector; Completed : Completed_BIT_Vectors.Vector; Faults : Fault_Vectors.Vector; end record;
   type Foreign_Key is record K,S : US.Unbounded_String; end record;
   type Installation_Details is record L : IR.Component_Location; O,B : Euler; end record;
   type MFA_Component is record ID : IR.UCI_ID; State_Value : Component_State := Not_Set; Temp : Long_Float := 0.0; Temp_State : Temperature_State := Not_Set; Location_ID : Foreign_Key; Details : Installation_Details; end record;
   package MFA_Component_Vectors is new Ada.Containers.Vectors (Positive, MFA_Component);
   type MFA_Component_List is record Values : MFA_Component_Vectors.Vector; end record;
   type About is record M,S,SW,B,H : US.Unbounded_String; end record;
   type MFA_Status is record State_Value : MFA_State := Not_Set; State_Text,Mode_Text : US.Unbounded_String; Transition : State_Transition_Status := Not_Set; About_Data : About; Components : MFA_Component_Vectors.Vector; end record;
   type Name_Value_Pair is record N,V : US.Unbounded_String; end record;
   package Name_Value_Pair_Vectors is new Ada.Containers.Vectors (Positive, Name_Value_Pair);
   type Name_Value_Pair_List is record Values : Name_Value_Pair_Vectors.Vector; end record;
   type Security_Artifact is record Component,Associated : IR.UCI_ID; end record;
   package Security_Artifact_Vectors is new Ada.Containers.Vectors (Positive, Security_Artifact);
   type Security_Artifact_List is record Values : Security_Artifact_Vectors.Vector; end record;
   type Security_Event is record Event_Kind : Security_Event_Kind := None; Category_Value : Natural := 0; Text : US.Unbounded_String; Subsystem,Service,MDF : IR.UCI_ID; end record;
   type Security_Audit_Record is record ID : IR.UCI_ID; Time : Long_Long_Integer := 0; Subsystem : IR.UCI_ID; Artifacts : Security_Artifact_Vectors.Vector; Event_Data : Security_Event; Outcome_Value : Security_Outcome := Not_Set; Severity_Value : Security_Severity := Not_Set; end record;
end AMS.MEL.Status;
