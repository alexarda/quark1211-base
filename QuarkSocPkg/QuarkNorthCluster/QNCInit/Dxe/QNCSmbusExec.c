/** @file
  Common code to implement SMBus bus protocols. Smbus PEI and DXE modules
  share the same version of this file.

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

#include "QNCSmbus.h"

/**
  Checks the parameter of SmbusExecute().

  This function checks the input parameters of SmbusExecute().  If the input parameters are valid
  for certain SMBus bus protocol, it will return EFI_SUCCESS; otherwise, it will return certain
  error code based on the input SMBus bus protocol.

  @param  SlaveAddress            The SMBus slave address of the device with which to communicate.
  @param  Command                 This command is transmitted by the SMBus host controller to the
                                  SMBus slave device and the interpretation is SMBus slave device
                                  specific. It can mean the offset to a list of functions inside an
                                  SMBus slave device. Not all operations or slave devices support
                                  this command's registers.
  @param  Operation               Signifies which particular SMBus hardware protocol instance that
                                  it will use to execute the SMBus transactions. This SMBus
                                  hardware protocol is defined by the SMBus Specification and is
                                  not related to EFI.
  @param  PecCheck                Defines if Packet Error Code (PEC) checking is required for this
                                  operation.
  @param  Length                  Signifies the number of bytes that this operation will do. The
                                  maximum number of bytes can be revision specific and operation
                                  specific. This field will contain the actual number of bytes that
                                  are executed for this operation. Not all operations require this
                                  argument.
  @param  Buffer                  Contains the value of data to execute to the SMBus slave device.
                                  Not all operations require this argument. The length of this
                                  buffer is identified by Length.

  @retval EFI_SUCCESS             All the parameters are valid for the corresponding SMBus bus
                                  protocol.
  @retval EFI_INVALID_PARAMETER   Operation is not defined in EFI_SMBUS_OPERATION.
  @retval EFI_INVALID_PARAMETER   Length/Buffer is NULL for operations except for EfiSmbusQuickRead
                                  and EfiSmbusQuickWrite. Length is outside the range of valid
                                  values.
  @retval EFI_UNSUPPORTED         The SMBus operation or PEC is not supported.
  @retval EFI_BUFFER_TOO_SMALL    Buffer is not sufficient for this operation.

**/
STATIC
EFI_STATUS
QncSmbusExecCheckParameters (
  IN     EFI_SMBUS_DEVICE_ADDRESS SlaveAddress,
  IN     EFI_SMBUS_DEVICE_COMMAND Command,
  IN     EFI_SMBUS_OPERATION      Operation,
  IN     BOOLEAN                  PecCheck,
  IN OUT UINTN                    *Length,
  IN OUT VOID                     *Buffer
  )
{
  EFI_STATUS  Status;
  UINTN       RequiredLen;

  //
  // Set default value to be 2:
  // for SmbusReadWord, SmbusWriteWord and SmbusProcessCall.
  //
  RequiredLen = 2;
  Status      = EFI_SUCCESS;
  switch (Operation) {
  case EfiSmbusQuickRead:
  case EfiSmbusQuickWrite:
    if (PecCheck || Command != 0) {
      return EFI_UNSUPPORTED;
    }
    break;
  case EfiSmbusReceiveByte:
  case EfiSmbusSendByte:
    if (Command != 0) {
      return EFI_UNSUPPORTED;
    }
    //
    // Cascade to check length parameter.
    //
  case EfiSmbusReadByte:
  case EfiSmbusWriteByte:
    RequiredLen = 1;
    //
    // Cascade to check length parameter.
    //
  case EfiSmbusReadWord:
  case EfiSmbusWriteWord:
  case EfiSmbusProcessCall:
    if (Buffer == NULL || Length == NULL) {
      return EFI_INVALID_PARAMETER;
    } else if (*Length < RequiredLen) {
      Status = EFI_BUFFER_TOO_SMALL;
    }
    *Length = RequiredLen;
    break;
  case EfiSmbusReadBlock:
  case EfiSmbusWriteBlock:
    if ((Buffer == NULL) ||
        (Length == NULL) ||
        (*Length < MIN_SMBUS_BLOCK_LEN) ||
        (*Length > MAX_SMBUS_BLOCK_LEN)) {
      return EFI_INVALID_PARAMETER;
    }
    break;
  case EfiSmbusBWBRProcessCall:
    return EFI_UNSUPPORTED;
  default:
    return EFI_INVALID_PARAMETER;
  }
  return Status;
}

/**
  Executes an SMBus operation to an SMBus controller. Returns when either the command has been
  executed or an error is encountered in doing the operation.

  The internal worker function provides a standard way to execute an operation as defined in the
  System Management Bus (SMBus) Specification. The resulting transaction will be either that the
  SMBus slave devices accept this transaction or that this function returns with error.

  @param  SlaveAddress            The SMBus slave address of the device with which to communicate.
  @param  Command                 This command is transmitted by the SMBus host controller to the
                                  SMBus slave device and the interpretation is SMBus slave device
                                  specific. It can mean the offset to a list of functions inside an
                                  SMBus slave device. Not all operations or slave devices support
                                  this command's registers.
  @param  Operation               Signifies which particular SMBus hardware protocol instance that
                                  it will use to execute the SMBus transactions. This SMBus
                                  hardware protocol is defined by the SMBus Specification and is
                                  not related to EFI.
  @param  PecCheck                Defines if Packet Error Code (PEC) checking is required for this
                                  operation.
  @param  Length                  Signifies the number of bytes that this operation will do. The
                                  maximum number of bytes can be revision specific and operation
                                  specific. This field will contain the actual number of bytes that
                                  are executed for this operation. Not all operations require this
                                  argument.
  @param  Buffer                  Contains the value of data to execute to the SMBus slave device.
                                  Not all operations require this argument. The length of this
                                  buffer is identified by Length.

  @retval EFI_SUCCESS             The last data that was returned from the access matched the poll
                                  exit criteria.
  @retval EFI_CRC_ERROR           Checksum is not correct (PEC is incorrect).
  @retval EFI_TIMEOUT             Timeout expired before the operation was completed. Timeout is
                                  determined by the SMBus host controller device.
  @retval EFI_OUT_OF_RESOURCES    The request could not be completed due to a lack of resources.
  @retval EFI_DEVICE_ERROR        The request was not completed because a failure that was
                                  reflected in the Host Status Register bit. Device errors are a
                                  result of a transaction collision, illegal command field,
                                  unclaimed cycle (host initiated), or bus errors (collisions).
  @retval EFI_INVALID_PARAMETER   Operation is not defined in EFI_SMBUS_OPERATION.
  @retval EFI_INVALID_PARAMETER   Length/Buffer is NULL for operations except for EfiSmbusQuickRead
                                  and EfiSmbusQuickWrite. Length is outside the range of valid
                                  values.
  @retval EFI_UNSUPPORTED         The SMBus operation or PEC is not supported.
  @retval EFI_BUFFER_TOO_SMALL    Buffer is not sufficient for this operation.

**/
EFI_STATUS
Execute (
  IN     EFI_SMBUS_DEVICE_ADDRESS SlaveAddress,
  IN     EFI_SMBUS_DEVICE_COMMAND Command,
  IN     EFI_SMBUS_OPERATION      Operation,
  IN     BOOLEAN                  PecCheck,
  IN OUT UINTN                    *Length,
  IN OUT VOID                     *Buffer
  )
{
  EFI_STATUS                      Status;
  UINTN                           SmbusAddress;
  UINTN                           WorkBufferLen;
  UINT8                           WorkBuffer[MAX_SMBUS_BLOCK_LEN];

  Status = QncSmbusExecCheckParameters (
             SlaveAddress,
             Command,
             Operation,
             PecCheck,
             Length,
             Buffer);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  
  SmbusAddress = SMBUS_LIB_ADDRESS (SlaveAddress.SmbusDeviceAddress, Command, *Length, PecCheck);
  
  switch (Operation) {
  case EfiSmbusQuickRead:
    SmBusQuickRead (SmbusAddress, &Status);
    break;
  case EfiSmbusQuickWrite:
    SmBusQuickWrite (SmbusAddress, &Status);
    break;
  case EfiSmbusReceiveByte:
    *(UINT8 *) Buffer = SmBusReceiveByte (SmbusAddress, &Status);
    break;
  case EfiSmbusSendByte:
    SmBusSendByte (SmbusAddress, *(UINT8 *) Buffer, &Status);
    break;
  case EfiSmbusReadByte:
    *(UINT8 *) Buffer = SmBusReadDataByte (SmbusAddress, &Status);
    break;
  case EfiSmbusWriteByte:
    SmBusWriteDataByte (SmbusAddress, *(UINT8 *) Buffer, &Status);
    break;
  case EfiSmbusReadWord:
    *(UINT16 *) Buffer = SmBusReadDataWord (SmbusAddress, &Status);
    break;
  case EfiSmbusWriteWord:
    SmBusWriteDataWord (SmbusAddress, *(UINT16 *) Buffer, &Status);
    break;
  case EfiSmbusProcessCall:
    *(UINT16 *) Buffer = SmBusProcessCall (SmbusAddress, *(UINT16 *) Buffer, &Status);
    break;
  case EfiSmbusReadBlock:
    WorkBufferLen = SmBusReadBlock (SmbusAddress, WorkBuffer, &Status);
    if (!EFI_ERROR (Status)) {
      //
      // Read block transaction is complete successfully, and then
      // check whether the output buffer is large enough.
      //
      if (*Length >= WorkBufferLen) {
        CopyMem (Buffer, WorkBuffer, WorkBufferLen);
      } else {
        Status = EFI_BUFFER_TOO_SMALL;
      }
      *Length = WorkBufferLen;
    }
    break;
  case EfiSmbusWriteBlock:
    SmBusWriteBlock (ADD_LENGTH (SmbusAddress, *Length), Buffer, &Status);
    break;
  default:
    break;
  }

  return Status;
}

