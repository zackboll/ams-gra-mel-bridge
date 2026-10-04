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
   subtype Element_Group_ID is Interfaces.Unsigned_64;
   subtype Weight_Type_ID is Interfaces.Unsigned_64;
   type UV_Line_Of_Sight is record
      U : Long_Float;
      V : Long_Float;
   end record;
   function Radiated_Power_DBW
     (Object              : Virtual_Aperture'Class;
      Element_Group       : Element_Group_ID;
      Power_Mode_ID       : Interfaces.Unsigned_32;
      Attenuation_DB      : Long_Float;
      Weight_Type         : Weight_Type_ID;
      Center_Frequency_Hz : Long_Float;
      Line_Of_Sight       : UV_Line_Of_Sight;
      VA_Instance_ID      : Interfaces.Unsigned_32 := 0) return Long_Float;
   function Peak_Radiated_Power_DBW
     (Object              : Virtual_Aperture'Class;
      Element_Group       : Element_Group_ID;
      Power_Mode_ID       : Interfaces.Unsigned_32;
      Attenuation_DB      : Long_Float;
      Center_Frequency_Hz : Long_Float;
      VA_Instance_ID      : Interfaces.Unsigned_32 := 0) return Long_Float;
   function Aperture_Gain_DB
     (Object              : Virtual_Aperture'Class;
      Element_Group       : Element_Group_ID;
      Power_Mode_ID       : Interfaces.Unsigned_32;
      Weight_Type         : Weight_Type_ID;
      Center_Frequency_Hz : Long_Float;
      Line_Of_Sight       : UV_Line_Of_Sight;
      VA_Instance_ID      : Interfaces.Unsigned_32 := 0) return Long_Float;
   function Max_Attenuation_DB
     (Object         : Virtual_Aperture'Class;
      Element_Group  : Element_Group_ID;
      Power_Mode_ID  : Interfaces.Unsigned_32;
      VA_Instance_ID : Interfaces.Unsigned_32 := 0) return Long_Float;
end AMS.MEL.RF.C2.Transmit_Power;
