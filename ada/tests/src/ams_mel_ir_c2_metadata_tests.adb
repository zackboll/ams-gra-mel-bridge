with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.C2.Metadata;

package body AMS_MEL_IR_C2_Metadata_Tests is
   package C2 renames AMS.MEL.IR.C2;
   package M renames AMS.MEL.IR.C2.Metadata;
   use type AMS.MEL.IR.Counter;
   use type AMS.MEL.IR.Byte;
   use type M.Metadata_Kind;
   use type M.BIT_Control_Interface;
   use type M.BIT_Result;
   use type M.Fault_Severity;
   use type M.Fault_State;
   use type C2.Command_ID;
   use type C2.Outcome;
   use type M.Command_State;
   use type M.Cannot_Comply;

   Zero : constant AMS.MEL.IR.UUID := [others => 0];
   Config : constant C2.Control_Config := C2.Create_Config
     (AMS.MEL.IR.Create_UCI_ID (Zero, "metadata channel"),
      AMS.MEL.IR.Create_UCI_ID (Zero, "metadata platform"),
      AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock"));

   function Long_Description return String is
      Value : String (1 .. 613) := [others => 'x'];
   begin
      Value (511) := Character'Val (16#E2#);
      Value (512) := Character'Val (16#82#);
      Value (513) := Character'Val (16#AC#);
      Value (514 .. Value'Last) := [others => 'y'];
      return Value;
   end Long_Description;

   procedure Check_Rich (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "metadata-rich");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
      Stream : M.Metadata_Stream := M.Open (Channel, 8);
   begin
      AMS.MEL.Close (Parent); C2.Close (Channel);
      declare
         Configuration : constant M.Metadata_Event := M.Receive (Stream, 1_000);
         First : constant M.BIT_Type := M.BIT_Type_At (Configuration, 1);
         Second : constant M.BIT_Type := M.BIT_Type_At (Configuration, 2);
         Status : constant M.Metadata_Event := M.Receive (Stream, 1_000);
         Active : constant M.Active_BIT := M.Active_BIT_At (Status, 1);
         Active_Zero : constant M.Active_BIT := M.Active_BIT_At (Status, 2);
         Active_Positive : constant M.Active_BIT := M.Active_BIT_At (Status, 3);
         Completed : constant M.Completed_BIT := M.Completed_BIT_At (Status, 1);
         Completed_Fail : constant M.Completed_BIT := M.Completed_BIT_At (Status, 2);
         Fault : constant M.Fault := M.Fault_At (Status, 1);
         Group : constant M.Fault_Ambiguity_Group := M.Ambiguity_Group_At (Fault, 1);
         Group_Two : constant M.Fault_Ambiguity_Group := M.Ambiguity_Group_At (Fault, 2);
      begin
         if M.Kind (Configuration) /= M.BIT_Configuration_Event
           or else M.BIT_Type_Count (Configuration) /= 2
           or else M.Accepted_Interface (First) /= M.Subsystem_BIT_Command
           or else M.Expected_Duration_NS (First) /= 123_456_789
           or else M.BIT_Item_Name_Count (First) /= 2
           or else M.BIT_Item_Name_At (First, 1) /= "sensor"
           or else M.BIT_Item_Name_At (First, 2) /= "optical-€"
           or else M.Subsystem_Component_Count (First) /= 2
           or else AMS.MEL.IR.Descriptive_Label (M.Subsystem_Component_At (First, 1)) /= "component one"
           or else AMS.MEL.IR.Descriptive_Label (M.Subsystem_Component_At (First, 2)) /= "component-β"
           or else AMS.MEL.IR.UUID_Value (M.BIT_ID (First)) (0) /= 16#80#
           or else M.Accepted_Interface (Second) /= M.Subsystem_Initiated
           or else M.Expected_Duration_NS (Second) /= 0
           or else M.BIT_Item_Name_Count (Second) /= 0
           or else M.Subsystem_Component_Count (Second) /= 0
         then raise Program_Error with "rich BIT configuration mismatch"; end if;
         if M.Kind (Status) /= M.BIT_Status_Event
           or else M.Active_BIT_Count (Status) /= 3
           or else M.Estimated_Completion_Time_NS (Active) /= -5
           or else M.Estimated_Percent_Complete (Active) /= 1.25
           or else AMS.MEL.IR.Descriptive_Label (M.BIT_ID (Active)) /= "active negative"
           or else M.Estimated_Completion_Time_NS (Active_Zero) /= 0
           or else M.Estimated_Percent_Complete (Active_Zero) /= 0.0
           or else M.Estimated_Completion_Time_NS (Active_Positive) /= 987_654_321
           or else M.Estimated_Percent_Complete (Active_Positive) /= 0.5
           or else M.Completed_BIT_Count (Status) /= 2
           or else M.Result (Completed) /= M.Pass
           or else M.Time_Tag_NS (Completed) /= 42
           or else M.Fail_Reason (Completed) /= ""
           or else M.BIT_Item_Count (Completed) /= 2
           or else M.BIT_Item_Name (M.BIT_Item_At (Completed, 1)) /= "item pass"
           or else M.Result (M.BIT_Item_At (Completed, 1)) /= M.Pass
           or else M.Fail_Reason (M.BIT_Item_At (Completed, 1)) /= ""
           or else M.BIT_Item_Name (M.BIT_Item_At (Completed, 2)) /= "item-γ"
           or else M.Result (M.BIT_Item_At (Completed, 2)) /= M.Fail
           or else M.Fail_Reason (M.BIT_Item_At (Completed, 2)) /= "item reason"
           or else M.Time_Tag_NS (Completed_Fail) /= -99
           or else M.Result (Completed_Fail) /= M.Fail
           or else M.Fail_Reason (Completed_Fail) /= "failure-€"
           or else M.BIT_Item_Count (Completed_Fail) /= 0
           or else M.Fault_Count (Status) /= 1
           or else AMS.MEL.IR.Descriptive_Label (M.Fault_ID (Fault)) /= "fault-ζ"
           or else M.Severity (Fault) /= M.Warning
           or else M.State (Fault) /= M.Set
           or else M.Detection_Time_NS (Fault) /= -1_234_567
           or else M.Fault_Code (Fault) /= "F-42"
           or else M.Fault_Description (Fault) /= "overheat-€"
           or else M.Fault_Data_Count (Fault) /= 2
           or else M.Key (M.Fault_Data_At (Fault, 1)) /= "temperature"
           or else M.Data_Value (M.Fault_Data_At (Fault, 1)) /= "101"
           or else M.Format (M.Fault_Data_At (Fault, 1)) /= "integer"
           or else M.Units (M.Fault_Data_At (Fault, 1)) /= "°C"
           or else M.Key (M.Fault_Data_At (Fault, 2)) /= "phase-δ"
           or else M.Data_Value (M.Fault_Data_At (Fault, 2)) /= "bad"
           or else M.Format (M.Fault_Data_At (Fault, 2)) /= "text"
           or else M.Units (M.Fault_Data_At (Fault, 2)) /= ""
           or else M.Component_Count (Fault) /= 2
           or else AMS.MEL.IR.Descriptive_Label (M.Component_At (Fault, 1)) /= "fault component one"
           or else AMS.MEL.IR.Descriptive_Label (M.Component_At (Fault, 2)) /= "fault component two"
           or else M.Ambiguity_Group_Count (Fault) /= 2
           or else M.Diagnostic_Test_Count (Group) /= 2
           or else M.Component_Count (Group) /= 2
           or else AMS.MEL.IR.Descriptive_Label (M.Diagnostic_Test_At (Group, 1)) /= "diagnostic one"
           or else AMS.MEL.IR.Descriptive_Label (M.Diagnostic_Test_At (Group, 2)) /= "diagnostic two"
           or else AMS.MEL.IR.Descriptive_Label (M.Component_At (Group, 1)) /= "ambiguous one"
           or else AMS.MEL.IR.Descriptive_Label (M.Component_At (Group, 2)) /= "ambiguous two"
           or else M.Diagnostic_Test_Count (Group_Two) /= 1
           or else M.Component_Count (Group_Two) /= 1
           or else AMS.MEL.IR.Descriptive_Label (M.Diagnostic_Test_At (Group_Two, 1)) /= "diagnostic three"
           or else AMS.MEL.IR.Descriptive_Label (M.Component_At (Group_Two, 1)) /= "ambiguous three"
         then raise Program_Error with "rich BIT status mismatch"; end if;
         begin
            declare
               Unexpected : constant M.Metadata_Event := M.Receive (Stream);
            begin
               raise Program_Error with M.Metadata_Kind'Image (M.Kind (Unexpected));
            end;
         exception
            when AMS.MEL.IR.Stream_Stopped => null;
         end;
         M.Close (Stream);
         --  Status is already wholly Ada-owned after native/provider teardown.
         if M.Fault_Description (M.Fault_At (Status, 1)) /= "overheat-€" then
            raise Program_Error with "Ada event did not outlive provider";
         end if;
      end;
   end Check_Rich;

   procedure Check_Overflow (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "metadata-overflow");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
      Stream : M.Metadata_Stream := M.Open (Channel, 2);
      Counts : constant M.Metadata_Counters := M.Counters (Stream);
      First : constant M.Metadata_Event := M.Receive (Stream);
      Second : constant M.Metadata_Event := M.Receive (Stream);
   begin
      if Counts.Events_Received /= 3 or else Counts.Events_Dropped_Queue_Full /= 1
        or else M.Kind (First) /= M.BIT_Configuration_Event
        or else M.Kind (Second) /= M.Command_Status_Event
        or else M.Command_ID (M.Command (Second)) /= 1
      then raise Program_Error with "metadata DROP-INCOMING mismatch"; end if;
      begin declare Unexpected : constant M.Metadata_Event := M.Receive (Stream); begin raise Program_Error with M.Metadata_Kind'Image (M.Kind (Unexpected)); end;
      exception when AMS.MEL.IR.Timeout_Error => null; end;
      M.Close (Stream); C2.Close (Channel); AMS.MEL.Close (Parent);
   end Check_Overflow;

   procedure Check_Malformed_And_Failure (Provider_Path : String) is
   begin
      declare Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "metadata-malformed"); Channel : C2.Control_Channel := C2.Open (Parent, Config); Stream : M.Metadata_Stream := M.Open (Channel, 4); Counts : constant M.Metadata_Counters := M.Counters (Stream); Valid : constant M.Metadata_Event := M.Receive (Stream); begin
         if Counts.Events_Received /= 6 or else Counts.Malformed_Or_Unsupported /= 4
           or else M.Command_ID (M.Command (Valid)) /= 5 then raise Program_Error with "malformed recovery mismatch"; end if;
         M.Close (Stream); C2.Close (Channel); AMS.MEL.Close (Parent);
      end;
      declare Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "metadata-register-fail"); Channel : C2.Control_Channel := C2.Open (Parent, Config); begin
         begin declare Stream : M.Metadata_Stream := M.Open (Channel, 2); begin M.Close (Stream); raise Program_Error with "partial registration succeeded"; end;
         exception when AMS.MEL.Provider_Error => null; end;
         C2.Close (Channel); AMS.MEL.Close (Parent);
      end;
   end Check_Malformed_And_Failure;

   procedure Check_Command_Status_And_Close (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open
        (Provider_Path, "metadata-command-status");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
      Stream : M.Metadata_Stream := M.Open (Channel, 4);
   begin
      C2.Enable (Channel);
      declare
         Request : C2.Mode_Request :=
           C2.Submit_Operate (Channel, 16#8000_0001#);
         Result : constant C2.Mode_Result := C2.Wait (Request, 1_000);
         Event : constant M.Metadata_Event := M.Receive (Stream, 1_000);
         Status : constant M.Command_Status := M.Command (Event);
      begin
         if C2.Status (Result) /= C2.Success
           or else M.Kind (Event) /= M.Command_Status_Event
           or else M.Command_ID (Status) /= 16#8000_0001#
           or else M.State (Status) /= M.Rejected
           or else M.Reason (Status) /= M.Invalid_Input_Parameter
           or else M.Reason_Description (Status) /= Long_Description
         then
            raise Program_Error with "Ada CommandStatus fidelity mismatch";
         end if;
         C2.Close (Request);
      end;
      M.Close (Stream);
      declare
         Request : C2.Mode_Request := C2.Submit_Operate (Channel, 9);
      begin
         if C2.Status (C2.Wait (Request, 1_000)) /= C2.Success then
            raise Program_Error with "command after metadata close failed";
         end if;
         C2.Close (Request);
      end;
      C2.Close (Channel); AMS.MEL.Close (Parent);
   end Check_Command_Status_And_Close;

   procedure Run (Provider_Path : String) is
   begin
      Check_Rich (Provider_Path); Check_Overflow (Provider_Path);
      Check_Malformed_And_Failure (Provider_Path);
      Check_Command_Status_And_Close (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada required IR C2 metadata contract");
   end Run;
end AMS_MEL_IR_C2_Metadata_Tests;
