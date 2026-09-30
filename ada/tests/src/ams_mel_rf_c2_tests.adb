with AMS.MEL;
with AMS.MEL.RF.C2;

package body AMS_MEL_RF_C2_Tests is
   package C2 renames AMS.MEL.RF.C2;

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
