with Ada.Unchecked_Conversion;
with Interfaces.C;
with System.Storage_Elements;

package body AMS.MEL.RF.Product_Rx is
   package C renames AMS.MEL_C_API;
   package US renames Ada.Strings.Unbounded;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_64;
   use type Interfaces.C.char;
   use type C.Size_T;
   use type C.RF_Product_Rx_Request_Handle;
   use type C.RF_Product_Rx_Handle;
   use type C.RF_Product_Rx_Event_Handle;
   use type System.Address;
   use type System.Storage_Elements.Integer_Address;
   type Diagnostic is array (C.Size_T range <>) of aliased Interfaces.C.char with Convention => C;
   subtype Fixed_Diagnostic is Diagnostic (0 .. 511);

   procedure Check_Complex_I16_Representation is
   begin
      --  Imported C type Object_Size and Alignment are not compile-time static
      --  on every supported GNAT. Check before any native span can be borrowed.
      if Complex_I16'Object_Size /= C.RF_Complex_I16_V1'Object_Size then
         raise Program_Error with "Complex_I16 object size does not match the C ABI";
      end if;
      if Complex_I16'Alignment /= C.RF_Complex_I16_V1'Alignment then
         raise Program_Error with "Complex_I16 alignment does not match the C ABI";
      end if;
   end Check_Complex_I16_Representation;

   function Checked_Count (Span : C.Span_V1) return Natural is
   begin
      if Interfaces.Unsigned_64 (Span.Size) > Interfaces.Unsigned_64 (Natural'Last)
        or else Interfaces.Unsigned_64 (Span.Size)
                > Interfaces.Unsigned_64 (Ada.Containers.Count_Type'Last)
        or else (Span.Size > 0 and then Span.Data = System.Null_Address)
      then
         raise Provider_Error with "invalid RF event span";
      end if;
      return Natural (Span.Size);
   end Checked_Count;

   function Binary (Value : Interfaces.Unsigned_32) return Boolean is
   begin
      if Value > 1 then
         raise Provider_Error with "invalid RF event boolean";
      end if;
      return Value = 1;
   end Binary;

   function Message (D : Diagnostic) return String is
      Last : Natural := 0;
   begin
      while Last < D'Length and then D (C.Size_T (Last)) /= Interfaces.C.nul loop
         Last := Last + 1;
      end loop;
      declare
         Text : String (1 .. Last);
      begin
         for I in Text'Range loop
            Text (I) := Character'Val (Interfaces.C.char'Pos (D (C.Size_T (I - 1))));
         end loop;
         return (if Last = 0 then "native RF ProductRx operation failed" else Text);
      end;
   end Message;

   procedure Check (Code : Interfaces.Integer_32; D : Diagnostic) is
   begin
      if Code = C.Timeout then
         raise Timeout_Error;
      elsif Code = C.Stream_Stopped then
         raise Stream_Stopped;
      elsif Code /= C.Success then
         raise Provider_Error with Message (D);
      end if;
   end Check;

   function Create_Config
     (Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Positive;
      Max_Samples_Per_Event : Positive) return Config
   is ((Region  => Region_Size_Bytes,
        Queue   => Queue_Capacity,
        Maximum => Max_Samples_Per_Event,
        Format  => Complex_INT16));

   function Create_Config_Complex_I8
     (Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Positive;
      Max_Samples_Per_Event : Positive) return Config
   is ((Region  => Region_Size_Bytes,
        Queue   => Queue_Capacity,
        Maximum => Max_Samples_Per_Event,
        Format  => Complex_INT8));

   function Create_Config_Direct_I8
     (Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Positive;
      Max_Samples_Per_Event : Positive) return Config
   is ((Region  => Region_Size_Bytes,
        Queue   => Queue_Capacity,
        Maximum => Max_Samples_Per_Event,
        Format  => Direct_INT8));

   function Create_Config_Direct_I16
     (Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Positive;
      Max_Samples_Per_Event : Positive) return Config
   is ((Region  => Region_Size_Bytes,
        Queue   => Queue_Capacity,
        Maximum => Max_Samples_Per_Event,
        Format  => Direct_INT16));

   function Submit (Parent : Data_MEL; Value : Config) return Create_Request is
      Raw : aliased constant C.RF_Product_Rx_Config_V1 :=
        (Data_Format           => Interfaces.Unsigned_32 (Value.Format),
         Region_Size_Bytes     => Value.Region,
         Queue_Capacity        => C.Size_T (Value.Queue),
         Max_Samples_Per_Event => C.Size_T (Value.Maximum));
      D   : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R   : aliased C.Size_T := 0;
   begin
      if not Is_Open (Parent) then
         raise Provider_Error with "RF DataMEL is closed";
      end if;
      return Result : Create_Request do
         if Value.Format = AMS_VITA_Extra_Large then
            Check
              (C.RF_Data_Submit_Product_Rx_V7
                 (Parent.Handle, Raw'Access, Result.Handle'Access, D'Address, D'Length, R'Access),
               D);
         elsif Value.Format = AMS_VITA_Large then
            Check
              (C.RF_Data_Submit_Product_Rx_V6
                 (Parent.Handle, Raw'Access, Result.Handle'Access, D'Address, D'Length, R'Access),
               D);
         elsif Value.Format = AMS_VITA_Medium then
            Check
              (C.RF_Data_Submit_Product_Rx_V5
                 (Parent.Handle, Raw'Access, Result.Handle'Access, D'Address, D'Length, R'Access),
               D);
         elsif Value.Format = AMS_VITA_Small then
            Check
              (C.RF_Data_Submit_Product_Rx_V4
                 (Parent.Handle, Raw'Access, Result.Handle'Access, D'Address, D'Length, R'Access),
               D);
         elsif Value.Format = Direct_INT8 or else Value.Format = Direct_INT16 then
            Check
              (C.RF_Data_Submit_Product_Rx_V3
                 (Parent.Handle, Raw'Access, Result.Handle'Access, D'Address, D'Length, R'Access),
               D);
         elsif Value.Format = Complex_INT8 then
            Check
              (C.RF_Data_Submit_Product_Rx_V2
                 (Parent.Handle, Raw'Access, Result.Handle'Access, D'Address, D'Length, R'Access),
               D);
         else
            Check
              (C.RF_Data_Submit_Product_Rx
                 (Parent.Handle, Raw'Access, Result.Handle'Access, D'Address, D'Length, R'Access),
               D);
         end if;
      end return;
   end Submit;

   function Is_Open (Request : Create_Request) return Boolean
   is (Request.Handle /= C.Null_RF_Product_Rx_Request);

   procedure Close (Request : in out Create_Request) is
      D : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check
        (C.RF_Product_Rx_Request_Close (Request.Handle'Access, D'Address, D'Length, R'Access), D);
   end Close;

   overriding
   procedure Finalize (Request : in out Create_Request) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored :=
        C.RF_Product_Rx_Request_Close (Request.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Request.Handle := C.Null_RF_Product_Rx_Request;
   end Finalize;

   function Outcome (Result : Create_Result) return Create_Outcome
   is (Result.State);
   function Error_Code (Result : Create_Result) return Create_Error_Code
   is (Result.Code);
   function Description (Result : Create_Result) return String
   is (US.To_String (Result.Text));

   function Wait (Request : Create_Request; Timeout_Milliseconds : Natural) return Create_Result is
      Raw    : aliased C.RF_Product_Rx_Request_Result_V1 := (Error_Code => 0);
      D      : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R      : aliased C.Size_T := 0;
      Status : constant Interfaces.Integer_32 :=
        C.RF_Product_Rx_Request_Wait
          (Request.Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Raw'Access,
           D'Address,
           D'Length,
           R'Access);
   begin
      if Status = C.Success then
         if Raw.Error_Code /= 0 then
            raise Provider_Error with "invalid successful RF creation result";
         end if;
         return (State => Created, Code => None, Text => US.Null_Unbounded_String);
      elsif Status = C.Provider_Failed then
         if Raw.Error_Code > Create_Error_Code'Enum_Rep (Unsupported) then
            raise Provider_Error with Message (D);
         end if;
         if R = 0 then
            raise Provider_Error with "invalid RF creation diagnostic length";
         end if;
         if Interfaces.Unsigned_64 (R) > Interfaces.Unsigned_64 (Natural'Last) then
            raise Provider_Error with "RF creation diagnostic length is not representable";
         end if;
         if R > D'Length then
            declare
               Full     : aliased Diagnostic (0 .. R - 1) := [others => Interfaces.C.nul];
               Again    : aliased C.RF_Product_Rx_Request_Result_V1 := (Error_Code => 0);
               Required : aliased C.Size_T := 0;
               Retry    : constant Interfaces.Integer_32 :=
                 C.RF_Product_Rx_Request_Wait
                   (Request.Handle, 0, Again'Access, Full'Address, Full'Length, Required'Access);
            begin
               if Retry /= Status or else Again.Error_Code /= Raw.Error_Code or else Required /= R
               then
                  raise Provider_Error with "RF creation terminal result changed during retry";
               end if;
               return
                 (State => Failed,
                  Code  => Create_Error_Code'Enum_Val (Raw.Error_Code),
                  Text  => US.To_Unbounded_String (Message (Full)));
            end;
         end if;
         return
           (State => Failed,
            Code  => Create_Error_Code'Enum_Val (Raw.Error_Code),
            Text  => US.To_Unbounded_String (Message (D)));
      else
         if Status = C.Timeout then
            raise Timeout_Error;
         end if;
         raise Provider_Error with Message (D);
      end if;
   end Wait;

   function Claim (Request : Create_Request'Class) return Endpoint is
      Info : aliased C.RF_Product_Rx_Info_V1 := (Endpoint_ID => 0, Assigned_Data_Format => 0);
      D    : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
   begin
      return Result : Endpoint do
         Check
           (C.RF_Product_Rx_Request_Claim
              (Request.Handle, Result.Handle'Access, Info'Access, D'Address, D'Length, R'Access),
            D);
         if Result.Handle = C.Null_RF_Product_Rx then
            raise Provider_Error with "null RF ProductRx endpoint after claim";
         end if;
         if Info.Assigned_Data_Format > Interfaces.Unsigned_32 (AMS_VITA_Extra_Large) then
            raise Provider_Error with "unexpected RF ProductRx format";
         end if;
         Result.ID := Info.Endpoint_ID;
         Result.Format := Job_Data_Format (Info.Assigned_Data_Format);
      end return;
   end Claim;

   function Is_Open (Object : Endpoint) return Boolean
   is (Object.Handle /= C.Null_RF_Product_Rx);
   function Endpoint_ID (Object : Endpoint) return Interfaces.Unsigned_64
   is (Object.ID);
   function Assigned_Data_Format (Object : Endpoint) return Job_Data_Format
   is (Object.Format);

   procedure Close (Object : in out Endpoint) is
      D : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check (C.RF_Product_Rx_Close (Object.Handle'Access, D'Address, D'Length, R'Access), D);
   end Close;

   overriding
   procedure Finalize (Object : in out Endpoint) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.RF_Product_Rx_Close (Object.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Object.Handle := C.Null_RF_Product_Rx;
   end Finalize;

   function Statistics (Object : Endpoint) return Counters is
      Raw : aliased C.RF_Product_Rx_Counters_V1 := (others => 0);
      D   : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R   : aliased C.Size_T := 0;
   begin
      Check
        (C.RF_Product_Rx_Get_Counters (Object.Handle, Raw'Access, D'Address, D'Length, R'Access),
         D);
      return
        (Counter (Raw.Callbacks_Received),
         Counter (Raw.Products_Queued),
         Counter (Raw.Products_Dropped_Queue_Full),
         Counter (Raw.Malformed_Or_Unsupported),
         Counter (Raw.Allocation_Failures),
         Counter (Raw.Callbacks_After_Close));
   end Statistics;

   type View_Access is access all C.RF_Product_Rx_Event_V1;
   function To_View is new Ada.Unchecked_Conversion (System.Address, View_Access);
   type View_V2_Access is access all C.RF_Product_Rx_Event_V2;
   function To_View_V2 is new Ada.Unchecked_Conversion (System.Address, View_V2_Access);
   type View_V3_Access is access all C.RF_Product_Rx_Event_V3;
   function To_View_V3 is new Ada.Unchecked_Conversion (System.Address, View_V3_Access);
   type View_V4_Access is access all C.RF_Product_Rx_Event_V4;
   function To_View_V4 is new Ada.Unchecked_Conversion (System.Address, View_V4_Access);

   type View_V5_Access is access all C.RF_Product_Rx_Event_V5;
   function To_View_V5 is new Ada.Unchecked_Conversion (System.Address, View_V5_Access);

   type View_V6_Access is access all C.RF_Product_Rx_Event_V6;
   function To_View_V6 is new Ada.Unchecked_Conversion (System.Address, View_V6_Access);

   type View_V7_Access is access all C.RF_Product_Rx_Event_V7;
   function To_View_V7 is new Ada.Unchecked_Conversion (System.Address, View_V7_Access);

   function Receive (Object : Endpoint'Class; Timeout_Milliseconds : Natural := 0) return Event is
      D              : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R              : aliased C.Size_T := 0;
      Address        : aliased System.Address := System.Null_Address;
      Sample_Span    : C.Span_V1;
      Event_ID       : Interfaces.Unsigned_64;
      Event_Format   : Interfaces.Unsigned_32;
      Event_Metadata : C.RF_Product_Rx_Metadata_V1;
   begin
      return Result : Event do
         Check
           (C.RF_Product_Rx_Receive
              (Object.Handle,
               Interfaces.Unsigned_32 (Timeout_Milliseconds),
               Result.Handle'Access,
               D'Address,
               D'Length,
               R'Access),
            D);
         if Result.Handle = C.Null_RF_Product_Rx_Event then
            raise Provider_Error with "null RF ProductRx event after receive";
         end if;
         if Object.Format = AMS_VITA_Extra_Large then
            Check
              (C.RF_Product_Rx_Event_View_V7
                 (Result.Handle, Address'Access, D'Address, D'Length, R'Access),
               D);
            if Address = System.Null_Address then
               raise Provider_Error with "null RF ProductRx event view";
            end if;
            declare
               Raw : C.RF_Product_Rx_Event_V7 renames To_View_V7 (Address).all;
            begin
               if Raw.Event.VITA_Large_Packets.Data /= System.Null_Address
                 or else Raw.Event.VITA_Large_Packets.Size /= 0
                 or else Raw.Event.Event.VITA_Medium_Packets.Data /= System.Null_Address
                 or else Raw.Event.Event.VITA_Medium_Packets.Size /= 0
                 or else Raw.Event.Event.Event.VITA_Small_Packets.Data /= System.Null_Address
                 or else Raw.Event.Event.Event.VITA_Small_Packets.Size /= 0
                 or else Raw.Event.Event.Event.Event.Data_Format
                         /= Interfaces.Unsigned_32 (AMS_VITA_Extra_Large)
                 or else Raw.Event.Event.Event.Event.Direct_Samples_I8.Size /= 0
                 or else Raw.Event.Event.Event.Event.Direct_Samples_I8.Data /= System.Null_Address
                 or else Raw.Event.Event.Event.Event.Direct_Samples_I16.Size /= 0
                 or else Raw.Event.Event.Event.Event.Direct_Samples_I16.Data /= System.Null_Address
                 or else Raw.Event.Event.Event.Event.Complex_Samples_I8.Size /= 0
                 or else Raw.Event.Event.Event.Event.Complex_Samples_I8.Data /= System.Null_Address
                 or else Raw.Event.Event.Event.Event.Complex_Samples_I16.Size /= 0
                 or else Raw.Event.Event.Event.Event.Complex_Samples_I16.Data /= System.Null_Address
                 or else (Raw.VITA_Extra_Large_Packets.Size = 0
                          and then Raw.VITA_Extra_Large_Packets.Data /= System.Null_Address)
                 or else (Raw.VITA_Extra_Large_Packets.Size > 0
                          and then Raw.VITA_Extra_Large_Packets.Data = System.Null_Address)
                 or else Raw.VITA_Extra_Large_Packets.Size
                         > C.Size_T'Last
                           / (C.RF_VITA_Extra_Large_Packet_V1'Object_Size / System.Storage_Unit)
                 or else Interfaces.Unsigned_64 (Raw.VITA_Extra_Large_Packets.Size)
                         > Interfaces.Unsigned_64 (System.Storage_Elements.Storage_Offset'Last)
                           / (C.RF_VITA_Extra_Large_Packet_V1'Object_Size / System.Storage_Unit)
                 or else (Raw.VITA_Extra_Large_Packets.Size > 0
                          and then System.Storage_Elements.To_Integer
                                     (Raw.VITA_Extra_Large_Packets.Data)
                                   mod C.RF_VITA_Extra_Large_Packet_V1'Alignment
                                   /= 0)
                 or else (Raw.VITA_Extra_Large_Packets.Size > 0
                          and then System.Storage_Elements.To_Integer
                                     (Raw.VITA_Extra_Large_Packets.Data)
                                   > System.Storage_Elements.Integer_Address'Last
                                     - System.Storage_Elements.Integer_Address
                                         (Raw.VITA_Extra_Large_Packets.Size)
                                       * (C.RF_VITA_Extra_Large_Packet_V1'Object_Size
                                          / System.Storage_Unit))
               then
                  raise Provider_Error with "invalid native VITA Large event";
               end if;
               Sample_Span :=
                 (Raw.VITA_Extra_Large_Packets.Data, Raw.VITA_Extra_Large_Packets.Size);
               Result.Packet_Address := Raw.VITA_Extra_Large_Packets.Data;
               Event_ID := Raw.Event.Event.Event.Event.Endpoint_ID;
               Event_Format := Raw.Event.Event.Event.Event.Data_Format;
               Event_Metadata := Raw.Event.Event.Event.Event.Metadata;
            end;
         elsif Object.Format = AMS_VITA_Large then
            Check
              (C.RF_Product_Rx_Event_View_V6
                 (Result.Handle, Address'Access, D'Address, D'Length, R'Access),
               D);
            if Address = System.Null_Address then
               raise Provider_Error with "null RF ProductRx event view";
            end if;
            declare
               Raw : C.RF_Product_Rx_Event_V6 renames To_View_V6 (Address).all;
            begin
               if Raw.Event.VITA_Medium_Packets.Data /= System.Null_Address
                 or else Raw.Event.VITA_Medium_Packets.Size /= 0
                 or else Raw.Event.Event.VITA_Small_Packets.Data /= System.Null_Address
                 or else Raw.Event.Event.VITA_Small_Packets.Size /= 0
                 or else Raw.Event.Event.Event.Data_Format
                         /= Interfaces.Unsigned_32 (AMS_VITA_Large)
                 or else Raw.Event.Event.Event.Direct_Samples_I8.Size /= 0
                 or else Raw.Event.Event.Event.Direct_Samples_I8.Data /= System.Null_Address
                 or else Raw.Event.Event.Event.Direct_Samples_I16.Size /= 0
                 or else Raw.Event.Event.Event.Direct_Samples_I16.Data /= System.Null_Address
                 or else Raw.Event.Event.Event.Complex_Samples_I8.Size /= 0
                 or else Raw.Event.Event.Event.Complex_Samples_I8.Data /= System.Null_Address
                 or else Raw.Event.Event.Event.Complex_Samples_I16.Size /= 0
                 or else Raw.Event.Event.Event.Complex_Samples_I16.Data /= System.Null_Address
                 or else (Raw.VITA_Large_Packets.Size = 0
                          and then Raw.VITA_Large_Packets.Data /= System.Null_Address)
                 or else (Raw.VITA_Large_Packets.Size > 0
                          and then Raw.VITA_Large_Packets.Data = System.Null_Address)
                 or else Raw.VITA_Large_Packets.Size
                         > C.Size_T'Last
                           / (C.RF_VITA_Large_Packet_V1'Object_Size / System.Storage_Unit)
                 or else Interfaces.Unsigned_64 (Raw.VITA_Large_Packets.Size)
                         > Interfaces.Unsigned_64 (System.Storage_Elements.Storage_Offset'Last)
                           / (C.RF_VITA_Large_Packet_V1'Object_Size / System.Storage_Unit)
                 or else (Raw.VITA_Large_Packets.Size > 0
                          and then System.Storage_Elements.To_Integer (Raw.VITA_Large_Packets.Data)
                                   mod C.RF_VITA_Large_Packet_V1'Alignment
                                   /= 0)
                 or else (Raw.VITA_Large_Packets.Size > 0
                          and then System.Storage_Elements.To_Integer (Raw.VITA_Large_Packets.Data)
                                   > System.Storage_Elements.Integer_Address'Last
                                     - System.Storage_Elements.Integer_Address
                                         (Raw.VITA_Large_Packets.Size)
                                       * (C.RF_VITA_Large_Packet_V1'Object_Size
                                          / System.Storage_Unit))
               then
                  raise Provider_Error with "invalid native VITA Large event";
               end if;
               Sample_Span := (Raw.VITA_Large_Packets.Data, Raw.VITA_Large_Packets.Size);
               Result.Packet_Address := Raw.VITA_Large_Packets.Data;
               Event_ID := Raw.Event.Event.Event.Endpoint_ID;
               Event_Format := Raw.Event.Event.Event.Data_Format;
               Event_Metadata := Raw.Event.Event.Event.Metadata;
            end;
         elsif Object.Format = AMS_VITA_Medium then
            Check
              (C.RF_Product_Rx_Event_View_V5
                 (Result.Handle, Address'Access, D'Address, D'Length, R'Access),
               D);
            if Address = System.Null_Address then
               raise Provider_Error with "null RF ProductRx event view";
            end if;
            declare
               Raw : C.RF_Product_Rx_Event_V5 renames To_View_V5 (Address).all;
            begin
               if Raw.Event.VITA_Small_Packets.Data /= System.Null_Address
                 or else Raw.Event.VITA_Small_Packets.Size /= 0
                 or else Raw.Event.Event.Data_Format /= Interfaces.Unsigned_32 (AMS_VITA_Medium)
                 or else Raw.Event.Event.Direct_Samples_I8.Size /= 0
                 or else Raw.Event.Event.Direct_Samples_I8.Data /= System.Null_Address
                 or else Raw.Event.Event.Direct_Samples_I16.Size /= 0
                 or else Raw.Event.Event.Direct_Samples_I16.Data /= System.Null_Address
                 or else Raw.Event.Event.Complex_Samples_I8.Size /= 0
                 or else Raw.Event.Event.Complex_Samples_I8.Data /= System.Null_Address
                 or else Raw.Event.Event.Complex_Samples_I16.Size /= 0
                 or else Raw.Event.Event.Complex_Samples_I16.Data /= System.Null_Address
                 or else (Raw.VITA_Medium_Packets.Size = 0
                          and then Raw.VITA_Medium_Packets.Data /= System.Null_Address)
                 or else Raw.VITA_Medium_Packets.Size
                         > C.Size_T'Last
                           / (C.RF_VITA_Medium_Packet_V1'Object_Size / System.Storage_Unit)
                 or else Interfaces.Unsigned_64 (Raw.VITA_Medium_Packets.Size)
                         > Interfaces.Unsigned_64 (System.Storage_Elements.Storage_Offset'Last)
                           / (C.RF_VITA_Medium_Packet_V1'Object_Size / System.Storage_Unit)
                 or else (Raw.VITA_Medium_Packets.Size > 0
                          and then System.Storage_Elements.To_Integer (Raw.VITA_Medium_Packets.Data)
                                   mod C.RF_VITA_Medium_Packet_V1'Alignment
                                   /= 0)
               then
                  raise Provider_Error with "invalid native VITA Medium event";
               end if;
               Sample_Span := (Raw.VITA_Medium_Packets.Data, Raw.VITA_Medium_Packets.Size);
               Result.Packet_Address := Raw.VITA_Medium_Packets.Data;
               Event_ID := Raw.Event.Event.Endpoint_ID;
               Event_Format := Raw.Event.Event.Data_Format;
               Event_Metadata := Raw.Event.Event.Metadata;
            end;
         elsif Object.Format = AMS_VITA_Small then
            Check
              (C.RF_Product_Rx_Event_View_V4
                 (Result.Handle, Address'Access, D'Address, D'Length, R'Access),
               D);
            if Address = System.Null_Address then
               raise Provider_Error with "null RF ProductRx event view";
            end if;
            declare
               Raw : C.RF_Product_Rx_Event_V4 renames To_View_V4 (Address).all;
            begin
               if Raw.Event.Data_Format /= Interfaces.Unsigned_32 (AMS_VITA_Small)
                 or else Raw.Event.Direct_Samples_I8.Size /= 0
                 or else Raw.Event.Direct_Samples_I8.Data /= System.Null_Address
                 or else Raw.Event.Direct_Samples_I16.Size /= 0
                 or else Raw.Event.Direct_Samples_I16.Data /= System.Null_Address
                 or else Raw.Event.Complex_Samples_I8.Size /= 0
                 or else Raw.Event.Complex_Samples_I8.Data /= System.Null_Address
                 or else Raw.Event.Complex_Samples_I16.Size /= 0
                 or else Raw.Event.Complex_Samples_I16.Data /= System.Null_Address
                 or else (Raw.VITA_Small_Packets.Size = 0
                          and then Raw.VITA_Small_Packets.Data /= System.Null_Address)
                 or else Raw.VITA_Small_Packets.Size
                         > C.Size_T'Last
                           / (C.RF_VITA_Small_Packet_V1'Object_Size / System.Storage_Unit)
                 or else Interfaces.Unsigned_64 (Raw.VITA_Small_Packets.Size)
                         > Interfaces.Unsigned_64 (System.Storage_Elements.Storage_Offset'Last)
                           / (C.RF_VITA_Small_Packet_V1'Object_Size / System.Storage_Unit)
                 or else (Raw.VITA_Small_Packets.Size > 0
                          and then System.Storage_Elements.To_Integer (Raw.VITA_Small_Packets.Data)
                                   mod C.RF_VITA_Small_Packet_V1'Alignment
                                   /= 0)
               then
                  raise Provider_Error with "invalid native VITA Small event";
               end if;
               Sample_Span := (Raw.VITA_Small_Packets.Data, Raw.VITA_Small_Packets.Size);
               Result.Packet_Address := Raw.VITA_Small_Packets.Data;
               Event_ID := Raw.Event.Endpoint_ID;
               Event_Format := Raw.Event.Data_Format;
               Event_Metadata := Raw.Event.Metadata;
            end;
         elsif Object.Format = Direct_INT8 or else Object.Format = Direct_INT16 then
            Check
              (C.RF_Product_Rx_Event_View_V3
                 (Result.Handle, Address'Access, D'Address, D'Length, R'Access),
               D);
            if Address = System.Null_Address then
               raise Provider_Error with "null RF ProductRx event view";
            end if;
            declare
               Raw : C.RF_Product_Rx_Event_V3 renames To_View_V3 (Address).all;
            begin
               if Raw.Data_Format /= Interfaces.Unsigned_32 (Object.Format)
                 or else Raw.Complex_Samples_I8.Size /= 0
                 or else Raw.Complex_Samples_I8.Data /= System.Null_Address
                 or else Raw.Complex_Samples_I16.Size /= 0
                 or else Raw.Complex_Samples_I16.Data /= System.Null_Address
               then
                  raise Provider_Error with "invalid native RF Direct event format";
               end if;
               if Object.Format = Direct_INT8 then
                  if Raw.Direct_Samples_I16.Size /= 0
                    or else Raw.Direct_Samples_I16.Data /= System.Null_Address
                  then
                     raise Provider_Error with "inactive DirectINT16 span";
                  end if;
                  Sample_Span := (Raw.Direct_Samples_I8.Data, Raw.Direct_Samples_I8.Size);
               else
                  if Raw.Direct_Samples_I8.Size /= 0
                    or else Raw.Direct_Samples_I8.Data /= System.Null_Address
                  then
                     raise Provider_Error with "inactive DirectINT8 span";
                  end if;
                  Sample_Span := (Raw.Direct_Samples_I16.Data, Raw.Direct_Samples_I16.Size);
               end if;
               Event_ID := Raw.Endpoint_ID;
               Event_Format := Raw.Data_Format;
               Event_Metadata := Raw.Metadata;
            end;
         elsif Object.Format = Complex_INT8 then
            Check
              (C.RF_Product_Rx_Event_View_V2
                 (Result.Handle, Address'Access, D'Address, D'Length, R'Access),
               D);
            if Address = System.Null_Address then
               raise Provider_Error with "null RF ProductRx event view";
            end if;
            declare
               Raw : C.RF_Product_Rx_Event_V2 renames To_View_V2 (Address).all;
            begin
               if Raw.Data_Format /= C.RF_Job_Data_Format_Complex_INT8
                 or else Raw.Samples_I16.Size /= 0
                 or else Raw.Samples_I16.Data /= System.Null_Address
               then
                  raise Provider_Error with "invalid native RF I8 event format";
               end if;
               Event_ID := Raw.Endpoint_ID;
               Event_Format := Raw.Data_Format;
               Sample_Span := (Raw.Samples_I8.Data, Raw.Samples_I8.Size);
               Event_Metadata := Raw.Metadata;
            end;
         else
            Check
              (C.RF_Product_Rx_Event_View
                 (Result.Handle, Address'Access, D'Address, D'Length, R'Access),
               D);
            if Address = System.Null_Address then
               raise Provider_Error with "null RF ProductRx event view";
            end if;
            declare
               Raw : C.RF_Product_Rx_Event_V1 renames To_View (Address).all;
            begin
               Event_ID := Raw.Endpoint_ID;
               Event_Format := Raw.Data_Format;
               Sample_Span := Raw.Samples;
               Event_Metadata := Raw.Metadata;
            end;
         end if;
         declare
            Count : constant Natural := Checked_Count (Sample_Span);
            IDs   : constant Natural :=
              Checked_Count
                ((Data => Event_Metadata.Rx_Stream_IDs.Data,
                  Size => Event_Metadata.Rx_Stream_IDs.Size));
            type Raw_IDs is array (Natural range <>) of aliased Interfaces.Unsigned_32
            with Convention => C;
         begin
            if Event_Format /= Interfaces.Unsigned_32 (Object.Format) then
               raise Provider_Error with "invalid native RF event format";
            end if;
            Result.ID := Event_ID;
            Result.Format := Job_Data_Format (Event_Format);
            Result.Count := Count;
            if Object.Format /= AMS_VITA_Small
              and then Object.Format /= AMS_VITA_Medium
              and then Object.Format /= AMS_VITA_Large
              and then Object.Format /= AMS_VITA_Extra_Large
            then
               Result.Sample_Address := Sample_Span.Data;
            end if;
            Result.Info :=
              (Protocol     => Event_Metadata.MEL_Protocol_Version_ID,
               Definition   => Event_Metadata.VA_Definition_ID,
               Instance     => Event_Metadata.VA_Instance_ID,
               Details      => Event_Metadata.Job_Details_ID,
               Interval     => Event_Metadata.Job_Interval_ID,
               LF_Type      => Event_Metadata.LF_Type_ID,
               LF_Instance  => Event_Metadata.LF_Instance_ID,
               Coherent     => Binary (Event_Metadata.Phase_Coherence_With_Prior),
               Seconds      => Event_Metadata.First_Rx_Event_Start_S,
               Femtoseconds => Event_Metadata.First_Rx_Event_Start_FS,
               Stream_IDs   => ID_Vectors.Empty_Vector);
            if IDs > 0 then
               declare
                  Values : Raw_IDs (0 .. IDs - 1)
                  with Import, Address => Event_Metadata.Rx_Stream_IDs.Data;
               begin
                  for Item of Values loop
                     Result.Info.Stream_IDs.Append (Item);
                  end loop;
               end;
            end if;
         end;
      end return;
   end Receive;

   function Is_Open (Value : Event) return Boolean
   is (Value.Handle /= C.Null_RF_Product_Rx_Event);
   function Endpoint_ID (Value : Event) return Interfaces.Unsigned_64
   is (Value.ID);
   function Data_Format (Value : Event) return Job_Data_Format
   is (Value.Format);
   function Metadata (Value : Event) return Product_Metadata
   is (Value.Info);
   function Sample_Count (Value : Event) return Natural
   is (Value.Count);

   procedure With_Samples
     (Value : Event; Process : not null access procedure (Samples : Complex_I16_Array)) is
   begin
      if not Is_Open (Value) then
         raise Provider_Error with "RF event is closed";
      end if;
      if Value.Format /= Complex_INT16 then
         raise Constraint_Error with "RF event is not ComplexINT16";
      end if;
      if Value.Count = 0 then
         declare
            Empty : Complex_I16_Array (1 .. 0);
         begin
            Process (Empty);
         end;
      else
         declare
            Samples : Complex_I16_Array (0 .. Value.Count - 1)
            with Import, Address => Value.Sample_Address;
         begin
            Process (Samples);
         end;
      end if;
   end With_Samples;

   function Copy_Samples (Value : Event) return Complex_I16_Array is
      Result : Complex_I16_Array (1 .. Value.Count);
      procedure Copy (Samples : Complex_I16_Array) is
      begin
         Result := Samples;
      end Copy;
   begin
      With_Samples (Value, Copy'Access);
      return Result;
   end Copy_Samples;

   procedure With_Samples_I8
     (Value : Event; Process : not null access procedure (Samples : Complex_I8_Array)) is
   begin
      if not Is_Open (Value) then
         raise Provider_Error with "RF event is closed";
      end if;
      if Value.Format /= Complex_INT8 then
         raise Constraint_Error with "RF event is not ComplexINT8";
      end if;
      if Value.Count = 0 then
         declare
            Empty : Complex_I8_Array (1 .. 0);
         begin
            Process (Empty);
         end;
      else
         declare
            Samples : Complex_I8_Array (0 .. Value.Count - 1)
            with Import, Address => Value.Sample_Address;
         begin
            Process (Samples);
         end;
      end if;
   end With_Samples_I8;

   function Copy_Samples_I8 (Value : Event) return Complex_I8_Array is
      Result : Complex_I8_Array (1 .. Value.Count);
      procedure Copy (Samples : Complex_I8_Array) is
      begin
         Result := Samples;
      end Copy;
   begin
      With_Samples_I8 (Value, Copy'Access);
      return Result;
   end Copy_Samples_I8;

   procedure With_Direct_Samples_I8
     (Value : Event; Process : not null access procedure (Samples : Direct_I8_Array)) is
   begin
      if not Is_Open (Value) then
         raise Provider_Error with "RF event is closed";
      end if;
      if Value.Format /= Direct_INT8 then
         raise Constraint_Error with "RF event is not DirectINT8";
      end if;
      if Value.Count = 0 then
         declare
            Empty : Direct_I8_Array (1 .. 0);
         begin
            Process (Empty);
         end;
      else
         declare
            Samples : Direct_I8_Array (0 .. Value.Count - 1)
            with Import, Address => Value.Sample_Address;
         begin
            Process (Samples);
         end;
      end if;
   end With_Direct_Samples_I8;

   function Copy_Direct_Samples_I8 (Value : Event) return Direct_I8_Array is
      Result : Direct_I8_Array (1 .. Value.Count);
      procedure Copy (Samples : Direct_I8_Array) is
      begin
         Result := Samples;
      end Copy;
   begin
      With_Direct_Samples_I8 (Value, Copy'Access);
      return Result;
   end Copy_Direct_Samples_I8;

   procedure With_Direct_Samples_I16
     (Value : Event; Process : not null access procedure (Samples : Direct_I16_Array)) is
   begin
      if not Is_Open (Value) then
         raise Provider_Error with "RF event is closed";
      end if;
      if Value.Format /= Direct_INT16 then
         raise Constraint_Error with "RF event is not DirectINT16";
      end if;
      if Value.Count = 0 then
         declare
            Empty : Direct_I16_Array (1 .. 0);
         begin
            Process (Empty);
         end;
      else
         declare
            Samples : Direct_I16_Array (0 .. Value.Count - 1)
            with Import, Address => Value.Sample_Address;
         begin
            Process (Samples);
         end;
      end if;
   end With_Direct_Samples_I16;

   function Copy_Direct_Samples_I16 (Value : Event) return Direct_I16_Array is
      Result : Direct_I16_Array (1 .. Value.Count);
      procedure Copy (Samples : Direct_I16_Array) is
      begin
         Result := Samples;
      end Copy;
   begin
      With_Direct_Samples_I16 (Value, Copy'Access);
      return Result;
   end Copy_Direct_Samples_I16;

   procedure Close (Value : in out Event) is
      D      : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R      : aliased C.Size_T := 0;
      Status : constant Interfaces.Integer_32 :=
        C.RF_Product_Rx_Event_Close (Value.Handle'Access, D'Address, D'Length, R'Access);
   begin
      Value.Sample_Address := System.Null_Address;
      Value.Packet_Address := System.Null_Address;
      Value.Count := 0;
      Check (Status, D);
   end Close;

   overriding
   procedure Finalize (Value : in out Event) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.RF_Product_Rx_Event_Close (Value.Handle'Access, System.Null_Address, 0, null);
      Value.Sample_Address := System.Null_Address;
      Value.Packet_Address := System.Null_Address;
      Value.Count := 0;
   exception
      when others =>
         Value.Handle := C.Null_RF_Product_Rx_Event;
         Value.Sample_Address := System.Null_Address;
         Value.Packet_Address := System.Null_Address;
         Value.Count := 0;
   end Finalize;

   function Create_Config_AMS_VITA_Small
     (Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Positive;
      Max_Samples_Per_Event : Positive) return Config
   is ((Region  => Region_Size_Bytes,
        Queue   => Queue_Capacity,
        Maximum => Max_Samples_Per_Event,
        Format  => AMS_VITA_Small));

   function VITA_Small_Packet_Count (Value : Event) return Natural is
   begin
      if not Is_Open (Value) then
         raise Provider_Error with "RF event is closed";
      end if;
      if Value.Format /= AMS_VITA_Small then
         raise Constraint_Error with "RF event is not AMSVitaSmall";
      end if;
      return Value.Count;
   end VITA_Small_Packet_Count;

   procedure With_VITA_Small_Packet
     (Value   : Event;
      Index   : Positive;
      Process : not null access procedure (Bytes : Packet_Byte_Array))
   is
      Count : constant Natural := VITA_Small_Packet_Count (Value);
      type Descriptors is array (Natural range <>) of C.RF_VITA_Small_Packet_V1
      with Convention => C;
   begin
      if Index > Count then
         raise Constraint_Error with "VITA packet index out of range";
      end if;
      if Value.Packet_Address = System.Null_Address then
         raise Provider_Error with "null VITA packet descriptor span";
      end if;
      declare
         Packets : Descriptors (1 .. Count)
         with Import, Address => Value.Packet_Address;
         Span    : C.U8_Span_V1 renames Packets (Index).Serialized_Bytes;
      begin
         if Span.Data = System.Null_Address
           or else Span.Size < 32
           or else Span.Size > 512
           or else Span.Size mod 4 /= 0
         then
            raise Provider_Error with "invalid serialized VITA packet span";
         end if;
         declare
            Bytes : Packet_Byte_Array (1 .. Natural (Span.Size))
            with Import, Address => Span.Data;
         begin
            Process (Bytes);
         end;
      end;
   end With_VITA_Small_Packet;

   function Copy_VITA_Small_Packet (Value : Event; Index : Positive) return Packet_Byte_Array is
      Buffer : Packet_Byte_Array (1 .. 512);
      Length : Natural := 0;
      procedure Copy (Bytes : Packet_Byte_Array) is
      begin
         Length := Bytes'Length;
         Buffer (1 .. Length) := Bytes;
      end Copy;
   begin
      With_VITA_Small_Packet (Value, Index, Copy'Access);
      return Buffer (1 .. Length);
   end Copy_VITA_Small_Packet;

   function Create_Config_AMS_VITA_Medium
     (Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Positive;
      Max_Samples_Per_Event : Positive) return Config
   is ((Region  => Region_Size_Bytes,
        Queue   => Queue_Capacity,
        Maximum => Max_Samples_Per_Event,
        Format  => AMS_VITA_Medium));

   function VITA_Medium_Packet_Count (Value : Event) return Natural is
   begin
      if not Is_Open (Value) then
         raise Provider_Error with "RF event is closed";
      end if;
      if Value.Format /= AMS_VITA_Medium then
         raise Constraint_Error with "RF event is not AMSVitaMedium";
      end if;
      return Value.Count;
   end VITA_Medium_Packet_Count;

   procedure With_VITA_Medium_Packet
     (Value   : Event;
      Index   : Positive;
      Process : not null access procedure (Bytes : Packet_Byte_Array))
   is
      Count : constant Natural := VITA_Medium_Packet_Count (Value);
      type Descriptors is array (Natural range <>) of C.RF_VITA_Medium_Packet_V1
      with Convention => C;
   begin
      if Index > Count then
         raise Constraint_Error with "VITA packet index out of range";
      end if;
      if Value.Packet_Address = System.Null_Address then
         raise Provider_Error with "null VITA packet descriptor span";
      end if;
      declare
         Packets : Descriptors (1 .. Count)
         with Import, Address => Value.Packet_Address;
         Span    : C.U8_Span_V1 renames Packets (Index).Serialized_Bytes;
      begin
         if Span.Data = System.Null_Address
           or else Span.Size < 32
           or else Span.Size > 1024
           or else Span.Size mod 4 /= 0
         then
            raise Provider_Error with "invalid serialized VITA packet span";
         end if;
         declare
            Bytes : Packet_Byte_Array (1 .. Natural (Span.Size))
            with Import, Address => Span.Data;
         begin
            Process (Bytes);
         end;
      end;
   end With_VITA_Medium_Packet;

   function Copy_VITA_Medium_Packet (Value : Event; Index : Positive) return Packet_Byte_Array is
      Buffer : Packet_Byte_Array (1 .. 1024);
      Length : Natural := 0;
      procedure Copy (Bytes : Packet_Byte_Array) is
      begin
         Length := Bytes'Length;
         Buffer (1 .. Length) := Bytes;
      end Copy;
   begin
      With_VITA_Medium_Packet (Value, Index, Copy'Access);
      return Buffer (1 .. Length);
   end Copy_VITA_Medium_Packet;

   function Create_Config_AMS_VITA_Large
     (Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Positive;
      Max_Samples_Per_Event : Positive) return Config
   is ((Region  => Region_Size_Bytes,
        Queue   => Queue_Capacity,
        Maximum => Max_Samples_Per_Event,
        Format  => AMS_VITA_Large));

   function VITA_Large_Packet_Count (Value : Event) return Natural is
   begin
      if not Is_Open (Value) then
         raise Provider_Error with "RF event is closed";
      end if;
      if Value.Format /= AMS_VITA_Large then
         raise Constraint_Error with "RF event is not AMSVitaLarge";
      end if;
      return Value.Count;
   end VITA_Large_Packet_Count;

   procedure With_VITA_Large_Packet
     (Value   : Event;
      Index   : Positive;
      Process : not null access procedure (Bytes : Packet_Byte_Array))
   is
      Count : constant Natural := VITA_Large_Packet_Count (Value);
      type Descriptors is array (Natural range <>) of C.RF_VITA_Large_Packet_V1
      with Convention => C;
   begin
      if Index > Count then
         raise Constraint_Error with "VITA packet index out of range";
      end if;
      if Value.Packet_Address = System.Null_Address then
         raise Provider_Error with "null VITA packet descriptor span";
      end if;
      declare
         Packets : Descriptors (1 .. Count)
         with Import, Address => Value.Packet_Address;
         Span    : C.U8_Span_V1 renames Packets (Index).Serialized_Bytes;
      begin
         if Span.Data = System.Null_Address
           or else Span.Size < 32
           or else Span.Size > 8192
           or else Span.Size mod 4 /= 0
           or else Interfaces.Unsigned_64 (Span.Size) > Interfaces.Unsigned_64 (Natural'Last)
           or else System.Storage_Elements.To_Integer (Span.Data)
                   > System.Storage_Elements.Integer_Address'Last
                     - System.Storage_Elements.Integer_Address (Span.Size)
         then
            raise Provider_Error with "invalid serialized VITA packet span";
         end if;
         declare
            Bytes : Packet_Byte_Array (1 .. Natural (Span.Size))
            with Import, Address => Span.Data;
         begin
            Process (Bytes);
         end;
      end;
   end With_VITA_Large_Packet;

   function Copy_VITA_Large_Packet (Value : Event; Index : Positive) return Packet_Byte_Array is
      Buffer : Packet_Byte_Array (1 .. 8192);
      Length : Natural := 0;
      procedure Copy (Bytes : Packet_Byte_Array) is
      begin
         Length := Bytes'Length;
         Buffer (1 .. Length) := Bytes;
      end Copy;
   begin
      With_VITA_Large_Packet (Value, Index, Copy'Access);
      return Buffer (1 .. Length);
   end Copy_VITA_Large_Packet;

   function Create_Config_AMS_VITA_Extra_Large
     (Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Positive;
      Max_Samples_Per_Event : Positive) return Config
   is ((Region  => Region_Size_Bytes,
        Queue   => Queue_Capacity,
        Maximum => Max_Samples_Per_Event,
        Format  => AMS_VITA_Extra_Large));

   function VITA_Extra_Large_Packet_Count (Value : Event) return Natural is
   begin
      if not Is_Open (Value) then
         raise Provider_Error with "RF event is closed";
      end if;
      if Value.Format /= AMS_VITA_Extra_Large then
         raise Constraint_Error with "RF event is not AMSVitaExtraLarge";
      end if;
      return Value.Count;
   end VITA_Extra_Large_Packet_Count;

   procedure With_VITA_Extra_Large_Packet
     (Value   : Event;
      Index   : Positive;
      Process : not null access procedure (Bytes : Packet_Byte_Array))
   is
      Count : constant Natural := VITA_Extra_Large_Packet_Count (Value);
      type Descriptors is array (Natural range <>) of C.RF_VITA_Extra_Large_Packet_V1
      with Convention => C;
   begin
      if Index > Count then
         raise Constraint_Error with "VITA packet index out of range";
      end if;
      if Value.Packet_Address = System.Null_Address then
         raise Provider_Error with "null VITA packet descriptor span";
      end if;
      declare
         Packets : Descriptors (1 .. Count)
         with Import, Address => Value.Packet_Address;
         Span    : C.U8_Span_V1 renames Packets (Index).Serialized_Bytes;
      begin
         if Span.Data = System.Null_Address
           or else Span.Size < 32
           or else Span.Size > 32768
           or else Span.Size mod 4 /= 0
           or else Interfaces.Unsigned_64 (Span.Size) > Interfaces.Unsigned_64 (Natural'Last)
           or else System.Storage_Elements.To_Integer (Span.Data)
                   > System.Storage_Elements.Integer_Address'Last
                     - System.Storage_Elements.Integer_Address (Span.Size)
         then
            raise Provider_Error with "invalid serialized VITA packet span";
         end if;
         declare
            Bytes : Packet_Byte_Array (1 .. Natural (Span.Size))
            with Import, Address => Span.Data;
         begin
            Process (Bytes);
         end;
      end;
   end With_VITA_Extra_Large_Packet;

   function Copy_VITA_Extra_Large_Packet (Value : Event; Index : Positive) return Packet_Byte_Array
   is
      Buffer : Packet_Byte_Array (1 .. 32768);
      Length : Natural := 0;
      procedure Copy (Bytes : Packet_Byte_Array) is
      begin
         Length := Bytes'Length;
         Buffer (1 .. Length) := Bytes;
      end Copy;
   begin
      With_VITA_Extra_Large_Packet (Value, Index, Copy'Access);
      return Buffer (1 .. Length);
   end Copy_VITA_Extra_Large_Packet;

   function MEL_Protocol_Version_ID (Value : Product_Metadata) return Interfaces.Unsigned_32
   is (Value.Protocol);
   function VA_Definition_ID (Value : Product_Metadata) return Interfaces.Unsigned_32
   is (Value.Definition);
   function VA_Instance_ID (Value : Product_Metadata) return Interfaces.Unsigned_32
   is (Value.Instance);
   function Job_Details_ID (Value : Product_Metadata) return Interfaces.Unsigned_32
   is (Value.Details);
   function Job_Interval_ID (Value : Product_Metadata) return Interfaces.Unsigned_32
   is (Value.Interval);
   function LF_Type_ID (Value : Product_Metadata) return Interfaces.Unsigned_32
   is (Value.LF_Type);
   function LF_Instance_ID (Value : Product_Metadata) return Interfaces.Unsigned_32
   is (Value.LF_Instance);
   function Phase_Coherence_With_Prior (Value : Product_Metadata) return Boolean
   is (Value.Coherent);
   function First_Rx_Event_Start_Seconds (Value : Product_Metadata) return Interfaces.Integer_64
   is (Value.Seconds);
   function First_Rx_Event_Start_Femtoseconds
     (Value : Product_Metadata) return Interfaces.Integer_64
   is (Value.Femtoseconds);
   function Rx_Stream_ID_Count (Value : Product_Metadata) return Natural
   is (Natural (Value.Stream_IDs.Length));
   function Rx_Stream_ID_At
     (Value : Product_Metadata; Index : Positive) return Interfaces.Unsigned_32
   is (Value.Stream_IDs (Index));
begin
   Check_Complex_I16_Representation;
   if Complex_I8'Object_Size /= C.RF_Complex_I8_V1'Object_Size
     or else Complex_I8'Alignment /= C.RF_Complex_I8_V1'Alignment
   then
      raise Program_Error with "Complex_I8 representation does not match the C ABI";
   end if;
end AMS.MEL.RF.Product_Rx;
