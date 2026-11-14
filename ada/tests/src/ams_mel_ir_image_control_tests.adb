with Ada.Exceptions;
with Ada.Strings.Fixed;
with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Discovery;
with AMS.MEL.IR.Image;
with AMS.MEL.IR.Image.Metadata;
with Interfaces;

package body AMS_MEL_IR_Image_Control_Tests is
   package IR renames AMS.MEL.IR;
   package Image renames IR.Image;
   package Channel renames IR.Channel;
   package Metadata renames Image.Metadata;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Unsigned_8;
   use type Channel.Outcome;
   use type Channel.Channel_Capability;
   use type Image.Navigation_Outcome;
   use type Image.Navigation_Error_Code;

   ID       : constant IR.UCI_ID := IR.Create_UCI_ID ([others => 0], "control-only");
   Location : constant IR.Component_Location :=
     IR.Create_Component_Location (1.25, -2.5, 3.75, "station", "mock");
   Config   : constant IR.Image_Control_Config := IR.Create_Image_Control_Config (ID, ID, Location);
   Report   : constant Image.Navigation_Report :=
     (System_Time_NS               => 0,
      State                        => Image.Blended,
      Latitude_Rad                 => 0.0,
      Longitude_Rad                => 0.0,
      Altitude_M                   => 0.0,
      Attitude                     => (others => 0.0),
      Attitude_Rate                => (Value => (others => 0.0), System_Time_NS => 0),
      Speed                        => (others => 0.0),
      Acceleration                 => (others => 0.0),
      Wander_Angle_Rad             => 0.0,
      Magnetic_Heading             => 0.0,
      Altitude_MSL                 => 0.0,
      Position_Velocity_Covariance => (others => 0.0));

   procedure Test_Control (Provider_Path, Scenario : String) is
      Parent     : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Advertised : constant IR.Discovery.Capability_Vectors.Vector :=
        IR.Discovery.Capabilities (Parent);
      Selected   : constant IR.Image_Control_Config :=
        IR.Create_Image_Control_Config
          (Channel.Channel_ID (Advertised.First_Element), ID, Location);
      Stream     : IR.Image_Stream := IR.Open_Image_Control (Parent, Selected);
      Capability : constant Channel.Channel_Capability := Image.Capabilities (Stream);
      View       : Channel.View := Image.As_Channel (Stream);
      Queue      : Metadata.Metadata_Stream := Metadata.Open (Stream, 8);
      Keep       : Channel.Return_Request := Channel.Send_Keep_Alive (View);
      Comms      : Channel.Comms_Request :=
        Channel.Submit_Comms_Test (View, 16#F000_0002#, 16#8000_0001#, 16#E000_0003#);
      Nav        : Image.Navigation_Request := Image.Submit_Navigation_Report (Stream, Report);
      Result     : constant Image.Navigation_Result := Image.Wait (Nav, 1_000);
   begin
      if Capability /= Advertised.First_Element
        or else Channel.Width (Capability) /= 0
        or else Channel.Height (Capability) /= 0
        or else Channel.Bit_Depth (Capability) /= 0
        or else Channel.Number_Of_Bands (Capability) /= 0
        or else Channel.Row_Pitch (Capability) /= 0
        or else Channel.Buffer_Size (Capability) /= 0
        or else Channel.Image_Size (Capability) /= 0
        or else Channel.Task_Schedule_Depth (Capability) /= 17
        or else Channel.Image_Band_Count (Capability) /= 2
        or else Channel.Nav_Frame_Count (Capability) /= 2
        or else IR.UUID_Value (Channel.Channel_ID (Capability)) (0) /= 16#11#
      then
         raise Program_Error with "control-only capability snapshot changed";
      end if;
      for Kind in Metadata.Metadata_Kind loop
         declare
            Event : constant Metadata.Metadata_Event := Metadata.Receive (Queue);
         begin
            if Metadata.Metadata_Kind'Pos (Metadata.Kind (Event))
              /= Metadata.Metadata_Kind'Pos (Kind)
            then
               raise Program_Error with "control-only metadata registration/reentry";
            end if;
         end;
      end loop;
      if Image.Status (Result) /= Image.Rejected
        or else Image.Rejection_Code (Result) /= Image.Invalid_State
        or else Image.Description (Result) /= "no navigation hardware"
        or else Channel.Status (Channel.Wait (Keep, 1_000)) /= Channel.Success
        or else Channel.Status (Channel.Wait (Comms, 1_000)) /= Channel.Success
      then
         raise Program_Error with "control-only request outcome";
      end if;
      Image.Close (Nav);
      Channel.Close (Keep);
      Channel.Close (Comms);
      IR.Start (Stream);
      begin
         declare
            Frame : constant IR.Frame := IR.Receive (Stream);
         begin
            raise Program_Error
              with "legacy receive accepted control-only" & Frame.Pixel_Count'Image;
         end;
      exception
         when Error : AMS.MEL.Provider_Error =>
            if Ada.Strings.Fixed.Index (Ada.Exceptions.Exception_Message (Error), "control-only")
              = 0
            then
               raise;
            end if;
      end;
      begin
         declare
            Lease : Image.Frame_Lease := Image.Acquire_Frame (Stream);
         begin
            Image.Close (Lease);
            raise Program_Error with "frame lease accepted control-only";
         end;
      exception
         when Error : AMS.MEL.Provider_Error =>
            if Ada.Strings.Fixed.Index (Ada.Exceptions.Exception_Message (Error), "control-only")
              = 0
            then
               raise;
            end if;
      end;
      Metadata.Close (Queue);
      Channel.Close (View);
      IR.Close (Stream);
      AMS.MEL.Close (Parent);
      if Channel.Task_Schedule_Depth (Capability) /= 17 then
         raise Program_Error with "capability did not outlive owner";
      end if;
   end Test_Control;

   procedure Test_Pending (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "navigation-lifetime");
      Stream : IR.Image_Stream := IR.Open_Image_Control (Parent, Config);
      Queue  : Metadata.Metadata_Stream := Metadata.Open (Stream, 4);
      Nav    : Image.Navigation_Request := Image.Submit_Navigation_Report (Stream, Report);
   begin
      AMS.MEL.Close (Parent);
      IR.Close (Stream);
      Metadata.Close (Queue);
      if Image.Status (Image.Wait (Nav, 1_000)) /= Image.Success then
         raise Program_Error with "control-only pending navigation cleanup";
      end if;
      Image.Close (Nav);
   end Test_Pending;

   procedure Run (Provider_Path : String) is
   begin
      Test_Control (Provider_Path, "image-control-zero");
      Test_Control (Provider_Path, "image-control-unexpected");
      Test_Pending (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada Image control-only attachment contract");
   end Run;
end AMS_MEL_IR_Image_Control_Tests;
