with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.C2.Common;
with AMS.MEL.IR.C2.Metadata;
with AMS.MEL.IR.Channel;
with Interfaces;

package body AMS_MEL_IR_C2_Common_Tests is
   package C2 renames AMS.MEL.IR.C2; package Common renames C2.Common;
   package M renames C2.Metadata; package V renames AMS.MEL.IR.Channel;
   use type C2.Command_Return; use type C2.Outcome; use type C2.Error_Code;
   use type C2.Command_ID; use type V.Comms_Request_ID; use type V.Channel_Type;
   use type V.Comms_Test_Report;
   use type V.Pixel_Format; use type V.Sensor_Type; use type V.Metadata_Capability;
   use type V.Band_Type; use type V.Coordinate_System_Type;
   use type M.Metadata_Kind; use type Interfaces.Unsigned_32;
   Zero : constant AMS.MEL.IR.UUID := [others => 0];
   Config : constant C2.Control_Config := C2.Create_Config
     (AMS.MEL.IR.Create_UCI_ID (Zero, "common channel"),
      AMS.MEL.IR.Create_UCI_ID (Zero, "common platform"),
      AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock"));

   procedure Test_Pre_Enable (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "comms-high");
      Control : C2.Control_Channel := C2.Open (Parent, Config);
      Stream : M.Metadata_Stream := M.Open (Control, 4);
   begin
      M.Enable_Comms_Test_Events (Stream);
      declare Keepalive : C2.Return_Request := Common.Send_Keep_Alive (Control);
      begin
         if C2.Value (C2.Wait (Keepalive, 1_000)) /= C2.Return_Success or else
           C2.Value (C2.Wait (Keepalive, 0)) /= C2.Return_Success then
            raise Program_Error with "Ada KeepAlive mismatch";
         end if; C2.Close (Keepalive);
      end;
      declare Request : Common.Comms_Request := Common.Submit_Comms_Test
        (Control, 16#F000_0002#, 16#8000_0001#, 16#E000_0003#);
         Event : constant M.Metadata_Event := M.Receive (Stream, 1_000);
         Callback : constant V.Comms_Test_Report := M.Comms_Test (Event);
         Result : constant Common.Comms_Result := Common.Wait (Request, 1_000);
         Report : constant V.Comms_Test_Report := Common.Report (Result);
      begin
         if M.Kind (Event) /= M.Channel_Comms_Test_Event or else
           Callback.Command_ID /= 16#8000_0001# or else Callback.Request_ID /= 16#E000_0003# or else
           Common.Status (Result) /= C2.Success or else Report.Command_ID /= Callback.Command_ID or else
           Report.Request_ID /= Callback.Request_ID or else Common.Report (Common.Wait (Request, 0)) /= Report
         then raise Program_Error with "Ada CommsTest mismatch"; end if;
         Common.Close (Request); Common.Close (Request);
      end;
      M.Close (Stream); C2.Close (Control); AMS.MEL.Close (Parent);
   end Test_Pre_Enable;

   procedure Test_Capability (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "capability-rich");
      Control : C2.Control_Channel := C2.Open (Parent, Config);
      Value : constant V.Channel_Capability := Common.Capabilities (Control);
   begin
      C2.Close (Control); AMS.MEL.Close (Parent);
      if AMS.MEL.IR.Descriptive_Label (V.Channel_ID (Value)) /= "channel-α" or else
        V.Height (Value) /= 1080 or else V.Width (Value) /= 1920 or else V.Bit_Depth (Value) /= 12 or else
        V.Row_Pitch (Value) /= 4096 or else V.Buffer_Size (Value) /= 8_388_608 or else
        V.Image_Size (Value) /= 4_147_200 or else V.Number_Of_Bands (Value) /= 3 or else V.Format (Value) /= V.RGB or else
        V.Sensor_Type_Count (Value) /= 2 or else V.Sensor_Type_At (Value, 1) /= V.Gimbal_Horizontal or else V.Sensor_Type_At (Value, 2) /= V.Step_Stare or else
        AMS.MEL.IR.Descriptive_Label (V.Platform_ID (Value)) /= "platform-€" or else
        AMS.MEL.IR.Offset_X_M (V.Sensor_Location (Value)) /= 1.25 or else AMS.MEL.IR.Offset_Y_M (V.Sensor_Location (Value)) /= -2.5 or else
        AMS.MEL.IR.Offset_Z_M (V.Sensor_Location (Value)) /= 3.75 or else AMS.MEL.IR.Key (V.Sensor_Location (Value)) /= "sensor-key" or else AMS.MEL.IR.System_Name (V.Sensor_Location (Value)) /= "system-β" or else
        V.Channel_Type_Count (Value) /= 3 or else V.Channel_Type_At (Value, 1) /= V.Command_And_Control or else V.Channel_Type_At (Value, 3) /= V.Reserved_2 or else
        V.Task_Schedule_Depth (Value) /= 17 or else not V.ODC_Available (Value) or else not V.NUC_Available (Value) or else
        V.Metadata_Capability_Count (Value) /= 4 or else not V.Has_Metadata_Capability (Value, V.Channel_Comms_Test_Rep) or else
        V.Image_Band_Count (Value) /= 2 or else V.Image_Band_Index_At (Value, 1) /= 2 or else V.Image_Band_Info_Count (Value, 1) /= 2 or else
        V.Image_Band_Info_At (Value, 1, 1).Kind /= V.IR_Longwave or else V.Image_Band_Index_At (Value, 2) /= 9 or else
        V.Nav_Frame_Count (Value) /= 2 or else V.Nav_Frame_At (Value, 1) /= V.NED_Sensor or else V.Nav_Frame_At (Value, 2) /= V.ECEF
      then raise Program_Error with "Ada ChannelCapability mismatch"; end if;
   end Test_Capability;

   procedure Test_Timeout_And_Rejection (Provider_Path : String) is
   begin
      declare Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "comms-delayed"); Control : C2.Control_Channel := C2.Open (Parent, Config); Request : Common.Comms_Request := Common.Submit_Comms_Test (Control, 7, 17, 19); begin
         begin declare Unexpected : constant Common.Comms_Result := Common.Wait (Request, 0); begin raise Program_Error with C2.Outcome'Image (Common.Status (Unexpected)); end; exception when AMS.MEL.IR.Timeout_Error => null; end;
         AMS.MEL.Close (Parent); C2.Close (Control); if Common.Report (Common.Wait (Request, 1_000)).Request_ID /= 19 then raise Program_Error; end if; Common.Close (Request);
      end;
      declare Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "comms-reject"); Control : C2.Control_Channel := C2.Open (Parent, Config); Request : Common.Comms_Request := Common.Submit_Comms_Test (Control, 1, 2, 3); Result : constant Common.Comms_Result := Common.Wait (Request, 1_000); begin
         if Common.Status (Result) /= C2.Rejected or else Common.Rejection_Code (Result) /= C2.Invalid_Parameters or else Common.Description (Result)'Length /= 613 then raise Program_Error with "Ada Comms rejection mismatch"; end if;
         Common.Close (Request); C2.Close (Control); AMS.MEL.Close (Parent);
      end;
   end Test_Timeout_And_Rejection;

   procedure Run (Provider_Path : String) is
   begin Test_Pre_Enable (Provider_Path); Test_Capability (Provider_Path); Test_Timeout_And_Rejection (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada common IR C2 Channel contract"); end Run;
end AMS_MEL_IR_C2_Common_Tests;
