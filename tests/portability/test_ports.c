/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#include "ports.h"
#include <assert.h>
#include <stdio.h>

static void test_stm32c0_port(void) {
    const ClockPortInterface *clk = ports_stm32c0_clock();
    assert(clk != NULL);
    assert(clk->get_tick_ms != NULL);
    assert(clk->delay_ms != NULL);

    const FlashPortInterface *flash = ports_stm32c0_flash();
    assert(flash != NULL);
    assert(flash->program_granule == 8U);
    assert(flash->erased_byte == 0xFFU);
    assert(flash->unlock != NULL);
    assert(flash->lock != NULL);
    assert(flash->erase_sector != NULL);
    assert(flash->program != NULL);
}

static void test_stm32f1_port(void) {
    const ClockPortInterface *clk = ports_stm32f1_clock();
    assert(clk != NULL);
    assert(clk->get_tick_ms != NULL);

    const FlashPortInterface *flash = ports_stm32f1_flash();
    assert(flash != NULL);
    assert(flash->program_granule == 2U);
    assert(flash->erased_byte == 0xFFU);
    assert(flash->unlock != NULL);
    assert(flash->lock != NULL);
    assert(flash->erase_sector != NULL);
    assert(flash->program != NULL);
}

static void test_stm32f4_port(void) {
    const ClockPortInterface *clk = ports_stm32f4_clock();
    assert(clk != NULL);

    const FlashPortInterface *flash = ports_stm32f4_flash();
    assert(flash != NULL);
    assert(flash->program_granule == 4U);
    assert(flash->erased_byte == 0xFFU);
    assert(flash->unlock != NULL);
    assert(flash->lock != NULL);
    assert(flash->erase_sector != NULL);
    assert(flash->program != NULL);
}

static void test_stm32f7_port(void) {
    const ClockPortInterface *clk = ports_stm32f7_clock();
    assert(clk != NULL);

    const FlashPortInterface *flash = ports_stm32f7_flash();
    assert(flash != NULL);
    assert(flash->program_granule == 4U);
    assert(flash->erased_byte == 0xFFU);
    assert(flash->unlock != NULL);
    assert(flash->lock != NULL);
    assert(flash->erase_sector != NULL);
    assert(flash->program != NULL);
}

static void test_stm32g4_port(void) {
    const ClockPortInterface *clk = ports_stm32g4_clock();
    assert(clk != NULL);

    const FlashPortInterface *flash = ports_stm32g4_flash();
    assert(flash != NULL);
    assert(flash->program_granule == 8U);
    assert(flash->erased_byte == 0xFFU);
    assert(flash->unlock != NULL);
    assert(flash->lock != NULL);
    assert(flash->erase_sector != NULL);
    assert(flash->program != NULL);
}

int main(void) {
    test_stm32c0_port();
    test_stm32f1_port();
    test_stm32f4_port();
    test_stm32f7_port();
    test_stm32g4_port();
    printf("All STM32 hardware abstraction ports passed tests successfully.\n");
    return 0;
}
