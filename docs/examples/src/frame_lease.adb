with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Image;
with Example_Support;

procedure Frame_Lease is
   package IR renames AMS.MEL.IR;
   package Image renames AMS.MEL.IR.Image;
   use Example_Support;
   Sum : Natural := 0;
   procedure Sum_Pixels (Pixels : IR.Pixel_Array) is
   begin
      for Index in Pixels'Range loop
         Sum := Sum + Natural (Pixels (Index));
      end loop;
   end Sum_Pixels;
begin
   declare
      Zero     : constant IR.UUID := [others => 0];
      Channel  : constant IR.UCI_ID := IR.Create_UCI_ID (Zero, "example lease");
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
         Lease : Image.Frame_Lease := Image.Acquire_Frame (Stream, 1_000);
      begin
         Image.With_Pixels (Lease, Sum_Pixels'Access);
         Require (Sum = 258, "borrowed fixture pixel sum");
         declare
            Copy : constant IR.Pixel_Array := Image.Copy_Pixels (Lease);
         begin
            Image.Close (Lease);
            IR.Stop (Stream);
            IR.Close (Stream);
            AMS.MEL.Close (Session);
            Sum := 0;
            Sum_Pixels (Copy);
            Require (Copy'Length = 12 and then Sum = 258, "independent copy after Close");
            Ada.Text_IO.Put_Line
              ("borrow sum=258; independent copy after Close: 12 pixels sum=258");
         end;
      end;
   end;
exception
   when Error : others =>
      Report_Failure (Error);
end Frame_Lease;
