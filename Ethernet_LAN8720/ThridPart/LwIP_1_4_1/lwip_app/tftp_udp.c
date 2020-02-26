#include "tftp_udp.h"
#include "sys_arch.h"
#include "lwip/dhcp.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "includes.h"

#define TFTP_PORT	69	//简单文件传送协议端口

typedef enum{
	TFTP_RRQ	= 1,		//读请求
	TFTP_WRQ,					//写请求
	TFTP_DATA,				//数据包
	TFTP_ACK,					//确认包
	TFTP_ERROR,				//错误代码包
}tftp_opcode;
/* TFTP error codes as specified in RFC1350  */
typedef enum {
  TFTP_ERR_NOTDEFINED,
  TFTP_ERR_FILE_NOT_FOUND,
  TFTP_ERR_ACCESS_VIOLATION,
  TFTP_ERR_DISKFULL,
  TFTP_ERR_ILLEGALOP,
  TFTP_ERR_UKNOWN_TRANSFER_ID,
  TFTP_ERR_FILE_ALREADY_EXISTS,
  TFTP_ERR_NO_SUCH_USER,
} tftp_errorcode;

#define TFTP_OPCODE_LEN		2
#define TFTP_BLKNUM_LEN		2
#define TFTP_ERRCODE_LEN	2
#define TFTP_DATA_LEN_MAX	512

#define TFTP_DATA_PKT_HDR_LEN		(TFTP_OPCODE_LEN + TFTP_BLKNUM_LEN)
#define TFTP_ERR_PKT_HDR_LEN		(TFTP_OPCODE_LEN + TFTP_ERRCODE_LEN)
#define TFTP_ACK_PKT_LEN				(TFTP_OPCODE_LEN + TFTP_BLKNUM_LEN)
#define TFTP_DATA_PKT_LEN_MAX		(TFTP_DATA_PKT_HDR_LEN + TFTP_DATA_LEN_MAX)

#define TFTP_SET_OPCODE(p,code) 		do{p[0] = 0; p[1] = code;}while(0)
#define TFTP_SET_ERROR_CODE(p,err)  do{p[2] = 0; p[3] = err;}while(0)
#define TFTP_SET_BLOCK(p,block) 		do{p[2] = (u8_t)((block>>8)&0xff); p[3] = (u8_t)(block&0xff);}while(0)

#define TFTP_GET_BLOCK(p)			((u16_t)((((u8_t *)p)[2]<<8) | ((u8_t *)p)[3]))
#define TFTP_GET_OPCODE(p)		((u16_t)((((u8_t *)p)[0]<<8) | ((u8_t *)p)[1]))
typedef struct{
	int op;													//操作码
	char data[TFTP_DATA_PKT_LEN_MAX];		//最近一次的TFTP数据包
	int data_len;										//数据的长度
	u16_t remot_port;								//客户端端口
	u16_t block;										//当前块编号
	u32_t total_bytes;							//客户端已经上传或需要下载的总长度
}tftp_connection_args;

struct udp_pcb *tftp_server_pcb = NULL;
/* tftp_errorcode error strings */
char *tftp_errorcode_string[] = {
                                  "not defined",
                                  "file not found",
                                  "access violation",
                                  "disk full",
                                  "illegal operation",
                                  "unknown transfer id",
                                  "file already exists",
                                  "no such user",
                                };
err_t tftp_send_message(struct udp_pcb *upcb,
					struct ip_addr *to_ip, int to_port, char *buf, int buflen)
{

  err_t err;
  struct pbuf *pkt_buf = NULL; /* Chain of pbuf's to be sent */

  pkt_buf = pbuf_alloc(PBUF_TRANSPORT, buflen, PBUF_RAM); /* PBUF_TRANSPORT - specifies the transport layer */

  if (!pkt_buf)      /*if the packet pbuf == NULL exit and EndTransfertransmission */
    return ERR_MEM;

  memcpy(pkt_buf->payload, buf, buflen);/* Copy the original data buffer over to the packet buffer's payload */

  err = udp_sendto(upcb, pkt_buf, to_ip, to_port);/* Sending packet by UDP protocol */
	if(err != ERR_OK){
		LOG_E("udp_sendto err :%d\n",err);
		return err;
	}
  pbuf_free(pkt_buf);/* free the buffer pbuf */
  return err;
}

/*
**检查应答是否正确
*/
u32_t tftp_is_correct_ack(char *buf, int block)
{
  /* first make sure this is a data ACK packet */
  if (TFTP_GET_OPCODE(buf) != TFTP_ACK)
    return 0;

  /* then compare block numbers */
  if (block != TFTP_GET_BLOCK(buf))
    return 0;

  return 1;
}
/*
**填写一个差错包,返回包长度
*/
int tftp_construct_error_message(char *buf, tftp_errorcode err)
{
	#define ERR_MSG_OFFSET	4
  int errorlen;
  TFTP_SET_OPCODE(buf, TFTP_ERROR);
  TFTP_SET_ERROR_CODE(buf, err);

	strcpy(buf + ERR_MSG_OFFSET, tftp_errorcode_string[err]);
  errorlen = strlen(tftp_errorcode_string[err]);
  return ERR_MSG_OFFSET + errorlen + 1;
}
/*
**发送一个差错包
*/
int tftp_send_error_message(struct udp_pcb *upcb, 
					struct ip_addr *to, int to_port, tftp_errorcode err)
{
	
	char buf[520];
  int error_pkt;
  error_pkt = tftp_construct_error_message(buf, err);
  return tftp_send_message(upcb, to, to_port, buf, error_pkt);
}

/*
**发送一个数据包
*/
int tftp_send_data_packet(struct udp_pcb *upcb, struct ip_addr *to, int to_port, int block,
                          char *buf, int buflen)
{

  TFTP_SET_OPCODE(buf, TFTP_DATA);
  TFTP_SET_BLOCK(buf, block);

  return tftp_send_message(upcb, to, to_port, buf, buflen + 4);
}
/*
**发送一个应答包
*/
int tftp_send_ack_packet(struct udp_pcb *upcb, struct ip_addr *to, int to_port, int block)
{

  char packet[TFTP_ACK_PKT_LEN];

  TFTP_SET_OPCODE(packet, TFTP_ACK);
  TFTP_SET_BLOCK(packet, block);

  return tftp_send_message(upcb, to, to_port, packet, TFTP_ACK_PKT_LEN);
}

/*
**关闭读取链接
*/
void tftp_cleanup_rd(struct udp_pcb *upcb, tftp_connection_args *args)
{
  /* Free the tftp_connection_args structure reserverd for */
  mem_free(args);
  /* Disconnect the udp_pcb*/
  udp_disconnect(upcb);
  /* close the connection */
  udp_remove(upcb);

}
/*
**关闭写入链接
*/
void tftp_cleanup_wr(struct udp_pcb *upcb, tftp_connection_args *args)
{
  /* Free the tftp_connection_args structure reserverd for */
  mem_free(args);
  /* Disconnect the udp_pcb*/
  udp_disconnect(upcb);
  /* close the connection */
  udp_remove(upcb);
}

/*
**收到客户端 数据包 的回调函数
*/
void wrq_recv_callback(void *_args, struct udp_pcb *upcb, struct pbuf *pkt_buf, struct ip_addr *addr, u16_t port)
{
	
  tftp_connection_args *args = (tftp_connection_args *)_args;
  u16_t next_block = 0;
  LOG_I("wrq pkt_buf->len: %d\n",pkt_buf->len);
  if (port != args->remot_port || pkt_buf->len != pkt_buf->tot_len)
  {
    tftp_cleanup_wr(upcb, args);
    pbuf_free(pkt_buf);
		LOG_ErrTag();
		LOG_I("port: %d\n",port);
		LOG_I("args->remot_port: %d\n",args->remot_port);
		LOG_I("pkt_buf->len: %d\n",pkt_buf->len);
		LOG_I("pkt_buf->tot_len: %d\n",pkt_buf->tot_len);
    return;
  }
  next_block = args->block + 1;
  /* Does this packet have any valid data to write? */
  if ((pkt_buf->len > TFTP_DATA_PKT_HDR_LEN) &&
      (TFTP_GET_BLOCK(pkt_buf->payload) == next_block))
  {
			args->block++;
			(args->total_bytes) += (pkt_buf->len - TFTP_DATA_PKT_HDR_LEN);

    /* This is a valid pkt but it has no data.  This would occur if the file being
       written is an exact multiple of 512 bytes.  In this case, the args->block
       value must still be updated, but we can skip everything else.    */
  }
  else if (TFTP_GET_BLOCK(pkt_buf->payload) == next_block)
  {
    /* update our block number to match the block number just received  */
    args->block++;
  }
  else
  {
	  	LOG_E("Block = %d\n" ,(args->block + 1));
  }

  tftp_send_ack_packet(upcb, addr, port, args->block);//发送应答

  /* If the last write returned less than the maximum TFTP data pkt length,
   * then we've received the whole file and so we can quit (this is how TFTP
   * signals the EndTransferof a transfer!)
   */
  if (pkt_buf->len < TFTP_DATA_PKT_LEN_MAX)
  {
		LOG_D("TFTP Write end...\n");
		LOG_D("Total bttes: %ld\n",args->total_bytes);
    tftp_cleanup_wr(upcb, args);
    pbuf_free(pkt_buf);
  }
  else
  {
    pbuf_free(pkt_buf);
    return;
  }
}

/*
**处理写请求
*/
int tftp_process_write(struct udp_pcb *upcb, struct ip_addr *to, int to_port, char *FileName)
{
  tftp_connection_args *args = NULL;

  args = mem_malloc(sizeof(tftp_connection_args));
  if (!args)
  {
    tftp_send_error_message(upcb, to, to_port, TFTP_ERR_NOTDEFINED);
    tftp_cleanup_wr(upcb, args);
		LOG_ErrTag();
    return 0;
  }

  args->op = TFTP_WRQ;
  args->remot_port = to_port;
  /* the block # used as a positive response to a WRQ is _always_ 0!!! (see RFC1350)  */
  args->block = 0;
  args->total_bytes = 0;

  /* set callback for receives on this UDP PCB (Protocol Control Block) */
  udp_recv(upcb, wrq_recv_callback, args);

  tftp_send_ack_packet(upcb, to, to_port, args->block);//向客户端回复确认应答
  return 0;
}



/*
**向客户端发送一个数据 BLOCK
*/
void tftp_send_next_block(struct udp_pcb *upcb, tftp_connection_args *args,
                          struct ip_addr *to_ip, u16_t to_port)
{

  int total_block = args->total_bytes/TFTP_DATA_LEN_MAX;

  if(args->total_bytes%TFTP_DATA_LEN_MAX != 0){
			total_block += 1;
  }
	LOG_D("args->total_bytes : %ld\n", args->total_bytes);
	LOG_D("total_block : %d\n", total_block);
	LOG_D("args->block : %d\n", args->block);


  args->data_len = TFTP_DATA_LEN_MAX;

	if(args->total_bytes >= TFTP_DATA_LEN_MAX){
		args->data_len = TFTP_DATA_LEN_MAX;	
	}else{
		args->data_len = args->total_bytes;
	}

	   
  
	args->total_bytes -= args->data_len;
  LOG_D("args->data_len : %d\n", args->data_len);
	
  memset(args->data + TFTP_DATA_PKT_HDR_LEN, ('a'-1) + args->block%26 , args->data_len);
  /*   NOTE: We need to sEndTransferanother data packet even if args->data_len = 0
     The reason for this is as follows:
     1) This function is only ever called if the previous packet payload was
        512 bytes.
     2) If args->data_len = 0 then that means the file being sent is an exact
         multiple of 512 bytes.
     3) RFC1350 specifically states that only a payload of <= 511 can EndTransfera
        transfer.
     4) Therefore, we must sEndTransferanother data message of length 0 to complete
        the transfer.                */


  /* sEndTransferthe data */
  tftp_send_data_packet(upcb, to_ip, to_port, args->block, args->data, args->data_len);
  args->block++;
}
void rrq_recv_callback(void *_args, struct udp_pcb *upcb, struct pbuf *p,
                       struct ip_addr *addr, u16_t port)
{
  /* Get our connection state  */
  tftp_connection_args *args = (tftp_connection_args *)_args;
  if(port != args->remot_port)
  {
    /* Clean the connection*/
    tftp_cleanup_rd(upcb, args);
    pbuf_free(p);
		LOG_ErrTag();
		return;
  }

  if (tftp_is_correct_ack(p->payload, args->block)){
    args->block++;
  }
  else
  {
    /* we did not receive the expected ACK, so
       do not update block #. This causes the current block to be resent. */
    LOG_W(" rrq_recv_callback ACK ERR\n");
  }

  /* if the last read returned less than the requested number of bytes
   * (i.e. TFTP_DATA_LEN_MAX), then we've sent the whole file and we can quit
   */
  if (args->data_len < TFTP_DATA_LEN_MAX)
  {
    /* Clean the connection*/
    tftp_cleanup_rd(upcb, args);
    pbuf_free(p);
		LOG_D("rrq_recv_callback send over\n");
		return;
  }
  /* if the whole file has not yet been sent then continue  */
  tftp_send_next_block(upcb, args, addr, port);
  pbuf_free(p);
}
/*
**处理读请求，发送第一个数据包
*/
int tftp_process_read(struct udp_pcb *upcb, struct ip_addr *to, int to_port, char* FileName)
{
  tftp_connection_args *args = NULL;

  args = mem_malloc(sizeof(tftp_connection_args));
  if (!args)
  {
    tftp_send_error_message(upcb, to, to_port, TFTP_ERR_NOTDEFINED);
    tftp_cleanup_rd(upcb, args);
		LOG_ErrTag();
    return 0;
  }

  /* initialize connection structure  */
  args->op = TFTP_RRQ;
  args->remot_port = to_port;
  args->block = 1; /* block number starts at 1 (not 0) according to RFC1350  */
  args->total_bytes = 1024*10+10;


  /* set callback for receives on this UDP PCB (Protocol Control Block) */
  udp_recv(upcb, rrq_recv_callback, args);

  /* initiate the transaction by sending the first block of data
   * further blocks will be sent when ACKs are received
   *   - the receive callbacks need to get the proper state    */

  tftp_send_next_block(upcb, args, to, to_port);

  return 1;
}

/*
**处理TFTP客户端的请求
*/
static void process_tftp_request(struct pbuf *pkt_buf, struct ip_addr *addr, u16_t port)
{
	tftp_opcode op = (tftp_opcode)((u8_t *)pkt_buf->payload)[1];		//读取操作码
	char file_name[50] = {0};
	struct udp_pcb *upcb = NULL;
	err_t err;
	u32_t ip_addr;
	
	ip_addr = addr->addr;
  LOG_D("TFTP RRQ from: [%d.%d.%d.%d]:[%d]\n\r", (u8_t)(ip_addr), \
        (u8_t)(ip_addr >> 8),(u8_t)(ip_addr >> 16),(u8_t)(ip_addr >> 24), port);
  upcb = udp_new();
	if(upcb == NULL){
		LOG_E("udp_new err\n");
		return;
	}
	
	err = udp_connect(upcb, addr, port);
  if (err != ERR_OK)
  {  
    LOG_E("connect upcb error\n");
    return;
  }	
	
	strncpy(file_name, ((char *)(pkt_buf->payload))+2, 30); //获取文件名 maxlen=30

	switch(op){
		case TFTP_RRQ:    /* TFTP RRQ (read request)  */
			LOG_I("TFTP client start to read file..[%s]..\n\r", file_name);
      tftp_process_read(upcb, addr, port, file_name);
			break;
		case TFTP_WRQ:    /* TFTP WRQ (write request)   */
			LOG_I("TFTP client start to write file..[%s]..\n\r", file_name);
			tftp_process_write(upcb, addr, port, file_name);
			break;
		default:
      /* sEndTransfera generic access violation message */
      tftp_send_error_message(upcb, addr, port, TFTP_ERR_ACCESS_VIOLATION);
      udp_remove(upcb);			
			break;
	}
	
}

/*
**服务器 端口 TFTP_PORT	69 接收回调函数
*/
static void server_recv_cb(void *arg,struct udp_pcb *upcb, 
							struct pbuf *p, ip_addr_t *addr, u16_t port)
{

	LOG_I("FUN:%s\n",__func__);	
	LOG_I("p->len: %x\n",p->len);	
	LOG_I("p->tot_len: %x\n",p->tot_len);	
  process_tftp_request(p, addr, port);
	pbuf_free(p);		

}



void tftp_server_init(void)
{
	err_t err;
	tftp_server_pcb = udp_new();
	if(tftp_server_pcb == NULL){
		LOG_ErrTag();
		return;
	}
	err = udp_bind(tftp_server_pcb, IP_ADDR_ANY, TFTP_PORT);
	if(err != ERR_OK){
		LOG_ErrTag();
		return;
	}
	udp_recv(tftp_server_pcb, server_recv_cb, NULL);
}





