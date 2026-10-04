with Ada.Command_Line;
with Ada.Text_IO;
with AMS.MEL.RF.Admin;
with AMS.MEL.RF.C2;
with AMS.MEL.RF.C2.Virtual_Aperture_Queries;
with AMS.MEL.RF.C2.Virtual_Aperture_Notifications;
with AMS.MEL.RF.C2.Element_Groups;
with AMS.MEL.RF.C2.Local_Functions;
with AMS.MEL.RF.C2.Data_Pipes;
with AMS.MEL.Status;
with Interfaces;

procedure AMS_MEL_Squall_RF_VA is
   package Admin renames AMS.MEL.RF.Admin;
   package C2 renames AMS.MEL.RF.C2;
   package Q renames C2.Virtual_Aperture_Queries;
   package N renames C2.Virtual_Aperture_Notifications;
   package E renames C2.Element_Groups;
   package LF renames C2.Local_Functions;
   package P renames C2.Data_Pipes;
   package Status renames AMS.MEL.Status;
   use type C2.Request_Outcome;
   use type Interfaces.Unsigned_32;
   use type Q.Status_Kind;
   use type E.Element_Group_Mode;
   procedure Connections (Value : P.Connection_Snapshot) is
   begin
      if P.Group_Count (Value) /= 1 then
         raise Program_Error with "Squall E5 group count mismatch";
      end if;
      declare
         G : constant P.Element_Group_Connection := P.Group_At (Value, 1);
      begin
         if P.Element_Group_Label (G) /= "0" or else P.Pipe_Count (G) /= 1 then
            raise Program_Error with "Squall E5 group/key mismatch";
         end if;
         declare
            Pipe : constant P.Data_Pipe_Connection := P.Pipe_At (G, 1);
         begin
            if P.Lookup_Label (Pipe) /= "default" or else P.Label (Pipe) /= "default"
              or else P.Endpoint_Count (Pipe) /= 0
            then
               raise Program_Error with "Squall E5 default fresh empty pipe mismatch";
            end if;
         end;
      end;
   end Connections;
   procedure Associations (VA : in out C2.Virtual_Aperture'Class) is
   begin
      Connections (P.Snapshot (VA));
      if not P.Associate_Endpoint (VA, "0", "default", 16#8000_0000_0000_0000#)
        or else not P.Associate_Endpoints (VA, "0", "default", [0, 7, Interfaces.Unsigned_64'Last])
      then
         raise Program_Error with "Squall E5 mutation method returned false";
      end if;
      --  Each call above used a fresh getDataPipes container/object. A NEW
      --  snapshot is empty; true is mutation-return evidence, not persistence.
      Connections (P.Snapshot (VA));
   end Associations;
   procedure Descriptors (Value : E.Element_Group_List; Included : Boolean) is
   begin
      if E.Count (Value) /= 1 or else E.Data_Pipes_Included (Value) /= Included then
         raise Program_Error with "Squall descriptor inclusion/count mismatch";
      end if;
      declare
         G : constant E.Element_Group_Descriptor := E.Descriptor_At (Value, 1);
      begin
         if E.Lookup_Label (G) /= "0" or else E.Label (G) /= "0"
           or else E.Mode (G) /= E.Receive
           or else E.Max_RF_Bandwidth_Hz (G) /= 2_048_000.0
           or else E.Max_Sample_Rate_Samples_Per_Second (G) /= 2_048_000.0
           or else E.Max_Data_Rate_Bits_Per_Second (G) /= 65_536_000.0
           or else E.Max_Duty_Factor (G) /= 1.0
           or else E.Data_Pipes_Included (G) /= Included
           or else E.Pipe_Count (G) /= (if Included then 1 else 0)
         then
            raise Program_Error with "Squall exact pinned descriptor values mismatch";
         end if;
         if Included then
            declare P : constant E.Data_Pipe_Info := E.Pipe_At (G, 1); begin
               if E.Lookup_Label (P) /= "default" or else E.Label (P) /= "default"
                 or else E.Endpoint_Count (P) /= 0
               then raise Program_Error with "Squall default pipe mismatch"; end if;
            end;
         end if;
      end;
   end Descriptors;
   procedure Queries (VA : C2.Virtual_Aperture'Class) is
      Catalog : constant LF.Local_Function_List := LF.Snapshot_Local_Functions (VA);
      Statuses : constant LF.Status_List := LF.Snapshot_Local_Function_Status (VA, 0, 0);
      All_IDs      : constant Q.Instance_ID_List := Q.Snapshot_All_Instances (VA);
      Face         : constant Q.Instance_ID_List := Q.Snapshot_Instances (VA, 0);
      Unknown_Face : constant Q.Instance_ID_List :=
        Q.Snapshot_Instances (VA, Interfaces.Unsigned_32'Last);
      Known        : constant Q.Instance_Status_Report := Q.Snapshot_Instance_Status_Report (VA, 0);
      Unknown      : constant Q.Instance_Status_Report :=
        Q.Snapshot_Instance_Status_Report (VA, Interfaces.Unsigned_32'Last);
   begin
      if Q.Query_ID (VA) /= 0
        or else Q.Cached_Waveform_Supported (VA)
        or else Q.Dynamic_Weights_Supported (VA)
        or else LF.Count (Catalog) /= 0
        or else LF.Count (Statuses) /= 0
        or else Q.Query_Status (VA) /= Q.Operational
        or else Q.Query_Instance_Status (VA, 0) /= Q.Operational
        or else Q.Query_Instance_Status (VA, Interfaces.Unsigned_32'Last) /= Q.Failed
        or else Q.Count (All_IDs) /= 1
        or else Q.Instance_ID_At (All_IDs, 1) /= 0
        or else Q.Count (Face) /= 1
        or else Q.Instance_ID_At (Face, 1) /= 0
        or else Q.Count (Unknown_Face) /= 0
        or else Q.Instance_ID (Known) /= 0
        or else Q.Status (Known) /= Q.Operational
        or else Q.Local_Function_Type_Count (Known) /= 0
        or else Q.Instance_ID (Unknown) /= Interfaces.Unsigned_32'Last
        or else Q.Status (Unknown) /= Q.Failed
        or else Q.Local_Function_Type_Count (Unknown) /= 0
      then
         raise Program_Error with "Squall live VA query values mismatch";
      end if;
   end Queries;
begin
   if Ada.Command_Line.Argument_Count /= 2 then
      raise Program_Error with "usage: ams_mel_squall_rf_va PROVIDER PROFILE";
   end if;
   declare
      Provider : constant String := Ada.Command_Line.Argument (1);
      Profile  : constant String := Ada.Command_Line.Argument (2);
      Control  : Admin.Admin_MEL := Admin.Open (Provider, Profile);
   begin
      if not Admin.Command_State (Control, Status.Standby)
        or else not Admin.Command_State (Control, Status.Operate_Rx_Only)
      then
         raise Program_Error with "Squall Admin rejected state";
      end if;
      declare
         Parent  : C2.C2_MEL := C2.Open (Provider, Profile);
         Config  : constant C2.Virtual_Aperture_Config :=
           C2.Create_Virtual_Aperture_Config (0, 1, "");
         Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, Config);
      begin
         if C2.Outcome (C2.Wait (Request, 10_000)) /= C2.Created then
            raise Program_Error with "Squall VA was not created";
         end if;
         declare
            VA           : C2.Virtual_Aperture := C2.Claim (Request);
            Subscription : N.Subscription := N.Open (VA);
            Basic : constant E.Element_Group_List := E.Snapshot_Element_Groups (VA);
            Full : constant E.Element_Group_List := E.Snapshot_Element_Groups (VA, True);
            Catalog : constant LF.Local_Function_List := LF.Snapshot_Local_Functions (VA);
            Statuses : constant LF.Status_List := LF.Snapshot_Local_Function_Status (VA, 0, 0);
            Pipes : constant P.Connection_Snapshot := P.Snapshot (VA);
         begin
            C2.Close (Request);
            Queries (VA);
            Descriptors (Basic, False);
            Connections (Pipes);
            Associations (VA);
            Descriptors (Full, True);
            begin
               N.Wait_For_Change (Subscription, 0);
               raise Program_Error with "Squall unexpectedly emitted VA notification";
            exception
               when C2.Timeout_Error =>
                  null;
            end;
            if C2.VA_Instance_ID_Count (VA) = 0
              or else C2.VA_Instance_ID_At (VA, 1) /= 0
              or else C2.Element_Group_Label_Count (VA) = 0
              or else C2.Element_Group_Label_At (VA, 1) /= "0"
              or else not C2.Is_Single_Group (VA)
            then
               raise Program_Error with "Squall VA snapshot mismatch";
            end if;
            C2.Close (Parent);
            Associations (VA);
            Queries (VA);
            Descriptors (E.Snapshot_Element_Groups (VA), False);
            Descriptors (E.Snapshot_Element_Groups (VA, True), True);
            N.Unsubscribe (VA, Subscription);
            if not N.Is_Open (Subscription) or else not N.Statistics (Subscription).Stopped then
               raise Program_Error with "Squall VA unsubscribe did not stop observer";
            end if;
            begin
               N.Wait_For_Change (Subscription, 0);
               raise Program_Error with "Squall stopped VA notification delivered";
            exception
               when N.Subscription_Stopped =>
                  null;
            end;
            N.Close (Subscription);
            if C2.VA_Instance_ID_At (VA, 1) /= 0
              or else C2.Element_Group_Label_At (VA, 1) /= "0"
              or else not C2.Is_Single_Group (VA)
            then
               raise Program_Error with "Squall VA invalid after parent Close";
            end if;
            declare
               IDs    : constant Q.Instance_ID_List := Q.Snapshot_All_Instances (VA);
               Report : constant Q.Instance_Status_Report :=
                 Q.Snapshot_Instance_Status_Report (VA, Interfaces.Unsigned_32'Last);
            begin
               C2.Close (VA);
               Connections (Pipes);
               if LF.Count (Catalog) /= 0 or else LF.Count (Statuses) /= 0 then
                  raise Program_Error with "copied Squall LF values changed after VA Close";
               end if;
               Descriptors (Basic, False);
               Descriptors (Full, True);
               if Q.Count (IDs) /= 1
                 or else Q.Instance_ID_At (IDs, 1) /= 0
                 or else Q.Instance_ID (Report) /= Interfaces.Unsigned_32'Last
                 or else Q.Status (Report) /= Q.Failed
                 or else Q.Local_Function_Type_Count (Report) /= 0
               then
                  raise Program_Error with "Squall Ada query values invalid after VA Close";
               end if;
            end;
         end;
      end;
      Admin.Close (Control);
   end;
   Ada.Text_IO.Put_Line
      ("PASS: safe Ada Squall VA and fresh E5 connection values + mutation returns; no persistence/RDMA/routing/hardware claim");
end AMS_MEL_Squall_RF_VA;
