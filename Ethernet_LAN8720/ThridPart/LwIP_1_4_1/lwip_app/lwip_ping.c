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

#define PING_DELAY	2000		//ping发送间隔
#define PING_ID			0xAFAF	//ICMP首部标识符
#define	PING_DATA_SIZE	32	//ping包数据长度

static u16_t ping_seq_num;		//ICMP首部序号
static u32_t ping_time;			//往返时间
static struct raw_pcb *ping_pcb = NULL;//原始链接控制块
static ip_addr_t ping_dst;	//ping 目的地址

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
	ping_pcb = raw_new(IP_PROTO_ICMP);	//申请一个ICMP控制块
	raw_recv(ping_pcb, ping_recv, NULL);//注册回调函数
	raw_bind(ping_pcb, IP_ADDR_ANY);		//设置本地IP地址
	sys_timeout(PING_DELAY, ping_timeout, ping_pcb);
}



/*
**内核超时后，回调执行该函数
*/
static void ping_timeout(void *arg)
{
	struct raw_pcb *pcb = (struct raw_pcb *)arg;
	ping_send(pcb, &ping_dst);
	sys_timeout(PING_DELAY, ping_timeout, ping_pcb);
}

/*
**构造一个发想目的IP的PING请求
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
		iecho = (struct icmp_echo_hdr *)p->payload;		//获取ICMP首部
		ping_prepare_echo(iecho, (u16_t)ping_size);		//填写ICMP数据包
		raw_sendto(raw, p, addr);
		
		ping_time = sys_now();
		LWIP_DEBUGF(PING_DEBUG, ("ping:[%"U16_F"]send", ping_seq_num));
		ip_addr_debug_print(PING_DEBUG,addr);
		LWIP_DEBUGF(PING_DEBUG,("\n"));
	}
	pbuf_free(p);
	
}
/*
**该函数完成ICMP数据包的填写
*/
static void ping_prepare_echo(struct icmp_echo_hdr *iecho, u16_t len)
{
	size_t i;
	size_t data_len = len - sizeof(struct icmp_echo_hdr);
	
	ICMPH_TYPE_SET(iecho, ICMP_ECHO);		//类型
	ICMPH_CODE_SET(iecho, 0);						//代码
	iecho->chksum = 0;									//初始化校验和
	iecho->id = PING_ID;								//标识符
	iecho->seqno = htons(++ping_seq_num);//序号
	
	for(i = 0; i < data_len; i++){			//数据区
		((u8_t *)iecho)[sizeof(struct icmp_echo_hdr) + i] = i;
	}
	
	/*注释掉可以ping通  不知道为啥*/
	//iecho->chksum = inet_chksum(iecho, len);			//
	//iecho->chksum = 0x5733;
	
}
/*
**ICMP接收回调函数
**	p		:	内核接收到的ICMP数据包
**	addr:	数据包源IP地址
**	pcb	:	原始协议控制块
*/
static u8_t ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p,ip_addr_t *addr)
{
	struct icmp_echo_hdr *iecho;
	
	if(p->tot_len > (PBUF_IP_HLEN + sizeof(struct icmp_echo_hdr)))
	{
		iecho = (struct icmp_echo_hdr *)((u8_t *)p->payload + PBUF_IP_HLEN);	//获取ICMP头部
		if((iecho->type == ICMP_ER)&&
			 (iecho->id		== PING_ID)&&
			 (iecho->seqno == htons(ping_seq_num))){
				/*收到正确的ping响应*/
				 LWIP_DEBUGF(PING_DEBUG, ("ping:recv"));
				 ip_addr_debug_print(PING_DEBUG, addr);
				 LWIP_DEBUGF(PING_DEBUG,("time=%"U32_F"ms\n",(sys_now()-ping_time)));
					
				 pbuf_free(p);
				 return 1;
		}
	}
	return 0;
}




