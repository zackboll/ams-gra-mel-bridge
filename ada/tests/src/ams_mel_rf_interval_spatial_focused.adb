with Ada.Command_Line;
with Ada.Environment_Variables;
with AMS_MEL_RF_Interval_Spatial;

procedure AMS_MEL_RF_Interval_Spatial_Focused is
begin
   if Ada.Command_Line.Argument_Count > 1 then
      raise Program_Error with "usage: ams_mel_rf_interval_spatial_focused PROVIDER";
   end if;
   AMS_MEL_RF_Interval_Spatial.Run
     (if Ada.Command_Line.Argument_Count = 1
      then Ada.Command_Line.Argument (1)
      else
        Ada.Environment_Variables.Value ("AMS_MEL_TEST_PROVIDER_DIR") & "/libmock_rf_provider.so");
end AMS_MEL_RF_Interval_Spatial_Focused;
