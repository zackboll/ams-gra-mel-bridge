with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Image;
with Interfaces;

package body AMS_MEL_IR_Image_Tests is
   use type Interfaces.Unsigned_32;
   use type AMS.MEL.IR.Byte;
   use type AMS.MEL.IR.Image_Flip;
   Zero : constant AMS.MEL.IR.UUID := [others => 0];
   Channel : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "full channel");
   Platform : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "full platform");
   Location : constant AMS.MEL.IR.Component_Location := AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "test", "Ada");
   Config : constant AMS.MEL.IR.Image_Config := AMS.MEL.IR.Create_Image_Config (Channel, Platform, Location, Buffer_Count => 3, Buffer_Size => 64, Queue_Capacity => 4);
   procedure Check (Value : AMS.MEL.IR.Image.Full_Frame) is
      use AMS.MEL.IR.Image;
      Sensor : constant Contributing_Sensor := Contributing_Sensor_Value (Value);
      I : constant Sensor_Inertial_State := Sensor_Inertial_State_At (Value, 1);
      N : constant Sensor_Nav_State := Sensor_Nav_State_At (Value, 1);
      N2 : constant Sensor_Nav_State := Sensor_Nav_State_At (Value, 2);
   begin
      if System_Time_NS (Value) /= -123456789 or else Integration_Time_NS (Value) /= 987654321
        or else Width (Value) /= 4 or else Height (Value) /= 3 or else Bits_Per_Pixel (Value) /= 8 or else Number_Of_Bands (Value) /= 1
        or else Horizontal_FOV_Rad (Value) /= 1.25 or else Vertical_FOV_Rad (Value) /= 2.5
        or else Frame_ID (Value) /= 16#FEDCBA98# or else Subframe_ID (Value) /= 17 or else Subframe_Total (Value) /= 19
        or else Image_Type (Value) /= Reserved13 or else Image_Flip (Value) /= AMS.MEL.IR.Both
        or else Dither_Row (Value) /= -0.75 or else Dither_Column (Value) /= 0.625 or else Row_Offset (Value) /= 23 or else Column_Offset (Value) /= 29 or else Band_Index (Value) /= 31
      then raise Program_Error with "Ada full root FrameHeader conversion failed"; end if;
      if AMS.MEL.IR.Offset_X_M (Sensor.Location) /= 1.25 or else AMS.MEL.IR.Offset_Y_M (Sensor.Location) /= -2.5 or else AMS.MEL.IR.Offset_Z_M (Sensor.Location) /= 3.75
        or else AMS.MEL.IR.Key (Sensor.Location) /= "face-α" or else AMS.MEL.IR.System_Name (Sensor.Location) /= "system-€" or else Sensor.Sensor_ID /= 16#F1234567#
      then raise Program_Error with "Ada contributing sensor conversion failed"; end if;
      if Image_Flag_Count (Value) /= 4 or else Image_Flag_At (Value, 1) /= Stare_Snapshot or else Image_Flag_At (Value, 2) /= Scan_First or else Image_Flag_At (Value, 3) /= Stare_Snapshot or else Image_Flag_At (Value, 4) /= Scan_Last
      then raise Program_Error with "Ada ordered flags conversion failed"; end if;
      if Sensor_Inertial_State_Count (Value) /= 2 or else I.System_Time_NS /= -101 or else I.Q_XYZW.X /= 1.0 or else I.Q_XYZW.W /= 4.0 or else I.Q_ECEF_XYZW.X /= 5.0 or else I.Sensor_Position.Z /= 11.0 or else I.Sensor_Velocity.Y /= 13.0 or else I.Uncertainty.Sensor_Uncertainties /= 16#81234567# or else I.Uncertainty.Platform_Uncertainties /= 16#FEDCBA98#
      then raise Program_Error with "Ada inertial conversion failed"; end if;
      if Sensor_Nav_State_Count (Value) /= 2 or else N.Coordinate_System /= NED_Platform or else N.Position.X /= 31.0 or else N.Position_Error.W /= 37.0 or else N.Velocity_Error.Z /= 43.0 or else N.Acceleration_Error.Y /= 49.0
      then raise Program_Error with "Ada nav scalar conversion failed"; end if;
      if N.Orientation.Kind /= Euler_Orientation or else N.Orientation.Euler_Value.Roll /= 52.0 or else N.Orientation_Velocity.Kind /= Quaternion_Orientation or else N.Orientation_Velocity.Quaternion_Value.W /= 62.0 or else N.Orientation_Acceleration.Kind /= Euler_Orientation or else N.Orientation_Acceleration.Euler_Value.Yaw /= 69.0
      then raise Program_Error with "Ada first mixed orientation conversion failed"; end if;
      if N2.Coordinate_System /= NED_Sensor or else N2.Orientation.Kind /= Quaternion_Orientation or else N2.Orientation.Quaternion_Value.X /= 95.0 or else N2.Orientation_Velocity.Kind /= Euler_Orientation or else N2.Orientation_Velocity.Euler_Value.Pitch /= 104.0 or else N2.Orientation_Acceleration.Kind /= Quaternion_Orientation or else N2.Orientation_Acceleration.Quaternion_Value.Z /= 112.0 or else N2.Orientation_Acceleration_Error.W /= 117.0
      then raise Program_Error with "Ada second mixed orientation conversion failed"; end if;
      if Pixel_Count (Value) /= 12 or else Pixels (Value) (1) /= 16#A0# or else Pixels (Value) (12) /= 16#AB# then raise Program_Error with "Ada pixel conversion failed"; end if;
   end Check;
   procedure Run (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "full-frame-rich");
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
   begin
      AMS.MEL.IR.Start (Stream);
      declare Value : constant AMS.MEL.IR.Image.Full_Frame := AMS.MEL.IR.Image.Receive (Stream, 1_000); begin
         AMS.MEL.Close (Parent); AMS.MEL.IR.Close (Stream); Check (Value);
      end;
      Ada.Text_IO.Put_Line ("PASS: Ada full IR FrameHeader contract");
   end Run;
end AMS_MEL_IR_Image_Tests;
