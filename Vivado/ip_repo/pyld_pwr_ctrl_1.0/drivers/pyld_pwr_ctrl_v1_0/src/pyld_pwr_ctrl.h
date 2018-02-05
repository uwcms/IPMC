
#ifndef Pyld_Pwr_Ctrl_H
#define Pyld_Pwr_Ctrl_H

/****************** Include Files ********************/
#include "xil_types.h"
#include "xstatus.h"


#define CORE_VER_REG 			 0
#define SW_OFF_REG 				 4
#define PD_INIT_REG 			 8
#define PU_INIT_REG 			12
#define PE_STATUS_REG 			16
#define PG_STATUS_REG 			20

#define PE_0_MASTER_CFG_REG     32
#define PE_0_SEQ_TMR_CFG_REG    36

#define PE_2_PE_ADDR_OFFSET      8

#define SW_OFF_MAGIC_WORD      0xC0DEa0FF


/**************************** Type Definitions *****************************/

/**
 * This typedef contains configuration information for the device.
 */
typedef struct {
    u16 DeviceId; /* Unique ID of device */
    UINTPTR BaseAddress; /* Device base address */
    u32 PECount; /* Number of available Power Enable output pins */
    u32 PGCount; /* Number of available Power Good input pins */
} Pyld_Pwr_Ctrl_Config_t;

/**
 * The Pyld_Pwr_Ctrl driver instance data. The user is required to allocate a
 * variable of this type for every Pyld_Pwr_Ctrl device in the system. A pointer
 * to a variable of this type is then passed to the driver API functions.
 **/
typedef struct {
    UINTPTR BaseAddress;    /* Device base address */
    u32 IsReady;        /* Device is initialized and ready */
    u32 PECount; /* Number of available Power Enable output pins */
    u32 PGCount; /* Number of available Power Good input pins */
} Pyld_Pwr_Ctrl_t;

/**
 * Power Enable Pin Configuration
 **/
typedef struct {
    u32 group;        /* power enable group  */
    u32 seq_tmr;      /* power up/down sequence timer config, in microseconds */
    bool sw_pd_en;    /* enable emergency power down triggered by SW */
    bool ext_pd_en;   /* enable emergency power down triggered by PL FW */
} PE_cfg_t;

/*
 * API Basic functions implemented in axi_Pyld_Pwr_Ctrl.c
 */
int Pyld_Pwr_Ctrl_Initialize(Pyld_Pwr_Ctrl_t *InstancePtr, u16 DeviceId);
Pyld_Pwr_Ctrl_Config_t *Pyld_Pwr_Ctrl_LookupConfig(u16 DeviceId);

int Pyld_Pwr_Ctrl_CfgInitialize(Pyld_Pwr_Ctrl_t *InstancePtr, Pyld_Pwr_Ctrl_Config_t * Config, UINTPTR EffectiveAddr);

u32 Pyld_Pwr_Ctrl_Get_Core_Ver(Pyld_Pwr_Ctrl_t *InstancePtr);
u32 Pyld_Pwr_Ctrl_Get_PE_Cnt(Pyld_Pwr_Ctrl_t *InstancePtr);
u32 Pyld_Pwr_Ctrl_Get_PG_Cnt(Pyld_Pwr_Ctrl_t *InstancePtr);

void Pyld_Pwr_Ctrl_Get_Pin_Cfg(Pyld_Pwr_Ctrl_t *InstancePtr, u32 pin, PE_cfg_t * PE_cfg);
void Pyld_Pwr_Ctrl_Set_Pin_Cfg(Pyld_Pwr_Ctrl_t *InstancePtr, u32 pin, PE_cfg_t PE_cfg);

void Pyld_Pwr_Ctrl_PDown_Force(Pyld_Pwr_Ctrl_t *InstancePtr);
void Pyld_Pwr_Ctrl_PDown_Release(Pyld_Pwr_Ctrl_t *InstancePtr);

void Pyld_Pwr_Ctrl_Init_PDown_Seq(Pyld_Pwr_Ctrl_t *InstancePtr, u32 group);
void Pyld_Pwr_Ctrl_Init_PUp_Seq(Pyld_Pwr_Ctrl_t *InstancePtr, u32 group);

#endif // Pyld_Pwr_Ctrl_H
