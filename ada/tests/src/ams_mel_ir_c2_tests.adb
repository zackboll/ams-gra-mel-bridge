with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.C2;

package body AMS_MEL_IR_C2_Tests is
   package C2 renames AMS.MEL.IR.C2;
   use type C2.Error_Code;
   use type C2.MFA_Mode;
   use type C2.Outcome;

   Zero_UUID : constant AMS.MEL.IR.UUID := [others => 0];
   Channel_ID : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Zero_UUID, "Ada IR C2 channel");
   Platform_ID : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Zero_UUID, "Ada platform");
   Location : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location
       (1.25, -2.5, 3.75, "station-1", "mock-aircraft");
   Config : constant C2.Control_Config :=
     C2.Create_Config (Channel_ID, Platform_ID, Location);

   function Long_Rejection return String is
      Result : String (1 .. 613) := [others => 'x'];
   begin
      Result (511) := Character'Val (16#E2#);
      Result (512) := Character'Val (16#82#);
      Result (513) := Character'Val (16#AC#);
      Result (514 .. Result'Last) := [others => 'y'];
      return Result;
   end Long_Rejection;

   procedure Test_Success (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-command-id");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
   begin
      C2.Enable (Channel);
      declare
         Request : C2.Mode_Request :=
           C2.Submit_Operate (Channel, 16#89AB_CDEF#);
         Result : constant C2.Mode_Result := C2.Wait (Request, 1_000);
      begin
         if C2.Status (Result) /= C2.Success
           or else C2.Mode (Result) /= C2.Task_Sched
         then
            raise Program_Error with "Ada C2 success conversion failed";
         end if;
         C2.Close (Request);
         C2.Close (Request);
         if C2.Is_Open (Request) then
            raise Program_Error with "Ada C2 request close did not clear owner";
         end if;
      end;
      C2.Close (Channel);
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Success;

   procedure Test_Timeout_Lifetime (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-delayed");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
   begin
      C2.Enable (Channel);
      declare
         Request : C2.Mode_Request := C2.Submit_Operate (Channel, 7);
      begin
         begin
            declare
               Unexpected : constant C2.Mode_Result := C2.Wait (Request, 0);
            begin
               raise Program_Error with
                 "Ada C2 timeout returned " & C2.Outcome'Image (C2.Status (Unexpected));
            end;
         exception
            when AMS.MEL.IR.Timeout_Error => null;
         end;
         AMS.MEL.Close (Parent);
         C2.Close (Channel);
         declare
            Result : constant C2.Mode_Result := C2.Wait (Request, 1_000);
         begin
            if C2.Mode (Result) /= C2.Task_Sched then
               raise Program_Error with "Ada C2 delayed result failed";
            end if;
         end;
         C2.Close (Request);
      end;
   end Test_Timeout_Lifetime;

   procedure Test_Rejection (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-reject");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
   begin
      C2.Enable (Channel);
      declare
         Request : C2.Mode_Request := C2.Submit_Operate (Channel);
         Result  : constant C2.Mode_Result := C2.Wait (Request, 1_000);
      begin
         if C2.Status (Result) /= C2.Rejected
           or else C2.Rejection_Code (Result) /= C2.Invalid_Parameters
           or else C2.Description (Result) /= "invalid task schedule"
         then
            raise Program_Error with "Ada C2 rejection conversion failed";
         end if;
         C2.Close (Request);
      end;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Rejection;

   procedure Test_Long_Rejection (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open
        (Provider_Path, "c2-reject-long");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
   begin
      C2.Enable (Channel);
      declare
         Request : C2.Mode_Request := C2.Submit_Operate (Channel);
         Result  : constant C2.Mode_Result := C2.Wait (Request, 1_000);
      begin
         if C2.Status (Result) /= C2.Rejected
           or else C2.Rejection_Code (Result) /= C2.Invalid_Parameters
           or else C2.Description (Result) /= Long_Rejection
         then
            raise Program_Error with
              "Ada C2 complete long rejection description was not preserved";
         end if;
         C2.Close (Request);
      end;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Long_Rejection;

   procedure Test_Pending_Finalization (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-lifetime");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
   begin
      C2.Enable (Channel);
      declare
         Request : constant C2.Mode_Request := C2.Submit_Operate (Channel);
      begin
         if not C2.Is_Open (Request) then
            raise Program_Error with "Ada C2 request was not published";
         end if;
      end;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Pending_Finalization;

   procedure Run (Provider_Path : String) is
   begin
      Test_Success (Provider_Path);
      Test_Timeout_Lifetime (Provider_Path);
      Test_Rejection (Provider_Path);
      Test_Long_Rejection (Provider_Path);
      Test_Pending_Finalization (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada IR C2 Operate/TaskSched contract");
   end Run;
end AMS_MEL_IR_C2_Tests;
