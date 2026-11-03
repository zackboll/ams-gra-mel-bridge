with Ada.Text_IO;
with Ada.Exceptions;
with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;
with Ada.Containers;
with Ada.Environment_Variables;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.C2.Metadata;
with AMS.MEL.IR.Calibration_Configuration;
with AMS.MEL.IR.Calibration_Configuration.Testing;
with AMS.MEL.IR.Channel;

package body AMS_MEL_IR_Calibration_Configuration_Tests is
   package CC renames AMS.MEL.IR.Calibration_Configuration;
   package C2 renames AMS.MEL.IR.C2;
   package M renames AMS.MEL.IR.C2.Metadata;
   package Ch renames AMS.MEL.IR.Channel;
   use type CC.Accepted_Interface;
   use type CC.Outcome;
   use type CC.Error_Code;
   use type Ada.Containers.Count_Type;
   use type AMS.MEL.IR.Byte;
   use type AMS.MEL.IR.Counter;
   Zero     : constant AMS.MEL.IR.UUID := [others => 0];
   ID       : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "calibration");
   Location : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock");
   Config   : constant C2.Control_Config := C2.Create_Config (ID, ID, Location);
   procedure Require (Value : Boolean; Text : String) is
   begin
      if not Value then
         raise Program_Error with "Ada CalibrationConfiguration: " & Text;
      end if;
   end Require;
   procedure Check_ID (Value : AMS.MEL.IR.UCI_ID; Seed : Natural; Label : String) is
      Bytes : constant AMS.MEL.IR.UUID := AMS.MEL.IR.UUID_Value (Value);
   begin
      Require (AMS.MEL.IR.Descriptive_Label (Value) = Label, "complete label");
      for I in Bytes'Range loop
         Require (Bytes (I) = AMS.MEL.IR.Byte ((Seed + I) mod 256), "all UUID bytes");
      end loop;
   end Check_ID;
   procedure Check_Rich (Value : CC.Calibration_Collection; Seed : Natural) is
      Long : constant String (1 .. 700) := [others => 'z'];
   begin
      Require (CC.Length (Value) = 3, "outer order and length");
      for I in 1 .. 3 loop
         declare
            E : constant CC.Subsystem_Calibration := CC.Element (Value, I);
         begin
            Check_ID
              (E.Calibration_ID,
               (Seed + I - 1) mod 256,
               (if I = 1 then "" elsif I = 2 then "calibration-1" else "calibration-2"));
            Require (E.Interface_Value = CC.Accepted_Interface'Val (I - 1), "exact interface");
            if I = 1 then
               Require
                 (E.Item_Names.Is_Empty and E.Component_IDs.Is_Empty and E.Capability_IDs.Is_Empty,
                  "empty independent nested collections");
            elsif I = 2 then
               Require
                 (E.Item_Names.Length = 3
                  and E.Component_IDs.Length = 1
                  and E.Capability_IDs.Length = 2,
                  "unequal independent lengths");
               Require
                 (E.Item_Names.Element (1) = ""
                  and E.Item_Names.Element (2) = "one"
                  and E.Item_Names.Element (3) = Long,
                  "empty/ordered/long names");
               Check_ID (E.Component_IDs.Element (1), 32, "component");
               Check_ID (E.Capability_IDs.Element (1), 64, "capability-a");
               Check_ID (E.Capability_IDs.Element (2), 96, "");
            else
               Require
                 (E.Item_Names.Length = 1
                  and E.Component_IDs.Length = 2
                  and E.Capability_IDs.Is_Empty,
                  "independent collections, not zipped");
               Require (E.Item_Names.Element (1) = "last", "last name");
               Check_ID (E.Component_IDs.Element (1), 128, "");
               Check_ID (E.Component_IDs.Element (2), 160, "component-b");
            end if;
         end;
      end loop;
   end Check_Rich;

   procedure Fidelity (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "calibration-rich");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
      Saved   : CC.Calibration_Collection;
   begin
      C2.Enable (Channel);
      declare
         First  : CC.Configuration_Request :=
           C2.Submit_Calibration_Configuration (Channel, (0, 16#FFFF_FFFF#));
         Second : CC.Configuration_Request :=
           C2.Submit_Calibration_Configuration (Channel, (16#FFFF_FFFF#, 0));
         R1     : constant CC.Configuration_Result := CC.Wait (First, 5_000);
         R2     : constant CC.Configuration_Result := CC.Wait (Second, 5_000);
      begin
         Require
           (CC.Is_Open (First) and CC.Status (R1) = Ch.Success and CC.Status (R2) = Ch.Success,
            "requests");
         Check_Rich (CC.Configuration (R1), 0);
         Check_Rich (CC.Configuration (R2), 255);
         Check_Rich (CC.Configuration (CC.Wait (First, 0)), 0);
         Saved := CC.Configuration (R1);
         declare
            Copy : CC.Calibration_Collection := Saved;
            E    : CC.Subsystem_Calibration := CC.Element (Copy, 2);
         begin
            E.Capability_IDs.Clear;
            E.Item_Names.Clear;
            CC.Append (Copy, E);
            Require (CC.Length (Copy) = 4, "independent Ada collection");
            Check_Rich (Saved, 0);
         end;
         CC.Close (First);
         CC.Close (First);
         CC.Close (Second);
         Require (not CC.Is_Open (First), "closed observer");
      end;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
      Check_Rich (Saved, 0); -- no provider object, channel, session, or request remains
      for N in 0 .. 1 loop
         declare
            P : AMS.MEL.Session :=
              AMS.MEL.Open
                (Provider_Path, (if N = 0 then "calibration-empty" else "calibration-one"));
            C : C2.Control_Channel := C2.Open (P, Config);
         begin
            C2.Enable (C);
            declare
               Q : CC.Configuration_Request := C2.Submit_Calibration_Configuration (C, (1, 2));
               R : constant CC.Configuration_Result := CC.Wait (Q, 5_000);
            begin
               Require
                 (CC.Status (R) = Ch.Success and then CC.Length (CC.Configuration (R)) = N,
                  "empty success is not rejection");
               CC.Close (Q);
            end;
            C2.Close (C);
            AMS.MEL.Close (P);
         end;
      end loop;
   end Fidelity;

   procedure Outcomes (Provider_Path : String) is
      procedure Expect (Scenario : String; Reject : Boolean := False; Long : Boolean := False) is
         P : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
         C : C2.Control_Channel := C2.Open (P, Config);
      begin
         C2.Enable (C);
         declare
            Q : CC.Configuration_Request := C2.Submit_Calibration_Configuration (C, (5, 6));
         begin
            if Reject then
               declare
                  R : constant CC.Configuration_Result := CC.Wait (Q, 5_000);
               begin
                  Require (CC.Status (R) = Ch.Rejected, "explicit rejection");
                  Require
                    (CC.Rejection_Code (R)
                     = (if Scenario = "no-calibration"
                        then Ch.Unsupported
                        else Ch.Invalid_Parameters),
                     "MEL rejection code");
                  if Long then
                     Require
                       (CC.Description (R) = String'(1 .. 700 => 'r'),
                        "complete cached rejection text");
                  end if;
               end;
            else
               begin
                  declare
                     R : constant CC.Configuration_Result := CC.Wait (Q, 5_000);
                  begin
                     raise Program_Error with "failure returned a result " & CC.Status (R)'Image;
                  end;
               exception
                  when AMS.MEL.Provider_Error =>
                     null;
               end;
            end if;
            CC.Close (Q);
         end;
         C2.Close (C);
         AMS.MEL.Close (P);
      end Expect;
   begin
      Expect ("no-calibration", True);
      Expect ("calibration-reject", True);
      Expect ("calibration-reject-long", True, True);
      Expect ("calibration-null");
      Expect ("calibration-future-throw");
      Expect ("calibration-sentinel");
      Expect ("calibration-unknown");
      Expect ("calibration-nul");
      Expect ("calibration-utf8");
      Ada.Environment_Variables.Set ("AMS_MEL_TEST_CALIBRATION_COPY_FAILURE", "allocation");
      Expect ("calibration-rich");
   end Outcomes;

   procedure Metadata (Provider_Path : String) is
      P      : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "calibration-callback-rich");
      C      : C2.Control_Channel := C2.Open (P, Config);
      Stream : M.Calibration_Configuration_Stream := M.Open_Calibration_Configuration (C, 3);
      Saved  : CC.Calibration_Collection;
   begin
      Require (M.Is_Open (Stream), "Attached callback owner");
      begin
         declare
            Duplicate : constant M.Calibration_Configuration_Stream :=
              M.Open_Calibration_Configuration (C);
         begin
            raise Program_Error with "duplicate callback accepted " & M.Is_Open (Duplicate)'Image;
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      C2.Close (C);
      AMS.MEL.Close (P);
      Saved := M.Receive_Calibration_Configuration (Stream);
      Check_Rich (Saved, 10);
      Require
        (CC.Length (M.Receive_Calibration_Configuration (Stream)) = 0, "empty metadata vector");
      Check_Rich (M.Receive_Calibration_Configuration (Stream), 20);
      Require
        (M.Calibration_Configuration_Counters (Stream).Events_Received = 3,
         "synchronous registration");
      begin
         Saved := M.Receive_Calibration_Configuration (Stream);
         raise Program_Error with "drained queue did not stop";
      exception
         when AMS.MEL.IR.Stream_Stopped =>
            null;
      end;
      M.Close (Stream);
      M.Close (Stream);
      Check_Rich (Saved, 10);
      for N in 0 .. 2 loop
         declare
            Parent  : AMS.MEL.Session :=
              AMS.MEL.Open
                (Provider_Path,
                 (if N = 0
                  then "calibration-callback-overflow"
                  elsif N = 1
                  then "calibration-callback-malformed"
                  else "calibration-callback-allocation"));
            Channel : C2.Control_Channel := C2.Open (Parent, Config);
         begin
            if N = 2 then
               Ada.Environment_Variables.Set
                 ("AMS_MEL_TEST_CALIBRATION_COPY_FAILURE", "allocation");
            end if;
            declare
               S      : M.Calibration_Configuration_Stream :=
                 M.Open_Calibration_Configuration (Channel, 1);
               Counts : constant M.Metadata_Counters := M.Calibration_Configuration_Counters (S);
            begin
               Require
                 (Counts.Events_Dropped_Queue_Full = (if N = 0 then 4 else 0),
                  "bounded incoming drops");
               Require
                 (Counts.Malformed_Or_Unsupported = (if N = 1 then 5 elsif N = 2 then 1 else 0),
                  "malformed atomic drops");
               Check_Rich
                 (M.Receive_Calibration_Configuration (S),
                  (if N = 0 then 0 elsif N = 1 then 10 else 1));
               M.Close (S);
            end;
            C2.Close (Channel);
            AMS.MEL.Close (Parent);
         end;
      end loop;
   end Metadata;

   procedure Submission_And_Optional (Provider_Path : String) is
      P : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "no-calibration");
      C : C2.Control_Channel := C2.Open (P, Config);
   begin
      begin
         declare
            Q : constant CC.Configuration_Request :=
              C2.Submit_Calibration_Configuration (C, (1, 2));
         begin
            raise Program_Error with "Attached command accepted " & CC.Is_Open (Q)'Image;
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      begin
         declare
            S : constant M.Calibration_Configuration_Stream := M.Open_Calibration_Configuration (C);
         begin
            raise Program_Error with "unsupported callback accepted " & M.Is_Open (S)'Image;
         end;
      exception
         when CC.Not_Supported =>
            null;
      end;
      C2.Enable (C); -- failed optional registration does not poison existing C2
      C2.Close (C);
      AMS.MEL.Close (P);
   end Submission_And_Optional;

   procedure Test_Held (Provider_Path : String) is
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
      Pin              : constant System.Address := Dlopen (IC.To_C (Provider_Path), 2);
      Release          : constant Release_Function :=
        To_Release (Dlsym (Pin, IC.To_C ("mock_completion_release_family")));
      Calibration_Send : constant IC.unsigned := 9;

      procedure Expect_Timeout (Request : CC.Configuration_Request) is
      begin
         declare
            Ignored : constant CC.Configuration_Result := CC.Wait (Request, 0);
         begin
            raise Program_Error with "Ada CalibrationConfigurationCmd Wait(0) did not time out";
         end;
      exception
         when AMS.MEL.IR.Timeout_Error =>
            null;
      end Expect_Timeout;
   begin
      Require (Pin /= System.Null_Address and then Release /= null, "provider gate");
      declare
         Parent  : AMS.MEL.Session :=
           AMS.MEL.Open_With_Options
             (Provider_Path, "calibration-held", Options => (Max_Async_Requests => 1));
         Channel : C2.Control_Channel := C2.Open (Parent, Config);
      begin
         C2.Enable (Channel);
         declare
            Request : CC.Configuration_Request :=
              C2.Submit_Calibration_Configuration (Channel, (21, 210));
         begin
            Expect_Timeout (Request);
            begin
               declare
                  Refused : CC.Configuration_Request :=
                    C2.Submit_Calibration_Configuration (Channel, (22, 220));
               begin
                  CC.Close (Refused);
                  raise Program_Error with "Ada CalibrationConfigurationCmd admission not enforced";
               end;
            exception
               when Error : AMS.MEL.Resource_Exhausted =>
                  Require
                    (Ada.Exceptions.Exception_Message (Error) = "async request limit reached",
                     "admission");
            end;
            Expect_Timeout (Request);
            Require (Release (Calibration_Send, 1) = 1, "release");
            Check_Rich (CC.Configuration (CC.Wait (Request, 5_000)), 21);
            CC.Close (Request);
         end;
         declare
            Observer : CC.Configuration_Request :=
              C2.Submit_Calibration_Configuration (Channel, (23, 230));
         begin
            C2.Close (Channel);
            AMS.MEL.Close (Parent);
            Expect_Timeout (Observer);
            Require (Release (Calibration_Send, 1) = 1, "pending release");
            Check_Rich (CC.Configuration (CC.Wait (Observer, 5_000)), 23);
            CC.Close (Observer);
         end;
      end;
      --  Public request Close before completion is not cancellation; finalization is silent.
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "calibration-held");
         Channel : C2.Control_Channel := C2.Open (Parent, Config);
      begin
         C2.Enable (Channel);
         declare
            Closed : CC.Configuration_Request :=
              C2.Submit_Calibration_Configuration (Channel, (24, 24));
         begin
            CC.Close (Closed);
         end;
         declare
            Finalized : constant CC.Configuration_Request :=
              C2.Submit_Calibration_Configuration (Channel, (25, 25));
         begin
            Require (CC.Is_Open (Finalized), "finalized request open");
         end;
         C2.Close (Channel);
         AMS.MEL.Close (Parent);
         Require (Release (Calibration_Send, 2) = 1, "closed release");
      end;
      Require (Dlclose (Pin) = 0, "provider unpin");
   end Test_Held;

   procedure Run (Provider_Path : String) is
   begin
      Fidelity (Provider_Path);
      Test_Held (Provider_Path);
      Outcomes (Provider_Path);
      Metadata (Provider_Path);
      Submission_And_Optional (Provider_Path);
      CC.Testing.Check_Malformed;
      Ada.Text_IO.Put_Line
        ("PASS: safe Ada CalibrationConfiguration owned nested data and metadata");
   end Run;
end AMS_MEL_IR_Calibration_Configuration_Tests;
