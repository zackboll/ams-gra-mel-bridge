with Ada.Unchecked_Conversion;
with Ada.Real_Time;
with AMS.MEL;
with AMS.MEL.RF;
with AMS.MEL.RF.Product_Rx;
with AMS.MEL.RF.Product_Rx.Testing;
with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;

package body AMS_MEL_RF_Product_Rx_Tests is
   package RF renames AMS.MEL.RF;
   package RX renames AMS.MEL.RF.Product_Rx;
   use type RX.Create_Outcome;
   use type RX.Create_Error_Code;
   use type RX.Counter;
   use type RF.Job_Data_Format;
   use type Interfaces.Integer_16;
   use type Interfaces.Integer_8;
   use type Interfaces.C.int;
   use type Interfaces.C.unsigned;
   use type Interfaces.C.size_t;
   use Ada.Real_Time;
   use type Interfaces.Integer_64;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Unsigned_8;
   use type System.Address;
   Config : constant RX.Config := RX.Create_Config (65_536, 4, 16);

   --  This integration-only import is looked up at runtime; the production
   --  library and safe RF packages do not link against the mock provider.
   function DL_Open
     (Path : Interfaces.C.Strings.chars_ptr; Flags : Interfaces.C.int) return System.Address;
   pragma Import (C, DL_Open, "dlopen");
   function DL_Sym
     (Handle : System.Address; Name : Interfaces.C.Strings.chars_ptr) return System.Address;
   pragma Import (C, DL_Sym, "dlsym");
   function DL_Close (Handle : System.Address) return Interfaces.C.int;
   pragma Import (C, DL_Close, "dlclose");
   type Emit_Access is
     access function
       (ID : Interfaces.Unsigned_64; Kind : Interfaces.C.int; Count : Interfaces.C.size_t)
        return Interfaces.C.int
   with Convention => C;
   function To_Emit is new Ada.Unchecked_Conversion (System.Address, Emit_Access);
   type Observer_From_Access is
     access function
       (Endpoint_Address : System.Address; Observer : access System.Address) return Interfaces.C.int
   with Convention => C;
   type Observer_Waiters_Access is
     access function
       (Observer : System.Address; Waiters : access Interfaces.C.size_t) return Interfaces.C.int
   with Convention => C;
   type Observer_Close_Access is access procedure (Observer : access System.Address)
   with Convention => C;
   function To_Observer_From is new Ada.Unchecked_Conversion (System.Address, Observer_From_Access);
   function To_Observer_Waiters is new
     Ada.Unchecked_Conversion (System.Address, Observer_Waiters_Access);
   function To_Observer_Close is new
     Ada.Unchecked_Conversion (System.Address, Observer_Close_Access);
   type Release_Access is access function return Interfaces.C.unsigned with Convention => C;
   function To_Release is new Ada.Unchecked_Conversion (System.Address, Release_Access);
   type Failpoint_Access is access procedure (Which : Interfaces.C.unsigned) with Convention => C;
   function To_Failpoint is new Ada.Unchecked_Conversion (System.Address, Failpoint_Access);

   procedure Check_Wrong_Formats (Value : RX.Event) is
      procedure Reject_Complex_INT16 (Samples : RX.Complex_I16_Array) is
      begin
         raise Program_Error with "wrong format touched samples" & Samples'Length'Image;
      end Reject_Complex_INT16;
      procedure Reject_Complex_INT8 (Samples : RX.Complex_I8_Array) is
      begin
         raise Program_Error with "wrong format touched samples" & Samples'Length'Image;
      end Reject_Complex_INT8;
      procedure Reject_Direct_INT8 (Samples : RX.Direct_I8_Array) is
      begin
         raise Program_Error with "wrong format touched samples" & Samples'Length'Image;
      end Reject_Direct_INT8;
      procedure Reject_Direct_INT16 (Samples : RX.Direct_I16_Array) is
      begin
         raise Program_Error with "wrong format touched samples" & Samples'Length'Image;
      end Reject_Direct_INT16;
   begin
      if RX.Data_Format (Value) /= RF.Complex_INT16 then
         begin
            RX.With_Samples (Value, Reject_Complex_INT16'Access);
            raise Program_Error with "wrong format accepted";
         exception
            when Constraint_Error =>
               null;
         end;
      end if;
      if RX.Data_Format (Value) /= RF.Complex_INT8 then
         begin
            RX.With_Samples_I8 (Value, Reject_Complex_INT8'Access);
            raise Program_Error with "wrong format accepted";
         exception
            when Constraint_Error =>
               null;
         end;
      end if;
      if RX.Data_Format (Value) /= RF.Direct_INT8 then
         begin
            RX.With_Direct_Samples_I8 (Value, Reject_Direct_INT8'Access);
            raise Program_Error with "wrong format accepted";
         exception
            when Constraint_Error =>
               null;
         end;
      end if;
      if RX.Data_Format (Value) /= RF.Direct_INT16 then
         begin
            RX.With_Direct_Samples_I16 (Value, Reject_Direct_INT16'Access);
            raise Program_Error with "wrong format accepted";
         exception
            when Constraint_Error =>
               null;
         end;
      end if;
      if RX.Data_Format (Value) /= RF.AMS_VITA_Small then
         begin
            declare
               Count : constant Natural := RX.VITA_Small_Packet_Count (Value);
            begin
               raise Program_Error with "wrong VITA format accepted" & Count'Image;
            end;
         exception
            when Constraint_Error =>
               null;
         end;
         begin
            declare
               Bytes : constant RX.Packet_Byte_Array := RX.Copy_VITA_Small_Packet (Value, 1);
            begin
               raise Program_Error with "wrong VITA copy accepted" & Bytes'Length'Image;
            end;
         exception
            when Constraint_Error =>
               null;
         end;
      end if;
      if RX.Data_Format (Value) /= RF.AMS_VITA_Small then
         begin
            declare
               Wrong : constant RX.Packet_Byte_Array := RX.Copy_VITA_Small_Packet (Value, 1);
            begin
               raise Program_Error with "wrong Small access accepted" & Wrong'Length'Image;
            end;
         exception
            when Constraint_Error =>
               null;
         end;
      end if;
      if RX.Data_Format (Value) /= RF.AMS_VITA_Medium then
         begin
            declare
               Wrong : constant RX.Packet_Byte_Array := RX.Copy_VITA_Medium_Packet (Value, 1);
            begin
               raise Program_Error with "wrong Medium access accepted" & Wrong'Length'Image;
            end;
         exception
            when Constraint_Error =>
               null;
         end;
      end if;
      if RX.Data_Format (Value) /= RF.AMS_VITA_Large then
         begin
            declare
               Wrong : constant RX.Packet_Byte_Array := RX.Copy_VITA_Large_Packet (Value, 1);
            begin
               raise Program_Error with "wrong Large access accepted" & Wrong'Length'Image;
            end;
         exception
            when Constraint_Error =>
               null;
         end;
      end if;
   end Check_Wrong_Formats;

   procedure Check_Rich (Value : RX.Event) is
      Info          : constant RX.Product_Metadata := RX.Metadata (Value);
      Alias_Address : constant System.Address := RX.Testing.Native_Sample_Address (Value);
      procedure Check_Samples (Samples : RX.Complex_I16_Array) is
      begin
         if Samples'Address /= Alias_Address
           or else Samples'Length /= 6
           or else Samples (0).Real /= 10
           or else Samples (0).Imag /= 20
           or else Samples (1).Real /= 30
           or else Samples (1).Imag /= 40
           or else Samples (2).Real /= -5
           or else Samples (2).Imag /= 6
           or else Samples (3).Real /= Interfaces.Integer_16'First
           or else Samples (3).Imag /= Interfaces.Integer_16'Last
           or else Samples (4).Real /= Interfaces.Integer_16'Last
           or else Samples (4).Imag /= Interfaces.Integer_16'First
         then
            raise Program_Error with "RF sample copy/alias mismatch";
         end if;
      end Check_Samples;
   begin
      if not RX.Is_Open (Value)
        or else RX.Data_Format (Value) /= RF.Complex_INT16
        or else RX.Sample_Count (Value) /= 6
        or else RX.MEL_Protocol_Version_ID (Info) /= 16#FEDC_BA98#
        or else RX.VA_Definition_ID (Info) /= 16#8000_0001#
        or else RX.VA_Instance_ID (Info) /= 16#7FFF_FFFE#
        or else RX.Job_Details_ID (Info) /= 16#DEAD_BEEF#
        or else RX.Job_Interval_ID (Info) /= 16#0001_0002#
        or else RX.LF_Type_ID (Info) /= 16#FFFF_FFFF#
        or else RX.LF_Instance_ID (Info) /= 16#1234_5678#
        or else not RX.Phase_Coherence_With_Prior (Info)
        or else RX.First_Rx_Event_Start_Seconds (Info) /= -4_102_444_801
        or else RX.First_Rx_Event_Start_Femtoseconds (Info) /= 987_654_321_098_765
        or else RX.Rx_Stream_ID_Count (Info) /= 4
        or else RX.Rx_Stream_ID_At (Info, 1) /= 16#FFFF_FFFF#
        or else RX.Rx_Stream_ID_At (Info, 2) /= 0
        or else RX.Rx_Stream_ID_At (Info, 3) /= 16#8000_0000#
        or else RX.Rx_Stream_ID_At (Info, 4) /= 42
      then
         raise Program_Error with "RF event metadata mismatch";
      end if;
      Check_Wrong_Formats (Value);
      RX.With_Samples (Value, Check_Samples'Access);
      declare
         Copy : constant RX.Complex_I16_Array := RX.Copy_Samples (Value);
      begin
         if Copy (1).Real /= 10 or else Copy (4).Imag /= Interfaces.Integer_16'Last then
            raise Program_Error with "RF explicit sample copy mismatch";
         end if;
      end;
   end Check_Rich;

   function Claim_After_Parent_Finalization (Provider_Path : String) return RX.Endpoint is
      Parent  : constant RF.Data_MEL := RF.Open (Provider_Path, "rx:ok");
      Request : constant RX.Create_Request := RX.Submit (Parent, Config);
   begin
      if RX.Outcome (RX.Wait (Request, 10_000)) /= RX.Created then
         raise Program_Error with "RF parent-finalization setup failed";
      end if;
      return RX.Claim (Request);
   --  Parent and Request finalize before the caller uses the endpoint.
   end Claim_After_Parent_Finalization;

   procedure Run (Provider_Path : String) is
      Path    : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Provider_Path);
      Symbol  : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_rx_emit");
      Library : constant System.Address := DL_Open (Path, 2);
      Emit    : constant Emit_Access := To_Emit (DL_Sym (Library, Symbol));
      Parent  : RF.Data_MEL := RF.Open (Provider_Path, "rx:ok");
      Request : RX.Create_Request := RX.Submit (Parent, Config);
      Ignored : Interfaces.C.int;
   begin
      RX.Testing.Check_Representation;
      Interfaces.C.Strings.Free (Path);
      Interfaces.C.Strings.Free (Symbol);
      if Library = System.Null_Address or else Emit = null then
         raise Program_Error with "cannot load mock RF emitter";
      end if;
      if not RX.Is_Open (Request)
        or else RX.Outcome (RX.Wait (Request, 10_000)) /= RX.Created
        or else RX.Outcome (RX.Wait (Request, 0)) /= RX.Created
      then
         raise Program_Error with "RF cached create result mismatch";
      end if;
      declare
         Endpoint : RX.Endpoint := RX.Claim (Request);
         ID       : constant Interfaces.Unsigned_64 := RX.Endpoint_ID (Endpoint);
      begin
         if not RX.Is_Open (Endpoint) or else RX.Assigned_Data_Format (Endpoint) /= RF.Complex_INT16
         then
            raise Program_Error with "RF claim mismatch";
         end if;
         begin
            declare
               Unexpected : constant RX.Event := RX.Receive (Endpoint);
            begin
               if RX.Is_Open (Unexpected) then
                  raise Program_Error with "empty RF queue returned an event";
               end if;
            end;
         exception
            when RX.Timeout_Error =>
               null;
         end;
         begin
            declare
               Other : constant RX.Endpoint := RX.Claim (Request);
            begin
               if RX.Is_Open (Other) then
                  raise Program_Error with "RF second claim succeeded";
               end if;
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         RF.Close (Parent);
         RX.Close (Request);
         if RF.Is_Open (Parent) or else RX.Is_Open (Request) then
            raise Program_Error with "RF parent-first closure failed";
         end if;
         if Emit (ID, 0, 0) /= 1 then
            raise Program_Error with "mock RF emit failed";
         end if;
         declare
            A      : RX.Event := RX.Receive (Endpoint);
            Copied : constant RX.Complex_I16_Array := RX.Copy_Samples (A);
         begin
            Check_Rich (A);
            declare
               procedure Unexpected_I8 (Samples : RX.Complex_I8_Array) is
               begin
                  raise Program_Error with "I16 exposed I8" & Samples'Length'Image;
               end Unexpected_I8;
            begin
               begin
                  RX.With_Samples_I8 (A, Unexpected_I8'Access);
                  raise Program_Error with "I16 wrong-width borrowed accessor accepted";
               exception
                  when Constraint_Error =>
                     null;
               end;
               begin
                  declare
                     Wrong : constant RX.Complex_I8_Array := RX.Copy_Samples_I8 (A);
                  begin
                     raise Program_Error with "I16 wrong-width copy accepted" & Wrong'Length'Image;
                  end;
               exception
                  when Constraint_Error =>
                     null;
               end;
            end;
            if Emit (ID, 12, 0) /= 1 or else Emit (ID, 5, 0) /= 1 then
               raise Program_Error with "mock RF buffer reuse failed";
            end if;
            Check_Rich (A);
            declare
               B     : RX.Event := RX.Receive (Endpoint);
               Empty : RX.Event := RX.Receive (Endpoint);
               procedure Check_Empty (Samples : RX.Complex_I16_Array) is
               begin
                  if Samples'Length /= 0 then
                     raise Program_Error with "nonempty RF zero event";
                  end if;
               end Check_Empty;
            begin
               if RX.Sample_Count (B) /= 3 or else RX.Sample_Count (Empty) /= 0 then
                  raise Program_Error with "RF event queue mismatch";
               end if;
               RX.With_Samples (Empty, Check_Empty'Access);
               if RX.Copy_Samples (Empty)'Length /= 0 then
                  raise Program_Error with "RF empty copy is not empty";
               end if;
               RX.Close (Empty);
               RX.Close (B);
            end;
            declare
               Stats : constant RX.Counters := RX.Statistics (Endpoint);
            begin
               if Stats.Callbacks_Received /= 3
                 or else Stats.Products_Queued /= 3
                 or else Stats.Products_Dropped_Queue_Full /= 0
                 or else Stats.Malformed_Or_Unsupported /= 0
                 or else Stats.Allocation_Failures /= 0
                 or else Stats.Callbacks_After_Close /= 0
               then
                  raise Program_Error with "RF counter conversion mismatch";
               end if;
            end;
            RX.Close (Endpoint);
            Check_Rich (A);
            RX.Close (A);
            if Copied (1).Real /= 10 or else Copied (4).Imag /= Interfaces.Integer_16'Last then
               raise Program_Error with "RF Ada sample copy did not outlive event";
            end if;
            if RX.Is_Open (A) then
               raise Program_Error with "RF event close failed";
            end if;
            declare
               procedure Unexpected (Samples : RX.Complex_I16_Array) is
               begin
                  raise Program_Error with "closed RF event borrowed samples";
               end Unexpected;
            begin
               begin
                  RX.With_Samples (A, Unexpected'Access);
               exception
                  when AMS.MEL.Provider_Error =>
                     null;
               end;
            end;
         end;
      end;
      declare
         Parent    : constant RF.Data_MEL := RF.Open (Provider_Path, "rx:ok");
         Request   : constant RX.Create_Request :=
           RX.Submit (Parent, RX.Create_Config (65_536, 1, 16));
         Name      : Interfaces.C.Strings.chars_ptr :=
           Interfaces.C.Strings.New_String ("ams_mel_test_rf_rx_failpoint");
         Program   : constant System.Address := DL_Open (Interfaces.C.Strings.Null_Ptr, 2);
         Failpoint : constant Failpoint_Access := To_Failpoint (DL_Sym (Program, Name));
      begin
         Interfaces.C.Strings.Free (Name);
         if RX.Outcome (RX.Wait (Request, 10_000)) /= RX.Created or else Failpoint = null then
            raise Program_Error with "RF counter test setup failed";
         end if;
         declare
            Endpoint : RX.Endpoint := RX.Claim (Request);
            ID       : constant Interfaces.Unsigned_64 := RX.Endpoint_ID (Endpoint);
         begin
            if Emit (ID, 0, 0) /= 1 or else Emit (ID, 12, 0) /= 1 or else Emit (ID, 1, 0) /= 1 then
               raise Program_Error with "RF counter test emit failed";
            end if;
            Failpoint (1);
            if Emit (ID, 4, 1) /= 1 then
               raise Program_Error with "RF allocation failpoint emit failed";
            end if;
            declare
               Counts : constant RX.Counters := RX.Statistics (Endpoint);
            begin
               if Counts.Callbacks_Received /= 4
                 or else Counts.Products_Queued /= 1
                 or else Counts.Products_Dropped_Queue_Full /= 1
                 or else Counts.Malformed_Or_Unsupported /= 1
                 or else Counts.Allocation_Failures /= 1
                 or else Counts.Callbacks_After_Close /= 0
               then
                  raise Program_Error with "RF six-counter conversion mismatch";
               end if;
            end;
            declare
               A : constant RX.Event := RX.Receive (Endpoint);
            begin
               Check_Rich (A);
            end;
            RX.Close (Endpoint);
         end;
         Ignored := DL_Close (Program);
      end;
      declare
         Delayed      : RF.Data_MEL := RF.Open (Provider_Path, "rx:delayed");
         Pending      : RX.Create_Request := RX.Submit (Delayed, Config);
         Release_Name : Interfaces.C.Strings.chars_ptr :=
           Interfaces.C.Strings.New_String ("mock_rf_rx_release");
         Release      : constant Release_Access := To_Release (DL_Sym (Library, Release_Name));
      begin
         Interfaces.C.Strings.Free (Release_Name);
         begin
            declare
               Unexpected : constant RX.Create_Result := RX.Wait (Pending, 0);
            begin
               if RX.Outcome (Unexpected) = RX.Created then
                  raise Program_Error with "pending RF creation completed";
               end if;
            end;
         exception
            when RX.Timeout_Error =>
               null;
         end;
         begin
            declare
               Unexpected : constant RX.Endpoint := RX.Claim (Pending);
            begin
               if RX.Is_Open (Unexpected) then
                  raise Program_Error with "pending RF claim succeeded";
               end if;
            end;
         exception
            when RX.Timeout_Error =>
               null;
         end;
         RF.Close (Delayed);
         if Release = null or else Release.all = 0 then
            raise Program_Error with "mock RF pending completion release failed";
         end if;
         if RX.Outcome (RX.Wait (Pending, 10_000)) /= RX.Created then
            raise Program_Error with "RF timeout cancelled pending request";
         end if;
         declare
            Claimed : RX.Endpoint := RX.Claim (Pending);
         begin
            RX.Close (Pending);
            RX.Close (Claimed);
         end;
      end;
      declare
         Endpoint : RX.Endpoint := Claim_After_Parent_Finalization (Provider_Path);
      begin
         if Emit (RX.Endpoint_ID (Endpoint), 0, 0) /= 1 then
            raise Program_Error with "RF endpoint lost finalized parent";
         end if;
         declare
            A : constant RX.Event := RX.Receive (Endpoint);
         begin
            Check_Rich (A);
         end;
         RX.Close (Endpoint);
      end;
      declare
         Data : constant RF.Data_MEL := RF.Open (Provider_Path, "rx:sync-throw");
      begin
         begin
            declare
               Request : constant RX.Create_Request := RX.Submit (Data, Config);
            begin
               if RX.Is_Open (Request) then
                  raise Program_Error with "RF synchronous create exception was ignored";
               end if;
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
      end;
      declare
         Failed  : constant RF.Data_MEL := RF.Open (Provider_Path, "rx:error-known");
         Pending : constant RX.Create_Request := RX.Submit (Failed, Config);
         Result  : constant RX.Create_Result := RX.Wait (Pending, 10_000);
      begin
         if RX.Outcome (Result) /= RX.Failed
           or else RX.Error_Code (Result) /= RX.Insufficient_Resources
           or else RX.Description (Result) /= "mock ProductRx resources exhausted"
         then
            raise Program_Error with "RF ErrorOr mapping mismatch";
         end if;
      end;
      declare
         Failed   : constant RF.Data_MEL := RF.Open (Provider_Path, "rx:error-long");
         Pending  : constant RX.Create_Request := RX.Submit (Failed, Config);
         Result   : constant RX.Create_Result := RX.Wait (Pending, 10_000);
         Expected : constant String :=
           String'(1 .. 600 => 'r')
           & Character'Val (16#E2#)
           & Character'Val (16#82#)
           & Character'Val (16#AC#)
           & "zzz";
      begin
         if RX.Outcome (Result) /= RX.Failed
           or else RX.Error_Code (Result) /= RX.Unsupported
           or else RX.Description (Result) /= Expected
           or else RX.Description (Result) /= RX.Description (RX.Wait (Pending, 0))
         then
            raise Program_Error with "RF long diagnostic retry mismatch";
         end if;
      end;
      declare
         Parent  : RF.Data_MEL := RF.Open (Provider_Path, "rx:ok");
         Request : RX.Create_Request := RX.Submit (Parent, Config);
      begin
         if RX.Outcome (RX.Wait (Request, 10_000)) /= RX.Created then
            raise Program_Error with "RF finalization setup failed";
         end if;
         declare
            Endpoint : constant RX.Endpoint := RX.Claim (Request);
         begin
            RF.Close (Parent);
            RX.Close (Request);
            if Emit (RX.Endpoint_ID (Endpoint), 0, 0) /= 1 then
               raise Program_Error with "RF finalization emit failed";
            end if;
            declare
               A : constant RX.Event := RX.Receive (Endpoint);
            begin
               Check_Rich (A);
            --  A and Endpoint finalize after the logically closed parent.
            end;
         end;
      end;
      declare
         Parent  : RF.Data_MEL := RF.Open (Provider_Path, "rx:ok:shutdown-throw");
         Request : RX.Create_Request := RX.Submit (Parent, Config);
      begin
         if RX.Outcome (RX.Wait (Request, 10_000)) /= RX.Created then
            raise Program_Error with "RF deferred shutdown test setup failed";
         end if;
         declare
            Endpoint : RX.Endpoint := RX.Claim (Request);
         begin
            RF.Close (Parent);
            RX.Close (Request);
            begin
               RX.Close (Endpoint);
               raise Program_Error with "RF deferred shutdown failure not reported";
            exception
               when AMS.MEL.Provider_Error =>
                  if RX.Is_Open (Endpoint) then
                     raise Program_Error with "RF failed endpoint close retained handle";
                  end if;
            end;
         end;
      end;
      declare
         Parent  : constant RF.Data_MEL := RF.Open (Provider_Path, "rx:ok");
         Request : constant RX.Create_Request := RX.Submit (Parent, Config);
      begin
         if RX.Outcome (RX.Wait (Request, 10_000)) /= RX.Created then
            raise Program_Error with "RF request finalization setup failed";
         end if;
      end;
      declare
         Parent           : constant RF.Data_MEL := RF.Open (Provider_Path, "rx:ok");
         Request          : constant RX.Create_Request := RX.Submit (Parent, Config);
         From_Name        : Interfaces.C.Strings.chars_ptr :=
           Interfaces.C.Strings.New_String ("ams_mel_test_rf_rx_observer_from");
         Waiters_Name     : Interfaces.C.Strings.chars_ptr :=
           Interfaces.C.Strings.New_String ("ams_mel_test_rf_rx_waiters");
         Close_Name       : Interfaces.C.Strings.chars_ptr :=
           Interfaces.C.Strings.New_String ("ams_mel_test_rf_rx_observer_close");
         Program          : constant System.Address := DL_Open (Interfaces.C.Strings.Null_Ptr, 2);
         From_Endpoint    : constant Observer_From_Access :=
           To_Observer_From (DL_Sym (Program, From_Name));
         Waiters          : constant Observer_Waiters_Access :=
           To_Observer_Waiters (DL_Sym (Program, Waiters_Name));
         Release_Observer : constant Observer_Close_Access :=
           To_Observer_Close (DL_Sym (Program, Close_Name));
      begin
         Interfaces.C.Strings.Free (From_Name);
         Interfaces.C.Strings.Free (Waiters_Name);
         Interfaces.C.Strings.Free (Close_Name);
         if From_Endpoint = null
           or else Waiters = null
           or else Release_Observer = null
           or else RX.Outcome (RX.Wait (Request, 10_000)) /= RX.Created
         then
            raise Program_Error with "RF stream-stopped test setup failed";
         end if;
         declare
            Endpoint : RX.Endpoint := RX.Claim (Request);
            Observer : aliased System.Address := System.Null_Address;
         begin
            if From_Endpoint (RX.Testing.Native_Endpoint_Address (Endpoint), Observer'Access) /= 1
              or else Observer = System.Null_Address
            then
               raise Program_Error with "RF observer did not attach";
            end if;
            declare
               task Receiver is
                  entry Finished;
               end Receiver;
               task body Receiver is
                  Outcome : Interfaces.C.int := 0;
               begin
                  begin
                     declare
                        Unexpected : constant RX.Event := RX.Receive (Endpoint, 10_000);
                     begin
                        if RX.Is_Open (Unexpected) then
                           Outcome := 2;
                        end if;
                     end;
                  exception
                     when RX.Stream_Stopped =>
                        Outcome := 1;
                     when others =>
                        Outcome := 3;
                  end;
                  accept Finished do
                     if Outcome /= 1 then
                        raise Program_Error with "RF receive did not map Stream_Stopped";
                     end if;
                  end Finished;
               end Receiver;
               Count    : aliased Interfaces.C.size_t := 0;
               Deadline : constant Ada.Real_Time.Time :=
                 Ada.Real_Time.Clock + Ada.Real_Time.Seconds (10);
            begin
               loop
                  if Waiters (Observer, Count'Access) /= 1 then
                     raise Program_Error with "RF waiter observation failed";
                  end if;
                  exit when Count = 1;
                  if Ada.Real_Time.Clock > Deadline then
                     raise Program_Error with "RF receive did not block";
                  end if;
               end loop;
               RX.Close (Endpoint);
               Receiver.Finished;
            end;
            Release_Observer (Observer'Access);
         end;
         Ignored := DL_Close (Program);
      end;
      for Iteration in 1 .. 50 loop
         declare
            Direct_Parent  : RF.Data_MEL := RF.Open (Provider_Path, "rx:ok:reference");
            Direct_Request : RX.Create_Request :=
              RX.Submit (Direct_Parent, RX.Create_Config_Direct_I8 (65_536, 4, 8));
            Expected       : constant RX.Direct_I8_Array := [-128, 127, -1, 0, 7, -11, 23, 42];
            procedure Check_Direct (Samples : RX.Direct_I8_Array) is
            begin
               if Samples'Length /= Expected'Length then
                  raise Program_Error with "Direct sample count mismatch";
               end if;
               for I in Expected'Range loop
                  if Samples (Samples'First + I - Expected'First) /= Expected (I) then
                     raise Program_Error with "Direct signed scalar fidelity mismatch";
                  end if;
               end loop;
            end Check_Direct;
         begin
            if RX.Outcome (RX.Wait (Direct_Request, 10_000)) /= RX.Created then
               raise Program_Error with "Direct creation failed";
            end if;
            declare
               Direct_Endpoint : RX.Endpoint := RX.Claim (Direct_Request);
            begin
               if RX.Assigned_Data_Format (Direct_Endpoint) /= RF.Direct_INT8 then
                  raise Program_Error with "Direct assigned format mismatch";
               end if;
               RX.Close (Direct_Request);
               RF.Close (Direct_Parent);
               if Emit (RX.Endpoint_ID (Direct_Endpoint), 0, 0) /= 1 then
                  raise Program_Error with "Direct emitter failed";
               end if;
               declare
                  Product : RX.Event := RX.Receive (Direct_Endpoint);
                  Copied  : constant RX.Direct_I8_Array := RX.Copy_Direct_Samples_I8 (Product);
               begin
                  if RX.Data_Format (Product) /= RF.Direct_INT8
                    or else RX.Sample_Count (Product) /= 8
                    or else RX.Job_Interval_ID (RX.Metadata (Product)) /= 19
                    or else RX.Rx_Stream_ID_Count (RX.Metadata (Product)) /= 3
                    or else RX.Rx_Stream_ID_At (RX.Metadata (Product), 1) /= 19
                    or else RX.Rx_Stream_ID_At (RX.Metadata (Product), 2) /= 0
                    or else RX.Rx_Stream_ID_At (RX.Metadata (Product), 3) /= 42
                  then
                     raise Program_Error with "Direct metadata mismatch";
                  end if;
                  Check_Wrong_Formats (Product);
                  RX.With_Direct_Samples_I8 (Product, Check_Direct'Access);
                  begin
                     declare
                        Wrong : constant RX.Complex_I16_Array := RX.Copy_Samples (Product);
                     begin
                        raise Program_Error
                          with "wrong-format accessor accepted" & Wrong'Length'Image;
                     end;
                  exception
                     when Constraint_Error =>
                        null;
                  end;
                  begin
                     declare
                        Wrong : constant RX.Complex_I8_Array := RX.Copy_Samples_I8 (Product);
                     begin
                        raise Program_Error
                          with "wrong-format accessor accepted" & Wrong'Length'Image;
                     end;
                  exception
                     when Constraint_Error =>
                        null;
                  end;
                  begin
                     declare
                        Wrong : constant RX.Direct_I16_Array :=
                          RX.Copy_Direct_Samples_I16 (Product);
                     begin
                        raise Program_Error
                          with "wrong-format accessor accepted" & Wrong'Length'Image;
                     end;
                  exception
                     when Constraint_Error =>
                        null;
                  end;
                  if Emit (RX.Endpoint_ID (Direct_Endpoint), 5, 0) /= 1 then
                     raise Program_Error with "Direct empty emitter failed";
                  end if;
                  declare
                     Empty      : RX.Event := RX.Receive (Direct_Endpoint);
                     Empty_Copy : constant RX.Direct_I8_Array := RX.Copy_Direct_Samples_I8 (Empty);
                  begin
                     if Empty_Copy'Length /= 0 then
                        raise Program_Error with "Direct empty count mismatch";
                     end if;
                     begin
                        declare
                           Wrong : constant RX.Complex_I16_Array := RX.Copy_Samples (Empty);
                        begin
                           raise Program_Error
                             with "empty Direct converted to complex" & Wrong'Length'Image;
                        end;
                     exception
                        when Constraint_Error =>
                           null;
                     end;
                     Check_Wrong_Formats (Empty);
                     RX.Close (Empty);
                  end;
                  RX.Close (Direct_Endpoint);
                  RX.With_Direct_Samples_I8 (Product, Check_Direct'Access);
                  RX.Close (Product);
                  Check_Direct (Copied);
               end;
            end;
         end;
      end loop;
      for Iteration in 1 .. 50 loop
         declare
            Direct_Parent  : RF.Data_MEL := RF.Open (Provider_Path, "rx:ok:reference");
            Direct_Request : RX.Create_Request :=
              RX.Submit (Direct_Parent, RX.Create_Config_Direct_I16 (65_536, 4, 8));
            Expected       : constant RX.Direct_I16_Array :=
              [-32768, 32767, -1, 0, 12345, -23456, 17, -19];
            procedure Check_Direct (Samples : RX.Direct_I16_Array) is
            begin
               if Samples'Length /= Expected'Length then
                  raise Program_Error with "Direct sample count mismatch";
               end if;
               for I in Expected'Range loop
                  if Samples (Samples'First + I - Expected'First) /= Expected (I) then
                     raise Program_Error with "Direct signed scalar fidelity mismatch";
                  end if;
               end loop;
            end Check_Direct;
         begin
            if RX.Outcome (RX.Wait (Direct_Request, 10_000)) /= RX.Created then
               raise Program_Error with "Direct creation failed";
            end if;
            declare
               Direct_Endpoint : RX.Endpoint := RX.Claim (Direct_Request);
            begin
               if RX.Assigned_Data_Format (Direct_Endpoint) /= RF.Direct_INT16 then
                  raise Program_Error with "Direct assigned format mismatch";
               end if;
               RX.Close (Direct_Request);
               RF.Close (Direct_Parent);
               if Emit (RX.Endpoint_ID (Direct_Endpoint), 0, 0) /= 1 then
                  raise Program_Error with "Direct emitter failed";
               end if;
               declare
                  Product : RX.Event := RX.Receive (Direct_Endpoint);
                  Copied  : constant RX.Direct_I16_Array := RX.Copy_Direct_Samples_I16 (Product);
               begin
                  if RX.Data_Format (Product) /= RF.Direct_INT16
                    or else RX.Sample_Count (Product) /= 8
                    or else RX.Job_Interval_ID (RX.Metadata (Product)) /= 20
                    or else RX.Rx_Stream_ID_Count (RX.Metadata (Product)) /= 3
                    or else RX.Rx_Stream_ID_At (RX.Metadata (Product), 1) /= 20
                    or else RX.Rx_Stream_ID_At (RX.Metadata (Product), 2) /= 0
                    or else RX.Rx_Stream_ID_At (RX.Metadata (Product), 3) /= 42
                  then
                     raise Program_Error with "Direct metadata mismatch";
                  end if;
                  Check_Wrong_Formats (Product);
                  RX.With_Direct_Samples_I16 (Product, Check_Direct'Access);
                  begin
                     declare
                        Wrong : constant RX.Complex_I16_Array := RX.Copy_Samples (Product);
                     begin
                        raise Program_Error
                          with "wrong-format accessor accepted" & Wrong'Length'Image;
                     end;
                  exception
                     when Constraint_Error =>
                        null;
                  end;
                  begin
                     declare
                        Wrong : constant RX.Complex_I8_Array := RX.Copy_Samples_I8 (Product);
                     begin
                        raise Program_Error
                          with "wrong-format accessor accepted" & Wrong'Length'Image;
                     end;
                  exception
                     when Constraint_Error =>
                        null;
                  end;
                  begin
                     declare
                        Wrong : constant RX.Direct_I8_Array := RX.Copy_Direct_Samples_I8 (Product);
                     begin
                        raise Program_Error
                          with "wrong-format accessor accepted" & Wrong'Length'Image;
                     end;
                  exception
                     when Constraint_Error =>
                        null;
                  end;
                  if Emit (RX.Endpoint_ID (Direct_Endpoint), 5, 0) /= 1 then
                     raise Program_Error with "Direct empty emitter failed";
                  end if;
                  declare
                     Empty      : RX.Event := RX.Receive (Direct_Endpoint);
                     Empty_Copy : constant RX.Direct_I16_Array :=
                       RX.Copy_Direct_Samples_I16 (Empty);
                  begin
                     if Empty_Copy'Length /= 0 then
                        raise Program_Error with "Direct empty count mismatch";
                     end if;
                     begin
                        declare
                           Wrong : constant RX.Complex_I16_Array := RX.Copy_Samples (Empty);
                        begin
                           raise Program_Error
                             with "empty Direct converted to complex" & Wrong'Length'Image;
                        end;
                     exception
                        when Constraint_Error =>
                           null;
                     end;
                     Check_Wrong_Formats (Empty);
                     RX.Close (Empty);
                  end;
                  RX.Close (Direct_Endpoint);
                  RX.With_Direct_Samples_I16 (Product, Check_Direct'Access);
                  RX.Close (Product);
                  Check_Direct (Copied);
               end;
            end;
         end;
      end loop;
      for Iteration in 1 .. 50 loop
         declare
            I8_Parent  : RF.Data_MEL := RF.Open (Provider_Path, "rx:ok:reference");
            I8_Request : RX.Create_Request :=
              RX.Submit (I8_Parent, RX.Create_Config_Complex_I8 (65_536, 4, 6));
         begin
            if RX.Outcome (RX.Wait (I8_Request, 10_000)) /= RX.Created then
               raise Program_Error with "I8 creation failed";
            end if;
            declare
               I8_Endpoint : RX.Endpoint := RX.Claim (I8_Request);
               procedure Check_I8 (Samples : RX.Complex_I8_Array) is
                  Expected : constant RX.Complex_I8_Array :=
                    [(-128, 127), (127, -128), (-1, 0), (0, -1), (7, -11), (23, 42)];
               begin
                  if Samples'Length /= Expected'Length then
                     raise Program_Error with "I8 sample count mismatch";
                  end if;
                  for Index in Expected'Range loop
                     if Samples (Samples'First + Index - Expected'First).Real
                       /= Expected (Index).Real
                       or else Samples (Samples'First + Index - Expected'First).Imag
                               /= Expected (Index).Imag
                     then
                        raise Program_Error with "I8 signed fidelity mismatch";
                     end if;
                  end loop;
               end Check_I8;
               procedure Unexpected_I16 (Samples : RX.Complex_I16_Array) is
               begin
                  raise Program_Error with "I8 widened to I16" & Samples'Length'Image;
               end Unexpected_I16;
            begin
               if RX.Assigned_Data_Format (I8_Endpoint) /= RF.Complex_INT8 then
                  raise Program_Error with "I8 assigned format mismatch";
               end if;
               RF.Close (I8_Parent);
               RX.Close (I8_Request);
               if Emit (RX.Endpoint_ID (I8_Endpoint), 0, 0) /= 1 then
                  raise Program_Error with "I8 emitter failed";
               end if;
               declare
                  Product : RX.Event := RX.Receive (I8_Endpoint);
                  Copied  : constant RX.Complex_I8_Array := RX.Copy_Samples_I8 (Product);
               begin
                  if RX.Data_Format (Product) /= RF.Complex_INT8
                    or else RX.Sample_Count (Product) /= 6
                    or else RX.Job_Interval_ID (RX.Metadata (Product)) /= 18
                    or else RX.Rx_Stream_ID_Count (RX.Metadata (Product)) /= 3
                  then
                     raise Program_Error with "I8 metadata mismatch";
                  end if;
                  RX.With_Samples_I8 (Product, Check_I8'Access);
                  begin
                     declare
                        Wrong : constant RX.Complex_I16_Array := RX.Copy_Samples (Product);
                     begin
                        raise Program_Error
                          with "I8 wrong-width copy accepted" & Wrong'Length'Image;
                     end;
                  exception
                     when Constraint_Error =>
                        null;
                  end;
                  begin
                     RX.With_Samples (Product, Unexpected_I16'Access);
                     raise Program_Error with "wrong-width I16 accessor accepted I8";
                  exception
                     when Constraint_Error =>
                        null;
                  end;
                  if Emit (RX.Endpoint_ID (I8_Endpoint), 5, 0) /= 1 then
                     raise Program_Error with "I8 empty emitter failed";
                  end if;
                  declare
                     Empty      : RX.Event := RX.Receive (I8_Endpoint);
                     Empty_Copy : constant RX.Complex_I8_Array := RX.Copy_Samples_I8 (Empty);
                  begin
                     if Empty_Copy'Length /= 0 then
                        raise Program_Error with "I8 empty copy mismatch";
                     end if;
                     begin
                        RX.With_Samples (Empty, Unexpected_I16'Access);
                        raise Program_Error with "empty I8 wrong-width accessor accepted";
                     exception
                        when Constraint_Error =>
                           null;
                     end;
                     Check_Wrong_Formats (Empty);
                     RX.Close (Empty);
                  end;
                  RX.Close (I8_Endpoint);
                  RX.With_Samples_I8 (Product, Check_I8'Access);
                  RX.Close (Product);
                  Check_I8 (Copied);
               end;
            end;
         end;
      end loop;
      for Iteration in 1 .. 50 loop
         declare
            VITA_Parent  : RF.Data_MEL := RF.Open (Provider_Path, "rx:ok:reference");
            VITA_Request : RX.Create_Request :=
              RX.Submit (VITA_Parent, RX.Create_Config_AMS_VITA_Small (65_536, 2, 3));
            procedure Check_Packet (Bytes : RX.Packet_Byte_Array; Payload_Words, Seed : Natural) is
               Words    : constant Natural := Payload_Words + 8;
               type Word_Array is array (Natural range <>) of Interfaces.Unsigned_32;
               Expected : Word_Array (0 .. Words - 1);
            begin
               if Bytes'Length /= Words * 4 then
                  raise Program_Error with "VITA serialized length mismatch";
               end if;
               Expected (0) :=
                 16#1CE0_0000#
                 + Interfaces.Unsigned_32 (Seed mod 16) * 65_536
                 + Interfaces.Unsigned_32 (Words);
               Expected (1) := 16#1234_5600# + Interfaces.Unsigned_32 (Seed);
               Expected (2) := 16#00AA_AAAA#;
               Expected (3) := 16#0500_0D05#;
               Expected (4) := 16#2345_6700# + Interfaces.Unsigned_32 (Seed);
               Expected (5) := 16#3456_7800# + Interfaces.Unsigned_32 (Seed);
               Expected (6) := 16#4567_8900# + Interfaces.Unsigned_32 (Seed);
               for I in 1 .. Payload_Words loop
                  Expected (6 + I) :=
                    16#0102_0304#
                    + Interfaces.Unsigned_32 (I - 1) * 16#070B_0D11#
                    + Interfaces.Unsigned_32 (Seed);
               end loop;
               Expected (Words - 1) := 16#00C0_0000# + Interfaces.Unsigned_32 (Seed mod 4) * 1_024;
               for I in Expected'Range loop
                  for J in 0 .. 3 loop
                     if Bytes (Bytes'First + I * 4 + J)
                       /= Interfaces.Unsigned_8
                            (Interfaces.Shift_Right (Expected (I), 24 - J * 8) and 255)
                     then
                        raise Program_Error with "VITA packet byte fidelity mismatch";
                     end if;
                  end loop;
               end loop;
            end Check_Packet;
         begin
            if RX.Outcome (RX.Wait (VITA_Request, 10_000)) /= RX.Created then
               raise Program_Error with "VITA create failed";
            end if;
            declare
               VITA_Endpoint : RX.Endpoint := RX.Claim (VITA_Request);
            begin
               RX.Close (VITA_Request);
               if RX.Assigned_Data_Format (VITA_Endpoint) /= RF.AMS_VITA_Small
                 or else Emit (RX.Endpoint_ID (VITA_Endpoint), 0, 0) /= 1
               then
                  raise Program_Error with "VITA assigned format/emitter mismatch";
               end if;
               declare
                  Product     : RX.Event := RX.Receive (VITA_Endpoint);
                  Short_Copy  : constant RX.Packet_Byte_Array :=
                    RX.Copy_VITA_Small_Packet (Product, 1);
                  Middle_Copy : constant RX.Packet_Byte_Array :=
                    RX.Copy_VITA_Small_Packet (Product, 2);
                  Full_Copy   : constant RX.Packet_Byte_Array :=
                    RX.Copy_VITA_Small_Packet (Product, 3);
                  procedure Check_Short (Bytes : RX.Packet_Byte_Array) is
                  begin
                     Check_Packet (Bytes, 0, 1);
                  end Check_Short;
               begin
                  if RX.Sample_Count (Product) /= 3
                    or else RX.VITA_Small_Packet_Count (Product) /= 3
                    or else RX.Job_Interval_ID (RX.Metadata (Product)) /= 21
                    or else RX.Rx_Stream_ID_At (RX.Metadata (Product), 1) /= 21
                  then
                     raise Program_Error with "VITA count/metadata mismatch";
                  end if;
                  Check_Wrong_Formats (Product);
                  declare
                     Zero : constant Natural := RX.VITA_Small_Packet_Count (Product) - 3;
                  begin
                     RX.With_VITA_Small_Packet (Product, Zero, Check_Short'Access);
                     raise Program_Error with "zero VITA index accepted";
                  exception
                     when Constraint_Error =>
                        null;
                  end;
                  RX.With_VITA_Small_Packet (Product, 1, Check_Short'Access);
                  begin
                     RX.With_VITA_Small_Packet (Product, 4, Check_Short'Access);
                     raise Program_Error with "out of range VITA index accepted";
                  exception
                     when Constraint_Error =>
                        null;
                  end;
                  if Emit (RX.Endpoint_ID (VITA_Endpoint), 5, 0) /= 1 then
                     raise Program_Error with "VITA empty emitter failed";
                  end if;
                  declare
                     Empty : RX.Event := RX.Receive (VITA_Endpoint);
                  begin
                     if RX.VITA_Small_Packet_Count (Empty) /= 0 then
                        raise Program_Error with "VITA empty count mismatch";
                     end if;
                     Check_Wrong_Formats (Empty);
                     begin
                        RX.With_VITA_Small_Packet (Empty, 1, Check_Short'Access);
                        raise Program_Error with "empty VITA index accepted";
                     exception
                        when Constraint_Error =>
                           null;
                     end;
                     RX.Close (Empty);
                  end;
                  RF.Close (VITA_Parent);
                  RX.Close (VITA_Endpoint);
                  RX.With_VITA_Small_Packet (Product, 1, Check_Short'Access);
                  Check_Packet (RX.Copy_VITA_Small_Packet (Product, 2), 9, 10);
                  Check_Packet (RX.Copy_VITA_Small_Packet (Product, 3), 120, 121);
                  RX.Close (Product);
                  Check_Packet (Short_Copy, 0, 1);
                  Check_Packet (Middle_Copy, 9, 10);
                  Check_Packet (Full_Copy, 120, 121);
               end;
            end;
         end;
      end loop;
      for Iteration in 1 .. 50 loop
         declare
            VITA_Parent  : RF.Data_MEL := RF.Open (Provider_Path, "rx:ok:reference");
            VITA_Request : RX.Create_Request :=
              RX.Submit (VITA_Parent, RX.Create_Config_AMS_VITA_Medium (65_536, 2, 3));
            procedure Check_Packet (Bytes : RX.Packet_Byte_Array; Payload_Words, Seed : Natural) is
               Words    : constant Natural := Payload_Words + 8;
               type Word_Array is array (Natural range <>) of Interfaces.Unsigned_32;
               Expected : Word_Array (0 .. Words - 1);
            begin
               if Bytes'Length /= Words * 4 then
                  raise Program_Error with "VITA serialized length mismatch";
               end if;
               Expected (0) :=
                 16#1CE0_0000#
                 + Interfaces.Unsigned_32 (Seed mod 16) * 65_536
                 + Interfaces.Unsigned_32 (Words);
               Expected (1) := 16#1234_5600# + Interfaces.Unsigned_32 (Seed);
               Expected (2) := 16#00AA_AAAA#;
               Expected (3) := 16#0500_0D05#;
               Expected (4) := 16#2345_6700# + Interfaces.Unsigned_32 (Seed);
               Expected (5) := 16#3456_7800# + Interfaces.Unsigned_32 (Seed);
               Expected (6) := 16#4567_8900# + Interfaces.Unsigned_32 (Seed);
               for I in 1 .. Payload_Words loop
                  Expected (6 + I) :=
                    16#0102_0304#
                    + Interfaces.Unsigned_32 (I - 1) * 16#070B_0D11#
                    + Interfaces.Unsigned_32 (Seed);
               end loop;
               Expected (Words - 1) := 16#00C0_0000# + Interfaces.Unsigned_32 (Seed mod 4) * 1_024;
               for I in Expected'Range loop
                  for J in 0 .. 3 loop
                     if Bytes (Bytes'First + I * 4 + J)
                       /= Interfaces.Unsigned_8
                            (Interfaces.Shift_Right (Expected (I), 24 - J * 8) and 255)
                     then
                        raise Program_Error with "VITA packet byte fidelity mismatch";
                     end if;
                  end loop;
               end loop;
            end Check_Packet;
         begin
            if RX.Outcome (RX.Wait (VITA_Request, 10_000)) /= RX.Created then
               raise Program_Error with "VITA create failed";
            end if;
            declare
               VITA_Endpoint : RX.Endpoint := RX.Claim (VITA_Request);
            begin
               RX.Close (VITA_Request);
               if RX.Assigned_Data_Format (VITA_Endpoint) /= RF.AMS_VITA_Medium
                 or else Emit (RX.Endpoint_ID (VITA_Endpoint), 0, 0) /= 1
               then
                  raise Program_Error with "VITA assigned format/emitter mismatch";
               end if;
               declare
                  Product     : RX.Event := RX.Receive (VITA_Endpoint);
                  Short_Copy  : constant RX.Packet_Byte_Array :=
                    RX.Copy_VITA_Medium_Packet (Product, 1);
                  Middle_Copy : constant RX.Packet_Byte_Array :=
                    RX.Copy_VITA_Medium_Packet (Product, 2);
                  Full_Copy   : constant RX.Packet_Byte_Array :=
                    RX.Copy_VITA_Medium_Packet (Product, 3);
                  procedure Check_Short (Bytes : RX.Packet_Byte_Array) is
                  begin
                     Check_Packet (Bytes, 0, 1);
                  end Check_Short;
               begin
                  if RX.Sample_Count (Product) /= 3
                    or else RX.VITA_Medium_Packet_Count (Product) /= 3
                    or else RX.Job_Interval_ID (RX.Metadata (Product)) /= 22
                    or else RX.Rx_Stream_ID_At (RX.Metadata (Product), 1) /= 22
                  then
                     raise Program_Error with "VITA count/metadata mismatch";
                  end if;
                  Check_Wrong_Formats (Product);
                  declare
                     Zero : constant Natural := RX.VITA_Medium_Packet_Count (Product) - 3;
                  begin
                     RX.With_VITA_Medium_Packet (Product, Zero, Check_Short'Access);
                     raise Program_Error with "zero VITA index accepted";
                  exception
                     when Constraint_Error =>
                        null;
                  end;
                  RX.With_VITA_Medium_Packet (Product, 1, Check_Short'Access);
                  begin
                     RX.With_VITA_Medium_Packet (Product, 4, Check_Short'Access);
                     raise Program_Error with "out of range VITA index accepted";
                  exception
                     when Constraint_Error =>
                        null;
                  end;
                  if Emit (RX.Endpoint_ID (VITA_Endpoint), 5, 0) /= 1 then
                     raise Program_Error with "VITA empty emitter failed";
                  end if;
                  declare
                     Empty : RX.Event := RX.Receive (VITA_Endpoint);
                  begin
                     if RX.VITA_Medium_Packet_Count (Empty) /= 0 then
                        raise Program_Error with "VITA empty count mismatch";
                     end if;
                     Check_Wrong_Formats (Empty);
                     begin
                        RX.With_VITA_Medium_Packet (Empty, 1, Check_Short'Access);
                        raise Program_Error with "empty VITA index accepted";
                     exception
                        when Constraint_Error =>
                           null;
                     end;
                     RX.Close (Empty);
                  end;
                  RF.Close (VITA_Parent);
                  RX.Close (VITA_Endpoint);
                  RX.With_VITA_Medium_Packet (Product, 1, Check_Short'Access);
                  Check_Packet (RX.Copy_VITA_Medium_Packet (Product, 2), 121, 122);
                  Check_Packet (RX.Copy_VITA_Medium_Packet (Product, 3), 248, 249);
                  RX.Close (Product);
                  Check_Packet (Short_Copy, 0, 1);
                  Check_Packet (Middle_Copy, 121, 122);
                  Check_Packet (Full_Copy, 248, 249);
               end;
            end;
         end;
      end loop;
      for Iteration in 1 .. 50 loop
         declare
            VITA_Parent  : RF.Data_MEL := RF.Open (Provider_Path, "rx:ok:reference");
            VITA_Request : RX.Create_Request :=
              RX.Submit (VITA_Parent, RX.Create_Config_AMS_VITA_Large (65_536, 2, 3));
            procedure Check_Packet (Bytes : RX.Packet_Byte_Array; Payload_Words, Seed : Natural) is
               Words    : constant Natural := Payload_Words + 8;
               type Word_Array is array (Natural range <>) of Interfaces.Unsigned_32;
               Expected : Word_Array (0 .. Words - 1);
            begin
               if Bytes'Length /= Words * 4 then
                  raise Program_Error with "VITA serialized length mismatch";
               end if;
               Expected (0) :=
                 16#1CE0_0000#
                 + Interfaces.Unsigned_32 (Seed mod 16) * 65_536
                 + Interfaces.Unsigned_32 (Words);
               Expected (1) := 16#1234_5600# + Interfaces.Unsigned_32 (Seed);
               Expected (2) := 16#00AA_AAAA#;
               Expected (3) := 16#0500_0D05#;
               Expected (4) := 16#2345_6700# + Interfaces.Unsigned_32 (Seed);
               Expected (5) := 16#3456_7800# + Interfaces.Unsigned_32 (Seed);
               Expected (6) := 16#4567_8900# + Interfaces.Unsigned_32 (Seed);
               for I in 1 .. Payload_Words loop
                  Expected (6 + I) :=
                    16#0102_0304#
                    + Interfaces.Unsigned_32 (I - 1) * 16#070B_0D11#
                    + Interfaces.Unsigned_32 (Seed);
               end loop;
               Expected (Words - 1) := 16#00C0_0000# + Interfaces.Unsigned_32 (Seed mod 4) * 1_024;
               for I in Expected'Range loop
                  for J in 0 .. 3 loop
                     if Bytes (Bytes'First + I * 4 + J)
                       /= Interfaces.Unsigned_8
                            (Interfaces.Shift_Right (Expected (I), 24 - J * 8) and 255)
                     then
                        raise Program_Error with "VITA packet byte fidelity mismatch";
                     end if;
                  end loop;
               end loop;
            end Check_Packet;
         begin
            if RX.Outcome (RX.Wait (VITA_Request, 10_000)) /= RX.Created then
               raise Program_Error with "VITA create failed";
            end if;
            declare
               VITA_Endpoint : RX.Endpoint := RX.Claim (VITA_Request);
            begin
               RX.Close (VITA_Request);
               if Emit (RX.Endpoint_ID (VITA_Endpoint), 4, 1) /= 1 then
                  raise Program_Error with "Large one-packet emitter failed";
               end if;
               declare
                  Single : RX.Event := RX.Receive (VITA_Endpoint);
               begin
                  if RX.Sample_Count (Single) /= 1 or else RX.VITA_Large_Packet_Count (Single) /= 1
                  then
                     raise Program_Error with "Large one-packet count mismatch";
                  end if;
                  Check_Packet (RX.Copy_VITA_Large_Packet (Single, 1), 0, 1);
                  Check_Wrong_Formats (Single);
                  RX.Close (Single);
               end;
               if RX.Assigned_Data_Format (VITA_Endpoint) /= RF.AMS_VITA_Large
                 or else Emit (RX.Endpoint_ID (VITA_Endpoint), 0, 0) /= 1
               then
                  raise Program_Error with "VITA assigned format/emitter mismatch";
               end if;
               declare
                  Product     : RX.Event := RX.Receive (VITA_Endpoint);
                  Short_Copy  : constant RX.Packet_Byte_Array :=
                    RX.Copy_VITA_Large_Packet (Product, 1);
                  Middle_Copy : constant RX.Packet_Byte_Array :=
                    RX.Copy_VITA_Large_Packet (Product, 2);
                  Full_Copy   : constant RX.Packet_Byte_Array :=
                    RX.Copy_VITA_Large_Packet (Product, 3);
                  procedure Check_Short (Bytes : RX.Packet_Byte_Array) is
                  begin
                     Check_Packet (Bytes, 0, 1);
                  end Check_Short;
               begin
                  if RX.Sample_Count (Product) /= 3
                    or else RX.VITA_Large_Packet_Count (Product) /= 3
                    or else RX.Job_Interval_ID (RX.Metadata (Product)) /= 23
                    or else RX.Rx_Stream_ID_At (RX.Metadata (Product), 1) /= 23
                  then
                     raise Program_Error with "VITA count/metadata mismatch";
                  end if;
                  Check_Wrong_Formats (Product);
                  declare
                     Zero : constant Natural := RX.VITA_Large_Packet_Count (Product) - 3;
                  begin
                     RX.With_VITA_Large_Packet (Product, Zero, Check_Short'Access);
                     raise Program_Error with "zero VITA index accepted";
                  exception
                     when Constraint_Error =>
                        null;
                  end;
                  RX.With_VITA_Large_Packet (Product, 1, Check_Short'Access);
                  begin
                     RX.With_VITA_Large_Packet (Product, 4, Check_Short'Access);
                     raise Program_Error with "out of range VITA index accepted";
                  exception
                     when Constraint_Error =>
                        null;
                  end;
                  if Emit (RX.Endpoint_ID (VITA_Endpoint), 5, 0) /= 1 then
                     raise Program_Error with "VITA empty emitter failed";
                  end if;
                  declare
                     Empty : RX.Event := RX.Receive (VITA_Endpoint);
                  begin
                     if RX.VITA_Large_Packet_Count (Empty) /= 0 then
                        raise Program_Error with "VITA empty count mismatch";
                     end if;
                     Check_Wrong_Formats (Empty);
                     begin
                        RX.With_VITA_Large_Packet (Empty, 1, Check_Short'Access);
                        raise Program_Error with "empty VITA index accepted";
                     exception
                        when Constraint_Error =>
                           null;
                     end;
                     RX.Close (Empty);
                  end;
                  RF.Close (VITA_Parent);
                  RX.Close (VITA_Endpoint);
                  RX.With_VITA_Large_Packet (Product, 1, Check_Short'Access);
                  Check_Packet (RX.Copy_VITA_Large_Packet (Product, 2), 249, 2);
                  Check_Packet (RX.Copy_VITA_Large_Packet (Product, 3), 2_040, 3);
                  RX.Close (Product);
                  Check_Packet (Short_Copy, 0, 1);
                  Check_Packet (Middle_Copy, 249, 2);
                  Check_Packet (Full_Copy, 2_040, 3);
               end;
            end;
         end;
      end loop;
      Ignored := DL_Close (Library);
   end Run;
end AMS_MEL_RF_Product_Rx_Tests;
