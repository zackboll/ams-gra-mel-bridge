package body AMS.MEL.IR.Optical_Distortion_Map is
   --  Accessors return stored provider doubles unchanged, including non-finite values.
   pragma Validity_Checks ("F");

   function Width (Value : Map_Value) return Interfaces.Unsigned_32
   is (Value.Width);
   function Height (Value : Map_Value) return Interfaces.Unsigned_32
   is (Value.Height);
   function Right_Size (Value : Map_Value) return Interfaces.Unsigned_32
   is (Value.Right_Size);
   function Down_Size (Value : Map_Value) return Interfaces.Unsigned_32
   is (Value.Down_Size);
   function Pixels_Per_ODM (Value : Map_Value) return Interfaces.Unsigned_32
   is (Value.Pixels_Per_ODM);
   function Right_Min (Value : Map_Value) return Long_Float
   is (Value.Right_Min);
   function Right_Max (Value : Map_Value) return Long_Float
   is (Value.Right_Max);
   function Down_Min (Value : Map_Value) return Long_Float
   is (Value.Down_Min);
   function Down_Max (Value : Map_Value) return Long_Float
   is (Value.Down_Max);
   function FRD_Count (Value : Map_Value) return Natural
   is (Natural (Value.FRD.Length));
   function FRD_At (Value : Map_Value; Index : Positive) return Forward_Right_Down
   is (Value.FRD.Element (Index));
   function RowCol_Count (Value : Map_Value) return Natural
   is (Natural (Value.Row_Col.Length));
   function RowCol_At (Value : Map_Value; Index : Positive) return Row_Column
   is (Value.Row_Col.Element (Index));
   function Create
     (Width, Height, Right_Size, Down_Size, Pixels_Per_ODM : Interfaces.Unsigned_32;
      Right_Min, Right_Max, Down_Min, Down_Max             : Long_Float) return Map_Value
   is (Width          => Width,
       Height         => Height,
       Right_Size     => Right_Size,
       Down_Size      => Down_Size,
       Pixels_Per_ODM => Pixels_Per_ODM,
       Right_Min      => Right_Min,
       Right_Max      => Right_Max,
       Down_Min       => Down_Min,
       Down_Max       => Down_Max,
       FRD            => FRD_Vectors.Empty_Vector,
       Row_Col        => RowCol_Vectors.Empty_Vector);
   procedure Append_FRD (Value : in out Map_Value; Sample : Forward_Right_Down) is
   begin
      Value.FRD.Append (Sample);
   end Append_FRD;
   procedure Append_RowCol (Value : in out Map_Value; Sample : Row_Column) is
   begin
      Value.Row_Col.Append (Sample);
   end Append_RowCol;
end AMS.MEL.IR.Optical_Distortion_Map;
