with System;

package body AMS.MEL.IR.Image.Checkouts is
   package C renames AMS.MEL_C_API;
   use type Interfaces.Integer_32;
   use type C.Checkout_Lease_Handle;

   procedure Check (Code : Interfaces.Integer_32; Result : Interfaces.Unsigned_32 := 16#FFFF_FFFF#)
   is
   begin
      if Code = C.Timeout then
         raise Timeout_Error with "No opaque checkout available";
      elsif Code /= C.Success then
         raise Provider_Error
           with "Checkout bridge status" & Code'Image & "; provider Return" & Result'Image;
      end if;
   end Check;

   function Open
     (Object         : AMS.MEL.IR.Image_Stream;
      Queue_Capacity : Positive := 16;
      Max_Checkouts  : Positive := 16) return Checkout_Receiver is
   begin
      return Receiver : Checkout_Receiver do
         Check
           (C.IR_Image_Checkout_Open
              (Object.Handle,
               C.Size_T (Queue_Capacity),
               C.Size_T (Max_Checkouts),
               Receiver.Handle'Access,
               System.Null_Address,
               0,
               null));
      end return;
   end Open;

   function Receive
     (Receiver : Checkout_Receiver; Timeout_Milliseconds : Natural := 0) return Checkout_Lease is
   begin
      return Lease : Checkout_Lease do
         Check
           (C.IR_Image_Checkout_Receive
              (Receiver.Handle,
               Interfaces.Unsigned_32 (Timeout_Milliseconds),
               Lease.Owner.Handle'Access,
               System.Null_Address,
               0,
               null));
      end return;
   end Receive;

   function Is_Open (Lease : Checkout_Lease) return Boolean
   is (Lease.Owner.Handle /= C.Null_Checkout_Lease);

   function Counters (Receiver : Checkout_Receiver) return Diagnostic_Counters is
      Raw : aliased C.Checkout_Counters_V1;
   begin
      Check
        (C.IR_Image_Checkout_Get_Counters
           (Receiver.Handle, Raw'Access, System.Null_Address, 0, null));
      return
        (Raw.Callbacks_Received,
         Raw.Checkouts_Accepted,
         Raw.Queue_Overflow,
         Raw.Callbacks_Rejected,
         Raw.Releases_Succeeded,
         Raw.Releases_Uncertain);
   end Counters;

   procedure Close (Lease : in out Checkout_Lease) is
      Result : aliased Interfaces.Unsigned_32 := Interfaces.Unsigned_32'Last;
      Code   : Interfaces.Integer_32;
   begin
      Code :=
        C.IR_Image_Checkout_Lease_Close
          (Lease.Owner.Handle'Access, Result'Access, System.Null_Address, 0, null);
      Check (Code, Result);
   end Close;

   procedure Close (Receiver : in out Checkout_Receiver) is
   begin
      Check (C.IR_Image_Checkout_Close (Receiver.Handle'Access, System.Null_Address, 0, null));
   end Close;

   overriding
   procedure Finalize (Lease : in out Lease_Owner) is
      Result  : aliased Interfaces.Unsigned_32;
      Ignored : Interfaces.Integer_32;
   begin
      Ignored :=
        C.IR_Image_Checkout_Lease_Close
          (Lease.Handle'Access, Result'Access, System.Null_Address, 0, null);
   exception
      when others =>
         null;
   end Finalize;

   overriding
   procedure Finalize (Receiver : in out Checkout_Receiver) is
   begin
      Close (Receiver);
   exception
      when others =>
         null;
   end Finalize;
end AMS.MEL.IR.Image.Checkouts;
