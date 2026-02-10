// Copyright (c) 2026
// SPDX-License-Identifier: Apache-2.0
// Coskun ERGAN <coskunergan@gmail.com>

#![no_std]

extern crate alloc;

use embassy_time::{Duration, Timer};

#[cfg(feature = "executor-thread")]
use embassy_executor::Executor;

#[cfg(feature = "executor-zephyr")]
use zephyr::embassy::Executor;

use embassy_executor::Spawner;
use static_cell::StaticCell;

use zephyr::device::gpio::GpioPin;

use pin::{GlobalPin, Pin};

mod button;
mod pin;
mod uart_bridge;
mod usage;

static EXECUTOR_MAIN: StaticCell<Executor> = StaticCell::new();
static RED_LED_PIN: GlobalPin = GlobalPin::new();
static GREEN_LED_PIN: GlobalPin = GlobalPin::new();
static BOOT0_PIN: GlobalPin = GlobalPin::new();
static NRST_PIN: GlobalPin = GlobalPin::new();

//====================================================================================
//====================================================================================
#[embassy_executor::task]
async fn led_task(spawner: Spawner) {

    let button = zephyr::devicetree::labels::button::get_instance().unwrap();

    declare_buttons!(
        spawner,
        [(
            button,
            move |state| {
                if state {
                    //zephyr::printk!("Button Pressed!\n");
                } else {
                    //zephyr::printk!("Button Released!\n");
                }
                GREEN_LED_PIN.get().toggle();
            },
            Duration::from_millis(10)
        )]
    );

    BOOT0_PIN.get().set(true);
    NRST_PIN.get().set(false);
    Timer::after(Duration::from_millis(30)).await;    
    NRST_PIN.get().set(true);    
 
    zephyr::printk!("Bridge ready.");    
    Timer::after(Duration::from_millis(10)).await;   

    let _ = uart_bridge::spawn_uart_bridge(spawner);

    loop {
        let _ = Timer::after(Duration::from_millis(20)).await;
        RED_LED_PIN.get().toggle();
        GREEN_LED_PIN.get().toggle();       
        //log::info!("Endless Loop!");
    }
}
//====================================================================================
#[no_mangle]
extern "C" fn rust_main() {
    //let _ = usage::set_logger();

    BOOT0_PIN.init(Pin::new(
        zephyr::devicetree::labels::boot0_pin::get_instance().expect("boot0 not found!"),
    ));
    NRST_PIN.init(Pin::new(
        zephyr::devicetree::labels::nrst_pin::get_instance().expect("nrst not found!"),
    ));

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
