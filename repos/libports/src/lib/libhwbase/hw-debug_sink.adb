--
-- Copyright (C) 2015 secunet Security Networks AG
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--

with Interfaces.C; use Interfaces.C;

package body HW.Debug_Sink is

   procedure Genode_Put (C : Integer);
   pragma Import (C, Genode_Put, "genode_put");

   procedure Put (Item : String) is
   begin
     for I in Item'Range loop
        declare
           C : Character := Item (i);
        begin
           Genode_Put (Character'Pos(C));
        end;
     end loop;
   end Put;

   procedure Put_Char (Item : Character) is
   begin
     Genode_Put (Character'Pos(Item));
   end Put_Char;

   procedure New_Line is
   begin
     Genode_Put (16#0a#);
   end New_Line;

end HW.Debug_Sink;
