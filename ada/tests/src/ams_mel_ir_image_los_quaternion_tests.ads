--  IRMEL-ADA-010 safe Ada Image LineOfSightQuaternion contract, deterministic mock only.

package AMS_MEL_IR_Image_LOS_Quaternion_Tests is
   procedure Run (Provider_Path : String);
   --  All in-process cases. The isolated in-flight teardown case is native-only because it
   --  needs a deliberately non-quiescing provider thread joined only at library unload.
end AMS_MEL_IR_Image_LOS_Quaternion_Tests;
