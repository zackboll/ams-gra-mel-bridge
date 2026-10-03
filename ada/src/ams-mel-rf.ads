private with Ada.Containers.Vectors;
private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with Interfaces.C;
private with AMS.MEL_C_API;
with Interfaces;

package AMS.MEL.RF is
   type Data_MEL is limited private;
   function Open (Library_Path : String; Configuration : String) return Data_MEL;
   function Is_Open (Object : Data_MEL) return Boolean;
   procedure Close (Object : in out Data_MEL);
   function Query_Provider_Version (Object : Data_MEL) return AMS.MEL.Provider_Version;
   function Quantize_Duration
     (Data : Data_MEL; Femtoseconds : Interfaces.Integer_64) return Interfaces.Integer_64;

   --  Ada-owned point-in-time value; no native/provider handle survives return.
   type Physical_Data is private;
   function Snapshot_Physical_Data
     (Data : Data_MEL; Face_ID : Interfaces.Unsigned_32) return Physical_Data;
   function Antenna_Height_M (Value : Physical_Data) return Long_Float;
   function Antenna_Width_M (Value : Physical_Data) return Long_Float;
   function Lattice_Angle_Radians (Value : Physical_Data) return Long_Float;
   function Location_Offset_X_M (Value : Physical_Data) return Long_Float;
   function Location_Offset_Y_M (Value : Physical_Data) return Long_Float;
   function Location_Offset_Z_M (Value : Physical_Data) return Long_Float;
   function Orientation_Roll_Radians (Value : Physical_Data) return Long_Float;
   function Orientation_Pitch_Radians (Value : Physical_Data) return Long_Float;
   function Orientation_Yaw_Radians (Value : Physical_Data) return Long_Float;
   function Boresight_Roll_Radians (Value : Physical_Data) return Long_Float;
   function Boresight_Pitch_Radians (Value : Physical_Data) return Long_Float;
   function Boresight_Yaw_Radians (Value : Physical_Data) return Long_Float;
   function Location_Key (Value : Physical_Data) return String;
   function Location_System_Name (Value : Physical_Data) return String;

   type Job_Data_Format is mod 2**32 with Size => 32;
   Direct_INT8          : constant Job_Data_Format := 0;
   Direct_INT16         : constant Job_Data_Format := 1;
   Complex_INT8         : constant Job_Data_Format := 2;
   Complex_INT16        : constant Job_Data_Format := 3;
   AMS_VITA_Small       : constant Job_Data_Format := 4;
   AMS_VITA_Medium      : constant Job_Data_Format := 5;
   AMS_VITA_Large       : constant Job_Data_Format := 6;
   AMS_VITA_Extra_Large : constant Job_Data_Format := 7;
   PDW_Type1            : constant Job_Data_Format := 8;
   PDW_Type2            : constant Job_Data_Format := 9;
   PDW_Type3            : constant Job_Data_Format := 10;
   LF_Type1             : constant Job_Data_Format := 11;
   LF_Type2             : constant Job_Data_Format := 12;
   LF_Type3             : constant Job_Data_Format := 13;

   type Frequency_Range is record
      Min_Hz : Long_Float;
      Max_Hz : Long_Float;
   end record;

   type Face_Info is private;
   function Face_ID (Value : Face_Info) return Interfaces.Unsigned_32;
   function Supports_Receive (Value : Face_Info) return Boolean;
   function Supports_Transmit (Value : Face_Info) return Boolean;
   function Requires_Endpoint_Association (Value : Face_Info) return Boolean;
   function AGC_Processing_Time_FS (Value : Face_Info) return Interfaces.Integer_64;
   function Min_Job_Request_Lead_Time_FS (Value : Face_Info) return Interfaces.Integer_64;
   function Max_Job_Request_Lead_Time_FS (Value : Face_Info) return Interfaces.Integer_64;
   function Min_Job_Detail_Lead_Time_FS (Value : Face_Info) return Interfaces.Integer_64;
   function Tx_Rx_Switching_Time_FS (Value : Face_Info) return Interfaces.Integer_64;
   function Rx_Tx_Switching_Time_FS (Value : Face_Info) return Interfaces.Integer_64;
   function Tx_Tx_Switching_Time_FS (Value : Face_Info) return Interfaces.Integer_64;
   function Rx_Rx_Switching_Time_FS (Value : Face_Info) return Interfaces.Integer_64;
   function Rx_Frequency_Range_Count (Value : Face_Info) return Natural;
   function Rx_Frequency_Range_At (Value : Face_Info; Index : Positive) return Frequency_Range;
   function Tx_Frequency_Range_Count (Value : Face_Info) return Natural;
   function Tx_Frequency_Range_At (Value : Face_Info; Index : Positive) return Frequency_Range;
   function Sample_Frequency_Range_Count (Value : Face_Info) return Natural;
   function Sample_Frequency_Range_At (Value : Face_Info; Index : Positive) return Frequency_Range;

   type MFA_Info is private;
   function Snapshot_MFA_Info (Object : Data_MEL) return MFA_Info;
   function Reported_Num_Faces (Value : MFA_Info) return Interfaces.Unsigned_64;
   function Contains_Open_Additions (Value : MFA_Info) return Boolean;
   function Scheduler_Resolution_FS (Value : MFA_Info) return Interfaces.Integer_64;
   function Max_User_Defined_Context_Bytes (Value : MFA_Info) return Interfaces.Unsigned_64;
   function Supported_Data_Format_Count (Value : MFA_Info) return Natural;
   function Supported_Data_Format_At (Value : MFA_Info; Index : Positive) return Job_Data_Format;
   function Face_Count (Value : MFA_Info) return Natural;
   function Face_At (Value : MFA_Info; Index : Positive) return Face_Info;

private
   type Physical_Data is record
      Antenna_Height_M_Value          : Long_Float := 0.0;
      Antenna_Width_M_Value           : Long_Float := 0.0;
      Lattice_Angle_Radians_Value     : Long_Float := 0.0;
      Location_Offset_X_M_Value       : Long_Float := 0.0;
      Location_Offset_Y_M_Value       : Long_Float := 0.0;
      Location_Offset_Z_M_Value       : Long_Float := 0.0;
      Orientation_Roll_Radians_Value  : Long_Float := 0.0;
      Orientation_Pitch_Radians_Value : Long_Float := 0.0;
      Orientation_Yaw_Radians_Value   : Long_Float := 0.0;
      Boresight_Roll_Radians_Value    : Long_Float := 0.0;
      Boresight_Pitch_Radians_Value   : Long_Float := 0.0;
      Boresight_Yaw_Radians_Value     : Long_Float := 0.0;
      Key, System_Name                : Ada.Strings.Unbounded.Unbounded_String;
   end record;
   pragma
     Compile_Time_Error
       (Long_Float'Size < Interfaces.C.double'Size
          or else Long_Float'Digits < Interfaces.C.double'Digits,
        "RF frequency ranges require a lossless C double representation");
   package Range_Vectors is new Ada.Containers.Vectors (Positive, Frequency_Range);
   package Format_Vectors is new Ada.Containers.Vectors (Positive, Job_Data_Format);
   type Face_Info is record
      ID                                        : Interfaces.Unsigned_32 := 0;
      Receive_OK, Transmit_OK, Association      : Boolean := False;
      AGC, Min_Request, Max_Request, Min_Detail : Interfaces.Integer_64 := 0;
      Tx_Rx, Rx_Tx, Tx_Tx, Rx_Rx                : Interfaces.Integer_64 := 0;
      Rx_Ranges, Tx_Ranges, Sample_Ranges       : Range_Vectors.Vector;
   end record;
   package Face_Vectors is new Ada.Containers.Vectors (Positive, Face_Info);
   type MFA_Info is record
      Reported       : Interfaces.Unsigned_64 := 0;
      Open_Additions : Boolean := False;
      Resolution     : Interfaces.Integer_64 := 0;
      Context_Bytes  : Interfaces.Unsigned_64 := 0;
      Formats        : Format_Vectors.Vector;
      Faces          : Face_Vectors.Vector;
   end record;
   type Data_MEL is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_Data_Handle := AMS.MEL_C_API.Null_RF_Data;
   end record;
   overriding
   procedure Finalize (Object : in out Data_MEL);
end AMS.MEL.RF;
