package body AMS.MEL.IR.Channel is
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
        Coordinate_Vectors.Vector) return Channel_Capability
   is ((Channel_ID,
        Platform_ID,
        Height,
        Width,
        Bit_Depth,
        Row_Pitch,
        Buffer_Size,
        Image_Size,
        Number_Of_Bands,
        Task_Schedule_Depth,
        Format,
        Sensor_Types,
        Sensor_Location,
        Channel_Types,
        ODC_Available,
        NUC_Available,
        Metadata,
        Image_Bands,
        Nav_Frames));
   function Channel_ID (Value : Channel_Capability) return UCI_ID
   is (Value.ID);
   function Height (Value : Channel_Capability) return Interfaces.Unsigned_32
   is (Value.H);
   function Width (Value : Channel_Capability) return Interfaces.Unsigned_32
   is (Value.W);
   function Bit_Depth (Value : Channel_Capability) return Interfaces.Unsigned_32
   is (Value.Depth);
   function Row_Pitch (Value : Channel_Capability) return Interfaces.Unsigned_32
   is (Value.Pitch);
   function Buffer_Size (Value : Channel_Capability) return Interfaces.Unsigned_32
   is (Value.Buffer);
   function Image_Size (Value : Channel_Capability) return Interfaces.Unsigned_32
   is (Value.Image);
   function Number_Of_Bands (Value : Channel_Capability) return Interfaces.Unsigned_32
   is (Value.Band_Count);
   function Format (Value : Channel_Capability) return Pixel_Format
   is (Value.Pixel);
   function Sensor_Type_Count (Value : Channel_Capability) return Natural
   is (Natural (Value.Sensors.Length));
   function Sensor_Type_At (Value : Channel_Capability; Index : Positive) return Sensor_Type
   is (Value.Sensors (Index));
   function Platform_ID (Value : Channel_Capability) return UCI_ID
   is (Value.Platform);
   function Sensor_Location (Value : Channel_Capability) return Component_Location
   is (Value.Location);
   function Channel_Type_Count (Value : Channel_Capability) return Natural
   is (Natural (Value.Channels.Length));
   function Channel_Type_At (Value : Channel_Capability; Index : Positive) return Channel_Type
   is (Value.Channels (Index));
   function Task_Schedule_Depth (Value : Channel_Capability) return Interfaces.Unsigned_32
   is (Value.Schedule);
   function ODC_Available (Value : Channel_Capability) return Boolean
   is (Value.ODC);
   function NUC_Available (Value : Channel_Capability) return Boolean
   is (Value.NUC);
   function Metadata_Capability_Count (Value : Channel_Capability) return Natural
   is (Natural (Value.Metadata.Length));
   function Metadata_Capability_At
     (Value : Channel_Capability; Index : Positive) return Metadata_Capability
   is (Value.Metadata (Index));
   function Has_Metadata_Capability
     (Value : Channel_Capability; Item : Metadata_Capability) return Boolean
   is (Value.Metadata.Contains (Item));
   function Image_Band_Count (Value : Channel_Capability) return Natural
   is (Natural (Value.Bands.Length));
   function Image_Band_Index_At
     (Value : Channel_Capability; Index : Positive) return Interfaces.Unsigned_32
   is (Value.Bands (Index).Band_Index);
   function Image_Band_Info_Count (Value : Channel_Capability; Index : Positive) return Natural
   is (Natural (Value.Bands (Index).Bands.Length));
   function Image_Band_Info_At
     (Value : Channel_Capability; Band_Index, Info_Index : Positive) return Band_Info
   is (Value.Bands (Band_Index).Bands (Info_Index));
   function Nav_Frame_Count (Value : Channel_Capability) return Natural
   is (Natural (Value.Frames.Length));
   function Nav_Frame_At
     (Value : Channel_Capability; Index : Positive) return Coordinate_System_Type
   is (Value.Frames (Index));
end AMS.MEL.IR.Channel;
