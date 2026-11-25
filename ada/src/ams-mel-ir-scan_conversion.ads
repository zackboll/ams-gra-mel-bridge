with AMS.MEL.IR.C2;
with AMS.MEL_C_API;

private package AMS.MEL.IR.Scan_Conversion is
   function To_Raw (Value : AMS.MEL.IR.C2.Scan_Parameters) return AMS.MEL_C_API.IR_Scan_Param_V1;
   function From_Raw (Value : AMS.MEL_C_API.IR_Scan_Param_V1) return AMS.MEL.IR.C2.Scan_Parameters;
end AMS.MEL.IR.Scan_Conversion;
