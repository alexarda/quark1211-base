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

  QNCSmmCore.c

Abstract:

  This driver is responsible for the registration of child drivers
  and the abstraction of the QNC SMI sources.

--*/

//
// Include common header file for this module.
//
#include "CommonHeader.h"

#include "QNCSmm.h"
#include "QNCSmmHelpers.h"

//
// /////////////////////////////////////////////////////////////////////////////
// MODULE / GLOBAL DATA
//
// Module variables used by the both the main dispatcher and the source dispatchers
// Declared in QNCSmmSources.h
//
UINT32                    mPciData;
UINT32                    mPciAddress;

PRIVATE_DATA              mPrivateData = {  // for the structure
  {
    NULL
  },                                        // CallbackDataBase linked list head
  NULL,                                     // Handler returned whan calling SmiHandlerRegister
  NULL,                                     // EFI handle returned when calling InstallMultipleProtocolInterfaces
  {                                         // protocol arrays
    // elements within the array
    //
    {
      PROTOCOL_SIGNATURE,
      SxType,
      &gEfiSmmSxDispatch2ProtocolGuid,
      {
        {
          (QNC_SMM_GENERIC_REGISTER) QNCSmmCoreRegister,
          (QNC_SMM_GENERIC_UNREGISTER) QNCSmmCoreUnRegister
        }
      }
    },
    {
      PROTOCOL_SIGNATURE,
      SwType,
      &gEfiSmmSwDispatch2ProtocolGuid,
      {
        {
          (QNC_SMM_GENERIC_REGISTER) QNCSmmCoreRegister,
          (QNC_SMM_GENERIC_UNREGISTER) QNCSmmCoreUnRegister,
          (UINTN) MAXIMUM_SWI_VALUE
        }
      }
    },
    {
      PROTOCOL_SIGNATURE,
      GpiType,
      &gEfiSmmGpiDispatch2ProtocolGuid,
      {
        {
          (QNC_SMM_GENERIC_REGISTER) QNCSmmCoreRegister,
          (QNC_SMM_GENERIC_UNREGISTER) QNCSmmCoreUnRegister,
          (UINTN) 1
        }
      }
    },
    {
      PROTOCOL_SIGNATURE,
      QNCnType,
      &gEfiSmmIchnDispatch2ProtocolGuid,
      {
        {
          (QNC_SMM_GENERIC_REGISTER) QNCSmmCoreRegister,
          (QNC_SMM_GENERIC_UNREGISTER) QNCSmmCoreUnRegister
        }
      }
    },
    {
      PROTOCOL_SIGNATURE,
      PowerButtonType,
      &gEfiSmmPowerButtonDispatch2ProtocolGuid,
      {
        {
          (QNC_SMM_GENERIC_REGISTER) QNCSmmCoreRegister,
          (QNC_SMM_GENERIC_UNREGISTER) QNCSmmCoreUnRegister
        }
      }
    },
    {
      PROTOCOL_SIGNATURE,
      PeriodicTimerType,
      &gEfiSmmPeriodicTimerDispatch2ProtocolGuid,
      {
        {
          (QNC_SMM_GENERIC_REGISTER) QNCSmmCoreRegister,
          (QNC_SMM_GENERIC_UNREGISTER) QNCSmmCoreUnRegister,
          (UINTN) QNCSmmPeriodicTimerDispatchGetNextShorterInterval
        }
      }
    },
  }
};

CONTEXT_FUNCTIONS         mContextFunctions[NUM_PROTOCOLS] = {
  {
    SxGetContext,
    SxCmpContext,
    NULL
  },
  {
    SwGetContext,
    SwCmpContext,
    SwGetBuffer
  },
  {
    NULL,
    NULL,
    NULL
  },
  {
    NULL,
    NULL,
    NULL
  },
  {
    NULL,
    NULL,
    NULL
  },
  {
    PeriodicTimerGetContext,
    PeriodicTimerCmpContext,
    PeriodicTimerGetBuffer,
  },
};

//
// /////////////////////////////////////////////////////////////////////////////
// PROTOTYPES
//
// Functions use only in this file
//
EFI_STATUS
QNCSmmCoreDispatcher (
  IN     EFI_HANDLE               DispatchHandle,
  IN     CONST VOID               *Context,        OPTIONAL
  IN OUT VOID                     *CommBuffer,     OPTIONAL
  IN OUT UINTN                    *CommBufferSize  OPTIONAL
  );


UINTN
DevicePathSize (
  IN EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  );

//
// /////////////////////////////////////////////////////////////////////////////
// FUNCTIONS
//
// Driver entry point
//
EFI_STATUS
EFIAPI
InitializeQNCSmmDispatcher (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
/*++

Routine Description:

  Initializes the QNC SMM Dispatcher

Arguments:

  ImageHandle   - Pointer to the loaded image protocol for this driver
  SystemTable   - Pointer to the EFI System Table

Returns:
  Status        - EFI_SUCCESS

--*/
{
  EFI_STATUS                Status;

  QNCSmmPublishDispatchProtocols ();

  //
  // Register a callback function to handle subsequent SMIs.  This callback
  // will be called by SmmCoreDispatcher.
  //
  Status = gSmst->SmiHandlerRegister (QNCSmmCoreDispatcher, NULL, &mPrivateData.SmiHandle);
  ASSERT_EFI_ERROR (Status);

  //
  // Initialize Callback DataBase
  //
  InitializeListHead (&mPrivateData.CallbackDataBase);

  //
  // Enable SMIs on the QNC now that we have a callback
  //
  QNCSmmInitHardware ();

  return EFI_SUCCESS;
}

EFI_STATUS
SaveState (
  VOID
  )
/*++

Routine Description:

  Save Index registers to avoid corrupting the foreground environment

Arguments:
  None

Returns:
  Status - EFI_SUCCESS

--*/
{
  mPciAddress = IoRead32 (EFI_PCI_ADDRESS_PORT);
  return EFI_SUCCESS;
}

EFI_STATUS
RestoreState (
  VOID
  )
/*++

Routine Description:

  Restore Index registers to avoid corrupting the foreground environment

Arguments:
  None

Returns:
  Status - EFI_SUCCESS

--*/
{
  IoWrite32 (EFI_PCI_ADDRESS_PORT, mPciAddress);
  return EFI_SUCCESS;
}

EFI_STATUS
SmiInputValueDuplicateCheck (
  UINTN           FedSwSmiInputValue
  )
/*++

Routine Description:

  Check the Fed SwSmiInputValue to see if there is a duplicated one in the database

Arguments:
  None

Returns:
  Status - EFI_SUCCESS, EFI_INVALID_PARAMETER

--*/
// GC_TODO:    FedSwSmiInputValue - add argument and description to function comment
{

  DATABASE_RECORD *RecordInDb;
  LIST_ENTRY      *LinkInDb;

  LinkInDb = GetFirstNode (&mPrivateData.CallbackDataBase);
  while (!IsNull (&mPrivateData.CallbackDataBase, LinkInDb)) {
    RecordInDb = DATABASE_RECORD_FROM_LINK (LinkInDb);

    if (RecordInDb->ProtocolType == SwType) {
      if (RecordInDb->ChildContext.Sw.SwSmiInputValue == FedSwSmiInputValue) {
        return EFI_INVALID_PARAMETER;
      }
    }

    LinkInDb = GetNextNode (&mPrivateData.CallbackDataBase, &RecordInDb->Link);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
QNCSmmCoreRegister (
  IN  QNC_SMM_GENERIC_PROTOCOL                          *This,
  IN  EFI_SMM_HANDLER_ENTRY_POINT2                      DispatchFunction,
  IN  QNC_SMM_CONTEXT                                    *RegisterContext,
  OUT EFI_HANDLE                                        *DispatchHandle
  )
/*++

Routine Description:

Arguments:

Returns:

--*/
// GC_TODO:    This - add argument and description to function comment
// GC_TODO:    DispatchFunction - add argument and description to function comment
// GC_TODO:    RegisterContext - add argument and description to function comment
// GC_TODO:    DispatchHandle - add argument and description to function comment
// GC_TODO:    EFI_OUT_OF_RESOURCES - add return value to function comment
// GC_TODO:    EFI_INVALID_PARAMETER - add return value to function comment
// GC_TODO:    EFI_SUCCESS - add return value to function comment
// GC_TODO:    EFI_INVALID_PARAMETER - add return value to function comment
{
  EFI_STATUS                  Status;
  DATABASE_RECORD             *Record;
  QNC_SMM_QUALIFIED_PROTOCOL  *Qualified;
  INTN                        Index;

  //
  // Check for invalid parameter
  //
  if (This == NULL || RegisterContext == NULL || DispatchHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Create database record and add to database
  //
  Record = (DATABASE_RECORD *) AllocateZeroPool (sizeof (DATABASE_RECORD));
  if (Record == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Gather information about the registration request
  //
  Record->Callback          = DispatchFunction;
  Record->ChildContext      = *RegisterContext;

  Qualified                 = QUALIFIED_PROTOCOL_FROM_GENERIC (This);

  Record->ProtocolType      = Qualified->Type;

  CopyMem (&Record->ContextFunctions, &mContextFunctions[Qualified->Type], sizeof (Record->ContextFunctions));
  //
  // Perform linked list housekeeping
  //
  Record->Signature = DATABASE_RECORD_SIGNATURE;

  switch (Qualified->Type) {
  //
  // By the end of this switch statement, we'll know the
  // source description the child is registering for
  //
  case SxType:
    //
    // Check the validity of Context Type and Phase
    //
    if ((Record->ChildContext.Sx.Type < SxS0) ||
        (Record->ChildContext.Sx.Type >= EfiMaximumSleepType) ||
        (Record->ChildContext.Sx.Phase < SxEntry) ||
        (Record->ChildContext.Sx.Phase >= EfiMaximumPhase)
        ) {
      goto Error;
    }

    InsertTailList (&mPrivateData.CallbackDataBase, &Record->Link);
    CopyMem (&Record->SrcDesc, &SX_SOURCE_DESC, sizeof (Record->SrcDesc));
    //
    // use default clear source function
    //
    break;

  case SwType:
    if (RegisterContext->Sw.SwSmiInputValue == (UINTN)-1) {
      //
      // If SwSmiInputValue is set to (UINTN) -1 then a unique value will be assigned and returned in the structure.
      //
      Status = EFI_NOT_FOUND;
      for (Index = 1; Index < MAXIMUM_SWI_VALUE; Index++) {
        Status = SmiInputValueDuplicateCheck (Index);
        if (!EFI_ERROR (Status)) {
          RegisterContext->Sw.SwSmiInputValue = Index;
          break;
        }
      }
      if (RegisterContext->Sw.SwSmiInputValue == (UINTN)-1) {
        Status = gSmst->SmmFreePool (Record);
        return EFI_OUT_OF_RESOURCES;
      }
      //
      // Update ChildContext again as SwSmiInputValue has been changed
      //
      Record->ChildContext = *RegisterContext;
    }

    //
    // Check the validity of Context Value
    //
    if (Record->ChildContext.Sw.SwSmiInputValue > MAXIMUM_SWI_VALUE) {
      goto Error;
    }

    if (EFI_ERROR (SmiInputValueDuplicateCheck (Record->ChildContext.Sw.SwSmiInputValue))) {
      goto Error;
    }

    InsertTailList (&mPrivateData.CallbackDataBase, &Record->Link);
    CopyMem (&Record->SrcDesc, &SW_SOURCE_DESC, sizeof (Record->SrcDesc));
    Record->BufferSize = sizeof (EFI_SMM_SW_REGISTER_CONTEXT);
    //
    // use default clear source function
    //
    break;

  case GpiType:

    InsertTailList (&mPrivateData.CallbackDataBase, &Record->Link);
    CopyMem (&Record->SrcDesc, &GPI_SOURCE_DESC, sizeof (Record->SrcDesc));
    //
    // use default clear source function
    //
    break;

  case QNCnType:
    //
    // Check the validity of Context Type
    //
    if ((Record->ChildContext.QNCn.Type < IchnMch) || (Record->ChildContext.QNCn.Type >= NUM_ICHN_TYPES)) {
      goto Error;
    }

    InsertTailList (&mPrivateData.CallbackDataBase, &Record->Link);
    CopyMem (&Record->SrcDesc, &QNCN_SOURCE_DESCS[Record->ChildContext.QNCn.Type], sizeof (Record->SrcDesc));
    Record->ClearSource = QNCSmmQNCnClearSource;
    break;

  case PeriodicTimerType:

    Status = MapPeriodicTimerToSrcDesc (RegisterContext, &(Record->SrcDesc));
    if (EFI_ERROR (Status)) {
      goto Error;
    }

    InsertTailList (&mPrivateData.CallbackDataBase, &Record->Link);
    Record->BufferSize = sizeof (EFI_SMM_PERIODIC_TIMER_CONTEXT);
    Record->ClearSource = QNCSmmPeriodicTimerClearSource;
    break;

  default:
    goto Error;
    break;
  };

  if (Record->ClearSource == NULL) {
    //
    // Clear the SMI associated w/ the source using the default function
    //
    QNCSmmClearSource (&Record->SrcDesc);
  } else {
    //
    // This source requires special handling to clear
    //
    Record->ClearSource (&Record->SrcDesc);
  }

  QNCSmmEnableSource (&Record->SrcDesc);

  //
  // Child's handle will be the address linked list link in the record
  //
  *DispatchHandle = (EFI_HANDLE) (&Record->Link);

  return EFI_SUCCESS;

Error:
  FreePool (Record);
  //
  // DEBUG((EFI_D_ERROR,"Free pool status %d\n", Status ));
  //
  return EFI_INVALID_PARAMETER;
}

EFI_STATUS
QNCSmmCoreUnRegister (
  IN QNC_SMM_GENERIC_PROTOCOL                         *This,
  IN EFI_HANDLE                                        DispatchHandle
  )
/*++

Routine Description:

Arguments:

Returns:

--*/
// GC_TODO:    This - add argument and description to function comment
// GC_TODO:    DispatchHandle - add argument and description to function comment
// GC_TODO:    EFI_INVALID_PARAMETER - add return value to function comment
// GC_TODO:    EFI_INVALID_PARAMETER - add return value to function comment
// GC_TODO:    EFI_SUCCESS - add return value to function comment
{
  BOOLEAN         SafeToDisable;
  DATABASE_RECORD *RecordToDelete;
  DATABASE_RECORD *RecordInDb;
  LIST_ENTRY      *LinkInDb;

  if (DispatchHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (BASE_CR (DispatchHandle, DATABASE_RECORD, Link)->Signature != DATABASE_RECORD_SIGNATURE) {
    return EFI_INVALID_PARAMETER;
  }

  RecordToDelete = DATABASE_RECORD_FROM_LINK (DispatchHandle);

  RemoveEntryList (&RecordToDelete->Link);
  RecordToDelete->Signature = 0;

  //
  // See if we can disable the source, reserved for future use since this might
  //  not be the only criteria to disable
  //
  SafeToDisable = TRUE;
  LinkInDb = GetFirstNode (&mPrivateData.CallbackDataBase);
  while(!IsNull (&mPrivateData.CallbackDataBase, LinkInDb)) {
    RecordInDb = DATABASE_RECORD_FROM_LINK (LinkInDb);
    if (CompareEnables (&RecordToDelete->SrcDesc, &RecordInDb->SrcDesc)) {
      SafeToDisable = FALSE;
      break;
    }
    LinkInDb = GetNextNode (&mPrivateData.CallbackDataBase, &RecordInDb->Link);
  }
  if (SafeToDisable) {
    QNCSmmDisableSource( &RecordToDelete->SrcDesc );
}

  FreePool (RecordToDelete);

  return EFI_SUCCESS;
}

/**
  This function is the main entry point for an SMM handler dispatch
  or communicate-based callback.

  @param  DispatchHandle  The unique handle assigned to this handler by SmiHandlerRegister().
  @param  RegisterContext Points to an optional handler context which was specified when the handler was registered.
  @param  CommBuffer      A pointer to a collection of data in memory that will
                          be conveyed from a non-SMM environment into an SMM environment.
  @param  CommBufferSize  The size of the CommBuffer.

  @return Status Code

**/
EFI_STATUS
QNCSmmCoreDispatcher (
  IN     EFI_HANDLE               DispatchHandle,
  IN     CONST VOID               *RegisterContext,
  IN OUT VOID                     *CommBuffer,
  IN OUT UINTN                    *CommBufferSize
  )
{
  //
  // Used to prevent infinite loops
  //
  UINTN               EscapeCount;

  BOOLEAN             ContextsMatch;
  BOOLEAN             ResetListSearch;
  BOOLEAN             EosSet;
  BOOLEAN             SxChildWasDispatched;
  BOOLEAN             ChildWasDispatched;

  DATABASE_RECORD     *RecordInDb;
  LIST_ENTRY          *LinkInDb;
  DATABASE_RECORD     *RecordToExhaust;
  LIST_ENTRY          *LinkToExhaust;

  QNC_SMM_CONTEXT     Context;
  VOID                *CommunicationBuffer;
  UINTN               BufferSize;

  EFI_STATUS          Status;

  QNC_SMM_SOURCE_DESC ActiveSource = NULL_SOURCE_DESC_INITIALIZER;

  EscapeCount           = 100;
  ContextsMatch         = FALSE;
  ResetListSearch       = FALSE;
  EosSet                = FALSE;
  SxChildWasDispatched  = FALSE;
  Status                = EFI_WARN_INTERRUPT_SOURCE_PENDING;
  ChildWasDispatched    = FALSE;

  //
  // Preserve Index registers
  //
  SaveState ();

  if (!IsListEmpty (&mPrivateData.CallbackDataBase)) {
    //
    // We have children registered w/ us -- continue
    //
    while ((!EosSet) && (EscapeCount > 0)) {
      EscapeCount--;

      //
      // Reset this flag in order to be able to process multiple SMI Sources in one loop.
      //
      ResetListSearch = FALSE;

      LinkInDb = GetFirstNode (&mPrivateData.CallbackDataBase);

      while ((!IsNull (&mPrivateData.CallbackDataBase, LinkInDb)) && (ResetListSearch == FALSE)) {
        RecordInDb = DATABASE_RECORD_FROM_LINK (LinkInDb);

        //
        // look for the first active source
        //
        if (!SourceIsActive (&RecordInDb->SrcDesc)) {
          //
          // Didn't find the source yet, keep looking
          //
          LinkInDb = GetNextNode (&mPrivateData.CallbackDataBase, &RecordInDb->Link);

        } else {
          //
          // We found a source. If this is a sleep type, we have to go to
          // appropriate sleep state anyway.No matter there is sleep child or not
          //
          if (RecordInDb->ProtocolType == SxType) {
            SxChildWasDispatched = TRUE;
          }
          //
          // "cache" the source description and don't query I/O anymore
          //
          CopyMem (&ActiveSource, &RecordInDb->SrcDesc, sizeof (ActiveSource));
          LinkToExhaust = LinkInDb;

          //
          // exhaust the rest of the queue looking for the same source
          //
          while (!IsNull (&mPrivateData.CallbackDataBase, LinkToExhaust)) {
            RecordToExhaust = DATABASE_RECORD_FROM_LINK (LinkToExhaust);

            if (CompareSources (&RecordToExhaust->SrcDesc, &ActiveSource)) {
              //
              // These source descriptions are equal, so this callback should be
              // dispatched.
              //
              if (RecordToExhaust->ContextFunctions.GetContext != NULL) {
                //
                // This child requires that we get a calling context from
                // hardware and compare that context to the one supplied
                // by the child.
                //
                ASSERT (RecordToExhaust->ContextFunctions.CmpContext != NULL);

                //
                // Make sure contexts match before dispatching event to child
                //
                RecordToExhaust->ContextFunctions.GetContext (RecordToExhaust, &Context);
                ContextsMatch = RecordToExhaust->ContextFunctions.CmpContext (&Context, &RecordToExhaust->ChildContext);

              } else {
                //
                // This child doesn't require any more calling context beyond what
                // it supplied in registration.  Simply pass back what it gave us.
                //
                ASSERT (RecordToExhaust->Callback != NULL);
                Context       = RecordToExhaust->ChildContext;
                ContextsMatch = TRUE;
              }

              if (ContextsMatch) {

                if (RecordToExhaust->BufferSize != 0) {
                  ASSERT (RecordToExhaust->ContextFunctions.GetBuffer != NULL);

                  RecordToExhaust->ContextFunctions.GetBuffer (RecordToExhaust);

                  CommunicationBuffer = &RecordToExhaust->CommBuffer;
                  BufferSize = RecordToExhaust->BufferSize;
                } else {
                  CommunicationBuffer = NULL;
                  BufferSize = 0;
                }

                ASSERT (RecordToExhaust->Callback != NULL);

                RecordToExhaust->Callback (
                                   (EFI_HANDLE) & RecordToExhaust->Link,
                                   &Context,
                                   CommunicationBuffer,
                                   &BufferSize
                                   );

                ChildWasDispatched = TRUE;
                if (RecordToExhaust->ProtocolType == SxType) {
                  SxChildWasDispatched = TRUE;
                }
              }
            }
            //
            // Get next record in DB
            //
            LinkToExhaust = GetNextNode (&mPrivateData.CallbackDataBase, &RecordToExhaust->Link);
          }

          if (RecordInDb->ClearSource == NULL) {
            //
            // Clear the SMI associated w/ the source using the default function
            //
            QNCSmmClearSource (&ActiveSource);
          } else {
            //
            // This source requires special handling to clear
            //
            RecordInDb->ClearSource (&ActiveSource);
          }

          if (ChildWasDispatched) {
            //
            // The interrupt was handled and quiesced
            //
            Status = EFI_SUCCESS;
          } else {
            //
            // The interrupt was not handled but quiesced
            //
            Status = EFI_WARN_INTERRUPT_SOURCE_QUIESCED;
          }

          //
          // Queue is empty, reset the search
          //
          ResetListSearch = TRUE;

        }
      }
      EosSet = QNCSmmSetAndCheckEos ();
    }
  }
  //
  // If you arrive here, there are two possible reasons:
  // (1) you've got problems with clearing the SMI status bits in the
  // ACPI table.  If you don't properly clear the SMI bits, then you won't be able to set the
  // EOS bit.  If this happens too many times, the loop exits.
  // (2) there was a SMM communicate for callback messages that was received prior
  // to this driver.
  // If there is an asynchronous SMI that occurs while processing the Callback, let
  // all of the drivers (including this one) have an opportunity to scan for the SMI
  // and handle it.
  // If not, we don't want to exit and have the foreground app. clear EOS without letting
  // these other sources get serviced.
  //
  ASSERT (EscapeCount > 0);

  //
  // Restore Index registers
  //
  RestoreState ();

  if (SxChildWasDispatched) {
    //
    // A child of the SmmSxDispatch protocol was dispatched during this call;
    // put the system to sleep.
    //
    QNCSmmSxGoToSleep ();
  }

  return Status;

}
