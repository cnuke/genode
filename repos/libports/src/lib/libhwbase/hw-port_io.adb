--
-- \brief  Genode HW Port I/O
-- \author Josef Soentgen
-- \date   2017-10-12
--
--
-- Copyright (C) 2017 Genode Labs GmbH
--
-- This file is part of the Genode OS framework, which is distributed
-- under the terms of the GNU Affero General Public License version 3.
--


with System;
with HW.Debug;

with Interfaces; use Interfaces;
with Interfaces.C;

package body HW.Port_IO
with
   Refined_State  => (State => null) --,
   --  SPARK_Mode     => Off
is

   function Genode_InB (Port : Port_Type) return Interfaces.C.unsigned_char;
   pragma Import (C, Genode_InB, "genode_inb");

   procedure InB (Value : out Word8; Port : Port_Type) is
   begin
      Value := Unsigned_8 (Genode_InB (Port));
   end InB;

   function Genode_InW (Port : Port_Type) return Interfaces.C.unsigned_short;
   pragma Import (C, Genode_InW, "genode_inw");

   procedure InW (Value : out Word16; Port : Port_Type) is
   begin
      Value := Unsigned_16 (Genode_InW (Port));
   end InW;

   function Genode_InL (Port : Port_Type) return Interfaces.C.unsigned_long;
   pragma Import (C, Genode_InL, "genode_inl");

   procedure InL (Value : out Word32; Port : Port_Type) is
   begin
      Value := Unsigned_32 (Genode_InL (Port));
   end InL;

   ----------------------------------------------------------------------------

   procedure Genode_OutB (Port : Port_Type; Value : Word8);
   pragma Import (C, Genode_OutB, "genode_outb");

   procedure OutB (Port : Port_Type; Value : Word8) is
   begin
      Genode_OutB (Port, Value);
   end OutB;

   procedure Genode_OutW (Port : Port_Type; Value : Word16);
   pragma Import (C, Genode_OutW, "genode_outw");

   procedure OutW (Port : Port_Type; Value : Word16) is
   begin
      Genode_OutW (Port, Value);
   end OutW;

   procedure Genode_OutL (Port : Port_Type; Value : Word32);
   pragma Import (C, Genode_OutL, "genode_outl");

   procedure OutL (Port : Port_Type; Value : Word32) is
   begin
      Genode_OutL (Port, Value);
   end OutL;

end HW.Port_IO;
