/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */

#ifndef UDS_FLASH_FSM_H
#define UDS_FLASH_FSM_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Flash state machine lifecycle states.
 */
typedef enum {
    FLASH_STATE_IDLE = 0,
    FLASH_STATE_WRITE,
    FLASH_STATE_WRITE_DONE,
    FLASH_STATE_WRITE_FAIL,
    FLASH_STATE_ERASE,
    FLASH_STATE_ERASE_DONE,
    FLASH_STATE_ERASE_FAIL
} FlashState_t;

#define FLASH_FSM_CHUNK_SIZE 256U

/**
 * @brief Low-level driver write callback signature.
 * @param addr Flash target destination address.
 * @param data Pointer to chunk data buffer.
 * @param len Chunk length in bytes.
 * @retval true Write completed successfully.
 * @retval false Write failed.
 */
typedef bool (*FlashIf_WriteFn)(uint32_t addr, const uint8_t *data, uint16_t len);

/**
 * @brief Low-level driver erase callback signature.
 * @param addr Page or sector start address to erase.
 * @retval true Erase completed successfully.
 * @retval false Erase failed.
 */
typedef bool (*FlashIf_EraseFn)(uint32_t addr);

/**
 * @brief Flash FSM runtime context.
 */
typedef struct {
    FlashState_t state;
    uint8_t write_data[FLASH_FSM_CHUNK_SIZE];
    uint16_t write_length;
    uint32_t start_addr;
    uint32_t total_size;
    uint32_t cur_addr;
    uint32_t written_size;
    uint32_t erase_addr;
    FlashIf_WriteFn write_fn;
    FlashIf_EraseFn erase_fn;
} FlashContext_t;

/**
 * @brief Initializes the Flash State Machine and zeros context.
 */
void Flash_Init(void);

/**
 * @brief Registers low-level hardware flash driver callbacks.
 */
void Flash_SetHardwareInterface(FlashIf_WriteFn write_fn, FlashIf_EraseFn erase_fn);

/**
 * @brief Clears Flash FSM context and resets state to IDLE.
 */
void Flash_ContextClear(void);

/**
 * @brief Flash FSM cyclic polling function.
 * Must be called periodically from the main superloop or an RTOS background task.
 * Performs chunk-by-chunk non-blocking writes and asynchronous erase cycles.
 */
void Flash_MainFunction(void);

/**
 * @brief Submits a chunk write request to the FSM.
 * @param payload Pointer to data bytes to write (max FLASH_FSM_CHUNK_SIZE).
 * @param length Number of bytes in payload.
 * @retval true Request accepted (state transitioned to FLASH_STATE_WRITE).
 * @retval false Rejected (busy, invalid payload, or length exceeds chunk size).
 */
bool Flash_RequestWrite(const uint8_t *payload, uint16_t length);

/**
 * @brief Sets up target memory region for multi-chunk write operations.
 * @param memory_addr Start address in Flash.
 * @param memory_size Total expected transfer size in bytes.
 */
void Flash_SetWriteInfo(uint32_t memory_addr, uint32_t memory_size);

/**
 * @brief Submits a sector/page erase request to the FSM.
 * @param addr Page or sector start address.
 * @retval true Request accepted (state transitioned to FLASH_STATE_ERASE).
 * @retval false Rejected (busy).
 */
bool Flash_RequestErase(uint32_t addr);

/**
 * @brief Returns the current FSM operational state.
 */
FlashState_t Flash_GetState(void);

/**
 * @brief Returns true if a write or erase operation is currently in progress.
 */
bool Flash_IsBusy(void);

/**
 * @brief Acknowledges completion or failure and returns FSM to IDLE state.
 */
void Flash_StateClear(void);

#ifdef __cplusplus
}
#endif

#endif /* UDS_FLASH_FSM_H */
