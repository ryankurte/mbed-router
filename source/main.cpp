/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */

#include <string.h>

#include "mbed.h"
#include "borderrouter_tasklet.h"
#include "drivers/eth_driver.h"
#include "sal-stack-nanostack-slip/Slip.h"

#include "ns_hal_init.h"
#include "cmsis_os.h"
#include "arm_hal_interrupt.h"

#include "mbed_trace.h"
#define TRACE_GROUP "app"

#define APP_DEFINED_HEAP_SIZE MBED_CONF_APP_HEAP_SIZE
static uint8_t app_stack_heap[APP_DEFINED_HEAP_SIZE];
static uint8_t mac[6] = {0};
static mem_stat_t heap_info;

static DigitalOut led1(LED0);

static Ticker led_ticker;

static void app_heap_error_handler(heap_fail_t event);

static void toggle_led1() {
    led1 = !led1;
}

static int8_t slipdrv_id;

/**
 * \brief Initializes the SLIP MAC backhaul driver.
 * This function is called by the border router module.
 */
void backhaul_driver_init(void (*backhaul_driver_status_cb)(uint8_t, int8_t)) {
    SlipMACDriver *pslipmacdriver = new SlipMACDriver(SERIAL_TX, SERIAL_RX);
    if (pslipmacdriver == NULL) {
        printf("Unable to create SLIP driver");
        return;
    }

    printf("Using SLIP backhaul driver...");

    slipdrv_id = pslipmacdriver->Slip_Init(mac, MBED_CONF_APP_SLIP_SERIAL_BAUD_RATE);
    if (slipdrv_id >= 0) {
        backhaul_driver_status_cb(1, slipdrv_id);
        return;
    }

    printf("Backhaul driver init failed, retval = %d", slipdrv_id);
}

/**
 * \brief The entry point for this application.
 * Sets up the application and starts the border router module.
 */

int main(int argc, char **argv) {
    ns_hal_init(app_stack_heap, APP_DEFINED_HEAP_SIZE, app_heap_error_handler, &heap_info);

    //mbed_trace_init(); // set up the tracing library
    //mbed_trace_print_function_set(trace_printer);
    //mbed_trace_config_set(TRACE_LEVEL_DEBUG | TRACE_CARRIAGE_RETURN);

    printf("Starting border router\n");

    // Fetch MAC address from UID
    mbed_mac_address((char *)mac);

    led_ticker.attach_us(toggle_led1, 500000);
    border_router_tasklet_start();
}

/**
 * \brief Error handler for errors in dynamic memory handling.
 */
static void app_heap_error_handler(heap_fail_t event) {
    printf("Dyn mem error %x", (int8_t)event);

    switch (event) {
        case NS_DYN_MEM_NULL_FREE:
            break;
        case NS_DYN_MEM_DOUBLE_FREE:
            break;
        case NS_DYN_MEM_ALLOCATE_SIZE_NOT_VALID:
            break;
        case NS_DYN_MEM_POINTER_NOT_VALID:
            break;
        case NS_DYN_MEM_HEAP_SECTOR_CORRUPTED:
            break;
        case NS_DYN_MEM_HEAP_SECTOR_UNITIALIZED:
            break;
        default:
            break;
    }

    while (1);
}

