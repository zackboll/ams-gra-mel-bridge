with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Health_Status;
with AMS.MEL.IR.Stacked_Image;
with AMS.MEL.IR.Image;
with AMS.MEL.IR.Instrumentation;
with AMS.MEL.IR.Track;
with AMS.MEL.IR.Scheduling;
with Ada.Text_IO;

package body AMS_MEL_IR_Disable_Tests is
   package IR renames AMS.MEL.IR;
   package V renames IR.Channel;
   use type V.Outcome;
   ID       : constant IR.UCI_ID := IR.Create_UCI_ID ([others => 0], "disable");
   Location : constant IR.Component_Location :=
     IR.Create_Component_Location (0.0, 0.0, 0.0, "", "");

   procedure Refused (View : V.View) is
   begin
      V.Disable (View);
      raise Program_Error with "ineligible Disable succeeded";
   exception
      when AMS.MEL.Provider_Error =>
         null;
   end Refused;

   procedure Exercise (Provider_Path : String; Family : Positive) is
      Session : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "common-meta-disable019-ada");
      C2      : IR.C2.Control_Channel :=
        IR.C2.Open (Session, IR.C2.Create_Config (ID, ID, Location));
      Health  : IR.Health_Status.Health_Channel :=
        IR.Health_Status.Open (Session, IR.Health_Status.Create_Config (ID, ID, Location));
      Stacked : IR.Stacked_Image.Stacked_Channel :=
        IR.Stacked_Image.Open (Session, IR.Stacked_Image.Create_Config (ID, ID, Location));
      Image   : IR.Image_Stream :=
        IR.Open_Image_Control (Session, IR.Create_Image_Control_Config (ID, ID, Location));

      function Open_View return V.View is
      begin
         case Family is
            when 1      =>
               return IR.C2.As_Channel (C2);

            when 2      =>
               return IR.Health_Status.As_Channel (Health);

            when 3      =>
               return IR.Stacked_Image.As_Channel (Stacked);

            when others =>
               return IR.Image.As_Channel (Image);
         end case;
      end Open_View;
      procedure Enable is
      begin
         case Family is
            when 1      =>
               IR.C2.Enable (C2);

            when 2      =>
               IR.Health_Status.Enable (Health);

            when 3      =>
               IR.Stacked_Image.Enable (Stacked);

            when others =>
               IR.Start (Image);
         end case;
      end Enable;
   begin
      declare
         View   : V.View := Open_View;
         Second : V.View := Open_View;
      begin
         V.Disable (View);
         Enable;
         declare
            Pending : V.Return_Request := V.Send_Keep_Alive (View);
         begin
            begin
               declare
                  Unexpected : constant V.Return_Result := V.Wait (Pending, 0);
               begin
                  raise Program_Error
                    with "request was not pending: " & V.Status (Unexpected)'Image;
               end;
            exception
               when IR.Timeout_Error =>
                  null;
            end;
            V.Disable (Second);
            V.Disable (View);
            if V.Status (V.Wait (Pending, 1_000)) /= V.Success then
               raise Program_Error with "Disable affected independent request";
            end if;
            V.Close (Pending);
         end;
         declare
            Capability : constant V.Channel_Capability := V.Capabilities (View);
         begin
            if V.Channel_Type_Count (Capability) = 0 then
               raise Program_Error with "Disable detached owner";
            end if;
         end;
         Enable;
         V.Disable (View);
         IR.C2.Close (C2);
         IR.Health_Status.Close (Health);
         IR.Stacked_Image.Close (Stacked);
         IR.Close (Image);
         Refused (Second);
         AMS.MEL.Close (Session);
         Refused (View);
         V.Close (View);
         Refused (View);
         V.Close (Second);
      end;
   end Exercise;

   procedure Run (Provider_Path : String) is
   begin
      for Family in 1 .. 4 loop
         Exercise (Provider_Path, Family);
      end loop;
      declare
         Session    : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "sched-ada-common");
         Instr      : IR.Instrumentation.Instrumentation_Channel :=
           IR.Instrumentation.Open (Session, IR.Instrumentation.Create_Config (ID, ID, Location));
         Track      : IR.Track.Track_Channel :=
           IR.Track.Open (Session, IR.Track.Create_Config (ID, ID, Location));
         Scheduling : IR.Scheduling.Scheduling_Channel :=
           IR.Scheduling.Open (Session, IR.Scheduling.Create_Config (ID, ID, Location));
         IV         : V.View := IR.Instrumentation.As_Channel (Instr);
         TV         : V.View := IR.Track.As_Channel (Track);
         SV         : V.View := IR.Scheduling.As_Channel (Scheduling);
      begin
         Refused (IV);
         Refused (TV);
         Refused (SV);
         V.Close (IV);
         V.Close (TV);
         V.Close (SV);
         IR.Instrumentation.Close (Instr);
         IR.Track.Close (Track);
         IR.Scheduling.Close (Scheduling);
         AMS.MEL.Close (Session);
      end;
      Ada.Text_IO.Put_Line ("PASS: safe Ada standalone common Disable");
   end Run;
end AMS_MEL_IR_Disable_Tests;
