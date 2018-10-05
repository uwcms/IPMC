

/***************************** Include Files *******************************/
#include "xparameters.h"
#include "xil_io.h"

#include "ipmi_sensor_proc.h"

/* IP Core register map */
#define RESET_REG 					(0)
#define IRQ_REQ_REG 				(4)
#define IRQ_ACK_REG 				(8)

#define CH_STAT_BASE_OFFSET			(256)

#define RAW_READING_REG 			(CH_STAT_BASE_OFFSET + 0 * 256)
#define THR_STATUS_REG 				(CH_STAT_BASE_OFFSET + 1 * 256)
#define EV_ASSERT_EN_REG 			(CH_STAT_BASE_OFFSET + 2 * 256)
#define EV_DEASSERT_EN_REG 			(CH_STAT_BASE_OFFSET + 3 * 256)
#define EV_ASSERT_REARM_REG 		(CH_STAT_BASE_OFFSET + 4 * 256)
#define EV_DEASSERT_REARM_REG 		(CH_STAT_BASE_OFFSET + 5 * 256)
#define EV_ASSERT_CUR_ST_REG 		(CH_STAT_BASE_OFFSET + 6 * 256)
#define EV_ASSERT_ST_REG 			(CH_STAT_BASE_OFFSET + 7 * 256)
#define EV_DEASSERT_ST_REG 			(CH_STAT_BASE_OFFSET + 8 * 256)


#define C_HYST_NEG  (7)
#define C_HYST_POS  (6)
#define C_UNR 		(5)
#define C_UCR 		(4)
#define C_UNC 		(3)
#define C_LNR 		(2)
#define C_LCR 		(1)
#define C_LNC 		(0)

#define CH_CONFIG_BASE_OFFSET		(4096)

#define HYST_POS_REG 				(CH_CONFIG_BASE_OFFSET + C_HYST_POS * 256)
#define HYST_NEG_REG 				(CH_CONFIG_BASE_OFFSET + C_HYST_NEG * 256)
#define UNR_REG 					(CH_CONFIG_BASE_OFFSET + C_UNR * 256)
#define UCR_REG 					(CH_CONFIG_BASE_OFFSET + C_UCR * 256)
#define UNC_REG 					(CH_CONFIG_BASE_OFFSET + C_UNC * 256)
#define LNR_REG 					(CH_CONFIG_BASE_OFFSET + C_LNR * 256)
#define LCR_REG 					(CH_CONFIG_BASE_OFFSET + C_LCR * 256)
#define LNC_REG 					(CH_CONFIG_BASE_OFFSET + C_LNC * 256)


/************************** Function Definitions ***************************/
extern IPMI_SENSOR_PROC_Config IPMI_SENSOR_PROC_ConfigTable[];

/************************** Function Definitions ***************************/

#define IPMI_Sensor_Proc_WriteReg(BaseAddress, RegOffset, Data) \
	Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))

#define IPMI_Sensor_Proc_ReadReg(BaseAddress, RegOffset) \
	Xil_In32((BaseAddress) + (RegOffset))

IPMI_SENSOR_PROC_Config *IPMI_Sensor_Proc_LookupConfig(u16 DeviceId)
{
	IPMI_SENSOR_PROC_Config *CfgPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_IPMI_SENSOR_PROC_NUM_INSTANCES; Index++) {
		if (IPMI_SENSOR_PROC_ConfigTable[Index].DeviceId == DeviceId) {
			CfgPtr = &IPMI_SENSOR_PROC_ConfigTable[Index];
			break;
		}
	}
	return CfgPtr;
}

int IPMI_Sensor_Proc_CfgInitialize(IPMI_Sensor_Proc * InstancePtr, IPMI_SENSOR_PROC_Config * Config,
			UINTPTR EffectiveAddr)
{
	/* Assert arguments */
	Xil_AssertNonvoid(InstancePtr != NULL);

	/* Set some default values. */
	InstancePtr->BaseAddress = EffectiveAddr;

	InstancePtr->sensor_ch_cnt = Config->SENSOR_CH_CNT;
	InstancePtr->sensor_data_width = Config->SENSOR_DATA_WIDTH;

	/*
	 * Indicate the instance is now ready to use, initialized without error
	 */
	InstancePtr->IsReady = XIL_COMPONENT_IS_READY;
	return (XST_SUCCESS);
}

XStatus IPMI_Sensor_Proc_Initialize(IPMI_Sensor_Proc *InstancePtr, u16 DeviceId)
{

	IPMI_SENSOR_PROC_Config *ConfigPtr;

	/*
	 * Assert arguments
	 */
	Xil_AssertNonvoid(InstancePtr != NULL);

	/*
	 * Lookup configuration data in the device configuration table.
	 * Use this configuration info down below when initializing this
	 * driver.
	 */
	ConfigPtr = IPMI_Sensor_Proc_LookupConfig(DeviceId);

	if (ConfigPtr == (IPMI_SENSOR_PROC_Config *) NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return IPMI_Sensor_Proc_CfgInitialize(InstancePtr, ConfigPtr, ConfigPtr->BaseAddress);
}

void IPMI_Sensor_Proc_Reset(IPMI_Sensor_Proc *InstancePtr)
{
	IPMI_Sensor_Proc_WriteReg(InstancePtr->BaseAddress, RESET_REG, 1);
	IPMI_Sensor_Proc_WriteReg(InstancePtr->BaseAddress, RESET_REG, 0);
}


XStatus IPMI_Sensor_Proc_Set_Hyst(const IPMI_Sensor_Proc *InstancePtr, const u32 ch, const Hyst_Cfg * cfg)
{
	if (ch > (InstancePtr->sensor_ch_cnt-1))
		return XST_INVALID_PARAM;

	IPMI_Sensor_Proc_WriteReg(InstancePtr->BaseAddress, HYST_POS_REG + ch * 4, cfg->hyst_pos);
	IPMI_Sensor_Proc_WriteReg(InstancePtr->BaseAddress, HYST_NEG_REG + ch * 4, cfg->hyst_neg);

	return XST_SUCCESS;
}


XStatus IPMI_Sensor_Proc_Get_Hyst(const IPMI_Sensor_Proc *InstancePtr, const u32 ch, Hyst_Cfg * cfg)
{
	if (ch > (InstancePtr->sensor_ch_cnt-1))
		return XST_INVALID_PARAM;

	cfg->hyst_pos = IPMI_Sensor_Proc_ReadReg(InstancePtr->BaseAddress, HYST_POS_REG + ch * 4);
	cfg->hyst_neg = IPMI_Sensor_Proc_ReadReg(InstancePtr->BaseAddress, HYST_NEG_REG + ch * 4);

	return XST_SUCCESS;
}


XStatus IPMI_Sensor_Proc_Set_Thr(const IPMI_Sensor_Proc *InstancePtr, const u32 ch, const Thr_Cfg * cfg)
{
	if (ch > (InstancePtr->sensor_ch_cnt-1))
		return XST_INVALID_PARAM;

	IPMI_Sensor_Proc_WriteReg(InstancePtr->BaseAddress, UNR_REG + ch * 4, cfg->UNR);
	IPMI_Sensor_Proc_WriteReg(InstancePtr->BaseAddress, UCR_REG + ch * 4, cfg->UCR);
	IPMI_Sensor_Proc_WriteReg(InstancePtr->BaseAddress, UNC_REG + ch * 4, cfg->UNC);
	IPMI_Sensor_Proc_WriteReg(InstancePtr->BaseAddress, LNR_REG + ch * 4, cfg->LNR);
	IPMI_Sensor_Proc_WriteReg(InstancePtr->BaseAddress, LCR_REG + ch * 4, cfg->LCR);
	IPMI_Sensor_Proc_WriteReg(InstancePtr->BaseAddress, LNC_REG + ch * 4, cfg->LNC);

	return XST_SUCCESS;
}

XStatus IPMI_Sensor_Proc_Get_Thr(const IPMI_Sensor_Proc *InstancePtr, const u32 ch, Thr_Cfg * cfg)
{
	if (ch > (InstancePtr->sensor_ch_cnt-1))
		return XST_INVALID_PARAM;

	cfg->UNR = IPMI_Sensor_Proc_ReadReg(InstancePtr->BaseAddress, UNR_REG + ch * 4);
	cfg->UCR = IPMI_Sensor_Proc_ReadReg(InstancePtr->BaseAddress, UCR_REG + ch * 4);
	cfg->UNC = IPMI_Sensor_Proc_ReadReg(InstancePtr->BaseAddress, UNC_REG + ch * 4);
	cfg->LNR = IPMI_Sensor_Proc_ReadReg(InstancePtr->BaseAddress, LNR_REG + ch * 4);
	cfg->LCR = IPMI_Sensor_Proc_ReadReg(InstancePtr->BaseAddress, LCR_REG + ch * 4);
	cfg->LNC = IPMI_Sensor_Proc_ReadReg(InstancePtr->BaseAddress, LNC_REG + ch * 4);

	return XST_SUCCESS;
}


XStatus IPMI_Sensor_Proc_Set_Event_Enable(const IPMI_Sensor_Proc *InstancePtr,
		const u32 ch, const u16 assert_en, const u16 deassert_en )
{
	if (ch > (InstancePtr->sensor_ch_cnt-1))
		return XST_INVALID_PARAM;

	IPMI_Sensor_Proc_WriteReg(InstancePtr->BaseAddress, EV_ASSERT_EN_REG + ch * 4, assert_en);
	IPMI_Sensor_Proc_WriteReg(InstancePtr->BaseAddress, EV_DEASSERT_EN_REG + ch * 4, deassert_en);

	return XST_SUCCESS;
}


XStatus IPMI_Sensor_Proc_Get_Event_Enable(const IPMI_Sensor_Proc *InstancePtr,
		const u32 ch, u16 * assert_en, u16 * deassert_en )
{
	if (ch > (InstancePtr->sensor_ch_cnt-1))
		return XST_INVALID_PARAM;

	*assert_en = IPMI_Sensor_Proc_ReadReg(InstancePtr->BaseAddress, EV_ASSERT_EN_REG + ch * 4);
	*deassert_en = IPMI_Sensor_Proc_ReadReg(InstancePtr->BaseAddress, EV_DEASSERT_EN_REG + ch * 4);

	return XST_SUCCESS;
}


XStatus IPMI_Sensor_Proc_Rearm_Event_Enable(const IPMI_Sensor_Proc *InstancePtr,
		const u32 ch, const u16 assert_rearm, const u16 deassert_rearm )
{
	if (ch > (InstancePtr->sensor_ch_cnt-1))
		return XST_INVALID_PARAM;

	IPMI_Sensor_Proc_WriteReg(InstancePtr->BaseAddress, EV_ASSERT_REARM_REG + ch * 4, assert_rearm);
	IPMI_Sensor_Proc_WriteReg(InstancePtr->BaseAddress, EV_ASSERT_REARM_REG + ch * 4, 0);

	IPMI_Sensor_Proc_WriteReg(InstancePtr->BaseAddress, EV_DEASSERT_REARM_REG + ch * 4, deassert_rearm);
	IPMI_Sensor_Proc_WriteReg(InstancePtr->BaseAddress, EV_DEASSERT_REARM_REG + ch * 4, 0);

	return XST_SUCCESS;
}



XStatus IPMI_Sensor_Proc_Get_Latched_Event_Status(const IPMI_Sensor_Proc *InstancePtr,
		const u32 ch, u16 * assert_status, u16 * deassert_status )
{
	if (ch > (InstancePtr->sensor_ch_cnt-1))
		return XST_INVALID_PARAM;

	*assert_status = IPMI_Sensor_Proc_ReadReg(InstancePtr->BaseAddress, EV_ASSERT_ST_REG + ch * 4);
	*deassert_status = IPMI_Sensor_Proc_ReadReg(InstancePtr->BaseAddress, EV_DEASSERT_ST_REG + ch * 4);

	return XST_SUCCESS;
}

XStatus IPMI_Sensor_Proc_Get_Current_Event_Status(const IPMI_Sensor_Proc *InstancePtr,
		const u32 ch, u16 * assert_status, u16 * deassert_status )
{
	if (ch > (InstancePtr->sensor_ch_cnt-1))
		return XST_INVALID_PARAM;

	*assert_status = IPMI_Sensor_Proc_ReadReg(InstancePtr->BaseAddress, EV_ASSERT_CUR_ST_REG + ch * 4);

	//TODO: deassert status
	*deassert_status = 0;

	return XST_SUCCESS;
}

XStatus IPMI_Sensor_Proc_Get_Sensor_Reading(const IPMI_Sensor_Proc *InstancePtr,
		const u32 ch, u16 * sensor_reading, u8 * thr_status )
{
	if (ch > (InstancePtr->sensor_ch_cnt-1))
		return XST_INVALID_PARAM;

	*sensor_reading = IPMI_Sensor_Proc_ReadReg(InstancePtr->BaseAddress, RAW_READING_REG + ch * 4);
	*thr_status = IPMI_Sensor_Proc_ReadReg(InstancePtr->BaseAddress, THR_STATUS_REG + ch * 4);

	return XST_SUCCESS;
}

u32 IPMI_Sensor_Proc_Get_IRQ_Status(const IPMI_Sensor_Proc *InstancePtr)
{
	return  IPMI_Sensor_Proc_ReadReg(InstancePtr->BaseAddress, IRQ_REQ_REG);
}

void IPMI_Sensor_Proc_Ack_IRQ(const IPMI_Sensor_Proc *InstancePtr, u32 irq_ack)
{
	IPMI_Sensor_Proc_WriteReg(InstancePtr->BaseAddress, IRQ_ACK_REG, irq_ack);
}
