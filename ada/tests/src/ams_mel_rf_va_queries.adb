with Ada.Environment_Variables;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.RF.C2;
with AMS.MEL.RF.C2.Virtual_Aperture_Queries;
with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;
with GNAT.Source_Info;

procedure AMS_MEL_RF_VA_Queries is
   package C2 renames AMS.MEL.RF.C2;
   package Q renames C2.Virtual_Aperture_Queries;
   package CS renames Interfaces.C.Strings;
   use type Interfaces.Unsigned_32;
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
   type Count_Access is
     access function (Method : Interfaces.C.unsigned) return Interfaces.C.unsigned
   with Convention => C;
   function To_Count is new Ada.Unchecked_Conversion (System.Address, Count_Access);
   Provider : constant String :=
     Ada.Environment_Variables.Value ("AMS_MEL_TEST_PROVIDER_DIR") & "/libmock_rf_provider.so";
   Path     : CS.chars_ptr := CS.New_String (Provider);
   Library  : constant System.Address := DL_Open (Path, 2);
   Name     : CS.chars_ptr := CS.New_String ("mock_rf_va_query_calls");
   Calls    : constant Count_Access := To_Count (DL_Sym (Library, Name));
   procedure Verify (Condition : Boolean; Location : String := GNAT.Source_Info.Source_Location) is
   begin
      if not Condition then
         raise Program_Error with "VA query assertion at " & Location;
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
   procedure Report (Value : Q.Instance_Status_Report; State : Q.Status_Kind) is
      First : constant Q.Local_Function_Status_Group := Q.Local_Function_Group_At (Value, 1);
      Empty : constant Q.Local_Function_Status_Group := Q.Local_Function_Group_At (Value, 2);
      Last  : constant Q.Local_Function_Status_Group := Q.Local_Function_Group_At (Value, 3);
   begin
      Verify
        (Q.Instance_ID (Value) = 42
         and Q.Status (Value) = State
         and Q.Local_Function_Type_Count (Value) = 3);
      Verify
        (Q.Local_Function_Type_ID (First) = 0
         and Q.Instance_Status_Count (First) = 4
         and Q.Instance_Status_At (First, 1) = Q.Failed
         and Q.Instance_Status_At (First, 2) = Q.None
         and Q.Instance_Status_At (First, 3) = Q.Degraded
         and Q.Instance_Status_At (First, 4) = Q.Operational);
      Verify
        (Q.Local_Function_Type_ID (Empty) = 16#8000_0001# and Q.Instance_Status_Count (Empty) = 0);
      Verify
        (Q.Local_Function_Type_ID (Last) = Interfaces.Unsigned_32'Last
         and Q.Instance_Status_Count (Last) = 3
         and Q.Instance_Status_At (Last, 1) = Q.Operational
         and Q.Instance_Status_At (Last, 2) = Q.Operational
         and Q.Instance_Status_At (Last, 3) = Q.Failed);
   end Report;
   procedure Invoke (VA : C2.Virtual_Aperture'Class; Method : Natural) is
   begin
      case Method is
         when 0      =>
            Verify (Q.Query_ID (VA) = 16#8765_4321#);

         when 1      =>
            declare
               Value : constant Q.Status_Kind := Q.Query_Status (VA);
            begin
               Verify (Value'Valid);
            end;

         when 2      =>
            Verify (Q.Query_Instance_Status (VA, Interfaces.Unsigned_32'Last) = Q.Failed);

         when 3      =>
            declare
               Value : constant Q.Instance_ID_List := Q.Snapshot_All_Instances (VA);
            begin
               Verify (Q.Count (Value) > 0);
            end;

         when 4      =>
            declare
               Value : constant Q.Instance_ID_List := Q.Snapshot_Instances (VA, 16#FEDC_BA98#);
            begin
               Verify (Q.Count (Value) = 3 and Q.Instance_ID_At (Value, 3) = 42);
            end;

         when 5      =>
            declare
               Value : constant Q.Instance_Status_Report :=
                 Q.Snapshot_Instance_Status_Report (VA, 16#DEAD_BEEF#);
            begin
               Verify (Q.Instance_ID (Value) = 42);
            end;

         when others =>
            raise Program_Error;
      end case;
   end Invoke;
begin
   CS.Free (Path);
   CS.Free (Name);
   Verify (Library /= System.Null_Address and Calls /= null);
   declare
      Parent  : C2.C2_MEL := C2.Open (Provider, "c2:va-query-changing");
      Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, Config);
   begin
      Verify (C2.Outcome (C2.Wait (Request, 3_000)) = C2.Created);
      declare
         VA : C2.Virtual_Aperture := C2.Claim (Request);
      begin
         C2.Close (Request);
         for M in Interfaces.C.unsigned range 0 .. 5 loop
            Verify (Calls (M) = 0);
         end loop;
         Verify (Q.Query_ID (VA) = 16#8765_4321#);
         for S in Q.Status_Kind loop
            Verify (Q.Query_Status (VA) = S);
         end loop;
         Verify (Q.Query_Instance_Status (VA, 16#DEAD_BEEF#) = Q.Failed);
         declare
            A     : constant Q.Instance_ID_List := Q.Snapshot_All_Instances (VA);
            B     : Q.Instance_ID_List := Q.Snapshot_All_Instances (VA);
            Face  : constant Q.Instance_ID_List := Q.Snapshot_Instances (VA, 16#FEDC_BA98#);
            Empty : constant Q.Instance_ID_List :=
              Q.Snapshot_Instances (VA, Interfaces.Unsigned_32'Last);
            RA    : constant Q.Instance_Status_Report :=
              Q.Snapshot_Instance_Status_Report (VA, 16#DEAD_BEEF#);
            RB    : Q.Instance_Status_Report :=
              Q.Snapshot_Instance_Status_Report (VA, 16#DEAD_BEEF#);
         begin
            Verify
              (Q.Count (A) = 4
               and Q.Instance_ID_At (A, 1) = Interfaces.Unsigned_32'Last
               and Q.Instance_ID_At (A, 2) = 7
               and Q.Instance_ID_At (A, 3) = 0
               and Q.Instance_ID_At (A, 4) = 7);
            Verify
              (Q.Count (B) = 2 and Q.Instance_ID_At (B, 1) = 2 and Q.Instance_ID_At (B, 2) = 42);
            Verify
              (Q.Count (Face) = 3
               and Q.Instance_ID_At (Face, 1) = 42
               and Q.Instance_ID_At (Face, 2) = 2
               and Q.Instance_ID_At (Face, 3) = 42
               and Q.Count (Empty) = 0);
            Report (RA, Q.Degraded);
            Report (RB, Q.Operational);
            B := Empty;
            RB := RA;
            Verify (Q.Count (A) = 4);
            Report (RA, Q.Degraded);
            for E in 1 .. 4 loop
               Ada.Environment_Variables.Set
                 ("AMS_MEL_TEST_VA_QUERY_EXCEPTION",
                  (case E is
                     when 1      => "standard",
                     when 2      => "unknown",
                     when 3      => "allocation",
                     when others => "long"));
               for M in 0 .. 5 loop
                  declare
                     Before : constant Interfaces.C.unsigned := Calls (Interfaces.C.unsigned (M));
                  begin
                     begin
                        Invoke (VA, M);
                        raise Program_Error with "missing query failure";
                     exception
                        when AMS.MEL.Provider_Error =>
                           null;
                     end;
                     Verify (Calls (Interfaces.C.unsigned (M)) = Before + 1);
                  end;
               end loop;
               Ada.Environment_Variables.Clear ("AMS_MEL_TEST_VA_QUERY_EXCEPTION");
               for M in 0 .. 5 loop
                  Invoke (VA, M);
               end loop;
            end loop;
            Ada.Environment_Variables.Set ("AMS_MEL_TEST_VA_BAD_REPORT", "1");
            begin
               Invoke (VA, 5);
               raise Program_Error with "malformed report accepted";
            exception
               when AMS.MEL.Provider_Error =>
                  null;
            end;
            Ada.Environment_Variables.Clear ("AMS_MEL_TEST_VA_BAD_REPORT");
            Ada.Environment_Variables.Set ("AMS_MEL_TEST_VA_UNKNOWN_STATUS", "1");
            for M in 1 .. 2 loop
               begin
                  Invoke (VA, M);
                  raise Program_Error with "unknown status accepted";
               exception
                  when AMS.MEL.Provider_Error =>
                     null;
               end;
            end loop;
            Ada.Environment_Variables.Clear ("AMS_MEL_TEST_VA_UNKNOWN_STATUS");
            Ada.Environment_Variables.Set ("AMS_MEL_TEST_RF_VA_FAILURE", "query-report-nested");
            begin
               Invoke (VA, 5);
               raise Program_Error with "nested allocation failure accepted";
            exception
               when AMS.MEL.Provider_Error =>
                  null;
            end;
            Ada.Environment_Variables.Clear ("AMS_MEL_TEST_RF_VA_FAILURE");
            C2.Close (Parent);
            for M in 0 .. 5 loop
               Invoke (VA, M);
            end loop;
            Verify (C2.VA_Instance_ID_Count (VA) = 3 and C2.VA_Instance_ID_At (VA, 3) = 9);
            C2.Close (VA);
            for M in 0 .. 5 loop
               begin
                  Invoke (VA, M);
                  raise Program_Error with "closed VA accepted";
               exception
                  when AMS.MEL.Provider_Error =>
                     null;
               end;
            end loop;
            Verify (Q.Count (A) = 4 and Q.Count (B) = 0 and Q.Count (Face) = 3);
            Report (RA, Q.Degraded);
            Report (RB, Q.Degraded);
            begin
               Verify (Q.Instance_ID_At (A, 5) = 0);
               raise Program_Error with "list bounds accepted";
            exception
               when Constraint_Error =>
                  null;
            end;
            begin
               Verify (Q.Instance_Status_At (Q.Local_Function_Group_At (RA, 2), 1) = Q.None);
               raise Program_Error with "empty LF bounds accepted";
            exception
               when Constraint_Error =>
                  null;
            end;
         end;
      end;
   end;
   Verify (DL_Close (Library) = 0);
   Ada.Text_IO.Put_Line
     ("PASS: safe Ada VA live queries, nested copy-out, failures and parent-first lifetime");
end AMS_MEL_RF_VA_Queries;
