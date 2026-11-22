with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Scheduling;
with AMS.MEL.IR.Scheduling.Metadata;
with Interfaces;

package body AMS_MEL_IR_Scheduling_Update_Tests is
   package IR renames AMS.MEL.IR;
   package Sch renames IR.Scheduling;
   package M renames Sch.Metadata;
   use type M.Metadata_Counters;
   use type M.Track_List_Receipt;

   Config : constant Sch.Scheduling_Config :=
     Sch.Create_Config
       (IR.Create_UCI_ID ([others => 0], "track list metadata"),
        IR.Create_UCI_ID ([others => 1], "platform"),
        IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock"));

   procedure Require (Condition : Boolean; Message : String) is
   begin
      if not Condition then
         raise Program_Error with "UpdateTrackListResponse: " & Message;
      end if;
   end Require;

   procedure Payload (Provider_Path : String) is
      type Values is array (M.Track_List_Receipt) of M.Update_Track_List_Response;
      Expected : constant Values :=
        [M.None             => (0, M.None),
         M.Rejected         => (Interfaces.Unsigned_32'Last, M.Rejected),
         M.Rejected_Expired => (16#8000_0001#, M.Rejected_Expired),
         M.Accepted         => (Interfaces.Unsigned_32'Last, M.Accepted)];
      Actual   : Values;
   begin
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-update-sync");
         Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
         Stream  : M.Update_Track_List_Stream := M.Open_Update_Track_List (Channel, 4);
      begin
         Require (M.Update_Track_List_Counters (Stream) = (4, 0, 0), "synchronous counters");
         Sch.Close (Channel);
         AMS.MEL.Close (Parent);
         for Receipt in Actual'Range loop
            Actual (Receipt) := M.Receive_Update_Track_List (Stream);
         end loop;
         begin
            Require (M.Receive_Update_Track_List (Stream).Response = M.None, "expected stop");
            raise Program_Error with "received after draining a stopped stream";
         exception
            when IR.Stream_Stopped =>
               null;
         end;
         M.Close (Stream);
         M.Close (Stream);
      end;
      Require (Actual = Expected, "both fields in exact callback order after all owners close");
   end Payload;

   procedure Malformed (Provider_Path : String) is
      Parent  : constant AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-update-malformed");
      Channel : Sch.Scheduling_Channel := Sch.Open (Parent, Config);
   begin
      Sch.Enable (Channel);
      declare
         Stream : constant M.Update_Track_List_Stream := M.Open_Update_Track_List (Channel, 1);
      begin
         Require (M.Update_Track_List_Counters (Stream) = (3, 0, 3), "malformed counters");
         Require (M.Receive_Update_Track_List (Stream, 2).Response = M.None, "expected timeout");
         raise Program_Error with "malformed payload published";
      exception
         when IR.Timeout_Error =>
            null;
      end;
   end Malformed;

   procedure Refusal (Provider_Path : String) is
      Parent  : constant AMS.MEL.Session :=
        AMS.MEL.Open (Provider_Path, "sched-update-not-supported");
      Channel : constant Sch.Scheduling_Channel := Sch.Open (Parent, Config);
   begin
      for Attempt in 1 .. 2 loop
         begin
            declare
               Stream : M.Update_Track_List_Stream := M.Open_Update_Track_List (Channel, 1);
            begin
               M.Close (Stream);
               raise Program_Error with "unexpected registration success";
            end;
         exception
            when M.Not_Supported =>
               Require (Attempt = 1, "NotSupported consumes the attempt");
            when AMS.MEL.Provider_Error =>
               Require (Attempt = 2, "expected NotSupported first");
         end;
      end loop;
   end Refusal;

   procedure Run (Provider_Path : String) is
   begin
      Payload (Provider_Path);
      Malformed (Provider_Path);
      Refusal (Provider_Path);
      Ada.Text_IO.Put_Line ("Scheduling UpdateTrackListResponse Ada tests passed");
   end Run;
end AMS_MEL_IR_Scheduling_Update_Tests;
