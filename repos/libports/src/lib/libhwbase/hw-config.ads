--
-- \brief  Genode config
-- \author Josef Soentgen
-- \date   2017-10-11
--
--
-- Copyright (C) 2017 Genode Labs GmbH
--
-- This file is part of the Genode OS framework, which is distributed
-- under the terms of the GNU Affero General Public License version 3.
--


package HW.Config
is

   Dynamic_MMIO            : constant Boolean := true;
   Default_MMConf_Base     : constant := 0;
   Default_MMConf_Base_Set : constant Boolean := Default_MMConf_Base /= 0;

end HW.Config;
