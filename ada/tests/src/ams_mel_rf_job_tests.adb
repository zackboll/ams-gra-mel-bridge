with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.RF.C2;
with Ada.Unchecked_Conversion;
with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;

package body AMS_MEL_RF_Job_Tests is
   package C2 renames AMS.MEL.RF.C2;
   use type C2.Request_Outcome;
   use type C2.Request_Error_Code;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Integer_64;
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
   type Release_Access is access function return Interfaces.C.unsigned with Convention => C;
   function To_Release is new Ada.Unchecked_Conversion (System.Address, Release_Access);
   function Job_Getter_Calls (Path : String) return Interfaces.C.unsigned is
      Name   : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Symbol : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_job_get_calls");
      Handle : constant System.Address := DL_Open (Name, 2);
      Calls  : Interfaces.C.unsigned;
   begin
      if Handle = System.Null_Address then
         raise Program_Error with "mock load failed";
      end if;
      Calls := To_Release (DL_Sym (Handle, Symbol)).all;
      if DL_Close (Handle) /= 0 then
         raise Program_Error with "mock unload failed";
      end if;
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (Symbol);
      return Calls;
   end Job_Getter_Calls;
   procedure Release_Job (Path : String) is
      Name   : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Symbol : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_job_release_one");
      Handle : constant System.Address := DL_Open (Name, 2);
   begin
      if Handle = System.Null_Address
        or else To_Release (DL_Sym (Handle, Symbol)).all /= 1
        or else DL_Close (Handle) /= 0
      then
         raise Program_Error with "mock Job release failed";
      end if;
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (Symbol);
   end Release_Job;

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

   procedure Snapshot (Object : C2.Job) is
   begin
      if C2.Actual_Start_Seconds (Object) /= -123456789
        or else C2.Actual_Start_Femtoseconds (Object) /= 999999999999999
        or else C2.Total_Job_Duration_Femtoseconds (Object) /= 7654321098765
        or else C2.VA_Instance_ID (Object) /= 42
        or else C2.VA_Definition_ID (Object) /= 16#ABCD_EF01#
        or else C2.Job_Details_ID (Object) /= 16#1020_3040#
        or else C2.Job_Request_ID (Object) /= 16#FEDC_BA98#
        or else C2.Lookahead_Femtoseconds (Object) /= -12345
        or else C2.RX_Stream_ID_Count (Object) /= 3
        or else C2.RX_Stream_ID_At (Object, 1) /= 0
        or else C2.RX_Stream_ID_At (Object, 2) /= 3
        or else C2.RX_Stream_ID_At (Object, 3) /= Interfaces.Unsigned_32'Last
      then
         raise Program_Error with "Ada Job snapshot lost data";
      end if;
   end Snapshot;

   procedure Run (Provider_Path : String) is
   begin
      declare
         Group : C2.RX_Element_Group_Config := C2.Create_RX_Element_Group ("rx", 1.0);
         procedure Expect_Bad_Label is
         begin
            declare
               Bad    : constant C2.RX_Element_Group_Config :=
                 C2.Create_RX_Element_Group ("rx" & Character'Val (0));
               Config : C2.Job_Config := C2.Create_Job_Config (0, 0, Bad);
            begin
               C2.Append_Instance_Selection (Config, 0);
            end;
            raise Program_Error with "NUL group label accepted";
         exception
            when Constraint_Error =>
               null;
         end Expect_Bad_Label;
      begin
         Expect_Bad_Label;
         begin
            declare
               Bad    : constant C2.RX_Element_Group_Config :=
                 C2.Create_RX_Element_Group ("rx", Data_Pipe_Label => "bad" & Character'Val (0));
               Config : C2.Job_Config := C2.Create_Job_Config (0, 0, Bad);
            begin
               C2.Append_Instance_Selection (Config, 0);
            end;
            raise Program_Error with "NUL data-pipe label accepted";
         exception
            when Constraint_Error =>
               null;
         end;
         begin
            C2.Append_Endpoint_ID (Group, 0);
            C2.Append_Endpoint_ID (Group, 0);
            raise Program_Error with "duplicate endpoint accepted";
         exception
            when Constraint_Error =>
               null;
         end;
         begin
            C2.Append_Expected_Center_Frequency (Group, 2.0, 1.0);
            raise Program_Error with "reversed frequency accepted";
         exception
            when Constraint_Error =>
               null;
         end;
         begin
            declare
               Bad    : constant C2.RX_Element_Group_Config :=
                 C2.Create_RX_Element_Group ("rx", 0.0);
               Config : C2.Job_Config := C2.Create_Job_Config (0, 0, Bad);
            begin
               C2.Append_Instance_Selection (Config, 0);
            end;
            raise Program_Error with "zero duty accepted";
         exception
            when Constraint_Error =>
               null;
         end;
      end;
      for Scenario in 1 .. 3 loop
         declare
            Parent     : C2.C2_MEL :=
              C2.Open (Provider_Path, (if Scenario = 1 then "c2:job-ok" else "c2:job-delayed"));
            VA_Request : C2.Virtual_Aperture_Request :=
              C2.Submit_Virtual_Aperture (Parent, VA_Config);
         begin
            if C2.Outcome (C2.Wait (VA_Request, 3_000)) /= C2.Created then
               raise Program_Error with "mock VA rejected";
            end if;
            declare
               VA      : C2.Virtual_Aperture := C2.Claim (VA_Request);
               Request : C2.Job_Request := C2.Submit_Job (VA, Job_Config);
            begin
               C2.Close (VA_Request);
               if Scenario /= 1 then
                  begin
                     declare
                        Ignore : constant C2.Job_Result := C2.Wait (Request, 0);
                     begin
                        raise Program_Error with "pending Job did not time out";
                     end;
                  exception
                     when C2.Timeout_Error =>
                        null;
                  end;
                  if Scenario = 3 then
                     C2.Close (VA);
                     C2.Close (Parent);
                  end if;
                  Release_Job (Provider_Path);
               end if;
               if C2.Outcome (C2.Wait (Request, 3_000)) /= C2.Created
                 or else C2.Outcome (C2.Wait (Request, 0)) /= C2.Created
               then
                  raise Program_Error with "mock Job not created";
               end if;
               declare
                  Object : C2.Job := C2.Claim (Request);
               begin
                  begin
                     declare
                        Second : constant C2.Job := C2.Claim (Request);
                     begin
                        if C2.Is_Open (Second) then
                           raise Program_Error with "second Job Claim accepted";
                        end if;
                     end;
                     raise Program_Error with "second Job Claim did not fail";
                  exception
                     when AMS.MEL.Provider_Error =>
                        null;
                  end;
                  C2.Close (Request);
                  C2.Close (Request);
                  if Scenario /= 3 then
                     C2.Close (VA);
                     C2.Close (Parent);
                  end if;
                  Snapshot (Object);
                  C2.Close (Object);
                  C2.Close (Object);
                  if C2.Is_Open (Object) or else C2.Is_Open (Request) then
                     raise Program_Error with "Job owners remained open";
                  end if;
               end;
            end;
         end;
      end loop;
      for Scenario in 1 .. 2 loop
         declare
            Parent     : C2.C2_MEL :=
              C2.Open
                (Provider_Path, (if Scenario = 1 then "c2:job-failure" else "c2:job-long-failure"));
            VA_Request : C2.Virtual_Aperture_Request :=
              C2.Submit_Virtual_Aperture (Parent, VA_Config);
         begin
            if C2.Outcome (C2.Wait (VA_Request, 3_000)) /= C2.Created then
               raise Program_Error;
            end if;
            declare
               VA      : C2.Virtual_Aperture := C2.Claim (VA_Request);
               Request : C2.Job_Request := C2.Submit_Job (VA, Job_Config);
               Result  : constant C2.Job_Result := C2.Wait (Request, 3_000);
            begin
               if C2.Outcome (Result) /= C2.Failed
                 or else C2.Error_Code (Result) /= C2.Invalid_Parameters
                 or else (if Scenario = 1
                          then C2.Description (Result) /= "mock Job rejected"
                          else
                            C2.Description (Result)'Length <= 800
                            or else C2.Description (Result)
                                      (C2.Description (Result)'Last
                                       - 5
                                       .. C2.Description (Result)'Last)
                                    /= "µ end")
               then
                  raise Program_Error with "Job failure diagnostic lost";
               end if;
               C2.Close (Request);
               C2.Close (VA);
               C2.Close (VA_Request);
               C2.Close (Parent);
            end;
         end;
      end loop;
      declare
         Parent     : C2.C2_MEL := C2.Open (Provider_Path, "c2:job-getter-throw");
         VA_Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, VA_Config);
      begin
         if C2.Outcome (C2.Wait (VA_Request, 3_000)) /= C2.Created then
            raise Program_Error;
         end if;
         declare
            VA      : C2.Virtual_Aperture := C2.Claim (VA_Request);
            Request : C2.Job_Request := C2.Submit_Job (VA, Job_Config);
            Before  : Interfaces.C.unsigned;
         begin
            if C2.Outcome (C2.Wait (Request, 3_000)) /= C2.Created then
               raise Program_Error;
            end if;
            Before := Job_Getter_Calls (Provider_Path);
            for I in 1 .. 2 loop
               begin
                  declare
                     Object : constant C2.Job := C2.Claim (Request);
                  begin
                     if not C2.Is_Open (Object) then
                        raise Program_Error;
                     end if;
                     raise Program_Error with "getter failure accepted";
                  end;
               exception
                  when AMS.MEL.Provider_Error =>
                     null;
               end;
            end loop;
            if Job_Getter_Calls (Provider_Path) /= Before + 1 then
               raise Program_Error with "Job getters were rerun after failed Claim";
            end if;
            C2.Close (Request);
            C2.Close (VA);
            C2.Close (VA_Request);
            C2.Close (Parent);
         end;
      end;
   end Run;
end AMS_MEL_RF_Job_Tests;
