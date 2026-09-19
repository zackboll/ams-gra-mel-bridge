with Ada.Environment_Variables;
with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Image;
with AMS.MEL.IR.Image.Metadata;
with AMS.MEL.Status;
with Interfaces;

package body AMS_MEL_IR_Image_Metadata_Tests is
   package Channel renames AMS.MEL.IR.Channel;
   package Image renames AMS.MEL.IR.Image;
   package Metadata renames AMS.MEL.IR.Image.Metadata;
   use type AMS.MEL.IR.Counter;
   use type Channel.Metadata_Capability;
   use type Channel.Pixel_Format;
   use type Interfaces.Unsigned_32;
   use type Metadata.Azimuth_Elevation;
   use type Metadata.Metadata_Kind;
   use type AMS.MEL.Status.Euler;

   Zero       : constant AMS.MEL.IR.UUID := [others => 0];
   Channel_ID : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Zero, "Image metadata channel");
   Platform   : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Zero, "Image metadata platform");
   Location   : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock");
   Config     : constant AMS.MEL.IR.Image_Config :=
     AMS.MEL.IR.Create_Image_Config
       (Channel_ID, Platform, Location, Buffer_Count => 3, Buffer_Size => 64, Queue_Capacity => 2);

   procedure Check_Rich_Event (Event : Metadata.Metadata_Event; First_Row : Interfaces.Unsigned_32)
   is
      Value  : constant Metadata.Bad_Pixel_List := Metadata.Bad_Pixel_List_Value (Event);
      First  : constant Metadata.Bad_Pixel := Metadata.Pixel_At (Value, 1);
      Second : constant Metadata.Bad_Pixel := Metadata.Pixel_At (Value, 2);
      Third  : constant Metadata.Bad_Pixel := Metadata.Pixel_At (Value, 3);
   begin
      if Metadata.Metadata_Kind'Image (Metadata.Kind (Event)) /= "BAD_PIXEL_LIST_EVENT"
        or else Metadata.Reported_Size (Value) /= 16#A5A5_0001#
        or else Metadata.Reported_Count (Value) /= 16#5A5A_0003#
        or else Metadata.Pixel_Count (Value) /= 3
        or else First.Row /= First_Row
        or else First.Column /= 16#8000_0001#
        or else Metadata.Bad_Pixel_Reason'Image (First.Reason) /= "UNKNOWN"
        or else Second.Row /= 16#F000_0002#
        or else Metadata.Bad_Pixel_Reason'Image (Second.Reason) /= "UNKNOWN"
        or else Third.Row /= First_Row + 122
        or else Third.Column /= 456
      then
         raise Program_Error with "Ada BadPixel value conversion failed";
      end if;
   end Check_Rich_Event;

   procedure Test_Capability (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "image-capability-rich");
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Value  : constant Channel.Channel_Capability := Image.Capabilities (Stream);
   begin
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
      if Channel.Width (Value) /= 320
        or else Channel.Height (Value) /= 200
        or else Channel.Bit_Depth (Value) /= 8
        or else Channel.Number_Of_Bands (Value) /= 1
        or else Channel.Format (Value) /= Channel.Mono
        or else not Channel.Has_Metadata_Capability (Value, Channel.Bad_Pixel_List)
      then
         raise Program_Error with "Ada Image capability conversion failed";
      end if;
   end Test_Capability;

   procedure Test_Rich_Lifetime (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "image-metadata-sync");
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Queue  : Metadata.Metadata_Stream := Metadata.Open (Stream, 2);
      Event  : constant Metadata.Metadata_Event := Metadata.Receive (Queue);
   begin
      Metadata.Close (Queue);
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
      Check_Rich_Event (Event, 1);
   end Test_Rich_Lifetime;

   procedure Check_Report (Event : Metadata.Metadata_Event) is
      Value : constant Metadata.Line_Of_Sight_Report := Metadata.Line_Of_Sight_Report_Value (Event);
   begin
      if Metadata.Kind (Event) /= Metadata.Line_Of_Sight_Report_Event
        or else Value.System_Time_NS /= -123456789
        or else Value.Pointing_Angle /= (1.25, -2.5)
        or else Value.Pointing_Angle_Rates /= (0.125, -0.25)
        or else not Value.At_Speed
        or else Value.In_Tolerance
        or else Value.Platform_Attitude /= (0.5, -0.75, 1.0)
        or else Value.Validity_Flag_Bitfield /= 16#A5A5_0003#
        or else Value.Image_Rotation_Rad /= -1.5
      then
         raise Program_Error with "Ada LineOfSightReport conversion failed";
      end if;
   end Check_Report;

   procedure Check_Euler (Event : Metadata.Metadata_Event) is
      Value : constant Metadata.Line_Of_Sight_Euler := Metadata.Line_Of_Sight_Euler_Value (Event);
   begin
      if Metadata.Kind (Event) /= Metadata.Line_Of_Sight_Euler_Event
        or else Value.System_Time_NS /= 987654321
        or else Value.Attitude /= (-0.5, 1.25, -2.0)
        or else Value.Attitude_Rates /= (0.25, -0.125, 0.0625)
      then
         raise Program_Error with "Ada LineOfSightEuler conversion failed";
      end if;
   end Check_Euler;

   procedure Test_LOS_Rich_Lifetime (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "image-metadata-los-rich");
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Queue  : Metadata.Metadata_Stream := Metadata.Open (Stream, 4);
      Report : constant Metadata.Metadata_Event := Metadata.Receive (Queue);
      Euler  : constant Metadata.Metadata_Event := Metadata.Receive (Queue);
   begin
      Metadata.Close (Queue);
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
      Check_Report (Report);
      Check_Euler (Euler);
   end Test_LOS_Rich_Lifetime;

   procedure Test_Mixed_LOS_FIFO (Provider_Path : String) is
      Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "image-metadata-los-mixed");
      Stream   : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Queue    : Metadata.Metadata_Stream := Metadata.Open (Stream, 2);
      First    : constant Metadata.Metadata_Event := Metadata.Receive (Queue);
      Second   : constant Metadata.Metadata_Event := Metadata.Receive (Queue);
      Counters : constant Metadata.Metadata_Counters := Metadata.Counters (Queue);
   begin
      if Counters.Events_Received /= 5
        or else Counters.Events_Dropped_Queue_Full /= 3
        or else Counters.Malformed_Or_Unsupported /= 0
      then
         raise Program_Error with "Ada mixed Image metadata FIFO counters failed";
      end if;
      Check_Rich_Event (First, 1);
      Check_Report (Second);
      Metadata.Close (Queue);
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Test_Mixed_LOS_FIFO;

   procedure Test_LOS_Malformed_Recovery (Provider_Path : String) is
      Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "image-metadata-los-report-null");
      Stream   : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Queue    : Metadata.Metadata_Stream := Metadata.Open (Stream, 2);
      Event    : constant Metadata.Metadata_Event := Metadata.Receive (Queue);
      Counters : constant Metadata.Metadata_Counters := Metadata.Counters (Queue);
   begin
      if Counters.Events_Received /= 2
        or else Counters.Events_Dropped_Queue_Full /= 0
        or else Counters.Malformed_Or_Unsupported /= 1
      then
         raise Program_Error with "Ada malformed LOS counters failed";
      end if;
      Check_Report (Event);
      Metadata.Close (Queue);
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Test_LOS_Malformed_Recovery;

   procedure Test_LOS_Allocation_Recovery (Provider_Path : String) is
      Parent : AMS.MEL.Session :=
        AMS.MEL.Open (Provider_Path, "image-metadata-los-report-allocation");
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
   begin
      Ada.Environment_Variables.Set ("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE", "allocation");
      declare
         Queue    : Metadata.Metadata_Stream := Metadata.Open (Stream, 2);
         Event    : constant Metadata.Metadata_Event := Metadata.Receive (Queue);
         Counters : constant Metadata.Metadata_Counters := Metadata.Counters (Queue);
      begin
         Ada.Environment_Variables.Clear ("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE");
         if Counters.Events_Received /= 2 or else Counters.Malformed_Or_Unsupported /= 1 then
            raise Program_Error with "Ada LOS allocation recovery counters failed";
         end if;
         Check_Report (Event);
         Metadata.Close (Queue);
      end;
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   exception
      when others =>
         Ada.Environment_Variables.Clear ("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE");
         raise;
   end Test_LOS_Allocation_Recovery;

   procedure Test_Malformed_Recovery (Provider_Path : String) is
      Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "image-metadata-malformed");
      Stream   : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Queue    : Metadata.Metadata_Stream := Metadata.Open (Stream, 2);
      Event    : constant Metadata.Metadata_Event := Metadata.Receive (Queue);
      Counters : constant Metadata.Metadata_Counters := Metadata.Counters (Queue);
   begin
      if Counters.Events_Received /= 2
        or else Counters.Events_Dropped_Queue_Full /= 0
        or else Counters.Malformed_Or_Unsupported /= 1
      then
         raise Program_Error with "Ada malformed BadPixel counters failed";
      end if;
      Check_Rich_Event (Event, 1);
      Metadata.Close (Queue);
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Test_Malformed_Recovery;

   procedure Test_Allocation_Recovery (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "image-metadata-allocation");
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
   begin
      Ada.Environment_Variables.Set ("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE", "allocation");
      declare
         Queue    : Metadata.Metadata_Stream := Metadata.Open (Stream, 2);
         Event    : constant Metadata.Metadata_Event := Metadata.Receive (Queue);
         Counters : constant Metadata.Metadata_Counters := Metadata.Counters (Queue);
      begin
         Ada.Environment_Variables.Clear ("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE");
         if Counters.Events_Received /= 2 or else Counters.Malformed_Or_Unsupported /= 1 then
            raise Program_Error with "Ada allocation-failure recovery counters failed";
         end if;
         Check_Rich_Event (Event, 10);
         Metadata.Close (Queue);
      end;
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   exception
      when others =>
         Ada.Environment_Variables.Clear ("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE");
         raise;
   end Test_Allocation_Recovery;

   procedure Test_Overflow (Provider_Path : String) is
      Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "image-metadata-overflow");
      Stream   : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Queue    : Metadata.Metadata_Stream := Metadata.Open (Stream, 2);
      First    : constant Metadata.Metadata_Event := Metadata.Receive (Queue);
      Second   : constant Metadata.Metadata_Event := Metadata.Receive (Queue);
      Counters : constant Metadata.Metadata_Counters := Metadata.Counters (Queue);
   begin
      if Counters.Events_Received /= 5
        or else Counters.Events_Dropped_Queue_Full /= 3
        or else Counters.Malformed_Or_Unsupported /= 0
      then
         raise Program_Error with "Ada BadPixel DROP-INCOMING counters failed";
      end if;
      Check_Rich_Event (First, 1);
      Check_Rich_Event (Second, 2);
      Metadata.Close (Queue);
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Test_Overflow;

   procedure Run (Provider_Path : String) is
   begin
      Test_Capability (Provider_Path);
      Test_Rich_Lifetime (Provider_Path);
      Test_LOS_Rich_Lifetime (Provider_Path);
      Test_Mixed_LOS_FIFO (Provider_Path);
      Test_LOS_Malformed_Recovery (Provider_Path);
      Test_LOS_Allocation_Recovery (Provider_Path);
      Test_Malformed_Recovery (Provider_Path);
      Test_Allocation_Recovery (Provider_Path);
      Test_Overflow (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada IR Image metadata contract");
   end Run;
end AMS_MEL_IR_Image_Metadata_Tests;
