/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2015-2016 Freescale Semiconductor, Inc.
 * Copyright 2016-2019, 2021-2023 NXP
 */

#ifndef PCI_S32CC_REGS_H
#define PCI_S32CC_REGS_H

/* Getters and builders for the field macros below */
#define BUILD_MASK_VALUE(field, x)	(((u32)(x) & (u32)(field##_MASK)) << (u32)(field##_LSB))
#define GET_MASK_VALUE(field)		((u32)(field##_MASK) << (u32)(field##_LSB))

#define BUILD_BIT_VALUE(field, x)	(((u32)(x) & (1)) << (u32)(field##_BIT))

/* Instance TYPE1 - DBI register offsets */

/* Status and Command Register. */

#define PCIE_CTRL_TYPE1_STATUS_COMMAND_REG	(0x4U)

/* Class Code and Revision ID Register. */

#define PCIE_CTRL_TYPE1_CLASS_CODE_REV_ID_REG	(0x8U)

/* Field definitions for TYPE1_STATUS_COMMAND_REG */

#define PCIE_IO_EN_VALUE(x)			(((x) & 0x00000001) << 0)
#define PCIE_IO_EN_BIT				(0)

#define PCIE_MSE_VALUE(x)			(((x) & 0x00000001) << 1)
#define PCIE_MSE_BIT				(1)

#define PCIE_BME_VALUE(x)			(((x) & 0x00000001) << 2)
#define PCIE_BME_BIT				(2)

#define PCIE_PERREN_BIT				(6)
#define PCIE_SERREN_BIT				(8)
#define PCIE_INT_EN_BIT				(10)

/* Instance PCIE_SPCIE_CAP_HEADER */

#define PCIE_SPCIE_CAP_HEADER_BASEADDRESS	(0x148U)

/* Lane Equalization Control Register for lanes 1 and 0. */

#define PCIE_SPCIE_CAP_SPCIE_CAP_OFF_0CH_REG \
		(PCIE_SPCIE_CAP_HEADER_BASEADDRESS + 0xCU)

/* Instance PCIE_CAP */

#define PCIE_CAP_BASEADDRESS			(0x0070U)

/* Device Control and Status Register. */

#define PCIE_CAP_DEVICE_CONTROL_DEVICE_STATUS	(PCIE_CAP_BASEADDRESS + 0x8U)

/* Slot Control and Status Register */
#define PCIE_SLOT_CONTROL_SLOT_STATUS		(PCIE_CAP_BASEADDRESS + 0x18U)
#define PCIE_CAP_PRESENCE_DETECT_CHANGE_EN_BIT	(3)
#define PCIE_CAP_HOT_PLUG_INT_EN_BIT		(5)
#define PCIE_CAP_DLL_STATE_CHANGED_EN_BIT	(12)

/* Link Control 2 and Status 2 Register. */

#define PCIE_CAP_LINK_CONTROL2_LINK_STATUS2_REG (PCIE_CAP_BASEADDRESS + 0x30U)

/* Field definitions for DEVICE_CONTROL_DEVICE_STATUS */

#define PCIE_CAP_MAX_PAYLOAD_SIZE_CS_LSB	(5)
#define PCIE_CAP_MAX_PAYLOAD_SIZE_CS_MASK	(0x00000007)

#define PCIE_CAP_MAX_READ_REQ_SIZE_LSB		(12)
#define PCIE_CAP_MAX_READ_REQ_SIZE_MASK		(0x00000007)

/* Field definitions for LINK_CONTROL2_LINK_STATUS2_REG */

#define PCIE_CAP_TARGET_LINK_SPEED_LSB		(0)
#define PCIE_CAP_TARGET_LINK_SPEED_MASK		(0x0000000F)

/* Instance PCIE_PORT_LOGIC - DBI register offsets */

#define PCIE_PORT_LOGIC_BASEADDRESS		(0x0700U)

/* Port Force Link Register. */

#define PCIE_PORT_LOGIC_PORT_FORCE_REG (PCIE_PORT_LOGIC_BASEADDRESS + 0x8U)

/* Port Link Control Register. */

#define PCIE_PORT_LOGIC_PORT_LINK_CTRL_REG (PCIE_PORT_LOGIC_BASEADDRESS + 0x10U)

/* Timer Control and Max Function Number Register. */

#define PCIE_PORT_LOGIC_TIMER_CTRL_MAX_FUNC_NUM_REG \
		(PCIE_PORT_LOGIC_BASEADDRESS + 0x18U)

/* Link Width and Speed Change Control Register. */

#define PCIE_PORT_LOGIC_GEN2_CTRL_REG \
		(PCIE_PORT_LOGIC_BASEADDRESS + 0x10CU)

/* Gen3 Control Register. */

#define PCIE_PORT_LOGIC_GEN3_RELATED_REG (PCIE_PORT_LOGIC_BASEADDRESS + 0x190U)

/* Gen3 EQ Control Register. */

#define PCIE_PORT_LOGIC_GEN3_EQ_CONTROL_REG \
		(PCIE_PORT_LOGIC_BASEADDRESS + 0x1A8U)

/* ACE Cache Coherency Control Register 3 */

#define PCIE_PORT_LOGIC_COHERENCY_CONTROL_3_REG \
		(PCIE_PORT_LOGIC_BASEADDRESS + 0x1E8U)

/* Field definitions for PORT_FORCE_REG */

#define PCIE_LINK_NUM_LSB			(0)
#define PCIE_LINK_NUM_MASK			(0x000000FF)

#define PCIE_FORCED_LTSSM_LSB			(8)
#define PCIE_FORCED_LTSSM_MASK			(0x0000000F)

#define PCIE_FORCE_EN_BIT			(15)

#define PCIE_LINK_STATE_LSB			(16)
#define PCIE_LINK_STATE_MASK			(0x0000003F)

/* Field definitions for PORT_LINK_CTRL_REG */

#define PCIE_RESET_ASSERT_VALUE(x)		(((x) & 0x00000001) << 3)
#define PCIE_RESET_ASSERT			BIT(3)

#define PCIE_DLL_LINK_EN_VALUE(x)		(((x) & 0x00000001) << 5)
#define PCIE_DLL_LINK_EN			BIT(5)

#define PCIE_LINK_DISABLE_VALUE(x)		(((x) & 0x00000001) << 6)
#define PCIE_LINK_DISABLE			BIT(6)

#define PCIE_FAST_LINK_MODE_VALUE(x)		(((x) & 0x00000001) << 7)
#define PCIE_FAST_LINK_MODE			BIT(7)

#define PCIE_LINK_RATE_LSB			(8)
#define PCIE_LINK_RATE_MASK			(0x0000000F)

#define PCIE_LINK_CAPABLE_LSB			(16)
#define PCIE_LINK_CAPABLE_MASK			(0x0000003F)

/* Field definitions for TIMER_CTRL_MAX_FUNC_NUM_REG */

#define PCIE_MAX_FUNC_NUM_LSB			(0)
#define PCIE_MAX_FUNC_NUM_MASK			(0x000000FF)

#define PCIE_FAST_LINK_SCALING_FACTOR_LSB	(29)
#define PCIE_FAST_LINK_SCALING_FACTOR_MASK	(0x00000003)

/* Field definitions for GEN2_CTRL_REG */

#define PCIE_NUM_OF_LANES_LSB			(8)
#define PCIE_NUM_OF_LANES_MASK			(0x0000001F)

#define PCIE_EQ_PHASE_2_3_VALUE(x)		(((x) & 0x00000001) << 9)
#define PCIE_EQ_PHASE_2_3			BIT(9)

/* Field definitions for GEN3_EQ_CONTROL_REG */

#define PCIE_GEN3_EQ_FB_MODE_LSB		(0)
#define PCIE_GEN3_EQ_FB_MODE_MASK		(0x0000000F)

#define PCIE_GEN3_EQ_PSET_REQ_VEC_LSB		(8)
#define PCIE_GEN3_EQ_PSET_REQ_VEC_MASK		(0x0000FFFF)

/* Field definitions for COHERENCY_CONTROL_3_REG */

#define PCIE_CFG_MSTR_ARDOMAIN_MODE_LSB		(0)
#define PCIE_CFG_MSTR_ARDOMAIN_MODE_MASK	(0x00000003)

#define PCIE_CFG_MSTR_ARCACHE_MODE_LSB		(3)
#define PCIE_CFG_MSTR_ARCACHE_MODE_MASK		(0x0000000F)

#define PCIE_CFG_MSTR_AWDOMAIN_MODE_LSB		(8)
#define PCIE_CFG_MSTR_AWDOMAIN_MODE_MASK	(0x00000003)

#define PCIE_CFG_MSTR_AWCACHE_MODE_LSB		(11)
#define PCIE_CFG_MSTR_AWCACHE_MODE_MASK		(0x0000000F)

#define PCIE_CFG_MSTR_ARDOMAIN_VALUE_LSB	(16)
#define PCIE_CFG_MSTR_ARDOMAIN_VALUE_MASK	(0x00000003)

#define PCIE_CFG_MSTR_ARCACHE_VALUE_LSB		(19)
#define PCIE_CFG_MSTR_ARCACHE_VALUE_MASK	(0x0000000F)

#define PCIE_CFG_MSTR_AWDOMAIN_VALUE_LSB	(24)
#define PCIE_CFG_MSTR_AWDOMAIN_VALUE_MASK	(0x00000003)

#define PCIE_CFG_MSTR_AWCACHE_VALUE_LSB		(27)
#define PCIE_CFG_MSTR_AWCACHE_VALUE_MASK	(0x0000000F)

/* Instance PCIE_SS - CTRL register offsets */

/* PCIe Controller 0 Link Debug 2 */

#define PCIE_SS_PE0_LINK_DBG_2			(0xB4U)

/* Field definitions for PCIE_PHY_GEN_CTRL */

#define PCIE_SS_REF_REPEAT_CLK_EN_BIT		(16)

#define PCIE_SS_REF_USE_PAD_BIT			(17)

/* Field definitions for PCIE_PHY_MPLLA_CTRL */

#define PCIE_SS_MPLLA_FORCE_EN_BIT		(0)

#define PCIE_SS_MPLLA_SSC_EN_BIT		(1)

#define PCIE_SS_MPLL_STATE_BIT			(30)

#define PCIE_SS_MPLLA_STATE_BIT			(31)

/* Field definitions for PCIE_PHY_MPLLB_CTRL */

#define PCIE_SS_MPLLB_FORCE_EN_BIT		(0)

#define PCIE_SS_MPLLB_SSC_EN_BIT		(1)

#define PCIE_SS_MPLLB_STATE_BIT			(31)

/* Field definitions for SS_RW_REG_0 */

#define PCIE_SS_SS_RW_REG_0_FIELD_LSB		(0)
#define PCIE_SS_SS_RW_REG_0_FIELD_MASK		(0xFFFFFFFF)

/* Field definitions for PE0_GEN_CTRL_1 */

#define PCIE_SS_DEVICE_TYPE_LSB			(0)
#define PCIE_SS_DEVICE_TYPE_MASK		(0x0000000F)

/* Field definitions for PE0_LINK_DBG_2 */

#define PCIE_SS_SMLH_LTSSM_STATE_LSB		(0)
#define PCIE_SS_SMLH_LTSSM_STATE_MASK		(0x0000003F)

#define PCIE_SS_SMLH_LINK_UP_BIT		(6)

#define PCIE_SS_RDLH_LINK_UP_BIT		(7)

/* Field definitions for PHY_REG_ADDR */

#define PCIE_SS_PHY_REG_ADDR_FIELD_LSB		(0)
#define PCIE_SS_PHY_REG_ADDR_FIELD_MASK		(0x0000FFFF)

#define PCIE_SS_PHY_REG_EN_BIT			(31)

/* Field definitions for PHY_REG_DATA */

#define PCIE_SS_PHY_REG_DATA_FIELD_LSB		(0)
#define PCIE_SS_PHY_REG_DATA_FIELD_MASK		(0x0000FFFF)

#define CAP_DEVICE_CONTROL_DEVICE_STATUS	(0x78U)
#define   CAP_CORR_ERR_REPORT_EN_BIT		(0)
#define   CAP_NON_FATAL_ERR_REPORT_EN_BIT	(1)
#define   CAP_FATAL_ERR_REPORT_EN_BIT		(2)
#define   CAP_UNSUPPORT_REQ_REP_EN_BIT		(3)
#define   CAP_EN_REL_ORDER_BIT			(4)
#define   CAP_MAX_PAYLOAD_SIZE_CS_LSB		(5)
#define   CAP_MAX_PAYLOAD_SIZE_CS_MASK		(0x00000007)
#define   CAP_MAX_READ_REQ_SIZE_LSB		(12)
#define   CAP_MAX_READ_REQ_SIZE_MASK		(0x00000007)

#define PORT_LOGIC_PORT_FORCE_OFF		(0x708U)
#define   PCIE_DO_DESKEW_FOR_SRIS_BIT		(23)
#define PORT_MSI_CTRL_INT_0_EN_OFF		(0x828U)
#define PORT_GEN3_RELATED_OFF			(0x890U)
#define PORT_LOGIC_GEN3_EQ_CONTROL		(0x8A8U)
#define   PCIE_GEN3_EQ_FB_MODE_LSB		(0)
#define   PCIE_GEN3_EQ_FB_MODE_MASK		(0x0000000F)

#define PORT_LOGIC_COHERENCY_CONTROL_1		(0x8E0U)
#define PORT_LOGIC_COHERENCY_CONTROL_2		(0x8E4U)
#define PORT_LOGIC_COHERENCY_CONTROL_3		(0x8E8U)
/* See definition of register "ACE Cache Coherency Control Register 1"
 * (COHERENCY_CONTROL_1_OFF) in the SoC RM
 */
#define CC_1_MEMTYPE_BOUNDARY_MASK		0xFFFFFFFC
#define CC_1_MEMTYPE_VALUE_MASK			0x1
#define CC_1_MEMTYPE_LOWER_PERIPH		0x0
#define CC_1_MEMTYPE_LOWER_MEM			0x1
#define   PCIE_CFG_MSTR_ARDOMAIN_MODE_LSB	(0)
#define   PCIE_CFG_MSTR_ARDOMAIN_MODE_MASK	(0x00000003)

#define   PCIE_CFG_MSTR_ARCACHE_MODE_LSB	(3)
#define   PCIE_CFG_MSTR_ARCACHE_MODE_MASK	(0x0000000F)

#define   PCIE_CFG_MSTR_AWDOMAIN_MODE_LSB	(8)
#define   PCIE_CFG_MSTR_AWDOMAIN_MODE_MASK	(0x00000003)

#define   PCIE_CFG_MSTR_AWCACHE_MODE_LSB	(11)
#define   PCIE_CFG_MSTR_AWCACHE_MODE_MASK	(0x0000000F)

#define   PCIE_CFG_MSTR_ARDOMAIN_VALUE_LSB	(16)
#define   PCIE_CFG_MSTR_ARDOMAIN_VALUE_MASK	(0x00000003)

#define   PCIE_CFG_MSTR_ARCACHE_VALUE_LSB	(19)
#define   PCIE_CFG_MSTR_ARCACHE_VALUE_MASK	(0x0000000F)

#define   PCIE_CFG_MSTR_AWDOMAIN_VALUE_LSB	(24)
#define   PCIE_CFG_MSTR_AWDOMAIN_VALUE_MASK	(0x00000003)

#define   PCIE_CFG_MSTR_AWCACHE_VALUE_LSB	(27)
#define   PCIE_CFG_MSTR_AWCACHE_VALUE_MASK	(0x0000000F)

#endif  /* PCI_S32CC_REGS_H */
