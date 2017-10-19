--
-- Based on 'hw-gfx-gma-gfx_test.adb' (3b654a0991) by Nico Huber.
--

with Ada.Unchecked_Conversion;
--  with Ada.Command_Line;
with Interfaces.C;

with HW.Time;
with HW.Debug;
with HW.PCI.Dev;
with HW.MMIO_Range;
with HW.GFX.GMA;
with HW.GFX.GMA.Config;
with HW.GFX.GMA.Display_Probing;

pragma Elaborate_All (HW.PCI.Dev, HW.MMIO_Range);

package body HW.GFX.GMA.GFX
is
   pragma Disable_Atomic_Synchronization;

   package Dev is new PCI.Dev (PCI.Address'(0, 2, 0));

   type GTT_PTE_Type is mod 2 ** (Config.GTT_PTE_Size * 8);
   type GTT_Registers_Type is array (GTT_Range) of GTT_PTE_Type;
   package GTT is new MMIO_Range
     (Base_Addr   => 0,
      Element_T   => GTT_PTE_Type,
      Index_T     => GTT_Range,
      Array_T     => GTT_Registers_Type);

   type Pixel_Type is record
      Red   : Byte;
      Green : Byte;
      Blue  : Byte;
      Alpha : Byte;
   end record;

   for Pixel_Type use record
      Blue  at 0 range 0 .. 7;
      Green at 1 range 0 .. 7;
      Red   at 2 range 0 .. 7;
      Alpha at 3 range 0 .. 7;
   end record;

   function Pixel_To_Word (P : Pixel_Type) return Word32
   with
      SPARK_Mode => Off
   is
      function To_Word is new Ada.Unchecked_Conversion (Pixel_Type, Word32);
   begin
      return To_Word (P);
   end Pixel_To_Word;

   Max_W    : constant := 4096;
   Max_H    : constant := 2160;
   FB_Align : constant := 16#0004_0000#;
   subtype Screen_Index is Natural range
      0 .. 3 * (Max_W * Max_H + FB_Align / 4) - 1;
   type Screen_Type is array (Screen_Index) of Word32;

   package Screen is new MMIO_Range (0, Word32, Screen_Index, Screen_Type);

   function Fill
     (X, Y        : Natural;
      Framebuffer : Framebuffer_Type;
      Pipe        : GMA.Pipe_Index)
      return Pixel_Type
   is
      use type HW.Byte;

      Xp : constant Natural := X * 256 / Natural (Framebuffer.Width);
      Yp : constant Natural := Y * 256 / Natural (Framebuffer.Height);
      Xn : constant Natural := 255 - Xp;
      Yn : constant Natural := 255 - Yp;

      function Map (X, Y : Natural) return Byte is
      begin
         return Byte (X * Y / 255);
      end Map;
   begin
      return
        (case Pipe is
         when GMA.Primary   => (Map (Xn, Yn), Map (Xp, Yn), Map (Xp, Yp), 255),
         when GMA.Secondary => (Map (Xn, Yp), Map (Xn, Yn), Map (Xp, Yn), 255),
         when GMA.Tertiary  => (Map (Xp, Yp), Map (Xn, Yp), Map (Xn, Yn), 255));
   end Fill;

   procedure Test_Screen
     (Framebuffer : Framebuffer_Type;
      Pipe        : GMA.Pipe_Index)
   is
      P        : Pixel_Type;
      -- We have pixel offset wheras the framebuffer has a byte offset
      Offset_Y : Natural := Natural (Framebuffer.Offset / 4);
      Offset   : Natural;
   begin
      for Y in 0 .. Natural (Framebuffer.Height) - 1 loop
         Offset := Offset_Y;
         for X in 0 .. Natural (Framebuffer.Width) - 1 loop
            if Y mod 16 = 0 or X mod 16 = 0 then
               P := (0, 0, 0, 0);
            else
               P := Fill (X, Y, Framebuffer, Pipe);
            end if;
            Screen.Write (Offset, Pixel_To_Word (P));
            Offset := Offset + 1;
         end loop;
         Offset_Y := Offset_Y + Natural (Framebuffer.Stride);
      end loop;
   end Test_Screen;

   procedure Calc_Framebuffer
     (FB       :    out Framebuffer_Type;
      Mode     : in     Mode_Type;
      Offset   : in out Word32)
   is
   begin
      Offset := (Offset + FB_Align - 1) and not (FB_Align - 1);
      FB :=
        (Width    => Width_Type (Mode.H_Visible),
         Height   => Height_Type (Mode.V_Visible),
         BPC      => 8,
         Stride   => Width_Type ((Word32 (Mode.H_Visible) + 15) and not 15),
         Offset   => Offset);
      Offset := Offset + Word32 (FB.Stride * FB.Height * 4);
   end Calc_Framebuffer;

   Pipes : GMA.Pipe_Configs;

   procedure Prepare_Configs
   is
      use type HW.GFX.GMA.Port_Type;

      Offset : Word32 := 0;
      Success : Boolean;
   begin
      GMA.Display_Probing.Scan_Ports (Pipes);

      for Pipe in GMA.Pipe_Index loop
         if Pipes (Pipe).Port /= GMA.Disabled then
            Calc_Framebuffer
              (FB       => Pipes (Pipe).Framebuffer,
               Mode     => Pipes (Pipe).Mode,
               Offset   => Offset);
            GMA.Setup_Default_FB
              (FB       => Pipes (Pipe).Framebuffer,
               Clear    => False,
               Success  => Success);
            if not Success then
               Pipes (Pipe).Port := GMA.Disabled;
            end if;
         end if;
      end loop;

      GMA.Dump_Configs (Pipes);
   end Prepare_Configs;

   function Initialize return Integer
   is
      use type HW.GFX.GMA.Port_Type;
      use type HW.Word64;
      use type Interfaces.C.int;

      Res_Addr : Word64;

      Dev_Init,
      Initialized : Boolean;
   begin

      Dev.Initialize (Dev_Init);
      if not Dev_Init then
         Debug.Put_Line ("Failed to map PCI config.");
         return 1;
      end if;

      Dev.Map (Res_Addr, PCI.Res0, Offset => Config.GTT_Offset);
      if Res_Addr = 0 then
         Debug.Put_Line ("Failed to map PCI resource0.");
         return 1;
      end if;
      GTT.Set_Base_Address (Res_Addr);

      --  Dev.Map (Res_Addr, PCI.Res2, WC => True);
      --  if Res_Addr = 0 then
      --     Debug.Put_Line ("Failed to map PCI resource2.");
      --     return 1;
      --  end if;
      --  Screen.Set_Base_Address (Res_Addr);

      Debug.Put_Line ("Before GMA.Initialize");

      GMA.Initialize
        (Clean_State => True,
         Success     => Initialized);

      if not Initialized then
         Debug.Put_Line ("Could not initialize GPU");
         return 1;
      end if;

      Prepare_Configs;

      GMA.Update_Outputs (Pipes);

      for Pipe in GMA.Pipe_Index loop
         if Pipes (Pipe).Port /= GMA.Disabled then
            --  Test_Screen
            --     (Framebuffer => Pipes (Pipe).Framebuffer,
            --      Pipe        => Pipe);
			null;
         end if;
      end loop;
      Debug.Put_Line ("Bye bye");
      return 0;
   end Initialize;

end HW.GFX.GMA.GFX;
