with Interfaces;
with Interfaces.C;

package body AMS.MEL.IR.System_Track_Data_Conversion is
   package Sys renames AMS.MEL.IR.Track.System_Data;
   package C renames AMS.MEL_C_API;

   --  The established Unsigned_8 representation for published bool values.
   function Flag (Value : Boolean) return Interfaces.Unsigned_8
   is (if Value then 1 else 0);

   function Raw_Az_El (Value : Sys.Azimuth_Elevation) return C.Az_El_V1
   is (Azimuth_Rad   => Interfaces.C.double (Value.Azimuth_Rad),
       Elevation_Rad => Interfaces.C.double (Value.Elevation_Rad));

   function To_Raw
     (Value : Sys.System_Track_Data_Response) return C.IR_System_Track_Data_Response_V1
   is ((System_Time_NS       => Interfaces.Integer_64 (Value.System_Time_NS),
        Command_ID           => Value.Command_ID,
        Request_ID           => Value.Request_ID,
        Track_ID             => Value.Track_ID,
        Range_M              => Interfaces.C.double (Value.Range_M),
        Range_Rate_MPS       => Interfaces.C.double (Value.Range_Rate_MPS),
        Range_Error_M        => Interfaces.C.double (Value.Range_Error_M),
        Range_Rate_Error_MPS => Interfaces.C.double (Value.Range_Rate_Error_MPS),
        Az_El_Valid          => Flag (Value.Az_El_Valid),
        Range_Valid          => Flag (Value.Range_Valid),
        Inertial_Az_El       => Raw_Az_El (Value.Inertial_Az_El),
        Az_El_Error          => Raw_Az_El (Value.Az_El_Error)));
end AMS.MEL.IR.System_Track_Data_Conversion;
