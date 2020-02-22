#include "stm32f4xx.h"
#include "stm32f4x7_eth.h"

#include "includes.h"
/*bsp*/
#include "bsp_uart.h"
#include "bsp_timer.h"

/*dev*/
#include "LAN8720.h"

#include "lwip_init.h"
#include "lwip_tcp_server.h"

u8 MAC_ADDR[6] = {1,2,3,4,5,6};


int main(void)
{	
	NVIC_SetPriorityGrouping(NVIC_PriorityGroup_4);
	Debug_USART_Config();
	SysTick_Init();

	LOG_D(">>>>Power On <<<<<\n");
	
	lwip_app_init();
  tcp_echoserver_init();

	while(1)
	{

	}
#if 0	
	ETH_BSP_Config(MAC_ADDR);


	
  u8 mydata[60] = {  	
		/*Target MAC*/
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		/*Sender MAC*/
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 
		/*Type ARP*/
		0x08, 0x06, 
		/*Hard Type: Ethernet*/
		0x00, 0x01, 
		/*Protocol Type: IPv4*/
		0x08, 0x00, 
		/*Hard Size*/
		0x06, 
		/*Protocol Size*/
		0x04, 
		/*Request*/
		0x00, 0x01,
		/*Sender MAC*/
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 
		/*Sender IP*/
		0xa9, 0xfe, 0xc4, 0x7a, 
		/*Target MAC*/
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		/*Target IP*/
		0xa9, 0xfe, 0xc4, 0x6f, 
		
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};



       
	
		while(1)
		{
			Delay_ms(3000);
			LOG_D("Send_data\n");
			LAN8720Send(mydata, 60);
		}
#endif
}



/* Exported macro ------------------------------------------------------------*/
#ifdef  USE_FULL_ASSERT

/**
  * @brief  The assert_param macro is used for function's parameters check.
  * @param  expr: If expr is false, it calls assert_failed function
  *   which reports the name of the source file and the source
  *   line number of the call that failed. 
  *   If expr is true, it returns no value.
  * @retval None
  */
  #define assert_param(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))
/* Exported functions ------------------------------------------------------- */
  void assert_failed(uint8_t* file, uint32_t line);
#else
  #define assert_param(expr) ((void)0)
#endif /* USE_FULL_ASSERT */
