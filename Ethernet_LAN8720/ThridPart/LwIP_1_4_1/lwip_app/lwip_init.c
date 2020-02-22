#include "lwip_init.h"
#include "LAN8720.h"
#include <stdio.h>

#include "ethernetif.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/tcp.h"
#include "lwip/tcp_impl.h"
#include "lwip/udp.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/init.h"

struct netif lan8720_neitif;

void lwip_app_init(void)
{
	static ip_addr_t ipaddr,netmask,gw;
	lwip_init();				//ÄÚºË³õÊ¼»¯
	
	IP4_ADDR(&gw,169,254,196,1);
	IP4_ADDR(&ipaddr,169,254,196,122);	
	IP4_ADDR(&netmask,255,255,255,0);	
	
	netif_add(&lan8720_neitif, 
						&ipaddr,
						&netmask,
						&gw,
						NULL,
						ethernetif_init,
						ethernet_input);
	
	netif_set_default(&lan8720_neitif);
	
	netif_set_up(&lan8720_neitif);

	
	
}
void input_process_hdl(void)
{
	ethernetif_input(&lan8720_neitif);
}
void ETH_IRQHandler(void)
{
    while(ETH_CheckFrameReceived()){ 
        input_process_hdl();
    }
    ETH_DMAClearITPendingBit(ETH_DMA_IT_R);
    ETH_DMAClearITPendingBit(ETH_DMA_IT_NIS);
}

