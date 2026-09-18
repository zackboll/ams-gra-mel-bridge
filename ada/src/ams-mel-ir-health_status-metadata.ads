private with Ada.Containers.Vectors;
private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with AMS.MEL.Status;
with Interfaces;

package AMS.MEL.IR.Health_Status.Metadata is
   package Status renames AMS.MEL.Status;

   type Metadata_Stream is limited private;
   function Open (Channel : Health_Channel; Queue_Capacity : Positive := 16)
      return Metadata_Stream;
   function Is_Open (Stream : Metadata_Stream) return Boolean;

   type Failure_Level is (NA, Critical, Major, Parametric, Informational,
      Available, Not_Present);
   type CSCI_Mode is (Unknown, Unused, Initialization, Maintenance, Idle,
      Operational, VSA, Quick_Look, Track, Imaging, Noise);
   type Subsystem_Dependency is private;
   function Subsystem_ID (Value : Subsystem_Dependency) return Interfaces.Unsigned_32;
   function Criticality (Value : Subsystem_Dependency) return Interfaces.Unsigned_32;
   function Failure (Value : Subsystem_Dependency) return Failure_Level;
   type Version is record
      Source, Major_Revision, Minor_Revision, Engineering_Revision : Interfaces.Unsigned_32;
   end record;
   type Subsystem_CSCI is private;
   function CSCI_Name (Value : Subsystem_CSCI) return String;
   function Mode (Value : Subsystem_CSCI) return CSCI_Mode;
   function Version_Value (Value : Subsystem_CSCI) return Version;
   function Criticality (Value : Subsystem_CSCI) return Interfaces.Unsigned_32;
   function Failure (Value : Subsystem_CSCI) return Failure_Level;
   function BIT_Report (Value : Subsystem_CSCI) return Interfaces.Unsigned_32;
   function Connection_Established (Value : Subsystem_CSCI) return Boolean;
   type Subsystem_Status is private;
   function Subsystem_ID (Value : Subsystem_Status) return Interfaces.Unsigned_32;
   function Criticality (Value : Subsystem_Status) return Interfaces.Unsigned_32;
   function Status_Sequence_Number (Value : Subsystem_Status) return Interfaces.Unsigned_32;
   function Failure (Value : Subsystem_Status) return Failure_Level;
   function Reported_Subsystem_Count (Value : Subsystem_Status) return Interfaces.Unsigned_32;
   function Subsystem_Count (Value : Subsystem_Status) return Natural;
   function Subsystem_At (Value : Subsystem_Status; Index : Positive)
      return Subsystem_Dependency;
   function Reported_CSCI_Count (Value : Subsystem_Status) return Interfaces.Unsigned_32;
   function CSCI_Count (Value : Subsystem_Status) return Natural;
   function CSCI_At (Value : Subsystem_Status; Index : Positive) return Subsystem_CSCI;

   type Metadata_Kind is (MFA_Status_Event, BIT_Status_Event,
      Subsystem_Status_Event, Discrete_Status_Event, Security_Audit_Event,
      MFA_Status_Detailed_Event);
   type Metadata_Event (Event_Kind : Metadata_Kind := MFA_Status_Event) is private;
   function Kind (Event : Metadata_Event) return Metadata_Kind;
   function MFA_Status_Value (Event : Metadata_Event) return Status.MFA_Status
      with Pre => Kind (Event) = MFA_Status_Event;
   function BIT_Status_Value (Event : Metadata_Event) return Status.BIT_Status
      with Pre => Kind (Event) = BIT_Status_Event;
   function Subsystem_Status_Value (Event : Metadata_Event) return Subsystem_Status
      with Pre => Kind (Event) = Subsystem_Status_Event;
   function Discrete_Status_Value (Event : Metadata_Event) return Status.Discrete_Status
      with Pre => Kind (Event) = Discrete_Status_Event;
   function Security_Audit_Value (Event : Metadata_Event)
      return Status.Security_Audit_Record
      with Pre => Kind (Event) = Security_Audit_Event;
   function MFA_Status_Detailed_Value (Event : Metadata_Event)
      return Status.MFA_Status_Detailed
      with Pre => Kind (Event) = MFA_Status_Detailed_Event;

   function Receive (Stream : Metadata_Stream; Timeout_Milliseconds : Natural := 0)
      return Metadata_Event;
   type Metadata_Counters is record
      Events_Received, Events_Dropped_Queue_Full,
      Malformed_Or_Unsupported : Counter;
   end record;
   function Counters (Stream : Metadata_Stream) return Metadata_Counters;
   procedure Close (Stream : in out Metadata_Stream);

private
   package US renames Ada.Strings.Unbounded;
   type Subsystem_Dependency is record
      ID, Importance : Interfaces.Unsigned_32;
      Failure_Value : Failure_Level := NA;
   end record;
   package Dependency_Vectors is new Ada.Containers.Vectors
      (Positive, Subsystem_Dependency);
   type Subsystem_CSCI is record
      Name : US.Unbounded_String;
      Mode_Value : CSCI_Mode := Unknown;
      Version_Data : Version := (others => 0);
      Importance : Interfaces.Unsigned_32 := 0;
      Failure_Value : Failure_Level := NA;
      Report : Interfaces.Unsigned_32 := 0;
      Connected : Boolean := False;
   end record;
   package CSCI_Vectors is new Ada.Containers.Vectors (Positive, Subsystem_CSCI);
   type Subsystem_Status is record
      ID, Importance, Sequence : Interfaces.Unsigned_32 := 0;
      Failure_Value : Failure_Level := NA;
      Reported_Subsystems : Interfaces.Unsigned_32 := 0;
      Subsystems : Dependency_Vectors.Vector;
      Reported_CSCI : Interfaces.Unsigned_32 := 0;
      CSCIs : CSCI_Vectors.Vector;
   end record;
   type Metadata_Event (Event_Kind : Metadata_Kind := MFA_Status_Event) is record
      case Event_Kind is
         when MFA_Status_Event => MFA : Status.MFA_Status;
         when BIT_Status_Event => BIT : Status.BIT_Status;
         when Subsystem_Status_Event => Subsystem : Subsystem_Status;
         when Discrete_Status_Event => Discrete : Status.Discrete_Status;
         when Security_Audit_Event => Security : Status.Security_Audit_Record;
         when MFA_Status_Detailed_Event => Detailed : Status.MFA_Status_Detailed;
      end case;
   end record;
   type Metadata_Stream is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Health_Metadata_Handle :=
         AMS.MEL_C_API.Null_Health_Metadata;
   end record;
   overriding procedure Finalize (Stream : in out Metadata_Stream);
end AMS.MEL.IR.Health_Status.Metadata;
