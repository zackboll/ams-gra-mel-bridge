with AMS.MEL_C_API;
with AMS.MEL.IR.Calibration_Status;

private package AMS.MEL.IR.Calibration_Status_Conversion is
   function To_Raw
     (Command : Calibration_Status.Status_Command)
      return AMS.MEL_C_API.IR_Calibration_Status_Command_V1;
   function To_Value
     (Raw : AMS.MEL_C_API.IR_Calibration_Status_V1) return Calibration_Status.Status_Value;
   --  Validate every pointer/count/string/enum representation before returning any owned result.
end AMS.MEL.IR.Calibration_Status_Conversion;
