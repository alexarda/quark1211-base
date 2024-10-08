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

  FwBlockService.h
  
Abstract:

  Firmware volume block driver for SPI device

--*/

#ifndef _FW_BLOCK_SERVICE_H
#define _FW_BLOCK_SERVICE_H


#include "SpiFlashDevice.h"
#include "EfiFlashMap.h"
#include "FlashMap.h"
#include "FlashLayout.h"

//
// Statements that include other header files

#include <Library/IoLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

#include "Guid/FlashMapHob.h"
#include <Guid/EventGroup.h>
#include <Guid/HobList.h>
#include <Guid/FirmwareFileSystem.h>
#include <Guid/SystemNvDataGuid.h>

#include <Protocol/SmmBase2.h>
#include <Protocol/FvbExtension.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/PlatformType.h>
#include <Protocol/PlatformSmmSpiReady.h>

#include <Framework/FirmwareVolumeHeader.h>
#include <CpuRegs.h>



#define EFI_INTERNAL_POINTER  0x00000004
#define FVB_PHYSICAL  0
#define FVB_VIRTUAL   1

typedef struct {
  EFI_LOCK                    FvbDevLock;
  UINTN                       FvBase[2];
  UINTN                       FvWriteBase[2];
  UINTN                       NumOfBlocks;
  BOOLEAN                     WriteEnabled;
  EFI_FIRMWARE_VOLUME_HEADER  VolumeHeader;
} EFI_FW_VOL_INSTANCE;

typedef struct {
  UINT32                NumFv;
  EFI_FW_VOL_INSTANCE   *FvInstance[2];
  UINT8                 *FvbScratchSpace[2];
  EFI_SPI_PROTOCOL      *SpiProtocol;
  EFI_SPI_PROTOCOL      *SmmSpiProtocol;  
} ESAL_FWB_GLOBAL;

//
// SPI default opcode slots
//
#define SPI_OPCODE_JEDEC_ID_INDEX        0
#define SPI_OPCODE_READ_ID_INDEX         1
#define SPI_OPCODE_WRITE_S_INDEX         2
#define SPI_OPCODE_WRITE_INDEX           3 
#define SPI_OPCODE_READ_INDEX            4
#define SPI_OPCODE_ERASE_INDEX           5
#define SPI_OPCODE_READ_S_INDEX          6
#define SPI_OPCODE_CHIP_ERASE_INDEX      7

#define SPI_ERASE_SECTOR_SIZE            FLASH_BLOCK_SIZE //This is the chipset requirement

//
// Fvb Protocol instance data
//
#define FVB_DEVICE_FROM_THIS(a)         CR (a, EFI_FW_VOL_BLOCK_DEVICE, FwVolBlockInstance, FVB_DEVICE_SIGNATURE)
#define FVB_EXTEND_DEVICE_FROM_THIS(a)  CR (a, EFI_FW_VOL_BLOCK_DEVICE, FvbExtension, FVB_DEVICE_SIGNATURE)
#define FVB_DEVICE_SIGNATURE            SIGNATURE_32 ('F', 'V', 'B', 'C')
//
// Device Path 
//
#define EFI_END_ENTIRE_DEVICE_PATH_SUBTYPE    0xff
#define EfiDevicePathType(a)                  (((a)->Type) & 0x7f)
#define EfiIsDevicePathEndType(a)             (EfiDevicePathType (a) == 0x7f)
#define EfiIsDevicePathEndSubType(a)          ((a)->SubType == EFI_END_ENTIRE_DEVICE_PATH_SUBTYPE)
#define EfiIsDevicePathEnd(a)                 (EfiIsDevicePathEndType (a) && EfiIsDevicePathEndSubType (a))

typedef struct {
  MEMMAP_DEVICE_PATH        MemMapDevPath;
  EFI_DEVICE_PATH_PROTOCOL  EndDevPath;
} FV_DEVICE_PATH;

typedef struct {
  UINTN                               Signature;
  FV_DEVICE_PATH                      DevicePath;
  UINTN                               Instance;
  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  FwVolBlockInstance;
  EFI_FVB_EXTENSION_PROTOCOL          FvbExtension;
} EFI_FW_VOL_BLOCK_DEVICE;

typedef struct {
  EFI_PHYSICAL_ADDRESS        BaseAddress;
  EFI_FIRMWARE_VOLUME_HEADER  FvbInfo;
  //
  // EFI_FV_BLOCK_MAP_ENTRY                ExtraBlockMap[n];//n=0
  //
  EFI_FV_BLOCK_MAP_ENTRY      End[1];
} EFI_FVB_MEDIA_INFO;

VOID
FvbVirtualddressChangeEvent (
  IN EFI_EVENT        Event,
  IN VOID             *Context
  );

EFI_STATUS
GetFvbInfo (
  IN  EFI_PHYSICAL_ADDRESS              FvBaseAddress,
  OUT EFI_FIRMWARE_VOLUME_HEADER        **FvbInfo
  );

BOOLEAN
SetPlatformFvbLock (
  IN UINTN                              LbaAddress
  );

EFI_STATUS
FvbReadBlock (
  IN UINTN                              Instance,
  IN EFI_LBA                            Lba,
  IN UINTN                              BlockOffset,
  IN OUT UINTN                          *NumBytes,
  IN UINT8                              *Buffer,
  IN ESAL_FWB_GLOBAL                    *Global,
  IN BOOLEAN                            Virtual
  );

EFI_STATUS
FvbWriteBlock (
  IN UINTN                              Instance,
  IN EFI_LBA                            Lba,
  IN UINTN                              BlockOffset,
  IN OUT UINTN                          *NumBytes,
  IN UINT8                              *Buffer,
  IN ESAL_FWB_GLOBAL                    *Global,
  IN BOOLEAN                            Virtual
  );

EFI_STATUS
FvbEraseBlock (
  IN UINTN                              Instance,
  IN EFI_LBA                            Lba,
  IN ESAL_FWB_GLOBAL                    *Global,
  IN BOOLEAN                            Virtual
  );

EFI_STATUS
FvbSetVolumeAttributes (
  IN UINTN                              Instance,
  IN OUT EFI_FVB_ATTRIBUTES_2             *Attributes,
  IN ESAL_FWB_GLOBAL                    *Global,
  IN BOOLEAN                            Virtual
  );

EFI_STATUS
FvbGetVolumeAttributes (
  IN UINTN                              Instance,
  OUT EFI_FVB_ATTRIBUTES_2                *Attributes,
  IN ESAL_FWB_GLOBAL                    *Global,
  IN BOOLEAN                            Virtual
  );

EFI_STATUS
FvbGetPhysicalAddress (
  IN UINTN                              Instance,
  OUT EFI_PHYSICAL_ADDRESS              *Address,
  IN ESAL_FWB_GLOBAL                    *Global,
  IN BOOLEAN                            Virtual
  );

EFI_STATUS
FvbInitialize (
  IN EFI_HANDLE                         ImageHandle,
  IN EFI_SYSTEM_TABLE                   *SystemTable
  );

VOID
FvbClassAddressChangeEvent (
  IN EFI_EVENT                          Event,
  IN VOID                               *Context
  );

EFI_STATUS
FvbSpecificInitialize (
  IN  ESAL_FWB_GLOBAL                   *mFvbModuleGlobal
  );

EFI_STATUS
FvbGetLbaAddress (
  IN  UINTN                             Instance,
  IN  EFI_LBA                           Lba,
  OUT UINTN                             *LbaAddress,
  OUT UINTN                             *LbaWriteAddress,
  OUT UINTN                             *LbaLength,
  OUT UINTN                             *NumOfBlocks,
  IN  ESAL_FWB_GLOBAL                   *Global,
  IN  BOOLEAN                           Virtual
  );

EFI_STATUS
FvbEraseCustomBlockRange (
  IN UINTN                              Instance,
  IN EFI_LBA                            StartLba,
  IN UINTN                              OffsetStartLba,
  IN EFI_LBA                            LastLba,
  IN UINTN                              OffsetLastLba,
  IN ESAL_FWB_GLOBAL                    *Global,
  IN BOOLEAN                            Virtual
  );

//
// Protocol APIs
//
EFI_STATUS
EFIAPI
FvbProtocolGetAttributes (
  IN CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
  OUT EFI_FVB_ATTRIBUTES_2                *Attributes
  );

EFI_STATUS
EFIAPI
FvbProtocolSetAttributes (
  IN CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
  IN OUT EFI_FVB_ATTRIBUTES_2             *Attributes
  );

EFI_STATUS
EFIAPI
FvbProtocolGetPhysicalAddress (
  IN CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
  OUT EFI_PHYSICAL_ADDRESS              *Address
  );

EFI_STATUS
FvbProtocolGetBlockSize (
  IN CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
  IN  EFI_LBA                           Lba,
  OUT UINTN                             *BlockSize,
  OUT UINTN                             *NumOfBlocks
  );

EFI_STATUS
EFIAPI
FvbProtocolRead (
  IN CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
  IN EFI_LBA                            Lba,
  IN UINTN                              Offset,
  IN OUT UINTN                          *NumBytes,
  IN UINT8                              *Buffer
  );

EFI_STATUS
EFIAPI
FvbProtocolWrite (
  IN CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
  IN EFI_LBA                            Lba,
  IN UINTN                              Offset,
  IN OUT UINTN                          *NumBytes,
  IN UINT8                              *Buffer
  );

EFI_STATUS
EFIAPI
FvbProtocolEraseBlocks (
  IN CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL         *This,
  ...
  );

EFI_STATUS
FvbExtendProtocolEraseCustomBlockRange (
  IN EFI_FVB_EXTENSION_PROTOCOL         *This,
  IN EFI_LBA                            StartLba,
  IN UINTN                              OffsetStartLba,
  IN EFI_LBA                            LastLba,
  IN UINTN                              OffsetLastLba
  );

extern EFI_GUID gEfiAlternateFvBlockGuid;
extern SPI_INIT_TABLE   mSpiInitTable[];

#endif
