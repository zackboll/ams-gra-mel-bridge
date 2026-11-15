with Ada.Command_Line;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Buffer;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Image;
with AMS.MEL.IR.Image.Checkouts;
with Interfaces;

procedure AMS_MEL_IR_Checkout_Example is
   package IR renames AMS.MEL.IR;
   package Q renames IR.Image.Checkouts;
   use type Interfaces.Unsigned_64;
   use type IR.Channel.Outcome;
   Parent   : AMS.MEL.Session := AMS.MEL.Open (Ada.Command_Line.Argument (1), "checkout021-normal");
   ID       : constant IR.UCI_ID := IR.Create_UCI_ID ([others => 0], "checkout");
   Location : constant IR.Component_Location :=
     IR.Create_Component_Location (0.0, 0.0, 0.0, "", "");
   Image    : IR.Image_Stream :=
     IR.Open_Image_Control (Parent, IR.Create_Image_Control_Config (ID, ID, Location));
   Receiver : Q.Checkout_Receiver := Q.Open (Image);
   Object   : IR.Buffer.Host_Buffer := IR.Buffer.Open (Parent, 16, -20);
   View     : IR.Channel.View := IR.Image.As_Channel (Image);
begin
   IR.Buffer.Write_Storage (Object, 0, [1 .. 16 => 16#A5#]);
   IR.Channel.Register_Buffer (View, Object);
   IR.Start (Image);
   --  The deterministic mock delivers a genuine onImage during KeepAlive.
   --  Ordinary providers need not deliver any callback for that request.
   declare
      Request : IR.Channel.Return_Request := IR.Channel.Send_Keep_Alive (View);
      Result  : constant IR.Channel.Return_Result := IR.Channel.Wait (Request, 1000);
      Lease   : Q.Checkout_Lease := Q.Receive (Receiver, 1000);
   begin
      if not Q.Is_Open (Lease)
        or else Q.Counters (Receiver).Checkouts_Accepted /= 1
        or else IR.Channel.Status (Result) /= IR.Channel.Success
      then
         raise Program_Error with "Genuine opaque checkout expected";
      end if;
      IR.Channel.Close (Request);
      IR.Channel.Disable (View);
      begin
         IR.Channel.Unregister_Buffer (View, Object);
         raise Program_Error with "Unregister must remain blocked by the lease";
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      Q.Close (Lease);
      Q.Close (Lease);
   end;
   Q.Close (Receiver);
   IR.Channel.Unregister_Buffer (View, Object);
   IR.Buffer.Close (Object);
   IR.Close (Image);
   IR.Channel.Close (View);
   AMS.MEL.Close (Parent);
end AMS_MEL_IR_Checkout_Example;
