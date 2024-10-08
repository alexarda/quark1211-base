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

  QNCSmmSw.c

Abstract:

  File to contain all the hardware specific stuff for the Smm Sw dispatch protocol.

--*/ 

//
// Include common header file for this module.
//
#include "CommonHeader.h"

#include "QNCSmmHelpers.h"

EFI_SMM_CPU_PROTOCOL  *mSmmCpu = NULL;

CONST QNC_SMM_SOURCE_DESC SW_SOURCE_DESC = { 
  QNC_SMM_NO_FLAGS,
  {
    {
      {GPE_ADDR_TYPE, {R_QNC_GPE0BLK_SMIE}}, S_QNC_GPE0BLK_SMIE, N_QNC_GPE0BLK_SMIE_APM
    },
    NULL_BIT_DESC_INITIALIZER
  },
  {
    {
      {GPE_ADDR_TYPE, {R_QNC_GPE0BLK_SMIS}}, S_QNC_GPE0BLK_SMIS, N_QNC_GPE0BLK_SMIS_APM
    }
  }
};

VOID
SwGetContext(
  IN  DATABASE_RECORD    *Record,
  OUT QNC_SMM_CONTEXT    *Context
  )
{
  Context->Sw.SwSmiInputValue = IoRead8 (R_APM_CNT);
}

BOOLEAN
SwCmpContext (
  IN QNC_SMM_CONTEXT     *Context1,
  IN QNC_SMM_CONTEXT     *Context2
  )
{
  return (BOOLEAN)( Context1->Sw.SwSmiInputValue == Context2->Sw.SwSmiInputValue );
}

VOID
SwGetBuffer (
  IN  DATABASE_RECORD     * Record
  )
{
  EFI_STATUS                 Status;
  UINTN                      Index;
  UINTN                      CpuIndex;
  EFI_SMM_SAVE_STATE_IO_INFO IoState;

  //
  // Locate SMM CPU protocol to retrive the CPU save state
  //
  if (mSmmCpu == NULL) {
    Status = gSmst->SmmLocateProtocol (&gEfiSmmCpuProtocolGuid, NULL, (VOID **) &mSmmCpu);
    ASSERT_EFI_ERROR (Status);
  }

  //
  // Find the CPU which generated the software SMI
  //
  CpuIndex = 0;
  for (Index = 0; Index < gSmst->NumberOfCpus; Index++) {
    Status = mSmmCpu->ReadSaveState (
                        mSmmCpu,
                        sizeof (EFI_SMM_SAVE_STATE_IO_INFO),
                        EFI_SMM_SAVE_STATE_REGISTER_IO,
                        Index,
                        &IoState
                        );
    if (!EFI_ERROR (Status) && (IoState.IoPort == R_APM_CNT)) {
      CpuIndex = Index;
      break;
    }
  }

  Record->CommBuffer.Sw.SwSmiCpuIndex = CpuIndex;
}
