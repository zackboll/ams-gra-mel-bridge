with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;
with System.Storage_Elements;

package body AMS.MEL.IR.Health_Status.Metadata is
   package C renames AMS.MEL_C_API;
   package S renames AMS.MEL.Status;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;
   use type C.Health_Handle;
   use type C.Health_Metadata_Handle;
   use type C.Health_Event_Handle;
   use System.Storage_Elements;

   type Diagnostic is array (C.Size_T range <>) of aliased Interfaces.C.char
      with Convention => C;
   subtype Fixed_Diagnostic is Diagnostic (0 .. 511);
   type Event_Access is access all C.IR_Health_Event_V1;
   type Char_Access is access all Interfaces.C.char;
   type ID_Access is access all C.UCI_ID_V1;
   type Active_Access is access all C.Active_BIT_V1;
   type Completed_Access is access all C.Completed_BIT_V1;
   type Item_Access is access all C.Completed_BIT_Item_V1;
   type Fault_Access is access all C.Fault_V1;
   type Data_Access is access all C.Fault_Data_V1;
   type Group_Access is access all C.Fault_Ambiguity_Group_V1;
   type Component_Access is access all C.MFA_Component_V1;
   type Dependency_Access is access all C.IR_Subsystem_Dep_Info_V1;
   type CSCI_Access is access all C.IR_Subsystem_CSCI_Info_V1;
   type Pair_Access is access all C.Name_Value_Pair_V1;
   type Artifact_Access is access all C.Security_Artifact_V1;
   function To_Event is new Ada.Unchecked_Conversion (System.Address, Event_Access);
   function To_Char is new Ada.Unchecked_Conversion (System.Address, Char_Access);
   function To_ID is new Ada.Unchecked_Conversion (System.Address, ID_Access);
   function To_Active is new Ada.Unchecked_Conversion (System.Address, Active_Access);
   function To_Completed is new Ada.Unchecked_Conversion (System.Address, Completed_Access);
   function To_Item is new Ada.Unchecked_Conversion (System.Address, Item_Access);
   function To_Fault is new Ada.Unchecked_Conversion (System.Address, Fault_Access);
   function To_Data is new Ada.Unchecked_Conversion (System.Address, Data_Access);
   function To_Group is new Ada.Unchecked_Conversion (System.Address, Group_Access);
   function To_Component is new Ada.Unchecked_Conversion (System.Address, Component_Access);
   function To_Dependency is new Ada.Unchecked_Conversion (System.Address, Dependency_Access);
   function To_CSCI is new Ada.Unchecked_Conversion (System.Address, CSCI_Access);
   function To_Pair is new Ada.Unchecked_Conversion (System.Address, Pair_Access);
   function To_Artifact is new Ada.Unchecked_Conversion (System.Address, Artifact_Access);

   function Address_At (Base : System.Address; Index, Bytes : Natural)
      return System.Address is (Base + Storage_Offset (Index * Bytes));
   function Message (Value : Diagnostic) return String is
      Last : Natural := 0;
   begin
      while Last < Value'Length and then Value (C.Size_T (Last)) /= Interfaces.C.nul loop
         Last := Last + 1;
      end loop;
      declare Result : String (1 .. Last); begin
         for I in Result'Range loop
            Result (I) := Character'Val (Interfaces.C.char'Pos
              (Value (C.Size_T (I - 1))));
         end loop;
         return (if Result'Length = 0 then "native IR Health metadata operation failed" else Result);
      end;
   end Message;
   function Copy (Value : C.String_View_V1) return String is
   begin
      if Value.Size = 0 then return ""; end if;
      declare Result : String (1 .. Natural (Value.Size)); begin
         for I in Result'Range loop
            Result (I) := Character'Val (Interfaces.C.char'Pos
              (To_Char (Address_At (Value.Data, I - 1, 1)).all));
         end loop;
         return Result;
      end;
   end Copy;
   function Copy (Value : C.UCI_ID_V1) return UCI_ID is
      Bytes : UUID;
   begin
      for I in Bytes'Range loop Bytes (I) := Value.UUID (I); end loop;
      return Create_UCI_ID (Bytes, Copy (Value.Descriptive_Label));
   end Copy;
   function Copy_IDs (Span : C.Span_V1) return S.UCI_ID_Vectors.Vector is
      Result : S.UCI_ID_Vectors.Vector;
   begin
      if Span.Size > 0 then
         for I in 0 .. Natural (Span.Size) - 1 loop
            Result.Append (Copy (To_ID (Address_At
              (Span.Data, I, C.UCI_ID_V1'Object_Size / System.Storage_Unit)).all));
         end loop;
      end if;
      return Result;
   end Copy_IDs;

   function Copy_BIT (Raw : C.BIT_Status_V1) return S.BIT_Status is
      Active : S.Active_BIT_List;
      Completed : S.Completed_BIT_List;
      Faults : S.Fault_List;
   begin
      if Raw.Active_BITS.Size > 0 then
         for I in 0 .. Natural (Raw.Active_BITS.Size) - 1 loop
            declare V : constant C.Active_BIT_V1 := To_Active (Address_At
              (Raw.Active_BITS.Data, I, C.Active_BIT_V1'Object_Size / System.Storage_Unit)).all;
            begin S.Append (Active, S.Create_Active_BIT (Copy (V.BIT_ID),
               Long_Long_Integer (V.Estimated_Completion_Time_NS),
               Long_Float (V.Estimated_Percent_Complete))); end;
         end loop;
      end if;
      if Raw.Completed_BITS.Size > 0 then
         for I in 0 .. Natural (Raw.Completed_BITS.Size) - 1 loop
            declare V : constant C.Completed_BIT_V1 := To_Completed (Address_At
              (Raw.Completed_BITS.Data, I, C.Completed_BIT_V1'Object_Size / System.Storage_Unit)).all;
               Items : S.Completed_BIT_Item_List;
            begin
               if V.Result > 4 then raise Provider_Error with "invalid native BIT result"; end if;
               if V.BIT_Items.Size > 0 then
                  for J in 0 .. Natural (V.BIT_Items.Size) - 1 loop
                     declare X : constant C.Completed_BIT_Item_V1 := To_Item (Address_At
                       (V.BIT_Items.Data, J, C.Completed_BIT_Item_V1'Object_Size / System.Storage_Unit)).all;
                     begin
                        if X.Result > 4 then raise Provider_Error with "invalid native BIT item result"; end if;
                        S.Append (Items, S.Create_Completed_BIT_Item (Copy (X.BIT_Item_Name),
                           S.BIT_Result'Val (X.Result), Copy (X.Fail_Reason)));
                     end;
                  end loop;
               end if;
               S.Append (Completed, S.Create_Completed_BIT (Copy (V.BIT_ID),
                  Long_Long_Integer (V.Time_Tag_NS), S.BIT_Result'Val (V.Result),
                  Copy (V.Fail_Reason), Items));
            end;
         end loop;
      end if;
      if Raw.Faults.Size > 0 then
         for I in 0 .. Natural (Raw.Faults.Size) - 1 loop
            declare V : constant C.Fault_V1 := To_Fault (Address_At
              (Raw.Faults.Data, I, C.Fault_V1'Object_Size / System.Storage_Unit)).all;
               Data : S.Fault_Data_List; Groups : S.Fault_Ambiguity_Group_List;
            begin
               if V.Severity > 4 or else V.State > 3 then
                  raise Provider_Error with "invalid native fault enum";
               end if;
               if V.Fault_Data.Size > 0 then
                  for J in 0 .. Natural (V.Fault_Data.Size) - 1 loop
                     declare X : constant C.Fault_Data_V1 := To_Data (Address_At
                       (V.Fault_Data.Data, J, C.Fault_Data_V1'Object_Size / System.Storage_Unit)).all;
                     begin S.Append (Data, S.Create_Fault_Data (Copy (X.Key), Copy (X.Value),
                        Copy (X.Format), Copy (X.Units))); end;
                  end loop;
               end if;
               if V.Ambiguity_Groups.Size > 0 then
                  for J in 0 .. Natural (V.Ambiguity_Groups.Size) - 1 loop
                     declare X : constant C.Fault_Ambiguity_Group_V1 := To_Group (Address_At
                       (V.Ambiguity_Groups.Data, J, C.Fault_Ambiguity_Group_V1'Object_Size / System.Storage_Unit)).all;
                     begin S.Append (Groups, S.Create_Ambiguity_Group
                       (Copy_IDs (X.Diagnostic_Test_IDs), Copy_IDs (X.Component_IDs))); end;
                  end loop;
               end if;
               S.Append (Faults, S.Create_Fault (Copy (V.Fault_ID),
                  S.Fault_Severity'Val (V.Severity), S.Fault_State'Val (V.State),
                  Long_Long_Integer (V.Detection_Time_NS), Copy (V.Fault_Code),
                  Copy (V.Fault_Description), Data, Copy_IDs (V.Component_IDs), Groups));
            end;
         end loop;
      end if;
      return S.Create_BIT_Status (Active, Completed, Faults);
   end Copy_BIT;

   function Copy_MFA (Raw : C.MFA_Status_V1) return S.MFA_Status is
      Components : S.MFA_Component_List;
   begin
      if Raw.State > 14 or else Raw.Transition_Status > 3 then
         raise Provider_Error with "invalid native MFA status enum";
      end if;
      if Raw.Components.Size > 0 then
         for I in 0 .. Natural (Raw.Components.Size) - 1 loop
            declare V : constant C.MFA_Component_V1 := To_Component (Address_At
              (Raw.Components.Data, I, C.MFA_Component_V1'Object_Size / System.Storage_Unit)).all;
               L : constant C.Component_Location_V1 := V.Installation_Details.Location;
            begin
               if V.State > 8 or else V.Temperature.State > 5 then
                  raise Provider_Error with "invalid native MFA component enum";
               end if;
               S.Append (Components, S.Create_MFA_Component (Copy (V.Component_ID),
                  S.Component_State'Val (V.State), Long_Float (V.Temperature.Temperature_C),
                  S.Temperature_State'Val (V.Temperature.State),
                  S.Create_Foreign_Key (Copy (V.Installation_Location_ID.Key),
                     Copy (V.Installation_Location_ID.System_Name)),
                  S.Create_Installation_Details
                    (Create_Component_Location (Long_Float (L.Offset_X_M),
                       Long_Float (L.Offset_Y_M), Long_Float (L.Offset_Z_M),
                       Copy (L.Key), Copy (L.System_Name)),
                     (Long_Float (V.Installation_Details.Orientation.Roll),
                      Long_Float (V.Installation_Details.Orientation.Pitch),
                      Long_Float (V.Installation_Details.Orientation.Yaw)),
                     (Long_Float (V.Installation_Details.Boresight.Roll),
                      Long_Float (V.Installation_Details.Boresight.Pitch),
                      Long_Float (V.Installation_Details.Boresight.Yaw)))));
            end;
         end loop;
      end if;
      return S.Create_MFA_Status (S.MFA_State'Val (Raw.State),
         Copy (Raw.State_Description), Copy (Raw.Mode_Description),
         S.State_Transition_Status'Val (Raw.Transition_Status),
         S.Create_About (Copy (Raw.About_Data.Model), Copy (Raw.About_Data.Serial_Number),
            Copy (Raw.About_Data.Software_Version),
            Copy (Raw.About_Data.Bootloader_Software_Version),
            Copy (Raw.About_Data.Hardware_Version)), Components);
   end Copy_MFA;

   function Copy_Subsystem (Raw : C.IR_Subsystem_Status_V1) return Subsystem_Status is
      Result : Subsystem_Status;
   begin
      if Raw.Failure > 6 then raise Provider_Error with "invalid native subsystem failure"; end if;
      Result := (ID => Raw.Subsystem_ID, Importance => Raw.Criticality,
         Sequence => Raw.Status_Sequence_Number,
         Failure_Value => Failure_Level'Val (Raw.Failure),
         Reported_Subsystems => Raw.Subsystem_Count, Subsystems => <>,
         Reported_CSCI => Raw.CSCI_Count, CSCIs => <>);
      if Raw.Subsystems.Size > 0 then
         for I in 0 .. Natural (Raw.Subsystems.Size) - 1 loop
            declare V : constant C.IR_Subsystem_Dep_Info_V1 := To_Dependency (Address_At
              (Raw.Subsystems.Data, I, C.IR_Subsystem_Dep_Info_V1'Object_Size / System.Storage_Unit)).all;
            begin
               if V.Failure > 6 then raise Provider_Error with "invalid native dependency failure"; end if;
               Result.Subsystems.Append (Subsystem_Dependency'
                 (V.Subsystem_ID, V.Criticality, Failure_Level'Val (V.Failure)));
            end;
         end loop;
      end if;
      if Raw.CSCI.Size > 0 then
         for I in 0 .. Natural (Raw.CSCI.Size) - 1 loop
            declare V : constant C.IR_Subsystem_CSCI_Info_V1 := To_CSCI (Address_At
              (Raw.CSCI.Data, I, C.IR_Subsystem_CSCI_Info_V1'Object_Size / System.Storage_Unit)).all;
            begin
               if V.Mode > 10 or else V.Failure > 6 or else V.Connection_Established > 1 then
                  raise Provider_Error with "invalid native CSCI value";
               end if;
               Result.CSCIs.Append (Subsystem_CSCI'(US.To_Unbounded_String (Copy (V.CSCI)),
                  CSCI_Mode'Val (V.Mode),
                  (V.Version.Source, V.Version.Major_Revision,
                   V.Version.Minor_Revision, V.Version.Engineering_Revision),
                  V.Criticality, Failure_Level'Val (V.Failure), V.BIT_Report,
                  V.Connection_Established = 1));
            end;
         end loop;
      end if;
      return Result;
   end Copy_Subsystem;

   function Copy_Pairs (Raw : C.Span_V1) return S.Name_Value_Pair_List is
      Result : S.Name_Value_Pair_List;
   begin
      if Raw.Size > 0 then
         for I in 0 .. Natural (Raw.Size) - 1 loop
            declare V : constant C.Name_Value_Pair_V1 := To_Pair (Address_At
              (Raw.Data, I, C.Name_Value_Pair_V1'Object_Size / System.Storage_Unit)).all;
            begin S.Append (Result, S.Create_Name_Value_Pair (Copy (V.Name), Copy (V.Value))); end;
         end loop;
      end if;
      return Result;
   end Copy_Pairs;

   function Copy_Security (Raw : C.Security_Audit_Record_V1)
      return S.Security_Audit_Record is
      Artifacts : S.Security_Artifact_List;
   begin
      if Raw.Event.Kind > 6 or else Raw.Outcome > 2 or else Raw.Severity > 4 then
         raise Provider_Error with "invalid native security audit enum";
      end if;
      if Raw.Artifacts.Size > 0 then
         for I in 0 .. Natural (Raw.Artifacts.Size) - 1 loop
            declare V : constant C.Security_Artifact_V1 := To_Artifact (Address_At
              (Raw.Artifacts.Data, I, C.Security_Artifact_V1'Object_Size / System.Storage_Unit)).all;
            begin S.Append (Artifacts, S.Create_Security_Artifact
              (Copy (V.Component_ID), Copy (V.Associated_ID))); end;
         end loop;
      end if;
      return S.Create_Security_Audit_Record (Copy (Raw.Security_Event_ID),
         Long_Long_Integer (Raw.Event_Timestamp_NS), Copy (Raw.Subsystem_ID), Artifacts,
         S.Create_Security_Event (S.Security_Event_Kind'Val (Raw.Event.Kind),
            Natural (Raw.Event.Category), Copy (Raw.Event.Details),
            Copy (Raw.Event.Subsystem_ID), Copy (Raw.Event.Service_ID),
            Copy (Raw.Event.MDF_ID)), S.Security_Outcome'Val (Raw.Outcome),
         S.Security_Severity'Val (Raw.Severity));
   end Copy_Security;

   function Copy_Event (Raw : C.IR_Health_Event_V1) return Metadata_Event is
   begin
      case Raw.Kind is
         when 1 => return (MFA_Status_Event, Copy_MFA (Raw.MFA_Status));
         when 2 => return (BIT_Status_Event, Copy_BIT (Raw.BIT_Status));
         when 3 => return (Subsystem_Status_Event, Copy_Subsystem (Raw.Subsystem_Status));
         when 4 => return (Discrete_Status_Event, Copy_Pairs (Raw.Discrete_Status));
         when 5 => return (Security_Audit_Event, Copy_Security (Raw.Security_Audit));
         when 6 => return (MFA_Status_Detailed_Event, Copy_Pairs (Raw.MFA_Status_Detailed));
         when others => raise Provider_Error with "invalid native Health metadata kind";
      end case;
   end Copy_Event;

   function Open (Channel : Health_Channel; Queue_Capacity : Positive := 16)
      return Metadata_Stream is
      D : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if Channel.Handle = C.Null_Health then raise Provider_Error with "Health channel is closed"; end if;
      return Result : Metadata_Stream do
         if C.IR_Health_Metadata_Open (Channel.Handle, C.Size_T (Queue_Capacity),
            Result.Handle'Access, D'Address, D'Length, Required'Access) /= C.Success then
            raise Provider_Error with Message (D);
         end if;
      end return;
   end Open;
   function Is_Open (Stream : Metadata_Stream) return Boolean is
     (Stream.Handle /= C.Null_Health_Metadata);

   function Receive (Stream : Metadata_Stream; Timeout_Milliseconds : Natural := 0)
      return Metadata_Event is
      Owner : aliased C.Health_Event_Handle := C.Null_Health_Event;
      Address : aliased System.Address := System.Null_Address;
      D : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
      Code : Interfaces.Integer_32;
      procedure Release is Ignored : Interfaces.Integer_32; begin
         Ignored := C.IR_Health_Event_Close (Owner'Access, System.Null_Address, 0, null);
      end Release;
   begin
      Code := C.IR_Health_Metadata_Receive (Stream.Handle,
         Interfaces.Unsigned_32 (Timeout_Milliseconds), Owner'Access,
         D'Address, D'Length, Required'Access);
      if Code = C.Timeout then raise Timeout_Error;
      elsif Code = C.Stream_Stopped then raise Stream_Stopped;
      elsif Code /= C.Success then raise Provider_Error with Message (D);
      end if;
      if C.IR_Health_Event_View (Owner, Address'Access, D'Address, D'Length,
         Required'Access) /= C.Success then Release; raise Provider_Error with Message (D); end if;
      declare Result : constant Metadata_Event := Copy_Event (To_Event (Address).all);
      begin Release; return Result; end;
   exception
      when others => if Owner /= C.Null_Health_Event then Release; end if; raise;
   end Receive;

   function Counters (Stream : Metadata_Stream) return Metadata_Counters is
      Raw : aliased C.Metadata_Counters_V1 := (others => 0);
      D : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if C.IR_Health_Metadata_Get_Counters (Stream.Handle, Raw'Access, D'Address,
         D'Length, Required'Access) /= C.Success then raise Provider_Error with Message (D); end if;
      return (Counter (Raw.Events_Received), Counter (Raw.Events_Dropped_Queue_Full),
         Counter (Raw.Malformed_Or_Unsupported));
   end Counters;
   procedure Close (Stream : in out Metadata_Stream) is
      D : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if C.IR_Health_Metadata_Close (Stream.Handle'Access, D'Address, D'Length,
         Required'Access) /= C.Success then raise Provider_Error with Message (D); end if;
   end Close;
   overriding procedure Finalize (Stream : in out Metadata_Stream) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Health_Metadata_Close
        (Stream.Handle'Access, System.Null_Address, 0, null);
   exception when others => Stream.Handle := C.Null_Health_Metadata;
   end Finalize;

   function Kind (Event : Metadata_Event) return Metadata_Kind is (Event.Event_Kind);
   function MFA_Status_Value (Event : Metadata_Event) return S.MFA_Status is (Event.MFA);
   function BIT_Status_Value (Event : Metadata_Event) return S.BIT_Status is (Event.BIT);
   function Subsystem_Status_Value (Event : Metadata_Event) return Subsystem_Status is (Event.Subsystem);
   function Discrete_Status_Value (Event : Metadata_Event) return S.Discrete_Status is (Event.Discrete);
   function Security_Audit_Value (Event : Metadata_Event) return S.Security_Audit_Record is (Event.Security);
   function MFA_Status_Detailed_Value (Event : Metadata_Event) return S.MFA_Status_Detailed is (Event.Detailed);
   function Subsystem_ID (Value : Subsystem_Dependency) return Interfaces.Unsigned_32 is (Value.ID);
   function Criticality (Value : Subsystem_Dependency) return Interfaces.Unsigned_32 is (Value.Importance);
   function Failure (Value : Subsystem_Dependency) return Failure_Level is (Value.Failure_Value);
   function CSCI_Name (Value : Subsystem_CSCI) return String is (US.To_String (Value.Name));
   function Mode (Value : Subsystem_CSCI) return CSCI_Mode is (Value.Mode_Value);
   function Version_Value (Value : Subsystem_CSCI) return Version is (Value.Version_Data);
   function Criticality (Value : Subsystem_CSCI) return Interfaces.Unsigned_32 is (Value.Importance);
   function Failure (Value : Subsystem_CSCI) return Failure_Level is (Value.Failure_Value);
   function BIT_Report (Value : Subsystem_CSCI) return Interfaces.Unsigned_32 is (Value.Report);
   function Connection_Established (Value : Subsystem_CSCI) return Boolean is (Value.Connected);
   function Subsystem_ID (Value : Subsystem_Status) return Interfaces.Unsigned_32 is (Value.ID);
   function Criticality (Value : Subsystem_Status) return Interfaces.Unsigned_32 is (Value.Importance);
   function Status_Sequence_Number (Value : Subsystem_Status) return Interfaces.Unsigned_32 is (Value.Sequence);
   function Failure (Value : Subsystem_Status) return Failure_Level is (Value.Failure_Value);
   function Reported_Subsystem_Count (Value : Subsystem_Status) return Interfaces.Unsigned_32 is (Value.Reported_Subsystems);
   function Subsystem_Count (Value : Subsystem_Status) return Natural is (Natural (Value.Subsystems.Length));
   function Subsystem_At (Value : Subsystem_Status; Index : Positive) return Subsystem_Dependency is (Value.Subsystems (Index));
   function Reported_CSCI_Count (Value : Subsystem_Status) return Interfaces.Unsigned_32 is (Value.Reported_CSCI);
   function CSCI_Count (Value : Subsystem_Status) return Natural is (Natural (Value.CSCIs.Length));
   function CSCI_At (Value : Subsystem_Status; Index : Positive) return Subsystem_CSCI is (Value.CSCIs (Index));
end AMS.MEL.IR.Health_Status.Metadata;
