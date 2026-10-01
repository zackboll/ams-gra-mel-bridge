with AMS.MEL;
with AMS.MEL.RF.C2;
with AMS.MEL.IR;
with Interfaces;

package body AMS_MEL_RF_C2_Tests is
   package C2 renames AMS.MEL.RF.C2;
   use type Interfaces.Unsigned_32;
   use type C2.Request_Outcome;
   use type C2.Request_Error_Code;

   procedure Expect_Open_Failure (Path, Scenario : String) is
   begin
      declare
         Bad : constant C2.C2_MEL := C2.Open (Path, Scenario);
      begin
         if C2.Is_Open (Bad) then
            raise Program_Error with "RF C2 factory failure accepted";
         end if;
      end;
      raise Program_Error with "RF C2 factory did not fail";
   exception
      when AMS.MEL.Provider_Error =>
         null;
   end Expect_Open_Failure;

   procedure Expect_VA_Failure (Path, Scenario : String; Long_Text : Boolean := False) is
      Parent : C2.C2_MEL := C2.Open (Path, Scenario);
      Config : C2.Virtual_Aperture_Config :=
        C2.Create_Virtual_Aperture_Config (16#FEDC_BA98#, 16#8000_0001#, "definition/β.json");
      First  : AMS.MEL.IR.UUID := [others => 0];
      Second : AMS.MEL.IR.UUID := [others => 0];
      Third  : AMS.MEL.IR.UUID := [others => 0];
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
      declare
         Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, Config);
         Result  : constant C2.Virtual_Aperture_Result := C2.Wait (Request, 3_000);
      begin
         if C2.Outcome (Result) /= C2.Failed
           or else C2.Error_Code (Result) /= C2.Invalid_Parameters
           or else (if Long_Text
                    then C2.Description (Result)'Length <= 800
                    else C2.Description (Result) /= "mock VA rejected")
         then
            raise Program_Error with "VA provider rejection lost";
         end if;
         C2.Close (Request);
      end;
      C2.Close (Parent);
   end Expect_VA_Failure;

   procedure Run (Provider_Path : String) is
      Object : C2.C2_MEL := C2.Open (Provider_Path, "c2:ok");
   begin
      if not C2.Is_Open (Object) then
         raise Program_Error with "RF C2 not open";
      end if;
      C2.Close (Object);
      C2.Close (Object);
      if C2.Is_Open (Object) then
         raise Program_Error with "RF C2 owner not consumed";
      end if;
      declare
         Parent : C2.C2_MEL := C2.Open (Provider_Path, "c2:va-ok");
         Config : C2.Virtual_Aperture_Config :=
           C2.Create_Virtual_Aperture_Config (16#FEDC_BA98#, 16#8000_0001#, "definition/β.json");
         First  : AMS.MEL.IR.UUID := [others => 0];
         Second : AMS.MEL.IR.UUID := [others => 0];
         Third  : AMS.MEL.IR.UUID := [others => 0];
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
         declare
            Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, Config);
         begin
            if C2.Outcome (C2.Wait (Request, 3_000)) /= C2.Created
              or else C2.Outcome (C2.Wait (Request, 0)) /= C2.Created
            then
               raise Program_Error with "VA request not created";
            end if;
            declare
               VA : C2.Virtual_Aperture := C2.Claim (Request);
            begin
               C2.Close (Request);
               C2.Close (Parent);
               if not C2.Is_Open (VA)
                 or else C2.VA_Instance_ID_Count (VA) /= 3
                 or else C2.VA_Instance_ID_At (VA, 1) /= 0
                 or else C2.VA_Instance_ID_At (VA, 2) /= 3
                 or else C2.VA_Instance_ID_At (VA, 3) /= 9
                 or else C2.Element_Group_Label_Count (VA) /= 3
                 or else C2.Element_Group_Label_At (VA, 1) /= "group/β"
                 or else C2.Element_Group_Label_At (VA, 2) /= ""
                 or else C2.Element_Group_Label_At (VA, 3) /= "0"
                 or else not C2.Is_Single_Group (VA)
               then
                  raise Program_Error with "VA snapshot after C2 Close invalid";
               end if;
               C2.Close (VA);
               C2.Close (VA);
               if C2.Is_Open (VA) then
                  raise Program_Error with "VA not closed";
               end if;
            end;
         end;
      end;
      Expect_VA_Failure (Provider_Path, "c2:va-failure");
      Expect_VA_Failure (Provider_Path, "c2:va-long-failure", True);
      declare
         Automatic : constant C2.C2_MEL := C2.Open (Provider_Path, "c2:ok");
      begin
         if not C2.Is_Open (Automatic) then
            raise Program_Error with "RF C2 automatic owner not open";
         end if;
      end;
      Expect_Open_Failure (Provider_Path, "c2:factory-null");
      Expect_Open_Failure (Provider_Path, "c2:factory-throw");
      Expect_Open_Failure (Provider_Path, "c2:factory-throw-unknown");
      declare
         Throwing : C2.C2_MEL := C2.Open (Provider_Path, "c2:shutdown-throw");
      begin
         begin
            C2.Close (Throwing);
            raise Program_Error with "RF C2 shutdown failure not reported";
         exception
            when AMS.MEL.Provider_Error =>
               if C2.Is_Open (Throwing) then
                  raise Program_Error with "RF C2 throwing close left owner open";
               end if;
         end;
         C2.Close (Throwing);
      end;
      begin
         declare
            Bad : constant C2.C2_MEL :=
              C2.Open (Provider_Path & Character'Val (0) & "tail", "c2:ok");
         begin
            if C2.Is_Open (Bad) then
               raise Program_Error;
            end if;
         end;
         raise Program_Error with "NUL library path accepted";
      exception
         when Constraint_Error =>
            null;
      end;
      begin
         declare
            Bad : constant C2.C2_MEL :=
              C2.Open (Provider_Path, "c2:ok" & Character'Val (0) & "tail");
         begin
            if C2.Is_Open (Bad) then
               raise Program_Error;
            end if;
         end;
         raise Program_Error with "NUL configuration accepted";
      exception
         when Constraint_Error =>
            null;
      end;
   end Run;
end AMS_MEL_RF_C2_Tests;
