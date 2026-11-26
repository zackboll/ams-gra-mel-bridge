with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with AMS.MEL;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.Scheduling.Metadata;
with Interfaces.C;
with System;

package body AMS_MEL_IR_Scheduling_Performance_Tests is
   package IR renames AMS.MEL.IR;
   package Sch renames IR.Scheduling;
   package M renames Sch.Metadata;
   use IR.C2;
   use type Interfaces.Unsigned_64;
   use type Interfaces.C.int;
   use type M.Metadata_Counters;
   use type System.Address;

   Config : constant Sch.Scheduling_Config :=
     Sch.Create_Config
       (IR.Create_UCI_ID ([others => 0], "scan performance metadata"),
        IR.Create_UCI_ID ([others => 1], "platform"),
        IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock"));

   function Dlsym (Handle : System.Address; Name : Interfaces.C.char_array) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   function Dlopen (Path : Interfaces.C.char_array; Flags : Interfaces.C.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function Dlclose (Handle : System.Address) return Interfaces.C.int
   with Import, Convention => C, External_Name => "dlclose";
   type Probe_Function is
     access function
       (Stream : System.Address; Operation : Interfaces.C.unsigned) return Interfaces.C.int
   with Convention => C;
   function To_Probe is new Ada.Unchecked_Conversion (System.Address, Probe_Function);

   procedure Require (Condition : Boolean; Message : String) is
   begin
      if not Condition then
         raise Program_Error with "ScanPerformanceReport: " & Message;
      end if;
   end Require;

   function Probe (Operation : Interfaces.C.unsigned) return Interfaces.C.int is
      Call : constant Probe_Function :=
        To_Probe
          (Dlsym (System.Null_Address, Interfaces.C.To_C ("ams_mel_test_scan_performance_probe")));
   begin
      Require (Call /= null, "test-only probe is available");
      return Call (System.Null_Address, Operation);
   end Probe;

   --  Read through an integer representation, not a floating function argument:
   --  GNAT validity checking intentionally remains enabled during the receive test.
   function Bits (Address : System.Address) return Interfaces.Unsigned_64 is
      Value : Interfaces.Unsigned_64
      with Import, Address => Address;
   begin
      return Value;
   end Bits;

   procedure Check_Param (Value : Scan_Parameters; Fixture, Side : Natural) is
      Float_Pattern : constant array (Natural range 0 .. 13) of Interfaces.Unsigned_64 :=
        [0,
         16#8000_0000_0000_0000#,
         16#3ff1_2345_6789_abcd#,
         16#c004_1234_5678_9abc#,
         1,
         16#8000_0000_0000_0067#,
         16#7ff0_0000_0000_0000#,
         16#fff0_0000_0000_0000#,
         16#7ff8_0000_0000_1234#,
         16#fff8_0000_0005_4321#,
         16#7ff0_0000_0000_5678#,
         16#4009_0000_0000_0000#,
         16#0010_0000_0000_0000#,
         16#c032_0000_0000_0000#];
      Word_Pattern  : constant array (Natural range 0 .. 9) of Scan_Value :=
        [0,
         Scan_Value'Last,
         16#8000_0000#,
         17,
         16#8000_0027#,
         103,
         16#f000_0003#,
         16#7fff_ffff#,
         8001,
         123456789];
      Float_Address : constant array (Natural range 0 .. 6) of System.Address :=
        [Value.Center_Azimuth_Rad'Address,
         Value.Center_Elevation_Rad'Address,
         Value.Scan_Width_Rad'Address,
         Value.Scan_Height_Rad'Address,
         Value.Scan_Rate_Rad_Per_Second'Address,
         Value.Preferred_Revisit_Interval_Seconds'Address,
         Value.Required_Revisit_Interval_Seconds'Address];
      Words         : constant array (Natural range 0 .. 7) of Scan_Value :=
        [Value.Continuous_Scan,
         Value.Returning,
         Value.Agile_Scan,
         Value.Scan_ID,
         Value.Max_Range_Of_Interest_M,
         Value.Min_Range_Of_Interest_M,
         Value.Elevation_Scan_Center_Altitude_M,
         Value.Elevation_Scan_Center_Range_M];
      Label         : constant String := "fixture" & Fixture'Image & ", side" & Side'Image;
   begin
      for J in Float_Address'Range loop
         Require
           (Bits (Float_Address (J)) = Float_Pattern ((Fixture + 7 * Side + J) mod 14),
            Label & " binary64 field" & J'Image);
      end loop;
      for J in Words'Range loop
         Require
           (Words (J) = Word_Pattern ((Fixture + 3 * Side + J) mod 10),
            Label & " unsigned field" & J'Image);
      end loop;
      Require
        (Value.Elevation_Defined_With_Range_And_Altitude = ((Fixture + Side) mod 2 = 1),
         Label & " elevation Boolean");
      Require
        (Coordinate_Frame_Reference'Enum_Rep (Value.Center_Frame_Reference_EL)
         = (Fixture / 2 + Side) mod 2,
         Label & " elevation frame");
      Require
        (Coordinate_Frame_Reference'Enum_Rep (Value.Center_Frame_Reference_AZ)
         = (Fixture / 4 + Side) mod 2,
         Label & " azimuth frame");
      Require
        (Degradation_Method'Enum_Rep (Value.Degradation) = (Fixture + 2 * Side) mod 4,
         Label & " degradation method");
   end Check_Param;

   procedure Payload (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-performance-sync");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      Stream  : M.Scan_Performance_Stream := M.Open_Scan_Performance (Channel, 14);
      Reports : array (Natural range 0 .. 13) of M.Scan_Performance_Report;
      Closed  : constant Interfaces.C.int := Probe (16);
   begin
      Require (M.Scan_Performance_Counters (Stream) = (21, 0, 7), "synchronous copy and rejection");
      Reports (0) := M.Receive_Scan_Performance (Stream);
      Sch.Close (Channel);
      AMS.MEL.Close (Parent);
      declare
         --  Linux RTLD_NOW | RTLD_NOLOAD: observe, do not load the mock again.
         Pin : constant System.Address := Dlopen (Interfaces.C.To_C (Provider_Path), 6);
      begin
         if Pin /= System.Null_Address then
            Require (Dlclose (Pin) = 0, "release loader probe");
            raise Program_Error with "provider unexpectedly retained after Scheduling cleanup";
         end if;
      end;
      for J in 1 .. Reports'Last loop
         Reports (J) := M.Receive_Scan_Performance (Stream);
      end loop;
      begin
         declare
            Unexpected : constant M.Scan_Performance_Report := M.Receive_Scan_Performance (Stream);
         begin
            raise Program_Error with "unexpected Scan_ID" & Unexpected.Commanded.Scan_ID'Image;
         end;
      exception
         when IR.Stream_Stopped =>
            null;
      end;
      M.Close (Stream);
      M.Close (Stream);
      Require (Probe (16) = Closed + 14, "native events closed on success");
      for J in Reports'Range loop
         Check_Param (Reports (J).Commanded, J, 0);
         Check_Param (Reports (J).Achieved, J, 1);
      end loop;
   end Payload;

   procedure Malformed_Views (Provider_Path : String) is
      Parent  : constant AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-performance-sync");
      Channel : constant Sch.Scheduling_Channel := Sch.Open (Parent, Config);
      Stream  : constant M.Scan_Performance_Stream := M.Open_Scan_Performance (Channel, 14);
   begin
      for Corruption in Interfaces.C.unsigned range 8 .. 15 loop
         declare
            Closed : constant Interfaces.C.int := Probe (16);
         begin
            Require (Probe (Corruption) = 1, "arm malformed native view");
            declare
               Unexpected : constant M.Scan_Performance_Report :=
                 M.Receive_Scan_Performance (Stream);
            begin
               raise Program_Error
                 with "accepted invalid view" & Unexpected.Commanded.Scan_ID'Image;
            end;
         exception
            when AMS.MEL.Provider_Error =>
               Require (Probe (16) = Closed + 1, "invalid event released");
         end;
      end loop;
      declare
         Report : constant M.Scan_Performance_Report := M.Receive_Scan_Performance (Stream);
      begin
         Check_Param (Report.Commanded, 8, 0);
         Check_Param (Report.Achieved, 8, 1);
      end;
   end Malformed_Views;

   procedure Optional (Provider_Path : String) is
      Parent  : constant AMS.MEL.Session :=
        AMS.MEL.Open (Provider_Path, "sched-performance-not-supported");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
   begin
      for Attempt in 1 .. 2 loop
         begin
            declare
               Stream : M.Scan_Performance_Stream := M.Open_Scan_Performance (Channel, 1);
            begin
               M.Close (Stream);
               raise Program_Error with "unexpected optional registration success";
            end;
         exception
            when M.Not_Supported =>
               Require (Attempt = 1, "provider refusal consumes one-shot");
            when AMS.MEL.Provider_Error =>
               Require (Attempt = 2, "first refusal must be Not_Supported");
         end;
      end loop;
      Sch.Enable (Channel);
      declare
         Events : M.Task_Events_Stream := M.Open_Task_Events (Channel, 1);
      begin
         Require (M.Task_Events_Counters (Events) = (0, 0, 0), "another stream remains usable");
         M.Close (Events);
      end;
   end Optional;

   procedure Run (Provider_Path : String) is
   begin
      Payload (Provider_Path);
      Malformed_Views (Provider_Path);
      Optional (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada Scheduling ScanPerformanceReport");
   end Run;
end AMS_MEL_IR_Scheduling_Performance_Tests;
