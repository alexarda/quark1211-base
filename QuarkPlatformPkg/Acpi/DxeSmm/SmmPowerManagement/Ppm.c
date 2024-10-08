/*++

  Processor power management initialization code.

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


--*/

#include "SmmPowerManagement.h"

//
// Global variables
//
UINT32 mPpmFlags = (FixedPcdGet32(PcdPpmFlags));      // Processor Power Management Flags
CPU_CONFIG_CONTEXT_BUFFER     *mCpuConfigConextBuffer;
extern EFI_ACPI_SDT_PROTOCOL   *mAcpiSdt;
extern EFI_ACPI_TABLE_PROTOCOL *mAcpiTable;

extern EFI_GUID gPowerManagementAcpiTableStorageGuid;

/**
  This function is the entry of processor power management initialization code. 
  It initializes the processor's power management features based on the user 
  configurations and hardware capablities.
**/
VOID
PpmInit (
  VOID
  )
{
  mGlobalNvsAreaPtr->Cfgd = mPpmFlags;

  //
  // Patch and publish power management related acpi tables
  //
  PpmPatchAndPublishAcpiTables();

}

/**
  This function is to patch and publish power management related acpi tables.
**/
VOID
PpmPatchAndPublishAcpiTables (
  VOID
  )
{
    //
    // Patch FADT table to enable C2,C3
    //
  PpmPatchFadtTable();
  
  //
    // Load all the power management acpi tables and patch IST table
    //
    PpmLoadAndPatchPMTables();
}

/**
  This function is to patch PLvl2Lat and PLvl3Lat to enable C2, C3 support in OS.
**/
VOID
PpmPatchFadtTable (
  VOID
  )
{
    EFI_STATUS                    Status;
  EFI_ACPI_DESCRIPTION_HEADER   *Table;
  EFI_ACPI_SDT_HEADER           *CurrentTable;
  EFI_ACPI_TABLE_VERSION        Version;
  UINTN                         Index;
  UINTN                         Handle;
  EFI_ACPI_3_0_FIXED_ACPI_DESCRIPTION_TABLE  *FadtPointer;  
  
  //
  // Scan all the acpi tables to find FADT 2.0
  //
  Index = 0;
  do {
    Status = mAcpiSdt->GetAcpiTable (                       
                       Index,
                       &CurrentTable,
                       &Version,
                       &Handle
                       );
    if (Status == EFI_NOT_FOUND) {
      break;
    }
    ASSERT_EFI_ERROR (Status);
    Index++;
  } while (CurrentTable->Signature != EFI_ACPI_3_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE || CurrentTable->Revision != 0x03);

  ASSERT (CurrentTable->Signature == EFI_ACPI_3_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE);
  
  Table  = NULL;
  Status = gBS->AllocatePool (EfiBootServicesData, CurrentTable->Length, (VOID **) &Table);
  ASSERT (Table != NULL);
  CopyMem (Table, CurrentTable, CurrentTable->Length);  
  
  FadtPointer = (EFI_ACPI_3_0_FIXED_ACPI_DESCRIPTION_TABLE*) Table;
  
    //
  // Update the ACPI table and recalculate checksum
  //
  Status = mAcpiTable->UninstallAcpiTable (mAcpiTable, Handle);
  if (EFI_ERROR (Status)) {
     //
     // Should not get an error here ever, but abort if we do.
     //
     return ;
  }
  
  //
  // Update the check sum 
  // It needs to be zeroed before the checksum calculation
  //
  ((EFI_ACPI_SDT_HEADER *)Table)->Checksum = 0;    
  ((EFI_ACPI_SDT_HEADER *)Table)->Checksum = 
    CalculateCheckSum8 ((VOID *)Table, Table->Length);  
    
  //
  // Add the table
  //
  Status = mAcpiTable->InstallAcpiTable (
                            mAcpiTable,
                            Table,
                            Table->Length,
                            &Handle
                            );                                 
  ASSERT_EFI_ERROR (Status);
  gBS->FreePool (Table);
}

VOID
SsdtTableUpdate (
  IN OUT   EFI_ACPI_DESCRIPTION_HEADER  *TableHeader
  )
/*++

  Routine Description:

    Update the SSDT table

  Arguments:

    Table   - The SSDT table to be patched

  Returns:

    None

--*/
{
  UINT8      *CurrPtr;
  UINT8      *SsdtPointer;
  UINT32     *Signature;
  
  //
  // Loop through the ASL looking for values that we must fix up.
  //
  CurrPtr = (UINT8 *) TableHeader;
  for (SsdtPointer = CurrPtr;
       SsdtPointer <= (CurrPtr + ((EFI_ACPI_COMMON_HEADER *) CurrPtr)->Length);
       SsdtPointer++
      )
  {
    Signature = (UINT32 *) SsdtPointer;
    if ((*Signature) == SIGNATURE_32 ('P', 'M', 'B', 'A')) {
      switch (*(Signature+1)) {
      case (SIGNATURE_32 ('L', 'V', 'L', '0')):
        Signature[0] = PcdGet16(PcdPmbaIoBaseAddress);
        Signature[1] = 0;
        break;
      case (SIGNATURE_32 ('L', 'V', 'L', '2')):
        Signature[0] = PcdGet16(PcdPmbaIoLVL2);
        Signature[1] = 0;
        break;
      }
    }    
  }
}

EFI_STATUS
LocateSupportProtocol (
  IN  EFI_GUID                       *Protocol,
  OUT VOID                           **Instance,
  IN  UINT32                         Type
  )
/*++

Routine Description:

  Locate the first instance of a protocol.  If the protocol requested is an
  FV protocol, then it will return the first FV that contains the ACPI table
  storage file.

Arguments:

  Protocol      The protocol to find.
  Instance      Return pointer to the first instance of the protocol

Returns:

  EFI_SUCCESS           The function completed successfully.
  EFI_NOT_FOUND         The protocol could not be located.
  EFI_OUT_OF_RESOURCES  There are not enough resources to find the protocol.

--*/
{
  EFI_STATUS              Status;
  EFI_HANDLE              *HandleBuffer;
  UINTN                   NumberOfHandles;
  EFI_FV_FILETYPE         FileType;
  UINT32                  FvStatus;
  EFI_FV_FILE_ATTRIBUTES  Attributes;
  UINTN                   Size;
  UINTN                   i;

  FvStatus = 0;

  //
  // Locate protocol.
  //
  Status = gBS->LocateHandleBuffer (
                   ByProtocol,
                   Protocol,
                   NULL,
                   &NumberOfHandles,
                   &HandleBuffer
                   );
  if (EFI_ERROR (Status)) {

    //
    // Defined errors at this time are not found and out of resources.
    //
    return Status;
  }



  //
  // Looking for FV with ACPI storage file
  //

  for (i = 0; i < NumberOfHandles; i++) {
    //
    // Get the protocol on this handle
    // This should not fail because of LocateHandleBuffer
    //
    Status = gBS->HandleProtocol (
                     HandleBuffer[i],
                     Protocol,
                     Instance
                     );
    ASSERT_EFI_ERROR (Status);

    if (!Type) {
      //
      // Not looking for the FV protocol, so find the first instance of the
      // protocol.  There should not be any errors because our handle buffer
      // should always contain at least one or LocateHandleBuffer would have
      // returned not found.
      //
      break;
    }

    //
    // See if it has the ACPI storage file
    //

    Status = ((EFI_FIRMWARE_VOLUME2_PROTOCOL*) (*Instance))->ReadFile (*Instance,
                                                              &gPowerManagementAcpiTableStorageGuid,
                                                              NULL,
                                                              &Size,
                                                              &FileType,
                                                              &Attributes,
                                                              &FvStatus
                                                              );

    //
    // If we found it, then we are done
    //
    if (Status == EFI_SUCCESS) {
      break;
    }
  }

  //
  // Our exit status is determined by the success of the previous operations
  // If the protocol was found, Instance already points to it.
  //

  //
  // Free any allocated buffers
  //
  gBS->FreePool (HandleBuffer);

  return Status;
}

/**
  This function is to load all the power management acpi tables and patch IST table.
**/
VOID
PpmLoadAndPatchPMTables (
  VOID
  )
{
    EFI_FIRMWARE_VOLUME2_PROTOCOL *FwVol;
    EFI_STATUS                    Status;    
    INTN                          Instance;
  EFI_ACPI_COMMON_HEADER        *CurrentTable;
  UINTN                         TableHandle;
  UINT32                        FvStatus;
  UINTN                         Size;
   EFI_ACPI_TABLE_VERSION       Version;
    
    Status = LocateSupportProtocol (&gEfiFirmwareVolume2ProtocolGuid, (VOID**)&FwVol, 1);    
    if (EFI_ERROR (Status)) {
    return;
  }
  
  //
  // Read tables from the storage file.
  //  
  Instance = 0;
  CurrentTable = NULL;
  
  while (Status == EFI_SUCCESS) {

    Status = FwVol->ReadSection (
                      FwVol,
                      &gPowerManagementAcpiTableStorageGuid,
                      EFI_SECTION_RAW,
                      Instance,
                      (VOID**)&CurrentTable,
                      &Size,
                      &FvStatus
                      );

    if (!EFI_ERROR(Status)) {        
        Version = EFI_ACPI_TABLE_VERSION_1_0B | EFI_ACPI_TABLE_VERSION_2_0 | EFI_ACPI_TABLE_VERSION_3_0;
      
      if(((EFI_ACPI_DESCRIPTION_HEADER*) CurrentTable)->OemTableId == SIGNATURE_64 ('C', 'p', 'u', '0', 'I', 's', 't', 0)) {
          Version = EFI_ACPI_TABLE_VERSION_NONE;              
      } else if(((EFI_ACPI_DESCRIPTION_HEADER*) CurrentTable)->OemTableId == SIGNATURE_64 ('C', 'p', 'u', '1', 'I', 's', 't', 0)) {
          Version = EFI_ACPI_TABLE_VERSION_NONE;                
      }

      SsdtTableUpdate ((EFI_ACPI_DESCRIPTION_HEADER *) CurrentTable);
      
      //
      // Update the check sum 
      // It needs to be zeroed before the checksum calculation
      //
      ((EFI_ACPI_SDT_HEADER *)CurrentTable)->Checksum = 0;    
      ((EFI_ACPI_SDT_HEADER *)CurrentTable)->Checksum = (UINT8)
        CalculateCheckSum8 ((VOID *)CurrentTable, CurrentTable->Length);
      
      //
      // Add the table
      //
      TableHandle = 0;
      Status = mAcpiTable->InstallAcpiTable (
                              mAcpiTable,
                              CurrentTable,
                              CurrentTable->Length,
                              &TableHandle
                              );
                              
      ASSERT_EFI_ERROR (Status);

      //
      // Increment the instance
      //
      Instance++;
      CurrentTable = NULL;
    }
  }
  
}