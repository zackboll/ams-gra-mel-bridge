with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;
with System.Storage_Elements;
with AMS.MEL.IR.Channel;

package body AMS.MEL.IR.NUC_Conversion is
   package C renames AMS.MEL_C_API;
   package N renames AMS.MEL.IR.NUC;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;
   use type System.Address;
   use type C.NUC_Metadata_Handle;
   use System.Storage_Elements;

   Not_Supported_Return : constant Interfaces.Unsigned_32 := 3;

   type View_Access is access all C.IR_NUC_Temp_Data_Event_V1;
   type Point_Access is access all C.IR_Temp_Point_V1;
   function To_View is new Ada.Unchecked_Conversion (System.Address, View_Access);
   function To_Point is new Ada.Unchecked_Conversion (System.Address, Point_Access);
   --  C array stride includes tail padding; 'Size does not, 'Object_Size does.
   Point_Stride : constant Natural := C.IR_Temp_Point_V1'Object_Size / System.Storage_Unit;

   type Diagnostic is array (C.Size_T range 0 .. 511) of aliased Interfaces.C.char
   with Convention => C;

   function Message (Value : Diagnostic; Default : String) return String is
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
         return (if Result'Length = 0 then Default else Result);
      end;
   end Message;

   procedure Check (Code : Interfaces.Integer_32; D : Diagnostic) is
   begin
      if Code = C.Timeout then
         raise Timeout_Error;
      elsif Code = C.Stream_Stopped then
         raise Stream_Stopped;
      elsif Code /= C.Success then
         raise Provider_Error with Message (D, "native NUC_TempData operation failed");
      end if;
   end Check;

   procedure Check_Open
     (Code : Interfaces.Integer_32; Registration : Interfaces.Unsigned_32; Message : String) is
   begin
      if Code = C.Success then
         return;
      elsif Code = C.Provider_Failed and then Registration = Not_Supported_Return then
         raise N.Not_Supported with Message;
      else
         raise Provider_Error with Message;
      end if;
   end Check_Open;

   function Pixel (Value : C.IR_Pixel_Value_V1) return N.Pixel_Value is
   begin
      case Value.Kind is
         when 1      =>
            if Value.Value > 16#FF# then
               raise Provider_Error with "NUC uint8 PixelValue out of range";
            end if;
            return (N.Unsigned_8_Pixel, Interfaces.Unsigned_8 (Value.Value));

         when 2      =>
            if Value.Value > 16#FFFF# then
               raise Provider_Error with "NUC uint16 PixelValue out of range";
            end if;
            return (N.Unsigned_16_Pixel, Interfaces.Unsigned_16 (Value.Value));

         when 3      =>
            return (N.Unsigned_32_Pixel, Value.Value);

         when others =>
            raise Provider_Error with "invalid NUC PixelValue alternative";
      end case;
   end Pixel;

   function Copy (Raw : C.IR_NUC_Temp_Data_Event_V1) return N.NUC_Temp_Data is
      Result : N.NUC_Temp_Data;
   begin
      if Raw.Temp_Points.Size > 0 then
         if Raw.Temp_Points.Data = System.Null_Address then
            raise Provider_Error with "inconsistent NUC TempPoint span";
         end if;
         for I in 0 .. Natural (Raw.Temp_Points.Size) - 1 loop
            declare
               P : constant Point_Access :=
                 To_Point (Raw.Temp_Points.Data + Storage_Offset (I * Point_Stride));
            begin
               if P.Frame_Band.Kind > 14 then
                  raise Provider_Error with "invalid NUC BandType";
               elsif P.Point_Type > 2 then
                  raise Provider_Error with "invalid NUC TempPointType";
               end if;
               Result.Points.Append
                 (N.Temp_Point'
                    (Timestamp_NS  => Long_Long_Integer (P.Timestamp_NS),
                     Frame_Band    =>
                       (Kind             => AMS.MEL.IR.Channel.Band_Type'Val (P.Frame_Band.Kind),
                        Min_Wavelength_M => Long_Float (P.Frame_Band.Min_Wavelength_M),
                        Max_Wavelength_M => Long_Float (P.Frame_Band.Max_Wavelength_M)),
                     Pixel         => Pixel (P.Pixel_Value),
                     Temperature_K => Long_Float (P.Temperature_K),
                     Point_Type    => N.Temp_Point_Type'Val (P.Point_Type)));
            end;
         end loop;
      end if;
      return Result;
   end Copy;

   function Receive
     (Handle : C.NUC_Metadata_Handle; Timeout_Milliseconds : Natural) return N.NUC_Temp_Data
   is
      Event   : aliased C.NUC_Event_Handle := C.Null_NUC_Event;
      Address : aliased System.Address := System.Null_Address;
      D       : aliased Diagnostic := [others => Interfaces.C.nul];
      R       : aliased C.Size_T := 0;
      procedure Release is
         Ignored : Interfaces.Integer_32;
      begin
         Ignored := C.IR_NUC_Event_Close (Event'Access, System.Null_Address, 0, null);
      end Release;
   begin
      if Handle = C.Null_NUC_Metadata then
         raise Provider_Error with "NUC_TempData metadata owner is closed";
      end if;
      Check
        (C.IR_NUC_Receive
           (Handle,
            Interfaces.Unsigned_32 (Timeout_Milliseconds),
            Event'Access,
            D'Address,
            D'Length,
            R'Access),
         D);
      begin
         Check (C.IR_NUC_Event_View (Event, Address'Access, D'Address, D'Length, R'Access), D);
         declare
            Result : constant N.NUC_Temp_Data := Copy (To_View (Address).all);
         begin
            Release;
            return Result;
         end;
      exception
         when others =>
            Release;
            raise;
      end;
   end Receive;

   function Counters (Handle : C.NUC_Metadata_Handle) return C.Metadata_Counters_V1 is
      Raw : aliased C.Metadata_Counters_V1 := (others => 0);
      D   : aliased Diagnostic := [others => Interfaces.C.nul];
      R   : aliased C.Size_T := 0;
   begin
      if Handle = C.Null_NUC_Metadata then
         raise Provider_Error with "NUC_TempData metadata owner is closed";
      end if;
      Check (C.IR_NUC_Get_Counters (Handle, Raw'Access, D'Address, D'Length, R'Access), D);
      return Raw;
   end Counters;

   procedure Close (Handle : aliased in out C.NUC_Metadata_Handle) is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check (C.IR_NUC_Close (Handle'Access, D'Address, D'Length, R'Access), D);
   end Close;
end AMS.MEL.IR.NUC_Conversion;
