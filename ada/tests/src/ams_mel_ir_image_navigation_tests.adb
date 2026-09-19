with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Image;
with Interfaces;

package body AMS_MEL_IR_Image_Navigation_Tests is
   package Image renames AMS.MEL.IR.Image;
   use type Image.Navigation_Outcome;
   use type Image.Navigation_Error_Code;
   use type Interfaces.Unsigned_32;

   Zero       : constant AMS.MEL.IR.UUID := [others => 0];
   Channel_ID : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Zero, "Image navigation channel");
   Platform   : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Zero, "Image navigation platform");
   Location   : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock");
   Config     : constant AMS.MEL.IR.Image_Config :=
     AMS.MEL.IR.Create_Image_Config
       (Channel_ID, Platform, Location, Buffer_Count => 2, Buffer_Size => 64, Queue_Capacity => 2);

   function Rich_Report return Image.Navigation_Report
   is ((System_Time_NS               => -123_456_789_012,
        State                        => Image.Blended,
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

   procedure Test_Success_And_Fidelity (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "navigation-fidelity");
      Stream  : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Request : Image.Navigation_Request := Image.Submit_Navigation_Report (Stream, Rich_Report);
      Result  : constant Image.Navigation_Result := Image.Wait (Request, 1_000);
      Value   : constant Image.Navigation_Response := Image.Response (Result);
   begin
      if Image.Status (Result) /= Image.Success
        or else Value.System_Time_NS /= -8_765_432_109
        or else Value.Command_ID /= 16#F123_4567#
        or else Value.Request_ID /= 16#89AB_CDEF#
      then
         raise Program_Error with "Ada NavigationReport success/fidelity failed";
      end if;
      Image.Close (Request);
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Test_Success_And_Fidelity;

   procedure Test_Rejection (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "navigation-reject");
      Stream  : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Request : Image.Navigation_Request := Image.Submit_Navigation_Report (Stream, Rich_Report);
      Result  : constant Image.Navigation_Result := Image.Wait (Request, 1_000);
   begin
      if Image.Status (Result) /= Image.Rejected
        or else Image.Rejection_Code (Result) /= Image.Invalid_Parameters
        or else Image.Description (Result) /= "invalid navigation report"
      then
         raise Program_Error with "Ada NavigationReport rejection mapping failed";
      end if;
      Image.Close (Request);
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Test_Rejection;

   procedure Test_Timeout_Then_Success (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "navigation-delayed");
      Stream  : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Request : Image.Navigation_Request := Image.Submit_Navigation_Report (Stream, Rich_Report);
   begin
      begin
         declare
            Ignored : constant Image.Navigation_Result := Image.Wait (Request, 0);
         begin
            null;
         end;
         raise Program_Error with "Ada NavigationReport Wait(0) did not time out";
      exception
         when AMS.MEL.IR.Timeout_Error =>
            null;
      end;
      declare
         First  : constant Image.Navigation_Result := Image.Wait (Request, 1_000);
         Second : constant Image.Navigation_Result := Image.Wait (Request, 0);
      begin
         if Image.Status (First) /= Image.Success
           or else Image.Status (Second) /= Image.Success
           or else Image.Response (First).Command_ID /= Image.Response (Second).Command_ID
         then
            raise Program_Error with "Ada NavigationReport cached Wait mismatch";
         end if;
      end;
      Image.Close (Request);
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Test_Timeout_Then_Success;

   procedure Test_Session_Close_First (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "navigation-lifetime");
      Stream  : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Request : Image.Navigation_Request := Image.Submit_Navigation_Report (Stream, Rich_Report);
   begin
      AMS.MEL.Close (Parent);
      AMS.MEL.IR.Close (Stream);
      declare
         Result : constant Image.Navigation_Result := Image.Wait (Request, 1_000);
      begin
         if Image.Status (Result) /= Image.Success then
            raise Program_Error with "Ada NavigationReport parent-first close failed";
         end if;
      end;
      Image.Close (Request);
   end Test_Session_Close_First;

   procedure Run (Provider_Path : String) is
   begin
      Test_Success_And_Fidelity (Provider_Path);
      Test_Rejection (Provider_Path);
      Test_Timeout_Then_Success (Provider_Path);
      Test_Session_Close_First (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada IR Image navigation request contract");
   end Run;
end AMS_MEL_IR_Image_Navigation_Tests;
