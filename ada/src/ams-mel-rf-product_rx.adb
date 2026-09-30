with Ada.Unchecked_Conversion;
with Interfaces.C;

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
   is ((Region => Region_Size_Bytes, Queue => Queue_Capacity, Maximum => Max_Samples_Per_Event));

   function Submit (Parent : Data_MEL; Value : Config) return Create_Request is
      Raw : aliased constant C.RF_Product_Rx_Config_V1 :=
        (Data_Format           => C.RF_Job_Data_Format_Complex_INT16,
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
         Check
           (C.RF_Data_Submit_Product_Rx
              (Parent.Handle, Raw'Access, Result.Handle'Access, D'Address, D'Length, R'Access),
            D);
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
      elsif Status = 11 then
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
         if Info.Assigned_Data_Format /= C.RF_Job_Data_Format_Complex_INT16 then
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

   function Receive (Object : Endpoint'Class; Timeout_Milliseconds : Natural := 0) return Event is
      D       : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R       : aliased C.Size_T := 0;
      Address : aliased System.Address := System.Null_Address;
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
         Check
           (C.RF_Product_Rx_Event_View
              (Result.Handle, Address'Access, D'Address, D'Length, R'Access),
            D);
         if Address = System.Null_Address then
            raise Provider_Error with "null RF ProductRx event view";
         end if;
         declare
            Raw   : C.RF_Product_Rx_Event_V1 renames To_View (Address).all;
            Count : constant Natural := Checked_Count (Raw.Samples);
            IDs   : constant Natural :=
              Checked_Count
                ((Data => Raw.Metadata.Rx_Stream_IDs.Data,
                  Size => Raw.Metadata.Rx_Stream_IDs.Size));
            type Raw_IDs is array (Natural range <>) of aliased Interfaces.Unsigned_32
            with Convention => C;
         begin
            if Raw.Data_Format /= C.RF_Job_Data_Format_Complex_INT16 then
               raise Provider_Error with "invalid native RF event format";
            end if;
            Result.ID := Raw.Endpoint_ID;
            Result.Format := Job_Data_Format (Raw.Data_Format);
            Result.Count := Count;
            Result.Sample_Address := Raw.Samples.Data;
            Result.Info :=
              (Protocol     => Raw.Metadata.MEL_Protocol_Version_ID,
               Definition   => Raw.Metadata.VA_Definition_ID,
               Instance     => Raw.Metadata.VA_Instance_ID,
               Details      => Raw.Metadata.Job_Details_ID,
               Interval     => Raw.Metadata.Job_Interval_ID,
               LF_Type      => Raw.Metadata.LF_Type_ID,
               LF_Instance  => Raw.Metadata.LF_Instance_ID,
               Coherent     => Binary (Raw.Metadata.Phase_Coherence_With_Prior),
               Seconds      => Raw.Metadata.First_Rx_Event_Start_S,
               Femtoseconds => Raw.Metadata.First_Rx_Event_Start_FS,
               Stream_IDs   => ID_Vectors.Empty_Vector);
            if IDs > 0 then
               declare
                  Values : Raw_IDs (0 .. IDs - 1)
                  with Import, Address => Raw.Metadata.Rx_Stream_IDs.Data;
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

   procedure Close (Value : in out Event) is
      D      : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R      : aliased C.Size_T := 0;
      Status : constant Interfaces.Integer_32 :=
        C.RF_Product_Rx_Event_Close (Value.Handle'Access, D'Address, D'Length, R'Access);
   begin
      Value.Sample_Address := System.Null_Address;
      Value.Count := 0;
      Check (Status, D);
   end Close;

   overriding
   procedure Finalize (Value : in out Event) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.RF_Product_Rx_Event_Close (Value.Handle'Access, System.Null_Address, 0, null);
      Value.Sample_Address := System.Null_Address;
      Value.Count := 0;
   exception
      when others =>
         Value.Handle := C.Null_RF_Product_Rx_Event;
         Value.Sample_Address := System.Null_Address;
         Value.Count := 0;
   end Finalize;

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
end AMS.MEL.RF.Product_Rx;
