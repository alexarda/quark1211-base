/** @file

   Internal functions to operate Working Block Space.

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


#include "FaultTolerantWrite.h"

EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER mWorkingBlockHeader = {ZERO_GUID, 0, 0, 0, 0, {0, 0, 0}, 0};

/**
  Initialize a local work space header.

  Since Signature and WriteQueueSize have been known, Crc can be calculated out,
  then the work space header will be fixed.
**/
VOID
InitializeLocalWorkSpaceHeader (
  VOID
  )
{
  EFI_STATUS                              Status;

  //
  // Check signature with gEdkiiWorkingBlockSignatureGuid.
  //
  if (CompareGuid (&gEdkiiWorkingBlockSignatureGuid, &mWorkingBlockHeader.Signature)) {
    //
    // The local work space header has been initialized.
    //
    return;
  }

  SetMem (
    &mWorkingBlockHeader,
    sizeof (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER),
    FTW_ERASED_BYTE
    );

  //
  // Here using gEdkiiWorkingBlockSignatureGuid as the signature.
  //
  CopyMem (
    &mWorkingBlockHeader.Signature,
    &gEdkiiWorkingBlockSignatureGuid,
    sizeof (EFI_GUID)
    );
  mWorkingBlockHeader.WriteQueueSize = (UINT64) (PcdGet32 (PcdFlashNvStorageFtwWorkingSize) - sizeof (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER));

  //
  // Crc is calculated with all the fields except Crc and STATE, so leave them as FTW_ERASED_BYTE.
  //

  //
  // Calculate the Crc of woking block header
  //
  Status = gBS->CalculateCrc32 (
                  &mWorkingBlockHeader,
                  sizeof (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER),
                  &mWorkingBlockHeader.Crc
                  );
  ASSERT_EFI_ERROR (Status);

  mWorkingBlockHeader.WorkingBlockValid    = FTW_VALID_STATE;
  mWorkingBlockHeader.WorkingBlockInvalid  = FTW_INVALID_STATE;
}

/**
  Check to see if it is a valid work space.


  @param WorkingHeader   Pointer of working block header

  @retval TRUE          The work space is valid.
  @retval FALSE         The work space is invalid.

**/
BOOLEAN
IsValidWorkSpace (
  IN EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER *WorkingHeader
  )
{
  if (WorkingHeader == NULL) {
    return FALSE;
  }

  if (CompareMem (WorkingHeader, &mWorkingBlockHeader, sizeof (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER)) == 0) {
    return TRUE;
  }

  DEBUG ((EFI_D_ERROR, "Ftw: Work block header check error\n"));
  return FALSE;
}

/**
  Initialize a work space when there is no work space.

  @param WorkingHeader   Pointer of working block header

  @retval  EFI_SUCCESS    The function completed successfully
  @retval  EFI_ABORTED    The function could not complete successfully.

**/
EFI_STATUS
InitWorkSpaceHeader (
  IN EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER *WorkingHeader
  )
{
  if (WorkingHeader == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  CopyMem (WorkingHeader, &mWorkingBlockHeader, sizeof (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER));

  return EFI_SUCCESS;
}

/**
  Read from working block to refresh the work space in memory.

  @param FtwDevice   Point to private data of FTW driver

  @retval  EFI_SUCCESS    The function completed successfully
  @retval  EFI_ABORTED    The function could not complete successfully.

**/
EFI_STATUS
WorkSpaceRefresh (
  IN EFI_FTW_DEVICE  *FtwDevice
  )
{
  EFI_STATUS                      Status;
  UINTN                           Length;
  UINTN                           RemainingSpaceSize;
  UINT8                           *Ptr;
  UINT8                           LbaIndex;
  UINTN                           Base;

  Status = EFI_SUCCESS;

  //
  // Initialize WorkSpace as FTW_ERASED_BYTE
  //
  SetMem (
    FtwDevice->FtwWorkSpace,
    FtwDevice->FtwWorkSpaceSize,
    FTW_ERASED_BYTE
    );

  //
  // Read from working block
  //
  Ptr = FtwDevice->FtwWorkSpace;
  for (LbaIndex = 0; LbaIndex < (FtwDevice->FtwWorkSpaceSize/FtwDevice->BlockSize); LbaIndex++) {
    Base = 0;
    if (LbaIndex == ((FtwDevice->FtwWorkSpaceSize/FtwDevice->BlockSize) - 1)) {
      if ((FtwDevice->FtwWorkSpaceSize%FtwDevice->BlockSize) == 0) {
        Length = FtwDevice->BlockSize;
      } else {
        Length = FtwDevice->FtwWorkSpaceSize%FtwDevice->BlockSize;
      }
    } else if (LbaIndex == 0) {
      Length = (FtwDevice->BlockSize - FtwDevice->FtwWorkSpaceBase);
      Base = FtwDevice->FtwWorkSpaceBase;
    } else {
      Length = FtwDevice->BlockSize;
    }

    Status = FtwDevice->FtwFvBlock->Read (
                                      FtwDevice->FtwFvBlock,
                                      (FtwDevice->FtwWorkSpaceLba + LbaIndex),
                                      Base,
                                      &Length,
                                      Ptr
                                      );
    Ptr += Length;
  }
  if (EFI_ERROR (Status)) {
    return EFI_ABORTED;
  }
  //
  // Refresh the FtwLastWriteHeader
  //
  Status = FtwGetLastWriteHeader (
            FtwDevice->FtwWorkSpaceHeader,
            FtwDevice->FtwWorkSpaceSize,
            &FtwDevice->FtwLastWriteHeader
            );
  RemainingSpaceSize = FtwDevice->FtwWorkSpaceSize - ((UINTN) FtwDevice->FtwLastWriteHeader - (UINTN) FtwDevice->FtwWorkSpace);
  DEBUG ((EFI_D_INFO, "Ftw: Remaining work space size - %x\n", RemainingSpaceSize));
  //
  // If FtwGetLastWriteHeader() returns error, or the remaining space size is even not enough to contain
  // one EFI_FAULT_TOLERANT_WRITE_HEADER + one EFI_FAULT_TOLERANT_WRITE_RECORD(It will cause that the header
  // pointed by FtwDevice->FtwLastWriteHeader or record pointed by FtwDevice->FtwLastWriteRecord may contain invalid data),
  // it needs to reclaim work space.
  //
  if (EFI_ERROR (Status) || RemainingSpaceSize < sizeof (EFI_FAULT_TOLERANT_WRITE_HEADER) + sizeof (EFI_FAULT_TOLERANT_WRITE_RECORD)) {
    //
    // reclaim work space in working block.
    //
    Status = FtwReclaimWorkSpace (FtwDevice, TRUE);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Ftw: Reclaim workspace - %r\n", Status));
      return EFI_ABORTED;
    }
    //
    // Read from working block again
    //
    Ptr = FtwDevice->FtwWorkSpace;
    for (LbaIndex = 0; LbaIndex < (FtwDevice->FtwWorkSpaceSize/FtwDevice->BlockSize); LbaIndex++) {
      Base = 0;
      if (LbaIndex == ((FtwDevice->FtwWorkSpaceSize/FtwDevice->BlockSize) - 1)) {
        if ((FtwDevice->FtwWorkSpaceSize%FtwDevice->BlockSize) == 0) {
          Length = FtwDevice->BlockSize;
        } else {
          Length = FtwDevice->FtwWorkSpaceSize%FtwDevice->BlockSize;
        }
      } else if (LbaIndex == 0) {
        Length = (FtwDevice->BlockSize - FtwDevice->FtwWorkSpaceBase);
        Base = FtwDevice->FtwWorkSpaceBase;
      } else {
        Length = FtwDevice->BlockSize;
      }

      Status = FtwDevice->FtwFvBlock->Read (
                                        FtwDevice->FtwFvBlock,
                                        (FtwDevice->FtwWorkSpaceLba + LbaIndex),
                                        Base,
                                        &Length,
                                        Ptr
                                        );
      Ptr += Length;
    }
    if (EFI_ERROR (Status)) {
      return EFI_ABORTED;
    }

    Status = FtwGetLastWriteHeader (
              FtwDevice->FtwWorkSpaceHeader,
              FtwDevice->FtwWorkSpaceSize,
              &FtwDevice->FtwLastWriteHeader
              );
    if (EFI_ERROR (Status)) {
      return EFI_ABORTED;
    }
  }
  //
  // Refresh the FtwLastWriteRecord
  //
  Status = FtwGetLastWriteRecord (
            FtwDevice->FtwLastWriteHeader,
            &FtwDevice->FtwLastWriteRecord
            );
  if (EFI_ERROR (Status)) {
    return EFI_ABORTED;
  }

  return EFI_SUCCESS;
}

/**
  Reclaim the work space on the working block.

  @param FtwDevice       Point to private data of FTW driver
  @param PreserveRecord  Whether to preserve the working record is needed

  @retval EFI_SUCCESS            The function completed successfully
  @retval EFI_OUT_OF_RESOURCES   Allocate memory error
  @retval EFI_ABORTED            The function could not complete successfully

**/
EFI_STATUS
FtwReclaimWorkSpace (
  IN EFI_FTW_DEVICE  *FtwDevice,
  IN BOOLEAN         PreserveRecord
  )
{
  EFI_STATUS                              Status;
  UINTN                                   Length;
  EFI_FAULT_TOLERANT_WRITE_HEADER         *Header;
  UINT8                                   *TempBuffer;
  UINTN                                   TempBufferSize;
  UINTN                                   SpareBufferSize;
  UINT8                                   *SpareBuffer;
  EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER *WorkingBlockHeader;
  UINTN                                   Index;
  UINT8                                   *Ptr;
  EFI_LBA                                 WorkSpaceLbaOffset;

  DEBUG ((EFI_D_INFO, "Ftw: start to reclaim work space\n"));

  WorkSpaceLbaOffset = FtwDevice->FtwWorkSpaceLba - FtwDevice->FtwWorkBlockLba;

  //
  // Read all original data from working block to a memory buffer
  //
  TempBufferSize = FtwDevice->SpareAreaLength;
  TempBuffer     = AllocateZeroPool (TempBufferSize);
  if (TempBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Ptr = TempBuffer;
  for (Index = 0; Index < FtwDevice->NumberOfSpareBlock; Index += 1) {
    Length = FtwDevice->BlockSize;
    Status = FtwDevice->FtwFvBlock->Read (
                                          FtwDevice->FtwFvBlock,
                                          FtwDevice->FtwWorkBlockLba + Index,
                                          0,
                                          &Length,
                                          Ptr
                                          );
    if (EFI_ERROR (Status)) {
      FreePool (TempBuffer);
      return EFI_ABORTED;
    }

    Ptr += Length;
  }
  //
  // Clean up the workspace, remove all the completed records.
  //
  Ptr = TempBuffer +
        (UINTN) WorkSpaceLbaOffset * FtwDevice->BlockSize +
        FtwDevice->FtwWorkSpaceBase;

  //
  // Clear the content of buffer that will save the new work space data
  //
  SetMem (Ptr, FtwDevice->FtwWorkSpaceSize, FTW_ERASED_BYTE);

  //
  // Copy EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER to buffer
  //
  CopyMem (
    Ptr,
    FtwDevice->FtwWorkSpaceHeader,
    sizeof (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER)
    );
  if (PreserveRecord) {
    //
    // Get the last record following the header,
    //
    Status = FtwGetLastWriteHeader (
               FtwDevice->FtwWorkSpaceHeader,
               FtwDevice->FtwWorkSpaceSize,
               &FtwDevice->FtwLastWriteHeader
               );
    Header = FtwDevice->FtwLastWriteHeader;
    if (!EFI_ERROR (Status) && (Header != NULL) && (Header->Complete != FTW_VALID_STATE) && (Header->HeaderAllocated == FTW_VALID_STATE)) {
      CopyMem (
        Ptr + sizeof (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER),
        FtwDevice->FtwLastWriteHeader,
        FTW_WRITE_TOTAL_SIZE (Header->NumberOfWrites, Header->PrivateDataSize)
        );
    }
  }

  CopyMem (
    FtwDevice->FtwWorkSpace,
    Ptr,
    FtwDevice->FtwWorkSpaceSize
    );

  FtwGetLastWriteHeader (
    FtwDevice->FtwWorkSpaceHeader,
    FtwDevice->FtwWorkSpaceSize,
    &FtwDevice->FtwLastWriteHeader
    );

  FtwGetLastWriteRecord (
    FtwDevice->FtwLastWriteHeader,
    &FtwDevice->FtwLastWriteRecord
    );

  //
  // Set the WorkingBlockValid and WorkingBlockInvalid as INVALID
  //
  WorkingBlockHeader                      = (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER *) (TempBuffer +
                                            (UINTN) WorkSpaceLbaOffset * FtwDevice->BlockSize +
                                            FtwDevice->FtwWorkSpaceBase);
  WorkingBlockHeader->WorkingBlockValid   = FTW_INVALID_STATE;
  WorkingBlockHeader->WorkingBlockInvalid = FTW_INVALID_STATE;

  //
  // Try to keep the content of spare block
  // Save spare block into a spare backup memory buffer (Sparebuffer)
  //
  SpareBufferSize = FtwDevice->SpareAreaLength;
  SpareBuffer     = AllocatePool (SpareBufferSize);
  if (SpareBuffer == NULL) {
    FreePool (TempBuffer);
    return EFI_OUT_OF_RESOURCES;
  }

  Ptr = SpareBuffer;
  for (Index = 0; Index < FtwDevice->NumberOfSpareBlock; Index += 1) {
    Length = FtwDevice->BlockSize;
    Status = FtwDevice->FtwBackupFvb->Read (
                                        FtwDevice->FtwBackupFvb,
                                        FtwDevice->FtwSpareLba + Index,
                                        0,
                                        &Length,
                                        Ptr
                                        );
    if (EFI_ERROR (Status)) {
      FreePool (TempBuffer);
      FreePool (SpareBuffer);
      return EFI_ABORTED;
    }

    Ptr += Length;
  }
  //
  // Write the memory buffer to spare block
  //
  Status  = FtwEraseSpareBlock (FtwDevice);
  Ptr     = TempBuffer;
  for (Index = 0; Index < FtwDevice->NumberOfSpareBlock; Index += 1) {
    Length = FtwDevice->BlockSize;
    Status = FtwDevice->FtwBackupFvb->Write (
                                            FtwDevice->FtwBackupFvb,
                                            FtwDevice->FtwSpareLba + Index,
                                            0,
                                            &Length,
                                            Ptr
                                            );
    if (EFI_ERROR (Status)) {
      FreePool (TempBuffer);
      FreePool (SpareBuffer);
      return EFI_ABORTED;
    }

    Ptr += Length;
  }
  //
  // Free TempBuffer
  //
  FreePool (TempBuffer);

  //
  // Set the WorkingBlockValid in spare block
  //
  Status = FtwUpdateFvState (
            FtwDevice->FtwBackupFvb,
            FtwDevice->FtwSpareLba + WorkSpaceLbaOffset,
            FtwDevice->FtwWorkSpaceBase + sizeof (EFI_GUID) + sizeof (UINT32),
            WORKING_BLOCK_VALID
            );
  if (EFI_ERROR (Status)) {
    FreePool (SpareBuffer);
    return EFI_ABORTED;
  }
  //
  // Before erase the working block, set WorkingBlockInvalid in working block.
  //
  // Offset = OFFSET_OF(EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER,
  //                          WorkingBlockInvalid);
  //
  Status = FtwUpdateFvState (
            FtwDevice->FtwFvBlock,
            FtwDevice->FtwWorkSpaceLba,
            FtwDevice->FtwWorkSpaceBase + sizeof (EFI_GUID) + sizeof (UINT32),
            WORKING_BLOCK_INVALID
            );
  if (EFI_ERROR (Status)) {
    FreePool (SpareBuffer);
    return EFI_ABORTED;
  }

  FtwDevice->FtwWorkSpaceHeader->WorkingBlockInvalid = FTW_VALID_STATE;

  //
  // Write the spare block to working block
  //
  Status = FlushSpareBlockToWorkingBlock (FtwDevice);
  if (EFI_ERROR (Status)) {
    FreePool (SpareBuffer);
    return Status;
  }
  //
  // Restore spare backup buffer into spare block , if no failure happened during FtwWrite.
  //
  Status  = FtwEraseSpareBlock (FtwDevice);
  Ptr     = SpareBuffer;
  for (Index = 0; Index < FtwDevice->NumberOfSpareBlock; Index += 1) {
    Length = FtwDevice->BlockSize;
    Status = FtwDevice->FtwBackupFvb->Write (
                                        FtwDevice->FtwBackupFvb,
                                        FtwDevice->FtwSpareLba + Index,
                                        0,
                                        &Length,
                                        Ptr
                                        );
    if (EFI_ERROR (Status)) {
      FreePool (SpareBuffer);
      return EFI_ABORTED;
    }

    Ptr += Length;
  }

  FreePool (SpareBuffer);

  DEBUG ((EFI_D_INFO, "Ftw: reclaim work space successfully\n"));

  return EFI_SUCCESS;
}
