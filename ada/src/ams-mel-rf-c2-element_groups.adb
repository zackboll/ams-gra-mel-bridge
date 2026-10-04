with Ada.Finalization;
with Ada.Containers;
with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;
with System.Storage_Elements;

package body AMS.MEL.RF.C2.Element_Groups is
   package C renames AMS.MEL_C_API;
   package US renames Ada.Strings.Unbounded;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Unsigned_64;
   use type Interfaces.C.char;
   use type C.Size_T;
   use type System.Address;
   use System.Storage_Elements;
   pragma
     Compile_Time_Error
       (Interfaces.C.double'Size /= Interfaces.Unsigned_64'Size
          or else Long_Float'Size < Interfaces.C.double'Size
          or else Long_Float'Digits < Interfaces.C.double'Digits,
        "element descriptors require lossless C double conversion/storage");
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
         raise Provider_Error
           with (if Last = 0 then "native element-group snapshot failed" else Text);
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
         raise Provider_Error with "invalid native element-group span";
      end if;
      return Natural (Span.Size);
   end Checked_Count;
   function Copy_String (Value : C.String_View_V1) return US.Unbounded_String is
      N    : constant Natural := Checked_Count ((Value.Data, Value.Size), 1, 1);
      Text : String (1 .. N);
   begin
      if N > 0 then
         declare
            Raw : String (1 .. N)
            with Import, Address => Value.Data;
         begin
            Text := Raw;
         end;
      end if;
      return US.To_Unbounded_String (Text);
   end Copy_String;
   type Temporary_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased C.RF_Element_Group_Snapshot_Handle := C.Null_RF_Element_Group_Snapshot;
   end record;
   overriding
   procedure Finalize (Owner : in out Temporary_Owner) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored :=
        C.RF_Element_Group_Snapshot_Close (Owner.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         null;
   end Finalize;
   function Copy_Numbers (Address : System.Address) return Numeric_Storage is
      --  Narrowly scoped: IEEE infinity/NaN are descriptive numeric values.
      pragma Validity_Checks ("F");
      Raw : C.RF_Element_Group_Descriptor_V1
      with Import, Address => Address;
      function Bits is new Ada.Unchecked_Conversion (Interfaces.C.double, Interfaces.Unsigned_64);
   begin
      return
        [Bits (Raw.Max_RF_Bandwidth_Hz),
         Bits (Raw.Max_Sample_Rate_Samples_Per_Second),
         Bits (Raw.Max_Data_Rate_Bits_Per_Second),
         Bits (Raw.Max_Duty_Factor)];
   end Copy_Numbers;
   function Number (Bits : Interfaces.Unsigned_64) return Long_Float is
      pragma Validity_Checks ("F");
      function Decode is new Ada.Unchecked_Conversion (Interfaces.Unsigned_64, Interfaces.C.double);
   begin
      return Long_Float (Decode (Bits));
   end Number;
   function Snapshot_Element_Groups
     (Object : Virtual_Aperture'Class; Include_Data_Pipes : Boolean := False)
      return Element_Group_List
   is
      Owner   : Temporary_Owner;
      Options : aliased C.RF_Element_Group_Snapshot_Options_V1 :=
        (Include_Data_Pipes => Boolean'Pos (Include_Data_Pipes));
      D       : aliased Diagnostic := [others => Interfaces.C.nul];
      Address : aliased System.Address := System.Null_Address;
      Result  : Element_Group_List;
      type Raw_Descriptors is array (Natural range <>) of aliased C.RF_Element_Group_Descriptor_V1
      with Convention => C;
      type Raw_Pipes is array (Natural range <>) of aliased C.RF_Data_Pipe_Info_V1
      with Convention => C;
      type Raw_Endpoints is array (Natural range <>) of aliased Interfaces.Unsigned_64
      with Convention => C;
   begin
      Check
        (C.RF_VA_Get_Element_Groups
           (Object.Handle, Options'Access, Owner.Handle'Access, D'Address, D'Length, null),
         D);
      Check
        (C.RF_Element_Group_Snapshot_View (Owner.Handle, Address'Access, D'Address, D'Length, null),
         D);
      declare
         Checked : constant Natural :=
           Checked_Count
             ((Address, 1),
              C.RF_Element_Group_Snapshot_V1'Size / System.Storage_Unit,
              C.RF_Element_Group_Snapshot_V1'Alignment);
         View    : C.RF_Element_Group_Snapshot_V1
         with Import, Address => Address;
         N       : constant Natural :=
           Checked_Count
             (View.Descriptors,
              C.RF_Element_Group_Descriptor_V1'Size / System.Storage_Unit,
              C.RF_Element_Group_Descriptor_V1'Alignment);
      begin
         if Checked /= 1
           or else View.Data_Pipes_Included > 1
           or else View.Data_Pipes_Included /= Options.Include_Data_Pipes
         then
            raise Provider_Error with "invalid native pipe inclusion flag";
         end if;
         Result.Included := View.Data_Pipes_Included = 1;
         if N > 0 then
            declare
               Raw : Raw_Descriptors (0 .. N - 1)
               with Import, Address => View.Descriptors.Data;
            begin
               for I in Raw'Range loop
                  declare
                     Group : Element_Group_Descriptor;
                     M     : constant Natural :=
                       Checked_Count
                         (Raw (I).Data_Pipes,
                          C.RF_Data_Pipe_Info_V1'Size / System.Storage_Unit,
                          C.RF_Data_Pipe_Info_V1'Alignment);
                  begin
                     if Raw (I).Mode > 1 or else (not Result.Included and then M /= 0) then
                        raise Provider_Error with "invalid native element-group mode/pipes";
                     end if;
                     Group.Key := Copy_String (Raw (I).Lookup_Label);
                     Group.Text := Copy_String (Raw (I).Label);
                     Group.State := Element_Group_Mode'Val (Integer (Raw (I).Mode));
                     Group.Included := Result.Included;
                     Group.Numbers := Copy_Numbers (Raw (I)'Address);
                     if M > 0 then
                        declare
                           Pipes : Raw_Pipes (0 .. M - 1)
                           with Import, Address => Raw (I).Data_Pipes.Data;
                        begin
                           for Pipe of Pipes loop
                              declare
                                 Value : Data_Pipe_Info;
                                 K     : constant Natural :=
                                   Checked_Count
                                     ((Pipe.Associated_Endpoint_IDs.Data,
                                       Pipe.Associated_Endpoint_IDs.Size),
                                      Interfaces.Unsigned_64'Size / System.Storage_Unit,
                                      Interfaces.Unsigned_64'Alignment);
                              begin
                                 Value.Key := Copy_String (Pipe.Lookup_Label);
                                 Value.Text := Copy_String (Pipe.Label);
                                 if K > 0 then
                                    declare
                                       IDs : Raw_Endpoints (0 .. K - 1)
                                       with Import, Address => Pipe.Associated_Endpoint_IDs.Data;
                                    begin
                                       for ID of IDs loop
                                          Value.Endpoints.Append (ID);
                                       end loop;
                                    end;
                                 end if;
                                 Group.Pipes.Append (Value);
                              end;
                           end loop;
                        end;
                     end if;
                     Result.Descriptors.Append (Group);
                  end;
               end loop;
            end;
         end if;
      end;
      Check (C.RF_Element_Group_Snapshot_Close (Owner.Handle'Access, D'Address, D'Length, null), D);
      return Result;
   end Snapshot_Element_Groups;
   function Count (Value : Element_Group_List) return Natural
   is (Natural (Value.Descriptors.Length));
   function Descriptor_At
     (Value : Element_Group_List; Index : Positive) return Element_Group_Descriptor
   is (Value.Descriptors.Element (Index));
   function Data_Pipes_Included (Value : Element_Group_List) return Boolean
   is (Value.Included);
   function Data_Pipes_Included (Value : Element_Group_Descriptor) return Boolean
   is (Value.Included);
   function Lookup_Label (Value : Element_Group_Descriptor) return String
   is (US.To_String (Value.Key));
   function Label (Value : Element_Group_Descriptor) return String
   is (US.To_String (Value.Text));
   function Mode (Value : Element_Group_Descriptor) return Element_Group_Mode
   is (Value.State);
   function Max_RF_Bandwidth_Hz (Value : Element_Group_Descriptor) return Long_Float is
      pragma Validity_Checks ("F");
   begin
      return Number (Value.Numbers (1));
   end Max_RF_Bandwidth_Hz;
   function Max_Sample_Rate_Samples_Per_Second (Value : Element_Group_Descriptor) return Long_Float
   is
      pragma Validity_Checks ("F");
   begin
      return Number (Value.Numbers (2));
   end Max_Sample_Rate_Samples_Per_Second;
   function Max_Data_Rate_Bits_Per_Second (Value : Element_Group_Descriptor) return Long_Float is
      pragma Validity_Checks ("F");
   begin
      return Number (Value.Numbers (3));
   end Max_Data_Rate_Bits_Per_Second;
   function Max_Duty_Factor (Value : Element_Group_Descriptor) return Long_Float is
      pragma Validity_Checks ("F");
   begin
      return Number (Value.Numbers (4));
   end Max_Duty_Factor;
   function Pipe_Count (Value : Element_Group_Descriptor) return Natural
   is (Natural (Value.Pipes.Length));
   function Pipe_At (Value : Element_Group_Descriptor; Index : Positive) return Data_Pipe_Info
   is (Value.Pipes.Element (Index));
   function Lookup_Label (Value : Data_Pipe_Info) return String
   is (US.To_String (Value.Key));
   function Label (Value : Data_Pipe_Info) return String
   is (US.To_String (Value.Text));
   function Endpoint_Count (Value : Data_Pipe_Info) return Natural
   is (Natural (Value.Endpoints.Length));
   function Endpoint_At (Value : Data_Pipe_Info; Index : Positive) return Interfaces.Unsigned_64
   is (Value.Endpoints.Element (Index));
end AMS.MEL.RF.C2.Element_Groups;
