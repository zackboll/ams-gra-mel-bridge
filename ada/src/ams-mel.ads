package AMS.MEL is
   --  This is the independent C facade's version, not a provider/MEL version.
   type Version is record
      Major : Natural;
      Minor : Natural;
   end record;

   function ABI_Version return Version;
   --  Raises Program_Error if the native facade violates the version contract.
   --  Does not initialize or load a sensor provider.
end AMS.MEL;
