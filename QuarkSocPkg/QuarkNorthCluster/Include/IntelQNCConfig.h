/** @file
	Some configuration of QNC Package

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

#ifndef __INTEL_QNC_CONFIG_H__
#define __INTEL_QNC_CONFIG_H__

//
// QNC Fixed configurations.
//

//
// Memory arbiter fixed config values.
//
#define QNC_FIXED_CONFIG_ASTATUS  ((UINT32) (\
          (ASTATUS_PRI_NORMAL << ASTATUS0_DEFAULT_BP) | \
          (ASTATUS_PRI_NORMAL << ASTATUS1_DEFAULT_BP) | \
          (ASTATUS_PRI_URGENT << ASTATUS0_RASISED_BP) | \
          (ASTATUS_PRI_URGENT << ASTATUS1_RASISED_BP) \
          ))

//
// Memory Manager fixed config values.
//
#define V_DRAM_NON_HOST_RQ_LIMIT                    2

//
// RMU Thermal config fixed config values for TS in Vref Mode.
//
#define V_TSCGF1_CONFIG_ISNSCURRENTSEL_VREF_MODE    0x04
#define V_TSCGF2_CONFIG2_ISPARECTRL_VREF_MODE       0x01
#define V_TSCGF1_CONFIG_IBGEN_VREF_MODE             1
#define V_TSCGF2_CONFIG_IDSCONTROL_VREF_MODE        0x011b
#define V_TSCGF2_CONFIG2_ICALCOARSETUNE_VREF_MODE   0x34

//
// RMU Thermal config fixed config values for TS in Ratiometric mode.
//
#define V_TSCGF1_CONFIG_ISNSCURRENTSEL_RATIO_MODE   0x04
#define V_TSCGF1_CONFIG_ISNSCHOPSEL_RATIO_MODE      0x02
#define V_TSCGF1_CONFIG_ISNSINTERNALVREFEN_RATIO_MODE 1
#define V_TSCGF2_CONFIG_IDSCONTROL_RATIO_MODE       0x011f
#define V_TSCGF2_CONFIG_IDSTIMING_RATIO_MODE        0x0001
#define V_TSCGF2_CONFIG2_ICALCONFIGSEL_RATIO_MODE   0x01
#define V_TSCGF2_CONFIG2_ISPARECTRL_RATIO_MODE      0x00
#define V_TSCGF1_CONFIG_IBGEN_RATIO_MODE            0
#define V_TSCGF1_CONFIG_IBGCHOPEN_RATIO_MODE        0
#define V_TSCGF3_CONFIG_ITSGAMMACOEFF_RATIO_MODE    0xC8
#define V_TSCGF2_CONFIG2_ICALCOARSETUNE_RATIO_MODE  0x17

//
// iCLK fixed config values.
//
#define V_MUXTOP_FLEX2                              3
#define V_MUXTOP_FLEX1                              1

//
// PCIe Root Port fixed config values.
//
#define V_PCIE_ROOT_PORT_SBIC_VALUE                 (B_QNC_PCIE_IOSFSBCTL_SBIC_IDLE_NEVER)

//
// QNC structures for configuration.
//

typedef union {
  struct {
    UINT32  PortErrorMask               :8;
    UINT32  SlotImplemented             :1;
    UINT32  Reserved1                   :1;
    UINT32  AspmEnable                  :1;
    UINT32  AspmAutoEnable              :1;
    UINT32  AspmL0sEnable               :2;
    UINT32  AspmL1Enable                :1;
    UINT32  PmeInterruptEnable          :1;    
    UINT32  PhysicalSlotNumber          :13;    
    UINT32  Reserved2                   :1; 
    UINT32  PmSciEnable                 :1;   
    UINT32  HotplugSciEnable            :1;
  } Bits;
  UINT32 Uint32;
} PCIEXP_ROOT_PORT_CONFIGURATION;

typedef union {
  UINT32 Uint32;
  struct {
	  UINT32 Pcie_0     :1;   // 0: Disabled; 1: Enabled*
	  UINT32 Pcie_1     :1;   // 0: Disabled; 1: Enabled*
	  UINT32 Smbus      :1;   // 0: Disabled; 1: Enabled*
	  UINT32 Rsvd       :29;  // 0
  } Bits;
} QNC_DEVICE_ENABLES;

#endif

