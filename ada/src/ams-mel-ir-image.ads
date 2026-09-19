with Ada.Containers.Vectors;
with AMS.MEL.IR.Channel;
with AMS.MEL.Status;
with Interfaces;

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
end AMS.MEL.IR.Image;
