// Copyright (c) 2025
// SPDX-License-Identifier: Apache-2.0
// Coskun ERGAN <coskunergan@gmail.com>

#![no_std]

extern crate alloc;
use alloc::format;

use embassy_time::{Duration, Timer};

#[cfg(feature = "executor-thread")]
use embassy_executor::Executor;

#[cfg(feature = "executor-zephyr")]
use zephyr::embassy::Executor;

use embassy_executor::Spawner;
use static_cell::StaticCell;

use zephyr::device::gpio::GpioPin;

use core::{sync::atomic::AtomicU16, sync::atomic::Ordering};

use pin::{GlobalPin, Pin};

mod button;
mod pin;
mod usage;

static EXECUTOR_MAIN: StaticCell<Executor> = StaticCell::new();
static RED_LED_PIN: GlobalPin = GlobalPin::new();
static GREEN_LED_PIN: GlobalPin = GlobalPin::new();

static COUNTER: AtomicU16 = AtomicU16::new(0);
static REGISTER: AtomicU16 = AtomicU16::new(0);

extern "C" {
    fn ext_flash_loader_start(timeout_sec: u32) -> i32;
}

//====================================================================================
//====================================================================================
#[embassy_executor::task]
async fn led_task(spawner: Spawner) {
    let red_led_pin = RED_LED_PIN.get();
    let green_led_pin = GREEN_LED_PIN.get();

    let button = zephyr::devicetree::labels::button::get_instance().unwrap();

    declare_buttons!(
        spawner,
        [(
            button,
            || {
                zephyr::printk!("Button Pressed!\n");
                REGISTER.fetch_add(1, Ordering::SeqCst);
                green_led_pin.toggle();
            },
            Duration::from_millis(10)
        )]
    );

    loop {
        let _ = Timer::after(Duration::from_millis(100)).await;
        red_led_pin.toggle();
        green_led_pin.toggle();
        log::info!("Endless Loop!");
        COUNTER.fetch_add(1, Ordering::SeqCst);
    }
}
//====================================================================================
fn receive_callback(data: &[u8]) {
    if let Ok(s) = core::str::from_utf8(data) {
        log::info!("Received data ({} byte): {}", data.len(), s);
    } else {
        log::info!(
            "Received data is not a valid UTF-8 string. Raw data ({} bytes): {:?}",
            data.len(),
            data
        );
    }
}
//====================================================================================
#[no_mangle]
extern "C" fn rust_main() {
    let _ = usage::set_logger();

    log::info!("Restart!!!\r\n");

    unsafe {
        ext_flash_loader_start(23);
    }

    RED_LED_PIN.init(Pin::new(
        zephyr::devicetree::labels::my_red_led::get_instance().expect("my_red_led not found!"),
    ));
    GREEN_LED_PIN.init(Pin::new(
        zephyr::devicetree::labels::my_green_led::get_instance().expect("my_green_led not found!"),
    ));

    let executor = EXECUTOR_MAIN.init(Executor::new());
    executor.run(|spawner| {
        spawner.spawn(led_task(spawner)).unwrap();
    })
}
//====================================================================================
//====================================================================================
//====================================================================================
