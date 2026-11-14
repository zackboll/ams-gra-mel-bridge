with Ada.Containers;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Discovery;
with Interfaces;
with Interfaces.C;
with System;

package body AMS_MEL_IR_Discovery_Tests is
   package IR renames AMS.MEL.IR;
   package V renames AMS.MEL.IR.Channel;
   package D renames AMS.MEL.IR.Discovery;
   use type Ada.Containers.Count_Type;
   use type Interfaces.Unsigned_32;
   use type IR.UUID;
   use type V.Channel_Type;
   use type V.Pixel_Format;
   use type V.Sensor_Type;
   use type V.Metadata_Capability;
   use type V.Band_Type;
   use type V.Coordinate_System_Type;
   use type D.Capability_Vectors.Vector;
   use type System.Address;
   use type Interfaces.C.int;

   --  Test-only loader observation, not an application-side native dependency.
   --  RTLD_NOLOAD must find nothing before we inspect the Ada-owned values.
   function Dlopen (Path : Interfaces.C.char_array; Flags : Interfaces.C.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function Dlclose (Handle : System.Address) return Interfaces.C.int
   with Import, Convention => C, External_Name => "dlclose";

   procedure Require (Condition : Boolean; Message : String) is
   begin
      if not Condition then
         raise Program_Error with Message;
      end if;
   end Require;

   procedure Run (Library_Path : String) is
      Types : constant array (Positive range 1 .. 4) of V.Channel_Type :=
        [V.Command_And_Control, V.Health_And_Status, V.IRST_Image, V.Stacked_Image];

      procedure Exercise (Rich : Boolean) is
         Session : AMS.MEL.Session :=
           AMS.MEL.Open (Library_Path, (if Rich then "discovery-rich" else "discovery-zero"));
         Values  : constant D.Capability_Vectors.Vector := D.Capabilities (Session);
      begin
         Require (Values.Length = 4, "complete Control list");
         Require (D.Capabilities (Session) = Values, "repeat discovery changed values");
         AMS.MEL.Close (Session);
         declare
            Handle : constant System.Address := Dlopen (Interfaces.C.To_C (Library_Path), 6);
         begin
            if Handle /= System.Null_Address then
               Require (Dlclose (Handle) = 0, "loader observer cleanup");
               raise Program_Error with "discovery retained provider library";
            end if;
         end;
         --  Native list/entry owners have already closed inside Capabilities.
         --  Inspect nested Ada values only after the last provider owner closes.
         for I in Types'Range loop
            declare
               Value    : constant V.Channel_Capability := Values (I);
               Expected : IR.UUID;
               Digit    : constant Character := Character'Val (Character'Pos ('0') + I - 1);
            begin
               for J in Expected'Range loop
                  Expected (J) := Interfaces.Unsigned_8 (16#10# + I - 1 + J * 7);
               end loop;
               Require (IR.UUID_Value (V.Channel_ID (Value)) = Expected, "UUID bytes");
               Require
                 (IR.Descriptive_Label (V.Channel_ID (Value))
                  = "discovery-" & Digit & "-" & Character'Val (16#CE#) & Character'Val (16#B1#),
                  "exact UTF-8 label");
               Require
                 (V.Channel_Type_Count (Value) = (if Rich then 3 else 1)
                  and then V.Channel_Type_At (Value, 1) = Types (I),
                  "enumeration order/type");
               if Rich then
                  Require
                    (V.Channel_Type_At (Value, 2) = V.Instrumentation
                     and then V.Channel_Type_At (Value, 3) = V.Reserved_2,
                     "remaining channel types");
                  Require
                    (V.Height (Value) = 1080
                     and V.Width (Value) = 1920
                     and V.Bit_Depth (Value) = 12
                     and V.Row_Pitch (Value) = 4096
                     and V.Buffer_Size (Value) = 8_388_608
                     and V.Image_Size (Value) = 4_147_200
                     and V.Number_Of_Bands (Value) = 3
                     and V.Format (Value) = V.RGB,
                     "image scalars");
                  Require
                    (V.Sensor_Type_Count (Value) = 2
                     and then V.Sensor_Type_At (Value, 1) = V.Gimbal_Horizontal
                     and then V.Sensor_Type_At (Value, 2) = V.Step_Stare,
                     "sensor vector");
                  Require
                    (V.Metadata_Capability_Count (Value) = 4
                     and then V.Metadata_Capability_At (Value, 1) = V.Bad_Pixel_List
                     and then V.Metadata_Capability_At (Value, 2) = V.Command_Status
                     and then V.Metadata_Capability_At (Value, 3) = V.Channel_Comms_Test_Rep
                     and then V.Metadata_Capability_At (Value, 4) = V.Reserved_10,
                     "metadata set");
                  Require
                    (V.Image_Band_Count (Value) = 2
                     and then V.Image_Band_Index_At (Value, 1) = 2
                     and then V.Image_Band_Info_Count (Value, 1) = 2
                     and then V.Image_Band_Info_At (Value, 1, 1).Kind = V.IR_Longwave
                     and then V.Image_Band_Info_At (Value, 1, 2).Min_Wavelength_M = 3.0E-6
                     and then V.Image_Band_Info_At (Value, 1, 2).Max_Wavelength_M = 5.0E-6
                     and then V.Image_Band_Index_At (Value, 2) = 9
                     and then V.Image_Band_Info_At (Value, 2, 1).Kind = V.Visible_Red,
                     "nested bands");
                  Require
                    (V.Nav_Frame_Count (Value) = 2
                     and then V.Nav_Frame_At (Value, 1) = V.NED_Sensor
                     and then V.Nav_Frame_At (Value, 2) = V.ECEF,
                     "nav frames");
                  Require
                    (V.Task_Schedule_Depth (Value) = 17
                     and V.ODC_Available (Value)
                     and V.NUC_Available (Value),
                     "flags/schedule");
                  Require
                    (IR.Descriptive_Label (V.Platform_ID (Value))
                     = "platform-"
                       & Character'Val (16#E2#)
                       & Character'Val (16#82#)
                       & Character'Val (16#AC#),
                     "platform label");
               else
                  Require
                    (V.Height (Value) = 0
                     and V.Width (Value) = 0
                     and V.Bit_Depth (Value) = 0
                     and V.Row_Pitch (Value) = 0
                     and V.Buffer_Size (Value) = 0
                     and V.Image_Size (Value) = 0
                     and V.Number_Of_Bands (Value) = 0,
                     "unconfigured image preserved");
               end if;
            end;
         end loop;
      end Exercise;

      procedure Failure (Scenario : String) is
         Session : AMS.MEL.Session := AMS.MEL.Open (Library_Path, Scenario);
      begin
         begin
            declare
               Unexpected : constant D.Capability_Vectors.Vector := D.Capabilities (Session);
            begin
               raise Program_Error with "discovery accepted " & Scenario & Unexpected.Length'Image;
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Require (AMS.MEL.Is_Open (Session), "discovery failed Session");
         AMS.MEL.Close (Session);
      end Failure;
   begin
      Exercise (False);
      Exercise (True);
      declare
         Session : AMS.MEL.Session := AMS.MEL.Open (Library_Path, "discovery-empty");
      begin
         Require (D.Capabilities (Session).Is_Empty, "empty list rejected");
         AMS.MEL.Close (Session);
         begin
            declare
               Unexpected : constant D.Capability_Vectors.Vector := D.Capabilities (Session);
            begin
               raise Program_Error with "closed discovery accepted" & Unexpected.Length'Image;
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
      end;
      Failure ("discovery-malformed");
      Failure ("discovery-bad-label");
      Failure ("discovery-throw");
      Failure ("discovery-unknown-throw");
      Failure ("discovery-bad-alloc");
   end Run;
end AMS_MEL_IR_Discovery_Tests;
