with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Track;
with Interfaces;

--  Task 029B1 covers only the Track channel ownership/lifecycle foundation.
--  No IRSTTrackReport callback, Track metadata package, or Track command
--  surface exists yet, so none is exercised here.

package body AMS_MEL_IR_Track_Tests is
   package Trk renames AMS.MEL.IR.Track;
   package V renames AMS.MEL.IR.Channel;
   use type V.Channel_Type;
   use type Interfaces.Unsigned_32;

   Zero       : constant AMS.MEL.IR.UUID := [others => 0];
   Channel_ID : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "IR track channel");
   Platform   : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "track platform");
   Location   : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (1.25, -2.5, 3.75, "station-1", "mock-aircraft");
   Config     : constant Trk.Track_Config := Trk.Create_Config (Channel_ID, Platform, Location);

   procedure Test_Open_Capabilities_Enable (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-capabilities");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
   begin
      if not Trk.Is_Open (Channel) then
         raise Program_Error with "Ada Track channel did not open";
      end if;
      --  Capabilities are valid while attached, before Enable.
      declare
         Attached : constant V.Channel_Capability := Trk.Capabilities (Channel);
      begin
         if V.Width (Attached) /= 1920
           or else V.Height (Attached) /= 1080
           or else V.Channel_Type_Count (Attached) /= 1
           or else V.Channel_Type_At (Attached, 1) /= V.IRST_Track
         then
            raise Program_Error with "Ada Track attached capability mismatch";
         end if;
      end;
      Trk.Enable (Channel);
      --  Enable is idempotent once enabled.
      Trk.Enable (Channel);
      declare
         Enabled : constant V.Channel_Capability := Trk.Capabilities (Channel);
      begin
         if V.Bit_Depth (Enabled) /= 12 or else V.Channel_Type_At (Enabled, 1) /= V.IRST_Track then
            raise Program_Error with "Ada Track enabled capability mismatch";
         end if;
      end;
      Trk.Close (Channel);
      if Trk.Is_Open (Channel) then
         raise Program_Error with "Ada Track channel remained open after Close";
      end if;
      AMS.MEL.Close (Parent);
   end Test_Open_Capabilities_Enable;

   --  The Track channel keeps the provider/session graph alive independently
   --  of the public Session owner.
   procedure Test_Parent_First_Close (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-parent-first");
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
   begin
      Trk.Enable (Channel);
      AMS.MEL.Close (Parent);
      if AMS.MEL.Is_Open (Parent) then
         raise Program_Error with "Ada Track parent session remained open";
      end if;
      declare
         Value : constant V.Channel_Capability := Trk.Capabilities (Channel);
      begin
         if V.Channel_Type_At (Value, 1) /= V.IRST_Track then
            raise Program_Error with "Ada Track capability after parent close mismatch";
         end if;
      end;
      Trk.Close (Channel);
   end Test_Parent_First_Close;

   --  Finalization closes an un-closed Track channel without raising.
   procedure Test_Finalization (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "track-finalization");
   begin
      declare
         Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
      begin
         Trk.Enable (Channel);
      end;
      AMS.MEL.Close (Parent);
   end Test_Finalization;

   procedure Test_Enable_Failure (Provider_Path : String; Scenario : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
   begin
      begin
         Trk.Enable (Channel);
         raise Program_Error with "Ada Track enable failure was accepted";
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      Trk.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Enable_Failure;

   procedure Test_Open_Failure (Provider_Path : String; Scenario : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
   begin
      begin
         declare
            Channel : Trk.Track_Channel := Trk.Open (Parent, Config);
         begin
            Trk.Close (Channel);
         end;
         raise Program_Error with "Ada Track open failure was accepted";
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      AMS.MEL.Close (Parent);
   end Test_Open_Failure;

   procedure Run (Provider_Path : String) is
   begin
      Test_Open_Capabilities_Enable (Provider_Path);
      Test_Parent_First_Close (Provider_Path);
      Test_Finalization (Provider_Path);
      Test_Enable_Failure (Provider_Path, "track-enable-fail");
      Test_Enable_Failure (Provider_Path, "track-enable-throw");
      Test_Open_Failure (Provider_Path, "track-attach-null");
      Test_Open_Failure (Provider_Path, "track-wrong-concrete");
      Test_Open_Failure (Provider_Path, "track-capability-wrong");
      Test_Open_Failure (Provider_Path, "track-capability-throw");
      Ada.Text_IO.Put_Line ("PASS: Ada IR Track channel foundation contract");
   end Run;
end AMS_MEL_IR_Track_Tests;
