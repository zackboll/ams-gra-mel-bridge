with AMS.MEL;
with AMS.MEL.RF;
with Ada.Exceptions;
with Interfaces;

package body AMS_MEL_RF_Tests is
   package RF renames AMS.MEL.RF;
   use type RF.Job_Data_Format;
   use type RF.Frequency_Range;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Unsigned_64;
   use type Interfaces.Integer_64;
   use type AMS.MEL.Provider_Version_Number;

   procedure Verify (Value : RF.MFA_Info; Reported : Interfaces.Unsigned_64) is
      Seven     : constant RF.Face_Info := RF.Face_At (Value, 1);
      Forty_Two : constant RF.Face_Info := RF.Face_At (Value, 2);
   begin
      if RF.Reported_Num_Faces (Value) /= Reported
        or else RF.Face_Count (Value) /= 2
        or else not RF.Contains_Open_Additions (Value)
        or else RF.Scheduler_Resolution_FS (Value) /= 12_345
        or else RF.Max_User_Defined_Context_Bytes (Value) /= 9_876
        or else RF.Supported_Data_Format_Count (Value) /= 4
        or else RF.Supported_Data_Format_At (Value, 1) /= RF.Direct_INT8
        or else RF.Supported_Data_Format_At (Value, 2) /= RF.Complex_INT16
        or else RF.Supported_Data_Format_At (Value, 3) /= RF.AMS_VITA_Large
        or else RF.Supported_Data_Format_At (Value, 4) /= RF.LF_Type3
        or else RF.Face_ID (Seven) /= 7
        or else RF.Face_ID (Forty_Two) /= 42
        or else not RF.Supports_Receive (Seven)
        or else RF.Supports_Transmit (Seven)
        or else not RF.Requires_Endpoint_Association (Seven)
        or else RF.Supports_Receive (Forty_Two)
        or else not RF.Supports_Transmit (Forty_Two)
        or else RF.Requires_Endpoint_Association (Forty_Two)
        or else RF.AGC_Processing_Time_FS (Seven) /= 1_001
        or else RF.Min_Job_Request_Lead_Time_FS (Seven) /= 1_002
        or else RF.Max_Job_Request_Lead_Time_FS (Seven) /= 1_003_000_000_000_000
        or else RF.Min_Job_Detail_Lead_Time_FS (Seven) /= 1_004
        or else RF.Tx_Rx_Switching_Time_FS (Seven) /= 1_005
        or else RF.Rx_Tx_Switching_Time_FS (Seven) /= 1_006
        or else RF.Tx_Tx_Switching_Time_FS (Seven) /= 1_007
        or else RF.Rx_Rx_Switching_Time_FS (Seven) /= 1_008
        or else RF.AGC_Processing_Time_FS (Forty_Two) /= 0
        or else RF.Min_Job_Request_Lead_Time_FS (Forty_Two) /= 42_001
        or else RF.Max_Job_Request_Lead_Time_FS (Forty_Two) /= Interfaces.Integer_64'Last
        or else RF.Min_Job_Detail_Lead_Time_FS (Forty_Two) /= 42_003
        or else RF.Tx_Rx_Switching_Time_FS (Forty_Two) /= 42_004
        or else RF.Rx_Tx_Switching_Time_FS (Forty_Two) /= 42_005
        or else RF.Tx_Tx_Switching_Time_FS (Forty_Two) /= 42_006
        or else RF.Rx_Rx_Switching_Time_FS (Forty_Two) /= -42_007
        or else RF.Rx_Frequency_Range_Count (Seven) /= 2
        or else RF.Rx_Frequency_Range_At (Seven, 1) /= (100_250_000.125, 200_500_000.5)
        or else RF.Rx_Frequency_Range_At (Seven, 2) /= (1_500_000_000.25, 2_750_000_000.75)
        or else RF.Rx_Frequency_Range_Count (Forty_Two) /= 0
        or else RF.Tx_Frequency_Range_Count (Seven) /= 0
        or else RF.Tx_Frequency_Range_Count (Forty_Two) /= 1
        or else RF.Tx_Frequency_Range_At (Forty_Two, 1) /= (433_125_000.5, 434_875_000.25)
        or else RF.Sample_Frequency_Range_Count (Seven) /= 1
        or else RF.Sample_Frequency_Range_At (Seven, 1) /= (1_024_000.5, 2_048_000.25)
        or else RF.Sample_Frequency_Range_Count (Forty_Two) /= 2
        or else RF.Sample_Frequency_Range_At (Forty_Two, 1) /= (3_000_000.75, 1_000_000.125)
        or else RF.Sample_Frequency_Range_At (Forty_Two, 2) /= (0.5, 0.25)
      then
         raise Program_Error with "RF MFA snapshot mismatch";
      end if;
   end Verify;

   procedure Run (Provider_Path : String) is
      procedure Test_Quantization is
         Inputs : constant array (Positive range 1 .. 13) of Interfaces.Integer_64 :=
           [0,
            1,
            9,
            10,
            19,
            -1,
            -9,
            -10,
            -19,
            1_234_567_890_123_456_789,
            -1_234_567_890_123_456_789,
            Interfaces.Integer_64'Last,
            Interfaces.Integer_64'First];
         Data   : RF.Data_MEL := RF.Open (Provider_Path, "quantize");
      begin
         for Input of Inputs loop
            if RF.Quantize_Duration (Data, Input) /= (Input / 10) * 10 or else not RF.Is_Open (Data)
            then
               raise Program_Error with "RF live quantization mismatch";
            end if;
         end loop;
         RF.Close (Data);
         begin
            declare
               Ignored : constant Interfaces.Integer_64 := RF.Quantize_Duration (Data, 1);
            begin
               raise Program_Error
                 with
                   "closed RF DataMEL accepted quantization"
                   & Interfaces.Integer_64'Image (Ignored);
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         for Scenario in 1 .. 3 loop
            declare
               Bad : RF.Data_MEL :=
                 RF.Open
                   (Provider_Path,
                    (if Scenario = 1
                     then "quantize-throw"
                     elsif Scenario = 2
                     then "quantize-unknown"
                     else "quantize-alloc"));
            begin
               begin
                  declare
                     Ignored : constant Interfaces.Integer_64 := RF.Quantize_Duration (Bad, -19);
                  begin
                     raise Program_Error
                       with
                         "provider quantization exception lost"
                         & Interfaces.Integer_64'Image (Ignored);
                  end;
               exception
                  when AMS.MEL.Provider_Error =>
                     null;
               end;
               if not RF.Is_Open (Bad) then
                  raise Program_Error with "failed quantization closed DataMEL";
               end if;
               RF.Close (Bad);
            end;
         end loop;
      end Test_Quantization;
      procedure Expect_Open_Failure (Scenario : String) is
      begin
         declare
            Bad : constant RF.Data_MEL := RF.Open (Provider_Path, Scenario);
         begin
            if RF.Is_Open (Bad) then
               raise Program_Error with "RF factory failure accepted";
            end if;
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end Expect_Open_Failure;
      Data     : RF.Data_MEL := RF.Open (Provider_Path, "inconsistent-faces");
      Version  : constant AMS.MEL.Provider_Version := RF.Query_Provider_Version (Data);
      Snapshot : constant RF.MFA_Info := RF.Snapshot_MFA_Info (Data);
   begin
      Test_Quantization;
      if not RF.Is_Open (Data)
        or else AMS.MEL.API_Version (Version) /= 16#0000_A5A5#
        or else AMS.MEL.Library_Version (Version) /= 16#5A5A_0000#
        or else AMS.MEL.Vendor (Version) /= "Mock RF µVendor"
        or else AMS.MEL.Description (Version) /= "Deterministic Task 033B RF DataMEL"
      then
         raise Program_Error with "RF version/open mismatch";
      end if;
      Verify (Snapshot, 5);
      RF.Close (Data);
      RF.Close (Data);
      if RF.Is_Open (Data) then
         raise Program_Error with "RF DataMEL close did not consume owner";
      end if;
      Verify (Snapshot, 5);
      declare
         Future : constant RF.Data_MEL := RF.Open (Provider_Path, "future-format");
         Value  : constant RF.MFA_Info := RF.Snapshot_MFA_Info (Future);
      begin
         if RF.Supported_Data_Format_Count (Value) /= 5
           or else RF.Supported_Data_Format_At (Value, 1) /= 16#F000_0001#
         then
            raise Program_Error with "future RF format narrowed";
         end if;
      end;
      declare
         Bad : constant RF.Data_MEL := RF.Open (Provider_Path, "mfa-throw-once");
      begin
         begin
            declare
               Ignored : constant RF.MFA_Info := RF.Snapshot_MFA_Info (Bad);
            begin
               if RF.Face_Count (Ignored) > 0 then
                  raise Program_Error with "expected RF snapshot failure";
               end if;
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         Verify (RF.Snapshot_MFA_Info (Bad), 2);
      end;
      begin
         declare
            Bad : constant RF.Data_MEL := RF.Open (Provider_Path & Character'Val (0), "x");
         begin
            if RF.Is_Open (Bad) then
               raise Program_Error with "NUL path accepted";
            end if;
         end;
      exception
         when Constraint_Error =>
            null;
      end;
      begin
         declare
            Bad : constant RF.Data_MEL := RF.Open (Provider_Path, "x" & Character'Val (0));
         begin
            if RF.Is_Open (Bad) then
               raise Program_Error with "NUL configuration accepted";
            end if;
         end;
      exception
         when Constraint_Error =>
            null;
      end;
      declare
         Long_Version     : constant RF.Data_MEL := RF.Open (Provider_Path, "version-long");
         Vendor_Text      : constant String :=
           String'(1 .. 300 => 'v')
           & Character'Val (16#E2#)
           & Character'Val (16#82#)
           & Character'Val (16#AC#);
         Description_Text : constant String :=
           String'(1 .. 1_000 => 'd')
           & Character'Val (16#F0#)
           & Character'Val (16#9F#)
           & Character'Val (16#93#)
           & Character'Val (16#A1#);
         Value            : constant AMS.MEL.Provider_Version :=
           RF.Query_Provider_Version (Long_Version);
      begin
         if AMS.MEL.API_Version (Value) /= 16#FEDC_BA98#
           or else AMS.MEL.Library_Version (Value) /= 16#0123_4567#
           or else AMS.MEL.Vendor (Value) /= Vendor_Text
           or else AMS.MEL.Description (Value) /= Description_Text
         then
            raise Program_Error with "RF version two-call sizing mismatch";
         end if;
      end;
      Expect_Open_Failure ("factory-null");
      Expect_Open_Failure ("factory-throw");
      Expect_Open_Failure ("factory-throw-unknown");
      begin
         declare
            Bad : constant RF.Data_MEL := RF.Open ("/definitely/missing/librfmel.so", "success");
         begin
            if RF.Is_Open (Bad) then
               raise Program_Error with "missing RF provider accepted";
            end if;
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      declare
         Bad : RF.Data_MEL := RF.Open (Provider_Path, "shutdown-throw");
      begin
         begin
            RF.Close (Bad);
            raise Program_Error with "RF shutdown exception not reported";
         exception
            when E : AMS.MEL.Provider_Error =>
               if Ada.Exceptions.Exception_Message (E) /= "mock RF shutdown exception"
                 or else RF.Is_Open (Bad)
               then
                  raise Program_Error with "RF failed Close retained owner or lost diagnostic";
               end if;
         end;
         RF.Close (Bad);
      end;
   end Run;
end AMS_MEL_RF_Tests;
