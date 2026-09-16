with Interfaces.C;
with System;

package body AMS.MEL.IR is
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;
   use type AMS.MEL_C_API.Stream_Handle;
   package C renames AMS.MEL_C_API;

   Diagnostic_Capacity : constant := 512;
   subtype Diagnostic_Index is Interfaces.C.size_t range 0 .. Diagnostic_Capacity - 1;
   type Diagnostic_Array is array (Diagnostic_Index) of aliased Interfaces.C.char
     with Convention => C;

   function Message (Buffer : Diagnostic_Array) return String is
      Length : Natural := 0;
   begin
      while Length < Buffer'Length
        and then Buffer (Interfaces.C.size_t (Length)) /= Interfaces.C.nul
      loop
         Length := Length + 1;
      end loop;
      if Length = 0 then
         return "native IR stream operation failed";
      end if;
      declare
         Result : String (1 .. Length);
      begin
         for Index in Result'Range loop
            Result (Index) := Character'Val
              (Interfaces.C.char'Pos
                 (Buffer (Interfaces.C.size_t (Index - 1))));
         end loop;
         return Result;
      end;
   end Message;

   procedure Reject_NUL (Value : String; Name : String) is
   begin
      for Item of Value loop
         if Item = Character'Val (0) then
            raise Constraint_Error with Name & " contains an embedded NUL";
         end if;
      end loop;
   end Reject_NUL;

   function Create_UCI_ID (Value : UUID; Descriptive_Label : String) return UCI_ID is
   begin
      Reject_NUL (Descriptive_Label, "Descriptive_Label");
      return (Value => Value, Label => US.To_Unbounded_String (Descriptive_Label));
   end Create_UCI_ID;

   function Create_Component_Location
     (Offset_X_M : Long_Float; Offset_Y_M : Long_Float; Offset_Z_M : Long_Float;
      Key : String; System_Name : String) return Component_Location is
   begin
      Reject_NUL (Key, "Key");
      Reject_NUL (System_Name, "System_Name");
      return (X => Offset_X_M, Y => Offset_Y_M, Z => Offset_Z_M,
              Key_Value => US.To_Unbounded_String (Key),
              System_Value => US.To_Unbounded_String (System_Name));
   end Create_Component_Location;

   function Create_Image_Config
     (Channel_ID : UCI_ID; Platform_ID : UCI_ID;
      Sensor_Location : Component_Location; Buffer_Count : Positive := 3;
      Buffer_Size : Positive := 1_048_576; Queue_Capacity : Positive := 4)
      return Image_Config is
     ((Channel => Channel_ID, Platform => Platform_ID,
       Location => Sensor_Location, Buffers => Buffer_Count,
       Size => Buffer_Size, Queue => Queue_Capacity));

   function String_View (Value : String) return C.String_View_V1 is
     ((Data => (if Value'Length = 0 then System.Null_Address else Value'Address),
       Size => Interfaces.C.size_t (Value'Length)));

   function Open_Image_Stream
     (Parent : Session; Config : Image_Config) return Image_Stream
   is
      Channel_Label : aliased constant String := US.To_String (Config.Channel.Label);
      Platform_Label : aliased constant String := US.To_String (Config.Platform.Label);
      Key : aliased constant String := US.To_String (Config.Location.Key_Value);
      System_Name : aliased constant String := US.To_String (Config.Location.System_Value);
      Raw : aliased C.IR_Stream_Config_V1 :=
        (Channel_Type => 1,
         Channel_ID => (UUID => C.Byte_Array_16 (Config.Channel.Value),
                        Descriptive_Label => String_View (Channel_Label)),
         Platform_ID => (UUID => C.Byte_Array_16 (Config.Platform.Value),
                         Descriptive_Label => String_View (Platform_Label)),
         Sensor_Location =>
           (Offset_X_M => Interfaces.C.double (Config.Location.X),
            Offset_Y_M => Interfaces.C.double (Config.Location.Y),
            Offset_Z_M => Interfaces.C.double (Config.Location.Z),
            Key => String_View (Key), System_Name => String_View (System_Name)),
         Buffer_Count => Interfaces.C.size_t (Config.Buffers),
         Buffer_Size => Interfaces.C.size_t (Config.Size),
         Queue_Capacity => Interfaces.C.size_t (Config.Queue));
      Diagnostic : aliased Diagnostic_Array := (others => Interfaces.C.nul);
      Required : aliased C.Size_T := 0;
   begin
      if not AMS.MEL.Is_Open (Parent) then
         raise Provider_Error with "provider session is closed";
      end if;
      return Result : Image_Stream do
         declare
            Status : constant Interfaces.Integer_32 := C.IR_Stream_Open
              (Parent.Handle, Raw'Access, Result.Handle'Access,
               Diagnostic'Address, Diagnostic'Length, Required'Access);
         begin
            if Status /= C.Success then
               raise Provider_Error with Message (Diagnostic);
            end if;
         end;
      end return;
   end Open_Image_Stream;

   function Is_Open (Object : Image_Stream) return Boolean is
     (Object.Handle /= C.Null_Stream);

   procedure Start (Object : in out Image_Stream) is
      Diagnostic : aliased Diagnostic_Array := (others => Interfaces.C.nul);
      Required : aliased C.Size_T := 0;
      Status : constant Interfaces.Integer_32 := C.IR_Stream_Start
        (Object.Handle, Diagnostic'Address, Diagnostic'Length, Required'Access);
   begin
      if Status /= C.Success then raise Provider_Error with Message (Diagnostic); end if;
   end Start;

   function Empty_Raw_Frame return C.IR_Frame_V1 is
     ((System_Time_NS => 0, Integration_Time_NS => 0, Width => 0, Height => 0,
       Bits_Per_Pixel => 0, Number_Of_Bands => 0, Horizontal_FOV_Rad => 0.0,
       Vertical_FOV_Rad => 0.0, Pixel_Format => 0, Frame_ID => 0,
       Subframe_ID => 0, Subframe_Total => 0, Image_Type => 0, Image_Flip => 0,
       Image_Flags => 0, Dither_Row => 0.0, Dither_Column => 0.0,
       Row_Offset => 0, Column_Offset => 0, Band_Index => 0,
       Reserved => (others => 0), Pixels => System.Null_Address,
       Pixel_Capacity => 0, Pixel_Required => 0));

   procedure Check_Receive_Status
     (Status : Interfaces.Integer_32; Diagnostic : Diagnostic_Array) is
   begin
      if Status = C.Timeout then raise Timeout_Error with "IR receive timed out";
      elsif Status = C.Stream_Stopped then raise Stream_Stopped with "IR stream stopped";
      elsif Status /= C.Success and then Status /= C.Buffer_Too_Small then
         raise Provider_Error with Message (Diagnostic);
      end if;
   end Check_Receive_Status;

   function Receive
     (Object : Image_Stream; Timeout_Milliseconds : Natural := 0) return Frame
   is
      Raw : aliased C.IR_Frame_V1 := Empty_Raw_Frame;
      Diagnostic : aliased Diagnostic_Array := (others => Interfaces.C.nul);
      Required : aliased C.Size_T := 0;
      Timeout : Interfaces.Unsigned_32;
      Status : Interfaces.Integer_32;
   begin
      Timeout := Interfaces.Unsigned_32 (Timeout_Milliseconds);
      Status := C.IR_Stream_Receive (Object.Handle, Timeout, Raw'Access,
        Diagnostic'Address, Diagnostic'Length, Required'Access);
      Check_Receive_Status (Status, Diagnostic);
      if Status /= C.Buffer_Too_Small or else Raw.Pixel_Required = 0 then
         raise Provider_Error with "native IR frame size discovery failed";
      end if;
      return Result : Frame (Natural (Raw.Pixel_Required)) do
         Raw.Pixels := Result.Pixels'Address;
         Raw.Pixel_Capacity := Raw.Pixel_Required;
         Status := C.IR_Stream_Receive (Object.Handle, 0, Raw'Access,
           Diagnostic'Address, Diagnostic'Length, Required'Access);
         Check_Receive_Status (Status, Diagnostic);
         if Status /= C.Success or else Raw.Pixel_Format /= 0
           or else Raw.Bits_Per_Pixel /= 8 or else Raw.Number_Of_Bands /= 1
           or else Raw.Image_Type > 1 or else Raw.Image_Flip > 3
         then
            raise Provider_Error with "native IR frame profile violation";
         end if;
         Result.System_Time_NS := Long_Long_Integer (Raw.System_Time_NS);
         Result.Integration_Time_NS := Long_Long_Integer (Raw.Integration_Time_NS);
         Result.Width := Raw.Width; Result.Height := Raw.Height;
         Result.Bits_Per_Pixel := Raw.Bits_Per_Pixel;
         Result.Number_Of_Bands := Raw.Number_Of_Bands;
         Result.Horizontal_FOV_Rad := Long_Float (Raw.Horizontal_FOV_Rad);
         Result.Vertical_FOV_Rad := Long_Float (Raw.Vertical_FOV_Rad);
         Result.Frame_ID := Raw.Frame_ID; Result.Subframe_ID := Raw.Subframe_ID;
         Result.Subframe_Total := Raw.Subframe_Total;
         Result.Kind := Image_Type'Val (Raw.Image_Type);
         Result.Flip := Image_Flip'Val (Raw.Image_Flip);
         Result.Image_Flags := Raw.Image_Flags;
         Result.Dither_Row := Long_Float (Raw.Dither_Row);
         Result.Dither_Column := Long_Float (Raw.Dither_Column);
         Result.Row_Offset := Raw.Row_Offset; Result.Column_Offset := Raw.Column_Offset;
         Result.Band_Index := Raw.Band_Index;
      end return;
   end Receive;

   function Counters (Object : Image_Stream) return Stream_Counters is
      Raw : aliased C.IR_Counters_V1 := (others => 0);
      Diagnostic : aliased Diagnostic_Array := (others => Interfaces.C.nul);
      Required : aliased C.Size_T := 0;
      Status : constant Interfaces.Integer_32 := C.IR_Stream_Get_Counters
        (Object.Handle, Raw'Access, Diagnostic'Address, Diagnostic'Length,
         Required'Access);
   begin
      if Status /= C.Success then raise Provider_Error with Message (Diagnostic); end if;
      return (Counter (Raw.Frames_Received), Counter (Raw.Frames_Dropped_Queue_Full),
              Counter (Raw.Malformed_Or_Unsupported));
   end Counters;

   procedure Stop (Object : in out Image_Stream) is
      Diagnostic : aliased Diagnostic_Array := (others => Interfaces.C.nul);
      Required : aliased C.Size_T := 0;
      Status : constant Interfaces.Integer_32 := C.IR_Stream_Stop
        (Object.Handle, Diagnostic'Address, Diagnostic'Length, Required'Access);
   begin
      if Status /= C.Success then raise Provider_Error with Message (Diagnostic); end if;
   end Stop;

   procedure Close (Object : in out Image_Stream) is
      Diagnostic : aliased Diagnostic_Array := (others => Interfaces.C.nul);
      Required : aliased C.Size_T := 0;
      Status : constant Interfaces.Integer_32 := C.IR_Stream_Close
        (Object.Handle'Access, Diagnostic'Address, Diagnostic'Length, Required'Access);
   begin
      if Status /= C.Success then raise Provider_Error with Message (Diagnostic); end if;
   end Close;

   overriding procedure Finalize (Object : in out Image_Stream) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Stream_Close (Object.Handle'Access, System.Null_Address, 0, null);
   exception
      when others => Object.Handle := C.Null_Stream;
   end Finalize;
end AMS.MEL.IR;
