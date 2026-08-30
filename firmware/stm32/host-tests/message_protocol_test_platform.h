#ifndef MESSAGE_PROTOCOL_TEST_PLATFORM_H
#define MESSAGE_PROTOCOL_TEST_PLATFORM_H

#include <stdint.h>

#ifndef __get_PRIMASK
#define __get_PRIMASK() UINT32_C(0)
#endif

#ifndef __set_PRIMASK
#define __set_PRIMASK(mask) ((void)(mask))
#endif

#endif /* MESSAGE_PROTOCOL_TEST_PLATFORM_H */
