/*
 * smap.c - SMAP device routines.
 *
 * Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
 *
 * See the file LICENSE, located within this directory, for licensing terms.
 */

#include "smap.h"

static int init = 0;

int smap_txbd_check(smap_state_t *state);
static int smap_receive(smap_state_t *state);
static int smap_transmit(smap_state_t *state);

static int smap_phy_init();
static int smap_phy_read(int reg, u16 *data);
static int smap_phy_write(int reg, u16 data);

static int smap_intr_cb(int unused)
{
	dev9IntrDisable(SMAP_INTR_BITMSK);
	iSetEventFlag(smap_evflg, SMAP_EVENT_INTR);
	return 0;
}

int smap_reset(smap_state_t *state)
{
	u16 hwaddr[4];
	USE_SPD_REGS;
	USE_SMAP_REGS;
	USE_SMAP_EMAC3_REGS;
	USE_SMAP_TX_BD; USE_SMAP_RX_BD;
	u32 val, checksum = 0;
	int i;

	if (!(SPD_REG16(SPD_R_REV_3) & 0x01) || SPD_REG16(SPD_R_REV_1) <= 16)
		return -1;

	dev9IntrDisable(SMAP_INTR_BITMSK);

	/* Reset the transmit FIFO.  */
	SMAP_REG8(SMAP_R_TXFIFO_CTRL) = SMAP_TXFIFO_RESET;
	for (i = 9; i; i--) {
		if (!(SMAP_REG8(SMAP_R_TXFIFO_CTRL) & SMAP_TXFIFO_RESET))
			break;
		DelayThread(1000);
	}
	if (!i)
		return -1;

	/* Reset the receive FIFO.  */
	SMAP_REG8(SMAP_R_RXFIFO_CTRL) = SMAP_RXFIFO_RESET;
	for (i = 9; i; i--) {
		if (!(SMAP_REG8(SMAP_R_RXFIFO_CTRL) & SMAP_RXFIFO_RESET))
			break;
		DelayThread(1000);
	}
	if (!i)
		return -1;

	/* Perform soft reset of EMAC3.  */
	SMAP_EMAC3_SET(SMAP_R_EMAC3_MODE0, SMAP_E3_SOFT_RESET);
	for (i = 9; i; i--) {
		if (!(SMAP_EMAC3_GET(SMAP_R_EMAC3_MODE0) & SMAP_E3_SOFT_RESET))
			break;
		DelayThread(1000);
	}
	if (!i)
		return -1;

	SMAP_REG8(SMAP_R_BD_MODE) = 0;

	/* Initialize all RX and TX buffer descriptors.  */
	for (i = 0; i < SMAP_BD_MAX_ENTRY; i++, tx_bd++) {
			tx_bd->ctrl_stat = 0;
			tx_bd->reserved  = 0;
			tx_bd->length    = 0;
			tx_bd->pointer   = 0;
	}
	for (i = 0; i < SMAP_BD_MAX_ENTRY; i++, rx_bd++) { 
			rx_bd->ctrl_stat = SMAP_BD_RX_EMPTY;
			rx_bd->reserved  = 0;
			rx_bd->length    = 0;
			rx_bd->pointer   = 0;
	}

	SMAP_REG16(SMAP_R_INTR_CLR) = SMAP_INTR_BITMSK;

	/* Get our MAC address and make sure it checks out.  */
	if (dev9GetEEPROM(hwaddr) < 0)
		return -1;
	if (!hwaddr[0] || !hwaddr[1] || !hwaddr[2])
		return -1;

	/* Verify the checksum.  */
	for (i = 0; i < 3; i++)
		checksum += hwaddr[i];
	if (checksum != hwaddr[3])
		return -1;

	val = SMAP_E3_FDX_ENABLE|SMAP_E3_IGNORE_SQE|SMAP_E3_MEDIA_100M|
		SMAP_E3_RXFIFO_2K|SMAP_E3_TXFIFO_1K|SMAP_E3_TXREQ0_MULTI;
	SMAP_EMAC3_SET(SMAP_R_EMAC3_MODE1, val);

	val = (7 << SMAP_E3_TX_LOW_REQ_BITSFT) | (15 << SMAP_E3_TX_URG_REQ_BITSFT);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_TxMODE1, val);

	val = SMAP_E3_RX_STRIP_PAD|SMAP_E3_RX_STRIP_FCS|SMAP_E3_RX_INDIVID_ADDR|
		SMAP_E3_RX_BCAST|SMAP_E3_RX_MCAST;
	SMAP_EMAC3_SET(SMAP_R_EMAC3_RxMODE, val);

	val = SMAP_E3_INTR_DEAD_0|SMAP_E3_INTR_SQE_ERR_0|SMAP_E3_INTR_TX_ERR_0;
	SMAP_EMAC3_SET(SMAP_R_EMAC3_INTR_STAT, val);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_INTR_ENABLE, val);

	val = (u16)((hwaddr[0] >> 8)|(hwaddr[0] << 8));
	SMAP_EMAC3_SET(SMAP_R_EMAC3_ADDR_HI, val);
	val = (((hwaddr[1] >> 8)|(hwaddr[1] << 8)) << 16)|(u16)((hwaddr[2] >> 8)|
			(hwaddr[2] << 8));
	SMAP_EMAC3_SET(SMAP_R_EMAC3_ADDR_LO, val);

	SMAP_EMAC3_SET(SMAP_R_EMAC3_PAUSE_TIMER, 0xffff);

	SMAP_EMAC3_SET(SMAP_R_EMAC3_GROUP_HASH1, 0);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_GROUP_HASH2, 0);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_GROUP_HASH3, 0);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_GROUP_HASH4, 0);

	SMAP_EMAC3_SET(SMAP_R_EMAC3_INTER_FRAME_GAP, 4);

	val = 12 << SMAP_E3_TX_THRESHLD_BITSFT;
	SMAP_EMAC3_SET(SMAP_R_EMAC3_TX_THRESHOLD, val);

	val = (16 << SMAP_E3_RX_LO_WATER_BITSFT) | (128 << SMAP_E3_RX_HI_WATER_BITSFT);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_RX_WATERMARK, val);

	/* Register the callbacks for the interrupts we're handling.  */
	for (i = 2; i < 7; i++)
		dev9RegisterIntrCb(i, smap_intr_cb);

	return 0;
}

int smap_init_event(smap_state_t *state)
{
	USE_SMAP_EMAC3_REGS;

	if (init)
		return 0;

	dev9IntrEnable(SMAP_INTR_EMAC3|SMAP_INTR_RXEND|SMAP_INTR_RXDNV);
	if (smap_phy_init() < 0)
		return 1;

	SMAP_EMAC3_SET(SMAP_R_EMAC3_MODE0, SMAP_E3_TXMAC_ENABLE|SMAP_E3_RXMAC_ENABLE);
	DelayThread(10000);

	init = 1;
	return 0;
}

void smap_exit_event(smap_state_t *state)
{
	USE_SMAP_EMAC3_REGS;

	if (!init)
		return;

	dev9IntrDisable(SMAP_INTR_EMAC3|SMAP_INTR_RXEND|SMAP_INTR_RXDNV);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_MODE0, 0);
	init = 0;
}

int smap_interrupt_event(smap_state_t *state)
{
	USE_SPD_REGS;
	USE_SMAP_REGS;
	USE_SMAP_EMAC3_REGS;
	u32 val;
	u16 istat = SPD_REG16(SPD_R_INTR_STAT);

	if (!(istat & (SMAP_INTR_EMAC3|SMAP_INTR_RXEND|SMAP_INTR_RXDNV|SMAP_INTR_TXDNV)))
		return 0;

	if (istat & SMAP_INTR_EMAC3) {
		val = SMAP_EMAC3_REG(SMAP_R_EMAC3_INTR_STAT);
		SMAP_REG16(SMAP_R_INTR_CLR) = SMAP_INTR_EMAC3;
		val = SMAP_E3_INTR_DEAD_0|SMAP_E3_INTR_SQE_ERR_0|SMAP_E3_INTR_TX_ERR_0;
		SMAP_EMAC3_SET(SMAP_R_EMAC3_INTR_STAT, val);
	}

	if (istat & SMAP_INTR_RXEND) {
		SMAP_REG16(SMAP_R_INTR_CLR) = SMAP_INTR_RXEND;
		smap_receive(state);
	}

	if (istat & SMAP_INTR_RXDNV) {
		SMAP_REG16(SMAP_R_INTR_CLR) = SMAP_INTR_RXDNV;
	}

	if (istat & SMAP_INTR_TXDNV) {
		smap_txbd_check(state);
		return 1;
	}

	return 0;
}

void smap_tx_event(smap_state_t *state)
{
	USE_SMAP_EMAC3_REGS;

	smap_transmit(state);
	smap_txbd_check(state);
	dev9IntrEnable(SMAP_INTR_EMAC3|SMAP_INTR_RXEND|SMAP_INTR_RXDNV);

	if (state->txbd_used > 0) {
		SMAP_EMAC3_SET(SMAP_R_EMAC3_TxMODE0, SMAP_E3_TX_GNP_0);
		dev9IntrEnable(SMAP_INTR_TXDNV);
	}
}

int smap_txbd_check(smap_state_t *state)
{
	USE_SMAP_TX_BD;
	u16 stat, len, ptr;
	int count = 0;

	while (state->txbd_used > 0) {
		if ((stat = tx_bd[state->txbdi].ctrl_stat) & SMAP_BD_TX_READY)
			return count;

		len = tx_bd[state->txbdi].length;
		ptr = tx_bd[state->txbdi].pointer;

		if (stat & SMAP_BD_TX_ERROR) {
		}

		count++;
		state->txbp += (len + 3) & 0xfffc;
		state->txbdi++;
		--state->txbd_used;
	}

	return count;
}

static u32 smap_dma_transfer(void *buf, u32 size, int dir)
{
	USE_SMAP_REGS;
	u32 dma_size = size / 128;

	if (!dma_size)
		return 0;

	if (dir == SMAP_DMA_OUT) {
		SMAP_REG16(SMAP_R_TXFIFO_SIZE) = dma_size;
		SMAP_REG8(SMAP_R_TXFIFO_CTRL) = SMAP_TXFIFO_DMAEN;
	} else {
		SMAP_REG16(SMAP_R_RXFIFO_SIZE) = dma_size;
		SMAP_REG8(SMAP_R_RXFIFO_CTRL) = SMAP_RXFIFO_DMAEN;
	}

	if (dev9DmaTransfer(1, buf, (dma_size << 16)|0x20, dir) < 0) {
		if (dir == SMAP_DMA_OUT)
			SMAP_REG8(SMAP_R_TXFIFO_CTRL) = 0;
		else
			SMAP_REG8(SMAP_R_RXFIFO_CTRL) = 0;
		return 0;
	}

	/* Wait for DMA to complete.  */
	if (dir == SMAP_DMA_OUT) {
		while (SMAP_REG8(SMAP_R_TXFIFO_CTRL) & SMAP_TXFIFO_DMAEN)
			;
		return dma_size * 128;
	}

	while (SMAP_REG8(SMAP_R_RXFIFO_CTRL) & SMAP_RXFIFO_DMAEN)
		;
	return dma_size * 128;
}

/* Read in all data from the RX buffers and turn them over to the high-level
   interface.  */
static int smap_receive(smap_state_t *state)
{
	USE_SMAP_REGS;
	USE_SMAP_RX_BD;
	struct pbuf *p, *q;
	int res, received = 0;
	u16 stat, len, ptr, plen;

	while (1) {
		if ((stat = rx_bd[state->rxbdi].ctrl_stat) & SMAP_BD_RX_EMPTY)
			break;

		len = rx_bd[state->rxbdi].length;
		ptr = rx_bd[state->rxbdi].pointer;

		if (stat & SMAP_BD_TX_ERROR) {

			goto error;
		}

		plen = (len + 3) & 0xfffc;
		if (!(p = pbuf_alloc(PBUF_LINK, plen, PBUF_POOL))) {
error:
			SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = ptr + ((len + 3) & 0xfffc);
			goto next_bd;
		}

		/* Now we are ready to receive the data from the RX buffer.
		   First we DMA the data in, and anything left over we copy.  */
		for (q = p; q; q = q->next) {
			SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = ptr;
			if ((res = smap_dma_transfer(q->payload, q->len, SMAP_DMA_IN)) > 0)
				q->payload += res;

			while (res < q->len) {
				*(u32 *)q->payload = SMAP_REG32(SMAP_R_RXFIFO_DATA);
				q->payload += 4;
				res += 4;
			}
			/* Increment the BD pointer.  */
			ptr += res;
		}

		/* Pass the data to the LwIP API.  */
		received = 1;
		/* smapif_input(state, p); */

next_bd:
		SMAP_REG8(SMAP_R_RXFIFO_FRAME_DEC) = 0;
		rx_bd[state->rxbdi].ctrl_stat = SMAP_BD_RX_EMPTY;
		state->rxbdi++;
	}

	return received;
}

static int smap_transmit(smap_state_t *state)
{
	struct pbuf *p = state->tx_pbuf, *q;
	int transmitted = 0;

	while (1) {
		if (!p)
			goto done;

		if (state->txbd_used >= 64)
			goto done;
	}

done:
	return transmitted;
}

static int smap_phy_read(int reg, u16 *data)
{
	USE_SMAP_EMAC3_REGS;
	u32 i, val;

	val = SMAP_E3_PHY_READ|(SMAP_DsPHYTER_ADDRESS << SMAP_E3_PHY_ADDR_BITSFT)|
		(reg & SMAP_E3_PHY_REG_ADDR_MSK);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_STA_CTRL, val);

	/* Wait for the read operation to complete.  */
	for (i = 0; i < 100; i++) {
		if (SMAP_EMAC3_GET(SMAP_R_EMAC3_STA_CTRL) & SMAP_E3_PHY_OP_COMP)
			break;
		DelayThread(1000);
	}
	if (i == 100 || !data)
		return 1;

	*data = SMAP_EMAC3_GET(SMAP_R_EMAC3_STA_CTRL) >> SMAP_E3_PHY_DATA_BITSFT;
	return 0;
}

static int smap_phy_write(int reg, u16 data)
{
	USE_SMAP_EMAC3_REGS;
	u32 i, val;

	val = ((data & SMAP_E3_PHY_DATA_MSK) << SMAP_E3_PHY_DATA_BITSFT)|
		SMAP_E3_PHY_WRITE|(SMAP_DsPHYTER_ADDRESS << SMAP_E3_PHY_ADDR_BITSFT)|
		(reg & SMAP_E3_PHY_REG_ADDR_MSK);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_STA_CTRL, val);

	/* Wait for the write operation to complete.  */
	for (i = 0; i < 100; i++) {
		if (SMAP_EMAC3_GET(SMAP_R_EMAC3_STA_CTRL) & SMAP_E3_PHY_OP_COMP)
			break;
		DelayThread(1000);
	}
	return (i == 100);
}

static int smap_phy_init()
{
	u32 val;
	int i;
	u16 phydata;

	/* Reset the PHY.  */
	smap_phy_write(SMAP_DsPHYTER_BMCR, SMAP_PHY_BMCR_RST);
	/* Wait for it to come out of reset.  */
	for (i = 9; i; i--) {
		if (smap_phy_read(SMAP_DsPHYTER_BMCR, &phydata) != 0)
			return 4;
		if (!(phydata & SMAP_PHY_BMCR_RST))
			break;
		DelayThread(1000);
	}
	if (!i)
		return -1;

	val = SMAP_PHY_BMCR_LPBK|SMAP_PHY_BMCR_100M|SMAP_PHY_BMCR_DUPM;
	smap_phy_write(SMAP_DsPHYTER_BMCR, val);

	return 0;
}
