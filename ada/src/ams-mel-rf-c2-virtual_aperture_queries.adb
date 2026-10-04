with Ada.Finalization;
with Ada.Containers;
with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;
with System.Storage_Elements;

package body AMS.MEL.RF.C2.Virtual_Aperture_Queries is
   package C renames AMS.MEL_C_API;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Unsigned_64;
   use type Interfaces.C.char;
   use type C.Size_T;
   use type System.Address;
   use System.Storage_Elements;
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
         raise Provider_Error with (if Last = 0 then "native VA query failed" else Text);
      end;
   end Check;
   function Decode (Value : Interfaces.Unsigned_32) return Status_Kind is
   begin
      if Value > 3 then
         raise Provider_Error with "unknown native VirtualApertureStatus";
      end if;
      return Status_Kind'Val (Integer (Value));
   end Decode;
   function Checked_Count (Span : C.Span_V1; Element_Size, Alignment : Positive) return Natural is
      N : constant Interfaces.Unsigned_64 := Interfaces.Unsigned_64 (Span.Size);
   begin
      if N > Interfaces.Unsigned_64 (Natural'Last)
        or else N > Interfaces.Unsigned_64 (Ada.Containers.Count_Type'Last)
        or else N
                > Interfaces.Unsigned_64 (Storage_Offset'Last)
                  / Interfaces.Unsigned_64 (Element_Size)
        or else (N > 0
                 and then (Span.Data = System.Null_Address
                           or else To_Integer (Span.Data) mod Integer_Address (Alignment) /= 0
                           or else To_Integer (Span.Data)
                                   > Integer_Address'Last
                                     - Integer_Address (N * Interfaces.Unsigned_64 (Element_Size))))
      then
         raise Provider_Error with "invalid native VA query span";
      end if;
      return Natural (Span.Size);
   end Checked_Count;
   type Temporary_Owner is new Ada.Finalization.Limited_Controlled with record
      List   : aliased C.RF_VA_Instance_List_Handle := C.Null_RF_VA_Instance_List;
      Report : aliased C.RF_VA_Instance_Status_Report_Handle := C.Null_RF_VA_Instance_Status_Report;
   end record;
   overriding
   procedure Finalize (Owner : in out Temporary_Owner) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.RF_VA_Instance_List_Close (Owner.List'Access, System.Null_Address, 0, null);
      Ignored :=
        C.RF_VA_Instance_Status_Report_Close (Owner.Report'Access, System.Null_Address, 0, null);
   exception
      when others =>
         null;
   end Finalize;
   function Query_ID (Object : Virtual_Aperture'Class) return Interfaces.Unsigned_32 is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      V : aliased Interfaces.Unsigned_32 := 0;
   begin
      Check (C.RF_VA_Get_ID (Object.Handle, V'Access, D'Address, D'Length, null), D);
      return V;
   end Query_ID;
   function Cached_Waveform_Supported (Object : Virtual_Aperture'Class) return Boolean is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      V : aliased Interfaces.Unsigned_32 := 0;
   begin
      Check
        (C.RF_VA_Cached_Waveform_Supported (Object.Handle, V'Access, D'Address, D'Length, null), D);
      if V > 1 then
         raise Provider_Error with "invalid native capability Boolean";
      end if;
      return V = 1;
   end Cached_Waveform_Supported;
   function Dynamic_Weights_Supported (Object : Virtual_Aperture'Class) return Boolean is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      V : aliased Interfaces.Unsigned_32 := 0;
   begin
      Check
        (C.RF_VA_Dynamic_Weights_Supported (Object.Handle, V'Access, D'Address, D'Length, null), D);
      if V > 1 then
         raise Provider_Error with "invalid native capability Boolean";
      end if;
      return V = 1;
   end Dynamic_Weights_Supported;
   function Query_Status (Object : Virtual_Aperture'Class) return Status_Kind is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      V : aliased Interfaces.Unsigned_32 := 0;
   begin
      Check (C.RF_VA_Get_Status (Object.Handle, V'Access, D'Address, D'Length, null), D);
      return Decode (V);
   end Query_Status;
   function Query_Instance_Status
     (Object : Virtual_Aperture'Class; Instance_ID : Interfaces.Unsigned_32) return Status_Kind
   is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      V : aliased Interfaces.Unsigned_32 := 0;
   begin
      Check
        (C.RF_VA_Get_Instance_Status
           (Object.Handle, Instance_ID, V'Access, D'Address, D'Length, null),
         D);
      return Decode (V);
   end Query_Instance_Status;
   function Copy_List (Owner : in out Temporary_Owner) return Instance_ID_List is
      D      : aliased Diagnostic := [others => Interfaces.C.nul];
      Span   : aliased C.Span_V1;
      Result : Instance_ID_List;
      type Raw_Array is array (Natural range <>) of aliased Interfaces.Unsigned_32
      with Convention => C;
   begin
      Check (C.RF_VA_Instance_List_View (Owner.List, Span'Access, D'Address, D'Length, null), D);
      declare
         N : constant Natural :=
           Checked_Count
             (Span,
              Interfaces.Unsigned_32'Size / System.Storage_Unit,
              Interfaces.Unsigned_32'Alignment);
      begin
         if N > 0 then
            declare
               Raw : Raw_Array (0 .. N - 1)
               with Import, Address => Span.Data;
            begin
               for ID of Raw loop
                  Result.IDs.Append (ID);
               end loop;
            end;
         end if;
      end;
      Check (C.RF_VA_Instance_List_Close (Owner.List'Access, D'Address, D'Length, null), D);
      return Result;
   end Copy_List;
   function Snapshot_All_Instances (Object : Virtual_Aperture'Class) return Instance_ID_List is
      Owner : Temporary_Owner;
      D     : aliased Diagnostic := [others => Interfaces.C.nul];
   begin
      Check
        (C.RF_VA_Get_All_Instances (Object.Handle, Owner.List'Access, D'Address, D'Length, null),
         D);
      return Copy_List (Owner);
   end Snapshot_All_Instances;
   function Snapshot_Instances
     (Object : Virtual_Aperture'Class; Face_ID : Interfaces.Unsigned_32) return Instance_ID_List
   is
      Owner : Temporary_Owner;
      D     : aliased Diagnostic := [others => Interfaces.C.nul];
   begin
      Check
        (C.RF_VA_Get_Instances
           (Object.Handle, Face_ID, Owner.List'Access, D'Address, D'Length, null),
         D);
      return Copy_List (Owner);
   end Snapshot_Instances;
   function Snapshot_Instance_Status_Report
     (Object : Virtual_Aperture'Class; Instance_ID : Interfaces.Unsigned_32)
      return Instance_Status_Report
   is
      Owner   : Temporary_Owner;
      D       : aliased Diagnostic := [others => Interfaces.C.nul];
      Address : aliased System.Address := System.Null_Address;
      Result  : Instance_Status_Report;
      type Report_Access is access all C.RF_VA_Instance_Status_Report_V1;
      function To_Report is new Ada.Unchecked_Conversion (System.Address, Report_Access);
      type Raw_Groups is array (Natural range <>) of aliased C.RF_VA_Local_Function_Status_V1
      with Convention => C;
      type Raw_Statuses is array (Natural range <>) of aliased Interfaces.Unsigned_32
      with Convention => C;
   begin
      Check
        (C.RF_VA_Get_Instance_Status_Report
           (Object.Handle, Instance_ID, Owner.Report'Access, D'Address, D'Length, null),
         D);
      Check
        (C.RF_VA_Instance_Status_Report_View
           (Owner.Report, Address'Access, D'Address, D'Length, null),
         D);
      if Address = System.Null_Address then
         raise Provider_Error with "null native VA report";
      end if;
      declare
         Checked : constant Natural :=
           Checked_Count
             ((Address, 1),
              C.RF_VA_Instance_Status_Report_V1'Size / System.Storage_Unit,
              C.RF_VA_Instance_Status_Report_V1'Alignment);
         View    : constant C.RF_VA_Instance_Status_Report_V1 := To_Report (Address).all;
         N       : constant Natural :=
           Checked_Count
             (View.Local_Functions,
              C.RF_VA_Local_Function_Status_V1'Size / System.Storage_Unit,
              C.RF_VA_Local_Function_Status_V1'Alignment);
      begin
         if Checked /= 1 then
            raise Provider_Error with "invalid native VA report";
         end if;
         Result.ID := View.VA_Instance_ID;
         Result.State := Decode (View.Status);
         if N > 0 then
            declare
               Groups : Raw_Groups (0 .. N - 1)
               with Import, Address => View.Local_Functions.Data;
            begin
               for Item of Groups loop
                  declare
                     Group : Local_Function_Status_Group;
                     M     : constant Natural :=
                       Checked_Count
                         (Item.Statuses,
                          Interfaces.Unsigned_32'Size / System.Storage_Unit,
                          Interfaces.Unsigned_32'Alignment);
                  begin
                     Group.ID := Item.Local_Function_Type_ID;
                     if M > 0 then
                        declare
                           Values : Raw_Statuses (0 .. M - 1)
                           with Import, Address => Item.Statuses.Data;
                        begin
                           for Value of Values loop
                              Group.Statuses.Append (Decode (Value));
                           end loop;
                        end;
                     end if;
                     Result.Groups.Append (Group);
                  end;
               end loop;
            end;
         end if;
      end;
      Check
        (C.RF_VA_Instance_Status_Report_Close (Owner.Report'Access, D'Address, D'Length, null), D);
      return Result;
   end Snapshot_Instance_Status_Report;
   function Count (Value : Instance_ID_List) return Natural
   is (Natural (Value.IDs.Length));
   function Instance_ID_At
     (Value : Instance_ID_List; Index : Positive) return Interfaces.Unsigned_32
   is (Value.IDs.Element (Index));
   function Local_Function_Type_ID
     (Value : Local_Function_Status_Group) return Interfaces.Unsigned_32
   is (Value.ID);
   function Instance_Status_Count (Value : Local_Function_Status_Group) return Natural
   is (Natural (Value.Statuses.Length));
   function Instance_Status_At
     (Value : Local_Function_Status_Group; Index : Positive) return Status_Kind
   is (Value.Statuses.Element (Index));
   function Instance_ID (Value : Instance_Status_Report) return Interfaces.Unsigned_32
   is (Value.ID);
   function Status (Value : Instance_Status_Report) return Status_Kind
   is (Value.State);
   function Local_Function_Type_Count (Value : Instance_Status_Report) return Natural
   is (Natural (Value.Groups.Length));
   function Local_Function_Group_At
     (Value : Instance_Status_Report; Index : Positive) return Local_Function_Status_Group
   is (Value.Groups.Element (Index));
end AMS.MEL.RF.C2.Virtual_Aperture_Queries;
