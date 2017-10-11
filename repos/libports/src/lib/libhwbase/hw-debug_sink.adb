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

--  with Interfaces.C.Strings; use Interfaces.C.Strings;

package body HW.Debug_Sink is

   --  procedure Genode_Put (Str : chars_ptr);
   --  pragma Import (C, Genode_Put, "genode_put");

   procedure Put (Item : String) is
   begin
     --  Genode_Put (New_String(Item));
     null;
   end Put;

   procedure Put_Char (Item : Character) is null;

   procedure New_Line is null;

end HW.Debug_Sink;
