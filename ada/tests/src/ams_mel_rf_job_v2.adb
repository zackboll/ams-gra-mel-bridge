with Ada.Environment_Variables;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with AMS.MEL.IR;
with AMS.MEL.RF.C2;
with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;

package body AMS_MEL_RF_Job_V2 is
   package C2 renames AMS.MEL.RF.C2;
   package Env renames Ada.Environment_Variables;
   use type Interfaces.Integer_64;
   use type Interfaces.Unsigned_32;
   use type C2.Request_Outcome;
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
   procedure Release_Job (Provider_Path : String) is
      type Operation is access function return Interfaces.C.unsigned with Convention => C;
      function Convert is new Ada.Unchecked_Conversion (System.Address, Operation);
      Path    : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Provider_Path);
      Name    : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_job_release_one");
      Library : constant System.Address := DL_Open (Path, 2);
   begin
      if Library = System.Null_Address
        or else Convert (DL_Sym (Library, Name)).all /= 1
        or else DL_Close (Library) /= 0
      then
         raise Program_Error with "mock release failed";
      end if;
      Interfaces.C.Strings.Free (Path);
      Interfaces.C.Strings.Free (Name);
   end Release_Job;
   function VA_Config return C2.Virtual_Aperture_Config is
      Value   : C2.Virtual_Aperture_Config :=
        C2.Create_Virtual_Aperture_Config (16#FEDC_BA98#, 16#8000_0001#, "definition/β.json");
      A, B, C : AMS.MEL.IR.UUID := [others => 0];
   begin
      C2.Append_Local_Function_Info (Value, "alpha");
      C2.Append_Local_Function_Info (Value, "µ-local");
      C2.Append_Local_Function_Info (Value, "");
      A (1) := 16#80#;
      A (2) := 16#FF#;
      B (0) := 16#FF#;
      C (15) := 16#80#;
      C2.Append_Capability_ID (Value, AMS.MEL.IR.Create_UCI_ID (A, "first"));
      C2.Append_Capability_ID (Value, AMS.MEL.IR.Create_UCI_ID (B, ""));
      C2.Append_Capability_ID (Value, AMS.MEL.IR.Create_UCI_ID (C, "µ-third"));
      return Value;
   end VA_Config;
   function Config (Case_Name : String) return C2.Job_Config is
      A : C2.RX_Element_Group_Config := C2.Create_RX_Element_Group ("rx/α", 0.625, "products/β");
      B : C2.RX_Element_Group_Config := C2.Create_RX_Element_Group ("", 0.5);
      C : C2.RX_Element_Group_Config := C2.Create_RX_Element_Group ("rx/γ", 1.0);
   begin
      C2.Append_Expected_Center_Frequency (A, 1000000.25, 2000000.5);
      C2.Append_Expected_Center_Frequency (A, 987654321.125, 987654322.875);
      C2.Append_Endpoint_ID (A, 0);
      C2.Append_Endpoint_ID (A, 16#8000_0000_0000_0000#);
      C2.Append_Endpoint_ID (A, Interfaces.Unsigned_64'Last);
      C2.Append_Endpoint_ID (A, "secondary", 7);
      C2.Append_Endpoint_ID (A, "secondary", 42);
      C2.Append_Endpoint_ID (B, 9);
      C2.Append_Expected_Center_Frequency (C, -1.25, 3.5);
      return
         Value : C2.Job_Config :=
           C2.Create_Job_Config (16#FEDC_BA98#, 16#8000_0001#, A, 16#7FFF_FFFE#, True)
      do
         C2.Append_RX_Element_Group (Value, B);
         C2.Append_RX_Element_Group (Value, C);
         C2.Set_Min_Start_Time (Value, C2.Create_UTC_Time (-5, 123456789012345));
         C2.Set_Max_Complete_Time (Value, C2.Create_UTC_Time (42, 999999999999999));
         C2.Set_Duration_Femtoseconds (Value, -123456789012345);
         C2.Set_Lookahead_Femtoseconds (Value, Interfaces.Integer_64'Last);
         C2.Set_Capability_ID (Value, [0, 16#FF#, 16#80#, 0, 7]);
         C2.Set_Activity_ID (Value, [16#DE#, 16#AD#, 0, 16#BE#, 16#EF#]);
         C2.Set_TX_Power_Mode_IDs (Value, [Interfaces.Unsigned_32'Last, 7, 7, 16#8000_0000#]);
         if Case_Name = "boundaries" then
            C2.Set_Min_Start_Time (Value, C2.Create_UTC_Time (Interfaces.Integer_64'First, 0));
            C2.Set_Max_Complete_Time
              (Value, C2.Create_UTC_Time (Interfaces.Integer_64'Last, 999999999999999));
            C2.Set_Duration_Femtoseconds (Value, Interfaces.Integer_64'First);
            C2.Set_Lookahead_Femtoseconds (Value, Interfaces.Integer_64'First);
            C2.Set_TX_Power_Mode_IDs (Value, [9, 0, 9, Interfaces.Unsigned_32'Last, 16#8000_0000#]);
            C2.Append_Instance_Selection (Value, Interfaces.Unsigned_32'Last);
            C2.Append_Instance_Selection (Value, 7);
            C2.Append_Instance_Selection (Value, Interfaces.Unsigned_32'Last);
            C2.Append_Instance_Selection (Value, 0);
         else
            C2.Append_Instance_Selection (Value, 42);
            C2.Append_Instance_Selection (Value, 0);
            C2.Append_Instance_Selection (Value, 42);
            C2.Append_Instance_Selection (Value, Interfaces.Unsigned_32'Last);
         end if;
         if Case_Name = "empty" then
            C2.Set_Capability_ID (Value, []);
            C2.Set_Activity_ID (Value, []);
            C2.Set_TX_Power_Mode_IDs (Value, []);
            C2.Set_Duration_Femtoseconds (Value, 0);
            C2.Set_Lookahead_Femtoseconds (Value, 0);
            C2.Set_Min_Start_Time
              (Value, C2.Create_UTC_Time (Interfaces.Integer_64'Last, 123456789012345));
            C2.Set_Max_Complete_Time
              (Value, C2.Create_UTC_Time (Interfaces.Integer_64'First, 999999999999999));
         elsif Case_Name = "zero" then
            C2.Set_Capability_ID (Value, [0]);
            C2.Set_Activity_ID (Value, [0]);
            C2.Set_Duration_Femtoseconds (Value, Interfaces.Integer_64'Last);
         elsif Case_Name = "binary" then
            declare
               Bytes, Reverse_Bytes : C2.Byte_Array (0 .. 256);
            begin
               for I in Bytes'Range loop
                  Bytes (I) := Interfaces.Unsigned_8 (I mod 256);
                  Reverse_Bytes (I) := Interfaces.Unsigned_8 ((256 - I) mod 256);
               end loop;
               C2.Set_Capability_ID (Value, Bytes);
               C2.Set_Activity_ID (Value, Reverse_Bytes);
            end;
         end if;
      end return;
   end Config;
   procedure Run (Provider_Path : String) is
      Group : C2.RX_Element_Group_Config := C2.Create_RX_Element_Group ("test");
   begin
      for Fraction of C2.Unsigned_32_Array'[0, 1] loop
         begin
            declare
               Invalid : constant C2.UTC_Time :=
                 C2.Create_UTC_Time (0, (if Fraction = 0 then -1 else 1_000_000_000_000_000));
            begin
               raise Program_Error with "accepted invalid UTC" & C2.Seconds (Invalid)'Image;
            end;
         exception
            when Constraint_Error =>
               null;
         end;
      end loop;
      declare
         Low  : constant C2.UTC_Time := C2.Create_UTC_Time (Interfaces.Integer_64'First, 0);
         High : constant C2.UTC_Time :=
           C2.Create_UTC_Time (Interfaces.Integer_64'Last, 999999999999999);
      begin
         if C2.Seconds (Low) /= Interfaces.Integer_64'First
           or else C2.Fractional_Femtoseconds (Low) /= 0
           or else C2.Seconds (High) /= Interfaces.Integer_64'Last
           or else C2.Fractional_Femtoseconds (High) /= 999999999999999
         then
            raise Program_Error with "UTC component fidelity";
         end if;
      end;
      C2.Append_Endpoint_ID (Group, "other", 7);
      begin
         C2.Append_Endpoint_ID (Group, "other", 7);
         raise Program_Error with "accepted duplicate endpoint";
      exception
         when Constraint_Error =>
            null;
      end;
      C2.Append_Endpoint_ID (Group, 7); -- Same ID in different collection is valid.
      for Iteration in 1 .. 50 loop
         declare
            Case_Name  : constant String :=
              (case Iteration mod 5 is
                 when 0      => "primary",
                 when 1      => "boundaries",
                 when 2      => "empty",
                 when 3      => "zero",
                 when others => "binary");
            Parent     : C2.C2_MEL := C2.Open (Provider_Path, "c2:f1");
            VA_Request : C2.Virtual_Aperture_Request :=
              C2.Submit_Virtual_Aperture (Parent, VA_Config);
         begin
            if C2.Outcome (C2.Wait (VA_Request, 3000)) /= C2.Created then
               raise Program_Error with "VA rejected";
            end if;
            declare
               VA : C2.Virtual_Aperture := C2.Claim (VA_Request);
            begin
               C2.Close (VA_Request);
               Env.Set ("AMS_MEL_TEST_F1_CASE", Case_Name);
               declare
                  Request : C2.Job_Request := C2.Submit_Job (VA, Config (Case_Name));
               begin
                  C2.Close (Parent);
                  C2.Close (VA);
                  if C2.Outcome (C2.Wait (Request, 3000)) /= C2.Created then
                     raise Program_Error with "F1 Job rejected";
                  end if;
                  declare
                     Object : C2.Job := C2.Claim (Request);
                  begin
                     C2.Close (Request);
                     C2.Close (Object);
                  end;
               end;
               Env.Clear ("AMS_MEL_TEST_F1_CASE");
            end;
         end;
      end loop;
      Ada.Text_IO.Put_Line ("PASS: safe Ada JobRequest v2 public API focused repeat 50/50");
      declare
         Parent     : C2.C2_MEL := C2.Open (Provider_Path, "c2:f1");
         VA_Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, VA_Config);
      begin
         if C2.Outcome (C2.Wait (VA_Request, 3000)) /= C2.Created then
            raise Program_Error with "delayed VA rejected";
         end if;
         declare
            VA : C2.Virtual_Aperture := C2.Claim (VA_Request);
         begin
            C2.Close (VA_Request);
            Env.Set ("AMS_MEL_TEST_F1_CASE", "delayed");
            declare
               Request : C2.Job_Request := C2.Submit_Job (VA, Config ("primary"));
            begin
               begin
                  declare
                     Unexpected : constant C2.Job_Result := C2.Wait (Request, 0);
                  begin
                     raise Program_Error with "expected timeout: " & C2.Description (Unexpected);
                  end;
               exception
                  when C2.Timeout_Error =>
                     null;
               end;
               C2.Close (Parent);
               C2.Close (VA);
               Release_Job (Provider_Path);
               if C2.Outcome (C2.Wait (Request, 3000)) /= C2.Created then
                  raise Program_Error with "delayed Job rejected";
               end if;
               declare
                  Object : C2.Job := C2.Claim (Request);
               begin
                  C2.Close (Request);
                  C2.Close (Object);
               end;
            end;
            Env.Clear ("AMS_MEL_TEST_F1_CASE");
         end;
      end;
   end Run;
end AMS_MEL_RF_Job_V2;
