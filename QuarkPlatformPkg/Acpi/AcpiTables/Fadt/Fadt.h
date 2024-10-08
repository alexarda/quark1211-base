/*++

Copyright (c) 2013-2016 Intel Corporation.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.
* Neither the name of Intel Corporation nor the names of its
contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Module Name:

  Fadt.h

Abstract:

  This file describes the contents of the ACPI Fixed ACPI Description Table (FADT)
  .  Some additional ACPI values are defined in Acpi10.h, Acpi20.h, and Acpi30.h
  All changes to the FADT contents should be done in this file.

--*/

#ifndef _FADT_H_
#define _FADT_H_

//
// Statements that include other files
//
#include <IndustryStandard/Acpi.h>

//
// ACPI table information used to initialize tables.
//
#define EFI_ACPI_OEM_ID           'I','N','T','E','L',' '   // OEMID 6 bytes long
#define EFI_ACPI_OEM_TABLE_ID     SIGNATURE_64('T','I','A','N','O',' ',' ',' ') // OEM table id 8 bytes long
#define EFI_ACPI_OEM_REVISION     0x00000004
#define EFI_ACPI_CREATOR_ID       SIGNATURE_32('I','N','T','L')
#define EFI_ACPI_CREATOR_REVISION 0x0100000D  

//
// FADT Definitions
//
#define PM_PROFILE      0x01
#define INT_MODEL       0x01
#define SCI_INT_VECTOR  0x0009
#define ACPI_ENABLE     0x0a0
#define ACPI_DISABLE    0x0a1
#define S4BIOS_REQ      0x00
#define PM1_EVT_LEN     0x04
#define PM1_CNT_LEN     0x02
#define PM2_CNT_LEN     0x00
#define PM_TM_LEN       0x04
#define GPE0_BLK_LEN    0x08
#define GPE1_BLK_LEN    0x00
#define GPE1_BASE       0x00
#define RESERVED        0x00
#define P_LVL2_LAT      0x0065
#define P_LVL3_LAT      0x03e9
#define FLUSH_SIZE      0x0400
#define FLUSH_STRIDE    0x0010
#define DUTY_OFFSET     0x01
#define DUTY_WIDTH      0x03
#define DAY_ALRM        0x00
#define MON_ALRM        0x00
#define CENTURY         0x00
#define IAPC_BOOT_ARCH  EFI_ACPI_2_0_LEGACY_DEVICES
#define FLAG            (EFI_ACPI_1_0_WBINVD | EFI_ACPI_1_0_PROC_C1 | EFI_ACPI_1_0_SLP_BUTTON | EFI_ACPI_1_0_RTC_S4)
#define FLAG2           (EFI_ACPI_2_0_WBINVD | EFI_ACPI_2_0_PROC_C1 | EFI_ACPI_2_0_PWR_BUTTON | EFI_ACPI_2_0_SLP_BUTTON | EFI_ACPI_2_0_RTC_S4 | EFI_ACPI_2_0_RESET_REG_SUP | EFI_ACPI_3_0_USE_PLATFORM_CLOCK)

#define RESET_REG_ADDRESS_SPACE_ID     EFI_ACPI_2_0_SYSTEM_IO
#define RESET_REG_BIT_WIDTH            0x08
#define RESET_REG_BIT_OFFSET           0x00
#define RESET_REG_ADDRESS              0x0000000000000CF9
#define RESET_VALUE                    0x02

#define ACPI_RUNTIME_UPDATE            0x00

#define PM1a_EVT_BLK_ADDRESS_SPACE_ID  EFI_ACPI_2_0_SYSTEM_IO
#define PM1a_EVT_BLK_BIT_WIDTH         0x20
#define PM1a_EVT_BLK_BIT_OFFSET        0x00

#define PM1b_EVT_BLK_ADDRESS_SPACE_ID  EFI_ACPI_2_0_SYSTEM_IO
#define PM1b_EVT_BLK_BIT_WIDTH         0x00
#define PM1b_EVT_BLK_BIT_OFFSET        0x00
#define PM1b_EVT_BLK_ADDRESS           0x0000000000000000

#define PM1a_CNT_BLK_ADDRESS_SPACE_ID  EFI_ACPI_2_0_SYSTEM_IO
#define PM1a_CNT_BLK_BIT_WIDTH         0x10
#define PM1a_CNT_BLK_BIT_OFFSET        0x00

#define PM1b_CNT_BLK_ADDRESS_SPACE_ID  EFI_ACPI_2_0_SYSTEM_IO
#define PM1b_CNT_BLK_BIT_WIDTH         0x00
#define PM1b_CNT_BLK_BIT_OFFSET        0x00
#define PM1b_CNT_BLK_ADDRESS           0x0000000000000000

#define PM2_CNT_BLK_ADDRESS_SPACE_ID   EFI_ACPI_2_0_SYSTEM_IO
#define PM2_CNT_BLK_BIT_WIDTH          0x00
#define PM2_CNT_BLK_BIT_OFFSET         0x00
#define PM2_CNT_BLK_ADDRESS            0x0000000000000000

#define PM_TMR_BLK_ADDRESS_SPACE_ID    EFI_ACPI_2_0_SYSTEM_IO
#define PM_TMR_BLK_BIT_WIDTH           0x20
#define PM_TMR_BLK_BIT_OFFSET          0x00

#define GPE0_BLK_ADDRESS_SPACE_ID      EFI_ACPI_2_0_SYSTEM_IO
#define GPE0_BLK_BIT_WIDTH             0x40
#define GPE0_BLK_BIT_OFFSET            0x00

#define GPE1_BLK_ADDRESS_SPACE_ID      EFI_ACPI_2_0_SYSTEM_IO
#define GPE1_BLK_BIT_WIDTH             0x00
#define GPE1_BLK_BIT_OFFSET            0x00
#define GPE1_BLK_ADDRESS               0x0000000000000000
#endif
