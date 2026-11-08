pragma Validity_Checks ("F");
--  Only optional floating validity checks are disabled in this fixture, so the intentional IEEE
--  infinities, NaN and signed zero delivered by the mock can be inspected; all other validity
--  and range checks remain active.

with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with Interfaces;
with Interfaces.C;
with System;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Image.Metadata;
with AMS.MEL.IR.Optical_Distortion_Map;
with AMS.MEL.IR.Optical_Distortion_Map.Testing;

package body AMS_MEL_IR_Image_ODM_Tests is
   package Metadata renames AMS.MEL.IR.Image.Metadata;
   package ODM renames AMS.MEL.IR.Optical_Distortion_Map;
   package IC renames Interfaces.C;
   use type AMS.MEL.IR.Counter;
   use type Metadata.Metadata_Kind;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Unsigned_64;
   use type IC.int;
   use type IC.unsigned;
   use type System.Address;

   Zero       : constant AMS.MEL.IR.UUID := [others => 0];
   Channel_ID : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "Ada ODM channel");
   Platform   : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "Ada ODM platform");
   Location   : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock");
   Config     : constant AMS.MEL.IR.Image_Config :=
     AMS.MEL.IR.Create_Image_Config
       (Channel_ID, Platform, Location, Buffer_Count => 3, Buffer_Size => 64, Queue_Capacity => 2);

   function Dlopen (Path : IC.char_array; Flags : IC.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function Dlsym (Handle : System.Address; Name : IC.char_array) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   function Dlclose (Handle : System.Address) return IC.int
   with Import, Convention => C, External_Name => "dlclose";
   type Deliver_Function is
     access function
       (Fixture : IC.unsigned; Tag : Interfaces.Unsigned_32; Mode : IC.unsigned) return IC.int
   with Convention => C;
   type Registrations_Function is access function return IC.unsigned with Convention => C;
   type Probe_Function is access function (Operation : IC.unsigned; Value : IC.size_t) return IC.int
   with Convention => C;
   function To_Deliver is new Ada.Unchecked_Conversion (System.Address, Deliver_Function);
   function To_Registrations is new
     Ada.Unchecked_Conversion (System.Address, Registrations_Function);
   function To_Probe is new Ada.Unchecked_Conversion (System.Address, Probe_Function);
   function Bits is new Ada.Unchecked_Conversion (Long_Float, Interfaces.Unsigned_64);

   procedure Require (Value : Boolean; Text : String) is
   begin
      if not Value then
         raise Program_Error with "Ada Image OpticalDistortionMap: " & Text;
      end if;
   end Require;

   --  Bit-exact comparison, so signed zero is distinguished from zero.
   function Same (Actual, Expected : Long_Float) return Boolean
   is (Bits (Actual) = Bits (Expected));

   --  Mock delivery on the calling thread; the provider is pinned only for the call.
   procedure Deliver
     (Provider_Path : String;
      Fixture       : Natural;
      Tag           : Interfaces.Unsigned_32;
      Null_Payload  : Boolean := False)
   is
      Pin  : constant System.Address := Dlopen (IC.To_C (Provider_Path), 2);
      Call : constant Deliver_Function := To_Deliver (Dlsym (Pin, IC.To_C ("mock_odm_deliver")));
   begin
      Require (Pin /= System.Null_Address and then Call /= null, "mock delivery binding");
      Require
        (Call (IC.unsigned (Fixture), Tag, (if Null_Payload then 1 else 0)) = 1,
         "mock delivered to a registered callback");
      Require (Dlclose (Pin) = 0, "delivery unpin");
   end Deliver;

   function Registrations (Provider_Path : String) return Natural is
      Pin    : constant System.Address := Dlopen (IC.To_C (Provider_Path), 2);
      Call   : constant Registrations_Function :=
        To_Registrations (Dlsym (Pin, IC.To_C ("mock_odm_registrations")));
      Result : Natural;
   begin
      Require (Pin /= System.Null_Address and then Call /= null, "mock registration counter");
      Result := Natural (Call.all);
      Require (Dlclose (Pin) = 0, "counter unpin");
      return Result;
   end Registrations;

   function Probe (Operation : Natural; Value : Natural := 0) return Integer is
      Call : constant Probe_Function :=
        To_Probe (Dlsym (System.Null_Address, IC.To_C ("ams_mel_test_image_odm_probe")));
   begin
      Require (Call /= null, "test-facade ODM probe binding");
      return Integer (Call (IC.unsigned (Operation), IC.size_t (Value)));
   end Probe;

   procedure Counters_Are
     (Stream : Metadata.Optical_Distortion_Map_Stream; Received, Dropped, Malformed : Natural)
   is
      Count : constant Metadata.Metadata_Counters :=
        Metadata.Optical_Distortion_Map_Counters (Stream);
   begin
      Require
        (Count.Events_Received = AMS.MEL.IR.Counter (Received)
         and then Count.Events_Dropped_Queue_Full = AMS.MEL.IR.Counter (Dropped)
         and then Count.Malformed_Or_Unsupported = AMS.MEL.IR.Counter (Malformed),
         "counters");
   end Counters_Are;

   procedure Check_Empty (Value : ODM.Map_Value) is
   begin
      Require (ODM.Width (Value) = 0 and then ODM.Height (Value) = 0, "empty dimensions");
      Require (ODM.Right_Size (Value) = 0 and then ODM.Down_Size (Value) = 0, "empty grid");
      Require (ODM.Pixels_Per_ODM (Value) = 0, "empty pixels per ODM");
      Require
        (Same (ODM.Right_Min (Value), 0.0) and then Same (ODM.Down_Max (Value), 0.0),
         "empty extrema");
      Require
        (ODM.FRD_Count (Value) = 0 and then ODM.RowCol_Count (Value) = 0, "empty collections");
   end Check_Empty;

   procedure Check_FRD (Value : ODM.Map_Value; Index : Positive; Forward, Right, Down : Long_Float)
   is
      E : constant ODM.Forward_Right_Down := ODM.FRD_At (Value, Index);
   begin
      Require (Same (E.Forward, Forward), "FRD forward" & Index'Image);
      Require (Same (E.Right, Right), "FRD right" & Index'Image);
      Require (Same (E.Down, Down), "FRD down" & Index'Image);
   end Check_FRD;

   procedure Check_RowCol (Value : ODM.Map_Value; Index : Positive; Row, Column : Long_Float) is
      E : constant ODM.Row_Column := ODM.RowCol_At (Value, Index);
   begin
      Require (Same (E.Row, Row), "RowCol row" & Index'Image);
      Require (Same (E.Column, Column), "RowCol column" & Index'Image);
   end Check_RowCol;

   --  Fixture 1: uint32 maximum/zero, signed zero, extremes and a subnormal extremum; four FRD
   --  samples with an adjacent duplicate and three RowCol samples: lengths differ.
   procedure Check_Rich (Value : ODM.Map_Value) is
   begin
      Require (ODM.Width (Value) = 16#FFFF_FFFF# and then ODM.Height (Value) = 0, "rich size");
      Require (ODM.Right_Size (Value) = 16#8000_0001# and then ODM.Down_Size (Value) = 1, "grid");
      Require (ODM.Pixels_Per_ODM (Value) = 16#7FFF_FFFF#, "rich pixels per ODM");
      Require (Same (ODM.Right_Min (Value), -0.0), "signed zero right minimum");
      Require (Same (ODM.Right_Max (Value), Long_Float'Last), "largest right maximum");
      Require (Same (ODM.Down_Min (Value), -Long_Float'Last), "most negative down minimum");
      Require (Bits (ODM.Down_Max (Value)) = 1, "smallest subnormal down maximum");
      Require (ODM.FRD_Count (Value) = 4 and then ODM.RowCol_Count (Value) = 3, "rich lengths");
      Check_FRD (Value, 1, 1.0, 0.0, 0.0);
      Check_FRD (Value, 2, 0.5, -0.25, 0.125);
      Check_FRD (Value, 3, 0.5, -0.25, 0.125);
      Check_FRD (Value, 4, -0.75, 1.0E-300, -2.5);
      Check_RowCol (Value, 1, 0.0, 639.5);
      Check_RowCol (Value, 2, 479.25, -0.0);
      Check_RowCol (Value, 3, 1.0E300, -1.0E-300);
   end Check_Rich;

   procedure Check_Tagged (Value : ODM.Map_Value; Tag : Interfaces.Unsigned_32) is
   begin
      Require (ODM.Width (Value) = Tag and then ODM.Height (Value) = 6, "tagged dimensions");
      Require
        (Same (ODM.Down_Min (Value), -6.5) and then Same (ODM.Down_Max (Value), 6.5),
         "tagged extrema");
      Require (ODM.FRD_Count (Value) = 1 and then ODM.RowCol_Count (Value) = 1, "tagged lengths");
      Check_FRD (Value, 1, Long_Float (Tag), 1.0, 2.0);
      Check_RowCol (Value, 1, Long_Float (Tag), 3.0);
   end Check_Tagged;

   --  Synchronous registration delivery and full fidelity of empty, rich, FRD-only, RowCol-only
   --  and non-finite maps; values outlive the owner, stream and Session.
   procedure Test_Fidelity (Provider_Path : String) is
      Empty, Rich, FRD_Only, RowCol_Only, Non_Finite : ODM.Map_Value;
   begin
      declare
         Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "odm-sync");
         Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
         Queue  : Metadata.Optical_Distortion_Map_Stream :=
           Metadata.Open_Optical_Distortion_Map (Stream, 8);
      begin
         Counters_Are (Queue, 4, 0, 0);
         Deliver (Provider_Path, 4, 0);
         Empty := Metadata.Receive_Optical_Distortion_Map (Queue);
         Rich := Metadata.Receive_Optical_Distortion_Map (Queue);
         FRD_Only := Metadata.Receive_Optical_Distortion_Map (Queue);
         RowCol_Only := Metadata.Receive_Optical_Distortion_Map (Queue);
         Non_Finite := Metadata.Receive_Optical_Distortion_Map (Queue);
         Metadata.Close (Queue);
         Metadata.Close (Queue);
         AMS.MEL.IR.Close (Stream);
         AMS.MEL.Close (Parent);
      end;
      Check_Empty (Empty);
      Check_Rich (Rich);
      Require (ODM.Width (FRD_Only) = 640 and then ODM.Pixels_Per_ODM (FRD_Only) = 20, "FRD-only");
      Require (ODM.FRD_Count (FRD_Only) = 1 and then ODM.RowCol_Count (FRD_Only) = 0, "FRD-only n");
      Check_FRD (FRD_Only, 1, 0.25, 0.5, 0.75);
      Require (ODM.FRD_Count (RowCol_Only) = 0 and then ODM.RowCol_Count (RowCol_Only) = 1, "RC n");
      Require
        (Same (ODM.Right_Min (RowCol_Only), 1.0) and then Same (ODM.Down_Max (RowCol_Only), 4.0),
         "RowCol-only extrema");
      Check_RowCol (RowCol_Only, 1, 12.5, 7.25);
      --  Non-finite provider values are carried, never replaced by zero.
      Require
        (not ODM.Right_Min (Non_Finite)'Valid and then ODM.Right_Min (Non_Finite) > 0.0,
         "positive infinity extremum");
      Require
        (not ODM.Right_Max (Non_Finite)'Valid and then ODM.Right_Max (Non_Finite) < 0.0,
         "negative infinity extremum");
      Require (ODM.Down_Min (Non_Finite) /= ODM.Down_Min (Non_Finite), "NaN extremum");
      Require (Same (ODM.Down_Max (Non_Finite), -0.0), "signed zero extremum");
      Require (ODM.FRD_Count (Non_Finite) = 1 and then ODM.RowCol_Count (Non_Finite) = 2, "NF n");
      declare
         E : constant ODM.Forward_Right_Down := ODM.FRD_At (Non_Finite, 1);
         R : constant ODM.Row_Column := ODM.RowCol_At (Non_Finite, 1);
      begin
         Require
           (E.Forward /= E.Forward
            and then not E.Right'Valid
            and then E.Right > 0.0
            and then not E.Down'Valid
            and then E.Down < 0.0,
            "non-finite FRD");
         Require (not R.Row'Valid and then R.Row < 0.0 and then R.Column /= R.Column, "NF RowCol");
      end;
      Check_RowCol (Non_Finite, 2, 0.0, -0.0);
      --  One-based access beyond each independent length fails closed.
      begin
         declare
            Ignored : constant ODM.Forward_Right_Down := ODM.FRD_At (Rich, 5);
         begin
            raise Program_Error with "FRD index beyond length was accepted";
         end;
      exception
         when Constraint_Error =>
            null;
      end;
      begin
         declare
            Ignored : constant ODM.Row_Column := ODM.RowCol_At (Rich, 4);
         begin
            raise Program_Error with "RowCol index beyond length was accepted";
         end;
      exception
         when Constraint_Error =>
            null;
      end;
   end Test_Fidelity;

   --  Provider Fail or exception: Provider_Error (not Not_Supported), no owner, one-shot.
   procedure Expect_Failure (Provider_Path, Scenario : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
   begin
      for Attempt in 1 .. 2 loop
         begin
            declare
               Ignored : Metadata.Optical_Distortion_Map_Stream :=
                 Metadata.Open_Optical_Distortion_Map (Stream, 2);
            begin
               raise Program_Error with "failed ODM registration published owner: " & Scenario;
            end;
         exception
            when ODM.Not_Supported =>
               raise Program_Error with "provider failure reported as Not_Supported: " & Scenario;
            when AMS.MEL.Provider_Error =>
               null;
         end;
      end loop;
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Expect_Failure;

   --  NotSupported is a distinct exception; Fail and exception are Provider_Error; both are
   --  one-shot and leave the base metadata owner fully usable.
   procedure Test_Registration (Provider_Path : String) is
   begin
      declare
         Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "image-metadata-sync");
         Stream   : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
         Existing : Metadata.Metadata_Stream := Metadata.Open (Stream, 2);
         Before   : constant Natural := Registrations (Provider_Path);
      begin
         Require (Registrations (Provider_Path) = Before, "base Open registered no ODM callback");
         begin
            declare
               Ignored : Metadata.Optical_Distortion_Map_Stream :=
                 Metadata.Open_Optical_Distortion_Map (Stream, 2);
            begin
               raise Program_Error with "ODM NotSupported was accepted";
            end;
         exception
            when ODM.Not_Supported =>
               null;
         end;
         Require (Registrations (Provider_Path) = Before + 1, "exactly one ODM attempt");
         begin
            declare
               Ignored : Metadata.Optical_Distortion_Map_Stream :=
                 Metadata.Open_Optical_Distortion_Map (Stream, 2);
            begin
               raise Program_Error with "ODM one-shot repeated after NotSupported";
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Require (Registrations (Provider_Path) = Before + 1, "one-shot refused before provider");
         Require
           (Metadata.Kind (Metadata.Receive (Existing)) = Metadata.Bad_Pixel_List_Event,
            "base owner unaffected");
         Metadata.Close (Existing);
         AMS.MEL.IR.Close (Stream);
         AMS.MEL.Close (Parent);
      end;
      Expect_Failure (Provider_Path, "odm-register-fail");
      Expect_Failure (Provider_Path, "odm-register-throw");
   end Test_Registration;

   --  Bounded drop-incoming: a full queue never builds a snapshot for a valid map yet still
   --  classifies malformed input; copy failure rejects one event; reception recovers.
   procedure Test_Capacity (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "odm-capacity");
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Queue  : Metadata.Optical_Distortion_Map_Stream :=
        Metadata.Open_Optical_Distortion_Map (Stream, 1);
      Copies : Integer;
   begin
      Deliver (Provider_Path, 6, 1);
      Copies := Probe (5);
      Require (Probe (6, 1) = 1, "arm copy failure");
      Deliver (Provider_Path, 1, 0);
      Deliver (Provider_Path, 0, 0, Null_Payload => True);
      Counters_Are (Queue, 3, 1, 1);
      Require (Probe (7) = 1 and then Probe (5) = Copies, "full queue built no snapshot");
      Check_Tagged (Metadata.Receive_Optical_Distortion_Map (Queue), 1);
      Deliver (Provider_Path, 6, 2);
      Require (Probe (7) = 0, "copy failure consumed on the copy path");
      Counters_Are (Queue, 4, 1, 2);
      begin
         declare
            Ignored : constant ODM.Map_Value := Metadata.Receive_Optical_Distortion_Map (Queue);
         begin
            raise Program_Error with "rejected ODM copy was published";
         end;
      exception
         when AMS.MEL.IR.Timeout_Error =>
            null;
      end;
      Deliver (Provider_Path, 6, 3);
      Check_Tagged (Metadata.Receive_Optical_Distortion_Map (Queue, 1_000), 3);
      Counters_Are (Queue, 5, 1, 2);
      Metadata.Close (Queue);
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Test_Capacity;

   --  Stop and Close preserve queued maps for draining, then Stream_Stopped; owner Close before
   --  stream Close leaves received values owned; Receive after owner Close fails closed.
   procedure Test_Lifecycle (Provider_Path : String) is
      Kept : ODM.Map_Value;
   begin
      declare
         Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "odm-late");
         Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
         Queue  : Metadata.Optical_Distortion_Map_Stream :=
           Metadata.Open_Optical_Distortion_Map (Stream, 8);
      begin
         Check_Empty (Metadata.Receive_Optical_Distortion_Map (Queue));
         AMS.MEL.IR.Stop (Stream);
         Check_Rich (Metadata.Receive_Optical_Distortion_Map (Queue));
         AMS.MEL.IR.Close (Stream);
         Kept := Metadata.Receive_Optical_Distortion_Map (Queue);
         Require (ODM.RowCol_Count (Metadata.Receive_Optical_Distortion_Map (Queue)) = 1, "drain");
         begin
            declare
               Ignored : constant ODM.Map_Value :=
                 Metadata.Receive_Optical_Distortion_Map (Queue, 50);
            begin
               raise Program_Error with "stopped ODM owner returned a map";
            end;
         exception
            when AMS.MEL.IR.Stream_Stopped =>
               null;
         end;
         Counters_Are (Queue, 5, 0, 0);
         Metadata.Close (Queue);
         begin
            declare
               Ignored : constant ODM.Map_Value := Metadata.Receive_Optical_Distortion_Map (Queue);
            begin
               raise Program_Error with "closed ODM owner returned a map";
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         AMS.MEL.Close (Parent);
      end;
      Require (ODM.FRD_Count (Kept) = 1 and then ODM.RowCol_Count (Kept) = 0, "kept after close");
      Check_FRD (Kept, 1, 0.25, 0.5, 0.75);
      declare
         Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "odm-late-closed");
         Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
         Queue  : Metadata.Optical_Distortion_Map_Stream :=
           Metadata.Open_Optical_Distortion_Map (Stream, 8);
      begin
         Kept := Metadata.Receive_Optical_Distortion_Map (Queue);
         Metadata.Close (Queue);
         Deliver (Provider_Path, 6, 9);
         AMS.MEL.IR.Close (Stream);
         AMS.MEL.Close (Parent);
      end;
      Check_Empty (Kept);
   end Test_Lifecycle;

   procedure Run (Provider_Path : String) is
   begin
      AMS.MEL.IR.Optical_Distortion_Map.Testing.Check_Representation;
      Test_Fidelity (Provider_Path);
      Test_Registration (Provider_Path);
      Test_Capacity (Provider_Path);
      Test_Lifecycle (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada IR Image OpticalDistortionMap contract");
   end Run;
end AMS_MEL_IR_Image_ODM_Tests;
