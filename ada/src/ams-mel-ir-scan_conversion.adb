with Interfaces.C;

package body AMS.MEL.IR.Scan_Conversion is
   package C renames AMS.MEL_C_API;
   use AMS.MEL.IR.C2;
   function To_Raw (Value : AMS.MEL.IR.C2.Scan_Parameters) return C.IR_Scan_Param_V1
   is ((Elevation_Defined_With_Range_And_Altitude =>
          (if Value.Elevation_Defined_With_Range_And_Altitude then 1 else 0),
        Center_AZ_Rad                             => Interfaces.C.double (Value.Center_Azimuth_Rad),
        Center_EL_Rad                             =>
          Interfaces.C.double (Value.Center_Elevation_Rad),
        Center_Frame_Ref_EL                       =>
          Coordinate_Frame_Reference'Enum_Rep (Value.Center_Frame_Reference_EL),
        Center_Frame_Ref_AZ                       =>
          Coordinate_Frame_Reference'Enum_Rep (Value.Center_Frame_Reference_AZ),
        Scan_Width_Rad                            => Interfaces.C.double (Value.Scan_Width_Rad),
        Scan_Height_Rad                           => Interfaces.C.double (Value.Scan_Height_Rad),
        Scan_Type                                 =>
          (Interfaces.Unsigned_32 (Value.Continuous_Scan),
           Interfaces.Unsigned_32 (Value.Returning),
           Interfaces.Unsigned_32 (Value.Agile_Scan)),
        Scan_ID                                   => Interfaces.Unsigned_32 (Value.Scan_ID),
        Scan_Rate_Rad_Per_Second                  =>
          Interfaces.C.double (Value.Scan_Rate_Rad_Per_Second),
        Preferred_Revisit_Interval_Seconds        =>
          Interfaces.C.double (Value.Preferred_Revisit_Interval_Seconds),
        Required_Revisit_Interval_Seconds         =>
          Interfaces.C.double (Value.Required_Revisit_Interval_Seconds),
        Max_Range_Of_Interest_M                   =>
          Interfaces.Unsigned_32 (Value.Max_Range_Of_Interest_M),
        Min_Range_Of_Interest_M                   =>
          Interfaces.Unsigned_32 (Value.Min_Range_Of_Interest_M),
        Elevation_Scan_Center_Altitude_M          =>
          Interfaces.Unsigned_32 (Value.Elevation_Scan_Center_Altitude_M),
        Elevation_Scan_Center_Range_M             =>
          Interfaces.Unsigned_32 (Value.Elevation_Scan_Center_Range_M),
        Degradation_Method                        =>
          Degradation_Method'Enum_Rep (Value.Degradation)));

end AMS.MEL.IR.Scan_Conversion;
