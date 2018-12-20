package Aes_Cbc_4k is

   -- pragma Pure; -- not possible because libsparkcrypto is not known as pure

   type Byte              is mod 2**8 with Size => 8;
   type Key_Type          is array (Natural range 1 .. 32)   of Byte;
   type Block_Data_Type   is array (Natural range 1 .. 4096) of Byte;
   type Plaintext_Type    is new Block_Data_Type;
   type Ciphertext_Type   is new Block_Data_Type;
   type Block_Number_Type is mod 2**64 with Size => 64;

   procedure Encrypt ( Key          :     Key_Type;
                       Block_Number :     Block_Number_Type;
                       Plaintext    :     Plaintext_Type;
                       Ciphertext   : out Ciphertext_Type )
   with Export,
      Convention    => C,
      External_Name => "_ZN10Aes_cbc_4k7encryptERKNS_3KeyENS_12Block_numberERKNS_9PlaintextERNS_10CiphertextE";

   procedure Decrypt ( Key          :     Key_Type;
                       Block_Number :     Block_Number_Type;
                       Ciphertext   :     Ciphertext_Type;
                       Plaintext    : out Plaintext_Type )
   with Export,
      Convention    => C,
      External_Name => "_ZN10Aes_cbc_4k7decryptERKNS_3KeyENS_12Block_numberERKNS_10CiphertextERNS_9PlaintextE";

end Aes_Cbc_4k;
