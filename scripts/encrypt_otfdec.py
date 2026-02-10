import argparse
from Crypto.Cipher import AES

def reverse_bytes_in_chunks(data):
    """16-byte chunk'lar halinde ters çevir - her zaman 16'nın katı döner"""
    result = bytearray()
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        if len(chunk) < 16:
            # Son chunk 16'dan küçükse padding ekle
            chunk = chunk + bytes(16 - len(chunk))
        result.extend(chunk[::-1])
    return bytes(result)

def main():
    parser = argparse.ArgumentParser(description='STM32H7 OTFDEC - Windows Script Clone')
    parser.add_argument('input', help='Giriş dosyası (zephyr.bin)')
    parser.add_argument('output', help='Çıkış dosyası (zephyr_enc.bin)')
    parser.add_argument('--key', default="210FEDCBA9876543210FEDCBA9876543", 
                        help='Encryption key (hex)')
    parser.add_argument('--iv', default="55555555111111110000A5A519000040",
                        help='IV (hex)')
    parser.add_argument('--skip-bytes', type=int, default=1024,
                        help='Bytes to skip from input')
    args = parser.parse_args()

    KEY_HEX = args.key
    IV_HEX = args.iv
    SKIP_BYTES = args.skip_bytes

    print(f"[*] KEY: {KEY_HEX}")
    print(f"[*] IV: {IV_HEX}")
    print(f"[*] SKIP: {SKIP_BYTES} bytes")
    
    # Key ve IV'yi bytes'a çevir
    key = bytes.fromhex(KEY_HEX)
    iv = bytes.fromhex(IV_HEX)

    # Input dosyasını oku
    with open(args.input, 'rb') as f:
        data = f.read()

    # MCUboot padding'i atla
    plaintext = data[SKIP_BYTES:] if len(data) > SKIP_BYTES else data
    original_length = len(plaintext)
    
    # 16'nın katına yuvarla (Windows reverse_byte_in_binary.py bunu yapıyor)
    padded_length = ((original_length + 15) // 16) * 16
    
    print(f"[*] Plaintext: {original_length} bytes")
    print(f"[*] Padded length: {padded_length} bytes ({padded_length - original_length} byte padding)")
    
    # STEP 1: Byte reversal (pre-processing)
    print("[*] Step 1: Reversing bytes...")
    plaintext_reversed = reverse_bytes_in_chunks(plaintext)
    print(f"[*] After reverse: {len(plaintext_reversed)} bytes")
    
    # STEP 2: AES-CTR encryption
    print("[*] Step 2: AES-CTR encryption...")
    cipher = AES.new(key, AES.MODE_CTR, initial_value=iv, nonce=b'')
    encrypted_reversed = cipher.encrypt(plaintext_reversed)
    print(f"[*] After encryption: {len(encrypted_reversed)} bytes")
    
    # STEP 3: Byte reversal (post-processing)
    print("[*] Step 3: Reversing encrypted bytes...")
    encrypted_final = reverse_bytes_in_chunks(encrypted_reversed)
    print(f"[*] After final reverse: {len(encrypted_final)} bytes")
    
    # Windows scripti PADDING DAHİL yazıyor (16'nın katı)
    # Bizim reverse_bytes_in_chunks her zaman 16'nın katı döndürüyor
    with open(args.output, 'wb') as f:
        f.write(encrypted_final)  # Tamamını yaz, kesme!

    import os
    actual_size = os.path.getsize(args.output)
    
    print(f"[✓] Tamamlandı: {args.output}")
    print(f"[✓] Original plaintext: {original_length} bytes")
    print(f"[✓] Output file size: {actual_size} bytes")
    print(f"[✓] Padding: {actual_size - original_length} bytes")

if __name__ == '__main__':
    main()