with AMS.MEL_C_API;
with AMS.MEL.IR.Camera_Protection;

--  IRMEL-ADA-004 private conversions shared by the C2 CameraProtectCmd submission, the request
--  Wait and the opt-in C2 CameraProtectCmdResp owner.

private package AMS.MEL.IR.Camera_Protection_Conversion is
   function To_Raw
     (Command : AMS.MEL.IR.Camera_Protection.Protection_Command)
      return AMS.MEL_C_API.IR_Camera_Protect_Command_V1;
   --  Exact field-by-field conversion; Protect becomes exactly 1 or 0.
   function To_Response
     (Raw : AMS.MEL_C_API.IR_Camera_Protect_Response_V1)
      return AMS.MEL.IR.Camera_Protection.Protection_Response;
   --  Raises Provider_Error when command_response is not one of the three pinned alternatives;
   --  unknown values are never remapped.
end AMS.MEL.IR.Camera_Protection_Conversion;
