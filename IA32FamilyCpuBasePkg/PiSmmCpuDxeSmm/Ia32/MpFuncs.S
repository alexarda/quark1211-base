#-------------------------------------------------------------------------------
#
# Copyright (c) 2013-2016 Intel Corporation.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in
# the documentation and/or other materials provided with the
# distribution.
# * Neither the name of Intel Corporation nor the names of its
# contributors may be used to endorse or promote products derived
# from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
#
# Module Name:
#
#   MpFuncs.S
# 
# Abstract:
# 
#   This is the assembly code for Multi-processor S3 support
#
#-------------------------------------------------------------------------------

.equ                   VacantFlag,       0x0
.equ                   NotVacantFlag,    0xff

.equ                   LockLocation,     RendezvousFunnelProcEnd - RendezvousFunnelProcStart
.equ                   StackStart,       RendezvousFunnelProcEnd - RendezvousFunnelProcStart + 0x04
.equ                   StackSize,        RendezvousFunnelProcEnd - RendezvousFunnelProcStart + 0x08
.equ                   RendezvousProc,   RendezvousFunnelProcEnd - RendezvousFunnelProcStart + 0x0C
.equ                   GdtrProfile,      RendezvousFunnelProcEnd - RendezvousFunnelProcStart + 0x10
.equ                   IdtrProfile,      RendezvousFunnelProcEnd - RendezvousFunnelProcStart + 0x16
.equ                   BufferStart,      RendezvousFunnelProcEnd - RendezvousFunnelProcStart + 0x1C

#-------------------------------------------------------------------------------------
#RendezvousFunnelProc  procedure follows. All APs execute their procedure. This
#procedure serializes all the AP processors through an Init sequence. It must be
#noted that APs arrive here very raw...ie: real mode, no stack.
#ALSO THIS PROCEDURE IS EXECUTED BY APs ONLY ON 16 BIT MODE. HENCE THIS PROC
#IS IN MACHINE CODE.
#-------------------------------------------------------------------------------------
#RendezvousFunnelProc (&WakeUpBuffer,MemAddress);

ASM_GLOBAL ASM_PFX(RendezvousFunnelProc)
ASM_PFX(RendezvousFunnelProc):
RendezvousFunnelProcStart:

# At this point CS = 0x(vv00) and ip= 0x0.

        .byte 0x8c,0xc8               # mov        ax,  cs
        .byte 0x8e,0xd8               # mov        ds,  ax
        .byte 0x8e,0xc0               # mov        es,  ax
        .byte 0x8e,0xd0               # mov        ss,  ax 
        .byte 0x33,0xc0               # xor        ax,  ax
        .byte 0x8e,0xe0               # mov        fs,  ax
        .byte 0x8e,0xe8               # mov        gs,  ax

flat32Start:

        .byte 0xBE
        .word BufferStart
        .byte 0x66,0x8B,0x14          # mov        edx,dword ptr [si]          ; EDX is keeping the start address of wakeup buffer

        .byte 0xBE
        .word GdtrProfile
        .byte 0x66                    # db         66h
        .byte 0x2E,0xF,0x1,0x14       # lgdt       fword ptr cs:[si]

        .byte 0xBE
        .word IdtrProfile
        .byte 0x66                    # db         66h
        .byte 0x2E,0xF,0x1,0x1C       # lidt       fword ptr cs:[si]

        .byte 0x33,0xC0               # xor        ax,  ax
        .byte 0x8E,0xD8               # mov        ds,  ax

        .byte 0xF,0x20,0xC0           # mov        eax, cr0                    ; Get control register 0
        .byte 0x66,0x83,0xC8,0x1      # or         eax, 000000001h             ; Set PE bit (bit #0)
        .byte 0xF,0x22,0xC0           # mov        cr0, eax

FLAT32_JUMP:

        .byte 0x66,0x67,0xEA          # far jump
        .long 0x0                     # 32-bit offset
        .word 0x20                    # 16-bit selector

PMODE_ENTRY:                          # protected mode entry point

        movw        $0x8,%ax
        .byte       0x66
        movw        %ax,%ds
        .byte       0x66
        movw        %ax,%es
        .byte       0x66
        movw        %ax,%fs
        .byte       0x66
        movw        %ax,%gs
        .byte       0x66
        movw        %ax,%ss           # Flat mode setup.

        movl        %edx,%esi

        movl        %esi,%edi
        addl        $LockLocation, %edi
        movb        $NotVacantFlag, %al
TestLock:
        xchgb       (%edi), %al
        cmpb        $NotVacantFlag, %al
        jz          TestLock

ProgramStack:

        movl        %esi,%edi
        addl        $StackSize, %edi
        movl        (%edi),%eax
        movl        %esi,%edi
        addl        $StackStart, %edi
        addl        (%edi),%eax
        movl        %eax,%esp
        movl        %eax,(%edi)

Releaselock:

        movb        $VacantFlag, %al
        movl        %esi,%edi
        addl        $LockLocation, %edi
        xchgb       (%edi), %al

        #
        # Call assembly function to initialize FPU.
        #
        lea         ASM_PFX(InitializeFloatingPointUnits), %ebx
        call        *%ebx
        #
        # Call C Function
        #
        movl        %esi,%edi
        addl        $RendezvousProc, %edi
        movl        (%edi),%eax

        testl       %eax,%eax
        jz          GoToSleep
        call        *%eax                         # Call C function

GoToSleep:
        cli
        hlt
        jmp         GoToSleep

RendezvousFunnelProcEnd:
#-------------------------------------------------------------------------------------
#  AsmGetAddressMap (&AddressMap);
#-------------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(AsmGetAddressMap)
ASM_PFX(AsmGetAddressMap):

        pushal
        movl        %esp,%ebp

        movl        0x24(%ebp), %ebx
        movl        $RendezvousFunnelProcStart, (%ebx)
        movl        $(PMODE_ENTRY - RendezvousFunnelProcStart), 0x4(%ebx)
        movl        $(FLAT32_JUMP - RendezvousFunnelProcStart), 0x8(%ebx)
        movl        $(RendezvousFunnelProcEnd - RendezvousFunnelProcStart), 0x0c(%ebx)

        popal
        ret
