with Ada.Command_Line;
with AMS_MEL_RF_Job_Pointing;

procedure AMS_MEL_RF_Pointing_Focused is
begin
   if Ada.Command_Line.Argument_Count /= 1 then
      raise Program_Error with "usage: ams_mel_rf_pointing_focused PROVIDER";
   end if;
   AMS_MEL_RF_Job_Pointing.Run (Ada.Command_Line.Argument (1));
end AMS_MEL_RF_Pointing_Focused;
