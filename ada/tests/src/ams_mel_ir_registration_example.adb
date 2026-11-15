with Ada.Command_Line;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Buffer;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.Channel;

procedure AMS_MEL_IR_Registration_Example is
   package IR renames AMS.MEL.IR;
   Parent   : AMS.MEL.Session :=
     AMS.MEL.Open (Ada.Command_Line.Argument (1), "registration020-normal");
   ID       : constant IR.UCI_ID := IR.Create_UCI_ID ([others => 0], "example");
   Location : constant IR.Component_Location :=
     IR.Create_Component_Location (0.0, 0.0, 0.0, "", "");
   C2       : IR.C2.Control_Channel := IR.C2.Open (Parent, IR.C2.Create_Config (ID, ID, Location));
   Object   : IR.Buffer.Host_Buffer := IR.Buffer.Open (Parent, 16, -20);
   View     : IR.Channel.View := IR.C2.As_Channel (C2);
begin
   IR.Buffer.Write_Storage (Object, 0, [1 .. 16 => 16#A5#]);
   IR.Buffer.Set_Flags (Object, [IR.Buffer.Host]);
   IR.Channel.Register_Buffer (View, Object);
   IR.C2.Enable (C2);
   IR.Channel.Disable (View);
   IR.Channel.Unregister_Buffer (View, Object);
   if IR.Buffer.Read_Storage (Object, 0, 16)'Length /= 16 then
      raise Program_Error;
   end if;
   IR.Buffer.Close (Object);
   IR.C2.Close (C2);
   IR.Channel.Close (View);
   AMS.MEL.Close (Parent);
end AMS_MEL_IR_Registration_Example;
