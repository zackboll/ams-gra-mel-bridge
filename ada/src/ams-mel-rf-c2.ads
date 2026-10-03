private with Ada.Finalization;
private with Ada.Containers.Vectors;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with AMS.MEL.IR;
with Interfaces;

package AMS.MEL.RF.C2 is
   type C2_MEL is tagged limited private;
   function Open (Library_Path : String; Configuration : String) return C2_MEL;
   function Is_Open (Object : C2_MEL) return Boolean;
   procedure Close (Object : in out C2_MEL);
   Timeout_Error                       : exception;
   type Virtual_Aperture_Config is private;
   function Create_Virtual_Aperture_Config
     (VA_Definition_ID        : Interfaces.Unsigned_32;
      Priority                : Interfaces.Unsigned_32;
      VA_Definition_File_Info : String) return Virtual_Aperture_Config;
   procedure Append_Local_Function_Info (Config : in out Virtual_Aperture_Config; Value : String);
   procedure Append_Capability_ID
     (Config : in out Virtual_Aperture_Config; Value : AMS.MEL.IR.UCI_ID);
   type Virtual_Aperture_Request is tagged limited private;
   function Submit_Virtual_Aperture
     (Parent : C2_MEL'Class; Config : Virtual_Aperture_Config) return Virtual_Aperture_Request;
   function Is_Open (Request : Virtual_Aperture_Request) return Boolean;
   procedure Close (Request : in out Virtual_Aperture_Request);
   type Request_Outcome is (Created, Failed);
   type Request_Error_Code is
     (None,
      Invalid_ID,
      Invalid_State,
      Invalid_Parameters,
      Insufficient_Permissions,
      Insufficient_Resources,
      Insufficient_Local_Resources,
      Insufficient_Remote_Resources,
      Unsupported);
   for Request_Error_Code use
     (None                          => 0,
      Invalid_ID                    => 1,
      Invalid_State                 => 2,
      Invalid_Parameters            => 3,
      Insufficient_Permissions      => 4,
      Insufficient_Resources        => 5,
      Insufficient_Local_Resources  => 6,
      Insufficient_Remote_Resources => 7,
      Unsupported                   => 8);
   type Virtual_Aperture_Result is private;
   function Outcome (Result : Virtual_Aperture_Result) return Request_Outcome;
   function Error_Code (Result : Virtual_Aperture_Result) return Request_Error_Code;
   function Description (Result : Virtual_Aperture_Result) return String;
   function Wait
     (Request : Virtual_Aperture_Request; Timeout_Milliseconds : Natural)
      return Virtual_Aperture_Result;
   type Virtual_Aperture is tagged limited private;
   function Claim (Request : Virtual_Aperture_Request'Class) return Virtual_Aperture;
   function Is_Open (Object : Virtual_Aperture) return Boolean;
   function VA_Instance_ID_Count (Object : Virtual_Aperture) return Natural;
   function VA_Instance_ID_At
     (Object : Virtual_Aperture; Index : Positive) return Interfaces.Unsigned_32;
   function Element_Group_Label_Count (Object : Virtual_Aperture) return Natural;
   function Element_Group_Label_At (Object : Virtual_Aperture; Index : Positive) return String;
   function Is_Single_Group (Object : Virtual_Aperture) return Boolean;
   procedure Close (Object : in out Virtual_Aperture);
   type RX_Element_Group_Config is private;
   function Create_RX_Element_Group
     (Label               : String;
      Desired_Duty_Factor : Long_Float := 1.0;
      Data_Pipe_Label     : String := "default") return RX_Element_Group_Config;
   procedure Append_Expected_Center_Frequency
     (Group : in out RX_Element_Group_Config; Min_Hz, Max_Hz : Long_Float);
   procedure Append_Endpoint_ID
     (Group : in out RX_Element_Group_Config; ID : Interfaces.Unsigned_64);
   type Job_Config is private;
   function Create_Job_Config
     (Request_ID, Priority       : Interfaces.Unsigned_32;
      Group                      : RX_Element_Group_Config;
      Precedence_Within_Priority : Interfaces.Unsigned_32 := 0;
      Interruptable              : Boolean := False) return Job_Config;
   procedure Append_Instance_Selection (Config : in out Job_Config; ID : Interfaces.Unsigned_32);
   type Job_Request is tagged limited private;
   function Submit_Job (VA : Virtual_Aperture'Class; Config : Job_Config) return Job_Request;
   function Is_Open (Request : Job_Request) return Boolean;
   procedure Close (Request : in out Job_Request);
   type Job_Result is private;
   function Outcome (Result : Job_Result) return Request_Outcome;
   function Error_Code (Result : Job_Result) return Request_Error_Code;
   function Description (Result : Job_Result) return String;
   function Wait (Request : Job_Request; Timeout_Milliseconds : Natural) return Job_Result;
   type Job is limited private;
   function Claim (Request : Job_Request'Class) return Job;
   function Is_Open (Object : Job) return Boolean;
   --  Matches RF MEL 762ce84: numeric_limits<Femtoseconds>::max() has
   --  count zero, unlike Femtoseconds::max(). This aliases an ordinary zero
   --  relative start; the scalar provider interface cannot distinguish intent.
   --  INT64_MAX is not the pinned continuation sentinel. No quantization or
   --  remapping is performed. Matching this value does not prove scheduling.
   Continue_From_Previous_Femtoseconds : constant Interfaces.Integer_64 := 0;
   type RX_Receive_Event_Config is private;
   function Create_RX_Receive_Event
     (Event_ID                    : Interfaces.Unsigned_32;
      Element_Group_Label         : String;
      Start_Femtoseconds          : Interfaces.Integer_64;
      Duration_Femtoseconds       : Interfaces.Integer_64;
      Center_Frequency_Hz         : Long_Float;
      Sample_Frequency_Hz         : Long_Float;
      AGC_Processing_Iterations   : Interfaces.Unsigned_64 := 0;
      Ignored_Post_AGC_Iterations : Interfaces.Unsigned_64 := 0;
      Max_Extension_Femtoseconds  : Interfaces.Integer_64 := 0) return RX_Receive_Event_Config;
   type RX_Job_Interval_Config is private;
   type Interval_Status_Enable is (Never, Always, On_Exception);
   for Interval_Status_Enable use (Never => 0, Always => 1, On_Exception => 2);
   procedure Set_Interval_Status_Enable
     (Interval : in out RX_Job_Interval_Config; Mode : Interval_Status_Enable);
   function Create_RX_Job_Interval
     (Interval_ID                        : Interfaces.Unsigned_32;
      Sequence_Duration_Femtoseconds     : Interfaces.Integer_64;
      Job_Details_ID                     : Interfaces.Unsigned_32 := 0;
      Interval_Start_Femtoseconds        : Interfaces.Integer_64 :=
        Continue_From_Previous_Femtoseconds;
      Interval_Starting_Gap_Femtoseconds : Interfaces.Integer_64 := 0;
      Sequence_Repeat_Count              : Interfaces.Unsigned_64 := 1;
      Calibration_Duration_Femtoseconds  : Interfaces.Integer_64 := 0;
      Interval_Ending_Gap_Femtoseconds   : Interfaces.Integer_64 := 0;
      Phase_Coherence_With_Prior         : Boolean := False;
      Iterations_Per_Signal              : Interfaces.Unsigned_64 := 0;
      Max_Data_Rate_BPS                  : Long_Float := 0.0;
      Max_Sample_Rate_Hz                 : Long_Float := 0.0) return RX_Job_Interval_Config;
   procedure Append_RX_Event
     (Interval : in out RX_Job_Interval_Config; Event : RX_Receive_Event_Config);
   type RX_Job_Interval_List is private;
   procedure Append_Job_Interval
     (Intervals : in out RX_Job_Interval_List; Interval : RX_Job_Interval_Config);
   function Job_Interval_Count (Intervals : RX_Job_Interval_List) return Natural;
   --  Synchronous, externally serialized with same-Job calls and Close.
   --  Add/Flush reject after Finalize or full Cancel has been attempted.
   --  Cancel_Remaining is repeatable, including during/after Finalize, until
   --  full Cancel has been attempted. Failures raise Provider_Error; no retry.
   procedure Add_RX_Job_Intervals (Object : in out Job; Intervals : RX_Job_Interval_List);
   procedure Flush_Job (Object : in out Job);
   procedure Cancel_Remaining_Job_Intervals (Object : in out Job);
   type Job_Status is
     (None, In_Progress, Complete, Failed_Invalid_ID, Failed_Interrupted, Failed_Invalid_State);
   for Job_Status use
     (None                 => 0,
      In_Progress          => 1,
      Complete             => 2,
      Failed_Invalid_ID    => 3,
      Failed_Interrupted   => 4,
      Failed_Invalid_State => 5);
   type Cancel_Error is (None);
   for Cancel_Error use (None => 0);
   type Cancel_Result is private;
   function Cancelled (Result : Cancel_Result) return Boolean;
   function Error_Code (Result : Cancel_Result) return Cancel_Error;
   procedure Finalize_Job (Object : in out Job);
   function Wait_Job_Status (Object : Job; Timeout_Milliseconds : Natural) return Job_Status;
   function Cancel_Job (Object : in out Job) return Cancel_Result;
   procedure Close (Object : in out Job);
   function Actual_Start_Seconds (Object : Job) return Interfaces.Integer_64;
   function Actual_Start_Femtoseconds (Object : Job) return Interfaces.Integer_64;
   function Total_Job_Duration_Femtoseconds (Object : Job) return Interfaces.Integer_64;
   function VA_Instance_ID (Object : Job) return Interfaces.Unsigned_32;
   function VA_Definition_ID (Object : Job) return Interfaces.Unsigned_32;
   function Job_Details_ID (Object : Job) return Interfaces.Unsigned_32;
   function Job_Request_ID (Object : Job) return Interfaces.Unsigned_32;
   function Lookahead_Femtoseconds (Object : Job) return Interfaces.Integer_64;
   function RX_Stream_ID_Count (Object : Job) return Natural;
   function RX_Stream_ID_At (Object : Job; Index : Positive) return Interfaces.Unsigned_32;
private
   pragma
     Compile_Time_Error
       (Job_Status'Enum_Rep (Failed_Invalid_State)
          /= Integer (AMS.MEL_C_API.RF_Job_Status_Failed_Invalid_State),
        "RF Job status representation mismatch");
   pragma
     Compile_Time_Error
       (Cancel_Error'Enum_Rep (None) /= Integer (AMS.MEL_C_API.RF_Cancel_Error_None),
        "RF CancelError representation mismatch");
   type RX_Receive_Event_Config is record
      Event_ID                                               : Interfaces.Unsigned_32;
      Label                                                  :
        Ada.Strings.Unbounded.Unbounded_String;
      Start_Femtoseconds, Duration_Femtoseconds              : Interfaces.Integer_64;
      Center_Frequency_Hz, Sample_Frequency_Hz               : Long_Float;
      AGC_Processing_Iterations, Ignored_Post_AGC_Iterations : Interfaces.Unsigned_64;
      Max_Extension_Femtoseconds                             : Interfaces.Integer_64;
   end record;
   package RX_Event_Vectors is new Ada.Containers.Vectors (Positive, RX_Receive_Event_Config);
   type RX_Job_Interval_Config is record
      Interval_ID, Job_Details_ID                  : Interfaces.Unsigned_32;
      Interval_Start_Femtoseconds,
      Interval_Starting_Gap_Femtoseconds,
      Sequence_Duration_Femtoseconds,
      Calibration_Duration_Femtoseconds,
      Interval_Ending_Gap_Femtoseconds             : Interfaces.Integer_64;
      Sequence_Repeat_Count, Iterations_Per_Signal : Interfaces.Unsigned_64;
      Phase_Coherence_With_Prior                   : Boolean;
      Max_Data_Rate_BPS, Max_Sample_Rate_Hz        : Long_Float;
      Events                                       : RX_Event_Vectors.Vector;
      Status_Enable                                : Interval_Status_Enable := Never;
   end record;
   package RX_Interval_Vectors is new Ada.Containers.Vectors (Positive, RX_Job_Interval_Config);
   type RX_Job_Interval_List is record
      Values : RX_Interval_Vectors.Vector;
   end record;
   type Cancel_Result is record
      Was_Cancelled : Boolean := False;
      Code          : Cancel_Error := None;
   end record;
   package Text_Vectors is new
     Ada.Containers.Vectors
       (Positive,
        Ada.Strings.Unbounded.Unbounded_String,
        Ada.Strings.Unbounded."=");
   package UCI_Vectors is new Ada.Containers.Vectors (Positive, AMS.MEL.IR.UCI_ID, AMS.MEL.IR."=");
   package ID_Vectors is new
     Ada.Containers.Vectors (Positive, Interfaces.Unsigned_32, Interfaces."=");
   package Endpoint_Vectors is new
     Ada.Containers.Vectors (Positive, Interfaces.Unsigned_64, Interfaces."=");
   type Frequency_Range is record
      Min_Hz, Max_Hz : Long_Float;
   end record;
   package Frequency_Vectors is new Ada.Containers.Vectors (Positive, Frequency_Range);
   type RX_Element_Group_Config is record
      Label, Pipe : Ada.Strings.Unbounded.Unbounded_String;
      Duty        : Long_Float;
      Frequencies : Frequency_Vectors.Vector;
      Endpoints   : Endpoint_Vectors.Vector;
   end record;
   type Job_Config is record
      ID, Priority, Precedence : Interfaces.Unsigned_32;
      Interruptable            : Boolean;
      Group                    : RX_Element_Group_Config;
      Instances                : ID_Vectors.Vector;
   end record;
   type Job_Result is record
      State : Request_Outcome := Created;
      Code  : Request_Error_Code := None;
      Text  : Ada.Strings.Unbounded.Unbounded_String;
   end record;
   type Job_Request is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_Job_Request_Handle := AMS.MEL_C_API.Null_RF_Job_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Job_Request);
   type Job is new Ada.Finalization.Limited_Controlled with record
      Handle                                                   :
        aliased AMS.MEL_C_API.RF_Job_Handle := AMS.MEL_C_API.Null_RF_Job;
      Start_Seconds, Start_Femtoseconds, Duration_Femtoseconds : Interfaces.Integer_64 := 0;
      Instance_ID, Definition_ID, Details_ID, Request_ID       : Interfaces.Unsigned_32 := 0;
      Lookahead                                                : Interfaces.Integer_64 := 0;
      Streams                                                  : ID_Vectors.Vector;
   end record;
   overriding
   procedure Finalize (Object : in out Job);
   type Virtual_Aperture_Config is record
      ID, Priority : Interfaces.Unsigned_32;
      File_Info    : Ada.Strings.Unbounded.Unbounded_String;
      Local        : Text_Vectors.Vector;
      Capabilities : UCI_Vectors.Vector;
   end record;
   type Virtual_Aperture_Result is record
      State : Request_Outcome := Created;
      Code  : Request_Error_Code := None;
      Text  : Ada.Strings.Unbounded.Unbounded_String;
   end record;
   type Virtual_Aperture_Request is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_VA_Request_Handle := AMS.MEL_C_API.Null_RF_VA_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Virtual_Aperture_Request);
   type Virtual_Aperture is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_VA_Handle := AMS.MEL_C_API.Null_RF_VA;
      IDs    : ID_Vectors.Vector;
      Labels : Text_Vectors.Vector;
      Single : Boolean := False;
   end record;
   overriding
   procedure Finalize (Object : in out Virtual_Aperture);
   type C2_MEL is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_C2_Handle := AMS.MEL_C_API.Null_RF_C2;
   end record;
   overriding
   procedure Finalize (Object : in out C2_MEL);
end AMS.MEL.RF.C2;
