#ifndef _UTILI_H
#define _UTILI_H

#include "log.h"

#define CHECK(ret, msg, args...) if(ret != 0) \
	LOG_ERROR(msg, args) 

#endif
