with Ada.Environment_Variables;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.RF.C2;
with AMS.MEL.RF.C2.Data_Pipes;
with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;
with GNAT.Source_Info;

procedure AMS_MEL_RF_Data_Pipes is
   package C2 renames AMS.MEL.RF.C2;
   package P renames C2.Data_Pipes;
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
   type Input_Access is
     access function (Which : Interfaces.C.unsigned) return Interfaces.Unsigned_64
   with Convention => C;
   type Change_Access is access procedure (Generation : Interfaces.C.unsigned) with Convention => C;
   function To_Count is new Ada.Unchecked_Conversion (System.Address, Count_Access);
   function To_Input is new Ada.Unchecked_Conversion (System.Address, Input_Access);
   function To_Change is new Ada.Unchecked_Conversion (System.Address, Change_Access);
   Provider : constant String :=
     Ada.Environment_Variables.Value ("AMS_MEL_TEST_PROVIDER_DIR") & "/libmock_rf_provider.so";
   Path     : CS.chars_ptr := CS.New_String (Provider);
   Library  : constant System.Address := DL_Open (Path, 2);
   function Symbol (Name : String) return System.Address is
      Text  : CS.chars_ptr := CS.New_String (Name);
      Value : constant System.Address := DL_Sym (Library, Text);
   begin
      CS.Free (Text);
      return Value;
   end Symbol;
   Calls    : constant Count_Access := To_Count (Symbol ("mock_rf_connection_calls"));
   Lifetime : constant String := "/tmp/ams-rf-pipes-ada-" & Interfaces.C.int'Image (Process_ID);
   E1       : constant Count_Access := To_Count (Symbol ("mock_rf_va_query_calls"));
   Input    : constant Input_Access := To_Input (Symbol ("mock_rf_connection_input"));
   Change   : constant Change_Access := To_Change (Symbol ("mock_rf_connection_change"));
   procedure Verify (Condition : Boolean; Location : String := GNAT.Source_Info.Source_Location) is
   begin
      if not Condition then
         raise Program_Error with "DataPipe assertion at " & Location;
      end if;
   end Verify;
   function Config return C2.Virtual_Aperture_Config is
      V       : C2.Virtual_Aperture_Config :=
        C2.Create_Virtual_Aperture_Config (16#FEDC_BA98#, 16#8000_0001#, "definition/β.json");
      A, B, C : AMS.MEL.IR.UUID := [others => 0];
   begin
      C2.Append_Local_Function_Info (V, "alpha");
      C2.Append_Local_Function_Info (V, "µ-local");
      C2.Append_Local_Function_Info (V, "");
      A (1) := 16#80#;
      A (2) := 16#FF#;
      B (0) := 16#FF#;
      C (15) := 16#80#;
      C2.Append_Capability_ID (V, AMS.MEL.IR.Create_UCI_ID (A, "first"));
      C2.Append_Capability_ID (V, AMS.MEL.IR.Create_UCI_ID (B, ""));
      C2.Append_Capability_ID (V, AMS.MEL.IR.Create_UCI_ID (C, "µ-third"));
      return V;
   end Config;
   procedure Connections (Value : P.Connection_Snapshot) is
   begin
      Verify (P.Group_Count (Value) = 3);
      Verify (P.Element_Group_Label (P.Group_At (Value, 1)) = "");
      Verify (P.Element_Group_Label (P.Group_At (Value, 2)) = "rx/main");
      Verify (P.Element_Group_Label (P.Group_At (Value, 3)) = "µ-group");
      Verify (P.Pipe_Count (P.Group_At (Value, 3)) = 0);
      for I in 1 .. 2 loop
         declare
            G : constant P.Element_Group_Connection := P.Group_At (Value, I);
            A : constant P.Data_Pipe_Connection := P.Pipe_At (G, 1);
            B : constant P.Data_Pipe_Connection := P.Pipe_At (G, 2);
         begin
            Verify (P.Pipe_Count (G) = 2);
            Verify (P.Lookup_Label (A) = (if I = 1 then "a" else "default"));
            Verify (P.Lookup_Label (B) = (if I = 1 then "default" else "z"));
            Verify (P.Label (A) = "returned-µ" and P.Label (B) = "");
            Verify (P.Endpoint_Count (A) = 3 and P.Endpoint_Count (B) = 0);
            Verify (P.Endpoint_At (A, 1) = 0);
            Verify (P.Endpoint_At (A, 2) = 16#8000_0000_0000_0000#);
            Verify (P.Endpoint_At (A, 3) = Interfaces.Unsigned_64'Last);
         end;
      end loop;
   end Connections;
   procedure Invoke (VA : in out C2.Virtual_Aperture'Class; Method : Natural) is
   begin
      if Method = 0 then
         Connections (P.Snapshot (VA));
      elsif Method = 1 then
         Verify (P.Associate_Endpoint (VA, "", "a", 16#8000_0000_0000_0000#));
      else
         Verify
           (P.Associate_Endpoints
              (VA, "", "a", [Interfaces.Unsigned_64'Last, 7, 0, 7, 16#8000_0000_0000_0000#]));
      end if;
   end Invoke;
begin
   Verify (Library /= System.Null_Address);
   CS.Free (Path);
   Change (0);
   Ada.Environment_Variables.Set ("AMS_MEL_TEST_LIFETIME_LOG", Lifetime);
   declare
      Log : Ada.Text_IO.File_Type;
   begin
      Ada.Text_IO.Create (Log, Ada.Text_IO.Out_File, Lifetime);
      Ada.Text_IO.Close (Log);
   end;
   declare
      Parent  : C2.C2_MEL := C2.Open (Provider, "c2:element-ok");
      Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, Config);
      Result  : constant C2.Virtual_Aperture_Result := C2.Wait (Request, 3_000);
   begin
      Verify (C2.Outcome (Result) = C2.Created);
      declare
         VA : C2.Virtual_Aperture := C2.Claim (Request);
         A  : constant P.Connection_Snapshot := P.Snapshot (VA);
      begin
         C2.Close (Request);
         Verify (Calls (0) = 1 and Calls (1) = 4 and Calls (2) = 4);
         Connections (A);
         for ID of P.Endpoint_ID_Array'[0, 16#8000_0000_0000_0000#, Interfaces.Unsigned_64'Last]
         loop
            Verify (P.Associate_Endpoint (VA, "", "a", ID));
            Verify (Input (0) = ID);
         end loop;
         Invoke (VA, 2);
         Verify
           (Input (5) = 4
            and Input (1) = 0
            and Input (2) = 7
            and Input (3) = 16#8000_0000_0000_0000#
            and Input (4) = Interfaces.Unsigned_64'Last);
         Verify (P.Associate_Endpoints (VA, "", "a", []));
         Verify (Input (5) = 0);
         Connections (P.Snapshot (VA)); -- Fresh-object state, not forced persistence.
         Ada.Environment_Variables.Set ("AMS_MEL_TEST_CONNECTION_CASE", "false");
         for I in 1 .. 2 loop
            Verify (not P.Associate_Endpoint (VA, "", "a", 0));
            Verify (not P.Associate_Endpoints (VA, "", "a", []));
         end loop;
         Ada.Environment_Variables.Clear ("AMS_MEL_TEST_CONNECTION_CASE");
         for Many in Boolean loop
            declare
               Before  : constant Interfaces.C.unsigned := Calls (0);
               Ignored : Boolean;
            begin
               begin
                  Ignored :=
                    (if Many
                     then P.Associate_Endpoints (VA, "bad" & Character'Val (0), "a", [])
                     else P.Associate_Endpoint (VA, "", "a" & Character'Val (0), 0));
                  raise Program_Error with "embedded NUL accepted";
               exception
                  when Constraint_Error =>
                     null;
               end;
               Verify (Calls (0) = Before);
               begin
                  Ignored :=
                    (if Many
                     then P.Associate_Endpoints (VA, "missing", "a", [])
                     else P.Associate_Endpoint (VA, "", "missing", 0));
                  raise Program_Error with "missing lookup accepted";
               exception
                  when AMS.MEL.Provider_Error =>
                     null;
               end;
            end;
         end loop;
         for M in 0 .. 4 loop
            Ada.Environment_Variables.Set
              ("AMS_MEL_TEST_CONNECTION_THROW_METHOD", Natural'Image (M));
            for Kind in 1 .. 3 loop
               Ada.Environment_Variables.Set
                 ("AMS_MEL_TEST_CONNECTION_THROW_KIND",
                  (case Kind is
                     when 1      => "standard",
                     when 2      => "unknown",
                     when others => "allocation"));
               begin
                  Invoke (VA, (if M < 3 then 0 elsif M = 3 then 1 else 2));
                  raise Program_Error with "provider exception accepted";
               exception
                  when AMS.MEL.Provider_Error =>
                     null;
               end;
            end loop;
            Ada.Environment_Variables.Clear ("AMS_MEL_TEST_CONNECTION_THROW_METHOD");
            Ada.Environment_Variables.Clear ("AMS_MEL_TEST_CONNECTION_THROW_KIND");
            Invoke (VA, 0);
            Connections (A);
         end loop;
         for F in 1 .. 4 loop
            Ada.Environment_Variables.Set
              ("AMS_MEL_TEST_RF_VA_FAILURE",
               (case F is
                  when 1      => "connections-owner",
                  when 2      => "connections-group",
                  when 3      => "connections-pipe",
                  when others => "connections-endpoints"));
            begin
               Invoke (VA, 0);
               raise Program_Error with "allocation failure accepted";
            exception
               when AMS.MEL.Provider_Error =>
                  null;
            end;
            Ada.Environment_Variables.Clear ("AMS_MEL_TEST_RF_VA_FAILURE");
            Connections (A);
         end loop;
         C2.Close (Parent);
         for M in 0 .. 2 loop
            Invoke (VA, M);
         end loop;
         for M in 0 .. 5 loop
            Verify (E1 (Interfaces.C.unsigned (M)) = 0);
         end loop;
         Verify (DL_Close (Library) = 0);
         C2.Close (VA);
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
                  end if;
               end;
            end loop;
            Verify (Stage = 4);
            Ada.Text_IO.Delete (F);
         end;
         Connections (A);
         for M in 0 .. 2 loop
            begin
               Invoke (VA, M);
               raise Program_Error with "closed VA accepted";
            exception
               when AMS.MEL.Provider_Error =>
                  null;
            end;
         end loop;
      end;
   end;
   Ada.Text_IO.Put_Line
     ("PASS: safe Ada VA connections and exact synchronous association, copied values after unload");
end AMS_MEL_RF_Data_Pipes;
