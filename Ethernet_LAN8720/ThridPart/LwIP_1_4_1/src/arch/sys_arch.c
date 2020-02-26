#include "sys_arch.h"


extern u32_t lwip_localtime;

u32_t sys_now(void)
{
	return lwip_localtime;
}









