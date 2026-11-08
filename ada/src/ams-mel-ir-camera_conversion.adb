with Interfaces;

package body AMS.MEL.IR.Camera_Conversion is
   package Cam renames AMS.MEL.IR.Camera;
   use type Interfaces.Unsigned_32;

   function To_Raw (Command : Cam.Camera_Command) return AMS.MEL_C_API.IR_Camera_Command_V1
   is ((Command_ID          => Interfaces.Unsigned_32 (Command.ID),
        System_Time_NS      => Interfaces.Integer_64 (Command.System_Time_NS),
        Image_Type          => Cam.Image_Type'Pos (Command.Kind),
        Width               => Interfaces.Unsigned_32 (Command.Width),
        Height              => Interfaces.Unsigned_32 (Command.Height),
        Frame_Rate_Hz       => Interfaces.Unsigned_32 (Command.Frame_Rate_Hz),
        Integration_Time_NS => Interfaces.Integer_64 (Command.Integration_Time_NS),
        Row_Offset          => Interfaces.Unsigned_32 (Command.Row_Offset),
        Column_Offset       => Interfaces.Unsigned_32 (Command.Column_Offset),
        Direction           => Cam.Scan_Direction'Pos (Command.Direction),
        Number_Of_Stages    => Interfaces.Unsigned_32 (Command.Number_Of_Stages)));

   function To_Response
     (Raw : AMS.MEL_C_API.IR_Camera_Command_Response_V1) return Cam.Camera_Command_Response is
   begin
      if Raw.Command_Success > 1 then
         raise Provider_Error with "native CameraCommandResp success flag is not Boolean";
      end if;
      return
        (ID              => Cam.Command_ID (Raw.Command_ID),
         Size            => Cam.Unsigned_32 (Raw.Size),
         Timestamp_NS    => Cam.Nanoseconds (Raw.Timestamp_NS),
         Command_Success => Raw.Command_Success = 1);
   end To_Response;
end AMS.MEL.IR.Camera_Conversion;
