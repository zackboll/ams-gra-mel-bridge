with AMS.MEL.IR.Channel;
private with Ada.Strings.Unbounded;

package AMS.MEL.IR.Camera is
   --  Canonical owned values and request result for the pinned upstream CameraCommand and
   --  CameraCommandResp (@RequiredIfCameraCtrl). One definition is shared by the independent
   --  C2.Submit_Camera_Command and Image.Submit_Camera_Command operations and by the opt-in
   --  Image.Metadata CameraCommandResp owner. No physical limits, timestamp normalization or
   --  command substitution are applied by the bridge.

   Not_Supported : exception;
   --  The provider answered NotSupported for the optional Image CameraCommandResp
   --  registration; existing Image metadata and stream owners are unaffected.

   subtype Command_ID is AMS.MEL.IR.Channel.Command_ID;
   --  Canonical unsigned 32-bit command identifier; all bits are preserved.

   type Unsigned_32 is mod 2**32 with Size => 32;
   --  Unsigned 32-bit pixel, rate, offset, stage or size value, copied without range limits.

   type Nanoseconds is range -(2**63) .. (2**63) - 1 with Size => 64;
   --  Signed 64-bit nanosecond count; no clock domain or epoch is asserted by the bridge.

   type Image_Type is (Staring, Scanning);
   --  Exactly the two meaningful pinned ImageType imaging modes. The upstream Reserved13
   --  discriminator is deliberately not representable and is never sent by this binding.
   --  @enum Staring Staring framing mode, upstream value 0.
   --  @enum Scanning Scanning framing mode, upstream value 1.

   type Scan_Direction is (Negative_To_Positive_Azimuth, Positive_To_Negative_Azimuth);
   --  Exactly the two pinned ScanDir alternatives.
   --  @enum Negative_To_Positive_Azimuth Upstream NegToPosAz, value 0 and the upstream default.
   --  @enum Positive_To_Negative_Azimuth Upstream PosToNegAz, value 1.

   type Camera_Command is record
      ID                  : Command_ID := 0;
      System_Time_NS      : Nanoseconds := 0;
      Kind                : Image_Type := Staring;
      Width               : Unsigned_32 := 0;
      Height              : Unsigned_32 := 0;
      Frame_Rate_Hz       : Unsigned_32 := 0;
      Integration_Time_NS : Nanoseconds := 0;
      Row_Offset          : Unsigned_32 := 0;
      Column_Offset       : Unsigned_32 := 0;
      Direction           : Scan_Direction := Negative_To_Positive_Azimuth;
      Number_Of_Stages    : Unsigned_32 := 0;
   end record;
   --  Owned copy of all eleven CameraCommand fields; defaults equal the upstream defaults.
   --  @field ID Unique command identifier supplied by the caller.
   --  @field System_Time_NS System time for this command, signed nanoseconds.
   --  @field Kind Requested framing mode.
   --  @field Width Requested window width in pixels; must be within the system upstream.
   --  @field Height Requested window height in pixels; must be within the system upstream.
   --  @field Frame_Rate_Hz Requested frame rate in hertz.
   --  @field Integration_Time_NS Requested integration time, signed nanoseconds.
   --  @field Row_Offset Row offset of the frame start in sensor coordinates.
   --  @field Column_Offset Column offset of the frame start in sensor coordinates.
   --  @field Direction When scanning, the direction to scan.
   --  @field Number_Of_Stages When scanning, the number of TDI stages to use.

   type Camera_Command_Response is record
      ID              : Command_ID := 0;
      Size            : Unsigned_32 := 0;
      Timestamp_NS    : Nanoseconds := 0;
      Command_Success : Boolean := False;
   end record;
   --  Owned copy of all four CameraCommandResp fields, independent of native owners.
   --  @field ID Command identifier reported by the MFA.
   --  @field Size Upstream size value, copied without interpretation.
   --  @field Timestamp_NS Response timestamp, signed nanoseconds.
   --  @field Command_Success True when the MFA satisfies all requested parameters. False is a
   --  normal completed response, never an asynchronous MEL rejection.

   subtype Outcome is AMS.MEL.IR.Channel.Outcome;
   use type Outcome;
   --  Success (a response, whose Command_Success may be False) or explicit MEL rejection.
   subtype Error_Code is AMS.MEL.IR.Channel.Error_Code;
   --  Known MEL rejection categories; unknown codes raise Provider_Error instead.

   type Camera_Result is private;
   --  Owned terminal request result: a response or an explicit rejection. Provider
   --  exceptions, null successful responses, unknown error codes and deferred cleanup failure
   --  raise exceptions rather than becoming results.
   function Status (Result : Camera_Result) return Outcome;
   --  Read the terminal category without provider access.
   --  @param Result Owned terminal result.
   --  @return Success or Rejected.
   function Response (Result : Camera_Result) return Camera_Command_Response
   with Pre => Status (Result) = AMS.MEL.IR.Channel.Success;
   --  Read the complete response of a successful result.
   --  @param Result Successful terminal result.
   --  @return Owned response, which may report Command_Success False.
   function Rejection_Code (Result : Camera_Result) return Error_Code
   with Pre => Status (Result) = AMS.MEL.IR.Channel.Rejected;
   --  Read the MEL rejection category of a rejected result.
   --  @param Result Rejected terminal result.
   --  @return Mapped upstream error code.
   function Description (Result : Camera_Result) return String
   with Pre => Status (Result) = AMS.MEL.IR.Channel.Rejected;
   --  Copy the complete provider rejection description.
   --  @param Result Rejected terminal result.
   --  @return Owned description, retried from the native cache when long.

   type Camera_Request is new AMS.MEL.IR.Camera_Request_Owner with private;
   --  Limited request owner returned by C2/Image Submit_Camera_Command. Pending native work
   --  retains its channel, Session and provider library after request/channel/Session
   --  closure; finalization is non-raising and Close never cancels provider work.

   function Is_Open (Request : Camera_Request) return Boolean;
   --  Test local handle presence without polling completion.
   --  @param Request Request owner to inspect.
   --  @return True while a native request handle is held.
   function Wait (Request : Camera_Request; Timeout_Milliseconds : Natural) return Camera_Result;
   --  Wait finitely for the terminal result. Timeout raises Timeout_Error and neither consumes
   --  nor cancels the request; repeated terminal waits return the cached result.
   --  @param Request Open request; must not race Close.
   --  @param Timeout_Milliseconds Maximum wait; zero polls.
   --  @return Owned terminal response or explicit rejection.
   --  @exception Timeout_Error The request is still pending.
   --  @exception Provider_Error Closed owner, provider/future exception, null response, unknown
   --  error code, deferred cleanup failure or native failure.
   procedure Close (Request : in out Camera_Request);
   --  Idempotently release only the public observer; pending work continues.
   --  @param Request Request owner; serialize against Wait.
   --  @exception Provider_Error Native close failed.

private
   package US renames Ada.Strings.Unbounded;
   type Camera_Request is new AMS.MEL.IR.Camera_Request_Owner with record
      null;
   end record;

   type Camera_Result is record
      Result_Status   : Outcome := AMS.MEL.IR.Channel.Success;
      Result_Response : Camera_Command_Response;
      Result_Code     : Error_Code := AMS.MEL.IR.Channel.None;
      Result_Text     : US.Unbounded_String;
   end record;

end AMS.MEL.IR.Camera;
