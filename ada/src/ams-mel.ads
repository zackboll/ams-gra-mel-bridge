private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
private with Interfaces;

package AMS.MEL is
   --  Consumer-side MEL bridge foundation and IR provider-session ownership.
   --  The native facade ABI version is independent of provider API and library
   --  versions. Child packages expose only the implemented MEL service slices.
   --  Serialize operations on an owner against its closure; this package does
   --  not provide a task-safe lifetime protocol for a shared Ada Session.

   type Version is record
      Major : Natural;
      Minor : Natural;
   end record;
   --  Major and minor version of the consumer-side C facade ABI.
   --  These numbers are not the Common MEL API string or provider versions.
   --  @field Major Major ABI version reported by the linked native bridge.
   --  @field Minor Minor ABI version reported by the linked native bridge.

   function ABI_Version return Version;
   --  Query the linked native facade without loading or initializing a provider.
   --  @return Native bridge ABI major and minor numbers.
   --  @exception Program_Error The native query fails or a number cannot be
   --  represented by Natural.

   Provider_Error : exception;
   --  Native bridge or provider operation failure. This includes load, symbol,
   --  initialization and version-query failures; it is not a direct propagation
   --  of a C++ exception across the language boundary.

   Resource_Exhausted : exception;
   --  Asynchronous submission refused by bridge admission before provider send.
   --  Distinct from a provider rejecting a command. Submission wrappers do not
   --  queue or retry a refused request.

   type Async_Request_Limit is mod 2**32 with Size => 32;
   --  Session-wide asynchronous admission bound; zero selects unlimited mode.

   type Session_Options is record
      Max_Async_Requests : Async_Request_Limit := 0;
   end record;
   --  Admission configuration copied when a Session opens. It cannot be changed
   --  on that Session. A request timeout does not cancel provider work. Closing
   --  a request or waiting for it is not itself a capacity-release operation.
   --  @field Max_Async_Requests Maximum admitted outstanding asynchronous work
   --  across the Session's participating channels; zero means unlimited.

   type Provider_Version_Number is mod 2**32 with Size => 32;
   --  Complete unsigned 32-bit provider version number, without bridge decoding
   --  into major/minor fields or substitution of the facade ABI version.

   type Provider_Version is private;
   --  Ada-owned snapshot of provider API/library numbers and descriptive text.
   --  A default value has zero numbers and empty strings. Queried values remain
   --  usable after the Session closes; they contain no live provider reference.

   function API_Version (Value : Provider_Version) return Provider_Version_Number;
   --  Read the provider API number from a version snapshot.
   --  @param Value Ada-owned provider version snapshot.
   --  @return Unmodified provider API version number, not the bridge ABI version.

   function Library_Version (Value : Provider_Version) return Provider_Version_Number;
   --  Read the provider implementation-library number from a version snapshot.
   --  @param Value Ada-owned provider version snapshot.
   --  @return Unmodified provider library version number.

   function Vendor (Value : Provider_Version) return String;
   --  Copy the vendor text stored in a version snapshot.
   --  @param Value Ada-owned provider version snapshot.
   --  @return Independent Ada String of UTF-8 bytes, without transcoding.

   function Description (Value : Provider_Version) return String;
   --  Copy the library description stored in a version snapshot.
   --  @param Value Ada-owned provider version snapshot.
   --  @return Independent Ada String of UTF-8 bytes, without transcoding.

   type Session is limited private;
   --  Unique Ada owner of an initialized IR provider session. A default Session
   --  is closed. Explicit Close releases its native handle; controlled
   --  finalization is a non-raising cleanup fallback. Native child resources and
   --  outstanding requests can retain provider objects and code after closure.
   --  Neither closure nor finalization promises cancellation or physical
   --  completion of every asynchronous provider operation.

   function Open
     (Library_Path : String; Instance : String; Aperture_Config_ID : String := "") return Session;
   --  Load the provider, obtain its API manager and IR Control, and initialize
   --  the Control, using the default unlimited admission configuration.
   --  Strings are supplied as UTF-8 bytes without transcoding; this open path
   --  rejects embedded NUL but does not validate UTF-8. C++ ABI compatibility
   --  with the provider is still required behind the C facade.
   --  @param Library_Path Provider shared-library path passed to the loader.
   --  @param Instance Provider instance passed to both factory functions.
   --  @param Aperture_Config_ID Configuration identifier passed to Control init;
   --  the empty default is supported and its meaning is provider-defined.
   --  @return Limited owner of the successfully initialized provider session.
   --  @exception Constraint_Error Any input contains an embedded NUL; rejected
   --  before loading the provider.
   --  @exception Provider_Error Loading, factory lookup, object acquisition or
   --  initialization fails, including a non-Success initialization result.

   function Open_With_Options
     (Library_Path       : String;
      Instance           : String;
      Options            : Session_Options;
      Aperture_Config_ID : String := "") return Session;
   --  Open an IR Session with a copied asynchronous admission configuration.
   --  Loading, initialization and string handling are the same as Open. A bound
   --  limits bridge admission, not provider acceptance or completion time.
   --  @param Library_Path Provider shared-library path passed to the loader.
   --  @param Instance Provider instance passed to both factory functions.
   --  @param Options Session-wide admission limit; zero is unlimited. Refused
   --  asynchronous submissions are not queued or retried.
   --  @param Aperture_Config_ID Configuration identifier passed to Control init;
   --  the empty default is supported and its meaning is provider-defined.
   --  @return Limited owner of the successfully initialized provider session.
   --  @exception Constraint_Error Any input string contains an embedded NUL.
   --  @exception Provider_Error Native loading, acquisition or initialization
   --  fails; a failed open does not return an open Session.

   function Is_Open (Object : Session) return Boolean;
   --  Inspect local handle presence without calling the provider.
   --  @param Object Session whose local ownership state is inspected.
   --  @return True when the Ada object holds a native session handle. This is
   --  not a provider-health or outstanding-work-completion test.

   function Query_Provider_Version (Object : Session) return Provider_Version;
   --  Query Control version information and copy its numbers and complete text
   --  into Ada storage. Native validation requires UTF-8 text without NUL.
   --  The two-call size/copy protocol is not a provider version negotiation.
   --  @param Object Open Session; serialize this query against Close.
   --  @return Independent snapshot with full 32-bit numbers and Ada-owned text.
   --  @exception Provider_Error The Session is closed, the provider query fails,
   --  or the text is invalid or cannot be copied by the native protocol.
   --  @exception Program_Error The native protocol reports a zero required
   --  string-buffer size.

   procedure Close (Object : in out Session);
   --  Consume the public native handle; repeated closure is harmless. Releases
   --  this Session's shared ownership, not every child or request's ownership.
   --  The IR Control interface has no separate shutdown operation: object
   --  destruction and library unloading occur only when retained owners allow.
   --  This is not cancellation, a wait for all futures, or a quiescence promise.
   --  @param Object Session to close; its handle is cleared by native closure.
   --  Serialize closure against other operations on this Ada owner.
   --  @exception Provider_Error Native closure reports failure.

private
   procedure Check_Submission (Code : Interfaces.Integer_32; Diagnostic : String);
   --  Shared by safe asynchronous submission wrappers; never retries.

   type Provider_Version is record
      API_Value         : Provider_Version_Number := 0;
      Library_Value     : Provider_Version_Number := 0;
      Vendor_Value      : Ada.Strings.Unbounded.Unbounded_String;
      Description_Value : Ada.Strings.Unbounded.Unbounded_String;
   end record;

   type Session is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Session_Handle := AMS.MEL_C_API.Null_Session;
   end record;
   overriding
   procedure Finalize (Object : in out Session);
end AMS.MEL;
