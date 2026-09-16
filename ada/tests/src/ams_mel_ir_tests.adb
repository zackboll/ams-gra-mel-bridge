with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with Interfaces;

package body AMS_MEL_IR_Tests is
   use type AMS.MEL.IR.Byte;
   use type AMS.MEL.IR.Counter;
   use type AMS.MEL.IR.Image_Flip;
   use type AMS.MEL.IR.Image_Type;
   use type Interfaces.Unsigned_32;

   Zero_UUID : constant AMS.MEL.IR.UUID := (others => 0);
   Channel_ID : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Zero_UUID, "Ada IR channel");
   Platform_ID : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Zero_UUID, "Ada platform");
   Location : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (1.25, -2.5, 3.75,
                                            "station-1", "mock-aircraft");
   Config : constant AMS.MEL.IR.Image_Config := AMS.MEL.IR.Create_Image_Config
     (Channel_ID, Platform_ID, Location, Buffer_Count => 3,
      Buffer_Size => 64, Queue_Capacity => 4);

   procedure Check_Frame (Value : AMS.MEL.IR.Frame; ID : Positive) is
   begin
      if Value.Pixel_Count /= 12 or else Value.Width /= 4 or else Value.Height /= 3
        or else Value.Bits_Per_Pixel /= 8 or else Value.Number_Of_Bands /= 1
        or else Value.Frame_ID /= Interfaces.Unsigned_32 (ID)
        or else Value.Subframe_ID /= 2 or else Value.Subframe_Total /= 4
        or else Value.Kind /= AMS.MEL.IR.Staring
        or else Value.Flip /= AMS.MEL.IR.Horizontal
        or else Value.Image_Flags /= 4 or else Value.Row_Offset /= 7
        or else Value.Column_Offset /= 9 or else Value.Band_Index /= 3
        or else Value.Horizontal_FOV_Rad /= 0.25
        or else Value.Vertical_FOV_Rad /= 0.125
        or else Value.Dither_Row /= 0.5 or else Value.Dither_Column /= -0.25
      then
         raise Program_Error with "Ada frame metadata conversion failed";
      end if;
      for Index in Value.Pixels'Range loop
         if Value.Pixels (Index) /= AMS.MEL.IR.Byte (ID * 16 + Index - 1) then
            raise Program_Error with "Ada frame pixel conversion failed";
         end if;
      end loop;
   end Check_Frame;

   procedure Test_Success (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "ada-ir-success");
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
   begin
      AMS.MEL.IR.Start (Stream);
      declare
         First : constant AMS.MEL.IR.Frame := AMS.MEL.IR.Receive (Stream, 1_000);
         Second : constant AMS.MEL.IR.Frame := AMS.MEL.IR.Receive (Stream, 1_000);
         Counts : constant AMS.MEL.IR.Stream_Counters := AMS.MEL.IR.Counters (Stream);
      begin
         Check_Frame (First, 1); Check_Frame (Second, 2);
         if Counts.Frames_Received < 2
           or else Counts.Malformed_Or_Unsupported /= 0
         then
            raise Program_Error with "Ada stream counters failed";
         end if;
      end;
      AMS.MEL.Close (Parent);
      AMS.MEL.IR.Stop (Stream);
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.IR.Close (Stream);
      if AMS.MEL.IR.Is_Open (Stream) then
         raise Program_Error with "Ada stream close did not clear owner";
      end if;
   end Test_Success;

   procedure Test_Timeout_And_Stop (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "idle");
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
   begin
      AMS.MEL.IR.Start (Stream);
      begin
         declare
            Unexpected : constant AMS.MEL.IR.Frame := AMS.MEL.IR.Receive (Stream, 2);
         begin
            raise Program_Error with "idle stream returned" & Unexpected.Pixel_Count'Image;
         end;
      exception
         when AMS.MEL.IR.Timeout_Error => null;
      end;
      AMS.MEL.IR.Stop (Stream);
      begin
         declare
            Unexpected : constant AMS.MEL.IR.Frame := AMS.MEL.IR.Receive (Stream);
         begin
            raise Program_Error with "stopped stream returned" & Unexpected.Pixel_Count'Image;
         end;
      exception
         when AMS.MEL.IR.Stream_Stopped => null;
      end;
      AMS.MEL.IR.Close (Stream); AMS.MEL.Close (Parent);
   end Test_Timeout_And_Stop;

   procedure Test_Finalization (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "idle");
   begin
      declare
         Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      begin
         AMS.MEL.IR.Start (Stream);
      end;
      AMS.MEL.Close (Parent);
   end Test_Finalization;
   procedure Run (Provider_Path : String) is
   begin
      Test_Success (Provider_Path);
      Test_Timeout_And_Stop (Provider_Path);
      Test_Finalization (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada IR Mono8 receive/timeout/lifetime contract");
   end Run;
end AMS_MEL_IR_Tests;
