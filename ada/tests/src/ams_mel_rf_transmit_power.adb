with Ada.Environment_Variables;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.RF.C2;
with AMS.MEL.RF.C2.Transmit_Power;
with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;
with GNAT.Source_Info;

procedure AMS_MEL_RF_Transmit_Power is
   package C2 renames AMS.MEL.RF.C2;
   package T renames C2.Transmit_Power;
   package CS renames Interfaces.C.Strings;
   use type Interfaces.Unsigned_64;
   use type Interfaces.C.unsigned;
   use type Interfaces.C.int;
   use type C2.Request_Outcome;
   use type System.Address;
   function DL_Open (Path : CS.chars_ptr; Flags : Interfaces.C.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function DL_Sym (Handle : System.Address; Name : CS.chars_ptr) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   function DL_Close (Handle : System.Address) return Interfaces.C.int
   with Import, Convention => C, External_Name => "dlclose";
   function Process_ID return Interfaces.C.int
   with Import, Convention => C, External_Name => "getpid";
   type Count_Access is
     access function (Method : Interfaces.C.unsigned) return Interfaces.C.unsigned
   with Convention => C;
   type Scalar_Count_Access is access function return Interfaces.C.unsigned with Convention => C;
   type ID_Access is
     access function (Method, Field : Interfaces.C.unsigned) return Interfaces.Unsigned_64
   with Convention => C;
   type Number_Access is
     access function (Method, Field : Interfaces.C.unsigned) return Interfaces.C.double
   with Convention => C;
   type Change_Access is access procedure (Generation : Interfaces.C.unsigned) with Convention => C;
   type Max_Access is access function return Interfaces.Unsigned_64 with Convention => C;
   function To_Count is new Ada.Unchecked_Conversion (System.Address, Count_Access);
   function To_Scalar_Count is new Ada.Unchecked_Conversion (System.Address, Scalar_Count_Access);
   function To_ID is new Ada.Unchecked_Conversion (System.Address, ID_Access);
   function To_Number is new Ada.Unchecked_Conversion (System.Address, Number_Access);
   function To_Change is new Ada.Unchecked_Conversion (System.Address, Change_Access);
   function To_Max is new Ada.Unchecked_Conversion (System.Address, Max_Access);
   Provider  : constant String :=
     Ada.Environment_Variables.Value ("AMS_MEL_TEST_PROVIDER_DIR") & "/libmock_rf_provider.so";
   Path      : CS.chars_ptr := CS.New_String (Provider);
   Library   : constant System.Address := DL_Open (Path, 2);
   function Symbol (Name : String) return System.Address is
      Text  : CS.chars_ptr := CS.New_String (Name);
      Value : constant System.Address := DL_Sym (Library, Text);
   begin
      CS.Free (Text);
      if Value = System.Null_Address then
         raise Program_Error with "missing mock symbol " & Name;
      end if;
      return Value;
   end Symbol;
   Calls     : constant Count_Access := To_Count (Symbol ("mock_rf_tx_query_calls"));
   ID        : constant ID_Access := To_ID (Symbol ("mock_rf_tx_query_id"));
   Number    : constant Number_Access := To_Number (Symbol ("mock_rf_tx_query_double"));
   Change    : constant Change_Access := To_Change (Symbol ("mock_rf_tx_query_change"));
   Size_Max  : constant Interfaces.Unsigned_64 := To_Max (Symbol ("mock_rf_tx_size_max")).all;
   E1        : constant Count_Access := To_Count (Symbol ("mock_rf_va_query_calls"));
   E3        : constant Count_Access := To_Count (Symbol ("mock_rf_element_calls"));
   E4        : constant Count_Access := To_Count (Symbol ("mock_rf_va_lf_calls"));
   E5        : constant Count_Access := To_Count (Symbol ("mock_rf_connection_calls"));
   Getters   : constant Scalar_Count_Access := To_Scalar_Count (Symbol ("mock_rf_getter_calls"));
   Modes     : constant Scalar_Count_Access :=
     To_Scalar_Count (Symbol ("mock_rf_tx_collection_calls"));
   Direct    : constant Scalar_Count_Access := To_Scalar_Count (Symbol ("mock_rf_tx_direct_calls"));
   Forbidden : constant Scalar_Count_Access := To_Scalar_Count (Symbol ("mock_rf_forbidden_calls"));
   Lifetime  : constant String := "/tmp/ams-rf-tx-ada-" & Interfaces.C.int'Image (Process_ID);
   A         : constant array (0 .. 3) of Long_Float := [47.125, -3.5, 17.75, 63.25];
   B         : constant array (0 .. 3) of Long_Float := [-28.625, 91.5, -6.25, 12.875];
   Group     : Interfaces.Unsigned_64 := Size_Max;
   Weight    : Interfaces.Unsigned_64 :=
     (if Size_Max = Interfaces.Unsigned_64'Last then 16#8000_0000_0000_0000# else Size_Max);
   procedure Verify (Condition : Boolean; Location : String := GNAT.Source_Info.Source_Location) is
   begin
      if not Condition then
         raise Program_Error with "TX assertion at " & Location;
      end if;
   end Verify;
   function Config return C2.Virtual_Aperture_Config is
      V       : C2.Virtual_Aperture_Config :=
        C2.Create_Virtual_Aperture_Config (16#FEDC_BA98#, 16#8000_0001#, "definition/β.json");
      X, Y, Z : AMS.MEL.IR.UUID := [others => 0];
   begin
      C2.Append_Local_Function_Info (V, "alpha");
      C2.Append_Local_Function_Info (V, "µ-local");
      C2.Append_Local_Function_Info (V, "");
      X (1) := 16#80#;
      X (2) := 16#FF#;
      Y (0) := 16#FF#;
      Z (15) := 16#80#;
      C2.Append_Capability_ID (V, AMS.MEL.IR.Create_UCI_ID (X, "first"));
      C2.Append_Capability_ID (V, AMS.MEL.IR.Create_UCI_ID (Y, ""));
      C2.Append_Capability_ID (V, AMS.MEL.IR.Create_UCI_ID (Z, "µ-third"));
      return V;
   end Config;
   function Invoke (VA : C2.Virtual_Aperture'Class; M : Natural) return Long_Float is
      pragma Validity_Checks ("F");
   begin
      case M is
         when 0      =>
            return
              T.Radiated_Power_DBW
                (VA,
                 Group,
                 16#FEDC_BA98#,
                 -12.75,
                 Weight,
                 987654321.125,
                 (-0.75, 0.625),
                 16#DEAD_BEEF#);

         when 1      =>
            return
              T.Peak_Radiated_Power_DBW
                (VA, Group, 16#FEDC_BA98#, -12.75, 987654321.125, 16#DEAD_BEEF#);

         when 2      =>
            return
              T.Aperture_Gain_DB
                (VA, Group, 16#FEDC_BA98#, Weight, 987654321.125, (-0.75, 0.625), 16#DEAD_BEEF#);

         when others =>
            return T.Max_Attenuation_DB (VA, Group, 16#FEDC_BA98#, 16#DEAD_BEEF#);
      end case;
   end Invoke;
   procedure Success (VA : C2.Virtual_Aperture'Class; M : Natural; Expected : Long_Float) is
      Before : array (0 .. 3) of Interfaces.C.unsigned;
      K      : constant Interfaces.C.unsigned := Interfaces.C.unsigned (M);
   begin
      for J in Before'Range loop
         Before (J) := Calls (Interfaces.C.unsigned (J));
      end loop;
      Verify (Invoke (VA, M) = Expected);
      for J in Before'Range loop
         Verify (Calls (Interfaces.C.unsigned (J)) = Before (J) + (if J = M then 1 else 0));
      end loop;
      Verify (ID (K, 0) = Group and ID (K, 2) = 16#FEDC_BA98# and ID (K, 3) = 16#DEAD_BEEF#);
      if M = 0 or M = 2 then
         Verify
           (ID (K, 1) = Weight
            and Long_Float (Number (K, 2)) = -0.75
            and Long_Float (Number (K, 3)) = 0.625);
      end if;
      if M < 2 then
         Verify (Long_Float (Number (K, 0)) = -12.75);
      end if;
      if M < 3 then
         Verify (Long_Float (Number (K, 1)) = 987654321.125);
      end if;
   end Success;
   procedure Special (VA : C2.Virtual_Aperture'Class; M, Kind : Natural) is
      pragma Validity_Checks ("F");
      Value : constant Long_Float := Invoke (VA, M);
   begin
      case Kind is
         when 0      =>
            Verify (Value = 0.0 and Long_Float'Copy_Sign (1.0, Value) = 1.0);

         when 1      =>
            Verify (Value = 0.0 and Long_Float'Copy_Sign (1.0, Value) = -1.0);

         when 2      =>
            Verify (Value > Long_Float'Last and Value > 0.0);

         when 3      =>
            Verify (Value < Long_Float'First and Value < 0.0);

         when others =>
            Verify (Value /= Value);
      end case;
   end Special;
   procedure Failure (VA : C2.Virtual_Aperture'Class; M : Natural) is
      Value : Long_Float;
   begin
      Value := Invoke (VA, M);
      raise Program_Error with "TX failure missing " & Long_Float'Image (Value);
   exception
      when AMS.MEL.Provider_Error =>
         null;
   end Failure;
   procedure Special_Inputs (VA : C2.Virtual_Aperture'Class) is
      pragma Validity_Checks ("F");
      function Decode is new Ada.Unchecked_Conversion (Interfaces.Unsigned_64, Long_Float);
      pragma Compile_Time_Error (Long_Float'Size /= 64, "IEEE fixture needs binary64 Long_Float");
      Z           : constant Long_Float := Decode (16#8000_0000_0000_0000#);
      Inf         : constant Long_Float := Decode (16#7FF0_0000_0000_0000#);
      NaN         : constant Long_Float := Decode (16#7FF8_0000_0000_0000#);
      Result      : constant Long_Float :=
        T.Radiated_Power_DBW (VA, Group, 16#FEDC_BA98#, Z, Weight, -Inf, (NaN, Inf), 16#DEAD_BEEF#);
      Attenuation : constant Long_Float := Long_Float (Number (0, 0));
      Frequency   : constant Long_Float := Long_Float (Number (0, 1));
      U           : constant Long_Float := Long_Float (Number (0, 2));
      V           : constant Long_Float := Long_Float (Number (0, 3));
   begin
      Verify (Result = A (0));
      Verify (Attenuation = 0.0 and Long_Float'Copy_Sign (1.0, Attenuation) = -1.0);
      Verify (Frequency < Long_Float'First and U /= U and V > Long_Float'Last);
   end Special_Inputs;
begin
   CS.Free (Path);
   Verify (Library /= System.Null_Address);
   Ada.Environment_Variables.Set ("AMS_MEL_TEST_LIFETIME_LOG", Lifetime);
   declare
      F : Ada.Text_IO.File_Type;
   begin
      Ada.Text_IO.Create (F, Ada.Text_IO.Out_File, Lifetime);
      Ada.Text_IO.Close (F);
   end;
   declare
      Parent  : C2.C2_MEL := C2.Open (Provider, "c2:va-ok");
      Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, Config);
   begin
      Verify (C2.Outcome (C2.Wait (Request, 3_000)) = C2.Created);
      declare
         VA     : C2.Virtual_Aperture := C2.Claim (Request);
         Copies : array (0 .. 3) of Long_Float;
      begin
         C2.Close (Request);
         for M in 0 .. 3 loop
            Verify (Calls (Interfaces.C.unsigned (M)) = 0);
            Success (VA, M, A (M));
            Copies (M) := Invoke (VA, M);
         end loop;
         Change (1);
         for M in 0 .. 3 loop
            Success (VA, M, B (M));
            Verify (Copies (M) = A (M));
         end loop;
         Change (0);
         for M in 0 .. 3 loop
            Group := 0;
            Weight := 0;
            Success (VA, M, A (M));
            Group := Size_Max;
            Weight := Size_Max;
            Success (VA, M, A (M));
            if Size_Max < Interfaces.Unsigned_64'Last then
               declare
                  Before : constant Interfaces.C.unsigned := Calls (Interfaces.C.unsigned (M));
               begin
                  Group := Size_Max + 1;
                  Failure (VA, M);
                  Verify (Calls (Interfaces.C.unsigned (M)) = Before);
                  Group := Size_Max;
                  if M = 0 or M = 2 then
                     Weight := Size_Max + 1;
                     Failure (VA, M);
                     Verify (Calls (Interfaces.C.unsigned (M)) = Before);
                     Weight := Size_Max;
                  end if;
               end;
            end if;
            for K in 0 .. 2 loop
               Ada.Environment_Variables.Set
                 ("AMS_MEL_TEST_TX_EXCEPTION",
                  (case K is
                     when 0      => "standard",
                     when 1      => "unknown",
                     when others => "allocation"));
               declare
                  Before : constant Interfaces.C.unsigned := Calls (Interfaces.C.unsigned (M));
               begin
                  Failure (VA, M);
                  Verify (Calls (Interfaces.C.unsigned (M)) = Before + 1);
               end;
               Ada.Environment_Variables.Clear ("AMS_MEL_TEST_TX_EXCEPTION");
               Success (VA, M, A (M));
            end loop;
            for K in 0 .. 4 loop
               Ada.Environment_Variables.Set
                 ("AMS_MEL_TEST_TX_RESULT",
                  (case K is
                     when 0      => "positive-zero",
                     when 1      => "negative-zero",
                     when 2      => "positive-infinity",
                     when 3      => "negative-infinity",
                     when others => "nan"));
               declare
                  Before : constant Interfaces.C.unsigned := Calls (Interfaces.C.unsigned (M));
               begin
                  Special (VA, M, K);
                  Verify (Calls (Interfaces.C.unsigned (M)) = Before + 1);
               end;
               Ada.Environment_Variables.Clear ("AMS_MEL_TEST_TX_RESULT");
            end loop;
         end loop;
         --  Default instance is genuinely zero, not the primary explicit ID.
         Verify (T.Max_Attenuation_DB (VA, 0, 0) = A (3));
         Verify (ID (3, 3) = 0);
         Special_Inputs (VA);
         for M in 0 .. 5 loop
            Verify (E1 (Interfaces.C.unsigned (M)) = 0);
         end loop;
         for M in 0 .. 9 loop
            Verify (E3 (Interfaces.C.unsigned (M)) = 0);
         end loop;
         for M in 0 .. 3 loop
            Verify (E4 (Interfaces.C.unsigned (M)) = 0);
         end loop;
         for M in 0 .. 4 loop
            Verify (E5 (Interfaces.C.unsigned (M)) = 0);
         end loop;
         Verify (Getters.all = 0 and Modes.all = 0 and Direct.all = 0 and Forbidden.all = 0);
         Verify (DL_Close (Library) = 0);
         C2.Close (Parent);
         for M in 0 .. 3 loop
            Success (VA, M, A (M));
         end loop;
         C2.Close (VA);
         for M in 0 .. 3 loop
            Failure (VA, M);
            Verify (Copies (M) = A (M));
         end loop;
         declare
            F     : Ada.Text_IO.File_Type;
            Stage : Natural := 0;
         begin
            Ada.Text_IO.Open (F, Ada.Text_IO.In_File, Lifetime);
            while not Ada.Text_IO.End_Of_File (F) loop
               declare
                  Line : constant String := Ada.Text_IO.Get_Line (F);
               begin
                  if Line = "rf_va_destroyed" then
                     Verify (Stage = 0);
                     Stage := 1;
                  elsif Line = "rf_c2_shutdown" then
                     Verify (Stage = 1);
                     Stage := 2;
                  elsif Line = "rf_c2_destroyed" then
                     Verify (Stage = 2);
                     Stage := 3;
                  elsif Line = "library_unloaded" then
                     Verify (Stage = 3);
                     Stage := 4;
                  elsif Line = "rf_forbidden_call" then
                     raise Program_Error with "hidden provider call";
                  end if;
               end;
            end loop;
            Verify (Stage = 4);
            Ada.Text_IO.Delete (F);
         end;
      end;
   end;
   Ada.Text_IO.Put_Line
     ("PASS: safe Ada TX power exact inputs/results, IEEE outputs and actual DSO unload");
end AMS_MEL_RF_Transmit_Power;
