with Ada.Unchecked_Conversion;
with Interfaces;
with Interfaces.C;
with System;

package body AMS_MEL_Track_Fixture is
   package IC renames Interfaces.C;
   use type IC.int;
   use type Interfaces.Integer_32;
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
         raise Program_Error with "TrackTask fixture pin failed";
      end if;
   end Open;

   procedure Release_Pending (Count : Positive) is
      Release : constant Release_Function :=
        To_Release (Dlsym (Pin, IC.To_C ("mock_completion_release_family")));
   begin
      if Release = null or else Release (18, IC.unsigned (Count)) /= 1 then
         raise Program_Error with "TrackTask fixture release failed";
      end if;
   end Release_Pending;

   procedure Verify (Value : AMS.MEL.IR.Scheduling.Commands.Track_Task) is
      package Cmd renames AMS.MEL.IR.Scheduling.Commands;
      subtype U64 is Interfaces.Unsigned_64;
      use type U64;
      type Values_Array is array (Natural range 0 .. 1_282) of U64 with Convention => C;
      type Values_Function is access procedure (Output : access Values_Array) with Convention => C;
      function To_Values is new Ada.Unchecked_Conversion (System.Address, Values_Function);
      function Bits is new Ada.Unchecked_Conversion (Long_Float, U64);
      function Time_Bits is new Ada.Unchecked_Conversion (Interfaces.Integer_64, U64);
      Read     : constant Values_Function :=
        To_Values (Dlsym (Pin, IC.To_C ("mock_scheduling_track_values")));
      T        : Cmd.Task_Info renames Value.Info;
      Actual   : aliased Values_Array := [others => 0];
      Expected : Values_Array := [others => 0];
   begin
      Expected (0 .. 7) :=
        [U64 (Value.Command_ID),
         Time_Bits (Value.Parameters.Duration_NS),
         U64 (Value.Parameters.Count),
         Time_Bits (T.System_Time_NS),
         U64 (T.Request_ID),
         Cmd.Task_List_Action'Pos (T.Action),
         U64 (T.Precedence_Within_Priority),
         U64 (T.Priority)];
      for Index in Value.Parameters.Targets'Range loop
         declare
            Target : Cmd.Track_Target renames Value.Parameters.Targets (Index);
            Base   : constant Natural := 8 + 5 * (Index - 1);
            function ID_Bits is new
              Ada.Unchecked_Conversion (Interfaces.Integer_32, Interfaces.Unsigned_32);
         begin
            Expected (Base) := U64 (ID_Bits (Target.Target_ID));
            if Target.Target_ID < 0 then
               Expected (Base) := Expected (Base) or 16#FFFF_FFFF_0000_0000#;
            end if;
            Expected (Base + 1) := Bits (Target.Location_Azimuth_Rad);
            Expected (Base + 2) := Bits (Target.Location_Elevation_Rad);
            Expected (Base + 3) := Bits (Target.Rate_Azimuth_Rad_Per_Second);
            Expected (Base + 4) := Bits (Target.Rate_Elevation_Rad_Per_Second);
         end;
      end loop;
      if Read = null then
         raise Program_Error with "TrackTask getter capture unavailable";
      end if;
      Read (Actual'Access);
      for Index in Actual'Range loop
         if Actual (Index) /= Expected (Index) then
            raise Program_Error with "TrackTask getter mismatch at" & Index'Image;
         end if;
      end loop;
   end Verify;

   procedure Drain is
      Notify   : constant Notify_Function :=
        To_Notify (Dlsym (System.Null_Address, IC.To_C ("ams_mel_test_completion_owner_notify")));
      Observed : aliased Interfaces.Unsigned_64 := 0;
   begin
      if Notify = null or else Notify (1, 0, 0, Observed'Access) /= 1 then
         raise Program_Error with "TrackTask completion notification did not retire";
      end if;
   end Drain;

   procedure Close is
   begin
      Drain;
      if Dlclose (Pin) /= 0 then
         raise Program_Error with "TrackTask fixture unpin failed";
      end if;
      Pin := System.Null_Address;
   end Close;
end AMS_MEL_Track_Fixture;
