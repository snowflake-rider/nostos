/* Include the actual runtime to exercise its task steps without a fake RTOS
 * scheduler. Public Mesh callbacks and driver calls remain the real C paths. */
#include "../../firmware/esp32/main/bridge_runtime_v2.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mock_messages.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#x); exit(1); } } while(0)
static uint32_t clock_ms;
static bool mesh_up=true, fifo_blocked=false, send_failure=false;
static uint8_t mesh_bytes[64],uart_bytes[512];
static size_t mesh_length,uart_length;
static unsigned sends,notifications;
static uint16_t own_address=0x202;
int64_t esp_timer_get_time(void) { return (int64_t)clock_ms*1000; }
bool mesh_node_ready(void) { return mesh_up; }
uint16_t mesh_node_primary(void) { return own_address; }
esp_err_t mesh_node_send_event(const uint8_t *w,size_t n)
{ CHECK(n<=64); memcpy(mesh_bytes,w,n); mesh_length=n; ++sends; return send_failure?ESP_FAIL:ESP_OK; }
int xTaskCreate(void (*run)(void *),const char *name,uint32_t stack,void *arg,unsigned priority,TaskHandle_t *handle)
{ (void)run;(void)name;(void)arg;(void)priority; CHECK(stack>=4096); if(handle) *handle=(void *)1; return pdPASS; }
void vTaskDelete(TaskHandle_t t) { (void)t; }
void vTaskDelay(TickType_t t) { clock_ms+=t; }
uint32_t ulTaskNotifyTake(int clear,TickType_t wait) { (void)clear;(void)wait; return 1; }
void xTaskNotifyGive(TaskHandle_t t) { CHECK(t); ++notifications; }
int xQueueReceive(QueueHandle_t q,void *e,TickType_t w) { (void)q;(void)e;(void)w; return 0; }
int xQueueReset(QueueHandle_t q) { (void)q; return 1; }
esp_err_t uart_param_config(int port,const uart_config_t *cfg) { CHECK(port==1 && cfg->baud_rate==115200); return ESP_OK; }
esp_err_t uart_set_pin(int p,int tx,int rx,int rts,int cts) { CHECK(p==1 && tx==17 && rx==18 && rts==-1 && cts==-1); return ESP_OK; }
esp_err_t uart_driver_install(int p,int rx,int tx,int ec,QueueHandle_t *q,int flags)
{ CHECK(p==1 && rx==512 && tx==0 && ec==16 && flags==0); *q=(void *)1; return ESP_OK; }
esp_err_t uart_driver_delete(int p) { (void)p; return ESP_OK; }
int uart_tx_chars(int p,const char *w,uint32_t n)
{
    CHECK(p==1); if(fifo_blocked) return 0;
    size_t take=n>7?7:n; CHECK(uart_length+take<=sizeof(uart_bytes));
    memcpy(uart_bytes+uart_length,w,take); uart_length+=take; return (int)take;
}
esp_err_t uart_wait_tx_done(int p,TickType_t wait) { CHECK(p==1 && wait==20); return ESP_OK; }
int uart_read_bytes(int p,void *w,uint32_t n,TickType_t t) { (void)p;(void)w;(void)n;(void)t; return 0; }
esp_err_t uart_flush_input(int p) { (void)p; return ESP_OK; }
static void all_message_types(void)
{
    CHECK(sizeof(fixtures)/sizeof(fixtures[0])==NOSTOS_TYPE_COUNT);
    for(size_t f=0;f<sizeof(fixtures)/sizeof(fixtures[0]);++f) {
        const fixture_t *fixture=&fixtures[f];
        uint8_t frame[NOSTOS_UART_FRAME_MAX], remote[NOSTOS_WIRE_MAX], decoded[NOSTOS_WIRE_MAX];
        size_t frame_length=0, decoded_length=0;
        unsigned before_sends=sends, before_notify=notifications;
        CHECK(nostos_uart_encode(fixture->wire,fixture->length,frame,sizeof(frame),&frame_length)==NOSTOS_OK);
        for(size_t i=0;i<frame_length;++i)
            CHECK(consume_uart_byte(frame[i])==(i+1==frame_length?NOSTOS_OK:NOSTOS_EMPTY));
        CHECK(notifications==before_notify+1 && process_one()==NOSTOS_OK && sends==before_sends+1);
        CHECK(mesh_length==fixture->length && !memcmp(mesh_bytes,fixture->wire,fixture->length));
        CHECK(process_one()==NOSTOS_EMPTY);
        /* This ESP32 is source2; incoming fixtures originate at source1. */
        memcpy(remote,fixture->wire,fixture->length); remote[2]=1;
        uart_length=0;
        bridge_runtime_mesh_rx(remote,fixture->length,0x101,own_address);
        memset(remote,0,sizeof(remote)); /* Callback must own its queued bytes. */
        CHECK(process_one()==NOSTOS_OK && sends==before_sends+1);
        nostos_uart_parser_t parser={0};
        for(size_t i=0;i<uart_length;++i)
            CHECK(nostos_uart_feed(&parser,uart_bytes[i],clock_ms,decoded,&decoded_length)==
                  (i+1==uart_length?NOSTOS_OK:NOSTOS_EMPTY));
        memcpy(remote,fixture->wire,fixture->length); remote[2]=1;
        CHECK(decoded_length==fixture->length && !memcmp(decoded,remote,decoded_length));
        CHECK(process_one()==NOSTOS_EMPTY); /* No application mesh re-broadcast. */
        printf("ESP32_RUNTIME %-16s UART->Mesh / Mesh->UART owned bytes PASS\n",fixture->name);
    }
}
int main(void)
{
    CHECK(bridge_runtime_init()==ESP_OK);
    all_message_types();
    sends=0; notifications=0; uart_length=0; mesh_length=0;
    nostos_message_t m={.type=NOSTOS_ENVIRONMENT,.source_id=2,.session_id=1,.sequence=7};
    m.payload.environment=(nostos_environment_t){362,603,NOSTOS_VALID,NOSTOS_VALID};
    uint8_t w[64],frame[NOSTOS_UART_FRAME_MAX]; size_t n=0,f=0;
    CHECK(nostos_message_encode(&m,w,sizeof(w),&n)==NOSTOS_OK);
    CHECK(nostos_uart_encode(w,n,frame,sizeof(frame),&f)==NOSTOS_OK);
    for(size_t i=0;i<f;++i) {
        nostos_result_t r=consume_uart_byte(frame[i]); CHECK(r==(i+1==f?NOSTOS_OK:NOSTOS_EMPTY));
    }
    CHECK(notifications==1 && process_one()==NOSTOS_OK && sends==1);
    CHECK(mesh_length==n && !memcmp(mesh_bytes,w,n));
    CHECK(process_one()==NOSTOS_EMPTY);
    /* Receiving from source1 preserves its header and never calls mesh send. */
    w[2]=1; bridge_runtime_mesh_rx(w,n,0x101,own_address); memset(w,0,sizeof(w));
    CHECK(process_one()==NOSTOS_OK && sends==1);
    nostos_uart_parser_t p={0}; size_t decoded=0; nostos_result_t r=NOSTOS_EMPTY;
    for(size_t i=0;i<uart_length;++i) r=nostos_uart_feed(&p,uart_bytes[i],clock_ms,w,&decoded);
    CHECK(r==NOSTOS_OK && decoded==n && w[2]==1 && w[9]==137 && w[10]==121);
    bridge_runtime_mesh_rx(w,n,0x303,own_address); CHECK(process_one()==NOSTOS_EMPTY);
    fifo_blocked=true; bridge_runtime_mesh_rx(w,n,0x101,own_address);
    uint32_t start=clock_ms; CHECK(process_one()==NOSTOS_IO_ERROR && clock_ms-start<=21);
    CHECK(process_one()==NOSTOS_EMPTY); /* No retry after partial write/timeout. */
    fifo_blocked=false; mesh_up=false; w[2]=2;
    CHECK(enqueue(NOSTOS_TO_MESH,w,n,0)==NOSTOS_NOT_READY);
    mesh_up=true; own_address=0x303; CHECK(enqueue(NOSTOS_TO_MESH,w,n,0)==NOSTOS_NOT_READY);
    own_address=0x202; send_failure=true;
    CHECK(enqueue(NOSTOS_TO_MESH,w,n,0)==NOSTOS_OK && process_one()==NOSTOS_IO_ERROR);
    CHECK(process_one()==NOSTOS_EMPTY);
    bridge_runtime_mesh_complete(0); bridge_runtime_mesh_complete(-1); bridge_runtime_log_status();
    puts("Actual ESP32 v2 UART/worker/Mesh-callback C paths PASS; ESP-IDF/RTOS/radio APIs=MOCK");
    return 0;
}
