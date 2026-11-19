with AMS.MEL.IR.C2;
with Ada.Unchecked_Conversion;
with Interfaces;
with Interfaces.C;
with System;

package body AMS_MEL_Scan_Fixture is
   package IC renames Interfaces.C;
   use type IC.int;
   use type System.Address;
   function Dlopen (Path : IC.char_array; Flags : IC.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function Dlsym (Handle : System.Address; Name : IC.char_array) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   function Dlclose (Handle : System.Address) return IC.int
   with Import, Convention => C, External_Name => "dlclose";
   type Release_Function is access function (Family, Amount : IC.unsigned) return IC.int
   with Convention => C;
   function To_Release is new Ada.Unchecked_Conversion (System.Address, Release_Function);
   type Notify_Function is
     access function
       (Family, Operation : IC.unsigned;
        Target            : Interfaces.Unsigned_64;
        Observed          : access Interfaces.Unsigned_64) return IC.int
   with Convention => C;
   function To_Notify is new Ada.Unchecked_Conversion (System.Address, Notify_Function);
   Pin : System.Address := System.Null_Address;

   procedure Open (Provider_Path : String) is
   begin
      Pin := Dlopen (IC.To_C (Provider_Path), 2);
      if Pin = System.Null_Address then
         raise Program_Error with "ScanTask fixture pin failed";
      end if;
   end Open;

   procedure Release_Pending (Count : Positive) is
      Release : constant Release_Function :=
        To_Release (Dlsym (Pin, IC.To_C ("mock_completion_release_family")));
   begin
      if Release = null or else Release (17, IC.unsigned (Count)) /= 1 then
         raise Program_Error with "ScanTask fixture release failed";
      end if;
   end Release_Pending;

   procedure Verify (Value : AMS.MEL.IR.Scheduling.Commands.Scan_Task) is
      package Cmd renames AMS.MEL.IR.Scheduling.Commands;
      package C2 renames AMS.MEL.IR.C2;
      subtype U64 is Interfaces.Unsigned_64;
      use type U64;
      type Values_Array is array (Natural range 0 .. 25) of U64 with Convention => C;
      type Values_Function is access procedure (Output : access Values_Array) with Convention => C;
      function To_Values is new Ada.Unchecked_Conversion (System.Address, Values_Function);
      function Bits is new Ada.Unchecked_Conversion (Long_Float, U64);
      function Time_Bits is new Ada.Unchecked_Conversion (Interfaces.Integer_64, U64);
      Read     : constant Values_Function :=
        To_Values (Dlsym (Pin, IC.To_C ("mock_scheduling_scan_values")));
      S        : C2.Scan_Parameters renames Value.Parameters;
      T        : Cmd.Task_Info renames Value.Info;
      Actual   : aliased Values_Array := [others => 0];
      Expected : constant Values_Array :=
        [U64 (Value.Command_ID),
         Boolean'Pos (S.Elevation_Defined_With_Range_And_Altitude),
         Bits (S.Center_Azimuth_Rad),
         Bits (S.Center_Elevation_Rad),
         C2.Coordinate_Frame_Reference'Pos (S.Center_Frame_Reference_EL),
         C2.Coordinate_Frame_Reference'Pos (S.Center_Frame_Reference_AZ),
         Bits (S.Scan_Width_Rad),
         Bits (S.Scan_Height_Rad),
         U64 (S.Continuous_Scan),
         U64 (S.Returning),
         U64 (S.Agile_Scan),
         U64 (S.Scan_ID),
         Bits (S.Scan_Rate_Rad_Per_Second),
         Bits (S.Preferred_Revisit_Interval_Seconds),
         Bits (S.Required_Revisit_Interval_Seconds),
         U64 (S.Max_Range_Of_Interest_M),
         U64 (S.Min_Range_Of_Interest_M),
         U64 (S.Elevation_Scan_Center_Altitude_M),
         U64 (S.Elevation_Scan_Center_Range_M),
         C2.Degradation_Method'Pos (S.Degradation),
         Time_Bits (T.System_Time_NS),
         U64 (T.Request_ID),
         Cmd.Task_List_Action'Pos (T.Action),
         U64 (T.Precedence_Within_Priority),
         U64 (T.Priority),
         0];
   begin
      if Read = null then
         raise Program_Error with "ScanTask getter capture unavailable";
      end if;
      Read (Actual'Access);
      for Index in Actual'Range loop
         if Actual (Index) /= Expected (Index) then
            raise Program_Error with "ScanTask getter mismatch at" & Index'Image;
         end if;
      end loop;
   end Verify;

   procedure Drain is
      Notify   : constant Notify_Function :=
        To_Notify (Dlsym (System.Null_Address, IC.To_C ("ams_mel_test_completion_owner_notify")));
      Observed : aliased Interfaces.Unsigned_64 := 0;
   begin
      if Notify = null or else Notify (1, 0, 0, Observed'Access) /= 1 then
         raise Program_Error with "ScanTask completion notification did not retire";
      end if;
   end Drain;

   procedure Close is
   begin
      Drain;
      if Dlclose (Pin) /= 0 then
         raise Program_Error with "ScanTask fixture unpin failed";
      end if;
      Pin := System.Null_Address;
   end Close;
end AMS_MEL_Scan_Fixture;
