with Ada.Environment_Variables;
with Ada.Exceptions;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.RF.C2;
with AMS.MEL.RF.C2.Virtual_Aperture_Queries;
with AMS.MEL.RF.C2.Virtual_Aperture_Notifications;
with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;
with GNAT.Source_Info;

procedure AMS_MEL_RF_VA_Notifications is
   package C2 renames AMS.MEL.RF.C2;
   package Q renames C2.Virtual_Aperture_Queries;
   package N renames C2.Virtual_Aperture_Notifications;
   package CS renames Interfaces.C.Strings;
   use type Interfaces.C.unsigned;
   use type Interfaces.C.int;
   use type Interfaces.C.size_t;
   use type Interfaces.Unsigned_64;
   use type C2.Request_Outcome;
   use type Q.Status_Kind;
   use type System.Address;
   function DL_Open (Path : CS.chars_ptr; Flags : Interfaces.C.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function DL_Sym (Handle : System.Address; Name : CS.chars_ptr) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   function DL_Close (Handle : System.Address) return Interfaces.C.int
   with Import, Convention => C, External_Name => "dlclose";
   type Last_Access is access function return Interfaces.C.unsigned with Convention => C;
   type Value_Access is
     access function (Index, Field : Interfaces.C.unsigned) return Interfaces.C.size_t
   with Convention => C;
   type Emit_Access is
     access function (Index, Count, Late : Interfaces.C.unsigned) return Interfaces.C.unsigned
   with Convention => C;
   type Query_Access is
     access function (Method : Interfaces.C.unsigned) return Interfaces.C.unsigned
   with Convention => C;
   function To_Last is new Ada.Unchecked_Conversion (System.Address, Last_Access);
   function To_Value is new Ada.Unchecked_Conversion (System.Address, Value_Access);
   function To_Emit is new Ada.Unchecked_Conversion (System.Address, Emit_Access);
   function To_Query is new Ada.Unchecked_Conversion (System.Address, Query_Access);
   Provider : constant String :=
     Ada.Environment_Variables.Value ("AMS_MEL_TEST_PROVIDER_DIR") & "/libmock_rf_provider.so";
   Path     : CS.chars_ptr := CS.New_String (Provider);
   Library  : constant System.Address := DL_Open (Path, 2);
   function Symbol (Name : String) return System.Address is
      Text   : CS.chars_ptr := CS.New_String (Name);
      Result : constant System.Address := DL_Sym (Library, Text);
   begin
      CS.Free (Text);
      return Result;
   end Symbol;
   Last     : constant Last_Access := To_Last (Symbol ("mock_rf_va_notification_last"));
   Value    : constant Value_Access := To_Value (Symbol ("mock_rf_va_notification_value"));
   Emit     : constant Emit_Access := To_Emit (Symbol ("mock_rf_va_notification_emit"));
   Queries  : constant Query_Access := To_Query (Symbol ("mock_rf_va_query_calls"));
   procedure Verify (Condition : Boolean; Location : String := GNAT.Source_Info.Source_Location) is
   begin
      if not Condition then
         raise Program_Error with "VA notification assertion at " & Location;
      end if;
   end Verify;
   function Config return C2.Virtual_Aperture_Config is
      Result               : C2.Virtual_Aperture_Config :=
        C2.Create_Virtual_Aperture_Config (16#FEDC_BA98#, 16#8000_0001#, "definition/β.json");
      First, Second, Third : AMS.MEL.IR.UUID := [others => 0];
   begin
      C2.Append_Local_Function_Info (Result, "alpha");
      C2.Append_Local_Function_Info (Result, "µ-local");
      C2.Append_Local_Function_Info (Result, "");
      First (1) := 16#80#;
      First (2) := 16#FF#;
      Second (0) := 16#FF#;
      Third (15) := 16#80#;
      C2.Append_Capability_ID (Result, AMS.MEL.IR.Create_UCI_ID (First, "first"));
      C2.Append_Capability_ID (Result, AMS.MEL.IR.Create_UCI_ID (Second, ""));
      C2.Append_Capability_ID (Result, AMS.MEL.IR.Create_UCI_ID (Third, "µ-third"));
      return Result;
   end Config;
   procedure Stopped (Object : N.Subscription) is
   begin
      N.Wait_For_Change (Object, 0);
      raise Program_Error with "stopped notification delivered";
   exception
      when N.Subscription_Stopped =>
         null;
   end Stopped;
   procedure Reject_Open (VA : in out C2.Virtual_Aperture'Class) is
   begin
      declare
         S : constant N.Subscription := N.Open (VA);
      begin
         Verify (not N.Is_Open (S));
         raise Program_Error with "invalid Open accepted";
      end;
   exception
      when AMS.MEL.Provider_Error =>
         null;
   end Reject_Open;
begin
   Verify
     (Library /= System.Null_Address
      and Last /= null
      and Value /= null
      and Emit /= null
      and Queries /= null);
   CS.Free (Path);
   declare
      Parent  : C2.C2_MEL := C2.Open (Provider, "c2:va-notify-reference");
      Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, Config);
   begin
      Verify (C2.Outcome (C2.Wait (Request, 3_000)) = C2.Created);
      declare
         VA           : C2.Virtual_Aperture := C2.Claim (Request);
         Index        : constant Interfaces.C.unsigned := Last.all;
         Subscription : N.Subscription := N.Open (VA);
         --  Initial state AFTER registration: no query-before-subscribe gap,
         --  still not an atomic transaction. No getter runs in the callback.
         Current      : Q.Status_Kind := Q.Query_Status (VA);
         Earlier      : constant Q.Instance_Status_Report :=
           Q.Snapshot_Instance_Status_Report (VA, 42);
         type Counts is array (0 .. 5) of Interfaces.C.unsigned;
         Before       : Counts;
      begin
         C2.Close (Request);
         Verify (N.Is_Open (Subscription) and Current = Q.Operational);
         Verify (DL_Close (Library) = 0); -- Only permanent shell now pins provider code.
         Reject_Open (VA);
         begin
            N.Wait_For_Change (Subscription, 0);
            raise Program_Error with "initial notification synthesized";
         exception
            when C2.Timeout_Error =>
               null;
         end;
         for M in Before'Range loop
            Before (M) := Queries (Interfaces.C.unsigned (M));
         end loop;
         Verify (Emit (Index, 3, 0) = 1);
         Verify
           (N.Statistics (Subscription).Callback_Entries = 3
            and N.Statistics (Subscription).Callbacks_Coalesced = 2
            and N.Statistics (Subscription).Pending);
         for M in Before'Range loop
            Verify (Before (M) = Queries (Interfaces.C.unsigned (M)));
         end loop;
         N.Wait_For_Change (Subscription, 100);
         Current := Q.Query_Status (VA);
         Verify (Current = Q.Degraded and Q.Status (Earlier) = Q.Operational);
         Verify (Emit (Index, 2, 0) = 1);
         Ada.Environment_Variables.Set ("AMS_MEL_TEST_VA_QUERY_EXCEPTION", "standard");
         begin
            Current := Q.Query_Status (VA);
            raise Program_Error with "query exception missing";
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Ada.Environment_Variables.Clear ("AMS_MEL_TEST_VA_QUERY_EXCEPTION");
         Verify (N.Statistics (Subscription).Pending);
         N.Wait_For_Change (Subscription, 0);
         Verify (Q.Status (Earlier) = Q.Operational);
         C2.Close (Parent);
         Current := Q.Query_Status (VA);
         Verify (Current = Q.Degraded);
         N.Unsubscribe (VA, Subscription);
         N.Unsubscribe (VA, Subscription);
         Verify
           (Value (Index, 1) = 1
            and N.Is_Open (Subscription)
            and N.Statistics (Subscription).Stopped);
         Stopped (Subscription);
         N.Close (Subscription);
         Verify (not N.Is_Open (Subscription));
         N.Close (Subscription);
         begin
            N.Wait_For_Change (Subscription, 0);
            raise Program_Error with "closed subscription accepted";
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         begin
            N.Unsubscribe (VA, Subscription);
            raise Program_Error with "closed unsubscribe accepted";
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         C2.Close (VA);
         Reject_Open (VA);
         Verify (Q.Status (Earlier) = Q.Operational and Value (Index, 1) = 1);
      end;
   end;
   --  Subscription finalization before VA: local-only, automatic removal later.
   declare
      Parent  : C2.C2_MEL := C2.Open (Provider, "c2:va-notify-copy");
      Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, Config);
   begin
      Verify (C2.Outcome (C2.Wait (Request, 3_000)) = C2.Created);
      declare
         VA    : C2.Virtual_Aperture := C2.Claim (Request);
         Index : constant Interfaces.C.unsigned := Last.all;
      begin
         C2.Close (Request);
         declare
            S : constant N.Subscription := N.Open (VA);
         begin
            Verify (N.Is_Open (S));
         end;
         Verify (Value (Index, 1) = 0);
         Reject_Open (VA);
         C2.Close (Parent);
         C2.Close (VA);
         Verify (Value (Index, 1) = 1 and Value (Index, 7) = 0);
      end;
   end;
   --  VA scope ends first: returned observer retains no provider graph.
   declare
      Index : Interfaces.C.unsigned := 0;
      function Observer return N.Subscription is
         Parent  : constant C2.C2_MEL := C2.Open (Provider, "c2:va-notify-sync");
         Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, Config);
      begin
         Verify (C2.Outcome (C2.Wait (Request, 3_000)) = C2.Created);
         declare
            VA : C2.Virtual_Aperture := C2.Claim (Request);
         begin
            Index := Last.all;
            C2.Close (Request);
            return N.Open (VA);
         end;
      end Observer;
      S     : constant N.Subscription := Observer;
   begin
      Verify
        (N.Is_Open (S)
         and N.Statistics (S).Stopped
         and not N.Statistics (S).Pending
         and Value (Index, 7) = 0);
      Stopped (S);
      Verify (Emit (Index, 1, 1) = 1 and N.Statistics (S).Callbacks_After_Stop = 1);
   end;
   --  A different VA with the SAME provider key must not stop either owner.
   declare
      Parent : constant C2.C2_MEL := C2.Open (Provider, "c2:va-notify-zero");
      A      : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, Config);
      B      : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, Config);
   begin
      Verify
        (C2.Outcome (C2.Wait (A, 3_000)) = C2.Created
         and C2.Outcome (C2.Wait (B, 3_000)) = C2.Created);
      declare
         VA    : C2.Virtual_Aperture := C2.Claim (A);
         Wrong : C2.Virtual_Aperture := C2.Claim (B);
         S     : N.Subscription := N.Open (VA);
         Other : N.Subscription := N.Open (Wrong);
      begin
         C2.Close (A);
         C2.Close (B);
         begin
            N.Unsubscribe (Wrong, S);
            raise Program_Error with "wrong-owner unsubscribe accepted";
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Verify (not N.Statistics (S).Stopped and not N.Statistics (Other).Stopped);
         N.Unsubscribe (VA, S);
         N.Unsubscribe (Wrong, Other);
      end;
   end;
   --  Preparation failure leaves the attempt available; exposed throw does not.
   for Exposed in Boolean loop
      declare
         Parent  : constant C2.C2_MEL :=
           C2.Open
             (Provider,
              (if Exposed
               then "c2:va-notify-reference-sync-stored-throw"
               else "c2:va-notify-copy"));
         Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, Config);
      begin
         Verify (C2.Outcome (C2.Wait (Request, 3_000)) = C2.Created);
         declare
            VA    : C2.Virtual_Aperture := C2.Claim (Request);
            Index : constant Interfaces.C.unsigned := Last.all;
         begin
            C2.Close (Request);
            if not Exposed then
               Ada.Environment_Variables.Set
                 ("AMS_MEL_TEST_RF_VA_FAILURE", "subscription-callable");
            end if;
            Reject_Open (VA);
            if not Exposed then
               Ada.Environment_Variables.Clear ("AMS_MEL_TEST_RF_VA_FAILURE");
               Verify (Value (Index, 0) = 0);
               declare
                  S : N.Subscription := N.Open (VA);
               begin
                  N.Unsubscribe (VA, S);
               end;
            else
               Verify (Value (Index, 0) = 1);
               Reject_Open (VA);
               Verify (Q.Query_Status (VA) = Q.Operational);
            end if;
            C2.Close (VA);
            Verify (Value (Index, 1) = (if Exposed then 0 else 1));
         end;
      end;
   end loop;
   --  Fixed mutation diagnostics, cached allocation/standard/unknown outcomes.
   for Case_Index in 1 .. 3 loop
      declare
         Parent  : constant C2.C2_MEL := C2.Open (Provider, "c2:va-notify-copy");
         Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, Config);
      begin
         Verify (C2.Outcome (C2.Wait (Request, 3_000)) = C2.Created);
         declare
            VA    : C2.Virtual_Aperture := C2.Claim (Request);
            S     : N.Subscription := N.Open (VA);
            Index : constant Interfaces.C.unsigned := Last.all;
         begin
            C2.Close (Request);
            Ada.Environment_Variables.Set
              ("AMS_MEL_TEST_VA_REMOVE_EXCEPTION",
               (case Case_Index is
                  when 1      => "long",
                  when 2      => "allocation",
                  when others => "unknown"));
            for Attempt in 1 .. 2 loop
               begin
                  N.Unsubscribe (VA, S);
                  raise Program_Error with "removal failure missing";
               exception
                  when E : AMS.MEL.Provider_Error =>
                     Verify (Ada.Exceptions.Exception_Message (E)'Length > 0);
                     if Case_Index = 1 then
                        Verify (Ada.Exceptions.Exception_Message (E)'Length <= 511);
                     end if;
               end;
               Verify (Value (Index, 1) = 1 and N.Statistics (S).Stopped);
            end loop;
            Ada.Environment_Variables.Clear ("AMS_MEL_TEST_VA_REMOVE_EXCEPTION");
            begin
               C2.Close (VA);
               raise Program_Error with "cached VA Close failure missing";
            exception
               when AMS.MEL.Provider_Error =>
                  null;
            end;
            Verify (not C2.Is_Open (VA) and Value (Index, 1) = 1);
            Stopped (S);
         end;
      end;
   end loop;
   Ada.Text_IO.Put_Line
     ("PASS: safe Ada subscribe-first VA notification flow, local finalization and stopped observers");
end AMS_MEL_RF_VA_Notifications;
