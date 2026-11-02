with AMS.MEL_C_API;

package body AMS.MEL.IR.Camera_Protection.Testing is
   procedure Check_Representation is
      package C renames AMS.MEL_C_API;
      Command : constant C.IR_Camera_Protect_Command_V1 :=
        (Command_ID => 0, Response_Time_NS => 0, Protect_Flag => 0);
      Result  : constant C.IR_Camera_Protect_Result_V1 :=
        (Response   => (Command_ID => 0, Response_Time_NS => 0, Command_Response => 0),
         Error_Code => 0);
   begin
      --  Compiled C test_ir_camera_protect static assertions fix these sizes and offsets.
      if C.IR_Camera_Protect_Command_V1'Object_Size /= 192
        or else Command.Response_Time_NS'Position /= 8
        or else Command.Protect_Flag'Position /= 16
        or else C.IR_Camera_Protect_Response_V1'Object_Size /= 192
        or else Result.Response.Response_Time_NS'Position /= 8
        or else Result.Response.Command_Response'Position /= 16
        or else Result.Error_Code'Position /= 24
        or else C.IR_Camera_Protect_Result_V1'Object_Size /= 256
        or else Protection_Status'Pos (Protection_OK) /= 0
        or else Protection_Status'Pos (Protection_Unavailable) /= 1
        or else Protection_Status'Pos (Protection_Unsupported) /= 2
        or else Nanoseconds'Size /= 64
      then
         raise Program_Error with "CameraProtectCmd C/Ada layout mismatch";
      end if;
   end Check_Representation;
end AMS.MEL.IR.Camera_Protection.Testing;
