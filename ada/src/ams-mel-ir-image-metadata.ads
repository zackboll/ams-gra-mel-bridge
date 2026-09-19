with Ada.Containers.Vectors;
private with Ada.Finalization;
with Interfaces;
private with AMS.MEL_C_API;

package AMS.MEL.IR.Image.Metadata is
   type Metadata_Kind is (Bad_Pixel_List_Event);
   type Bad_Pixel_Reason is (Unknown);
   type Bad_Pixel is record
      Row    : Interfaces.Unsigned_32;
      Column : Interfaces.Unsigned_32;
      Reason : Bad_Pixel_Reason;
   end record;
   type Bad_Pixel_List is private;
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
   type Metadata_Event is record
      Value : Bad_Pixel_List;
   end record;
end AMS.MEL.IR.Image.Metadata;
