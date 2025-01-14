// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2020-2024 NXP
 * S32CC PCIe Host driver
 */

#include <common.h>
#include <cmd_pci.h>
#include <dm.h>
#include <errno.h>
#include <generic-phy.h>
#include <hwconfig.h>
#include <malloc.h>
#include <misc.h>
#include <nvmem.h>
#include <pci.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include <dm/uclass-internal.h>
#include <dm/uclass.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/sizes.h>
#include <linux/time.h>
#include <s32-cc/pcie.h>
#include <s32-cc/serdes_hwconfig.h>
#include <dt-bindings/phy/phy.h>

#include "pci-s32cc-regs.h"
#include "pci_s32cc.h"

#define PCIE_DEFAULT_INTERNAL_CLK 0

#define PCIE_ALIGNMENT 2

#define PCIE_TABLE_HEADER \
"PCIe:   BusDevFun       VendorId   DeviceId   Device Class       Sub-Class\n" \
"__________________________________________________________________________\n"

#define PCI_MAX_BUS_NUM			256
#define PCI_UNROLL_OFF			0x200
#define PCI_UPPER_ADDR_SHIFT		32

#define PCIE_SYMBOL_TIMER_FILTER_1	0x71cU
#define   CX_FLT_MASK_MSG_DROP_BIT	29

#define PCIE_LINKUP_MASK		(BIT_32(PCIE_SS_SMLH_LINK_UP_BIT) | \
					BIT_32(PCIE_SS_RDLH_LINK_UP_BIT) | \
					PCIE_SS_SMLH_LTSSM_STATE_MASK)
#define PCIE_LINKUP_EXPECT		(BIT_32(PCIE_SS_SMLH_LINK_UP_BIT) | \
					BIT_32(PCIE_SS_RDLH_LINK_UP_BIT) | \
					BUILD_MASK_VALUE(PCIE_SS_SMLH_LTSSM_STATE, \
							LTSSM_STATE_L0))

/* Default timeout (ms) */
#define PCIE_CX_CPL_BASE_TIMER_VALUE	100

/* PHY link timeout */
#define PCIE_LINK_TIMEOUT_MS		1000
#define PCIE_LINK_TIMEOUT_US		(PCIE_LINK_TIMEOUT_MS * USEC_PER_MSEC)
#define PCIE_LINK_WAIT_US		100

enum pcie_dev_type_val {
	PCIE_EP_VAL = 0x0,
	PCIE_RC_VAL = 0x4
};

static inline
void s32cc_pcie_write(struct dw_pcie *pci,
		      void __iomem *base, u32 reg, size_t size, u32 val)
{
	int ret;
	struct s32cc_pcie *s32cc_pci = to_s32cc_from_dw_pcie(pci);

	if (IS_ENABLED(CONFIG_PCI_S32CC_DEBUG_WRITES)) {
		if ((uintptr_t)base == (uintptr_t)(s32cc_pci->ctrl_base))
			dev_dbg(pci->dev, "W%d(ctrl+0x%x, 0x%x)\n",
				(int)size * 8, (u32)(reg), (u32)(val));
		else if ((uintptr_t)base == (uintptr_t)(pci->atu_base))
			dev_dbg(pci->dev, "W%d(atu+0x%x, 0x%x)\n",
				(int)size * 8, (u32)(reg), (u32)(val));
		else if ((uintptr_t)base == (uintptr_t)(pci->dbi_base))
			dev_dbg(pci->dev, "W%d(dbi+0x%x, 0x%x)\n",
				(int)size * 8, (u32)(reg), (u32)(val));
		else if ((uintptr_t)base == (uintptr_t)(pci->dbi_base2))
			dev_dbg(pci->dev, "W%d(dbi2+0x%x, 0x%x)\n",
				(int)size * 8, (u32)(reg), (u32)(val));
		else
			dev_dbg(pci->dev, "W%d(%lx+0x%x, 0x%x)\n",
				(int)size * 8, (uintptr_t)(base), (u32)(reg),
				(u32)(val));
	}

	ret = dw_pcie_write(base + reg, size, val);
	if (ret)
		dev_err(pci->dev, "PCIe%d: Write to address 0x%lx failed\n",
			s32cc_pci->id, (uintptr_t)(base + reg));
}

void dw_pcie_writel_ctrl(struct s32cc_pcie *pci, u32 reg, u32 val)
{
	s32cc_pcie_write(&pci->pcie, pci->ctrl_base, reg, 0x4, val);
}

u32 dw_pcie_readl_ctrl(struct s32cc_pcie *pci, u32 reg)
{
	u32 val = 0;

	if (dw_pcie_read(pci->ctrl_base + reg, 0x4, &val))
		dev_err(pci->pcie.dev, "Read ctrl address failed\n");

	return val;
}

static void s32cc_pcie_cfg0_set_busdev(struct s32cc_pcie *s32cc_pp, u32 busdev)
{
	dw_pcie_writel_ob_unroll(&s32cc_pp->pcie, PCIE_ATU_REGION_INDEX0,
				 PCIE_ATU_UNR_LOWER_TARGET, busdev);
}

static void s32cc_pcie_cfg1_set_busdev(struct s32cc_pcie *s32cc_pp, u32 busdev)
{
	dw_pcie_writel_ob_unroll(&s32cc_pp->pcie, PCIE_ATU_REGION_INDEX1,
				 PCIE_ATU_UNR_LOWER_TARGET, busdev);
}

void s32cc_pcie_dump_atu(struct s32cc_pcie *s32cc_pp)
{
	int i;
	struct dw_pcie *pcie = &s32cc_pp->pcie;

	if (!IS_ENABLED(CONFIG_PCI_S32CC_DEBUG))
		return;

	for (i = 0; i < s32cc_pp->atu_out_num; i++) {
		debug("PCIe%d: OUT iATU%d:\n", s32cc_pp->id, i);
		debug("\tLOWER PHYS 0x%08x\n",
		      dw_pcie_readl_ob_unroll(pcie, i,
					      PCIE_ATU_UNR_LOWER_BASE));
		debug("\tUPPER PHYS 0x%08x\n",
		      dw_pcie_readl_ob_unroll(pcie, i,
					      PCIE_ATU_UNR_UPPER_BASE));
		debug("\tLOWER BUS  0x%08x\n",
		      dw_pcie_readl_ob_unroll(pcie, i,
					      PCIE_ATU_UNR_LOWER_TARGET));
		debug("\tUPPER BUS  0x%08x\n",
		      dw_pcie_readl_ob_unroll(pcie, i,
					      PCIE_ATU_UNR_UPPER_TARGET));
		debug("\tLIMIT      0x%08x\n",
		      dw_pcie_readl_ob_unroll(pcie, i,
					      PCIE_ATU_UNR_LIMIT));
		debug("\tCR1        0x%08x\n",
		      dw_pcie_readl_ob_unroll(pcie, i,
					      PCIE_ATU_UNR_REGION_CTRL1));
		debug("\tCR2        0x%08x\n",
		      dw_pcie_readl_ob_unroll(pcie, i,
					      PCIE_ATU_UNR_REGION_CTRL2));
	}

	for (i = 0; i < s32cc_pp->atu_in_num; i++) {
		debug("PCIe%d: IN iATU%d:\n", s32cc_pp->id, i);
		debug("\tLOWER PHYS 0x%08x\n",
		      dw_pcie_readl_ib_unroll(pcie, i,
					      PCIE_ATU_UNR_LOWER_BASE));
		debug("\tUPPER PHYS 0x%08x\n",
		      dw_pcie_readl_ib_unroll(pcie, i,
					      PCIE_ATU_UNR_UPPER_BASE));
		debug("\tLOWER BUS  0x%08x\n",
		      dw_pcie_readl_ib_unroll(pcie, i,
					      PCIE_ATU_UNR_LOWER_TARGET));
		debug("\tUPPER BUS  0x%08x\n",
		      dw_pcie_readl_ib_unroll(pcie, i,
					      PCIE_ATU_UNR_UPPER_TARGET));
		debug("\tLIMIT      0x%08x\n",
		      dw_pcie_readl_ib_unroll(pcie, i,
					      PCIE_ATU_UNR_LIMIT));
		debug("\tCR1        0x%08x\n",
		      dw_pcie_readl_ib_unroll(pcie, i,
					      PCIE_ATU_UNR_REGION_CTRL1));
		debug("\tCR2        0x%08x\n",
		      dw_pcie_readl_ib_unroll(pcie, i,
					      PCIE_ATU_UNR_REGION_CTRL2));
	}
}

static void s32cc_pcie_rc_setup_atu(struct s32cc_pcie *s32cc_pp)
{
	struct dw_pcie *pcie = &s32cc_pp->pcie;
	u64 cfg_start = (u64)s32cc_pp->cfg0;
	size_t cfg_size = pcie->cfg_size / 2;
	u64 limit = cfg_start + cfg_size;

	struct pci_region *io = NULL, *mem = NULL, *pref = NULL;

	s32cc_pp->atu_out_num = 0;
	s32cc_pp->atu_in_num = 0;

	debug("PCIe%d: %s: Create outbound windows\n",
	      s32cc_pp->id, __func__);

	pcie_dw_prog_outbound_atu_unroll(pcie, s32cc_pp->atu_out_num++,
					 PCIE_ATU_TYPE_CFG0, cfg_start,
					 0x0, cfg_size);
	pcie_dw_prog_outbound_atu_unroll(pcie, s32cc_pp->atu_out_num++,
					 PCIE_ATU_TYPE_CFG1, limit,
					 0x0, cfg_size);

	/* Create regions returned by pci_get_regions() */

	pci_get_regions(pcie->dev, &io, &mem, &pref);

	if (io) {
		/* OUTBOUND WIN: IO */
		pcie_dw_prog_outbound_atu_unroll(pcie, s32cc_pp->atu_out_num++,
						 PCIE_ATU_TYPE_IO, io->phys_start,
						 io->bus_start, io->size);
	}
	if (mem) {
		/* OUTBOUND WIN: MEM */
		pcie_dw_prog_outbound_atu_unroll(pcie, s32cc_pp->atu_out_num++,
						 PCIE_ATU_TYPE_MEM, mem->phys_start,
						 mem->bus_start, mem->size);
	}
	if (pref) {
		/* OUTBOUND WIN: pref MEM */
		pcie_dw_prog_outbound_atu_unroll(pcie, s32cc_pp->atu_out_num++,
						 PCIE_ATU_TYPE_MEM, pref->phys_start,
						 pref->bus_start, pref->size);
	}

	s32cc_pcie_dump_atu(s32cc_pp);
}

/* Return 0 if the address is valid, -errno if not valid */
static int s32cc_pcie_addr_valid(struct s32cc_pcie *pcie_rc, pci_dev_t bdf)
{
	struct pcie_dw *pcie = &pcie_rc->pcie;
	struct udevice *bus = pcie->dev;

	if ((readb(pcie->dbi_base + PCI_HEADER_TYPE) & 0x7f) == PCI_HEADER_TYPE_NORMAL)
		return -ENODEV;

	debug("%s: bus: %d seq: %d\n", __func__, PCI_BUS(bdf), dev_seq(bus));
	if (PCI_BUS(bdf) < dev_seq(bus))
		return -EINVAL;

	if ((PCI_BUS(bdf) > dev_seq(bus)) && (!pcie->ops || !pcie->ops->link_up(pcie)))
		return -EINVAL;

	if (PCI_BUS(bdf) <= (dev_seq(bus) + 1) && (PCI_DEV(bdf) > 0))
		return -EINVAL;

	return 0;
}

static
int s32cc_pcie_conf_address(const struct udevice *bus, pci_dev_t bdf,
			    uint offset, void **paddress)
{
	struct s32cc_pcie *pcie_rc = dev_get_priv(bus);
	struct pcie_dw *pcie = &pcie_rc->pcie;
	u32 busdev;

	if (s32cc_pcie_addr_valid(pcie_rc, bdf))
		return -EINVAL;

	if (PCI_BUS(bdf) == dev_seq(bus)) {
		*paddress = pcie->dbi_base + offset;
		return 0;
	}

	busdev = PCIE_ATU_BUS(PCI_BUS(bdf) - dev_seq(bus)) |
		 PCIE_ATU_DEV(PCI_DEV(bdf)) |
		 PCIE_ATU_FUNC(PCI_FUNC(bdf));

	if (PCI_BUS(bdf) == dev_seq(bus) + 1) {
		s32cc_pcie_cfg0_set_busdev(pcie_rc, busdev);
		*paddress = pcie_rc->cfg0 + offset;
	} else {
		s32cc_pcie_cfg1_set_busdev(pcie_rc, busdev);
		*paddress = pcie_rc->cfg1 + offset;
	}

	return 0;
}

static int s32cc_pcie_read_config(const struct udevice *bus, pci_dev_t bdf,
				  uint offset, ulong *valuep,
				  enum pci_size_t size)
{
	if (IS_ENABLED(CONFIG_PCI_S32CC_USE_DW_CFG_IATU_SETUP))
		return pcie_dw_read_config(bus, bdf, offset, valuep, size);

	return pci_generic_mmap_read_config(bus, s32cc_pcie_conf_address,
					    bdf, offset, valuep, size);
}

static int s32cc_pcie_write_config(struct udevice *bus, pci_dev_t bdf,
				   uint offset, ulong value,
				   enum pci_size_t size)
{
	if (IS_ENABLED(CONFIG_PCI_S32CC_USE_DW_CFG_IATU_SETUP))
		return pcie_dw_write_config(bus, bdf, offset, value, size);

	return pci_generic_mmap_write_config(bus, s32cc_pcie_conf_address,
					     bdf, offset, value, size);
}

/* Clear multi-function bit */
static void s32cc_pcie_clear_multifunction(struct s32cc_pcie *pcie)
{
	dw_pcie_writeb_dbi(&pcie->pcie, PCI_HEADER_TYPE,
			   PCI_HEADER_TYPE_BRIDGE);
}

/* Drop MSG TLP except for Vendor MSG */
static void s32cc_pcie_drop_msg_tlp(struct s32cc_pcie *pcie)
{
	u32 val;

	val = dw_pcie_readl_dbi(&pcie->pcie, PCIE_SYMBOL_TIMER_FILTER_1);
	val &= ~BIT_32(CX_FLT_MASK_MSG_DROP_BIT);
	dw_pcie_writel_dbi(&pcie->pcie, PCIE_SYMBOL_TIMER_FILTER_1, val);
}

int s32cc_check_serdes(struct udevice *dev)
{
	struct nvmem_cell c;
	int ret;
	u32 serdes_presence = 0;

	ret = nvmem_cell_get_by_name(dev, "serdes_presence", &c);
	if (ret) {
		printf("Failed to get 'serdes_presence' cell\n");
		return ret;
	}

	ret = nvmem_cell_read(&c, &serdes_presence, sizeof(serdes_presence));
	if (ret) {
		printf("%s: Failed to read cell 'serdes_presence' (err = %d)\n",
		       __func__, ret);
		return ret;
	}

	if (!serdes_presence) {
		printf("SerDes Subsystem not present, skipping PCIe config\n");
		return -ENODEV;
	}

	return 0;
}

static u32 s32cc_pcie_get_dev_id_variant(struct udevice *dev)
{
	struct nvmem_cell c;
	int ret;
	u32 variant_bits = 0;

	ret = nvmem_cell_get_by_name(dev, "pcie_dev_id", &c);
	if (ret) {
		printf("Failed to get 'pcie_dev_id' cell\n");
		return ret;
	}

	ret = nvmem_cell_read(&c, &variant_bits, sizeof(variant_bits));
	if (ret) {
		printf("%s: Failed to read cell 'pcie_dev_id' (err = %d)\n",
		       __func__, ret);
		return ret;
	}

	return variant_bits;
}

static void s32cc_pcie_disable_ltssm(struct s32cc_pcie *pci)
{
	u32 gen_ctrl_3 = dw_pcie_readl_ctrl(pci, PE0_GEN_CTRL_3);

	gen_ctrl_3 &= ~(LTSSM_EN_MASK);

	dw_pcie_dbi_ro_wr_en(&pci->pcie);
	dw_pcie_writel_ctrl(pci, PE0_GEN_CTRL_3, gen_ctrl_3);
	dw_pcie_dbi_ro_wr_dis(&pci->pcie);
}

static void s32cc_pcie_enable_ltssm(struct s32cc_pcie *pci)
{
	u32 gen_ctrl_3 = dw_pcie_readl_ctrl(pci, PE0_GEN_CTRL_3) |
				LTSSM_EN_MASK;

	dw_pcie_dbi_ro_wr_en(&pci->pcie);
	dw_pcie_writel_ctrl(pci, PE0_GEN_CTRL_3, gen_ctrl_3);
	dw_pcie_dbi_ro_wr_dis(&pci->pcie);
}

static bool is_s32cc_pcie_ltssm_enabled(struct s32cc_pcie *pci)
{
	return (dw_pcie_readl_ctrl(pci, PE0_GEN_CTRL_3) & LTSSM_EN_MASK);
}

static bool has_data_phy_link(struct s32cc_pcie *s32cc_pp)
{
	u32 val = dw_pcie_readl_ctrl(s32cc_pp, PCIE_SS_PE0_LINK_DBG_2);

	return (val & PCIE_LINKUP_MASK) == PCIE_LINKUP_EXPECT;
}

static int s32cc_pcie_link_is_up(struct dw_pcie *pcie)
{
	struct s32cc_pcie *s32cc_pp = to_s32cc_from_dw_pcie(pcie);

	if (!is_s32cc_pcie_ltssm_enabled(s32cc_pp))
		return 0;

	return has_data_phy_link(s32cc_pp);
}

static int wait_phy_data_link(struct s32cc_pcie *s32cc_pp)
{
	bool has_link;
	int ret = read_poll_timeout(has_data_phy_link, s32cc_pp, has_link, has_link,
			PCIE_LINK_WAIT_US, PCIE_LINK_TIMEOUT_US);

	if (ret)
		dev_dbg(s32cc_pp->pcie.dev, "Failed to stabilize PHY link\n");

	return ret;
}

static bool speed_change_completed(struct dw_pcie *pcie)
{
	u32 ctrl = dw_pcie_readl_dbi(pcie, PCIE_LINK_WIDTH_SPEED_CONTROL);

	return ctrl & PORT_LOGIC_SPEED_CHANGE;
}

static int s32cc_pcie_get_link_speed(struct dw_pcie *pcie)
{
	u32 cap_offset = dw_pcie_find_capability(pcie, PCI_CAP_ID_EXP);
	u32 link_sta = dw_pcie_readw_dbi(pcie, cap_offset + PCI_EXP_LNKSTA);

	/* return link speed based on negotiated link status */
	return link_sta & PCI_EXP_LNKSTA_CLS;
}

static u32 s32cc_pcie_get_link_width(struct dw_pcie *pcie)
{
	u32 cap_offset = dw_pcie_find_capability(pcie, PCI_CAP_ID_EXP);
	u32 link_sta = dw_pcie_readw_dbi(pcie, cap_offset + PCI_EXP_LNKSTA);

	return (link_sta & PCI_EXP_LNKSTA_NLW) >> PCI_EXP_LNKSTA_NLW_SHIFT;
}

static int s32cc_pcie_start_link(struct dw_pcie *pcie)
{
	struct s32cc_pcie *s32cc_pp = to_s32cc_from_dw_pcie(pcie);
	u32 tmp, cap_offset;
	bool speed_set;
	int ret = 0;

	/* Don't do anything if not Root Complex */
	if (!is_s32cc_pcie_rc(s32cc_pp->mode))
		return 0;

	/* Try to (re)establish the link, starting with Gen1 */
	s32cc_pcie_disable_ltssm(s32cc_pp);

	dw_pcie_dbi_ro_wr_en(pcie);
	cap_offset = dw_pcie_find_capability(pcie, PCI_CAP_ID_EXP);
	tmp = (dw_pcie_readl_dbi(pcie, cap_offset + PCI_EXP_LNKCAP) &
			~(PCI_EXP_LNKCAP_SLS)) | PCI_EXP_LNKCAP_SLS_2_5GB;
	dw_pcie_writel_dbi(pcie, cap_offset + PCI_EXP_LNKCAP, tmp);
	dw_pcie_dbi_ro_wr_dis(pcie);

	/* Start LTSSM. */
	s32cc_pcie_enable_ltssm(s32cc_pp);

	dw_pcie_dbi_ro_wr_en(pcie);
	/* Allow Gen2 or Gen3 mode after the link is up.
	 * s32cc_pcie.linkspeed is one of the speeds defined in pci_regs.h:
	 * PCI_EXP_LNKCAP_SLS_2_5GB for Gen1
	 * PCI_EXP_LNKCAP_SLS_5_0GB for Gen2
	 * PCI_EXP_LNKCAP_SLS_8_0GB for Gen3
	 */
	tmp = (dw_pcie_readl_dbi(pcie, cap_offset + PCI_EXP_LNKCAP) &
			~(PCI_EXP_LNKCAP_SLS)) | s32cc_pp->linkspeed;
	dw_pcie_writel_dbi(pcie, cap_offset + PCI_EXP_LNKCAP, tmp);

	/*
	 * Start Directed Speed Change so the best possible speed both link
	 * partners support can be negotiated.
	 * The manual says:
	 * When you set the default of the Directed Speed Change field of the
	 * Link Width and Speed Change Control register
	 * (GEN2_CTRL_OFF.DIRECT_SPEED_CHANGE) using the
	 * DEFAULT_GEN2_SPEED_CHANGE configuration parameter to 1, then
	 * the speed change is initiated automatically after link up, and the
	 * controller clears the contents of GEN2_CTRL_OFF.DIRECT_SPEED_CHANGE.
	 */
	tmp = dw_pcie_readl_dbi(pcie, PCIE_LINK_WIDTH_SPEED_CONTROL) |
			PORT_LOGIC_SPEED_CHANGE;
	dw_pcie_writel_dbi(pcie, PCIE_LINK_WIDTH_SPEED_CONTROL, tmp);
	dw_pcie_dbi_ro_wr_dis(pcie);

	ret = read_poll_timeout(speed_change_completed, pcie, speed_set,
				speed_set, PCIE_LINK_WAIT_US,
				PCIE_LINK_TIMEOUT_US);

	/* Make sure link training is finished as well! */
	if (!ret) {
		ret = wait_phy_data_link(s32cc_pp);
	} else {
		dev_err(pcie->dev, "Speed change timeout\n");
		ret = -EINVAL;
	}

	if (!ret)
		dev_info(pcie->dev, "X%d, Gen%d\n",
			 s32cc_pcie_get_link_width(pcie),
			 s32cc_pcie_get_link_speed(pcie));

	return ret;
}

void s32cc_pcie_set_device_id(struct s32cc_pcie *s32cc_pp)
{
	struct dw_pcie *pcie = &s32cc_pp->pcie;
	u32 pcie_vendor_id = PCI_VENDOR_ID_FREESCALE, pcie_variant_bits = 0;
	struct udevice *dev = s32cc_pp->pcie.dev;

	pcie_variant_bits = dev_read_u32_default(dev, "pcie_device_id", 0);
	if (!pcie_variant_bits)
		pcie_variant_bits = s32cc_pcie_get_dev_id_variant(dev);
	if (!pcie_variant_bits) {
		dev_info(dev, "Could not set DEVICE ID\n");
		return;
	}

	/* Write PCI Vendor and Device ID. */
	pcie_vendor_id |= pcie_variant_bits << PCI_DEVICE_ID_SHIFT;
	dev_dbg(dev, "Setting PCI Device and Vendor IDs to 0x%x:0x%x\n",
		(u32)(pcie_vendor_id >> PCI_DEVICE_ID_SHIFT),
		(u32)(pcie_vendor_id & GENMASK(15, 0)));
	dw_pcie_dbi_ro_wr_en(pcie);
	dw_pcie_writel_dbi(pcie, PCI_VENDOR_ID, pcie_vendor_id);

	if (pcie_vendor_id != dw_pcie_readl_dbi(pcie, PCI_VENDOR_ID))
		dev_info(dev, "PCI Device and Vendor IDs could not be set\n");

	dw_pcie_dbi_ro_wr_dis(pcie);
}

static void s32cc_pcie_set_phy_mode(struct s32cc_pcie *s32cc_pp)
{
	struct dw_pcie *pcie = &s32cc_pp->pcie;
	struct udevice *dev = pcie->dev;
	const char *pcie_phy_mode;

	pcie_phy_mode = dev_read_string(dev, "nxp,phy-mode");
	if (!pcie_phy_mode) {
		dev_info(dev, "Missing 'nxp,phy-mode' property, using default CRNS\n");
		s32cc_pp->phy_mode = CRNS;
	} else if (!strcmp(pcie_phy_mode, "crns")) {
		s32cc_pp->phy_mode = CRNS;
	} else if (!strcmp(pcie_phy_mode, "crss")) {
		s32cc_pp->phy_mode = CRSS;
	} else if (!strcmp(pcie_phy_mode, "srns")) {
		s32cc_pp->phy_mode = SRNS;

		/* When using internal clock, SRNS phy mode is only
		 * supported @speed < GEN3
		 */
		if (!s32_serdes_is_external_clk_in_hwconfig(s32cc_pp->id)) {
			if (s32cc_pp->linkspeed > GEN2) {
				dev_info(dev, "SRNS phy mode with internal clock @maximum GEN2 speed\n");
				s32cc_pp->linkspeed = GEN2;
			}
		}
	} else if (!strcmp(pcie_phy_mode, "sris")) {
		s32cc_pp->phy_mode = SRIS;
	} else {
		dev_warn(dev, "Unsupported 'nxp,phy-mode' specified, using default CRNS\n");
		s32cc_pp->phy_mode = CRNS;
	}
}

int s32cc_pcie_dt_init_common(struct s32cc_pcie *s32cc_pp)
{
	struct dw_pcie *pcie = &s32cc_pp->pcie;
	struct udevice *dev = pcie->dev;
	int ret = 0;

	ret = dev_read_alias_seq(dev, &s32cc_pp->id);
	if (ret < 0) {
		dev_dbg(dev, "Failed to get PCIe sequence id\n");
		s32cc_pp->id = dev_read_s32_default(dev, "device_id", (-1));
		if (s32cc_pp->id == (-1)) {
			dev_err(dev, "Failed to get PCIe id\n");
			return -EINVAL;
		}
	}

	ret = generic_phy_get_by_name(dev, "serdes_lane0", &s32cc_pp->phy0);
	if (ret) {
		dev_err(dev, "Failed to get PHY 'serdes_lane0'\n");
		return ret;
	}
	/* PHY on lane 1 is optional */
	ret = generic_phy_get_by_name(dev, "serdes_lane1", &s32cc_pp->phy1);
	if (ret)
		dev_dbg(dev, "PHY 'serdes_lane1' not available\n");

	pcie->dbi_base = (void *)dev_read_addr_name(dev, "dbi");
	if ((fdt_addr_t)pcie->dbi_base == FDT_ADDR_T_NONE) {
		dev_err(dev, "PCIe%d: resource 'dbi' not found\n",
			s32cc_pp->id);
		return -EINVAL;
	}
	dev_dbg(dev, "Dbi base: 0x%lx\n", (uintptr_t)pcie->dbi_base);

	pcie->dbi_base2 = (void *)dev_read_addr_name(dev, "dbi2");
	if ((fdt_addr_t)pcie->dbi_base2 == FDT_ADDR_T_NONE) {
		dev_err(dev, "PCIe%d: resource 'dbi2' not found\n",
			s32cc_pp->id);
		return -EINVAL;
	}
	dev_dbg(dev, "Dbi2 base: 0x%lx\n", (uintptr_t)pcie->dbi_base2);

	pcie->atu_base = (void *)dev_read_addr_name(dev, "atu");
	if ((fdt_addr_t)pcie->atu_base == FDT_ADDR_T_NONE) {
		dev_err(dev, "PCIe%d: resource 'atu' not found\n",
			s32cc_pp->id);
		return -EINVAL;
	}
	dev_dbg(dev, "Atu base: 0x%lx\n", (uintptr_t)pcie->atu_base);

	pcie->cfg_base = (void *)dev_read_addr_size_name(dev, "config",
							 &pcie->cfg_size);
	if ((fdt_addr_t)pcie->cfg_base == FDT_ADDR_T_NONE) {
		dev_err(dev, "PCIe%d: resource 'config' not found\n",
			s32cc_pp->id);
		return -EINVAL;
	}
	dev_dbg(dev, "Config base: 0x%lx\n", (uintptr_t)pcie->cfg_base);

	if (!IS_ENABLED(CONFIG_PCI_S32CC_USE_DW_CFG_IATU_SETUP)) {
		s32cc_pp->cfg0 = pcie->cfg_base;

		s32cc_pp->cfg1 = s32cc_pp->cfg0 + pcie->cfg_size / 2;
		s32cc_pp->cfg0_seq = -ENODEV;
	}

	s32cc_pp->ctrl_base  = (void *)dev_read_addr_name(dev, "ctrl");
	if ((fdt_addr_t)s32cc_pp->ctrl_base == FDT_ADDR_T_NONE) {
		dev_err(dev, "PCIe%d: resource 'ctrl' not found\n",
			s32cc_pp->id);
		return -EINVAL;
	}
	dev_dbg(dev, "Ctrl base: 0x%lx\n", (uintptr_t)s32cc_pp->ctrl_base);

	/* get supported speed (Gen1/Gen2/Gen3) from device tree */
	s32cc_pp->linkspeed = (enum pcie_link_speed)
		dev_read_u32_default(dev, "max-link-speed", GEN1);
	if (s32cc_pp->linkspeed  < GEN1 || s32cc_pp->linkspeed > GEN3) {
		dev_info(dev, "PCIe%d: Invalid speed\n", s32cc_pp->id);
		s32cc_pp->linkspeed  = GEN1;
	}

	s32cc_pcie_set_phy_mode(s32cc_pp);

	return 0;
}

/* s32cc_pcie_dt_init_host - Function intended to initialize platform
 * data from the (live) device tree.
 * Note that it is called before the probe function.
 */
int s32cc_pcie_dt_init_host(struct udevice *dev)
{
	struct s32cc_pcie *s32cc_pp = dev_get_priv(dev);
	struct dw_pcie *pcie = &s32cc_pp->pcie;

	s32cc_pp->mode = DW_PCIE_UNKNOWN_TYPE;
	pcie->dev = dev;

	return s32cc_pcie_dt_init_common(s32cc_pp);
}

static void disable_equalization(struct dw_pcie *pcie)
{
	u32 val;

	dw_pcie_dbi_ro_wr_en(pcie);

	val = dw_pcie_readl_dbi(pcie, PORT_LOGIC_GEN3_EQ_CONTROL);
	val &= ~(GET_MASK_VALUE(PCIE_GEN3_EQ_FB_MODE) |
		GET_MASK_VALUE(PCIE_GEN3_EQ_PSET_REQ_VEC));
	val |= BUILD_MASK_VALUE(PCIE_GEN3_EQ_FB_MODE, 1) |
		BUILD_MASK_VALUE(PCIE_GEN3_EQ_PSET_REQ_VEC, 0x84);
	dw_pcie_writel_dbi(pcie, PORT_LOGIC_GEN3_EQ_CONTROL, val);

	dw_pcie_dbi_ro_wr_dis(pcie);

	/* Test value */
	dev_dbg(pcie->dev, "PCIE_PORT_LOGIC_GEN3_EQ_CONTROL: 0x%08x\n",
		dw_pcie_readl_dbi(pcie, PORT_LOGIC_GEN3_EQ_CONTROL));
}

static int init_pcie(struct s32cc_pcie *s32cc_pp)
{
	struct dw_pcie *pcie = &s32cc_pp->pcie;
	struct udevice *dev = pcie->dev;
	u32 val;

	if (is_s32cc_pcie_rc(s32cc_pp->mode))
		val = dw_pcie_readl_ctrl(s32cc_pp, PE0_GEN_CTRL_1) |
				BUILD_MASK_VALUE(DEVICE_TYPE, PCIE_RC_VAL);
	else
		val = dw_pcie_readl_ctrl(s32cc_pp, PE0_GEN_CTRL_1) |
				BUILD_MASK_VALUE(DEVICE_TYPE, PCIE_EP_VAL);

	dw_pcie_writel_ctrl(s32cc_pp, PE0_GEN_CTRL_1, val);

	if (s32cc_pp->phy_mode == SRIS) {
		val = dw_pcie_readl_ctrl(s32cc_pp, PE0_GEN_CTRL_1) |
				SRIS_MODE_MASK;
		dw_pcie_writel_ctrl(s32cc_pp, PE0_GEN_CTRL_1, val);
	}

	/* Enable writing dbi registers */
	dw_pcie_dbi_ro_wr_en(pcie);

	/* Enable direct speed change */
	val = dw_pcie_readl_dbi(pcie, PCIE_LINK_WIDTH_SPEED_CONTROL);
	val |= PORT_LOGIC_SPEED_CHANGE;
	dw_pcie_writel_dbi(pcie, PCIE_LINK_WIDTH_SPEED_CONTROL, val);
	dw_pcie_dbi_ro_wr_dis(pcie);

	/* Disable phase 2,3 equalization */
	disable_equalization(pcie);

	/* PCIE_COHERENCY_CONTROL_<n> registers provide defaults that configure
	 * the transactions as Outer Shareable, Write-Back cacheable; we won't
	 * change those defaults.
	 */

	/* Make sure DBI registers are R/W */
	dw_pcie_dbi_ro_wr_en(pcie);

	val = dw_pcie_readl_dbi(pcie, PORT_LOGIC_PORT_FORCE_OFF);
	val |= BIT_32(PCIE_DO_DESKEW_FOR_SRIS_BIT);
	dw_pcie_writel_dbi(pcie, PORT_LOGIC_PORT_FORCE_OFF, val);

	if (is_s32cc_pcie_rc(s32cc_pp->mode)) {
		/* Set max payload supported, 256 bytes and
		 * relaxed ordering.
		 */
		val = dw_pcie_readl_dbi(pcie, CAP_DEVICE_CONTROL_DEVICE_STATUS);
		val &= ~(BIT_32(CAP_EN_REL_ORDER_BIT) |
			GET_MASK_VALUE(CAP_MAX_PAYLOAD_SIZE_CS) |
			GET_MASK_VALUE(CAP_MAX_READ_REQ_SIZE));
		val |= BIT_32(CAP_EN_REL_ORDER_BIT) |
			BUILD_MASK_VALUE(CAP_MAX_PAYLOAD_SIZE_CS, 1) |
			BUILD_MASK_VALUE(CAP_MAX_READ_REQ_SIZE, 1),
		dw_pcie_writel_dbi(pcie, CAP_DEVICE_CONTROL_DEVICE_STATUS, val);

		/* Enable the IO space, Memory space, Bus master,
		 * Parity error, Serr and disable INTx generation
		 */
		dw_pcie_writel_dbi(pcie, PCIE_CTRL_TYPE1_STATUS_COMMAND_REG,
				   BIT_32(PCIE_SERREN_BIT) | BIT_32(PCIE_PERREN_BIT) |
				   BIT_32(PCIE_INT_EN_BIT) | BIT_32(PCIE_IO_EN_BIT) |
				   BIT_32(PCIE_MSE_BIT) | BIT_32(PCIE_BME_BIT));
		/* Test value */
		dev_dbg(dev, "PCIE_CTRL_TYPE1_STATUS_COMMAND_REG reg: 0x%08x\n",
			dw_pcie_readl_dbi(pcie,
					  PCIE_CTRL_TYPE1_STATUS_COMMAND_REG));

		/* Enable errors */
		val = dw_pcie_readl_dbi(pcie, CAP_DEVICE_CONTROL_DEVICE_STATUS);
		val |=  BIT_32(CAP_CORR_ERR_REPORT_EN_BIT) |
			BIT_32(CAP_NON_FATAL_ERR_REPORT_EN_BIT) |
			BIT_32(CAP_FATAL_ERR_REPORT_EN_BIT) |
			BIT_32(CAP_UNSUPPORT_REQ_REP_EN_BIT);
		dw_pcie_writel_dbi(pcie, CAP_DEVICE_CONTROL_DEVICE_STATUS, val);
	}

	val = dw_pcie_readl_dbi(pcie, PORT_GEN3_RELATED_OFF);
	val |= PCIE_EQ_PHASE_2_3;
	dw_pcie_writel_dbi(pcie, PORT_GEN3_RELATED_OFF, val);

	/* Disable writing dbi registers */
	dw_pcie_dbi_ro_wr_dis(pcie);

	s32cc_pcie_enable_ltssm(s32cc_pp);

	return 0;
}

static int init_pcie_phy(struct s32cc_pcie *s32cc_pp)
{
	struct dw_pcie *pcie = &s32cc_pp->pcie;
	struct udevice *dev = pcie->dev;
	int ret = 0;

	if (!generic_phy_valid(&s32cc_pp->phy0))
		return -ENODEV;

	generic_phy_reset(&s32cc_pp->phy0);
	ret = generic_phy_init(&s32cc_pp->phy0);
	if (ret) {
		dev_err(dev, "Failed to init PHY 'serdes_lane0'\n");
		return ret;
	}

	ret = generic_phy_set_mode_ext(&s32cc_pp->phy0, PHY_TYPE_PCIE,
				       s32cc_pp->phy_mode);
	if (ret) {
		dev_err(dev, "Failed to set mode on PHY 'serdes_lane0'\n");
		return ret;
	}

	ret = generic_phy_power_on(&s32cc_pp->phy0);
	if (ret) {
		dev_err(dev, "Failed to power on PHY 'serdes_lane0'\n");
		return ret;
	}

	if (!generic_phy_valid(&s32cc_pp->phy1))
		return ret;

	generic_phy_reset(&s32cc_pp->phy1);
	ret = generic_phy_init(&s32cc_pp->phy1);
	if (ret) {
		dev_err(dev, "Failed to init PHY 'serdes_lane1'\n");
		return ret;
	}

	ret = generic_phy_set_mode_ext(&s32cc_pp->phy1, PHY_TYPE_PCIE,
				       s32cc_pp->phy_mode);
	if (ret) {
		dev_err(dev, "Failed to set mode on PHY 'serdes_lane1'\n");
		return ret;
	}

	ret = generic_phy_power_on(&s32cc_pp->phy1);
	if (ret) {
		dev_err(dev, "Failed to power on PHY 'serdes_lane1'\n");
		return ret;
	}

	return 0;
}

int s32cc_pcie_init_controller(struct s32cc_pcie *s32cc_pp)
{
	struct dw_pcie *pcie = &s32cc_pp->pcie;
	int ret = 0;

	s32cc_pcie_disable_ltssm(s32cc_pp);

	ret = init_pcie_phy(s32cc_pp);
	if (ret)
		return ret;

	ret = init_pcie(s32cc_pp);
	if (ret)
		return ret;

	dev_info(pcie->dev, "Configuring as %s\n",
		 s32cc_pcie_ep_rc_mode_str(s32cc_pp->mode));

	return 0;
}

static int s32cc_pcie_config_host(struct s32cc_pcie *s32cc_pp)
{
	struct dw_pcie *pcie = &s32cc_pp->pcie;
	int ret = 0;

	s32cc_pcie_set_device_id(s32cc_pp);

	ret = s32cc_pcie_init_controller(s32cc_pp);
	if (ret)
		return ret;

	dw_pcie_setup_rc(pcie);

	ret = s32cc_pcie_start_link(pcie);
	if (ret) {
		dev_info(pcie->dev, "Failed to get link up\n");
		return 0;
	}

	/* Enable writing dbi registers */
	dw_pcie_dbi_ro_wr_en(&s32cc_pp->pcie);

	if (!IS_ENABLED(CONFIG_PCI_S32CC_USE_DW_CFG_IATU_SETUP))
		s32cc_pcie_rc_setup_atu(s32cc_pp);
	else
		pcie_dw_prog_outbound_atu_unroll(pcie, PCIE_ATU_REGION_INDEX0,
						 PCIE_ATU_TYPE_MEM,
						 pcie->mem.phys_start,
						 pcie->mem.bus_start,
						 pcie->mem.size);

	s32cc_pcie_clear_multifunction(s32cc_pp);
	s32cc_pcie_drop_msg_tlp(s32cc_pp);

	/* Disable writing dbi registers */
	dw_pcie_dbi_ro_wr_dis(&s32cc_pp->pcie);

	return 0;
}

struct dw_pcie_ops s32cc_dw_pcie_ops = {
	.link_up = s32cc_pcie_link_is_up,
	.start_link = s32cc_pcie_start_link,
	.write_dbi = s32cc_pcie_write,
	.write_dbi2 = s32cc_pcie_write,
};

static int s32cc_pcie_probe(struct udevice *dev)
{
	struct s32cc_pcie *s32cc_pp = dev_get_priv(dev);
	struct dw_pcie *pcie = &s32cc_pp->pcie;
	int ret = 0;

	ret = s32cc_check_serdes(dev);
	if (ret)
		return ret;

	pcie->first_busno = dev_seq(dev);
	pcie->ops = &s32cc_dw_pcie_ops;

	s32cc_pp->mode = DW_PCIE_RC_TYPE;

	ret = s32cc_pcie_config_host(s32cc_pp);
	if (ret) {
		dev_err(dev, "Failed to set PCIe host settings\n");
		s32cc_pp->mode = DW_PCIE_UNKNOWN_TYPE;
	}

	dw_pcie_dbi_ro_wr_dis(pcie);
	return ret;
}

static
void show_pcie_devices_aligned(struct udevice *bus, struct udevice *dev,
			       int depth, int last_flag, bool *parsed_bus)
{
	int i, is_last;
	struct udevice *child;
	struct pci_child_plat *pplat;

	for (i = depth; i >= 0; i--) {
		is_last = (last_flag >> i) & 1;
		if (i) {
			if (is_last)
				printf("    ");
			else
				printf("|   ");
		} else {
			if (is_last)
				printf("`-- ");
			else
				printf("|-- ");
		}
	}

	pplat = dev_get_parent_plat(dev);
	printf("%02x:%02x.%02x", dev_seq(bus),
	       PCI_DEV(pplat->devfn), PCI_FUNC(pplat->devfn));
	if (dev_seq(bus) < PCI_MAX_BUS_NUM)
		parsed_bus[dev_seq(bus)] = true;

	for (i = (PCIE_ALIGNMENT - depth + 1); i > 0; i--)
		printf("    ");
	pci_header_show_brief(dev);

	list_for_each_entry(child, &dev->child_head, sibling_node) {
		is_last = list_is_last(&child->sibling_node, &dev->child_head);
		show_pcie_devices_aligned(dev, child, depth + 1,
					  (last_flag << 1) | is_last,
					  parsed_bus);
	}
}

static int pci_get_depth(struct udevice *dev)
{
	if (!dev)
		return 0;

	return (1 + pci_get_depth(dev->parent));
}

int show_pcie_devices(void)
{
	struct udevice *bus;
	bool show_header = true;
	bool parsed_bus[PCI_MAX_BUS_NUM];

	memset(parsed_bus, false, sizeof(bool) * PCI_MAX_BUS_NUM);

	/* Show Host controllers with children */
	for (uclass_find_first_device(UCLASS_PCI, &bus);
		     bus;
		     uclass_find_next_device(&bus)) {
		struct udevice *dev;
		struct s32cc_pcie *pcie = dev_get_priv(bus);

		if (dev_seq(bus) >= PCI_MAX_BUS_NUM) {
			dev_dbg(pcie->pcie.dev, "Invalid seq number\n");
			continue;
		}
		if (parsed_bus[dev_seq(bus)])
			continue;

		if (pcie && pcie->mode != DW_PCIE_UNKNOWN_TYPE) {
			if (show_header) {
				printf(PCIE_TABLE_HEADER);
				show_header = false;
			}
			if (s32cc_pcie_link_is_up(&pcie->pcie))
				printf("%s %s (X%d, Gen%d)\n", bus->name,
				       s32cc_pcie_ep_rc_mode_str(pcie->mode),
				       s32cc_pcie_get_link_width(&pcie->pcie),
				       s32cc_pcie_get_link_speed(&pcie->pcie));
			else
				printf("%s %s\n", bus->name,
				       s32cc_pcie_ep_rc_mode_str(pcie->mode));

			for (device_find_first_child(bus, &dev);
				dev;
				device_find_next_child(&dev)) {
				int depth = pci_get_depth(dev);
				int is_last = list_is_last(&dev->sibling_node,
						&bus->child_head);
				if (dev_seq(dev) < 0 || dev_seq(bus) >= PCI_MAX_BUS_NUM) {
					dev_dbg(dev, "Invalid seq number\n");
					continue;
				}
				show_pcie_devices_aligned(bus, dev, depth - 3,
							  is_last, parsed_bus);
			}
		}
	}

	memset(parsed_bus, false, sizeof(bool) * PCI_MAX_BUS_NUM);

	/* Show End Point controllers */
	for (uclass_find_first_device(UCLASS_PCI_EP, &bus);
		     bus;
		     uclass_find_next_device(&bus)) {
		struct s32cc_pcie *pcie = dev_get_priv(bus);

		if (dev_seq(bus) >= PCI_MAX_BUS_NUM) {
			dev_dbg(pcie->pcie.dev, "Invalid seq number\n");
			continue;
		}
		if (parsed_bus[dev_seq(bus)])
			continue;

		if (pcie && pcie->mode != DW_PCIE_UNKNOWN_TYPE) {
			if (show_header) {
				printf(PCIE_TABLE_HEADER);
				show_header = false;
			}
			if (s32cc_pcie_link_is_up(&pcie->pcie))
				printf("%s %s (X%d, Gen%d)\n", bus->name,
				       s32cc_pcie_ep_rc_mode_str(pcie->mode),
				       s32cc_pcie_get_link_width(&pcie->pcie),
				       s32cc_pcie_get_link_speed(&pcie->pcie));
			else
				printf("%s %s\n", bus->name,
				       s32cc_pcie_ep_rc_mode_str(pcie->mode));
		}
	}

	return 0;
}

static const struct dm_pci_ops s32cc_dm_pcie_ops = {
	.read_config	= s32cc_pcie_read_config,
	.write_config	= s32cc_pcie_write_config,
};

static const struct udevice_id s32cc_pcie_of_match[] = {
	{ .compatible = PCIE_COMPATIBLE_RC },
	{ }
};

U_BOOT_DRIVER(pci_s32cc) = {
	.name = "pci_s32cc",
	.id = UCLASS_PCI,
	.of_match = s32cc_pcie_of_match,
	.ops = &s32cc_dm_pcie_ops,
	.of_to_plat = s32cc_pcie_dt_init_host,
	.probe	= s32cc_pcie_probe,
	.priv_auto = sizeof(struct s32cc_pcie),
	.flags = DM_FLAG_SEQ_PARENT_ALIAS,
};
