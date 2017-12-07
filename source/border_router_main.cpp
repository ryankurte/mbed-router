/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */

#include <string.h>

#include "mbed.h"
#include "borderrouter_tasklet.h"
#include "drivers/eth_driver.h"
#include "sal-stack-nanostack-slip/Slip.h"

#ifdef MBED_CONF_APP_DEBUG_TRACE
#if MBED_CONF_APP_DEBUG_TRACE == 1
#define APP_TRACE_LEVEL TRACE_ACTIVE_LEVEL_DEBUG
#else
#define APP_TRACE_LEVEL TRACE_ACTIVE_LEVEL_INFO
#endif
#endif //MBED_CONF_APP_DEBUG_TRACE

#include "ns_hal_init.h"
#include "cmsis_os.h"
#include "arm_hal_interrupt.h"
#include "mbed_mem_trace.h"
#include "mbed_stats.h"

#include "mbed_trace.h"
#define TRACE_GROUP "app"

#define tr_debug(...) printf(__VA_ARGS__)

#define APP_DEFINED_HEAP_SIZE MBED_CONF_APP_HEAP_SIZE
static uint8_t app_stack_heap[APP_DEFINED_HEAP_SIZE];
static uint8_t mac[6] = {0};
static mem_stat_t heap_info;

static DigitalOut led1(LED0);
static DigitalOut led2(LED1);

static Ticker led_ticker;

static void app_heap_error_handler(heap_fail_t event);

static void toggle_led1()
{
    led1 = !led1;
}

/**
 * \brief Prints string to serial (adds CRLF).
 */
static void trace_printer(const char *str)
{
    printf("%s\n", str);
}

void stats_print()
{
    mbed_stats_heap_t heap_stats;
    mbed_stats_heap_get(&heap_stats);
    printf("Heap current: %lu reserved: %lu max: %lu\r\n", heap_stats.current_size, heap_stats.reserved_size, heap_stats.max_size);
}

/**
 * \brief Initializes the SLIP MAC backhaul driver.
 * This function is called by the border router module.
 */
void backhaul_driver_init(void (*backhaul_driver_status_cb)(uint8_t, int8_t))
{
    tr_debug("Backhaul driver init\n");

// Values allowed in "backhaul-driver" option
#define ETH 0
#define SLIP 1
#if MBED_CONF_APP_BACKHAUL_DRIVER == SLIP
    SlipMACDriver *pslipmacdriver;
    int8_t slipdrv_id = -1;
#if defined(MBED_CONF_APP_SLIP_HW_FLOW_CONTROL)
    pslipmacdriver = new SlipMACDriver(SERIAL_TX, SERIAL_RX, SERIAL_RTS, SERIAL_CTS);
#else
    pslipmacdriver = new SlipMACDriver(SERIAL_TX, SERIAL_RX);
#endif

    if (pslipmacdriver == NULL)
    {
        tr_debug("Unable to create SLIP driver");
        return;
    }

    tr_debug("Using SLIP backhaul driver...");

#ifdef MBED_CONF_APP_SLIP_SERIAL_BAUD_RATE
    slipdrv_id = pslipmacdriver->Slip_Init(mac, MBED_CONF_APP_SLIP_SERIAL_BAUD_RATE);
    tr_debug("slip set baud rate to %d\n", MBED_CONF_APP_SLIP_SERIAL_BAUD_RATE);
#else
    tr_debug("baud rate for slip not defined");
#endif

    if (slipdrv_id >= 0)
    {
        backhaul_driver_status_cb(1, slipdrv_id);
        return;
    }

    tr_debug("Backhaul driver init failed, retval = %d", slipdrv_id);
#elif MBED_CONF_APP_BACKHAUL_DRIVER == ETH
    tr_debug("Using ETH backhaul driver...");
    arm_eth_phy_device_register(mac, backhaul_driver_status_cb);
    return;
#else
#error "Unsupported backhaul driver"
#endif
}

/**
 * \brief The entry point for this application.
 * Sets up the application and starts the border router module.
 */

//Serial pc(PA6, PA7, 115200);

int main(int argc, char **argv)
{
    ns_hal_init(app_stack_heap, APP_DEFINED_HEAP_SIZE, app_heap_error_handler, &heap_info);

    mbed_trace_init(); // set up the tracing library
    //mbed_trace_print_function_set(trace_printer);
    //mbed_trace_config_set(TRACE_MODE_PLAIN | APP_TRACE_LEVEL | TRACE_CARRIAGE_RETURN);

#define BOARD 0
#define CONFIG 1
#if MBED_CONF_APP_BACKHAUL_MAC_SRC == BOARD
    /* Setting the MAC Address from UID.
     * Takes UID Mid low and UID low and shuffles them around. */
    mbed_mac_address((char *)mac);
#elif MBED_CONF_APP_BACKHAUL_MAC_SRC == CONFIG
    const uint8_t mac48[] = MBED_CONF_APP_BACKHAUL_MAC;
    for (uint32_t i = 0; i < sizeof(mac); ++i)
    {
        mac[i] = mac48[i];
    }
#else
#error "MAC address not defined"
#endif

    if (MBED_CONF_APP_LED != NC)
    {
        led_ticker.attach_us(toggle_led1, 500000);
    }
    border_router_tasklet_start();
}

/**
 * \brief Error handler for errors in dynamic memory handling.
 */
static void app_heap_error_handler(heap_fail_t event)
{
    tr_error("Dyn mem error %x", (int8_t)event);

    switch (event)
    {
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

    while (1)
        ;
}

void os_error(uint32_t error_code)
{
    printf("OS Error %u\n", error_code);

    __asm__("BKPT #0");
    while (1)
        ;
}

void HardFault_Handler(void)
{

    SCB_Type *scb = SCB;
    (void *)scb;

    led1 = 1;
    led2 = 1;

    if (SCB->HFSR & SCB_HFSR_FORCED_Msk)
    {

        // Forced fault
        size_t mmu_fault = (SCB->CFSR >> 0) & 0xFF;
        if (SCB->CFSR & SCB_CFSR_MMARVALID_Msk)
            __asm("BKPT #0");
        if (SCB->CFSR & SCB_CFSR_MLSPERR_Msk)
            __asm("BKPT #0");
        if (SCB->CFSR & SCB_CFSR_MSTKERR_Msk)
            __asm("BKPT #0");
        if (SCB->CFSR & SCB_CFSR_MUNSTKERR_Msk)
            __asm("BKPT #0");
        if (SCB->CFSR & SCB_CFSR_DACCVIOL_Msk)
            __asm("BKPT #0");
        if (SCB->CFSR & SCB_CFSR_IACCVIOL_Msk)
            __asm("BKPT #0");

        size_t bus_fault = (SCB->CFSR >> 8) & 0xFF;
        if (SCB->CFSR & SCB_CFSR_BFARVALID_Msk)
            __asm("BKPT #0");
        if (SCB->CFSR & SCB_CFSR_LSPERR_Msk)
            __asm("BKPT #0");
        if (SCB->CFSR & SCB_CFSR_STKERR_Msk)
            __asm("BKPT #0");
        if (SCB->CFSR & SCB_CFSR_UNSTKERR_Msk)
            __asm("BKPT #0");
        if (SCB->CFSR & SCB_CFSR_IMPRECISERR_Msk)
            __asm("BKPT #0");
        if (SCB->CFSR & SCB_CFSR_PRECISERR_Msk)
            __asm("BKPT #0");
        if (SCB->CFSR & SCB_CFSR_IBUSERR_Msk)
            __asm("BKPT #0");

        size_t usage_fault = (SCB->CFSR >> 16) & 0xFFF;
        if (SCB->CFSR & SCB_CFSR_DIVBYZERO_Msk)
            __asm("BKPT 0");
        if (SCB->CFSR & SCB_CFSR_UNALIGNED_Msk)
            __asm("BKPT 0");
        if (SCB->CFSR & SCB_CFSR_NOCP_Msk)
            __asm("BKPT 0");
        if (SCB->CFSR & SCB_CFSR_INVPC_Msk)
            __asm("BKPT 0");
        if (SCB->CFSR & SCB_CFSR_INVSTATE_Msk)
            __asm("BKPT 0");
        if (SCB->CFSR & SCB_CFSR_UNDEFINSTR_Msk)
            __asm("BKPT 0");

        __asm("BKPT #0");
    }
    else if (SCB->HFSR & SCB_HFSR_VECTTBL_Msk)
    {
        // Vector table bus fault

        __asm("BKPT #0");
    }

    __asm("BKPT #0");
    while (1)
        ;
}
