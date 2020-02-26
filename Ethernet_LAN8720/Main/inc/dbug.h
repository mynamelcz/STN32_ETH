#ifndef __DEBUG_H
#define __DEBUG_H
#include <stdio.h>

#define __ASSERT_PARAM
#define __DBG_ENABLE

#ifdef __DBG_ENABLE
#define DBUG_Printf		printf
#define DBUG_Put_hex	
#else
#define DBUG_Printf(...)
#define DBUG_Put_hex(...)	
#endif

#define DBG_LOG		0
#define DBG_INF		1
#define DBG_WAR		2
#define DBG_ERR		3
#define DBG_NOT		4

#define DBG_LEVEL		DBG_LOG
#ifdef __DBG_ENABLE
	
	#if (DBG_LOG >= DBG_LEVEL)
		#define LOG_D(...)		DBUG_Printf("[log]:"__VA_ARGS__)
	#else
		#define LOG_D(...)
	#endif

	#if (DBG_INF >= DBG_LEVEL)
		#define LOG_I(...)		DBUG_Printf("[inf]:"__VA_ARGS__)
	#else
		#define LOG_I(...)
	#endif
	
	#if (DBG_WAR >= DBG_LEVEL)
		#define LOG_W(...)		DBUG_Printf("[war]:"__VA_ARGS__)
	#else
		#define LOG_W(...)
	#endif

	#if (DBG_ERR >= DBG_LEVEL)
	#define LOG_E(...)		DBUG_Printf("[err addr]:\n\t__file:%s\n\t__line:%d\n\t__fun:%s\n[err inf]:\n\t",__FILE__,__LINE__,__func__); \
												DBUG_Printf(__VA_ARGS__)
												
	#define LOG_ErrTag()	DBUG_Printf("[err addr]:\n\t__file:%s\n\t__line:%d\n\t__fun:%s\n[err inf]:\n\t",__FILE__,__LINE__,__func__) 										
	#else
		#define LOG_E(...)
		#define LOG_ErrTag()
	#endif
	
#else
	#define LOG_D(...)
	#define LOG_I(...)
	#define LOG_W(...)
	#define LOG_E(...)
	#define LOG_ErrTag()
#endif


#ifdef __ASSERT_PARAM
	extern void my_assrt_filed(unsigned char *file, unsigned int line);
	#define ASSERT(arg)	\
	do{			\
		if(!arg)	my_assrt_filed((u8 *)__FILE__, __LENE__);\
	}while(0);
#else
	#define ASSERT(arg) 
#endif



#endif








