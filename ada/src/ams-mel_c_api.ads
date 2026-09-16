with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;

private package AMS.MEL_C_API is
   pragma SPARK_Mode (Off);

   type Version_V1 is record
      Major : Interfaces.Unsigned_32;
      Minor : Interfaces.Unsigned_32;
   end record
     with Convention => C;

   Success : constant Interfaces.Integer_32 := 0;
   Buffer_Too_Small : constant Interfaces.Integer_32 := 7;
   Timeout : constant Interfaces.Integer_32 := 9;
   Stream_Stopped : constant Interfaces.Integer_32 := 10;

   subtype Size_T is Interfaces.C.size_t;
   type Session_Handle is new System.Address;
   Null_Session : constant Session_Handle := Session_Handle (System.Null_Address);
   type Stream_Handle is new System.Address;
   Null_Stream : constant Stream_Handle := Stream_Handle (System.Null_Address);

   type Byte_Array_16 is array (0 .. 15) of Interfaces.Unsigned_8
     with Convention => C;
   type String_View_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record with Convention => C;
   type UCI_ID_V1 is record
      UUID              : Byte_Array_16;
      Descriptive_Label : String_View_V1;
   end record with Convention => C;
   type Component_Location_V1 is record
      Offset_X_M : Interfaces.C.double;
      Offset_Y_M : Interfaces.C.double;
      Offset_Z_M : Interfaces.C.double;
      Key         : String_View_V1;
      System_Name : String_View_V1;
   end record with Convention => C;
   type IR_Stream_Config_V1 is record
      Channel_Type   : Interfaces.Unsigned_32;
      Channel_ID     : UCI_ID_V1;
      Platform_ID    : UCI_ID_V1;
      Sensor_Location : Component_Location_V1;
      Buffer_Count   : Size_T;
      Buffer_Size    : Size_T;
      Queue_Capacity : Size_T;
   end record with Convention => C;

   type Reserved_Byte_Array is array (0 .. 6) of Interfaces.Unsigned_8
     with Convention => C;

   type IR_Frame_V1 is record
      System_Time_NS       : Interfaces.Integer_64;
      Integration_Time_NS  : Interfaces.Integer_64;
      Width                : Interfaces.Unsigned_32;
      Height               : Interfaces.Unsigned_32;
      Bits_Per_Pixel       : Interfaces.Unsigned_32;
      Number_Of_Bands      : Interfaces.Unsigned_32;
      Horizontal_FOV_Rad   : Interfaces.C.double;
      Vertical_FOV_Rad     : Interfaces.C.double;
      Pixel_Format         : Interfaces.Unsigned_32;
      Frame_ID             : Interfaces.Unsigned_32;
      Subframe_ID          : Interfaces.Unsigned_32;
      Subframe_Total       : Interfaces.Unsigned_32;
      Image_Type           : Interfaces.Unsigned_32;
      Image_Flip           : Interfaces.Unsigned_32;
      Image_Flags          : Interfaces.Unsigned_32;
      Dither_Row           : Interfaces.C.double;
      Dither_Column        : Interfaces.C.double;
      Row_Offset           : Interfaces.Unsigned_32;
      Column_Offset        : Interfaces.Unsigned_32;
      Band_Index           : Interfaces.Unsigned_8;
      Reserved             : Reserved_Byte_Array;
      Pixels               : System.Address;
      Pixel_Capacity       : Size_T;
      Pixel_Required       : Size_T;
   end record with Convention => C;

   type IR_Counters_V1 is record
      Frames_Received           : Interfaces.Unsigned_64;
      Frames_Dropped_Queue_Full : Interfaces.Unsigned_64;
      Malformed_Or_Unsupported  : Interfaces.Unsigned_64;
   end record with Convention => C;

   type Provider_Version_V1 is record
      API_Version          : Interfaces.Unsigned_32;
      Library_Version      : Interfaces.Unsigned_32;
      Vendor               : System.Address;
      Vendor_Capacity      : Size_T;
      Vendor_Required      : Size_T;
      Description          : System.Address;
      Description_Capacity : Size_T;
      Description_Required : Size_T;
   end record
     with Convention => C;

   function Get_ABI_Version
     (Output : access Version_V1) return Interfaces.Integer_32
     with Import,
          Convention    => C,
          External_Name => "ams_mel_get_abi_version";

   function Session_Open
     (Library_Path          : Interfaces.C.Strings.chars_ptr;
      Instance              : Interfaces.C.Strings.chars_ptr;
      Aperture_Config_ID    : Interfaces.C.Strings.chars_ptr;
      Output                : access Session_Handle;
      Diagnostic            : System.Address;
      Diagnostic_Capacity   : Size_T;
      Diagnostic_Required   : access Size_T) return Interfaces.Integer_32
     with Import, Convention => C, External_Name => "ams_mel_session_open";

   function Session_Get_Provider_Version
     (Handle                : Session_Handle;
      Output                : access Provider_Version_V1;
      Diagnostic            : System.Address;
      Diagnostic_Capacity   : Size_T;
      Diagnostic_Required   : access Size_T) return Interfaces.Integer_32
     with Import, Convention => C,
          External_Name => "ams_mel_session_get_provider_version";

   function Session_Close
     (Handle                : access Session_Handle;
      Diagnostic            : System.Address;
      Diagnostic_Capacity   : Size_T;
      Diagnostic_Required   : access Size_T) return Interfaces.Integer_32
     with Import, Convention => C, External_Name => "ams_mel_session_close";

   function IR_Stream_Open
     (Session               : Session_Handle;
      Config                : access IR_Stream_Config_V1;
      Output                : access Stream_Handle;
      Diagnostic            : System.Address;
      Diagnostic_Capacity   : Size_T;
      Diagnostic_Required   : access Size_T) return Interfaces.Integer_32
     with Import, Convention => C, External_Name => "ams_mel_ir_stream_open";
   function IR_Stream_Start
     (Stream                : Stream_Handle;
      Diagnostic            : System.Address;
      Diagnostic_Capacity   : Size_T;
      Diagnostic_Required   : access Size_T) return Interfaces.Integer_32
     with Import, Convention => C, External_Name => "ams_mel_ir_stream_start";
   function IR_Stream_Receive
     (Stream                : Stream_Handle;
      Timeout_MS            : Interfaces.Unsigned_32;
      Output                : access IR_Frame_V1;
      Diagnostic            : System.Address;
      Diagnostic_Capacity   : Size_T;
      Diagnostic_Required   : access Size_T) return Interfaces.Integer_32
     with Import, Convention => C, External_Name => "ams_mel_ir_stream_receive";
   function IR_Stream_Get_Counters
     (Stream                : Stream_Handle;
      Output                : access IR_Counters_V1;
      Diagnostic            : System.Address;
      Diagnostic_Capacity   : Size_T;
      Diagnostic_Required   : access Size_T) return Interfaces.Integer_32
     with Import, Convention => C,
          External_Name => "ams_mel_ir_stream_get_counters";
   function IR_Stream_Stop
     (Stream                : Stream_Handle;
      Diagnostic            : System.Address;
      Diagnostic_Capacity   : Size_T;
      Diagnostic_Required   : access Size_T) return Interfaces.Integer_32
     with Import, Convention => C, External_Name => "ams_mel_ir_stream_stop";
   function IR_Stream_Close
     (Stream                : access Stream_Handle;
      Diagnostic            : System.Address;
      Diagnostic_Capacity   : Size_T;
      Diagnostic_Required   : access Size_T) return Interfaces.Integer_32
     with Import, Convention => C, External_Name => "ams_mel_ir_stream_close";
end AMS.MEL_C_API;
