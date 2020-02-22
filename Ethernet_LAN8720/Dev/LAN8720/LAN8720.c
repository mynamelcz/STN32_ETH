/* Includes ------------------------------------------------------------------*/
#include "LAN8720.h"
#include "includes.h"
#include "ETH_cfg.h"
/*LwIP*/
#include "pbuf.h"


ETH_InitTypeDef ETH_InitStructure;




extern __align(4) ETH_DMADESCTypeDef  DMARxDscrTab[ETH_RXBUFNB];/* Ethernet Rx MA Descriptor */
extern __align(4) ETH_DMADESCTypeDef  DMATxDscrTab[ETH_TXBUFNB];/* Ethernet Tx DMA Descriptor */
extern __align(4) uint8_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE]; /* Ethernet Receive Buffer */
extern __align(4) uint8_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE]; /* Ethernet Transmit Buffer */
extern __IO ETH_DMADESCTypeDef  *DMATxDescToSet;
extern __IO ETH_DMADESCTypeDef  *DMARxDescToGet;
extern ETH_DMA_Rx_Frame_infos RX_Frame_Descriptor;
extern __IO ETH_DMA_Rx_Frame_infos *DMA_RX_FRAME_infos;

extern void Delay_ms(__IO u32 nTime);
/* Private function prototypes -----------------------------------------------*/
static void ETH_GPIO_Config(void);
static void ETH_MACDMA_Config(void);
static void ETH_NVIC_Config(void);
/* Private functions ---------------------------------------------------------*/
/* Bit 2 from Basic Status Register in PHY */
#define GET_PHY_LINK_STATUS()		(ETH_ReadPHYRegister(ETHERNET_PHY_ADDRESS, PHY_BSR) & 0x00000004)





static void ETH_50HZ_CLK_Out (void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOA, &GPIO_InitStructure); 
	RCC_MCO1Config(RCC_MCO1Source_HSE, RCC_MCO1Div_1);
}


void ETH_BSP_Config(u8 *mac_addr)
{
	int i;
  ETH_GPIO_Config();
	GPIO_SetBits(GPIOC,GPIO_Pin_0);
	Delay_ms(10);
	GPIO_ResetBits(GPIOC,GPIO_Pin_0);
	Delay_ms(10);
	GPIO_SetBits(GPIOC,GPIO_Pin_0);
	
	ETH_NVIC_Config();
  ETH_MACDMA_Config();
  if(GET_PHY_LINK_STATUS())
  {
		LOG_D("Link ok\n");
  }	

  ETH_MACAddressConfig(ETH_MAC_Address0, mac_addr);
  ETH_DMATxDescChainInit(DMATxDscrTab, &Tx_Buff[0][0], ETH_TXBUFNB);
  ETH_DMARxDescChainInit(DMARxDscrTab, &Rx_Buff[0][0], ETH_RXBUFNB);
	
#ifdef CHECKSUM_BY_HARDWARE
  for(i = 0; i < ETH_TXBUFNB; i++) {
      ETH_DMATxDescChecksumInsertionConfig(&DMATxDscrTab[i], ETH_DMATxDesc_ChecksumTCPUDPICMPFull);
  }
#endif
  ETH_Start();

}


static void ETH_MACDMA_Config(void)
{
  uint32_t  EthStatus = 0;
  /* Enable ETHERNET clock  */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_ETH_MAC | RCC_AHB1Periph_ETH_MAC_Tx |
                        RCC_AHB1Periph_ETH_MAC_Rx, ENABLE);
                        
  /* Reset ETHERNET on AHB Bus */
  ETH_DeInit();

  /* Software reset */
  ETH_SoftwareReset();

  /* Wait for software reset */
  while (ETH_GetSoftwareResetStatus() == SET);

  /* ETHERNET Configuration --------------------------------------------------*/
  /* Call ETH_StructInit if you don't like to configure all ETH_InitStructure parameter */
  ETH_StructInit(&ETH_InitStructure);

  /* Fill ETH_InitStructure parametrs */
  /*------------------------   MAC   -----------------------------------*/
	/* 开启网络自适应功能 */
  ETH_InitStructure.ETH_AutoNegotiation = ETH_AutoNegotiation_Enable;
//  ETH_InitStructure.ETH_AutoNegotiation = ETH_AutoNegotiation_Disable; 
//  ETH_InitStructure.ETH_Speed = ETH_Speed_10M;
//  ETH_InitStructure.ETH_Mode = ETH_Mode_FullDuplex;   
  /* 关闭反馈 */
  ETH_InitStructure.ETH_LoopbackMode = ETH_LoopbackMode_Disable;
	/* 关闭重传功能 */
  ETH_InitStructure.ETH_RetryTransmission = ETH_RetryTransmission_Disable;
	/* 关闭自动去除PDA/CRC功能  */
  ETH_InitStructure.ETH_AutomaticPadCRCStrip = ETH_AutomaticPadCRCStrip_Disable;
	/* 关闭接收所有的帧 */
  ETH_InitStructure.ETH_ReceiveAll = ETH_ReceiveAll_Disable;
	/* 允许接收所有广播帧 */
  ETH_InitStructure.ETH_BroadcastFramesReception = ETH_BroadcastFramesReception_Enable;
	/* 关闭混合模式的地址过滤  */
  ETH_InitStructure.ETH_PromiscuousMode = ETH_PromiscuousMode_Disable;
	/* 对于组播地址使用完美地址过滤    */
  ETH_InitStructure.ETH_MulticastFramesFilter = ETH_MulticastFramesFilter_Perfect;
	/* 对单播地址使用完美地址过滤  */
  ETH_InitStructure.ETH_UnicastFramesFilter = ETH_UnicastFramesFilter_Perfect;
#ifdef CHECKSUM_BY_HARDWARE
	/* 开启ipv4和TCP/UDP/ICMP的帧校验和卸载   */
  ETH_InitStructure.ETH_ChecksumOffload = ETH_ChecksumOffload_Enable;
#endif

  /*------------------------   DMA   -----------------------------------*/  

  /* When we use the Checksum offload feature, we need to enable the Store and Forward mode:
  the store and forward guarantee that a whole frame is stored in the FIFO, so the MAC can insert/verify the checksum, 
  if the checksum is OK the DMA can handle the frame otherwise the frame is dropped */
	/*当我们使用帧校验和卸载功能的时候，一定要使能存储转发模式,存储转发模式中要保证整个帧存储在FIFO中,
	这样MAC能插入/识别出帧校验值,当真校验正确的时候DMA就可以处理帧,否则就丢弃掉该帧*/
	
	/* 开启丢弃TCP/IP错误帧 */
  ETH_InitStructure.ETH_DropTCPIPChecksumErrorFrame = ETH_DropTCPIPChecksumErrorFrame_Enable;
	/* 开启接收数据的存储转发模式  */
  ETH_InitStructure.ETH_ReceiveStoreForward = ETH_ReceiveStoreForward_Enable;
	/* 开启发送数据的存储转发模式   */
  ETH_InitStructure.ETH_TransmitStoreForward = ETH_TransmitStoreForward_Enable;

	/* 禁止转发错误帧 */
  ETH_InitStructure.ETH_ForwardErrorFrames = ETH_ForwardErrorFrames_Disable;
	/* 不转发过小的好帧 */
  ETH_InitStructure.ETH_ForwardUndersizedGoodFrames = ETH_ForwardUndersizedGoodFrames_Disable;
	/* 打开处理第二帧功能 */
  ETH_InitStructure.ETH_SecondFrameOperate = ETH_SecondFrameOperate_Enable;
	/* 开启DMA传输的地址对齐功能 */
  ETH_InitStructure.ETH_AddressAlignedBeats = ETH_AddressAlignedBeats_Enable;
	/* 开启固定突发功能 */
  ETH_InitStructure.ETH_FixedBurst = ETH_FixedBurst_Enable;
	/* DMA发送的最大突发长度为32个节拍 */
  ETH_InitStructure.ETH_RxDMABurstLength = ETH_RxDMABurstLength_32Beat;
	/*DMA接收的最大突发长度为32个节拍 */
  ETH_InitStructure.ETH_TxDMABurstLength = ETH_TxDMABurstLength_32Beat;
  ETH_InitStructure.ETH_DMAArbitration = ETH_DMAArbitration_RoundRobin_RxTx_2_1;

  /* Configure Ethernet */
	/* 配置ETH */
  EthStatus = ETH_Init(&ETH_InitStructure, ETHERNET_PHY_ADDRESS);
	if(EthStatus == ETH_SUCCESS){
		ETH_DMAITConfig(ETH_DMA_IT_NIS|ETH_DMA_IT_R,ENABLE);  
	}else{
		LOG_D("ETH_Init Err\n");
	}
}



static void ETH_GPIO_Config(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;

  RCC_AHB1PeriphClockCmd(	RCC_AHB1Periph_GPIOA|
													RCC_AHB1Periph_GPIOB|
													RCC_AHB1Periph_GPIOC, 
													ENABLE);

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	ETH_50HZ_CLK_Out();

  SYSCFG_ETH_MediaInterfaceConfig(SYSCFG_ETH_MediaInterface_RMII);




/* Ethernet pins configuration ************************************************/
/*
	ETH_RST --------------------------> PC0
	
	
	
	
	ETH_MII_RX_CLK/ETH_RMII_REF_CLK---> PA1	
	ETH_MDIO -------------------------> PA2
	ETH_MII_RX_DV/ETH_RMII_CRS_DV ----> PA7

	ETH_MDC --------------------------> PC1
	ETH_MII_RXD0/ETH_RMII_RXD0 -------> PC4
	ETH_MII_RXD1/ETH_RMII_RXD1 -------> PC5
	
	ETH_MII_TX_EN/ETH_RMII_TX_EN -----> PB11
	ETH_MII_TXD0/ETH_RMII_TXD0 -------> PB12
	ETH_MII_TXD1/ETH_RMII_TXD1 -------> PB13
																						*/

  /* Configure ETH_NRST */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_Init(GPIOC, &GPIO_InitStructure);


	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_7;	
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_ETH);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_ETH);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_ETH);
	
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1|GPIO_Pin_4|GPIO_Pin_5;	
  GPIO_Init(GPIOC, &GPIO_InitStructure);
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource1, GPIO_AF_ETH);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource4, GPIO_AF_ETH);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource5, GPIO_AF_ETH);	


  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11|GPIO_Pin_12|GPIO_Pin_13;	
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_ETH);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource12, GPIO_AF_ETH);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_ETH);	
	
	
		
}
static void ETH_NVIC_Config(void) 
{
    NVIC_InitTypeDef   NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = ETH_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}


/*
//接收一个网卡数据包
//返回值:网络数据包帧结构体
*/
FrameTypeDef ETH_Rx_Packet(void)
{ 
	u32 framelength=0;
	FrameTypeDef frame={0,0};   
	//检查当前描述符,是否属于ETHERNET DMA(设置的时候)/CPU(复位的时候)
	if((DMARxDescToGet->Status&ETH_DMARxDesc_OWN)!=(u32)RESET)
	{	
		frame.length=ETH_ERROR; 
		if ((ETH->DMASR&ETH_DMASR_RBUS)!=(u32)RESET)  
		{ 
			ETH->DMASR = ETH_DMASR_RBUS;//清除ETH DMA的RBUS位 
			ETH->DMARPDR=0;//恢复DMA接收
		}
		return frame;//错误,OWN位被设置了
	}  
	if(((DMARxDescToGet->Status&ETH_DMARxDesc_ES)==(u32)RESET)&& 
	((DMARxDescToGet->Status & ETH_DMARxDesc_LS)!=(u32)RESET)&&  
	((DMARxDescToGet->Status & ETH_DMARxDesc_FS)!=(u32)RESET))  
	{       
		framelength=((DMARxDescToGet->Status&ETH_DMARxDesc_FL)>>ETH_DMARxDesc_FrameLengthShift)-4;//得到接收包帧长度(不包含4字节CRC)
 		frame.buffer = DMARxDescToGet->Buffer1Addr;//得到包数据所在的位置
	}else framelength=ETH_ERROR;//错误  
	frame.length=framelength; 
	frame.descriptor=DMARxDescToGet;  
	//更新ETH DMA全局Rx描述符为下一个Rx描述符
	//为下一次buffer读取设置下一个DMA Rx描述符
	DMARxDescToGet=(ETH_DMADESCTypeDef*)(DMARxDescToGet->Buffer2NextDescAddr);   
	return frame;  
}
/*
//发送一个网卡数据包
//FrameLength:数据包长度
//返回值:ETH_ERROR,发送失败(0)
//		ETH_SUCCESS,发送成功(1)
*/
u8 ETH_Tx_Packet(u16 FrameLength)
{   
	//检查当前描述符,是否属于ETHERNET DMA(设置的时候)/CPU(复位的时候)
	if((DMATxDescToSet->Status&ETH_DMATxDesc_OWN)!=(u32)RESET)return ETH_ERROR;//错误,OWN位被设置了 
 	DMATxDescToSet->ControlBufferSize=(FrameLength&ETH_DMATxDesc_TBS1);//设置帧长度,bits[12:0]
	DMATxDescToSet->Status|=ETH_DMATxDesc_LS|ETH_DMATxDesc_FS;//设置最后一个和第一个位段置位(1个描述符传输一帧)
  	DMATxDescToSet->Status|=ETH_DMATxDesc_OWN;//设置Tx描述符的OWN位,buffer重归ETH DMA
	if((ETH->DMASR&ETH_DMASR_TBUS)!=(u32)RESET)//当Tx Buffer不可用位(TBUS)被设置的时候,重置它.恢复传输
	{ 
		ETH->DMASR=ETH_DMASR_TBUS;//重置ETH DMA TBUS位 
		ETH->DMATPDR=0;//恢复DMA发送
	} 
	//更新ETH DMA全局Tx描述符为下一个Tx描述符
	//为下一次buffer发送设置下一个DMA Tx描述符 
	DMATxDescToSet=(ETH_DMADESCTypeDef*)(DMATxDescToSet->Buffer2NextDescAddr);    
	return ETH_SUCCESS;   
}

/*
//得到当前描述符的Tx buffer地址
//返回值:Tx buffer地址
*/
u32 ETH_GetCurrentTxBuffer(void)
{  
  return DMATxDescToSet->Buffer1Addr;//返回Tx buffer地址  
}







void LAN8720Send(u8* data, u16 length)
{
    memcpy((u8 *)DMATxDescToSet->Buffer1Addr, data, length);
    ETH_Prepare_Transmit_Descriptors(length);
}
void Pkt_Handle(void) 
{
		u32 i,receiveLen = 0;
		u8 *receiveBuffer = NULL;
    FrameTypeDef frame;
		__IO ETH_DMADESCTypeDef *DMARxNextDesc;
	
    frame = ETH_Get_Received_Frame();
	
    receiveLen = frame.length;
    receiveBuffer = (u8 *)frame.buffer;

    LOG_D("%d\n", receiveLen);   

		if(receiveLen == 60){
			for (i = 0; i < receiveLen; i++) {
				LOG_D("%02x ", receiveBuffer[i]);
			}	
			LOG_D("\n");			
		}
    


		/* Check if frame with multiple DMA buffer segments */
		if (DMA_RX_FRAME_infos->Seg_Count > 1) {
				DMARxNextDesc = DMA_RX_FRAME_infos->FS_Rx_Desc;
		} else {
				DMARxNextDesc = frame.descriptor;
		}

		/* Set Own bit in Rx descriptors: gives the buffers back to DMA */
		for (i = 0; i < DMA_RX_FRAME_infos->Seg_Count; i++) {
				DMARxNextDesc->Status = ETH_DMARxDesc_OWN;
				DMARxNextDesc = (ETH_DMADESCTypeDef *)(DMARxNextDesc->Buffer2NextDescAddr);
		}
		/* Clear Segment_Count */
		DMA_RX_FRAME_infos->Seg_Count = 0;
		/* When Rx Buffer unavailable flag is set: clear it and resume reception */
		if ((ETH->DMASR & ETH_DMASR_RBUS) != (u32)RESET) {
				/* Clear RBUS ETHERNET DMA flag */
				ETH->DMASR = ETH_DMASR_RBUS;
				/* Resume DMA reception */
				ETH->DMARPDR = 0;
		}	
}





/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
