with Ada.Containers;
with AMS.MEL.IR.Channel;
with Interfaces;

package body AMS_MEL_IR_NUC_Checks is
   package N renames AMS.MEL.IR.NUC;
   package V renames AMS.MEL.IR.Channel;
   use type Ada.Containers.Count_Type;
   use type Interfaces.Unsigned_32;
   use type N.Pixel_Width;
   use type N.Temp_Point_Type;
   use type V.Band_Type;

   procedure Check
     (P          : N.Temp_Point;
      NS         : Long_Long_Integer;
      Band       : V.Band_Type;
      Min, Max   : Long_Float;
      Width      : N.Pixel_Width;
      Pixel      : Interfaces.Unsigned_32;
      Kelvin     : Long_Float;
      Point_Type : N.Temp_Point_Type) is
   begin
      if P.Timestamp_NS /= NS
        or else P.Frame_Band.Kind /= Band
        or else P.Frame_Band.Min_Wavelength_M /= Min
        or else P.Frame_Band.Max_Wavelength_M /= Max
        or else P.Pixel.Width /= Width
        or else P.Temperature_K /= Kelvin
        or else P.Point_Type /= Point_Type
      then
         raise Program_Error with "Ada NUC TempPoint field mismatch";
      end if;
      case P.Pixel.Width is
         when N.Unsigned_8_Pixel  =>
            if Interfaces.Unsigned_32 (P.Pixel.Value_8) /= Pixel then
               raise Program_Error with "Ada NUC uint8 PixelValue mismatch";
            end if;

         when N.Unsigned_16_Pixel =>
            if Interfaces.Unsigned_32 (P.Pixel.Value_16) /= Pixel then
               raise Program_Error with "Ada NUC uint16 PixelValue mismatch";
            end if;

         when N.Unsigned_32_Pixel =>
            if P.Pixel.Value_32 /= Pixel then
               raise Program_Error with "Ada NUC uint32 PixelValue mismatch";
            end if;
      end case;
   end Check;

   procedure Check_Rich (Empty, One, Three : N.NUC_Temp_Data) is
   begin
      if Empty.Points.Length /= 0 or else One.Points.Length /= 1 or else Three.Points.Length /= 3
      then
         raise Program_Error with "Ada NUC point collection length mismatch";
      end if;
      Check
        (One.Points (1),
         Long_Long_Integer'First,
         V.IR_Midwave,
         3.0e-6,
         5.0e-6,
         N.Unsigned_8_Pixel,
         16#FF#,
         300.25,
         N.NUC_Point);
      Check
        (Three.Points (1),
         -1,
         V.IR_Longwave,
         8.0e-6,
         1.2e-5,
         N.Unsigned_16_Pixel,
         16#FFFF#,
         0.0,
         N.MFA_Estimate);
      Check
        (Three.Points (2),
         Long_Long_Integer'Last,
         V.UV_Vacuum,
         1.0e-8,
         2.0e-7,
         N.Unsigned_32_Pixel,
         16#FFFF_FFFF#,
         1.0e6,
         N.Other);
      Check (Three.Points (3), 42, V.Invalid, -1.0, 0.0, N.Unsigned_8_Pixel, 0, -5.5, N.NUC_Point);
   end Check_Rich;

   procedure Check_Single (Value : N.NUC_Temp_Data; Timestamp_NS : Long_Long_Integer) is
   begin
      if Value.Points.Length /= 1 then
         raise Program_Error with "Ada NUC single-point length mismatch";
      end if;
      Check
        (Value.Points (1),
         Timestamp_NS,
         V.IR_Shortwave,
         1.0e-6,
         2.5e-6,
         N.Unsigned_16_Pixel,
         1234,
         273.15,
         N.NUC_Point);
   end Check_Single;
end AMS_MEL_IR_NUC_Checks;
