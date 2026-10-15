private with Ada.Finalization;
private with AMS.MEL_C_API;
with AMS.MEL.Status;

package AMS.MEL.RF.Admin is
   --  Independently owned RF administrative interface, separate from DataMEL
   --  and C2MEL. State commands use the provider's UCI status-control interface
   --  and the shared Common MEL state enumeration; no separate RF state model
   --  is introduced. Acceptance is not evidence of a physical state transition.
   --  Externally serialize operations on each owner, including closure and
   --  finalization. This package does not provide concurrent-handle protection.

   type Admin_MEL is limited private;
   --  Unique limited controlled owner of an AdminMEL and its provider-library
   --  reference. A default-initialized owner is closed. Explicit Close reports
   --  failures; finalization attempts closure without propagating exceptions.
   --  Native shutdown failure permanently retains the provider/library graph
   --  rather than destroying objects or unloading code at an uncertain boundary.
   function Open (Library_Path : String; Configuration : String) return Admin_MEL;
   --  Load the provider library and invoke only its createAdminMEL factory with
   --  the configuration text. Does not open or borrow a DataMEL or C2MEL.
   --  Embedded NUL in either input raises a constraint error before allocation
   --  of temporary C strings or any provider call. Temporary strings are freed
   --  on both normal and exceptional exits. Native loading, symbol lookup,
   --  allocation, null-factory-result and factory-exception failures raise
   --  a provider error; no open owner is returned on those failures.
   --  @param Library_Path Provider shared-library path passed to the native loader.
   --  @param Configuration Provider-defined text passed synchronously as a string view; no bridge interpretation.
   --  @return Open unique administrative owner retaining the provider and library.
   function Is_Open (Object : Admin_MEL) return Boolean;
   --  Inspect only the local handle, without querying provider health or MFA
   --  state. False after Close even when shutdown failed and native resources
   --  remain permanently retained.
   --  @param Object Administrative owner whose local handle is inspected.
   --  @return True exactly when the owner holds a nonnull public native handle.
   function Command_State (Object : Admin_MEL; State : AMS.MEL.Status.MFA_State) return Boolean;
   --  Obtain fresh shared UCI and status-control owners and call upstream
   --  StatusControl::commandState. All fifteen Common MEL representations
   --  0 through 14 are accepted by bridge validation, including the unset,
   --  Unknown and Degraded; provider support and acceptance remain separate.
   --  There is no bridge transition table, status polling or completion wait.
   --  Closed owners, null required provider interfaces, provider exceptions or
   --  malformed native Boolean results raise a provider error, not False.
   --  @param Object Open administrative owner; externally serialize its use.
   --  @param State Shared MFA state command, mapped by its checked enumeration representation.
   --  @return Provider acceptance Boolean; False is normal rejection and True does not prove hardware transition.
   procedure Close (Object : in out Admin_MEL);
   --  Consume the public handle before invoking provider shutdown once. Closing
   --  an already closed owner is harmless. On success, destroy the AdminMEL
   --  before dropping its library reference; other library references may still
   --  prevent unloading. If shutdown throws, raise a provider error but leave the
   --  public owner closed and retain the complete native graph for process
   --  lifetime. A later Close does not retry that failed shutdown. Finalization
   --  follows the same native retention policy without propagating exceptions.
   --  @param Object Owner to close; serialize against all other owner operations.
private
   type Admin_MEL is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_Admin_Handle := AMS.MEL_C_API.Null_RF_Admin;
   end record;
   overriding
   procedure Finalize (Object : in out Admin_MEL);
end AMS.MEL.RF.Admin;
