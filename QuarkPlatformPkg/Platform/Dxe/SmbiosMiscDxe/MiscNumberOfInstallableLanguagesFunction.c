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

  MiscNumberOfInstallableLanguagesFunction.c
  
Abstract: 

  This driver parses the mSmbiosMiscDataTable structure and reports
  any generated data.

--*/


#include "CommonHeader.h"

#include "SmbiosMisc.h"
/*++
  Check whether the language is supported for given HII handle

  @param   HiiHandle     The HII package list handle.
  @param   Offset        The offest of current lanague in the supported languages.
  @param   CurrentLang   The language code.

  @retval  TRUE          Supported.
  @retval  FALSE         Not Supported.

--*/
BOOLEAN
EFIAPI
CurrentLanguageMatch (
  IN  EFI_HII_HANDLE                   HiiHandle,
  OUT UINT16                           *Offset,
  OUT CHAR8                            *CurrentLang
  )
{
  CHAR8     *DefaultLang;
  CHAR8     *BestLanguage;
  CHAR8     *Languages;
  CHAR8     *MatchLang;
  CHAR8     *EndMatchLang;
  UINTN     CompareLength;
  BOOLEAN   LangMatch;
  
  Languages = HiiGetSupportedLanguages (HiiHandle);
  if (Languages == NULL) {
    return FALSE;
  }

  LangMatch = FALSE;
  CurrentLang  = GetEfiGlobalVariable (L"PlatformLang");
  DefaultLang  = (CHAR8 *) PcdGetPtr (PcdUefiVariableDefaultPlatformLang);
  BestLanguage = GetBestLanguage (
                   Languages,
                   FALSE,
                   (CurrentLang != NULL) ? CurrentLang : "",
                   DefaultLang,
                   NULL
                   );
  if (BestLanguage != NULL) {
    //
    // Find the best matching RFC 4646 language, compute the offset.
    //
    LangMatch = TRUE;
    CompareLength = AsciiStrLen (BestLanguage);
    for (MatchLang = Languages, (*Offset) = 0; *MatchLang != '\0'; (*Offset)++) {
      //
      // Seek to the end of current match language. 
      //
      for (EndMatchLang = MatchLang; *EndMatchLang != '\0' && *EndMatchLang != ';'; EndMatchLang++);
  
      if ((EndMatchLang == MatchLang + CompareLength) && AsciiStrnCmp(MatchLang, BestLanguage, CompareLength) == 0) {
        //
        // Find the current best Language in the supported languages
        //
        break;
      }
      //
      // best language match be in the supported language.
      //
      ASSERT (*EndMatchLang == ';');
      MatchLang = EndMatchLang + 1;
    }
    FreePool (BestLanguage);
  }

  FreePool (Languages);
  if (CurrentLang != NULL) {
    FreePool (CurrentLang);
  }
  return LangMatch;
}


/**
  Get next language from language code list (with separator ';').

  @param  LangCode       Input: point to first language in the list. On
                         Otput: point to next language in the list, or
                                NULL if no more language in the list.
  @param  Lang           The first language in the list.

**/
VOID
EFIAPI
GetNextLanguage (
  IN OUT CHAR8      **LangCode,
  OUT CHAR8         *Lang
  )
{
  UINTN  Index;
  CHAR8  *StringPtr;

  ASSERT (LangCode != NULL);
  ASSERT (*LangCode != NULL);
  ASSERT (Lang != NULL);

  Index     = 0;
  StringPtr = *LangCode;
  while (StringPtr[Index] != 0 && StringPtr[Index] != ';') {
    Index++;
  }

  CopyMem (Lang, StringPtr, Index);
  Lang[Index] = 0;

  if (StringPtr[Index] == ';') {
    Index++;
  }
  *LangCode = StringPtr + Index;
}

/**
  This function returns the number of supported languages on HiiHandle.

  @param   HiiHandle    The HII package list handle.

  @retval  The number of supported languages.

**/
UINT16
EFIAPI
GetSupportedLanguageNumber (
  IN EFI_HII_HANDLE    HiiHandle
  )
{
  CHAR8   *Lang;
  CHAR8   *Languages;
  CHAR8   *LanguageString;
  UINT16  LangNumber;
  
  Languages = HiiGetSupportedLanguages (HiiHandle);
  if (Languages == NULL) {
    return 0;
  }

  LangNumber = 0;
  Lang = AllocatePool (AsciiStrSize (Languages));
  if (Lang != NULL) {
    LanguageString = Languages;
    while (*LanguageString != 0) {
      GetNextLanguage (&LanguageString, Lang);
      LangNumber++;
    }
    FreePool (Lang);
  }
  FreePool (Languages);
  return LangNumber;
}


/**
  This function makes boot time changes to the contents of the
  MiscNumberOfInstallableLanguages (Type 13).

  @param  RecordData                 Pointer to copy of RecordData from the Data Table.

  @retval EFI_SUCCESS                All parameters were valid.
  @retval EFI_UNSUPPORTED            Unexpected RecordType value.
  @retval EFI_INVALID_PARAMETER      Invalid parameter was found.

**/
MISC_SMBIOS_TABLE_FUNCTION(NumberOfInstallableLanguages)
{
  UINTN                                     LangStrLen;
  CHAR8                                     CurrentLang[SMBIOS_STRING_MAX_LENGTH + 1];
  CHAR8                                     *OptionalStrStart;
  UINT16                                    Offset;
  BOOLEAN                                   LangMatch;
  EFI_STATUS                                Status;
  EFI_SMBIOS_HANDLE                         SmbiosHandle;
  SMBIOS_TABLE_TYPE13                       *SmbiosRecord;
  EFI_MISC_NUMBER_OF_INSTALLABLE_LANGUAGES  *ForType13InputData;
 
  ForType13InputData = (EFI_MISC_NUMBER_OF_INSTALLABLE_LANGUAGES *)RecordData;

  //
  // First check for invalid parameters.
  //
  if (RecordData == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  
  ForType13InputData->NumberOfInstallableLanguages = GetSupportedLanguageNumber (mHiiHandle);

  //
  // Try to check if current langcode matches with the langcodes in installed languages
  //
  LangMatch = FALSE;
  ZeroMem(CurrentLang, SMBIOS_STRING_MAX_LENGTH + 1);
  LangMatch = CurrentLanguageMatch (mHiiHandle, &Offset, CurrentLang);
  LangStrLen = AsciiStrLen(CurrentLang);

  //
  // Two zeros following the last string.
  //
  SmbiosRecord = AllocatePool(sizeof (SMBIOS_TABLE_TYPE13) + LangStrLen + 1 + 1);
  ZeroMem(SmbiosRecord, sizeof (SMBIOS_TABLE_TYPE13) + LangStrLen + 1 + 1);

  SmbiosRecord->Hdr.Type = EFI_SMBIOS_TYPE_BIOS_LANGUAGE_INFORMATION;
  SmbiosRecord->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE13);
  //
  // Make handle chosen by smbios protocol.add automatically.
  //
  SmbiosRecord->Hdr.Handle = 0;  

  SmbiosRecord->InstallableLanguages = (UINT8)ForType13InputData->NumberOfInstallableLanguages;
  SmbiosRecord->Flags = (UINT8)ForType13InputData->LanguageFlags.AbbreviatedLanguageFormat;
  SmbiosRecord->CurrentLanguages = 1;
  OptionalStrStart = (CHAR8 *)(SmbiosRecord + 1);
  AsciiStrCpy(OptionalStrStart, CurrentLang);
  //
  // Now we have got the full smbios record, call smbios protocol to add this record.
  //
  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
  Status = Smbios-> Add(
                      Smbios, 
                      NULL,
                      &SmbiosHandle, 
                      (EFI_SMBIOS_TABLE_HEADER *) SmbiosRecord
                      );
  FreePool(SmbiosRecord);
  return Status;
}
