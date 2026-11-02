with Interfaces;

package body AMS.MEL.IR.Camera_Protection_Conversion is
   package CP renames AMS.MEL.IR.Camera_Protection;
   use type Interfaces.Unsigned_32;

   function To_Raw
     (Command : CP.Protection_Command) return AMS.MEL_C_API.IR_Camera_Protect_Command_V1
   is ((Command_ID       => Interfaces.Unsigned_32 (Command.ID),
        Response_Time_NS => Interfaces.Integer_64 (Command.Response_Time_NS),
        Protect_Flag     => (if Command.Protect then 1 else 0)));

   function To_Response
     (Raw : AMS.MEL_C_API.IR_Camera_Protect_Response_V1) return CP.Protection_Response is
   begin
      if Raw.Command_Response > CP.Protection_Status'Pos (CP.Protection_Status'Last) then
         raise Provider_Error with "native CameraProtectCmdResp CmdResp value is unknown";
      end if;
      return
        (ID               => CP.Command_ID (Raw.Command_ID),
         Response_Time_NS => CP.Nanoseconds (Raw.Response_Time_NS),
         Status           => CP.Protection_Status'Val (Raw.Command_Response));
   end To_Response;
end AMS.MEL.IR.Camera_Protection_Conversion;
