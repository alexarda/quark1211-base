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

  SmbiosMiscDataTable.c
  
Abstract: 

  This driver parses the mSmbiosMiscDataTable structure and reports
  any generated data using SMBIOS protocol. 

--*/


#include "CommonHeader.h"

#include "SmbiosMisc.h"


//
// External definitions referenced by Data Table entries.
//

MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_BIOS_VENDOR, MiscBiosVendor, MiscBiosVendor);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_SYSTEM_MANUFACTURER, MiscSystemManufacturer, MiscSystemManufacturer);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_SYSTEM_MANUFACTURER, MiscBaseBoardManufacturer, MiscBaseBoardManufacturer);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_CHASSIS_MANUFACTURER, MiscChassisManufacturer, MiscChassisManufacturer);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_BOOT_INFORMATION_STATUS, MiscBootInfoStatus, MiscBootInfoStatus);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_NUMBER_OF_INSTALLABLE_LANGUAGES, NumberOfInstallableLanguages, NumberOfInstallableLanguages);
MISC_SMBIOS_TABLE_EXTERNS (EFI_MISC_SYSTEM_OPTION_STRING, SystemOptionString, SystemOptionString);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_ONBOARD_DEVICE, MiscOnboardDeviceVideo, MiscOnboardDevice);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_OEM_STRING,MiscOemString, MiscOemString);

MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_PORT_INTERNAL_CONNECTOR_DESIGNATOR, MiscPortConnector1, MiscPortInternalConnectorDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_PORT_INTERNAL_CONNECTOR_DESIGNATOR, MiscPortConnector2, MiscPortInternalConnectorDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_PORT_INTERNAL_CONNECTOR_DESIGNATOR, MiscPortConnector3, MiscPortInternalConnectorDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_PORT_INTERNAL_CONNECTOR_DESIGNATOR, MiscPortConnector4, MiscPortInternalConnectorDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_PORT_INTERNAL_CONNECTOR_DESIGNATOR, MiscPortConnector5, MiscPortInternalConnectorDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_PORT_INTERNAL_CONNECTOR_DESIGNATOR, MiscPortConnector6, MiscPortInternalConnectorDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_PORT_INTERNAL_CONNECTOR_DESIGNATOR, MiscPortConnector7, MiscPortInternalConnectorDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_PORT_INTERNAL_CONNECTOR_DESIGNATOR, MiscPortConnector8, MiscPortInternalConnectorDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_PORT_INTERNAL_CONNECTOR_DESIGNATOR, MiscPortConnector9, MiscPortInternalConnectorDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_PORT_INTERNAL_CONNECTOR_DESIGNATOR, MiscPortConnector10, MiscPortInternalConnectorDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_PORT_INTERNAL_CONNECTOR_DESIGNATOR, MiscPortConnector11, MiscPortInternalConnectorDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_PORT_INTERNAL_CONNECTOR_DESIGNATOR, MiscPortConnector12, MiscPortInternalConnectorDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_PORT_INTERNAL_CONNECTOR_DESIGNATOR, MiscPortConnector13, MiscPortInternalConnectorDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_PORT_INTERNAL_CONNECTOR_DESIGNATOR, MiscPortConnector14, MiscPortInternalConnectorDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_SYSTEM_SLOT_DESIGNATION, MiscSystemSlot1, MiscSystemSlotDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_SYSTEM_SLOT_DESIGNATION, MiscSystemSlot2, MiscSystemSlotDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_SYSTEM_SLOT_DESIGNATION, MiscSystemSlot3, MiscSystemSlotDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_SYSTEM_SLOT_DESIGNATION, MiscSystemSlot4, MiscSystemSlotDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_SYSTEM_SLOT_DESIGNATION, MiscSystemSlot5, MiscSystemSlotDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_SYSTEM_SLOT_DESIGNATION, MiscSystemSlot6, MiscSystemSlotDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_SYSTEM_SLOT_DESIGNATION, MiscSystemSlot7, MiscSystemSlotDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_SYSTEM_SLOT_DESIGNATION, MiscSystemSlot8, MiscSystemSlotDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_SYSTEM_SLOT_DESIGNATION, MiscSystemSlot9, MiscSystemSlotDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_SYSTEM_SLOT_DESIGNATION, MiscSystemSlot10, MiscSystemSlotDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_SYSTEM_SLOT_DESIGNATION, MiscSystemSlot11, MiscSystemSlotDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_SYSTEM_SLOT_DESIGNATION, MiscSystemSlot12, MiscSystemSlotDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_SYSTEM_SLOT_DESIGNATION, MiscSystemSlot13, MiscSystemSlotDesignator);
MISC_SMBIOS_TABLE_EXTERNS(EFI_MISC_SYSTEM_SLOT_DESIGNATION, MiscSystemSlot14, MiscSystemSlotDesignator);


//
// Data Table
//
EFI_MISC_SMBIOS_DATA_TABLE mSmbiosMiscDataTable[] = {
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscBiosVendor, MiscBiosVendor),
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscSystemManufacturer, MiscSystemManufacturer), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscBaseBoardManufacturer, MiscBaseBoardManufacturer), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscChassisManufacturer, MiscChassisManufacturer),
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscPortConnector1, MiscPortInternalConnectorDesignator), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscPortConnector2, MiscPortInternalConnectorDesignator), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscPortConnector3, MiscPortInternalConnectorDesignator), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscPortConnector4, MiscPortInternalConnectorDesignator), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscPortConnector5, MiscPortInternalConnectorDesignator), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscPortConnector6, MiscPortInternalConnectorDesignator),  
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscPortConnector7, MiscPortInternalConnectorDesignator),
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscPortConnector8, MiscPortInternalConnectorDesignator),
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscPortConnector9, MiscPortInternalConnectorDesignator), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscPortConnector10, MiscPortInternalConnectorDesignator), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscPortConnector11, MiscPortInternalConnectorDesignator), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscPortConnector12, MiscPortInternalConnectorDesignator),
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscPortConnector13, MiscPortInternalConnectorDesignator),
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscPortConnector14, MiscPortInternalConnectorDesignator),
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscSystemSlot1, MiscSystemSlotDesignator),
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscSystemSlot2, MiscSystemSlotDesignator), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscSystemSlot3, MiscSystemSlotDesignator), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscSystemSlot4, MiscSystemSlotDesignator),
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscSystemSlot5, MiscSystemSlotDesignator),
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscSystemSlot6, MiscSystemSlotDesignator), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscSystemSlot7, MiscSystemSlotDesignator), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscSystemSlot8, MiscSystemSlotDesignator),
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscSystemSlot9, MiscSystemSlotDesignator),
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscSystemSlot10, MiscSystemSlotDesignator), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscSystemSlot11, MiscSystemSlotDesignator), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscSystemSlot12, MiscSystemSlotDesignator), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscSystemSlot13, MiscSystemSlotDesignator), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscSystemSlot14, MiscSystemSlotDesignator),
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscOnboardDeviceVideo, MiscOnboardDevice), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscOemString, MiscOemString),
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(SystemOptionString, SystemOptionString), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(NumberOfInstallableLanguages, NumberOfInstallableLanguages), 
  MISC_SMBIOS_TABLE_ENTRY_DATA_AND_FUNCTION(MiscBootInfoStatus, MiscBootInfoStatus)
};

//
// Number of Data Table entries.
//
UINTN mSmbiosMiscDataTableEntries =
  (sizeof mSmbiosMiscDataTable) / sizeof(EFI_MISC_SMBIOS_DATA_TABLE);
