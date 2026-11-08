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
with AMS.MEL.IR.Line_Of_Sight_Quaternion;
with AMS.MEL.IR.Line_Of_Sight_Quaternion.Testing;

package body AMS_MEL_IR_Image_LOS_Quaternion_Tests is
   package Metadata renames AMS.MEL.IR.Image.Metadata;
   package LOSQ renames AMS.MEL.IR.Line_Of_Sight_Quaternion;
   package IC renames Interfaces.C;
   use type AMS.MEL.IR.Counter;
   use type Metadata.Metadata_Kind;
   use type Interfaces.Unsigned_64;
   use type IC.int;
   use type IC.unsigned;
   use type System.Address;

   Zero       : constant AMS.MEL.IR.UUID := [others => 0];
   Channel_ID : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "Ada LOSQ channel");
   Platform   : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "Ada LOSQ platform");
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
         raise Program_Error with "Ada Image LineOfSightQuaternion: " & Text;
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
      Call : constant Deliver_Function := To_Deliver (Dlsym (Pin, IC.To_C ("mock_losq_deliver")));
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
        To_Registrations (Dlsym (Pin, IC.To_C ("mock_losq_registrations")));
      Result : Natural;
   begin
      Require (Pin /= System.Null_Address and then Call /= null, "mock registration counter");
      Result := Natural (Call.all);
      Require (Dlclose (Pin) = 0, "counter unpin");
      return Result;
   end Registrations;

   function Probe (Operation : Natural; Value : Natural := 0) return Integer is
      Call : constant Probe_Function :=
        To_Probe (Dlsym (System.Null_Address, IC.To_C ("ams_mel_test_image_los_quaternion_probe")));
   begin
      Require (Call /= null, "test-facade LOS quaternion probe binding");
      return Integer (Call (IC.unsigned (Operation), IC.size_t (Value)));
   end Probe;

   procedure Counters_Are
     (Stream : Metadata.Line_Of_Sight_Quaternion_Stream; Received, Dropped, Malformed : Natural)
   is
      Count : constant Metadata.Metadata_Counters :=
        Metadata.Line_Of_Sight_Quaternion_Counters (Stream);
   begin
      Require
        (Count.Events_Received = AMS.MEL.IR.Counter (Received)
         and then Count.Events_Dropped_Queue_Full = AMS.MEL.IR.Counter (Dropped)
         and then Count.Malformed_Or_Unsupported = AMS.MEL.IR.Counter (Malformed),
         "counters");
   end Counters_Are;

   --  Every component compared by name and bit pattern, so a swap or a lost signed zero fails.
   procedure Check
     (Value                : LOSQ.Line_Of_Sight_Value;
      Time                 : Long_Long_Integer;
      X, Y, Z, W           : Long_Float;
      Forward, Right, Down : Long_Float;
      Label                : String) is
   begin
      Require (Value.System_Time_NS = Time, Label & " system time");
      Require (Same (Value.Q_XYZW.X, X), Label & " quaternion x");
      Require (Same (Value.Q_XYZW.Y, Y), Label & " quaternion y");
      Require (Same (Value.Q_XYZW.Z, Z), Label & " quaternion z");
      Require (Same (Value.Q_XYZW.W, W), Label & " quaternion w");
      Require (Same (Value.PQR_RPS.Forward, Forward), Label & " forward rate");
      Require (Same (Value.PQR_RPS.Right, Right), Label & " right rate");
      Require (Same (Value.PQR_RPS.Down, Down), Label & " down rate");
   end Check;

   Q1W : constant Long_Float := 0.8227241335952167;

   --  Fixture 0: the upstream default value.
   procedure Check_Zero (Value : LOSQ.Line_Of_Sight_Value) is
   begin
      Check (Value, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, "default");
   end Check_Zero;

   --  Fixture 1: minimum signed time, asymmetric near-unit quaternion, small/negative rates.
   procedure Check_First (Value : LOSQ.Line_Of_Sight_Value) is
   begin
      Check
        (Value, Long_Long_Integer'First, 0.125, -0.25, 0.5, Q1W, 1.0E-9, -2.5E-3, 0.0625, "first");
   end Check_First;

   --  Fixture 2: the opposite-sign representation of fixture 1, never canonicalized.
   procedure Check_Opposite (Value : LOSQ.Line_Of_Sight_Value) is
   begin
      Check (Value, -1, -0.125, 0.25, -0.5, -Q1W, -1.0E-9, 2.5E-3, -0.0625, "opposite");
   end Check_Opposite;

   --  Fixture 3: a non-unit quaternion and large rates, never normalized or clamped.
   procedure Check_Non_Unit (Value : LOSQ.Line_Of_Sight_Value) is
      Q : constant LOSQ.Quaternion := Value.Q_XYZW;
   begin
      Require (Q.X * Q.X + Q.Y * Q.Y + Q.Z * Q.Z + Q.W * Q.W = 348.0, "non-unit norm kept");
      Check (Value, 0, 3.0, -7.0, 11.0, -13.0, 6283.185307179586, -1.5E6, 42.0, "non-unit");
   end Check_Non_Unit;

   procedure Check_Tagged (Value : LOSQ.Line_Of_Sight_Value; Tag : Natural) is
   begin
      Check
        (Value,
         Long_Long_Integer (Tag),
         Long_Float (Tag),
         0.5,
         -0.5,
         0.25,
         -4.0,
         8.0,
         -16.0,
         "tagged");
   end Check_Tagged;

   --  Synchronous registration delivery (fixtures 0..3) and on-demand delivery of signed-zero,
   --  non-finite, subnormal/extreme and tagged values. The mock overwrites and destroys every
   --  payload after the callback returns. All values outlive the owner, stream and Session.
   procedure Test_Fidelity (Provider_Path : String) is
      Values : array (1 .. 8) of LOSQ.Line_Of_Sight_Value;
   begin
      declare
         Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "losq-sync");
         Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
         Queue  : Metadata.Line_Of_Sight_Quaternion_Stream :=
           Metadata.Open_Line_Of_Sight_Quaternion (Stream, 8);
      begin
         Counters_Are (Queue, 4, 0, 0);
         Deliver (Provider_Path, 4, 0);
         Deliver (Provider_Path, 5, 0);
         Deliver (Provider_Path, 6, 106);
         Deliver (Provider_Path, 7, 107);
         for Index in Values'Range loop
            Values (Index) := Metadata.Receive_Line_Of_Sight_Quaternion (Queue);
         end loop;
         Metadata.Close (Queue);
         Metadata.Close (Queue);
         AMS.MEL.IR.Close (Stream);
         AMS.MEL.Close (Parent);
      end;
      Check_Zero (Values (1));
      Check_First (Values (2));
      Check_Opposite (Values (3));
      Check_Non_Unit (Values (4));
      --  Fixture 4: signed zero only in X and in the down rate.
      Check (Values (5), 1, -0.0, 0.375, -0.625, 0.75, 0.875, -1.125, -0.0, "signed zero");
      Require (not Same (Values (5).Q_XYZW.X, 0.0), "negative zero X is not positive zero");
      --  Fixture 5: non-finite values are carried, never replaced; W stays exact.
      declare
         V : constant LOSQ.Line_Of_Sight_Value := Values (6);
      begin
         Require (V.System_Time_NS = Long_Long_Integer'Last, "maximum signed time");
         Require (V.Q_XYZW.X /= V.Q_XYZW.X, "NaN quaternion x");
         Require (not V.Q_XYZW.Y'Valid and then V.Q_XYZW.Y > 0.0, "positive infinity quaternion y");
         Require (not V.Q_XYZW.Z'Valid and then V.Q_XYZW.Z < 0.0, "negative infinity quaternion z");
         Require (Same (V.Q_XYZW.W, 0.5), "finite quaternion w");
         Require
           (not V.PQR_RPS.Forward'Valid and then V.PQR_RPS.Forward < 0.0,
            "negative infinity forward rate");
         Require (V.PQR_RPS.Right /= V.PQR_RPS.Right, "NaN right rate");
         Require
           (not V.PQR_RPS.Down'Valid and then V.PQR_RPS.Down > 0.0, "positive infinity down rate");
      end;
      --  Fixture 6: smallest subnormal and largest finite magnitudes, both signs.
      declare
         V : constant LOSQ.Line_Of_Sight_Value := Values (7);
      begin
         Require (V.System_Time_NS = 106, "extreme time");
         Require (Bits (V.Q_XYZW.X) = 1, "smallest subnormal quaternion x");
         Require (Bits (V.Q_XYZW.Y) = 16#8000_0000_0000_0001#, "negative subnormal quaternion y");
         Require (Same (V.Q_XYZW.Z, Long_Float'Last), "largest quaternion z");
         Require (Same (V.Q_XYZW.W, -Long_Float'Last), "most negative quaternion w");
         Require (Bits (V.PQR_RPS.Forward) = 16#8000_0000_0000_0001#, "negative subnormal rate");
         Require (Same (V.PQR_RPS.Right, Long_Float'Last), "largest right rate");
         Require (Same (V.PQR_RPS.Down, -Long_Float'Last), "most negative down rate");
      end;
      Check_Tagged (Values (8), 107);
   end Test_Fidelity;

   --  Provider Fail, NotImplemented, unknown Return or exception: Provider_Error (not
   --  Not_Supported), no owner, and the one-shot stays consumed.
   procedure Expect_Failure (Provider_Path, Scenario : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Before : constant Natural := Registrations (Provider_Path);
   begin
      for Attempt in 1 .. 2 loop
         begin
            declare
               Ignored : Metadata.Line_Of_Sight_Quaternion_Stream :=
                 Metadata.Open_Line_Of_Sight_Quaternion (Stream, 2);
            begin
               raise Program_Error with "failed LOSQ registration published owner: " & Scenario;
            end;
         exception
            when LOSQ.Not_Supported =>
               raise Program_Error with "provider failure reported as Not_Supported: " & Scenario;
            when AMS.MEL.Provider_Error =>
               null;
         end;
      end loop;
      Require (Registrations (Provider_Path) = Before + 1, "one attempt only: " & Scenario);
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Expect_Failure;

   --  NotSupported is a distinct exception; the other outcomes are Provider_Error; every
   --  attempt is one-shot and the base metadata owner stays fully usable and unregistered.
   procedure Test_Registration (Provider_Path : String) is
   begin
      declare
         Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "image-metadata-sync");
         Stream   : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
         Before   : constant Natural := Registrations (Provider_Path);
         Existing : Metadata.Metadata_Stream := Metadata.Open (Stream, 2);
      begin
         Require (Registrations (Provider_Path) = Before, "base Open registered no LOSQ callback");
         begin
            declare
               Ignored : Metadata.Line_Of_Sight_Quaternion_Stream :=
                 Metadata.Open_Line_Of_Sight_Quaternion (Stream, 2);
            begin
               raise Program_Error with "LOSQ NotSupported was accepted";
            end;
         exception
            when LOSQ.Not_Supported =>
               null;
         end;
         Require (Registrations (Provider_Path) = Before + 1, "exactly one LOSQ attempt");
         begin
            declare
               Ignored : Metadata.Line_Of_Sight_Quaternion_Stream :=
                 Metadata.Open_Line_Of_Sight_Quaternion (Stream, 2);
            begin
               raise Program_Error with "LOSQ one-shot repeated after NotSupported";
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
      Expect_Failure (Provider_Path, "losq-register-fail");
      Expect_Failure (Provider_Path, "losq-register-not-implemented");
      Expect_Failure (Provider_Path, "losq-register-unknown");
      Expect_Failure (Provider_Path, "losq-register-throw");
      --  One-shot after success.
      declare
         Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "losq-running");
         Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
         Queue  : Metadata.Line_Of_Sight_Quaternion_Stream :=
           Metadata.Open_Line_Of_Sight_Quaternion (Stream, 2);
      begin
         begin
            declare
               Ignored : Metadata.Line_Of_Sight_Quaternion_Stream :=
                 Metadata.Open_Line_Of_Sight_Quaternion (Stream, 2);
            begin
               raise Program_Error with "LOSQ one-shot repeated after success";
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Counters_Are (Queue, 0, 0, 0);
         Metadata.Close (Queue);
         AMS.MEL.IR.Close (Stream);
         AMS.MEL.Close (Parent);
      end;
   end Test_Registration;

   --  Bounded drop-incoming: a full queue never allocates a snapshot yet still counts null
   --  payloads malformed; an allocation failure rejects one event; reception recovers; FIFO.
   procedure Test_Capacity (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "losq-capacity");
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Queue  : Metadata.Line_Of_Sight_Quaternion_Stream :=
        Metadata.Open_Line_Of_Sight_Quaternion (Stream, 1);
      Copies : Integer;
   begin
      Deliver (Provider_Path, 7, 1);
      Copies := Probe (4);
      Require (Probe (5, 1) = 1, "arm allocation failure");
      Deliver (Provider_Path, 1, 0);
      Deliver (Provider_Path, 0, 0, Null_Payload => True);
      Counters_Are (Queue, 3, 1, 1);
      Require (Probe (6) = 1 and then Probe (4) = Copies, "full queue allocated no snapshot");
      Check_Tagged (Metadata.Receive_Line_Of_Sight_Quaternion (Queue), 1);
      Deliver (Provider_Path, 7, 2);
      Require (Probe (6) = 0, "allocation failure consumed on the copy path");
      Counters_Are (Queue, 4, 1, 2);
      begin
         declare
            Ignored : constant LOSQ.Line_Of_Sight_Value :=
              Metadata.Receive_Line_Of_Sight_Quaternion (Queue);
         begin
            raise Program_Error with "rejected LOSQ copy was published";
         end;
      exception
         when AMS.MEL.IR.Timeout_Error =>
            null;
      end;
      Deliver (Provider_Path, 7, 3);
      Check_Tagged (Metadata.Receive_Line_Of_Sight_Quaternion (Queue, 1_000), 3);
      Counters_Are (Queue, 5, 1, 2);
      Metadata.Close (Queue);
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
      declare
         Larger_Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "losq-capacity");
         Larger_Stream : AMS.MEL.IR.Image_Stream :=
           AMS.MEL.IR.Open_Image_Stream (Larger_Parent, Config);
         Larger        : Metadata.Line_Of_Sight_Quaternion_Stream :=
           Metadata.Open_Line_Of_Sight_Quaternion (Larger_Stream, 3);
      begin
         for Tag in 40 .. 44 loop
            Deliver (Provider_Path, 7, Interfaces.Unsigned_32 (Tag));
         end loop;
         Counters_Are (Larger, 5, 2, 0);
         for Tag in 40 .. 42 loop
            Check_Tagged (Metadata.Receive_Line_Of_Sight_Quaternion (Larger), Tag);
         end loop;
         Metadata.Close (Larger);
         AMS.MEL.IR.Close (Larger_Stream);
         AMS.MEL.Close (Larger_Parent);
      end;
   end Test_Capacity;

   --  Stop and Close preserve queued values for draining, then Stream_Stopped; owner Close before
   --  stream Close leaves received values owned; Receive after owner Close fails closed.
   procedure Test_Lifecycle (Provider_Path : String) is
      Kept : LOSQ.Line_Of_Sight_Value;
   begin
      declare
         Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "losq-late");
         Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
         Queue  : Metadata.Line_Of_Sight_Quaternion_Stream :=
           Metadata.Open_Line_Of_Sight_Quaternion (Stream, 8);
      begin
         Check_Zero (Metadata.Receive_Line_Of_Sight_Quaternion (Queue));
         AMS.MEL.IR.Stop (Stream);
         Check_First (Metadata.Receive_Line_Of_Sight_Quaternion (Queue));
         AMS.MEL.IR.Close (Stream);
         Kept := Metadata.Receive_Line_Of_Sight_Quaternion (Queue);
         Check_Non_Unit (Metadata.Receive_Line_Of_Sight_Quaternion (Queue));
         begin
            declare
               Ignored : constant LOSQ.Line_Of_Sight_Value :=
                 Metadata.Receive_Line_Of_Sight_Quaternion (Queue, 50);
            begin
               raise Program_Error with "stopped LOSQ owner returned a value";
            end;
         exception
            when AMS.MEL.IR.Stream_Stopped =>
               null;
         end;
         Counters_Are (Queue, 5, 0, 0);
         Metadata.Close (Queue);
         begin
            declare
               Ignored : constant LOSQ.Line_Of_Sight_Value :=
                 Metadata.Receive_Line_Of_Sight_Quaternion (Queue);
            begin
               raise Program_Error with "closed LOSQ owner returned a value";
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         AMS.MEL.Close (Parent);
      end;
      Check_Opposite (Kept);
      declare
         Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "losq-late-closed");
         Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
         Queue  : Metadata.Line_Of_Sight_Quaternion_Stream :=
           Metadata.Open_Line_Of_Sight_Quaternion (Stream, 8);
      begin
         Kept := Metadata.Receive_Line_Of_Sight_Quaternion (Queue);
         Metadata.Close (Queue);
         Deliver (Provider_Path, 7, 9);
         AMS.MEL.IR.Close (Stream);
         AMS.MEL.Close (Parent);
      end;
      Check_Zero (Kept);
   end Test_Lifecycle;

   procedure Run (Provider_Path : String) is
   begin
      AMS.MEL.IR.Line_Of_Sight_Quaternion.Testing.Check_Representation;
      Test_Fidelity (Provider_Path);
      Test_Registration (Provider_Path);
      Test_Capacity (Provider_Path);
      Test_Lifecycle (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada IR Image LineOfSightQuaternion contract");
   end Run;
end AMS_MEL_IR_Image_LOS_Quaternion_Tests;
