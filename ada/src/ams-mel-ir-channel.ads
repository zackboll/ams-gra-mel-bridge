with Ada.Containers.Vectors;
with AMS.MEL.IR.C2;
with Interfaces;

package AMS.MEL.IR.Channel is
   type Comms_Channel_ID is mod 2**32 with Size => 32;
   type Comms_Request_ID is mod 2**32 with Size => 32;
   type Comms_Test_Report is record
      Command_ID : C2.Command_ID;
      Request_ID : Comms_Request_ID;
   end record;

   type Channel_Type is
     (IRST_Track,
      IRST_Image,
      Command_And_Control,
      Scheduling,
      Health_And_Status,
      Instrumentation,
      Stacked_Image,
      Reserved_1,
      Reserved_2);
   type Pixel_Format is (Mono, RGB, Bayer);
   type Sensor_Type is
     (Unspecified, Gimbal_Horizontal, Gimbal_Vertical, Gimbal_Rotation, Step_Stare);
   type Metadata_Capability is
     (Bad_Pixel_List,
      Optical_Distortion_Map,
      LF_Status,
      Line_Of_Sight_Report,
      Line_Of_Sight_Quaternion,
      Line_Of_Sight_Euler,
      MFA_Status,
      MFA_Status_Detailed,
      BIT_Configuration,
      Command_Status,
      BIT_Status,
      Candidate_Object_Message,
      Task_Executing_Rep,
      Subsystem_Status_Resp,
      Execute_Task_Ack,
      Sched_Created_Rep,
      IRST_Track_Report,
      Channel_Comms_Test_Rep,
      Camera_Command_Resp,
      Camera_Protect_Cmd_Resp,
      Instrumentation_Report,
      Navigation_Report_Resp,
      Request_System_Track_Data,
      Update_Track_List_Response,
      LOS_3D_Kinematics_Type,
      Candidate_Object_Preproc_Message,
      Task_Events,
      Scan_Performance_Report,
      Reserved_3,
      Reserved_5,
      Reserved_9,
      Reserved_10);
   type Band_Type is
     (Invalid,
      Multiband,
      IR_Far,
      IR_Near,
      IR_Longwave,
      IR_Midwave,
      IR_Shortwave,
      Visible_White,
      Visible_Red,
      Visible_Green,
      Visible_Blue,
      UVA,
      UVB,
      UVC,
      UV_Vacuum);
   type Coordinate_System_Type is (LLA, ECEF, NED_Platform, NED_Sensor);
   type Band_Info is record
      Kind                               : Band_Type;
      Min_Wavelength_M, Max_Wavelength_M : Long_Float;
   end record;

   package Sensor_Type_Vectors is new Ada.Containers.Vectors (Positive, Sensor_Type);
   package Channel_Type_Vectors is new Ada.Containers.Vectors (Positive, Channel_Type);
   package Metadata_Vectors is new Ada.Containers.Vectors (Positive, Metadata_Capability);
   package Band_Info_Vectors is new Ada.Containers.Vectors (Positive, Band_Info);
   type Image_Band is record
      Band_Index : Interfaces.Unsigned_32;
      Bands      : Band_Info_Vectors.Vector;
   end record;
   package Image_Band_Vectors is new Ada.Containers.Vectors (Positive, Image_Band);
   package Coordinate_Vectors is new Ada.Containers.Vectors (Positive, Coordinate_System_Type);

   type Channel_Capability is private;
   function Create_Capability
     (Channel_ID                                                                    : UCI_ID;
      Height, Width, Bit_Depth, Row_Pitch, Buffer_Size, Image_Size, Number_Of_Bands :
        Interfaces.Unsigned_32;
      Format                                                                        : Pixel_Format;
      Sensor_Types                                                                  :
        Sensor_Type_Vectors.Vector;
      Platform_ID                                                                   : UCI_ID;
      Sensor_Location                                                               :
        Component_Location;
      Channel_Types                                                                 :
        Channel_Type_Vectors.Vector;
      Task_Schedule_Depth                                                           :
        Interfaces.Unsigned_32;
      ODC_Available, NUC_Available                                                  : Boolean;
      Metadata                                                                      :
        Metadata_Vectors.Vector;
      Image_Bands                                                                   :
        Image_Band_Vectors.Vector;
      Nav_Frames                                                                    :
        Coordinate_Vectors.Vector) return Channel_Capability;

   function Channel_ID (Value : Channel_Capability) return UCI_ID;
   function Height (Value : Channel_Capability) return Interfaces.Unsigned_32;
   function Width (Value : Channel_Capability) return Interfaces.Unsigned_32;
   function Bit_Depth (Value : Channel_Capability) return Interfaces.Unsigned_32;
   function Row_Pitch (Value : Channel_Capability) return Interfaces.Unsigned_32;
   function Buffer_Size (Value : Channel_Capability) return Interfaces.Unsigned_32;
   function Image_Size (Value : Channel_Capability) return Interfaces.Unsigned_32;
   function Number_Of_Bands (Value : Channel_Capability) return Interfaces.Unsigned_32;
   function Format (Value : Channel_Capability) return Pixel_Format;
   function Sensor_Type_Count (Value : Channel_Capability) return Natural;
   function Sensor_Type_At (Value : Channel_Capability; Index : Positive) return Sensor_Type;
   function Platform_ID (Value : Channel_Capability) return UCI_ID;
   function Sensor_Location (Value : Channel_Capability) return Component_Location;
   function Channel_Type_Count (Value : Channel_Capability) return Natural;
   function Channel_Type_At (Value : Channel_Capability; Index : Positive) return Channel_Type;
   function Task_Schedule_Depth (Value : Channel_Capability) return Interfaces.Unsigned_32;
   function ODC_Available (Value : Channel_Capability) return Boolean;
   function NUC_Available (Value : Channel_Capability) return Boolean;
   function Metadata_Capability_Count (Value : Channel_Capability) return Natural;
   function Metadata_Capability_At
     (Value : Channel_Capability; Index : Positive) return Metadata_Capability;
   function Has_Metadata_Capability
     (Value : Channel_Capability; Item : Metadata_Capability) return Boolean;
   function Image_Band_Count (Value : Channel_Capability) return Natural;
   function Image_Band_Index_At
     (Value : Channel_Capability; Index : Positive) return Interfaces.Unsigned_32;
   function Image_Band_Info_Count (Value : Channel_Capability; Index : Positive) return Natural;
   function Image_Band_Info_At
     (Value : Channel_Capability; Band_Index, Info_Index : Positive) return Band_Info;
   function Nav_Frame_Count (Value : Channel_Capability) return Natural;
   function Nav_Frame_At
     (Value : Channel_Capability; Index : Positive) return Coordinate_System_Type;
private
   type Channel_Capability is record
      ID, Platform                                            : UCI_ID;
      H, W, Depth, Pitch, Buffer, Image, Band_Count, Schedule : Interfaces.Unsigned_32;
      Pixel                                                   : Pixel_Format := Mono;
      Sensors                                                 : Sensor_Type_Vectors.Vector;
      Location                                                : Component_Location;
      Channels                                                : Channel_Type_Vectors.Vector;
      ODC, NUC                                                : Boolean := False;
      Metadata                                                : Metadata_Vectors.Vector;
      Bands                                                   : Image_Band_Vectors.Vector;
      Frames                                                  : Coordinate_Vectors.Vector;
   end record;
end AMS.MEL.IR.Channel;
