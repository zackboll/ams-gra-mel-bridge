package body AMS.MEL.RF.Product_Rx.Testing is
   procedure Check_Representation is
   begin
      if Complex_I16'Size /= AMS.MEL_C_API.RF_Complex_I16_V1'Size then
         raise Program_Error with "RF Complex_I16 Size mismatch";
      end if;
      if Complex_I16'Object_Size /= AMS.MEL_C_API.RF_Complex_I16_V1'Object_Size then
         raise Program_Error with "RF Complex_I16 Object_Size mismatch";
      end if;
      if Complex_I16'Alignment /= AMS.MEL_C_API.RF_Complex_I16_V1'Alignment then
         raise Program_Error with "RF Complex_I16 Alignment mismatch";
      end if;
   end Check_Representation;

   function Native_Sample_Address (Value : Event) return System.Address
   is (Value.Sample_Address);
   function Native_Endpoint_Address (Value : Endpoint) return System.Address
   is (System.Address (Value.Handle));
end AMS.MEL.RF.Product_Rx.Testing;
