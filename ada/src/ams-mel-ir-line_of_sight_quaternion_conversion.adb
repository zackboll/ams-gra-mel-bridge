package body AMS.MEL.IR.Line_Of_Sight_Quaternion_Conversion is
   --  Provider IEEE doubles, including infinities and NaN, are copied as values; only the
   --  optional floating-point validity checks are disabled, never range or overflow checks.
   pragma Validity_Checks ("F");
   package C renames AMS.MEL_C_API;
   package LOSQ renames AMS.MEL.IR.Line_Of_Sight_Quaternion;

   --  Every signed 64-bit nanosecond count must be representable without narrowing.
   pragma
     Compile_Time_Error
       (Long_Long_Integer'First > Long_Long_Integer (Interfaces.Integer_64'First)
          or else Long_Long_Integer'Last < Long_Long_Integer (Interfaces.Integer_64'Last),
        "Long_Long_Integer cannot hold every signed 64-bit nanosecond count");

   function To_Value (Raw : C.IR_Line_Of_Sight_Quaternion_V1) return LOSQ.Line_Of_Sight_Value
   is ((System_Time_NS => Long_Long_Integer (Raw.System_Time_NS),
        Q_XYZW         =>
          (X => Long_Float (Raw.Q_XYZW.X),
           Y => Long_Float (Raw.Q_XYZW.Y),
           Z => Long_Float (Raw.Q_XYZW.Z),
           W => Long_Float (Raw.Q_XYZW.W)),
        PQR_RPS        =>
          (Forward => Long_Float (Raw.PQR_RPS.Forward),
           Right   => Long_Float (Raw.PQR_RPS.Right),
           Down    => Long_Float (Raw.PQR_RPS.Down))));
end AMS.MEL.IR.Line_Of_Sight_Quaternion_Conversion;
