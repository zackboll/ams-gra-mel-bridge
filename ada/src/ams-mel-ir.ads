with Interfaces;
private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;

package AMS.MEL.IR is
   subtype Byte is Interfaces.Unsigned_8;
   type UUID is array (Natural range 0 .. 15) of Byte;
   type Pixel_Array is array (Natural range <>) of Byte;

   type UCI_ID is private;
   function Create_UCI_ID (Value : UUID; Descriptive_Label : String) return UCI_ID;
   function UUID_Value (ID : UCI_ID) return UUID;
   function Descriptive_Label (ID : UCI_ID) return String;

   type Component_Location is private;
   function Create_Component_Location
     (Offset_X_M : Long_Float;
      Offset_Y_M : Long_Float;
      Offset_Z_M : Long_Float;
      Key         : String;
      System_Name : String) return Component_Location;

   type Image_Config is private;
   function Create_Image_Config
     (Channel_ID      : UCI_ID;
      Platform_ID     : UCI_ID;
      Sensor_Location : Component_Location;
      Buffer_Count    : Positive := 3;
      Buffer_Size     : Positive := 1_048_576;
      Queue_Capacity  : Positive := 4) return Image_Config;

   type Image_Stream is limited private;
   function Open_Image_Stream
     (Parent : Session; Config : Image_Config) return Image_Stream;
   function Is_Open (Object : Image_Stream) return Boolean;
   procedure Start (Object : in out Image_Stream);

   type Image_Type is (Staring, Scanning);
   type Image_Flip is (No_Flip, Vertical, Horizontal, Both);
   type Frame (Pixel_Count : Natural) is record
      System_Time_NS      : Long_Long_Integer;
      Integration_Time_NS : Long_Long_Integer;
      Width               : Interfaces.Unsigned_32;
      Height              : Interfaces.Unsigned_32;
      Bits_Per_Pixel      : Interfaces.Unsigned_32;
      Number_Of_Bands     : Interfaces.Unsigned_32;
      Horizontal_FOV_Rad  : Long_Float;
      Vertical_FOV_Rad    : Long_Float;
      Frame_ID            : Interfaces.Unsigned_32;
      Subframe_ID         : Interfaces.Unsigned_32;
      Subframe_Total      : Interfaces.Unsigned_32;
      Kind                : Image_Type;
      Flip                : Image_Flip;
      Image_Flags         : Interfaces.Unsigned_32;
      Dither_Row          : Long_Float;
      Dither_Column       : Long_Float;
      Row_Offset          : Interfaces.Unsigned_32;
      Column_Offset       : Interfaces.Unsigned_32;
      Band_Index          : Byte;
      Pixels              : Pixel_Array (1 .. Pixel_Count);
   end record;

   Timeout_Error  : exception;
   Stream_Stopped : exception;
   --  At most one task may call Receive for a given Image_Stream at a time.
   --  Frames queued before Stop or provider failure are returned first.
   function Receive
     (Object : Image_Stream; Timeout_Milliseconds : Natural := 0) return Frame;

   type Counter is mod 2 ** 64 with Size => 64;
   type Stream_Counters is record
      Frames_Received           : Counter;
      Frames_Dropped_Queue_Full : Counter;
      Malformed_Or_Unsupported  : Counter;
   end record;
   function Counters (Object : Image_Stream) return Stream_Counters;
   procedure Stop (Object : in out Image_Stream);
   procedure Close (Object : in out Image_Stream);

private
   package US renames Ada.Strings.Unbounded;
   type UCI_ID is record
      Value : UUID;
      Label : US.Unbounded_String;
   end record;
   type Component_Location is record
      X, Y, Z     : Long_Float;
      Key_Value   : US.Unbounded_String;
      System_Value : US.Unbounded_String;
   end record;
   type Image_Config is record
      Channel  : UCI_ID;
      Platform : UCI_ID;
      Location : Component_Location;
      Buffers  : Positive;
      Size     : Positive;
      Queue    : Positive;
   end record;
   type Image_Stream is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Stream_Handle := AMS.MEL_C_API.Null_Stream;
   end record;
   overriding procedure Finalize (Object : in out Image_Stream);
end AMS.MEL.IR;
