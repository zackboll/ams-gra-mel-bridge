with Ada.Containers.Vectors;
private with Ada.Finalization;
private with Ada.Strings.Unbounded;
with AMS.MEL.IR.Channel;
with AMS.MEL.Status;
with Interfaces;
private with AMS.MEL_C_API;

package AMS.MEL.IR.Image is
   use type Interfaces.Unsigned_8;
   type Full_Image_Type is (Staring, Scanning, Reserved13);
   type Image_Flag is (Scan_First, Scan_Last, Stare_Snapshot, Stare_Rolling);
   type Directional is record
      X, Y, Z : Long_Float;
   end record;
   type Quaternion is record
      X, Y, Z, W : Long_Float;
   end record;
   type Nav_Error is record
      X, Y, Z, W : Long_Float;
   end record;
   type Uncertainty is record
      Sensor_Uncertainties, Platform_Uncertainties : Interfaces.Unsigned_32;
   end record;
   type Contributing_Sensor is record
      Location  : AMS.MEL.IR.Component_Location;
      Sensor_ID : Interfaces.Unsigned_32;
   end record;
   type Orientation_Kind is (Euler_Orientation, Quaternion_Orientation);
   type Orientation (Kind : Orientation_Kind := Euler_Orientation) is record
      case Kind is
         when Euler_Orientation =>
            Euler_Value : AMS.MEL.Status.Euler;

         when Quaternion_Orientation =>
            Quaternion_Value : Quaternion;
      end case;
   end record;
   type Sensor_Inertial_State is record
      System_Time_NS                   : Long_Long_Integer;
      Q_XYZW, Q_ECEF_XYZW              : Quaternion;
      Sensor_Position, Sensor_Velocity : Directional;
      Uncertainty                      : Image.Uncertainty;
   end record;
   type Coordinate_System is (LLA, ECEF, NED_Platform, NED_Sensor);
   type Sensor_Nav_State is record
      Position, Velocity, Acceleration                   : Directional;
      Position_Error, Velocity_Error, Acceleration_Error : Nav_Error;
      Orientation                                        : Image.Orientation;
      Orientation_Error                                  : Nav_Error;
      Orientation_Velocity                               : Image.Orientation;
      Orientation_Velocity_Error                         : Nav_Error;
      Orientation_Acceleration                           : Image.Orientation;
      Orientation_Acceleration_Error                     : Nav_Error;
      Coordinate_System                                  : Image.Coordinate_System;
   end record;

   type Full_Frame is private;
   function Capabilities
     (Object : AMS.MEL.IR.Image_Stream) return AMS.MEL.IR.Channel.Channel_Capability;
   function Receive
     (Object : AMS.MEL.IR.Image_Stream; Timeout_Milliseconds : Natural := 0) return Full_Frame;
   function System_Time_NS (Value : Full_Frame) return Long_Long_Integer;
   function Integration_Time_NS (Value : Full_Frame) return Long_Long_Integer;
   function Width (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Height (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Bits_Per_Pixel (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Number_Of_Bands (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Horizontal_FOV_Rad (Value : Full_Frame) return Long_Float;
   function Vertical_FOV_Rad (Value : Full_Frame) return Long_Float;
   function Contributing_Sensor_Value (Value : Full_Frame) return Contributing_Sensor;
   function Pixel_Format (Value : Full_Frame) return AMS.MEL.IR.Channel.Pixel_Format;
   function Frame_ID (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Subframe_ID (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Subframe_Total (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Image_Type (Value : Full_Frame) return Full_Image_Type;
   function Image_Flip (Value : Full_Frame) return AMS.MEL.IR.Image_Flip;
   function Image_Flag_Count (Value : Full_Frame) return Natural;
   function Image_Flag_At (Value : Full_Frame; Index : Positive) return Image_Flag;
   function Dither_Row (Value : Full_Frame) return Long_Float;
   function Dither_Column (Value : Full_Frame) return Long_Float;
   function Row_Offset (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Column_Offset (Value : Full_Frame) return Interfaces.Unsigned_32;
   function Sensor_Inertial_State_Count (Value : Full_Frame) return Natural;
   function Sensor_Inertial_State_At
     (Value : Full_Frame; Index : Positive) return Sensor_Inertial_State;
   function Sensor_Nav_State_Count (Value : Full_Frame) return Natural;
   function Sensor_Nav_State_At (Value : Full_Frame; Index : Positive) return Sensor_Nav_State;
   function Band_Index (Value : Full_Frame) return AMS.MEL.IR.Byte;
   function Pixel_Count (Value : Full_Frame) return Natural;
   function Pixels (Value : Full_Frame) return AMS.MEL.IR.Pixel_Array;

   --  Complete published mel::PositionSolutionState. MaxExclusive is not a
   --  valid safe value.
   type Position_Solution_State is (Not_Set, Aligning, Free_Inertial, GPS, Blended);
   for Position_Solution_State use
     (Not_Set => 0, Aligning => 1, Free_Inertial => 2, GPS => 3, Blended => 4);

   type North_East_Down is record
      North, East, Down : Long_Float;
   end record;

   type Attitude_Rate is record
      Value          : AMS.MEL.Status.Euler;
      System_Time_NS : Long_Long_Integer;
   end record;

   --  Complete published mel::PositionVelocityCovariance. All 18 terms are
   --  named explicitly to avoid matrix-order mistakes.
   type Position_Velocity_Covariance is record
      Position_Position_Pn_Pn : Long_Float;
      Position_Position_Pn_Pe : Long_Float;
      Position_Position_Pn_Pd : Long_Float;
      Position_Position_Pe_Pe : Long_Float;
      Position_Position_Pe_Pd : Long_Float;
      Position_Position_Pd_Pd : Long_Float;
      Position_Velocity_Pn_Vn : Long_Float;
      Position_Velocity_Pn_Ve : Long_Float;
      Position_Velocity_Pn_Vd : Long_Float;
      Position_Velocity_Pe_Ve : Long_Float;
      Position_Velocity_Pe_Vd : Long_Float;
      Position_Velocity_Pd_Vd : Long_Float;
      Velocity_Velocity_Vn_Vn : Long_Float;
      Velocity_Velocity_Vn_Ve : Long_Float;
      Velocity_Velocity_Vn_Vd : Long_Float;
      Velocity_Velocity_Ve_Ve : Long_Float;
      Velocity_Velocity_Ve_Vd : Long_Float;
      Velocity_Velocity_Vd_Vd : Long_Float;
   end record;

   --  Complete published mel::NavigationReport.
   type Navigation_Report is record
      System_Time_NS               : Long_Long_Integer;
      State                        : Position_Solution_State;
      Latitude_Rad                 : Long_Float;
      Longitude_Rad                : Long_Float;
      Altitude_M                   : Long_Float;
      Attitude                     : AMS.MEL.Status.Euler;
      Attitude_Rate                : Image.Attitude_Rate;
      Speed                        : North_East_Down;
      Acceleration                 : North_East_Down;
      Wander_Angle_Rad             : Long_Float;
      Magnetic_Heading             : Long_Float;
      Altitude_MSL                 : Long_Float;
      Position_Velocity_Covariance : Image.Position_Velocity_Covariance;
   end record;

   --  Canonical safe Navigation_Response; see AMS.MEL.IR.Image.Metadata for
   --  the source-compatible subtype used by the metadata callback path.
   type Navigation_Response is record
      System_Time_NS : Long_Long_Integer;
      Command_ID     : Interfaces.Unsigned_32;
      Request_ID     : Interfaces.Unsigned_32;
   end record;

   type Navigation_Outcome is (Success, Rejected);
   type Navigation_Error_Code is
     (None,
      Invalid_ID,
      Invalid_State,
      Invalid_Parameters,
      Insufficient_Permissions,
      Insufficient_Resources,
      Insufficient_Local_Resources,
      Insufficient_Remote_Resources,
      Unsupported);
   type Navigation_Result is private;
   function Status (Result : Navigation_Result) return Navigation_Outcome;
   function Response (Result : Navigation_Result) return Navigation_Response
   with Pre => Status (Result) = Success;
   function Rejection_Code (Result : Navigation_Result) return Navigation_Error_Code
   with Pre => Status (Result) = Rejected;
   function Description (Result : Navigation_Result) return String
   with Pre => Status (Result) = Rejected;

   --  Navigation submission is valid while the stream is logically Attached
   --  or Running; a prior Start is not required. Close is not cancellation:
   --  a pending request keeps the underlying provider future/channel alive
   --  independently of the public Image_Stream and Session owners.
   type Navigation_Request is limited private;
   function Submit_Navigation_Report
     (Object : AMS.MEL.IR.Image_Stream; Report : Navigation_Report) return Navigation_Request;
   function Is_Open (Request : Navigation_Request) return Boolean;
   --  Timeout_Error is inherited from AMS.MEL.IR. Timeout never cancels or
   --  consumes the request. Wait may be repeated and a later Wait may return
   --  its cached terminal result. Close must not race Wait on the same
   --  Navigation_Request.
   function Wait
     (Request : Navigation_Request; Timeout_Milliseconds : Natural) return Navigation_Result;
   procedure Close (Request : in out Navigation_Request);
private
   package Flag_Vectors is new Ada.Containers.Vectors (Positive, Image_Flag);
   package Inertial_Vectors is new Ada.Containers.Vectors (Positive, Sensor_Inertial_State);
   package Nav_Vectors is new Ada.Containers.Vectors (Positive, Sensor_Nav_State);
   package Pixel_Vectors is new Ada.Containers.Vectors (Positive, AMS.MEL.IR.Byte);
   type Full_Frame is record
      Time, Integration     : Long_Long_Integer;
      W, H, BPP, Bands      : Interfaces.Unsigned_32;
      HFOV, VFOV            : Long_Float;
      Sensor                : Contributing_Sensor;
      Format                : AMS.MEL.IR.Channel.Pixel_Format;
      ID, Sub_ID, Sub_Total : Interfaces.Unsigned_32;
      Kind                  : Full_Image_Type;
      Flip                  : AMS.MEL.IR.Image_Flip;
      Flags                 : Flag_Vectors.Vector;
      D_Row, D_Column       : Long_Float;
      Row, Column           : Interfaces.Unsigned_32;
      Inertial              : Inertial_Vectors.Vector;
      Nav                   : Nav_Vectors.Vector;
      Band                  : AMS.MEL.IR.Byte;
      Data                  : Pixel_Vectors.Vector;
   end record;

   package US renames Ada.Strings.Unbounded;
   type Navigation_Result is record
      Result_Status   : Navigation_Outcome := Success;
      Result_Response : Navigation_Response := (0, 0, 0);
      Result_Code     : Navigation_Error_Code := None;
      Result_Text     : US.Unbounded_String;
   end record;
   type Navigation_Request_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Navigation_Request_Handle :=
        AMS.MEL_C_API.Null_Navigation_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Navigation_Request_Owner);
   type Navigation_Request is limited record
      Owner : Navigation_Request_Owner;
   end record;
end AMS.MEL.IR.Image;
