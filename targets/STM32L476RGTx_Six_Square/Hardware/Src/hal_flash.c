/*----------------------------------------------------------------------------
 * Copyright (c) <2016-2018>, <Huawei Technologies Co., Ltd>
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *---------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------
 * Notice of Export Control Law
 * ===============================================
 * Huawei LiteOS may be subject to applicable export control laws and regulations, which might
 * include those applicable to Huawei LiteOS of U.S. and the country in which you are located.
 * Import, export and usage of Huawei LiteOS in any manner by you shall be in compliance with such
 * applicable export control laws and regulations.
 *---------------------------------------------------------------------------*/

#include "stm32l4xx.h"
#include "stm32l4xx_hal_flash.h"
#include "hal_flash.h"


void hal_flash_unlock(void)
{
    HAL_FLASH_Unlock();
}

void hal_flash_lock(void)
{
    (void)HAL_FLASH_Lock();
}

/**
  * @brief  Get the page of the given address
  * @param  addr: Address of the FLASH Memory
  * @retval The page of the given address
  */
static uint32_t get_page(uint32_t addr)
{
    uint32_t page = 0;
    if (addr < (FLASH_BASE + FLASH_BANK_SIZE))
    {
        /* Bank 1 */
        page = (addr - FLASH_BASE) / FLASH_PAGE_SIZE;
    }
    else
    {
        /* Bank 2 */
        page = (addr - (FLASH_BASE + FLASH_BANK_SIZE)) / FLASH_PAGE_SIZE;
    }
    return page;
}

/**
  * @brief  Get the bank of the given address
  * @param  addr: Address of the FLASH Memory
  * @retval The bank of the given address
  */
static uint32_t get_bank(uint32_t addr)
{
    uint32_t bank = 0;
    if (READ_BIT(SYSCFG->MEMRMP, SYSCFG_MEMRMP_FB_MODE) == 0)
    {
        /* No Bank swap */
        if (addr < (FLASH_BASE + FLASH_BANK_SIZE))
        {
            bank = FLASH_BANK_1;
        }
        else
        {
            bank = FLASH_BANK_2;
        }
    }
    else
    {
        /* Bank swap */
        if (addr < (FLASH_BASE + FLASH_BANK_SIZE))
        {
            bank = FLASH_BANK_2;
        }
        else
        {
            bank = FLASH_BANK_1;
        }
    }
    return bank;
}

int hal_flash_erase(uint32_t addr, int32_t len)
{
    uint32_t page_err = 0;
    HAL_StatusTypeDef status;
    FLASH_EraseInitTypeDef erase;

    HAL_FLASH_Unlock();

    /* Clear OPTVERR bit set on virgin samples */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

    uint32_t bank_begin = get_bank(addr);
    uint32_t bank_end = get_bank(addr + len - 1);
    uint32_t page_begin = get_page(addr);
    uint32_t page_end = get_page(addr + len - 1);

    if (bank_begin == bank_end)
    {
        erase.Banks = bank_begin;
        erase.TypeErase = FLASH_TYPEERASE_PAGES;
        erase.Page = page_begin;
        erase.NbPages = page_end - page_begin + 1;
    }
    else
    {
        erase.Banks = bank_begin;
        erase.TypeErase = FLASH_TYPEERASE_PAGES;
        erase.Page = page_begin;
        erase.NbPages = 255 - page_begin + 1;

        status = HAL_FLASHEx_Erase(&erase, &page_err);
        if (status != HAL_OK)
        {
            return -1;
        }

        erase.Banks = bank_end;
        erase.Page = 0;
        erase.NbPages = page_end + 1;
    }

    status = HAL_FLASHEx_Erase(&erase, &page_err);
    if (status != HAL_OK)
    {
        return -1;
    }

    return 0;
}

/**
  * @brief  Writes Data into Memory.
  * @param  buf: Pointer to the source buffer. Address to be written to.
  * @param  location: Pointer to the destination buffer.
  * @param  len: Number of data to be written (in bytes).
  * @retval 0 if operation is successeful, -1 else.
  */
int hal_flash_write(const uint8_t *buf, int32_t len, uint32_t *location)
{
    int32_t i = 0;

    HAL_FLASH_Unlock();

    /* Clear OPTVERR bit set on virgin samples */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

    for (i = 0; i < len; i += 8)
    {
        /* Device voltage range supposed to be [2.7V to 3.6V], the operation will
           be done by byte */
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, (uint32_t)(*location + i), *(uint64_t *)(buf + i)) == HAL_OK)
        {
            /* Check the written value */
            if (*(uint64_t *)(buf + i) != *(uint64_t *)(*location + i))
            {
                /* Flash content doesn't match SRAM content */
                return -1;
            }
        }
        else
        {
            /* Error occurred while writing data in Flash memory */
            return -1;
        }
    }

    return 0;
}

int hal_flash_erase_write(const uint8_t *buf, int32_t len, uint32_t location)
{
    if (NULL == buf)
    {
        return -1;
    }

    HAL_FLASH_Unlock();

    if (hal_flash_erase(location, len) != 0)
    {
        (void)HAL_FLASH_Lock();
        return -1;
    }

    if (hal_flash_write(buf, len, &location) != 0)
    {
        (void)HAL_FLASH_Lock();
        return -1;
    }

    return 0;
}

/**
  * @brief  Reads Data into Memory.
  * @param  buf: Pointer to the source buffer. Address to be written to.
  * @param  location: Pointer to the destination buffer.
  * @param  len: Number of data to be read (in bytes).
  * @retval return 0
  */
int hal_flash_read(uint8_t *buf, int32_t len, uint32_t location)
{
    HAL_FLASH_Unlock();

    int32_t i;
    for (i = 0; i < len; i++)
    {
        buf[i] = *(__IO uint8_t *)(location + i);
    }

    /* Return a valid address to avoid HardFault */
    return 0;
}

