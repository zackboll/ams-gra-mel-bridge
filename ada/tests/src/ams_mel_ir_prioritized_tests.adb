with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Scheduling;
with AMS.MEL.IR.Scheduling.Commands;
with AMS.MEL.IR.Scheduling.Metadata;
with Interfaces;
with Interfaces.C;
with Ada.Unchecked_Conversion;
with System;

package body AMS_MEL_IR_Prioritized_Tests is
   package Sch renames AMS.MEL.IR.Scheduling;
   package Cmd renames AMS.MEL.IR.Scheduling.Commands;
   package Ch renames AMS.MEL.IR.Channel;
   package Meta renames AMS.MEL.IR.Scheduling.Metadata;
   use type Ch.Outcome;
   use type Ch.Error_Code;
   use type Cmd.Track_List_Response;
   use type Interfaces.Integer_32;
   use type Interfaces.Integer_64;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.int;
   use type System.Address;

   function Dlopen (Path : Interfaces.C.char_array; Flags : Interfaces.C.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function Dlsym (Handle : System.Address; Name : Interfaces.C.char_array) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   function Dlclose (Handle : System.Address) return Interfaces.C.int
   with Import, Convention => C, External_Name => "dlclose";
   type Release_Function is
     access function (Family, Amount : Interfaces.C.unsigned) return Interfaces.C.int
   with Convention => C;
   function To_Release is new Ada.Unchecked_Conversion (System.Address, Release_Function);
   type Notify_Function is
     access function
       (Family, Operation : Interfaces.C.unsigned;
        Target            : Interfaces.Unsigned_64;
        Observed          : access Interfaces.Unsigned_64) return Interfaces.C.int
   with Convention => C;
   function To_Notify is new Ada.Unchecked_Conversion (System.Address, Notify_Function);

   procedure Require (Test : Boolean) is
   begin
      if not Test then
         raise Program_Error with "unexpected prioritized track list behavior";
      end if;
   end Require;

   Config : constant Sch.Scheduling_Config :=
     Sch.Create_Config
       (AMS.MEL.IR.Create_UCI_ID ([others => 0], "prioritized"),
        AMS.MEL.IR.Create_UCI_ID ([others => 1], "platform"),
        AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock"));

   procedure Run (Path : String) is
      procedure Completed (Suffix : String; Expected : Cmd.Track_List_Response) is
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Path, "sched-prioritized-" & Suffix);
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
         Input   : Cmd.Prioritized_Track_List_Cmd (Length => 512);
      begin
         begin
            declare
               Request : Cmd.Track_List_Request := Cmd.Send_Prioritized_Tracks (Channel, Input);
            begin
               Cmd.Close (Request);
               Require (False);
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Sch.Enable (Channel);
         Input.Schedule_Time_NS := Interfaces.Integer_64'First;
         Input.Time_To_Live_NS := Interfaces.Integer_64'Last;
         Input.Track_Count := 0;
         Input.Command_ID := Interfaces.Unsigned_32'Last;
         for I in Input.Tracks'Range loop
            Input.Tracks (I) :=
              (Timestamp_NS         => -Interfaces.Integer_64 (I),
               Track_ID             => -33,
               IRST_Priority        => 16#8123_4567#,
               Reserved_Priority    => 16#8000_0000# + Interfaces.Unsigned_32 (I),
               IRST_Precedence      => 16#F234_5678#,
               Reserved_Precedence  => 13,
               Is_Skippable         => I mod 2 = 0,
               Maintain_Outside_FOS => I mod 3 = 0,
               IRST_Track_ID        => Interfaces.Integer_32'First,
               Position             => (-0.0, 1.25, -2.5, 3.75),
               Velocity             => (5.5, 6.25, -7.5, -8.75),
               Task_Kind            => Meta.Task_Kind'Val (I mod 17),
               Command_ID           => Interfaces.Unsigned_32 (I));
         end loop;
         declare
            Request : Cmd.Track_List_Request := Cmd.Send_Prioritized_Tracks (Channel, Input);
         begin
            Sch.Close (Channel);
            AMS.MEL.Close (Parent);
            declare
               Result : constant Cmd.Track_List_Result := Cmd.Wait (Request, 5_000);
            begin
               Require (Cmd.Value (Cmd.Wait (Request, 0)) = Expected);
               Cmd.Close (Request);
               Cmd.Close (Request);
               Require (Cmd.Status (Result) = Ch.Success and then Cmd.Value (Result) = Expected);
            end;
         end;
      end Completed;
   begin
      Completed ("none", Cmd.None);
      Completed ("rejected", Cmd.Rejected);
      Completed ("expired", Cmd.Rejected_Expired);
      Completed ("accepted", Cmd.Accepted);
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Path, "sched-prioritized-mel-error");
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
         Empty   : constant Cmd.Prioritized_Track_List_Cmd (0) := (Length => 0, others => <>);
      begin
         Sch.Enable (Channel);
         declare
            Request : Cmd.Track_List_Request := Cmd.Send_Prioritized_Tracks (Channel, Empty);
            First   : constant Cmd.Track_List_Result := Cmd.Wait (Request, 5_000);
            Cached  : constant Cmd.Track_List_Result := Cmd.Wait (Request, 0);
         begin
            Cmd.Close (Request);
            Sch.Close (Channel);
            AMS.MEL.Close (Parent);
            Require
              (Cmd.Status (First) = Ch.Rejected
               and then Cmd.Rejection_Code (Cached) = Ch.Invalid_State);
            Require
              (Cmd.Description (First)
               = "err" & ASCII.NUL & Character'Val (255) & String'(1 .. 1024 => 'x'));
            Require (Cmd.Description (Cached) = Cmd.Description (First));
         end;
      end;
      declare
         Parent   : AMS.MEL.Session :=
           AMS.MEL.Open_With_Options (Path, "sched-prioritized-delayed", (Max_Async_Requests => 1));
         Channel  : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
         Empty    : constant Cmd.Prioritized_Track_List_Cmd (0) := (Length => 0, others => <>);
         Pin      : constant System.Address := Dlopen (Interfaces.C.To_C (Path), 2);
         Observed : aliased Interfaces.Unsigned_64 := 0;
      begin
         Require (Pin /= System.Null_Address);
         Sch.Enable (Channel);
         declare
            Request : constant Cmd.Track_List_Request :=
              Cmd.Send_Prioritized_Tracks (Channel, Empty);
         begin
            begin
               Require (Cmd.Value (Cmd.Wait (Request, 0)) = Cmd.Accepted);
               Require (False);
            exception
               when AMS.MEL.IR.Timeout_Error =>
                  null;
            end;
         --  Scope exit finalizes the observer before the future is released.
         end;
         begin
            declare
               Extra : Cmd.Track_List_Request := Cmd.Send_Prioritized_Tracks (Channel, Empty);
            begin
               Cmd.Close (Extra);
               Require (False);
            end;
         exception
            when AMS.MEL.Resource_Exhausted =>
               null;
         end;
         Sch.Close (Channel);
         AMS.MEL.Close (Parent);
         declare
            Release          : constant Release_Function :=
              To_Release (Dlsym (Pin, Interfaces.C.To_C ("mock_completion_release_family")));
            Await_Retirement : constant Notify_Function :=
              To_Notify
                (Dlsym
                   (System.Null_Address,
                    Interfaces.C.To_C ("ams_mel_test_completion_owner_notify")));
         begin
            Require (Release /= null and then Release (19, 1) = 1);
            Require
              (Await_Retirement /= null and then Await_Retirement (12, 0, 0, Observed'Access) = 1);
         end;
         Require (Dlclose (Pin) = 0);
      end;
   end Run;
end AMS_MEL_IR_Prioritized_Tests;
