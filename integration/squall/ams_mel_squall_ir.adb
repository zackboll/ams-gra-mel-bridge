with Ada.Command_Line;
with Ada.Exceptions;
with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.C2.Metadata;
with Interfaces;

procedure AMS_MEL_Squall_IR is
   package C2 renames AMS.MEL.IR.C2;
   package Metadata renames AMS.MEL.IR.C2.Metadata;
   use type AMS.MEL.IR.Counter;
   use type C2.MFA_Mode;
   use type C2.Command_Return;
   use type C2.Error_Code;
   use type C2.Outcome;
   use type C2.Command_ID;
   use type Metadata.Metadata_Kind;
   use type Metadata.Command_State;
   use type Metadata.Cannot_Comply;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Unsigned_64;

   Channel_UUID : constant AMS.MEL.IR.UUID :=
     [0 => 16#00#, 1 => 16#40#, 2 => 16#04#, 3 => 16#10#,
      4 => 16#11#, 5 => 16#22#, 6 => 16#43#, 7 => 16#44#,
      8 => 16#85#, 9 => 16#66#, 10 => 16#77#, 11 => 16#88#,
      12 => 16#99#, 13 => 16#AA#, 14 => 16#BB#, 15 => 16#CC#];
   Platform_UUID : constant AMS.MEL.IR.UUID :=
     [0 => 16#00#, 1 => 16#40#, 2 => 16#04#, 3 => 16#11#,
      4 => 16#21#, 5 => 16#32#, 6 => 16#43#, 7 => 16#54#,
      8 => 16#86#, 9 => 16#67#, 10 => 16#78#, 11 => 16#89#,
      12 => 16#9A#, 13 => 16#AB#, 14 => 16#BC#, 15 => 16#CD#];
   Channel_ID : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Channel_UUID, "Task 004 Ada IR");
   Platform_ID : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Platform_UUID, "Task 004 integration platform");
   Location : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location
       (0.0, 0.0, 0.0, "task-004-station", "ams-mel-squall-integration");
   Image_Config : constant AMS.MEL.IR.Image_Config :=
     AMS.MEL.IR.Create_Image_Config
       (Channel_ID, Platform_ID, Location, Buffer_Count => 4,
        Buffer_Size => 1024 * 1024, Queue_Capacity => 8);
   C2_Config : constant C2.Control_Config :=
     C2.Create_Config (Channel_ID, Platform_ID, Location);

   function Checksum (Pixels : AMS.MEL.IR.Pixel_Array) return Interfaces.Unsigned_64 is
      Value : Interfaces.Unsigned_64 := 16#1465_0FB0_739D_0383#;
   begin
      for Pixel of Pixels loop
         Value := (Value xor Interfaces.Unsigned_64 (Pixel)) * 16#0000_0100_0000_01B3#;
      end loop;
      return Value;
   end Checksum;

   procedure Expect_Command_Status
     (Stream : Metadata.Metadata_Stream; ID : C2.Command_ID;
      State : Metadata.Command_State; Reason : Metadata.Cannot_Comply;
      Description_Empty : Boolean)
   is
      Event : constant Metadata.Metadata_Event := Metadata.Receive (Stream, 5_000);
      Status : constant Metadata.Command_Status := Metadata.Command (Event);
   begin
      if Metadata.Kind (Event) /= Metadata.Command_Status_Event
        or else Metadata.Command_ID (Status) /= ID
        or else Metadata.State (Status) /= State
        or else Metadata.Reason (Status) /= Reason
        or else (Metadata.Reason_Description (Status)'Length = 0) /= Description_Empty
      then
         raise Program_Error with "Squall CommandStatus mismatch for" & ID'Image;
      end if;
      Ada.Text_IO.Put_Line
        ("CommandStatus: id=" & ID'Image & " state=" &
         Metadata.Command_State'Image (State) & " reason=" &
         Metadata.Cannot_Comply'Image (Reason) & " description=" &
         Metadata.Reason_Description (Status));
   end Expect_Command_Status;

   procedure Expect_Empty_BIT_Status (Stream : Metadata.Metadata_Stream) is
      Event : constant Metadata.Metadata_Event := Metadata.Receive (Stream, 5_000);
   begin
      if Metadata.Kind (Event) /= Metadata.BIT_Status_Event
        or else Metadata.Active_BIT_Count (Event) /= 0
        or else Metadata.Completed_BIT_Count (Event) /= 0
        or else Metadata.Fault_Count (Event) /= 0
      then
         raise Program_Error with "Squall BIT_Status was not well-formed and empty";
      end if;
      Ada.Text_IO.Put_Line ("BIT_Status: empty default");
   end Expect_Empty_BIT_Status;

begin
   if Ada.Command_Line.Argument_Count < 2 or else Ada.Command_Line.Argument_Count > 4 then
      Ada.Text_IO.Put_Line
        (Ada.Text_IO.Standard_Error,
         "usage: ams_mel_squall_ir PROVIDER_SO PROFILE_JSON [FRAME_COUNT] [TIMEOUT_MS]");
      Ada.Command_Line.Set_Exit_Status (Ada.Command_Line.Failure);
      return;
   end if;

   declare
      Frame_Count : constant Positive :=
        (if Ada.Command_Line.Argument_Count >= 3
         then Positive'Value (Ada.Command_Line.Argument (3)) else 3);
      Timeout_MS : constant Positive :=
        (if Ada.Command_Line.Argument_Count >= 4
         then Positive'Value (Ada.Command_Line.Argument (4)) else 10_000);
      Parent : AMS.MEL.Session := AMS.MEL.Open
        (Ada.Command_Line.Argument (1), Ada.Command_Line.Argument (2));
      Version : constant AMS.MEL.Provider_Version :=
        AMS.MEL.Query_Provider_Version (Parent);
      Stream : AMS.MEL.IR.Image_Stream :=
        AMS.MEL.IR.Open_Image_Stream (Parent, Image_Config);
   begin
      if Frame_Count < 3 then
         raise Constraint_Error with "frame count must be at least three";
      end if;
      Ada.Text_IO.Put_Line
        ("provider version: api=" & AMS.MEL.Provider_Version_Number'Image
           (AMS.MEL.API_Version (Version)) & " library=" &
         AMS.MEL.Provider_Version_Number'Image (AMS.MEL.Library_Version (Version)) &
         " vendor=" & AMS.MEL.Vendor (Version) &
         " description=" & AMS.MEL.Description (Version));

      --  Data destination and buffers are ready before Operate is submitted.
      AMS.MEL.IR.Start (Stream);
      declare
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
         Metadata_Stream : Metadata.Metadata_Stream := Metadata.Open (Channel, 32);
      begin
         declare
            Configuration : constant Metadata.Metadata_Event :=
              Metadata.Receive (Metadata_Stream, 5_000);
         begin
            if Metadata.Kind (Configuration) /= Metadata.BIT_Configuration_Event
              or else Metadata.BIT_Type_Count (Configuration) /= 0
            then
               raise Program_Error with
                 "Squall BIT_Configuration was not well-formed and empty";
            end if;
            Ada.Text_IO.Put_Line ("BIT_Configuration: empty default");
         end;
         Expect_Empty_BIT_Status (Metadata_Stream);
         C2.Enable (Channel);
         declare
            Request : C2.Mode_Request := C2.Submit_Mode
              (Channel, 16#0040_1701#, C2.Standby, C2.Unused);
            Result : constant C2.Mode_Result := C2.Wait (Request, 5_000);
         begin
            if C2.Status (Result) /= C2.Success or else C2.Mode (Result) /= C2.Unused then
               raise Program_Error with "Squall did not accept Standby/Unused";
            end if;
            Ada.Text_IO.Put_Line ("general mode result: STANDBY/UNUSED");
            Expect_Command_Status
              (Metadata_Stream, 16#0040_1701#, Metadata.Accepted,
               Metadata.Not_Set, Description_Empty => True);
            C2.Close (Request);
         end;
         declare
            Request : C2.Mode_Request := C2.Submit_Mode
              (Channel, 16#0040_1702#, C2.Operate, C2.Scan_Volume_Sched);
            Result : constant C2.Mode_Result := C2.Wait (Request, 5_000);
         begin
            if C2.Status (Result) /= C2.Rejected or else
              C2.Rejection_Code (Result) /= C2.Invalid_Parameters
            then
               raise Program_Error with "Squall did not reject unsupported scan mode";
            end if;
            Ada.Text_IO.Put_Line ("scan mode result: REJECTED/INVALID_PARAMETERS");
            Expect_Command_Status
              (Metadata_Stream, 16#0040_1702#, Metadata.Rejected,
               Metadata.Invalid_Input_Parameter, Description_Empty => False);
            C2.Close (Request);
         end;
         declare
            Empty_Request : C2.Return_Request := C2.Submit_Config_Set
              (Channel, 16#0040_1703#);
            Payload_Request : C2.Return_Request := C2.Submit_Config_Set
              (Channel, 16#0040_1704#, Config => "task-017");
         begin
            if C2.Value (C2.Wait (Empty_Request, 5_000)) /= C2.Return_Success or else
              C2.Value (C2.Wait (Payload_Request, 5_000)) /= C2.Fail
            then
               raise Program_Error with "Squall ConfigSet behavior changed";
            end if;
            Ada.Text_IO.Put_Line ("ConfigSet empty result: SUCCESS");
            Expect_Command_Status
              (Metadata_Stream, 16#0040_1703#, Metadata.Accepted,
               Metadata.Not_Set, Description_Empty => True);
            Ada.Text_IO.Put_Line ("ConfigSet payload result: FAIL");
            Expect_Command_Status
              (Metadata_Stream, 16#0040_1704#, Metadata.Rejected,
               Metadata.Invalid_Input_Parameter, Description_Empty => False);
            C2.Close (Empty_Request); C2.Close (Payload_Request);
         end;
         declare
            Payload_Request : C2.Return_Request := C2.Submit_BIT_Initiate
              (Channel, [16#8000_0001#], 16#0040_1705#);
         begin
            if C2.Value (C2.Wait (Payload_Request, 5_000)) /= C2.Fail then
               raise Program_Error with "Squall did not return Fail for BIT payload";
            end if;
            Ada.Text_IO.Put_Line ("BIT payload result: FAIL");
            Expect_Command_Status
              (Metadata_Stream, 16#0040_1705#, Metadata.Rejected,
               Metadata.Invalid_Input_Parameter, Description_Empty => False);
            C2.Close (Payload_Request);
         end;
         declare
            BIT_Request : C2.Return_Request :=
              C2.Submit_BIT_No_Op (Channel, 16#0040_1402#);
            BIT_Result : constant C2.Return_Result := C2.Wait (BIT_Request, 5_000);
         begin
            if C2.Status (BIT_Result) /= C2.Success
              or else C2.Value (BIT_Result) /= C2.Return_Success
            then
               raise Program_Error with "Squall did not return BIT Success";
            end if;
            Ada.Text_IO.Put_Line ("BIT result: SUCCESS");
            Expect_Command_Status
              (Metadata_Stream, 16#0040_1402#, Metadata.Accepted,
               Metadata.Not_Set, Description_Empty => True);
            Expect_Empty_BIT_Status (Metadata_Stream);
            C2.Close (BIT_Request);
         end;
         declare
            Request : C2.Mode_Request := C2.Submit_Mode
              (Channel, 16#0040_0402#, C2.Operate, C2.Task_Sched);
            Result : constant C2.Mode_Result := C2.Wait (Request, 5_000);
         begin
            if C2.Status (Result) /= C2.Success
              or else C2.Mode (Result) /= C2.Task_Sched
            then
               raise Program_Error with "Squall did not return Task_Sched";
            end if;
            Ada.Text_IO.Put_Line ("C2 result: TASK_SCHED");
            Expect_Command_Status
              (Metadata_Stream, 16#0040_0402#, Metadata.Accepted,
               Metadata.Not_Set, Description_Empty => True);

            --  Exercise retained child/request ownership after parent close.
            AMS.MEL.Close (Parent);
            declare
               Cached : constant C2.Mode_Result := C2.Wait (Request, 0);
            begin
               if C2.Status (Cached) /= C2.Success
                 or else C2.Mode (Cached) /= C2.Task_Sched
               then
                  raise Program_Error with "completed request was not retained";
               end if;
            end;

            declare
               Previous_ID : Interfaces.Unsigned_32 := 0;
            begin
               for Index in 1 .. Frame_Count loop
                  declare
                     Frame : constant AMS.MEL.IR.Frame :=
                       AMS.MEL.IR.Receive (Stream, Timeout_MS);
                     Expected : constant Long_Long_Integer :=
                       Long_Long_Integer (Frame.Width) * Long_Long_Integer (Frame.Height);
                  begin
                     if Frame.Width = 0 or else Frame.Height = 0
                       or else Frame.Bits_Per_Pixel /= 8
                       or else Frame.Number_Of_Bands /= 1
                       or else Expected /= Long_Long_Integer (Frame.Pixel_Count)
                       or else (Index > 1 and then Frame.Frame_ID <= Previous_ID)
                     then
                        raise Program_Error with "invalid Squall Mono8 frame";
                     end if;
                     Previous_ID := Frame.Frame_ID;
                     Ada.Text_IO.Put_Line
                       ("frame" & Index'Image & ": id=" & Frame.Frame_ID'Image &
                        " geometry=" & Frame.Width'Image & "x" & Frame.Height'Image &
                        " bytes=" & Frame.Pixel_Count'Image & " checksum=" &
                        Interfaces.Unsigned_64'Image (Checksum (Frame.Pixels)));
                  end;
               end loop;
            end;

            declare
               Counts : constant AMS.MEL.IR.Stream_Counters :=
                 AMS.MEL.IR.Counters (Stream);
            begin
               Ada.Text_IO.Put_Line
                 ("counters: received=" & Counts.Frames_Received'Image &
                  " dropped=" & Counts.Frames_Dropped_Queue_Full'Image &
                  " malformed=" & Counts.Malformed_Or_Unsupported'Image);
               if Counts.Frames_Received < AMS.MEL.IR.Counter (Frame_Count)
                 or else Counts.Malformed_Or_Unsupported /= 0
               then
                  raise Program_Error with "invalid Squall stream counters";
               end if;
            end;
            C2.Close (Request);
         end;
         declare
            Counts : constant Metadata.Metadata_Counters :=
              Metadata.Counters (Metadata_Stream);
         begin
            Ada.Text_IO.Put_Line
              ("metadata counters: received=" & Counts.Events_Received'Image &
               " dropped=" & Counts.Events_Dropped_Queue_Full'Image &
               " malformed=" & Counts.Malformed_Or_Unsupported'Image);
            if Counts.Events_Received < 10
              or else Counts.Events_Dropped_Queue_Full /= 0
              or else Counts.Malformed_Or_Unsupported /= 0
            then
               raise Program_Error with "invalid Squall metadata counters";
            end if;
         end;
         C2.Close (Channel);
         begin
            declare
               Unexpected : constant Metadata.Metadata_Event :=
                 Metadata.Receive (Metadata_Stream, 0);
            begin
               raise Program_Error with
                 "metadata remained after close: " &
                 Metadata.Metadata_Kind'Image (Metadata.Kind (Unexpected));
            end;
         exception
            when AMS.MEL.IR.Stream_Stopped => null;
         end;
         Metadata.Close (Metadata_Stream);
      end;
      AMS.MEL.IR.Close (Stream);
      Ada.Text_IO.Put_Line ("PASS: real Squall IR Ada integration");
   end;
exception
   when Error : others =>
      Ada.Text_IO.Put_Line
        (Ada.Text_IO.Standard_Error,
         "FAIL: real Squall IR Ada integration: " &
         Ada.Exceptions.Exception_Information (Error));
      Ada.Command_Line.Set_Exit_Status (Ada.Command_Line.Failure);
end AMS_MEL_Squall_IR;
