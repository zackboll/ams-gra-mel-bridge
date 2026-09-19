with AMS.MEL_C_API;
with AMS.MEL.IR.Channel;

private package AMS.MEL.IR.Capability_Conversion is
   function To_Channel_Capability
     (Value : AMS.MEL_C_API.IR_Channel_Capability_V1) return AMS.MEL.IR.Channel.Channel_Capability;
end AMS.MEL.IR.Capability_Conversion;
