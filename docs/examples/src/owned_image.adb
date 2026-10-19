with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Image;
with Example_Support;
with Interfaces;

procedure Owned_Image is
   package IR renames AMS.MEL.IR;
   package Image renames AMS.MEL.IR.Image;
   use Example_Support;
   use type Interfaces.Unsigned_32;
   use type IR.Byte;
begin
   declare
      Zero     : constant IR.UUID := [others => 0];
      Channel  : constant IR.UCI_ID := IR.Create_UCI_ID (Zero, "example image");
      Platform : constant IR.UCI_ID := IR.Create_UCI_ID (Zero, "example platform");
      Location : constant IR.Component_Location :=
        IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock");
      Config   : constant IR.Image_Config :=
        IR.Create_Image_Config (Channel, Platform, Location, 3, 64, 4);
      Session  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Configuration);
      Stream   : IR.Image_Stream := IR.Open_Image_Stream (Session, Config);
   begin
      IR.Start (Stream);
      declare
         Frame : constant Image.Full_Frame := Image.Receive (Stream, 1_000);
      begin
         Require (Image.Width (Frame) = 4 and then Image.Height (Frame) = 3, "mock dimensions");
         Require
           (Image.Bits_Per_Pixel (Frame) = 8 and then Image.Number_Of_Bands (Frame) = 1,
            "Mono8 single band");
         Require
           (Image.System_Time_NS (Frame) = -123456789
            and then Image.Frame_ID (Frame) = 16#FEDCBA98#
            and then Image.Sensor_Nav_State_Count (Frame) = 2
            and then IR.Key (Image.Contributing_Sensor_Value (Frame).Location) = "face-α",
            "rich complete-header metadata");
         IR.Stop (Stream);
         IR.Close (Stream);
         AMS.MEL.Close (Session);
         Require
           (Image.Pixel_Count (Frame) = 12
            and then Image.Pixels (Frame) (1) = 16#A0#
            and then Image.Pixels (Frame) (12) = 16#AB#,
            "owned pixels after Close");
         Ada.Text_IO.Put_Line ("4x3 Mono8; frame=0xFEDCBA98; nav states=2; sensor=face-α");
         Ada.Text_IO.Put_Line ("owned after Close: 12 pixels, first=160 last=171");
      end;
   end;
exception
   when Error : others =>
      Report_Failure (Error);
end Owned_Image;
