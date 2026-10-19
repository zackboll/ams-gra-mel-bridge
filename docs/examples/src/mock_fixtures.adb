with AMS.MEL.IR;
with Interfaces;

package body Mock_Fixtures is
   package C2 renames AMS.MEL.RF.C2;
   function Navigation_Fidelity_Report return AMS.MEL.IR.Image.Navigation_Report
   is ((System_Time_NS               => -123_456_789_012,
        State                        => AMS.MEL.IR.Image.Blended,
        Latitude_Rad                 => 0.523598,
        Longitude_Rad                => -1.308997,
        Altitude_M                   => 987.5,
        Attitude                     => (Roll => 0.1, Pitch => 0.2, Yaw => 0.3),
        Attitude_Rate                =>
          (Value => (Roll => 0.11, Pitch => -0.22, Yaw => 0.33), System_Time_NS => -424_242),
        Speed                        => (North => 10.0, East => -20.0, Down => 30.0),
        Acceleration                 => (North => -1.0, East => 2.0, Down => -3.0),
        Wander_Angle_Rad             => 0.05,
        Magnetic_Heading             => 12.5,
        Altitude_MSL                 => 1000.25,
        Position_Velocity_Covariance =>
          (Position_Position_Pn_Pn => 1.5,
           Position_Position_Pn_Pe => 2.5,
           Position_Position_Pn_Pd => 3.5,
           Position_Position_Pe_Pe => 4.5,
           Position_Position_Pe_Pd => 5.5,
           Position_Position_Pd_Pd => 6.5,
           Position_Velocity_Pn_Vn => 7.5,
           Position_Velocity_Pn_Ve => 8.5,
           Position_Velocity_Pn_Vd => 9.5,
           Position_Velocity_Pe_Ve => 10.5,
           Position_Velocity_Pe_Vd => 11.5,
           Position_Velocity_Pd_Vd => 12.5,
           Velocity_Velocity_Vn_Vn => 13.5,
           Velocity_Velocity_Vn_Ve => 14.5,
           Velocity_Velocity_Vn_Vd => 15.5,
           Velocity_Velocity_Ve_Ve => 16.5,
           Velocity_Velocity_Ve_Vd => 17.5,
           Velocity_Velocity_Vd_Vd => 18.5)));

   function Job_Mock_Aperture_Config return C2.Virtual_Aperture_Config is
      Config               : C2.Virtual_Aperture_Config :=
        C2.Create_Virtual_Aperture_Config (16#FEDC_BA98#, 16#8000_0001#, "definition/β.json");
      First, Second, Third : AMS.MEL.IR.UUID := [others => 0];
   begin
      C2.Append_Local_Function_Info (Config, "alpha");
      C2.Append_Local_Function_Info (Config, "µ-local");
      C2.Append_Local_Function_Info (Config, "");
      First (1) := 16#80#;
      First (2) := 16#FF#;
      Second (0) := 16#FF#;
      Third (15) := 16#80#;
      C2.Append_Capability_ID (Config, AMS.MEL.IR.Create_UCI_ID (First, "first"));
      C2.Append_Capability_ID (Config, AMS.MEL.IR.Create_UCI_ID (Second, ""));
      C2.Append_Capability_ID (Config, AMS.MEL.IR.Create_UCI_ID (Third, "µ-third"));
      return Config;
   end Job_Mock_Aperture_Config;

   function Job_Mock_Config return C2.Job_Config is
      Group : C2.RX_Element_Group_Config :=
        C2.Create_RX_Element_Group ("rx/µ-main", 0.625, "products/β");
   begin
      C2.Append_Expected_Center_Frequency (Group, 1000000.25, 2000000.5);
      C2.Append_Expected_Center_Frequency (Group, 987654321.125, 987654322.875);
      C2.Append_Endpoint_ID (Group, 0);
      C2.Append_Endpoint_ID (Group, 16#8000_0000_0000_0000#);
      C2.Append_Endpoint_ID (Group, Interfaces.Unsigned_64'Last);
      return
         Config : C2.Job_Config :=
           C2.Create_Job_Config (16#FEDC_BA98#, 16#8000_0001#, Group, 16#7FFF_FFFE#, True)
      do
         C2.Append_Instance_Selection (Config, 0);
         C2.Append_Instance_Selection (Config, 42);
         C2.Append_Instance_Selection (Config, Interfaces.Unsigned_32'Last);
      end return;
   end Job_Mock_Config;

end Mock_Fixtures;
