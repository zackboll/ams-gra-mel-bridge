with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Buffer;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Health_Status;
with AMS.MEL.IR.Stacked_Image;
with AMS.MEL.IR.Image;
with AMS.MEL.IR.Channel.Metadata;
with AMS.MEL.IR.C2.Metadata;

package body AMS_MEL_IR_Registration_Tests is
   package IR renames AMS.MEL.IR;
   package B renames IR.Buffer;
   package V renames IR.Channel;
   use type IR.Pixel_Array;
   use type V.Comms_Test_Report;
   use type IR.C2.Metadata.Metadata_Kind;
   ID       : constant IR.UCI_ID := IR.Create_UCI_ID ([others => 0], "registration020");
   Location : constant IR.Component_Location :=
     IR.Create_Component_Location (0.0, 0.0, 0.0, "", "");

   procedure Exercise (Provider_Path : String; Family : Positive; Refusal : Boolean) is
      Session     : AMS.MEL.Session :=
        AMS.MEL.Open
          (Provider_Path,
           (if Refusal then "registration020-register-retained" else "registration020-ada"));
      C2          : IR.C2.Control_Channel :=
        IR.C2.Open (Session, IR.C2.Create_Config (ID, ID, Location));
      Health      : IR.Health_Status.Health_Channel :=
        IR.Health_Status.Open (Session, IR.Health_Status.Create_Config (ID, ID, Location));
      Stacked     : IR.Stacked_Image.Stacked_Channel :=
        IR.Stacked_Image.Open (Session, IR.Stacked_Image.Create_Config (ID, ID, Location));
      Image       : IR.Image_Stream :=
        IR.Open_Image_Control (Session, IR.Create_Image_Control_Config (ID, ID, Location));
      Other       : IR.C2.Control_Channel :=
        IR.C2.Open (Session, IR.C2.Create_Config (ID, ID, Location));
      Object      : B.Host_Buffer := B.Open (Session, 16, -20);
      Independent : B.Host_Buffer := B.Open (Session, 16, 42);
      Pattern     : constant IR.Pixel_Array (3 .. 18) := [others => 16#A5#];

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

      procedure Close_Typed is
      begin
         case Family is
            when 1      =>
               IR.C2.Close (C2);

            when 2      =>
               IR.Health_Status.Close (Health);

            when 3      =>
               IR.Stacked_Image.Close (Stacked);

            when others =>
               IR.Close (Image);
         end case;
      end Close_Typed;
   begin
      B.Write_Storage (Object, 0, Pattern);
      B.Set_Flags (Object, [B.Host]);
      if B.User_Context (Object) /= -20
        or B.User_Context (Independent) /= 42
        or B.Read_Storage (Independent, 0, 16) /= [1 .. 16 => 0]
      then
         raise Program_Error with "Buffer identity or context changed";
      end if;
      declare
         View   : V.View := Open_View;
         Second : V.View := Open_View;
         Wrong  : V.View := IR.C2.As_Channel (Other);
      begin
         if Refusal then
            begin
               V.Register_Buffer (View, Object);
               raise Program_Error with "provider refusal became success";
            exception
               when AMS.MEL.Provider_Error =>
                  null;
            end;
         else
            V.Register_Buffer (View, Object);
         end if;
         for Operation in 1 .. 6 loop
            begin
               case Operation is
                  when 1      =>
                     V.Register_Buffer (View, Object);

                  when 2      =>
                     V.Register_Buffer (Second, Object);

                  when 3      =>
                     V.Unregister_Buffer (Wrong, Object);

                  when 4      =>
                     B.Close (Object);

                  when 5      =>
                     Close_Typed;

                  when others =>
                     B.Write_Storage (Object, 0, Pattern);
               end case;
               raise Program_Error with "unsafe registration operation accepted";
            exception
               when AMS.MEL.Provider_Error =>
                  null;
            end;
         end loop;
         if not B.Is_Open (Object) then
            raise Program_Error with "rejected Close lost Buffer handle";
         end if;
         AMS.MEL.Close (Session);
         if not Refusal then
            for Cycle in 1 .. 3 loop
               Enable;
               begin
                  V.Unregister_Buffer (View, Object);
                  raise Program_Error with "enabled unregister accepted";
               exception
                  when AMS.MEL.Provider_Error =>
                     null;
               end;
               V.Disable (View);
            end loop;
         end if;
         V.Unregister_Buffer (Second, Object);
         if B.Read_Storage (Object, 0, 16) /= Pattern
           or not B.Properties (Object).Buffer_Address_Matches
         then
            raise Program_Error with "storage changed or moved";
         end if;
         B.Close (Object);
         B.Close (Independent);
         Close_Typed;
         V.Close (View);
         V.Close (Second);
         V.Close (Wrong);
      end;
      IR.C2.Close (Other);
      IR.C2.Close (C2);
      IR.Health_Status.Close (Health);
      IR.Stacked_Image.Close (Stacked);
      IR.Close (Image);
   end Exercise;

   procedure Reentrant_Metadata (Provider_Path : String) is
      Session : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "registration020-ada");
      C2      : IR.C2.Control_Channel :=
        IR.C2.Open (Session, IR.C2.Create_Config (ID, ID, Location));
      Health  : IR.Health_Status.Health_Channel :=
        IR.Health_Status.Open (Session, IR.Health_Status.Create_Config (ID, ID, Location));
      CV      : V.View := IR.C2.As_Channel (C2);
      HV      : V.View := IR.Health_Status.As_Channel (Health);
      CM      : IR.C2.Metadata.Metadata_Stream := IR.C2.Metadata.Open (C2);
      HM      : V.Metadata.Comms_Metadata_Stream := V.Metadata.Open (HV);
      Object  : B.Host_Buffer := B.Open (Session, 16, -20);
   begin
      IR.C2.Metadata.Enable_Comms_Test_Events (CM);
      V.Register_Buffer (CV, Object);
      declare
         Event : constant IR.C2.Metadata.Metadata_Event := IR.C2.Metadata.Receive (CM);
      begin
         if IR.C2.Metadata.Kind (Event) /= IR.C2.Metadata.Channel_Comms_Test_Event
           or else IR.C2.Metadata.Comms_Test (Event) /= (20, 1)
           or else V.Metadata.Receive (HM) /= (20, 1)
         then
            raise Program_Error with "synchronous registration metadata reentry";
         end if;
      end;
      V.Unregister_Buffer (CV, Object);
      declare
         Event : constant IR.C2.Metadata.Metadata_Event := IR.C2.Metadata.Receive (CM);
      begin
         if IR.C2.Metadata.Comms_Test (Event) /= (20, 2) or else V.Metadata.Receive (HM) /= (20, 2)
         then
            raise Program_Error with "synchronous unregister metadata reentry";
         end if;
      end;
      B.Close (Object);
      V.Metadata.Close (HM);
      IR.C2.Metadata.Close (CM);
      V.Close (CV);
      V.Close (HV);
      IR.C2.Close (C2);
      IR.Health_Status.Close (Health);
      AMS.MEL.Close (Session);
   end Reentrant_Metadata;

   procedure Run (Provider_Path : String) is
   begin
      for Family in 1 .. 4 loop
         Exercise (Provider_Path, Family, False);
         Exercise (Provider_Path, Family, True);
      end loop;
      Reentrant_Metadata (Provider_Path);
      --  Non-raising finalization must retain a graph with an active obligation.
      declare
         Session : constant AMS.MEL.Session :=
           AMS.MEL.Open (Provider_Path, "registration020-finalize");
         C2      : constant IR.C2.Control_Channel :=
           IR.C2.Open (Session, IR.C2.Create_Config (ID, ID, Location));
         Object  : B.Host_Buffer := B.Open (Session, 16, -20);
         View    : V.View := IR.C2.As_Channel (C2);
      begin
         V.Register_Buffer (View, Object);
         V.Close (View);
      end;
      Ada.Text_IO.Put_Line ("PASS: safe Ada application Buffer registration (four families)");
   end Run;
end AMS_MEL_IR_Registration_Tests;
