--
-- \brief  Genode HW timer
-- \author Josef Soentgen
-- \date   2017-10-11
--
--
-- Copyright (C) 2017 Genode Labs GmbH
--
-- This file is part of the Genode OS framework, which is distributed
-- under the terms of the GNU Affero General Public License version 3.
--


with Interfaces.C;

package body HW.Time.Timer
with
   Refined_State => (Timer_State => null,
                     Abstract_Time => null)
is

   function Genode_Timer_Now return Interfaces.C.unsigned_long;
   pragma Import (C, Genode_Timer_Now, "genode_timer_now");

   function Raw_Value_Min return T
   is
      Now : constant Interfaces.C.unsigned_long := Genode_Timer_Now;
   begin
      return T(Now);
   end Raw_Value_Min;

   function Raw_Value_Max return T
   is
   begin
      return Raw_Value_Min + 1;
   end Raw_Value_Max;

   function Hz return T
   is
   begin
      return 1_000_000_000;
   end Hz;

end HW.Time.Timer;
