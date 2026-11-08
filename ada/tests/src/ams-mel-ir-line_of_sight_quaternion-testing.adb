with AMS.MEL_C_API;

package body AMS.MEL.IR.Line_Of_Sight_Quaternion.Testing is
   procedure Check_Representation is
      package C renames AMS.MEL_C_API;
      Value : constant C.IR_Line_Of_Sight_Quaternion_V1 :=
        (System_Time_NS => 0,
         Q_XYZW         => (X => 0.0, Y => 0.0, Z => 0.0, W => 0.0),
         PQR_RPS        => (Forward => 0.0, Right => 0.0, Down => 0.0));
   begin
      --  The compiled C11 image_los_quaternion_layout_probe and test_ir_image_los_quaternion
      --  static assertions fix these LP64 sizes and offsets.
      if C.Size_T'Size = 64 then
         if C.IR_Line_Of_Sight_Quaternion_V1'Object_Size /= 512
           or else Value.Q_XYZW'Position /= 8
           or else Value.PQR_RPS'Position /= 40
           or else Value.Q_XYZW.W'Position /= 24
           or else Value.PQR_RPS.Down'Position /= 16
           or else C.IR_Quaternion_V1'Object_Size /= 256
           or else C.IR_Forward_Right_Down_V1'Object_Size /= 192
         then
            raise Program_Error with "LineOfSightQuaternion C/Ada layout mismatch";
         end if;
      end if;
   end Check_Representation;
end AMS.MEL.IR.Line_Of_Sight_Quaternion.Testing;
