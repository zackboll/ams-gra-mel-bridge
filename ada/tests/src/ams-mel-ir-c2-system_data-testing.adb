with AMS.MEL_C_API;
with System;

package body AMS.MEL.IR.C2.System_Data.Testing is
   --  The distinct C2 result mirrors the 40-byte C record (status 0, code 32)
   --  and both shared enumerations keep their exact native values.
   procedure Check_Representation is
      package C renames AMS.MEL_C_API;
      Raw : constant C.IR_C2_System_Response_Result_V1 :=
        (Status => (0, 0, 0, (System.Null_Address, 0)), Error_Code => 0);
   begin
      if C.IR_C2_System_Response_Result_V1'Object_Size /= 320
        or else Raw.Status'Position /= 0
        or else Raw.Error_Code'Position /= 32
        or else C.IR_System_Track_Data_Response_V1'Object_Size /= 768
        or else Command_State'Enum_Rep (AMS.MEL.IR.Track.System_Data.Cancelled) /= 4
        or else Cannot_Comply'Enum_Rep (AMS.MEL.IR.Track.System_Data.Alignment_Maneuver) /= 46
        or else Cannot_Comply'Pos (Cannot_Comply'Last) /= 46
      then
         raise Program_Error with "C2 SystemTrackDataResponse C/Ada layout mismatch";
      end if;
   end Check_Representation;
end AMS.MEL.IR.C2.System_Data.Testing;
