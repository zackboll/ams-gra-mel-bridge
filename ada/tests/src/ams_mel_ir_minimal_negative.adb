with Ada.Text_IO;
with AMS_MEL_IR_Minimal_Checks;

procedure AMS_MEL_IR_Minimal_Negative is
   package Q renames AMS_MEL_IR_Minimal_Checks;
   Good : constant Q.Evidence :=
     (Registrations => 17,
      Keep_Alives   => 4,
      Futures       => 8,
      Callbacks     => [2, 2, 2, 2],
      others        => True);
   procedure Reject (Value : Q.Evidence) is
   begin
      begin
         Q.Complete (Value);
      exception
         when Program_Error =>
            return;
      end;
      raise Program_Error with "negative evidence incorrectly accepted";
   end Reject;
begin
   Q.Complete (Good);
   for Case_ID in 1 .. 10 loop
      declare
         Bad : Q.Evidence := Good;
      begin
         case Case_ID is
            when 1  =>
               Bad.Callbacks (4) := 1;

            when 2  =>
               Bad.Callbacks := [others => 0];

            when 3  =>
               Bad.Quiet := False;

            when 4  =>
               Bad.BIT_Expected := False;

            when 5  =>
               Bad.Config_Expected := False;

            when 6  =>
               Bad.Mode_Expected := False;

            when 7  =>
               Bad.Navigation_Expected := False;

            when 8  =>
               Bad.Cleanup_Complete := False;

            when 9  =>
               Bad.Futures := 7;

            when 10 =>
               Bad.Registrations := 16;
         end case;
         Reject (Bad);
      end;
   end loop;
   for Case_ID in 1 .. 4 loop
      declare
         Rejected : Boolean := False;
      begin
         begin
            case Case_ID is
               when 1 =>
                  Q.Identity ("wrong-provider", "description", 4, 1);

               when 2 =>
                  Q.Selection ([1, 1, 0, 1], 3, True);

               when 3 =>
                  Q.Selection ([1, 1, 1, 1], 4, False);

               when 4 =>
                  Q.Correlation ((100, 999), (100, 200), "callback");
            end case;
         exception
            when Program_Error =>
               Rejected := True;
         end;
         Q.Require (Rejected, "negative identity/discovery/correlation accepted");
      end;
   end loop;
   Ada.Text_IO.Put_Line ("PASS: minimal consumer negative qualification rules (14 controls)");
end AMS_MEL_IR_Minimal_Negative;
