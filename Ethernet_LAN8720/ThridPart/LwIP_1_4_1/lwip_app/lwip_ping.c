#include "lwip_ping.h"
#include "sys_arch.h"
#include "lwip/raw.h"
#include "lwip/timers.h"
#include "lwip/opt.h"
#include "arch/cc.h"
#include "lwip/inet_chksum.h"

#include "ethernetif.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/tcp.h"
#include "lwip/tcp_impl.h"
#include "lwip/udp.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/init.h"

#define PING_DEBUG	LWIP_DBG_ON

#define PING_DELAY	2000		//ping���ͼ��
#define PING_ID			0xAFAF	//ICMP�ײ���ʶ��
#define	PING_DATA_SIZE	32	//ping�����ݳ���

static u16_t ping_seq_num;		//ICMP�ײ����
static u32_t ping_time;			//����ʱ��
static struct raw_pcb *ping_pcb = NULL;//ԭʼ���ӿ��ƿ�
static ip_addr_t ping_dst;	//ping Ŀ�ĵ�ַ

static void ping_raw_init(void);
static u8_t ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p,ip_addr_t *addr);
static void ping_timeout(void *arg);
static void ping_send(struct raw_pcb *raw, ip_addr_t *addr);
static void ping_prepare_echo(struct icmp_echo_hdr *iecho, u16_t len);

void lwip_ping_init(void)
{
	IP4_ADDR(&ping_dst,169,254,196,111);
	ping_raw_init();
}



static void ping_raw_init(void)
{
	ping_pcb = raw_new(IP_PROTO_ICMP);	//����һ��ICMP���ƿ�
	raw_recv(ping_pcb, ping_recv, NULL);//ע��ص�����
	raw_bind(ping_pcb, IP_ADDR_ANY);		//���ñ���IP��ַ
	sys_timeout(PING_DELAY, ping_timeout, ping_pcb);
}



/*
**�ں˳�ʱ�󣬻ص�ִ�иú���
*/
static void ping_timeout(void *arg)
{
	struct raw_pcb *pcb = (struct raw_pcb *)arg;
	ping_send(pcb, &ping_dst);
	sys_timeout(PING_DELAY, ping_timeout, ping_pcb);
}

/*
**����һ������Ŀ��IP��PING����
*/
static void ping_send(struct raw_pcb *raw, ip_addr_t *addr)
{
	struct pbuf *p;
	static struct icmp_echo_hdr *iecho;
	size_t ping_size = sizeof(struct icmp_echo_hdr) + PING_DATA_SIZE;
	
	p = pbuf_alloc(PBUF_IP, (u16_t)ping_size, PBUF_RAM);
	if(!p)
		return;
	
	if((p->len == p->tot_len)&&(p->next == NULL)){
		iecho = (struct icmp_echo_hdr *)p->payload;		//��ȡICMP�ײ�
		ping_prepare_echo(iecho, (u16_t)ping_size);		//��дICMP���ݰ�
		raw_sendto(raw, p, addr);
		
		ping_time = sys_now();
		LWIP_DEBUGF(PING_DEBUG, ("ping:[%"U16_F"]send", ping_seq_num));
		ip_addr_debug_print(PING_DEBUG,addr);
		LWIP_DEBUGF(PING_DEBUG,("\n"));
	}
	pbuf_free(p);
	
}
/*
**�ú������ICMP���ݰ�����д
*/
static void ping_prepare_echo(struct icmp_echo_hdr *iecho, u16_t len)
{
	size_t i;
	size_t data_len = len - sizeof(struct icmp_echo_hdr);
	
	ICMPH_TYPE_SET(iecho, ICMP_ECHO);		//����
	ICMPH_CODE_SET(iecho, 0);						//����
	iecho->chksum = 0;									//��ʼ��У���
	iecho->id = PING_ID;								//��ʶ��
	iecho->seqno = htons(++ping_seq_num);//���
	
	for(i = 0; i < data_len; i++){			//������
		((u8_t *)iecho)[sizeof(struct icmp_echo_hdr) + i] = i;
	}
	
	/*ע�͵�����pingͨ  ��֪��Ϊɶ*/
	//iecho->chksum = inet_chksum(iecho, len);			//
	//iecho->chksum = 0x5733;
	
}
/*
**ICMP���ջص�����
**	p		:	�ں˽��յ���ICMP���ݰ�
**	addr:	���ݰ�ԴIP��ַ
**	pcb	:	ԭʼЭ����ƿ�
*/
static u8_t ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p,ip_addr_t *addr)
{
	struct icmp_echo_hdr *iecho;
	
	if(p->tot_len > (PBUF_IP_HLEN + sizeof(struct icmp_echo_hdr)))
	{
		iecho = (struct icmp_echo_hdr *)((u8_t *)p->payload + PBUF_IP_HLEN);	//��ȡICMPͷ��
		if((iecho->type == ICMP_ER)&&
			 (iecho->id		== PING_ID)&&
			 (iecho->seqno == htons(ping_seq_num))){
				/*�յ���ȷ��ping��Ӧ*/
				 LWIP_DEBUGF(PING_DEBUG, ("ping:recv"));
				 ip_addr_debug_print(PING_DEBUG, addr);
				 LWIP_DEBUGF(PING_DEBUG,("time=%"U32_F"ms\n",(sys_now()-ping_time)));
					
				 pbuf_free(p);
				 return 1;
		}
	}
	return 0;
}




