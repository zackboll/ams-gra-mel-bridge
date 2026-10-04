with Ada.Environment_Variables;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.RF.C2;
with AMS.MEL.RF.C2.Virtual_Aperture_Queries;
with AMS.MEL.RF.C2.Local_Functions;
with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;
with GNAT.Source_Info;

procedure AMS_MEL_RF_Local_Functions is
   package C2 renames AMS.MEL.RF.C2;
   package Q renames C2.Virtual_Aperture_Queries;
   package LF renames C2.Local_Functions;
   package CS renames Interfaces.C.Strings;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Unsigned_64;
   use type Interfaces.C.unsigned;
   use type Interfaces.C.int;
   use type C2.Request_Outcome;
   use type Q.Status_Kind;
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
     access function (Which : Interfaces.C.unsigned) return Interfaces.Unsigned_32
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
   Calls    : constant Count_Access := To_Count (Symbol ("mock_rf_va_lf_calls"));
   Lifetime : constant String := "/tmp/ams-rf-lf-ada-" & Interfaces.C.int'Image (Process_ID);
   E1       : constant Count_Access := To_Count (Symbol ("mock_rf_va_query_calls"));
   Input    : constant Input_Access := To_Input (Symbol ("mock_rf_va_lf_input"));
   Change   : constant Change_Access := To_Change (Symbol ("mock_rf_va_lf_change"));
   procedure Verify (Condition : Boolean; Location : String := GNAT.Source_Info.Source_Location) is
   begin
      if not Condition then
         raise Program_Error with "LF assertion at " & Location;
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
   procedure Catalog (V : LF.Local_Function_List) is
      IDs    : constant array (1 .. 4) of Interfaces.Unsigned_32 :=
        [0, 7, 16#8000_0001#, Interfaces.Unsigned_32'Last];
      Counts : constant array (1 .. 4) of Interfaces.Unsigned_64 := [2, 3, 1, 0];
   begin
      Verify (LF.Count (V) = 4);
      for I in IDs'Range loop
         Verify
           (LF.Type_ID (LF.Info_At (V, I)) = IDs (I)
            and LF.Instance_Count (LF.Info_At (V, I)) = Counts (I));
      end loop;
   end Catalog;
   procedure Statuses (V : LF.Status_List) is
      Values : constant array (1 .. 5) of Q.Status_Kind :=
        [Q.Failed, Q.None, Q.Operational, Q.Degraded, Q.Failed];
   begin
      Verify (LF.Count (V) = 5);
      for I in Values'Range loop
         Verify (LF.Status_At (V, I) = Values (I));
      end loop;
   end Statuses;
   procedure Invoke (VA : C2.Virtual_Aperture'Class; M : Natural) is
   begin
      case M is
         when 0      =>
            Verify (Q.Cached_Waveform_Supported (VA));

         when 1      =>
            Verify (not Q.Dynamic_Weights_Supported (VA));

         when 2      =>
            Catalog (LF.Snapshot_Local_Functions (VA));

         when others =>
            Statuses (LF.Snapshot_Local_Function_Status (VA, 16#DEAD_BEEF#, 16#8000_0001#));
      end case;
   end Invoke;
begin
   Verify (Library /= System.Null_Address);
   CS.Free (Path);
   Change (0);
   Ada.Environment_Variables.Set ("AMS_MEL_TEST_LIFETIME_LOG", Lifetime);
   declare
      F : Ada.Text_IO.File_Type;
   begin
      Ada.Text_IO.Create (F, Ada.Text_IO.Out_File, Lifetime);
      Ada.Text_IO.Close (F);
   end;
   declare
      Parent  : C2.C2_MEL := C2.Open (Provider, "c2:va-lf");
      Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, Config);
   begin
      Verify (C2.Outcome (C2.Wait (Request, 3_000)) = C2.Created);
      declare
         VA : C2.Virtual_Aperture := C2.Claim (Request);
         A  : LF.Local_Function_List;
         SA : LF.Status_List;
      begin
         C2.Close (Request);
         for M in 0 .. 3 loop
            Verify (Calls (Interfaces.C.unsigned (M)) = 0);
         end loop;
         A := LF.Snapshot_Local_Functions (VA);
         SA := LF.Snapshot_Local_Function_Status (VA, 16#DEAD_BEEF#, 16#8000_0001#);
         Catalog (A);
         Statuses (SA);
         Verify (Input (0) = 16#DEAD_BEEF# and Input (1) = 16#8000_0001#);
         Verify (Q.Cached_Waveform_Supported (VA) and not Q.Dynamic_Weights_Supported (VA));
         declare
            R : constant Q.Instance_Status_Report := Q.Snapshot_Instance_Status_Report (VA, 0);
         begin
            Verify (Q.Local_Function_Type_Count (R) = 3 and E1 (5) = 1 and Calls (3) = 1);
         end;
         Change (1);
         Verify (not Q.Cached_Waveform_Supported (VA) and Q.Dynamic_Weights_Supported (VA));
         declare
            B  : constant LF.Local_Function_List := LF.Snapshot_Local_Functions (VA);
            SB : constant LF.Status_List :=
              LF.Snapshot_Local_Function_Status (VA, 16#DEAD_BEEF#, 16#8000_0001#);
         begin
            Verify
              (LF.Count (B) = 1
               and LF.Type_ID (LF.Info_At (B, 1)) = 42
               and LF.Instance_Count (LF.Info_At (B, 1)) = 4);
            Verify
              (LF.Count (SB) = 3
               and LF.Status_At (SB, 1) = Q.Degraded
               and LF.Status_At (SB, 2) = Q.None
               and LF.Status_At (SB, 3) = Q.Degraded);
            Catalog (A);
            Statuses (SA);
         end;
         Change (2);
         Verify
           (LF.Count (LF.Snapshot_Local_Functions (VA)) = 0
            and LF.Count (LF.Snapshot_Local_Function_Status (VA, 0, 0)) = 0);
         Change (3);
         declare
            B : constant LF.Local_Function_List := LF.Snapshot_Local_Functions (VA);
         begin
            Verify
              (LF.Instance_Count (LF.Info_At (B, 1))
               = Interfaces.Unsigned_64 (Interfaces.C.size_t'Last));
         end;
         Change (0);
         for F in 1 .. 3 loop
            Ada.Environment_Variables.Set
              ("AMS_MEL_TEST_LF_EXCEPTION",
               (case F is
                  when 1      => "standard",
                  when 2      => "unknown",
                  when others => "allocation"));
            for M in 0 .. 3 loop
               declare
                  Before : constant Interfaces.C.unsigned := Calls (Interfaces.C.unsigned (M));
               begin
                  begin
                     Invoke (VA, M);
                     raise Program_Error with "missing provider exception";
                  exception
                     when AMS.MEL.Provider_Error =>
                        null;
                  end;
                  Verify (Calls (Interfaces.C.unsigned (M)) = Before + 1);
               end;
            end loop;
            Ada.Environment_Variables.Clear ("AMS_MEL_TEST_LF_EXCEPTION");
            for M in 0 .. 3 loop
               Invoke (VA, M);
            end loop;
         end loop;
         Ada.Environment_Variables.Set ("AMS_MEL_TEST_LF_UNKNOWN_STATUS", "1");
         begin
            Invoke (VA, 3);
            raise Program_Error with "unknown LF status accepted";
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Ada.Environment_Variables.Clear ("AMS_MEL_TEST_LF_UNKNOWN_STATUS");
         for F in 1 .. 4 loop
            Ada.Environment_Variables.Set
              ("AMS_MEL_TEST_RF_VA_FAILURE",
               (case F is
                  when 1      => "lf-list-owner",
                  when 2      => "lf-list-copy",
                  when 3      => "lf-status-owner",
                  when others => "lf-status-copy"));
            begin
               Invoke (VA, (if F < 3 then 2 else 3));
               raise Program_Error with "missing LF allocation failure";
            exception
               when AMS.MEL.Provider_Error =>
                  null;
            end;
            Ada.Environment_Variables.Clear ("AMS_MEL_TEST_RF_VA_FAILURE");
            Invoke (VA, (if F < 3 then 2 else 3));
            Catalog (A);
            Statuses (SA);
         end loop;
         C2.Close (Parent);
         for M in 0 .. 3 loop
            Invoke (VA, M);
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
         for M in 0 .. 3 loop
            begin
               Invoke (VA, M);
               raise Program_Error with "closed VA accepted";
            exception
               when AMS.MEL.Provider_Error =>
                  null;
            end;
         end loop;
         Catalog (A);
         Statuses (SA);
      end;
   end;
   Ada.Text_IO.Put_Line
     ("PASS: safe Ada required booleans and owned LF values, exact calls, rollback and parent-first lifetime");
end AMS_MEL_RF_Local_Functions;
