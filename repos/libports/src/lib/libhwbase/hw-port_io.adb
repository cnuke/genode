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

package body HW.Port_IO
with
   Refined_State  => (State => null) --,
   --  SPARK_Mode     => Off
is

   procedure InB (Value : out Word8; Port : Port_Type) is
   begin
      Value := 0;
   end InB;

   procedure InW (Value : out Word16; Port : Port_Type) is
   begin
      Value := 0;
   end InW;

   procedure InL (Value : out Word32; Port : Port_Type) is
   begin
      Value := 0;
   end InL;

   ----------------------------------------------------------------------------

   procedure OutB (Port : Port_Type; Value : Word8) is
   begin
      null;
   end OutB;

   procedure OutW (Port : Port_Type; Value : Word16) is
   begin
      null;
   end OutW;

   procedure OutL (Port : Port_Type; Value : Word32) is
   begin
      null;
   end OutL;

end HW.Port_IO;
