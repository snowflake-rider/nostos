#include "message_protocol_service.h"
#include "main.h"
#include "alert.h"
#include "audio_service.h"
#include "buzzer.h"
#include "stop_request_audio.h"
#include "speed_down_request_audio.h"
#include "speed_up_request_audio.h"
#include "cheer_up_audio.h"
#include "uart_service.h"
#include "safety_service.h"
#include "mpu6050.h"
#include "message_router.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#x); exit(1); } } while(0)
GPIO_TypeDef test_gpio_a,test_gpio_b,test_gpio_c;
static uint32_t tick;
static bool dreq=true, spi_error=false;
static uint8_t sdi[32], uart_frame[NOSTOS_UART_FRAME_MAX];
static size_t sdi_n, uart_n;
static unsigned sdi_calls;
static uint8_t *interrupt_byte;
static bool distance_ok=true;
void ultrasonic_init(void) {}
bool ultrasonic_read(float *cm) { if(distance_ok) *cm=100.0f; return distance_ok; }
bool mpu6050_init(I2C_HandleTypeDef *i2c) { (void)i2c; return true; }
bool mpu6050_read(mpu6050_data_t *data) { *data=(mpu6050_data_t){.accel_z=1.0f}; return true; }
uint8_t mpu6050_get_address(void) { return 0x68; }
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart,uint8_t *rx,uint16_t n)
{ (void)uart; CHECK(n==1); interrupt_byte=rx; return HAL_OK; }
uint32_t HAL_GetTick(void) { return tick; }
void HAL_Delay(uint32_t ms) { tick+=ms; }
void HAL_GPIO_WritePin(GPIO_TypeDef *p,uint16_t pin,GPIO_PinState s)
{ if(s==GPIO_PIN_SET) p->output|=pin; else p->output&=(uint16_t)~pin; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p,uint16_t pin)
{ if(p==VS_DREQ_GPIO_Port && pin==VS_DREQ_Pin) return dreq?GPIO_PIN_SET:GPIO_PIN_RESET; return (p->output&pin)?GPIO_PIN_SET:GPIO_PIN_RESET; }
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi,uint8_t *tx,uint8_t *rx,uint16_t n,uint32_t timeout)
{
    (void)spi; CHECK(timeout<=10); memset(rx,0,n);
    if(spi_error) return HAL_ERROR;
    if(!(VS_XCS_GPIO_Port->output&VS_XCS_Pin)) {
        CHECK(n==4 && tx[0]==3); rx[2]=8; rx[3]=0;
    } else {
        CHECK(!(VS_XDCS_GPIO_Port->output&VS_XDCS_Pin) && dreq && n<=32);
        memcpy(sdi,tx,n); sdi_n=n; ++sdi_calls;
    }
    return HAL_OK;
}
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *spi,uint8_t *tx,uint16_t n,uint32_t timeout)
{ (void)spi;(void)tx;(void)n;(void)timeout; return spi_error?HAL_ERROR:HAL_OK; }
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *uart,uint8_t *tx,uint16_t n,uint32_t timeout)
{ (void)uart; CHECK(n<=sizeof(uart_frame) && timeout==20); memcpy(uart_frame,tx,n); uart_n=n; return HAL_OK; }
static nostos_result_t deliver(nostos_message_t m)
{
    uint8_t w[64], f[NOSTOS_UART_FRAME_MAX]; size_t n=0,fn=0;
    CHECK(nostos_message_encode(&m,w,sizeof(w),&n)==NOSTOS_OK);
    CHECK(nostos_uart_encode(w,n,f,sizeof(f),&fn)==NOSTOS_OK);
    nostos_result_t r=NOSTOS_EMPTY;
    for(size_t i=0;i<fn;++i) r=message_protocol_service_receive(f[i],tick);
    return r;
}
static nostos_message_t msg(uint8_t type,uint16_t seq)
{ nostos_message_t m={.type=type,.source_id=2,.session_id=1,.sequence=seq}; m.payload.incident=(nostos_incident_ref_t){1,1}; return m; }
int main(void)
{
    UART_HandleTypeDef uart={0}; SPI_HandleTypeDef spi={0}; uint16_t mode=0;
    CHECK(vs1003b_init(&spi,&mode)==VS1003B_STATUS_OK && mode==0x800);
    CHECK(message_protocol_service_init(&uart,3,1,VS1003B_STATUS_OK)==NOSTOS_OK);
    CHECK(uart_service_init(&uart)==HAL_OK);
    nostos_endpoint_t *e=message_protocol_service_endpoint(); CHECK(e);
    CHECK(nostos_receiver_approve_session(&e->receiver,2,1,0)==NOSTOS_OK);
    /* Actual HAL callback only enqueues. Main loop does the decode/apply. */
    uint8_t wire[64],frame[NOSTOS_UART_FRAME_MAX]; size_t wn=0,fn=0;
    nostos_message_t heartbeat=msg(NOSTOS_HEARTBEAT,30); heartbeat.payload.status=0;
    CHECK(nostos_message_encode(&heartbeat,wire,sizeof(wire),&wn)==NOSTOS_OK);
    CHECK(nostos_uart_encode(wire,wn,frame,sizeof(frame),&fn)==NOSTOS_OK);
    for(size_t i=0;i<fn;++i) { *interrupt_byte=frame[i]; HAL_UART_RxCpltCallback(&uart); }
    CHECK(!e->receiver.shared_data.nodes[1].health.report.seen);
    message_protocol_service_process(); CHECK(e->receiver.shared_data.nodes[1].health.report.seen);
    heartbeat.sequence=31;
    CHECK(nostos_message_encode(&heartbeat,wire,sizeof(wire),&wn)==NOSTOS_OK);
    CHECK(nostos_uart_encode(wire,wn,frame,sizeof(frame),&fn)==NOSTOS_OK);
    for(size_t i=0;i<600;++i) message_protocol_service_rx_isr(0x13,tick);
    for(size_t i=0;i<fn;++i) message_protocol_service_rx_isr(frame[i],tick);
    message_protocol_service_process();
    CHECK(e->receiver.shared_data.nodes[1].health.report.sequence==30 && !e->receiver.request_count);
    CHECK(message_protocol_service_stats()->overflows==1);
    for(size_t i=0;i<fn;++i) message_protocol_service_rx_isr(frame[i],tick);
    message_protocol_service_process(); CHECK(e->receiver.shared_data.nodes[1].health.report.sequence==31);
    /* Unknown v2 payload includes STOP and UART delimiter; neither is executed. */
    wire[1]=0x70; wire[9]=0x13; wire[10]=0x7e; wn=11;
    CHECK(nostos_uart_encode(wire,wn,frame,sizeof(frame),&fn)==NOSTOS_OK);
    for(size_t i=0;i<fn;++i) message_protocol_service_rx_isr(frame[i],tick);
    message_protocol_service_process(); CHECK(!e->receiver.request_count && !audio_service_is_playing());
    CHECK(deliver(msg(NOSTOS_REAR_SAFE,0))==NOSTOS_OK); message_protocol_service_process();
    CHECK(RGB_G_GPIO_Port->output&RGB_G_Pin); CHECK(!(RGB_R_GPIO_Port->output&RGB_R_Pin));
    CHECK(deliver(msg(NOSTOS_FALL,1))==NOSTOS_OK); message_protocol_service_process();
    CHECK(RGB_R_GPIO_Port->output&RGB_R_Pin); CHECK(!(RGB_G_GPIO_Port->output&RGB_G_Pin));
    CHECK(BUZZER_GPIO_Port->output&BUZZER_Pin); CHECK(sdi_calls==0); /* No invented FALL MP3. */
    CHECK(nostos_receiver_mute(&e->receiver,2,NOSTOS_FALL,(nostos_incident_ref_t){1,1})==NOSTOS_OK);
    message_protocol_service_process(); CHECK(!(BUZZER_GPIO_Port->output&BUZZER_Pin));
    CHECK(RGB_R_GPIO_Port->output&RGB_R_Pin);
    CHECK(deliver(msg(NOSTOS_FALL_CLEAR,2))==NOSTOS_OK); message_protocol_service_process();
    CHECK(alert_get_state()==ALERT_STATE_REAR_SAFE);
    const uint8_t types[]={NOSTOS_STOP,NOSTOS_SPEED_DOWN,NOSTOS_SPEED_UP,NOSTOS_SAFETY_REMINDER};
    const uint8_t *assets[]={stop_request_audio_data,speed_down_request_audio_data,speed_up_request_audio_data,cheer_up_audio_data};
    for(unsigned i=0;i<4;++i) {
        nostos_message_t m=msg(types[i],(uint16_t)(10+i));
        CHECK(deliver(m)==NOSTOS_OK); dreq=false;
        unsigned previous=sdi_calls; message_protocol_service_process(); CHECK(sdi_calls==previous);
        dreq=true; message_protocol_service_process(); CHECK(sdi_n==32 && !memcmp(sdi,assets[i],32));
        CHECK(deliver(m)==NOSTOS_DUPLICATE);
        for(unsigned budget=0;audio_service_is_playing() && budget<100000;++budget) message_protocol_service_process();
        CHECK(!audio_service_is_playing() && !e->receiver.request_count);
    }
    nostos_message_t env=msg(NOSTOS_ENVIRONMENT,0);
    env.payload.environment=(nostos_environment_t){362,603,NOSTOS_VALID,NOSTOS_VALID};
    CHECK(nostos_endpoint_publish(e,&env,tick)==NOSTOS_OK && uart_n>11);
    CHECK(e->receiver.shared_data.nodes[2].environment.humidity_pct_x10.value==605);
    CHECK(deliver(msg(NOSTOS_STOP,20))==NOSTOS_OK); dreq=false; message_protocol_service_process();
    CHECK(deliver(msg(NOSTOS_SPEED_UP,21))==NOSTOS_OK); tick+=2001; message_protocol_service_process();
    CHECK(e->expired_requests==1 && !e->receiver.request_count);
    dreq=true; spi_error=true; message_protocol_service_process(); CHECK(!audio_service_is_playing());
    /* Existing sensor producer path: no echo must not invent a SAFE report. */
    spi_error=false; I2C_HandleTypeDef i2c={0};
    safety_service_init(&i2c); message_router_init();
    for(unsigned i=0;i<10;++i) { tick+=100; safety_service_process(); }
    CHECK(e->receiver.shared_data.nodes[2].rear.state==NOSTOS_REAR_IS_SAFE);
    distance_ok=false;
    for(unsigned i=0;i<3;++i) { tick+=100; safety_service_process(); }
    CHECK(!safety_service_get_status()->distance_valid);
    CHECK(e->receiver.shared_data.nodes[2].rear.state==NOSTOS_REAR_IS_UNKNOWN);
    distance_ok=true; tick+=100; safety_service_process();
    CHECK(e->receiver.shared_data.nodes[2].rear.state==NOSTOS_REAR_IS_SAFE);
    printf("Real STM32 service + RGB/buzzer GPIO + 4 MP3 assets + VS1003B DREQ/SPI<=32B + expiry/error PASS\n");
    printf("HAL=MOCK; PHYSICAL_OUTPUT=NOT_TESTED; receiver_RAM=%zu endpoint_RAM=%zu\n",sizeof(nostos_receiver_t),sizeof(nostos_endpoint_t));
    return 0;
}
