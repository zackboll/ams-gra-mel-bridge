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
   --  TX JobRequest requirements only; this does not execute a TX interval.
   --  Power is exact provider-defined TxPowerLevel, not a TxPowerModeID.
   type TX_Element_Group_Config is private;
   function Create_TX_Element_Group
     (Label               : String;
      TX_Power_Level      : Interfaces.Unsigned_32;
      Desired_Duty_Factor : Long_Float := 1.0) return TX_Element_Group_Config;
   procedure Append_Expected_Center_Frequency
     (Group : in out TX_Element_Group_Config; Min_Hz, Max_Hz : Long_Float);
   function Create_RX_Element_Group
     (Label               : String;
      Desired_Duty_Factor : Long_Float := 1.0;
      Data_Pipe_Label     : String := "default") return RX_Element_Group_Config;
   procedure Append_Expected_Center_Frequency
     (Group : in out RX_Element_Group_Config; Min_Hz, Max_Hz : Long_Float);
   procedure Append_Endpoint_ID
     (Group : in out RX_Element_Group_Config; ID : Interfaces.Unsigned_64);
   --  Explicit pipe collections retain insertion order. Duplicate IDs within
   --  one collection raise Constraint_Error; empty collections are omitted.
   procedure Append_Endpoint_ID
     (Group           : in out RX_Element_Group_Config;
      Data_Pipe_Label : String;
      ID              : Interfaces.Unsigned_64);
   type UTC_Time is private;
   function Create_UTC_Time
     (Seconds, Fractional_Femtoseconds : Interfaces.Integer_64) return UTC_Time;
   function Seconds (Value : UTC_Time) return Interfaces.Integer_64;
   function Fractional_Femtoseconds (Value : UTC_Time) return Interfaces.Integer_64;
   --  Exact caller-selected coordinates: meters, meters/second and radians.
   --  No conversion, normalization, physical range or finite-value checks.
   type Pointing_Kind is (ECEF, LLA, Platform_Relative, Face_Relative, Baseline_Relative);
   type Pointing is private;
   function Create_ECEF_Pointing
     (Location_X_M, Location_Y_M, Location_Z_M       : Long_Float;
      Velocity_X_MPS, Velocity_Y_MPS, Velocity_Z_MPS : Long_Float;
      Time_Of_Validity                               : UTC_Time) return Pointing;
   function Create_LLA_Pointing
     (Latitude_Rad, Longitude_Rad, Altitude_M                  : Long_Float;
      Velocity_North_MPS, Velocity_East_MPS, Velocity_Down_MPS : Long_Float;
      Time_Of_Validity                                         : UTC_Time) return Pointing;
   function Create_Platform_Relative_Pointing
     (Azimuth_Rad, Elevation_Rad : Long_Float) return Pointing;
   function Create_Face_Relative_Pointing (Azimuth_Rad, Elevation_Rad : Long_Float) return Pointing;
   function Create_Baseline_Relative_Pointing (Conic_Rad : Long_Float) return Pointing;
   procedure Append_Expected_Pointing (Group : in out RX_Element_Group_Config; Value : Pointing);
   type Byte_Array is array (Natural range <>) of Interfaces.Unsigned_8;
   type Unsigned_32_Array is array (Natural range <>) of Interfaces.Unsigned_32;
   type Job_Config is private;
   procedure Set_Estimated_Stab_Point (Config : in out Job_Config; Value : Pointing);
   procedure Clear_Estimated_Stab_Point (Config : in out Job_Config);
   function Create_Job_Config
     (Request_ID, Priority       : Interfaces.Unsigned_32;
      Group                      : RX_Element_Group_Config;
      Precedence_Within_Priority : Interfaces.Unsigned_32 := 0;
      Interruptable              : Boolean := False) return Job_Config;
   procedure Append_Instance_Selection (Config : in out Job_Config; ID : Interfaces.Unsigned_32);
   function Create_Job_Config
     (Request_ID, Priority       : Interfaces.Unsigned_32;
      Group                      : TX_Element_Group_Config;
      Precedence_Within_Priority : Interfaces.Unsigned_32 := 0;
      Interruptable              : Boolean := False) return Job_Config;
   --  Append calls define one global order across RX/TX, including duplicates.
   procedure Append_TX_Element_Group (Config : in out Job_Config; Group : TX_Element_Group_Config);
   procedure Append_RX_Element_Group (Config : in out Job_Config; Group : RX_Element_Group_Config);
   procedure Set_Min_Start_Time (Config : in out Job_Config; Value : UTC_Time);
   procedure Set_Max_Complete_Time (Config : in out Job_Config; Value : UTC_Time);
   procedure Set_Duration_Femtoseconds (Config : in out Job_Config; Value : Interfaces.Integer_64);
   procedure Set_Capability_ID (Config : in out Job_Config; Value : Byte_Array);
   procedure Set_Activity_ID (Config : in out Job_Config; Value : Byte_Array);
   procedure Set_TX_Power_Mode_IDs (Config : in out Job_Config; Value : Unsigned_32_Array);
   procedure Set_Lookahead_Femtoseconds (Config : in out Job_Config; Value : Interfaces.Integer_64);
   --  Create_Job_Config inserts Group first and retains the historical upstream
   --  defaults: zero times/duration/lookahead and [0] identity/power-mode IDs.
   --  Instance selection is a vector; TX power modes are serialized as a set.
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
   type Pulse_Threshold_Reference is (DBQ, DB_Above_Noise, DB_Below_Saturation);
   for Pulse_Threshold_Reference use (DBQ => 0, DB_Above_Noise => 1, DB_Below_Saturation => 2);
   type Pulse_Time_Tag_Threshold is (At_50_Percent, At_90_Percent);
   for Pulse_Time_Tag_Threshold use (At_50_Percent => 0, At_90_Percent => 1);
   type Pulse_Detection_Settings is private;
   function Create_Pulse_Detection_Settings
     (Reference                                    : Pulse_Threshold_Reference;
      Leading_M, Leading_N, Trailing_M, Trailing_N : Interfaces.Unsigned_8;
      Min_Pulse_Width_Femtoseconds                 : Interfaces.Integer_64;
      Time_Tag_Threshold                           : Pulse_Time_Tag_Threshold)
      return Pulse_Detection_Settings;
   procedure Append_Pulse_Detection_Threshold
     (Settings : in out Pulse_Detection_Settings; Leading_Edge_DB, Trailing_Edge_DB : Long_Float);
   type RX_Receive_Event_Config is private;
   procedure Set_Pulse_Detection_Settings
     (Event : in out RX_Receive_Event_Config; Settings : Pulse_Detection_Settings);
   procedure Clear_Pulse_Detection_Settings (Event : in out RX_Receive_Event_Config);
   type Execution_Type is (Normal, Conditional);
   for Execution_Type use (Normal => 0, Conditional => 1);
   type Event_Termination_Type is (Inhibit_Event, Cancel_Event);
   for Event_Termination_Type use (Inhibit_Event => 0, Cancel_Event => 1);
   --  Ordered copied Stokes values, no normalization or finite checks. A third
   --  append raises Constraint_Error. Provider semantic validation remains final.
   procedure Append_Polarization
     (Event : in out RX_Receive_Event_Config; S0, S1, S2, S3 : Long_Float);
   procedure Set_Polarization_Beam_Steer_Correction
     (Event : in out RX_Receive_Event_Config; Enabled : Boolean);
   procedure Set_Phase_Offset_Radians (Event : in out RX_Receive_Event_Config; Value : Long_Float);
   procedure Set_Event_Execution_Type
     (Event : in out RX_Receive_Event_Config; Value : Execution_Type);
   procedure Set_Event_Termination_Type
     (Event : in out RX_Receive_Event_Config; Value : Event_Termination_Type);
   procedure Set_Allow_Delay_Start (Event : in out RX_Receive_Event_Config; Enabled : Boolean);
   procedure Set_Iteration_Hold_Count
     (Event : in out RX_Receive_Event_Config; Count : Interfaces.Unsigned_64);
   procedure Set_Iteration_Termination_Count
     (Event : in out RX_Receive_Event_Config; Count : Interfaces.Unsigned_64);
   procedure Set_Channelization_Enabled (Event : in out RX_Receive_Event_Config; Enabled : Boolean);
   --  Portable provider indices, not labels. No local count/range relationship
   --  is inferred. Applicable group order and duplicates are preserved.
   procedure Set_Stab_Point_Index
     (Event : in out RX_Receive_Event_Config; Index : Interfaces.Unsigned_64);
   procedure Append_Applicable_RX_Element_Group
     (Event : in out RX_Receive_Event_Config; Element_Group_Index : Interfaces.Unsigned_64);
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
   procedure Set_Interval_TX_Power_Mode_ID
     (Interval : in out RX_Job_Interval_Config; Value : Interfaces.Unsigned_32);
   procedure Set_Interval_Activity_ID
     (Interval : in out RX_Job_Interval_Config; Value : Byte_Array);
   procedure Set_Interval_Execution_Type
     (Interval : in out RX_Job_Interval_Config; Value : Execution_Type);
   procedure Append_Stab_Point (Interval : in out RX_Job_Interval_Config; Value : Pointing);
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
   --  Repeatable, including before/during/after Finalize, until full Cancel
   --  has been attempted (even False or an exception). No status stream is
   --  required. Normal return means only the provider's void method returned
   --  without throwing, not acceptance, effective extension or notification.
   --  Exact signed femtoseconds; no validation, arithmetic or quantization.
   --  Never automatically retries. Explicit retries after failure require
   --  provider-specific knowledge because mutation may precede an exception.
   --  Same-Job calls/Close are externally serialized; snapshot is unchanged.
   procedure Extend_Job_Event
     (Object                      : in out Job;
      Interval_ID                 : Interfaces.Unsigned_32;
      Event_ID                    : Interfaces.Unsigned_32;
      Added_Duration_Femtoseconds : Interfaces.Integer_64);
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
       (Execution_Type'Enum_Rep (Normal) /= Integer (AMS.MEL_C_API.RF_Execution_Normal),
        "RF execution representation mismatch");
   pragma
     Compile_Time_Error
       (Execution_Type'Enum_Rep (Conditional) /= Integer (AMS.MEL_C_API.RF_Execution_Conditional),
        "RF execution representation mismatch");
   pragma
     Compile_Time_Error
       (Event_Termination_Type'Enum_Rep (Inhibit_Event)
          /= Integer (AMS.MEL_C_API.RF_Event_Termination_Inhibit),
        "RF execution representation mismatch");
   pragma
     Compile_Time_Error
       (Event_Termination_Type'Enum_Rep (Cancel_Event)
          /= Integer (AMS.MEL_C_API.RF_Event_Termination_Cancel),
        "RF execution representation mismatch");
   pragma
     Compile_Time_Error
       (Interval_Status_Enable'Enum_Rep (Never) /= Integer (AMS.MEL_C_API.Rf_Interval_Status_Never),
        "RF reporting enum representation mismatch");
   pragma
     Compile_Time_Error
       (Interval_Status_Enable'Enum_Rep (Always)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Status_Always),
        "RF reporting enum representation mismatch");
   pragma
     Compile_Time_Error
       (Interval_Status_Enable'Enum_Rep (On_Exception)
          /= Integer (AMS.MEL_C_API.Rf_Interval_Status_On_Exception),
        "RF reporting enum representation mismatch");
   pragma
     Compile_Time_Error
       (Job_Status'Enum_Rep (Failed_Invalid_State)
          /= Integer (AMS.MEL_C_API.RF_Job_Status_Failed_Invalid_State),
        "RF Job status representation mismatch");
   pragma
     Compile_Time_Error
       (Cancel_Error'Enum_Rep (None) /= Integer (AMS.MEL_C_API.RF_Cancel_Error_None),
        "RF CancelError representation mismatch");
   type UTC_Time is record
      Seconds, Fraction : Interfaces.Integer_64 := 0;
   end record;
   type Pointing_Components is array (Positive range 1 .. 6) of Long_Float;
   type Pointing is record
      Kind       : Pointing_Kind := Face_Relative;
      Components : Pointing_Components := [others => 0.0];
      Time       : UTC_Time;
   end record;
   package Pointing_Vectors is new Ada.Containers.Vectors (Positive, Pointing);
   package RX_Index_Vectors is new
     Ada.Containers.Vectors (Positive, Interfaces.Unsigned_64, Interfaces."=");
   type Stokes_Components is array (Positive range 1 .. 4) of Long_Float;
   package Stokes_Vectors is new Ada.Containers.Vectors (Positive, Stokes_Components);
   package Byte_Vectors is new
     Ada.Containers.Vectors (Positive, Interfaces.Unsigned_8, Interfaces."=");
   type Pulse_Threshold is record
      Leading_Edge_DB, Trailing_Edge_DB : Long_Float;
   end record;
   package Pulse_Threshold_Vectors is new Ada.Containers.Vectors (Positive, Pulse_Threshold);
   type Pulse_Detection_Settings is record
      Reference                                    : Pulse_Threshold_Reference := DBQ;
      Leading_M, Leading_N, Trailing_M, Trailing_N : Interfaces.Unsigned_8 := 0;
      Min_Pulse_Width_Femtoseconds                 : Interfaces.Integer_64 := 0;
      Time_Tag_Threshold                           : Pulse_Time_Tag_Threshold := At_50_Percent;
      Thresholds                                   : Pulse_Threshold_Vectors.Vector;
   end record;
   type RX_Receive_Event_Config is record
      Event_ID                                               : Interfaces.Unsigned_32;
      Label                                                  :
        Ada.Strings.Unbounded.Unbounded_String;
      Start_Femtoseconds, Duration_Femtoseconds              : Interfaces.Integer_64;
      Center_Frequency_Hz, Sample_Frequency_Hz               : Long_Float;
      AGC_Processing_Iterations, Ignored_Post_AGC_Iterations : Interfaces.Unsigned_64;
      Max_Extension_Femtoseconds                             : Interfaces.Integer_64;
      Stab_Point_Index                                       : Interfaces.Unsigned_64 := 0;
      Applicable_RX_Element_Groups                           : RX_Index_Vectors.Vector;
      Polarization                                           : Stokes_Vectors.Vector;
      Beam_Correction                                        : Boolean := False;
      Phase_Offset                                           : Long_Float := 0.0;
      Execution                                              : Execution_Type := Normal;
      Termination                                            : Event_Termination_Type :=
        Inhibit_Event;
      Allow_Delay                                            : Boolean := False;
      Hold_Count, Termination_Count                          : Interfaces.Unsigned_64 := 0;
      Channelization                                         : Boolean := False;
      Has_Pulse_Settings                                     : Boolean := False;
      Pulse_Settings                                         : Pulse_Detection_Settings;
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
      Stab_Points                                  : Pointing_Vectors.Vector;
      TX_Power_Mode                                : Interfaces.Unsigned_32 := 0;
      Activity                                     : Byte_Vectors.Vector;
      Execution                                    : Execution_Type := Normal;
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
   type Pipe_Config is record
      Label     : Ada.Strings.Unbounded.Unbounded_String;
      Endpoints : Endpoint_Vectors.Vector;
   end record;
   package Pipe_Vectors is new Ada.Containers.Vectors (Positive, Pipe_Config);
   type RX_Element_Group_Config is record
      Label, Pipe : Ada.Strings.Unbounded.Unbounded_String;
      Duty        : Long_Float;
      Frequencies : Frequency_Vectors.Vector;
      Pipes       : Pipe_Vectors.Vector;
      Pointings   : Pointing_Vectors.Vector;
   end record;
   type TX_Element_Group_Config is record
      Label       : Ada.Strings.Unbounded.Unbounded_String;
      Power       : Interfaces.Unsigned_32;
      Duty        : Long_Float;
      Frequencies : Frequency_Vectors.Vector;
   end record;
   type Group_Mode is (RX, TX);
   type Job_Group (Mode : Group_Mode := RX) is record
      case Mode is
         when RX =>
            Receive_Group : RX_Element_Group_Config;

         when TX =>
            Transmit_Group : TX_Element_Group_Config;
      end case;
   end record;
   package Group_Vectors is new Ada.Containers.Vectors (Positive, Job_Group);
   type Job_Config is record
      ID, Priority, Precedence : Interfaces.Unsigned_32;
      Interruptable            : Boolean;
      Groups                   : Group_Vectors.Vector;
      Instances                : ID_Vectors.Vector;
      Min_Start, Max_Complete  : UTC_Time;
      Duration, Lookahead      : Interfaces.Integer_64 := 0;
      Capability, Activity     : Byte_Vectors.Vector;
      Power_Modes              : ID_Vectors.Vector;
      Has_Estimated_Point      : Boolean := False;
      Estimated_Point          : Pointing;
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
