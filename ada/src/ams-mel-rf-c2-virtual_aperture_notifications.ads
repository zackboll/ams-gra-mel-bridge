private with Ada.Finalization;
private with AMS.MEL_C_API;
with Interfaces;

--  Signal-only change notification, not a callback-time status snapshot.
--  Register first, query E1 on the application thread, wait, then query again.
--  Several changes may coalesce; a later query can see a later state, or the
--  same state. Queries do not consume notifications. E1 copies stay immutable.

package AMS.MEL.RF.C2.Virtual_Aperture_Notifications is
   --  Observe signal-only callback entry, not live provider state. Register,
   --  query current status on the application thread, wait, then query again.
   --  Notifications contain no snapshot and do not guarantee changed state.

   type Subscription is tagged limited private;
   --  Limited local signal owner with non-raising controlled cleanup. The
   --  permanent callable/signal/DSO shell retains no VA or C2 owner. Its callback
   --  ignores the provider reference entirely and never dereferences it.

   --  One attempted registration per public VA, even after Unsubscribe/Close.
   --  Serialize Open/Unsubscribe with all same-VA calls, including VA Close.
   function Open (VA : in out Virtual_Aperture'Class) return Subscription;
   --  Attempt one registration per VA; it may invoke the callback synchronously.
   --  Normal return proves registration returned, not later notification delivery.
   --  Provider/native failures raise Provider Error; no arbitrary retry follows
   --  an exposed registration attempt, even after local closure or unsubscribe.
   --  @param VA Live VA; serialize registration with all same-VA operations.
   --  @return Local observer, not an owned provider-resource graph.

   --  Local wrapper ownership only, not active provider registration.
   function Is_Open (Object : Subscription) return Boolean;
   --  Inspect only local wrapper ownership, not provider registration status.
   --  @param Object Subscription to inspect.
   --  @return True if the native local handle exists, even when stopped.
   Subscription_Stopped : exception;
   --  Wait observed logical stop; distinct from timeout or query status values.

   --  Zero polls; maps native timeout to C2.Timeout_Error. Stop takes priority
   --  over pending changes and raises Subscription_Stopped.
   procedure Wait_For_Change (Object : Subscription; Timeout_Milliseconds : Natural);
   --  Consume one pending signal. Multiple callback entries coalesce into one
   --  pending notification; successful waits increment delivered, not entries.
   --  Stop clears pending and has priority. Raises Timeout Error on absence,
   --  Subscription Stopped on stop, and Provider Error for other native errors.
   --  @param Object Open local subscription; no provider getter is called.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   type Subscription_Statistics is record
      Callback_Entries        : Interfaces.Unsigned_64;
      Callbacks_Coalesced     : Interfaces.Unsigned_64;
      Notifications_Delivered : Interfaces.Unsigned_64;
      Callbacks_After_Stop    : Interfaces.Unsigned_64;
      Pending, Stopped        : Boolean;
   end record;
   --  Consistent nonconsuming bridge signal statistics; counters saturate at
   --  Unsigned 64'Last rather than wrapping. No provider state is included.
   --  @field Callback_Entries All callable entries, including stopped entries.
   --  @field Callbacks_Coalesced Active entries arriving while already pending.
   --  @field Notifications_Delivered Successful waits consuming a pending signal.
   --  @field Callbacks_After_Stop Stopped callback entries counted and discarded.
   --  @field Pending An unconsumed coalesced signal exists, not a delivered snapshot.
   --  @field Stopped Reception has stopped; pending is cleared.

   --  Consistent, nonconsuming snapshot. Counters saturate at Unsigned_64'Last:
   --  entries includes all calls, coalesced counts active/already-pending calls,
   --  delivered counts successful waits, after-stop counts discarded calls.
   function Statistics (Object : Subscription) return Subscription_Statistics;
   --  Copy all counters and flags under one native lock without consuming signals.
   --  @param Object Subscription with a local handle, possibly stopped.
   --  @return Consistent signal statistics; no provider query or state guarantee.

   --  Live matching VA required. Stops first, then removes the exact key once;
   --  repeated calls return the cached outcome, including failure. May block in
   --  provider removal; no callback-quiescence guarantee or bridge drain wait.
   procedure Unsubscribe (VA : in out Virtual_Aperture'Class; Object : in out Subscription);
   --  Stop reception before attempting exact provider-key removal once. Cache
   --  the removal outcome, including failure; repeated calls do not retry.
   --  Provider failures raise Provider Error. Removal can block and provides no
   --  callback-quiescence guarantee; the callable shell remains permanent.
   --  @param VA Matching live VA; serialize with same-VA operations and Close.
   --  @param Object Matching local subscription; remains locally open but stopped.

   --  Local stop/closure only: NO provider call. Nonraising finalization does
   --  only this. Automatic removal occurs at VA Close even after local Close.
   --  Wait/Statistics may overlap VA Close, not this wrapper's destruction.
   procedure Close (Object : in out Subscription);
   --  Stop and release only the local wrapper, idempotently, with no provider
   --  unsubscribe. VA Close performs its own registered-callback cleanup even
   --  after local Close. Controlled cleanup uses this non-raising local path.
   --  @param Object Subscription to consume; do not race wrapper destruction.

   --  Each exposed registration permanently retains one small callable/signal/
   --  DSO shell, never VA/C2. Provider must supply a valid callback reference;
   --  the bridge does not inspect it or invoke Ada/application code.
private
   type Subscription is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_VA_Subscription_Handle :=
        AMS.MEL_C_API.Null_RF_VA_Subscription;
   end record;
   overriding
   procedure Finalize (Object : in out Subscription);
end AMS.MEL.RF.C2.Virtual_Aperture_Notifications;
