#include "message_protocol_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#x); exit(1); } } while(0)

void protocol_checkpoint_test_reset(void);
void protocol_checkpoint_test_reboot(void);
void protocol_checkpoint_test_fail_program_after(int word_count);
void protocol_checkpoint_test_fail_erase(bool fail);
void protocol_checkpoint_test_fail_unlock(bool fail);
void protocol_checkpoint_test_fail_commit(bool fail);
void protocol_checkpoint_test_fail_lock(bool fail);
void protocol_checkpoint_test_corrupt_verify(bool corrupt);
void protocol_checkpoint_test_inject(const message_protocol_checkpoint_t *checkpoint,
    uint32_t generation,unsigned sector,unsigned slot,bool committed);
void protocol_checkpoint_test_corrupt_crc(unsigned sector,unsigned slot);
bool protocol_checkpoint_test_scan(message_protocol_checkpoint_t *checkpoint,uint32_t *generation);

static message_protocol_checkpoint_t restored;
static bool did_shutdown;
nostos_result_t message_protocol_service_restore(UART_HandleTypeDef *uart,
    vs1003b_status_t status,const message_protocol_checkpoint_t *checkpoint)
{
    (void)uart; (void)status;
    if(!message_protocol_checkpoint_valid(checkpoint)) return NOSTOS_BAD_VALUE;
    restored=*checkpoint;
    return NOSTOS_OK;
}
nostos_result_t message_protocol_service_checkpoint(message_protocol_checkpoint_t *checkpoint)
{ *checkpoint=restored; return NOSTOS_OK; }
void message_protocol_service_shutdown(void) { did_shutdown=true; }

static message_protocol_checkpoint_t valid_checkpoint(uint32_t sequence)
{
    message_protocol_checkpoint_t checkpoint={
        .source_id=2,.session_id=1,.next_sequence=sequence,.next_incident=1};
    for(size_t i=0;i<NOSTOS_NODE_COUNT;++i) checkpoint.windows[i]=(nostos_rx_window_t){
        .session_id=1,.approved=true};
    return checkpoint;
}

int main(void)
{
    message_protocol_checkpoint_t first=valid_checkpoint(1),second=valid_checkpoint(2),found;
    message_protocol_checkpoint_t fault=valid_checkpoint(22);
    uint32_t generation=0;
    protocol_checkpoint_test_reset();
    CHECK(!protocol_checkpoint_test_scan(&found,&generation));
    CHECK(message_protocol_service_checkpoint_commit(&first)==NOSTOS_OK);
    CHECK(message_protocol_service_checkpoint_commit(&second)==NOSTOS_OK);
    CHECK(protocol_checkpoint_test_scan(&found,&generation));
    CHECK(generation==2 && found.next_sequence==2);

    message_protocol_checkpoint_t invalid=second; invalid.source_id=0;
    CHECK(message_protocol_service_checkpoint_commit(&invalid)==NOSTOS_BAD_VALUE);
    protocol_checkpoint_test_inject(&invalid,3,6,2,true);
    protocol_checkpoint_test_inject(&second,4,6,3,false);
    CHECK(protocol_checkpoint_test_scan(&found,&generation));
    CHECK(generation==2 && found.next_sequence==2);
    protocol_checkpoint_test_corrupt_crc(6,1);
    CHECK(protocol_checkpoint_test_scan(&found,&generation));
    CHECK(generation==1 && found.next_sequence==1);

    protocol_checkpoint_test_reset();
    protocol_checkpoint_test_fail_program_after(5);
    CHECK(message_protocol_service_checkpoint_commit(&first)==NOSTOS_IO_ERROR);
    protocol_checkpoint_test_fail_program_after(-1);
    CHECK(message_protocol_service_checkpoint_commit(&second)==NOSTOS_OK);
    CHECK(protocol_checkpoint_test_scan(&found,&generation));
    CHECK(generation==1 && found.next_sequence==2);

    protocol_checkpoint_test_reset();
    protocol_checkpoint_test_fail_unlock(true);
    CHECK(message_protocol_service_checkpoint_commit(&first)==NOSTOS_IO_ERROR);
    CHECK(!protocol_checkpoint_test_scan(&found,&generation));
    protocol_checkpoint_test_fail_unlock(false);
    protocol_checkpoint_test_fail_commit(true);
    CHECK(message_protocol_service_checkpoint_commit(&first)==NOSTOS_IO_ERROR);
    CHECK(!protocol_checkpoint_test_scan(&found,&generation));
    protocol_checkpoint_test_fail_commit(false);
    CHECK(message_protocol_service_checkpoint_commit(&first)==NOSTOS_OK);
    CHECK(protocol_checkpoint_test_scan(&found,&generation) && generation==1);

    protocol_checkpoint_test_reset();
    protocol_checkpoint_test_fail_lock(true);
    CHECK(message_protocol_service_checkpoint_commit(&first)==NOSTOS_IO_ERROR);
    CHECK(protocol_checkpoint_test_scan(&found,&generation) && generation==1);
    protocol_checkpoint_test_fail_lock(false);
    CHECK(message_protocol_service_checkpoint_commit(&fault)==NOSTOS_OK);
    CHECK(protocol_checkpoint_test_scan(&found,&generation));
    CHECK(generation==2 && found.next_sequence==22);

    protocol_checkpoint_test_reset();
    protocol_checkpoint_test_corrupt_verify(true);
    CHECK(message_protocol_service_checkpoint_commit(&first)==NOSTOS_IO_ERROR);
    CHECK(!protocol_checkpoint_test_scan(&found,&generation));
    protocol_checkpoint_test_corrupt_verify(false);
    CHECK(message_protocol_service_checkpoint_commit(&fault)==NOSTOS_OK);
    CHECK(protocol_checkpoint_test_scan(&found,&generation));
    CHECK(generation==1 && found.next_sequence==22);

    protocol_checkpoint_test_reset();
    for(unsigned i=0;i<256U;++i) {
        first.next_sequence=i;
        CHECK(message_protocol_service_checkpoint_commit(&first)==NOSTOS_OK);
    }
    protocol_checkpoint_test_fail_erase(true);
    first.next_sequence=256;
    CHECK(message_protocol_service_checkpoint_commit(&first)==NOSTOS_IO_ERROR);
    CHECK(protocol_checkpoint_test_scan(&found,&generation));
    CHECK(generation==256 && found.next_sequence==255);
    protocol_checkpoint_test_fail_erase(false);
    CHECK(message_protocol_service_checkpoint_commit(&first)==NOSTOS_OK);
    CHECK(protocol_checkpoint_test_scan(&found,&generation));
    CHECK(generation==257 && found.next_sequence==256);
    protocol_checkpoint_test_reboot();
    for(unsigned i=257;i<512U;++i) {
        first.next_sequence=i;
        CHECK(message_protocol_service_checkpoint_commit(&first)==NOSTOS_OK);
    }
    first.next_sequence=512;
    CHECK(message_protocol_service_checkpoint_commit(&first)==NOSTOS_OK);
    CHECK(protocol_checkpoint_test_scan(&found,&generation));
    CHECK(generation==513 && found.next_sequence==512);

    protocol_checkpoint_test_reset();
    first=valid_checkpoint(7);
    protocol_checkpoint_test_inject(&first,1,6,0,true);
    second=valid_checkpoint(8);
    protocol_checkpoint_test_inject(&second,2,6,1,false);
    protocol_checkpoint_test_reboot();
    restored=valid_checkpoint(0); did_shutdown=false;
    UART_HandleTypeDef reboot_uart={0};
    CHECK(message_protocol_service_boot(&reboot_uart,VS1003B_STATUS_OK)==NOSTOS_OK);
    CHECK(!did_shutdown && restored.next_sequence==7);
    CHECK(protocol_checkpoint_test_scan(&found,&generation));
    CHECK(generation==2 && found.next_sequence==7);

    protocol_checkpoint_test_reset();
    first=valid_checkpoint(9);
    protocol_checkpoint_test_inject(&first,UINT32_MAX,6,0,true);
    CHECK(message_protocol_service_checkpoint_commit(&first)==NOSTOS_EXHAUSTED);
    protocol_checkpoint_test_reboot();
    restored=valid_checkpoint(0); did_shutdown=false;
    UART_HandleTypeDef exhausted_uart={0};
    CHECK(message_protocol_service_boot(&exhausted_uart,VS1003B_STATUS_OK)==NOSTOS_EXHAUSTED);
    CHECK(did_shutdown && restored.next_sequence==9);

    protocol_checkpoint_test_reset();
    restored=valid_checkpoint(0); did_shutdown=false;
    UART_HandleTypeDef uart={0};
    CHECK(message_protocol_service_boot(&uart,VS1003B_STATUS_OK)==NOSTOS_OK);
    CHECK(!did_shutdown && restored.source_id==2 && restored.session_id==1);
    puts("STM32 checkpoint CRC/commit/fallback/fault/rollover/exhaustion PASS");
    return 0;
}
