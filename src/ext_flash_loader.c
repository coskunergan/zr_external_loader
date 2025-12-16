// Copyright (c) 2025
// SPDX-License-Identifier: Apache-2.0
// Coskun ERGAN <coskunergan@gmail.com>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
#include "ext_flash_loader.h"

LOG_MODULE_REGISTER(ext_flash_loader, LOG_LEVEL_INF);

#define UART_DEV DEVICE_DT_GET(DT_ALIAS(usart3))

static const struct flash_area *flash_area_ptr = NULL;
static bool flash_initialized = false;
static int64_t last_activity_time = 0;

// Helper: Calculate XOR checksum
static uint8_t calculate_checksum(const uint8_t *data, uint16_t len)
{
    uint8_t checksum = 0;
    for(uint16_t i = 0; i < len; i++)
    {
        checksum ^= data[i];
    }
    return checksum;
}

// Helper: Send ACK byte
static void send_ack(void)
{
    uart_poll_out(UART_DEV, EFL_ACK);
    LOG_DBG("Sent ACK");
}

// Helper: Send NACK byte
static void send_nack(void)
{
    uart_poll_out(UART_DEV, EFL_NACK);
    LOG_WRN("Sent NACK");
}

// Helper: Receive byte with timeout
static int receive_byte(uint8_t *byte, int timeout_ms)
{
    int64_t start = k_uptime_get();

    while(k_uptime_get() - start < timeout_ms)
    {
        if(uart_poll_in(UART_DEV, byte) == 0)
        {
            last_activity_time = k_uptime_get();
            return 0;
        }
        //k_usleep(500);
    }

    return -ETIMEDOUT;
}

// Helper: Receive multiple bytes with timeout
static int receive_bytes(uint8_t *buffer, uint32_t len, int timeout_ms)
{
    for(uint32_t i = 0; i < len; i++)
    {
        if(receive_byte(&buffer[i], timeout_ms) < 0)
        {
            return -ETIMEDOUT;
        }
    }
    return 0;
}

// Helper: Wait for sync byte (0x7F)
static int wait_for_sync(int timeout_ms)
{
    uint8_t byte;
    LOG_INF("Waiting for sync byte (0x7F)...");

    if(receive_byte(&byte, timeout_ms) < 0)
    {
        LOG_ERR("Sync timeout");
        return -ETIMEDOUT;
    }

    if(byte != EFL_SYNC_BYTE)
    {
        LOG_ERR("Invalid sync byte: 0x%02X (expected 0x7F)", byte);
        return -EIO;
    }

    LOG_INF("Sync received!");
    send_ack();
    return 0;
}

// Command: Extended Erase
static int cmd_extended_erase(void)
{
    LOG_INF("Extended Erase command started");

    uint8_t data[3];
    if(receive_bytes(data, 3, EFL_WRITE_TIMEOUT) < 0)
    {
        LOG_ERR("Erase parameters timeout");
        send_nack();
        return -ETIMEDOUT;
    }

    uint16_t erase_code = (data[0] << 8) | data[1];
    uint8_t checksum = data[2];

    if(checksum != (data[0] ^ data[1]))
    {
        LOG_ERR("Erase checksum failed");
        send_nack();
        return -EIO;
    }

    //send_ack();

    if(erase_code == EFL_ERASE_ALL)
    {
        LOG_INF("Erasing entire external flash...");

        if(!flash_area_ptr)
        {
            LOG_ERR("Flash area not initialized");
            send_nack();
            return -EIO;
        }

        int ret = flash_area_erase(flash_area_ptr, 0, flash_area_ptr->fa_size);
        if(ret != 0)
        {
            LOG_ERR("Flash erase failed: %d", ret);
            send_nack();
            return ret;
        }

        LOG_INF("Flash erased successfully (%u bytes)", flash_area_ptr->fa_size);
        send_ack();
        return 0;
    }
    else
    {
        LOG_WRN("Sector erase not implemented (code: 0x%04X)", erase_code);
        //send_nack();
        k_usleep(5000);
        uint8_t dummy;
        while(!uart_poll_in(UART_DEV, dummy));
        send_ack();
        return 0;
        //return -ENOTSUP;
    }
}

// Command: Write Memory
static int cmd_write_memory(void)
{
    LOG_DBG("Write Memory command started");

    // Receive address (4 bytes + 1 checksum)
    uint8_t addr_buf[5];
    if(receive_bytes(addr_buf, 5, EFL_WRITE_TIMEOUT) < 0)
    {
        LOG_ERR("Address receive timeout");
        send_nack();
        return -ETIMEDOUT;
    }

    uint32_t address = ((uint32_t)addr_buf[0] << 24) |
                       ((uint32_t)addr_buf[1] << 16) |
                       ((uint32_t)addr_buf[2] << 8) |
                       ((uint32_t)addr_buf[3]);

    uint8_t addr_checksum = calculate_checksum(addr_buf, 4);
    if(addr_checksum != addr_buf[4])
    {
        LOG_ERR("Address checksum failed");
        send_nack();
        return -EIO;
    }

    if(address < 0x90000000)
    {        
        LOG_ERR("Addres Data not fit. 0x90000000");
        send_nack();
        return -ETIMEDOUT;
    }
    else
    {
        address -= 0x90000000;
        send_ack();
    }

    // Receive data length
    uint8_t n;
    if(receive_byte(&n, EFL_WRITE_TIMEOUT) < 0)
    {
        LOG_ERR("Data length timeout");
        send_nack();
        return -ETIMEDOUT;
    }

    uint32_t data_len = n + 1; // N = length - 1

    // Receive data + checksum
    uint8_t data_buf[EFL_MAX_CHUNK_SIZE + 1];
    if(receive_bytes(data_buf, data_len + 1, EFL_WRITE_TIMEOUT) < 0)
    {
        LOG_ERR("Data receive timeout");
        send_nack();
        return -ETIMEDOUT;
    }

    uint8_t expected_checksum = n;
    for(uint32_t i = 0; i < data_len; i++)
    {
        expected_checksum ^= data_buf[i];
    }

    if(expected_checksum != data_buf[data_len])
    {
        LOG_ERR("Data checksum failed");
        send_nack();
        return -EIO;
    }

    // Write to external flash
    int ret = flash_area_write(flash_area_ptr, address, data_buf, data_len);
    if(ret != 0)
    {
        LOG_ERR("Flash write failed at 0x%08X: %d", address, ret);
        send_nack();
        return ret;
    }

    LOG_DBG("Written %u bytes to 0x%08X", data_len, address);
    send_ack();
    return 0;
}

// Command: Read Memory
static int cmd_read_memory(void)
{
    LOG_DBG("Read Memory command started");

    // Receive address (4 bytes + 1 checksum)
    uint8_t addr_buf[5];
    if(receive_bytes(addr_buf, 5, EFL_READ_TIMEOUT) < 0)
    {
        LOG_ERR("Address receive timeout");
        send_nack();
        return -ETIMEDOUT;
    }

    uint32_t address = ((uint32_t)addr_buf[0] << 24) |
                       ((uint32_t)addr_buf[1] << 16) |
                       ((uint32_t)addr_buf[2] << 8) |
                       ((uint32_t)addr_buf[3]);

    uint8_t addr_checksum = calculate_checksum(addr_buf, 4);
    if(addr_checksum != addr_buf[4])
    {
        LOG_ERR("Address checksum failed");
        send_nack();
        return -EIO;
    }

    if(address < 0x90000000)
    {        
        LOG_ERR("Addres Data not fit. 0x90000000");
        send_nack();
        return -ETIMEDOUT;
    }
    else
    {
        address -= 0x90000000;
        send_ack();
    }

    // Receive length (N and ~N)
    uint8_t len_buf[2];
    if(receive_bytes(len_buf, 2, EFL_READ_TIMEOUT) < 0)
    {
        LOG_ERR("Length receive timeout");
        send_nack();
        return -ETIMEDOUT;
    }

    uint8_t n = len_buf[0];
    uint8_t n_complement = len_buf[1];

    if(n != (uint8_t)(~n_complement))
    {
        LOG_ERR("Length complement check failed");
        send_nack();
        return -EIO;
    }

    uint32_t read_len = n + 1;
    send_ack();

    // Read from external flash
    uint8_t read_buf[EFL_MAX_CHUNK_SIZE];
    int ret = flash_area_read(flash_area_ptr, address, read_buf, read_len);
    if(ret != 0)
    {
        LOG_ERR("Flash read failed at 0x%08X: %d", address, ret);
        send_nack();
        return ret;
    }

    // Send data (no checksum in read response)
    for(uint32_t i = 0; i < read_len; i++)
    {
        uart_poll_out(UART_DEV, read_buf[i]);
    }

    LOG_DBG("Read %u bytes from 0x%08X", read_len, address);
    return 0;
}

// Main loader function
int ext_flash_loader_start(uint32_t timeout_sec)
{
    int ret;

    LOG_INF("========================================");
    LOG_INF("External Flash Loader Started V1.0.0");
    LOG_INF("Protocol: AN2606 UART Bootloader");
    LOG_INF("Timeout: %u seconds", timeout_sec);
    LOG_INF("========================================");

    k_msleep(100);

    struct uart_config cfg =
    {
        .baudrate = 115200,
        .parity   = UART_CFG_PARITY_EVEN,
        .stop_bits = UART_CFG_STOP_BITS_1,
        .data_bits = UART_CFG_DATA_BITS_8,
        .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
    };

    ret = uart_configure(UART_DEV, &cfg);
    if(ret)
    {
        LOG_ERR("UART configure error: %d\n", ret);
    }

    // Initialize flash area
    ret = flash_area_open(FIXED_PARTITION_ID(nor_part), &flash_area_ptr);
    if(ret != 0)
    {
        LOG_ERR("Failed to open nor_part: %d", ret);
        return ret;
    }

    LOG_INF("Flash partition opened: 0x%08X (%u bytes)",
            (uint32_t)flash_area_ptr->fa_off,
            flash_area_ptr->fa_size);

    flash_initialized = true;
    last_activity_time = k_uptime_get();

    // Wait for initial sync
    ret = wait_for_sync(EFL_SYNC_TIMEOUT);
    if(ret < 0)
    {
        goto cleanup;
    }

    // Main command loop
    uint64_t timeout_ms = timeout_sec * 1000;
    bool running = true;

    while(running)
    {
        // Check global timeout
        if((k_uptime_get() - last_activity_time) > timeout_ms)
        {
            LOG_WRN("Global timeout reached, exiting loader");
            ret = -ETIMEDOUT;
            break;
        }

        // Wait for command
        uint8_t cmd;
        if(receive_byte(&cmd, EFL_CMD_TIMEOUT) < 0)
        {
            LOG_WRN("Command timeout, exiting loader");
            ret = -ETIMEDOUT;
            break;
        }

        // Receive command complement
        uint8_t cmd_complement;
        if(receive_byte(&cmd_complement, 1000) < 0)
        {
            LOG_ERR("Command complement timeout");
            send_nack();
            continue;
        }

        // Verify complement
        if(cmd != (uint8_t)(~cmd_complement))
        {
            LOG_ERR("Command complement mismatch: 0x%02X vs 0x%02X", cmd, cmd_complement);
            send_nack();
            continue;
        }

        send_ack();

        // Execute command
        switch(cmd)
        {
            case EFL_CMD_ERASE:
                LOG_INF(">>> Command: Extended Erase (0x44)");
                ret = cmd_extended_erase();
                if(ret < 0)
                {
                    LOG_ERR("Erase command failed: %d", ret);
                }
                break;

            case EFL_CMD_WRITE:
                LOG_DBG(">>> Command: Write Memory (0x31)");
                ret = cmd_write_memory();
                if(ret < 0)
                {
                    LOG_ERR("Write command failed: %d", ret);
                }
                break;

            case EFL_CMD_READ:
                LOG_DBG(">>> Command: Read Memory (0x11)");
                ret = cmd_read_memory();
                if(ret < 0)
                {
                    LOG_ERR("Read command failed: %d", ret);
                }
                break;

            default:
                LOG_WRN("Unknown command: 0x%02X", cmd);
                send_nack();
                break;
        }
    }

cleanup:
    if(flash_area_ptr)
    {
        flash_area_close(flash_area_ptr);
    }

    LOG_INF("========================================");
    if(ret == 0)
    {
        LOG_INF("External Flash Loader: SUCCESS");
    }
    else if(ret == -ETIMEDOUT)
    {
        LOG_WRN("External Flash Loader: TIMEOUT");
    }
    else
    {
        LOG_ERR("External Flash Loader: FAILED (%d)", ret);
    }
    LOG_INF("========================================");

    return ret;
}