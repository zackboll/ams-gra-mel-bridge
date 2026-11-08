with AMS.MEL_C_API;
with AMS.MEL.IR.Camera;

--  IRMEL-ADA-003 private conversions shared by the C2 and Image CameraCommand submissions,
--  the shared request Wait, and the opt-in Image CameraCommandResp owner.

private package AMS.MEL.IR.Camera_Conversion is
   function To_Raw
     (Command : AMS.MEL.IR.Camera.Camera_Command) return AMS.MEL_C_API.IR_Camera_Command_V1;
   --  Exact field-by-field conversion; only the two meaningful ImageType and ScanDir values exist.
   function To_Response
     (Raw : AMS.MEL_C_API.IR_Camera_Command_Response_V1)
      return AMS.MEL.IR.Camera.Camera_Command_Response;
   --  Raises Provider_Error when command_success is neither 0 nor 1.
end AMS.MEL.IR.Camera_Conversion;
