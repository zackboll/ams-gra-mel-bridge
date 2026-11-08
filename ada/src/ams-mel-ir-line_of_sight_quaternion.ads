with AMS.MEL.IR.Image;
with AMS.MEL.IR.Optical_Distortion_Map;

package AMS.MEL.IR.Line_Of_Sight_Quaternion is
   --  Owned values of the pinned upstream @RequiredIfLOSQuaternion LineOfSightQuaternion,
   --  received through the opt-in Line_Of_Sight_Quaternion_Stream owner of
   --  AMS.MEL.IR.Image.Metadata. Every published getter is copied exactly once: the signed
   --  nanosecond time, the X, Y, Z, W quaternion and the forward, right, down rotation rates.
   --  Values are independent of the native event, image stream, Session and provider library.
   --  The bridge supplies data only: it does not normalize the quaternion, canonicalize its
   --  sign, reorder its components, convert it to Euler angles, convert coordinate frames,
   --  infer a time epoch or clamp rates. Receipt is not evidence that the attitude is
   --  normalized, accurate or suitable for an independent pointing calculation.
   --  Long_Float components carry the provider IEEE doubles unchanged: finite, negative,
   --  subnormal, extreme and signed-zero values are exact. Infinity and NaN are also carried
   --  unchanged, never replaced by zero or a substitute, but they are not valid Ada scalar
   --  values. Only the private conversion and receive path disable the optional floating
   --  validity checks; a caller compiled with floating validity checks enabled (for example
   --  GNAT -gnatVa) may raise Constraint_Error when it copies or passes such a component as a
   --  scalar, so test components with 'Valid or IEEE comparisons before checked use. NaN
   --  payload bits are not promised.

   Not_Supported : exception;
   --  The provider answered NotSupported for the LineOfSightQuaternion registration, so it does
   --  not provide quaternion line of sight. The other Image metadata owners of the same stream
   --  are unaffected.

   subtype Quaternion is AMS.MEL.IR.Image.Quaternion;
   --  The canonical Image quaternion record, components X, Y, Z and W from upstream
   --  getQuaternionX/Y/Z/W in that order. Here it is the line-of-sight rotation in the
   --  upstream navigation coordinate frame (North/East/Down relative to wander North). It is
   --  not normalized and its sign is not canonicalized: q and -q stay distinct values.

   subtype Forward_Right_Down is AMS.MEL.IR.Optical_Distortion_Map.Forward_Right_Down;
   --  The canonical ForwardRightDown record, components Forward, Right and Down from upstream
   --  getForward/getRight/getDown. Here it holds rotation rates about the image coordinate frame
   --  axes, in radians per second; rates are not clamped or rescaled.

   type Line_Of_Sight_Value is record
      System_Time_NS : Long_Long_Integer := 0;
      Q_XYZW         : Quaternion := (others => 0.0);
      PQR_RPS        : Forward_Right_Down := (others => 0.0);
   end record;
   --  Complete owned LineOfSightQuaternion. The default value equals the upstream default:
   --  zero time, zero quaternion and zero rates.
   --  @field System_Time_NS Time of validity in signed nanoseconds, from getSystemTime().count();
   --  the full 64-bit range is kept and no clock or epoch is specified or inferred.
   --  @field Q_XYZW Line-of-sight quaternion in X, Y, Z, W order, from getQ_xyzw, in the
   --  upstream navigation coordinate frame; not normalized or sign-canonicalized.
   --  @field PQR_RPS Rotation rates about the image-frame forward, right and down axes in
   --  radians per second, from getPqr_rps; not clamped.
end AMS.MEL.IR.Line_Of_Sight_Quaternion;
