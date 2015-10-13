/*
 * Driver for Vitesse PHYs
 *
 * Author: Kriston Carson
 *
 * Copyright (c) 2005, 2009, 2011 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/delay.h>

/* Vitesse Extended Page Magic Register(s) */
#define MII_VSC82X4_EXT_PAGE_16E	0x10
#define MII_VSC82X4_EXT_PAGE_17E	0x11
#define MII_VSC82X4_EXT_PAGE_18E	0x12

/* Vitesse Extended Control Register 1 */
#define MII_VSC8244_EXT_CON1           0x17
#define MII_VSC8244_EXTCON1_INIT       0x0000
#define MII_VSC8244_EXTCON1_TX_SKEW_MASK	0x0c00
#define MII_VSC8244_EXTCON1_RX_SKEW_MASK	0x0300
#define MII_VSC8244_EXTCON1_TX_SKEW	0x0800
#define MII_VSC8244_EXTCON1_RX_SKEW	0x0200

/* Vitesse Interrupt Mask Register */
#define MII_VSC8244_IMASK		0x19
#define MII_VSC8244_IMASK_IEN		0x8000
#define MII_VSC8244_IMASK_SPEED		0x4000
#define MII_VSC8244_IMASK_LINK		0x2000
#define MII_VSC8244_IMASK_DUPLEX	0x1000
#define MII_VSC8244_IMASK_MASK		0xf000

#define MII_VSC8221_IMASK_MASK		0xa000

/* Vitesse Interrupt Status Register */
#define MII_VSC8244_ISTAT		0x1a
#define MII_VSC8244_ISTAT_STATUS	0x8000
#define MII_VSC8244_ISTAT_SPEED		0x4000
#define MII_VSC8244_ISTAT_LINK		0x2000
#define MII_VSC8244_ISTAT_DUPLEX	0x1000

/* Vitesse Auxiliary Control/Status Register */
#define MII_VSC8244_AUX_CONSTAT		0x1c
#define MII_VSC8244_AUXCONSTAT_INIT	0x0000
#define MII_VSC8244_AUXCONSTAT_DUPLEX	0x0020
#define MII_VSC8244_AUXCONSTAT_SPEED	0x0018
#define MII_VSC8244_AUXCONSTAT_GBIT	0x0010
#define MII_VSC8244_AUXCONSTAT_100	0x0008

#define MII_VSC8221_AUXCONSTAT_INIT	0x0004 /* need to set this bit? */
#define MII_VSC8221_AUXCONSTAT_RESERVED	0x0004

/* Vitesse Extended Page Access Register */
#define MII_VSC82X4_EXT_PAGE_ACCESS	0x1f
#define MII_VSC8574_EXT_MAIN  	0x0000
#define MII_VSC8574_EXT_1      	0x0001
#define MII_VSC8574_EXT_2	      0x0002
#define MII_VSC8574_EXT_3	      0x0003
#define MII_VSC8574_EXT_GENERAL 0x0010
#define MII_VSC8574_EXT_TEST		0x2A30
#define MII_VSC8574_EXT_TR			0x52B5
#define MII_VSC8574_EXT_1588		0x1588
#define MII_VSC8574_EXT_MACSEC	0x0004
#define MII_VSC8574_EXT_2DAF		0x2DAF

#define PHY_ID_VSC8234			0x000fc620
#define PHY_ID_VSC8244			0x000fc6c0
#define PHY_ID_VSC8514			0x00070670
#define PHY_ID_VSC8574			0x000704a0
#define PHY_ID_VSC8662			0x00070660
#define PHY_ID_VSC8221			0x000fc550
#define PHY_ID_VSC8211			0x000fc4b0

#define PHY_ID_VSC8574_REV_A			0x000704a0
#define PHY_ID_VSC8574_REV_B			0x000704a1
#define PHY_ID_VSC8574_MASK       0x000ffff1

MODULE_DESCRIPTION("Vitesse PHY driver");
MODULE_AUTHOR("Kriston Carson");
MODULE_LICENSE("GPL");

static int vsc824x_add_skew(struct phy_device *phydev)
{
	int err;
	int extcon;

	extcon = phy_read(phydev, MII_VSC8244_EXT_CON1);

	if (extcon < 0)
		return extcon;

	extcon &= ~(MII_VSC8244_EXTCON1_TX_SKEW_MASK |
			MII_VSC8244_EXTCON1_RX_SKEW_MASK);

	extcon |= (MII_VSC8244_EXTCON1_TX_SKEW |
			MII_VSC8244_EXTCON1_RX_SKEW);

	err = phy_write(phydev, MII_VSC8244_EXT_CON1, extcon);

	return err;
}

static int vsc8574_startup_cfg(struct phy_device *phydev)
{
	/*
		VSC8574 datasheet Ch. "Configuration" - SGMII copper
		Assume that COMA_MODE is tied to ground.
		This config seq should be placed after applying PHY_API from the vendor.
	*/

	int err = 0;
	// mdelay(120); // "Wait 120ms minimum after the release of the reset"


	/* Access General Page */
	err |= phy_write(phydev, MII_VSC82X4_EXT_PAGE_ACCESS, MII_VSC8574_EXT_GENERAL);
	/* write 19G 15:14=00, this is ignored since it is the default value */
	/* write 18G = 0x80F0, configure for all 4x PHY SGMII */
	/*
		this configuration applis for all 4x PHY SGMII while every PHY is introduced.
		would not interrupt other PHY ports since it is fixed when the first one is introduced.
	*/
	while(1){
		if(phy_read(phydev, 18) & 0x8000) mdelay(1); // wait until not busy
		else break;
		// miss timeout scheme here
		// printk(KERN_ERR "Hongbo: vsc8574_startup 18G is busy - before cfg.\n");
	}
	err |= phy_write(phydev, 18, 0x80F0);
	while(1){
		//mdelay(25); // command may take up to 25ms to complete according to the DS.
		mdelay(40); // 25ms is not enough, try more.
		if(phy_read(phydev, 18) == 0x00F0) break; // wait until not busy
		// miss timeout scheme here
		// printk(KERN_ERR "Hongbo: vsc8574_startup 18G is busy - after cfg.\n");
	}

	/* Access Main Page */
	err |= phy_write(phydev, MII_VSC82X4_EXT_PAGE_ACCESS, MII_VSC8574_EXT_MAIN);
	/* write 23M 10:8=000 12=0, this is ignored since it is the default value */
	/* write 0M, software reset PHY to apply initial SGMII Configure */
	err |= phy_write(phydev, 0, 0x9140);
	while(1){
		if(phy_read(phydev, 0) & 0x8000) mdelay(1);
		else break;
		// miss timeout scheme here
		// printk(KERN_ERR "Hongbo: vsc8574_startup 0M is busy - resetting.\n");
	}

	/* write 16E3, bit 7 =1, for SGMII autonegotiation */
	err |= phy_write(phydev, MII_VSC82X4_EXT_PAGE_ACCESS, MII_VSC8574_EXT_3);
	err |= phy_write(phydev, 16, 0x0180);

	/* Full Dumplex, Reg 0.bit8 = 1 */
	err |= phy_write(phydev, MII_VSC82X4_EXT_PAGE_ACCESS, MII_VSC8574_EXT_MAIN);
	err |= phy_write(phydev, 0, 0x1140);

	return err;
}

static int vsc8574_config_init(struct phy_device *phydev)
{
	int err = 0;

	if (phydev->interface == PHY_INTERFACE_MODE_SGMII)
		err = vsc8574_startup_cfg(phydev);
	else // this is NOT an error
		printk(KERN_ERR "Hongbo: vsc8574_config_init PHY is not in SGMII mode\n");

	return err;
}

static int vsc8574_led_mode(struct phy_device *phydev)
{
	int err = 0;

	err |= phy_write(phydev, MII_VSC82X4_EXT_PAGE_ACCESS, MII_VSC8574_EXT_MAIN);
	/* write Reg29, select LED mode value=0x800a */
	err |= phy_write(phydev, 29, 0x800a);
	/* write Reg30, select LED behavior value=0x040f */
	err |= phy_write(phydev, 30, 0x040f);

	return err;
}

static int vsc8574_reva_tune(struct phy_device *phydev)
{
	int err = 0;
	// printk(KERN_ERR "Hongbo: enter in vsc8574_reva_tune\n");

	// hongbo: tuning for Rev A VSC8574
	// TODO: 8051 patch from tesla_revA_8051_patch_9_27_2011
	/* ****** for rev A. ****** */
	err |= phy_write(phydev, MII_VSC82X4_EXT_PAGE_ACCESS, MII_VSC8574_EXT_MAIN);
	/* turn on broadcast writes */
	err |= phy_write(phydev, 22, 0x3201); //VTSS_PHY_EXTENDED_CONTROL_AND_STATUS
	/* Set 100BASE-TX edge rate to optimal setting */
	err |= phy_write(phydev, 24, 0x2040); //VTSS_PHY_EXTENDED_PHY_CONTROL_2

	err |= phy_write(phydev, MII_VSC82X4_EXT_PAGE_ACCESS, MII_VSC8574_EXT_2);
	/* Set 100BASE-TX amplitude to optimal setting after MDI-cal tweak */
	err |= phy_write(phydev, 16, 0x02f0); //VTSS_PHY_CU_PMD_TX_CTRL

	err |= phy_write(phydev, MII_VSC82X4_EXT_PAGE_ACCESS, MII_VSC8574_EXT_TEST);
	err |= phy_write(phydev, 20, 0x0140); //VTSS_PHY_TEST_PAGE_20
	err |= phy_write(phydev, 9, 0x180c); //VTSS_PHY_TEST_PAGE_9
	err |= phy_write(phydev, 8, 0x8012); //VTSS_PHY_TEST_PAGE_8

	err |= phy_write(phydev, MII_VSC82X4_EXT_PAGE_ACCESS, MII_VSC8574_EXT_TR);
	/* Write eee_TrKp*_1000 */
	err |= phy_write(phydev, 18, 0x0000);
	err |= phy_write(phydev, 17, 0x0011);
	err |= phy_write(phydev, 16, 0x96a0);

	/* Write eee_TrKf1000,ph_shift_num1000_* */
	err |= phy_write(phydev, 18, 0x0000);
	err |= phy_write(phydev, 17, 0x7100);
	err |= phy_write(phydev, 16, 0x96a2);

	/* Write SSTrK*,SS[EN]cUpdGain1000 */
	err |= phy_write(phydev, 18, 0x00d2);
	err |= phy_write(phydev, 17, 0x547f);
	err |= phy_write(phydev, 16, 0x968c);

	/* Write eee_TrKp*_100 */
	err |= phy_write(phydev, 18, 0x00f0);
	err |= phy_write(phydev, 17, 0xf00d);
	err |= phy_write(phydev, 16, 0x96b0);

	/* Write eee_TrKf100,ph_shift_num100_* */
	err |= phy_write(phydev, 18, 0x0000);
	err |= phy_write(phydev, 17, 0x7100);
	err |= phy_write(phydev, 16, 0x96b2);

	/* Write lpi_tr_tmr_val* */
	err |= phy_write(phydev, 18, 0x0000);
	err |= phy_write(phydev, 17, 0x345f);
	err |= phy_write(phydev, 16, 0x96b4);

	/* Write non/zero_det_thr*1000 */
	err |= phy_write(phydev, 18, 0x0000);
	err |= phy_write(phydev, 17, 0xf7df);
	err |= phy_write(phydev, 16, 0x8fd4);

	/* Write non/zero_det_thr*100 */
	err |= phy_write(phydev, 18, 0x0000);
	err |= phy_write(phydev, 17, 0xf3df);
	err |= phy_write(phydev, 16, 0x8fd2);

	/* Write DSPreadyTime100,LongVgaThresh100,EnabRandUpdTrig,CMAforces */
	err |= phy_write(phydev, 18, 0x000e);
	err |= phy_write(phydev, 17, 0x2b00);
	err |= phy_write(phydev, 16, 0x8fb0);

	/* Write SwitchToLD10,PwrUpBoth*,dac10_keepalive_en,ld10_pwrlvl_* */
	err |= phy_write(phydev, 18, 0x000b);
	err |= phy_write(phydev, 17, 0x05a0);
	err |= phy_write(phydev, 16, 0x8fe0);

	/* Write ld10_edge_ctrl* */
	err |= phy_write(phydev, 18, 0x0000);
	err |= phy_write(phydev, 17, 0x00ba);
	err |= phy_write(phydev, 16, 0x8fe2);

	/* Write register containing VgaGain10 */
	err |= phy_write(phydev, 18, 0x0000);
	err |= phy_write(phydev, 17, 0x4689);
	err |= phy_write(phydev, 16, 0x8f92);

	/* Improve 100BASE-TX link startup robustness to address interop issue */
	err |= phy_write(phydev, 18, 0x0060);
	err |= phy_write(phydev, 17, 0x0980);
	err |= phy_write(phydev, 16, 0x8f90);

	err |= phy_write(phydev, MII_VSC82X4_EXT_PAGE_ACCESS, MII_VSC8574_EXT_TEST);
	/* Disable token-ring after complete */
	err |= phy_write(phydev, 8, 0x0012);

	err |= phy_write(phydev, MII_VSC82X4_EXT_PAGE_ACCESS, MII_VSC8574_EXT_MAIN);
	/* Turn off broadcast writes */
	err |= phy_write(phydev, 22, 0x3200); //VTSS_PHY_EXTENDED_CONTROL_AND_STATUS

	return err;
}

static int vsc824x_config_init(struct phy_device *phydev)
{
	int err = 0;

	if (phydev->interface == PHY_INTERFACE_MODE_RGMII_ID)
		err = vsc824x_add_skew(phydev);

	return err;
}

static int vsc824x_ack_interrupt(struct phy_device *phydev)
{
	int err = 0;

	/* Don't bother to ACK the interrupts if interrupts
	 * are disabled.  The 824x cannot clear the interrupts
	 * if they are disabled.
	 */
	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_read(phydev, MII_VSC8244_ISTAT);

	return (err < 0) ? err : 0;
}

static int vsc82xx_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, MII_VSC8244_IMASK,
				(phydev->drv->phy_id == PHY_ID_VSC8234 ||
				phydev->drv->phy_id == PHY_ID_VSC8244 ||
				phydev->drv->phy_id == PHY_ID_VSC8514 ||
				phydev->drv->phy_id == PHY_ID_VSC8574) ?
				MII_VSC8244_IMASK_MASK :
				MII_VSC8221_IMASK_MASK);
	else {
		/* The Vitesse PHY cannot clear the interrupt
		 * once it has disabled them, so we clear them first
		 */
		err = phy_read(phydev, MII_VSC8244_ISTAT);

		if (err < 0)
			return err;

		err = phy_write(phydev, MII_VSC8244_IMASK, 0);
	}

	return err;
}

static int vsc8221_config_init(struct phy_device *phydev)
{
	int err;

	err = phy_write(phydev, MII_VSC8244_AUX_CONSTAT,
			MII_VSC8221_AUXCONSTAT_INIT);
	return err;

	/* Perhaps we should set EXT_CON1 based on the interface?
	 * Options are 802.3Z SerDes or SGMII
	 */
}

/* vsc82x4_config_autocross_enable - Enable auto MDI/MDI-X for forced links
 * @phydev: target phy_device struct
 *
 * Enable auto MDI/MDI-X when in 10/100 forced link speeds by writing
 * special values in the VSC8234/VSC8244 extended reserved registers
 */
static int vsc82x4_config_autocross_enable(struct phy_device *phydev)
{
	int ret;

	if ((phydev->autoneg == AUTONEG_ENABLE) || (phydev->speed > SPEED_100))
		return 0;

	/* map extended registers set 0x10 - 0x1e */
	ret = phy_write(phydev, MII_VSC82X4_EXT_PAGE_ACCESS, 0x52b5);
	if (ret >= 0)
		ret = phy_write(phydev, MII_VSC82X4_EXT_PAGE_18E, 0x0012);
	if (ret >= 0)
		ret = phy_write(phydev, MII_VSC82X4_EXT_PAGE_17E, 0x2803);
	if (ret >= 0)
		ret = phy_write(phydev, MII_VSC82X4_EXT_PAGE_16E, 0x87fa);
	/* map standard registers set 0x10 - 0x1e */
	if (ret >= 0)
		ret = phy_write(phydev, MII_VSC82X4_EXT_PAGE_ACCESS, 0x0000);
	else
		phy_write(phydev, MII_VSC82X4_EXT_PAGE_ACCESS, 0x0000);

	return ret;
}

/* vsc82x4_config_aneg - restart auto-negotiation or write BMCR
 * @phydev: target phy_device struct
 *
 * Description: If auto-negotiation is enabled, we configure the
 *   advertising, and then restart auto-negotiation.  If it is not
 *   enabled, then we write the BMCR and also start the auto
 *   MDI/MDI-X feature
 */
static int vsc82x4_config_aneg(struct phy_device *phydev)
{
	int ret;

	/* Enable auto MDI/MDI-X when in 10/100 forced link speeds by
	 * writing special values in the VSC8234 extended reserved registers
	 */
	if (phydev->autoneg != AUTONEG_ENABLE && phydev->speed <= SPEED_100) {
		ret = genphy_setup_forced(phydev);

		if (ret < 0) /* error */
			return ret;

		return vsc82x4_config_autocross_enable(phydev);
	}

	ret =  genphy_config_aneg(phydev);

	/* Restart MAC interface autonegotiation */
	/* write 16 @ EXT 3, bit 7 =1, for SGMII autonegotiation */
//	err = phy_write(phydev, MII_VSC82X4_EXT_PAGE_ACCESS,
//                MII_VSC8574_EXT_3);
//	err &= phy_write(phydev, 16,
//                0x0180);
//	err = phy_write(phydev, MII_VSC82X4_EXT_PAGE_ACCESS,
//                MII_VSC8574_EXT_MAIN);
	return ret;
}

/* Vitesse 82xx */
static struct phy_driver vsc82xx_driver[] = {
{
	.phy_id         = PHY_ID_VSC8234,
	.name           = "Vitesse VSC8234",
	.phy_id_mask    = 0x000ffff0,
	.features       = PHY_GBIT_FEATURES,
	.flags          = PHY_HAS_INTERRUPT,
	.config_init    = &vsc824x_config_init,
	.config_aneg    = &vsc82x4_config_aneg,
	.read_status    = &genphy_read_status,
	.ack_interrupt  = &vsc824x_ack_interrupt,
	.config_intr    = &vsc82xx_config_intr,
	.driver         = { .owner = THIS_MODULE,},
}, {
	.phy_id		= PHY_ID_VSC8244,
	.name		= "Vitesse VSC8244",
	.phy_id_mask	= 0x000fffc0,
	.features	= PHY_GBIT_FEATURES,
	.flags		= PHY_HAS_INTERRUPT,
	.config_init	= &vsc824x_config_init,
	.config_aneg	= &vsc82x4_config_aneg,
	.read_status	= &genphy_read_status,
	.ack_interrupt	= &vsc824x_ack_interrupt,
	.config_intr	= &vsc82xx_config_intr,
	.driver		= { .owner = THIS_MODULE,},
}, {
	.phy_id		= PHY_ID_VSC8514,
	.name		= "Vitesse VSC8514",
	.phy_id_mask	= 0x000ffff0,
	.features	= PHY_GBIT_FEATURES,
	.flags		= PHY_HAS_INTERRUPT,
	.config_init	= &vsc824x_config_init,
	.config_aneg	= &vsc82x4_config_aneg,
	.read_status	= &genphy_read_status,
	.ack_interrupt	= &vsc824x_ack_interrupt,
	.config_intr	= &vsc82xx_config_intr,
	.driver		= { .owner = THIS_MODULE,},
}, {
	.phy_id         = PHY_ID_VSC8574,
	.name           = "Vitesse VSC8574",
	.phy_id_mask    = 0x000ffff0,
	.features       = PHY_GBIT_FEATURES,
	.flags          = PHY_HAS_INTERRUPT,
	.config_init    = &vsc8574_config_init,
	.config_aneg    = &vsc82x4_config_aneg,
	.read_status    = &genphy_read_status,
	.ack_interrupt  = &vsc824x_ack_interrupt,
	.config_intr    = &vsc82xx_config_intr,
	.driver         = { .owner = THIS_MODULE,},
}, {
	.phy_id         = PHY_ID_VSC8662,
	.name           = "Vitesse VSC8662",
	.phy_id_mask    = 0x000ffff0,
	.features       = PHY_GBIT_FEATURES,
	.flags          = PHY_HAS_INTERRUPT,
	.config_init    = &vsc824x_config_init,
	.config_aneg    = &vsc82x4_config_aneg,
	.read_status    = &genphy_read_status,
	.ack_interrupt  = &vsc824x_ack_interrupt,
	.config_intr    = &vsc82xx_config_intr,
	.driver         = { .owner = THIS_MODULE,},
}, {
	/* Vitesse 8221 */
	.phy_id		= PHY_ID_VSC8221,
	.phy_id_mask	= 0x000ffff0,
	.name		= "Vitesse VSC8221",
	.features	= PHY_GBIT_FEATURES,
	.flags		= PHY_HAS_INTERRUPT,
	.config_init	= &vsc8221_config_init,
	.config_aneg	= &genphy_config_aneg,
	.read_status	= &genphy_read_status,
	.ack_interrupt	= &vsc824x_ack_interrupt,
	.config_intr	= &vsc82xx_config_intr,
	.driver		= { .owner = THIS_MODULE,},
}, {
	/* Vitesse 8211 */
	.phy_id		= PHY_ID_VSC8211,
	.phy_id_mask	= 0x000ffff0,
	.name		= "Vitesse VSC8211",
	.features	= PHY_GBIT_FEATURES,
	.flags		= PHY_HAS_INTERRUPT,
	.config_init	= &vsc8221_config_init,
	.config_aneg	= &genphy_config_aneg,
	.read_status	= &genphy_read_status,
	.ack_interrupt	= &vsc824x_ack_interrupt,
	.config_intr	= &vsc82xx_config_intr,
	.driver		= { .owner = THIS_MODULE,},
} };

static int vsc8574_reva_phy_fixup(struct phy_device *phydev)
{
	int err = 0;
	// printk(KERN_ERR "Hongbo: enter in vsc8574_reva_phy_fixup\n");
	printk(KERN_ERR "MeshSr: vsc8574 rev.a phy fixup\n");

	err |= vsc8574_led_mode(phydev);
	err |= vsc8574_reva_tune(phydev);
	return err;
}

static int vsc8574_revb_phy_fixup(struct phy_device *phydev)
{
	int err = 0;
	// printk(KERN_ERR "Hongbo: enter in vsc8574_revb_phy_fixup\n");
	printk(KERN_ERR "MeshSr: vsc8574 rev.b phy fixup\n");

	err |= vsc8574_led_mode(phydev);
	return err;
}

static int __init vsc82xx_init(void)
{
	phy_register_fixup_for_uid(PHY_ID_VSC8574_REV_A, PHY_ID_VSC8574_MASK, vsc8574_reva_phy_fixup);
	phy_register_fixup_for_uid(PHY_ID_VSC8574_REV_B, PHY_ID_VSC8574_MASK, vsc8574_revb_phy_fixup);

	return phy_drivers_register(vsc82xx_driver,
		ARRAY_SIZE(vsc82xx_driver));
}

static void __exit vsc82xx_exit(void)
{
	return phy_drivers_unregister(vsc82xx_driver,
		ARRAY_SIZE(vsc82xx_driver));
}

module_init(vsc82xx_init);
module_exit(vsc82xx_exit);

static struct mdio_device_id __maybe_unused vitesse_tbl[] = {
	{ PHY_ID_VSC8234, 0x000ffff0 },
	{ PHY_ID_VSC8244, 0x000fffc0 },
	{ PHY_ID_VSC8514, 0x000ffff0 },
	{ PHY_ID_VSC8574, 0x000ffff0 },
	{ PHY_ID_VSC8662, 0x000ffff0 },
	{ PHY_ID_VSC8221, 0x000ffff0 },
	{ PHY_ID_VSC8211, 0x000ffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, vitesse_tbl);
