with Ada.Containers.Vectors;
with Interfaces;
with AMS.MEL.IR.Channel;

package AMS.MEL.IR.NUC is
   --  Owned values of the pinned upstream @Optional NUC_TempData metadata, received through
   --  the opt-in NUC owners of AMS.MEL.IR.Image.Metadata and
   --  AMS.MEL.IR.Health_Status.Metadata. Every publicly readable TempPoint field is copied.
   --  The pinned TempPoint also stores an optional tempError, but upstream declares no public
   --  accessor for it; it is deliberately not represented, inferred or reported here.

   Not_Supported : exception;
   --  The provider answered NotSupported for the optional NUC_TempData registration. The
   --  existing metadata owners of the same channel are unaffected.

   type Temp_Point_Type is (NUC_Point, MFA_Estimate, Other);
   --  Exactly the three pinned TempPointType alternatives; unknown values are rejected.
   --  @enum NUC_Point Point measured in the MFA's NUC process, upstream value 0.
   --  @enum MFA_Estimate Point interpolated or extrapolated from supplier data or NUC points.
   --  @enum Other Invalid or unsubstantiated point, upstream value 2.

   type Pixel_Width is (Unsigned_8_Pixel, Unsigned_16_Pixel, Unsigned_32_Pixel);
   --  Active alternative of the upstream PixelValue variant; never inferred from magnitude.
   --  @enum Unsigned_8_Pixel The uint8_t alternative.
   --  @enum Unsigned_16_Pixel The uint16_t alternative.
   --  @enum Unsigned_32_Pixel The uint32_t alternative.

   type Pixel_Value (Width : Pixel_Width := Unsigned_8_Pixel) is record
      case Width is
         when Unsigned_8_Pixel =>
            Value_8 : Interfaces.Unsigned_8;

         when Unsigned_16_Pixel =>
            Value_16 : Interfaces.Unsigned_16;

         when Unsigned_32_Pixel =>
            Value_32 : Interfaces.Unsigned_32;
      end case;
   end record;
   --  Lossless tagged copy of TempPoint::PixelValue in its original integer width.
   --  @field Width Active upstream alternative; selects the only valid value component.
   --  @field Value_8 Exact uint8_t pixel value.
   --  @field Value_16 Exact uint16_t pixel value.
   --  @field Value_32 Exact uint32_t pixel value.

   type Temp_Point is record
      Timestamp_NS  : Long_Long_Integer;
      Frame_Band    : AMS.MEL.IR.Channel.Band_Info;
      Pixel         : Pixel_Value;
      Temperature_K : Long_Float;
      Point_Type    : Temp_Point_Type;
   end record;
   --  Owned copy of every publicly readable TempPoint field. The inaccessible upstream
   --  tempError is not a component: absence here is not a claim that no error exists.
   --  @field Timestamp_NS Signed nanosecond count, unconverted; clock and epoch unspecified.
   --  @field Frame_Band Band type and vacuum wavelength limits, in meters, of the point.
   --  @field Pixel Point pixel value with its original integer width.
   --  @field Temperature_K Temperature of the pixel value in Kelvin, copied without checks.
   --  @field Point_Type Upstream classification of the point.

   package Temp_Point_Vectors is new Ada.Containers.Vectors (Positive, Temp_Point);
   --  Positive-index vector preserving provider point order and duplicates.

   type NUC_Temp_Data is record
      Points : Temp_Point_Vectors.Vector;
   end record;
   --  Owned complete ordered getTempPoints() collection; empty messages have no points. The
   --  value is independent of the owner, channel, Session and provider library.
   --  @field Points Copied points in the order supplied by the provider.
end AMS.MEL.IR.NUC;
