with Ada.Environment_Variables;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with AMS.MEL.IR;
with AMS.MEL.RF.C2;
with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;

package body AMS_MEL_RF_Job_TX_Groups is
   package C2 renames AMS.MEL.RF.C2;
   package Env renames Ada.Environment_Variables;
   use type Interfaces.Integer_64;
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
   function RX_Group (Label : String) return C2.RX_Element_Group_Config is
      Group : C2.RX_Element_Group_Config := C2.Create_RX_Element_Group (Label, 0.625, "products/β");
   begin
      C2.Append_Expected_Center_Frequency (Group, 1000000.25, 2000000.5);
      C2.Append_Expected_Center_Frequency (Group, 987654321.125, 987654322.875);
      C2.Append_Endpoint_ID (Group, 0);
      C2.Append_Endpoint_ID (Group, 16#8000_0000_0000_0000#);
      C2.Append_Endpoint_ID (Group, Interfaces.Unsigned_64'Last);
      C2.Append_Endpoint_ID (Group, "secondary", 7);
      C2.Append_Endpoint_ID (Group, "secondary", 42);
      C2.Append_Expected_Pointing
        (Group,
         C2.Create_ECEF_Pointing
           (1.25, -2.5, 3.75, -4.5, 5.625, -6.75, C2.Create_UTC_Time (-7, 123456789012345)));
      C2.Append_Expected_Pointing (Group, C2.Create_Platform_Relative_Pointing (-0.75, 0.25));
      return Group;
   end RX_Group;
   function TX_Group
     (Label : String; Power : Interfaces.Unsigned_32; Last : Boolean := False)
      return C2.TX_Element_Group_Config
   is
      Group : C2.TX_Element_Group_Config :=
        C2.Create_TX_Element_Group (Label, Power, (if Last then 1.0 else 0.375));
   begin
      if Last then
         C2.Append_Expected_Center_Frequency (Group, -1.25, 3.5);
      else
         C2.Append_Expected_Center_Frequency (Group, 100000000.25, 100000001.5);
         C2.Append_Expected_Center_Frequency (Group, 915000000.0, 915000000.0);
      end if;
      return Group;
   end TX_Group;
   function Config
     (Case_Name : String; Power : Interfaces.Unsigned_32 := 16#DEAD_BEEF#) return C2.Job_Config
   is
      A     : constant C2.RX_Element_Group_Config :=
        RX_Group ((if Case_Name = "repeated" then "same" else "rx/a"));
      T     : constant C2.TX_Element_Group_Config :=
        TX_Group ((if Case_Name = "repeated" then "same" else "tx/a"), Power);
      Value : C2.Job_Config :=
        (if Case_Name = "tx-only" or Case_Name = "repeated"
         then C2.Create_Job_Config (16#FEDC_BA98#, 16#8000_0001#, T, 16#7FFF_FFFE#, True)
         else C2.Create_Job_Config (16#FEDC_BA98#, 16#8000_0001#, A, 16#7FFF_FFFE#, True));
   begin
      if Case_Name = "primary" then
         C2.Append_TX_Element_Group (Value, T);
         C2.Append_RX_Element_Group (Value, RX_Group ("rx/b"));
         C2.Append_TX_Element_Group (Value, TX_Group ("tx/b", Interfaces.Unsigned_32'Last, True));
      elsif Case_Name = "repeated" then
         C2.Append_RX_Element_Group (Value, A);
         C2.Append_TX_Element_Group (Value, TX_Group ("same", Interfaces.Unsigned_32'Last, True));
      end if;
      C2.Set_Estimated_Stab_Point
        (Value,
         C2.Create_LLA_Pointing
           (-8.125,
            9.25,
            -999.5,
            -21.25,
            22.5,
            -23.75,
            C2.Create_UTC_Time (Interfaces.Integer_64'First, 0)));
      C2.Set_Min_Start_Time (Value, C2.Create_UTC_Time (-5, 123456789012345));
      C2.Set_Max_Complete_Time (Value, C2.Create_UTC_Time (42, 999999999999999));
      C2.Set_Duration_Femtoseconds (Value, -123456789012345);
      C2.Set_Lookahead_Femtoseconds (Value, Interfaces.Integer_64'Last);
      C2.Set_Capability_ID (Value, [0, 16#FF#, 16#80#, 0, 7]);
      C2.Set_Activity_ID (Value, [16#DE#, 16#AD#, 0, 16#BE#, 16#EF#]);
      C2.Set_TX_Power_Mode_IDs (Value, [Interfaces.Unsigned_32'Last, 7, 7, 16#8000_0000#]);
      C2.Append_Instance_Selection (Value, 42);
      C2.Append_Instance_Selection (Value, 0);
      C2.Append_Instance_Selection (Value, 42);
      C2.Append_Instance_Selection (Value, Interfaces.Unsigned_32'Last);
      return Value;
   end Config;
   procedure Run (Provider_Path : String) is
      Powers : constant C2.Unsigned_32_Array := [0, 1, 16#8000_0000#, Interfaces.Unsigned_32'Last];
      type Failure_Kind is (Mismatch, Standard_Error, Unknown_Error, Allocation_Error);
      function Failure_Name (Kind : Failure_Kind) return String
      is (case Kind is
            when Mismatch         => "2:mismatch",
            when Standard_Error   => "2:power-std",
            when Unknown_Error    => "2:power-unknown",
            when Allocation_Error => "2:power-alloc");
   begin
      for Iteration in 1 .. 50 loop
         declare
            Case_Name  : constant String :=
              (case Iteration mod 4 is
                 when 0      => "primary",
                 when 1      => "tx-only",
                 when 2      => "rx-only",
                 when others => "repeated");
            Power      : constant Interfaces.Unsigned_32 :=
              (if Case_Name = "tx-only" then Powers ((Iteration / 4) mod 4) else 16#DEAD_BEEF#);
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
               Env.Set ("AMS_MEL_TEST_F3_CASE", Case_Name);
               if Case_Name = "tx-only" then
                  Env.Set ("AMS_MEL_TEST_F3_POWER", Power'Image);
               end if;
               declare
                  Request : C2.Job_Request := C2.Submit_Job (VA, Config (Case_Name, Power));
               begin
                  C2.Close (Parent);
                  C2.Close (VA);
                  if C2.Outcome (C2.Wait (Request, 3000)) /= C2.Created then
                     raise Program_Error with "F3 Job rejected";
                  end if;
                  declare
                     Object : C2.Job := C2.Claim (Request);
                  begin
                     C2.Close (Request);
                     C2.Close (Object);
                  end;
               end;
               if Case_Name = "tx-only" then
                  Env.Clear ("AMS_MEL_TEST_F3_POWER");
               end if;
               Env.Clear ("AMS_MEL_TEST_F3_CASE");
            end;
         end;
      end loop;
      --  Synchronous TX mode mismatch and setter exceptions must be Provider_Error.
      for Failure in Failure_Kind loop
         declare
            Parent : C2.C2_MEL := C2.Open (Provider_Path, "c2:f1");
            VR     : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, VA_Config);
         begin
            if C2.Outcome (C2.Wait (VR, 3000)) /= C2.Created then
               raise Program_Error;
            end if;
            declare
               VA : C2.Virtual_Aperture := C2.Claim (VR);
            begin
               C2.Close (VR);
               Env.Set ("AMS_MEL_TEST_F3_CASE", "primary");
               Env.Set ("AMS_MEL_TEST_F3_FAILURE", Failure_Name (Failure));
               begin
                  declare
                     Unexpected : C2.Job_Request := C2.Submit_Job (VA, Config ("primary"));
                  begin
                     C2.Close (Unexpected);
                     raise Program_Error with "accepted failure";
                  end;
               exception
                  when AMS.MEL.Provider_Error =>
                     null;
               end;
               Env.Clear ("AMS_MEL_TEST_F3_FAILURE");
               declare
                  Request : C2.Job_Request := C2.Submit_Job (VA, Config ("primary"));
               begin
                  if C2.Outcome (C2.Wait (Request, 3000)) /= C2.Created then
                     raise Program_Error;
                  end if;
                  declare
                     Object : C2.Job := C2.Claim (Request);
                  begin
                     C2.Close (Request);
                     C2.Close (Object);
                  end;
               end;
               Env.Clear ("AMS_MEL_TEST_F3_CASE");
               C2.Close (VA);
               C2.Close (Parent);
            end;
         end;
      end loop;
      Ada.Text_IO.Put_Line ("PASS: safe Ada RF mixed TX/RX focused repeat 50/50");
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
            Env.Set ("AMS_MEL_TEST_F3_CASE", "primary");
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
            Env.Clear ("AMS_MEL_TEST_F3_CASE");
         end;
      end;
   end Run;
end AMS_MEL_RF_Job_TX_Groups;
