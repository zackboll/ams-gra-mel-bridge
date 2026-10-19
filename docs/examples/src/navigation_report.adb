with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Image;
with Example_Support;
with Mock_Fixtures;
with Interfaces;

procedure Navigation_Report is
   package IR renames AMS.MEL.IR;
   package Image renames AMS.MEL.IR.Image;
   use Example_Support;
   use type Image.Navigation_Error_Code;
   use type Interfaces.Unsigned_32;
begin
   declare
      Zero     : constant IR.UUID := [others => 0];
      Channel  : constant IR.UCI_ID := IR.Create_UCI_ID (Zero, "example navigation");
      Platform : constant IR.UCI_ID := IR.Create_UCI_ID (Zero, "example platform");
      Location : constant IR.Component_Location :=
        IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock");
      Config   : constant IR.Image_Config :=
        IR.Create_Image_Config (Channel, Platform, Location, 2, 64, 2);
      Session  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Configuration);
      Stream   : IR.Image_Stream := IR.Open_Image_Stream (Session, Config);
      --  The helper constructs every field, including all 18 covariance terms.
      Supplied : constant Image.Navigation_Report := Mock_Fixtures.Navigation_Fidelity_Report;
      Request  : Image.Navigation_Request := Image.Submit_Navigation_Report (Stream, Supplied);
      Result   : constant Image.Navigation_Result := Image.Wait (Request, 1_000);
   begin
      --  The stream is attached; no Start is required for this submission.
      case Image.Status (Result) is
         when Image.Success  =>
            declare
               Response : constant Image.Navigation_Response := Image.Response (Result);
            begin
               Require
                 (Response.System_Time_NS = -8_765_432_109
                  and then Response.Command_ID = 16#F123_4567#
                  and then Response.Request_ID = 16#89AB_CDEF#,
                  "mock navigation response");
               Ada.Text_IO.Put_Line ("navigation SUCCESS: command=0xF1234567 request=0x89ABCDEF");
            end;

         when Image.Rejected =>
            Require
              (Image.Rejection_Code (Result) = Image.Invalid_Parameters
               and then Image.Description (Result) = "invalid navigation report",
               "mock rejection");
            Ada.Text_IO.Put_Line ("navigation REJECTED: " & Image.Description (Result));
      end case;
      Image.Close (Request);
      IR.Close (Stream);
      AMS.MEL.Close (Session);
   end;
exception
   when Error : others =>
      Report_Failure (Error);
end Navigation_Report;
