/** @file
  Enable SMM profile.

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

#include "PiSmmCpuDxeSmm.h"
#include "SmmProfileInternal.h"

SMM_PROFILE_HEADER        *mSmmProfileBase;
MSR_DS_AREA_STRUCT        *mMsrDsAreaBase;
//
// The buffer to store SMM profile data.
//
UINTN                     mSmmProfileSize  = FixedPcdGet32 (PcdCpuSmmProfileSize);

//
// The buffer to enable branch trace store.
//
UINTN                     mMsrDsAreaSize   = SMM_PROFILE_DTS_SIZE;

//
// The flag indicates if execute-disable is supported by processor.
//
BOOLEAN                   mXdSupported     = FALSE;

//
// The flag indicates if BTS is supported by processor.
//
BOOLEAN                   mBtsSupported     = FALSE;

//
// The flag indicates if SMM profile starts to record data.
//
BOOLEAN                   mSmmProfileStart = FALSE;

//
// Record the page fault exception count for one instruction execution.
//
UINTN                     mPFEntryCount[FixedPcdGet32 (PcdCpuMaxLogicalProcessorNumber)];

UINT64                    mLastPFEntryValue[FixedPcdGet32 (PcdCpuMaxLogicalProcessorNumber)][MAX_PF_ENTRY_COUNT];
UINT64                    *mLastPFEntryPointer[FixedPcdGet32 (PcdCpuMaxLogicalProcessorNumber)][MAX_PF_ENTRY_COUNT];

MSR_DS_AREA_STRUCT        *mMsrDsArea[FixedPcdGet32 (PcdCpuMaxLogicalProcessorNumber)];
BRANCH_TRACE_RECORD       *mMsrBTSRecord[FixedPcdGet32 (PcdCpuMaxLogicalProcessorNumber)];
UINTN                     mBTSRecordNumber;
PEBS_RECORD               *mMsrPEBSRecord[FixedPcdGet32 (PcdCpuMaxLogicalProcessorNumber)];

//
// These memory ranges are always present, they does not generate the access type of page fault exception,
// but they possibly generate instruction fetch type of page fault exception.
//
MEMORY_PROTECTION_RANGE   *mProtectionMemRange     = NULL;
UINTN                     mProtectionMemRangeCount = 0;

//
// Some pre-defined memory ranges.
//
MEMORY_PROTECTION_RANGE mProtectionMemRangeTemplate[] = {
  //
  // SMRAM range (to be fixed in runtime).
  // It is always present and instruction fetches are allowed.
  //
  {{0x00000000, 0x00000000},TRUE,FALSE},

  //
  // SMM profile data range( to be fixed in runtime).
  // It is always present and instruction fetches are not allowed.
  //
  {{0x00000000, 0x00000000},TRUE,TRUE},
  
  //
  // Future exended range could be added here.
  //

  //
  // PCI MMIO ranges (to be added in runtime).
  // They are always present and instruction fetches are not allowed.
  //
};

//
// These memory ranges are mapped by 4KB-page instead of 2MB-page.
//
MEMORY_RANGE              *mSplitMemRange          = NULL;
UINTN                     mSplitMemRangeCount      = 0;

//
// SMI command port.
//
UINT32                    mSmiCommandPort;

/**
  Disable branch trace store.
  
**/
VOID
DisableBTS (
  VOID
  )
{
  AsmMsrAnd64 (MSR_DEBUG_CTL, ~((UINT64)(MSR_DEBUG_CTL_BTS | MSR_DEBUG_CTL_TR)));
}

/**
  Enable branch trace store.
  
**/
VOID
EnableBTS (
  VOID
  )
{
  AsmMsrOr64 (MSR_DEBUG_CTL, (MSR_DEBUG_CTL_BTS | MSR_DEBUG_CTL_TR));
}

/**
  Get CPU Index from Apic ID.
  
**/
UINTN
GetCpuIndex (
  VOID
  )
{
  UINTN     Index;
  UINT32    ApicId;

  ApicId = GetApicId ();

  for (Index = 0; Index < FixedPcdGet32 (PcdCpuMaxLogicalProcessorNumber); Index++) {
    if (gSmmCpuPrivate->ProcessorInfo[Index].ProcessorId == ApicId) {
      return Index;
    }
  }
  ASSERT (FALSE);
  return 0;
}

/**
  Get the source of IP after execute-disable exception is triggered.
  
  @param  CpuIndex        The index of CPU.
  @param  DestinationIP   The destination address.
  
**/
UINT64
GetSourceFromDestinationOnBts (
  UINTN  CpuIndex,
  UINT64 DestinationIP
  )
{
  BRANCH_TRACE_RECORD  *CurrentBTSRecord;
  UINTN                Index;
  BOOLEAN              FirstMatch;
  
  FirstMatch = FALSE;

  CurrentBTSRecord = (BRANCH_TRACE_RECORD *)mMsrDsArea[CpuIndex]->BTSIndex;
  for (Index = 0; Index < mBTSRecordNumber; Index++) {
    if ((UINTN)CurrentBTSRecord < (UINTN)mMsrBTSRecord[CpuIndex]) {
      //
      // Underflow
      //
      CurrentBTSRecord = (BRANCH_TRACE_RECORD *)((UINTN)mMsrDsArea[CpuIndex]->BTSAbsoluteMaximum - 1);
      CurrentBTSRecord --;
    }
    if (CurrentBTSRecord->LastBranchTo == DestinationIP) {
      //
      // Good! find 1st one, then find 2nd one.
      //
      if (!FirstMatch) {
        //
        // The first one is DEBUG exception
        //
        FirstMatch = TRUE;
      } else {
        //
        // Good find proper one.
        //
        return CurrentBTSRecord->LastBranchFrom;
      }
    }
    CurrentBTSRecord--;
  }

  return 0;
}

/**
  SMM profile specific INT 1 (single-step) exception handler.

  @param  InterruptType    Defines the type of interrupt or exception that
                           occurred on the processor.This parameter is processor architecture specific.
  @param  SystemContext    A pointer to the processor context when
                           the interrupt occurred on the processor.
**/
VOID
EFIAPI
DebugExceptionHandler (
    IN EFI_EXCEPTION_TYPE   InterruptType,
    IN EFI_SYSTEM_CONTEXT   SystemContext
  )
{
  UINTN  CpuIndex;
  UINTN  PFEntry;

  if (!mSmmProfileStart) {
    return;
  }
  CpuIndex = GetCpuIndex ();

  //
  // Clear last PF entries
  //
  for (PFEntry = 0; PFEntry < mPFEntryCount[CpuIndex]; PFEntry++) {
    *mLastPFEntryPointer[CpuIndex][PFEntry] = mLastPFEntryValue[CpuIndex][PFEntry];
  }
  
  //
  // Reset page fault exception count for next page fault.
  //
  mPFEntryCount[CpuIndex] = 0;
  
  //
  // Flush TLB
  //
  CpuFlushTlb ();
}

/**
  Check if the memory address will be mapped by 4KB-page.
  
  @param  Address  The address of Memory.
  @param  Nx       The flag indicates if the memory is execute-disable.
  
**/
BOOLEAN
IsAddressValid (
  IN EFI_PHYSICAL_ADDRESS   Address,
  IN BOOLEAN                *Nx
  )
{
  UINTN  Index;

  //
  // Check config
  //
  for (Index = 0; Index < mProtectionMemRangeCount; Index++) {
    if ((Address >= mProtectionMemRange[Index].Range.Base) && (Address < mProtectionMemRange[Index].Range.Top)) {
      *Nx = mProtectionMemRange[Index].Nx;
      return mProtectionMemRange[Index].Present;
    }
  }

  //
  // Return default
  //
  *Nx = FALSE;
  return FALSE;
}

/**
  Check if the memory address will be mapped by 4KB-page.
  
  @param  Address  The address of Memory.
  
**/
BOOLEAN
IsAddressSplit (
  IN EFI_PHYSICAL_ADDRESS   Address
  )
{
  UINTN  Index;

  //
  // Check config
  //
  for (Index = 0; Index < mSplitMemRangeCount; Index++) {
    if ((Address >= mSplitMemRange[Index].Base) && (Address < mSplitMemRange[Index].Top)) {
      return TRUE;
    }
  }

  //
  // Return default
  //
  return FALSE;
}

/**
  Initialize the protected memory ranges and the 4KB-page mapped memory ranges.

**/
VOID
InitProtectedMemRange (
  VOID
  )
{
  UINTN                            Index;
  UINTN                            NumberOfDescriptors;
  UINTN                            NumberOfMmioDescriptors;
  UINTN                            NumberOfProtectRange;
  UINTN                            NumberOfSpliteRange;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *MemorySpaceMap;
  UINTN                            TotalSize;
  EFI_STATUS                       Status;
  EFI_PHYSICAL_ADDRESS             ProtectBaseAddress;
  EFI_PHYSICAL_ADDRESS             ProtectEndAddress;
  EFI_PHYSICAL_ADDRESS             Top2MBAlignedAddress;
  EFI_PHYSICAL_ADDRESS             Base2MBAlignedAddress;
  UINT64                           High4KBPageSize;
  UINT64                           Low4KBPageSize;

  NumberOfDescriptors      = 0;
  NumberOfMmioDescriptors  = 0;
  NumberOfSpliteRange      = 0;
  MemorySpaceMap           = NULL;

  //
  // Get MMIO ranges from GCD and add them into protected memory ranges.
  //
  Status = gDS->GetMemorySpaceMap (
                &NumberOfDescriptors,
                &MemorySpaceMap
                );  
  for (Index = 0; Index < NumberOfDescriptors; Index++) {
    if (MemorySpaceMap[Index].GcdMemoryType == EfiGcdMemoryTypeMemoryMappedIo) {
      NumberOfMmioDescriptors++;
    }
  }
  
  if (NumberOfMmioDescriptors != 0) {
    TotalSize = NumberOfMmioDescriptors * sizeof (MEMORY_PROTECTION_RANGE) + sizeof (mProtectionMemRangeTemplate);
    mProtectionMemRange = (MEMORY_PROTECTION_RANGE *) AllocateZeroPool (TotalSize);
    ASSERT (mProtectionMemRange != NULL);
    mProtectionMemRangeCount = TotalSize / sizeof (MEMORY_PROTECTION_RANGE);
    
    //
    // Copy existing ranges.
    //
    CopyMem (mProtectionMemRange, mProtectionMemRangeTemplate, sizeof (mProtectionMemRangeTemplate));

    //
    // Create split ranges which come from protected ranges.
    //
    TotalSize = (TotalSize / sizeof (MEMORY_PROTECTION_RANGE)) * sizeof (MEMORY_RANGE);
    mSplitMemRange = (MEMORY_RANGE *) AllocateZeroPool (TotalSize);
    ASSERT (mSplitMemRange != NULL);
    
    //
    // Create MMIO ranges which are set to present and execution-disable.
    //
    NumberOfProtectRange    = sizeof (mProtectionMemRangeTemplate) / sizeof (MEMORY_PROTECTION_RANGE);
    for (Index = 0; Index < NumberOfDescriptors; Index++) {
      if (MemorySpaceMap[Index].GcdMemoryType != EfiGcdMemoryTypeMemoryMappedIo) {
        continue;
      }
      mProtectionMemRange[NumberOfProtectRange].Range.Base = MemorySpaceMap[Index].BaseAddress;
      mProtectionMemRange[NumberOfProtectRange].Range.Top  = MemorySpaceMap[Index].BaseAddress + MemorySpaceMap[Index].Length;
      mProtectionMemRange[NumberOfProtectRange].Present    = TRUE;
      mProtectionMemRange[NumberOfProtectRange].Nx         = TRUE;
      NumberOfProtectRange++;
    }
  }
    
  //
  // According to protected ranges, create the ranges which will be mapped by 2KB page.
  //
  NumberOfSpliteRange  = 0;
  NumberOfProtectRange = mProtectionMemRangeCount;
  for (Index = 0; Index < NumberOfProtectRange; Index++) {
    //
    // If MMIO base address is not 2MB alignment, make 2MB alignment for create 4KB page in page table.
    //
    ProtectBaseAddress = mProtectionMemRange[Index].Range.Base;
    ProtectEndAddress  = mProtectionMemRange[Index].Range.Top;
    if (((ProtectBaseAddress & (SIZE_2MB - 1)) != 0) || ((ProtectEndAddress  & (SIZE_2MB - 1)) != 0)) {
      //
      // Check if it is possible to create 4KB-page for not 2MB-aligned range and to create 2MB-page for 2MB-aligned range.
      // A mix of 4KB and 2MB page could save SMRAM space.
      //
      Top2MBAlignedAddress  = ProtectEndAddress & ~(SIZE_2MB - 1);
      Base2MBAlignedAddress = (ProtectBaseAddress + SIZE_2MB - 1) & ~(SIZE_2MB - 1);
      if ((Top2MBAlignedAddress > Base2MBAlignedAddress) && 
          ((Top2MBAlignedAddress - Base2MBAlignedAddress) >= SIZE_2MB)) {
        //
        // There is an range which could be mapped by 2MB-page.
        //
        High4KBPageSize = ((ProtectEndAddress + SIZE_2MB - 1) & ~(SIZE_2MB - 1)) - (ProtectEndAddress & ~(SIZE_2MB - 1));
        Low4KBPageSize  = ((ProtectBaseAddress + SIZE_2MB - 1) & ~(SIZE_2MB - 1)) - (ProtectBaseAddress & ~(SIZE_2MB - 1));
        if (High4KBPageSize != 0) {
          //
          // Add not 2MB-aligned range to be mapped by 4KB-page.
          //
          mSplitMemRange[NumberOfSpliteRange].Base = ProtectEndAddress & ~(SIZE_2MB - 1);
          mSplitMemRange[NumberOfSpliteRange].Top  = (ProtectEndAddress + SIZE_2MB - 1) & ~(SIZE_2MB - 1);
          NumberOfSpliteRange++;
        }
        if (Low4KBPageSize != 0) {
          //
          // Add not 2MB-aligned range to be mapped by 4KB-page.
          //
          mSplitMemRange[NumberOfSpliteRange].Base = ProtectBaseAddress & ~(SIZE_2MB - 1);
          mSplitMemRange[NumberOfSpliteRange].Top  = (ProtectBaseAddress + SIZE_2MB - 1) & ~(SIZE_2MB - 1);
          NumberOfSpliteRange++;
        }
      } else {
        //
        // The range could only be mapped by 4KB-page.
        //
        mSplitMemRange[NumberOfSpliteRange].Base = ProtectBaseAddress & ~(SIZE_2MB - 1);
        mSplitMemRange[NumberOfSpliteRange].Top  = (ProtectEndAddress + SIZE_2MB - 1) & ~(SIZE_2MB - 1);
        NumberOfSpliteRange++;
      }
    }
  }
  
  mSplitMemRangeCount = NumberOfSpliteRange;
  
  DEBUG ((EFI_D_INFO, "SMM Profile Memory Ranges:\n"));
  for (Index = 0; Index < mProtectionMemRangeCount; Index++) {
    DEBUG ((EFI_D_INFO, "mProtectionMemRange[%d].Base = %lx\n", Index, mProtectionMemRange[Index].Range.Base));
    DEBUG ((EFI_D_INFO, "mProtectionMemRange[%d].Top  = %lx\n", Index, mProtectionMemRange[Index].Range.Top));
  }
  for (Index = 0; Index < mSplitMemRangeCount; Index++) {
    DEBUG ((EFI_D_INFO, "mSplitMemRange[%d].Base = %lx\n", Index, mSplitMemRange[Index].Base));
    DEBUG ((EFI_D_INFO, "mSplitMemRange[%d].Top  = %lx\n", Index, mSplitMemRange[Index].Top));
  }
}

/**
  Update page table according to protected memory ranges and the 4KB-page mapped memory ranges.

**/
VOID
InitPaging (
  VOID
  )
{
  UINT64                            *Pml4;
  UINT64                            *Pde;
  UINT64                            *Pte;
  UINT64                            *Pt;
  UINTN                             Address;
  UINTN                             Level1;
  UINTN                             Level2;
  UINTN                             Level3;
  UINTN                             Level4;
  UINTN                             NumberOfPdpEntries;
  UINTN                             NumberOfPml4Entries;
  UINTN                             SizeOfMemorySpace;
  BOOLEAN                           Nx;

  if (sizeof (UINTN) == sizeof (UINT64)) {
    Pml4 = (UINT64*)(UINTN)gSmiCr3;    
    SizeOfMemorySpace = HighBitSet64 (gPhyMask) + 1;
    //
    // Calculate the table entries of PML4E and PDPTE.
    //
    if (SizeOfMemorySpace <= 39 ) {
      NumberOfPml4Entries = 1;
      NumberOfPdpEntries = (UINT32)LShiftU64 (1, (SizeOfMemorySpace - 30));
    } else {
      NumberOfPml4Entries = (UINT32)LShiftU64 (1, (SizeOfMemorySpace - 39));
      NumberOfPdpEntries = 512;
    }
  } else {
    NumberOfPml4Entries = 1;
    NumberOfPdpEntries  = 4;
  }

  //
  // Go through page table and change 2MB-page into 4KB-page.
  //
  for (Level1 = 0; Level1 < NumberOfPml4Entries; Level1++) {
    if (sizeof (UINTN) == sizeof (UINT64)) {
      Pde = (UINT64 *)(UINTN)(Pml4[Level1] & PHYSICAL_ADDRESS_MASK);
    } else {
      Pde = (UINT64*)(UINTN)gSmiCr3;
    }
    for (Level2 = 0; Level2 < NumberOfPdpEntries; Level2++, Pde++) {
      Pte = (UINT64 *)(UINTN)(*Pde & PHYSICAL_ADDRESS_MASK);
      if (Pte == 0) {
        continue;
      }      
      for (Level3 = 0; Level3 < SIZE_4KB / sizeof (*Pte); Level3++, Pte++) {
        Address = (((Level2 << 9) + Level3) << 21);
      
        //
        // if it is 2M page, check IsAddressSplit()
        //
        if (((*Pte & IA32_PG_PS) != 0) && IsAddressSplit (Address)) {
          //
          // Based on current page table, create 4KB page table for split area.
          //
          ASSERT (Address == (*Pte & PHYSICAL_ADDRESS_MASK));
      
          Pt = AllocatePages (1);
          ASSERT (Pt != NULL);
      
          *Pte = (UINTN)Pt | IA32_PG_RW | IA32_PG_P;
      
          // Split it
          for (Level4 = 0; Level4 < SIZE_4KB / sizeof(*Pt); Level4++, Pt++) {
            *Pt = Address + ((Level4 << 12) | IA32_PG_RW | IA32_PG_P);
          } // end for PT
        } // end if IsAddressSplit
      } // end for PTE
    } // end for PDE
  }

  //
  // Go through page table and set several page table entries to absent or execute-disable.
  //
  DEBUG ((EFI_D_INFO, "Patch page table start ...\n"));
  for (Level1 = 0; Level1 < NumberOfPml4Entries; Level1++) {
    if (sizeof (UINTN) == sizeof (UINT64)) {
      Pde = (UINT64 *)(UINTN)(Pml4[Level1] & PHYSICAL_ADDRESS_MASK);
    } else {
      Pde = (UINT64*)(UINTN)gSmiCr3;
    }
    for (Level2 = 0; Level2 < NumberOfPdpEntries; Level2++, Pde++) {
      Pte = (UINT64 *)(UINTN)(*Pde & PHYSICAL_ADDRESS_MASK);
      if (Pte == 0) {
        continue;
      }
      for (Level3 = 0; Level3 < SIZE_4KB / sizeof (*Pte); Level3++, Pte++) {
        Address = (((Level2 << 9) + Level3) << 21);
      
        if ((*Pte & IA32_PG_PS) != 0) {
          // 2MB page
      
          if (!IsAddressValid (Address, &Nx)) {
            //
            // Patch to remove present flag and rw flag
            //
            *Pte = *Pte & (INTN)(INT32)(~(IA32_PG_RW | IA32_PG_P));
          }
          if (Nx && mXdSupported) {
            *Pte = *Pte | IA32_PG_NX;
          }
        } else {
          // 4KB page
          Pt = (UINT64 *)(UINTN)(*Pte & PHYSICAL_ADDRESS_MASK);
          if (Pt == 0) {
            continue;
          }
          for (Level4 = 0; Level4 < SIZE_4KB / sizeof(*Pt); Level4++, Pt++) {
            if (!IsAddressValid (Address, &Nx)) {
              *Pt = *Pt & (INTN)(INT32)(~(IA32_PG_RW | IA32_PG_P));
            }
            if (Nx && mXdSupported) {
              *Pt = *Pt | IA32_PG_NX;
            }
            Address += SIZE_4KB;
          } // end for PT
        } // end if PS
      } // end for PTE
    } // end for PDE
  }

  //
  // Flush TLB
  //
  CpuFlushTlb ();
  DEBUG ((EFI_D_INFO, "Patch page table done!\n"));

  return ;
}

/**
  To find Fadt in Acpi tables.

  @param AcpiTableGuid   The guid used to find ACPI table in UEFI ConfigurationTable.
  
  @return  Fadt table pointer.
**/
EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE  *
FindAcpiFadtTableByAcpiGuid (
  IN EFI_GUID  *AcpiTableGuid
  )
{
  EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER  *Rsdp;
  EFI_ACPI_DESCRIPTION_HEADER                   *Rsdt;
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE     *Fadt;
  UINTN                                         Index;
  UINT32                                        Data32;
  Rsdp  = NULL;
  Rsdt  = NULL;
  Fadt  = NULL;
  //
  // found ACPI table RSD_PTR from system table
  //
  for (Index = 0; Index < gST->NumberOfTableEntries; Index++) {
    if (CompareGuid (&(gST->ConfigurationTable[Index].VendorGuid), AcpiTableGuid)) {
      //
      // A match was found.
      //
      Rsdp = gST->ConfigurationTable[Index].VendorTable;
      break;
    }
  }

  if (Rsdp == NULL) {
    return NULL;
  }

  Rsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN) Rsdp->RsdtAddress;
  if (Rsdt == NULL || Rsdt->Signature != EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_TABLE_SIGNATURE) {
    return NULL;
  }

  for (Index = sizeof (EFI_ACPI_DESCRIPTION_HEADER); Index < Rsdt->Length; Index = Index + sizeof (UINT32)) {

    Data32  = *(UINT32 *) ((UINT8 *) Rsdt + Index);
    Fadt    = (EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *) (UINT32 *) (UINTN) Data32;
    if (Fadt->Header.Signature == EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE) {
      break;
    }
  }

  if (Fadt == NULL || Fadt->Header.Signature != EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE) {
    return NULL;
  }

  return Fadt;
}

/**
  To find Fadt in Acpi tables.
  
  @return  Fadt table pointer.
**/
EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE  *
FindAcpiFadtTable (
  VOID
  )
{
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *Fadt;

  Fadt = FindAcpiFadtTableByAcpiGuid (&gEfiAcpi20TableGuid);
  if (Fadt != NULL) {
    return Fadt;
  }

  return FindAcpiFadtTableByAcpiGuid (&gEfiAcpi10TableGuid);
}

/**
  To get system port address of the SMI Command Port in FADT table.

**/
VOID
GetSmiCommandPort (
  VOID
  )
{
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *Fadt;
  
  Fadt = FindAcpiFadtTable ();
  ASSERT (Fadt != NULL);
  
  mSmiCommandPort = Fadt->SmiCmd;
  DEBUG ((EFI_D_ERROR, "mSmiCommandPort = %x\n", mSmiCommandPort));
}

/**
  The SmmReadyToLock protocol callback function updates page table
  to make some memory ranges (like system memory) absent and make 
  some memory ranges (like MMIO) present and execute disable. It also
  update 2MB-page to 4KB-page for some memory ranges.

  @param  Protocol   Points to the protocol's unique identifier.
  @param  Interface  Points to the interface instance.
  @param  Handle     The handle on which the interface was installed.

  @retval EFI_SUCCESS SmmReadyToLock protocol callback runs successfully.
**/
EFI_STATUS
EFIAPI
SmmProfileStartCallBack (
  IN CONST EFI_GUID  *Protocol,
  IN VOID            *Interface,
  IN EFI_HANDLE      Handle
  )
{
  EFI_STATUS         Status;

  //
  // Save to variable so that SMM profile data can be found.
  //
  Status = gRT->SetVariable (
                  SMM_PROFILE_NAME,
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof(mSmmProfileBase),
                  &mSmmProfileBase
                  );
  ASSERT_EFI_ERROR (Status);

  //
  // Get Software SMI from Fadt
  //
  GetSmiCommandPort ();
  
  //
  // Init protected mem range for patching page table below.
  //
  InitProtectedMemRange ();

  //
  // Create a mix of 2MB and 4KB page table. Update some memory ranges absent and execute-disable.
  //
  InitPaging ();
  
  //
  // The flag indicates SMM profile starts to work.
  //
  mSmmProfileStart = TRUE;

  return EFI_SUCCESS;
}

/**
  Initialize SMM profile data structures.

**/
VOID
InitSmmProfileInternal (
  VOID
  )
{
  EFI_STATUS                 Status;
  EFI_PHYSICAL_ADDRESS       Base;
  VOID                       *Registration;
  UINTN                      Index;
  UINTN                      MsrDsAreaSizePerCpu;
  UINTN                      TotalSize;

  //
  // Allocate memory for SmmProfile below 4GB.
  // The base address 
  //
  ASSERT ((mSmmProfileSize & 0xFFF) == 0);
  
  if (mBtsSupported) {
    TotalSize = mSmmProfileSize + mMsrDsAreaSize;
  } else {
    TotalSize = mSmmProfileSize;
  }

  Base = 0xFFFFFFFF;
  Status = gBS->AllocatePages (
                  AllocateMaxAddress,
                  EfiReservedMemoryType,
                  EFI_SIZE_TO_PAGES (TotalSize),
                  &Base
                  );
  ASSERT_EFI_ERROR (Status);
  ZeroMem ((VOID *)(UINTN)Base, TotalSize);
  mSmmProfileBase = (SMM_PROFILE_HEADER *)(UINTN)Base;
  
  //
  // Initialize SMM profile data header.
  //
  mSmmProfileBase->HeaderSize     = sizeof (SMM_PROFILE_HEADER);
  mSmmProfileBase->MaxDataEntries = (UINT64)((mSmmProfileSize - sizeof(SMM_PROFILE_HEADER)) / sizeof (SMM_PROFILE_ENTRY));
  mSmmProfileBase->MaxDataSize    = MultU64x64 (mSmmProfileBase->MaxDataEntries, sizeof(SMM_PROFILE_ENTRY));
  mSmmProfileBase->CurDataEntries = 0;
  mSmmProfileBase->CurDataSize    = 0;
  mSmmProfileBase->TsegStart      = mCpuHotPlugData.SmrrBase;
  mSmmProfileBase->TsegSize       = mCpuHotPlugData.SmrrSize;
  mSmmProfileBase->NumSmis        = 0;
  mSmmProfileBase->NumCpus        = gSmmCpuPrivate->SmmCoreEntryContext.NumberOfCpus;

  if (mBtsSupported) {
    mMsrDsAreaBase  = (MSR_DS_AREA_STRUCT *)((UINTN)Base + mSmmProfileSize);
    MsrDsAreaSizePerCpu = mMsrDsAreaSize / FixedPcdGet32 (PcdCpuMaxLogicalProcessorNumber);
    mBTSRecordNumber    = (MsrDsAreaSizePerCpu - sizeof(PEBS_RECORD) * PEBS_RECORD_NUMBER - sizeof(MSR_DS_AREA_STRUCT)) / sizeof(BRANCH_TRACE_RECORD);
    for (Index = 0; Index < FixedPcdGet32 (PcdCpuMaxLogicalProcessorNumber); Index++) {
      mMsrDsArea[Index]     = (MSR_DS_AREA_STRUCT *)((UINTN)mMsrDsAreaBase + MsrDsAreaSizePerCpu * Index);
      mMsrBTSRecord[Index]  = (BRANCH_TRACE_RECORD *)((UINTN)mMsrDsArea[Index] + sizeof(MSR_DS_AREA_STRUCT));
      mMsrPEBSRecord[Index] = (PEBS_RECORD *)((UINTN)mMsrDsArea[Index] + MsrDsAreaSizePerCpu - sizeof(PEBS_RECORD) * PEBS_RECORD_NUMBER);
    
      mMsrDsArea[Index]->BTSBufferBase          = (UINTN)mMsrBTSRecord[Index];
      mMsrDsArea[Index]->BTSIndex               = mMsrDsArea[Index]->BTSBufferBase;
      mMsrDsArea[Index]->BTSAbsoluteMaximum     = mMsrDsArea[Index]->BTSBufferBase + mBTSRecordNumber * sizeof(BRANCH_TRACE_RECORD) + 1;
      mMsrDsArea[Index]->BTSInterruptThreshold  = mMsrDsArea[Index]->BTSAbsoluteMaximum + 1;
    
      mMsrDsArea[Index]->PEBSBufferBase         = (UINTN)mMsrPEBSRecord[Index];
      mMsrDsArea[Index]->PEBSIndex              = mMsrDsArea[Index]->PEBSBufferBase;
      mMsrDsArea[Index]->PEBSAbsoluteMaximum    = mMsrDsArea[Index]->PEBSBufferBase + PEBS_RECORD_NUMBER * sizeof(PEBS_RECORD) + 1;
      mMsrDsArea[Index]->PEBSInterruptThreshold = mMsrDsArea[Index]->PEBSAbsoluteMaximum + 1;
    }
  }

  mProtectionMemRange      = mProtectionMemRangeTemplate;
  mProtectionMemRangeCount = sizeof (mProtectionMemRangeTemplate) / sizeof (MEMORY_PROTECTION_RANGE);

  //
  // Update TSeg entry.
  //
  mProtectionMemRange[0].Range.Base = mCpuHotPlugData.SmrrBase;
  mProtectionMemRange[0].Range.Top  = mCpuHotPlugData.SmrrBase + mCpuHotPlugData.SmrrSize;
  
  //
  // Update SMM profile entry.
  //
  mProtectionMemRange[1].Range.Base = (EFI_PHYSICAL_ADDRESS)(UINTN)mSmmProfileBase;
  mProtectionMemRange[1].Range.Top  = (EFI_PHYSICAL_ADDRESS)(UINTN)mSmmProfileBase + TotalSize;

  //
  // Allocate memory reserved for creating 4KB pages.
  //
  InitPagesForPFHandler ();

  //
  // Start SMM profile when SmmReadyToLock protocol is installed.
  //
  Status = gSmst->SmmRegisterProtocolNotify (
                    &gEfiSmmReadyToLockProtocolGuid,
                    SmmProfileStartCallBack,
                    &Registration
                    );
  ASSERT_EFI_ERROR (Status);

  return ;
}

/**
  Check if XD feature is supported by a processor.

**/
VOID
CheckFeatureSupported (
  VOID
  )
{
  UINT32                 RegEax;
  UINT32                 RegEdx;
  
  if (mXdSupported) {
    AsmCpuid (EFI_CPUID_EXTENDED_FUNCTION, &RegEax, NULL, NULL, NULL);
    if (RegEax <= EFI_CPUID_EXTENDED_FUNCTION) {
      //
      // Entended CPUID functions are not supported on this processor.
      //
      mXdSupported = FALSE;
    }
    
    AsmCpuid (EFI_CPUID_EXTENDED_CPU_SIG, NULL, NULL, NULL, &RegEdx);
    if ((RegEdx & CPUID1_EDX_XD_SUPPORT) == 0) {
      //
      // Execute Disable Bit feature is not supported on this processor.
      //
      mXdSupported = FALSE;
    }
  }

  if (mBtsSupported) {
    AsmCpuid (EFI_CPUID_VERSION_INFO, NULL, NULL, NULL, &RegEdx);
    if ((RegEdx & CPUID1_EDX_BTS_AVAILABLE) != 0) {
      //
      // Per IA32 manuals:
      // When CPUID.1:EDX[21] is set, the following BTS facilities are available:
      // 1. The BTS_UNAVAILABLE flag in the IA32_MISC_ENABLE MSR indicates the 
      //    availability of the BTS facilities, including the ability to set the BTS and
      //    BTINT bits in the MSR_DEBUGCTLA MSR.
      // 2. The IA32_DS_AREA MSR can be programmed to point to the DS save area.  
      //
      if ((AsmMsrBitFieldRead64 (MSR_IA32_MISC_ENABLE, 11, 11) == 0) &&
          (AsmMsrBitFieldRead64 (MSR_IA32_MISC_ENABLE, 12, 12) == 0)) {
        //
        // BTS facilities is supported.
        //
        mBtsSupported = FALSE;
      }
    }
  }
}

/**
  Check if XD and BTS features are supported by all processors.
  
  @param  MpServices   The MP services protocol.

**/
VOID
CheckProcessorFeature (
  IN EFI_MP_SERVICES_PROTOCOL  *MpServices
  )
{
  mXdSupported  = TRUE;
  mBtsSupported = TRUE;

  //
  // Check if XD and BTS are supported on all processors.
  //
  CheckFeatureSupported ();

  //
  //Check on other processors if BSP supports this
  //
  if (mXdSupported || mBtsSupported) {
    MpServices->StartupAllAPs (
                  MpServices,
                  (EFI_AP_PROCEDURE) CheckFeatureSupported,
                  TRUE,
                  NULL,
                  0,
                  NULL,
                  NULL
                  );
  }
}

/**
  Enable XD feature.

**/
VOID
ActivateXd (
  VOID
  )
{
  UINT64           MsrRegisters;    

  MsrRegisters = AsmReadMsr64 (MSR_EFER);
  if ((MsrRegisters & MSR_EFER_XD) != 0) {
    return ;
  }
  MsrRegisters |= MSR_EFER_XD;
  AsmWriteMsr64 (MSR_EFER, MsrRegisters);
}

/**
  Enable single step.

**/
VOID
ActivateSingleStepDB (
  VOID
  )
{
  UINTN    Dr6;

  Dr6 = AsmReadDr6 ();
  if ((Dr6 & DR6_SINGLE_STEP) != 0) {
    return;
  }
  Dr6 |= DR6_SINGLE_STEP;
  AsmWriteDr6 (Dr6);
}

/**
  Enable last branch.

**/
VOID
ActivateLBR (
  VOID
  )
{
  UINT64  DebugCtl;

  DebugCtl = AsmReadMsr64 (MSR_DEBUG_CTL);
  if ((DebugCtl & MSR_DEBUG_CTL_LBR) != 0) {
    return ;
  }
  AsmWriteMsr64 (MSR_LER_FROM_LIP, 0);
  AsmWriteMsr64 (MSR_LER_TO_LIP, 0);
  DebugCtl |= MSR_DEBUG_CTL_LBR;
  AsmWriteMsr64 (MSR_DEBUG_CTL, DebugCtl);
}

/**
  Enable branch trace store.

  @param  CpuIndex  The index of the processor.

**/
VOID
ActivateBTS (
  IN      UINTN                     CpuIndex
  )
{
  UINT64  DebugCtl;

  DebugCtl = AsmReadMsr64 (MSR_DEBUG_CTL);
  if ((DebugCtl & MSR_DEBUG_CTL_BTS) != 0) {
    return ;
  }

  AsmWriteMsr64 (MSR_DS_AREA, (UINT64)(UINTN)mMsrDsArea[CpuIndex]);
  DebugCtl |= (MSR_DEBUG_CTL_BTS | MSR_DEBUG_CTL_TR);
  DebugCtl &= ~MSR_DEBUG_CTL_BTINT;
  AsmWriteMsr64 (MSR_DEBUG_CTL, DebugCtl);
}

/**
  Increase SMI number in each SMI entry.

**/
VOID
SmmProfileRecordSmiNum (
  VOID
  )
{
  if (mSmmProfileStart) {
    mSmmProfileBase->NumSmis++;
  }
}

/**
  Initialize processor environment for SMM profile.

  @param  CpuIndex  The index of the processor.

**/
VOID
ActivateSmmProfile (
  IN UINTN CpuIndex
  )
{
  //
  // Try to enable NX
  //
  if (mXdSupported) {
    ActivateXd ();
  }

  //
  // Enable Single Step DB#
  //
  ActivateSingleStepDB ();
  
  if (mBtsSupported) {
    //
    // We can not get useful information from LER, so we have to use BTS.
    //
    ActivateLBR ();
    
    //
    // Enable BTS
    //
    ActivateBTS (CpuIndex);
  }
}

/**
  Initialize SMM profile in SMM CPU entrypoint.

**/
VOID
InitSmmProfile (
  VOID
  )
{
  EFI_STATUS                        Status;
  EFI_MP_SERVICES_PROTOCOL          *MpServices;

  Status = gBS->LocateProtocol (&gEfiMpServiceProtocolGuid, NULL, (VOID **)&MpServices);
  ASSERT_EFI_ERROR (Status);

  //
  // First detect if XD and BTS are supported
  //
  CheckProcessorFeature (MpServices);

  //
  // Init SmmProfile here
  //
  InitSmmProfileInternal ();

  //
  // Init profile IDT.
  //
  InitIdtr ();

  //
  // Patch SmmS3ResumeState->SmmS3Cr3
  //
  InitSmmS3Cr3 ();
}

/**
  Update page table to map the memory correctly in order to make the instruction 
  which caused page fault execute successfully. And it also save the original page
  table to be restored in single-step exception.

  @param  PageTable           PageTable Address.
  @param  PFAddress           The memory address which caused page fault exception.
  @param  CpuIndex            The index of the processor.
  @param  ErrorCode           The Error code of exception.

**/
VOID
RestorePageTableBelow4G (
  UINT64        *PageTable,
  UINT64        PFAddress,
  UINTN         CpuIndex,
  UINTN         ErrorCode
  )
{
  UINTN         PTIndex;
  UINTN         PFIndex;
    
  //
  // PML4
  //
  if (sizeof(UINT64) == sizeof(UINTN)) {
    PTIndex = (UINTN)BitFieldRead64 (PFAddress, 39, 47);
    ASSERT (PageTable[PTIndex] != 0);
    PageTable = (UINT64*)(UINTN)(PageTable[PTIndex] & PHYSICAL_ADDRESS_MASK);
  }
  
  //
  // PDPTE
  //
  PTIndex = (UINTN)BitFieldRead64 (PFAddress, 30, 38);
  ASSERT (PageTable[PTIndex] != 0);
  PageTable = (UINT64*)(UINTN)(PageTable[PTIndex] & PHYSICAL_ADDRESS_MASK);
  
  //
  // PD
  //
  PTIndex = (UINTN)BitFieldRead64 (PFAddress, 21, 29);
  if ((PageTable[PTIndex] & IA32_PG_PS) != 0) {
    //
    // Large page 
    //
  
    //
    // Record old entries with non-present status
    // Old entries include the memory which instruction is at and the memory which instruction access.
    // 
    //
    ASSERT (mPFEntryCount[CpuIndex] < MAX_PF_ENTRY_COUNT);
    if (mPFEntryCount[CpuIndex] < MAX_PF_ENTRY_COUNT) {
      PFIndex = mPFEntryCount[CpuIndex];
      mLastPFEntryValue[CpuIndex][PFIndex]   = PageTable[PTIndex];
      mLastPFEntryPointer[CpuIndex][PFIndex] = &PageTable[PTIndex];
      mPFEntryCount[CpuIndex]++;
    }
  
    //
    // Set new entry
    //
    PageTable[PTIndex] = (PFAddress & ~((1ull << 21) - 1));
    PageTable[PTIndex] |= IA32_PG_PS;
    PageTable[PTIndex] |= IA32_PG_RW | IA32_PG_P;
    if ((ErrorCode & IA32_PF_EC_ID) != 0) {
      PageTable[PTIndex] &= ~IA32_PG_NX;
    }
  } else {
    //
    // Small page
    //
    ASSERT (PageTable[PTIndex] != 0);
    PageTable = (UINT64*)(UINTN)(PageTable[PTIndex] & PHYSICAL_ADDRESS_MASK);
  
    //
    // 4K PTE
    //
    PTIndex = (UINTN)BitFieldRead64 (PFAddress, 12, 20);

    //
    // Record old entries with non-present status
    // Old entries include the memory which instruction is at and the memory which instruction access.
    // 
    //
    ASSERT (mPFEntryCount[CpuIndex] < MAX_PF_ENTRY_COUNT);
    if (mPFEntryCount[CpuIndex] < MAX_PF_ENTRY_COUNT) {
      PFIndex = mPFEntryCount[CpuIndex];
      mLastPFEntryValue[CpuIndex][PFIndex]   = PageTable[PTIndex];
      mLastPFEntryPointer[CpuIndex][PFIndex] = &PageTable[PTIndex];
      mPFEntryCount[CpuIndex]++;
    }
  
    //
    // Set new entry
    //
    PageTable[PTIndex] = (PFAddress & ~((1ull << 12) - 1));
    PageTable[PTIndex] |= IA32_PG_RW | IA32_PG_P;
    if ((ErrorCode & IA32_PF_EC_ID) != 0) {
      PageTable[PTIndex] &= ~IA32_PG_NX;
    }
  }  
}

/**
  The Page fault handler to save SMM profile data.

  @param  Rip        The RIP when exception happens.
  @param  ErrorCode  The Error code of exception.

**/
VOID
SmmProfilePFHandler (
  UINTN Rip,
  UINTN ErrorCode
  )
{
  UINT64                *PageTable;
  UINT64                PFAddress;
  UINTN                 CpuIndex;
  UINTN                 Index;
  UINT64                InstructionAddress;
  UINTN                 MaxEntryNumber;
  UINTN                 CurrentEntryNumber;
  BOOLEAN               IsValidPFAddress;
  SMM_PROFILE_ENTRY     *SmmProfileEntry;
  UINT64                SmiCommand;
  EFI_STATUS            Status;
  UINTN                 SwSmiCpuIndex;
  UINT8                 SoftSmiValue;
  EFI_SMM_SAVE_STATE_IO_INFO    IoInfo;

  if (!mSmmProfileStart) {
    //
    // If SMM profile does not start, call original page fault handler. 
    //
    SmiDefaultPFHandler ();
    return;
  }

  if (mBtsSupported) {
    DisableBTS ();
  }

  IsValidPFAddress  = FALSE;
  PageTable         = (UINT64 *)AsmReadCr3 ();
  PFAddress         = AsmReadCr2 ();
  CpuIndex          = GetCpuIndex ();

  if (PFAddress <= 0xFFFFFFFF) {
    RestorePageTableBelow4G (PageTable, PFAddress, CpuIndex, ErrorCode);
  } else {
    RestorePageTableAbove4G (PageTable, PFAddress, CpuIndex, ErrorCode, &IsValidPFAddress);
  }

  if (!IsValidPFAddress) {
    InstructionAddress = Rip;
    if ((ErrorCode & IA32_PF_EC_ID) != 0 && (mBtsSupported)) {
      //
      // If it is instruction fetch failure, get the correct IP from BTS.
      //
      InstructionAddress = GetSourceFromDestinationOnBts (CpuIndex, Rip);
      if (InstructionAddress == 0) {
        //
        // It indicates the instruction which caused page fault is not a jump instruction,
        // set instruction address same as the page fault address.
        //
        InstructionAddress = PFAddress;
      }
    }

    //
    // Try to find which CPU trigger SWSMI
    //
    SwSmiCpuIndex = 0;
    //
    // Indicate it is not software SMI
    //
    SmiCommand    = 0xFFFFFFFFFFFFFFFFULL;
    for (Index = 0; Index < gSmst->NumberOfCpus; Index++) {
      Status = SmmReadSaveState(&mSmmCpu, sizeof(IoInfo), EFI_SMM_SAVE_STATE_REGISTER_IO, Index, &IoInfo);
      if (EFI_ERROR (Status)) {
        continue;
      }
      if (IoInfo.IoPort == mSmiCommandPort) {
        //
        // Great! Find it.
        //
        SwSmiCpuIndex = Index;
        //
        // A software SMI triggered by SMI command port has been found, get SmiCommand from SMI command port.
        //
        SoftSmiValue = IoRead8 (mSmiCommandPort);
        SmiCommand = (UINT64)SoftSmiValue;
        break;
      }
    }
    
    SmmProfileEntry = (SMM_PROFILE_ENTRY *)(UINTN)(mSmmProfileBase + 1);
    //
    // Check if there is already a same entry in profile data.
    //  
    for (Index = 0; Index < (UINTN) mSmmProfileBase->CurDataEntries; Index++) {
      if ((SmmProfileEntry[Index].ErrorCode   == (UINT64)ErrorCode) &&
          (SmmProfileEntry[Index].Address     == PFAddress) &&
          (SmmProfileEntry[Index].CpuNum      == (UINT64)CpuIndex) &&
          (SmmProfileEntry[Index].Instruction == InstructionAddress) &&
          (SmmProfileEntry[Index].SmiCmd      == SmiCommand)) {
        //
        // Same record exist, need not save again.
        //
        break;
      }
    }
    if (Index == mSmmProfileBase->CurDataEntries) {
      CurrentEntryNumber = (UINTN) mSmmProfileBase->CurDataEntries;
      MaxEntryNumber     = (UINTN) mSmmProfileBase->MaxDataEntries;
      if (FeaturePcdGet (PcdCpuSmmProfileRingBuffer)) {
        CurrentEntryNumber = CurrentEntryNumber % MaxEntryNumber;
      }
      if (CurrentEntryNumber < MaxEntryNumber) {
        //
        // Log the new entry
        //
        SmmProfileEntry[CurrentEntryNumber].SmiNum      = mSmmProfileBase->NumSmis;
        SmmProfileEntry[CurrentEntryNumber].ErrorCode   = (UINT64)ErrorCode;
        SmmProfileEntry[CurrentEntryNumber].ApicId      = (UINT64)GetApicId ();
        SmmProfileEntry[CurrentEntryNumber].CpuNum      = (UINT64)CpuIndex;
        SmmProfileEntry[CurrentEntryNumber].Address     = PFAddress;
        SmmProfileEntry[CurrentEntryNumber].Instruction = InstructionAddress;
        SmmProfileEntry[CurrentEntryNumber].SmiCmd      = SmiCommand;
        //
        // Update current entry index and data size in the header.
        //
        mSmmProfileBase->CurDataEntries++;
        mSmmProfileBase->CurDataSize = MultU64x64 (mSmmProfileBase->CurDataEntries, sizeof (SMM_PROFILE_ENTRY));
      }
    }
  }
  //
  // Flush TLB
  //
  CpuFlushTlb ();

  if (mBtsSupported) {
    EnableBTS ();
  }
}

/**
  Replace INT1 exception handler to restore page table to absent/execute-diable state
  in order to trigger page fault again to save SMM profile data..

**/
VOID
InitIdtr (
  VOID
  )
{
  SmmRegisterExceptionHandler (&mSmmCpuService, EXCEPT_IA32_DEBUG, DebugExceptionHandler);
}
