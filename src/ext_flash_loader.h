// Copyright (c) 2025
// SPDX-License-Identifier: Apache-2.0
// Coskun ERGAN <coskunergan@gmail.com>
// STM32 Bootloader for External Flash Programming

//#pragma once

#include <stdint.h>

/**
 * @brief Start external flash loader in blocking mode
 * 
 * This function implements AN2606 UART bootloader protocol to program
 * external flash memory. It waits for 0x7F sync byte and processes
 * erase/write/read commands until timeout or completion.
 * 
 * Supported commands:
 * - 0x44: Extended Erase
 * - 0x31: Write Memory
 * - 0x11: Read Memory
 * 
 * @param timeout_sec Maximum time to wait for activity (seconds)
 * @return 0 on success, negative error code on failure
 *         -ETIMEDOUT: No activity within timeout period
 *         -EIO: Communication or flash operation error
 */
int ext_flash_loader_start(uint32_t timeout_sec);

// Protocol constants
#define EFL_ACK             0x79
#define EFL_NACK            0x1F
#define EFL_SYNC_BYTE       0x7F

#define EFL_CMD_ERASE       0x44
#define EFL_CMD_WRITE       0x31
#define EFL_CMD_READ        0x11

#define EFL_ERASE_ALL       0xFFFF

// Timeout values (milliseconds)
#define EFL_SYNC_TIMEOUT    5000
#define EFL_CMD_TIMEOUT     10000
#define EFL_ERASE_TIMEOUT   30000
#define EFL_WRITE_TIMEOUT   2000
#define EFL_READ_TIMEOUT    2000

// Flash parameters
#define EFL_MAX_CHUNK_SIZE  256
#define EFL_FLASH_BASE      0x90000000