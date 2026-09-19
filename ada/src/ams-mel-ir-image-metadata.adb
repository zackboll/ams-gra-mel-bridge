with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;
with System.Storage_Elements;

package body AMS.MEL.IR.Image.Metadata is
   package C renames AMS.MEL_C_API;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use System.Storage_Elements;
   type Pixel_Access is access all C.IR_Bad_Pixel_V1;
   type View_Access is access all C.IR_Image_Metadata_Event_V1;
   function To_Pixel is new Ada.Unchecked_Conversion (System.Address, Pixel_Access);
   function To_View is new Ada.Unchecked_Conversion (System.Address, View_Access);
   function Address_At (Base : System.Address; Index, Bytes : Natural) return System.Address
   is (Base + Storage_Offset (Index * Bytes));
   type Diagnostic is array (C.Size_T range 0 .. 511) of aliased Interfaces.C.char
   with Convention => C;
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
         return (if Result'Length = 0 then "native Image metadata operation failed" else Result);
      end;
   end Message;
   procedure Check (Code : Interfaces.Integer_32; D : Diagnostic) is
   begin
      if Code = C.Timeout then
         raise Timeout_Error;
      elsif Code = C.Stream_Stopped then
         raise Stream_Stopped;
      elsif Code /= C.Success then
         raise Provider_Error with Message (D);
      end if;
   end Check;
   procedure Release (Handle : aliased in out C.Image_Metadata_Event_Handle) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Image_Metadata_Event_Close (Handle'Access, System.Null_Address, 0, null);
   end Release;
   function Open
     (Stream : AMS.MEL.IR.Image_Stream; Queue_Capacity : Positive) return Metadata_Stream
   is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      return Result : Metadata_Stream do
         Check
           (C.IR_Image_Metadata_Open
              (Stream.Handle,
               C.Size_T (Queue_Capacity),
               Result.Handle'Access,
               D'Address,
               D'Length,
               R'Access),
            D);
      end return;
   end Open;
   function Receive
     (Stream : Metadata_Stream; Timeout_Milliseconds : Natural := 0) return Metadata_Event
   is
      Handle  : aliased C.Image_Metadata_Event_Handle := C.Null_Image_Metadata_Event;
      Address : aliased System.Address := System.Null_Address;
      D       : aliased Diagnostic := [others => Interfaces.C.nul];
      R       : aliased C.Size_T := 0;
   begin
      Check
        (C.IR_Image_Metadata_Receive
           (Stream.Handle,
            Interfaces.Unsigned_32 (Timeout_Milliseconds),
            Handle'Access,
            D'Address,
            D'Length,
            R'Access),
         D);
      begin
         Check
           (C.IR_Image_Metadata_Event_View (Handle, Address'Access, D'Address, D'Length, R'Access),
            D);
         declare
            Raw    : constant View_Access := To_View (Address);
            Result : Metadata_Event;
         begin
            if Raw.Kind /= 1 then
               raise Provider_Error with "invalid Image metadata kind";
            end if;
            Result.Value.Size := Raw.Bad_Pixel_List.Reported_Size;
            Result.Value.Count := Raw.Bad_Pixel_List.Reported_Count;
            if Raw.Bad_Pixel_List.Pixels.Size > 0 then
               for I in 0 .. Natural (Raw.Bad_Pixel_List.Pixels.Size) - 1 loop
                  declare
                     P : constant Pixel_Access :=
                       To_Pixel
                         (Address_At
                            (Raw.Bad_Pixel_List.Pixels.Data, I, C.IR_Bad_Pixel_V1'SIZE / 8));
                  begin
                     if P.Reason /= 0 then
                        raise Provider_Error with "invalid BadPixel reason";
                     end if;
                     Result.Value.Pixels.Append (Bad_Pixel'(P.Row, P.Column, Unknown));
                  end;
               end loop;
            end if;
            Release (Handle);
            return Result;
         end;
      exception
         when others =>
            Release (Handle);
            raise;
      end;
   end Receive;
   function Counters (Stream : Metadata_Stream) return Metadata_Counters is
      Raw : aliased C.Metadata_Counters_V1;
      D   : aliased Diagnostic := [others => Interfaces.C.nul];
      R   : aliased C.Size_T := 0;
   begin
      Check
        (C.IR_Image_Metadata_Get_Counters
           (Stream.Handle, Raw'Access, D'Address, D'Length, R'Access),
         D);
      return
        (AMS.MEL.IR.Counter (Raw.Events_Received),
         AMS.MEL.IR.Counter (Raw.Events_Dropped_Queue_Full),
         AMS.MEL.IR.Counter (Raw.Malformed_Or_Unsupported));
   end;
   procedure Close (Stream : in out Metadata_Stream) is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check (C.IR_Image_Metadata_Close (Stream.Handle'Access, D'Address, D'Length, R'Access), D);
   end;
   overriding
   procedure Finalize (Stream : in out Metadata_Stream) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Image_Metadata_Close (Stream.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Stream.Handle := C.Null_Image_Metadata;
   end;
   function Reported_Size (Value : Bad_Pixel_List) return Interfaces.Unsigned_32
   is (Value.Size);
   function Reported_Count (Value : Bad_Pixel_List) return Interfaces.Unsigned_32
   is (Value.Count);
   function Pixel_Count (Value : Bad_Pixel_List) return Natural
   is (Natural (Value.Pixels.Length));
   function Pixel_At (Value : Bad_Pixel_List; Index : Positive) return Bad_Pixel
   is (Value.Pixels.Element (Index));
   function Kind (Event : Metadata_Event) return Metadata_Kind
   is (Bad_Pixel_List_Event);
   function Bad_Pixel_List_Value (Event : Metadata_Event) return Bad_Pixel_List
   is (Event.Value);
end AMS.MEL.IR.Image.Metadata;
