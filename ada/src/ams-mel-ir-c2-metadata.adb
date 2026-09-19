with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;
with System.Storage_Elements;

package body AMS.MEL.IR.C2.Metadata is
   package C renames AMS.MEL_C_API;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.size_t;
   use type Interfaces.C.char;
   use type C.C2_Handle;
   use type C.Metadata_Handle;
   use type C.Metadata_Event_Handle;
   use System.Storage_Elements;

   Diagnostic_Capacity : constant := 512;
   type Diagnostic_Array is array (C.Size_T range <>) of aliased Interfaces.C.char
   with Convention => C;
   subtype Diagnostic is Diagnostic_Array (0 .. Diagnostic_Capacity - 1);
   type Event_Access is access all C.Metadata_Event_V1;
   type String_View_Access is access all C.String_View_V1;
   type ID_Access is access all C.UCI_ID_V1;
   type BIT_Type_Access is access all C.BIT_Type_V1;
   type Active_Access is access all C.Active_BIT_V1;
   type Completed_Access is access all C.Completed_BIT_V1;
   type Item_Access is access all C.Completed_BIT_Item_V1;
   type Fault_Access is access all C.Fault_V1;
   type Data_Access is access all C.Fault_Data_V1;
   type Group_Access is access all C.Fault_Ambiguity_Group_V1;
   type Char_Access is access all Interfaces.C.char;
   function To_Event is new Ada.Unchecked_Conversion (System.Address, Event_Access);
   function To_String_View is new Ada.Unchecked_Conversion (System.Address, String_View_Access);
   function To_ID is new Ada.Unchecked_Conversion (System.Address, ID_Access);
   function To_BIT_Type is new Ada.Unchecked_Conversion (System.Address, BIT_Type_Access);
   function To_Active is new Ada.Unchecked_Conversion (System.Address, Active_Access);
   function To_Completed is new Ada.Unchecked_Conversion (System.Address, Completed_Access);
   function To_Item is new Ada.Unchecked_Conversion (System.Address, Item_Access);
   function To_Fault is new Ada.Unchecked_Conversion (System.Address, Fault_Access);
   function To_Data is new Ada.Unchecked_Conversion (System.Address, Data_Access);
   function To_Group is new Ada.Unchecked_Conversion (System.Address, Group_Access);
   function To_Char is new Ada.Unchecked_Conversion (System.Address, Char_Access);

   function Address_At (Base : System.Address; Index, Bytes : Natural) return System.Address
   is (Base + Storage_Offset (Index * Bytes));
   function Message (Value : Diagnostic_Array) return String is
      Last : Natural := 0;
   begin
      while Last < Value'Length and then Value (C.Size_T (Last)) /= Interfaces.C.nul loop
         Last := Last + 1;
      end loop;
      declare
         Result : String (1 .. Last);
      begin
         for I in Result'Range loop
            Result (I) := Character'Val (Interfaces.C.char'Pos (Value (C.Size_T (I - 1))));
         end loop;
         return (if Result'Length = 0 then "native C2 metadata operation failed" else Result);
      end;
   end Message;
   function Copy (Value : C.String_View_V1) return String is
   begin
      if Value.Size = 0 then
         return "";
      end if;
      declare
         Result : String (1 .. Natural (Value.Size));
      begin
         for I in Result'Range loop
            Result (I) :=
              Character'Val
                (Interfaces.C.char'Pos (To_Char (Address_At (Value.Data, I - 1, 1)).all));
         end loop;
         return Result;
      end;
   end Copy;
   function Copy (Value : C.UCI_ID_V1) return UCI_ID is
      Bytes : UUID;
   begin
      for I in Bytes'Range loop
         Bytes (I) := Value.UUID (I);
      end loop;
      return Create_UCI_ID (Bytes, Copy (Value.Descriptive_Label));
   end Copy;

   function Open (Channel : Control_Channel; Queue_Capacity : Positive := 16) return Metadata_Stream
   is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      if Channel.Handle = C.Null_C2 then
         raise Provider_Error with "C2 channel is closed";
      end if;
      return Result : Metadata_Stream do
         declare
            Code : constant Interfaces.Integer_32 :=
              C.IR_C2_Metadata_Open
                (Channel.Handle,
                 C.Size_T (Queue_Capacity),
                 Result.Handle'Access,
                 D'Address,
                 D'Length,
                 R'Access);
         begin
            if Code /= C.Success then
               raise Provider_Error with Message (D);
            end if;
         end;
      end return;
   end Open;
   function Is_Open (Stream : Metadata_Stream) return Boolean
   is (Stream.Handle /= C.Null_Metadata);
   procedure Enable_Comms_Test_Events (Stream : in out Metadata_Stream) is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      if C.IR_C2_Metadata_Register_Comms_Test (Stream.Handle, D'Address, D'Length, R'Access)
        /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
   end Enable_Comms_Test_Events;

   function Copy_IDs (Span : C.Span_V1) return ID_Vectors.Vector is
      Result : ID_Vectors.Vector;
   begin
      for I in 0 .. Natural (Span.Size) - 1 loop
         Result.Append
           (Copy
              (To_ID
                 (Address_At (Span.Data, I, C.UCI_ID_V1'Object_Size / System.Storage_Unit)).all));
      end loop;
      return Result;
   end Copy_IDs;
   function Copy_Event (Raw : C.Metadata_Event_V1) return Metadata_Event is
      Result : Metadata_Event;
   begin
      if Raw.Kind not in 1 .. 4 then
         raise Provider_Error with "invalid native metadata kind";
      end if;
      Result.Event_Kind := Metadata_Kind'Val (Integer (Raw.Kind) - 1);
      if Raw.Kind = 1 then
         if Raw.Command_Status.State > 4 or else Raw.Command_Status.Reason_ID > 46 then
            raise Provider_Error with "invalid native CommandStatus enum";
         end if;
         Result.Status :=
           (C2.Command_ID (Raw.Command_Status.Command_ID),
            Command_State'Val (Raw.Command_Status.State),
            Cannot_Comply'Val (Raw.Command_Status.Reason_ID),
            US.To_Unbounded_String (Copy (Raw.Command_Status.Reason_Description)));
      elsif Raw.Kind = 2 then
         for I in 0 .. Natural (Raw.BIT_Configuration.BIT_Types.Size) - 1 loop
            declare
               B : constant C.BIT_Type_V1 :=
                 To_BIT_Type
                   (Address_At
                      (Raw.BIT_Configuration.BIT_Types.Data,
                       I,
                       C.BIT_Type_V1'Object_Size / System.Storage_Unit)).all;
               V : BIT_Type;
            begin
               if B.Accepted_Interface > 3 then
                  raise Provider_Error with "invalid native BIT interface";
               end if;
               V.ID := Copy (B.BIT_ID);
               V.Interface_Value := BIT_Control_Interface'Val (B.Accepted_Interface);
               V.Duration := Long_Long_Integer (B.Expected_Duration_NS);
               for J in 0 .. Natural (B.BIT_Item_Names.Size) - 1 loop
                  V.Names.Append
                    (US.To_Unbounded_String
                       (Copy
                          (To_String_View
                             (Address_At
                                (B.BIT_Item_Names.Data,
                                 J,
                                 C.String_View_V1'Object_Size / System.Storage_Unit)).all)));
               end loop;
               V.Components := Copy_IDs (B.Subsystem_Component_IDs);
               Result.BIT_Types.Append (V);
            end;
         end loop;
      elsif Raw.Kind = 3 then
         for I in 0 .. Natural (Raw.BIT_Status.Active_BITS.Size) - 1 loop
            declare
               A : constant C.Active_BIT_V1 :=
                 To_Active
                   (Address_At
                      (Raw.BIT_Status.Active_BITS.Data,
                       I,
                       C.Active_BIT_V1'Object_Size / System.Storage_Unit)).all;
            begin
               Result.Active.Append
                 (Active_BIT'
                    (Copy (A.BIT_ID),
                     Long_Long_Integer (A.Estimated_Completion_Time_NS),
                     Long_Float (A.Estimated_Percent_Complete)));
            end;
         end loop;
         for I in 0 .. Natural (Raw.BIT_Status.Completed_BITS.Size) - 1 loop
            declare
               B : constant C.Completed_BIT_V1 :=
                 To_Completed
                   (Address_At
                      (Raw.BIT_Status.Completed_BITS.Data,
                       I,
                       C.Completed_BIT_V1'Object_Size / System.Storage_Unit)).all;
               V : Completed_BIT;
            begin
               if B.Result > 4 then
                  raise Provider_Error with "invalid native BIT result";
               end if;
               V.ID := Copy (B.BIT_ID);
               V.Time := Long_Long_Integer (B.Time_Tag_NS);
               V.Value := BIT_Result'Val (B.Result);
               V.Why := US.To_Unbounded_String (Copy (B.Fail_Reason));
               for J in 0 .. Natural (B.BIT_Items.Size) - 1 loop
                  declare
                     X : constant C.Completed_BIT_Item_V1 :=
                       To_Item
                         (Address_At
                            (B.BIT_Items.Data,
                             J,
                             C.Completed_BIT_Item_V1'Object_Size / System.Storage_Unit)).all;
                  begin
                     if X.Result > 4 then
                        raise Provider_Error with "invalid native item result";
                     end if;
                     V.Items.Append
                       (Completed_BIT_Item'
                          (US.To_Unbounded_String (Copy (X.BIT_Item_Name)),
                           US.To_Unbounded_String (Copy (X.Fail_Reason)),
                           BIT_Result'Val (X.Result)));
                  end;
               end loop;
               Result.Completed.Append (V);
            end;
         end loop;
         for I in 0 .. Natural (Raw.BIT_Status.Faults.Size) - 1 loop
            declare
               F : constant C.Fault_V1 :=
                 To_Fault
                   (Address_At
                      (Raw.BIT_Status.Faults.Data,
                       I,
                       C.Fault_V1'Object_Size / System.Storage_Unit)).all;
               V : Fault;
            begin
               if F.Severity > 4 or else F.State > 3 then
                  raise Provider_Error with "invalid native fault enum";
               end if;
               V.ID := Copy (F.Fault_ID);
               V.Severity_Value := Fault_Severity'Val (F.Severity);
               V.State_Value := Fault_State'Val (F.State);
               V.Time := Long_Long_Integer (F.Detection_Time_NS);
               V.Code := US.To_Unbounded_String (Copy (F.Fault_Code));
               V.Description := US.To_Unbounded_String (Copy (F.Fault_Description));
               V.Components := Copy_IDs (F.Component_IDs);
               for J in 0 .. Natural (F.Fault_Data.Size) - 1 loop
                  declare
                     X : constant C.Fault_Data_V1 :=
                       To_Data
                         (Address_At
                            (F.Fault_Data.Data,
                             J,
                             C.Fault_Data_V1'Object_Size / System.Storage_Unit)).all;
                  begin
                     V.Data.Append
                       (Fault_Data'
                          (US.To_Unbounded_String (Copy (X.Key)),
                           US.To_Unbounded_String (Copy (X.Value)),
                           US.To_Unbounded_String (Copy (X.Format)),
                           US.To_Unbounded_String (Copy (X.Units))));
                  end;
               end loop;
               for J in 0 .. Natural (F.Ambiguity_Groups.Size) - 1 loop
                  declare
                     X : constant C.Fault_Ambiguity_Group_V1 :=
                       To_Group
                         (Address_At
                            (F.Ambiguity_Groups.Data,
                             J,
                             C.Fault_Ambiguity_Group_V1'Object_Size / System.Storage_Unit)).all;
                     G : Fault_Ambiguity_Group;
                  begin
                     G.Tests := Copy_IDs (X.Diagnostic_Test_IDs);
                     G.Components := Copy_IDs (X.Component_IDs);
                     V.Groups.Append (G);
                  end;
               end loop;
               Result.Faults.Append (V);
            end;
         end loop;
      else
         Result.Comms :=
           (C2.Command_ID (Raw.Channel_Comms_Test.Command_ID),
            IR.Channel.Comms_Request_ID (Raw.Channel_Comms_Test.Request_ID));
      end if;
      return Result;
   end Copy_Event;

   function Receive
     (Stream : Metadata_Stream; Timeout_Milliseconds : Natural := 0) return Metadata_Event
   is
      Owner   : aliased C.Metadata_Event_Handle := C.Null_Metadata_Event;
      Address : aliased System.Address := System.Null_Address;
      D       : aliased Diagnostic := [others => Interfaces.C.nul];
      R       : aliased C.Size_T := 0;
      Code    : Interfaces.Integer_32;
   begin
      Code :=
        C.IR_C2_Metadata_Receive
          (Stream.Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Owner'Access,
           D'Address,
           D'Length,
           R'Access);
      if Code = C.Timeout then
         raise Timeout_Error;
      elsif Code = C.Stream_Stopped then
         raise Stream_Stopped;
      elsif Code /= C.Success then
         raise Provider_Error with Message (D);
      end if;
      Code := C.IR_C2_Metadata_Event_View (Owner, Address'Access, D'Address, D'Length, R'Access);
      if Code /= C.Success then
         declare
            Ignored : constant Interfaces.Integer_32 :=
              C.IR_C2_Metadata_Event_Close (Owner'Access, System.Null_Address, 0, null);
         begin
            raise Provider_Error with Message (D);
         end;
      end if;
      declare
         Result  : constant Metadata_Event := Copy_Event (To_Event (Address).all);
         Ignored : constant Interfaces.Integer_32 :=
           C.IR_C2_Metadata_Event_Close (Owner'Access, System.Null_Address, 0, null);
      begin
         return Result;
      end;
   exception
      when others =>
         if Owner /= C.Null_Metadata_Event then
            declare
               Ignored : constant Interfaces.Integer_32 :=
                 C.IR_C2_Metadata_Event_Close (Owner'Access, System.Null_Address, 0, null);
            begin
               null;
            end;
         end if;
         raise;
   end Receive;

   function Kind (Event : Metadata_Event) return Metadata_Kind
   is (Event.Event_Kind);
   function Command (Event : Metadata_Event) return Command_Status
   is (Event.Status);
   function Command_ID (Value : Command_Status) return C2.Command_ID
   is (Value.ID);
   function State (Value : Command_Status) return Command_State
   is (Value.Status);
   function Reason (Value : Command_Status) return Cannot_Comply
   is (Value.Why);
   function Reason_Description (Value : Command_Status) return String
   is (US.To_String (Value.Description));
   function BIT_Type_Count (Event : Metadata_Event) return Natural
   is (Natural (Event.BIT_Types.Length));
   function BIT_Type_At (Event : Metadata_Event; Index : Positive) return BIT_Type
   is (Event.BIT_Types (Index));
   function BIT_ID (Value : BIT_Type) return UCI_ID
   is (Value.ID);
   function Accepted_Interface (Value : BIT_Type) return BIT_Control_Interface
   is (Value.Interface_Value);
   function Expected_Duration_NS (Value : BIT_Type) return Long_Long_Integer
   is (Value.Duration);
   function BIT_Item_Name_Count (Value : BIT_Type) return Natural
   is (Natural (Value.Names.Length));
   function BIT_Item_Name_At (Value : BIT_Type; Index : Positive) return String
   is (US.To_String (Value.Names (Index)));
   function Subsystem_Component_Count (Value : BIT_Type) return Natural
   is (Natural (Value.Components.Length));
   function Subsystem_Component_At (Value : BIT_Type; Index : Positive) return UCI_ID
   is (Value.Components (Index));
   function Active_BIT_Count (Event : Metadata_Event) return Natural
   is (Natural (Event.Active.Length));
   function Active_BIT_At (Event : Metadata_Event; Index : Positive) return Active_BIT
   is (Event.Active (Index));
   function BIT_ID (Value : Active_BIT) return UCI_ID
   is (Value.ID);
   function Estimated_Completion_Time_NS (Value : Active_BIT) return Long_Long_Integer
   is (Value.Completion);
   function Estimated_Percent_Complete (Value : Active_BIT) return Long_Float
   is (Value.Percent);
   function Completed_BIT_Count (Event : Metadata_Event) return Natural
   is (Natural (Event.Completed.Length));
   function Completed_BIT_At (Event : Metadata_Event; Index : Positive) return Completed_BIT
   is (Event.Completed (Index));
   function BIT_ID (Value : Completed_BIT) return UCI_ID
   is (Value.ID);
   function Time_Tag_NS (Value : Completed_BIT) return Long_Long_Integer
   is (Value.Time);
   function Result (Value : Completed_BIT) return BIT_Result
   is (Value.Value);
   function Fail_Reason (Value : Completed_BIT) return String
   is (US.To_String (Value.Why));
   function BIT_Item_Count (Value : Completed_BIT) return Natural
   is (Natural (Value.Items.Length));
   function BIT_Item_At (Value : Completed_BIT; Index : Positive) return Completed_BIT_Item
   is (Value.Items (Index));
   function BIT_Item_Name (Value : Completed_BIT_Item) return String
   is (US.To_String (Value.Name));
   function Result (Value : Completed_BIT_Item) return BIT_Result
   is (Value.Value);
   function Fail_Reason (Value : Completed_BIT_Item) return String
   is (US.To_String (Value.Why));
   function Fault_Count (Event : Metadata_Event) return Natural
   is (Natural (Event.Faults.Length));
   function Fault_At (Event : Metadata_Event; Index : Positive) return Fault
   is (Event.Faults (Index));
   function Fault_ID (Value : Fault) return UCI_ID
   is (Value.ID);
   function Severity (Value : Fault) return Fault_Severity
   is (Value.Severity_Value);
   function State (Value : Fault) return Fault_State
   is (Value.State_Value);
   function Detection_Time_NS (Value : Fault) return Long_Long_Integer
   is (Value.Time);
   function Fault_Code (Value : Fault) return String
   is (US.To_String (Value.Code));
   function Fault_Description (Value : Fault) return String
   is (US.To_String (Value.Description));
   function Fault_Data_Count (Value : Fault) return Natural
   is (Natural (Value.Data.Length));
   function Fault_Data_At (Value : Fault; Index : Positive) return Fault_Data
   is (Value.Data (Index));
   function Component_Count (Value : Fault) return Natural
   is (Natural (Value.Components.Length));
   function Component_At (Value : Fault; Index : Positive) return UCI_ID
   is (Value.Components (Index));
   function Ambiguity_Group_Count (Value : Fault) return Natural
   is (Natural (Value.Groups.Length));
   function Ambiguity_Group_At (Value : Fault; Index : Positive) return Fault_Ambiguity_Group
   is (Value.Groups (Index));
   function Comms_Test (Event : Metadata_Event) return IR.Channel.Comms_Test_Report
   is (Event.Comms);
   function Key (Value : Fault_Data) return String
   is (US.To_String (Value.K));
   function Data_Value (Value : Fault_Data) return String
   is (US.To_String (Value.V));
   function Format (Value : Fault_Data) return String
   is (US.To_String (Value.F));
   function Units (Value : Fault_Data) return String
   is (US.To_String (Value.U));
   function Diagnostic_Test_Count (Value : Fault_Ambiguity_Group) return Natural
   is (Natural (Value.Tests.Length));
   function Diagnostic_Test_At (Value : Fault_Ambiguity_Group; Index : Positive) return UCI_ID
   is (Value.Tests (Index));
   function Component_Count (Value : Fault_Ambiguity_Group) return Natural
   is (Natural (Value.Components.Length));
   function Component_At (Value : Fault_Ambiguity_Group; Index : Positive) return UCI_ID
   is (Value.Components (Index));

   function Counters (Stream : Metadata_Stream) return Metadata_Counters is
      Raw  : aliased C.Metadata_Counters_V1 := (others => 0);
      D    : aliased Diagnostic := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 :=
        C.IR_C2_Metadata_Get_Counters (Stream.Handle, Raw'Access, D'Address, D'Length, R'Access);
   begin
      if Code /= C.Success then
         raise Provider_Error with Message (D);
      end if;
      return
        (Counter (Raw.Events_Received),
         Counter (Raw.Events_Dropped_Queue_Full),
         Counter (Raw.Malformed_Or_Unsupported));
   end Counters;
   procedure Close (Stream : in out Metadata_Stream) is
      D    : aliased Diagnostic := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 :=
        C.IR_C2_Metadata_Close (Stream.Handle'Access, D'Address, D'Length, R'Access);
   begin
      if Code /= C.Success then
         raise Provider_Error with Message (D);
      end if;
   end Close;
   overriding
   procedure Finalize (Stream : in out Metadata_Stream) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_C2_Metadata_Close (Stream.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Stream.Handle := C.Null_Metadata;
   end Finalize;
end AMS.MEL.IR.C2.Metadata;
