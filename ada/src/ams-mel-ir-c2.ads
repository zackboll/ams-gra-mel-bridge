private with Ada.Finalization;
private with Ada.Strings.Unbounded;
with Ada.Containers.Indefinite_Vectors;
private with AMS.MEL_C_API;

package AMS.MEL.IR.C2 is
   type Control_Config is private;
   function Create_Config
     (Channel_ID      : UCI_ID;
      Platform_ID     : UCI_ID;
      Sensor_Location : Component_Location) return Control_Config;

   type Control_Channel is limited private;
   function Open (Parent : Session; Config : Control_Config)
     return Control_Channel;
   function Is_Open (Channel : Control_Channel) return Boolean;
   --  Enable, submit, and Close calls for one
   --  Control_Channel must be externally serialized. Parent Session close
   --  rules are unchanged.
   procedure Enable (Channel : in out Control_Channel);

   type Command_ID is mod 2 ** 32 with Size => 32;
   type BIT_ID is mod 2 ** 32 with Size => 32;
   type BIT_ID_Array is array (Positive range <>) of BIT_ID;
   package Fault_Code_Vectors is new Ada.Containers.Indefinite_Vectors
     (Index_Type => Positive, Element_Type => String);

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

   type Coordinate_Frame_Reference is (Inertial, Aircraft);
   for Coordinate_Frame_Reference use (Inertial => 0, Aircraft => 1);
   for Coordinate_Frame_Reference'Size use 32;
   type Degradation_Method is
     (Capacity_Degradation, Volume_Degradation, Range_Degradation,
      Revisit_Degradation);
   for Degradation_Method use
     (Capacity_Degradation => 0, Volume_Degradation => 1,
      Range_Degradation => 2, Revisit_Degradation => 3);
   for Degradation_Method'Size use 32;
   type Scan_Value is mod 2 ** 32 with Size => 32;
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
   Default_Scan_Parameters : constant Scan_Parameters := (others => <>);

   type Mode_Request is limited private;
   type MFA_Mode is (Unused, Task_Sched, Scan_Volume_Sched, Scan_Bar_Sched);
   function Submit_Mode
     (Channel         : Control_Channel;
      ID              : Command_ID;
      State           : MFA_State;
      Mode            : MFA_Mode;
      Scan_Parameters : AMS.MEL.IR.C2.Scan_Parameters := Default_Scan_Parameters)
      return Mode_Request;
   function Submit_Operate
     (Channel : Control_Channel; ID : Command_ID := 0) return Mode_Request;
   function Is_Open (Request : Mode_Request) return Boolean;

   type Return_Request is limited private;
   function Submit_BIT_No_Op
     (Channel : Control_Channel; ID : Command_ID := 0) return Return_Request;
   function Is_Open (Request : Return_Request) return Boolean;
   --  These safe operations enforce the upstream one-choice BIT rule. Each
   --  payload-specific operation requires a nonempty owned Ada collection.
   function Submit_BIT_Initiate
     (Channel : Control_Channel; IDs : BIT_ID_Array; ID : Command_ID := 0)
      return Return_Request with Pre => IDs'Length > 0;
   function Submit_BIT_Cancel
     (Channel : Control_Channel; IDs : BIT_ID_Array; ID : Command_ID := 0)
      return Return_Request with Pre => IDs'Length > 0;
   function Submit_BIT_Clear_Faults
     (Channel : Control_Channel; Fault_Codes : Fault_Code_Vectors.Vector;
      ID : Command_ID := 0) return Return_Request
      with Pre => not Fault_Codes.Is_Empty;
   type System_Time_Nanoseconds is range -(2 ** 63) .. (2 ** 63) - 1
     with Size => 64;
   function Submit_Config_Set
     (Channel : Control_Channel; ID : Command_ID := 0;
      System_Time_NS : System_Time_Nanoseconds := 0; Config : String := "")
      return Return_Request;

   type Outcome is (Success, Rejected);
   type Error_Code is
     (None, Invalid_ID, Invalid_State, Invalid_Parameters,
      Insufficient_Permissions, Insufficient_Resources,
      Insufficient_Local_Resources, Insufficient_Remote_Resources,
      Unsupported);
   type Mode_Result is private;
   function Status (Result : Mode_Result) return Outcome;
   function Mode (Result : Mode_Result) return MFA_Mode
     with Pre => Status (Result) = Success;
   function Rejection_Code (Result : Mode_Result) return Error_Code
     with Pre => Status (Result) = Rejected;
   function Description (Result : Mode_Result) return String
     with Pre => Status (Result) = Rejected;
   type Command_Return is
     (Return_Success, Bad_Pointer, Fail, Not_Supported, Not_Implemented);
   --  Success means RequestFor<Return> completed with a Command_Return value;
   --  that value may be Fail or any other enumerator above. Rejected represents
   --  upstream ErrorOr(Error), so Success with Value = Fail is not a facade or
   --  provider failure.
   type Return_Result is private;
   function Status (Result : Return_Result) return Outcome;
   function Value (Result : Return_Result) return Command_Return
     with Pre => Status (Result) = Success;
   function Rejection_Code (Result : Return_Result) return Error_Code
     with Pre => Status (Result) = Rejected;
   function Description (Result : Return_Result) return String
     with Pre => Status (Result) = Rejected;

   --  Timeout_Error is inherited from AMS.MEL.IR. Timeout never cancels or
   --  consumes either request type; Wait may be repeated and a later Wait may
   --  return its cached terminal result. Close must not race Wait on the same
   --  Mode_Request or on the same Return_Request.
   function Wait
     (Request : Mode_Request; Timeout_Milliseconds : Natural) return Mode_Result;
   function Wait
     (Request : Return_Request; Timeout_Milliseconds : Natural) return Return_Result;

   procedure Close (Request : in out Mode_Request);
   procedure Close (Request : in out Return_Request);
   procedure Close (Channel : in out Control_Channel);

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
   overriding procedure Finalize (Channel : in out Control_Channel);
   type Request_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Mode_Request_Handle :=
        AMS.MEL_C_API.Null_Mode_Request;
   end record;
   overriding procedure Finalize (Request : in out Request_Owner);
   type Mode_Request is limited record
      Owner : Request_Owner;
   end record;
   type Return_Request_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Return_Request_Handle :=
        AMS.MEL_C_API.Null_Return_Request;
   end record;
   overriding procedure Finalize (Request : in out Return_Request_Owner);
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
