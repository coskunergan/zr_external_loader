import argparse
import struct
from Crypto.Cipher import AES

def main():
    parser = argparse.ArgumentParser(description='STM32H7 OTFDEC %100 Donanım Uyumlu Şifreleyici')
    parser.add_argument('input', help='Giriş dosyası (zephyr.bin)')
    parser.add_argument('output', help='Çıkış dosyası (zephyr_enc.bin)')
    args = parser.parse_args()

    # --- AYARLAR ---
    # C tarafındaki uint32_t key[4] = {W0, W1, W2, W3} sırası
    KEY_WORDS = [0xA9876543, 0x210FEDCB, 0xA9876543, 0x210FEDCB]
    NONCE_WORDS = [0x11111111, 0x55555555] # Nonce[0], Nonce[1]
    VERSION = 0xA5A5
    REGION_INDEX = 1     # OTFDEC_REGION2 = 1
    APP_START_ADDR = 0x90000400 # Uygulamanın Flash'taki fiziksel adresi

    # 1. Anahtarı Hazırla (Full 16-byte Reversal)
    # Donanım K0..K3'ü aynalanmış (Big-Endian) sırada AES çekirdeğine besler.
    key_bytes = struct.pack('<IIII', *KEY_WORDS)
    key_aes = key_bytes[::-1] 
    
    cipher = AES.new(key_aes, AES.MODE_ECB)
    ciphertext = bytearray()

    with open(args.input, 'rb') as f:
        data = f.read()

    # Zephyr 1024 byte padding eklediyse temizle (veya direkt oku)
    plaintext = data[1024:] if len(data) > 1024 else data

    print(f"[*] OTFDEC Adres-Tabanlı Şifreleme: {hex(APP_START_ADDR)}")

    for i in range(0, len(plaintext), 16):
        block = plaintext[i:i+16]
        if len(block) < 16: block = block.ljust(16, b'\x00')
        
        curr_addr = APP_START_ADDR + i
        
        # 2. IV (Counter) Oluşturma (AN5281 Sf. 5 ve RM0455)
        # Yapı: [NonceL, NonceH, Version, Address|Region]
        # Bu yapı AES çekirdeğine girerken de tamamen aynalanır (Full Reverse).
        iv_le = struct.pack('<IIII', NONCE_WORDS[0], NONCE_WORDS[1], 
                            (VERSION << 16), (curr_addr & 0xFFFFFFF0) | REGION_INDEX)
        iv_aes = iv_le[::-1] 

        # 3. Keystream Üret ve Aynala (Post-processing)
        # Donanım, AES çekirdeğinden çıkan sonucu XOR'lamadan önce tekrar aynalar.
        keystream_aes = cipher.encrypt(iv_aes)
        ks_final = keystream_aes[::-1]

        # 4. XOR İşlemi
        for b, k in zip(block, ks_final):
            ciphertext.append(b ^ k)

    with open(args.output, 'wb') as f:
        f.write(ciphertext[:len(plaintext)])

    print(f"[!] Şifreleme AN5281 ve RM0455 standartlarına göre tamamlandı.")

if __name__ == '__main__':
    main()