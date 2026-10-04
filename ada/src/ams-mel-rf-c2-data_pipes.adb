with Ada.Finalization;
with Ada.Containers;
with Interfaces.C;
with System;
with System.Storage_Elements;

package body AMS.MEL.RF.C2.Data_Pipes is
   package C renames AMS.MEL_C_API;
   package US renames Ada.Strings.Unbounded;
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
         raise Provider_Error
           with (if Last = 0 then "native VA DataPipe operation failed" else Text);
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
         raise Provider_Error with "invalid native VA DataPipe span";
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
      Handle : aliased C.RF_VA_Data_Pipe_Connections_Snapshot_Handle :=
        C.Null_RF_VA_Data_Pipe_Connections_Snapshot;
   end record;
   overriding
   procedure Finalize (Owner : in out Temporary_Owner) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored :=
        C.RF_VA_Data_Pipe_Connections_Snapshot_Close
          (Owner.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         null;
   end Finalize;
   function Snapshot (Object : Virtual_Aperture'Class) return Connection_Snapshot is
      Owner   : Temporary_Owner;
      D       : aliased Diagnostic := [others => Interfaces.C.nul];
      Address : aliased System.Address := System.Null_Address;
      Result  : Connection_Snapshot;
      type Raw_Groups is array (Natural range <>) of aliased C.RF_VA_Data_Pipe_Group_V1
      with Convention => C;
      type Raw_Pipes is array (Natural range <>) of aliased C.RF_Data_Pipe_Info_V1
      with Convention => C;
      type Raw_Endpoints is array (Natural range <>) of aliased Interfaces.Unsigned_64
      with Convention => C;
   begin
      Check
        (C.RF_VA_Get_Data_Pipes (Object.Handle, Owner.Handle'Access, D'Address, D'Length, null), D);
      Check
        (C.RF_VA_Data_Pipe_Connections_Snapshot_View
           (Owner.Handle, Address'Access, D'Address, D'Length, null),
         D);
      declare
         Checked : constant Natural :=
           Checked_Count
             ((Address, 1),
              C.RF_VA_Data_Pipe_Connections_Snapshot_V1'Size / System.Storage_Unit,
              C.RF_VA_Data_Pipe_Connections_Snapshot_V1'Alignment);
         View    : C.RF_VA_Data_Pipe_Connections_Snapshot_V1
         with Import, Address => Address;
         N       : constant Natural :=
           Checked_Count
             (View.Groups,
              C.RF_VA_Data_Pipe_Group_V1'Size / System.Storage_Unit,
              C.RF_VA_Data_Pipe_Group_V1'Alignment);
      begin
         if Checked /= 1 then
            raise Provider_Error with "invalid native VA DataPipe view";
         end if;
         if N > 0 then
            declare
               Raw : Raw_Groups (0 .. N - 1)
               with Import, Address => View.Groups.Data;
            begin
               for Item of Raw loop
                  declare
                     Group : Element_Group_Connection;
                     M     : constant Natural :=
                       Checked_Count
                         (Item.Data_Pipes,
                          C.RF_Data_Pipe_Info_V1'Size / System.Storage_Unit,
                          C.RF_Data_Pipe_Info_V1'Alignment);
                  begin
                     Group.Key := Copy_String (Item.Element_Group_Lookup_Label);
                     if M > 0 then
                        declare
                           Pipes : Raw_Pipes (0 .. M - 1)
                           with Import, Address => Item.Data_Pipes.Data;
                        begin
                           for Pipe of Pipes loop
                              declare
                                 Value : Data_Pipe_Connection;
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
                     Result.Groups.Append (Group);
                  end;
               end loop;
            end;
         end if;
      end;
      Check
        (C.RF_VA_Data_Pipe_Connections_Snapshot_Close
           (Owner.Handle'Access, D'Address, D'Length, null),
         D);
      return Result;
   end Snapshot;
   function Group_Count (Value : Connection_Snapshot) return Natural
   is (Natural (Value.Groups.Length));
   function Group_At (Value : Connection_Snapshot; Index : Positive) return Element_Group_Connection
   is (Value.Groups.Element (Index));
   function Element_Group_Label (Value : Element_Group_Connection) return String
   is (US.To_String (Value.Key));
   function Pipe_Count (Value : Element_Group_Connection) return Natural
   is (Natural (Value.Pipes.Length));
   function Pipe_At (Value : Element_Group_Connection; Index : Positive) return Data_Pipe_Connection
   is (Value.Pipes.Element (Index));
   function Lookup_Label (Value : Data_Pipe_Connection) return String
   is (US.To_String (Value.Key));
   function Label (Value : Data_Pipe_Connection) return String
   is (US.To_String (Value.Text));
   function Endpoint_Count (Value : Data_Pipe_Connection) return Natural
   is (Natural (Value.Endpoints.Length));
   function Endpoint_At
     (Value : Data_Pipe_Connection; Index : Positive) return Interfaces.Unsigned_64
   is (Value.Endpoints.Element (Index));
   function Input_View (Text : String) return C.String_View_V1 is
   begin
      for Item of Text loop
         if Item = Character'Val (0) then
            raise Constraint_Error with "DataPipe lookup label contains embedded NUL";
         end if;
      end loop;
      return
        (if Text'Length = 0
         then (System.Null_Address, 0)
         else (Text'Address, C.Size_T (Text'Length)));
   end Input_View;
   function Accepted_Value (Value : Interfaces.Unsigned_32) return Boolean is
   begin
      if Value > 1 then
         raise Provider_Error with "invalid native DataPipe acceptance Boolean";
      end if;
      return Value = 1;
   end Accepted_Value;
   function Associate_Endpoint
     (Object                               : in out Virtual_Aperture'Class;
      Element_Group_Label, Data_Pipe_Label : String;
      Endpoint_ID                          : Interfaces.Unsigned_64) return Boolean
   is
      Group    : constant C.String_View_V1 := Input_View (Element_Group_Label);
      Pipe     : constant C.String_View_V1 := Input_View (Data_Pipe_Label);
      D        : aliased Diagnostic := [others => Interfaces.C.nul];
      Accepted : aliased Interfaces.Unsigned_32 := 0;
   begin
      Check
        (C.RF_VA_Associate_Data_Pipe_Endpoint
           (Object.Handle,
            C.String_View_By_Copy_V1 (Group),
            C.String_View_By_Copy_V1 (Pipe),
            Endpoint_ID,
            Accepted'Access,
            D'Address,
            D'Length,
            null),
         D);
      return Accepted_Value (Accepted);
   end Associate_Endpoint;
   function Associate_Endpoints
     (Object                               : in out Virtual_Aperture'Class;
      Element_Group_Label, Data_Pipe_Label : String;
      Endpoint_IDs                         : Endpoint_ID_Array) return Boolean
   is
      Group    : constant C.String_View_V1 := Input_View (Element_Group_Label);
      Pipe     : constant C.String_View_V1 := Input_View (Data_Pipe_Label);
      D        : aliased Diagnostic := [others => Interfaces.C.nul];
      Accepted : aliased Interfaces.Unsigned_32 := 0;
      type Native_IDs is array (Natural range <>) of aliased Interfaces.Unsigned_64
      with Convention => C;
      IDs      : Native_IDs (Endpoint_IDs'Range);
      Span     : C.U64_Span_V1 := (System.Null_Address, 0);
   begin
      for I in Endpoint_IDs'Range loop
         IDs (I) := Endpoint_IDs (I);
      end loop;
      if IDs'Length > 0 then
         Span := (IDs'Address, C.Size_T (IDs'Length));
      end if;
      Check
        (C.RF_VA_Associate_Data_Pipe_Endpoints
           (Object.Handle,
            C.String_View_By_Copy_V1 (Group),
            C.String_View_By_Copy_V1 (Pipe),
            C.U64_Span_By_Copy_V1 (Span),
            Accepted'Access,
            D'Address,
            D'Length,
            null),
         D);
      return Accepted_Value (Accepted);
   end Associate_Endpoints;
end AMS.MEL.RF.C2.Data_Pipes;
