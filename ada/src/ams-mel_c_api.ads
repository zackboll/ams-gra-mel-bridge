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

   subtype Size_T is Interfaces.C.size_t;
   type Session_Handle is new System.Address;
   Null_Session : constant Session_Handle := Session_Handle (System.Null_Address);

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
end AMS.MEL_C_API;
