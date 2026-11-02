with Ada.Exceptions;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.Camera;
with AMS.MEL.IR.Camera.Testing;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Image;
with AMS.MEL.IR.Image.Metadata;

package body AMS_MEL_IR_Camera_Command_Tests is
   package Cam renames AMS.MEL.IR.Camera;
   package C2 renames AMS.MEL.IR.C2;
   package Image renames AMS.MEL.IR.Image;
   package Metadata renames AMS.MEL.IR.Image.Metadata;
   package Ch renames AMS.MEL.IR.Channel;
   use type Cam.Camera_Command_Response;
   use type Cam.Outcome;
   use type Cam.Error_Code;
   use type AMS.MEL.IR.Counter;
   use type Cam.Nanoseconds;
   use type Metadata.Metadata_Kind;

   type Command_Array is array (Positive range <>) of Cam.Camera_Command;

   Zero       : constant AMS.MEL.IR.UUID := [others => 0];
   Channel_ID : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "Ada camera channel");
   Platform   : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Zero, "Ada camera platform");
   Location   : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "station", "mock");
   C2_Config  : constant C2.Control_Config := C2.Create_Config (Channel_ID, Platform, Location);
   Config     : constant AMS.MEL.IR.Image_Config :=
     AMS.MEL.IR.Create_Image_Config
       (Channel_ID, Platform, Location, Buffer_Count => 2, Buffer_Size => 64, Queue_Capacity => 2);

   procedure Require (Value : Boolean; Text : String) is
   begin
      if not Value then
         raise Program_Error with "Ada CameraCommand: " & Text;
      end if;
   end Require;

   --  Boundary command: both alternatives, unsigned/signed extremes.
   function Boundary return Cam.Camera_Command
   is ((ID                  => 16#FFFF_FFFF#,
        System_Time_NS      => Cam.Nanoseconds'First,
        Kind                => Cam.Scanning,
        Width               => Cam.Unsigned_32'Last,
        Height              => Cam.Unsigned_32'Last,
        Frame_Rate_Hz       => Cam.Unsigned_32'Last,
        Integration_Time_NS => Cam.Nanoseconds'Last,
        Row_Offset          => Cam.Unsigned_32'Last,
        Column_Offset       => Cam.Unsigned_32'Last,
        Direction           => Cam.Positive_To_Negative_Azimuth,
        Number_Of_Stages    => Cam.Unsigned_32'Last));

   function Ordinary (ID : Cam.Command_ID) return Cam.Camera_Command
   is ((ID                  => ID,
        System_Time_NS      => Cam.Nanoseconds'Last,
        Kind                => Cam.Staring,
        Width               => 640,
        Height              => 512,
        Frame_Rate_Hz       => 60,
        Integration_Time_NS => Cam.Nanoseconds'First,
        Row_Offset          => 0,
        Column_Offset       => 0,
        Direction           => Cam.Negative_To_Positive_Azimuth,
        Number_Of_Stages    => 0));

   --  The mock echoes ID, Size = Width and Timestamp = System_Time.
   procedure Check_Echo
     (Request : Cam.Camera_Request; Command : Cam.Camera_Command; Success : Boolean)
   is
      First  : constant Cam.Camera_Result := Cam.Wait (Request, 5_000);
      Second : constant Cam.Camera_Result := Cam.Wait (Request, 0);
      Want   : constant Cam.Camera_Command_Response :=
        (ID              => Command.ID,
         Size            => Command.Width,
         Timestamp_NS    => Command.System_Time_NS,
         Command_Success => Success);
   begin
      Require (Cam.Status (First) = Ch.Success and then Cam.Status (Second) = Ch.Success, "status");
      Require (Cam.Response (First) = Want and then Cam.Response (Second) = Want, "response");
   end Check_Echo;

   procedure Test_C2 (Provider_Path : String; Scenario : String; Success : Boolean) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
   begin
      C2.Enable (Channel);
      for Command of Command_Array'[Boundary, Ordinary (0), Ordinary (16#8000_0001#)] loop
         declare
            Request : Cam.Camera_Request := C2.Submit_Camera_Command (Channel, Command);
         begin
            Require (Cam.Is_Open (Request), "C2 request open");
            Check_Echo (Request, Command, Success);
            Cam.Close (Request);
            Require (not Cam.Is_Open (Request), "C2 request closed");
            Cam.Close (Request);
         end;
      end loop;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_C2;

   procedure Test_Image (Provider_Path : String; Scenario : String; Success : Boolean) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
   begin
      for Command of Command_Array'[Boundary, Ordinary (7), Ordinary (16#7FFF_FFFF#)] loop
         declare
            Request : Cam.Camera_Request := Image.Submit_Camera_Command (Stream, Command);
         begin
            Check_Echo (Request, Command, Success);
            Cam.Close (Request);
         end;
      end loop;
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Test_Image;

   --  Rejection and failure categories on both families are distinct outcomes.
   procedure Test_Outcomes (Provider_Path : String; Image_Family : Boolean) is
      procedure Expect
        (Scenario : String; Code : Cam.Error_Code; Text : String; Raises : Boolean := False)
      is
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
         Stream  : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      begin
         C2.Enable (Channel);
         declare
            Request : Cam.Camera_Request :=
              (if Image_Family
               then Image.Submit_Camera_Command (Stream, Ordinary (9))
               else C2.Submit_Camera_Command (Channel, Ordinary (9)));
         begin
            if Raises then
               begin
                  declare
                     Ignored : constant Cam.Camera_Result := Cam.Wait (Request, 5_000);
                  begin
                     raise Program_Error with "Ada CameraCommand failure did not raise";
                  end;
               exception
                  when Error : AMS.MEL.Provider_Error =>
                     Require (Ada.Exceptions.Exception_Message (Error) = Text, "failure text");
               end;
            else
               declare
                  Result : constant Cam.Camera_Result := Cam.Wait (Request, 5_000);
               begin
                  Require (Cam.Status (Result) = Ch.Rejected, "rejected status");
                  Require (Cam.Rejection_Code (Result) = Code, "rejection code");
                  Require (Cam.Description (Result) = Text, "rejection text");
               end;
            end if;
            Cam.Close (Request);
         end;
         C2.Close (Channel);
         AMS.MEL.IR.Close (Stream);
         AMS.MEL.Close (Parent);
      end Expect;
      Long : String (1 .. 613) := [others => 'x'];
   begin
      Long (511) := Character'Val (16#E2#);
      Long (512) := Character'Val (16#82#);
      Long (513) := Character'Val (16#AC#);
      Long (514 .. Long'Last) := [others => 'y'];
      Expect ("no-camera-control", Ch.Unsupported, "");
      Expect ("camera-reject", Ch.Invalid_Parameters, "camera command rejected");
      Expect ("camera-reject-long", Ch.Insufficient_Resources, Long);
      Expect ("camera-null", Ch.None, "provider returned null successful CameraCommandResp", True);
      Expect ("camera-unknown-error", Ch.None, "provider returned unknown MEL error code", True);
      Expect ("camera-future-throw", Ch.None, "mock camera future exception", True);
   end Test_Outcomes;

   --  C2 requires Enabled; send exceptions raise; Stop refuses Image submission.
   procedure Test_Submission_Errors (Provider_Path : String) is
   begin
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "camera-echo");
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
         Stream  : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      begin
         begin
            declare
               Ignored : Cam.Camera_Request := C2.Submit_Camera_Command (Channel, Ordinary (1));
            begin
               raise Program_Error with "Ada C2 CameraCommand accepted before Enable";
            end;
         exception
            when Error : AMS.MEL.Provider_Error =>
               Require
                 (Ada.Exceptions.Exception_Message (Error) = "C2 channel is not enabled",
                  "enabled");
         end;
         AMS.MEL.IR.Stop (Stream);
         begin
            declare
               Ignored : Cam.Camera_Request := Image.Submit_Camera_Command (Stream, Ordinary (1));
            begin
               raise Program_Error with "Ada Image CameraCommand accepted after Stop";
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         C2.Close (Channel);
         AMS.MEL.IR.Close (Stream);
         AMS.MEL.Close (Parent);
      end;
      declare
         Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "camera-send-throw");
         Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      begin
         declare
            Ignored : Cam.Camera_Request := Image.Submit_Camera_Command (Stream, Ordinary (1));
         begin
            raise Program_Error with "Ada CameraCommand send exception not raised";
         end;
      exception
         when Error : AMS.MEL.Provider_Error =>
            Require
              (Ada.Exceptions.Exception_Message (Error) = "mock camera send exception", "send");
            AMS.MEL.IR.Close (Stream);
            AMS.MEL.Close (Parent);
      end;
   end Test_Submission_Errors;

   --  Held completion through the deterministic provider promise gate: timeout does not
   --  consume, request/channel/Session closure before completion does not cancel, and
   --  Session admission refuses a second pending request.
   procedure Test_Held (Provider_Path : String) is
      package IC renames Interfaces.C;
      use type IC.int;
      use type System.Address;
      function Dlopen (Path : IC.char_array; Flags : IC.int) return System.Address
      with Import, Convention => C, External_Name => "dlopen";
      function Dlsym (Handle : System.Address; Name : IC.char_array) return System.Address
      with Import, Convention => C, External_Name => "dlsym";
      function Dlclose (Handle : System.Address) return IC.int
      with Import, Convention => C, External_Name => "dlclose";
      type Release_Function is access function (Family, Amount : IC.unsigned) return IC.int
      with Convention => C;
      function To_Release is new Ada.Unchecked_Conversion (System.Address, Release_Function);
      Pin     : constant System.Address := Dlopen (IC.To_C (Provider_Path), 2);
      Release : constant Release_Function :=
        To_Release (Dlsym (Pin, IC.To_C ("mock_completion_release_family")));

      procedure Expect_Timeout (Request : Cam.Camera_Request) is
      begin
         declare
            Ignored : constant Cam.Camera_Result := Cam.Wait (Request, 0);
         begin
            raise Program_Error with "Ada CameraCommand Wait(0) did not time out";
         end;
      exception
         when AMS.MEL.IR.Timeout_Error =>
            null;
      end Expect_Timeout;
   begin
      Require (Pin /= System.Null_Address and then Release /= null, "provider gate");
      --  C2: timeout, then request Close + channel Close + Session Close while pending.
      declare
         Parent  : AMS.MEL.Session :=
           AMS.MEL.Open_With_Options
             (Provider_Path, "camera-held", Options => (Max_Async_Requests => 1));
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
      begin
         C2.Enable (Channel);
         declare
            Request : Cam.Camera_Request := C2.Submit_Camera_Command (Channel, Ordinary (21));
         begin
            Expect_Timeout (Request);
            begin
               declare
                  Refused : Cam.Camera_Request := C2.Submit_Camera_Command (Channel, Ordinary (22));
               begin
                  Cam.Close (Refused);
                  raise Program_Error with "Ada CameraCommand admission not enforced";
               end;
            exception
               when Error : AMS.MEL.Resource_Exhausted =>
                  Require
                    (Ada.Exceptions.Exception_Message (Error) = "async request limit reached",
                     "admission");
            end;
            Expect_Timeout (Request);
            Require (Release (6, 1) = 1, "C2 release");
            Check_Echo (Request, Ordinary (21), True);
            Cam.Close (Request);
         end;
         declare
            Observer : Cam.Camera_Request := C2.Submit_Camera_Command (Channel, Ordinary (23));
         begin
            C2.Close (Channel);
            AMS.MEL.Close (Parent);
            Expect_Timeout (Observer);
            Require (Release (6, 1) = 1, "C2 pending release");
            Check_Echo (Observer, Ordinary (23), True);
            Cam.Close (Observer);
         end;
      end;
      --  Image: Stop and Close with a pending request; result still delivered.
      declare
         Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "camera-held");
         Stream   : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
         Observer : Cam.Camera_Request := Image.Submit_Camera_Command (Stream, Ordinary (31));
         Closed   : Cam.Camera_Request := Image.Submit_Camera_Command (Stream, Ordinary (32));
      begin
         Cam.Close (Closed);
         AMS.MEL.IR.Stop (Stream);
         AMS.MEL.IR.Close (Stream);
         AMS.MEL.Close (Parent);
         Expect_Timeout (Observer);
         Require (Release (7, 2) = 1, "Image release");
         Check_Echo (Observer, Ordinary (31), True);
         Cam.Close (Observer);
      end;
      Require (Dlclose (Pin) = 0, "provider unpin");
   end Test_Held;

   --  Opt-in Image CameraCommandResp owner: complete fields, ordering, values independent of
   --  owner/stream/Session, one-shot, stopped state and counters.
   procedure Test_Callback (Provider_Path : String) is
      Kept : Cam.Camera_Command_Response;
   begin
      declare
         Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "camera-callback-rich");
         Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
         Owner  : Metadata.Camera_Response_Stream := Metadata.Open_Camera_Response (Stream, 4);
         Counts : constant Metadata.Metadata_Counters := Metadata.Camera_Response_Counters (Owner);
      begin
         Require
           (Counts.Events_Received = 3
            and then Counts.Events_Dropped_Queue_Full = 0
            and then Counts.Malformed_Or_Unsupported = 0,
            "callback counters");
         Require
           (Metadata.Receive_Camera_Response (Owner)
            = (ID => 1, Size => 0, Timestamp_NS => Cam.Nanoseconds'First, Command_Success => True),
            "first response");
         Require
           (Metadata.Receive_Camera_Response (Owner)
            = (ID              => 16#FFFF_FFFF#,
               Size            => Cam.Unsigned_32'Last,
               Timestamp_NS    => Cam.Nanoseconds'Last,
               Command_Success => False),
            "second response");
         Kept := Metadata.Receive_Camera_Response (Owner);
         begin
            declare
               Ignored : constant Cam.Camera_Command_Response :=
                 Metadata.Receive_Camera_Response (Owner, 1);
            begin
               raise Program_Error with "Ada CameraCommandResp empty queue did not time out";
            end;
         exception
            when AMS.MEL.IR.Timeout_Error =>
               null;
         end;
         begin
            declare
               Again : Metadata.Camera_Response_Stream := Metadata.Open_Camera_Response (Stream, 4);
            begin
               Metadata.Close (Again);
               raise Program_Error with "Ada CameraCommandResp registered twice";
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         AMS.MEL.IR.Stop (Stream);
         begin
            declare
               Ignored : constant Cam.Camera_Command_Response :=
                 Metadata.Receive_Camera_Response (Owner, 1_000);
            begin
               raise Program_Error with "Ada CameraCommandResp did not stop";
            end;
         exception
            when AMS.MEL.IR.Stream_Stopped =>
               null;
         end;
         Metadata.Close (Owner);
         Metadata.Close (Owner);
         AMS.MEL.IR.Close (Stream);
         AMS.MEL.Close (Parent);
      end;
      Require
        (Kept = (ID => 0, Size => 7, Timestamp_NS => -1, Command_Success => True),
         "response independent of owners");
   end Test_Callback;

   --  Without programmable camera control the opt-in owner reports Not_Supported while the
   --  existing Image metadata owner and stream keep working.
   procedure Test_Callback_Not_Supported (Provider_Path : String) is
      Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "image-metadata-sync");
      Stream   : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Existing : Metadata.Metadata_Stream := Metadata.Open (Stream, 4);
   begin
      begin
         declare
            Ignored : Metadata.Camera_Response_Stream := Metadata.Open_Camera_Response (Stream, 2);
         begin
            raise Program_Error with "Ada CameraCommandResp NotSupported not reported";
         end;
      exception
         when Cam.Not_Supported =>
            null;
      end;
      declare
         Event : constant Metadata.Metadata_Event := Metadata.Receive (Existing, 1_000);
      begin
         Require (Metadata.Kind (Event) = Metadata.Bad_Pixel_List_Event, "existing metadata");
      end;
      Metadata.Close (Existing);
      AMS.MEL.IR.Start (Stream);
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Test_Callback_Not_Supported;

   --  Synchronous delivery during send; the callback is independent of the request future.
   procedure Test_Callback_Synchronous (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "camera-sync");
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Owner  : Metadata.Camera_Response_Stream := Metadata.Open_Camera_Response (Stream, 2);
   begin
      declare
         Request : Cam.Camera_Request := Image.Submit_Camera_Command (Stream, Ordinary (16#5151#));
      begin
         Require
           (Metadata.Receive_Camera_Response (Owner)
            = (ID => 16#5151#, Size => 77, Timestamp_NS => -77, Command_Success => True),
            "synchronous callback");
         Check_Echo (Request, Ordinary (16#5151#), True);
         Cam.Close (Request);
      end;
      Metadata.Close (Owner);
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Test_Callback_Synchronous;

   procedure Run (Provider_Path : String) is
   begin
      Cam.Testing.Check_Representation;
      Test_C2 (Provider_Path, "camera-echo", True);
      Test_C2 (Provider_Path, "camera-unsatisfied", False);
      Test_Image (Provider_Path, "camera-echo", True);
      Test_Image (Provider_Path, "camera-unsatisfied", False);
      Test_Outcomes (Provider_Path, Image_Family => False);
      Test_Outcomes (Provider_Path, Image_Family => True);
      Test_Submission_Errors (Provider_Path);
      Test_Held (Provider_Path);
      Test_Callback (Provider_Path);
      Test_Callback_Not_Supported (Provider_Path);
      Test_Callback_Synchronous (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada IR C2/Image CameraCommand and CameraCommandResp contract");
   end Run;
end AMS_MEL_IR_Camera_Command_Tests;
