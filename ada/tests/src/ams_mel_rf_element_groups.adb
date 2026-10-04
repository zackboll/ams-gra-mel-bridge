with Ada.Environment_Variables;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.RF.C2;
with AMS.MEL.RF.C2.Element_Groups;
with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;
with GNAT.Source_Info;

procedure AMS_MEL_RF_Element_Groups is
   package C2 renames AMS.MEL.RF.C2;
   package E renames C2.Element_Groups;
   package Env renames Ada.Environment_Variables;
   package CS renames Interfaces.C.Strings;
   use type Interfaces.Unsigned_64;
   use type Interfaces.C.unsigned;
   use type Interfaces.C.int;
   use type C2.Request_Outcome;
   use type E.Element_Group_Mode;
   use type System.Address;
   function DL_Open (Path : CS.chars_ptr; Flags : Interfaces.C.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function DL_Sym (Handle : System.Address; Name : CS.chars_ptr) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   function DL_Close (Handle : System.Address) return Interfaces.C.int
   with Import, Convention => C, External_Name => "dlclose";
   type Counter is access function (Method : Interfaces.C.unsigned) return Interfaces.C.unsigned
   with Convention => C;
   type Change_Access is access procedure (Generation : Interfaces.C.unsigned) with Convention => C;
   function To_Counter is new Ada.Unchecked_Conversion (System.Address, Counter);
   function To_Change is new Ada.Unchecked_Conversion (System.Address, Change_Access);
   Provider : constant String :=
     Env.Value ("AMS_MEL_TEST_PROVIDER_DIR") & "/libmock_rf_provider.so";
   Path     : CS.chars_ptr := CS.New_String (Provider);
   Library  : constant System.Address := DL_Open (Path, 2);
   function Symbol (Name : String) return System.Address is
      Text   : CS.chars_ptr := CS.New_String (Name);
      Result : constant System.Address := DL_Sym (Library, Text);
   begin
      CS.Free (Text);
      return Result;
   end Symbol;
   Calls    : constant Counter := To_Counter (Symbol ("mock_rf_element_calls"));
   Live     : constant Counter := To_Counter (Symbol ("mock_rf_element_live"));
   Change   : constant Change_Access := To_Change (Symbol ("mock_rf_element_change"));
   procedure Verify (Condition : Boolean; Location : String := GNAT.Source_Info.Source_Location) is
   begin
      if not Condition then
         raise Program_Error with "element-group assertion at " & Location;
      end if;
   end Verify;
   function Config return C2.Virtual_Aperture_Config is
      Value                : C2.Virtual_Aperture_Config :=
        C2.Create_Virtual_Aperture_Config (16#FEDC_BA98#, 16#8000_0001#, "definition/β.json");
      First, Second, Third : AMS.MEL.IR.UUID := [others => 0];
   begin
      C2.Append_Local_Function_Info (Value, "alpha");
      C2.Append_Local_Function_Info (Value, "µ-local");
      C2.Append_Local_Function_Info (Value, "");
      First (1) := 16#80#;
      First (2) := 16#FF#;
      Second (0) := 16#FF#;
      Third (15) := 16#80#;
      C2.Append_Capability_ID (Value, AMS.MEL.IR.Create_UCI_ID (First, "first"));
      C2.Append_Capability_ID (Value, AMS.MEL.IR.Create_UCI_ID (Second, ""));
      C2.Append_Capability_ID (Value, AMS.MEL.IR.Create_UCI_ID (Third, "µ-third"));
      return Value;
   end Config;
   procedure Check (Value : E.Element_Group_List; Included : Boolean; Later : Boolean := False) is
   begin
      Verify (E.Count (Value) = 3 and E.Data_Pipes_Included (Value) = Included);
      for I in 1 .. 3 loop
         declare
            G : constant E.Element_Group_Descriptor := E.Descriptor_At (Value, I);
         begin
            Verify (E.Lookup_Label (G) = (if I = 1 then "" elsif I = 2 then "z-map" else "µ-map"));
            Verify
              (E.Label (G)
               = (if Later
                  then "descriptor-later-β"
                  elsif I = 1
                  then "returned-empty-key"
                  elsif I = 2
                  then "returned-z"
                  else "returned-µ"));
            Verify (E.Mode (G) = (if I = 2 then E.Transmit else E.Receive));
            Verify (E.Max_RF_Bandwidth_Hz (G) = (if Later then 42.25 else 12345678.25));
            Verify
              (E.Max_Sample_Rate_Samples_Per_Second (G) = 2500000.5
               and E.Max_Data_Rate_Bits_Per_Second (G) = 987654321.125
               and E.Max_Duty_Factor (G) = 0.625);
            Verify (E.Data_Pipes_Included (G) = Included);
            Verify (E.Pipe_Count (G) = (if Included and I /= 2 then 2 else 0));
            if Included and I /= 2 then
               declare
                  P     : constant E.Data_Pipe_Info := E.Pipe_At (G, 1);
                  Empty : constant E.Data_Pipe_Info := E.Pipe_At (G, 2);
               begin
                  Verify (E.Lookup_Label (P) = "a-pipe" and E.Lookup_Label (Empty) = "µ-pipe");
                  Verify (E.Label (P) = (if Later then "pipe-later-β" else "pipe-returned-µ"));
                  Verify
                    (E.Label (Empty) = (if Later then "pipe-later-β" else "")
                     and E.Endpoint_Count (Empty) = 0);
                  Verify (E.Endpoint_Count (P) = (if Later then 2 else 3));
                  Verify (E.Endpoint_At (P, 1) = (if Later then 7 else 0));
                  if not Later then
                     Verify (E.Endpoint_At (P, 2) = 16#8000_0000_0000_0000#);
                  end if;
                  Verify (E.Endpoint_At (P, E.Endpoint_Count (P)) = Interfaces.Unsigned_64'Last);
               end;
            end if;
         end;
      end loop;
   end Check;
   procedure Numeric (Value : E.Element_Group_Descriptor) is
      --  Classification through public accessors only; IEEE values are intentional.
      pragma Validity_Checks ("F");
      Z   : constant Long_Float := E.Max_RF_Bandwidth_Hz (Value);
      Inf : constant Long_Float := E.Max_Sample_Rate_Samples_Per_Second (Value);
      NaN : constant Long_Float := E.Max_Data_Rate_Bits_Per_Second (Value);
   begin
      Verify (Z = 0.0 and Long_Float'Copy_Sign (1.0, Z) = -1.0);
      Verify (Inf > Long_Float'Last and Inf > 0.0);
      Verify (NaN /= NaN and E.Max_Duty_Factor (Value) = -0.625);
   end Numeric;
begin
   CS.Free (Path);
   Verify (Library /= System.Null_Address);
   declare
      Parent  : C2.C2_MEL := C2.Open (Provider, "c2:element-ok");
      Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, Config);
   begin
      Verify (C2.Outcome (C2.Wait (Request, 3000)) = C2.Created);
      declare
         VA : C2.Virtual_Aperture := C2.Claim (Request);
      begin
         C2.Close (Request);
         for M in 0 .. 9 loop
            Verify (Calls (Interfaces.C.unsigned (M)) = 0);
         end loop;
         declare
            Basic      : constant E.Element_Group_List := E.Snapshot_Element_Groups (VA);
            A          : constant E.Element_Group_List := E.Snapshot_Element_Groups (VA, True);
            B          : E.Element_Group_List;
            Individual : constant E.Element_Group_Descriptor := E.Descriptor_At (A, 1);
            procedure Failure (Include : Boolean := True) is
            begin
               begin
                  B := E.Snapshot_Element_Groups (VA, Include);
                  raise Program_Error with "expected Provider_Error";
               exception
                  when AMS.MEL.Provider_Error =>
                     null;
               end;
               Verify (Live (0) = 0 and Live (1) = 0);
               Check (A, True);
            end Failure;
         begin
            Check (Basic, False);
            Check (A, True);
            Verify (Calls (0) = 2);
            for M in 1 .. 6 loop
               Verify (Calls (Interfaces.C.unsigned (M)) = 6);
            end loop;
            Verify (Calls (7) = 3 and Calls (8) = 4 and Calls (9) = 4);
            for M in 0 .. 9 loop
               for K in 1 .. 3 loop
                  Env.Set ("AMS_MEL_TEST_ELEMENT_THROW_METHOD", Natural'Image (M));
                  Env.Set
                    ("AMS_MEL_TEST_ELEMENT_THROW_KIND",
                     (if K = 1 then "standard" elsif K = 2 then "unknown" else "allocation"));
                  Failure;
                  Env.Clear ("AMS_MEL_TEST_ELEMENT_THROW_METHOD");
                  Env.Clear ("AMS_MEL_TEST_ELEMENT_THROW_KIND");
                  B := E.Snapshot_Element_Groups (VA, True);
                  Check (B, True);
               end loop;
            end loop;
            for Category in 1 .. 4 loop
               Env.Set
                 ("AMS_MEL_TEST_ELEMENT_BAD_STRING",
                  (case Category is
                     when 1      => "outer-key",
                     when 2      => "descriptor-label",
                     when 3      => "pipe-key",
                     when others => "pipe-label"));
               for Kind in 1 .. 2 loop
                  Env.Set ("AMS_MEL_TEST_ELEMENT_CASE", (if Kind = 1 then "utf8" else "nul"));
                  Failure;
                  if Category > 2 then
                     B := E.Snapshot_Element_Groups (VA);
                     Check (B, False);
                  end if;
               end loop;
               Env.Clear ("AMS_MEL_TEST_ELEMENT_BAD_STRING");
               Env.Clear ("AMS_MEL_TEST_ELEMENT_CASE");
            end loop;
            for Kind in 1 .. 4 loop
               Env.Set
                 ("AMS_MEL_TEST_ELEMENT_CASE",
                  (case Kind is
                     when 1      => "null-descriptor",
                     when 2      => "mode",
                     when 3      => "null-pipe",
                     when others => "forbidden-pipes"));
               Failure;
               if Kind > 2 then
                  B := E.Snapshot_Element_Groups (VA);
                  Check (B, False);
               end if;
               Env.Clear ("AMS_MEL_TEST_ELEMENT_CASE");
            end loop;
            for Kind in 1 .. 4 loop
               Env.Set
                 ("AMS_MEL_TEST_RF_VA_FAILURE",
                  (case Kind is
                     when 1      => "element-owner",
                     when 2      => "element-descriptor",
                     when 3      => "element-pipe",
                     when others => "element-endpoints"));
               Failure;
               Env.Clear ("AMS_MEL_TEST_RF_VA_FAILURE");
               B := E.Snapshot_Element_Groups (VA, True);
               Check (B, True);
            end loop;
            for Kind in 1 .. 5 loop
               Env.Set
                 ("AMS_MEL_TEST_ELEMENT_CASE",
                  (case Kind is
                     when 1      => "empty",
                     when 2      => "empty-pipes",
                     when 3      => "alias",
                     when 4      => "long",
                     when others => "numeric"));
               for Include in Boolean loop
                  B := E.Snapshot_Element_Groups (VA, Include);
                  Verify (E.Data_Pipes_Included (B) = Include);
                  if Kind = 1 then
                     Verify (E.Count (B) = 0);
                  else
                     declare
                        G : constant E.Element_Group_Descriptor := E.Descriptor_At (B, 1);
                     begin
                        Verify (E.Count (B) = 3 and E.Data_Pipes_Included (G) = Include);
                        if Kind = 2 then
                           Verify (E.Pipe_Count (G) = 0);
                        elsif Kind = 3 then
                           Verify (E.Label (G) = E.Label (E.Descriptor_At (B, 3)));
                           if Include then
                              Verify (E.Endpoint_Count (E.Pipe_At (G, 2)) = 3);
                           end if;
                        elsif Kind = 4 then
                           Verify (E.Lookup_Label (G)'Length = 110 and E.Label (G)'Length = 122);
                           if Include then
                              Verify (E.Label (E.Pipe_At (G, 1))'Length = 102);
                           end if;
                        else
                           Numeric (G);
                        end if;
                     end;
                  end if;
               end loop;
               Env.Clear ("AMS_MEL_TEST_ELEMENT_CASE");
            end loop;
            begin
               B := E.Element_Group_List'(A);
               Verify (E.Label (E.Descriptor_At (B, 4)) = "");
               raise Program_Error;
            exception
               when Constraint_Error =>
                  null;
            end;
            begin
               Verify (E.Endpoint_At (E.Pipe_At (Individual, 2), 1) = 0);
               raise Program_Error;
            exception
               when Constraint_Error =>
                  null;
            end;
            Change (1);
            B := E.Snapshot_Element_Groups (VA, True);
            Check (B, True, True);
            Check (A, True);
            C2.Close (Parent);
            B := E.Snapshot_Element_Groups (VA, True);
            Check (B, True, True);
            Verify (Live (0) = 0 and Live (1) = 0);
            Verify (DL_Close (Library) = 0);
            C2.Close (VA);
            begin
               B := E.Snapshot_Element_Groups (VA, True);
               raise Program_Error with "closed VA accepted";
            exception
               when AMS.MEL.Provider_Error =>
                  null;
            end;
            Check (A, True);
            Check (Basic, False);
            Check (B, True, True);
            Verify
              (E.Data_Pipes_Included (Individual) and E.Label (Individual) = "returned-empty-key");
         end;
      end;
   end;
   Ada.Text_IO.Put_Line
     ("PASS: safe Ada element-group values, optional pipes, exact numerics/endpoints and checked copy-out");
end AMS_MEL_RF_Element_Groups;
