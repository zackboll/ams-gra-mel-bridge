with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Track;
with AMS.MEL.IR.Track.Metadata;
with Example_Support;
with Interfaces;

procedure Track_Metadata is
   package IR renames AMS.MEL.IR;
   package Track renames AMS.MEL.IR.Track;
   package Metadata renames AMS.MEL.IR.Track.Metadata;
   use Example_Support;
   use type Metadata.Metadata_Kind;
   use type Interfaces.Unsigned_32;
begin
   declare
      Zero       : constant IR.UUID := [others => 0];
      Channel_ID : constant IR.UCI_ID := IR.Create_UCI_ID (Zero, "example metadata");
      Platform   : constant IR.UCI_ID := IR.Create_UCI_ID (Zero, "example platform");
      Location   : constant IR.Component_Location :=
        IR.Create_Component_Location (1.25, -2.5, 3.75, "station-1", "mock-aircraft");
      Config     : constant Track.Track_Config :=
        Track.Create_Config (Channel_ID, Platform, Location);
      Session    : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Configuration);
      Channel    : Track.Track_Channel := Track.Open (Session, Config);
      Consumer   : Metadata.Metadata_Channel := Metadata.Open (Channel, Queue_Capacity => 3);
   begin
      for Position in 1 .. 3 loop
         declare
            Event : constant Metadata.Metadata_Event := Metadata.Receive_Event (Consumer, 1_000);
         begin
            case Event.Kind is
               when Metadata.Request_System_Track_Data_Event =>
                  Require
                    (Position /= 2
                     and then Event.Request.Command_ID = 16#C1234567#
                     and then Event.Request.Track_ID = 16#E3456789#,
                     "mock request order/content");
                  Ada.Text_IO.Put_Line ("request: track=0xE3456789 command=0xC1234567");

               when Metadata.IRST_Track_Report_Event         =>
                  Require
                    (Position = 2
                     and then Event.Report.Activity_ID = 16#F1234567#
                     and then Event.Report.Range_M = 123_456.75,
                     "mock report order/content");
                  Ada.Text_IO.Put_Line ("report: activity=0xF1234567 range_m=123456.75");

               when others                                   =>
                  raise Program_Error with "unexpected fixture metadata kind";
            end case;
         end;
      end loop;
      Metadata.Close (Consumer);
      Track.Close (Channel);
      AMS.MEL.Close (Session);
   end;
exception
   when Error : others =>
      Report_Failure (Error);
end Track_Metadata;
