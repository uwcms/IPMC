#ifndef XPARAMETERS_H   /* prevent circular inclusions */
#define XPARAMETERS_H   /* by using protection macros */

/* Definition for CPU ID */
#define XPAR_CPU_ID 0U

/* Definitions for peripheral PS7_CORTEXA9_0 */
#define XPAR_PS7_CORTEXA9_0_CPU_CLK_FREQ_HZ 666666687


/******************************************************************/

/* Canonical definitions for peripheral PS7_CORTEXA9_0 */
#define XPAR_CPU_CORTEXA9_0_CPU_CLK_FREQ_HZ 666666687


/******************************************************************/

#include "xparameters_ps.h"

#define STDIN_BASEADDRESS 0xE0000000
#define STDOUT_BASEADDRESS 0xE0000000

/******************************************************************/

/* Platform specific definitions */
#define PLATFORM_ZYNQ
 
/* Definitions for sleep timer configuration */
 
 
/******************************************************************/
/* Definitions for driver AD7689_S */
#define XPAR_AD7689_S_NUM_INSTANCES 3

/* Definitions for peripheral AD7689_S_0 */
#define XPAR_AD7689_S_0_DEVICE_ID 0
#define XPAR_AD7689_S_0_S_AXI_BASEADDR 0x43C00000
#define XPAR_AD7689_S_0_S_AXI_HIGHADDR 0x43C0FFFF
#define XPAR_AD7689_S_0_SLAVES 1


/* Definitions for peripheral AD7689_S_1 */
#define XPAR_AD7689_S_1_DEVICE_ID 1
#define XPAR_AD7689_S_1_S_AXI_BASEADDR 0x43C10000
#define XPAR_AD7689_S_1_S_AXI_HIGHADDR 0x43C1FFFF
#define XPAR_AD7689_S_1_SLAVES 1


/* Definitions for peripheral AD7689_S_2 */
#define XPAR_AD7689_S_2_DEVICE_ID 2
#define XPAR_AD7689_S_2_S_AXI_BASEADDR 0x43C50000
#define XPAR_AD7689_S_2_S_AXI_HIGHADDR 0x43C5FFFF
#define XPAR_AD7689_S_2_SLAVES 3


/******************************************************************/


/******************************************************************/


/* Definitions for peripheral PS7_DDR_0 */
#define XPAR_PS7_DDR_0_S_AXI_BASEADDR 0x00100000
#define XPAR_PS7_DDR_0_S_AXI_HIGHADDR 0x0FFFFFFF


/******************************************************************/

/* Definitions for driver DEVCFG */
#define XPAR_XDCFG_NUM_INSTANCES 1U

/* Definitions for peripheral PS7_DEV_CFG_0 */
#define XPAR_PS7_DEV_CFG_0_DEVICE_ID 0U
#define XPAR_PS7_DEV_CFG_0_BASEADDR 0xF8007000U
#define XPAR_PS7_DEV_CFG_0_HIGHADDR 0xF80070FFU


/******************************************************************/

/* Canonical definitions for peripheral PS7_DEV_CFG_0 */
#define XPAR_XDCFG_0_DEVICE_ID XPAR_PS7_DEV_CFG_0_DEVICE_ID
#define XPAR_XDCFG_0_BASEADDR 0xF8007000U
#define XPAR_XDCFG_0_HIGHADDR 0xF80070FFU


/******************************************************************/

/* Definitions for driver DMAPS */
#define XPAR_XDMAPS_NUM_INSTANCES 2

/* Definitions for peripheral PS7_DMA_NS */
#define XPAR_PS7_DMA_NS_DEVICE_ID 0
#define XPAR_PS7_DMA_NS_BASEADDR 0xF8004000
#define XPAR_PS7_DMA_NS_HIGHADDR 0xF8004FFF


/* Definitions for peripheral PS7_DMA_S */
#define XPAR_PS7_DMA_S_DEVICE_ID 1
#define XPAR_PS7_DMA_S_BASEADDR 0xF8003000
#define XPAR_PS7_DMA_S_HIGHADDR 0xF8003FFF


/******************************************************************/

/* Canonical definitions for peripheral PS7_DMA_NS */
#define XPAR_XDMAPS_0_DEVICE_ID XPAR_PS7_DMA_NS_DEVICE_ID
#define XPAR_XDMAPS_0_BASEADDR 0xF8004000
#define XPAR_XDMAPS_0_HIGHADDR 0xF8004FFF

/* Canonical definitions for peripheral PS7_DMA_S */
#define XPAR_XDMAPS_1_DEVICE_ID XPAR_PS7_DMA_S_DEVICE_ID
#define XPAR_XDMAPS_1_BASEADDR 0xF8003000
#define XPAR_XDMAPS_1_HIGHADDR 0xF8003FFF


/******************************************************************/

/* Definitions for driver EMACPS */
#define XPAR_XEMACPS_NUM_INSTANCES 1

/* Definitions for peripheral PS7_ETHERNET_0 */
#define XPAR_PS7_ETHERNET_0_DEVICE_ID 0
#define XPAR_PS7_ETHERNET_0_BASEADDR 0xE000B000
#define XPAR_PS7_ETHERNET_0_HIGHADDR 0xE000BFFF
#define XPAR_PS7_ETHERNET_0_ENET_CLK_FREQ_HZ 125000000
#define XPAR_PS7_ETHERNET_0_ENET_SLCR_1000MBPS_DIV0 8
#define XPAR_PS7_ETHERNET_0_ENET_SLCR_1000MBPS_DIV1 1
#define XPAR_PS7_ETHERNET_0_ENET_SLCR_100MBPS_DIV0 8
#define XPAR_PS7_ETHERNET_0_ENET_SLCR_100MBPS_DIV1 5
#define XPAR_PS7_ETHERNET_0_ENET_SLCR_10MBPS_DIV0 8
#define XPAR_PS7_ETHERNET_0_ENET_SLCR_10MBPS_DIV1 50
#define XPAR_PS7_ETHERNET_0_ENET_TSU_CLK_FREQ_HZ 0


/******************************************************************/

#define XPAR_PS7_ETHERNET_0_IS_CACHE_COHERENT 0
/* Canonical definitions for peripheral PS7_ETHERNET_0 */
#define XPAR_XEMACPS_0_DEVICE_ID XPAR_PS7_ETHERNET_0_DEVICE_ID
#define XPAR_XEMACPS_0_BASEADDR 0xE000B000
#define XPAR_XEMACPS_0_HIGHADDR 0xE000BFFF
#define XPAR_XEMACPS_0_ENET_CLK_FREQ_HZ 125000000
#define XPAR_XEMACPS_0_ENET_SLCR_1000Mbps_DIV0 8
#define XPAR_XEMACPS_0_ENET_SLCR_1000Mbps_DIV1 1
#define XPAR_XEMACPS_0_ENET_SLCR_100Mbps_DIV0 8
#define XPAR_XEMACPS_0_ENET_SLCR_100Mbps_DIV1 5
#define XPAR_XEMACPS_0_ENET_SLCR_10Mbps_DIV0 8
#define XPAR_XEMACPS_0_ENET_SLCR_10Mbps_DIV1 50
#define XPAR_XEMACPS_0_ENET_TSU_CLK_FREQ_HZ 0


/******************************************************************/


/* Definitions for peripheral AXI_JTAG_0 */
#define XPAR_AXI_JTAG_0_BASEADDR 0x43C20000
#define XPAR_AXI_JTAG_0_HIGHADDR 0x43C2FFFF


/* Definitions for peripheral PS7_AFI_0 */
#define XPAR_PS7_AFI_0_S_AXI_BASEADDR 0xF8008000
#define XPAR_PS7_AFI_0_S_AXI_HIGHADDR 0xF8008FFF


/* Definitions for peripheral PS7_AFI_1 */
#define XPAR_PS7_AFI_1_S_AXI_BASEADDR 0xF8009000
#define XPAR_PS7_AFI_1_S_AXI_HIGHADDR 0xF8009FFF


/* Definitions for peripheral PS7_AFI_2 */
#define XPAR_PS7_AFI_2_S_AXI_BASEADDR 0xF800A000
#define XPAR_PS7_AFI_2_S_AXI_HIGHADDR 0xF800AFFF


/* Definitions for peripheral PS7_AFI_3 */
#define XPAR_PS7_AFI_3_S_AXI_BASEADDR 0xF800B000
#define XPAR_PS7_AFI_3_S_AXI_HIGHADDR 0xF800BFFF


/* Definitions for peripheral PS7_DDRC_0 */
#define XPAR_PS7_DDRC_0_S_AXI_BASEADDR 0xF8006000
#define XPAR_PS7_DDRC_0_S_AXI_HIGHADDR 0xF8006FFF


/* Definitions for peripheral PS7_GLOBALTIMER_0 */
#define XPAR_PS7_GLOBALTIMER_0_S_AXI_BASEADDR 0xF8F00200
#define XPAR_PS7_GLOBALTIMER_0_S_AXI_HIGHADDR 0xF8F002FF


/* Definitions for peripheral PS7_GPV_0 */
#define XPAR_PS7_GPV_0_S_AXI_BASEADDR 0xF8900000
#define XPAR_PS7_GPV_0_S_AXI_HIGHADDR 0xF89FFFFF


/* Definitions for peripheral PS7_INTC_DIST_0 */
#define XPAR_PS7_INTC_DIST_0_S_AXI_BASEADDR 0xF8F01000
#define XPAR_PS7_INTC_DIST_0_S_AXI_HIGHADDR 0xF8F01FFF


/* Definitions for peripheral PS7_IOP_BUS_CONFIG_0 */
#define XPAR_PS7_IOP_BUS_CONFIG_0_S_AXI_BASEADDR 0xE0200000
#define XPAR_PS7_IOP_BUS_CONFIG_0_S_AXI_HIGHADDR 0xE0200FFF


/* Definitions for peripheral PS7_L2CACHEC_0 */
#define XPAR_PS7_L2CACHEC_0_S_AXI_BASEADDR 0xF8F02000
#define XPAR_PS7_L2CACHEC_0_S_AXI_HIGHADDR 0xF8F02FFF


/* Definitions for peripheral PS7_OCMC_0 */
#define XPAR_PS7_OCMC_0_S_AXI_BASEADDR 0xF800C000
#define XPAR_PS7_OCMC_0_S_AXI_HIGHADDR 0xF800CFFF


/* Definitions for peripheral PS7_PL310_0 */
#define XPAR_PS7_PL310_0_S_AXI_BASEADDR 0xF8F02000
#define XPAR_PS7_PL310_0_S_AXI_HIGHADDR 0xF8F02FFF


/* Definitions for peripheral PS7_PMU_0 */
#define XPAR_PS7_PMU_0_S_AXI_BASEADDR 0xF8891000
#define XPAR_PS7_PMU_0_S_AXI_HIGHADDR 0xF8891FFF
#define XPAR_PS7_PMU_0_PMU1_S_AXI_BASEADDR 0xF8893000
#define XPAR_PS7_PMU_0_PMU1_S_AXI_HIGHADDR 0xF8893FFF


/* Definitions for peripheral PS7_QSPI_LINEAR_0 */
#define XPAR_PS7_QSPI_LINEAR_0_S_AXI_BASEADDR 0xFC000000
#define XPAR_PS7_QSPI_LINEAR_0_S_AXI_HIGHADDR 0xFCFFFFFF


/* Definitions for peripheral PS7_RAM_0 */
#define XPAR_PS7_RAM_0_S_AXI_BASEADDR 0x00000000
#define XPAR_PS7_RAM_0_S_AXI_HIGHADDR 0x0003FFFF


/* Definitions for peripheral PS7_RAM_1 */
#define XPAR_PS7_RAM_1_S_AXI_BASEADDR 0xFFFC0000
#define XPAR_PS7_RAM_1_S_AXI_HIGHADDR 0xFFFFFFFF


/* Definitions for peripheral PS7_SCUC_0 */
#define XPAR_PS7_SCUC_0_S_AXI_BASEADDR 0xF8F00000
#define XPAR_PS7_SCUC_0_S_AXI_HIGHADDR 0xF8F000FC


/* Definitions for peripheral PS7_SLCR_0 */
#define XPAR_PS7_SLCR_0_S_AXI_BASEADDR 0xF8000000
#define XPAR_PS7_SLCR_0_S_AXI_HIGHADDR 0xF8000FFF


/******************************************************************/

/* Definitions for driver GPIO */
#define XPAR_XGPIO_NUM_INSTANCES 4

/* Definitions for peripheral AXI_GPIO_0 */
#define XPAR_AXI_GPIO_0_BASEADDR 0x41210000
#define XPAR_AXI_GPIO_0_HIGHADDR 0x4121FFFF
#define XPAR_AXI_GPIO_0_DEVICE_ID 0
#define XPAR_AXI_GPIO_0_INTERRUPT_PRESENT 1
#define XPAR_AXI_GPIO_0_IS_DUAL 1


/* Definitions for peripheral AXI_GPIO_HNDL_SW */
#define XPAR_AXI_GPIO_HNDL_SW_BASEADDR 0x41220000
#define XPAR_AXI_GPIO_HNDL_SW_HIGHADDR 0x4122FFFF
#define XPAR_AXI_GPIO_HNDL_SW_DEVICE_ID 1
#define XPAR_AXI_GPIO_HNDL_SW_INTERRUPT_PRESENT 1
#define XPAR_AXI_GPIO_HNDL_SW_IS_DUAL 0


/* Definitions for peripheral ESM_AXI_GPIO_ESM */
#define XPAR_ESM_AXI_GPIO_ESM_BASEADDR 0x41200000
#define XPAR_ESM_AXI_GPIO_ESM_HIGHADDR 0x4120FFFF
#define XPAR_ESM_AXI_GPIO_ESM_DEVICE_ID 2
#define XPAR_ESM_AXI_GPIO_ESM_INTERRUPT_PRESENT 0
#define XPAR_ESM_AXI_GPIO_ESM_IS_DUAL 0


/* Definitions for peripheral ELM_AXI_GPIO_0 */
#define XPAR_ELM_AXI_GPIO_0_BASEADDR 0x41230000
#define XPAR_ELM_AXI_GPIO_0_HIGHADDR 0x4123FFFF
#define XPAR_ELM_AXI_GPIO_0_DEVICE_ID 3
#define XPAR_ELM_AXI_GPIO_0_INTERRUPT_PRESENT 1
#define XPAR_ELM_AXI_GPIO_0_IS_DUAL 1


/******************************************************************/

/* Canonical definitions for peripheral AXI_GPIO_0 */
#define XPAR_GPIO_0_BASEADDR 0x41210000
#define XPAR_GPIO_0_HIGHADDR 0x4121FFFF
#define XPAR_GPIO_0_DEVICE_ID XPAR_AXI_GPIO_0_DEVICE_ID
#define XPAR_GPIO_0_INTERRUPT_PRESENT 1
#define XPAR_GPIO_0_IS_DUAL 1

/* Canonical definitions for peripheral AXI_GPIO_HNDL_SW */
#define XPAR_GPIO_1_BASEADDR 0x41220000
#define XPAR_GPIO_1_HIGHADDR 0x4122FFFF
#define XPAR_GPIO_1_DEVICE_ID XPAR_AXI_GPIO_HNDL_SW_DEVICE_ID
#define XPAR_GPIO_1_INTERRUPT_PRESENT 1
#define XPAR_GPIO_1_IS_DUAL 0

/* Canonical definitions for peripheral ESM_AXI_GPIO_ESM */
#define XPAR_GPIO_2_BASEADDR 0x41200000
#define XPAR_GPIO_2_HIGHADDR 0x4120FFFF
#define XPAR_GPIO_2_DEVICE_ID XPAR_ESM_AXI_GPIO_ESM_DEVICE_ID
#define XPAR_GPIO_2_INTERRUPT_PRESENT 0
#define XPAR_GPIO_2_IS_DUAL 0

/* Canonical definitions for peripheral ELM_AXI_GPIO_0 */
#define XPAR_GPIO_3_BASEADDR 0x41230000
#define XPAR_GPIO_3_HIGHADDR 0x4123FFFF
#define XPAR_GPIO_3_DEVICE_ID XPAR_ELM_AXI_GPIO_0_DEVICE_ID
#define XPAR_GPIO_3_INTERRUPT_PRESENT 1
#define XPAR_GPIO_3_IS_DUAL 1


/******************************************************************/

/* Definitions for driver GPIOPS */
#define XPAR_XGPIOPS_NUM_INSTANCES 1

/* Definitions for peripheral PS7_GPIO_0 */
#define XPAR_PS7_GPIO_0_DEVICE_ID 0
#define XPAR_PS7_GPIO_0_BASEADDR 0xE000A000
#define XPAR_PS7_GPIO_0_HIGHADDR 0xE000AFFF


/******************************************************************/

/* Canonical definitions for peripheral PS7_GPIO_0 */
#define XPAR_XGPIOPS_0_DEVICE_ID XPAR_PS7_GPIO_0_DEVICE_ID
#define XPAR_XGPIOPS_0_BASEADDR 0xE000A000
#define XPAR_XGPIOPS_0_HIGHADDR 0xE000AFFF


/******************************************************************/

/* Definitions for driver IIC */
#define XPAR_XIIC_NUM_INSTANCES 1

/* Definitions for peripheral AXI_IIC_PIM400 */
#define XPAR_AXI_IIC_PIM400_DEVICE_ID 0
#define XPAR_AXI_IIC_PIM400_BASEADDR 0x41600000
#define XPAR_AXI_IIC_PIM400_HIGHADDR 0x4160FFFF
#define XPAR_AXI_IIC_PIM400_TEN_BIT_ADR 0
#define XPAR_AXI_IIC_PIM400_GPO_WIDTH 1


/******************************************************************/

/* Canonical definitions for peripheral AXI_IIC_PIM400 */
#define XPAR_IIC_0_DEVICE_ID XPAR_AXI_IIC_PIM400_DEVICE_ID
#define XPAR_IIC_0_BASEADDR 0x41600000
#define XPAR_IIC_0_HIGHADDR 0x4160FFFF
#define XPAR_IIC_0_TEN_BIT_ADR 0
#define XPAR_IIC_0_GPO_WIDTH 1


/******************************************************************/

/* Definitions for driver IICPS */
#define XPAR_XIICPS_NUM_INSTANCES 2

/* Definitions for peripheral PS7_I2C_0 */
#define XPAR_PS7_I2C_0_DEVICE_ID 0
#define XPAR_PS7_I2C_0_BASEADDR 0xE0004000
#define XPAR_PS7_I2C_0_HIGHADDR 0xE0004FFF
#define XPAR_PS7_I2C_0_I2C_CLK_FREQ_HZ 111111115


/* Definitions for peripheral PS7_I2C_1 */
#define XPAR_PS7_I2C_1_DEVICE_ID 1
#define XPAR_PS7_I2C_1_BASEADDR 0xE0005000
#define XPAR_PS7_I2C_1_HIGHADDR 0xE0005FFF
#define XPAR_PS7_I2C_1_I2C_CLK_FREQ_HZ 111111115


/******************************************************************/

/* Canonical definitions for peripheral PS7_I2C_0 */
#define XPAR_XIICPS_0_DEVICE_ID XPAR_PS7_I2C_0_DEVICE_ID
#define XPAR_XIICPS_0_BASEADDR 0xE0004000
#define XPAR_XIICPS_0_HIGHADDR 0xE0004FFF
#define XPAR_XIICPS_0_I2C_CLK_FREQ_HZ 111111115

/* Canonical definitions for peripheral PS7_I2C_1 */
#define XPAR_XIICPS_1_DEVICE_ID XPAR_PS7_I2C_1_DEVICE_ID
#define XPAR_XIICPS_1_BASEADDR 0xE0005000
#define XPAR_XIICPS_1_HIGHADDR 0xE0005FFF
#define XPAR_XIICPS_1_I2C_CLK_FREQ_HZ 111111115


/******************************************************************/

/* Definitions for driver IPMI_SENSOR_PROC */
#define XPAR_IPMI_SENSOR_PROC_NUM_INSTANCES 1

/* Definitions for peripheral IPMI_SENSOR_PROC_0 */
#define XPAR_IPMI_SENSOR_PROC_0_DEVICE_ID 0
#define XPAR_IPMI_SENSOR_PROC_0_S_AXI_BASEADDR 0x43C60000
#define XPAR_IPMI_SENSOR_PROC_0_S_AXI_HIGHADDR 0x43C6FFFF
#define XPAR_IPMI_SENSOR_PROC_0_SENSOR_CNT 33
#define XPAR_IPMI_SENSOR_PROC_0_SENSOR_DATA_WIDTH 16


/******************************************************************/


/******************************************************************/

/* Definitions for driver LED_CONTROLLER */
#define XPAR_LED_CONTROLLER_NUM_INSTANCES 2

/* Definitions for peripheral AXI_ATCA_LED_CTRL */
#define XPAR_AXI_ATCA_LED_CTRL_DEVICE_ID 0
#define XPAR_AXI_ATCA_LED_CTRL_S_AXI_BASEADDR 0x43C40000
#define XPAR_AXI_ATCA_LED_CTRL_S_AXI_HIGHADDR 0x43C4FFFF
#define XPAR_AXI_ATCA_LED_CTRL_LED_INTERFACES 5


/* Definitions for peripheral AXI_USER_LED_CTRL */
#define XPAR_AXI_USER_LED_CTRL_DEVICE_ID 1
#define XPAR_AXI_USER_LED_CTRL_S_AXI_BASEADDR 0x43C30000
#define XPAR_AXI_USER_LED_CTRL_S_AXI_HIGHADDR 0x43C3FFFF
#define XPAR_AXI_USER_LED_CTRL_LED_INTERFACES 2


/******************************************************************/

/* Canonical definitions for peripheral AXI_ATCA_LED_CTRL */
#define XPAR_LED_CONTROLLER_0_NUM_INSTANCES 0
#define XPAR_LED_CONTROLLER_0_DEVICE_ID XPAR_AXI_ATCA_LED_CTRL_DEVICE_ID
#define XPAR_LED_CONTROLLER_0_S_AXI_BASEADDR 0x43C40000
#define XPAR_LED_CONTROLLER_0_S_AXI_HIGHADDR 0x43C4FFFF
#define XPAR_LED_CONTROLLER_0_LED_INTERFACES 5

/* Canonical definitions for peripheral AXI_USER_LED_CTRL */
#define XPAR_LED_CONTROLLER_1_NUM_INSTANCES 0
#define XPAR_LED_CONTROLLER_1_DEVICE_ID XPAR_AXI_USER_LED_CTRL_DEVICE_ID
#define XPAR_LED_CONTROLLER_1_S_AXI_BASEADDR 0x43C30000
#define XPAR_LED_CONTROLLER_1_S_AXI_HIGHADDR 0x43C3FFFF
#define XPAR_LED_CONTROLLER_1_LED_INTERFACES 2


/******************************************************************/

/* Definitions for driver MGMT_ZONE_CTRL */
#define XPAR_MGMT_ZONE_CTRL_NUM_INSTANCES 1

/* Definitions for peripheral MGMT_ZONE_CTRL_0 */
#define XPAR_MGMT_ZONE_CTRL_0_DEVICE_ID 0
#define XPAR_MGMT_ZONE_CTRL_0_S_AXI_BASEADDR 0x43C70000
#define XPAR_MGMT_ZONE_CTRL_0_S_AXI_HIGHADDR 0x43C7FFFF
#define XPAR_MGMT_ZONE_CTRL_0_MZ_CNT 5
#define XPAR_MGMT_ZONE_CTRL_0_HF_CNT 33
#define XPAR_MGMT_ZONE_CTRL_0_PWREN_CNT 14


/******************************************************************/


/******************************************************************/

/* Definitions for driver QSPIPS */
#define XPAR_XQSPIPS_NUM_INSTANCES 1

/* Definitions for peripheral PS7_QSPI_0 */
#define XPAR_PS7_QSPI_0_DEVICE_ID 0
#define XPAR_PS7_QSPI_0_BASEADDR 0xE000D000
#define XPAR_PS7_QSPI_0_HIGHADDR 0xE000DFFF
#define XPAR_PS7_QSPI_0_QSPI_CLK_FREQ_HZ 200000000
#define XPAR_PS7_QSPI_0_QSPI_MODE 0
#define XPAR_PS7_QSPI_0_QSPI_BUS_WIDTH 2


/******************************************************************/

/* Canonical definitions for peripheral PS7_QSPI_0 */
#define XPAR_XQSPIPS_0_DEVICE_ID XPAR_PS7_QSPI_0_DEVICE_ID
#define XPAR_XQSPIPS_0_BASEADDR 0xE000D000
#define XPAR_XQSPIPS_0_HIGHADDR 0xE000DFFF
#define XPAR_XQSPIPS_0_QSPI_CLK_FREQ_HZ 200000000
#define XPAR_XQSPIPS_0_QSPI_MODE 0
#define XPAR_XQSPIPS_0_QSPI_BUS_WIDTH 2


/******************************************************************/

/* Definitions for Fabric interrupts connected to ps7_scugic_0 */
#define XPAR_FABRIC_AXI_IIC_PIM400_IIC2INTC_IRPT_INTR 61U
#define XPAR_FABRIC_ESM_AXI_QUAD_SPI_ESM_IP2INTC_IRPT_INTR 62U
#define XPAR_FABRIC_ESM_AXI_UARTLITE_ESM_INTERRUPT_INTR 63U
#define XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR 64U
#define XPAR_FABRIC_AXI_QUAD_SPI_DAC_IP2INTC_IRPT_INTR 65U
#define XPAR_FABRIC_AXI_GPIO_HNDL_SW_IP2INTC_IRPT_INTR 67U
#define XPAR_FABRIC_ELM_AXI_GPIO_0_IP2INTC_IRPT_INTR 68U
#define XPAR_FABRIC_ELM_AXI_UARTLITE_0_INTERRUPT_INTR 84U

/******************************************************************/

/* Canonical definitions for Fabric interrupts connected to ps7_scugic_0 */
#define XPAR_FABRIC_IIC_0_VEC_ID XPAR_FABRIC_AXI_IIC_PIM400_IIC2INTC_IRPT_INTR
#define XPAR_FABRIC_SPI_1_VEC_ID XPAR_FABRIC_ESM_AXI_QUAD_SPI_ESM_IP2INTC_IRPT_INTR
#define XPAR_FABRIC_UARTLITE_0_VEC_ID XPAR_FABRIC_ESM_AXI_UARTLITE_ESM_INTERRUPT_INTR
#define XPAR_FABRIC_GPIO_0_VEC_ID XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR
#define XPAR_FABRIC_SPI_0_VEC_ID XPAR_FABRIC_AXI_QUAD_SPI_DAC_IP2INTC_IRPT_INTR
#define XPAR_FABRIC_GPIO_1_VEC_ID XPAR_FABRIC_AXI_GPIO_HNDL_SW_IP2INTC_IRPT_INTR
#define XPAR_FABRIC_GPIO_3_VEC_ID XPAR_FABRIC_ELM_AXI_GPIO_0_IP2INTC_IRPT_INTR
#define XPAR_FABRIC_UARTLITE_1_VEC_ID XPAR_FABRIC_ELM_AXI_UARTLITE_0_INTERRUPT_INTR

/******************************************************************/

/* Definitions for driver SCUGIC */
#define XPAR_XSCUGIC_NUM_INSTANCES 1U

/* Definitions for peripheral PS7_SCUGIC_0 */
#define XPAR_PS7_SCUGIC_0_DEVICE_ID 0U
#define XPAR_PS7_SCUGIC_0_BASEADDR 0xF8F00100U
#define XPAR_PS7_SCUGIC_0_HIGHADDR 0xF8F001FFU
#define XPAR_PS7_SCUGIC_0_DIST_BASEADDR 0xF8F01000U


/******************************************************************/

/* Canonical definitions for peripheral PS7_SCUGIC_0 */
#define XPAR_SCUGIC_0_DEVICE_ID 0U
#define XPAR_SCUGIC_0_CPU_BASEADDR 0xF8F00100U
#define XPAR_SCUGIC_0_CPU_HIGHADDR 0xF8F001FFU
#define XPAR_SCUGIC_0_DIST_BASEADDR 0xF8F01000U


/******************************************************************/

/* Definitions for driver SCUTIMER */
#define XPAR_XSCUTIMER_NUM_INSTANCES 1

/* Definitions for peripheral PS7_SCUTIMER_0 */
#define XPAR_PS7_SCUTIMER_0_DEVICE_ID 0
#define XPAR_PS7_SCUTIMER_0_BASEADDR 0xF8F00600
#define XPAR_PS7_SCUTIMER_0_HIGHADDR 0xF8F0061F


/******************************************************************/

/* Canonical definitions for peripheral PS7_SCUTIMER_0 */
#define XPAR_XSCUTIMER_0_DEVICE_ID XPAR_PS7_SCUTIMER_0_DEVICE_ID
#define XPAR_XSCUTIMER_0_BASEADDR 0xF8F00600
#define XPAR_XSCUTIMER_0_HIGHADDR 0xF8F0061F


/******************************************************************/

/* Definitions for driver SCUWDT */
#define XPAR_XSCUWDT_NUM_INSTANCES 1

/* Definitions for peripheral PS7_SCUWDT_0 */
#define XPAR_PS7_SCUWDT_0_DEVICE_ID 0
#define XPAR_PS7_SCUWDT_0_BASEADDR 0xF8F00620
#define XPAR_PS7_SCUWDT_0_HIGHADDR 0xF8F006FF


/******************************************************************/

/* Canonical definitions for peripheral PS7_SCUWDT_0 */
#define XPAR_SCUWDT_0_DEVICE_ID XPAR_PS7_SCUWDT_0_DEVICE_ID
#define XPAR_SCUWDT_0_BASEADDR 0xF8F00620
#define XPAR_SCUWDT_0_HIGHADDR 0xF8F006FF


/******************************************************************/

/* Definitions for driver SPI */
#define XPAR_XSPI_NUM_INSTANCES 2U

/* Definitions for peripheral AXI_QUAD_SPI_DAC */
#define XPAR_AXI_QUAD_SPI_DAC_DEVICE_ID 0U
#define XPAR_AXI_QUAD_SPI_DAC_BASEADDR 0x41E10000U
#define XPAR_AXI_QUAD_SPI_DAC_HIGHADDR 0x41E1FFFFU
#define XPAR_AXI_QUAD_SPI_DAC_FIFO_DEPTH 16U
#define XPAR_AXI_QUAD_SPI_DAC_FIFO_EXIST 1U
#define XPAR_AXI_QUAD_SPI_DAC_SPI_SLAVE_ONLY 0U
#define XPAR_AXI_QUAD_SPI_DAC_NUM_SS_BITS 1U
#define XPAR_AXI_QUAD_SPI_DAC_NUM_TRANSFER_BITS 8U
#define XPAR_AXI_QUAD_SPI_DAC_SPI_MODE 0U
#define XPAR_AXI_QUAD_SPI_DAC_TYPE_OF_AXI4_INTERFACE 0U
#define XPAR_AXI_QUAD_SPI_DAC_AXI4_BASEADDR 0U
#define XPAR_AXI_QUAD_SPI_DAC_AXI4_HIGHADDR 0U
#define XPAR_AXI_QUAD_SPI_DAC_XIP_MODE 0U

/* Canonical definitions for peripheral AXI_QUAD_SPI_DAC */
#define XPAR_SPI_0_DEVICE_ID 0U
#define XPAR_SPI_0_BASEADDR 0x41E10000U
#define XPAR_SPI_0_HIGHADDR 0x41E1FFFFU
#define XPAR_SPI_0_FIFO_DEPTH 16U
#define XPAR_SPI_0_FIFO_EXIST 1U
#define XPAR_SPI_0_SPI_SLAVE_ONLY 0U
#define XPAR_SPI_0_NUM_SS_BITS 1U
#define XPAR_SPI_0_NUM_TRANSFER_BITS 8U
#define XPAR_SPI_0_SPI_MODE 0U
#define XPAR_SPI_0_TYPE_OF_AXI4_INTERFACE 0U
#define XPAR_SPI_0_AXI4_BASEADDR 0U
#define XPAR_SPI_0_AXI4_HIGHADDR 0U
#define XPAR_SPI_0_XIP_MODE 0U
#define XPAR_SPI_0_USE_STARTUP 0U



/* Definitions for peripheral ESM_AXI_QUAD_SPI_ESM */
#define XPAR_ESM_AXI_QUAD_SPI_ESM_DEVICE_ID 1U
#define XPAR_ESM_AXI_QUAD_SPI_ESM_BASEADDR 0x41E00000U
#define XPAR_ESM_AXI_QUAD_SPI_ESM_HIGHADDR 0x41E0FFFFU
#define XPAR_ESM_AXI_QUAD_SPI_ESM_FIFO_DEPTH 16U
#define XPAR_ESM_AXI_QUAD_SPI_ESM_FIFO_EXIST 1U
#define XPAR_ESM_AXI_QUAD_SPI_ESM_SPI_SLAVE_ONLY 0U
#define XPAR_ESM_AXI_QUAD_SPI_ESM_NUM_SS_BITS 1U
#define XPAR_ESM_AXI_QUAD_SPI_ESM_NUM_TRANSFER_BITS 8U
#define XPAR_ESM_AXI_QUAD_SPI_ESM_SPI_MODE 0U
#define XPAR_ESM_AXI_QUAD_SPI_ESM_TYPE_OF_AXI4_INTERFACE 0U
#define XPAR_ESM_AXI_QUAD_SPI_ESM_AXI4_BASEADDR 0U
#define XPAR_ESM_AXI_QUAD_SPI_ESM_AXI4_HIGHADDR 0U
#define XPAR_ESM_AXI_QUAD_SPI_ESM_XIP_MODE 0U

/* Canonical definitions for peripheral ESM_AXI_QUAD_SPI_ESM */
#define XPAR_SPI_1_DEVICE_ID 1U
#define XPAR_SPI_1_BASEADDR 0x41E00000U
#define XPAR_SPI_1_HIGHADDR 0x41E0FFFFU
#define XPAR_SPI_1_FIFO_DEPTH 16U
#define XPAR_SPI_1_FIFO_EXIST 1U
#define XPAR_SPI_1_SPI_SLAVE_ONLY 0U
#define XPAR_SPI_1_NUM_SS_BITS 1U
#define XPAR_SPI_1_NUM_TRANSFER_BITS 8U
#define XPAR_SPI_1_SPI_MODE 0U
#define XPAR_SPI_1_TYPE_OF_AXI4_INTERFACE 0U
#define XPAR_SPI_1_AXI4_BASEADDR 0U
#define XPAR_SPI_1_AXI4_HIGHADDR 0U
#define XPAR_SPI_1_XIP_MODE 0U
#define XPAR_SPI_1_USE_STARTUP 0U



/******************************************************************/

/* Definitions for driver SPIPS */
#define XPAR_XSPIPS_NUM_INSTANCES 1

/* Definitions for peripheral PS7_SPI_0 */
#define XPAR_PS7_SPI_0_DEVICE_ID 0
#define XPAR_PS7_SPI_0_BASEADDR 0xE0006000
#define XPAR_PS7_SPI_0_HIGHADDR 0xE0006FFF
#define XPAR_PS7_SPI_0_SPI_CLK_FREQ_HZ 166666672


/******************************************************************/

/* Canonical definitions for peripheral PS7_SPI_0 */
#define XPAR_XSPIPS_0_DEVICE_ID XPAR_PS7_SPI_0_DEVICE_ID
#define XPAR_XSPIPS_0_BASEADDR 0xE0006000
#define XPAR_XSPIPS_0_HIGHADDR 0xE0006FFF
#define XPAR_XSPIPS_0_SPI_CLK_FREQ_HZ 166666672


/******************************************************************/

/* Definitions for driver UARTLITE */
#define XPAR_XUARTLITE_NUM_INSTANCES 2

/* Definitions for peripheral ESM_AXI_UARTLITE_ESM */
#define XPAR_ESM_AXI_UARTLITE_ESM_BASEADDR 0x42C00000
#define XPAR_ESM_AXI_UARTLITE_ESM_HIGHADDR 0x42C0FFFF
#define XPAR_ESM_AXI_UARTLITE_ESM_DEVICE_ID 0
#define XPAR_ESM_AXI_UARTLITE_ESM_BAUDRATE 115200
#define XPAR_ESM_AXI_UARTLITE_ESM_USE_PARITY 0
#define XPAR_ESM_AXI_UARTLITE_ESM_ODD_PARITY 0
#define XPAR_ESM_AXI_UARTLITE_ESM_DATA_BITS 8


/* Definitions for peripheral ELM_AXI_UARTLITE_0 */
#define XPAR_ELM_AXI_UARTLITE_0_BASEADDR 0x42C10000
#define XPAR_ELM_AXI_UARTLITE_0_HIGHADDR 0x42C1FFFF
#define XPAR_ELM_AXI_UARTLITE_0_DEVICE_ID 1
#define XPAR_ELM_AXI_UARTLITE_0_BAUDRATE 9600
#define XPAR_ELM_AXI_UARTLITE_0_USE_PARITY 0
#define XPAR_ELM_AXI_UARTLITE_0_ODD_PARITY 0
#define XPAR_ELM_AXI_UARTLITE_0_DATA_BITS 8


/******************************************************************/

/* Canonical definitions for peripheral ESM_AXI_UARTLITE_ESM */
#define XPAR_UARTLITE_0_DEVICE_ID XPAR_ESM_AXI_UARTLITE_ESM_DEVICE_ID
#define XPAR_UARTLITE_0_BASEADDR 0x42C00000
#define XPAR_UARTLITE_0_HIGHADDR 0x42C0FFFF
#define XPAR_UARTLITE_0_BAUDRATE 115200
#define XPAR_UARTLITE_0_USE_PARITY 0
#define XPAR_UARTLITE_0_ODD_PARITY 0
#define XPAR_UARTLITE_0_DATA_BITS 8

/* Canonical definitions for peripheral ELM_AXI_UARTLITE_0 */
#define XPAR_UARTLITE_1_DEVICE_ID XPAR_ELM_AXI_UARTLITE_0_DEVICE_ID
#define XPAR_UARTLITE_1_BASEADDR 0x42C10000
#define XPAR_UARTLITE_1_HIGHADDR 0x42C1FFFF
#define XPAR_UARTLITE_1_BAUDRATE 9600
#define XPAR_UARTLITE_1_USE_PARITY 0
#define XPAR_UARTLITE_1_ODD_PARITY 0
#define XPAR_UARTLITE_1_DATA_BITS 8


/******************************************************************/

/* Definitions for driver UARTPS */
#define XPAR_XUARTPS_NUM_INSTANCES 1

/* Definitions for peripheral PS7_UART_0 */
#define XPAR_PS7_UART_0_DEVICE_ID 0
#define XPAR_PS7_UART_0_BASEADDR 0xE0000000
#define XPAR_PS7_UART_0_HIGHADDR 0xE0000FFF
#define XPAR_PS7_UART_0_UART_CLK_FREQ_HZ 100000000
#define XPAR_PS7_UART_0_HAS_MODEM 0


/******************************************************************/

/* Canonical definitions for peripheral PS7_UART_0 */
#define XPAR_XUARTPS_0_DEVICE_ID XPAR_PS7_UART_0_DEVICE_ID
#define XPAR_XUARTPS_0_BASEADDR 0xE0000000
#define XPAR_XUARTPS_0_HIGHADDR 0xE0000FFF
#define XPAR_XUARTPS_0_UART_CLK_FREQ_HZ 100000000
#define XPAR_XUARTPS_0_HAS_MODEM 0


/******************************************************************/

/* Definitions for driver WDTPS */
#define XPAR_XWDTPS_NUM_INSTANCES 1

/* Definitions for peripheral PS7_WDT_0 */
#define XPAR_PS7_WDT_0_DEVICE_ID 0
#define XPAR_PS7_WDT_0_BASEADDR 0xF8005000
#define XPAR_PS7_WDT_0_HIGHADDR 0xF8005FFF
#define XPAR_PS7_WDT_0_WDT_CLK_FREQ_HZ 111111115


/******************************************************************/

/* Canonical definitions for peripheral PS7_WDT_0 */
#define XPAR_XWDTPS_0_DEVICE_ID XPAR_PS7_WDT_0_DEVICE_ID
#define XPAR_XWDTPS_0_BASEADDR 0xF8005000
#define XPAR_XWDTPS_0_HIGHADDR 0xF8005FFF
#define XPAR_XWDTPS_0_WDT_CLK_FREQ_HZ 111111115


/******************************************************************/

/* Definitions for driver XADCPS */
#define XPAR_XADCPS_NUM_INSTANCES 1

/* Definitions for peripheral PS7_XADC_0 */
#define XPAR_PS7_XADC_0_DEVICE_ID 0
#define XPAR_PS7_XADC_0_BASEADDR 0xF8007100
#define XPAR_PS7_XADC_0_HIGHADDR 0xF8007120


/******************************************************************/

/* Canonical definitions for peripheral PS7_XADC_0 */
#define XPAR_XADCPS_0_DEVICE_ID XPAR_PS7_XADC_0_DEVICE_ID
#define XPAR_XADCPS_0_BASEADDR 0xF8007100
#define XPAR_XADCPS_0_HIGHADDR 0xF8007120


/******************************************************************/

/* Xilinx FAT File System Library (XilFFs) User Settings */
#define FILE_SYSTEM_USE_MKFS
#define FILE_SYSTEM_NUM_LOGIC_VOL 2
#define FILE_SYSTEM_USE_STRFUNC 0
#define FILE_SYSTEM_SET_FS_RPATH 0
#define FILE_SYSTEM_WORD_ACCESS
#endif  /* end of protection macro */
