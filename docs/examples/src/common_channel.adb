with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.Channel;
with Example_Support;

procedure Common_Channel is
   package IR renames AMS.MEL.IR;
   package C2 renames AMS.MEL.IR.C2;
   package Common renames AMS.MEL.IR.Channel;
   use Example_Support;
   use type Common.Outcome;
   use type Common.Command_Return;
   use type Common.Channel_Type;
begin
   declare
      Zero         : constant IR.UUID := [others => 0];
      Channel_ID   : constant IR.UCI_ID := IR.Create_UCI_ID (Zero, "example common channel");
      Platform     : constant IR.UCI_ID := IR.Create_UCI_ID (Zero, "example platform");
      Location     : constant IR.Component_Location :=
        IR.Create_Component_Location (1.25, -2.5, 3.75, "station", "mock");
      Config       : constant C2.Control_Config :=
        C2.Create_Config (Channel_ID, Platform, Location);
      Session      : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Configuration);
      Channel      : C2.Control_Channel := C2.Open (Session, Config);
      View         : Common.View := C2.As_Channel (Channel);
      Capabilities : constant Common.Channel_Capability := Common.Capabilities (View);
      Request      : Common.Return_Request := Common.Send_Keep_Alive (View);
      Result       : Common.Return_Result;
   begin
      Require
        (Common.Channel_Type_Count (Capabilities) = 1
         and then Common.Channel_Type_At (Capabilities, 1) = Common.Command_And_Control,
         "advertised common C2 capability");
      Ada.Text_IO.Put_Line ("capability: COMMAND_AND_CONTROL (owned snapshot)");
      --  Completion may win this poll. Timeout preserves this same request.
      begin
         Result := Common.Wait (Request, 0);
      exception
         when IR.Timeout_Error =>
            Require (Common.Is_Open (Request), "timeout consumed request");
            Result := Common.Wait (Request, 1_000);
      end;
      case Common.Status (Result) is
         when Common.Success  =>
            Require (Common.Value (Result) = Common.Return_Success, "KeepAlive payload");

         when Common.Rejected =>
            raise Program_Error with "KeepAlive rejected: " & Common.Description (Result);
      end case;
      declare
         Cached : constant Common.Return_Result := Common.Wait (Request, 0);
      begin
         Require (Common.Status (Cached) = Common.Success, "cached outcome");
         Require (Common.Value (Cached) = Common.Value (Result), "cached payload");
      end;
      Common.Close (Request);
      Common.Close (View);
      C2.Close (Channel);
      AMS.MEL.Close (Session);
      Require (Common.Channel_Type_Count (Capabilities) > 0, "owned capability after Close");
      Ada.Text_IO.Put_Line
        ("KeepAlive=RETURN_SUCCESS; cached result agrees; snapshot survives Close");
   end;
exception
   when Error : others =>
      Report_Failure (Error);
end Common_Channel;
