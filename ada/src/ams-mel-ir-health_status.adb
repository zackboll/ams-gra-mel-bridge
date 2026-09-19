with Ada.Unchecked_Conversion;
with Interfaces;
with Interfaces.C;
with System;
with System.Storage_Elements;

package body AMS.MEL.IR.Health_Status is
   package C renames AMS.MEL_C_API;
   package V renames IR.Channel;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;
   use type C.Health_Handle;
   use type C.Session_Handle;
   use System.Storage_Elements;

   type Diagnostic is array (C.Size_T range <>) of aliased Interfaces.C.char with Convention => C;
   subtype Fixed_Diagnostic is Diagnostic (0 .. 511);
   type Char_Access is access all Interfaces.C.char;
   type Cap_Access is access all C.IR_Channel_Capability_V1;
   type U32_Access is access all Interfaces.Unsigned_32;
   type Band_Access is access all C.IR_Image_Band_V1;
   type Info_Access is access all C.IR_Band_Info_V1;
   function To_Char is new Ada.Unchecked_Conversion (System.Address, Char_Access);
   function To_Cap is new Ada.Unchecked_Conversion (System.Address, Cap_Access);
   function To_U32 is new Ada.Unchecked_Conversion (System.Address, U32_Access);
   function To_Band is new Ada.Unchecked_Conversion (System.Address, Band_Access);
   function To_Info is new Ada.Unchecked_Conversion (System.Address, Info_Access);

   function Address_At (Base : System.Address; Index, Bytes : Natural) return System.Address
   is (Base + Storage_Offset (Index * Bytes));
   function Message (Value : Diagnostic) return String is
      Last : Natural := 0;
   begin
      while Last < Value'Length and then Value (C.Size_T (Last)) /= Interfaces.C.nul loop
         Last := Last + 1;
      end loop;
      declare
         Result : String (1 .. Last);
      begin
         for I in Result'Range loop
            Result (I) := Character'Val (Interfaces.C.char'Pos (Value (C.Size_T (I - 1))));
         end loop;
         return (if Result'Length = 0 then "native IR Health operation failed" else Result);
      end;
   end Message;
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
   function String_View (Value : String) return C.String_View_V1
   is (Data => (if Value'Length = 0 then System.Null_Address else Value'Address),
       Size => C.Size_T (Value'Length));

   function Create_Config
     (Channel_ID : UCI_ID; Platform_ID : UCI_ID; Sensor_Location : Component_Location)
      return Health_Config
   is ((Channel_ID, Platform_ID, Sensor_Location));

   function Open (Parent : Session; Config : Health_Config) return Health_Channel is
      Channel_Label  : aliased constant String := US.To_String (Config.Channel.Label);
      Platform_Label : aliased constant String := US.To_String (Config.Platform.Label);
      Key            : aliased constant String := US.To_String (Config.Location.Key_Value);
      System_Name    : aliased constant String := US.To_String (Config.Location.System_Value);
      Raw            : aliased C.IR_Health_Config_V1 :=
        ((C.Byte_Array_16 (Config.Channel.Value), String_View (Channel_Label)),
         4,
         (C.Byte_Array_16 (Config.Platform.Value), String_View (Platform_Label)),
         (Interfaces.C.double (Config.Location.X),
          Interfaces.C.double (Config.Location.Y),
          Interfaces.C.double (Config.Location.Z),
          String_View (Key),
          String_View (System_Name)));
      D              : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required       : aliased C.Size_T := 0;
   begin
      if Parent.Handle = C.Null_Session then
         raise Provider_Error with "provider session is closed";
      end if;
      return Result : Health_Channel do
         if C.IR_Health_Open
              (Parent.Handle,
               Raw'Access,
               Result.Handle'Access,
               D'Address,
               D'Length,
               Required'Access)
           /= C.Success
         then
            raise Provider_Error with Message (D);
         end if;
      end return;
   end Open;

   function Is_Open (Channel : Health_Channel) return Boolean
   is (Channel.Handle /= C.Null_Health);

   procedure Enable (Channel : in out Health_Channel) is
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if C.IR_Health_Enable (Channel.Handle, D'Address, D'Length, Required'Access) /= C.Success then
         raise Provider_Error with Message (D);
      end if;
   end Enable;

   function Capabilities (Channel : Health_Channel) return V.Channel_Capability is
      Owner    : aliased C.Capability_Handle := C.Null_Capability;
      Address  : aliased System.Address := System.Null_Address;
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
      procedure Release is
         Ignored : Interfaces.Integer_32;
      begin
         Ignored := C.IR_Capability_Close (Owner'Access, System.Null_Address, 0, null);
      end Release;
   begin
      if C.IR_Health_Get_Capabilities
           (Channel.Handle, Owner'Access, D'Address, D'Length, Required'Access)
        /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
      if C.IR_Capability_View (Owner, Address'Access, D'Address, D'Length, Required'Access)
        /= C.Success
      then
         Release;
         raise Provider_Error with Message (D);
      end if;
      declare
         Raw      : constant Cap_Access := To_Cap (Address);
         Sensors  : V.Sensor_Type_Vectors.Vector;
         Channels : V.Channel_Type_Vectors.Vector;
         Metadata : V.Metadata_Vectors.Vector;
         Bands    : V.Image_Band_Vectors.Vector;
         Frames   : V.Coordinate_Vectors.Vector;
      begin
         if Raw.Sensor_Types.Size > 0 then
            for I in 0 .. Natural (Raw.Sensor_Types.Size) - 1 loop
               Sensors.Append
                 (V.Sensor_Type'Val (To_U32 (Address_At (Raw.Sensor_Types.Data, I, 4)).all));
            end loop;
         end if;
         if Raw.Channel_Types.Size > 0 then
            for I in 0 .. Natural (Raw.Channel_Types.Size) - 1 loop
               Channels.Append
                 (V.Channel_Type'Val (To_U32 (Address_At (Raw.Channel_Types.Data, I, 4)).all));
            end loop;
         end if;
         if Raw.Metadata_Capabilities.Size > 0 then
            for I in 0 .. Natural (Raw.Metadata_Capabilities.Size) - 1 loop
               Metadata.Append
                 (V.Metadata_Capability'Val
                    (To_U32 (Address_At (Raw.Metadata_Capabilities.Data, I, 4)).all));
            end loop;
         end if;
         if Raw.Image_Bands.Size > 0 then
            for I in 0 .. Natural (Raw.Image_Bands.Size) - 1 loop
               declare
                  B     : constant Band_Access :=
                    To_Band (Address_At (Raw.Image_Bands.Data, I, C.IR_Image_Band_V1'Size / 8));
                  Infos : V.Band_Info_Vectors.Vector;
               begin
                  if B.Bands.Size > 0 then
                     for J in 0 .. Natural (B.Bands.Size) - 1 loop
                        declare
                           X : constant Info_Access :=
                             To_Info (Address_At (B.Bands.Data, J, C.IR_Band_Info_V1'Size / 8));
                        begin
                           Infos.Append
                             (V.Band_Info'
                                (V.Band_Type'Val (X.Kind),
                                 Long_Float (X.Min_Wavelength_M),
                                 Long_Float (X.Max_Wavelength_M)));
                        end;
                     end loop;
                  end if;
                  Bands.Append (V.Image_Band'(B.Band_Index, Infos));
               end;
            end loop;
         end if;
         if Raw.Nav_Frames.Size > 0 then
            for I in 0 .. Natural (Raw.Nav_Frames.Size) - 1 loop
               Frames.Append
                 (V.Coordinate_System_Type'Val
                    (To_U32 (Address_At (Raw.Nav_Frames.Data, I, 4)).all));
            end loop;
         end if;
         declare
            Result : constant V.Channel_Capability :=
              V.Create_Capability
                (Copy (Raw.Channel_ID),
                 Raw.Height,
                 Raw.Width,
                 Raw.Bit_Depth,
                 Raw.Row_Pitch,
                 Raw.Buffer_Size,
                 Raw.Image_Size,
                 Raw.Number_Of_Bands,
                 V.Pixel_Format'Val (Raw.Pixel_Format),
                 Sensors,
                 Copy (Raw.Platform_ID),
                 Create_Component_Location
                   (Long_Float (Raw.Sensor_Location.Offset_X_M),
                    Long_Float (Raw.Sensor_Location.Offset_Y_M),
                    Long_Float (Raw.Sensor_Location.Offset_Z_M),
                    Copy (Raw.Sensor_Location.Key),
                    Copy (Raw.Sensor_Location.System_Name)),
                 Channels,
                 Raw.Task_Schedule_Depth,
                 Raw.ODC_Available = 1,
                 Raw.NUC_Available = 1,
                 Metadata,
                 Bands,
                 Frames);
         begin
            Release;
            return Result;
         end;
      exception
         when others =>
            Release;
            raise;
      end;
   end Capabilities;

   procedure Close (Channel : in out Health_Channel) is
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if C.IR_Health_Close (Channel.Handle'Access, D'Address, D'Length, Required'Access)
        /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
   end Close;
   overriding
   procedure Finalize (Channel : in out Health_Channel) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Health_Close (Channel.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Channel.Handle := C.Null_Health;
   end Finalize;
end AMS.MEL.IR.Health_Status;
