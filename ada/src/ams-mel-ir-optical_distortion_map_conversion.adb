with Ada.Unchecked_Conversion;
with System.Storage_Elements;

package body AMS.MEL.IR.Optical_Distortion_Map_Conversion is
   --  Provider IEEE doubles, including infinities and NaN, are copied as values; only the
   --  optional floating-point validity checks are disabled, never range or overflow checks.
   pragma Validity_Checks ("F");
   package C renames AMS.MEL_C_API;
   package ODM renames AMS.MEL.IR.Optical_Distortion_Map;
   use type C.Size_T;
   use type System.Address;
   use System.Storage_Elements;

   type FRD_Access is access all C.IR_Forward_Right_Down_V1;
   type RowCol_Access is access all C.IR_Row_Col_V1;
   function To_FRD is new Ada.Unchecked_Conversion (System.Address, FRD_Access);
   function To_RowCol is new Ada.Unchecked_Conversion (System.Address, RowCol_Access);
   --  C array stride includes tail padding; 'Object_Size does.
   FRD_Bytes    : constant Positive := C.IR_Forward_Right_Down_V1'Object_Size / System.Storage_Unit;
   RowCol_Bytes : constant Positive := C.IR_Row_Col_V1'Object_Size / System.Storage_Unit;

   procedure Check (Data : System.Address; Count : C.Size_T; Bytes : Positive) is
   begin
      if Count > C.Size_T (Interfaces.Integer_32'Last)
        or else (Count > 0 and then Data = System.Null_Address)
        or else Count > C.Size_T (Storage_Offset'Last) / C.Size_T (Bytes)
        or else (Count > 0
                 and then To_Integer (Data)
                          > Integer_Address'Last - Integer_Address (Count * C.Size_T (Bytes)))
      then
         raise Provider_Error with "invalid native OpticalDistortionMap span";
      end if;
   end Check;

   function Address_At
     (Data : System.Address; Index : Natural; Bytes : Positive) return System.Address
   is (Data + Storage_Offset (Index) * Storage_Offset (Bytes));

   function To_Value (Raw : C.IR_Optical_Distortion_Map_V1) return ODM.Map_Value is
   begin
      --  Each collection is validated and copied independently; lengths are never zipped,
      --  compared with each other or derived from the scalar dimensions.
      Check (Raw.FRD.Data, Raw.FRD.Size, FRD_Bytes);
      Check (Raw.Row_Col.Data, Raw.Row_Col.Size, RowCol_Bytes);
      return
         Result : ODM.Map_Value :=
           ODM.Create
             (Width          => Raw.Width,
              Height         => Raw.Height,
              Right_Size     => Raw.Right_Size,
              Down_Size      => Raw.Down_Size,
              Pixels_Per_ODM => Raw.Pixels_Per_ODM,
              Right_Min      => Long_Float (Raw.Right_Min),
              Right_Max      => Long_Float (Raw.Right_Max),
              Down_Min       => Long_Float (Raw.Down_Min),
              Down_Max       => Long_Float (Raw.Down_Max))
      do
         for I in 1 .. Natural (Raw.FRD.Size) loop
            declare
               E : constant FRD_Access := To_FRD (Address_At (Raw.FRD.Data, I - 1, FRD_Bytes));
            begin
               ODM.Append_FRD
                 (Result,
                  (Forward => Long_Float (E.Forward),
                   Right   => Long_Float (E.Right),
                   Down    => Long_Float (E.Down)));
            end;
         end loop;
         for I in 1 .. Natural (Raw.Row_Col.Size) loop
            declare
               E : constant RowCol_Access :=
                 To_RowCol (Address_At (Raw.Row_Col.Data, I - 1, RowCol_Bytes));
            begin
               ODM.Append_RowCol
                 (Result, (Row => Long_Float (E.Row), Column => Long_Float (E.Column)));
            end;
         end loop;
      end return;
   end To_Value;
end AMS.MEL.IR.Optical_Distortion_Map_Conversion;
