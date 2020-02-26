#include "lwip_udp.h"
#include "ethernetif.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/tcp.h"
#include "lwip/tcp_impl.h"
#include "lwip/udp.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/init.h"
#include "includes.h"
#include <string.h>
#define UDP_ECHO_PORT		7//echo 服务器端口号

/*
** udp接收到数据后的回调函数
*/
void lwip_udp_recv_cb(void *arg,struct udp_pcb *upcb, 
							struct pbuf *p, ip_addr_t *addr, u16_t port)
{
	//udp_sendto(upcb, p, addr, port);	//直接把接受到的数据返回
	//pbuf_free(p);
	const char *reply = "This is reply!!!\n";
	struct pbuf *q = NULL;
	
	printf("recv len: %d\n",p->tot_len);
	pbuf_free(p);

	q = pbuf_alloc(PBUF_TRANSPORT, strlen(reply), PBUF_RAM);
	
	if(!q){
		LOG_ErrTag();
		return;
	}
	
	memset(q->payload, 0, q->len);
	memcpy(q->payload, reply, strlen(reply));
	
	udp_sendto(upcb, q, addr, port);
	pbuf_free(q);	
	
}


void lwip_udp_init(void)
{
	struct udp_pcb *upcb;
	upcb = udp_new();
	
	udp_bind(upcb, IP_ADDR_ANY, UDP_ECHO_PORT);//作为服务器绑定到熟识端口
	udp_recv(upcb, lwip_udp_recv_cb, NULL);	//注册回调函数
}





