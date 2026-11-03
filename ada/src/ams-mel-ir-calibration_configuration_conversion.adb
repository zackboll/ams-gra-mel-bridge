with Ada.Unchecked_Conversion;
with Interfaces.C;
with System.Storage_Elements;

package body AMS.MEL.IR.Calibration_Configuration_Conversion is
   package C renames AMS.MEL_C_API;
   package CC renames Calibration_Configuration;
   use type C.Size_T;
   use type Interfaces.Unsigned_32;
   use type System.Address;
   use System.Storage_Elements;
   type Entry_Access is access all C.IR_Subsystem_Calibration_V1;
   type Name_Access is access all C.String_View_V1;
   type ID_Access is access all C.UCI_ID_V1;
   type Char_Access is access all Interfaces.C.char;
   function To_Entry is new Ada.Unchecked_Conversion (System.Address, Entry_Access);
   function To_Name is new Ada.Unchecked_Conversion (System.Address, Name_Access);
   function To_ID is new Ada.Unchecked_Conversion (System.Address, ID_Access);
   function To_Char is new Ada.Unchecked_Conversion (System.Address, Char_Access);
   function To_Raw
     (Command : CC.Configuration_Command) return C.IR_Calibration_Configuration_Command_V1
   is ((Interfaces.Unsigned_32 (Command.Command_ID), Interfaces.Unsigned_32 (Command.Request_ID)));

   procedure Check (Data : System.Address; Count : C.Size_T; Bytes : Positive) is
   begin
      if Count > C.Size_T (Natural'Last)
        or else (Count > 0 and then Data = System.Null_Address)
        or else Count > C.Size_T (Storage_Offset'Last) / C.Size_T (Bytes)
        or else (Count > 0
                 and then To_Integer (Data)
                          > Integer_Address'Last - Integer_Address (Count * C.Size_T (Bytes)))
      then
         raise Provider_Error with "invalid native calibration span";
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
               raise Provider_Error with "NUL in native calibration string";
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
   function To_Collection (Raw : C.IR_Calibration_Configuration_V1) return CC.Calibration_Collection
   is
      Result : CC.Calibration_Collection;
   begin
      Check (Raw.Data, Raw.Size, C.IR_Subsystem_Calibration_V1'Size / System.Storage_Unit);
      for I in 1 .. Natural (Raw.Size) loop
         declare
            Source : constant C.IR_Subsystem_Calibration_V1 :=
              To_Entry
                (Address_At
                   (Raw.Data, I - 1, C.IR_Subsystem_Calibration_V1'Size / System.Storage_Unit)).all;
            Value  : CC.Subsystem_Calibration;
         begin
            if Source.Accepted_Interface > 2 then
               raise Provider_Error with "unknown native calibration accepted interface";
            end if;
            Check
              (Source.Calibration_Item_Names.Data,
               Source.Calibration_Item_Names.Size,
               C.String_View_V1'Size / System.Storage_Unit);
            Check
              (Source.Subsystem_Component_IDs.Data,
               Source.Subsystem_Component_IDs.Size,
               C.UCI_ID_V1'Size / System.Storage_Unit);
            Check
              (Source.Capability_IDs.Data,
               Source.Capability_IDs.Size,
               C.UCI_ID_V1'Size / System.Storage_Unit);
            Value.Calibration_ID := Copy (Source.Calibration_ID);
            Value.Interface_Value := CC.Accepted_Interface'Val (Source.Accepted_Interface);
            for J in 1 .. Natural (Source.Calibration_Item_Names.Size) loop
               Value.Item_Names.Append
                 (Copy
                    (To_Name
                       (Address_At
                          (Source.Calibration_Item_Names.Data,
                           J - 1,
                           C.String_View_V1'Size / System.Storage_Unit)).all));
            end loop;
            for J in 1 .. Natural (Source.Subsystem_Component_IDs.Size) loop
               Value.Component_IDs.Append
                 (Copy
                    (To_ID
                       (Address_At
                          (Source.Subsystem_Component_IDs.Data,
                           J - 1,
                           C.UCI_ID_V1'Size / System.Storage_Unit)).all));
            end loop;
            for J in 1 .. Natural (Source.Capability_IDs.Size) loop
               Value.Capability_IDs.Append
                 (Copy
                    (To_ID
                       (Address_At
                          (Source.Capability_IDs.Data,
                           J - 1,
                           C.UCI_ID_V1'Size / System.Storage_Unit)).all));
            end loop;
            CC.Append (Result, Value);
         end;
      end loop;
      return Result;
   end To_Collection;
end AMS.MEL.IR.Calibration_Configuration_Conversion;
