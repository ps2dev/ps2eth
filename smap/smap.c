/*
 * smap.c - SMAP device routines.
 *
 * Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
 *
 * See the file LICENSE, located within this directory, for licensing terms.
 */

#include "smap.h"

static int smap_init_event(smap_state_t *state);
static void smap_exit_event(smap_state_t *state);
static int smap_tx_event(smap_state_t *state);
static void smap_check_link(smap_state_t *state);

static int smap_txbd_check(smap_state_t *state);
static int smap_receive(smap_state_t *state);
static int smap_transmit(smap_state_t *state);

static int smap_phy_init(smap_state_t *state);
static int smap_phy_read(int reg, u16 *data);
static int smap_phy_write(int reg, u16 data);

static int smap_intr_cb(int unused)
{
	dev9IntrDisable(SMAP_INTR_BITMSK);
	iSetEventFlag(smap_state.evflg, SMAP_EVENT_INTR);
	return 0;
}

static unsigned int smap_alarm_cb(void *arg)
{
	smap_state_t *state = (smap_state_t *)arg;

	iSetEventFlag(state->evflg, SMAP_EVENT_ALARM);
	return state->timeout.lo;
}

int smap_reset(smap_state_t *state)
{
	USE_SPD_REGS;
	USE_SMAP_REGS;
	USE_SMAP_EMAC3_REGS;
	USE_SMAP_TX_BD; USE_SMAP_RX_BD;
	u16 *hwaddr = (u16 *)&state->eth_addr;
	u32 val, checksum = 0;
	int i;

	if (!(SPD_REG16(SPD_R_REV_3) & 0x01) || SPD_REG16(SPD_R_REV_1) <= 16)
		return 1;

	dev9IntrDisable(SMAP_INTR_BITMSK);

	/* Reset the transmit FIFO.  */
	SMAP_REG8(SMAP_R_TXFIFO_CTRL) = SMAP_TXFIFO_RESET;
	for (i = 9; i; i--) {
		if (!(SMAP_REG8(SMAP_R_TXFIFO_CTRL) & SMAP_TXFIFO_RESET))
			break;
		DelayThread(1000);
	}
	if (!i)
		return 2;

	/* Reset the receive FIFO.  */
	SMAP_REG8(SMAP_R_RXFIFO_CTRL) = SMAP_RXFIFO_RESET;
	for (i = 9; i; i--) {
		if (!(SMAP_REG8(SMAP_R_RXFIFO_CTRL) & SMAP_RXFIFO_RESET))
			break;
		DelayThread(1000);
	}
	if (!i)
		return 3;

	/* Perform soft reset of EMAC3.  */
	SMAP_EMAC3_SET(SMAP_R_EMAC3_MODE0, SMAP_E3_SOFT_RESET);
	for (i = 9; i; i--) {
		if (!(SMAP_EMAC3_GET(SMAP_R_EMAC3_MODE0) & SMAP_E3_SOFT_RESET))
			break;
		DelayThread(1000);
	}
	if (!i)
		return 4;

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
	memset(hwaddr, 0, 6);
	if (dev9GetEEPROM(hwaddr) < 0)
		return 5;
	if (!hwaddr[0] || !hwaddr[1] || !hwaddr[2])
		return 6;

	/* Verify the checksum.  */
	for (i = 0; i < 3; i++)
		checksum += hwaddr[i];
	if (checksum != state->checksum)
		return 7;

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

void smap_thread(void *arg)
{
	USE_SPD_REGS;
	USE_SMAP_REGS;
	USE_SMAP_EMAC3_REGS;
	smap_state_t *state = (smap_state_t *)arg;
	int res, got_rx, link_retry = 3;
	u32 bits, val;
	u16 istat;

	state->txfree = SMAP_TX_BUFSIZE;

	while (1) {
		if ((res = WaitEventFlag(state->evflg, SMAP_EVENT_ALL, 0x11, &bits)) != 0)
			break;

		if (bits & SMAP_EVENT_EXIT)
			smap_exit_event(state);

		if (bits & SMAP_EVENT_INIT) {
			smap_init_event(state);

			/* Let the interface thread know we are ready.  */
			SetEventFlag(state->if_evflg, SMAP_IF_EVENT_INCPL);

			/* Setup the alarm handler for link checking.  */
			if (!state->has_alarm_init) {
				USec2SysClock(1000000, &state->timeout);
				SetAlarm(&state->timeout, smap_alarm_cb, state);
				state->has_alarm_init = 1;
			}

			if (!state->has_init)
				continue;
		}

		/* Make sure we have the interrupts we requested.  */
		got_rx = 0;
		if ((bits & SMAP_EVENT_INTR) &&
				((istat = SPD_REG16(SPD_R_INTR_STAT)) &
				 (SMAP_INTR_EMAC3|SMAP_INTR_RXEND|SMAP_INTR_RXDNV|
				  SMAP_INTR_TXDNV))) {

			if (istat & SMAP_INTR_EMAC3) {
				val = SMAP_EMAC3_REG(SMAP_R_EMAC3_INTR_STAT);
				SMAP_REG16(SMAP_R_INTR_CLR) = SMAP_INTR_EMAC3;
				val = SMAP_E3_INTR_DEAD_0|SMAP_E3_INTR_SQE_ERR_0|
					SMAP_E3_INTR_TX_ERR_0;
				SMAP_EMAC3_SET(SMAP_R_EMAC3_INTR_STAT, val);
			}
			if (istat & SMAP_INTR_RXEND) {
				SMAP_REG16(SMAP_R_INTR_CLR) = SMAP_INTR_RXEND;
				got_rx = smap_receive(state);
			}
			if (istat & SMAP_INTR_RXDNV) {
				SMAP_REG16(SMAP_R_INTR_CLR) = SMAP_INTR_RXDNV;
			}
			if (istat & SMAP_INTR_TXDNV) {
				smap_txbd_check(state);
				bits |= SMAP_EVENT_TX;
			}
		}

		if (bits & SMAP_EVENT_TX)
			smap_tx_event(state);

		if (got_rx) {
			link_retry = 3;
			continue;
		}

		if (bits & SMAP_EVENT_ALARM) {
			if (--link_retry > 0)
				continue;

			smap_check_link(state);
		}
	}
	DPRINTF("WaitEventFlag returned %d\n", res);
}

static int smap_init_event(smap_state_t *state)
{
	USE_SMAP_EMAC3_REGS;
	int res;

	if (state->has_init)
		return 0;

	dev9IntrEnable(SMAP_INTR_EMAC3|SMAP_INTR_RXEND|SMAP_INTR_RXDNV);
	if ((res = smap_phy_init(state)) != 0)
		return res;

	SMAP_EMAC3_SET(SMAP_R_EMAC3_MODE0, SMAP_E3_TXMAC_ENABLE|SMAP_E3_RXMAC_ENABLE);
	DelayThread(10000);

	state->has_init = 1;
	return 0;
}

static void smap_exit_event(smap_state_t *state)
{
	USE_SMAP_EMAC3_REGS;

	if (!state->has_init)
		return;

	dev9IntrDisable(SMAP_INTR_EMAC3|SMAP_INTR_RXEND|SMAP_INTR_RXDNV);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_MODE0, 0);
	state->has_init = 0;
}

static int smap_tx_event(smap_state_t *state)
{
	USE_SMAP_EMAC3_REGS;
	int res;

	res = smap_transmit(state);
	smap_txbd_check(state);
	dev9IntrEnable(SMAP_INTR_EMAC3|SMAP_INTR_RXEND|SMAP_INTR_RXDNV);

	if (state->txbd_used > 0) {
		SMAP_EMAC3_SET(SMAP_R_EMAC3_TxMODE0, SMAP_E3_TX_GNP_0);
		dev9IntrEnable(SMAP_INTR_TXDNV);
	}

	return res;
}

static void smap_check_link(smap_state_t *state)
{
	u16 phydata;

	smap_phy_read(SMAP_DsPHYTER_BMSR, &phydata);
	if (!(phydata & SMAP_PHY_BMSR_LINK)) {
		state->has_link = 0;
	}
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
	u8 *payload;
	int rxbdi, res, received = 0;
	u16 stat, len, ptr, plen;

	DPRINTF("enter\n");
	while (1) {
		rxbdi = state->rxbdi & 0x3f;
		if ((stat = rx_bd[rxbdi].ctrl_stat) & SMAP_BD_RX_EMPTY)
			break;

		len = rx_bd[rxbdi].length;
		ptr = rx_bd[rxbdi].pointer;

		if (stat & SMAP_BD_RX_ERROR) {
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
			payload = q->payload;
			SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = ptr;
			if ((res = smap_dma_transfer(payload, q->len, SMAP_DMA_IN)) > 0)
				payload += res;

			while (res < q->len) {
				*(u32 *)payload = SMAP_REG32(SMAP_R_RXFIFO_DATA);
				payload += 4;
				res += 4;
			}
		}

		/* Add this to our chain */
		received++;
		smap_p_enqueue(&state->rxq, p);

next_bd:
		SMAP_REG8(SMAP_R_RXFIFO_FRAME_DEC) = 0;
		rx_bd[rxbdi].ctrl_stat = SMAP_BD_RX_EMPTY;
		state->rxbdi++;
	}

	/* All packets were received, notify the interface thread.  */
	if (received)
		SetEventFlag(state->if_evflg, SMAP_IF_EVENT_RECV);
	DPRINTF("exit, returning %d\n", received);
	return received;
}

static int smap_transmit(smap_state_t *state)
{
	USE_SMAP_REGS;
	USE_SMAP_TX_BD;
	struct pbuf *q;
	u8 *payload;
	int txbdi, res, transmitted = 0;
	u16 alen, ptr;

	DPRINTF("enter\n");
	/* Send each pbuf in the chain individually.  */
	while (1) {
		if (!(q = state->lasttxp)) {
			if (!(q = smap_p_next(&state->txq)))
				break;

			state->lasttxp = q;
		}
		if (state->txbd_used >= SMAP_BD_MAX_ENTRY)
			break;

		alen = (q->len + 3) & 0xfffc;
		if (alen > state->txfree)
			break;

		payload = q->payload;
		DPRINTF("pbuf %p, payload is %p, alen is %d, flags %04x\n",
				q, payload, alen, q->flags);

		ptr = SMAP_REG16(SMAP_R_TXFIFO_WR_PTR) + SMAP_TX_BASE;
		if ((res = smap_dma_transfer(payload, alen, SMAP_DMA_OUT)) > 0)
			payload += res;

		while (res < alen) {
			SMAP_REG32(SMAP_R_TXFIFO_DATA) = *(u32 *)payload;
			payload += 4;
			res += 4;
		}

		transmitted++;
		txbdi = state->txbdi & 0x3f;
		tx_bd[txbdi].length = q->len;
		tx_bd[txbdi].pointer = ptr;
		SMAP_REG8(SMAP_R_TXFIFO_FRAME_INC) = 0;
		tx_bd[txbdi].ctrl_stat = SMAP_BD_TX_READY| \
			SMAP_BD_TX_GENFCS|SMAP_BD_TX_GENPAD;

		state->txbdi++;
		state->txbd_used++;
		state->txfree -= alen;

		state->lasttxp = NULL;
		pbuf_free(q);
	}

	DPRINTF("exit, returning %d\n", transmitted);
	return transmitted;
}

static int smap_txbd_check(smap_state_t *state)
{
	USE_SMAP_TX_BD;
	u16 stat, len, ptr;
	int txbdsi, count = 0;

	while (state->txbd_used > 0) {
		txbdsi = state->txbdsi & 0x3f;
		if ((stat = tx_bd[txbdsi].ctrl_stat) & SMAP_BD_TX_READY)
			return count;

		len = tx_bd[txbdsi].length;
		ptr = tx_bd[txbdsi].pointer;

		if (stat & SMAP_BD_TX_ERROR) {
		}

		count++;
		state->txfree += (len + 3) & 0xfffc;
		state->txbdsi++;
		--state->txbd_used;
	}

	return count;
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

static int smap_phy_init(smap_state_t *state)
{
	USE_SMAP_EMAC3_REGS;
	u32 val;
	int i, j;
	u16 phydata, idr1, idr2;

	/* Reset the PHY.  */
	smap_phy_write(SMAP_DsPHYTER_BMCR, SMAP_PHY_BMCR_RST);
	/* Wait for it to come out of reset.  */
	for (i = 9; i; i--) {
		if (smap_phy_read(SMAP_DsPHYTER_BMCR, &phydata) != 0)
			return 1;
		if (!(phydata & SMAP_PHY_BMCR_RST))
			break;
		DelayThread(1000);
	}
	if (!i)
		return 2;

	val = SMAP_PHY_BMCR_ANEN|SMAP_PHY_BMCR_RSAN;
	smap_phy_write(SMAP_DsPHYTER_BMCR, val);

	/* Attempt to complete autonegotiation up to 3 times.  */
	for (i = 0; i < 3; i++) {
		DelayThread(3000000);
		if (smap_phy_read(SMAP_DsPHYTER_BMSR, &phydata) != 0)
			return 3;

		if (phydata & SMAP_PHY_BMSR_ANCP) {
			for (j = 0; j < 20; j++) {
				DelayThread(200000);
				if (smap_phy_read(SMAP_DsPHYTER_BMSR, &phydata) != 0)
					return 4;

				if (phydata & SMAP_PHY_BMSR_LINK) {
					state->has_link = 1;
					goto auto_done;
				}
			}
		}
		/* If autonegotiation failed, we got here, so restart it.  */
		smap_phy_write(SMAP_DsPHYTER_BMCR, val);
	}
	/* Autonegotiation failed.  */
	return 5;

auto_done:
	/* Now, read our speed and duplex mode from the PHY.  */
	if (smap_phy_read(SMAP_DsPHYTER_PHYSTS, &phydata) != 0)
		return 6;

	val = SMAP_EMAC3_GET(SMAP_R_EMAC3_MODE1);
	if (phydata & SMAP_PHY_STS_FDX)
		val |= SMAP_E3_FDX_ENABLE|SMAP_E3_FLOWCTRL_ENABLE|SMAP_E3_ALLOW_PF;
	else
		val &= ~(SMAP_E3_FDX_ENABLE|SMAP_E3_FLOWCTRL_ENABLE|SMAP_E3_ALLOW_PF);

	val &= ~SMAP_E3_MEDIA_MSK;
	if (phydata & SMAP_PHY_STS_10M) {
		val &= ~SMAP_E3_IGNORE_SQE;
		val |= SMAP_E3_MEDIA_10M;
	} else {
		val |= SMAP_E3_MEDIA_100M;
	}

	SMAP_EMAC3_SET(SMAP_R_EMAC3_MODE1, val);

	/* DSP setup.  */
	if (smap_phy_read(SMAP_DsPHYTER_PHYIDR1, &idr1) != 0)
		return 7;
	if (smap_phy_read(SMAP_DsPHYTER_PHYIDR2, &idr2) != 0)
		return 8;

	if (idr1 == SMAP_PHY_IDR1_VAL &&
			((idr2 & SMAP_PHY_IDR2_MSK) == SMAP_PHY_IDR2_VAL)) {
		if (phydata & SMAP_PHY_STS_10M)
			smap_phy_write(0x1a, 0x104);

		smap_phy_write(0x13, 0x0001);
		smap_phy_write(0x19, 0x1898);
		smap_phy_write(0x1f, 0x0000);
		smap_phy_write(0x1d, 0x5040);
		smap_phy_write(0x1e, 0x008c);
		smap_phy_write(0x13, 0x0000);
	}

	return 0;
}
