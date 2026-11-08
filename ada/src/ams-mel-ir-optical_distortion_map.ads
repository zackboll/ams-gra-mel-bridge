private with Ada.Containers.Vectors;
with Interfaces;

package AMS.MEL.IR.Optical_Distortion_Map is
   --  Owned values of the pinned upstream @Optional OpticalDistortionMap, received through the
   --  opt-in Optical_Distortion_Map_Stream owner of AMS.MEL.IR.Image.Metadata. Every published
   --  getter is copied: five unsigned 32-bit values, four double extrema and the complete FRD
   --  and RowCol collections. Values are independent of the native event, image stream, Session
   --  and provider library. The two collections are independent of each other and of the scalar
   --  dimensions: no length equality or product relation is assumed and no element is inferred.
   --  Long_Float components carry the provider IEEE doubles unchanged: finite, negative and
   --  signed-zero values are exact. Infinity and NaN are also carried unchanged, never replaced
   --  by zero or a substitute, but they are not valid Ada scalar values: callers must test them
   --  with 'Valid or IEEE comparisons before checked arithmetic. NaN payload bits are not
   --  promised.

   Not_Supported : exception;
   --  The provider answered NotSupported for the optional OpticalDistortionMap registration.
   --  The other Image metadata owners of the same stream are unaffected.

   type Forward_Right_Down is record
      Forward : Long_Float;
      Right   : Long_Float;
      Down    : Long_Float;
   end record;
   --  One copied upstream ForwardRightDown direction-cosine sample, in upstream getter order.
   --  The frame is the line-of-sight frame described upstream; no normalization is applied.
   --  @field Forward Forward direction-cosine component, from getForward.
   --  @field Right Right direction-cosine component, from getRight.
   --  @field Down Down direction-cosine component, from getDown.

   type Row_Column is record
      Row    : Long_Float;
      Column : Long_Float;
   end record;
   --  One copied upstream RowCol sample. No pixel-origin convention or rounding is inferred.
   --  @field Row Row coordinate, from getRow.
   --  @field Column Column coordinate, from getCol.

   type Map_Value is private;
   --  Complete owned OpticalDistortionMap. The default value is the empty map with every scalar
   --  zero and both collections empty.

   function Width (Value : Map_Value) return Interfaces.Unsigned_32;
   --  Number of forward-map samples across, preserved from getWidth.
   --  @param Value Owned map to inspect.
   --  @return Provider width value, including zero and the unsigned 32-bit maximum.
   function Height (Value : Map_Value) return Interfaces.Unsigned_32;
   --  Number of forward-map samples down, preserved from getHeight.
   --  @param Value Owned map to inspect.
   --  @return Provider height value, including zero and the unsigned 32-bit maximum.
   function Right_Size (Value : Map_Value) return Interfaces.Unsigned_32;
   --  Number of backward-map samples in the right direction, preserved from getRightSize.
   --  @param Value Owned map to inspect.
   --  @return Provider right size, including zero and the unsigned 32-bit maximum.
   function Down_Size (Value : Map_Value) return Interfaces.Unsigned_32;
   --  Number of backward-map samples in the down direction, preserved from getDownSize.
   --  @param Value Owned map to inspect.
   --  @return Provider down size, including zero and the unsigned 32-bit maximum.
   function Pixels_Per_ODM (Value : Map_Value) return Interfaces.Unsigned_32;
   --  Number of pixels per distortion-map sample, preserved from getPixelsPerOdm.
   --  @param Value Owned map to inspect.
   --  @return Provider pixels-per-sample value, including zero and the unsigned 32-bit maximum.
   function Right_Min (Value : Map_Value) return Long_Float;
   --  Right direction-cosine minimum extent, preserved from getRightMin without clamping.
   --  @param Value Owned map to inspect.
   --  @return Provider value; may be infinity or NaN, see the package description.
   function Right_Max (Value : Map_Value) return Long_Float;
   --  Right direction-cosine maximum extent, preserved from getRightMax without clamping.
   --  @param Value Owned map to inspect.
   --  @return Provider value; may be infinity or NaN, see the package description.
   function Down_Min (Value : Map_Value) return Long_Float;
   --  Down direction-cosine minimum extent, preserved from getDownMin without clamping.
   --  @param Value Owned map to inspect.
   --  @return Provider value; may be infinity or NaN, see the package description.
   function Down_Max (Value : Map_Value) return Long_Float;
   --  Down direction-cosine maximum extent, preserved from getDownMax without clamping.
   --  @param Value Owned map to inspect.
   --  @return Provider value; may be infinity or NaN, see the package description.
   function FRD_Count (Value : Map_Value) return Natural;
   --  Actual length of the copied getFrd collection, independent of RowCol_Count and of every
   --  scalar dimension.
   --  @param Value Owned map to inspect.
   --  @return Number of copied FRD samples; zero for an empty collection.
   function FRD_At (Value : Map_Value; Index : Positive) return Forward_Right_Down;
   --  Copied FRD sample at a one-based position in provider order, duplicates kept.
   --  @param Value Owned map to inspect.
   --  @param Index One-based position; raises Constraint_Error beyond FRD_Count.
   --  @return Owned FRD sample.
   function RowCol_Count (Value : Map_Value) return Natural;
   --  Actual length of the copied getRowCol collection, independent of FRD_Count and of every
   --  scalar dimension.
   --  @param Value Owned map to inspect.
   --  @return Number of copied RowCol samples; zero for an empty collection.
   function RowCol_At (Value : Map_Value; Index : Positive) return Row_Column;
   --  Copied RowCol sample at a one-based position in provider order, duplicates kept.
   --  @param Value Owned map to inspect.
   --  @param Index One-based position; raises Constraint_Error beyond RowCol_Count.
   --  @return Owned RowCol sample.

   function Create
     (Width, Height, Right_Size, Down_Size, Pixels_Per_ODM : Interfaces.Unsigned_32;
      Right_Min, Right_Max, Down_Min, Down_Max             : Long_Float) return Map_Value;
   --  Build an owned map with every scalar set exactly as given and both collections empty.
   --  Values are stored unchanged; no consistency between them is checked or assumed.
   --  @param Width Forward-map sample count across.
   --  @param Height Forward-map sample count down.
   --  @param Right_Size Backward-map sample count in the right direction.
   --  @param Down_Size Backward-map sample count in the down direction.
   --  @param Pixels_Per_ODM Pixels per distortion-map sample.
   --  @param Right_Min Right direction-cosine minimum extent.
   --  @param Right_Max Right direction-cosine maximum extent.
   --  @param Down_Min Down direction-cosine minimum extent.
   --  @param Down_Max Down direction-cosine maximum extent.
   --  @return Owned map with empty FRD and RowCol collections.
   procedure Append_FRD (Value : in out Map_Value; Sample : Forward_Right_Down);
   --  Append a copy of one FRD sample at the end, preserving order and duplicates. The RowCol
   --  collection is unchanged.
   --  @param Value Destination map.
   --  @param Sample Sample copied to the end of the FRD collection only.
   procedure Append_RowCol (Value : in out Map_Value; Sample : Row_Column);
   --  Append a copy of one RowCol sample at the end, preserving order and duplicates. The FRD
   --  collection is unchanged.
   --  @param Value Destination map.
   --  @param Sample Sample copied to the end of the RowCol collection only.

private
   pragma Validity_Checks ("F");
   --  Non-finite provider doubles are intentional values in this private storage.

   package FRD_Vectors is new Ada.Containers.Vectors (Positive, Forward_Right_Down);
   package RowCol_Vectors is new Ada.Containers.Vectors (Positive, Row_Column);
   type Map_Value is record
      Width, Height, Right_Size, Down_Size, Pixels_Per_ODM : Interfaces.Unsigned_32 := 0;
      Right_Min, Right_Max, Down_Min, Down_Max             : Long_Float := 0.0;
      FRD                                                  : FRD_Vectors.Vector;
      Row_Col                                              : RowCol_Vectors.Vector;
   end record;
end AMS.MEL.IR.Optical_Distortion_Map;
