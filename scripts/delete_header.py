import argparse
import struct

def main():
    parser = argparse.ArgumentParser(description='Header silici..')
    
    parser.add_argument('input', help='Giriş dosyası (zephyr.bin)')
    parser.add_argument('output', help='Çıkış dosyası (zephyr_raw.bin)')
    args = parser.parse_args()

    ciphertext = bytearray()

    with open(args.input, 'rb') as f:
        data = f.read()

    # Zephyr 1024 byte padding eklediyse temizle (veya direkt oku)
    plaintext = data[1024:] if len(data) > 1024 else data

    print(f"[*] dosyadan 1kb giris header siliniyor.")

    with open(args.output, 'wb') as f:
        f.write(plaintext)

    print(f"[!] silme islemi tamamlandı.")

if __name__ == '__main__':
    main()