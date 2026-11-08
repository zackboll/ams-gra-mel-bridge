with AMS.MEL_C_API;
with AMS.MEL.IR.Track.System_Data;

--  IRMEL-ADA-008 private, state-free conversion of the one public owned
--  System_Track_Data_Response to the existing native record. Shared by the
--  independent Track.System_Data.Submit and C2.System_Data submissions; it owns
--  no channel state, request, admission or lifecycle accounting.

private package AMS.MEL.IR.System_Track_Data_Conversion is
   function To_Raw
     (Value : AMS.MEL.IR.Track.System_Data.System_Track_Data_Response)
      return AMS.MEL_C_API.IR_System_Track_Data_Response_V1;
   --  Exact field-by-field copy of all twelve fields: signed nanoseconds,
   --  three independent unsigned identifiers, four native doubles, both angle
   --  pairs and both Booleans as exactly 0 or 1. No clamping, normalization,
   --  coordinate transform, inferred epoch or zeroing of inactive fields.
end AMS.MEL.IR.System_Track_Data_Conversion;
