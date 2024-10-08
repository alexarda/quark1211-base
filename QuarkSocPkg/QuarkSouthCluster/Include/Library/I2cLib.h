/** @file

  Intel I2C library implementation built upon I/O library


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

#ifndef _I2C_LIB_H_
#define _I2C_LIB_H_

#include "I2cRegs.h"

/**

  The I2cWriteByte() function is a wrapper function for the WriteByte() function.
  Provides a standard way to execute a standard single byte write to an IC2 device
  (without accessing sub-addresses), as defined in the I2C Specification.

  @param SlaveAddress The I2C slave address of the device
                      with which to communicate.

  @param AddrMode     I2C Addressing Mode: 7-bit or 10-bit address.

  @param Buffer       Contains the value of byte data to execute to the
                      I2C slave device.


  @retval EFI_SUCCESS           Transfer success.
  @retval EFI_INVALID_PARAMETER  This or Buffer pointers are invalid.
  @retval EFI_TIMEOUT           Timeout while waiting xfer.
  @retval EFI_ABORTED           Controller aborted xfer.
  @retval EFI_DEVICE_ERROR      Device error detected by controller.

**/
EFI_STATUS
EFIAPI
I2cWriteByte (
  IN        EFI_I2C_DEVICE_ADDRESS  SlaveAddress,
  IN        EFI_I2C_ADDR_MODE       AddrMode,
  IN OUT VOID                       *Buffer
  );

/**

  The I2cReadByte() function is a wrapper function for the ReadByte() function.
  Provides a standard way to execute a standard single byte read to an I2C device
  (without accessing sub-addresses), as defined in the I2C Specification.

  @param SlaveAddress The I2C slave address of the device
                      with which to communicate.

  @param AddrMode     I2C Addressing Mode: 7-bit or 10-bit address.

  @param Buffer       Contains the value of byte data read from the
                      I2C slave device.


  @retval EFI_SUCCESS           Transfer success.
  @retval EFI_INVALID_PARAMETER This or Buffer pointers are invalid.
  @retval EFI_TIMEOUT           Timeout while waiting xfer.
  @retval EFI_ABORTED           Controller aborted xfer.
  @retval EFI_DEVICE_ERROR      Device error detected by controller.

**/
EFI_STATUS
EFIAPI
I2cReadByte (
  IN        EFI_I2C_DEVICE_ADDRESS  SlaveAddress,
  IN        EFI_I2C_ADDR_MODE       AddrMode,
  IN OUT VOID                       *Buffer
  );

/**

  The I2cWriteMultipleByte() function is a wrapper function for the WriteMultipleByte()
  function. Provides a standard way to execute multiple byte writes to an I2C device (e.g. when
  accessing sub-addresses or writing block of data), as defined in the I2C Specification.

  @param SlaveAddress The I2C slave address of the device
                      with which to communicate.

  @param AddrMode     I2C Addressing Mode: 7-bit or 10-bit address.

  @param Length       No. of bytes to be written.

  @param Buffer       Contains the value of byte to be written to the
                      I2C slave device.

  @retval EFI_SUCCESS            Transfer success.
  @retval EFI_INVALID_PARAMETER  This, Length or Buffer pointers are invalid.
  @retval EFI_UNSUPPORTED        Unsupported input param.
  @retval EFI_TIMEOUT            Timeout while waiting xfer.
  @retval EFI_ABORTED            Controller aborted xfer.
  @retval EFI_DEVICE_ERROR       Device error detected by controller.

**/
EFI_STATUS
EFIAPI
I2cWriteMultipleByte (
  IN        EFI_I2C_DEVICE_ADDRESS  SlaveAddress,
  IN        EFI_I2C_ADDR_MODE       AddrMode,
  IN UINTN                          *Length,
  IN OUT VOID                       *Buffer
  );

/**

  The I2cReadMultipleByte() function is a wrapper function for the ReadMultipleByte
  function. Provides a standard way to execute multiple byte writes to an IC2 device
  (e.g. when accessing sub-addresses or when reading block of data), as defined
  in the I2C Specification (I2C combined write/read protocol).

  @param SlaveAddress The I2C slave address of the device
                      with which to communicate.

  @param AddrMode     I2C Addressing Mode: 7-bit or 10-bit address.

  @param WriteLength  No. of bytes to be written. In this case data
                      written typically contains sub-address or sub-addresses
                      in Hi-Lo format, that need to be read (I2C combined
                      write/read protocol).

  @param ReadLength   No. of bytes to be read from I2C slave device.
                      need to be read.

  @param Buffer       Contains the value of byte data read from the
                      I2C slave device.

  @retval EFI_SUCCESS            Transfer success.
  @retval EFI_INVALID_PARAMETER  This, WriteLength, ReadLength or Buffer
                                 pointers are invalid.
  @retval EFI_UNSUPPORTED        Unsupported input param.
  @retval EFI_TIMEOUT            Timeout while waiting xfer.
  @retval EFI_ABORTED            Controller aborted xfer.
  @retval EFI_DEVICE_ERROR       Device error detected by controller.

**/
EFI_STATUS
EFIAPI
I2cReadMultipleByte (
  IN        EFI_I2C_DEVICE_ADDRESS  SlaveAddress,
  IN        EFI_I2C_ADDR_MODE       AddrMode,
  IN UINTN                          *WriteLength,
  IN UINTN                          *ReadLength,
  IN OUT VOID                       *Buffer
  );

#endif
