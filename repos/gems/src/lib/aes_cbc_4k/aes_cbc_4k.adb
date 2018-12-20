pragma Ada_2012;

with LSC.AES.CBC;
with LSC.SHA256;
with Ada.Unchecked_Conversion;

package body Aes_Cbc_4k is

   subtype LSC_Plaintext_Type  is LSC.AES.Message_Type (1 .. Plaintext_Type'Length / 16);
   subtype LSC_Ciphertext_Type is LSC.AES.Message_Type (1 .. Ciphertext_Type'Length / 16);

   function Convert is new Ada.Unchecked_Conversion(Key_Type, LSC.AES.AES256_Key_Type);

   function Init_IV (Key : Key_Type; Block_Number : Block_Number_Type)
      return LSC.AES.Block_Type
   is
      type Padding_Type is array (Natural range <>) of Byte;
      type Hash_Input_Type is record
         Key     : Key_Type;
         Padding : Padding_Type(1 .. 32);
      end record
      with Size => 512;

      type Block_Number_Text_Type is record
         Block_Number : Block_Number_Type;
         Padding      : Padding_Type(1 .. 8);
      end record
      with Size => 128;

      subtype LSC_Hash_Input_Type is LSC.SHA256.Message_Type (1 .. 1);

      function Convert is new Ada.Unchecked_Conversion(Hash_Input_Type,             LSC_Hash_Input_Type);
      function Convert is new Ada.Unchecked_Conversion(LSC.SHA256.SHA256_Hash_Type, Key_Type);
      function Convert is new Ada.Unchecked_Conversion(Block_Number_Text_Type,      LSC.AES.Block_Type);

      Hash_Input : constant Hash_Input_Type := ( Key     => Key,
                                                 Padding => (others => 0) );
      Salt : constant Key_Type :=
        Convert (  LSC.SHA256.Hash(Message => Convert(Hash_Input),
                                   Length  => Key'Size) );

      Block_Number_Text : constant Block_Number_Text_Type :=
         ( Block_Number => Block_Number, Padding => (others => 0) );

   begin
      return LSC.AES.Encrypt
        ( Context    => LSC.AES.Create_AES256_Enc_Context(Convert(Salt)),
          Plaintext  => Convert(Block_Number_Text) );
   end Init_IV;


   procedure Encrypt ( Key          :     Key_Type;
                       Block_Number :     Block_Number_Type;
                       Plaintext    :     Plaintext_Type;
                       Ciphertext   : out Ciphertext_Type )
   is
      LSC_Ciphertext : LSC_Ciphertext_Type;

      function Convert is new Ada.Unchecked_Conversion(Plaintext_Type,      LSC_Plaintext_Type);
      function Convert is new Ada.Unchecked_Conversion(LSC_Ciphertext_Type, Ciphertext_Type);

   begin

      LSC.AES.CBC.Encrypt
        ( Context    => LSC.AES.Create_AES256_Enc_Context(Convert(Key)),
          IV         => Init_IV(Key, Block_Number),
          Plaintext  => Convert(Plaintext),
          Length     => LSC_Plaintext_Type'Length,
          Ciphertext => LSC_Ciphertext );

      Ciphertext := Convert(LSC_Ciphertext);
   end Encrypt;


   procedure Decrypt ( Key          :     Key_Type;
                       Block_Number :     Block_Number_Type;
                       Ciphertext   :     Ciphertext_Type;
                       Plaintext    : out Plaintext_Type )
   is
      LSC_Plaintext : LSC_Plaintext_Type;

      function Convert is new Ada.Unchecked_Conversion(Ciphertext_Type,    LSC_Ciphertext_Type);
      function Convert is new Ada.Unchecked_Conversion(LSC_Plaintext_Type, Plaintext_Type);

   begin

      LSC.AES.CBC.Decrypt
        ( Context     => LSC.AES.Create_AES256_Dec_Context(Convert(Key)),
          IV          => Init_IV(Key, Block_Number),
          Ciphertext  => Convert(Ciphertext),
          Length      => LSC_Ciphertext_Type'Length,
          Plaintext   => LSC_Plaintext );

      Plaintext := Convert(LSC_Plaintext);
   end Decrypt;

end Aes_Cbc_4k;
