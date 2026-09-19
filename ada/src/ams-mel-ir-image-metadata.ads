with Ada.Containers.Vectors;
private with Ada.Finalization;
with Interfaces;
with AMS.MEL.Status;
private with AMS.MEL_C_API;

package AMS.MEL.IR.Image.Metadata is
   type Metadata_Kind is
     (Bad_Pixel_List_Event, Line_Of_Sight_Report_Event, Line_Of_Sight_Euler_Event);
   type Bad_Pixel_Reason is (Unknown);
   type Bad_Pixel is record
      Row    : Interfaces.Unsigned_32;
      Column : Interfaces.Unsigned_32;
      Reason : Bad_Pixel_Reason;
   end record;
   type Bad_Pixel_List is private;
   type Azimuth_Elevation is record
      Azimuth_Rad, Elevation_Rad : Long_Float;
   end record;
   type Line_Of_Sight_Report is record
      System_Time_NS         : Long_Long_Integer;
      Pointing_Angle         : Azimuth_Elevation;
      Pointing_Angle_Rates   : Azimuth_Elevation;
      At_Speed               : Boolean;
      In_Tolerance           : Boolean;
      Platform_Attitude      : AMS.MEL.Status.Euler;
      Validity_Flag_Bitfield : Interfaces.Unsigned_32;
      Image_Rotation_Rad     : Long_Float;
   end record;
   type Line_Of_Sight_Euler is record
      System_Time_NS : Long_Long_Integer;
      Attitude       : AMS.MEL.Status.Euler;
      Attitude_Rates : AMS.MEL.Status.Euler;
   end record;
   function Reported_Size (Value : Bad_Pixel_List) return Interfaces.Unsigned_32;
   function Reported_Count (Value : Bad_Pixel_List) return Interfaces.Unsigned_32;
   function Pixel_Count (Value : Bad_Pixel_List) return Natural;
   function Pixel_At (Value : Bad_Pixel_List; Index : Positive) return Bad_Pixel;
   type Metadata_Counters is record
      Events_Received           : AMS.MEL.IR.Counter;
      Events_Dropped_Queue_Full : AMS.MEL.IR.Counter;
      Malformed_Or_Unsupported  : AMS.MEL.IR.Counter;
   end record;
   type Metadata_Stream is limited private;
   type Metadata_Event is private;
   function Open
     (Stream : AMS.MEL.IR.Image_Stream; Queue_Capacity : Positive) return Metadata_Stream;
   function Receive
     (Stream : Metadata_Stream; Timeout_Milliseconds : Natural := 0) return Metadata_Event;
   function Counters (Stream : Metadata_Stream) return Metadata_Counters;
   procedure Close (Stream : in out Metadata_Stream);
   function Kind (Event : Metadata_Event) return Metadata_Kind;
   function Bad_Pixel_List_Value (Event : Metadata_Event) return Bad_Pixel_List;
   function Line_Of_Sight_Report_Value (Event : Metadata_Event) return Line_Of_Sight_Report;
   function Line_Of_Sight_Euler_Value (Event : Metadata_Event) return Line_Of_Sight_Euler;
private
   package Pixel_Vectors is new Ada.Containers.Vectors (Positive, Bad_Pixel);
   type Bad_Pixel_List is record
      Size, Count : Interfaces.Unsigned_32;
      Pixels      : Pixel_Vectors.Vector;
   end record;
   type Metadata_Stream is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Image_Metadata_Handle := AMS.MEL_C_API.Null_Image_Metadata;
   end record;
   overriding
   procedure Finalize (Stream : in out Metadata_Stream);
   type Metadata_Event (Kind_Value : Metadata_Kind := Bad_Pixel_List_Event) is record
      case Kind_Value is
         when Bad_Pixel_List_Event =>
            Bad_Pixel_Value : Bad_Pixel_List;

         when Line_Of_Sight_Report_Event =>
            Report_Value : Line_Of_Sight_Report;

         when Line_Of_Sight_Euler_Event =>
            Euler_Value : Line_Of_Sight_Euler;
      end case;
   end record;
end AMS.MEL.IR.Image.Metadata;
