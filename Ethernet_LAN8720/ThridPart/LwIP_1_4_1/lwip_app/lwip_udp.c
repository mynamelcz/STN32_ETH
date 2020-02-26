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
#define UDP_ECHO_PORT		7//echo �������˿ں�

/*
** udp���յ����ݺ�Ļص�����
*/
void lwip_udp_recv_cb(void *arg,struct udp_pcb *upcb, 
							struct pbuf *p, ip_addr_t *addr, u16_t port)
{
	//udp_sendto(upcb, p, addr, port);	//ֱ�Ӱѽ��ܵ������ݷ���
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
	
	udp_bind(upcb, IP_ADDR_ANY, UDP_ECHO_PORT);//��Ϊ�������󶨵���ʶ�˿�
	udp_recv(upcb, lwip_udp_recv_cb, NULL);	//ע��ص�����
}





