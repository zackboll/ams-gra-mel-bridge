with Ada.Command_Line;
with AMS_MEL_RF_Job_V2;

procedure AMS_MEL_RF_Job_V2_Smoke is
begin
   if Ada.Command_Line.Argument_Count /= 1 then
      raise Program_Error with "usage: ams_mel_rf_job_v2_smoke MOCK_PROVIDER";
   end if;
   AMS_MEL_RF_Job_V2.Run (Ada.Command_Line.Argument (1));
end AMS_MEL_RF_Job_V2_Smoke;
