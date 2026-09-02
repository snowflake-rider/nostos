#include "message_router.h"

#include "uart_service.h"

static uint32_t local_message_count = 0U;

void message_router_init(void)
{
    local_message_count = 0U;
}

HAL_StatusTypeDef message_router_publish_local(message_type_t message)
{
    if ((message == MSG_NONE) || (message == MSG_UNKNOWN))
    {
        return HAL_ERROR;
    }

    ++local_message_count;
    return uart_service_send_message(message);
}

uint32_t message_router_get_local_count(void)
{
    return local_message_count;
}
