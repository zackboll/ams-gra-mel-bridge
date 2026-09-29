package body AMS.MEL.RF.Product_Rx.Testing is
   function Native_Sample_Address (Value : Event) return System.Address
   is (Value.Sample_Address);
   function Native_Endpoint_Address (Value : Endpoint) return System.Address
   is (System.Address (Value.Handle));
end AMS.MEL.RF.Product_Rx.Testing;
