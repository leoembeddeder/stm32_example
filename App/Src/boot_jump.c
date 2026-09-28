#include "boot_jump.h"
#include "uds_bootloader.h"

#if defined(USE_HAL_DRIVER) || defined(STM32F767xx) || defined(STM32C092xx)
#include "main.h"
#endif

typedef void (*AppEntryFn)(void);

void uds_bootloader_jump_to_app(uint32_t app_vector_addr) {
#if defined(STM32C092xx) || defined(CORTEX_M0PLUS)
    /* Sanity gate: Validate vector table before attempting execution */
    if (!uds_bootloader_is_application_valid(app_vector_addr)) {
        return;
    }

    uint32_t app_msp = *(__IO uint32_t *)(uintptr_t)app_vector_addr;
    AppEntryFn app_entry =
        (AppEntryFn)(uintptr_t)(*(__IO uint32_t *)(uintptr_t)(app_vector_addr + 4U));

    /* 1. Disable all interrupts */
    __disable_irq();

    /* 2. Disable SysTick timer and clear pending register */
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    /* 3. Disable all peripherals & clear NVIC pending interrupts (single 32-bit register on Cortex-M0+) */
    NVIC->ICER[0] = 0xFFFFFFFFUL;
    NVIC->ICPR[0] = 0xFFFFFFFFUL;

    /* 4. Set Vector Table Offset Register (VTOR) */
    SCB->VTOR = app_vector_addr;

    /* 5. Reset CONTROL register to Privileged Thread Mode on MSP */
    __set_CONTROL(0U);

    /* 6. Memory Synchronization Barriers */
    __DSB();
    __ISB();

    /* 7. Set Main Stack Pointer and branch to reset handler */
    __set_MSP(app_msp);
    app_entry();
#elif defined(CORTEX_M7) || defined(STM32F767xx)
    /* Sanity gate: Validate vector table before attempting execution */
    if (!uds_bootloader_is_application_valid(app_vector_addr)) {
        return;
    }

    uint32_t app_msp = *(__IO uint32_t *)(uintptr_t)app_vector_addr;
    AppEntryFn app_entry =
        (AppEntryFn)(uintptr_t)(*(__IO uint32_t *)(uintptr_t)(app_vector_addr + 4U));

    /* 1. Disable all interrupts */
    __disable_irq();

    /* 2. Disable SysTick timer and clear pending register */
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    /* 3. Disable all peripherals & clear NVIC pending interrupts */
    for (uint32_t i = 0U; i < 8U; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFUL;
        NVIC->ICPR[i] = 0xFFFFFFFFUL;
    }

    /* 4. Clear pending system exceptions (PendSV, SysTick) */
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;

    /* 5. Disable and clean Cortex-M7 L1 Caches */
    SCB_DisableICache();
    SCB_DisableDCache();
    SCB_InvalidateICache();
    SCB_CleanInvalidateDCache();

    /* 6. Set Vector Table Offset Register (VTOR) */
    SCB->VTOR = app_vector_addr;

    /* 7. Reset CONTROL register to Privileged Thread Mode on MSP */
    __set_CONTROL(0U);

    /* 8. Memory Synchronization Barriers */
    __DSB();
    __ISB();

    /* 9. Set Main Stack Pointer and branch to reset handler */
    __set_MSP(app_msp);
    app_entry();
#else
    (void)app_vector_addr;
#endif
}
