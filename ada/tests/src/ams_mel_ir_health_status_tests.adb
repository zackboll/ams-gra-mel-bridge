with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Health_Status;
with AMS.MEL.IR.Health_Status.Metadata;
with AMS.MEL.Status;
with Interfaces;

package body AMS_MEL_IR_Health_Status_Tests is
   package H renames AMS.MEL.IR.Health_Status;
   package M renames AMS.MEL.IR.Health_Status.Metadata;
   package S renames AMS.MEL.Status;
   package V renames AMS.MEL.IR.Channel;
   use type AMS.MEL.IR.Counter;
   use type M.Metadata_Kind;
   use type M.Failure_Level;
   use type M.CSCI_Mode;
   use type S.MFA_State;
   use type S.Component_State;
   use type S.Temperature_State;
   use type S.Security_Event_Kind;
   use type S.Security_Outcome;
   use type S.Security_Severity;
   use type V.Channel_Type;
   use type Interfaces.Unsigned_32;

   Zero : constant AMS.MEL.IR.UUID := [others => 0];
   Config : constant H.Health_Config := H.Create_Config
     (AMS.MEL.IR.Create_UCI_ID (Zero, "health channel"),
      AMS.MEL.IR.Create_UCI_ID (Zero, "health platform"),
      AMS.MEL.IR.Create_Component_Location (1.25, -2.5, 3.75, "station", "mock"));

   procedure Check_Rich (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "health-rich");
      Channel : H.Health_Channel := H.Open (Parent, Config);
      Stream : M.Metadata_Stream := M.Open (Channel, 32);
   begin
      H.Enable (Channel);
      declare Capability : constant V.Channel_Capability := H.Capabilities (Channel); begin
         if V.Channel_Type_Count (Capability) /= 1
           or else V.Channel_Type_At (Capability, 1) /= V.Health_And_Status
           or else not V.Has_Metadata_Capability (Capability, V.MFA_Status)
           or else not V.Has_Metadata_Capability (Capability, V.BIT_Status)
           or else not V.Has_Metadata_Capability (Capability, V.Subsystem_Status_Resp)
         then raise Program_Error with "Health capability mismatch"; end if;
      end;
      declare
         Event : constant M.Metadata_Event := M.Receive (Stream, 1_000);
         Value : constant S.MFA_Status := M.MFA_Status_Value (Event);
         Component : constant S.MFA_Component := S.Component_At (Value, 1);
         Details : constant S.Installation_Details := S.Installation (Component);
      begin
         if M.Kind (Event) /= M.MFA_Status_Event
           or else S.State (Value) /= S.Maintenance
           or else S.State_Description (Value) /= "healthy-α"
           or else S.Mode_Description (Value) /= "mode-€"
           or else S.Component_Count (Value) /= 2
           or else S.State (Component) /= S.Operational
           or else S.Temperature_C (Component) /= 42.25
           or else S.Temperature_Status (Component) /= S.Normal
           or else S.Key (S.Installation_Location_ID (Component)) /= "rack-β"
           or else S.Orientation (Details).Roll /= 0.1
           or else S.Boresight (Details).Yaw /= -0.6
         then raise Program_Error with "rich MFA_Status mismatch"; end if;
      end;
      declare
         Event : constant M.Metadata_Event := M.Receive (Stream, 1_000);
         Value : constant S.BIT_Status := M.BIT_Status_Value (Event);
         Completed : constant S.Completed_BIT := S.Completed_BIT_At (Value, 1);
         Fault : constant S.Fault := S.Fault_At (Value, 1);
      begin
         if M.Kind (Event) /= M.BIT_Status_Event
           or else S.Active_BIT_Count (Value) /= 3
           or else S.Completed_BIT_Count (Value) /= 2
           or else S.Fault_Count (Value) /= 1
           or else S.Estimated_Completion_Time_NS (S.Active_BIT_At (Value, 1)) /= -5
           or else S.BIT_Item_Name (S.BIT_Item_At (Completed, 2)) /= "item-γ"
           or else S.Ambiguity_Group_Count (Fault) /= 2
         then raise Program_Error with "rich BIT_Status mismatch"; end if;
      end;
      declare
         Event : constant M.Metadata_Event := M.Receive (Stream, 1_000);
         Value : constant M.Subsystem_Status := M.Subsystem_Status_Value (Event);
         CSCI : constant M.Subsystem_CSCI := M.CSCI_At (Value, 1);
      begin
         if M.Kind (Event) /= M.Subsystem_Status_Event
           or else M.Subsystem_ID (Value) /= 16#8000_0001#
           or else M.Status_Sequence_Number (Value) /= 16#F000_0002#
           or else M.Failure (Value) /= M.Major
           or else M.Reported_Subsystem_Count (Value) /= 99
           or else M.Subsystem_Count (Value) /= 2
           or else M.Reported_CSCI_Count (Value) /= 88
           or else M.CSCI_Count (Value) /= 2
           or else M.CSCI_Name (CSCI) /= "flight-δ"
           or else M.Mode (CSCI) /= M.Operational
           or else M.Version_Value (CSCI).Engineering_Revision /= 4
           or else not M.Connection_Established (CSCI)
         then raise Program_Error with "rich SubsystemStatusResp mismatch"; end if;
      end;
      declare
         Event : constant M.Metadata_Event := M.Receive (Stream, 1_000);
         Value : constant S.Discrete_Status := M.Discrete_Status_Value (Event);
      begin
         if S.Pair_Count (Value) /= 3
           or else S.Name (S.Pair_At (Value, 1)) /= "duplicate"
           or else S.Name (S.Pair_At (Value, 2)) /= "duplicate"
           or else S.Pair_Value (S.Pair_At (Value, 2)) /= ""
           or else S.Pair_Value (S.Pair_At (Value, 3)) /= "€"
         then raise Program_Error with "rich DiscreteStatus mismatch"; end if;
      end;
      for Expected in S.Security_Event_Kind loop
         declare
            Event : constant M.Metadata_Event := M.Receive (Stream, 1_000);
            Value : constant S.Security_Audit_Record := M.Security_Audit_Value (Event);
            Detail : constant S.Security_Event := S.Event (Value);
         begin
            if S.Kind (Detail) /= Expected or else S.Timestamp_NS (Value) /= -987_654_321
              or else S.Artifact_Count (Value) /= 2
              or else S.Outcome (Value) /= S.Failure
              or else S.Severity (Value) /= S.Warning
              or else (Expected = S.Authentication and then
                (S.Category (Detail) /= 5 or else S.Details (Detail) /= "auth-ζ"
                 or else AMS.MEL.IR.Descriptive_Label (S.Service_ID (Detail)) /= "auth service"))
            then raise Program_Error with "rich SecurityAudit variant mismatch"; end if;
         end;
      end loop;
      declare
         Event : constant M.Metadata_Event := M.Receive (Stream, 1_000);
         Value : constant S.MFA_Status_Detailed := M.MFA_Status_Detailed_Value (Event);
      begin
         AMS.MEL.Close (Parent); H.Close (Channel);
         if S.Pair_Count (Value) /= 3
           or else S.Name (S.Pair_At (Value, 1)) /= "load"
           or else S.Pair_Value (S.Pair_At (Value, 3)) /= "μ"
         then raise Program_Error with "Ada-owned MFA_StatusDetailed mismatch"; end if;
      end;
      begin
         declare Unexpected : constant M.Metadata_Event := M.Receive (Stream); begin
            raise Program_Error with M.Metadata_Kind'Image (M.Kind (Unexpected));
         end;
      exception when AMS.MEL.IR.Stream_Stopped => null; end;
      M.Close (Stream);
   end Check_Rich;

   procedure Check_Overflow_And_Close (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "health-overflow");
      Channel : H.Health_Channel := H.Open (Parent, Config);
      Stream : M.Metadata_Stream := M.Open (Channel, 2);
      Counts : constant M.Metadata_Counters := M.Counters (Stream);
   begin
      if Counts.Events_Received /= 12 or else Counts.Events_Dropped_Queue_Full /= 10
        or else Counts.Malformed_Or_Unsupported /= 0
      then raise Program_Error with "Health DROP-INCOMING counters mismatch"; end if;
      if M.Kind (M.Receive (Stream)) /= M.MFA_Status_Event
        or else M.Kind (M.Receive (Stream)) /= M.BIT_Status_Event
      then raise Program_Error with "Health DROP-INCOMING order mismatch"; end if;
      M.Close (Stream);
      H.Enable (Channel);
      H.Close (Channel);
      AMS.MEL.Close (Parent);
   end Check_Overflow_And_Close;

   procedure Run (Provider_Path : String) is
   begin
      Check_Rich (Provider_Path);
      Check_Overflow_And_Close (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada IR Health/Status contract");
   end Run;
end AMS_MEL_IR_Health_Status_Tests;
