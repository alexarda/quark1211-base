/** @file
  This PEIM initialize platform for MRC, following action is performed,
    1. Initizluize GMCH
    2. Detect boot mode
    3. Detect video adapter to determine whether we need pre allocated memory 
    4. Calls MRC to initialize memory and install a PPI notify to do post memory initialization.
  This file contains the main entrypoint of the PEIM.
  
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

**/

#include "CommonHeader.h"
#include "PlatformEarlyInit.h"
#include "PeiFvSecurity.h"
#include <Library/I2cLib.h>

#if !defined(__GNUC__)
//
// If not GCC build we have space to trace MFH items.
//
#define TRACE_MFH_ITEMS
#else
#endif

EFI_STATUS
EFIAPI
EndOfPeiSignalPpiNotifyCallback (
  IN EFI_PEI_SERVICES           **PeiServices,
  IN EFI_PEI_NOTIFY_DESCRIPTOR  *NotifyDescriptor,
  IN VOID                       *Ppi
  );

//
// Function prototypes to routines implemented in other source modules
// within this component.
//

EFI_STATUS
EFIAPI
PlatformErratasPostMrc (
  VOID
  );

//
// The global indicator, the FvFileLoader callback will modify it to TRUE after loading PEIM into memory
//
BOOLEAN ImageInMemory = FALSE;

BOARD_LEGACY_GPIO_CONFIG      mBoardLegacyGpioConfigTable[]  = { PLATFORM_LEGACY_GPIO_TABLE_DEFINITION };
UINTN                         mBoardLegacyGpioConfigTableLen = (sizeof(mBoardLegacyGpioConfigTable) / sizeof(BOARD_LEGACY_GPIO_CONFIG));
BOARD_GPIO_CONTROLLER_CONFIG  mBoardGpioControllerConfigTable[]  = { PLATFORM_GPIO_CONTROLLER_CONFIG_DEFINITION };
UINTN                         mBoardGpioControllerConfigTableLen = (sizeof(mBoardGpioControllerConfigTable) / sizeof(BOARD_GPIO_CONTROLLER_CONFIG));
UINT8                         ChipsetDefaultMac [6] = {0xff,0xff,0xff,0xff,0xff,0xff};

STATIC EFI_PEI_PPI_DESCRIPTOR mPpiBootMode[1] = {
  {
    (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gEfiPeiMasterBootModePpiGuid,
    NULL
  }
};

EFI_PEI_NOTIFY_DESCRIPTOR mMemoryDiscoveredNotifyList[1] = {
  { 
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gEfiPeiMemoryDiscoveredPpiGuid,
    MemoryDiscoveredPpiNotifyCallback 
  }
};

EFI_PEI_NOTIFY_DESCRIPTOR mEndOfPeiSignalPpiNotifyList[1] = {
  { 
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gEfiEndOfPeiSignalPpiGuid,
    EndOfPeiSignalPpiNotifyCallback
  }
};

EFI_PEI_STALL_PPI mStallPpi = {
  PEI_STALL_RESOLUTION,
  Stall
};

EFI_PEI_PPI_DESCRIPTOR mPpiStall[1] = {
  {
    (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gEfiPeiStallPpiGuid,
    &mStallPpi
  }
};

/**
  Set Mac address on chipset ethernet device.

  @param  Bus      PCI Bus number of chipset ethernet device.
  @param  Device   Device number of chipset ethernet device.
  @param  Func     PCI Function number of chipset ethernet device.
  @param  MacAddr  MAC Address to set.

**/
VOID
EFIAPI
SetLanControllerMacAddr (
  IN CONST UINT8                          Bus,
  IN CONST UINT8                          Device,
  IN CONST UINT8                          Func,
  IN CONST UINT8                          *MacAddr,
  IN CONST UINT32                         Bar0
  )
{
  UINT32                            Data32;
  UINT16                            PciVid;
  UINT16                            PciDid;
  UINT32                            Addr;
  UINT32                            MacVer;
  volatile UINT8                    *Wrote;
  UINT32                            DevPcieAddr;
  UINT16                            SaveCmdReg;
  UINT32                            SaveBarReg;

  DevPcieAddr = PCI_LIB_ADDRESS (
                  Bus,
                  Device,
                  Func,
                  0
                  );

  //
  // Do nothing if not a supported device.
  //
  PciVid = PciRead16 (DevPcieAddr + PCI_VENDOR_ID_OFFSET);
  PciDid = PciRead16 (DevPcieAddr + PCI_DEVICE_ID_OFFSET);
  if((PciVid != V_IOH_MAC_VENDOR_ID) || (PciDid != V_IOH_MAC_DEVICE_ID)) {
    return;
  }

  //
  // Save current settings for PCI CMD/BAR registers
  //
  SaveCmdReg = PciRead16 (DevPcieAddr + PCI_COMMAND_OFFSET);
  SaveBarReg = PciRead32 (DevPcieAddr + R_IOH_MAC_MEMBAR);

  //
  // Use predefined tempory memory resource
  //
  PciWrite32 ( DevPcieAddr + R_IOH_MAC_MEMBAR, Bar0);
  PciWrite8 ( DevPcieAddr + PCI_COMMAND_OFFSET, EFI_PCI_COMMAND_MEMORY_SPACE); 

  Addr =  Bar0 + R_IOH_MAC_GMAC_REG_8;
  MacVer = *((volatile UINT32 *) (UINTN)(Addr));

  DEBUG ((EFI_D_INFO, "Ioh MAC [B:%d, D:%d, F:%d] VER:%04x ADDR:",
    (UINTN) Bus,
    (UINTN) Device,
    (UINTN) Func,
    (UINTN) MacVer
    ));

  //
  // Set MAC Address0 Low Register (GMAC_REG_17) ADDRLO bits.
  //
  Addr =  Bar0 + R_IOH_MAC_GMAC_REG_17;
  Data32 = *((UINT32 *) (UINTN)(&MacAddr[0]));
  *((volatile UINT32 *) (UINTN)(Addr)) = Data32;
  Wrote = (volatile UINT8 *) (UINTN)(Addr);
  DEBUG ((EFI_D_INFO, "%02x-%02x-%02x-%02x-",
    (UINTN) Wrote[0],
    (UINTN) Wrote[1],
    (UINTN) Wrote[2],
    (UINTN) Wrote[3]
    ));

  //
  // Set MAC Address0 High Register (GMAC_REG_16) ADDRHI bits
  // and Address Enable (AE) bit.
  //
  Addr =  Bar0 + R_IOH_MAC_GMAC_REG_16;
  Data32 =
    ((UINT32) MacAddr[4]) |
    (((UINT32)MacAddr[5]) << 8) |
    B_IOH_MAC_AE;
  *((volatile UINT32 *) (UINTN)(Addr)) = Data32;
  Wrote = (volatile UINT8 *) (UINTN)(Addr);

  DEBUG ((EFI_D_INFO, "%02x-%02x\n", (UINTN) Wrote[0], (UINTN) Wrote[1]));

  //
  // Restore settings for PCI CMD/BAR registers
  //
  PciWrite32 ((DevPcieAddr + R_IOH_MAC_MEMBAR), SaveBarReg);
  PciWrite16 (DevPcieAddr + PCI_COMMAND_OFFSET, SaveCmdReg);
}

/**
  Main platform init PEI tasks.

  @param  PeiServices Describes the list of possible PEI Services.
  @param  InMemory    If == TRUE then being called when driver in memory.

  @retval EFI_SUCCESS if it completed successfully.
**/
EFI_STATUS
EFIAPI
PlatformMainInit (
  IN CONST EFI_PEI_SERVICES     **PeiServices,
  IN CONST BOOLEAN              InMemory
  )
{
  EFI_STATUS                              Status;
  EFI_BOOT_MODE                           BootMode;
  EFI_PEI_STALL_PPI                       *StallPpi;
  EFI_PEI_PPI_DESCRIPTOR                  *StallPeiPpiDescriptor;
  EFI_PLATFORM_INFO                       *PlatformInfo;
  EFI_HOB_GUID_TYPE                       *GuidHob;
  EFI_PLATFORM_TYPE                       PlatformType;

  GuidHob = GetFirstGuidHob (&gEfiPlatformInfoGuid);
  PlatformInfo  = GET_GUID_HOB_DATA (GuidHob);
  ASSERT (PlatformInfo != NULL);
  PlatformType = (EFI_PLATFORM_TYPE) PlatformInfo->Type;

  if (!InMemory) {
    //
    // Do platform specific logic to create a boot mode
    //
    Status = UpdateBootMode ((EFI_PEI_SERVICES**)PeiServices, &BootMode);
    ASSERT_EFI_ERROR (Status);
  } else {
    Status = PeiServicesGetBootMode (&BootMode);
    ASSERT_EFI_ERROR (Status);
  }

  //
  // Initialize Firmware Volume security.
  // This must be done before any firmware volume accesses (excl. BFV)
  //
  Status = PeiInitializeFvSecurity();
  ASSERT_EFI_ERROR (Status);

  //
  // Allocate an initial buffer from heap for debugger use
  //
  DEBUG_CODE (
    BpeDsAllocation ();
  );

  //
  // Do any early platform specific initialisation.
  //
  EarlyPlatformInit (PlatformInfo, BootMode);

  if (InMemory) {

    //
    // This is a second path on entry, in recovery boot path the Stall PPI
    // need to be memory-based to improve recovery performance.
    //

    //
    // Now that module in memory, update the 
    // PPI that describes the Stall to other modules
    //
    Status = PeiServicesLocatePpi (
               &gEfiPeiStallPpiGuid,
               0,
               &StallPeiPpiDescriptor,
               (VOID **) &StallPpi
               );

    if (!EFI_ERROR (Status)) {

      Status = PeiServicesReInstallPpi (
                 StallPeiPpiDescriptor,
                 &mPpiStall[0]
                 );
    } else {
      Status = PeiServicesInstallPpi (&mPpiStall[0]);
    }
    return Status;
  }

  //
  // Initialise System Phys
  //

  // Program USB Phy
  InitializeUSBPhy();

  //
  // Signal possible dependent modules that there has been a 
  // final boot mode determination
  //
  if (!EFI_ERROR(Status)) {
    Status = PeiServicesInstallPpi (&mPpiBootMode[0]);
    ASSERT_EFI_ERROR (Status);
  }

  if (BootMode != BOOT_ON_S3_RESUME) {    
    QNCClearSmiAndWake ();    
  }

  //
  // Create the platform Flash Map
  //
  Status = PeimInitializeFlashMap (PeiServices);
  ASSERT_EFI_ERROR (Status);

  Status = PeimInstallFlashMapPpi (PeiServices);
  ASSERT_EFI_ERROR (Status);

  DEBUG ((EFI_D_INFO, "MRC Entry\n"));
  MemoryInit ((EFI_PEI_SERVICES**)PeiServices);

  //
  // Do Early PCIe init.
  //
  DEBUG ((EFI_D_INFO, "Early PCIe controller initialisation\n"));
  PlatformPciExpressEarlyInit (PlatformType);


  DEBUG ((EFI_D_INFO, "Platform Erratas After MRC\n"));
  PlatformErratasPostMrc ();

  //
  // Now that all of the pre-permament memory activities have
  // been taken care of, post a call-back for the permament-memory
  // resident services, such as HOB construction.
  // PEI Core will switch stack after this PEIM exit.  After that the MTRR
  // can be set.
  //
  Status = PeiServicesNotifyPpi (&mMemoryDiscoveredNotifyList[0]);
  ASSERT_EFI_ERROR (Status);

  //
  // Platform UART0 Init.
  //
  PlatformUart0Init (PlatformType, BootMode);

  return Status;
}

EFI_STATUS
EFIAPI
EndOfPeiSignalPpiNotifyCallback (
  IN EFI_PEI_SERVICES           **PeiServices,
  IN EFI_PEI_NOTIFY_DESCRIPTOR  *NotifyDescriptor,
  IN VOID                       *Ppi
  )
{
  EFI_STATUS                            Status;

  DEBUG ((EFI_D_INFO, "End of PEI Signal Callback\n"));

    //
  // Restore the flash region to be UC
  // for both normal boot as we build a Resource Hob to 
  // describe this region as UC to DXE core.
  //
  WriteBackInvalidateDataCacheRange (
    (VOID *) (UINTN) PcdGet32 (PcdFlashAreaBaseAddress),
    PcdGet32 (PcdFlashAreaSize)
  );

  Status = MtrrSetMemoryAttribute (PcdGet32 (PcdFlashAreaBaseAddress), PcdGet32 (PcdFlashAreaSize), CacheUncacheable);
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

/**
  This function will initialize USB Phy registers associated with QuarkSouthCluster.

  @param  VOID                  No Argument

  @retval EFI_SUCCESS           All registers have been initialized
**/
VOID
EFIAPI
InitializeUSBPhy (
    VOID
   )
{
    UINT32 RegData32;

    /** In order to configure the PHY to use clk120 (ickusbcoreclk) as PLL reference clock
     *  and Port2 as a USB device port, the following sequence must be followed
     *
     **/

    // Sideband register write to USB AFE (Phy)
    RegData32 = QNCAltPortRead (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_GLOBAL_PORT);
    RegData32 &= ~(BIT1);
    //
    // PDNRESCFG [8:7] of USB2_GLOBAL_PORT = 11b.
    // For port 0 & 1 as host and port 2 as device.
    //
    RegData32 |= (BIT8 | BIT7);
    QNCAltPortWrite (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_GLOBAL_PORT, RegData32);

    //
    // Required BIOS change on Disconnect vref to change to 600mV.
    //
    RegData32 = QNCAltPortRead (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_COMPBG);
    RegData32 &= ~(BIT10 | BIT9 | BIT8 | BIT7);
    RegData32 |= (BIT10 | BIT7);
    QNCAltPortWrite (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_COMPBG, RegData32);

    // Sideband register write to USB AFE (Phy)
    // (pllbypass) to bypass/Disable PLL before switch
    RegData32 = QNCAltPortRead (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL2);
    RegData32 |= BIT29;
    QNCAltPortWrite (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL2, RegData32);

    // Sideband register write to USB AFE (Phy)
    // (coreclksel) to select 120MHz (ickusbcoreclk) clk source.
    // (Default 0 to select 96MHz (ickusbclk96_npad/ppad))
    RegData32 = QNCAltPortRead (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL1);
    RegData32 |= BIT1;
    QNCAltPortWrite (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL1, RegData32);

    // Sideband register write to USB AFE (Phy)
    // (divide by 8) to achieve internal 480MHz clock
    // for 120MHz input refclk.  (Default: 4'b1000 (divide by 10) for 96MHz)
    RegData32 = QNCAltPortRead (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL1);
    RegData32 &= ~(BIT5 | BIT4 | BIT3);
    RegData32 |= BIT6;
    QNCAltPortWrite (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL1, RegData32);

    // Sideband register write to USB AFE (Phy)
    // Clear (pllbypass)
    RegData32 = QNCAltPortRead (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL2);
    RegData32 &= ~BIT29;
    QNCAltPortWrite (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL2, RegData32);

    // Sideband register write to USB AFE (Phy)
    // Set (startlock) to force the PLL FSM to restart the lock
    // sequence due to input clock/freq switch.
    RegData32 = QNCAltPortRead (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL2);
    RegData32 |= BIT24;
    QNCAltPortWrite (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL2, RegData32);

    // At this point the PLL FSM and COMP FSM will complete

}

/**
  This function provides early platform Thermal sensor initialisation.
**/
VOID
EFIAPI
EarlyPlatformThermalSensorInit (
  VOID
  )
{
  DEBUG ((EFI_D_INFO, "Early Platform Thermal Sensor Init\n"));

  //
  // Set Thermal sensor mode.
  //
  QNCThermalSensorSetRatiometricMode ();

  //
  // Enable RMU Thermal sensor with a Catastrophic Trip point.
  //
  QNCThermalSensorEnableWithCatastrophicTrip (PLATFORM_CATASTROPHIC_TRIP_CELSIUS);

  //
  // Lock all RMU Thermal sensor control & trip point registers.
  //
  QNCThermalSensorLockAllRegisters ();
}

/**
  Trace MFH and Platform Data lists.
**/
STATIC
VOID
EFIAPI
TraceMfhItems (
  VOID
  )
{
#if defined (TRACE_MFH_ITEMS)
  MFH_LIB_FINDCONTEXT               MfhFindContext;
  MFH_FLASH_ITEM                    *FlashItem;

  DEBUG ((EFI_D_INFO, "MFH Flash Item List:\n"));
  FlashItem = MfhLibFindFirstWithFilter (
                NULL,
                MFH_FIND_ANY_FIT_FILTER,
                FALSE,
                &MfhFindContext
                );
  while (FlashItem != NULL) {
    DEBUG ((EFI_D_INFO, "\tQuark 0x%08X:0x%08X %s\n", FlashItem->FlashAddress, FlashItem->LengthBytes, MfhLibFlashItemTypePrintString(FlashItem->Type)));
    FlashItem = MfhLibFindNextWithFilter (
                  MFH_FIND_ANY_FIT_FILTER,
                  &MfhFindContext
                  );
  }
  DEBUG ((EFI_D_INFO, "MFH Boot Priority List:\n"));
  FlashItem = MfhLibFindFirstWithFilter (
                NULL,
                MFH_FIND_ALL_STAGE1_FILTER,
                TRUE,
                &MfhFindContext
                );
  while (FlashItem != NULL) {
    DEBUG ((EFI_D_INFO, "\tQuark 0x%08X:0x%08X %s\n", FlashItem->FlashAddress, FlashItem->LengthBytes, MfhLibFlashItemTypePrintString(FlashItem->Type)));
    FlashItem = MfhLibFindNextWithFilter (
                  MFH_FIND_ALL_STAGE1_FILTER,
                  &MfhFindContext
                  );
  }
#endif
}

/**
  Trace Platform Data items.
**/
STATIC
VOID
EFIAPI
TracePlatformDataItems (
  VOID
  )
{
  PDAT_ITEM                         *Item;
  PDAT_LIB_FINDCONTEXT              PDatFindContext;
  CHAR8                             Desc[PDAT_ITEM_DESC_LENGTH+1];

  DEBUG ((EFI_D_INFO, "\nPlatform Data Item List in System Area:\n"));
  Item = PDatLibFindFirstWithFilter (NULL, PDAT_FIND_ANY_ITEM_FILTER, &PDatFindContext, NULL);
  if (Item != NULL) {
    Desc[PDAT_ITEM_DESC_LENGTH] = 0;
    do {
      CopyMem (Desc, Item->Header.Description, PDAT_ITEM_DESC_LENGTH);
      DEBUG ((EFI_D_INFO, "\tQuark Data Id:Len = 0x%04X:0x%04X Desc = %a Ver=%04x\n", Item->Header.Identifier, Item->Header.Length, Desc, (UINTN) Item->Header.Version));
      Item = PDatLibFindNextWithFilter(PDAT_FIND_ANY_ITEM_FILTER, &PDatFindContext, NULL);
    } while (Item != NULL);
  }
}

/**
  Print early platform info messages includeing the Stage1 module that's
  running, MFH item list and platform data item list.
**/
VOID
EFIAPI
EarlyPlatformInfoMessages (
  VOID
  )
{
  DEBUG_CODE_BEGIN ();
  QUARK_EDKII_STAGE1_HEADER         *Edk2ImageHeader;

  //
  // Find which 'Stage1' image we are running and print the details
  //
  Edk2ImageHeader = (QUARK_EDKII_STAGE1_HEADER *) (FixedPcdGet32 (PcdEsramStage1Base) + FixedPcdGet32 (PcdFvSecurityHeaderSize));

  DEBUG (
    (EFI_D_INFO,
    "Quark EDKII SECURE LOCKDOWN %a\n",
    QuarkCheckSecureLockBoot() ? "ENABLED" : "DISABLED"
    ));

  switch ((UINT8)Edk2ImageHeader->ImageIndex & QUARK_STAGE1_IMAGE_TYPE_MASK) {
    case QUARK_STAGE1_BOOT_IMAGE_TYPE:
      DEBUG ((EFI_D_INFO, "Quark EDKII Stage1 Boot Image %d\n", ((UINT8)Edk2ImageHeader->ImageIndex & ~(QUARK_STAGE1_IMAGE_TYPE_MASK))));
      break;

    case QUARK_STAGE1_RECOVERY_IMAGE_TYPE:
      DEBUG ((EFI_D_INFO, "Quark EDKII Stage1 Recovery Image %d\n", ((UINT8)Edk2ImageHeader->ImageIndex & ~(QUARK_STAGE1_IMAGE_TYPE_MASK))));
      break;

    default:
      DEBUG ((EFI_D_INFO, "Quark EDKII Stage1 Unknown Image\n"));
      break;
  }
  DEBUG (
    (EFI_D_INFO,
    "Quark EDKII Stage2 0x%08X:0x%08X - Payload 0x%08X:0x%08X\n" ,
    (UINTN) PcdGet32 (PcdFlashFvMainBase),
    (UINTN) PcdGet32 (PcdFlashFvMainSize),
    (UINTN) PcdGet32 (PcdFlashFvPayloadBase),
    (UINTN) PcdGet32 (PcdFlashFvPayloadSize)
    ));

  if (!PlatformIsBootWithRecoveryStage1()) {
    TraceMfhItems ();
  }

  TracePlatformDataItems ();

  DEBUG_CODE_END ();
}

/**
  Check if system reset due to error condition.

  @param  ClearErrorBits  If TRUE clear error flags and value bits.

  @retval TRUE  if system reset due to error condition.
  @retval FALSE if NO reset error conditions.
**/
BOOLEAN
CheckForResetDueToErrors (
  IN BOOLEAN                              ClearErrorBits
  )
{
  UINT32                            RegValue;
  BOOLEAN                           ResetDueToError;

  ResetDueToError = FALSE;

  //
  // Check if RMU reset system due to access violations.
  // RMU updates a SOC Unit register before reseting the system.
  //
  RegValue = QNCAltPortRead (QUARK_SCSS_SOC_UNIT_SB_PORT_ID, QUARK_SCSS_SOC_UNIT_CFG_STICKY_RW);
  if ((RegValue & B_CFG_STICKY_RW_VIOLATION) != 0) {
    ResetDueToError = TRUE;

    DEBUG (
      (EFI_D_ERROR,
      "\nReset due to access violation: %s %s %s %s\n",
      ((RegValue & B_CFG_STICKY_RW_IMR_VIOLATION) != 0) ? L"'IMR'" : L".",
      ((RegValue & B_CFG_STICKY_RW_DECC_VIOLATION) != 0) ? L"'DECC'" : L".",
      ((RegValue & B_CFG_STICKY_RW_SMM_VIOLATION) != 0) ? L"'SMM'" : L".",
      ((RegValue & B_CFG_STICKY_RW_HMB_VIOLATION) != 0) ? L"'HMB'" : L"."
      ));

    //
    // Clear error bits.
    //
    if (ClearErrorBits) {
      RegValue &= ~(B_CFG_STICKY_RW_VIOLATION);
      QNCAltPortWrite (QUARK_SCSS_SOC_UNIT_SB_PORT_ID, QUARK_SCSS_SOC_UNIT_CFG_STICKY_RW, RegValue);
    }
  }

  return ResetDueToError;
}

/**
  This function provides early platform initialisation.

  @param  PlatformInfo  Pointer to platform Info structure.
  
  @param  BootMode      Boot mode for platform Info structure.

**/
VOID
EFIAPI
EarlyPlatformInit (
  IN CONST EFI_PLATFORM_INFO              *PlatformInfo,
  IN CONST EFI_BOOT_MODE                   BootMode
  )
{
  EFI_PLATFORM_TYPE                 PlatformType;

  PlatformType = (EFI_PLATFORM_TYPE) PlatformInfo->Type;

  //
  // Check if system reset due to error condition.
  //
  if (CheckForResetDueToErrors (TRUE)) {
    if(FeaturePcdGet (WaitIfResetDueToError)) {
      DEBUG ((EFI_D_ERROR, "Press any key to continue.\n"));
      PlatformDebugPortGetChar8 ();
    }
  }

  //
  // Display platform info messages.
  //
  EarlyPlatformInfoMessages ();

  //
  // Early Legacy Gpio Init.
  //
  EarlyPlatformLegacyGpioInit (PlatformType, BootMode);

  //
  // Early platform Legacy GPIO manipulation depending on GPIOs
  // setup by EarlyPlatformLegacyGpioInit.
  //
  EarlyPlatformLegacyGpioManipulation (PlatformType);

  //
  // Early platform specific GPIO Controller init & manipulation.
  // Combined for sharing of temp. memory bar.
  //
  EarlyPlatformGpioCtrlerInitAndManipulation (PlatformType, BootMode);

  //
  // Early Thermal Sensor Init.
  //
  EarlyPlatformThermalSensorInit ();

  //
  // Early Lan Ethernet Mac Init.
  //
  EarlyPlatformMacInit (
    PlatformInfo->SysData.IohMac0Address,
    PlatformInfo->SysData.IohMac1Address
    );

  //
  // Early TPM Init.
  //
  EarlyPlatformTpmInit (PlatformType);

  //
  // Init Redirect PEI services.
  //
  RedirectServicesInit ();
}

/**
  This function provides early platform Legacy GPIO initialisation.

  @param  PlatformType  Platform type for GPIO init.
  @param  BootMode      Boot mode for GPIO init.

**/
VOID
EFIAPI
EarlyPlatformLegacyGpioInit (
  IN CONST EFI_PLATFORM_TYPE              PlatformType,
  IN CONST EFI_BOOT_MODE                  BootMode
  )
{
  BOARD_LEGACY_GPIO_CONFIG          *LegacyGpioConfig;
  UINT32                            NewValue;
  UINT32                            GpioBaseAddress;
  UINTN                             TypeIndex;
  BOOLEAN                           TypeFound;

  TypeIndex = 0;
  TypeFound = PlatformTypeToTableIndex (PlatformType, &TypeIndex);

  //
  // Assert if platform type not found.
  //
  ASSERT (TypeFound);
  LegacyGpioConfig = &mBoardLegacyGpioConfigTable[TypeIndex];

  GpioBaseAddress = (UINT32)PcdGet16 (PcdGbaIoBaseAddress);

  NewValue     = 0x0;
  //
  // Program QNC Corewell GPIO Registers.
  //
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CGEN_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellEnable;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CGEN_CORE_WELL, NewValue );
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CGIO_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellIoSelect;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CGIO_CORE_WELL, NewValue);
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CGLVL_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellLvlForInputOrOutput;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CGLVL_CORE_WELL, NewValue);
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CGTPE_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellTriggerPositiveEdge;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CGTPE_CORE_WELL, NewValue );
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CGTNE_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellTriggerNegativeEdge;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CGTNE_CORE_WELL, NewValue);
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CGGPE_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellGPEEnable;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CGGPE_CORE_WELL, NewValue);
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CGSMI_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellSMIEnable;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CGSMI_CORE_WELL, NewValue );
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CGTS_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellTriggerStatus;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CGTS_CORE_WELL, NewValue);
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CNMIEN_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellNMIEnable;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CNMIEN_CORE_WELL, NewValue);

  if (BootMode != BOOT_ON_S3_RESUME) {
    //
    // Only program QNC Resumewell GPIO Registers if not resuming from S3.
    //
    NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RGEN_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellEnable;
    IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RGEN_RESUME_WELL, NewValue );
    NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RGIO_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellIoSelect;
    IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RGIO_RESUME_WELL, NewValue) ;
    NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RGLVL_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellLvlForInputOrOutput;
    IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RGLVL_RESUME_WELL, NewValue);
    NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RGTPE_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellTriggerPositiveEdge;
    IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RGTPE_RESUME_WELL, NewValue );
    NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RGTNE_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellTriggerNegativeEdge;
    IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RGTNE_RESUME_WELL, NewValue) ;
    NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RGGPE_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellGPEEnable;
    IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RGGPE_RESUME_WELL, NewValue);
    NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RGSMI_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellSMIEnable;
    IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RGSMI_RESUME_WELL, NewValue );
    NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RGTS_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellTriggerStatus;
    IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RGTS_RESUME_WELL, NewValue) ;
    NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RNMIEN_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellNMIEnable;
    IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RNMIEN_RESUME_WELL, NewValue);
  }
}

/**
  Performs any early platform specific Legacy GPIO manipulation.

  @param  PlatformType  Platform type GPIO manipulation.

**/
VOID
EFIAPI
EarlyPlatformLegacyGpioManipulation (
  IN CONST EFI_PLATFORM_TYPE              PlatformType
  )
{
  if (PlatformType == CrossHill) {

    //
    // Pull TPM reset low for 80us (equivalent to cold reset, Table 39
    // Infineon SLB9645 Databook), then pull TPM reset high and wait for
    // 150ms to give time for TPM to stabilise (Section 4.7.1 Infineon
    // SLB9645 Databook states TPM is ready to receive command after 30ms
    // but section 4.7 states some TPM commands may take longer to execute
    // upto 150ms after test).
    //

    PlatformLegacyGpioSetLevel (
      R_QNC_GPIO_RGLVL_RESUME_WELL,
      PLATFORM_RESUMEWELL_TPM_RST_GPIO,
      FALSE
      );
    MicroSecondDelay (80);

    PlatformLegacyGpioSetLevel (
      R_QNC_GPIO_RGLVL_RESUME_WELL,
      PLATFORM_RESUMEWELL_TPM_RST_GPIO,
      TRUE
      );
    MicroSecondDelay (150000);
  }

  if (PlatformType == RelianceCreek || PlatformType == RelianceCreekSPU) {
    PlatformLegacyGpioSetLevel (
      R_QNC_GPIO_CGLVL_CORE_WELL,
      PLATFORM_COREWELL_TPM_RST_GPIO_RCK,
      FALSE
      );
    MicroSecondDelay (80);

    PlatformLegacyGpioSetLevel (
      R_QNC_GPIO_CGLVL_CORE_WELL,
      PLATFORM_COREWELL_TPM_RST_GPIO_RCK,
      TRUE
      );
    MicroSecondDelay (150000);
  }

}

/**
  Performs any early platform specific GPIO Controller init & manipulation.

  @param  PlatformType  Platform type for GPIO init & manipulation.
  @param  BootMode      Boot mode for GPIO init & manipulation.

**/
VOID
EFIAPI
EarlyPlatformGpioCtrlerInitAndManipulation (
  IN CONST EFI_PLATFORM_TYPE              PlatformType,
  IN CONST EFI_BOOT_MODE                  BootMode
  )
{
  UINT32                            IohGpioBase;
  UINT32                            Data32;
  UINT32                            Addr;
  BOARD_GPIO_CONTROLLER_CONFIG      *GpioConfig;
  UINT32                            DevPcieAddr;
  UINT16                            SaveCmdReg;
  UINT32                            SaveBarReg;
  UINT16                            PciVid;
  UINT16                            PciDid;
  UINTN                             TypeIndex;
  BOOLEAN                           TypeFound;

  TypeIndex = 0;
  TypeFound = PlatformTypeToTableIndex (PlatformType, &TypeIndex);
  ASSERT (TypeFound);

  GpioConfig = &mBoardGpioControllerConfigTable[TypeIndex];

  IohGpioBase = (UINT32) FixedPcdGet64 (PcdIohGpioMmioBase);

  DevPcieAddr = PCI_LIB_ADDRESS (
                  FixedPcdGet8 (PcdIohGpioBusNumber),
                  FixedPcdGet8 (PcdIohGpioDevNumber),
                  FixedPcdGet8 (PcdIohGpioFunctionNumber), 
                  0
                  );

  //
  // Do nothing if not a supported device.
  //
  PciVid = PciRead16 (DevPcieAddr + PCI_VENDOR_ID_OFFSET);
  PciDid = PciRead16 (DevPcieAddr + PCI_DEVICE_ID_OFFSET);
  if((PciVid != V_IOH_I2C_GPIO_VENDOR_ID) || (PciDid != V_IOH_I2C_GPIO_DEVICE_ID)) {
    return;
  }

  //
  // Save current settings for PCI CMD/BAR registers.
  //
  SaveCmdReg = PciRead16 (DevPcieAddr + PCI_COMMAND_OFFSET);
  SaveBarReg = PciRead32 (DevPcieAddr + FixedPcdGet8 (PcdIohGpioBarRegister));

  //
  // Use predefined tempory memory resource.
  //
  PciWrite32 ( DevPcieAddr + FixedPcdGet8 (PcdIohGpioBarRegister), IohGpioBase); 
  PciWrite8 ( DevPcieAddr + PCI_COMMAND_OFFSET, EFI_PCI_COMMAND_MEMORY_SPACE); 

  //
  // Gpio Controller Init Tasks.
  //

  //
  // IEN- Interrupt Enable Register
  //
  Addr =  IohGpioBase + GPIO_INTEN;
  Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
  Data32 |= (GpioConfig->IntEn & 0x000FFFFF);
  *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

  //
  // ISTATUS- Interrupt Status Register
  //
  Addr =  IohGpioBase + GPIO_INTSTATUS;
  Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
  *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

  //
  // GPIO SWPORTA Direction Register - GPIO_SWPORTA_DR
  //
  Addr =  IohGpioBase + GPIO_SWPORTA_DR;
  Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
  Data32 |= (GpioConfig->PortADR & 0x000FFFFF);
  *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

  //
  // GPIO SWPORTA Data Direction Register - GPIO_SWPORTA_DDR - default input
  //
  Addr =  IohGpioBase + GPIO_SWPORTA_DDR;
  Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
  Data32 |= (GpioConfig->PortADir & 0x000FFFFF);
  *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

  //
  // Interrupt Mask Register - GPIO_INTMASK - default interrupts unmasked
  //
  Addr =  IohGpioBase + GPIO_INTMASK;
  Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
  Data32 |= (GpioConfig->IntMask & 0x000FFFFF);
  *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

  //
  // Interrupt Level Type Register - GPIO_INTTYPE_LEVEL - default is level sensitive
  //
  Addr =  IohGpioBase + GPIO_INTTYPE_LEVEL;
  Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
  Data32 |= (GpioConfig->IntType & 0x000FFFFF);
  *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

  //
  // Interrupt Polarity Type Register - GPIO_INT_POLARITY - default is active low
  //
  Addr =  IohGpioBase + GPIO_INT_POLARITY;
  Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
  Data32 |= (GpioConfig->IntPolarity & 0x000FFFFF);
  *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

  //
  // Interrupt Debounce Type Register - GPIO_DEBOUNCE - default no debounce
  //
  Addr =  IohGpioBase + GPIO_DEBOUNCE;
  Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
  Data32 |= (GpioConfig->Debounce & 0x000FFFFF);
  *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

  //
  // Interrupt Clock Synchronisation Register - GPIO_LS_SYNC - default no sync with pclk_intr(APB bus clk)
  //
  Addr =  IohGpioBase + GPIO_LS_SYNC;
  Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
  Data32 |= (GpioConfig->LsSync & 0x000FFFFF);
  *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

  //
  // Gpio Controller Manipulation Tasks.
  //

  if ((PlatformType == (EFI_PLATFORM_TYPE) Galileo) && 
       (BootMode != BOOT_ON_S3_RESUME)) {
    //
    // Reset Cypress Expander on Galileo Platform
    //
    Addr = IohGpioBase + GPIO_SWPORTA_DR;
    Data32 = *((volatile UINT32 *) (UINTN)(Addr));
    Data32 |= BIT4;                                 // Cypress Reset line controlled by GPIO<4>
    *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

    Data32 = *((volatile UINT32 *) (UINTN)(Addr));
    Data32 &= ~BIT4;                                // Cypress Reset line controlled by GPIO<4>
    *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

  }

  //
  // Restore settings for PCI CMD/BAR registers
  //
  PciWrite32 ((DevPcieAddr + FixedPcdGet8 (PcdIohGpioBarRegister)), SaveBarReg);
  PciWrite16 (DevPcieAddr + PCI_COMMAND_OFFSET, SaveCmdReg);
}

/**
  Performs any early platform init of SoC Ethernet Mac devices.

  @param  IohMac0Address  Mac address to program into Mac0 device.
  @param  IohMac1Address  Mac address to program into Mac1 device.

**/
VOID
EFIAPI
EarlyPlatformMacInit (
  IN CONST UINT8                          *IohMac0Address,
  IN CONST UINT8                          *IohMac1Address
  )
{
  BOOLEAN                           SetMacAddr;
  UINTN                             Index;
  UINT8                             Func;
  CONST UINT8                       *IohMacAddress;
  UINT32                            MacMmioBase;

  Func = IOH_MAC0_FUNCTION_NUMBER;
  IohMacAddress = IohMac0Address;
  MacMmioBase = FixedPcdGet64(PcdIohMac0MmioBase);

  for (Index = 0; Index < 2; Index++) {
    //
    // Set chipset MAC address if configured.
    //
    SetMacAddr =
      (CompareMem (ChipsetDefaultMac, IohMacAddress, sizeof (ChipsetDefaultMac))) != 0;
    if (SetMacAddr) {
      if ((*(IohMacAddress) & BIT0) != 0) {
        ASSERT (FALSE);
      } else {
        SetLanControllerMacAddr (
          IOH_MAC0_BUS_NUMBER,
          IOH_MAC0_DEVICE_NUMBER,
          Func,
          IohMacAddress,
          MacMmioBase
          );
      }
    }
    Func = IOH_MAC1_FUNCTION_NUMBER;
    IohMacAddress = IohMac1Address;
    MacMmioBase = FixedPcdGet64(PcdIohMac1MmioBase);
  }
}

/**
  TPM early init for platform type.

  @param  PlatformType  Platform type for TPM init.

**/
VOID
EFIAPI
EarlyPlatformTpmInit (
  IN CONST EFI_PLATFORM_TYPE              PlatformType
  )
{
  UINTN                             Size;

  Size = sizeof (EFI_GUID);

  //
  // Set TPM instance needed for platform type, build time will default to none
  // we need to update PCD if TPM actually needed.
  //

  //
  // Set Inst to gEfiTpmDeviceInstanceTpm12Guid for platforms with TPM1.2 Dev.
  //
  if (PlatformType == CrossHill) {
    PcdSetPtr (
      PcdTpmInstanceGuid,
      &Size,
      (VOID *) &gEfiTpmDeviceInstanceTpm12Guid
      );
  }
}


/**
  Uart0 init for platform type.

  @param  PlatformType  Platform type for Uart0 init.
  @param  BootMode      Boot mode for Uart0 init.

**/

VOID
EFIAPI
PlatformUart0Init (
  IN CONST EFI_PLATFORM_TYPE              PlatformType,
  IN CONST EFI_BOOT_MODE                  BootMode
  )
{
  //
  // Skip any signal not needed for recovery and flash update.
  //
  if (BootMode != BOOT_ON_FLASH_UPDATE && BootMode != BOOT_IN_RECOVERY_MODE) {
    //
    // Galileo Platform UART0 support.
    //
    if (PlatformType == Galileo && BootMode != BOOT_ON_S3_RESUME) {
      //
      // Use MUX to connect out UART0 pins.
      //
      PlatformInitializeUart0MuxGalileo ();
    }
    //
    // GalileoGen2 Platform UART0 support.
    //
    if (PlatformType == GalileoGen2) {
      //
      // Use route out UART0 pins.
      //
      PlatformInitializeUart0MuxGalileoGen2 ();
    }
  }
}

