
// Copyright (c) 2026
// SPDX-License-Identifier: Apache-2.0
// Coskun ERGAN <coskunergan@gmail.com>

use embassy_executor::Spawner;
use embassy_futures::yield_now;
use zephyr::raw::{device, device_get_binding};
use core::ptr::addr_of_mut;

//====================================================================================

#[repr(C)]
struct UartDriverApi {
    poll_in: Option<unsafe extern "C" fn(dev: *const zephyr::raw::device, p_char: *mut u8) -> i32>,
    poll_out: Option<unsafe extern "C" fn(dev: *const zephyr::raw::device, out_char: u8)>,
    err_check: Option<unsafe extern "C" fn(dev: *const zephyr::raw::device) -> i32>,
}

unsafe fn rust_uart_poll_in(dev: *const zephyr::raw::device, p_char: *mut u8) -> i32 {
    if dev.is_null() { return -1; }
    let api = (*dev).api as *const UartDriverApi;
    if let Some(func) = (*api).poll_in {
        return func(dev, p_char);
    }
    -1
}

unsafe fn rust_uart_poll_out(dev: *const zephyr::raw::device, out_char: u8) {
    if dev.is_null() { return; }
    let api = (*dev).api as *const UartDriverApi;
    if let Some(func) = (*api).poll_out {
        func(dev, out_char);
    }
}

unsafe fn rust_uart_err_check(dev: *const zephyr::raw::device) {
    if dev.is_null() { return; }
    let api = (*dev).api as *const UartDriverApi;
    if let Some(func) = (*api).err_check {
        func(dev);
    }
}

fn get_uart_device(name: &[u8]) -> Option<*const zephyr::raw::device> {
    unsafe {
        let dev: *const device = device_get_binding(name.as_ptr() as *const core::ffi::c_char);
        if dev.is_null() { None } else { Some(dev) }
    }
}

//====================================================================================

struct SimpleRingBuf<const N: usize> {
    buf: [u8; N],
    head: usize,
    tail: usize,
}

impl<const N: usize> SimpleRingBuf<N> {
    const fn new() -> Self {
        Self { buf: [0; N], head: 0, tail: 0 }
    }

    fn push(&mut self, byte: u8) -> bool {
        let next = (self.head + 1) % N;
        if next == self.tail { return false; }
        self.buf[self.head] = byte;
        self.head = next;
        true
    }

    fn pop(&mut self) -> Option<u8> {
        if self.head == self.tail { return None; }
        let byte = self.buf[self.tail];
        self.tail = (self.tail + 1) % N;
        Some(byte)
    }

    #[allow(dead_code)]
    fn is_empty(&self) -> bool {
        self.head == self.tail
    }
}

static mut BUF_A_TO_B: SimpleRingBuf<8192> = SimpleRingBuf::new();
static mut BUF_B_TO_A: SimpleRingBuf<8192> = SimpleRingBuf::new();

//====================================================================================

unsafe fn read_until_silence<const N: usize>(
    dev: *const zephyr::raw::device,
    buf_ptr: *mut SimpleRingBuf<N>, 
    silence_threshold: usize
) -> bool {
    let mut byte: u8 = 0;
    let mut silence_count = 0;

    if rust_uart_poll_in(dev, &mut byte) != 0 {
        return false;
    }
    
    (*buf_ptr).push(byte);

    loop {
        if rust_uart_poll_in(dev, &mut byte) == 0 {
            (*buf_ptr).push(byte);
            silence_count = 0;
        } else {
            rust_uart_err_check(dev);
            
            silence_count += 1;
            
            if silence_count > silence_threshold {
                break;
            }
            core::hint::spin_loop();
        }
    }
    
    true
}

//====================================================================================

#[embassy_executor::task]
async fn super_bridge_task() {
    let dev_a = get_uart_device(b"serial@40011000\0"); // UART A
    let dev_b = get_uart_device(b"serial@40004800\0"); // UART B

    if dev_a.is_none() || dev_b.is_none() { return; }
    let uart_a = dev_a.unwrap();
    let uart_b = dev_b.unwrap();

    let buf_a_ptr =  addr_of_mut!(BUF_A_TO_B);
    let buf_b_ptr =  addr_of_mut!(BUF_B_TO_A);

    loop {
        let mut activity = false;

        if unsafe { read_until_silence(uart_a, buf_a_ptr, 5000) } {
            activity = true;
        }

        if unsafe { read_until_silence(uart_b, buf_b_ptr, 20000) } {
            activity = true;
        }
        
        // A -> B
        unsafe {
            while let Some(byte) = (*buf_a_ptr).pop() {
                rust_uart_poll_out(uart_b, byte);
            }
        }

        // B -> A
        unsafe {
            while let Some(byte) = (*buf_b_ptr).pop() {
                rust_uart_poll_out(uart_a, byte);
            }
        }

        if !activity {
            yield_now().await;
        }
    }
}

pub fn spawn_uart_bridge(spawner: Spawner) -> Result<(), ()> {
    spawner.spawn(super_bridge_task()).ok();
    Ok(())
}
