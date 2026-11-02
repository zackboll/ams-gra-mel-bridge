with AMS.MEL_C_API;
with AMS.MEL.IR.Calibration_Configuration;

private package AMS.MEL.IR.Calibration_Configuration_Conversion is
   function To_Raw
     (Command : Calibration_Configuration.Configuration_Command)
      return AMS.MEL_C_API.IR_Calibration_Configuration_Command_V1;
   function To_Collection
     (Raw : AMS.MEL_C_API.IR_Calibration_Configuration_V1)
      return Calibration_Configuration.Calibration_Collection;
   --  Validate pointer/count/enum representations before returning any owned result.
end AMS.MEL.IR.Calibration_Configuration_Conversion;
