--
-- \brief  Ada test program that calls a external C functions
-- \author Norman Feske
-- \date   2009-09-23
--

--  with HW;
--  with HW.Debug;
--  with HW.PCI.Dev;
--  with HW.Port_IO;
--  with HW.Time;

with HW.GFX.GMA.GFX;

--
-- Main program
--
procedure Main is
begin
	HW.GFX.GMA.GFX.Main;
end Main;
