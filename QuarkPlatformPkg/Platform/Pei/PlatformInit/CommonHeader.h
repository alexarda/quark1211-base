/**@file
  Common header file shared by all source files.

  This file includes package header files, library classes and protocol, PPI & GUID definitions.

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

#ifndef __COMMON_HEADER_H_
#define __COMMON_HEADER_H_



#include <FrameworkPei.h>
#include <IntelQNCPeim.h>
#include "Ioh.h"
#include <Platform.h>
#include <PlatformBoards.h>

#include <IndustryStandard/SmBus.h>
#include <IndustryStandard/Pci22.h>

#include <Guid/AcpiVariable.h>
#include <Ppi/AtaController.h>
#include <Guid/Capsule.h>
#include <Ppi/SStateBootMode.h>
#include <Ppi/Cache.h>
#include <Ppi/MasterBootMode.h>
#include <Guid/PlatformInfo.h>
#include <Guid/MemoryTypeInformation.h>
#include <Guid/RecoveryDevice.h>
#include <Guid/MemoryConfigData.h>
#include <Guid/PlatformMemoryLayout.h>
#include <Guid/MemoryOverwriteControl.h>
#include <Guid/CapsuleVendor.h>
#include <Guid/QuarkCapsuleGuid.h>
#include <Ppi/ReadOnlyVariable2.h>
#include <Ppi/FvLoadFile.h>
#include <Guid/SmramMemoryReserve.h>
#include <Guid/TpmInstance.h>
#include <Ppi/DeviceRecoveryModule.h>
#include <Ppi/Capsule.h>
#include <Ppi/Reset.h>
#include <Ppi/Stall.h>
#include <Ppi/BootInRecoveryMode.h>
#include <Guid/FirmwareFileSystem2.h>
#include <Ppi/MemoryDiscovered.h>
#include <Ppi/RecoveryModule.h>
#include <Ppi/Smbus2.h>
#include <Ppi/FirmwareVolumeInfo.h>
#include <Ppi/EndOfPeiPhase.h>

#include <Library/DebugLib.h>
#include <Library/PeimEntryPoint.h>
#include <Library/BaseLib.h>
#include <Library/PeiServicesTablePointerLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/PciCf8Lib.h>
#include <Library/IoLib.h>
#include <Library/PciLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/IntelQNCLib.h>
#include <Library/PcdLib.h>
#include <Library/SmbusLib.h>
#include <Library/RecoveryOemHookLib.h>
#include <Library/TimerLib.h>
#include <Library/PrintLib.h>
#include <Library/ResetSystemLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PerformanceLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/MtrrLib.h>
#include <Library/QNCAccessLib.h>
#include <Library/SwBpeLib.h>
#include <Library/MfhLib.h>
#include <Library/PlatformHelperLib.h>
#include <Library/PlatformPcieHelperLib.h>
#include <Guid/PlatformDataFileNameGuids.h>
#include <CpuRegs.h>
#include <Library/PlatformDataLib.h>
#include <Library/QuarkBootRomLib.h>
#include <Library/RedirectPeiServicesLib.h>

#include <Uefi/UefiBaseType.h>

#endif
