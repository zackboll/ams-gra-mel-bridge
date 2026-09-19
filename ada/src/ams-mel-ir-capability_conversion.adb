with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;
with System.Storage_Elements;

package body AMS.MEL.IR.Capability_Conversion is
   package C renames AMS.MEL_C_API;
   package V renames AMS.MEL.IR.Channel;
   use type Interfaces.C.size_t;
   use type Interfaces.Unsigned_32;
   use System.Storage_Elements;

   type U32_Access is access all Interfaces.Unsigned_32;
   type Band_Access is access all C.IR_Image_Band_V1;
   type Info_Access is access all C.IR_Band_Info_V1;
   type Char_Access is access all Interfaces.C.char;
   function To_U32 is new Ada.Unchecked_Conversion (System.Address, U32_Access);
   function To_Band is new Ada.Unchecked_Conversion (System.Address, Band_Access);
   function To_Info is new Ada.Unchecked_Conversion (System.Address, Info_Access);
   function To_Char is new Ada.Unchecked_Conversion (System.Address, Char_Access);

   function Address_At (Base : System.Address; Index, Bytes : Natural) return System.Address
   is (Base + Storage_Offset (Index * Bytes));

   function Copy (Value : C.String_View_V1) return String is
   begin
      if Value.Size = 0 then
         return "";
      end if;
      declare
         Result : String (1 .. Natural (Value.Size));
      begin
         for I in Result'Range loop
            Result (I) :=
              Character'Val
                (Interfaces.C.char'Pos (To_Char (Address_At (Value.Data, I - 1, 1)).all));
         end loop;
         return Result;
      end;
   end Copy;

   function Copy (Value : C.UCI_ID_V1) return UCI_ID is
      Bytes : UUID;
   begin
      for I in Bytes'Range loop
         Bytes (I) := Value.UUID (I);
      end loop;
      return Create_UCI_ID (Bytes, Copy (Value.Descriptive_Label));
   end Copy;

   function To_Channel_Capability (Value : C.IR_Channel_Capability_V1) return V.Channel_Capability
   is
      Sensors  : V.Sensor_Type_Vectors.Vector;
      Channels : V.Channel_Type_Vectors.Vector;
      Metadata : V.Metadata_Vectors.Vector;
      Bands    : V.Image_Band_Vectors.Vector;
      Frames   : V.Coordinate_Vectors.Vector;
   begin
      if Value.Sensor_Types.Size > 0 then
         for I in 0 .. Natural (Value.Sensor_Types.Size) - 1 loop
            Sensors.Append
              (V.Sensor_Type'Val (To_U32 (Address_At (Value.Sensor_Types.Data, I, 4)).all));
         end loop;
      end if;
      if Value.Channel_Types.Size > 0 then
         for I in 0 .. Natural (Value.Channel_Types.Size) - 1 loop
            Channels.Append
              (V.Channel_Type'Val (To_U32 (Address_At (Value.Channel_Types.Data, I, 4)).all));
         end loop;
      end if;
      if Value.Metadata_Capabilities.Size > 0 then
         for I in 0 .. Natural (Value.Metadata_Capabilities.Size) - 1 loop
            Metadata.Append
              (V.Metadata_Capability'Val
                 (To_U32 (Address_At (Value.Metadata_Capabilities.Data, I, 4)).all));
         end loop;
      end if;
      if Value.Image_Bands.Size > 0 then
         for I in 0 .. Natural (Value.Image_Bands.Size) - 1 loop
            declare
               Band  : constant Band_Access :=
                 To_Band (Address_At (Value.Image_Bands.Data, I, C.IR_Image_Band_V1'Size / 8));
               Infos : V.Band_Info_Vectors.Vector;
            begin
               if Band.Bands.Size > 0 then
                  for J in 0 .. Natural (Band.Bands.Size) - 1 loop
                     declare
                        Info : constant Info_Access :=
                          To_Info (Address_At (Band.Bands.Data, J, C.IR_Band_Info_V1'Size / 8));
                     begin
                        Infos.Append
                          (V.Band_Info'
                             (V.Band_Type'Val (Info.Kind),
                              Long_Float (Info.Min_Wavelength_M),
                              Long_Float (Info.Max_Wavelength_M)));
                     end;
                  end loop;
               end if;
               Bands.Append (V.Image_Band'(Band.Band_Index, Infos));
            end;
         end loop;
      end if;
      if Value.Nav_Frames.Size > 0 then
         for I in 0 .. Natural (Value.Nav_Frames.Size) - 1 loop
            Frames.Append
              (V.Coordinate_System_Type'Val
                 (To_U32 (Address_At (Value.Nav_Frames.Data, I, 4)).all));
         end loop;
      end if;
      return
        V.Create_Capability
          (Copy (Value.Channel_ID),
           Value.Height,
           Value.Width,
           Value.Bit_Depth,
           Value.Row_Pitch,
           Value.Buffer_Size,
           Value.Image_Size,
           Value.Number_Of_Bands,
           V.Pixel_Format'Val (Value.Pixel_Format),
           Sensors,
           Copy (Value.Platform_ID),
           Create_Component_Location
             (Long_Float (Value.Sensor_Location.Offset_X_M),
              Long_Float (Value.Sensor_Location.Offset_Y_M),
              Long_Float (Value.Sensor_Location.Offset_Z_M),
              Copy (Value.Sensor_Location.Key),
              Copy (Value.Sensor_Location.System_Name)),
           Channels,
           Value.Task_Schedule_Depth,
           Value.ODC_Available = 1,
           Value.NUC_Available = 1,
           Metadata,
           Bands,
           Frames);
   end To_Channel_Capability;
end AMS.MEL.IR.Capability_Conversion;
