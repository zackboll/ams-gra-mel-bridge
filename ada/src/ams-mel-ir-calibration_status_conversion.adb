with Ada.Unchecked_Conversion;
with Interfaces.C;
with System.Storage_Elements;

package body AMS.MEL.IR.Calibration_Status_Conversion is
   package C renames AMS.MEL_C_API;
   package CS renames Calibration_Status;
   use type C.Size_T;
   use type Interfaces.Unsigned_32;
   use type System.Address;
   use System.Storage_Elements;
   type Completed_Access is access all C.IR_Completed_Calibration_V1;
   type Item_Access is access all C.IR_Completed_Calibration_Item_V1;
   type ID_Access is access all C.UCI_ID_V1;
   type Char_Access is access all Interfaces.C.char;
   function To_Completed is new Ada.Unchecked_Conversion (System.Address, Completed_Access);
   function To_Item is new Ada.Unchecked_Conversion (System.Address, Item_Access);
   function To_ID is new Ada.Unchecked_Conversion (System.Address, ID_Access);
   function To_Char is new Ada.Unchecked_Conversion (System.Address, Char_Access);
   Completed_Bytes : constant Positive := C.IR_Completed_Calibration_V1'Size / System.Storage_Unit;
   Item_Bytes      : constant Positive :=
     C.IR_Completed_Calibration_Item_V1'Size / System.Storage_Unit;
   ID_Bytes        : constant Positive := C.UCI_ID_V1'Size / System.Storage_Unit;

   function To_Raw (Command : CS.Status_Command) return C.IR_Calibration_Status_Command_V1
   is ((Interfaces.Unsigned_32 (Command.Command_ID), Interfaces.Unsigned_32 (Command.Request_ID)));

   procedure Check (Data : System.Address; Count : C.Size_T; Bytes : Positive) is
   begin
      if Count > C.Size_T (Interfaces.Integer_32'Last)
        or else (Count > 0 and then Data = System.Null_Address)
        or else Count > C.Size_T (Storage_Offset'Last) / C.Size_T (Bytes)
        or else (Count > 0
                 and then To_Integer (Data)
                          > Integer_Address'Last - Integer_Address (Count * C.Size_T (Bytes)))
      then
         raise Provider_Error with "invalid native calibration status span";
      end if;
   end Check;
   function Address_At
     (Data : System.Address; Index : Natural; Bytes : Positive) return System.Address
   is (Data + Storage_Offset (Index) * Storage_Offset (Bytes));
   function Copy (Raw : C.String_View_V1) return String is
   begin
      Check (Raw.Data, Raw.Size, 1);
      declare
         Value : String (1 .. Natural (Raw.Size));
      begin
         for I in Value'Range loop
            Value (I) :=
              Character'Val (Interfaces.C.char'Pos (To_Char (Address_At (Raw.Data, I - 1, 1)).all));
            if Value (I) = Character'Val (0) then
               raise Provider_Error with "NUL in native calibration status string";
            end if;
         end loop;
         return Value;
      end;
   end Copy;
   function Copy (Raw : C.UCI_ID_V1) return UCI_ID is
      Bytes : UUID;
   begin
      for I in Bytes'Range loop
         Bytes (I) := Byte (Raw.UUID (I));
      end loop;
      return Create_UCI_ID (Bytes, Copy (Raw.Descriptive_Label));
   end Copy;
   function Result_Of (Raw : Interfaces.Unsigned_32) return CS.Calibration_Result is
   begin
      --  MaxExclusive (5) and unknown values are rejected, never remapped.
      if Raw > 4 then
         raise Provider_Error with "unknown native calibration result";
      end if;
      return CS.Calibration_Result'Val (Raw);
   end Result_Of;
   function Copy (Raw : C.IR_Completed_Calibration_Item_V1) return CS.Completed_Item
   is (CS.Create_Item (Copy (Raw.Item_Name), Result_Of (Raw.Result), Copy (Raw.Fail_Reason)));
   function Copy (Raw : C.IR_Completed_Calibration_V1) return CS.Completed_Calibration is
   begin
      Check (Raw.Calibration_Items.Data, Raw.Calibration_Items.Size, Item_Bytes);
      declare
         Value : CS.Completed_Calibration :=
           CS.Create_Calibration
             (Copy (Raw.Calibration_ID),
              CS.Nanoseconds (Raw.Time_Tag_NS),
              Result_Of (Raw.Result),
              Copy (Raw.Fail_Reason));
      begin
         for J in 1 .. Natural (Raw.Calibration_Items.Size) loop
            CS.Append_Item
              (Value,
               Copy (To_Item (Address_At (Raw.Calibration_Items.Data, J - 1, Item_Bytes)).all));
         end loop;
         return Value;
      end;
   end Copy;

   function To_Value (Raw : C.IR_Calibration_Status_V1) return CS.Status_Value is
      Result : CS.Status_Value;
   begin
      --  Each collection is validated and copied independently; no lengths are zipped.
      Check (Raw.Active_Calibration_IDs.Data, Raw.Active_Calibration_IDs.Size, ID_Bytes);
      Check (Raw.Completed_Calibrations.Data, Raw.Completed_Calibrations.Size, Completed_Bytes);
      for I in 1 .. Natural (Raw.Active_Calibration_IDs.Size) loop
         CS.Append_Active
           (Result,
            Copy (To_ID (Address_At (Raw.Active_Calibration_IDs.Data, I - 1, ID_Bytes)).all));
      end loop;
      for I in 1 .. Natural (Raw.Completed_Calibrations.Size) loop
         CS.Append_Completed
           (Result,
            Copy
              (To_Completed
                 (Address_At (Raw.Completed_Calibrations.Data, I - 1, Completed_Bytes)).all));
      end loop;
      return Result;
   end To_Value;
end AMS.MEL.IR.Calibration_Status_Conversion;
