/* ---------------------------------------------------------------------------- */
/*                  Atmel Microcontroller Software Support                      */
/*                       SAM Software Package License                           */
/* ---------------------------------------------------------------------------- */
/* Copyright (c) 2015, Atmel Corporation                                        */
/*                                                                              */
/* All rights reserved.                                                         */
/*                                                                              */
/* Redistribution and use in source and binary forms, with or without           */
/* modification, are permitted provided that the following condition is met:    */
/*                                                                              */
/* - Redistributions of source code must retain the above copyright notice,     */
/* this list of conditions and the disclaimer below.                            */
/*                                                                              */
/* Atmel's name may not be used to endorse or promote products derived from     */
/* this software without specific prior written permission.                     */
/*                                                                              */
/* DISCLAIMER:  THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR   */
/* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE   */
/* DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,      */
/* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT */
/* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,  */
/* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    */
/* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING         */
/* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, */
/* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                           */
/* ---------------------------------------------------------------------------- */
#ifndef __linux__
/** \addtogroup flashd_module Flash Memory Interface
 * The flash driver manages the programming, erasing, locking and unlocking
 * sequences with dedicated commands.
 *
 * To implement flash programming operation, the user has to follow these few
 * steps :
 * <ul>
 * <li>Configure flash wait states to initializes the flash. </li>
 * <li>Checks whether a region to be programmed is locked. </li>
 * <li>Unlocks the user region to be programmed if the region have locked
 * before.</li>
 * <li>Erases the user page before program (optional).</li>
 * <li>Writes the user page from the page buffer.</li>
 * <li>Locks the region of programmed area if any.</li>
 * </ul>
 *
 * Writing 8-bit and 16-bit data is not allowed and may lead to unpredictable
 * data corruption.
 * A check of this validity and padding for 32-bit alignment should be done in
 * write algorithm.
 * Lock/unlock range associated with the user address range is automatically
 * translated.
 *
 * This security bit can be enabled through the command "Set General Purpose
 * NVM Bit 0".
 *
 * A 128-bit factory programmed unique ID could be read to serve several
 * purposes.
 *
 * The driver accesses the flash memory by calling the lowlevel module provided
 * in \ref efc_module.
 * For more accurate information, please look at the EEFC section of the
 * Datasheet.
 *
 * Related files :\n
 * \ref flashd.c\n
 * \ref flashd.h.\n
 * \ref efc.c\n
 * \ref efc.h.\n
 */
/*@{*/
/*@}*/


/**
 * \file
 *
 * The flash driver provides the unified interface for flash program operations.
 *
 */

/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <Scb/Scb.h>

#include "efc.h"
#include "flash.h"

/*----------------------------------------------------------------------------
 *        Definitions
 *----------------------------------------------------------------------------*/

#define GPNVM_NUM_MAX    9
#define min(a, b) (((a) < (b)) ? (a) : (b))

/*----------------------------------------------------------------------------
 *        Local variables
 *----------------------------------------------------------------------------*/

static uint32_t _pdwPageBuffer[IFLASH_MAX_PAGE_SIZE / sizeof(uint32_t)];
static uint32_t _dwUseIAP = 0; /* Use IAP interface by default. */


/*----------------------------------------------------------------------------
 *        Local functions
 *----------------------------------------------------------------------------*/


/**
 * \brief Computes the lock range associated with the given address range.
 *
 * \param dwStart  Start address of lock range.
 * \param dwEnd  End address of lock range.
 * \param pdwActualStart  Actual start address of lock range.
 * \param pdwActualEnd  Actual end address of lock range.
 */
static void ComputeLockRange(FlashAccess* flashAccess, uint32_t dwStart, uint32_t dwEnd,
							  uint32_t *pdwActualStart, uint32_t *pdwActualEnd)
{
	uint16_t wStartPage;
	uint16_t wEndPage;
	uint16_t wNumPagesInRegion;
	uint16_t wActualStartPage;
	uint16_t wActualEndPage;

	/* Convert start and end address in page numbers */
    EFC_TranslateAddress(flashAccess, dwStart, &wStartPage, 0);
    EFC_TranslateAddress(flashAccess, dwEnd, &wEndPage, 0);

	/* Find out the first page of the first region to lock */
    wNumPagesInRegion = flashAccess->lock_region_size / flashAccess->page_size;
	wActualStartPage = wStartPage - (wStartPage % wNumPagesInRegion);
	wActualEndPage = wEndPage;

	if ((wEndPage % wNumPagesInRegion) != 0)
		wActualEndPage += wNumPagesInRegion - (wEndPage % wNumPagesInRegion);

	/* Store actual page numbers */
    EFC_ComputeAddress(flashAccess, wActualStartPage, 0, pdwActualStart);
    EFC_ComputeAddress(flashAccess, wActualEndPage, 0, pdwActualEnd);
    //TRACE_DEBUG("Actual lock range is 0x%06X - 0x%06X\n\r",
    //			 (unsigned int)*pdwActualStart, (unsigned int)*pdwActualEnd);
}


/*----------------------------------------------------------------------------
 *        Exported functions
 *----------------------------------------------------------------------------*/

/**
 * \brief Initializes the flash driver.
 *
 * \param dwMCk     Master clock frequency in Hz.
 * \param dwUseIAP  0: use EEFC controller interface, 1: use IAP interface.
 *                  dwUseIAP should be set to 1 when running out of flash.
 */

extern void FLASHD_Initialize(FlashAccess* flashAccess, uint32_t dwMCk, uint32_t dwUseIAP)
{
	dwMCk = dwMCk; /* avoid warnings */

    EFC_DisableFrdyIt(flashAccess->efc);
	_dwUseIAP = dwUseIAP;
}

/**
 * \brief Erases the entire flash.
 *
 * \param dwAddress  Flash start address.
 * \return 0 if successful; otherwise returns an error code.
 */
extern uint32_t FLASHD_Erase(FlashAccess* flashAccess, uint32_t dwAddress)
{
	uint16_t wPage;
	uint16_t wOffset;
	uint32_t dwError;

    assert((dwAddress >= flashAccess->address)
            || (dwAddress <= (flashAccess->address + flashAccess->size)));

	/* Translate write address */
    EFC_TranslateAddress(flashAccess, dwAddress, &wPage, &wOffset);
    dwError = EFC_PerformCommand(flashAccess->efc, EFC_FCMD_EA, 0, _dwUseIAP);

	return dwError;
}

/**
 * \brief Erases flash by sector.
 *
 * \param dwAddress  Start address of be erased sector.
 *
 * \return 0 if successful; otherwise returns an error code.
 */
extern uint32_t FLASHD_EraseSector(FlashAccess* flashAccess, uint32_t dwAddress)
{
	uint16_t wPage;
	uint16_t wOffset;
	uint32_t dwError;

    assert((dwAddress >= flashAccess->address)
            || (dwAddress <= (flashAccess->address + flashAccess->size)));

	/* Translate write address */
    EFC_TranslateAddress(flashAccess, dwAddress, &wPage, &wOffset);
    dwError = EFC_PerformCommand(flashAccess->efc, EFC_FCMD_ES, wPage, _dwUseIAP);

	return dwError;
}

/**
 * \brief Erases flash by pages.
 *
 * \param dwAddress  Start address of be erased pages.
 * \param dwPageNum  Number of pages to be erased with EPA command (4, 8, 16, 32)
 *
 * \return 0 if successful; otherwise returns an error code.
 */
extern uint32_t FLASHD_ErasePages(FlashAccess* flashAccess, uint32_t dwAddress, uint32_t dwPageNum)
{
	uint16_t wPage;
	uint16_t wOffset;
	uint32_t dwError;
	static uint32_t dwFarg;

    assert((dwAddress >= flashAccess->address)
            || (dwAddress <= (flashAccess->address + flashAccess->size)));

	/* Translate write address */
    EFC_TranslateAddress(flashAccess, dwAddress, &wPage, &wOffset);

	/* Get FARG field for EPA command:
	 * The first page to be erased is specified in the FARG[15:2] field of
	 * the MC_FCR register. The first page number must be modulo 4, 8,16 or 32
	 * according to the number of pages to erase at the same time.
	 *
	 * The 2 lowest bits of the FARG field define the number of pages to
	 * be erased (FARG[1:0]).
	 */
	if (dwPageNum == 32) {
		wPage &= ~(32u - 1u);
		dwFarg = (wPage) | 3; /* 32 pages */
	} else if (dwPageNum == 16) {
		wPage &= ~(16u - 1u);
		dwFarg = (wPage) | 2; /* 16 pages */
	} else if (dwPageNum == 8) {
		wPage &= ~(8u - 1u);
		dwFarg = (wPage) | 1; /* 8 pages */
	} else {
		wPage &= ~(4u - 1u);
		dwFarg = (wPage) | 0; /* 4 pages */
	}

    dwError = EFC_PerformCommand(flashAccess->efc, EFC_FCMD_EPA, dwFarg, _dwUseIAP);

	return dwError;
}


/**
 * \brief Writes a data buffer in the internal flash
 *
 * \note This function works in polling mode, and thus only returns when the
 * data has been effectively written.
 * \param address  Write address.
 * \param pBuffer  Data buffer.
 * \param size  Size of data buffer in bytes.
 * \return 0 if successful, otherwise returns an error code.
 */
extern uint32_t FLASHD_Write(FlashAccess* flashAccess, uint32_t dwAddress,
							  const void *pvBuffer, uint32_t dwSize)
{
	uint16_t page;
	uint16_t offset;
	uint32_t writeSize;
	uint32_t pageAddress;
	uint16_t padding;
	uint32_t dwError;
	uint32_t dwIdx;
	uint32_t *pAlignedDestination;
	uint8_t  *pucPageBuffer = (uint8_t *)_pdwPageBuffer;

	assert(pvBuffer);
    assert(dwAddress >= flashAccess->address);
    assert((dwAddress + dwSize) <= (flashAccess->address + flashAccess->size));

	/* Translate write address */
    EFC_TranslateAddress(flashAccess, dwAddress, &page, &offset);

	/* Write all pages */
	while (dwSize > 0) {
		/* Copy data in temporary buffer to avoid alignment problems */
        writeSize = min((uint32_t)flashAccess->page_size - offset, dwSize);
        EFC_ComputeAddress(flashAccess, page, 0, &pageAddress);
        padding = flashAccess->page_size - offset - writeSize;

		/* Pre-buffer data */
		memcpy(pucPageBuffer, (void *) pageAddress, offset);

		/* Buffer data */
		memcpy(pucPageBuffer + offset, pvBuffer, writeSize);

		/* Post-buffer data */
		memcpy(pucPageBuffer + offset + writeSize,
				(void *) (pageAddress + offset + writeSize), padding);

		/* Write page
		 * Writing 8-bit and 16-bit data is not allowed and may
		    lead to unpredictable data corruption
		 */
		pAlignedDestination = (uint32_t *)pageAddress;

        for (dwIdx = 0; dwIdx < (flashAccess->page_size / sizeof(uint32_t)); ++ dwIdx) {
			*pAlignedDestination++ = _pdwPageBuffer[dwIdx];
            asm volatile ("dmb 0xF":::"memory");
		}

		/* Cache coherence operation before flash write*/
        Scb_cleanDCacheByAddr((uint32_t *)pageAddress, flashAccess->page_size);

		/* Note: It is not possible to use Erase and write Command (EWP) on all Flash
		(this command is available on the First 2 Small Sector, 16K Bytes).
		For the next block, Erase them first then use Write page command. */

		/* Send writing command */
        dwError = EFC_PerformCommand(flashAccess->efc, EFC_FCMD_WP, page, _dwUseIAP);

		if (dwError)
			return dwError;

		/* Progression */
		pvBuffer = (void *)((uint32_t) pvBuffer + writeSize);
		dwSize -= writeSize;
		page++;
		offset = 0;
	}

	return 0;
}

/**
 * \brief Locks all the regions in the given address range. The actual lock
 * range is reported through two output parameters.
 *
 * \param start  Start address of lock range.
 * \param end    End address of lock range.
 * \param pActualStart  Start address of the actual lock range (optional).
 * \param pActualEnd  End address of the actual lock range (optional).
 * \return 0 if successful, otherwise returns an error code.
 */
extern uint32_t FLASHD_Lock(FlashAccess* flashAccess, uint32_t start, uint32_t end,
							 uint32_t *pActualStart, uint32_t *pActualEnd)
{
	uint32_t actualStart, actualEnd;
	uint16_t startPage, endPage;
	uint32_t dwError;
    uint16_t numPagesInRegion = flashAccess->lock_region_size / flashAccess->page_size;

	/* Compute actual lock range and store it */
    ComputeLockRange(flashAccess, start, end, &actualStart, &actualEnd);

	if (pActualStart != NULL)
		*pActualStart = actualStart;

	if (pActualEnd != NULL)
		*pActualEnd = actualEnd;

	/* Compute page numbers */
    EFC_TranslateAddress(flashAccess, actualStart, &startPage, 0);
    EFC_TranslateAddress(flashAccess, actualEnd, &endPage, 0);

	/* Lock all pages */
	while (startPage < endPage) {
        dwError = EFC_PerformCommand(flashAccess->efc, EFC_FCMD_SLB, startPage, _dwUseIAP);

		if (dwError)
			return dwError;

		startPage += numPagesInRegion;
	}

	return 0;
}

/**
 * \brief Unlocks all the regions in the given address range. The actual unlock
 * range is reported through two output parameters.
 * \param start  Start address of unlock range.
 * \param end  End address of unlock range.
 * \param pActualStart  Start address of the actual unlock range (optional).
 * \param pActualEnd  End address of the actual unlock range (optional).
 * \return 0 if successful, otherwise returns an error code.
 */
extern uint32_t FLASHD_Unlock(FlashAccess* flashAccess, uint32_t start, uint32_t end,
							   uint32_t *pActualStart, uint32_t *pActualEnd)
{
	uint32_t actualStart, actualEnd;
	uint16_t startPage, endPage;
	uint32_t dwError;
    uint16_t numPagesInRegion = flashAccess->lock_region_size / flashAccess->page_size;

	/* Compute actual unlock range and store it */
    ComputeLockRange(flashAccess, start, end, &actualStart, &actualEnd);

	if (pActualStart != NULL)
		*pActualStart = actualStart;

	if (pActualEnd != NULL)
		*pActualEnd = actualEnd;

	/* Compute page numbers */
    EFC_TranslateAddress(flashAccess, actualStart, &startPage, 0);
    EFC_TranslateAddress(flashAccess, actualEnd, &endPage, 0);

	/* Unlock all pages */
	while (startPage < endPage) {
        dwError = EFC_PerformCommand(flashAccess->efc, EFC_FCMD_CLB, startPage, _dwUseIAP);

		if (dwError)
			return dwError;

		startPage += numPagesInRegion;
	}

	return 0;
}

/**
 * \brief Returns the number of locked regions inside the given address range.
 *
 * \param start  Start address of range
 * \param end    End address of range.
 */
extern uint32_t FLASHD_IsLocked(FlashAccess* flashAccess, uint32_t start, uint32_t end)
{
	uint32_t i, j;
	uint16_t startPage, endPage;
	uint8_t startRegion, endRegion;
	uint32_t numPagesInRegion;
    uint32_t status[flashAccess->lock_region_size / 32u];
	uint32_t numLockedRegions = 0;

	assert(end >= start);
    assert((start >= flashAccess->address) && (end <= flashAccess->address + flashAccess->size));

	/* Compute page numbers */
    EFC_TranslateAddress(flashAccess, start, &startPage, 0);
    EFC_TranslateAddress(flashAccess, end, &endPage, 0);

	/* Compute region numbers */
    numPagesInRegion = flashAccess->lock_region_size / flashAccess->page_size;
	startRegion = startPage / numPagesInRegion;
	endRegion = endPage / numPagesInRegion;

	if ((endPage % numPagesInRegion) != 0)
		endRegion++;

	/* Retrieve lock status */
    EFC_PerformCommand(flashAccess->efc, EFC_FCMD_GLB, 0, _dwUseIAP);

    for (i = 0; i < (flashAccess->lock_bits / 32u); i++)
        status[i] = EFC_GetResult(flashAccess->efc);

	/* Check status of each involved region */
	while (startRegion < endRegion) {
		i = startRegion / 32u;
		j = startRegion % 32u;

		if ((status[i] & (1 << j)) != 0)
			numLockedRegions++;

		startRegion++;
	}

	return numLockedRegions;
}

/**
 * \brief Check if the given GPNVM bit is set or not.
 *
 * \param gpnvm  GPNVM bit index.
 * \returns 1 if the given GPNVM bit is currently set; otherwise returns 0.
 */
extern uint32_t FLASHD_IsGPNVMSet(FlashAccess* flashAccess, uint8_t ucGPNVM)
{
	uint32_t dwStatus;

	assert(ucGPNVM < GPNVM_NUM_MAX);

	/* Get GPNVMs status */
    EFC_PerformCommand(flashAccess->efc, EFC_FCMD_GFB, 0, _dwUseIAP);
    dwStatus = EFC_GetResult(flashAccess->efc);

	/* Check if GPNVM is set */
	if ((dwStatus & (1 << ucGPNVM)) != 0)
		return 1;
	else
		return 0;
}

/**
 * \brief Sets the selected GPNVM bit.
 *
 * \param gpnvm  GPNVM bit index.
 * \returns 0 if successful; otherwise returns an error code.
 */
extern uint32_t FLASHD_SetGPNVM(FlashAccess* flashAccess, uint8_t ucGPNVM)
{
	assert(ucGPNVM < GPNVM_NUM_MAX);

    if (!FLASHD_IsGPNVMSet(flashAccess, ucGPNVM))
        return EFC_PerformCommand(flashAccess->efc, EFC_FCMD_SFB, ucGPNVM, _dwUseIAP);
	else
		return 0;
}

/**
 * \brief Clears the selected GPNVM bit.
 *
 * \param gpnvm  GPNVM bit index.
 * \returns 0 if successful; otherwise returns an error code.
 */
extern uint32_t FLASHD_ClearGPNVM(FlashAccess* flashAccess, uint8_t ucGPNVM)
{
	assert(ucGPNVM < GPNVM_NUM_MAX);

    if (FLASHD_IsGPNVMSet(flashAccess, ucGPNVM))
        return EFC_PerformCommand(flashAccess->efc, EFC_FCMD_CFB, ucGPNVM, _dwUseIAP);
	else
		return 0;
}

/**
 * \brief Read the unique ID.
 *
 * \param pdwUniqueID pointer on a 4bytes char containing the unique ID value.
 * \returns 0 if successful; otherwise returns an error code.
 */
#ifdef __ICCARM__
	extern __ramfunc uint32_t FLASHD_ReadUniqueID(uint32_t *pdwUniqueID)
#else
	__attribute__ ((section (".ramfunc")))
    uint32_t FLASHD_ReadUniqueID(FlashAccess* flashAccess, uint32_t *pdwUniqueID)
#endif
{
	uint32_t status;

	if (pdwUniqueID == NULL)
		return 1;

	pdwUniqueID[0] = 0;
	pdwUniqueID[1] = 0;
	pdwUniqueID[2] = 0;
	pdwUniqueID[3] = 0;

	/* Send the Start Read unique Identifier command (STUI) by writing the Flash
	   Command Register with the STUI command.*/
    flashAccess->efc->EEFC_FCR = EEFC_FCR_FKEY_PASSWD | EFC_FCMD_STUI;

	/* When the Unique Identifier is ready to be read, the FRDY bit in the Flash
	   Programming Status Register (EEFC_FSR) falls. */
	do {
        status = flashAccess->efc->EEFC_FSR;
	} while ((status & EEFC_FSR_FRDY) == EEFC_FSR_FRDY);

	/* The Unique Identifier is located in the first 128 bits of the Flash
	   memory mapping. So, at the address 0x400000-0x40000F. */
    pdwUniqueID[0] = *(uint32_t *)flashAccess->address;
    pdwUniqueID[1] = *(uint32_t *)(flashAccess->address + 4);
    pdwUniqueID[2] = *(uint32_t *)(flashAccess->address + 8);
    pdwUniqueID[3] = *(uint32_t *)(flashAccess->address + 12);

	/* To stop the Unique Identifier mode, the user needs to send the Stop Read
	   unique Identifier command (SPUI) by writing the Flash Command Register
	   with the SPUI command. */
    flashAccess->efc->EEFC_FCR = EEFC_FCR_FKEY_PASSWD | EFC_FCMD_SPUI;

	/* When the Stop read Unique Unique Identifier command (SPUI) has been
	performed, the FRDY bit in the Flash Programming Status Register (EEFC_FSR)
	rises. */
	do {
        status = flashAccess->efc->EEFC_FSR;
	} while ((status & EEFC_FSR_FRDY) != EEFC_FSR_FRDY);

	return 0;
}

#endif
