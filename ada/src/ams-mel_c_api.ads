with Interfaces;

private package AMS.MEL_C_API is
   pragma SPARK_Mode (Off);

   type Version_V1 is record
      Major : Interfaces.Unsigned_32;
      Minor : Interfaces.Unsigned_32;
   end record
     with Convention => C;

   Success : constant Interfaces.Integer_32 := 0;

   function Get_ABI_Version
     (Output : access Version_V1) return Interfaces.Integer_32
     with Import,
          Convention    => C,
          External_Name => "ams_mel_get_abi_version";
end AMS.MEL_C_API;
