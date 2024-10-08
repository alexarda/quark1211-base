/** @file

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

  PlatformLeds.c

Abstract:

  Platform helper LED routines.

--*/

#include <PiDxe.h>

#include "CommonHeader.h"

//
// Routines defined in other source modules of this component.
//

//
// Routines local to this source module.
//

VOID
GalileoGen2RouteOutFlashUpdateLed (
  VOID
  )
{
  //
  // For GpioNums below values 0 to 7 are for Port0 ie P0-0 - P0-7 and
  // values 8 to 15 are for Port1 ie P1-0 - P1-7.
  //

  //
  // Disable Pull-ups / pull downs on EXP0 pin for LVL_B_PU7 signal.
  //
  PlatformPcal9555GpioDisablePull (
    GALILEO_GEN2_IOEXP0_7BIT_SLAVE_ADDR,  // IO Expander 0.
    15                                   // P1-7.
    );

  //
  // Make LVL_B_OE7_N an output pin.
  //
  PlatformPcal9555GpioSetDir (
    GALILEO_GEN2_IOEXP0_7BIT_SLAVE_ADDR,  // IO Expander 0.
    14,                                   // P1-6.
    FALSE
    );

  //
  // Set level of LVL_B_OE7_N to low.
  //
  PlatformPcal9555GpioSetLevel (
    GALILEO_GEN2_IOEXP0_7BIT_SLAVE_ADDR,
    14,
    FALSE
    );

  //
  // Make MUX8_SEL an output pin.
  //
  PlatformPcal9555GpioSetDir (
    GALILEO_GEN2_IOEXP1_7BIT_SLAVE_ADDR,  // IO Expander 1.
    14,                                   // P1-6.
    FALSE
    );

  //
  // Set level of MUX8_SEL to low to route GPIO_SUS<5> to LED.
  //
  PlatformPcal9555GpioSetLevel (
    GALILEO_GEN2_IOEXP1_7BIT_SLAVE_ADDR,  // IO Expander 1.
    14,                                   // P1-6.
    FALSE
    );
}

//
// Routines exported by this source module.
//

/**
  Init platform LEDs into known state.

  @param   PlatformType     Executing platform type.
  @param   I2cBus           Pointer to I2c Host controller protocol.

  @retval  EFI_SUCCESS      Operation success.

**/
EFI_STATUS
EFIAPI
PlatformLedInit (
  IN CONST EFI_PLATFORM_TYPE              Type
  )
{
  EFI_BOOT_MODE             BootMode;

  BootMode = GetBootModeHob ();

  //
  // Init Flash update / recovery LED in OFF state.
  //
  if (BootMode == BOOT_ON_FLASH_UPDATE || BootMode == BOOT_IN_RECOVERY_MODE) {
    if (Type == GalileoGen2) {
      PlatformLegacyGpioSetLevel (R_QNC_GPIO_RGLVL_RESUME_WELL, GALILEO_GEN2_FLASH_UPDATE_LED_RESUMEWELL_GPIO, FALSE);
      GalileoGen2RouteOutFlashUpdateLed ();
    } else if (Type == Galileo) {
      PlatformLegacyGpioSetLevel (R_QNC_GPIO_RGLVL_RESUME_WELL, GALILEO_FLASH_UPDATE_LED_RESUMEWELL_GPIO, FALSE);
    } else {
      //
      // These platforms have no flash update LED.
      //
    }
  }

  return EFI_SUCCESS;
}

/**
  Turn on or off platform flash update LED.

  @param   PlatformType     Executing platform type.
  @param   TurnOn           If TRUE turn on else turn off.

  @retval  EFI_SUCCESS      Operation success.

**/
EFI_STATUS
EFIAPI
PlatformFlashUpdateLed (
  IN CONST EFI_PLATFORM_TYPE              Type,
  IN CONST BOOLEAN                        TurnOn
  )
{
  if (Type == GalileoGen2) {
    PlatformLegacyGpioSetLevel (R_QNC_GPIO_RGLVL_RESUME_WELL, GALILEO_GEN2_FLASH_UPDATE_LED_RESUMEWELL_GPIO, TurnOn);
  } else if (Type == Galileo) {
    PlatformLegacyGpioSetLevel (R_QNC_GPIO_RGLVL_RESUME_WELL, GALILEO_FLASH_UPDATE_LED_RESUMEWELL_GPIO, TurnOn);
  } else {
    //
    // These platforms have no flash update LED.
    //
  }

  return EFI_SUCCESS;
}
