with Interfaces.C;
with System.Storage_Elements;

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

   pragma
     Compile_Time_Error
       (Long_Float'Size /= 64
          or else Long_Float'Machine_Radix /= 2
          or else Long_Float'Machine_Mantissa /= 53
          or else Long_Float'Machine_Emax /= 1024
          or else Interfaces.C.double'Size /= 64
          or else Interfaces.C.double'Machine_Radix /= 2
          or else Interfaces.C.double'Machine_Mantissa /= 53
          or else Interfaces.C.double'Machine_Emax /= 1024,
        "ScanParam reception requires binary64 Long_Float and C double");

   --  As with SchedCreatedRep, move representations without evaluating any
   --  floating scalar: numeric conversion or GNAT validity checks could
   --  otherwise change or reject a provider's NaN, infinity or signed zero.
   procedure Copy_Binary64 (Source, Destination : System.Address) is
      type Bytes is
        array (1 .. 64 / System.Storage_Unit) of System.Storage_Elements.Storage_Element;
      Input  : Bytes
      with Import, Address => Source;
      Output : Bytes
      with Import, Address => Destination;
   begin
      Output := Input;
   end Copy_Binary64;

   function From_Raw (Value : C.IR_Scan_Param_V1) return Scan_Parameters is
      use type Interfaces.Unsigned_32;
   begin
      if Value.Elevation_Defined_With_Range_And_Altitude > 1
        or else Value.Center_Frame_Ref_EL > Coordinate_Frame_Reference'Enum_Rep (Aircraft)
        or else Value.Center_Frame_Ref_AZ > Coordinate_Frame_Reference'Enum_Rep (Aircraft)
        or else Value.Degradation_Method > Degradation_Method'Enum_Rep (Revisit_Degradation)
      then
         raise Provider_Error with "invalid native ScanParam discriminator";
      end if;

      return Result : Scan_Parameters do
         Result.Elevation_Defined_With_Range_And_Altitude :=
           Value.Elevation_Defined_With_Range_And_Altitude = 1;
         Result.Center_Frame_Reference_EL :=
           Coordinate_Frame_Reference'Enum_Val (Value.Center_Frame_Ref_EL);
         Result.Center_Frame_Reference_AZ :=
           Coordinate_Frame_Reference'Enum_Val (Value.Center_Frame_Ref_AZ);
         Result.Continuous_Scan := Scan_Value (Value.Scan_Type.Continuous_Scan);
         Result.Returning := Scan_Value (Value.Scan_Type.Returning);
         Result.Agile_Scan := Scan_Value (Value.Scan_Type.Agile_Scan);
         Result.Scan_ID := Scan_Value (Value.Scan_ID);
         Result.Max_Range_Of_Interest_M := Scan_Value (Value.Max_Range_Of_Interest_M);
         Result.Min_Range_Of_Interest_M := Scan_Value (Value.Min_Range_Of_Interest_M);
         Result.Elevation_Scan_Center_Altitude_M :=
           Scan_Value (Value.Elevation_Scan_Center_Altitude_M);
         Result.Elevation_Scan_Center_Range_M := Scan_Value (Value.Elevation_Scan_Center_Range_M);
         Result.Degradation := Degradation_Method'Enum_Val (Value.Degradation_Method);
         Copy_Binary64 (Value.Center_AZ_Rad'Address, Result.Center_Azimuth_Rad'Address);
         Copy_Binary64 (Value.Center_EL_Rad'Address, Result.Center_Elevation_Rad'Address);
         Copy_Binary64 (Value.Scan_Width_Rad'Address, Result.Scan_Width_Rad'Address);
         Copy_Binary64 (Value.Scan_Height_Rad'Address, Result.Scan_Height_Rad'Address);
         Copy_Binary64
           (Value.Scan_Rate_Rad_Per_Second'Address, Result.Scan_Rate_Rad_Per_Second'Address);
         Copy_Binary64
           (Value.Preferred_Revisit_Interval_Seconds'Address,
            Result.Preferred_Revisit_Interval_Seconds'Address);
         Copy_Binary64
           (Value.Required_Revisit_Interval_Seconds'Address,
            Result.Required_Revisit_Interval_Seconds'Address);
      end return;
   end From_Raw;

end AMS.MEL.IR.Scan_Conversion;
