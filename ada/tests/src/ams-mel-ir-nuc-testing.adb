with AMS.MEL_C_API;
with Interfaces;

package body AMS.MEL.IR.NUC.Testing is
   procedure Check_Representation is
      package C renames AMS.MEL_C_API;
      Point : constant C.IR_Temp_Point_V1 :=
        (Timestamp_NS  => 0,
         Frame_Band    => (Kind => 0, Min_Wavelength_M => 0.0, Max_Wavelength_M => 0.0),
         Pixel_Value   => (Kind => 0, Value => 0),
         Temperature_K => 0.0,
         Point_Type    => Interfaces.Unsigned_32'(0));
   begin
      --  Compiled C test_ir_nuc static assertions fix these LP64 sizes and offsets.
      if C.Size_T'Size = 64 then
         if C.IR_Pixel_Value_V1'Object_Size /= 64
           or else C.IR_Temp_Point_V1'Object_Size /= 448
           or else C.IR_Temp_Point_V1'Alignment /= 8
           or else Point.Frame_Band'Position /= 8
           or else Point.Pixel_Value'Position /= 32
           or else Point.Temperature_K'Position /= 40
           or else Point.Point_Type'Position /= 48
           or else C.IR_NUC_Temp_Data_Event_V1'Object_Size /= 128
         then
            raise Program_Error with "NUC_TempData C/Ada layout mismatch";
         end if;
      end if;
   end Check_Representation;
end AMS.MEL.IR.NUC.Testing;
