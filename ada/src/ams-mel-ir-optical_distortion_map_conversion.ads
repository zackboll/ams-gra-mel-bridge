with AMS.MEL_C_API;
with AMS.MEL.IR.Optical_Distortion_Map;

--  IRMEL-ADA-009 private conversion of the native event view into an owned Map_Value. Raw
--  addresses and C records never leave this unit.

private package AMS.MEL.IR.Optical_Distortion_Map_Conversion is
   function To_Value
     (Raw : AMS.MEL_C_API.IR_Optical_Distortion_Map_V1)
      return AMS.MEL.IR.Optical_Distortion_Map.Map_Value;
   --  Validates both spans (count within Integer_32'Last, non-null data for a nonzero count,
   --  no address or byte-size overflow) before reading any element, then copies every scalar
   --  and every element in order. Raises Provider_Error for an invalid span; no partial value
   --  escapes.
end AMS.MEL.IR.Optical_Distortion_Map_Conversion;
