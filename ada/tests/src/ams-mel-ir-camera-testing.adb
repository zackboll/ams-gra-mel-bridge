with AMS.MEL_C_API;

package body AMS.MEL.IR.Camera.Testing is
   procedure Check_Representation is
      package C renames AMS.MEL_C_API;
      Command : constant C.IR_Camera_Command_V1 :=
        (Command_ID          => 0,
         System_Time_NS      => 0,
         Image_Type          => 0,
         Width               => 0,
         Height              => 0,
         Frame_Rate_Hz       => 0,
         Integration_Time_NS => 0,
         Row_Offset          => 0,
         Column_Offset       => 0,
         Direction           => 0,
         Number_Of_Stages    => 0);
      Result  : constant C.IR_Camera_Command_Result_V1 :=
        (Response   => (Command_ID => 0, Size => 0, Timestamp_NS => 0, Command_Success => 0),
         Error_Code => 0);
   begin
      --  Compiled C test_ir_camera_command static assertions fix these sizes and offsets.
      if C.IR_Camera_Command_V1'Object_Size /= 448
        or else Command.System_Time_NS'Position /= 8
        or else Command.Image_Type'Position /= 16
        or else Command.Integration_Time_NS'Position /= 32
        or else Command.Direction'Position /= 48
        or else Command.Number_Of_Stages'Position /= 52
        or else C.IR_Camera_Command_Response_V1'Object_Size /= 192
        or else Result.Response.Timestamp_NS'Position /= 8
        or else Result.Response.Command_Success'Position /= 16
        or else Result.Error_Code'Position /= 24
        or else C.IR_Camera_Command_Result_V1'Object_Size /= 256
        or else Image_Type'Pos (Scanning) /= 1
        or else Scan_Direction'Pos (Positive_To_Negative_Azimuth) /= 1
      then
         raise Program_Error with "CameraCommand C/Ada layout mismatch";
      end if;
   end Check_Representation;
end AMS.MEL.IR.Camera.Testing;
