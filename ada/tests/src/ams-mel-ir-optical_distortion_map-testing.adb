with System;
with AMS.MEL_C_API;

package body AMS.MEL.IR.Optical_Distortion_Map.Testing is
   procedure Check_Representation is
      package C renames AMS.MEL_C_API;
      Map : constant C.IR_Optical_Distortion_Map_V1 :=
        (Width          => 0,
         Height         => 0,
         Right_Size     => 0,
         Down_Size      => 0,
         Pixels_Per_ODM => 0,
         Right_Min      => 0.0,
         Right_Max      => 0.0,
         Down_Min       => 0.0,
         Down_Max       => 0.0,
         FRD            => (Data => System.Null_Address, Size => 0),
         Row_Col        => (Data => System.Null_Address, Size => 0));
      FRD : constant C.IR_Forward_Right_Down_V1 := (Forward => 0.0, Right => 0.0, Down => 0.0);
   begin
      --  The compiled C11 image_odm_layout_probe and test_ir_image_odm static assertions fix
      --  these LP64 sizes and offsets.
      if C.Size_T'Size = 64 then
         if C.IR_Forward_Right_Down_V1'Object_Size /= 192
           or else FRD.Right'Position /= 8
           or else FRD.Down'Position /= 16
           or else C.IR_Row_Col_V1'Object_Size /= 128
           or else C.IR_Optical_Distortion_Map_V1'Object_Size /= 704
           or else Map.Pixels_Per_ODM'Position /= 16
           or else Map.Right_Min'Position /= 24
           or else Map.Down_Max'Position /= 48
           or else Map.FRD'Position /= 56
           or else Map.Row_Col'Position /= 72
         then
            raise Program_Error with "OpticalDistortionMap C/Ada layout mismatch";
         end if;
      end if;
   end Check_Representation;
end AMS.MEL.IR.Optical_Distortion_Map.Testing;
