with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Buffer;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Image;
with AMS.MEL.IR.Image.Checkouts;
with Interfaces;

package body AMS_MEL_IR_Checkout_Tests is
   package IR renames AMS.MEL.IR;
   package Q renames IR.Image.Checkouts;
   use type Interfaces.Unsigned_64;
   ID       : constant IR.UCI_ID := IR.Create_UCI_ID ([others => 0], "checkout");
   Location : constant IR.Component_Location :=
     IR.Create_Component_Location (0.0, 0.0, 0.0, "", "");

   procedure Exercise (Provider_Path, Scenario : String) is
      Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "checkout021-" & Scenario);
      Image    : IR.Image_Stream :=
        IR.Open_Image_Control (Parent, IR.Create_Image_Control_Config (ID, ID, Location));
      Receiver : Q.Checkout_Receiver := Q.Open (Image, 2, 3);
      Object   : IR.Buffer.Host_Buffer := IR.Buffer.Open (Parent, 16, -20);
      View     : IR.Channel.View := IR.Image.As_Channel (Image);
      Failed   : constant Boolean := Scenario = "fail" or else Scenario = "throw";
      procedure Poll (Milliseconds : Natural) is
      begin
         declare
            Unexpected : Q.Checkout_Lease := Q.Receive (Receiver, Milliseconds);
         begin
            Q.Close (Unexpected);
            raise Program_Error with "Timeout expected";
         end;
      exception
         when IR.Timeout_Error =>
            null;
      end Poll;
   begin
      Poll (0);
      Poll (1);
      IR.Buffer.Write_Storage (Object, 0, [1 .. 16 => 16#A5#]);
      IR.Channel.Register_Buffer (View, Object);
      IR.Start (Image);
      declare
         Request : IR.Channel.Return_Request := IR.Channel.Send_Keep_Alive (View);
         Lease   : Q.Checkout_Lease := Q.Receive (Receiver, 1000);
      begin
         IR.Channel.Close (Request);
         if not Q.Is_Open (Lease) or else Q.Counters (Receiver).Checkouts_Accepted /= 1 then
            raise Program_Error with "Opaque checkout ownership/counter mismatch";
         end if;
         Poll (0);
         IR.Channel.Disable (View);
         begin
            IR.Channel.Unregister_Buffer (View, Object);
            raise Program_Error with "Held lease allowed unregister";
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         AMS.MEL.Close (Parent);
         begin
            Q.Close (Lease);
            if Failed then
               raise Program_Error with "Release failure expected";
            end if;
         exception
            when AMS.MEL.Provider_Error =>
               if not Failed then
                  raise;
               end if;
         end;
         if Q.Is_Open (Lease) then
            raise Program_Error with "Close must consume a failed lease";
         end if;
         Q.Close (Lease);
      end;
      if Failed then
         if Q.Counters (Receiver).Releases_Uncertain /= 1 then
            raise Program_Error with "Uncertain counter expected";
         end if;
         begin
            IR.Channel.Unregister_Buffer (View, Object);
            raise Program_Error with "Uncertain release allowed unregister";
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
      else
         if Q.Counters (Receiver).Releases_Succeeded /= 1 then
            raise Program_Error with "Exactly one release expected";
         end if;
         Q.Close (Receiver);
         IR.Channel.Unregister_Buffer (View, Object);
         IR.Buffer.Close (Object);
         IR.Close (Image);
      end if;
      Q.Close (Receiver);
      IR.Channel.Close (View);
   end Exercise;

   procedure Run (Provider_Path : String) is
   begin
      Exercise (Provider_Path, "normal");
      Exercise (Provider_Path, "fail");
      Exercise (Provider_Path, "throw");
      Ada.Text_IO.Put_Line ("Ada opaque checkout/release tests passed");
   end Run;
end AMS_MEL_IR_Checkout_Tests;
