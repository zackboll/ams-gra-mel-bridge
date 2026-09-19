with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Instrumentation.Metadata;
with Interfaces;

package body AMS_MEL_IR_Instrumentation_Tests is
   package Instr renames AMS.MEL.IR.Instrumentation;
   package Meta renames AMS.MEL.IR.Instrumentation.Metadata;
   package V renames AMS.MEL.IR.Channel;
   use type Instr.Instrumentation_Outcome;
   use type Instr.Instrumentation_Error_Code;
   use type Instr.Priority;
   use type Instr.Instrumentation_Report;
   use type Interfaces.Unsigned_32;
   use type AMS.MEL.IR.Counter;
   use type V.Channel_Type;
   use type V.Metadata_Capability;

   Zero       : constant AMS.MEL.IR.UUID := [others => 0];
   Channel_ID : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Zero, "IR instrumentation channel");
   Platform   : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Zero, "instrumentation platform");
   Location   : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (1.25, -2.5, 3.75, "station-1", "mock-aircraft");
   Config     : constant Instr.Instrumentation_Config :=
     Instr.Create_Config (Channel_ID, Platform, Location);

   Rich_Command : constant Instr.Instrumentation_Level_Command :=
     (Command_ID => 16#E123_4567#, Priority => Instr.Debug);

   function Rich_Report_Matches (Value : Instr.Instrumentation_Report) return Boolean
   is (Value.Command_ID = 16#F123_4567#
       and then Value.Size = 16#89AB_CDEF#
       and then Value.Timestamp_NS = -8_765_432_109
       and then Value.Priority = Instr.Debug);

   procedure Test_Open_Enable_Capabilities (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "instr-capabilities");
      Channel : Instr.Instrumentation_Channel := Instr.Open (Parent, Config);
   begin
      if not Instr.Is_Open (Channel) then
         raise Program_Error with "Ada Instrumentation channel did not open";
      end if;
      --  Capabilities are valid while attached, before Enable.
      declare
         Attached : constant V.Channel_Capability := Instr.Capabilities (Channel);
      begin
         if V.Width (Attached) /= 1920
           or else V.Height (Attached) /= 1080
           or else V.Channel_Type_Count (Attached) /= 1
           or else V.Channel_Type_At (Attached, 1) /= V.Instrumentation
         then
            raise Program_Error with "Ada Instrumentation attached capability mismatch";
         end if;
         if not V.Has_Metadata_Capability (Attached, V.Instrumentation_Report) then
            raise Program_Error with "Ada Instrumentation metadata capability missing";
         end if;
      end;
      Instr.Enable (Channel);
      Instr.Enable (Channel);
      declare
         Enabled : constant V.Channel_Capability := Instr.Capabilities (Channel);
      begin
         if V.Bit_Depth (Enabled) /= 12 then
            raise Program_Error with "Ada Instrumentation enabled capability mismatch";
         end if;
      end;
      Instr.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Open_Enable_Capabilities;

   procedure Test_Rich_Submit (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "instr-failpoint");
      Channel : Instr.Instrumentation_Channel := Instr.Open (Parent, Config);
   begin
      Instr.Enable (Channel);
      declare
         Request : Instr.Instrumentation_Request := Instr.Submit (Channel, Rich_Command);
         First   : constant Instr.Instrumentation_Result := Instr.Wait (Request, 1_000);
         Second  : constant Instr.Instrumentation_Result := Instr.Wait (Request, 0);
      begin
         if not Instr.Is_Open (Request)
           or else Instr.Status (First) /= Instr.Success
           or else Instr.Status (Second) /= Instr.Success
           or else not Rich_Report_Matches (Instr.Report (First))
           or else not Rich_Report_Matches (Instr.Report (Second))
         then
            raise Program_Error with "Ada Instrumentation rich submit/fidelity failed";
         end if;
         Instr.Close (Request);
      end;
      Instr.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Rich_Submit;

   procedure Test_Submit_Before_Enable (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "instr-failpoint");
      Channel : Instr.Instrumentation_Channel := Instr.Open (Parent, Config);
   begin
      begin
         declare
            Ignored : Instr.Instrumentation_Request := Instr.Submit (Channel, Rich_Command);
         begin
            Instr.Close (Ignored);
         end;
         raise Program_Error with "Ada Instrumentation submit before Enable was accepted";
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      Instr.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Submit_Before_Enable;

   procedure Test_Enable_Failure (Provider_Path : String; Scenario : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : Instr.Instrumentation_Channel := Instr.Open (Parent, Config);
   begin
      begin
         Instr.Enable (Channel);
         raise Program_Error with "Ada Instrumentation enable failure was not reported";
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      Instr.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Enable_Failure;

   procedure Test_Rejection (Provider_Path : String) is
      Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "instr-reject");
      Channel  : Instr.Instrumentation_Channel := Instr.Open (Parent, Config);
      Expected : constant String :=
        [1 .. 510 => 'x']
        & Character'Val (16#E2#)
        & Character'Val (16#82#)
        & Character'Val (16#AC#)
        & [1 .. 100 => 'y'];
   begin
      Instr.Enable (Channel);
      declare
         Request : Instr.Instrumentation_Request := Instr.Submit (Channel, Rich_Command);
         Result  : constant Instr.Instrumentation_Result := Instr.Wait (Request, 1_000);
      begin
         --  The complete long UTF-8 rejection text is recovered from the
         --  cached terminal request.
         if Instr.Status (Result) /= Instr.Rejected
           or else Instr.Rejection_Code (Result) /= Instr.Invalid_State
           or else Instr.Description (Result) /= Expected
         then
            raise Program_Error with "Ada Instrumentation long rejection recovery failed";
         end if;
         Instr.Close (Request);
      end;
      Instr.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Rejection;

   procedure Test_Unknown_Outcome (Provider_Path : String; Scenario : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : Instr.Instrumentation_Channel := Instr.Open (Parent, Config);
   begin
      Instr.Enable (Channel);
      declare
         Request : Instr.Instrumentation_Request := Instr.Submit (Channel, Rich_Command);
      begin
         begin
            declare
               Ignored : constant Instr.Instrumentation_Result := Instr.Wait (Request, 1_000);
            begin
               null;
            end;
            raise Program_Error with "Ada Instrumentation invalid enum was not rejected";
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Instr.Close (Request);
      end;
      Instr.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Unknown_Outcome;

   procedure Test_Timeout_Then_Cached (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "instr-lifetime");
      Channel : Instr.Instrumentation_Channel := Instr.Open (Parent, Config);
   begin
      Instr.Enable (Channel);
      declare
         Request : Instr.Instrumentation_Request := Instr.Submit (Channel, Rich_Command);
      begin
         begin
            declare
               Ignored : constant Instr.Instrumentation_Result := Instr.Wait (Request, 0);
            begin
               null;
            end;
            raise Program_Error with "Ada Instrumentation Wait(0) did not time out";
         exception
            when AMS.MEL.IR.Timeout_Error =>
               null;
         end;
         declare
            First  : constant Instr.Instrumentation_Result := Instr.Wait (Request, 2_000);
            Second : constant Instr.Instrumentation_Result := Instr.Wait (Request, 0);
         begin
            if Instr.Status (First) /= Instr.Success
              or else not Rich_Report_Matches (Instr.Report (First))
              or else Instr.Report (First) /= Instr.Report (Second)
            then
               raise Program_Error with "Ada Instrumentation cached Wait mismatch";
            end if;
         end;
         Instr.Close (Request);
      end;
      Instr.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Timeout_Then_Cached;

   procedure Test_Parent_First_Close (Provider_Path : String) is
      --  Session and channel close while a request is still pending. Close is
      --  not cancellation; provider ownership persists until completion.
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "instr-lifetime");
      Channel : Instr.Instrumentation_Channel := Instr.Open (Parent, Config);
   begin
      Instr.Enable (Channel);
      declare
         Stream  : Meta.Metadata_Channel := Meta.Open (Channel, 8);
         Request : Instr.Instrumentation_Request := Instr.Submit (Channel, Rich_Command);
      begin
         Meta.Close (Stream);
         AMS.MEL.Close (Parent);
         Instr.Close (Channel);
         declare
            Result : constant Instr.Instrumentation_Result := Instr.Wait (Request, 2_000);
            Again  : constant Instr.Instrumentation_Result := Instr.Wait (Request, 0);
         begin
            if Instr.Status (Result) /= Instr.Success
              or else not Rich_Report_Matches (Instr.Report (Result))
              or else Instr.Status (Again) /= Instr.Success
            then
               raise Program_Error with "Ada Instrumentation parent-first close failed";
            end if;
         end;
         Instr.Close (Request);
      end;
   end Test_Parent_First_Close;

   procedure Test_Metadata_Synchronous_Events (Provider_Path : String) is
      --  The mock emits one report synchronously inside registration and one
      --  synchronously inside send. Neither may deadlock.
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "instr-rich");
      Channel : Instr.Instrumentation_Channel := Instr.Open (Parent, Config);
   begin
      Instr.Enable (Channel);
      declare
         Stream : Meta.Metadata_Channel := Meta.Open (Channel, 8);
      begin
         if not Meta.Is_Open (Stream) then
            raise Program_Error with "Ada Instrumentation metadata channel did not open";
         end if;
         declare
            Registration : constant Instr.Instrumentation_Report := Meta.Receive (Stream, 1_000);
         begin
            if not Rich_Report_Matches (Registration) then
               raise Program_Error with "Ada Instrumentation registration callback fidelity failed";
            end if;
         end;
         declare
            Request : Instr.Instrumentation_Request := Instr.Submit (Channel, Rich_Command);
            Result  : constant Instr.Instrumentation_Result := Instr.Wait (Request, 1_000);
            Emitted : constant Instr.Instrumentation_Report := Meta.Receive (Stream, 1_000);
         begin
            if Instr.Status (Result) /= Instr.Success
              or else not Rich_Report_Matches (Instr.Report (Result))
              or else not Rich_Report_Matches (Emitted)
            then
               raise Program_Error with "Ada Instrumentation send callback fidelity failed";
            end if;
            Instr.Close (Request);
         end;
         declare
            Values : constant Meta.Metadata_Counters := Meta.Counters (Stream);
         begin
            if Values.Events_Received /= 2
              or else Values.Events_Dropped_Queue_Full /= 0
              or else Values.Malformed_Or_Unsupported /= 0
            then
               raise Program_Error with "Ada Instrumentation metadata counters mismatch";
            end if;
         end;
         --  Metadata Close only deactivates public consumption; the provider
         --  channel is still alive and the callback state still belongs to it.
         Meta.Close (Stream);
         if Meta.Is_Open (Stream) then
            raise Program_Error with "Ada Instrumentation metadata close did not clear the owner";
         end if;
      end;
      Instr.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Metadata_Synchronous_Events;

   procedure Test_Metadata_Overflow (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "instr-overflow");
      Channel : Instr.Instrumentation_Channel := Instr.Open (Parent, Config);
   begin
      Instr.Enable (Channel);
      declare
         Stream : Meta.Metadata_Channel := Meta.Open (Channel, 2);
         Values : constant Meta.Metadata_Counters := Meta.Counters (Stream);
         First  : constant Instr.Instrumentation_Report := Meta.Receive (Stream, 0);
         Second : constant Instr.Instrumentation_Report := Meta.Receive (Stream, 0);
      begin
         --  Six synchronous events into a capacity-2 DROP-INCOMING queue keep
         --  the first two in FIFO order and count the rest as dropped.
         if Values.Events_Received /= 6
           or else Values.Events_Dropped_Queue_Full /= 4
           or else Values.Malformed_Or_Unsupported /= 0
           or else First.Command_ID /= 0
           or else Second.Command_ID /= 1
         then
            raise Program_Error with "Ada Instrumentation metadata overflow semantics failed";
         end if;
         begin
            declare
               Ignored : constant Instr.Instrumentation_Report := Meta.Receive (Stream, 0);
            begin
               null;
            end;
            raise Program_Error with "Ada Instrumentation drained queue did not time out";
         exception
            when AMS.MEL.IR.Timeout_Error =>
               null;
         end;
         Meta.Close (Stream);
      end;
      Instr.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Metadata_Overflow;

   procedure Test_Metadata_Malformed (Provider_Path : String; Scenario : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : Instr.Instrumentation_Channel := Instr.Open (Parent, Config);
   begin
      Instr.Enable (Channel);
      declare
         Stream : Meta.Metadata_Channel := Meta.Open (Channel, 4);
         Values : constant Meta.Metadata_Counters := Meta.Counters (Stream);
      begin
         if Values.Events_Received /= 1 or else Values.Malformed_Or_Unsupported /= 1 then
            raise Program_Error with "Ada Instrumentation malformed callback accounting failed";
         end if;
         Meta.Close (Stream);
      end;
      Instr.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Metadata_Malformed;

   procedure Run (Provider_Path : String) is
   begin
      Test_Open_Enable_Capabilities (Provider_Path);
      Test_Rich_Submit (Provider_Path);
      Test_Submit_Before_Enable (Provider_Path);
      Test_Enable_Failure (Provider_Path, "instr-enable-fail");
      Test_Enable_Failure (Provider_Path, "instr-enable-throw");
      Test_Rejection (Provider_Path);
      Test_Unknown_Outcome (Provider_Path, "instr-unknown-error");
      Test_Unknown_Outcome (Provider_Path, "instr-invalid-priority");
      Test_Unknown_Outcome (Provider_Path, "instr-null");
      Test_Unknown_Outcome (Provider_Path, "instr-future-throw");
      Test_Timeout_Then_Cached (Provider_Path);
      Test_Parent_First_Close (Provider_Path);
      Test_Metadata_Synchronous_Events (Provider_Path);
      Test_Metadata_Overflow (Provider_Path);
      Test_Metadata_Malformed (Provider_Path, "instr-callback-null");
      Test_Metadata_Malformed (Provider_Path, "instr-callback-invalid-priority");
      Ada.Text_IO.Put_Line ("PASS: Ada IR Instrumentation channel contract");
   end Run;
end AMS_MEL_IR_Instrumentation_Tests;
