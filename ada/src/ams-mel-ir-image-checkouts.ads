private with Ada.Finalization;
private with AMS.MEL_C_API;
with Interfaces;

package AMS.MEL.IR.Image.Checkouts is
   --  Opaque control-only Image checkout notifications, not image samples.
   --  No application callback runs on a provider thread. Serialize operations
   --  and Close on each public owner; independent leases may close concurrently.
   --  Prefer explicit Close. Finalization never raises or retries an uncertain
   --  release. Provider failures can permanently retain storage and provider code.

   type Checkout_Receiver is limited private;
   --  One-shot bounded receiver retaining the native Image and Session graph.
   type Checkout_Lease is limited private;
   --  Unique owner of one actual callback Buffer; exposes no address or pixels.

   type Diagnostic_Counters is record
      Callbacks_Received,
      Checkouts_Accepted,
      Queue_Overflow,
      Callbacks_Rejected,
      Releases_Succeeded,
      Releases_Uncertain : Interfaces.Unsigned_64;
   end record;
   --  Independent cumulative saturating counters, including rejected callbacks.
   --  @field Callbacks_Received All callbacks, including null deliveries.
   --  @field Checkouts_Accepted Actual wrappers transferred into the FIFO.
   --  @field Queue_Overflow Rejections at queue or outstanding owner capacity.
   --  @field Callbacks_Rejected Callbacks not accepted as opaque checkouts.
   --  @field Releases_Succeeded Successful release and safe wrapper destruction.
   --  @field Releases_Uncertain Failed release or conservative retained ownership.

   function Open
     (Object         : AMS.MEL.IR.Image_Stream;
      Queue_Capacity : Positive := 16;
      Max_Checkouts  : Positive := 16) return Checkout_Receiver;
   --  Install once before first Start, without a listener or image-buffer pool.
   --  Preallocates Max_Checkouts + 2 HOLD/deferred/uncertain slots for callback
   --  overlap. Max_Checkouts bounds queued plus leased owners. Excess delivery
   --  is rejected/released or retained; exhaustion of the process-wide 64-owner
   --  emergency reserve is fail-stop rather than unsafe wrapper destruction.
   --  @param Object Attached control-only Image; full acquisition is refused.
   --  @param Queue_Capacity Positive FIFO bound, at most Max_Checkouts.
   --  @param Max_Checkouts Maximum simultaneous queued and leased HOLD owners.
   --  @return Open receiver; closing it does not permit a second installation.
   --  @exception Provider_Error Invalid lifecycle, capacity or allocation failure.

   function Receive
     (Receiver : Checkout_Receiver; Timeout_Milliseconds : Natural := 0) return Checkout_Lease;
   --  Transfer the oldest delivered wrapper without retiring its obligation.
   --  Acceptance requires Running and completed active application registration.
   --  @param Receiver Open receiver, externally serialized against Close.
   --  @param Timeout_Milliseconds Maximum wait; zero is a nonblocking poll.
   --  @return Unique opaque lease, with no claim of valid image data.
   --  @exception Timeout_Error No checkout arrived; receiver is not cancelled.
   --  @exception Provider_Error Receiver stopped, native or provider failure.

   function Is_Open (Lease : Checkout_Lease) return Boolean;
   --  Inspect local lease ownership, not provider health or image validity.
   --  @param Lease Limited owner to inspect.
   --  @return True until explicit Close or finalization consumes the lease.

   function Counters (Receiver : Checkout_Receiver) return Diagnostic_Counters;
   --  Copy diagnostics under the native lifecycle mutex.
   --  @param Receiver Open receiver; must not race its Close.
   --  @return Independent counters including releases by independently held leases.
   --  @exception Provider_Error Closed receiver or native failure.

   procedure Close (Lease : in out Checkout_Lease);
   --  Idempotently consume this lease and release the exact callback Buffer once,
   --  outside bridge locks. Failed/throwing release permanently retains its graph;
   --  the public lease is still consumed and finalization never retries release.
   --  @param Lease Owner cleared even when the provider release fails.
   --  @exception Provider_Error Release failed or threw; actual Return is reported
   --  when available, otherwise the explicit no-Return sentinel is reported.

   procedure Close (Receiver : in out Checkout_Receiver);
   --  Stop admission, wake waiters and drain queued wrappers through the same
   --  release engine. Live leases survive. No Disable, unregister or detach.
   --  @param Receiver Owner consumed even if draining reports uncertainty.
   --  @exception Provider_Error Drain failed; exact uncertain owners stay retained.

private
   type Checkout_Receiver is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Checkout_Handle := AMS.MEL_C_API.Null_Checkout;
   end record;
   overriding
   procedure Finalize (Receiver : in out Checkout_Receiver);
   --  Non-raising fallback; queued uncertain owners remain conservatively retained.
   --  @param Receiver Receiver whose queued wrappers are drained without raising.
   type Lease_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Checkout_Lease_Handle := AMS.MEL_C_API.Null_Checkout_Lease;
   end record;
   overriding
   procedure Finalize (Lease : in out Lease_Owner);
   --  Non-raising fallback; native Close consumes the owner even on release failure.
   --  @param Lease Unique native owner; never retries a previously consumed release.
   type Checkout_Lease is limited record
      Owner : Lease_Owner;
   end record;
end AMS.MEL.IR.Image.Checkouts;
