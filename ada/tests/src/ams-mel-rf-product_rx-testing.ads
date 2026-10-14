with System;

package AMS.MEL.RF.Product_Rx.Testing is
   procedure Check_Representation;
   function Native_Sample_Address (Value : Event) return System.Address;
   function Native_Endpoint_Address (Value : Endpoint) return System.Address;
   procedure Check_Stabilization_Boundaries;
end AMS.MEL.RF.Product_Rx.Testing;
