private with Ada.Finalization;
private with AMS.MEL_C_API;
with Interfaces;

--  Signal-only change notification, not a callback-time status snapshot.
--  Register first, query E1 on the application thread, wait, then query again.
--  Several changes may coalesce; a later query can see a later state, or the
--  same state. Queries do not consume notifications. E1 copies stay immutable.

package AMS.MEL.RF.C2.Virtual_Aperture_Notifications is
   type Subscription is tagged limited private;
   --  One attempted registration per public VA, even after Unsubscribe/Close.
   --  Serialize Open/Unsubscribe with all same-VA calls, including VA Close.
   function Open (VA : in out Virtual_Aperture'Class) return Subscription;
   --  Local wrapper ownership only, not active provider registration.
   function Is_Open (Object : Subscription) return Boolean;
   Subscription_Stopped : exception;
   --  Zero polls; maps native timeout to C2.Timeout_Error. Stop takes priority
   --  over pending changes and raises Subscription_Stopped.
   procedure Wait_For_Change (Object : Subscription; Timeout_Milliseconds : Natural);
   type Subscription_Statistics is record
      Callback_Entries        : Interfaces.Unsigned_64;
      Callbacks_Coalesced     : Interfaces.Unsigned_64;
      Notifications_Delivered : Interfaces.Unsigned_64;
      Callbacks_After_Stop    : Interfaces.Unsigned_64;
      Pending, Stopped        : Boolean;
   end record;
   --  Consistent, nonconsuming snapshot. Counters saturate at Unsigned_64'Last:
   --  entries includes all calls, coalesced counts active/already-pending calls,
   --  delivered counts successful waits, after-stop counts discarded calls.
   function Statistics (Object : Subscription) return Subscription_Statistics;
   --  Live matching VA required. Stops first, then removes the exact key once;
   --  repeated calls return the cached outcome, including failure. May block in
   --  provider removal; no callback-quiescence guarantee or bridge drain wait.
   procedure Unsubscribe (VA : in out Virtual_Aperture'Class; Object : in out Subscription);
   --  Local stop/closure only: NO provider call. Nonraising finalization does
   --  only this. Automatic removal occurs at VA Close even after local Close.
   --  Wait/Statistics may overlap VA Close, not this wrapper's destruction.
   procedure Close (Object : in out Subscription);
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
