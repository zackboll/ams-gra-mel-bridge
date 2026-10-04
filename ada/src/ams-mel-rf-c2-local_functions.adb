with Ada.Finalization;
with Ada.Containers;
with Interfaces.C;
with System;
with System.Storage_Elements;

package body AMS.MEL.RF.C2.Local_Functions is
   package C renames AMS.MEL_C_API;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Unsigned_64;
   use type Interfaces.C.char;
   use type System.Address;
   use System.Storage_Elements;
   pragma
     Compile_Time_Error
       (C.Size_T'Size > Interfaces.Unsigned_64'Size,
        "LF counts require size_t representable in uint64");
   type Diagnostic is array (C.Size_T range 0 .. 511) of aliased Interfaces.C.char
   with Convention => C;
   procedure Check (Code : Interfaces.Integer_32; Buffer : Diagnostic) is
      Last : Natural := 0;
   begin
      if Code = C.Success then
         return;
      end if;
      while Last < Buffer'Length and then Buffer (C.Size_T (Last)) /= Interfaces.C.nul loop
         Last := Last + 1;
      end loop;
      declare
         Text : String (1 .. Last);
      begin
         for I in Text'Range loop
            Text (I) := Character'Val (Interfaces.C.char'Pos (Buffer (C.Size_T (I - 1))));
         end loop;
         raise Provider_Error with (if Last = 0 then "native LF query failed" else Text);
      end;
   end Check;
   function Checked_Count (Span : C.Span_V1; Element_Size, Alignment : Positive) return Natural is
      N : constant Interfaces.Unsigned_64 := Interfaces.Unsigned_64 (Span.Size);
   begin
      if N > Interfaces.Unsigned_64 (Natural'Last)
        or else N > Interfaces.Unsigned_64 (Ada.Containers.Count_Type'Last)
        or else N
                > Interfaces.Unsigned_64 (Storage_Offset'Last)
                  / Interfaces.Unsigned_64 (Element_Size)
        or else (N = 0 and then Span.Data /= System.Null_Address)
        or else (N > 0
                 and then (Span.Data = System.Null_Address
                           or else To_Integer (Span.Data) mod Integer_Address (Alignment) /= 0
                           or else To_Integer (Span.Data)
                                   > Integer_Address'Last
                                     - Integer_Address (N * Interfaces.Unsigned_64 (Element_Size))))
      then
         raise Provider_Error with "invalid native LF span";
      end if;
      return Natural (Span.Size);
   end Checked_Count;
   type Temporary_Owner is new Ada.Finalization.Limited_Controlled with record
      Catalog  : aliased C.RF_VA_LF_List_Handle := C.Null_RF_VA_LF_List;
      Statuses : aliased C.RF_VA_LF_Status_Handle := C.Null_RF_VA_LF_Status;
   end record;
   overriding
   procedure Finalize (Owner : in out Temporary_Owner) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.RF_VA_LF_List_Close (Owner.Catalog'Access, System.Null_Address, 0, null);
      Ignored := C.RF_VA_LF_Status_Close (Owner.Statuses'Access, System.Null_Address, 0, null);
   exception
      when others =>
         null;
   end Finalize;
   function Snapshot_Local_Functions (Object : Virtual_Aperture'Class) return Local_Function_List is
      Owner  : Temporary_Owner;
      D      : aliased Diagnostic := [others => Interfaces.C.nul];
      View   : aliased C.RF_VA_Local_Function_Info_Span_V1 := (System.Null_Address, 0);
      Result : Local_Function_List;
      type Raw_Infos is array (Natural range <>) of C.RF_VA_Local_Function_Info_V1
      with Convention => C;
   begin
      Check
        (C.RF_VA_Get_Local_Functions
           (Object.Handle, Owner.Catalog'Access, D'Address, D'Length, null),
         D);
      Check (C.RF_VA_LF_List_View (Owner.Catalog, View'Access, D'Address, D'Length, null), D);
      declare
         N : constant Natural :=
           Checked_Count
             (View,
              C.RF_VA_Local_Function_Info_V1'Size / System.Storage_Unit,
              C.RF_VA_Local_Function_Info_V1'Alignment);
      begin
         if N > 0 then
            declare
               Raw : Raw_Infos (0 .. N - 1)
               with Import, Address => View.Data;
            begin
               for Item of Raw loop
                  Result.Items.Append
                    (Local_Function_Info'(Item.Local_Function_Type_ID, Item.Instance_Count));
               end loop;
            end;
         end if;
      end;
      Check (C.RF_VA_LF_List_Close (Owner.Catalog'Access, D'Address, D'Length, null), D);
      return Result;
   end Snapshot_Local_Functions;
   function Snapshot_Local_Function_Status
     (Object                 : Virtual_Aperture'Class;
      VA_Instance_ID         : Interfaces.Unsigned_32;
      Local_Function_Type_ID : Interfaces.Unsigned_32) return Status_List
   is
      Owner  : Temporary_Owner;
      D      : aliased Diagnostic := [others => Interfaces.C.nul];
      View   : aliased C.Span_V1 := (System.Null_Address, 0);
      Result : Status_List;
      type Raw_Statuses is array (Natural range <>) of Interfaces.Unsigned_32 with Convention => C;
   begin
      Check
        (C.RF_VA_Get_Local_Function_Status
           (Object.Handle,
            VA_Instance_ID,
            Local_Function_Type_ID,
            Owner.Statuses'Access,
            D'Address,
            D'Length,
            null),
         D);
      Check (C.RF_VA_LF_Status_View (Owner.Statuses, View'Access, D'Address, D'Length, null), D);
      declare
         N : constant Natural :=
           Checked_Count
             (View,
              Interfaces.Unsigned_32'Size / System.Storage_Unit,
              Interfaces.Unsigned_32'Alignment);
      begin
         if N > 0 then
            declare
               Raw : Raw_Statuses (0 .. N - 1)
               with Import, Address => View.Data;
            begin
               for Item of Raw loop
                  if Item > 3 then
                     raise Provider_Error with "unknown native VirtualApertureStatus";
                  end if;
                  Result.Items.Append (Virtual_Aperture_Queries.Status_Kind'Val (Integer (Item)));
               end loop;
            end;
         end if;
      end;
      Check (C.RF_VA_LF_Status_Close (Owner.Statuses'Access, D'Address, D'Length, null), D);
      return Result;
   end Snapshot_Local_Function_Status;
   function Count (Value : Local_Function_List) return Natural
   is (Natural (Value.Items.Length));
   function Info_At (Value : Local_Function_List; Index : Positive) return Local_Function_Info
   is (Value.Items.Element (Index));
   function Type_ID (Value : Local_Function_Info) return Interfaces.Unsigned_32
   is (Value.ID);
   function Instance_Count (Value : Local_Function_Info) return Interfaces.Unsigned_64
   is (Value.Instances);
   function Count (Value : Status_List) return Natural
   is (Natural (Value.Items.Length));
   function Status_At
     (Value : Status_List; Index : Positive) return Virtual_Aperture_Queries.Status_Kind
   is (Value.Items.Element (Index));
end AMS.MEL.RF.C2.Local_Functions;
