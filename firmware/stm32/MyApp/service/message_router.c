#include "message_router.h"

#include "message_service.h"
#include "uart_service.h"

static uint32_t local_message_count = 0U;
static uint32_t remote_message_count = 0U;

void message_router_init(void)
{
    local_message_count = 0U;
    remote_message_count = 0U;
}

HAL_StatusTypeDef message_router_publish_local(message_type_t message)
{
    if ((message == MSG_NONE) || (message == MSG_UNKNOWN))
    {
        return HAL_ERROR;
    }

    ++local_message_count;
#if !NOSTOS_PROTOCOL_V2
    message_service_handle_local(message);
#endif
    return uart_service_send_message(message);
}

void message_router_deliver_remote(message_type_t message)
{
    if ((message == MSG_NONE) || (message == MSG_UNKNOWN))
    {
        return;
    }

#if !NOSTOS_PROTOCOL_V2
    ++remote_message_count;
    message_service_handle(message);
#endif
}

uint32_t message_router_get_local_count(void)
{
    return local_message_count;
}

uint32_t message_router_get_remote_count(void)
{
    return remote_message_count;
}
