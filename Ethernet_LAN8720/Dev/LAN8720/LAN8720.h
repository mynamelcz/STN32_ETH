#ifndef __LAN8720_H
#define __LAN8720_H
#include "stm32f4x7_eth.h"

#define ETHERNET_PHY_ADDRESS       0x00 /* Relative to STM324xG-EVAL Board */


FrameTypeDef ETH_Rx_Packet(void);
u8 ETH_Tx_Packet(u16 FrameLength);
u32 ETH_GetCurrentTxBuffer(void);
void ETH_BSP_Config(u8 *mac_addr);


void LAN8720Send(u8* data, u16 length);
#endif



