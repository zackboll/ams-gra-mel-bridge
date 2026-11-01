with Interfaces;
with AMS.MEL_C_API;
with AMS.MEL.IR.NUC;

--  IRMEL-ADA-002 private support shared by the Image and Health NUC owners.

private package AMS.MEL.IR.NUC_Conversion is
   procedure Check_Open
     (Code : Interfaces.Integer_32; Registration : Interfaces.Unsigned_32; Message : String);
   --  Raises NUC.Not_Supported for a NotSupported registration, else Provider_Error.
   function Receive
     (Handle : AMS.MEL_C_API.NUC_Metadata_Handle; Timeout_Milliseconds : Natural)
      return AMS.MEL.IR.NUC.NUC_Temp_Data;
   function Counters
     (Handle : AMS.MEL_C_API.NUC_Metadata_Handle) return AMS.MEL_C_API.Metadata_Counters_V1;
   procedure Close (Handle : aliased in out AMS.MEL_C_API.NUC_Metadata_Handle);
end AMS.MEL.IR.NUC_Conversion;
