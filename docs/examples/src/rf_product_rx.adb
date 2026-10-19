with Ada.Text_IO;
with AMS.MEL.RF;
with AMS.MEL.RF.Product_Rx;
with Example_Support;
with Interfaces;

procedure RF_Product_RX is
   package RF renames AMS.MEL.RF;
   package RX renames AMS.MEL.RF.Product_Rx;
   use Example_Support;
   use type RF.Job_Data_Format;
   use type Interfaces.Integer_16;
   use type Interfaces.Unsigned_32;
   use type RX.Create_Error_Code;
   use type RX.Counter;
   Sum : Integer := 0;
   procedure Process (Samples : RX.Complex_I16_Array) is
   begin
      for Index in Samples'Range loop
         Sum := Sum + Integer (Samples (Index).Real);
      end loop;
   end Process;
begin
   declare
      Data    : RF.Data_MEL := RF.Open (Provider_Path, Configuration);
      Config  : constant RX.Config := RX.Create_Config (65_536, 4, 16);
      Request : RX.Create_Request := RX.Submit (Data, Config);
      Result  : constant RX.Create_Result := RX.Wait (Request, 1_000);
   begin
      case RX.Outcome (Result) is
         when RX.Failed  =>
            Require
              (RX.Error_Code (Result) = RX.Insufficient_Resources
               and then RX.Description (Result) = "mock ProductRx resources exhausted",
               "mock creation rejection");
            Ada.Text_IO.Put_Line ("ProductRx FAILED: " & RX.Description (Result));

         when RX.Created =>
            declare
               --  Registration inside Claim may deliver before Claim returns.
               Endpoint : RX.Endpoint := RX.Claim (Request);
               Event    : RX.Event := RX.Receive (Endpoint, 1_000);
               Metadata : constant RX.Product_Metadata := RX.Metadata (Event);
            begin
               Require
                 (RX.Data_Format (Event) = RF.Complex_INT16 and then RX.Sample_Count (Event) = 2,
                  "Complex INT16 pair count");
               Require
                 (RX.Job_Details_ID (Metadata) = 16#DEAD_BEEF#
                  and then RX.MEL_Protocol_Version_ID (Metadata) = 16#FEDC_BA98#,
                  "copied ProductRx metadata");
               RX.With_Samples (Event, Process'Access);
               Require (Sum = 76, "borrowed real sum");
               declare
                  Copy : constant RX.Complex_I16_Array := RX.Copy_Samples (Event);
               begin
                  RX.Close (Event);
                  Require (RX.Statistics (Endpoint).Products_Queued = 1, "sole queued product");
                  begin
                     declare
                        Unexpected : RX.Event := RX.Receive (Endpoint, 0);
                     begin
                        RX.Close (Unexpected);
                        raise Program_Error with "expected empty-queue timeout";
                     end;
                  exception
                     when RX.Timeout_Error =>
                        Ada.Text_IO.Put_Line ("empty queue: Timeout_Error (endpoint remains open)");
                  end;
                  RX.Close (Endpoint);
                  RX.Close (Request);
                  RF.Close (Data);
                  Require
                    (Copy'Length = 2
                     and then Copy (Copy'First).Real = 77
                     and then Copy (Copy'First).Imag = -77
                     and then Copy (Copy'Last).Real = -1
                     and then Copy (Copy'Last).Imag = 1,
                     "independent actual sample pairs after Close");
                  Sum := 0;
                  Process (Copy);
                  Require (Sum = 76, "independent sum");
                  Ada.Text_IO.Put_Line
                    ("Complex INT16 pairs after Close: (77,-77) (-1,1); real sum=76");
                  Ada.Text_IO.Put_Line ("metadata Job_Details_ID=0xDEADBEEF; products queued=1");
               end;
            end;
      end case;
      RX.Close (Request);
      RF.Close (Data);
   end;
exception
   when Error : others =>
      Report_Failure (Error);
end RF_Product_RX;
