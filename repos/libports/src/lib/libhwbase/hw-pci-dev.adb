--
-- \brief  Genode HW PCI device
-- \author Josef Soentgen
-- \date   2017-10-12
--
--
-- Copyright (C) 2017 Genode Labs GmbH
--
-- This file is part of the Genode OS framework, which is distributed
-- under the terms of the GNU Affero General Public License version 3.
--


with HW.Config;
with HW.PCI.MMConf;

with HW.MMIO_Range;
pragma Elaborate_All (HW.MMIO_Range);

package body HW.PCI.Dev
with
   Refined_State =>
     (Address_State  => MM.Address_State,
      PCI_State      => MM.PCI_State)
is

   package MM is new HW.PCI.MMConf (Dev);

   procedure Read8 (Value : out Word8; Offset : Index) is
   begin
      Value := 0;
   end Read8;

   procedure Read16 (Value : out Word16; Offset : Index) is
   begin
      Value := 0;
   end Read16;

   procedure Read32 (Value : out Word32; Offset : Index) is
   begin
      Value := 0;
   end Read32;

   procedure Write8 (Offset : Index; Value : Word8) is
   begin
      null;
   end Write8;

   procedure Write16 (Offset : Index; Value : Word16) is
   begin
      null;
   end Write16;

   procedure Write32 (Offset : Index; Value : Word32) is
   begin
      null;
   end Write32;

   procedure Map
     (Addr     :    out Word64;
      Res      : in     Resource;
      Length   : in     Natural := 0;
      Offset   : in     Natural := 0;
      WC       : in     Boolean := False)
   is
   begin
      Addr := 0;
   end Map;

   procedure Resource_Size (Length : out Natural; Res : Resource)
   is
   begin
      Length := 0;
   end Resource_Size;

   procedure Initialize (Success : out Boolean; MMConf_Base : Word64 := 0)
   is
   begin
      Success := true;
   end Initialize;

end HW.PCI.Dev;
