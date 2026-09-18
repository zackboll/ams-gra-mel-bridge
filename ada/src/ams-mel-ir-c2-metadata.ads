private with Ada.Containers.Vectors;
private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with AMS.MEL.IR.Channel;

package AMS.MEL.IR.C2.Metadata is
   type Metadata_Stream is limited private;
   function Open (Channel : Control_Channel; Queue_Capacity : Positive := 16)
      return Metadata_Stream;
   function Is_Open (Stream : Metadata_Stream) return Boolean;

   procedure Enable_Comms_Test_Events (Stream : in out Metadata_Stream);
   type Metadata_Kind is (Command_Status_Event, BIT_Configuration_Event,
      BIT_Status_Event, Channel_Comms_Test_Event);
   type Command_State is (Not_Set, Received, Accepted, Rejected, Cancelled);
   for Command_State use (Not_Set => 0, Received => 1, Accepted => 2, Rejected => 3, Cancelled => 4);
   for Command_State'Size use 32;
   type Cannot_Comply is (Not_Set, Constraint_Attempts, Constraint_Endurance,
      Constraint_Classification, Constraint_FOR_FOV_Limit, Constraint_Gating,
      Constraint_Maneuver_Limit, Constraint_Op, Constraint_Occlusion,
      Capability_Range, Capability_Performance, Constraint_RF, Constraint_Route,
      Constraint_Safety, Constraint_Target_Angle, Constraint_Time,
      Constraint_System, Infeasible_Route, Mission_Event, State_Or_Settings,
      State_Or_Settings_Change, System_Unavailable, System_Fault, System_Conflict,
      Subsystem_Unavailable, Subsystem_Fault, Capability_Fault,
      Capability_Precedence, Capability_Unavailable, Insufficient_Resources,
      Ranking, Weather, Ineligible_Control_Source, Dependency_Predecessor,
      Dependency_All_Or_Nothing, Dependency_Either_Or, Init_Criteria_Not_Met,
      Unknown_ID, Invalid_Input_Parameter, Input_Other, MDF_Activation_Error,
      Multiple, Cancelled, Other, Unknown, Aborted, Alignment_Maneuver);
   for Cannot_Comply use
     (Not_Set => 0,
      Constraint_Attempts => 1,
      Constraint_Endurance => 2,
      Constraint_Classification => 3,
      Constraint_FOR_FOV_Limit => 4,
      Constraint_Gating => 5,
      Constraint_Maneuver_Limit => 6,
      Constraint_Op => 7,
      Constraint_Occlusion => 8,
      Capability_Range => 9,
      Capability_Performance => 10,
      Constraint_RF => 11,
      Constraint_Route => 12,
      Constraint_Safety => 13,
      Constraint_Target_Angle => 14,
      Constraint_Time => 15,
      Constraint_System => 16,
      Infeasible_Route => 17,
      Mission_Event => 18,
      State_Or_Settings => 19,
      State_Or_Settings_Change => 20,
      System_Unavailable => 21,
      System_Fault => 22,
      System_Conflict => 23,
      Subsystem_Unavailable => 24,
      Subsystem_Fault => 25,
      Capability_Fault => 26,
      Capability_Precedence => 27,
      Capability_Unavailable => 28,
      Insufficient_Resources => 29,
      Ranking => 30,
      Weather => 31,
      Ineligible_Control_Source => 32,
      Dependency_Predecessor => 33,
      Dependency_All_Or_Nothing => 34,
      Dependency_Either_Or => 35,
      Init_Criteria_Not_Met => 36,
      Unknown_ID => 37,
      Invalid_Input_Parameter => 38,
      Input_Other => 39,
      MDF_Activation_Error => 40,
      Multiple => 41,
      Cancelled => 42,
      Other => 43,
      Unknown => 44,
      Aborted => 45,
      Alignment_Maneuver => 46);
   for Cannot_Comply'Size use 32;
   type BIT_Control_Interface is (Not_Set, Subsystem_BIT_Command,
      Subsystem_State_Command, Subsystem_Initiated);
   for BIT_Control_Interface use (Not_Set => 0, Subsystem_BIT_Command => 1,
      Subsystem_State_Command => 2, Subsystem_Initiated => 3);
   for BIT_Control_Interface'Size use 32;
   type BIT_Result is (Not_Set, Pass, Fail, Interrupted, Not_Tested);
   for BIT_Result use (Not_Set => 0, Pass => 1, Fail => 2, Interrupted => 3, Not_Tested => 4);
   for BIT_Result'Size use 32;
   type Fault_Severity is (Not_Set, Nominal, Caution, Warning, Failed);
   for Fault_Severity use (Not_Set => 0, Nominal => 1, Caution => 2, Warning => 3, Failed => 4);
   for Fault_Severity'Size use 32;
   type Fault_State is (Not_Set, Set, Cleared, Unknown);
   for Fault_State use (Not_Set => 0, Set => 1, Cleared => 2, Unknown => 3);
   for Fault_State'Size use 32;

   type Command_Status is private;
   function Command_ID (Value : Command_Status) return C2.Command_ID;
   function State (Value : Command_Status) return Command_State;
   function Reason (Value : Command_Status) return Cannot_Comply;
   function Reason_Description (Value : Command_Status) return String;

   type BIT_Type is private;
   function BIT_ID (Value : BIT_Type) return UCI_ID;
   function Accepted_Interface (Value : BIT_Type) return BIT_Control_Interface;
   function Expected_Duration_NS (Value : BIT_Type) return Long_Long_Integer;
   function BIT_Item_Name_Count (Value : BIT_Type) return Natural;
   function BIT_Item_Name_At (Value : BIT_Type; Index : Positive) return String;
   function Subsystem_Component_Count (Value : BIT_Type) return Natural;
   function Subsystem_Component_At (Value : BIT_Type; Index : Positive) return UCI_ID;

   type Active_BIT is private;
   function BIT_ID (Value : Active_BIT) return UCI_ID;
   function Estimated_Completion_Time_NS (Value : Active_BIT) return Long_Long_Integer;
   function Estimated_Percent_Complete (Value : Active_BIT) return Long_Float;
   type Completed_BIT_Item is private;
   function BIT_Item_Name (Value : Completed_BIT_Item) return String;
   function Result (Value : Completed_BIT_Item) return BIT_Result;
   function Fail_Reason (Value : Completed_BIT_Item) return String;
   type Completed_BIT is private;
   function BIT_ID (Value : Completed_BIT) return UCI_ID;
   function Time_Tag_NS (Value : Completed_BIT) return Long_Long_Integer;
   function Result (Value : Completed_BIT) return BIT_Result;
   function Fail_Reason (Value : Completed_BIT) return String;
   function BIT_Item_Count (Value : Completed_BIT) return Natural;
   function BIT_Item_At (Value : Completed_BIT; Index : Positive) return Completed_BIT_Item;

   type Fault_Data is private;
   function Key (Value : Fault_Data) return String;
   function Data_Value (Value : Fault_Data) return String;
   function Format (Value : Fault_Data) return String;
   function Units (Value : Fault_Data) return String;
   type Fault_Ambiguity_Group is private;
   function Diagnostic_Test_Count (Value : Fault_Ambiguity_Group) return Natural;
   function Diagnostic_Test_At (Value : Fault_Ambiguity_Group; Index : Positive) return UCI_ID;
   function Component_Count (Value : Fault_Ambiguity_Group) return Natural;
   function Component_At (Value : Fault_Ambiguity_Group; Index : Positive) return UCI_ID;
   type Fault is private;
   function Fault_ID (Value : Fault) return UCI_ID;
   function Severity (Value : Fault) return Fault_Severity;
   function State (Value : Fault) return Fault_State;
   function Detection_Time_NS (Value : Fault) return Long_Long_Integer;
   function Fault_Code (Value : Fault) return String;
   function Fault_Description (Value : Fault) return String;
   function Fault_Data_Count (Value : Fault) return Natural;
   function Fault_Data_At (Value : Fault; Index : Positive) return Fault_Data;
   function Component_Count (Value : Fault) return Natural;
   function Component_At (Value : Fault; Index : Positive) return UCI_ID;
   function Ambiguity_Group_Count (Value : Fault) return Natural;
   function Ambiguity_Group_At (Value : Fault; Index : Positive) return Fault_Ambiguity_Group;

   type Metadata_Event is private;
   function Receive (Stream : Metadata_Stream; Timeout_Milliseconds : Natural := 0)
      return Metadata_Event;
   function Kind (Event : Metadata_Event) return Metadata_Kind;
   function Command (Event : Metadata_Event) return Command_Status
      with Pre => Kind (Event) = Command_Status_Event;
   function BIT_Type_Count (Event : Metadata_Event) return Natural
      with Pre => Kind (Event) = BIT_Configuration_Event;
   function BIT_Type_At (Event : Metadata_Event; Index : Positive) return BIT_Type
      with Pre => Kind (Event) = BIT_Configuration_Event;
   function Active_BIT_Count (Event : Metadata_Event) return Natural
      with Pre => Kind (Event) = BIT_Status_Event;
   function Active_BIT_At (Event : Metadata_Event; Index : Positive) return Active_BIT
      with Pre => Kind (Event) = BIT_Status_Event;
   function Completed_BIT_Count (Event : Metadata_Event) return Natural
      with Pre => Kind (Event) = BIT_Status_Event;
   function Completed_BIT_At (Event : Metadata_Event; Index : Positive) return Completed_BIT
      with Pre => Kind (Event) = BIT_Status_Event;
   function Fault_Count (Event : Metadata_Event) return Natural
      with Pre => Kind (Event) = BIT_Status_Event;
   function Fault_At (Event : Metadata_Event; Index : Positive) return Fault
      with Pre => Kind (Event) = BIT_Status_Event;
   function Comms_Test (Event : Metadata_Event) return IR.Channel.Comms_Test_Report
      with Pre => Kind (Event) = Channel_Comms_Test_Event;

   type Metadata_Counters is record
      Events_Received, Events_Dropped_Queue_Full,
      Malformed_Or_Unsupported : Counter;
   end record;
   function Counters (Stream : Metadata_Stream) return Metadata_Counters;
   procedure Close (Stream : in out Metadata_Stream);

private
   package US renames Ada.Strings.Unbounded;
   use type US.Unbounded_String;
   package String_Vectors is new Ada.Containers.Vectors (Positive, US.Unbounded_String);
   package ID_Vectors is new Ada.Containers.Vectors (Positive, UCI_ID);
   type Command_Status is record ID : C2.Command_ID := 0; Status : Command_State := Not_Set; Why : Cannot_Comply := Not_Set; Description : US.Unbounded_String; end record;
   type BIT_Type is record ID : UCI_ID; Interface_Value : BIT_Control_Interface := Not_Set; Duration : Long_Long_Integer := 0; Names : String_Vectors.Vector; Components : ID_Vectors.Vector; end record;
   package BIT_Type_Vectors is new Ada.Containers.Vectors (Positive, BIT_Type);
   type Active_BIT is record ID : UCI_ID; Completion : Long_Long_Integer := 0; Percent : Long_Float := 0.0; end record;
   package Active_Vectors is new Ada.Containers.Vectors (Positive, Active_BIT);
   type Completed_BIT_Item is record Name, Why : US.Unbounded_String; Value : BIT_Result := Not_Set; end record;
   package Item_Vectors is new Ada.Containers.Vectors (Positive, Completed_BIT_Item);
   type Completed_BIT is record ID : UCI_ID; Time : Long_Long_Integer := 0; Value : BIT_Result := Not_Set; Why : US.Unbounded_String; Items : Item_Vectors.Vector; end record;
   package Completed_Vectors is new Ada.Containers.Vectors (Positive, Completed_BIT);
   type Fault_Data is record K, V, F, U : US.Unbounded_String; end record;
   package Data_Vectors is new Ada.Containers.Vectors (Positive, Fault_Data);
   type Fault_Ambiguity_Group is record Tests, Components : ID_Vectors.Vector; end record;
   package Group_Vectors is new Ada.Containers.Vectors (Positive, Fault_Ambiguity_Group);
   type Fault is record ID : UCI_ID; Severity_Value : Fault_Severity := Not_Set; State_Value : Fault_State := Not_Set; Time : Long_Long_Integer := 0; Code, Description : US.Unbounded_String; Data : Data_Vectors.Vector; Components : ID_Vectors.Vector; Groups : Group_Vectors.Vector; end record;
   package Fault_Vectors is new Ada.Containers.Vectors (Positive, Fault);
   type Metadata_Event is record Event_Kind : Metadata_Kind := Command_Status_Event; Status : Command_Status; BIT_Types : BIT_Type_Vectors.Vector; Active : Active_Vectors.Vector; Completed : Completed_Vectors.Vector; Faults : Fault_Vectors.Vector; Comms : IR.Channel.Comms_Test_Report := (0, 0); end record;
   type Metadata_Stream is new Ada.Finalization.Limited_Controlled with record Handle : aliased AMS.MEL_C_API.Metadata_Handle := AMS.MEL_C_API.Null_Metadata; end record;
   overriding procedure Finalize (Stream : in out Metadata_Stream);
end AMS.MEL.IR.C2.Metadata;
