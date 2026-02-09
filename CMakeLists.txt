# SPDX-License-Identifier: Apache-2.0 OR MIT

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(Project)

file(GLOB SRC_C_FILES CONFIGURE_DEPENDS "src/*.c")
target_sources(app PRIVATE ${SRC_C_FILES})

rust_cargo_application()


# ==============================================================================
# OTFDEC ENCRYPTION & SIGNING
# ==============================================================================

#set(RAW_SCRIPT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/scripts/delete_header.py")
set(SCRIPT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/scripts/encrypt_otfdec.py")
set(KEY_FILE    "${CMAKE_CURRENT_SOURCE_DIR}/../../bootloader/mcuboot/root-rsa-2048.pem")

# Dosya Yolları
set(ZEPHYR_BIN  "${CMAKE_BINARY_DIR}/zephyr/zephyr.bin") 
set(RAW_BIN     "${CMAKE_BINARY_DIR}/zephyr/zephyr_raw.bin")
set(ENC_BIN     "${CMAKE_BINARY_DIR}/zephyr/zephyr_enc.bin")
set(SIGNED_BIN  "${CMAKE_BINARY_DIR}/zephyr/zephyr_signed_otfdec.bin")
set(FINAL_HEX   "${CMAKE_BINARY_DIR}/zephyr/zephyr_signed_otfdec.hex")

add_custom_target(otfdec_process
    ALL
    DEPENDS zephyr_final

    # 1. ADIM: Python Scriptini Çalıştır
    #COMMAND python3 ${RAW_SCRIPT_PATH} ${ZEPHYR_BIN} ${RAW_BIN}

    COMMAND python3 ${SCRIPT_PATH} ${ZEPHYR_BIN} ${ENC_BIN}

    # 2. ADIM: imgtool ile Tek bir 0x400 Header ekle ve imzala.
    COMMAND imgtool sign
            --key ${KEY_FILE}
            --header-size 0x400
            --align 32
            --version 1.0.0+1
            --slot-size 0x200000
            --pad-header
            ${ENC_BIN}
            ${SIGNED_BIN}

    # 3. ADIM: BIN -> HEX 
    COMMAND ${CMAKE_OBJCOPY} 
            -I binary -O ihex 
            --change-address 0x90000000 
            ${SIGNED_BIN} ${FINAL_HEX}

    COMMENT ">>> [OTFDEC] Zephyr padding temizlendi ve düzgünce imzalandi."
)