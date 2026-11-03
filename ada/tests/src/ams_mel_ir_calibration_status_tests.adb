with Ada.Text_IO;
with Ada.Exceptions;
with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;
with Ada.Environment_Variables;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.C2.Metadata;
with AMS.MEL.IR.Calibration_Configuration;
with AMS.MEL.IR.Calibration_Status;
with AMS.MEL.IR.Calibration_Status.Testing;
with AMS.MEL.IR.Channel;

package body AMS_MEL_IR_Calibration_Status_Tests is
   package CS renames AMS.MEL.IR.Calibration_Status;
   package C2 renames AMS.MEL.IR.C2;
   package M renames AMS.MEL.IR.C2.Metadata;
   package Ch renames AMS.MEL.IR.Channel;
   use type CS.Calibration_Result;
   use type CS.Nanoseconds;
   use type CS.Outcome;
   use type CS.Error_Code;
   use type AMS.MEL.IR.Byte;
   use type AMS.MEL.IR.Counter;
   Zero     : constant AMS.MEL.IR.UUID := [others => 0];
   ID       : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "calibration-status");
   Location : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock");
   Config   : constant C2.Control_Config := C2.Create_Config (ID, ID, Location);
   Times    : constant array (1 .. 5) of CS.Nanoseconds :=
     [0, -1, CS.Nanoseconds'First, CS.Nanoseconds'Last, 1_700_000_000_123_456_789];

   procedure Require (Value : Boolean; Text : String) is
   begin
      if not Value then
         raise Program_Error with "Ada CalibrationStatus: " & Text;
      end if;
   end Require;
   function Image (Value : Natural) return String is
      Text : constant String := Value'Image;
   begin
      return Text (Text'First + 1 .. Text'Last);
   end Image;
   function ID_Is (Value : AMS.MEL.IR.UCI_ID; Seed : Natural; Label : String) return Boolean is
      Bytes : constant AMS.MEL.IR.UUID := AMS.MEL.IR.UUID_Value (Value);
   begin
      if AMS.MEL.IR.Descriptive_Label (Value) /= Label then
         return False;
      end if;
      for I in Bytes'Range loop
         if Bytes (I) /= AMS.MEL.IR.Byte ((Seed + I) mod 256) then
            return False;
         end if;
      end loop;
      return True;
   end ID_Is;
   function Item_Is
     (Value : CS.Completed_Item; Name : String; Result : CS.Calibration_Result; Why : String)
      return Boolean
   is (CS.Item_Name (Value) = Name
       and then CS.Result (Value) = Result
       and then CS.Fail_Reason (Value) = Why);
   --  Completed calibration number I (1 .. 5) of the mock's status_values (Seed).
   function Completed_Is
     (Value : CS.Completed_Calibration; Seed : Natural; I : Positive) return Boolean
   is
      Counts : constant array (1 .. 5) of Natural := [0, 1, 3, 2, 0];
      Label  : constant String := (if I = 1 then "" else "completed-" & Image (I - 1));
   begin
      if not ID_Is (CS.Calibration_ID (Value), (Seed + 100 + I - 1) mod 256, Label)
        or else CS.Time_Tag (Value) /= Times (I)
        or else CS.Result (Value) /= CS.Calibration_Result'Val (I - 1)
        or else CS.Item_Count (Value) /= Counts (I)
      then
         return False;
      end if;
      case I is
         when 1      =>
            return CS.Fail_Reason (Value) = "";

         when 2      =>
            return
              CS.Fail_Reason (Value) = ""
              and then Item_Is (CS.Item_At (Value, 1), "alpha", CS.Completed, "");

         when 3      =>
            return
              CS.Fail_Reason (Value) = String'(1 .. 700 => 'f')
              and then Item_Is (CS.Item_At (Value, 1), "", CS.Failed, "item failure")
              and then Item_Is
                         (CS.Item_At (Value, 2), "beta", CS.Interrupted, String'(1 .. 600 => 'q'))
              and then Item_Is (CS.Item_At (Value, 3), "gamma", CS.Not_Set, "");

         when 4      =>
            return
              CS.Fail_Reason (Value) = "interrupted by operator"
              and then Item_Is (CS.Item_At (Value, 1), "delta", CS.Not_Calibrated, "not calibrated")
              and then Item_Is
                         (CS.Item_At (Value, 2), "delta", CS.Not_Calibrated, "not calibrated");

         when others =>
            return
              CS.Fail_Reason (Value)
              = Character'Val (16#C3#) & Character'Val (16#BC#) & "nicode not calibrated";
      end case;
   end Completed_Is;
   function Active_Is (Value : CS.Status_Value; Seed : Natural) return Boolean
   is (CS.Active_Count (Value) = 3
       and then ID_Is (CS.Active_ID (Value, 1), Seed mod 256, "")
       and then ID_Is (CS.Active_ID (Value, 2), (Seed + 40) mod 256, "active-b")
       and then ID_Is (CS.Active_ID (Value, 3), Seed mod 256, ""));
   function Rich_Is (Value : CS.Status_Value; Seed : Natural) return Boolean is
   begin
      if not Active_Is (Value, Seed) or else CS.Completed_Count (Value) /= 5 then
         return False;
      end if;
      for I in 1 .. 5 loop
         if not Completed_Is (CS.Completed_At (Value, I), Seed, I) then
            return False;
         end if;
      end loop;
      return True;
   end Rich_Is;
   procedure Check_Rich (Value : CS.Status_Value; Seed : Natural) is
   begin
      Require (Rich_Is (Value, Seed), "complete rich status seed" & Seed'Image);
   end Check_Rich;

   --  Executed Ada-side mutation controls: each single loss must defeat the predicate.
   procedure Mutation_Controls (Saved : CS.Status_Value; Seed : Natural) is
      function Rebuild
        (Drop_Item, Drop_Item_Result, Drop_Active, Drop_Completed : Boolean := False)
         return CS.Status_Value
      is
         Result : CS.Status_Value;
      begin
         if not Drop_Active then
            for I in 1 .. CS.Active_Count (Saved) loop
               CS.Append_Active (Result, CS.Active_ID (Saved, I));
            end loop;
         end if;
         if not Drop_Completed then
            for I in 1 .. CS.Completed_Count (Saved) loop
               declare
                  Source : constant CS.Completed_Calibration := CS.Completed_At (Saved, I);
                  Copy   : CS.Completed_Calibration :=
                    CS.Create_Calibration
                      (CS.Calibration_ID (Source),
                       CS.Time_Tag (Source),
                       CS.Result (Source),
                       CS.Fail_Reason (Source));
               begin
                  for J in 1 .. CS.Item_Count (Source) loop
                     declare
                        Item : constant CS.Completed_Item := CS.Item_At (Source, J);
                     begin
                        if not (Drop_Item and then I = 3 and then J = 3) then
                           CS.Append_Item
                             (Copy,
                              CS.Create_Item
                                (CS.Item_Name (Item),
                                 (if Drop_Item_Result and then I = 3 and then J = 2
                                  then CS.Not_Set
                                  else CS.Result (Item)),
                                 CS.Fail_Reason (Item)));
                        end if;
                     end;
                  end loop;
                  CS.Append_Completed (Result, Copy);
               end;
            end loop;
         end if;
         return Result;
      end Rebuild;
   begin
      Require (Rich_Is (Rebuild, Seed), "faithful rebuild positive control");
      Require (not Rich_Is (Rebuild (Drop_Item => True), Seed), "lost nested item detected");
      Require (not Rich_Is (Rebuild (Drop_Item_Result => True), Seed), "lost item result detected");
      Require
        (not Rich_Is (Rebuild (Drop_Active => True), Seed), "lost active collection detected");
      Require
        (not Rich_Is (Rebuild (Drop_Completed => True), Seed),
         "lost completed collection detected");
   end Mutation_Controls;

   procedure Fidelity (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "calstatus-rich");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
      Saved   : CS.Status_Value;
   begin
      C2.Enable (Channel);
      declare
         First  : CS.Status_Request := C2.Submit_Calibration_Status (Channel, (0, 16#FFFF_FFFF#));
         Second : CS.Status_Request := C2.Submit_Calibration_Status (Channel, (16#FFFF_FFFF#, 0));
         R1     : constant CS.Status_Result := CS.Wait (First, 5_000);
         R2     : constant CS.Status_Result := CS.Wait (Second, 5_000);
      begin
         Require
           (CS.Is_Open (First) and CS.Status (R1) = Ch.Success and CS.Status (R2) = Ch.Success,
            "requests");
         --  The mock seeds by command ID only; both IDs survive independently.
         Check_Rich (CS.Value (R1), 0);
         Check_Rich (CS.Value (R2), 255);
         Check_Rich (CS.Value (CS.Wait (First, 0)), 0);
         Saved := CS.Value (R1);
         Mutation_Controls (Saved, 0);
         declare
            Copy : CS.Status_Value := Saved;
         begin
            CS.Append_Active (Copy, ID);
            Require (CS.Active_Count (Copy) = 4, "independent Ada copy");
            Check_Rich (Saved, 0);
         end;
         CS.Close (First);
         CS.Close (First);
         CS.Close (Second);
         Require (not CS.Is_Open (First), "closed observer");
      end;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
      Check_Rich (Saved, 0); --  no provider object, channel, session or request remains
      for N in 0 .. 3 loop
         declare
            Scenario : constant String :=
              (case N is
                 when 0      => "calstatus-empty",
                 when 1      => "calstatus-active-only",
                 when 2      => "calstatus-completed-only",
                 when others => "calstatus-one");
            P        : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
            C        : C2.Control_Channel := C2.Open (P, Config);
         begin
            C2.Enable (C);
            declare
               Q : CS.Status_Request := C2.Submit_Calibration_Status (C, (7, 8));
               R : constant CS.Status_Result := CS.Wait (Q, 5_000);
               V : constant CS.Status_Value := CS.Value (R);
            begin
               Require (CS.Status (R) = Ch.Success, "shape success is not rejection");
               case N is
                  when 0      =>
                     Require
                       (CS.Active_Count (V) = 0 and CS.Completed_Count (V) = 0, "empty status");

                  when 1      =>
                     Require
                       (Active_Is (V, 7) and CS.Completed_Count (V) = 0, "active-only status");

                  when 2      =>
                     Require
                       (CS.Active_Count (V) = 0 and CS.Completed_Count (V) = 5, "completed-only");
                     for I in 1 .. 5 loop
                        Require (Completed_Is (CS.Completed_At (V, I), 7, I), "completed entry");
                     end loop;

                  when others =>
                     Require
                       (CS.Active_Count (V) = 1
                        and then ID_Is (CS.Active_ID (V, 1), 47, "active-b")
                        and then CS.Completed_Count (V) = 1
                        and then Completed_Is (CS.Completed_At (V, 1), 7, 3),
                        "one of each, unrelated");
               end case;
               CS.Close (Q);
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
            Q : CS.Status_Request := C2.Submit_Calibration_Status (C, (5, 6));
         begin
            if Reject then
               declare
                  R : constant CS.Status_Result := CS.Wait (Q, 5_000);
               begin
                  Require (CS.Status (R) = Ch.Rejected, "explicit rejection");
                  Require
                    (CS.Rejection_Code (R)
                     = (if Scenario = "no-calstatus" then Ch.Unsupported else Ch.Invalid_State),
                     "MEL rejection code");
                  if Long then
                     Require
                       (CS.Description (R) = String'(1 .. 700 => 'r'),
                        "complete cached rejection text");
                  elsif Scenario = "calstatus-reject" then
                     Require (CS.Description (R) = "calibration status rejected", "rejection text");
                  end if;
                  Require (CS.Status (CS.Wait (Q, 0)) = Ch.Rejected, "cached rejection");
               end;
            else
               for Attempt in 1 .. 2 loop
                  begin
                     declare
                        R : constant CS.Status_Result := CS.Wait (Q, 5_000);
                     begin
                        raise Program_Error
                          with Scenario & " failure returned a result " & CS.Status (R)'Image;
                     end;
                  exception
                     when AMS.MEL.Provider_Error =>
                        null;
                  end;
               end loop;
            end if;
            CS.Close (Q);
         end;
         C2.Close (C);
         AMS.MEL.Close (P);
      end Expect;
   begin
      Expect ("no-calstatus", True);
      Expect ("calstatus-reject", True);
      Expect ("calstatus-reject-long", True, True);
      Expect ("calstatus-null");
      Expect ("calstatus-future-throw");
      Expect ("calstatus-sentinel");
      Expect ("calstatus-unknown");
      Expect ("calstatus-item-sentinel");
      Expect ("calstatus-item-unknown");
      Expect ("calstatus-nul");
      Expect ("calstatus-item-nul");
      Expect ("calstatus-utf8");
      Expect ("calstatus-item-utf8");
      Expect ("calstatus-completed-utf8");
      Expect ("calstatus-unknown-error");
      Ada.Environment_Variables.Set ("AMS_MEL_TEST_CALIBRATION_STATUS_COPY_FAILURE", "allocation");
      Expect ("calstatus-rich");
   end Outcomes;

   procedure Metadata (Provider_Path : String) is
      P      : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "calstatus-callback-rich");
      C      : C2.Control_Channel := C2.Open (P, Config);
      Stream : M.Calibration_Status_Stream := M.Open_Calibration_Status (C, 3);
      Saved  : CS.Status_Value;
   begin
      Require (M.Is_Open (Stream), "Attached callback owner");
      begin
         declare
            Duplicate : constant M.Calibration_Status_Stream := M.Open_Calibration_Status (C);
         begin
            raise Program_Error with "duplicate callback accepted " & M.Is_Open (Duplicate)'Image;
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      Require
        (M.Calibration_Status_Counters (Stream).Events_Received = 3, "synchronous registration");
      C2.Close (C);
      AMS.MEL.Close (P);
      Saved := M.Receive_Calibration_Status (Stream);
      Check_Rich (Saved, 10);
      declare
         Empty : constant CS.Status_Value := M.Receive_Calibration_Status (Stream);
      begin
         Require
           (CS.Active_Count (Empty) = 0 and CS.Completed_Count (Empty) = 0,
            "empty metadata status");
      end;
      Check_Rich (M.Receive_Calibration_Status (Stream), 20);
      begin
         Saved := M.Receive_Calibration_Status (Stream);
         raise Program_Error with "drained queue did not stop";
      exception
         when AMS.MEL.IR.Stream_Stopped =>
            null;
      end;
      M.Close (Stream);
      M.Close (Stream);
      Require (not M.Is_Open (Stream), "closed stream");
      Check_Rich (Saved, 10);
      for N in 0 .. 2 loop
         declare
            Parent  : AMS.MEL.Session :=
              AMS.MEL.Open
                (Provider_Path,
                 (case N is
                    when 0      => "calstatus-callback-overflow",
                    when 1      => "calstatus-callback-malformed",
                    when others => "calstatus-callback-allocation"));
            Channel : C2.Control_Channel := C2.Open (Parent, Config);
         begin
            if N = 2 then
               Ada.Environment_Variables.Set
                 ("AMS_MEL_TEST_CALIBRATION_STATUS_COPY_FAILURE", "allocation");
            end if;
            declare
               S      : M.Calibration_Status_Stream := M.Open_Calibration_Status (Channel, 1);
               Counts : constant M.Metadata_Counters := M.Calibration_Status_Counters (S);
            begin
               Require
                 (Counts.Events_Received
                  = (case N is
                       when 0      => 5,
                       when 1      => 11,
                       when others => 2),
                  "received counter");
               Require
                 (Counts.Events_Dropped_Queue_Full = (if N = 0 then 4 else 0),
                  "bounded incoming drops");
               Require
                 (Counts.Malformed_Or_Unsupported
                  = (case N is
                       when 0      => 0,
                       when 1      => 10,
                       when others => 1),
                  "malformed atomic drops");
               Check_Rich
                 (M.Receive_Calibration_Status (S),
                  (case N is
                     when 0      => 0,
                     when 1      => 10,
                     when others => 1));
               M.Close (S);
            end;
            C2.Close (Channel);
            AMS.MEL.Close (Parent);
         end;
      end loop;
   end Metadata;

   procedure Optional_And_Independent (Provider_Path : String) is
   begin
      declare
         P : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "no-calstatus");
         C : C2.Control_Channel := C2.Open (P, Config);
      begin
         begin
            declare
               Q : constant CS.Status_Request := C2.Submit_Calibration_Status (C, (1, 2));
            begin
               raise Program_Error with "Attached command accepted " & CS.Is_Open (Q)'Image;
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         begin
            declare
               S : constant M.Calibration_Status_Stream := M.Open_Calibration_Status (C);
            begin
               raise Program_Error with "unsupported callback accepted " & M.Is_Open (S)'Image;
            end;
         exception
            when CS.Not_Supported =>
               null;
         end;
         begin
            declare
               S : constant M.Calibration_Status_Stream := M.Open_Calibration_Status (C);
            begin
               raise Program_Error with "second registration accepted " & M.Is_Open (S)'Image;
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null; --  one-shot even after NotSupported
         end;
         --  Distinct from the independent CalibrationConfiguration owner's own NotSupported.
         begin
            declare
               S : constant M.Calibration_Configuration_Stream :=
                 M.Open_Calibration_Configuration (C);
            begin
               raise Program_Error with "configuration callback accepted " & M.Is_Open (S)'Image;
            end;
         exception
            when AMS.MEL.IR.Calibration_Configuration.Not_Supported =>
               null;
         end;
         C2.Enable (C); --  failed optional registration does not poison existing C2
         C2.Close (C);
         AMS.MEL.Close (P);
      end;
      for N in 0 .. 1 loop
         declare
            P : AMS.MEL.Session :=
              AMS.MEL.Open
                (Provider_Path,
                 (if N = 0 then "calstatus-register-fail" else "calstatus-register-throw"));
            C : C2.Control_Channel := C2.Open (P, Config);
         begin
            begin
               declare
                  S : constant M.Calibration_Status_Stream := M.Open_Calibration_Status (C);
               begin
                  raise Program_Error with "failed registration accepted " & M.Is_Open (S)'Image;
               end;
            exception
               when Error : AMS.MEL.Provider_Error =>
                  Require
                    (Ada.Exceptions.Exception_Message (Error)
                     = (if N = 0
                        then "CalibrationStatus callback registration failed"
                        else "provider exception during CalibrationStatus registration"),
                     "Fail and exception stay distinct from NotSupported");
            end;
            C2.Close (C);
            AMS.MEL.Close (P);
         end;
      end loop;
      --  A callback delivered during the unlocked send neither completes the request nor is
      --  needed by it; closing the metadata owner does not cancel the request.
      declare
         P : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "calstatus-late");
         C : C2.Control_Channel := C2.Open (P, Config);
      begin
         C2.Enable (C);
         declare
            S : M.Calibration_Status_Stream := M.Open_Calibration_Status (C, 3);
            Q : CS.Status_Request := C2.Submit_Calibration_Status (C, (3, 4));
         begin
            M.Close (S);
            Check_Rich (CS.Value (CS.Wait (Q, 5_000)), 3);
            CS.Close (Q);
         end;
         C2.Close (C); --  retained callback invoked after public Close touches no Ada owner
         AMS.MEL.Close (P);
      end;
   end Optional_And_Independent;

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
      Pin         : constant System.Address := Dlopen (IC.To_C (Provider_Path), 2);
      Release     : constant Release_Function :=
        To_Release (Dlsym (Pin, IC.To_C ("mock_completion_release_family")));
      Status_Send : constant IC.unsigned := 10;

      procedure Expect_Timeout (Request : CS.Status_Request) is
      begin
         declare
            Ignored : constant CS.Status_Result := CS.Wait (Request, 0);
         begin
            raise Program_Error with "Ada CalibrationStatusCmd Wait(0) did not time out";
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
             (Provider_Path, "calstatus-held", Options => (Max_Async_Requests => 1));
         Other   : AMS.MEL.Session :=
           AMS.MEL.Open_With_Options
             (Provider_Path, "calstatus-held", Options => (Max_Async_Requests => 1));
         Channel : C2.Control_Channel := C2.Open (Parent, Config);
         Second  : C2.Control_Channel := C2.Open (Other, Config);
      begin
         C2.Enable (Channel);
         C2.Enable (Second);
         declare
            Request : CS.Status_Request := C2.Submit_Calibration_Status (Channel, (21, 210));
         begin
            Expect_Timeout (Request);
            begin
               declare
                  Refused : CS.Status_Request := C2.Submit_Calibration_Status (Channel, (22, 220));
               begin
                  CS.Close (Refused);
                  raise Program_Error with "Ada CalibrationStatusCmd admission not enforced";
               end;
            exception
               when Error : AMS.MEL.Resource_Exhausted =>
                  Require
                    (Ada.Exceptions.Exception_Message (Error) = "async request limit reached",
                     "admission");
            end;
            --  Admission is Session-specific: another Session still admits a request.
            declare
               Independent : CS.Status_Request := C2.Submit_Calibration_Status (Second, (31, 31));
            begin
               Expect_Timeout (Request);
               Require (Release (Status_Send, 2) = 1, "release");
               Check_Rich (CS.Value (CS.Wait (Request, 5_000)), 21);
               Check_Rich (CS.Value (CS.Wait (Independent, 5_000)), 31);
               Check_Rich (CS.Value (CS.Wait (Request, 0)), 21); --  repeated cached Wait
               CS.Close (Independent);
            end;
            CS.Close (Request);
         end;
         declare
            Observer : CS.Status_Request := C2.Submit_Calibration_Status (Channel, (23, 230));
         begin
            C2.Close (Channel);
            AMS.MEL.Close (Parent);
            Expect_Timeout (Observer);
            Require (Release (Status_Send, 1) = 1, "pending release");
            Check_Rich (CS.Value (CS.Wait (Observer, 5_000)), 23);
            CS.Close (Observer);
         end;
         C2.Close (Second);
         AMS.MEL.Close (Other);
      end;
      --  Public request Close before completion is not cancellation; finalization is silent.
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "calstatus-held");
         Channel : C2.Control_Channel := C2.Open (Parent, Config);
      begin
         C2.Enable (Channel);
         declare
            Closed : CS.Status_Request := C2.Submit_Calibration_Status (Channel, (24, 24));
         begin
            CS.Close (Closed);
         end;
         declare
            Finalized : constant CS.Status_Request :=
              C2.Submit_Calibration_Status (Channel, (25, 25));
         begin
            Require (CS.Is_Open (Finalized), "finalized request open");
         end;
         C2.Close (Channel);
         AMS.MEL.Close (Parent);
         Require (Release (Status_Send, 2) = 1, "closed release");
      end;
      Require (Dlclose (Pin) = 0, "provider unpin");
   end Test_Held;

   procedure Run (Provider_Path : String) is
   begin
      CS.Testing.Check_Representation;
      Fidelity (Provider_Path);
      Test_Held (Provider_Path);
      Outcomes (Provider_Path);
      Metadata (Provider_Path);
      Optional_And_Independent (Provider_Path);
      CS.Testing.Check_Malformed;
      Ada.Text_IO.Put_Line
        ("PASS: safe Ada CalibrationStatus owned nested data, request and metadata lifecycles");
   end Run;
end AMS_MEL_IR_Calibration_Status_Tests;
