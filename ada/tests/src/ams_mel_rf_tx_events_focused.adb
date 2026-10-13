with Ada.Strings.Fixed;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.RF.C2;
with AMS.MEL.RF.C2.Testing;
with AMS.MEL.RF.C2.Interval_Status;
with Ada.Environment_Variables;
with Ada.Text_IO;
with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with Ada.Unchecked_Conversion;
with System;

procedure AMS_MEL_RF_TX_Events_Focused is
   package C2 renames AMS.MEL.RF.C2;
   package Status renames AMS.MEL.RF.C2.Interval_Status;
   use type C2.Request_Outcome;
   use type Interfaces.Integer_32;
   use type Interfaces.Integer_8;
   use type Interfaces.Integer_16;
   use type Interfaces.C.int;
   use type Interfaces.C.unsigned;
   use type System.Address;
   function DL_Open
     (Path : Interfaces.C.Strings.chars_ptr; Flags : Interfaces.C.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function DL_Sym
     (Handle : System.Address; Name : Interfaces.C.Strings.chars_ptr) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   function DL_Close (Handle : System.Address) return Interfaces.C.int
   with Import, Convention => C, External_Name => "dlclose";
   function VA_Config return C2.Virtual_Aperture_Config is
      Config               : C2.Virtual_Aperture_Config :=
        C2.Create_Virtual_Aperture_Config (16#FEDC_BA98#, 16#8000_0001#, "definition/β.json");
      First, Second, Third : AMS.MEL.IR.UUID := [others => 0];
   begin
      C2.Append_Local_Function_Info (Config, "alpha");
      C2.Append_Local_Function_Info (Config, "µ-local");
      C2.Append_Local_Function_Info (Config, "");
      First (1) := 16#80#;
      First (2) := 16#FF#;
      Second (0) := 16#FF#;
      Third (15) := 16#80#;
      C2.Append_Capability_ID (Config, AMS.MEL.IR.Create_UCI_ID (First, "first"));
      C2.Append_Capability_ID (Config, AMS.MEL.IR.Create_UCI_ID (Second, ""));
      C2.Append_Capability_ID (Config, AMS.MEL.IR.Create_UCI_ID (Third, "µ-third"));
      return Config;
   end VA_Config;

   function Job_Config return C2.Job_Config is
      Group : C2.RX_Element_Group_Config :=
        C2.Create_RX_Element_Group ("rx/µ-main", 0.625, "products/β");
   begin
      C2.Append_Expected_Center_Frequency (Group, 1000000.25, 2000000.5);
      C2.Append_Expected_Center_Frequency (Group, 987654321.125, 987654322.875);
      C2.Append_Endpoint_ID (Group, 0);
      C2.Append_Endpoint_ID (Group, 16#8000_0000_0000_0000#);
      C2.Append_Endpoint_ID (Group, Interfaces.Unsigned_64'Last);
      return
         Config : C2.Job_Config :=
           C2.Create_Job_Config (16#FEDC_BA98#, 16#8000_0001#, Group, 16#7FFF_FFFE#, True)
      do
         C2.Append_Instance_Selection (Config, 0);
         C2.Append_Instance_Selection (Config, 42);
         C2.Append_Instance_Selection (Config, Interfaces.Unsigned_32'Last);
      end return;
   end Job_Config;

   Path     : constant String :=
     Ada.Environment_Variables.Value ("AMS_MEL_TEST_PROVIDER_DIR") & "/libmock_rf_provider.so";
   type Scalar_Access is
     access function (I, E, F : Interfaces.C.unsigned) return Interfaces.Integer_64
   with Convention => C;
   type Number_Access is
     access function (I, E, T, Edge : Interfaces.C.unsigned) return Interfaces.C.double
   with Convention => C;
   function To_Scalar is new Ada.Unchecked_Conversion (System.Address, Scalar_Access);
   function To_Number is new Ada.Unchecked_Conversion (System.Address, Number_Access);
   use type Interfaces.Integer_64;
   use type Interfaces.Unsigned_64;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.double;
   pragma Validity_Checks ("F");
   function IEEE is new Ada.Unchecked_Conversion (Interfaces.Unsigned_64, Long_Float);
   function Bits is new Ada.Unchecked_Conversion (Interfaces.C.double, Interfaces.Unsigned_64);
   type Failure_Array is array (Positive range <>) of String (1 .. 7);
   Failures : constant Failure_Array := ["std    ", "unknown", "alloc  "];
   procedure Observe (Reference, Tag : Natural; Width : Interfaces.Integer_64; Count : Natural) is
      Name        : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Scalar_Name : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_f6_scalar");
      Number_Name : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_f6_double");
      Handle      : constant System.Address := DL_Open (Name, 2);
      Scalar      : constant Scalar_Access := To_Scalar (DL_Sym (Handle, Scalar_Name));
      Number      : constant Number_Access := To_Number (DL_Sym (Handle, Number_Name));
   begin
      if Handle = System.Null_Address or else Scalar = null or else Number = null then
         raise Program_Error with "pulse observations unavailable";
      end if;
      for I in 0 .. 1 loop
         for E in 0 .. 1 loop
            if Scalar (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 0)
              /= Interfaces.Integer_64 (Reference)
              or else Scalar (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 5) /= Width
              or else Scalar (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 6)
                      /= Interfaces.Integer_64 (Tag)
              or else Scalar (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 7)
                      /= Interfaces.Integer_64 (Count)
            then
               raise Program_Error with "pulse scalar fidelity";
            end if;
            if Count > 0 then
               if Scalar (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 1) /= 255
                 or else Scalar (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 2) /= 0
                 or else Scalar (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 3) /= 0
                 or else Scalar (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 4) /= 255
                 or else Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 0, 0)
                         /= 12.25
                 or else Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 0, 1) /= -3.5
                 or else Bits (Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 1, 0))
                         /= 16#8000_0000_0000_0000#
                 or else Bits (Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 1, 1))
                         /= 16#7FF0_0000_0000_0000#
                 or else Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 2, 0)
                         = Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 2, 0)
                 or else Bits (Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 2, 1))
                         /= 16#FFF0_0000_0000_0000#
                 or else Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 3, 0)
                         /= 12.25
                 or else Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 3, 1) /= -3.5
               then
                  raise Program_Error with "pulse threshold or M/N fidelity";
               end if;
            end if;
         end loop;
      end loop;
      if DL_Close (Handle) /= 0 then
         raise Program_Error;
      end if;
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (Scalar_Name);
      Interfaces.C.Strings.Free (Number_Name);
   end Observe;
   function LF_Value
     (I, Command, Write, Field : Interfaces.C.unsigned) return Interfaces.Unsigned_64
   is
      type Getter is
        access function
          (I, Command, Write, Field : Interfaces.C.unsigned) return Interfaces.Unsigned_64
      with Convention => C;
      function Convert is new Ada.Unchecked_Conversion (System.Address, Getter);
      Path_C  : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Library : constant System.Address := DL_Open (Path_C, 2);
      Name    : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_f7_value");
      Address : constant System.Address := DL_Sym (Library, Name);
      Result  : Interfaces.Unsigned_64;
   begin
      if Library = System.Null_Address or else Address = System.Null_Address then
         raise Program_Error;
      end if;
      Result := Convert (Address) (I, Command, Write, Field);
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (Path_C);
      if DL_Close (Library) /= 0 then
         raise Program_Error;
      end if;
      return Result;
   end LF_Value;
   function Stream_Value (I, Index, Field : Interfaces.C.unsigned) return Interfaces.Unsigned_64 is
      type Getter is
        access function (I, Index, Field : Interfaces.C.unsigned) return Interfaces.Unsigned_64
      with Convention => C;
      function Convert is new Ada.Unchecked_Conversion (System.Address, Getter);
      Path_C  : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Library : constant System.Address := DL_Open (Path_C, 2);
      Name    : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_f8_value");
      Address : constant System.Address := DL_Sym (Library, Name);
      Result  : Interfaces.Unsigned_64;
   begin
      if Library = System.Null_Address or else Address = System.Null_Address then
         raise Program_Error;
      end if;
      Result := Convert (Address) (I, Index, Field);
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (Path_C);
      if DL_Close (Library) /= 0 then
         raise Program_Error;
      end if;
      return Result;
   end Stream_Value;
   function Map_Value
     (I : Interfaces.C.unsigned; Group_Label, Pipe_Label : String; Field : Interfaces.C.unsigned)
      return Interfaces.Unsigned_64
   is
      type Getter is
        access function
          (I    : Interfaces.C.unsigned;
           G, P : Interfaces.C.Strings.chars_ptr;
           F    : Interfaces.C.unsigned) return Interfaces.Unsigned_64
      with Convention => C;
      function Convert is new Ada.Unchecked_Conversion (System.Address, Getter);
      Path_C  : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Name    : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_f9_value");
      G       : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Group_Label);
      P       : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Pipe_Label);
      Handle  : constant System.Address := DL_Open (Path_C, 2);
      Address : constant System.Address := DL_Sym (Handle, Name);
      Result  : Interfaces.Unsigned_64;
   begin
      if Handle = System.Null_Address or else Address = System.Null_Address then
         raise Program_Error;
      end if;
      Result := Convert (Address) (I, G, P, Field);
      Interfaces.C.Strings.Free (Path_C);
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (G);
      Interfaces.C.Strings.Free (P);
      if DL_Close (Handle) /= 0 then
         raise Program_Error;
      end if;
      return Result;
   end Map_Value;
   function TX_Value (I, E, Field, G : Interfaces.C.unsigned) return Interfaces.Unsigned_64 is
      type Access_Value is
        access function (I, E, F, G : Interfaces.C.unsigned) return Interfaces.Unsigned_64
      with Convention => C;
      function Convert is new Ada.Unchecked_Conversion (System.Address, Access_Value);
      Name    : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Symbol  : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_f10_value");
      Handle  : constant System.Address := DL_Open (Name, 2);
      Address : constant System.Address := DL_Sym (Handle, Symbol);
      Result  : Interfaces.Unsigned_64;
   begin
      if Handle = System.Null_Address or else Address = System.Null_Address then
         raise Program_Error;
      end if;
      Result := Convert (Address) (I, E, Field, G);
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (Symbol);
      if DL_Close (Handle) /= 0 then
         raise Program_Error;
      end if;
      return Result;
   end TX_Value;
   function TX_Control (I, E, Field : Interfaces.C.unsigned) return Interfaces.Unsigned_64 is
      type Access_Value is
        access function (I, E, F : Interfaces.C.unsigned) return Interfaces.Unsigned_64
      with Convention => C;
      function Convert is new Ada.Unchecked_Conversion (System.Address, Access_Value);
      Name    : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Symbol  : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_f11_value");
      Handle  : constant System.Address := DL_Open (Name, 2);
      Address : constant System.Address := DL_Sym (Handle, Symbol);
      Result  : Interfaces.Unsigned_64;
   begin
      if Handle = System.Null_Address or else Address = System.Null_Address then
         raise Program_Error;
      end if;
      Result := Convert (Address) (I, E, Field);
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (Symbol);
      if DL_Close (Handle) /= 0 then
         raise Program_Error;
      end if;
      return Result;
   end TX_Control;
   function TX_Number (I, E, Field : Interfaces.C.unsigned) return Interfaces.C.double is
      type Access_Value is
        access function (I, E, F : Interfaces.C.unsigned) return Interfaces.C.double
      with Convention => C;
      function Convert is new Ada.Unchecked_Conversion (System.Address, Access_Value);
      Name    : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Symbol  : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_f10_double");
      Handle  : constant System.Address := DL_Open (Name, 2);
      Address : constant System.Address := DL_Sym (Handle, Symbol);
      Result  : Interfaces.C.double;
   begin
      if Handle = System.Null_Address or else Address = System.Null_Address then
         raise Program_Error;
      end if;
      Result := Convert (Address) (I, E, Field);
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (Symbol);
      if DL_Close (Handle) /= 0 then
         raise Program_Error;
      end if;
      return Result;
   end TX_Number;
   function TX_Component (I, E, Vector, Field : Interfaces.C.unsigned) return Interfaces.C.double is
      type Access_Value is
        access function (I, E, V, F : Interfaces.C.unsigned) return Interfaces.C.double
      with Convention => C;
      function Convert is new Ada.Unchecked_Conversion (System.Address, Access_Value);
      Name    : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Symbol  : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_f11_double");
      Handle  : constant System.Address := DL_Open (Name, 2);
      Address : constant System.Address := DL_Sym (Handle, Symbol);
      Result  : Interfaces.C.double;
   begin
      if Handle = System.Null_Address or else Address = System.Null_Address then
         raise Program_Error;
      end if;
      Result := Convert (Address) (I, E, Vector, Field);
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (Symbol);
      if DL_Close (Handle) /= 0 then
         raise Program_Error;
      end if;
      return Result;
   end TX_Component;
   function RX_Observation (I, F : Interfaces.C.unsigned) return Interfaces.Integer_64 is
      Name   : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Symbol : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_f5_scalar");
      Handle : constant System.Address := DL_Open (Name, 2);
      Fn     : constant Scalar_Access := To_Scalar (DL_Sym (Handle, Symbol));
      Result : Interfaces.Integer_64;
   begin
      if Fn = null then
         raise Program_Error;
      end if;
      Result := Fn (I, 0, F);
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (Symbol);
      if DL_Close (Handle) /= 0 then
         raise Program_Error;
      end if;
      return Result;
   end RX_Observation;
   function RX_Phase (I : Interfaces.C.unsigned) return Interfaces.C.double is
      Name   : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Symbol : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_f5_double");
      Handle : constant System.Address := DL_Open (Name, 2);
      Fn     : constant Number_Access := To_Number (DL_Sym (Handle, Symbol));
      Result : Interfaces.C.double;
   begin
      if Fn = null then
         raise Program_Error;
      end if;
      Result := Fn (I, 0, 0, 4);
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (Symbol);
      if DL_Close (Handle) /= 0 then
         raise Program_Error;
      end if;
      return Result;
   end RX_Phase;
   --  Independent public-value ownership and asymmetric common-control fixture.
   procedure Send_Distinct (Object : in out C2.Job) is
      List, Saved_List : C2.RX_Job_Interval_List;
      type Doubles is array (Natural range <>) of Long_Float;
      Phases           : constant Doubles :=
        [-0.0, IEEE (16#7FF0_0000_0000_0000#), IEEE (16#7FF8_0000_0000_0001#), -4.25];
      type Counts is array (Natural range <>) of Natural;
      Sizes            : constant Counts := [2, 1, 0, 2];
      type Matrix is
        array (Natural range 0 .. 3, Natural range 0 .. 1, Natural range 0 .. 3) of Long_Float;
      Values           : Matrix;
      function Label (K : Natural) return String
      is ("TX/β/" & Character'Val (Character'Pos ('A') + K));
      procedure Equal_Number (Actual : Interfaces.C.double; Expected : Long_Float) is
      begin
         if Expected /= Expected then
            if Actual = Actual then
               raise Program_Error with "NaN classification lost";
            end if;
         elsif Actual /= Interfaces.C.double (Expected) then
            raise Program_Error with "TX IEEE value lost";
         elsif Expected = 0.0 then
            if Bits (Actual) /= Bits (Interfaces.C.double (Expected)) then
               raise Program_Error with "TX signed zero lost";
            end if;
         end if;
      end Equal_Number;
   begin
      for K in 0 .. 3 loop
         for V in 0 .. 1 loop
            for C in 0 .. 3 loop
               Values (K, V, C) := Long_Float (1 + K * 20 + V * 4 + C);
            end loop;
         end loop;
      end loop;
      Values (0, 0, 1) := -0.0;
      Values (0, 0, 2) := IEEE (16#7FF0_0000_0000_0000#);
      Values (0, 0, 3) := IEEE (16#7FF8_0000_0000_0001#);
      Values (1, 0, 2) := IEEE (16#FFF0_0000_0000_0000#);
      for I in 0 .. 1 loop
         declare
            Interval : C2.RX_Job_Interval_Config :=
              C2.Create_RX_Job_Interval (10 + Interfaces.Unsigned_32 (I), 20);
            Copy     : C2.RX_Job_Interval_Config;
            RX       : C2.RX_Receive_Event_Config :=
              C2.Create_RX_Receive_Event
                (80 + Interfaces.Unsigned_32 (I), "retained RX", -9, 11, 12.5, 2.0);
         begin
            C2.Set_Phase_Offset_Radians (RX, 3.25 + Long_Float (I));
            C2.Set_Iteration_Hold_Count (RX, 9 + Interfaces.Unsigned_64 (I));
            C2.Set_Event_Execution_Type (RX, C2.Normal);
            C2.Set_Event_Termination_Type (RX, C2.Cancel_Event);
            C2.Append_RX_Event (Interval, RX);
            for E in 0 .. 1 loop
               declare
                  K     : constant Natural := I * 2 + E;
                  TX    : C2.TX_Transmit_Event_Config :=
                    C2.Create_TX_Transmit_Event
                      (17 + Interfaces.Unsigned_32 (K),
                       Label (K),
                       -7 - Interfaces.Integer_64 (K),
                       11 + Interfaces.Integer_64 (K),
                       100.25 + Long_Float (K),
                       -12.5 - Long_Float (K),
                       3 + Interfaces.Integer_64 (K),
                       -3 - Interfaces.Integer_64 (K));
                  Saved : C2.TX_Transmit_Event_Config;
               begin
                  C2.Set_Event_Stabilization_Point_Index
                    (TX,
                     Interfaces.Unsigned_64 (Interfaces.C.size_t'Last)
                     - Interfaces.Unsigned_64 (K));
                  C2.Append_Applicable_TX_Element_Group (TX, 0);
                  C2.Append_Applicable_TX_Element_Group
                    (TX, Interfaces.Unsigned_64 (Interfaces.C.size_t'Last));
                  for V in 1 .. Sizes (K) loop
                     C2.Append_Polarization
                       (TX,
                        Values (K, V - 1, 0),
                        Values (K, V - 1, 1),
                        Values (K, V - 1, 2),
                        Values (K, V - 1, 3));
                  end loop;
                  if Sizes (K) = 2 then
                     begin
                        C2.Append_Polarization (TX, 99.0, 98.0, 97.0, 96.0);
                        raise Program_Error with "third append accepted";
                     exception
                        when Constraint_Error =>
                           null;
                     end;
                  end if;
                  C2.Set_Polarization_Beam_Steer_Correction (TX, K mod 2 = 1);
                  C2.Set_Phase_Offset_Radians (TX, Phases (K));
                  C2.Set_Event_Execution_Type (TX, C2.Execution_Type'Val (K mod 2));
                  C2.Set_Event_Termination_Type (TX, C2.Event_Termination_Type'Val (1 - K mod 2));
                  C2.Set_Allow_Delay_Start (TX, I = 1);
                  C2.Set_Channelization_Enabled (TX, K = 0 or K = 3);
                  C2.Set_Iteration_Hold_Count
                    (TX,
                     Interfaces.Unsigned_64 (Interfaces.C.size_t'Last)
                     - Interfaces.Unsigned_64 (K));
                  C2.Set_Iteration_Termination_Count (TX, 2 + Interfaces.Unsigned_64 (K));
                  Saved := TX;
                  C2.Set_Phase_Offset_Radians (TX, 999.0);
                  C2.Set_TX_Attenuation_DB (TX, 999.0);
                  C2.Append_TX_Event (Interval, Saved);
                  C2.Set_Phase_Offset_Radians (Saved, 888.0);
                  C2.Set_TX_Edge_Durations (Saved, 888, 888);
               end;
            end loop;
            Copy := Interval;
            C2.Clear_TX_Events (Interval);
            C2.Append_Job_Interval (List, Copy);
            C2.Set_Empty_TX_Events (Copy);
         end;
      end loop;
      Saved_List := List;
      declare
         Extra : constant C2.RX_Job_Interval_Config := C2.Create_RX_Job_Interval (99, 99);
      begin
         C2.Append_Job_Interval (List, Extra);
      end;
      if C2.Job_Interval_Count (Saved_List) /= 2 then
         raise Program_Error with "list copy aliased";
      end if;
      C2.Add_Job_Intervals (Object, Saved_List);
      for I in Interfaces.C.unsigned range 0 .. 1 loop
         if TX_Value (I, 0, 0, 0) /= 2
           or else TX_Value (I, 0, 1, 0) /= 1
           or else TX_Value (I, 0, 14, 0) /= 80 + Interfaces.Unsigned_64 (I)
           or else RX_Observation (I, 9) /= 9 + Interfaces.Integer_64 (I)
           or else RX_Observation (I, 6) /= 0
           or else RX_Observation (I, 7) /= 1
           or else RX_Phase (I) /= 3.25 + Interfaces.C.double (I)
         then
            raise Program_Error with "RX retention";
         end if;
         for E in Interfaces.C.unsigned range 0 .. 1 loop
            declare
               K        : constant Natural := Natural (I * 2 + E);
               Expected : constant array (Natural range 0 .. 8) of Interfaces.Unsigned_64 :=
                 [Interfaces.Unsigned_64 (Sizes (K)),
                  Interfaces.Unsigned_64 (K mod 2),
                  Interfaces.Unsigned_64 (K mod 2),
                  Interfaces.Unsigned_64 (1 - K mod 2),
                  Interfaces.Unsigned_64 (I),
                  Interfaces.Unsigned_64 (Interfaces.C.size_t'Last) - Interfaces.Unsigned_64 (K),
                  2 + Interfaces.Unsigned_64 (K),
                  (if K = 0 or K = 3 then 1 else 0),
                  1];
               Text     : constant String := Label (K);
            begin
               for F in Expected'Range loop
                  if TX_Control (I, E, Interfaces.C.unsigned (F)) /= Expected (F) then
                     raise Program_Error with "asymmetric TX control";
                  end if;
               end loop;
               Equal_Number (TX_Component (I, E, 0, 4), Phases (K));
               for V in 1 .. Sizes (K) loop
                  for C in 0 .. 3 loop
                     Equal_Number
                       (TX_Component
                          (I, E, Interfaces.C.unsigned (V - 1), Interfaces.C.unsigned (C)),
                        Values (K, V - 1, C));
                  end loop;
               end loop;
               if TX_Value (I, E, 2, 0) /= 17 + Interfaces.Unsigned_64 (K)
                 or else TX_Value (I, E, 3, 0)
                         /= Interfaces.Unsigned_64 (Interfaces.C.size_t'Last)
                            - Interfaces.Unsigned_64 (K)
                 or else TX_Value (I, E, 6, 0)
                         /= Interfaces.Unsigned_64'Last - 6 - Interfaces.Unsigned_64 (K)
                 or else TX_Value (I, E, 7, 0) /= 11 + Interfaces.Unsigned_64 (K)
                 or else TX_Value (I, E, 8, 0) /= 3 + Interfaces.Unsigned_64 (K)
                 or else TX_Value (I, E, 9, 0)
                         /= Interfaces.Unsigned_64'Last - 2 - Interfaces.Unsigned_64 (K)
                 or else TX_Control (I, E, 9) /= 1
                 or else TX_Value (I, E, 4, 0) /= 2
                 or else TX_Value (I, E, 5, 0) /= 0
                 or else TX_Value (I, E, 5, 1) /= Interfaces.Unsigned_64 (Interfaces.C.size_t'Last)
                 or else TX_Value (I, E, 10, 0) /= 1
                 or else TX_Value (I, E, 11, 0) /= 1
                 or else TX_Value (I, E, 12, 0) /= Text'Length
               then
                  raise Program_Error with "TX inherited ownership";
               end if;
               for C in Text'Range loop
                  if TX_Value (I, E, 13, Interfaces.C.unsigned (C - Text'First))
                    /= Character'Pos (Text (C))
                  then
                     raise Program_Error with "TX label ownership";
                  end if;
               end loop;
               Equal_Number (TX_Number (I, E, 0), 100.25 + Long_Float (K));
               Equal_Number (TX_Number (I, E, 1), -12.5 - Long_Float (K));
            end;
         end loop;
      end loop;
      --  Toggle each Boolean/enum alone through the public API: a swapped setter
      --  cannot pass merely because two fields share a value in another fixture.
      for Field in Interfaces.C.unsigned range 1 .. 7 loop
         if Field /= 5 and Field /= 6 then
            declare
               TX : C2.TX_Transmit_Event_Config :=
                 C2.Create_TX_Transmit_Event (77, "independent", 0, 1, 1.0);
            begin
               for Enabled in reverse Boolean loop
                  case Field is
                     when 1      =>
                        C2.Set_Polarization_Beam_Steer_Correction (TX, Enabled);

                     when 2      =>
                        C2.Set_Event_Execution_Type
                          (TX, (if Enabled then C2.Conditional else C2.Normal));

                     when 3      =>
                        C2.Set_Event_Termination_Type
                          (TX, (if Enabled then C2.Cancel_Event else C2.Inhibit_Event));

                     when 4      =>
                        C2.Set_Allow_Delay_Start (TX, Enabled);

                     when others =>
                        C2.Set_Channelization_Enabled (TX, Enabled);
                  end case;
                  declare
                     Interval : C2.RX_Job_Interval_Config := C2.Create_RX_Job_Interval (1, 2);
                     One      : C2.RX_Job_Interval_List;
                  begin
                     C2.Append_TX_Event (Interval, TX);
                     C2.Append_Job_Interval (One, Interval);
                     C2.Add_Job_Intervals (Object, One);
                     for F in Interfaces.C.unsigned range 0 .. 7 loop
                        if TX_Control (0, 0, F) /= (if F = Field and Enabled then 1 else 0) then
                           raise Program_Error with "independent public control setter";
                        end if;
                     end loop;
                  end;
               end loop;
            end;
         end if;
      end loop;
      --  Observe Clear and explicit empty separately, including presence evidence
      --  emitted only by the test-enabled facade, not inferred from vector size.
      for Explicit_Empty in Boolean loop
         declare
            Interval : C2.RX_Job_Interval_Config := C2.Create_RX_Job_Interval (1, 2);
            TX       : constant C2.TX_Transmit_Event_Config :=
              C2.Create_TX_Transmit_Event (7, "clear source", 0, 1, 1.0);
            RX       : C2.RX_Receive_Event_Config :=
              C2.Create_RX_Receive_Event (91, "RX survives", 0, 1, 2.0, 3.0);
            Only     : C2.RX_Job_Interval_List;
            File     : Ada.Text_IO.File_Type;
            Trace    : constant String :=
              Ada.Environment_Variables.Value ("AMS_MEL_TEST_PROVIDER_DIR")
              & "/f11-ada-presence.log";
         begin
            C2.Set_Iteration_Hold_Count (RX, 37);
            C2.Append_RX_Event (Interval, RX);
            C2.Append_TX_Event (Interval, TX);
            C2.Append_Job_Interval (Only, Interval);
            C2.Add_Job_Intervals (Object, Only);
            declare
               Cleared : C2.RX_Job_Interval_List;
            begin
               if Explicit_Empty then
                  C2.Set_Empty_TX_Events (Interval);
               else
                  C2.Clear_TX_Events (Interval);
               end if;
               Ada.Text_IO.Create (File, Ada.Text_IO.Out_File, Trace);
               Ada.Text_IO.Close (File);
               Ada.Environment_Variables.Set ("AMS_MEL_TEST_RF_TX_TRACE", Trace);
               C2.Append_Job_Interval (Cleared, Interval);
               C2.Add_RX_Job_Intervals (Object, Cleared);
               Ada.Environment_Variables.Clear ("AMS_MEL_TEST_RF_TX_TRACE");
               Ada.Text_IO.Open (File, Ada.Text_IO.In_File, Trace);
               --  Text_IO.Create/Close emits an empty line before the C trace.
               if Ada.Text_IO.Get_Line (File) /= "" then
                  raise Program_Error with "unexpected trace prefix";
               end if;
               if Ada.Text_IO.Get_Line (File)
                 /= (if Explicit_Empty then "tx-present" else "tx-absent")
               then
                  raise Program_Error with "TX presence semantics";
               end if;
               if Explicit_Empty then
                  if Ada.Text_IO.Get_Line (File) /= "setTxEvents" then
                     raise Program_Error with "explicit empty setter not reached";
                  end if;
               end if;
               if not Ada.Text_IO.End_Of_File (File) then
                  raise Program_Error with "unexpected TX setter calls";
               end if;
               Ada.Text_IO.Delete (File);
               if TX_Value (0, 0, 0, 0) /= 0
                 or else TX_Value (0, 0, 1, 0) /= 1
                 or else TX_Value (0, 0, 14, 0) /= 91
                 or else RX_Observation (0, 9) /= 37
               then
                  raise Program_Error with "clear discarded RX";
               end if;
            end;
         end;
      end loop;
   end Send_Distinct;
   procedure Send_Waveform_Buffers (Object : in out C2.Job) is
      type Getter is
        access function (I, M, S, F : Interfaces.C.unsigned) return Interfaces.Integer_64
      with Convention => C;
      function Convert is new Ada.Unchecked_Conversion (System.Address, Getter);
      function Observe
        (M, S, F : Interfaces.C.unsigned; I : Interfaces.C.unsigned := 0)
         return Interfaces.Integer_64
      is
         Name    : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
         Symbol  : Interfaces.C.Strings.chars_ptr :=
           Interfaces.C.Strings.New_String ("mock_rf_f15_value");
         Handle  : constant System.Address := DL_Open (Name, 2);
         Address : constant System.Address := DL_Sym (Handle, Symbol);
         Result  : Interfaces.Integer_64;
      begin
         if Handle = System.Null_Address or else Address = System.Null_Address then
            raise Program_Error;
         end if;
         Result := Convert (Address) (I, M, S, F);
         Interfaces.C.Strings.Free (Name);
         Interfaces.C.Strings.Free (Symbol);
         if DL_Close (Handle) /= 0 then
            raise Program_Error;
         end if;
         return Result;
      end Observe;
      function Owner_Operation
        (Operation, M, S : Interfaces.C.unsigned; Value : Interfaces.C.int)
         return Interfaces.Integer_64
      is
         type Operation_Access is
           access function
             (Operation, I, M, S : Interfaces.C.unsigned; Value : Interfaces.C.int)
              return Interfaces.Integer_64
         with Convention => C;
         function To_Operation is new Ada.Unchecked_Conversion (System.Address, Operation_Access);
         Name    : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
         Symbol  : Interfaces.C.Strings.chars_ptr :=
           Interfaces.C.Strings.New_String ("mock_rf_f15_owner");
         Handle  : constant System.Address := DL_Open (Name, 2);
         Address : constant System.Address := DL_Sym (Handle, Symbol);
         Result  : Interfaces.Integer_64;
      begin
         if Handle = System.Null_Address or else Address = System.Null_Address then
            raise Program_Error;
         end if;
         Result := To_Operation (Address) (Operation, 0, M, S, Value);
         Interfaces.C.Strings.Free (Name);
         Interfaces.C.Strings.Free (Symbol);
         if DL_Close (Handle) /= 0 then
            raise Program_Error;
         end if;
         return Result;
      end Owner_Operation;
      procedure Require (Condition : Boolean) is
      begin
         if not Condition then
            raise Program_Error with "WaveformBuffer observation mismatch";
         end if;
      end Require;
      Config   : C2.Modulation_Config := C2.Tone;
      Config16 : C2.Modulation_Config := C2.Waveform_Buffer_I16 ([(32767, -32768), (123, -456)]);
      Table    : C2.Modulation_Table;
      Index    : C2.Modulation_Index;
      Interval : C2.RX_Job_Interval_Config := C2.Create_RX_Job_Interval (37, 20);
      List     : C2.RX_Job_Interval_List;
   begin
      declare
         Source : C2.Waveform_Sample_I8_Array := [(-128, 127), (-7, 3)];
      begin
         Config := C2.Waveform_Buffer_I8 (Source);
         Source (1) := (9, 8);
         Require (Source (1).Real = 9);
      end;
      C2.Append_Waveform_Sample_I8 (Config, (0, -9));
      begin
         C2.Append_Waveform_Sample_I16 (Config, (7, 8));
         raise Program_Error with "wrong sample width accepted";
      exception
         when Constraint_Error =>
            null;
      end;
      C2.Append_Modulation (Table, Config, Index);
      C2.Append_Modulation (Table, Config16, Index);
      C2.Append_Modulation (Table, C2.Waveform_Buffer_I8 ([]), Index);
      declare
         Profiles : C2.Waveform_Profile_Table;
         Patterns : C2.Waveform_Pattern_List;
         RX       : C2.RX_Receive_Event_Config :=
           C2.Create_RX_Receive_Event (91, "sample RX", -9, 11, 12.5, 2.0);
         TX       : C2.TX_Transmit_Event_Config :=
           C2.Create_TX_Transmit_Event (92, "sample TX", -7, 11, 100.25, -12.5, 3, -3);
      begin
         C2.Append_Modulation (Table, C2.Custom_Waveform (Profiles, Patterns), Index);
         C2.Append_Modulation (Table, C2.Phase_Code (-37, [-0.25, 1.5]), Index);
         C2.Append_Polarization (RX, 1.0, -2.0, 3.0, -4.0);
         C2.Set_Phase_Offset_Radians (RX, 3.25);
         C2.Append_RX_Event (Interval, RX);
         C2.Append_Polarization (TX, 1.0, -2.0, 3.0, -4.0);
         C2.Set_Phase_Offset_Radians (TX, -0.75);
         C2.Append_Modulation_Reference (TX, 2);
         C2.Append_Modulation_Reference (TX, 0);
         C2.Append_Modulation_Reference (TX, C2.No_Modulation);
         C2.Append_Modulation_Reference (TX, 4);
         C2.Append_Modulation_Reference (TX, 0);
         C2.Append_Modulation_Reference (TX, 3);
         C2.Append_Modulation_Reference (TX, 1);
         C2.Append_TX_Event (Interval, TX);
      end;
      C2.Set_Modulations (Interval, Table);
      C2.Append_Job_Interval (List, Interval);
      declare
         Second       : C2.RX_Job_Interval_Config := C2.Create_RX_Job_Interval (39, 30);
         Second_Table : C2.Modulation_Table;
         TX           : C2.TX_Transmit_Event_Config :=
           C2.Create_TX_Transmit_Event (93, "second sample TX", 2, 19, 200.5, -9.5, 2, 1);
      begin
         C2.Append_Modulation (Second_Table, Config16, Index);
         C2.Append_Modulation (Second_Table, C2.Waveform_Buffer_I8 ([(-11, 17)]), Index);
         C2.Set_Modulations (Second, Second_Table);
         C2.Append_Modulation_Reference (TX, 1);
         C2.Append_Modulation_Reference (TX, 0);
         C2.Append_Modulation_Reference (TX, 1);
         C2.Append_TX_Event (Second, TX);
         C2.Append_Job_Interval (List, Second);
      end;
      C2.Replace_Waveform_Samples_I8 (Config, [(9, 8)]);
      C2.Replace_Waveform_Samples_I16 (Config16, []);
      C2.Add_Job_Intervals (Object, List);
      Require (Observe (0, 0, 8) = 7);
      declare
         type Ref_Array is array (Natural range <>) of Interfaces.Integer_64;
         Refs : constant Ref_Array := [2, 0, -1, 4, 0, 3, 1];
      begin
         for N in Refs'Range loop
            Require (Observe (0, Interfaces.C.unsigned (N), 9) = Refs (N));
         end loop;
      end;
      Require (Observe (0, 0, 10) = 92);
      Require (Observe (3, 0, 11) = 1 and then Observe (4, 0, 12) = -37);
      Require (Observe (0, 0, 13) = 1 and then Observe (0, 0, 14) = 1);
      Require (Observe (0, 0, 3, 1) = 16 and then Observe (0, 0, 5, 1) = 2);
      Require (Observe (0, 0, 6, 1) = 32767 and then Observe (0, 0, 7, 1) = -32768);
      Require (Observe (0, 1, 6, 1) = 123 and then Observe (0, 1, 7, 1) = -456);
      Require (Observe (1, 0, 3, 1) = 8 and then Observe (1, 0, 5, 1) = 1);
      Require (Observe (1, 0, 6, 1) = -11 and then Observe (1, 0, 7, 1) = 17);
      Require (Observe (0, 0, 10, 1) = 93 and then Observe (0, 0, 8, 1) = 3);
      Require
        (Observe (0, 0, 9, 1) = 1
         and then Observe (0, 1, 9, 1) = 0
         and then Observe (0, 2, 9, 1) = 1);
      for M in Interfaces.C.unsigned range 0 .. 2 loop
         Require (Observe (M, 0, 0) = 1 and then Observe (M, 0, 1) = 1);
         Require (Observe (M, 0, 2) = 1 and then Observe (M, 0, 4) = 1);
      end loop;
      Require (Observe (0, 0, 3) = 8 and then Observe (0, 0, 5) = 3);
      Require (Observe (0, 0, 6) = -128 and then Observe (0, 0, 7) = 127);
      Require (Observe (0, 1, 6) = -7 and then Observe (0, 1, 7) = 3);
      Require (Observe (0, 2, 6) = 0 and then Observe (0, 2, 7) = -9);
      Require (Observe (1, 0, 3) = 16 and then Observe (1, 0, 5) = 2);
      Require (Observe (1, 0, 6) = 32767 and then Observe (1, 0, 7) = -32768);
      Require (Observe (1, 1, 6) = 123 and then Observe (1, 1, 7) = -456);
      Require (Observe (2, 0, 3) = 8 and then Observe (2, 0, 5) = 0);
      Require (Owner_Operation (0, 0, 0, 0) = 1);
      Require (Owner_Operation (0, 1, 0, 0) = 1);
      Require (Owner_Operation (1, 0, 0, -31) = 1);
      Require (Observe (0, 0, 6) = -31);
      C2.Add_RX_Job_Intervals (Object, List);
      Require (Observe (0, 0, 6) = -128);
      Require (Owner_Operation (2, 0, 0, 0) = -31);
      Require (Owner_Operation (3, 0, 0, 0) = 1);
      Require (Owner_Operation (5, 0, 0, 0) = 0);
      Require (Owner_Operation (6, 0, 0, 0) = 0);
      Require (Owner_Operation (4, 0, 0, 0) = 1);
      Require (Owner_Operation (5, 0, 0, 0) = 1);
      Require (Owner_Operation (6, 0, 0, 0) = 1);
      declare
         Changed_Table    : C2.Modulation_Table;
         Changed_List     : C2.RX_Job_Interval_List;
         Empty_Table      : C2.Modulation_Table;
         Empty_List       : C2.RX_Job_Interval_List;
         Changed_Interval : C2.RX_Job_Interval_Config := C2.Create_RX_Job_Interval (38, 20);
         Wrong_Kind       : C2.Modulation_Config := C2.Tone;
      begin
         begin
            C2.Replace_Waveform_Samples_I8 (Wrong_Kind, [(1, 2)]);
            raise Program_Error with "wrong kind accepted";
         exception
            when Constraint_Error =>
               null;
         end;
         begin
            C2.Replace_Waveform_Samples_I16 (Config, [(1, 2)]);
            raise Program_Error with "wrong width replacement accepted";
         exception
            when Constraint_Error =>
               null;
         end;
         C2.Append_Waveform_Sample_I16 (Config16, (-32768, 32767));
         C2.Append_Modulation (Changed_Table, Config, Index);
         C2.Append_Modulation (Changed_Table, Config16, Index);
         C2.Set_Modulations (Changed_Interval, Changed_Table);
         C2.Append_Job_Interval (Changed_List, Changed_Interval);
         C2.Add_Job_Intervals (Object, Changed_List);
         Require
           (Observe (0, 0, 5) = 1 and then Observe (0, 0, 6) = 9 and then Observe (0, 0, 7) = 8);
         Require
           (Observe (1, 0, 5) = 1
            and then Observe (1, 0, 6) = -32768
            and then Observe (1, 0, 7) = 32767);
         C2.Replace_Waveform_Samples_I8 (Config, [(1, -2), (-3, 4), (1, -2), (0, 0)]);
         C2.Replace_Waveform_Samples_I16 (Config16, [(7, -8), (-9, 0)]);
         Changed_Table := Empty_Table;
         C2.Append_Modulation (Changed_Table, Config, Index);
         C2.Append_Modulation (Changed_Table, Config16, Index);
         C2.Set_Modulations (Changed_Interval, Changed_Table);
         Changed_List := Empty_List;
         C2.Append_Job_Interval (Changed_List, Changed_Interval);
         C2.Add_Job_Intervals (Object, Changed_List);
         Require
           (Observe (0, 0, 5) = 4 and then Observe (0, 2, 6) = 1 and then Observe (0, 2, 7) = -2);
         Require (Observe (1, 1, 6) = -9 and then Observe (1, 1, 7) = 0);
         C2.Replace_Waveform_Samples_I8 (Config, []);
         C2.Replace_Waveform_Samples_I16 (Config16, []);
         Changed_Table := Empty_Table;
         C2.Append_Modulation (Changed_Table, Config, Index);
         C2.Append_Modulation (Changed_Table, Config16, Index);
         C2.Set_Modulations (Changed_Interval, Changed_Table);
         Changed_List := Empty_List;
         C2.Append_Job_Interval (Changed_List, Changed_Interval);
         C2.Add_Job_Intervals (Object, Changed_List);
         Require
           (Observe (0, 0, 5) = 0 and then Observe (0, 0, 4) = 1 and then Observe (0, 0, 3) = 8);
         Require
           (Observe (1, 0, 5) = 0 and then Observe (1, 0, 4) = 1 and then Observe (1, 0, 3) = 16);
      end;
   end Send_Waveform_Buffers;
   procedure Send_Custom_Waveforms (Object : in out C2.Job) is
      type Observation_Access is
        access function (I, M, P, S, F : Interfaces.C.unsigned) return Interfaces.Integer_64
      with Convention => C;
      type Scalar_Access is
        access function (I, M, P, F : Interfaces.C.unsigned) return Interfaces.C.double
      with Convention => C;
      function To_Observation is new Ada.Unchecked_Conversion (System.Address, Observation_Access);
      function To_Scalar is new Ada.Unchecked_Conversion (System.Address, Scalar_Access);
      Name              : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Library           : constant System.Address := DL_Open (Name, 2);
      function Symbol (Text : String) return System.Address is
         S      : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Text);
         Result : constant System.Address := DL_Sym (Library, S);
      begin
         Interfaces.C.Strings.Free (S);
         if Result = System.Null_Address then
            raise Program_Error with "CustomWaveform observer missing";
         end if;
         return Result;
      end Symbol;
      Observe           : constant Observation_Access :=
        To_Observation (Symbol ("mock_rf_f14_value"));
      Number            : constant Scalar_Access := To_Scalar (Symbol ("mock_rf_f14_double"));
      type Binary_Observation_Access is
        access function (I, M, F, N : Interfaces.C.unsigned) return Interfaces.Integer_64
      with Convention => C;
      function To_Binary_Observation is new
        Ada.Unchecked_Conversion (System.Address, Binary_Observation_Access);
      type Count_Access is access function return Interfaces.C.unsigned with Convention => C;
      function To_Count is new Ada.Unchecked_Conversion (System.Address, Count_Access);
      Add_Count         : constant Count_Access := To_Count (Symbol ("mock_rf_job_add_calls"));
      References        : constant Binary_Observation_Access :=
        To_Binary_Observation (Symbol ("mock_rf_f13_value"));
      List, Saved       : C2.RX_Job_Interval_List;
      Retained_Config   : C2.Modulation_Config;
      Retained_Profiles : C2.Waveform_Profile_Table;
      Retained_Patterns : C2.Waveform_Pattern_List;
      procedure Require (Condition : Boolean) is
      begin
         if not Condition then
            raise Program_Error with "public Ada CustomWaveform fidelity";
         end if;
      end Require;
      procedure Submit_One (Config : C2.Modulation_Config) is
         Table    : C2.Modulation_Table;
         Index    : C2.Modulation_Index;
         Interval : C2.RX_Job_Interval_Config := C2.Create_RX_Job_Interval (91, 0);
         One      : C2.RX_Job_Interval_List;
      begin
         C2.Append_Modulation (Table, Config, Index);
         C2.Set_Modulations (Interval, Table);
         C2.Append_Job_Interval (One, Interval);
         C2.Add_Job_Intervals (Object, One);
      end Submit_One;
   begin
      declare
         Profiles       : C2.Waveform_Profile_Table;
         Patterns       : C2.Waveform_Pattern_List;
         Pattern, Empty : C2.Event_Waveform_Pattern;
         Index          : C2.Waveform_Profile_Index;
         Profile        : C2.Waveform_Profile := C2.Create_Waveform_Profile;
      begin
         for P in 0 .. 6 loop
            C2.Set_Chip_Duration_Femtoseconds
              (Profile,
               (if P = 0
                then Interfaces.Integer_64'First
                elsif P = 1
                then Interfaces.Integer_64'Last
                else Interfaces.Integer_64 (-P)));
            C2.Set_Phase_Degrees (Profile, 90.0 + Long_Float (P), P = 0);
            C2.Set_Delta_Phase_Degrees (Profile, -180.0 + Long_Float (P), P = 1);
            C2.Set_Frequency_Offset_Hz (Profile, 11.0 + Long_Float (P), P = 2);
            C2.Set_Delta_Frequency_Hz (Profile, 23.0 + Long_Float (P), P = 3);
            C2.Set_Frequency_Slope_Hz_Sec (Profile, 37.0 + Long_Float (P), P = 4);
            C2.Set_Amplitude_Factor (Profile, -3.0 - Long_Float (P), P = 5);
            C2.Append_Profile (Profiles, Profile, Index);
            Require (Index = C2.Waveform_Profile_Index (P));
         end loop;
         C2.Append_Step (Pattern, 6, Interfaces.Integer_32'First);
         C2.Append_Step (Pattern, 0, 0);
         C2.Append_Step (Pattern, 6, Interfaces.Integer_32'Last);
         C2.Append_Pattern (Patterns, Pattern);
         C2.Append_Pattern (Patterns, Empty);
         C2.Append_Step (Pattern, 1, -7);
         C2.Append_Pattern (Patterns, Pattern);
         Retained_Profiles := Profiles;
         Retained_Patterns := Patterns;
         Retained_Config := C2.Custom_Waveform (Profiles, Patterns);
         C2.Set_Phase_Degrees (Profile, 777.0, False);
         C2.Append_Profile (Profiles, Profile, Index);
         C2.Append_Step (Pattern, 7, -13);
         C2.Append_Pattern (Patterns, Pattern);
         for I in 0 .. 1 loop
            declare
               Interval : C2.RX_Job_Interval_Config :=
                 C2.Create_RX_Job_Interval (Interfaces.Unsigned_32 (90 + I), 0);
               Table    : C2.Modulation_Table;
               M        : C2.Modulation_Index;
               TX       : C2.TX_Transmit_Event_Config :=
                 C2.Create_TX_Transmit_Event (7, "", 0, 0, 0.0);
            begin
               C2.Append_Modulation (Table, Retained_Config, M);
               C2.Append_Modulation_Reference (TX, M);
               C2.Append_Modulation (Table, C2.Custom_Waveform (Profiles, Patterns), M);
               C2.Append_Modulation_Reference (TX, M);
               C2.Append_Modulation_Reference (TX, 0);
               C2.Append_Modulation_Reference (TX, C2.No_Modulation);
               C2.Set_Modulations (Interval, Table);
               C2.Set_Phase_Offset_Radians (TX, -0.25);
               C2.Append_TX_Event (Interval, TX);
               C2.Append_TX_Event (Interval, TX);
               C2.Append_Job_Interval (List, Interval);
            end;
         end loop;
         Saved := List;
      end;
      --  Every source scope has expired before observing native retained copies.
      C2.Add_RX_Job_Intervals (Object, Saved);
      for I in Interfaces.C.unsigned range 0 .. 1 loop
         for M in Interfaces.C.unsigned range 0 .. 1 loop
            Require (Observe (I, M, 0, 0, 0) = 1 and then Observe (I, M, 0, 0, 1) = 1);
            Require (Observe (I, M, 0, 0, 2) = 7 + Interfaces.Integer_64 (M));
            Require (Observe (I, M, 0, 0, 3) = 3 + Interfaces.Integer_64 (M));
            Require (Observe (I, M, 1, 0, 4) = 0);
            for P in Interfaces.C.unsigned range 0 .. 6 loop
               Require
                 (Observe (I, M, P, 0, 7)
                  = (if P = 0
                     then Interfaces.Integer_64'First
                     elsif P = 1
                     then Interfaces.Integer_64'Last
                     else -Interfaces.Integer_64 (P)));
               for F in Interfaces.C.unsigned range 0 .. 5 loop
                  Require (Observe (I, M, P, 0, F + 8) = Boolean'Pos (P = F));
               end loop;
               Require (Number (I, M, P, 0) = 90.0 + Interfaces.C.double (P));
               Require (Number (I, M, P, 1) = -180.0 + Interfaces.C.double (P));
               Require (Number (I, M, P, 2) = 11.0 + Interfaces.C.double (P));
               Require (Number (I, M, P, 3) = 23.0 + Interfaces.C.double (P));
               Require (Number (I, M, P, 4) = 37.0 + Interfaces.C.double (P));
               Require (Number (I, M, P, 5) = -3.0 - Interfaces.C.double (P));
            end loop;
            for P in Interfaces.C.unsigned range 0 .. 2 loop
               if P /= 1 then
                  Require (Observe (I, M, P, 0, 4) = (if P = 0 then 3 else 4));
                  Require
                    (Observe (I, M, P, 0, 5) = Interfaces.Integer_64 (Interfaces.Integer_32'First));
                  Require (Observe (I, M, P, 0, 6) = 6);
                  Require (Observe (I, M, P, 1, 5) = 0 and then Observe (I, M, P, 1, 6) = 0);
                  Require
                    (Observe (I, M, P, 2, 5) = Interfaces.Integer_64 (Interfaces.Integer_32'Last));
                  Require (Observe (I, M, P, 2, 6) = 6);
                  if P = 2 then
                     Require (Observe (I, M, P, 3, 5) = -7 and then Observe (I, M, P, 3, 6) = 1);
                  end if;
               end if;
            end loop;
            if M = 1 then
               Require (Number (I, M, 7, 0) = 777.0 and then Observe (I, M, 3, 4, 6) = 7);
               Require (Observe (I, M, 3, 4, 5) = -13);
            end if;
         end loop;
         for E in Interfaces.C.unsigned range 0 .. 1 loop
            Require (References (I, E, 5, 0) = 4);
            for N in Interfaces.C.unsigned range 0 .. 3 loop
               Require
                 (References (I, E, 6, N)
                  = (case N is
                       when 0 | 2  => 0,
                       when 1      => 1,
                       when others => -1));
            end loop;
         end loop;
      end loop;
      declare
         Changed        : C2.Modulation_Config := Retained_Config;
         Short, Longer  : C2.Waveform_Profile_Table;
         Empty          : C2.Waveform_Pattern_List;
         Empty_Profiles : C2.Waveform_Profile_Table;
         Index          : C2.Waveform_Profile_Index;
         Before         : Interfaces.C.unsigned;
      begin
         C2.Append_Profile (Short, C2.Create_Waveform_Profile (19), Index);
         C2.Replace_Custom_Waveform_Profiles (Changed, Short);
         Before := Add_Count.all;
         begin
            Submit_One (Changed);
            raise Program_Error with "stale profile reference accepted";
         exception
            when Constraint_Error =>
               null;
         end;
         Require (Add_Count.all = Before);
         Require (Number (0, 0, 0, 0) = 90.0);
         C2.Replace_Custom_Waveform_Profiles (Changed, Retained_Profiles);
         Submit_One (Changed);
         Require (Observe (0, 0, 0, 0, 2) = 7);
         Longer := Retained_Profiles;
         C2.Append_Profile (Longer, C2.Create_Waveform_Profile (29), Index);
         C2.Replace_Custom_Waveform_Profiles (Changed, Longer);
         Submit_One (Changed);
         Require (Observe (0, 0, 0, 0, 2) = 8 and then Observe (0, 0, 7, 0, 7) = 29);
         C2.Replace_Custom_Waveform_Patterns (Changed, Empty);
         C2.Replace_Custom_Waveform_Profiles (Changed, Empty_Profiles);
         Submit_One (Changed);
         Require (Observe (0, 0, 0, 0, 2) = 0 and then Observe (0, 0, 0, 0, 3) = 0);
         C2.Replace_Custom_Waveform_Profiles (Changed, Retained_Profiles);
         C2.Replace_Custom_Waveform_Patterns (Changed, Retained_Patterns);
         Submit_One (Changed);
         Require (Observe (0, 0, 0, 0, 3) = 3 and then Observe (0, 0, 2, 3, 6) = 1);
      end;
      for Which in 0 .. 1 loop
         declare
            Wrong : C2.Modulation_Config := C2.Tone;
         begin
            begin
               if Which = 0 then
                  C2.Replace_Custom_Waveform_Profiles (Wrong, Retained_Profiles);
               else
                  C2.Replace_Custom_Waveform_Patterns (Wrong, Retained_Patterns);
               end if;
               raise Program_Error with "wrong-kind custom replacement accepted";
            exception
               when Constraint_Error =>
                  null;
            end;
            Submit_One (Wrong);
            Require (References (0, 0, 2, 0) = 0);
         end;
      end loop;
      C2.Add_Job_Intervals (Object, Saved);
      Require (Number (1, 1, 7, 0) = 777.0);
      declare
         Changed  : C2.Modulation_Config := Retained_Config;
         Profiles : C2.Waveform_Profile_Table;
         Pattern  : C2.Event_Waveform_Pattern;
         Patterns : C2.Waveform_Pattern_List;
         Index    : C2.Waveform_Profile_Index;
         Profile  : C2.Waveform_Profile := C2.Create_Waveform_Profile (-37);
         Before   : Interfaces.C.unsigned;
      begin
         C2.Set_Phase_Degrees (Profile, IEEE (16#8000_0000_0000_0000#), False);
         C2.Set_Delta_Phase_Degrees (Profile, IEEE (16#7FF0_0000_0000_0000#), False);
         C2.Set_Frequency_Offset_Hz (Profile, IEEE (16#FFF0_0000_0000_0000#), False);
         C2.Set_Delta_Frequency_Hz (Profile, IEEE (16#7FF8_0000_0000_0001#), False);
         C2.Set_Frequency_Slope_Hz_Sec (Profile, 19.25, False);
         C2.Set_Amplitude_Factor (Profile, -7.5, False);
         C2.Append_Profile (Profiles, Profile, Index);
         C2.Append_Step (Pattern, Index, -1);
         C2.Append_Pattern (Patterns, Pattern);
         C2.Replace_Custom_Waveform_Profiles (Changed, Profiles);
         C2.Replace_Custom_Waveform_Patterns (Changed, Patterns);
         Submit_One (Changed);
         Require (Observe (0, 0, 0, 0, 3) = 1 and then Observe (0, 0, 0, 0, 5) = -1);
         Require (Bits (Number (0, 0, 0, 0)) = 16#8000_0000_0000_0000#);
         Require (Bits (Number (0, 0, 0, 1)) = 16#7FF0_0000_0000_0000#);
         Require (Bits (Number (0, 0, 0, 2)) = 16#FFF0_0000_0000_0000#);
         Require (Number (0, 0, 0, 3) /= Number (0, 0, 0, 3));
         Require (Number (0, 0, 0, 4) = 19.25 and then Number (0, 0, 0, 5) = -7.5);
         for F in Interfaces.C.unsigned range 8 .. 13 loop
            Require (Observe (0, 0, 0, 0, F) = 0);
         end loop;
         C2.Add_Job_Intervals (Object, Saved);
         Before := Add_Count.all;
         Ada.Environment_Variables.Set ("AMS_MEL_TEST_RF_JOB_FAILURE", "later-custom-step");
         begin
            C2.Add_Job_Intervals (Object, Saved);
            raise Program_Error with "nested preparation failure not reached";
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Ada.Environment_Variables.Clear ("AMS_MEL_TEST_RF_JOB_FAILURE");
         Require (Add_Count.all = Before and then Number (1, 1, 7, 0) = 777.0);
         C2.Add_Job_Intervals (Object, Saved);
         Require (Observe (1, 1, 3, 4, 6) = 7);
      end;
      if DL_Close (Library) /= 0 then
         raise Program_Error;
      end if;
      Interfaces.C.Strings.Free (Name);
   end Send_Custom_Waveforms;

   procedure Send_Binary_Modulations (Object : in out C2.Job) is
      type Observation_Access is
        access function (I, M, F, N : Interfaces.C.unsigned) return Interfaces.Integer_64
      with Convention => C;
      type Double_Access is
        access function (I, M : Interfaces.C.unsigned) return Interfaces.C.double
      with Convention => C;
      function To_Observation is new Ada.Unchecked_Conversion (System.Address, Observation_Access);
      function To_Double is new Ada.Unchecked_Conversion (System.Address, Double_Access);
      Name        : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Library     : constant System.Address := DL_Open (Name, 2);
      function Symbol (Text : String) return System.Address is
         S      : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Text);
         Result : constant System.Address := DL_Sym (Library, S);
      begin
         Interfaces.C.Strings.Free (S);
         if Result = System.Null_Address then
            raise Program_Error with "binary observer missing";
         end if;
         return Result;
      end Symbol;
      Observe     : constant Observation_Access := To_Observation (Symbol ("mock_rf_f13_value"));
      Number      : constant Double_Access := To_Double (Symbol ("mock_rf_f13_double"));
      TX_Number   : constant Number_Access := To_Number (Symbol ("mock_rf_f11_double"));
      RX_Number   : constant Number_Access := To_Number (Symbol ("mock_rf_f5_double"));
      List, Saved : C2.RX_Job_Interval_List;
      procedure Require (Condition : Boolean) is
      begin
         if not Condition then
            raise Program_Error with "public Ada binary modulation fidelity";
         end if;
      end Require;
      procedure Check_Bits (I, M : Interfaces.C.unsigned; Values : C2.Modulation_Boolean_Array) is
      begin
         Require (Observe (I, M, 1, 0) = 1);
         Require (Observe (I, M, 4, 0) = Values'Length);
         for N in Values'Range loop
            Require
              (Observe (I, M, 8, Interfaces.C.unsigned (N - Values'First))
               = Boolean'Pos (Values (N)));
         end loop;
      end Check_Bits;
      procedure Submit_One (Config : C2.Modulation_Config; Count : Interfaces.Unsigned_64) is
         Interval : C2.RX_Job_Interval_Config :=
           C2.Create_RX_Job_Interval (21, 19, Sequence_Repeat_Count => Count);
         Table    : C2.Modulation_Table;
         Index    : C2.Modulation_Index;
         One      : C2.RX_Job_Interval_List;
      begin
         C2.Append_Modulation (Table, Config, Index);
         C2.Set_Modulations (Interval, Table);
         C2.Append_Job_Interval (One, Interval);
         C2.Add_Job_Intervals (Object, One);
      end Submit_One;
      procedure Reject_Shared_OOK is
         Interval : C2.RX_Job_Interval_Config := C2.Create_RX_Job_Interval (91, 19);
         Table    : C2.Modulation_Table;
         Index    : C2.Modulation_Index;
         Invalid  : C2.RX_Job_Interval_List;
      begin
         C2.Append_Modulation (Table, C2.On_Off_Keying ([True]), Index);
         for E in 1 .. 2 loop
            declare
               Event : C2.TX_Transmit_Event_Config :=
                 C2.Create_TX_Transmit_Event (7, "same ID", 1, 3, 17.5);
            begin
               C2.Append_Modulation_Reference (Event, Index);
               C2.Append_TX_Event (Interval, Event);
            end;
         end loop;
         C2.Set_Modulations (Interval, Table);
         C2.Append_Job_Interval (Invalid, Interval);
         begin
            C2.Add_Job_Intervals (Object, Invalid);
            raise Program_Error with "shared OOK accepted";
         exception
            when Constraint_Error =>
               null;
         end;
      end Reject_Shared_OOK;
   begin
      Interfaces.C.Strings.Free (Name);
      AMS.MEL.RF.C2.Testing.Check_Modulation_Representation;
      declare
         Source : C2.Modulation_Boolean_Array (1 .. 5) := [True, False, False, True, False];
         Chips  : C2.Modulation_Boolean_Array (1 .. 7) :=
           [False, True, False, True, True, False, False];
         OOK    : C2.Modulation_Config := C2.On_Off_Keying (Source);
         CPFSK  : C2.Modulation_Config :=
           C2.CPFSK_Pulse (Chips, Interfaces.Integer_64'First, Interfaces.Integer_64'Last, -0.0);
      begin
         for I in 0 .. 1 loop
            declare
               Interval : C2.RX_Job_Interval_Config :=
                 C2.Create_RX_Job_Interval
                   (Interfaces.Unsigned_32 (40 + I),
                    71,
                    Sequence_Repeat_Count => Interfaces.Unsigned_64 (5 - 2 * I));
               Table    : C2.Modulation_Table;
               Index    : C2.Modulation_Index;
               RX       : C2.RX_Receive_Event_Config :=
                 C2.Create_RX_Receive_Event (81, "binary RX", -9, 11, 12.5, 2.0);
            begin
               if I = 1 then
                  C2.Replace_OOK_Pulse_Enables (OOK, [False, True, True]);
                  CPFSK := C2.CPFSK_Pulse ([True, False, True, False], 37, -59, -7.25);
               end if;
               C2.Append_Modulation (Table, C2.Tone, Index);
               C2.Append_Modulation (Table, OOK, Index);
               Require (Index = 1);
               C2.Append_Modulation (Table, CPFSK, Index);
               C2.Append_Modulation (Table, OOK, Index);
               C2.Append_Modulation (Table, C2.Linear_Frequency_Modulation (13.5), Index);
               C2.Append_Modulation (Table, C2.Phase_Code (17, [1.25, -2.5]), Index);
               for E in 0 .. 1 loop
                  declare
                     Event : C2.TX_Transmit_Event_Config :=
                       C2.Create_TX_Transmit_Event (7, "duplicate ID", 1, 3, 17.5);
                  begin
                     C2.Append_Modulation_Reference (Event, C2.Modulation_Index (1 + 2 * E));
                     C2.Append_Modulation_Reference (Event, C2.No_Modulation);
                     C2.Append_Modulation_Reference (Event, 2);
                     C2.Append_Modulation_Reference (Event, C2.Modulation_Index (1 + 2 * E));
                     C2.Append_Modulation_Reference (Event, 5);
                     C2.Append_Polarization (Event, 1.0, -2.0, 3.0, -4.0);
                     C2.Append_TX_Event (Interval, Event);
                  end;
               end loop;
               C2.Append_Polarization (RX, 5.0, -6.0, 7.0, -8.0);
               C2.Append_RX_Event (Interval, RX);
               C2.Set_Modulations (Interval, Table);
               C2.Append_Job_Interval (List, Interval);
            end;
         end loop;
         Saved := List;
         for N in Source'Range loop
            Source (N) := not Source (N);
         end loop;
         for N in Chips'Range loop
            Chips (N) := not Chips (N);
         end loop;
         C2.Replace_CPFSK_Chips (CPFSK, []);
         C2.Replace_OOK_Pulse_Enables (OOK, []);
      end;
      C2.Add_Job_Intervals (Object, Saved);
      Check_Bits (0, 1, [True, False, False, True, False]);
      Check_Bits (0, 3, [True, False, False, True, False]);
      Check_Bits (0, 2, [False, True, False, True, True, False, False]);
      Check_Bits (1, 1, [False, True, True]);
      Check_Bits (1, 3, [False, True, True]);
      Check_Bits (1, 2, [True, False, True, False]);
      Reject_Shared_OOK;
      Check_Bits (1, 2, [True, False, True, False]);
      Require (Observe (0, 2, 2, 0) = 4 and Observe (0, 1, 2, 0) = 3);
      Require (Observe (0, 2, 3, 0) = Interfaces.Integer_64'Last);
      Require (Observe (0, 2, 7, 0) = Interfaces.Integer_64'First);
      Require (Observe (1, 2, 3, 0) = -59 and Observe (1, 2, 7, 0) = 37);
      Require (Bits (Number (0, 2)) = Bits (Interfaces.C.double (-0.0)) and Number (1, 2) = -7.25);
      for I in 0 .. 1 loop
         for E in 0 .. 1 loop
            Require (TX_Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 0, 0) = 1.0);
            Require (TX_Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 0, 1) = -2.0);
            Require (TX_Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 0, 2) = 3.0);
            Require (TX_Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 0, 3) = -4.0);
            Require (Observe (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 5, 0) = 5);
            Require
              (Observe (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 6, 0)
               = Interfaces.Integer_64 (1 + 2 * E));
            Require (Observe (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 6, 1) = -1);
            Require (Observe (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 6, 2) = 2);
            Require
              (Observe (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 6, 3)
               = Interfaces.Integer_64 (1 + 2 * E));
            Require (Observe (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 6, 4) = 5);
         end loop;
         Require (RX_Number (Interfaces.C.unsigned (I), 0, 0, 0) = 5.0);
         Require (RX_Number (Interfaces.C.unsigned (I), 0, 0, 1) = -6.0);
         Require (RX_Number (Interfaces.C.unsigned (I), 0, 0, 2) = 7.0);
         Require (RX_Number (Interfaces.C.unsigned (I), 0, 0, 3) = -8.0);
      end loop;
      declare
         Config : C2.Modulation_Config := C2.On_Off_Keying ([]);
      begin
         Submit_One (Config, 0);
         C2.Append_OOK_Pulse_Enable (Config, True);
         Submit_One (Config, 1);
         Check_Bits (0, 0, [True]);
         begin
            C2.Replace_CPFSK_Chips (Config, [False]);
            raise Program_Error with "wrong kind accepted";
         exception
            when Constraint_Error =>
               null;
         end;
         Submit_One (Config, 1);
         Check_Bits (0, 0, [True]);
         begin
            Submit_One (Config, 2);
            raise Program_Error with "OOK mismatch accepted";
         exception
            when Constraint_Error =>
               null;
         end;
         Check_Bits (0, 0, [True]);
         C2.Replace_OOK_Pulse_Enables (Config, [False, True, False]);
         Submit_One (Config, 3);
         Check_Bits (0, 0, [False, True, False]);
         Config := C2.CPFSK_Pulse ([], -31, 47, 19.25);
         C2.Append_CPFSK_Chip (Config, False);
         C2.Append_CPFSK_Chip (Config, True);
         Submit_One (Config, 1);
         Check_Bits (0, 0, [False, True]);
         C2.Replace_CPFSK_Chips (Config, [True]);
         Submit_One (Config, 1);
         Check_Bits (0, 0, [True]);
         C2.Replace_CPFSK_Chips (Config, []);
         Submit_One (Config, 1);
         Check_Bits (0, 0, []);
         for Length in 63 .. 65 loop
            declare
               Boundary : C2.Modulation_Boolean_Array (1 .. Length);
            begin
               for N in Boundary'Range loop
                  Boundary (N) := N mod 2 = 0;
               end loop;
               Config := C2.On_Off_Keying (Boundary);
               Submit_One (Config, Interfaces.Unsigned_64 (Length));
               Check_Bits (0, 0, Boundary);
               Config := C2.CPFSK_Pulse (Boundary, -31, 47, 19.25);
               Submit_One (Config, 1);
               Check_Bits (0, 0, Boundary);
            end;
         end loop;
         declare
            Values : constant C2.Phase_Chip_Array :=
              [IEEE (16#7FF0_0000_0000_0000#),
               IEEE (16#FFF0_0000_0000_0000#),
               IEEE (16#7FF8_0000_0000_0001#)];
         begin
            for Value of Values loop
               Config := C2.CPFSK_Pulse ([True], -31, 47, Value);
               Submit_One (Config, 1);
               if Value /= Value then
                  Require (Number (0, 0) /= Number (0, 0));
               else
                  Require (Number (0, 0) = Interfaces.C.double (Value));
               end if;
            end loop;
         end;
      end;
      C2.Add_RX_Job_Intervals (Object, List);
      Check_Bits (1, 2, [True, False, True, False]);
      if DL_Close (Library) /= 0 then
         raise Program_Error;
      end if;
   end Send_Binary_Modulations;
   procedure Send_Modulations (Object : in out C2.Job) is
      type Observation_Access is
        access function (I, M, F, N : Interfaces.C.unsigned) return Interfaces.Integer_64
      with Convention => C;
      type Double_Access is
        access function (I, M, N : Interfaces.C.unsigned) return Interfaces.C.double
      with Convention => C;
      type Count_Access is access function return Interfaces.C.unsigned with Convention => C;
      function To_Observation is new Ada.Unchecked_Conversion (System.Address, Observation_Access);
      function To_Double is new Ada.Unchecked_Conversion (System.Address, Double_Access);
      function To_Count is new Ada.Unchecked_Conversion (System.Address, Count_Access);
      Library_Name      : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Library           : constant System.Address := DL_Open (Library_Name, 2);
      function Symbol (Name : String) return System.Address is
         Text    : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Name);
         Address : constant System.Address := DL_Sym (Library, Text);
      begin
         Interfaces.C.Strings.Free (Text);
         if Address = System.Null_Address then
            raise Program_Error with "modulation observer missing: " & Name;
         end if;
         return Address;
      end Symbol;
      Observe           : constant Observation_Access :=
        To_Observation (Symbol ("mock_rf_f12_value"));
      Number            : constant Double_Access := To_Double (Symbol ("mock_rf_f12_double"));
      Add_Count         : constant Count_Access := To_Count (Symbol ("mock_rf_job_add_calls"));
      Positive_Infinity : constant Long_Float := IEEE (16#7FF0_0000_0000_0000#);
      Negative_Infinity : constant Long_Float := IEEE (16#FFF0_0000_0000_0000#);
      Quiet_NaN         : constant Long_Float := IEEE (16#7FF8_0000_0000_0001#);
      First_Chips       : constant C2.Phase_Chip_Array :=
        [1.25, -2.5, 0.0, -0.0, Positive_Infinity, Negative_Infinity, Quiet_NaN, 1.25];
      Second_Chips      : constant C2.Phase_Chip_Array := [-7.75, 3.5, -7.75];
      type References is array (Positive range <>) of C2.Modulation_Index;
      First_References  : constant References := [2, C2.No_Modulation, 1, 2, 0];
      Second_References : constant References := [1, 0, C2.No_Modulation, 1];
      List, Saved_List  : C2.RX_Job_Interval_List;
      procedure Require (Condition : Boolean; Message : String) is
      begin
         if not Condition then
            raise Program_Error with "modulation: " & Message;
         end if;
      end Require;
      procedure Equal_Number (Actual : Interfaces.C.double; Expected : Long_Float) is
      begin
         if Expected /= Expected then
            Require (Actual /= Actual, "NaN classification");
         else
            Require (Actual = Interfaces.C.double (Expected), "numeric value");
            if Expected = 0.0 then
               Require (Bits (Actual) = Bits (Interfaces.C.double (Expected)), "signed zero");
            end if;
         end if;
      end Equal_Number;
      procedure Check_Kind (I, M : Interfaces.C.unsigned; Kind : Interfaces.Integer_64) is
      begin
         Require (Observe (I, M, 1, 0) = 1, "outer CommonModulations alternative");
         Require (Observe (I, M, 2, 0) = Kind, "inner selected alternative");
      end Check_Kind;
      procedure Check_Phase
        (I, M     : Interfaces.C.unsigned;
         Duration : Interfaces.Integer_64;
         Chips    : C2.Phase_Chip_Array) is
      begin
         Check_Kind (I, M, 1);
         Require (Observe (I, M, 3, 0) = Duration, "chip duration");
         Require (Observe (I, M, 4, 0) = Chips'Length, "phase count");
         for N in Chips'Range loop
            Equal_Number (Number (I, M, Interfaces.C.unsigned (N - Chips'First)), Chips (N));
         end loop;
      end Check_Phase;
      procedure Check_LFM (I, M : Interfaces.C.unsigned; Extent : Long_Float) is
      begin
         Check_Kind (I, M, 2);
         Equal_Number (Number (I, M, 0), Extent);
      end Check_LFM;
      procedure Check_References (I, E : Interfaces.C.unsigned; Values : References) is
      begin
         Require (Observe (I, E, 5, 0) = Values'Length, "reference count");
         for N in Values'Range loop
            Require
              (Observe (I, E, 6, Interfaces.C.unsigned (N - Values'First))
               = Interfaces.Integer_64 (Values (N)),
               "every ordered reference");
         end loop;
      end Check_References;
      function Mixed_Interval (ID : Natural) return C2.RX_Job_Interval_Config is
         Result : C2.RX_Job_Interval_Config :=
           C2.Create_RX_Job_Interval
             (Interfaces.Unsigned_32 (ID),
              20,
              Interval_Start_Femtoseconds => Interfaces.Integer_64 (ID));
         RX     : C2.RX_Receive_Event_Config :=
           C2.Create_RX_Receive_Event
             (Interfaces.Unsigned_32 (80 + ID), "modulation RX", -9, 11, 12.5, 2.0);
      begin
         C2.Set_Phase_Offset_Radians (RX, 3.25 + Long_Float (ID));
         C2.Set_Iteration_Hold_Count (RX, Interfaces.Unsigned_64 (9 + ID));
         C2.Append_Polarization (RX, Long_Float (ID + 1), -2.0, 3.0, -4.0);
         C2.Append_RX_Event (Result, RX);
         return Result;
      end Mixed_Interval;
      function TX_Event (ID : Natural; Values : References) return C2.TX_Transmit_Event_Config is
         Result : C2.TX_Transmit_Event_Config :=
           C2.Create_TX_Transmit_Event
             (Interfaces.Unsigned_32 (ID), "modulation TX", -7, 11, 100.25, -12.5, 3, -3);
      begin
         C2.Append_Polarization (Result, Long_Float (ID + 1), -5.0, 6.0, -7.0);
         C2.Set_Phase_Offset_Radians (Result, -4.25 - Long_Float (ID));
         C2.Set_Iteration_Hold_Count (Result, Interfaces.Unsigned_64 (ID + 2));
         C2.Set_Channelization_Enabled (Result, True);
         for Value of Values loop
            C2.Append_Modulation_Reference (Result, Value);
         end loop;
         return Result;
      end TX_Event;
      procedure Check_Mixed
        (I : Interfaces.C.unsigned; ID, Event_ID : Natural; E : Interfaces.C.unsigned := 0)
      is
         RX_Value : Number_Access;
      begin
         Require
           (TX_Value (I, 0, 1, 0) = 1
            and then TX_Value (I, 0, 14, 0) = Interfaces.Unsigned_64 (80 + ID),
            "retained RX association");
         Require (TX_Value (I, 0, 15, 0) = Interfaces.Unsigned_64 (ID), "interval association");
         Require
           (RX_Observation (I, 4) = 1
            and then RX_Observation (I, 9) = Interfaces.Integer_64 (9 + ID),
            "RX controls");
         Equal_Number (RX_Phase (I), 3.25 + Long_Float (ID));
         RX_Value := To_Number (Symbol ("mock_rf_f5_double"));
         for C in Interfaces.C.unsigned range 0 .. 3 loop
            Equal_Number
              (RX_Value (I, 0, 0, C),
               (case C is
                  when 0      => Long_Float (ID + 1),
                  when 1      => -2.0,
                  when 2      => 3.0,
                  when others => -4.0));
         end loop;
         Require (TX_Value (I, E, 2, 0) = Interfaces.Unsigned_64 (Event_ID), "TX association");
         Require
           (TX_Control (I, E, 0) = 1
            and then TX_Control (I, E, 5) = Interfaces.Unsigned_64 (Event_ID + 2)
            and then TX_Control (I, E, 7) = 1,
            "retained TX controls");
         Equal_Number (TX_Component (I, E, 0, 4), -4.25 - Long_Float (Event_ID));
         for C in Interfaces.C.unsigned range 0 .. 3 loop
            Equal_Number
              (TX_Component (I, E, 0, C),
               (case C is
                  when 0      => Long_Float (Event_ID + 1),
                  when 1      => -5.0,
                  when 2      => 6.0,
                  when others => -7.0));
         end loop;
         Equal_Number (TX_Number (I, E, 0), 100.25);
         Equal_Number (TX_Number (I, E, 1), -12.5);
      end Check_Mixed;
      procedure Submit (Interval : C2.RX_Job_Interval_Config; RX_Name : Boolean := False) is
         Fresh : C2.RX_Job_Interval_List;
      begin
         C2.Append_Job_Interval (Fresh, Interval);
         if RX_Name then
            C2.Add_RX_Job_Intervals (Object, Fresh);
         else
            C2.Add_Job_Intervals (Object, Fresh);
         end if;
      end Submit;
      procedure Check_Asymmetric is
      begin
         Require (Observe (0, 0, 0, 0) = 3 and then Observe (1, 0, 0, 0) = 3, "table sizes");
         Check_Kind (0, 0, 0);
         Check_Phase (0, 1, Interfaces.Integer_64'First, First_Chips);
         Check_LFM (0, 2, 4.25);
         Check_LFM (1, 0, -7.5);
         Check_Phase (1, 1, Interfaces.Integer_64'Last, Second_Chips);
         Check_LFM (1, 2, Negative_Infinity);
         Check_References (0, 0, First_References);
         Check_References (0, 1, [C2.No_Modulation]);
         Check_References (1, 0, Second_References);
         Check_References (1, 1, []);
         Check_Mixed (0, 10, 20);
         Check_Mixed (1, 11, 22);
         Check_Mixed (0, 10, 21, 1);
         Check_Mixed (1, 11, 23, 1);
         Require
           (TX_Value (0, 1, 2, 0) = 21 and then TX_Value (1, 1, 2, 0) = 23,
            "second TX association");
      end Check_Asymmetric;
      procedure Check_Trace (Interval : C2.RX_Job_Interval_Config; Tables, Indices : Natural) is
         File                     : Ada.Text_IO.File_Type;
         Trace                    : constant String := Path & ".corrective-modulations.trace";
         Table_Calls, Index_Calls : Natural := 0;
      begin
         Ada.Text_IO.Create (File, Ada.Text_IO.Out_File, Trace);
         Ada.Text_IO.Close (File);
         Ada.Environment_Variables.Set ("AMS_MEL_TEST_RF_TX_TRACE", Trace);
         Submit (Interval, True);
         Ada.Environment_Variables.Clear ("AMS_MEL_TEST_RF_TX_TRACE");
         Ada.Text_IO.Open (File, Ada.Text_IO.In_File, Trace);
         while not Ada.Text_IO.End_Of_File (File) loop
            declare
               Line : constant String := Ada.Text_IO.Get_Line (File);
            begin
               if Line = "setModulations" then
                  Table_Calls := Table_Calls + 1;
               end if;
               if Line = "setModIndex" then
                  Index_Calls := Index_Calls + 1;
               end if;
            end;
         end loop;
         Ada.Text_IO.Delete (File);
         Require
           (Table_Calls = Tables and then Index_Calls = Indices, "absence/empty setter trace");
         Check_Mixed (0, 30, 31);
      end Check_Trace;
   begin
      Interfaces.C.Strings.Free (Library_Name);
      Require (Library /= System.Null_Address, "mock unavailable");
      C2.Testing.Check_Modulation_Representation;
      --  All source scopes end before observation of every copied entry/event.
      for I in 0 .. 1 loop
         declare
            Source   : C2.Phase_Chip_Array (1 .. (if I = 0 then 8 else 3)) :=
              (if I = 0 then First_Chips else Second_Chips);
            Phase    : C2.Modulation_Config :=
              C2.Phase_Code
                ((if I = 0 then Interfaces.Integer_64'First else Interfaces.Integer_64'Last),
                 Source);
            Table    : C2.Modulation_Table;
            Interval : C2.RX_Job_Interval_Config := Mixed_Interval (10 + I);
            Index    : C2.Modulation_Index;
         begin
            for N in Source'Range loop
               Source (N) := 99.0;
            end loop;
            C2.Append_Modulation
              (Table, (if I = 0 then C2.Tone else C2.Linear_Frequency_Modulation (-7.5)), Index);
            Require (Index = 0, "zero-based append index");
            C2.Append_Modulation (Table, Phase, Index);
            C2.Replace_Phase_Chips (Phase, [99.0]);
            C2.Append_Modulation
              (Table,
               C2.Linear_Frequency_Modulation ((if I = 0 then 4.25 else Negative_Infinity)),
               Index);
            C2.Set_Modulations (Interval, Table);
            C2.Append_Modulation (Table, C2.Tone, Index);
            for E in 0 .. 1 loop
               declare
                  Event : C2.TX_Transmit_Event_Config :=
                    TX_Event
                      (20 + 2 * I + E,
                       (if E = 0
                        then (if I = 0 then First_References else Second_References)
                        else []));
               begin
                  if E = 1 and I = 1 then
                     C2.Set_Empty_Modulation_References (Event);
                  end if;
                  C2.Append_TX_Event (Interval, Event);
                  C2.Clear_Modulation_References (Event);
                  C2.Append_Modulation_Reference (Event, 0);
               end;
            end loop;
            C2.Append_Job_Interval (List, Interval);
            C2.Clear_Modulations (Interval);
         end;
      end loop;
      Saved_List := List;
      declare
         Extra : constant C2.RX_Job_Interval_Config := Mixed_Interval (99);
      begin
         C2.Append_Job_Interval (List, Extra);
      end;
      C2.Add_Job_Intervals (Object, Saved_List);
      Check_Asymmetric;
      C2.Add_Job_Intervals (Object, List);
      Check_Asymmetric;
      Require
        (TX_Value (2, 0, 15, 0) = 99 and then TX_Value (2, 0, 14, 0) = 179,
         "mutated source list submitted independently");
      C2.Add_RX_Job_Intervals (Object, Saved_List);
      Check_Asymmetric;
      --  Submit BOTH retained and mutated source values, not just a length check.
      declare
         Source             : C2.Phase_Chip_Array := [8.5, -9.25, 8.5];
         Phase              : C2.Modulation_Config := C2.Phase_Code (-17, Source);
         Retained, Modified : C2.Modulation_Table;
         Original, Changed  : C2.RX_Job_Interval_Config := Mixed_Interval (30);
         Event              : C2.TX_Transmit_Event_Config := TX_Event (31, [0]);
         Index              : C2.Modulation_Index;
         procedure Submit_Phase (Config : C2.Modulation_Config; Expected : C2.Phase_Chip_Array) is
            Table    : C2.Modulation_Table;
            Interval : C2.RX_Job_Interval_Config := Mixed_Interval (30);
            Position : C2.Modulation_Index;
         begin
            C2.Append_Modulation (Table, Config, Position);
            C2.Append_Modulation
              (Table, C2.Linear_Frequency_Modulation (Positive_Infinity), Position);
            C2.Set_Modulations (Interval, Table);
            C2.Append_TX_Event (Interval, Event);
            Submit (Interval);
            Check_Phase (0, 0, -17, Expected);
            Check_LFM (0, 1, Positive_Infinity);
            Check_Mixed (0, 30, 31);
         end Submit_Phase;
      begin
         Source (1) := 77.0;
         Submit_Phase (Phase, [8.5, -9.25, 8.5]);
         Submit_Phase (C2.Phase_Code (-17, Source), [77.0, -9.25, 8.5]);
         C2.Append_Modulation (Retained, Phase, Index);
         C2.Append_Modulation (Retained, C2.Linear_Frequency_Modulation (-0.0), Index);
         C2.Set_Modulations (Original, Retained);
         C2.Append_TX_Event (Original, Event);
         declare
            Saved_Table     : constant C2.Modulation_Table := Retained;
            Source_Interval : C2.RX_Job_Interval_Config := Mixed_Interval (30);
         begin
            C2.Append_Modulation (Retained, C2.Linear_Frequency_Modulation (6.125), Index);
            C2.Set_Modulations (Source_Interval, Retained);
            C2.Append_TX_Event (Source_Interval, Event);
            Submit (Source_Interval);
            Require (Observe (0, 0, 0, 0) = 3, "source table mutation after Set");
            Check_Phase (0, 0, -17, [8.5, -9.25, 8.5]);
            Check_LFM (0, 1, -0.0);
            Check_LFM (0, 2, 6.125);
            Submit (Original);
            Require (Observe (0, 0, 0, 0) = 2, "Set retained independent table");
            Check_Phase (0, 0, -17, [8.5, -9.25, 8.5]);
            Check_LFM (0, 1, -0.0);
            Retained := Saved_Table;
         end;
         C2.Append_Phase_Chip (Phase, -11.5);
         Submit_Phase (Phase, [8.5, -9.25, 8.5, -11.5]);
         C2.Replace_Phase_Chips (Phase, [2.25]);
         Submit_Phase (Phase, [2.25]);
         C2.Replace_Phase_Chips (Phase, [0.0, -0.0, Quiet_NaN, Negative_Infinity, 5.5]);
         Submit_Phase (Phase, [0.0, -0.0, Quiet_NaN, Negative_Infinity, 5.5]);
         C2.Replace_Phase_Chips (Phase, []);
         Submit_Phase (Phase, []);
         Submit (Original);
         Check_Phase (0, 0, -17, [8.5, -9.25, 8.5]);
         Check_LFM (0, 1, -0.0);
         Modified := Retained;
         C2.Append_Modulation (Modified, C2.Linear_Frequency_Modulation (Quiet_NaN), Index);
         C2.Append_Modulation (Modified, C2.Linear_Frequency_Modulation (0.0), Index);
         Changed := Original;
         C2.Set_Modulations (Changed, Modified);
         Submit (Changed, True);
         Require (Observe (0, 0, 0, 0) = 4, "modified source table");
         Check_Phase (0, 0, -17, [8.5, -9.25, 8.5]);
         Check_LFM (0, 1, -0.0);
         Check_LFM (0, 2, Quiet_NaN);
         Check_LFM (0, 3, 0.0);
         Submit (Original);
         Require (Observe (0, 0, 0, 0) = 2, "retained table copy");
         Check_Phase (0, 0, -17, [8.5, -9.25, 8.5]);
         Check_LFM (0, 1, -0.0);
         C2.Append_Modulation_Reference (Event, 1);
         C2.Clear_TX_Events (Changed);
         C2.Append_TX_Event (Changed, Event);
         Submit (Changed);
         Check_References (0, 0, [0, 1]);
         Submit (Original);
         Check_References (0, 0, [0]);
         Check_Phase (0, 0, -17, [8.5, -9.25, 8.5]);
         Check_LFM (0, 1, -0.0);
         --  Every presence mutation is appended into a fresh submitted list.
         for Empty in Boolean loop
            Changed := Original;
            if Empty then
               C2.Set_Empty_Modulation_References (Event);
            else
               C2.Clear_Modulation_References (Event);
            end if;
            C2.Clear_TX_Events (Changed);
            C2.Append_TX_Event (Changed, Event);
            Check_Trace (Changed, 1, (if Empty then 1 else 0));
            Check_References (0, 0, (if Empty then [] else [C2.No_Modulation]));
            Check_Phase (0, 0, -17, [8.5, -9.25, 8.5]);
            Check_LFM (0, 1, -0.0);
            --  Compatible references when removing the table.
            if Empty then
               C2.Set_Empty_Modulations (Changed);
            else
               C2.Clear_Modulations (Changed);
            end if;
            Check_Trace (Changed, (if Empty then 1 else 0), (if Empty then 1 else 0));
            Require (Observe (0, 0, 0, 0) = 0, "clear/empty table");
            Check_References (0, 0, (if Empty then [] else [C2.No_Modulation]));
            --  Start next iteration with genuinely populated references.
            Event := TX_Event (31, [0, 1]);
         end loop;
         C2.Clear_Modulation_References (Event);
         C2.Append_Modulation_Reference (Event, 1);
         Changed := Original;
         C2.Clear_TX_Events (Changed);
         C2.Append_TX_Event (Changed, Event);
         Submit (Changed);
         Check_References (0, 0, [1]);
         --  Final-table shrink must not resolve via another interval/prior Add.
         declare
            Shrunk  : C2.Modulation_Table;
            Invalid : C2.RX_Job_Interval_List;
            Before  : constant Interfaces.C.unsigned := Add_Count.all;
         begin
            C2.Append_Modulation (Shrunk, C2.Tone, Index);
            C2.Set_Modulations (Changed, Shrunk);
            C2.Append_Job_Interval (Invalid, Changed);
            C2.Append_Job_Interval (Invalid, Original);
            begin
               C2.Add_RX_Job_Intervals (Object, Invalid);
               raise Program_Error with "final-table shrink accepted";
            exception
               when Constraint_Error =>
                  null;
            end;
            Require (Add_Count.all = Before, "rejection entered provider Add");
            Check_References (0, 0, [1]);
            Check_Phase (0, 0, -17, [8.5, -9.25, 8.5]);
            Check_LFM (0, 1, -0.0);
            Check_Mixed (0, 30, 31);
            C2.Set_Modulations (Changed, Retained);
            Submit (Changed, True);
            Check_References (0, 0, [1]);
            Check_Phase (0, 0, -17, [8.5, -9.25, 8.5]);
            Check_LFM (0, 1, -0.0);
         end;
         for Kind in 0 .. 1 loop
            declare
               Wrong    : C2.Modulation_Config :=
                 (if Kind = 0 then C2.Tone else C2.Linear_Frequency_Modulation (-6.75));
               Table    : C2.Modulation_Table;
               Interval : C2.RX_Job_Interval_Config := Mixed_Interval (30);
            begin
               begin
                  C2.Append_Phase_Chip (Wrong, 99.0);
                  raise Program_Error with "wrong-kind append accepted";
               exception
                  when Constraint_Error =>
                     null;
               end;
               begin
                  C2.Replace_Phase_Chips (Wrong, [99.0]);
                  raise Program_Error with "wrong-kind replace accepted";
               exception
                  when Constraint_Error =>
                     null;
               end;
               C2.Append_Modulation (Table, Wrong, Index);
               C2.Set_Modulations (Interval, Table);
               C2.Clear_Modulation_References (Event);
               C2.Append_TX_Event (Interval, Event);
               Submit (Interval);
               if Kind = 0 then
                  Check_Kind (0, 0, 0);
               else
                  Check_LFM (0, 0, -6.75);
               end if;
               Check_Mixed (0, 30, 31);
            end;
         end loop;
      end;
      Require (DL_Close (Library) = 0, "mock close");
   end Send_Modulations;
   procedure Send_Empty (Object : in out C2.Job) is
      Absent : constant C2.RX_Job_Interval_Config := C2.Create_RX_Job_Interval (1, 2);
      Empty  : C2.RX_Job_Interval_Config := Absent;
      List   : C2.RX_Job_Interval_List;
   begin
      declare
         TX : C2.TX_Transmit_Event_Config :=
           C2.Create_TX_Transmit_Event (7, "TX-only", -9, 17, 1.25);
      begin
         C2.Append_TX_Event (Empty, TX);
         C2.Append_Job_Interval (List, Empty);
         C2.Add_Job_Intervals (Object, List);
         if TX_Value (0, 0, 0, 0) /= 1 or else TX_Value (0, 0, 1, 0) /= 0 then
            raise Program_Error with "TX-only sequence";
         end if;
         begin
            TX := C2.Create_TX_Transmit_Event (0, "bad" & Character'Val (0), 0, 0, 0.0);
            raise Program_Error with "TX NUL accepted";
         exception
            when Constraint_Error =>
               null;
         end;
      end;
      declare
         Fresh : C2.RX_Job_Interval_List;
      begin
         List := Fresh;
      end;
      C2.Set_Empty_TX_Events (Empty);
      C2.Set_Interval_Product_Stream_Params (Empty, C2.Create_Product_Stream_Params);
      C2.Set_Interval_Endpoint_Connections (Empty, C2.Create_Endpoint_Connections);
      C2.Append_Job_Interval (List, Absent);
      C2.Append_Job_Interval (List, Empty);
      C2.Add_RX_Job_Intervals (Object, List);
      for I in Interfaces.C.unsigned range 0 .. 1 loop
         if Stream_Value (I, 0, 0) /= 0 or else Stream_Value (I, 0, 1) /= 0 then
            raise Program_Error with "legacy absent/explicit empty product stream";
         end if;
      end loop;
   end Send_Empty;
   procedure Send
     (Object : in out C2.Job; Enabled : Boolean; Polarizations : Natural; Clear : Boolean := False)
   is
      Event        : C2.RX_Receive_Event_Config :=
        C2.Create_RX_Receive_Event (1, "rx/µ-main", 0, 1, 1.0, 2.0);
      Interval     : C2.RX_Job_Interval_Config := C2.Create_RX_Job_Interval (1, 2);
      List         : C2.RX_Job_Interval_List;
      Reference    : constant C2.Pulse_Threshold_Reference :=
        C2.Pulse_Threshold_Reference'Val (Polarizations);
      Tag          : constant C2.Pulse_Time_Tag_Threshold :=
        C2.Pulse_Time_Tag_Threshold'Val (Polarizations mod 2);
      Width        : constant Interfaces.Integer_64 :=
        (if Polarizations = 0
         then Interfaces.Integer_64'First
         elsif Polarizations = 1
         then 0
         else Interfaces.Integer_64'Last);
      Settings     : C2.Pulse_Detection_Settings :=
        C2.Create_Pulse_Detection_Settings (Reference, 255, 0, 0, 255, Width, Tag);
      Copy         : C2.Pulse_Detection_Settings;
      Command      : C2.Local_Function_Command :=
        C2.Create_Local_Function_Command
          (16#8000_0001#, Interfaces.Unsigned_64 (Interfaces.C.size_t'Last));
      Command_Copy : C2.Local_Function_Command;
   begin
      declare
         TX    : C2.TX_Transmit_Event_Config :=
           C2.Create_TX_Transmit_Event
             (Interfaces.Unsigned_32'Last,
              "tx/β",
              Interfaces.Integer_64'First,
              Interfaces.Integer_64'Last,
              -0.0,
              -12.5,
              Interfaces.Integer_64'Last,
              Interfaces.Integer_64'First);
         Saved : C2.TX_Transmit_Event_Config;
      begin
         C2.Set_Event_Stabilization_Point_Index
           (TX, Interfaces.Unsigned_64 (Interfaces.C.size_t'Last));
         C2.Append_Applicable_TX_Element_Group (TX, 0);
         C2.Append_Applicable_TX_Element_Group
           (TX, Interfaces.Unsigned_64 (Interfaces.C.size_t'Last));
         C2.Append_Polarization (TX, 1.0, -2.0, 3.0, 4.0);
         C2.Append_Polarization (TX, 5.0, 6.0, -7.0, 8.0);
         begin
            C2.Append_Polarization (TX, 0.0, 0.0, 0.0, 0.0);
            raise Program_Error with "third TX polarization accepted";
         exception
            when Constraint_Error =>
               null;
         end;
         C2.Set_Polarization_Beam_Steer_Correction (TX, True);
         C2.Set_Phase_Offset_Radians (TX, -2.5);
         C2.Set_Event_Execution_Type (TX, C2.Conditional);
         C2.Set_Event_Termination_Type (TX, C2.Cancel_Event);
         C2.Set_Allow_Delay_Start (TX, True);
         C2.Set_Iteration_Hold_Count (TX, Interfaces.Unsigned_64 (Interfaces.C.size_t'Last));
         C2.Set_Iteration_Termination_Count (TX, 1);
         C2.Set_Channelization_Enabled (TX, True);
         Saved := TX;
         C2.Set_Phase_Offset_Radians (TX, 99.0);
         C2.Set_Event_Execution_Type (TX, C2.Normal);
         C2.Set_TX_Attenuation_DB (TX, 9.0);
         C2.Set_TX_Edge_Durations (TX, 1, 2);
         C2.Append_Applicable_TX_Element_Group (TX, 42);
         C2.Append_TX_Event (Interval, Saved);
         C2.Append_TX_Event (Interval, Saved);
         C2.Clear_TX_Events (Interval);
         C2.Set_Empty_TX_Events (Interval);
         if not Clear then
            C2.Append_TX_Event (Interval, Saved);
            C2.Append_TX_Event (Interval, Saved);
         end if;
      end;
      C2.Append_Polarization (Event, 1.0, -0.0, 2.25, -3.5);
      C2.Set_Event_Execution_Type (Event, C2.Conditional);
      C2.Set_Event_Termination_Type (Event, C2.Cancel_Event);
      C2.Set_Stab_Point_Index (Event, 999);
      C2.Append_Applicable_RX_Element_Group (Event, 2);
      C2.Append_Stab_Point (Interval, C2.Create_Face_Relative_Pointing (1.5, -0.5));
      C2.Append_Pulse_Detection_Threshold (Settings, 12.25, -3.5);
      C2.Append_Pulse_Detection_Threshold
        (Settings, IEEE (16#8000_0000_0000_0000#), IEEE (16#7FF0_0000_0000_0000#));
      C2.Append_Pulse_Detection_Threshold
        (Settings, IEEE (16#7FF8_0000_0000_0001#), IEEE (16#FFF0_0000_0000_0000#));
      C2.Append_Pulse_Detection_Threshold (Settings, 12.25, -3.5);
      Copy := Settings;
      C2.Append_Pulse_Detection_Threshold (Settings, 99.0, 88.0);
      C2.Set_Pulse_Detection_Settings (Event, Settings);
      C2.Set_Pulse_Detection_Settings (Event, Copy);
      C2.Clear_Pulse_Detection_Settings (Event);
      if not Clear then
         C2.Set_Pulse_Detection_Settings (Event, Copy);
      end if;
      if Enabled then
         C2.Set_Interval_Status_Enable (Interval, C2.Always);
      end if;
      C2.Append_RX_Event (Interval, Event);
      C2.Append_RX_Event (Interval, Event);
      if not Clear then
         C2.Append_Local_Function_Write (Command, 0, 0);
         C2.Append_Local_Function_Write
           (Command, Interfaces.Unsigned_64'Last, Interfaces.Unsigned_64'Last);
         Command_Copy := Command;
         C2.Append_Local_Function_Write (Command_Copy, 0, 7);
         C2.Append_Local_Function_Write (Command_Copy, 16#8000_0000_0000_0000#, 1);
         C2.Append_Interval_Local_Function_Command (Interval, Command);
         C2.Append_Interval_Local_Function_Command (Interval, Command);
         C2.Append_Interval_Local_Function_Command (Interval, Command_Copy);
         C2.Append_Local_Function_Write (Command, 42, 99);
         C2.Append_Interval_Local_Function_Command
           (Interval, C2.Create_Local_Function_Command (Interfaces.Unsigned_32'Last, 0));
         C2.Append_Interval_Local_Function_Command
           (Interval, C2.Create_Local_Function_Command (0, 1));
         C2.Append_Interval_Local_Function_Command
           (Interval, C2.Create_Local_Function_Command (1, 0));
         C2.Append_Interval_Local_Function_Command
           (Interval, C2.Create_Local_Function_Command (16#8000_0000#, 0));
      end if;
      declare
         Params : C2.Product_Stream_Params := C2.Create_Product_Stream_Params;
         Saved  : C2.Product_Stream_Params;
      begin
         C2.Append_Product_Stream_RX_Group (Params, 2);
         C2.Append_Product_Stream_RX_Group (Params, 0);
         C2.Append_Product_Stream_RX_Group (Params, 2);
         C2.Append_Product_Stream_Endpoint (Params, 0, 0, 0);
         C2.Append_Product_Stream_Endpoint
           (Params,
            Interfaces.Unsigned_64'Last,
            16#8000_0000_0000_0000#,
            Interfaces.Unsigned_64'Last);
         C2.Append_Product_Stream_Endpoint
           (Params,
            Interfaces.Unsigned_64'Last,
            16#8000_0000_0000_0000#,
            Interfaces.Unsigned_64'Last);
         C2.Append_Product_Stream_Endpoint (Params, 16#0123_4567_89AB_CDEF#, 1, 0);
         Saved := Params;
         C2.Append_Product_Stream_RX_Group (Params, 99);
         C2.Append_Product_Stream_Endpoint (Params, 99, 99, 99);
         C2.Set_Interval_Product_Stream_Params (Interval, Params);
         C2.Set_Interval_Product_Stream_Params (Interval, Saved);
         C2.Clear_Interval_Product_Stream_Params (Interval);
         if not Clear then
            C2.Set_Interval_Product_Stream_Params (Interval, Saved);
         end if;
         C2.Append_Product_Stream_RX_Group (Saved, 99);
         C2.Append_Product_Stream_Endpoint (Saved, 99, 99, 99);
      end;
      declare
         Connections : C2.Endpoint_Connections := C2.Create_Endpoint_Connections;
         Saved       : C2.Endpoint_Connections;
      begin
         begin
            C2.Append_Endpoint_Connection (Connections, "bad" & Character'Val (0), "IQ", 1);
            raise Program_Error with "embedded NUL accepted";
         exception
            when Constraint_Error =>
               null;
         end;
         C2.Append_Endpoint_Connection (Connections, "rx/β ", "IQ", 1);
         C2.Append_Endpoint_Connection (Connections, "rx/β ", "PDW", 0);
         C2.Append_Endpoint_Connection (Connections, "rx/β ", "IQ", Interfaces.Unsigned_64'Last);
         C2.Append_Endpoint_Connection (Connections, "", "", 16#8000_0000_0000_0000#);
         C2.Append_Endpoint_Connection (Connections, "", "default", 1);
         Saved := Connections;
         C2.Append_Endpoint_Connection (Connections, "extra", "IQ", 99);
         C2.Set_Interval_Endpoint_Connections (Interval, Connections);
         C2.Set_Interval_Endpoint_Connections (Interval, Saved);
         C2.Clear_Interval_Endpoint_Connections (Interval);
         if not Clear then
            C2.Set_Interval_Endpoint_Connections (Interval, Saved);
         end if;
         C2.Append_Endpoint_Connection (Saved, "rx/β ", "IQ", 0);
      end;
      C2.Append_Job_Interval (List, Interval);
      C2.Append_Job_Interval (List, Interval);
      C2.Add_RX_Job_Intervals (Object, List);
      if not Clear then
         if TX_Control (0, 0, 0) /= 2
           or else TX_Control (0, 0, 1) /= 1
           or else TX_Control (0, 0, 2) /= 1
           or else TX_Control (0, 0, 3) /= 1
           or else TX_Control (0, 0, 4) /= 1
           or else TX_Control (0, 0, 5) /= Interfaces.Unsigned_64 (Interfaces.C.size_t'Last)
           or else TX_Control (0, 0, 6) /= 1
           or else TX_Control (0, 0, 7) /= 1
         then
            raise Program_Error with "TX common controls fidelity";
         end if;
      end if;

      if not Clear then
         if TX_Component (0, 0, 0, 4) /= -2.5
           or else TX_Component (0, 0, 0, 0) /= 1.0
           or else TX_Component (0, 0, 0, 1) /= -2.0
           or else TX_Component (0, 0, 1, 2) /= -7.0
           or else TX_Component (0, 0, 1, 3) /= 8.0
         then
            raise Program_Error with "owned TX Stokes and phase copy";
         end if;
      end if;
      for I in Interfaces.C.unsigned range 0 .. 1 loop
         if TX_Value (I, 0, 0, 0) /= (if Clear then 0 else 2) or else TX_Value (I, 0, 1, 0) /= 2
         then
            raise Program_Error with "independent RX/TX vectors";
         end if;
         if not Clear then
            for E in Interfaces.C.unsigned range 0 .. 1 loop
               if TX_Value (I, E, 2, 0) /= Interfaces.Unsigned_64 (Interfaces.Unsigned_32'Last)
                 or else TX_Value (I, E, 3, 0) /= Interfaces.Unsigned_64 (Interfaces.C.size_t'Last)
                 or else TX_Value (I, E, 4, 0) /= 2
                 or else TX_Value (I, E, 5, 1) /= Interfaces.Unsigned_64 (Interfaces.C.size_t'Last)
                 or else TX_Value (I, E, 6, 0) /= 16#8000_0000_0000_0000#
                 or else TX_Value (I, E, 7, 0) /= 16#7FFF_FFFF_FFFF_FFFF#
                 or else TX_Value (I, E, 8, 0) /= 16#7FFF_FFFF_FFFF_FFFF#
                 or else TX_Value (I, E, 9, 0) /= 16#8000_0000_0000_0000#
                 or else Bits (TX_Number (I, E, 0)) /= 16#8000_0000_0000_0000#
                 or else TX_Number (I, E, 1) /= -12.5
                 or else TX_Value (I, E, 10, 0) /= 1
                 or else TX_Value (I, E, 11, 0) /= 1
               then
                  raise Program_Error with "TX copies, indices, IDs and defaults";
               end if;
            end loop;
         end if;
      end loop;
      for I in Interfaces.C.unsigned range 0 .. 1 loop
         if Stream_Value (I, 0, 0) /= (if Clear then 0 else 3)
           or else Stream_Value (I, 0, 1) /= (if Clear then 0 else 4)
           or else Stream_Value (I, 0, 6) /= (if Clear then 0 else 2)
           or else Stream_Value (I, 0, 7) /= (if Clear then 0 else 2)
         then
            raise Program_Error with "product stream counts/defaults";
         end if;
         if not Clear then
            if Map_Value (I, "rx/β ", "IQ", 3) /= Interfaces.Unsigned_64'Last
              or else Map_Value (I, "rx/β ", "PDW", 2) /= 1
              or else Map_Value (I, "rx/β ", "PDW", 3) /= 0
              or else Map_Value (I, "", "", 3) /= 16#8000_0000_0000_0000#
              or else Map_Value (I, "", "default", 3) /= 1
            then
               raise Program_Error with "endpoint map copy/last-wins/label fidelity";
            end if;
            --  The pinned applicable getter incorrectly aliases the endpoint map;
            --  these counts do not prove applicableElementGroupLabels were set.
            if Stream_Value (I, 0, 2) /= 2
              or else Stream_Value (I, 1, 2) /= 0
              or else Stream_Value (I, 2, 2) /= 2
              or else Stream_Value (I, 0, 3) /= 0
              or else Stream_Value (I, 0, 4) /= 0
              or else Stream_Value (I, 0, 5) /= 0
              or else Stream_Value (I, 1, 3) /= Interfaces.Unsigned_64'Last
              or else Stream_Value (I, 2, 3) /= Interfaces.Unsigned_64'Last
              or else Stream_Value (I, 1, 4) /= 16#8000_0000_0000_0000#
              or else Stream_Value (I, 2, 4) /= 16#8000_0000_0000_0000#
              or else Stream_Value (I, 1, 5) /= Interfaces.Unsigned_64'Last
              or else Stream_Value (I, 2, 5) /= Interfaces.Unsigned_64'Last
              or else Stream_Value (I, 3, 3) /= 16#0123_4567_89AB_CDEF#
              or else Stream_Value (I, 3, 4) /= 1
              or else Stream_Value (I, 3, 5) /= 0
            then
               raise Program_Error with "product stream copy/order/scalar fidelity";
            end if;
         end if;
         if Clear then
            if LF_Value (I, 0, 0, 0) /= 0 then
               raise Program_Error;
            end if;
         else
            if LF_Value (I, 0, 0, 0) /= 7
              or else LF_Value (I, 0, 0, 1) /= 16#8000_0001#
              or else LF_Value (I, 0, 0, 2) /= Interfaces.Unsigned_64 (Interfaces.C.size_t'Last)
              or else LF_Value (I, 0, 0, 3) /= 2
              or else LF_Value (I, 1, 0, 3) /= 2
              or else LF_Value (I, 2, 0, 3) /= 4
              or else LF_Value (I, 3, 0, 3) /= 0
              or else LF_Value (I, 0, 0, 4) /= 0
              or else LF_Value (I, 0, 0, 5) /= 0
              or else LF_Value (I, 0, 1, 4) /= Interfaces.Unsigned_64'Last
              or else LF_Value (I, 0, 1, 5) /= Interfaces.Unsigned_64'Last
              or else LF_Value (I, 2, 2, 4) /= 0
              or else LF_Value (I, 2, 2, 5) /= 7
              or else LF_Value (I, 2, 3, 4) /= 16#8000_0000_0000_0000#
              or else LF_Value (I, 2, 3, 5) /= 1
            then
               raise Program_Error with "LF value/copy fidelity";
            end if;
         end if;
      end loop;
      if Clear then
         Observe (0, 0, 0, 0);
      else
         Observe (Polarizations, Polarizations mod 2, Width, 4);
      end if;
   end Send;

begin
   for Repeat in 1 .. 50 loop
      declare
         Parent     : C2.C2_MEL := C2.Open (Path, "c2:job-ok");
         VA_Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, VA_Config);
      begin
         if C2.Outcome (C2.Wait (VA_Request, 3_000)) /= C2.Created then
            raise Program_Error;
         end if;
         declare
            VA      : C2.Virtual_Aperture := C2.Claim (VA_Request);
            Request : C2.Job_Request := C2.Submit_Job (VA, Job_Config);
         begin
            if C2.Outcome (C2.Wait (Request, 3_000)) /= C2.Created then
               raise Program_Error;
            end if;
            declare
               Object : C2.Job := C2.Claim (Request);
            begin
               Send_Distinct (Object);
               Send_Modulations (Object);
               Send_Binary_Modulations (Object);
               Send_Custom_Waveforms (Object);
               Send_Waveform_Buffers (Object);
               Send_Empty (Object);
               Ada.Environment_Variables.Set
                 ("AMS_MEL_TEST_RF_JOB_FAILURE", "interval-tx-allocation");
               begin
                  Send (Object, False, 0);
                  raise Program_Error with "preparation failure accepted";
               exception
                  when AMS.MEL.Provider_Error =>
                     null;
               end;
               Ada.Environment_Variables.Clear ("AMS_MEL_TEST_RF_JOB_FAILURE");
               Send (Object, False, 0, True);
               Send (Object, False, 0);
               Send (Object, False, 1);
               Send (Object, False, 2);
               begin
                  Send (Object, True, 2);
                  raise Program_Error with "status gate accepted";
               exception
                  when AMS.MEL.Provider_Error =>
                     null;
               end;
               declare
                  Stream : Status.Stream := Status.Open (Object, 2, 1, 0);
               begin
                  Send (Object, True, 2);
                  for Failure of Failures loop
                     Ada.Environment_Variables.Set
                       ("AMS_MEL_TEST_F4_ADD_FAILURE",
                        Ada.Strings.Fixed.Trim (Failure, Ada.Strings.Both));
                     begin
                        Send (Object, True, 2);
                        raise Program_Error with "provider failure accepted";
                     exception
                        when AMS.MEL.Provider_Error =>
                           null;
                     end;
                     Ada.Environment_Variables.Clear ("AMS_MEL_TEST_F4_ADD_FAILURE");
                     Send (Object, True, 2);
                  end loop;
                  Status.Close (Stream);
               end;
               C2.Close (Parent);
               C2.Close (VA);
               Send (Object, False, 2);
               C2.Close (Object);
            end;
            C2.Close (Request);
            C2.Close (VA);
         end;
         C2.Close (VA_Request);
         C2.Close (Parent);
      end;
   end loop;
   Ada.Text_IO.Put_Line ("PASS: safe Ada TX events focused 50/50");
end AMS_MEL_RF_TX_Events_Focused;
