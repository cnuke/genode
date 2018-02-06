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

with Interfaces.C;

with HW.Debug; use HW.Debug;
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

   function Genode_Pci_Read8 (Reg : in Index) return Word8;
   pragma Import (C, Genode_Pci_Read8, "genode_pci_read8");

   procedure Read8 (Value : out Word8; Offset : Index) is
   begin
      Value := Genode_Pci_Read8(Offset);
   end Read8;

   function Genode_Pci_Read16 (Reg : in Index) return Word16;
   pragma Import (C, Genode_Pci_Read16, "genode_pci_read16");

   procedure Read16 (Value : out Word16; Offset : Index) is
   begin
      Value := Genode_Pci_Read16(Offset);
   end Read16;

   function Genode_Pci_Read32 (Reg : in Index) return Word32;
   pragma Import (C, Genode_Pci_Read32, "genode_pci_read32");

   procedure Read32 (Value : out Word32; Offset : Index) is
   begin
      Value := Genode_Pci_Read32(Offset);
   end Read32;

   procedure Genode_Pci_Write8 (Reg : in Index; Value : in Word8);
   pragma Import (C, Genode_Pci_Write8, "genode_pci_write8");

   procedure Write8 (Offset : Index; Value : Word8) is
   begin
      Genode_Pci_Write8(Offset, Value);
   end Write8;

   procedure Genode_Pci_Write16 (Reg : in Index; Value : in Word16);
   pragma Import (C, Genode_Pci_Write16, "genode_pci_write16");

   procedure Write16 (Offset : Index; Value : Word16) is
   begin
      Genode_Pci_Write16(Offset, Value);
   end Write16;

   procedure Genode_Pci_Write32 (Reg : in Index; Value : in Word32);
   pragma Import (C, Genode_Pci_Write32, "genode_pci_write32");

   procedure Write32 (Offset : Index; Value : Word32) is
   begin
      Genode_Pci_Write32(Offset, Value);
   end Write32;

   function Resource_To_Int(Res : in Resource) return Integer is
   begin
      case Res is
         when Res0 =>
           return 0;
         when Res2 =>
           return 2;
         when others =>
           return -1;
      end case;
   end Resource_To_Int;

   --
   -- Genode PCI glue code
   --
   function Genode_Map_Resource (Res : in Integer; WC : in Integer) return Word64;
   pragma Import (C, Genode_Map_Resource, "genode_map_resource");

   procedure Map
     (Addr     :    out Word64;
      Res      : in     Resource;
      Length   : in     Natural := 0;
      Offset   : in     Natural := 0;
      WC       : in     Boolean := False)
   is
      Res_Int : constant Integer := Resource_To_Int(Res);
   begin
      Addr := 0;
      if Res_Int = -1 then
         return;
      end if;

      Addr := Genode_Map_Resource(Res_Int, (if WC = True then 1 else 0));
   end Map;

   --
   -- Genode PCI glue code
   --
   function Genode_Resource_Size (Res : in Integer) return Interfaces.C.unsigned_long;
   pragma Import (C, Genode_Resource_Size, "genode_resource_size");

   procedure Resource_Size (Length : out Natural; Res : Resource)
   is
      Res_Int : constant Integer := Resource_To_Int(Res);
   begin
      Length := 0;
      if Res_Int = -1 then
         return;
      end if;

      Length := Natural (Genode_Resource_Size (Res_Int));
   end Resource_Size;

   --
   -- Genode PCI glue code
   --
   function Genode_Open_Pci
      (Bus  : in Integer;
       Dev  : in Integer;
       Func : in Integer) return Boolean;
   pragma Import (C, Genode_Open_Pci, "genode_open_pci");

   procedure Initialize (Success : out Boolean; MMConf_Base : Word64 := 0)
   is
   begin
      Success := Boolean (Genode_Open_Pci (0, 2, 0));
   end Initialize;

end HW.PCI.Dev;
