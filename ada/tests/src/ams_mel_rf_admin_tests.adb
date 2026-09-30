with AMS.MEL;
with AMS.MEL.RF.Admin;
with AMS.MEL.Status;

package body AMS_MEL_RF_Admin_Tests is
   package Admin renames AMS.MEL.RF.Admin;
   package Status renames AMS.MEL.Status;

   procedure Expect_Open_Failure (Path, Scenario : String) is
   begin
      declare
         Bad : constant Admin.Admin_MEL := Admin.Open (Path, Scenario);
      begin
         if Admin.Is_Open (Bad) then
            raise Program_Error with "RF Admin factory failure accepted";
         end if;
      end;
      raise Program_Error with "RF Admin factory did not fail";
   exception
      when AMS.MEL.Provider_Error =>
         null;
   end Expect_Open_Failure;

   procedure Expect_Command_Failure (Path, Scenario : String) is
      Object : Admin.Admin_MEL := Admin.Open (Path, Scenario);
   begin
      declare
         Accepted : constant Boolean := Admin.Command_State (Object, Status.Standby);
      begin
         if Accepted then
            raise Program_Error with "RF Admin command unexpectedly accepted";
         end if;
      end;
      raise Program_Error with "RF Admin command did not fail";
   exception
      when AMS.MEL.Provider_Error =>
         Admin.Close (Object);
   end Expect_Command_Failure;

   procedure Run (Provider_Path : String) is
      Object : Admin.Admin_MEL := Admin.Open (Provider_Path, "admin:ok");
   begin
      if not Admin.Is_Open (Object)
        or else not Admin.Command_State (Object, Status.Standby)
        or else not Admin.Command_State (Object, Status.Operate_Rx_Only)
        or else not Admin.Command_State (Object, Status.Degraded)
      then
         raise Program_Error with "RF Admin state not accepted";
      end if;
      Admin.Close (Object);
      Admin.Close (Object);
      if Admin.Is_Open (Object) then
         raise Program_Error with "RF Admin owner not consumed";
      end if;
      declare
         Rejected : constant Admin.Admin_MEL := Admin.Open (Provider_Path, "admin:reject");
      begin
         if Admin.Command_State (Rejected, Status.Operate) then
            raise Program_Error with "RF Admin rejected state reported accepted";
         end if;
      end;
      Expect_Command_Failure (Provider_Path, "admin:command-throw");
      Expect_Command_Failure (Provider_Path, "admin:no-uci");
      Expect_Command_Failure (Provider_Path, "admin:no-status");
      Expect_Open_Failure (Provider_Path, "admin:factory-null");
      Expect_Open_Failure (Provider_Path, "admin:factory-throw");
      Expect_Open_Failure (Provider_Path, "admin:factory-throw-unknown");
      declare
         Throwing : Admin.Admin_MEL := Admin.Open (Provider_Path, "admin:shutdown-throw");
      begin
         begin
            Admin.Close (Throwing);
            raise Program_Error with "RF Admin shutdown failure not reported";
         exception
            when AMS.MEL.Provider_Error =>
               if Admin.Is_Open (Throwing) then
                  raise Program_Error with "RF Admin throwing close left owner open";
               end if;
         end;
         Admin.Close (Throwing);
      end;
      begin
         declare
            Bad : constant Admin.Admin_MEL :=
              Admin.Open (Provider_Path & Character'Val (0) & "tail", "admin:ok");
         begin
            if Admin.Is_Open (Bad) then
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
            Bad : constant Admin.Admin_MEL :=
              Admin.Open (Provider_Path, "admin:ok" & Character'Val (0) & "tail");
         begin
            if Admin.Is_Open (Bad) then
               raise Program_Error;
            end if;
         end;
         raise Program_Error with "NUL configuration accepted";
      exception
         when Constraint_Error =>
            null;
      end;
   end Run;
end AMS_MEL_RF_Admin_Tests;
