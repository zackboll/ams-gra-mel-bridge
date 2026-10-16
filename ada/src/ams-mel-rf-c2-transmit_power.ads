with Interfaces;

--  Four independent @RequiredIfTransmit provider calculations. No capability
--  auto-gate: callers may inspect capability APIs separately. Weight_Type is
--  only a provider-defined ID, not a Weights owner. No formulas or caching.
--  Existing VA claim permits calls after public C2 Close. Same-VA operations
--  including Close require external serialization. No new owner/worker/pin.
--  Attenuation and gain are dB, radiated powers dBW, frequency Hz. U/V are the
--  upstream AnglePair line-of-sight/stabilization components, unmodified.
--  Doubles are not clamped, range-checked, finite-checked or normalized;
--  signed zero, infinity and NaN retain their semantic value (no payload claim).
--  IDs not representable in the provider's size_t raise Provider_Error.

package AMS.MEL.RF.C2.Transmit_Power is
   --  Four independent conditional provider calculations, required upstream only if the
   --  associated MFA supports transmit. No local calculation or automatic capability gate is
   --  supplied. A successful numeric result is provider data, not proof of physical RF
   --  activity. Calls require an existing live VA and same-VA external serialization,
   --  including Close; the public C2 may already be closed.

   subtype Element_Group_ID is Interfaces.Unsigned_64;
   --  Unsigned provider transmit element-group ID, not a label. Native size t representability
   --  is checked before provider entry; failure raises Provider Error without truncation.

   subtype Weight_Type_ID is Interfaces.Unsigned_64;
   --  Unsigned provider weight-type ID, not an owned Weights object. Must fit native size t;
   --  unrepresentable values raise Provider Error before provider entry.

   type UV_Line_Of_Sight is record
      U : Long_Float;
      V : Long_Float;
   end record;
   --  Upstream AnglePair line-of-sight/stabilization components, forwarded without
   --  normalization, conversion or local physical range checks.
   --  @field U First upstream U component, passed unchanged.
   --  @field V Second upstream V component, passed unchanged.

   function Radiated_Power_DBW
     (Object              : Virtual_Aperture'Class;
      Element_Group       : Element_Group_ID;
      Power_Mode_ID       : Interfaces.Unsigned_32;
      Attenuation_DB      : Long_Float;
      Weight_Type         : Weight_Type_ID;
      Center_Frequency_Hz : Long_Float;
      Line_Of_Sight       : UV_Line_Of_Sight;
      VA_Instance_ID      : Interfaces.Unsigned_32 := 0) return Long_Float;
   --  Invoke conditional RequiredIfTransmit getTxRadiatedPower exactly once. No automatic
   --  capability gate, RF formula, caching, interpolation or reconciliation with separately
   --  queried values. Inputs/results preserve floating-point semantic values, including signed
   --  zero, infinity and NaN classification, without finite/range enforcement; no NaN payload-
   --  bit guarantee. Native/provider failures raise Provider Error. A returned number does not
   --  prove transmission, emission, antenna performance or valid hardware state.
   --  @param Object Live claimed VA; externally serialize with all same-VA calls, including
   --  Close. Its existing native claim remains usable after public C2 Close.
   --  @param Element_Group Provider numeric transmit-group ID; must fit native size t.
   --  @param Power_Mode_ID Provider power-mode ID, distinct from attenuation and weight type.
   --  @param VA_Instance_ID Provider-defined VA instance selection, default 0; no local
   --  membership check.
   --  @param Attenuation_DB Provider attenuation input in dB, forwarded without clipping.
   --  @param Weight_Type Provider weight-type ID fitting native size t; no Weights owner is
   --  allocated.
   --  @param Center_Frequency_Hz Center frequency in Hz, without unit conversion or
   --  interpolation.
   --  @param Line_Of_Sight U/V components passed verbatim as an AnglePair, not normalized.
   --  @return Provider radiated output power in dBW; no inferred relationship to other query
   --  results.

   function Peak_Radiated_Power_DBW
     (Object              : Virtual_Aperture'Class;
      Element_Group       : Element_Group_ID;
      Power_Mode_ID       : Interfaces.Unsigned_32;
      Attenuation_DB      : Long_Float;
      Center_Frequency_Hz : Long_Float;
      VA_Instance_ID      : Interfaces.Unsigned_32 := 0) return Long_Float;
   --  Invoke conditional RequiredIfTransmit getTxPeakRadiatedPower exactly once. No automatic
   --  capability gate, RF formula, caching, interpolation or reconciliation with separately
   --  queried values. Inputs/results preserve floating-point semantic values, including signed
   --  zero, infinity and NaN classification, without finite/range enforcement; no NaN payload-
   --  bit guarantee. Native/provider failures raise Provider Error. A returned number does not
   --  prove transmission, emission, antenna performance or valid hardware state.
   --  @param Object Live claimed VA; externally serialize with all same-VA calls, including
   --  Close. Its existing native claim remains usable after public C2 Close.
   --  @param Element_Group Provider numeric transmit-group ID; must fit native size t.
   --  @param Power_Mode_ID Provider power-mode ID, distinct from attenuation and weight type.
   --  @param VA_Instance_ID Provider-defined VA instance selection, default 0; no local
   --  membership check.
   --  @param Attenuation_DB Provider attenuation input in dB, forwarded without clipping.
   --  @param Center_Frequency_Hz Center frequency in Hz, without unit conversion or
   --  interpolation.
   --  @return Provider peak radiated output power in dBW; no inferred relationship to other
   --  query results.

   function Aperture_Gain_DB
     (Object              : Virtual_Aperture'Class;
      Element_Group       : Element_Group_ID;
      Power_Mode_ID       : Interfaces.Unsigned_32;
      Weight_Type         : Weight_Type_ID;
      Center_Frequency_Hz : Long_Float;
      Line_Of_Sight       : UV_Line_Of_Sight;
      VA_Instance_ID      : Interfaces.Unsigned_32 := 0) return Long_Float;
   --  Invoke conditional RequiredIfTransmit getTxApertureGain exactly once. No automatic
   --  capability gate, RF formula, caching, interpolation or reconciliation with separately
   --  queried values. Inputs/results preserve floating-point semantic values, including signed
   --  zero, infinity and NaN classification, without finite/range enforcement; no NaN payload-
   --  bit guarantee. Native/provider failures raise Provider Error. A returned number does not
   --  prove transmission, emission, antenna performance or valid hardware state.
   --  @param Object Live claimed VA; externally serialize with all same-VA calls, including
   --  Close. Its existing native claim remains usable after public C2 Close.
   --  @param Element_Group Provider numeric transmit-group ID; must fit native size t.
   --  @param Power_Mode_ID Provider power-mode ID, distinct from attenuation and weight type.
   --  @param VA_Instance_ID Provider-defined VA instance selection, default 0; no local
   --  membership check.
   --  @param Weight_Type Provider weight-type ID fitting native size t; no Weights owner is
   --  allocated.
   --  @param Center_Frequency_Hz Center frequency in Hz, without unit conversion or
   --  interpolation.
   --  @param Line_Of_Sight U/V components passed verbatim as an AnglePair, not normalized.
   --  @return Provider transmit aperture gain in dB; no inferred relationship to other query
   --  results.

   function Max_Attenuation_DB
     (Object         : Virtual_Aperture'Class;
      Element_Group  : Element_Group_ID;
      Power_Mode_ID  : Interfaces.Unsigned_32;
      VA_Instance_ID : Interfaces.Unsigned_32 := 0) return Long_Float;
   --  Invoke conditional RequiredIfTransmit getMaxTxAttenuation exactly once. No automatic
   --  capability gate, RF formula, caching, interpolation or reconciliation with separately
   --  queried values. Inputs/results preserve floating-point semantic values, including signed
   --  zero, infinity and NaN classification, without finite/range enforcement; no NaN payload-
   --  bit guarantee. Native/provider failures raise Provider Error. A returned number does not
   --  prove transmission, emission, antenna performance or valid hardware state.
   --  @param Object Live claimed VA; externally serialize with all same-VA calls, including
   --  Close. Its existing native claim remains usable after public C2 Close.
   --  @param Element_Group Provider numeric transmit-group ID; must fit native size t.
   --  @param Power_Mode_ID Provider power-mode ID, distinct from attenuation and weight type.
   --  @param VA_Instance_ID Provider-defined VA instance selection, default 0; no local
   --  membership check.
   --  @return Provider maximum available transmit attenuation in dB; no inferred relationship
   --  to other query results.

end AMS.MEL.RF.C2.Transmit_Power;
