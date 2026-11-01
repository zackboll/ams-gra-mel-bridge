with AMS.MEL.IR.NUC;

--  IRMEL-ADA-002 shared expectations for the deterministic mock NUC_TempData fixture.

package AMS_MEL_IR_NUC_Checks is
   procedure Check_Rich (Empty, One, Three : AMS.MEL.IR.NUC.NUC_Temp_Data);
   procedure Check_Single (Value : AMS.MEL.IR.NUC.NUC_Temp_Data; Timestamp_NS : Long_Long_Integer);
end AMS_MEL_IR_NUC_Checks;
