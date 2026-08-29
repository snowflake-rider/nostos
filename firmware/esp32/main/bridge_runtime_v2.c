/* Explicit alternative to bridge_runtime.c; never auto-detect raw v1 bytes. */
#include "bridge_runtime.h"
#include "nostos_bridge.h"
#include "mesh_node.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <inttypes.h>
#define TAG "NOSTOS_V2"
#define DATA_UART UART_NUM_1
static nostos_bridge_t bridge;
static nostos_uart_parser_t parser;
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t worker;
static QueueHandle_t events;
static uint32_t accepted, rejected, async_ok, async_failed;
static uint32_t now_ms(void) { return (uint32_t)((uint64_t)esp_timer_get_time()/1000U); }
static const nostos_peer_t peers[3]={
    {CONFIG_NOSTOS_SOURCE1_ADDRESS,1,1},
    {CONFIG_NOSTOS_SOURCE2_ADDRESS,2,2},
    {CONFIG_NOSTOS_SOURCE3_ADDRESS,3,3}
};
static bool ready(void)
{
    return mesh_node_ready() && mesh_node_primary()==peers[CONFIG_NOSTOS_LOCAL_SOURCE-1].mesh_address;
}
static nostos_result_t enqueue(nostos_direction_t d, const uint8_t *w,size_t n,uint16_t source)
{
    bool mesh_ready=ready(); uint32_t now=now_ms();
    portENTER_CRITICAL(&lock);
    nostos_result_t r=nostos_bridge_accept(&bridge,d,w,n,source,now,mesh_ready);
    if(r==NOSTOS_OK) ++accepted; else ++rejected;
    portEXIT_CRITICAL(&lock);
    if(r==NOSTOS_OK) xTaskNotifyGive(worker);
    return r;
}
static bool uart_send(const uint8_t *wire,size_t length)
{
    uint8_t frame[NOSTOS_UART_FRAME_MAX]; size_t n=0, sent=0;
    if(nostos_uart_encode(wire,length,frame,sizeof(frame),&n)!=NOSTOS_OK) return false;
    uint32_t start=now_ms();
    while(sent<n) {
        if((uint32_t)(now_ms()-start)>20) return false;
        int count=uart_tx_chars(DATA_UART,(const char *)frame+sent,(uint32_t)(n-sent));
        if(count<0) return false;
        sent+=(size_t)count;
        if(sent<n) vTaskDelay(1);
    }
    return uart_wait_tx_done(DATA_UART,pdMS_TO_TICKS(20))==ESP_OK;
    /* Failed/partial writes are never blindly retried. Next start flag resyncs. */
}
static nostos_result_t process_one(void)
{
    nostos_job_t job; bool mesh_ready=ready(); uint32_t now=now_ms();
    portENTER_CRITICAL(&lock);
    nostos_result_t r=nostos_bridge_next(&bridge,now,mesh_ready,&job);
    portEXIT_CRITICAL(&lock);
    if(r==NOSTOS_EMPTY) return r;
    bool sent=false;
    if(r==NOSTOS_OK) sent=job.direction==NOSTOS_TO_MESH?
        mesh_node_send_event(job.wire,job.length)==ESP_OK:uart_send(job.wire,job.length);
    ESP_LOGI(TAG,"%s type=0x%02x source=%u len=%u result=%s api=%s",
        job.direction==NOSTOS_TO_MESH?"MESH_TX":"UART_TX",job.wire[1],job.wire[2],
        (unsigned)job.length,nostos_result_name(r),sent?"accepted":"failed");
    /* STM32 persists every accepted v2 frame before applying it.  Pace only
       successful UART jobs so ordinary bursts do not outrun that boundary. */
    if(r==NOSTOS_OK && sent && job.direction==NOSTOS_TO_UART)
        vTaskDelay(pdMS_TO_TICKS(20));
    return r==NOSTOS_OK && !sent?NOSTOS_IO_ERROR:r;
}
static void worker_task(void *arg)
{
    (void)arg;
    for(;;) {
        ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
        for(;;) {
            nostos_result_t r=process_one();
            if(r==NOSTOS_EMPTY) break;
            vTaskDelay(1);
        }
    }
}
static nostos_result_t consume_uart_byte(uint8_t byte)
{
    uint8_t wire[NOSTOS_WIRE_MAX]; size_t n=0;
    nostos_result_t r=nostos_uart_feed(&parser,byte,now_ms(),wire,&n);
    if(r!=NOSTOS_OK) return r;
    nostos_result_t queued=enqueue(NOSTOS_TO_MESH,wire,n,0);
    if(n>=NOSTOS_HEADER_SIZE) {
        ESP_LOGI(TAG,"UART_RX type=0x%02x source=%u len=%u result=%s",
            wire[1],wire[2],(unsigned)n,nostos_result_name(queued));
    } else {
        ESP_LOGW(TAG,"UART_RX malformed len=%u result=%s",(unsigned)n,nostos_result_name(queued));
    }
    return queued;
}
static void uart_task(void *arg)
{
    (void)arg; uart_event_t event;
    for(;;) {
        if(xQueueReceive(events,&event,portMAX_DELAY)!=pdTRUE) continue;
        if(event.type==UART_DATA) {
            for(size_t i=0;i<event.size;++i) {
                uint8_t byte;
                if(uart_read_bytes(DATA_UART,&byte,1,0)!=1) break;
                nostos_result_t r=consume_uart_byte(byte);
                if(r!=NOSTOS_OK && r!=NOSTOS_EMPTY) ESP_LOGW(TAG,"UART_DROP %s",nostos_result_name(r));
            }
        } else if(event.type==UART_FIFO_OVF || event.type==UART_BUFFER_FULL ||
            event.type==UART_PARITY_ERR || event.type==UART_FRAME_ERR) {
            nostos_uart_reset(&parser); uart_flush_input(DATA_UART); xQueueReset(events);
            ESP_LOGW(TAG,"UART_HW_ERROR frame discarded");
        }
        vTaskDelay(1);
    }
}
void bridge_runtime_mesh_rx(const uint8_t *wire,size_t length,uint16_t source,uint16_t own)
{
    if(source==own || !worker || own!=peers[CONFIG_NOSTOS_LOCAL_SOURCE-1].mesh_address) return;
    nostos_result_t r=enqueue(NOSTOS_TO_UART,wire,length,source);
    if(length>=NOSTOS_HEADER_SIZE) {
        ESP_LOGI(TAG,"MESH_RX type=0x%02x source=%u address=0x%04x len=%u result=%s",
            wire[1],wire[2],source,(unsigned)length,nostos_result_name(r));
    } else {
        ESP_LOGW(TAG,"MESH_RX malformed address=0x%04x len=%u result=%s",
            source,(unsigned)length,nostos_result_name(r));
    }
}
void bridge_runtime_mesh_complete(int error)
{
    portENTER_CRITICAL(&lock);
    if(error) ++async_failed; else ++async_ok;
    portEXIT_CRITICAL(&lock);
}
void bridge_runtime_log_status(void)
{
    portENTER_CRITICAL(&lock);
    uint32_t a=accepted,r=rejected,ok=async_ok,bad=async_failed; size_t pending=bridge.count;
    portEXIT_CRITICAL(&lock);
    ESP_LOGI(TAG,"STATUS version=2 source=%d pending=%u accepted=%" PRIu32 " rejected=%" PRIu32
        " async_ok=%" PRIu32 " async_failed=%" PRIu32 "; peer_APPLY_ACK=not_automatic",
        CONFIG_NOSTOS_LOCAL_SOURCE,(unsigned)pending,a,r,ok,bad);
}
esp_err_t bridge_runtime_init(void)
{
    if(nostos_bridge_init(&bridge,CONFIG_NOSTOS_LOCAL_SOURCE,peers)!=NOSTOS_OK) {
        ESP_LOGE(TAG,"Configure distinct verified source1/2/3 Mesh addresses before v2 startup");
        return ESP_ERR_INVALID_ARG;
    }
    const uart_config_t cfg={.baud_rate=115200,.data_bits=UART_DATA_8_BITS,.parity=UART_PARITY_DISABLE,
        .stop_bits=UART_STOP_BITS_1,.flow_ctrl=UART_HW_FLOWCTRL_DISABLE,.source_clk=UART_SCLK_DEFAULT};
    esp_err_t err=uart_param_config(DATA_UART,&cfg); if(err!=ESP_OK) return err;
    err=uart_set_pin(DATA_UART,17,18,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE); if(err!=ESP_OK) return err;
    err=uart_driver_install(DATA_UART,512,0,16,&events,0); if(err!=ESP_OK) return err;
    if(xTaskCreate(worker_task,"nostos_v2_tx",4096,NULL,5,&worker)!=pdPASS) {
        uart_driver_delete(DATA_UART); return ESP_ERR_NO_MEM;
    }
    if(xTaskCreate(uart_task,"nostos_v2_rx",4096,NULL,4,NULL)!=pdPASS) {
        vTaskDelete(worker); worker=NULL; uart_driver_delete(DATA_UART); return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG,"UART1_READY version=2 TX=17 RX=18 115200/8N1; no legacy auto-detection");
    return ESP_OK;
}
