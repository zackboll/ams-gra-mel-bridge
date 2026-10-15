private with Ada.Containers.Vectors;
private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with Interfaces.C;
private with AMS.MEL_C_API;
with Interfaces;

package AMS.MEL.RF is
   --  RF DataMEL ownership and copied MFA, face, installation and transmit-power
   --  characteristics. This provider family is independent of an IR Session.
   --  Capability reports do not demonstrate active RF transmission or real
   --  hardware qualification. Child packages provide separately scoped services.
   --  Serialize operations on one DataMEL owner against each other and Close.
   --  Snapshot values contain no live provider reference and may outlive it.

   type Data_MEL is limited private;
   --  Limited controlled owner acquired from the RF provider factory. A default
   --  object is closed. Prefer explicit Close for error reporting; finalization
   --  attempts non-raising cleanup. Native asynchronous children can retain the
   --  provider independently of this public Ada owner's lifetime.

   function Open (Library_Path : String; Configuration : String) return Data_MEL;
   --  Load a provider library and call its RF DataMEL factory, not IR factories.
   --  Strings are passed as UTF-8 bytes without transcoding; this open path does
   --  not validate UTF-8. Compatible provider C++ ABI remains required behind the facade.
   --  @param Library_Path Shared-library path containing the RF factory.
   --  @param Configuration Text passed to the provider factory; its syntax and
   --  interpretation are provider-defined, not a bridge configuration schema.
   --  @return Limited owner of the acquired RF DataMEL.
   --  @exception Constraint_Error Either input contains an embedded NUL, rejected
   --  before loading the provider.
   --  @exception Provider_Error Native validation, loading, symbol lookup or
   --  factory acquisition fails, including a null factory result.

   function Is_Open (Object : Data_MEL) return Boolean;
   --  Inspect local native-handle presence without querying provider health.
   --  @param Object RF DataMEL owner whose local state is inspected.
   --  @return True if the owner holds a native handle, not proof of RF activity.

   procedure Close (Object : in out Data_MEL);
   --  Consume the public owner; closing an already closed owner is harmless.
   --  Native shutdown is deferred while asynchronous create-request/endpoint
   --  children retain claims. Otherwise shutdown precedes provider destruction.
   --  Throwing shutdown retains the provider graph rather than unloading unsafe
   --  code; registered callback code can also remain retained for process life.
   --  This is neither cancellation nor destruction of all independent children.
   --  @param Object Owner to close; serialize against operations on this object.
   --  @exception Provider_Error Native cleanup or immediate shutdown fails;
   --  the public handle is consumed even when shutdown reports failure.

   function Query_Provider_Version (Object : Data_MEL) return AMS.MEL.Provider_Version;
   --  Copy provider API/library numbers and validated UTF-8 vendor/description
   --  text using the native size/copy protocol. This is not ABI negotiation.
   --  @param Object Open RF DataMEL; serialize against its closure.
   --  @return Ada-owned version snapshot usable after DataMEL closure, distinct
   --  from the consumer-side bridge ABI version.
   --  @exception Provider_Error Closed owner, provider/native failure, invalid
   --  text or invalid/unrepresentable required string lengths.

   function Quantize_Duration
     (Data : Data_MEL; Femtoseconds : Interfaces.Integer_64) return Interfaces.Integer_64;
   --  Delegate duration quantization to the provider's MFA information method.
   --  The bridge applies no rounding, clamping or scheduler-resolution arithmetic;
   --  a universal rounding direction is not established by the pinned interface.
   --  @param Data Open RF DataMEL used to acquire the provider MFA information.
   --  @param Femtoseconds Unquantized signed 64-bit duration in femtoseconds,
   --  passed unchanged, including negative or zero input.
   --  @return Provider-quantized signed 64-bit femtosecond count, copied unchanged.
   --  @exception Provider_Error Closed owner, unavailable MFA information or
   --  provider/native failure during quantization.

   --  Ada-owned point-in-time value; no native/provider handle survives return.
   type Physical_Data is private;
   --  Ada-owned point-in-time face dimensions and installation geometry. No
   --  native/provider handle survives a snapshot return. Scalars are copied
   --  without coordinate conversion, normalization or physical-range validation.

   function Snapshot_Physical_Data
     (Data : Data_MEL; Face_ID : Interfaces.Unsigned_32) return Physical_Data;
   --  Query and copy one face's physical data, including complete location text.
   --  @param Data Open RF DataMEL; serialize the query against other owner operations.
   --  @param Face_ID Provider-assigned face identifier, not a one-based list index.
   --  @return Independent Ada-owned geometry and installation-location snapshot.
   --  @exception Provider_Error Closed owner, missing MFA information, invalid
   --  UTF-8 text or native/provider query/copy failure.

   function Antenna_Height_M (Value : Physical_Data) return Long_Float;
   --  Read antenna height along its short axis.
   --  @param Value Ada-owned physical-data snapshot; no provider call is made.
   --  @return Short-axis antenna dimension in meters.

   function Antenna_Width_M (Value : Physical_Data) return Long_Float;
   --  Read antenna width along its long axis.
   --  @param Value Ada-owned physical-data snapshot; no provider call is made.
   --  @return Long-axis antenna dimension in meters.

   function Lattice_Angle_Radians (Value : Physical_Data) return Long_Float;
   --  Read the relative orientation of antenna elements in their grid.
   --  @param Value Ada-owned physical-data snapshot; no provider call is made.
   --  @return Lattice angle in radians, used upstream to compute mainbeam shape.

   function Location_Offset_X_M (Value : Physical_Data) return Long_Float;
   --  Read the forward installation offset from the host navigational center.
   --  @param Value Ada-owned physical-data snapshot; no provider call is made.
   --  @return Forward displacement in meters.

   function Location_Offset_Y_M (Value : Physical_Data) return Long_Float;
   --  Read the rightward installation offset from the host navigational center.
   --  @param Value Ada-owned physical-data snapshot; no provider call is made.
   --  @return Rightward displacement in meters.

   function Location_Offset_Z_M (Value : Physical_Data) return Long_Float;
   --  Read the downward installation offset from the host navigational center.
   --  @param Value Ada-owned physical-data snapshot; no provider call is made.
   --  @return Downward displacement in meters.

   function Orientation_Roll_Radians (Value : Physical_Data) return Long_Float;
   --  Read the face installation orientation roll angle.
   --  @param Value Ada-owned physical-data snapshot; no provider call is made.
   --  @return Roll angle in radians in the host navigational-center frame; no
   --  normalization or coordinate transformation. Upstream specifies yaw/pitch/roll
   --  sequence; orientation and boresight remain separate values.

   function Orientation_Pitch_Radians (Value : Physical_Data) return Long_Float;
   --  Read the face installation orientation pitch angle.
   --  @param Value Ada-owned physical-data snapshot; no provider call is made.
   --  @return Pitch angle in radians in the host navigational-center frame; no
   --  normalization or coordinate transformation. Upstream specifies yaw/pitch/roll
   --  sequence; orientation and boresight remain separate values.

   function Orientation_Yaw_Radians (Value : Physical_Data) return Long_Float;
   --  Read the face installation orientation yaw angle.
   --  @param Value Ada-owned physical-data snapshot; no provider call is made.
   --  @return Yaw angle in radians in the host navigational-center frame; no
   --  normalization or coordinate transformation. Upstream specifies yaw/pitch/roll
   --  sequence; orientation and boresight remain separate values.

   function Boresight_Roll_Radians (Value : Physical_Data) return Long_Float;
   --  Read the face installation boresight roll angle.
   --  @param Value Ada-owned physical-data snapshot; no provider call is made.
   --  @return Roll angle in radians in the host navigational-center frame; no
   --  normalization or coordinate transformation. Upstream specifies yaw/pitch/roll
   --  sequence; orientation and boresight remain separate values.

   function Boresight_Pitch_Radians (Value : Physical_Data) return Long_Float;
   --  Read the face installation boresight pitch angle.
   --  @param Value Ada-owned physical-data snapshot; no provider call is made.
   --  @return Pitch angle in radians in the host navigational-center frame; no
   --  normalization or coordinate transformation. Upstream specifies yaw/pitch/roll
   --  sequence; orientation and boresight remain separate values.

   function Boresight_Yaw_Radians (Value : Physical_Data) return Long_Float;
   --  Read the face installation boresight yaw angle.
   --  @param Value Ada-owned physical-data snapshot; no provider call is made.
   --  @return Yaw angle in radians in the host navigational-center frame; no
   --  normalization or coordinate transformation. Upstream specifies yaw/pitch/roll
   --  sequence; orientation and boresight remain separate values.

   function Location_Key (Value : Physical_Data) return String;
   --  Copy the logical mounting-location reference, such as a pylon or bay key.
   --  @param Value Ada-owned physical-data snapshot; no provider call is made.
   --  @return Independent Ada String of validated UTF-8 location-key bytes.

   function Location_System_Name (Value : Physical_Data) return String;
   --  Copy the external naming system managing the mounting-location key.
   --  @param Value Ada-owned physical-data snapshot; no provider call is made.
   --  @return Independent UTF-8 naming-system text, not a resolved location.

   type Job_Data_Format is mod 2**32 with Size => 32;
   --  Native format identifier preserved in all 32 bits, including unknown
   --  reported values. Constants name formats, not implemented receive methods.
   --  Provider-supported format lists do not promise an Ada ProductRx binding.

   Direct_INT8 : constant Job_Data_Format := 0;
   --  Direct samples with signed eight-bit integer elements.

   Direct_INT16 : constant Job_Data_Format := 1;
   --  Direct samples with signed sixteen-bit integer elements.

   Complex_INT8 : constant Job_Data_Format := 2;
   --  Complex samples with signed eight-bit real/imaginary (IQ) component pairs.

   Complex_INT16 : constant Job_Data_Format := 3;
   --  Complex samples with signed sixteen-bit real/imaginary component pairs.
   --  The pinned variant uses int16 components despite inconsistent eight-bit prose.

   AMS_VITA_Small : constant Job_Data_Format := 4;
   --  AMS VITA signal-data packet objects with 512-byte storage capacity, not
   --  serialized length or signal-payload size. Packet headers determine length.

   AMS_VITA_Medium : constant Job_Data_Format := 5;
   --  AMS VITA signal-data packet objects with 1,024-byte storage capacity;
   --  valid serialized length is header-derived and includes non-payload overhead.

   AMS_VITA_Large : constant Job_Data_Format := 6;
   --  AMS VITA signal-data packet objects with 8,192-byte storage capacity;
   --  capacity is neither valid serialized length nor available signal payload.

   AMS_VITA_Extra_Large : constant Job_Data_Format := 7;
   --  AMS VITA signal-data packet objects with 32,768-byte storage capacity;
   --  valid serialized length is header-derived, with packet overhead included.

   PDW_Type1 : constant Job_Data_Format := 8;
   --  Pulse-descriptor-word type-one identifier. No corresponding alternative
   --  exists in the pinned data-pointer variant; current ProductRx rejects it.

   PDW_Type2 : constant Job_Data_Format := 9;
   --  Pulse-descriptor-word type-two identifier, not a current ProductRx receive
   --  format or a defined alternative of the pinned data-pointer variant.

   PDW_Type3 : constant Job_Data_Format := 10;
   --  Pulse-descriptor-word type-three identifier; no current ProductRx binding
   --  or pinned data-pointer alternative is provided for it.

   LF_Type1 : constant Job_Data_Format := 11;
   --  Local-function-defined type-one identifier. Declaring it does not supply
   --  a pinned data-pointer alternative or a ProductRx receive implementation.

   LF_Type2 : constant Job_Data_Format := 12;
   --  Local-function-defined type-two identifier, unsupported by the current
   --  ProductRx receive API and absent from the pinned data-pointer variant.

   LF_Type3 : constant Job_Data_Format := 13;
   --  Local-function-defined type-three identifier, not an implemented ProductRx
   --  receive format or a pinned data-pointer variant alternative.

   type Frequency_Range is record
      Min_Hz : Long_Float;
      Max_Hz : Long_Float;
   end record;
   --  Copied provider frequency bounds in hertz. No sorting, clamping, unit
   --  conversion or check that minimum is below maximum is imposed here.
   --  @field Min_Hz Provider-reported lower frequency bound in hertz.
   --  @field Max_Hz Provider-reported upper frequency bound in hertz.

   type Face_Info is private;
   --  Ada-owned face capability/timing snapshot, including provider-assigned ID
   --  and ordered frequency-range collections. It is not a live face resource.

   type Tx_Power_Mode is private;
   --  Copied characteristics of a provider-assigned transmit-power mode.
   --  Upstream warns that using a mode outside its stated characteristics,
   --  explicitly its frequency ranges, may damage hardware. This value reports
   --  limits; obtaining it does not validate a future transmit Job against them.

   type Tx_Power_Mode_List is private;
   --  Ada-owned ordered mode snapshots; default construction is an empty list.
   --  Provider order and duplicate mode IDs are preserved without normalization.

   function Snapshot_Tx_Power_Modes
     (Data : Data_MEL; Face_ID : Interfaces.Unsigned_32) return Tx_Power_Mode_List;
   --  Copy the provider's complete reported mode collection for one face.
   --  The query does not first gate on the face's supports-transmit flag.
   --  @param Data Open RF DataMEL, serialized against other owner operations.
   --  @param Face_ID Provider-assigned face identifier passed unchanged.
   --  @return Ada-owned ordered modes and nested ranges; an empty list is valid.
   --  @exception Provider_Error Closed owner, missing MFA information or
   --  provider/native query, representation or copy failure.

   function Snapshot_Tx_Power_Mode
     (Data : Data_MEL; Face_ID, Power_Mode_ID : Interfaces.Unsigned_32) return Tx_Power_Mode;
   --  Invoke the provider's direct mode query, not a search in a cached list.
   --  Returned IDs are provider-authoritative and need not match the requested ID;
   --  no generic not-found convention is specified by the pinned method.
   --  @param Data Open RF DataMEL, serialized against other owner operations.
   --  @param Face_ID Provider-assigned face identifier passed unchanged.
   --  @param Power_Mode_ID Requested provider mode ID, not a universal power level.
   --  @return Ada-owned mode returned by the provider, including its actual ID.
   --  @exception Provider_Error Closed owner, missing MFA information, failed
   --  query/copy or a direct native snapshot not containing exactly one mode.

   function Tx_Power_Mode_Count (Value : Tx_Power_Mode_List) return Natural;
   --  Count the mode snapshots in provider collection order.
   --  @param Value Ada-owned mode-list snapshot; no provider call is made.
   --  @return Number of stored entries, including duplicate IDs; zero for an empty list.

   function Tx_Power_Mode_At (Value : Tx_Power_Mode_List; Index : Positive) return Tx_Power_Mode;
   --  Copy one mode in stored order.
   --  @param Value Ada-owned source snapshot, independent of provider lifetime.
   --  @param Index Positive one-based position no greater than the stored count.
   --  @return Ada-owned mode with copied nested frequency ranges.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   function Tx_Power_Mode_ID (Value : Tx_Power_Mode) return Interfaces.Unsigned_32;
   --  Read the identifier assigned by the provider to this returned mode.
   --  @param Value Ada-owned transmit-mode snapshot; no provider call is made.
   --  @return Actual provider-assigned mode ID, not a universally fixed power setting.

   function Is_Linear_Operation (Value : Tx_Power_Mode) return Boolean;
   --  Read whether the mode specifies linear rather than compressed operation.
   --  @param Value Ada-owned transmit-mode snapshot; no provider call is made.
   --  @return True for linear operation, False for compressed operation.

   function Tx_Power_Level (Value : Tx_Power_Mode) return Interfaces.Unsigned_32;
   --  Read the power-level identifier associated with this mode.
   --  @param Value Ada-owned transmit-mode snapshot; no provider call is made.
   --  @return Unsigned provider level; zero is reserved upstream as the default level.
   --  This is distinct from the mode ID and is not a watt or dBm measurement.

   function Tx_Frequency_Range_Count (Value : Tx_Power_Mode) return Natural;
   --  Count the reported frequency ranges valid for this transmit mode.
   --  @param Value Ada-owned transmit-mode snapshot; no provider call is made.
   --  @return Number of stored ranges; no sorting, merging or deduplication.

   function Tx_Frequency_Range_At (Value : Tx_Power_Mode; Index : Positive) return Frequency_Range;
   --  Copy one valid transmit-mode frequency range in stored order.
   --  @param Value Ada-owned source snapshot, independent of provider lifetime.
   --  @param Index Positive one-based position no greater than the stored count.
   --  @return Copied hertz bounds. Using the mode outside its reported ranges risks
   --  hardware damage; this accessor does not enforce future Job constraints.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   function Max_Tx_Duty_Factor (Value : Tx_Power_Mode) return Long_Float;
   --  Read the maximum transmit duty allowed for this mode.
   --  @param Value Ada-owned transmit-mode snapshot; no provider call is made.
   --  @return Dimensionless provider-reported duty factor, without clamping or validation.

   function Max_Tx_Pulse_Width_NS (Value : Tx_Power_Mode) return Interfaces.Integer_64;
   --  Read the maximum transmit pulse duration allowed for this mode.
   --  @param Value Ada-owned transmit-mode snapshot; no provider call is made.
   --  @return Signed 64-bit nanosecond count, not femtoseconds; copied unchanged.

   function Max_Tx_Attenuation (Value : Tx_Power_Mode) return Long_Float;
   --  Read the maximum available attenuation at the snapshot time.
   --  @param Value Ada-owned transmit-mode snapshot; no provider call is made.
   --  @return Provider scalar, potentially changed by runtime calibration. The pinned
   --  declaration specifies no attenuation unit; the bridge invents none.

   function Tx_Attenuation_Step_Size (Value : Tx_Power_Mode) return Long_Float;
   --  Read the minimum commandable increment of transmit attenuation.
   --  @param Value Ada-owned transmit-mode snapshot; no provider call is made.
   --  @return Provider-reported attenuation step; no unit, rounding or policy is added.

   function Face_ID (Value : Face_Info) return Interfaces.Unsigned_32;
   --  Read the face identifier in this copied capability record.
   --  @param Value Ada-owned face-capability snapshot; no provider call is made.
   --  @return Provider-assigned face ID, not its one-based position in the snapshot.

   function Supports_Receive (Value : Face_Info) return Boolean;
   --  Read the provider-reported receive capability for this face.
   --  @param Value Ada-owned face-capability snapshot; no provider call is made.
   --  @return True if the provider reports receive support; not proof of active collection.

   function Supports_Transmit (Value : Face_Info) return Boolean;
   --  Read the provider-reported transmit capability for this face.
   --  @param Value Ada-owned face-capability snapshot; no provider call is made.
   --  @return True if transmit is reported supported, not evidence of active transmission
   --  or qualification on real RF hardware.

   function Requires_Endpoint_Association (Value : Face_Info) return Boolean;
   --  Read whether this face requires endpoint/data-pipe association.
   --  @param Value Ada-owned face-capability snapshot; no provider call is made.
   --  @return Provider requirement flag; this query does not perform the association.

   function AGC_Processing_Time_FS (Value : Face_Info) return Interfaces.Integer_64;
   --  Read AGC processing time required after AGC data collection.
   --  @param Value Ada-owned face-capability snapshot; no provider call is made.
   --  @return Signed femtoseconds used upstream to determine ignored post-AGC iterations.

   function Min_Job_Request_Lead_Time_FS (Value : Face_Info) return Interfaces.Integer_64;
   --  Read how far ahead a JobRequest must arrive for scheduler adjudication.
   --  @param Value Ada-owned face-capability snapshot; no provider call is made.
   --  @return Minimum request lead time in signed femtoseconds.

   function Max_Job_Request_Lead_Time_FS (Value : Face_Info) return Interfaces.Integer_64;
   --  Read how far ahead of job start the MFA is willing to accept a request.
   --  @param Value Ada-owned face-capability snapshot; no provider call is made.
   --  @return Maximum request lead time in signed femtoseconds; together with the minimum
   --  it describes the request scheduling-admission window.

   function Min_Job_Detail_Lead_Time_FS (Value : Face_Info) return Interfaces.Integer_64;
   --  Read the required interval delivery lead relative to actual job/interval start.
   --  @param Value Ada-owned face-capability snapshot; no provider call is made.
   --  @return Signed femtoseconds before job start and each successive interval start.

   function Tx_Rx_Switching_Time_FS (Value : Face_Info) return Interfaces.Integer_64;
   --  Read the minimum gap from a transmit event to a subsequent receive event.
   --  @param Value Ada-owned face-capability snapshot; no provider call is made.
   --  @return Provider-reported signed femtoseconds for TX then RX ordering.

   function Rx_Tx_Switching_Time_FS (Value : Face_Info) return Interfaces.Integer_64;
   --  Read the minimum gap from a receive event to a subsequent transmit event.
   --  @param Value Ada-owned face-capability snapshot; no provider call is made.
   --  @return Provider-reported signed femtoseconds for RX then TX ordering.

   function Tx_Tx_Switching_Time_FS (Value : Face_Info) return Interfaces.Integer_64;
   --  Read the minimum gap between a transmit event and subsequent transmit event.
   --  @param Value Ada-owned face-capability snapshot; no provider call is made.
   --  @return Provider-reported signed femtoseconds for TX then TX ordering.

   function Rx_Rx_Switching_Time_FS (Value : Face_Info) return Interfaces.Integer_64;
   --  Read the minimum gap between a receive event and subsequent receive event.
   --  @param Value Ada-owned face-capability snapshot; no provider call is made.
   --  @return Provider-reported signed femtoseconds for RX then RX ordering.

   function Rx_Frequency_Range_Count (Value : Face_Info) return Natural;
   --  Count this face's reported receive carrier-frequency ranges.
   --  @param Value Ada-owned face-capability snapshot; no provider call is made.
   --  @return Number of copied ranges in provider vector order, possibly zero.

   function Rx_Frequency_Range_At (Value : Face_Info; Index : Positive) return Frequency_Range;
   --  Copy one receive carrier-frequency range in stored order.
   --  @param Value Ada-owned source snapshot, independent of provider lifetime.
   --  @param Index Positive one-based position no greater than the stored count.
   --  @return Copied provider receive bounds in hertz, unchanged.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   function Tx_Frequency_Range_Count (Value : Face_Info) return Natural;
   --  Count this face's reported transmit carrier-frequency ranges.
   --  @param Value Ada-owned face-capability snapshot; no provider call is made.
   --  @return Number of copied ranges in provider vector order, possibly zero.

   function Tx_Frequency_Range_At (Value : Face_Info; Index : Positive) return Frequency_Range;
   --  Copy one transmit carrier-frequency range in stored order.
   --  @param Value Ada-owned source snapshot, independent of provider lifetime.
   --  @param Index Positive one-based position no greater than the stored count.
   --  @return Copied provider transmit bounds in hertz, unchanged.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   function Sample_Frequency_Range_Count (Value : Face_Info) return Natural;
   --  Count this face's reported sampling-frequency ranges.
   --  @param Value Ada-owned face-capability snapshot; no provider call is made.
   --  @return Number of copied ranges in provider vector order, possibly zero.

   function Sample_Frequency_Range_At (Value : Face_Info; Index : Positive) return Frequency_Range;
   --  Copy one sampling-frequency range in stored order.
   --  @param Value Ada-owned source snapshot, independent of provider lifetime.
   --  @param Index Positive one-based position no greater than the stored count.
   --  @return Copied provider sample-rate bounds in hertz, unchanged.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   type MFA_Info is private;
   --  Ada-owned point-in-time MFA characteristics, copied from several provider
   --  queries. Not a live reference or a promise of an atomic provider-wide view.
   --  Reported face count is kept separately from the copied face collection.

   function Snapshot_MFA_Info (Object : Data_MEL) return MFA_Info;
   --  Copy global and per-face characteristics, then release native snapshot storage.
   --  @param Object Open RF DataMEL, serialized against other owner operations.
   --  @return Independent Ada-owned capability snapshot usable after closure.
   --  @exception Provider_Error Closed owner, unavailable MFA information or
   --  invalid native spans/booleans, unrepresentable counts or provider/copy failure.
   function Reported_Num_Faces (Value : MFA_Info) return Interfaces.Unsigned_64;
   --  Read the provider's separately reported number of supported faces.
   --  @param Value Ada-owned MFA-capability snapshot; no provider call is made.
   --  @return Unsigned count from the provider, not the length of the copied face list;
   --  the two values can differ and are preserved independently.

   function Contains_Open_Additions (Value : MFA_Info) return Boolean;
   --  Read the provider's indication that open additions are included.
   --  @param Value Ada-owned MFA-capability snapshot; no provider call is made.
   --  @return Copied open-additions flag. The pinned interface gives no further generic
   --  feature inventory; this is not a bridge implementation-completeness claim.

   function Scheduler_Resolution_FS (Value : MFA_Info) return Interfaces.Integer_64;
   --  Read the finest duration quantum reported by the job scheduler.
   --  @param Value Ada-owned MFA-capability snapshot; no provider call is made.
   --  @return Signed femtosecond resolution. Upstream expects job durations to be
   --  multiples; this read neither quantizes nor validates a job timeline.

   function Max_User_Defined_Context_Bytes (Value : MFA_Info) return Interfaces.Unsigned_64;
   --  Read the provider constraint on JobInterval user-defined context size.
   --  @param Value Ada-owned MFA-capability snapshot; no provider call is made.
   --  @return Maximum reported bytes, not storage allocated or automatically registered
   --  by this package.

   function Supported_Data_Format_Count (Value : MFA_Info) return Natural;
   --  Count the copied provider-supported format identifiers.
   --  @param Value Ada-owned MFA-capability snapshot; no provider call is made.
   --  @return Number of entries copied from the provider set, not implemented Ada APIs.

   function Supported_Data_Format_At (Value : MFA_Info; Index : Positive) return Job_Data_Format;
   --  Copy one supported format identifier in stored order.
   --  @param Value Ada-owned source snapshot, independent of provider lifetime.
   --  @param Index Positive one-based position no greater than the stored count.
   --  @return Provider-reported format, including unknown 32-bit identifiers.
   --  Reporting support does not imply an implemented Ada ProductRx API.
   --  @exception Constraint_Error Index exceeds the stored collection length.

   function Face_Count (Value : MFA_Info) return Natural;
   --  Count face records actually copied from the provider face-ID set.
   --  @param Value Ada-owned MFA-capability snapshot; no provider call is made.
   --  @return Copied collection length, independent of the separately reported face count.

   function Face_At (Value : MFA_Info; Index : Positive) return Face_Info;
   --  Copy one face-capability record in stored order.
   --  @param Value Ada-owned source snapshot, independent of provider lifetime.
   --  @param Index Positive one-based position no greater than the stored count.
   --  @return Ada-owned face snapshot; set iteration yields ascending provider face IDs,
   --  which need not be contiguous or equal to the index.
   --  @exception Constraint_Error Index exceeds the stored collection length.

private
   type Physical_Data is record
      Antenna_Height_M_Value          : Long_Float := 0.0;
      Antenna_Width_M_Value           : Long_Float := 0.0;
      Lattice_Angle_Radians_Value     : Long_Float := 0.0;
      Location_Offset_X_M_Value       : Long_Float := 0.0;
      Location_Offset_Y_M_Value       : Long_Float := 0.0;
      Location_Offset_Z_M_Value       : Long_Float := 0.0;
      Orientation_Roll_Radians_Value  : Long_Float := 0.0;
      Orientation_Pitch_Radians_Value : Long_Float := 0.0;
      Orientation_Yaw_Radians_Value   : Long_Float := 0.0;
      Boresight_Roll_Radians_Value    : Long_Float := 0.0;
      Boresight_Pitch_Radians_Value   : Long_Float := 0.0;
      Boresight_Yaw_Radians_Value     : Long_Float := 0.0;
      Key, System_Name                : Ada.Strings.Unbounded.Unbounded_String;
   end record;
   pragma
     Compile_Time_Error
       (Long_Float'Size < Interfaces.C.double'Size
          or else Long_Float'Digits < Interfaces.C.double'Digits,
        "RF frequency ranges require a lossless C double representation");
   package Range_Vectors is new Ada.Containers.Vectors (Positive, Frequency_Range);
   package Format_Vectors is new Ada.Containers.Vectors (Positive, Job_Data_Format);
   type Tx_Power_Mode is record
      ID, Level                    : Interfaces.Unsigned_32 := 0;
      Linear                       : Boolean := False;
      Ranges                       : Range_Vectors.Vector;
      Duty, Attenuation, Step_Size : Long_Float := 0.0;
      Pulse_NS                     : Interfaces.Integer_64 := 0;
   end record;
   package Mode_Vectors is new Ada.Containers.Vectors (Positive, Tx_Power_Mode);
   type Tx_Power_Mode_List is record
      Modes : Mode_Vectors.Vector;
   end record;
   type Face_Info is record
      ID                                        : Interfaces.Unsigned_32 := 0;
      Receive_OK, Transmit_OK, Association      : Boolean := False;
      AGC, Min_Request, Max_Request, Min_Detail : Interfaces.Integer_64 := 0;
      Tx_Rx, Rx_Tx, Tx_Tx, Rx_Rx                : Interfaces.Integer_64 := 0;
      Rx_Ranges, Tx_Ranges, Sample_Ranges       : Range_Vectors.Vector;
   end record;
   package Face_Vectors is new Ada.Containers.Vectors (Positive, Face_Info);
   type MFA_Info is record
      Reported       : Interfaces.Unsigned_64 := 0;
      Open_Additions : Boolean := False;
      Resolution     : Interfaces.Integer_64 := 0;
      Context_Bytes  : Interfaces.Unsigned_64 := 0;
      Formats        : Format_Vectors.Vector;
      Faces          : Face_Vectors.Vector;
   end record;
   type Data_MEL is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_Data_Handle := AMS.MEL_C_API.Null_RF_Data;
   end record;
   overriding
   procedure Finalize (Object : in out Data_MEL);
end AMS.MEL.RF;
