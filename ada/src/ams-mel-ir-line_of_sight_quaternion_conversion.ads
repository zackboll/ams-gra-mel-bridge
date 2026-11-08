with AMS.MEL_C_API;
with AMS.MEL.IR.Line_Of_Sight_Quaternion;

--  IRMEL-ADA-010 private conversion of the native event view into an owned Line_Of_Sight_Value.
--  Raw addresses and C records never leave this unit.

private package AMS.MEL.IR.Line_Of_Sight_Quaternion_Conversion is
   function To_Value
     (Raw : AMS.MEL_C_API.IR_Line_Of_Sight_Quaternion_V1)
      return AMS.MEL.IR.Line_Of_Sight_Quaternion.Line_Of_Sight_Value;
   --  Copies the signed time and every quaternion and rate component by name, in upstream
   --  order, without normalization, reordering, clamping or replacement of non-finite values.
end AMS.MEL.IR.Line_Of_Sight_Quaternion_Conversion;
