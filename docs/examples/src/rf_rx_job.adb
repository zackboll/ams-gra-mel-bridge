with Ada.Text_IO;
with AMS.MEL.RF.C2;
with Example_Support;
with Mock_Fixtures;
with Interfaces;

procedure RF_RX_Job is
   package C2 renames AMS.MEL.RF.C2;
   use Example_Support;
   use type C2.Request_Outcome;
   use type C2.Job_Status;
   use type Interfaces.Unsigned_32;
begin
   declare
      Control         : C2.C2_MEL := C2.Open (Provider_Path, Configuration);
      Aperture_Config : constant C2.Virtual_Aperture_Config :=
        Mock_Fixtures.Job_Mock_Aperture_Config;
      Creation        : C2.Virtual_Aperture_Request :=
        C2.Submit_Virtual_Aperture (Control, Aperture_Config);
      Created         : constant C2.Virtual_Aperture_Result := C2.Wait (Creation, 1_000);
   begin
      if C2.Outcome (Created) /= C2.Created then
         raise Program_Error with "aperture creation: " & C2.Description (Created);
      end if;
      declare
         Aperture : C2.Virtual_Aperture := C2.Claim (Creation);
         Config   : constant C2.Job_Config := Mock_Fixtures.Job_Mock_Config;
      begin
         C2.Close (Creation);
         --  Control still admits new children when Submit_Job is called.
         declare
            Request : C2.Job_Request := C2.Submit_Job (Aperture, Config);
            Result  : constant C2.Job_Result := C2.Wait (Request, 1_000);
         begin
            if C2.Outcome (Result) /= C2.Created then
               raise Program_Error with "Job creation: " & C2.Description (Result);
            end if;
            declare
               Job       : C2.Job := C2.Claim (Request);
               Interval  : C2.RX_Job_Interval_Config :=
                 C2.Create_RX_Job_Interval
                   (Interval_ID                    => 1,
                    Sequence_Duration_Femtoseconds => 1_000_000_000,
                    Job_Details_ID                 => C2.Job_Details_ID (Job));
               Intervals : C2.RX_Job_Interval_List;
            begin
               C2.Close (Request);
               Require (C2.Job_Details_ID (Job) = 16#1020_3040#, "claimed mock Job identity");
               C2.Append_RX_Event
                 (Interval,
                  C2.Create_RX_Receive_Event
                    (1, "rx/µ-main", 0, 1_000_000_000, 1_500_000.0, 1_000_000.0));
               C2.Append_Job_Interval (Intervals, Interval);
               C2.Add_RX_Job_Intervals (Job, Intervals);
               C2.Finalize_Job (Job);
               declare
                  Status : constant C2.Job_Status := C2.Wait_Job_Status (Job, 1_000);
               begin
                  Ada.Text_IO.Put_Line ("Finalize future status: " & Status'Image);
                  Require (Status = C2.Complete, "mock Finalize status");
               end;
               C2.Close (Job);
            end;
         end;
         C2.Close (Aperture);
      end;
      C2.Close (Control);
      Ada.Text_IO.Put_Line
        ("claimed once each; Job_Details_ID=0x10203040; finite RX interval added");
   end;
exception
   when Error : others =>
      Report_Failure (Error);
end RF_RX_Job;
