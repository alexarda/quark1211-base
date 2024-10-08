/** @file
  Provides services to print debug and assert messages to a debug output device.
  
  The Debug library supports debug print and asserts based on a combination of macros and code.
  The debug library can be turned on and off so that the debug code does not increase the size of an image.

  Note that a reserved macro named MDEPKG_NDEBUG is introduced for the intention
  of size reduction when compiler optimization is disabled. If MDEPKG_NDEBUG is
  defined, then debug and assert related macros wrapped by it are the NULL implementations.

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

#ifndef __DEBUG_LIB_H__
#define __DEBUG_LIB_H__

//
// Declare bits for PcdDebugPropertyMask
//
#define DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED       0x01
#define DEBUG_PROPERTY_DEBUG_PRINT_ENABLED        0x02
#define DEBUG_PROPERTY_DEBUG_CODE_ENABLED         0x04
#define DEBUG_PROPERTY_CLEAR_MEMORY_ENABLED       0x08
#define DEBUG_PROPERTY_ASSERT_BREAKPOINT_ENABLED  0x10
#define DEBUG_PROPERTY_ASSERT_DEADLOOP_ENABLED    0x20

//
// Declare bits for PcdDebugPrintErrorLevel and the ErrorLevel parameter of DebugPrint()
//
#define DEBUG_INIT      0x00000001  // Initialization
#define DEBUG_WARN      0x00000002  // Warnings
#define DEBUG_LOAD      0x00000004  // Load events
#define DEBUG_FS        0x00000008  // EFI File system
#define DEBUG_POOL      0x00000010  // Alloc & Free's
#define DEBUG_PAGE      0x00000020  // Alloc & Free's
#define DEBUG_INFO      0x00000040  // Informational debug messages
#define DEBUG_DISPATCH  0x00000080  // PEI/DXE/SMM Dispatchers
#define DEBUG_VARIABLE  0x00000100  // Variable
#define DEBUG_BM        0x00000400  // Boot Manager
#define DEBUG_BLKIO     0x00001000  // BlkIo Driver
#define DEBUG_NET       0x00004000  // SNI Driver
#define DEBUG_UNDI      0x00010000  // UNDI Driver
#define DEBUG_LOADFILE  0x00020000  // UNDI Driver
#define DEBUG_EVENT     0x00080000  // Event messages
#define DEBUG_GCD       0x00100000  // Global Coherency Database changes
#define DEBUG_CACHE     0x00200000  // Memory range cachability changes
#define DEBUG_VERBOSE   0x00400000  // Detailed debug messages that may significantly impact boot performance
#define DEBUG_ERROR     0x80000000  // Error

//
// Aliases of debug message mask bits
//
#define EFI_D_INIT      DEBUG_INIT
#define EFI_D_WARN      DEBUG_WARN
#define EFI_D_LOAD      DEBUG_LOAD
#define EFI_D_FS        DEBUG_FS
#define EFI_D_POOL      DEBUG_POOL
#define EFI_D_PAGE      DEBUG_PAGE
#define EFI_D_INFO      DEBUG_INFO
#define EFI_D_DISPATCH  DEBUG_DISPATCH
#define EFI_D_VARIABLE  DEBUG_VARIABLE
#define EFI_D_BM        DEBUG_BM
#define EFI_D_BLKIO     DEBUG_BLKIO
#define EFI_D_NET       DEBUG_NET
#define EFI_D_UNDI      DEBUG_UNDI
#define EFI_D_LOADFILE  DEBUG_LOADFILE
#define EFI_D_EVENT     DEBUG_EVENT
#define EFI_D_VERBOSE   DEBUG_VERBOSE
#define EFI_D_ERROR     DEBUG_ERROR

/**
  Prints a debug message to the debug output device if the specified error level is enabled.

  If any bit in ErrorLevel is also set in DebugPrintErrorLevelLib function 
  GetDebugPrintErrorLevel (), then print the message specified by Format and the 
  associated variable argument list to the debug output device.

  If Format is NULL, then ASSERT().

  @param  ErrorLevel  The error level of the debug message.
  @param  Format      The format string for the debug message to print.
  @param  ...         The variable argument list whose contents are accessed 
                      based on the format string specified by Format.

**/
VOID
EFIAPI
DebugPrint (
  IN  UINTN        ErrorLevel,
  IN  CONST CHAR8  *Format,
  ...
  );


/**
  Prints an assert message containing a filename, line number, and description.  
  This may be followed by a breakpoint or a dead loop.

  Print a message of the form "ASSERT <FileName>(<LineNumber>): <Description>\n"
  to the debug output device.  If DEBUG_PROPERTY_ASSERT_BREAKPOINT_ENABLED bit of 
  PcdDebugProperyMask is set then CpuBreakpoint() is called. Otherwise, if 
  DEBUG_PROPERTY_ASSERT_DEADLOOP_ENABLED bit of PcdDebugProperyMask is set then 
  CpuDeadLoop() is called.  If neither of these bits are set, then this function 
  returns immediately after the message is printed to the debug output device.
  DebugAssert() must actively prevent recursion.  If DebugAssert() is called while
  processing another DebugAssert(), then DebugAssert() must return immediately.

  If FileName is NULL, then a <FileName> string of "(NULL) Filename" is printed.
  If Description is NULL, then a <Description> string of "(NULL) Description" is printed.

  @param  FileName     The pointer to the name of the source file that generated the assert condition.
  @param  LineNumber   The line number in the source file that generated the assert condition
  @param  Description  The pointer to the description of the assert condition.

**/
VOID
EFIAPI
DebugAssert (
  IN CONST CHAR8  *FileName,
  IN UINTN        LineNumber,
  IN CONST CHAR8  *Description
  );


/**
  Fills a target buffer with PcdDebugClearMemoryValue, and returns the target buffer.

  This function fills Length bytes of Buffer with the value specified by 
  PcdDebugClearMemoryValue, and returns Buffer.

  If Buffer is NULL, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT(). 

  @param   Buffer  The pointer to the target buffer to be filled with PcdDebugClearMemoryValue.
  @param   Length  The number of bytes in Buffer to fill with zeros PcdDebugClearMemoryValue. 

  @return  Buffer  The pointer to the target buffer filled with PcdDebugClearMemoryValue.

**/
VOID *
EFIAPI
DebugClearMemory (
  OUT VOID  *Buffer,
  IN UINTN  Length
  );


/**
  Returns TRUE if ASSERT() macros are enabled.

  This function returns TRUE if the DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED bit of 
  PcdDebugProperyMask is set.  Otherwise, FALSE is returned.

  @retval  TRUE    The DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED bit of PcdDebugProperyMask is set.
  @retval  FALSE   The DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED bit of PcdDebugProperyMask is clear.

**/
BOOLEAN
EFIAPI
DebugAssertEnabled (
  VOID
  );


/**  
  Returns TRUE if DEBUG() macros are enabled.

  This function returns TRUE if the DEBUG_PROPERTY_DEBUG_PRINT_ENABLED bit of 
  PcdDebugProperyMask is set.  Otherwise, FALSE is returned.

  @retval  TRUE    The DEBUG_PROPERTY_DEBUG_PRINT_ENABLED bit of PcdDebugProperyMask is set.
  @retval  FALSE   The DEBUG_PROPERTY_DEBUG_PRINT_ENABLED bit of PcdDebugProperyMask is clear.

**/
BOOLEAN
EFIAPI
DebugPrintEnabled (
  VOID
  );


/**  
  Returns TRUE if DEBUG_CODE() macros are enabled.

  This function returns TRUE if the DEBUG_PROPERTY_DEBUG_CODE_ENABLED bit of 
  PcdDebugProperyMask is set.  Otherwise, FALSE is returned.

  @retval  TRUE    The DEBUG_PROPERTY_DEBUG_CODE_ENABLED bit of PcdDebugProperyMask is set.
  @retval  FALSE   The DEBUG_PROPERTY_DEBUG_CODE_ENABLED bit of PcdDebugProperyMask is clear.

**/
BOOLEAN
EFIAPI
DebugCodeEnabled (
  VOID
  );


/**  
  Returns TRUE if DEBUG_CLEAR_MEMORY() macro is enabled.

  This function returns TRUE if the DEBUG_PROPERTY_CLEAR_MEMORY_ENABLED bit of 
  PcdDebugProperyMask is set.  Otherwise, FALSE is returned.

  @retval  TRUE    The DEBUG_PROPERTY_CLEAR_MEMORY_ENABLED bit of PcdDebugProperyMask is set.
  @retval  FALSE   The DEBUG_PROPERTY_CLEAR_MEMORY_ENABLED bit of PcdDebugProperyMask is clear.

**/
BOOLEAN
EFIAPI
DebugClearMemoryEnabled (
  VOID
  );


/**  
  Internal worker macro that calls DebugAssert().

  This macro calls DebugAssert(), passing in the filename, line number, and an
  expression that evaluated to FALSE.

  @param  Expression  Boolean expression that evaluated to FALSE

**/
//BugID31643 - Start
// #define _ASSERT(Expression)  DebugAssert (__FILE__, __LINE__, #Expression)
#if !defined(__GNUC__)
#define _ASSERT(Expression)  DebugAssert (__FILE__, __LINE__, #Expression)
#else
VOID
EFIAPI
CpuDeadLoop (
  VOID
  );

#define _ASSERT(Expression)  CpuDeadLoop()
#endif
//BugID31643 - End

/**  
  Internal worker macro that calls DebugPrint().

  This macro calls DebugPrint() passing in the debug error level, a format 
  string, and a variable argument list.

  @param  Expression  Expression containing an error level, a format string, 
                      and a variable argument list based on the format string.

**/
#define _DEBUG(Expression)   DebugPrint Expression


/**  
  Macro that calls DebugAssert() if an expression evaluates to FALSE.

  If MDEPKG_NDEBUG is not defined and the DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED 
  bit of PcdDebugProperyMask is set, then this macro evaluates the Boolean 
  expression specified by Expression.  If Expression evaluates to FALSE, then 
  DebugAssert() is called passing in the source filename, source line number, 
  and Expression.

  @param  Expression  Boolean expression.

**/
#if !defined(MDEPKG_NDEBUG)       
  #define ASSERT(Expression)        \
    do {                            \
      if (DebugAssertEnabled ()) {  \
        if (!(Expression)) {        \
          _ASSERT (Expression);     \
        }                           \
      }                             \
    } while (FALSE)
#else
  #define ASSERT(Expression)
#endif

/**  
  Macro that calls DebugPrint().

  If MDEPKG_NDEBUG is not defined and the DEBUG_PROPERTY_DEBUG_PRINT_ENABLED 
  bit of PcdDebugProperyMask is set, then this macro passes Expression to 
  DebugPrint().

  @param  Expression  Expression containing an error level, a format string, 
                      and a variable argument list based on the format string.
  

**/
#if !defined(MDEPKG_NDEBUG)      
  #define DEBUG(Expression)        \
    do {                           \
      if (DebugPrintEnabled ()) {  \
        _DEBUG (Expression);       \
      }                            \
    } while (FALSE)
#else
  #define DEBUG(Expression)
#endif

/**  
  Macro that calls DebugAssert() if an EFI_STATUS evaluates to an error code.

  If MDEPKG_NDEBUG is not defined and the DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED 
  bit of PcdDebugProperyMask is set, then this macro evaluates the EFI_STATUS 
  value specified by StatusParameter.  If StatusParameter is an error code, 
  then DebugAssert() is called passing in the source filename, source line 
  number, and StatusParameter.

  @param  StatusParameter  EFI_STATUS value to evaluate.

**/
#if !defined(MDEPKG_NDEBUG)
  #define ASSERT_EFI_ERROR(StatusParameter)                                              \
    do {                                                                                 \
      if (DebugAssertEnabled ()) {                                                       \
        if (EFI_ERROR (StatusParameter)) {                                               \
          DEBUG ((EFI_D_ERROR, "\nASSERT_EFI_ERROR (Status = %r)\n", StatusParameter));  \
          _ASSERT (!EFI_ERROR (StatusParameter));                                        \
        }                                                                                \
      }                                                                                  \
    } while (FALSE)
#else
  #define ASSERT_EFI_ERROR(StatusParameter)
#endif

/**  
  Macro that calls DebugAssert() if a protocol is already installed in the 
  handle database.

  If MDEPKG_NDEBUG is defined or the DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED bit 
  of PcdDebugProperyMask is clear, then return.

  If Handle is NULL, then a check is made to see if the protocol specified by Guid 
  is present on any handle in the handle database.  If Handle is not NULL, then 
  a check is made to see if the protocol specified by Guid is present on the 
  handle specified by Handle.  If the check finds the protocol, then DebugAssert() 
  is called passing in the source filename, source line number, and Guid.

  If Guid is NULL, then ASSERT().

  @param  Handle  The handle to check for the protocol.  This is an optional 
                  parameter that may be NULL.  If it is NULL, then the entire 
                  handle database is searched.

  @param  Guid    The pointer to a protocol GUID.

**/
#if !defined(MDEPKG_NDEBUG)
  #define ASSERT_PROTOCOL_ALREADY_INSTALLED(Handle, Guid)                               \
    do {                                                                                \
      if (DebugAssertEnabled ()) {                                                      \
        VOID  *Instance;                                                                \
        ASSERT (Guid != NULL);                                                          \
        if (Handle == NULL) {                                                           \
          if (!EFI_ERROR (gBS->LocateProtocol ((EFI_GUID *)Guid, NULL, &Instance))) {   \
            _ASSERT (Guid already installed in database);                               \
          }                                                                             \
        } else {                                                                        \
          if (!EFI_ERROR (gBS->HandleProtocol (Handle, (EFI_GUID *)Guid, &Instance))) { \
            _ASSERT (Guid already installed on Handle);                                 \
          }                                                                             \
        }                                                                               \
      }                                                                                 \
    } while (FALSE)
#else
  #define ASSERT_PROTOCOL_ALREADY_INSTALLED(Handle, Guid)
#endif

/**
  Macro that marks the beginning of debug source code.

  If the DEBUG_PROPERTY_DEBUG_CODE_ENABLED bit of PcdDebugProperyMask is set, 
  then this macro marks the beginning of source code that is included in a module.
  Otherwise, the source lines between DEBUG_CODE_BEGIN() and DEBUG_CODE_END() 
  are not included in a module.

**/
#define DEBUG_CODE_BEGIN()  do { if (DebugCodeEnabled ()) { UINT8  __DebugCodeLocal


/**  
  The macro that marks the end of debug source code.

  If the DEBUG_PROPERTY_DEBUG_CODE_ENABLED bit of PcdDebugProperyMask is set, 
  then this macro marks the end of source code that is included in a module.  
  Otherwise, the source lines between DEBUG_CODE_BEGIN() and DEBUG_CODE_END() 
  are not included in a module.

**/
#define DEBUG_CODE_END()    __DebugCodeLocal = 0; __DebugCodeLocal++; } } while (FALSE)


/**  
  The macro that declares a section of debug source code.

  If the DEBUG_PROPERTY_DEBUG_CODE_ENABLED bit of PcdDebugProperyMask is set, 
  then the source code specified by Expression is included in a module.  
  Otherwise, the source specified by Expression is not included in a module.

**/
#define DEBUG_CODE(Expression)  \
  DEBUG_CODE_BEGIN ();          \
  Expression                    \
  DEBUG_CODE_END ()


/**  
  The macro that calls DebugClearMemory() to clear a buffer to a default value.

  If the DEBUG_PROPERTY_CLEAR_MEMORY_ENABLED bit of PcdDebugProperyMask is set, 
  then this macro calls DebugClearMemory() passing in Address and Length.

  @param  Address  The pointer to a buffer.
  @param  Length   The number of bytes in the buffer to set.

**/
#define DEBUG_CLEAR_MEMORY(Address, Length)  \
  do {                                       \
    if (DebugClearMemoryEnabled ()) {        \
      DebugClearMemory (Address, Length);    \
    }                                        \
  } while (FALSE)


/**
  Macro that calls DebugAssert() if the containing record does not have a 
  matching signature.  If the signatures matches, then a pointer to the data 
  structure that contains a specified field of that data structure is returned.  
  This is a lightweight method hide information by placing a public data 
  structure inside a larger private data structure and using a pointer to the 
  public data structure to retrieve a pointer to the private data structure.

  If MDEPKG_NDEBUG is defined or the DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED bit 
  of PcdDebugProperyMask is clear, then this macro computes the offset, in bytes,
  of the field specified by Field from the beginning of the data structure specified 
  by TYPE.  This offset is subtracted from Record, and is used to return a pointer 
  to a data structure of the type specified by TYPE.

  If MDEPKG_NDEBUG is not defined and the DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED bit
  of PcdDebugProperyMask is set, then this macro computes the offset, in bytes, 
  of field specified by Field from the beginning of the data structure specified 
  by TYPE.  This offset is subtracted from Record, and is used to compute a pointer
  to a data structure of the type specified by TYPE.  The Signature field of the 
  data structure specified by TYPE is compared to TestSignature.  If the signatures 
  match, then a pointer to the pointer to a data structure of the type specified by 
  TYPE is returned.  If the signatures do not match, then DebugAssert() is called 
  with a description of "CR has a bad signature" and Record is returned.  

  If the data type specified by TYPE does not contain the field specified by Field, 
  then the module will not compile.

  If TYPE does not contain a field called Signature, then the module will not 
  compile.

  @param  Record         The pointer to the field specified by Field within a data 
                         structure of type TYPE.

  @param  TYPE           The name of the data structure type to return  This 
                         data structure must contain the field specified by Field. 

  @param  Field          The name of the field in the data structure specified 
                         by TYPE to which Record points.

  @param  TestSignature  The 32-bit signature value to match.

**/
#if !defined(MDEPKG_NDEBUG)
  #define CR(Record, TYPE, Field, TestSignature)                                              \
    (DebugAssertEnabled () && (BASE_CR (Record, TYPE, Field)->Signature != TestSignature)) ?  \
    (TYPE *) (_ASSERT (CR has Bad Signature), Record) :                                       \
    BASE_CR (Record, TYPE, Field)
#else
  #define CR(Record, TYPE, Field, TestSignature)                                              \
    BASE_CR (Record, TYPE, Field)
#endif
    
#endif
