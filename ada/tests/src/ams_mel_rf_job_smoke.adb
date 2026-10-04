with Ada.Command_Line;
with Ada.Text_IO;
with AMS_MEL_RF_Job_Tests;
with AMS_MEL_RF_Job_V2;

procedure AMS_MEL_RF_Job_Smoke is
begin
   if Ada.Command_Line.Argument_Count /= 1 then
      raise Program_Error with "usage: ams_mel_rf_job_smoke MOCK_PROVIDER";
   end if;
   AMS_MEL_RF_Job_Tests.Run (Ada.Command_Line.Argument (1));
   AMS_MEL_RF_Job_V2.Run (Ada.Command_Line.Argument (1));
   Ada.Text_IO.Put_Line ("PASS: safe Ada RF Job mock contract");
end AMS_MEL_RF_Job_Smoke;
