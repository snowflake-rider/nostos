#include "message_protocol_service.h"
#include "app_config.h"
#include "stm32f4xx_hal.h"
#include <stddef.h>
#include <string.h>

#if NOSTOS_PROTOCOL_V2

#define CHECKPOINT_MAGIC UINT32_C(0x4E563243)
#define CHECKPOINT_COMMIT UINT32_C(0xA53CC35A)
#define CHECKPOINT_VERSION 1U
#define CHECKPOINT_SLOT_SIZE 512U
#define CHECKPOINT_SECTOR_SIZE (128U*1024U)
#define CHECKPOINT_SECTOR6_ADDRESS UINT32_C(0x08040000)
#define CHECKPOINT_SECTOR7_ADDRESS UINT32_C(0x08060000)
#define CHECKPOINT_SLOTS (CHECKPOINT_SECTOR_SIZE/CHECKPOINT_SLOT_SIZE)

#if defined(NOSTOS_CHECKPOINT_HOST_TEST)
#define FLASH_SECTOR_6 6U
#define FLASH_SECTOR_7 7U
static uint8_t test_flash[CHECKPOINT_SECTOR_SIZE*2U];
static int test_fail_program_after=-1;
static bool test_fail_erase;
static bool test_fail_unlock, test_fail_commit, test_fail_lock, test_corrupt_verify;
#endif

typedef struct {
    uint32_t session_id;
    uint16_t floor, highest;
    uint64_t seen;
    uint8_t approved, started;
    uint16_t reserved;
} stored_window_t;

typedef struct {
    uint32_t session_id;
    uint16_t incident_id;
    uint8_t source_id, kind, used, closed, muted, reserved;
} stored_incident_t;

typedef struct {
    uint32_t commit, magic;
    uint16_t version, size;
    uint32_t generation;
    uint8_t local_source, reserved0[3];
    uint32_t session_id, next_sequence, next_incident;
    stored_window_t windows[NOSTOS_NODE_COUNT];
    stored_incident_t incidents[NOSTOS_INCIDENT_CAPACITY];
    uint32_t crc32;
} flash_checkpoint_t;

_Static_assert(sizeof(flash_checkpoint_t)<=CHECKPOINT_SLOT_SIZE,"checkpoint slot overflow");
_Static_assert((CHECKPOINT_SLOT_SIZE%4U)==0U,"checkpoint slot alignment");

volatile uint32_t protocol_checkpoint_debug_generation;
volatile uint32_t protocol_checkpoint_debug_address;
volatile HAL_StatusTypeDef protocol_checkpoint_debug_hal_status=HAL_ERROR;

static uint32_t active_sector_address=CHECKPOINT_SECTOR6_ADDRESS;
static uint32_t active_sector_number=FLASH_SECTOR_6;
static size_t next_slot;
static uint32_t generation;
static bool storage_ready;

static uint32_t crc32_bytes(const uint8_t *data,size_t length)
{
    uint32_t crc=UINT32_C(0xFFFFFFFF);
    for(size_t i=0;i<length;++i) {
        crc^=data[i];
        for(unsigned bit=0;bit<8;++bit) crc=(crc>>1)^((crc&1U)?UINT32_C(0xEDB88320):0U);
    }
    return ~crc;
}

static uint32_t slot_address(uint32_t sector,size_t slot)
{ return sector+(uint32_t)(slot*CHECKPOINT_SLOT_SIZE); }

static void flash_read(uint32_t address,void *destination,size_t length)
{
#if defined(NOSTOS_CHECKPOINT_HOST_TEST)
    size_t offset=(size_t)(address-CHECKPOINT_SECTOR6_ADDRESS);
    if(address<CHECKPOINT_SECTOR6_ADDRESS || offset>sizeof(test_flash) || length>sizeof(test_flash)-offset) {
        memset(destination,0,length);
        return;
    }
    memcpy(destination,test_flash+offset,length);
#else
    memcpy(destination,(const void *)(uintptr_t)address,length);
#endif
}

static bool slot_erased(uint32_t address)
{
    for(size_t i=0;i<CHECKPOINT_SLOT_SIZE/4U;++i) {
        uint32_t word;
        flash_read(address+(uint32_t)(i*4U),&word,sizeof(word));
        if(word!=UINT32_MAX) return false;
    }
    return true;
}

static bool valid_record(const flash_checkpoint_t *record)
{
    if(record->commit!=CHECKPOINT_COMMIT || record->magic!=CHECKPOINT_MAGIC ||
        record->version!=CHECKPOINT_VERSION || record->size!=sizeof(*record)) return false;
    const size_t begin=offsetof(flash_checkpoint_t,magic);
    const size_t length=offsetof(flash_checkpoint_t,crc32)-begin;
    return crc32_bytes((const uint8_t *)record+begin,length)==record->crc32;
}

static void checkpoint_from_flash(const flash_checkpoint_t *source,
    message_protocol_checkpoint_t *destination);

static bool scan_latest(flash_checkpoint_t *latest,uint32_t *latest_address)
{
    bool found=false;
    const uint32_t sectors[]={CHECKPOINT_SECTOR6_ADDRESS,CHECKPOINT_SECTOR7_ADDRESS};
    for(size_t sector=0;sector<2;++sector) for(size_t slot=0;slot<CHECKPOINT_SLOTS;++slot) {
        flash_checkpoint_t candidate;
        uint32_t address=slot_address(sectors[sector],slot);
        flash_read(address,&candidate,sizeof(candidate));
        message_protocol_checkpoint_t checkpoint;
        checkpoint_from_flash(&candidate,&checkpoint);
        if(valid_record(&candidate) && message_protocol_checkpoint_valid(&checkpoint) &&
            (!found || candidate.generation>latest->generation)) {
            *latest=candidate; *latest_address=address; found=true;
        }
    }
    return found;
}

static size_t first_erased_slot(uint32_t sector)
{
    for(size_t slot=0;slot<CHECKPOINT_SLOTS;++slot)
        if(slot_erased(slot_address(sector,slot))) return slot;
    return CHECKPOINT_SLOTS;
}

static HAL_StatusTypeDef erase_sector(uint32_t sector_number)
{
#if defined(NOSTOS_CHECKPOINT_HOST_TEST)
    if(test_fail_erase) return HAL_ERROR;
    size_t offset=sector_number==FLASH_SECTOR_6?0U:CHECKPOINT_SECTOR_SIZE;
    memset(test_flash+offset,0xFF,CHECKPOINT_SECTOR_SIZE);
    return HAL_OK;
#else
    FLASH_EraseInitTypeDef erase={.TypeErase=FLASH_TYPEERASE_SECTORS,
        .Sector=sector_number,.NbSectors=1,.VoltageRange=FLASH_VOLTAGE_RANGE_3};
    uint32_t error=0;
    HAL_StatusTypeDef status=HAL_FLASH_Unlock();
    if(status==HAL_OK) {
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP|FLASH_FLAG_OPERR|FLASH_FLAG_WRPERR|
            FLASH_FLAG_PGAERR|FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);
        status=HAL_FLASHEx_Erase(&erase,&error);
    }
    HAL_StatusTypeDef lock_status=HAL_FLASH_Lock();
    return status==HAL_OK?lock_status:status;
#endif
}

static void storage_prepare(void)
{
    flash_checkpoint_t latest={0}; uint32_t address=0;
    if(scan_latest(&latest,&address)) {
        generation=latest.generation;
        active_sector_address=address>=CHECKPOINT_SECTOR7_ADDRESS?
            CHECKPOINT_SECTOR7_ADDRESS:CHECKPOINT_SECTOR6_ADDRESS;
        active_sector_number=active_sector_address==CHECKPOINT_SECTOR7_ADDRESS?
            FLASH_SECTOR_7:FLASH_SECTOR_6;
    }
    next_slot=first_erased_slot(active_sector_address);
    storage_ready=true;
}

static void checkpoint_to_flash(const message_protocol_checkpoint_t *source,
    flash_checkpoint_t *destination,uint32_t next_generation)
{
    *destination=(flash_checkpoint_t){.commit=CHECKPOINT_COMMIT,.magic=CHECKPOINT_MAGIC,
        .version=CHECKPOINT_VERSION,.size=sizeof(*destination),.generation=next_generation,
        .local_source=source->source_id,.session_id=source->session_id,
        .next_sequence=source->next_sequence,.next_incident=source->next_incident};
    for(size_t i=0;i<NOSTOS_NODE_COUNT;++i) destination->windows[i]=(stored_window_t){
        .session_id=source->windows[i].session_id,.floor=source->windows[i].floor,
        .highest=source->windows[i].highest,.seen=source->windows[i].seen,
        .approved=source->windows[i].approved,.started=source->windows[i].started};
    for(size_t i=0;i<NOSTOS_INCIDENT_CAPACITY;++i) destination->incidents[i]=(stored_incident_t){
        .session_id=source->incidents[i].ref.session_id,
        .incident_id=source->incidents[i].ref.incident_id,
        .source_id=source->incidents[i].source_id,.kind=source->incidents[i].kind,
        .used=source->incidents[i].used,.closed=source->incidents[i].closed,
        .muted=source->incidents[i].muted};
    const size_t begin=offsetof(flash_checkpoint_t,magic);
    const size_t length=offsetof(flash_checkpoint_t,crc32)-begin;
    destination->crc32=crc32_bytes((const uint8_t *)destination+begin,length);
}

static void checkpoint_from_flash(const flash_checkpoint_t *source,
    message_protocol_checkpoint_t *destination)
{
    *destination=(message_protocol_checkpoint_t){.source_id=source->local_source,
        .session_id=source->session_id,.next_sequence=source->next_sequence,
        .next_incident=source->next_incident};
    for(size_t i=0;i<NOSTOS_NODE_COUNT;++i) destination->windows[i]=(nostos_rx_window_t){
        .session_id=source->windows[i].session_id,.floor=source->windows[i].floor,
        .highest=source->windows[i].highest,.seen=source->windows[i].seen,
        .approved=source->windows[i].approved!=0U,.started=source->windows[i].started!=0U};
    for(size_t i=0;i<NOSTOS_INCIDENT_CAPACITY;++i) destination->incidents[i]=(nostos_incident_record_t){
        .source_id=source->incidents[i].source_id,.kind=source->incidents[i].kind,
        .ref={source->incidents[i].session_id,source->incidents[i].incident_id},
        .used=source->incidents[i].used!=0U,.closed=source->incidents[i].closed!=0U,
        .muted=source->incidents[i].muted!=0U};
}

#if defined(NOSTOS_CHECKPOINT_HOST_TEST)
void protocol_checkpoint_test_reset(void)
{
    memset(test_flash,0xFF,sizeof(test_flash));
    test_fail_program_after=-1; test_fail_erase=false;
    test_fail_unlock=false; test_fail_commit=false; test_fail_lock=false;
    test_corrupt_verify=false;
    active_sector_address=CHECKPOINT_SECTOR6_ADDRESS;
    active_sector_number=FLASH_SECTOR_6;
    next_slot=0; generation=0; storage_ready=false;
    protocol_checkpoint_debug_generation=0;
    protocol_checkpoint_debug_address=0;
    protocol_checkpoint_debug_hal_status=HAL_ERROR;
}
void protocol_checkpoint_test_reboot(void)
{
    active_sector_address=CHECKPOINT_SECTOR6_ADDRESS;
    active_sector_number=FLASH_SECTOR_6;
    next_slot=0; generation=0; storage_ready=false;
    protocol_checkpoint_debug_generation=0;
    protocol_checkpoint_debug_address=0;
    protocol_checkpoint_debug_hal_status=HAL_ERROR;
}
void protocol_checkpoint_test_fail_program_after(int word_count)
{ test_fail_program_after=word_count; }
void protocol_checkpoint_test_fail_erase(bool fail)
{ test_fail_erase=fail; }
void protocol_checkpoint_test_fail_unlock(bool fail)
{ test_fail_unlock=fail; }
void protocol_checkpoint_test_fail_commit(bool fail)
{ test_fail_commit=fail; }
void protocol_checkpoint_test_fail_lock(bool fail)
{ test_fail_lock=fail; }
void protocol_checkpoint_test_corrupt_verify(bool corrupt)
{ test_corrupt_verify=corrupt; }
void protocol_checkpoint_test_inject(const message_protocol_checkpoint_t *checkpoint,
    uint32_t record_generation,unsigned sector,unsigned slot,bool committed)
{
    flash_checkpoint_t record;
    checkpoint_to_flash(checkpoint,&record,record_generation);
    if(!committed) record.commit=UINT32_MAX;
    uint32_t base=sector==6U?CHECKPOINT_SECTOR6_ADDRESS:CHECKPOINT_SECTOR7_ADDRESS;
    size_t offset=(size_t)(slot_address(base,slot)-CHECKPOINT_SECTOR6_ADDRESS);
    memcpy(test_flash+offset,&record,sizeof(record));
}
void protocol_checkpoint_test_corrupt_crc(unsigned sector,unsigned slot)
{
    uint32_t base=sector==6U?CHECKPOINT_SECTOR6_ADDRESS:CHECKPOINT_SECTOR7_ADDRESS;
    size_t offset=(size_t)(slot_address(base,slot)-CHECKPOINT_SECTOR6_ADDRESS)+
        offsetof(flash_checkpoint_t,crc32);
    test_flash[offset]^=0x01U;
}
bool protocol_checkpoint_test_scan(message_protocol_checkpoint_t *checkpoint,
    uint32_t *record_generation)
{
    flash_checkpoint_t latest={0}; uint32_t address=0;
    if(!scan_latest(&latest,&address)) return false;
    checkpoint_from_flash(&latest,checkpoint);
    if(record_generation) *record_generation=latest.generation;
    return true;
}
#endif

static nostos_result_t program_checkpoint(const flash_checkpoint_t *record,uint32_t address,
    bool *durable)
{
    *durable=false;
    if(!slot_erased(address)) return NOSTOS_CONFLICT;
#if defined(NOSTOS_CHECKPOINT_HOST_TEST)
    HAL_StatusTypeDef status=test_fail_unlock?HAL_ERROR:HAL_OK;
#else
    HAL_StatusTypeDef status=HAL_FLASH_Unlock();
    if(status==HAL_OK) {
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP|FLASH_FLAG_OPERR|FLASH_FLAG_WRPERR|
            FLASH_FLAG_PGAERR|FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);
#endif
        const uint32_t *words=(const uint32_t *)(const void *)record;
        const size_t word_count=(sizeof(*record)+3U)/4U;
        for(size_t i=1;i<word_count && status==HAL_OK;++i) {
#if defined(NOSTOS_CHECKPOINT_HOST_TEST)
            if(test_fail_commit || test_fail_program_after==0) status=HAL_ERROR;
            else {
                if(test_fail_program_after>0) --test_fail_program_after;
                size_t offset=(size_t)(address-CHECKPOINT_SECTOR6_ADDRESS)+(i*4U);
                uint32_t current;
                memcpy(&current,test_flash+offset,sizeof(current));
                if((current&words[i])!=words[i]) status=HAL_ERROR;
                else memcpy(test_flash+offset,&words[i],sizeof(words[i]));
            }
#else
            status=HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,address+(uint32_t)(i*4U),words[i]);
#endif
        }
        if(status==HAL_OK) {
#if defined(NOSTOS_CHECKPOINT_HOST_TEST)
            if(test_fail_program_after==0) status=HAL_ERROR;
            else {
                size_t offset=(size_t)(address-CHECKPOINT_SECTOR6_ADDRESS);
                uint32_t commit=CHECKPOINT_COMMIT;
                memcpy(test_flash+offset,&commit,sizeof(commit));
            }
#else
            status=HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,address,CHECKPOINT_COMMIT);
#endif
        }
#if !defined(NOSTOS_CHECKPOINT_HOST_TEST)
    }
    HAL_StatusTypeDef lock_status=HAL_FLASH_Lock();
    if(status==HAL_OK) status=lock_status;
#else
    if(status==HAL_OK && test_fail_lock) status=HAL_ERROR;
    if(test_corrupt_verify) {
        size_t offset=(size_t)(address-CHECKPOINT_SECTOR6_ADDRESS)+
            offsetof(flash_checkpoint_t,crc32);
        test_flash[offset]^=0x01U;
    }
#endif
    protocol_checkpoint_debug_hal_status=status;
    flash_checkpoint_t verify;
    flash_read(address,&verify,sizeof(verify));
    *durable=valid_record(&verify) && verify.generation==record->generation;
    return status==HAL_OK && *durable?NOSTOS_OK:NOSTOS_IO_ERROR;
}

nostos_result_t message_protocol_service_checkpoint_commit(
    const message_protocol_checkpoint_t *checkpoint)
{
    if(!checkpoint) return NOSTOS_BAD_ARGUMENT;
    if(!message_protocol_checkpoint_valid(checkpoint)) return NOSTOS_BAD_VALUE;
    if(!storage_ready) storage_prepare();
    if(generation==UINT32_MAX) return NOSTOS_EXHAUSTED;
    if(next_slot==CHECKPOINT_SLOTS) {
        uint32_t target_address=active_sector_address==CHECKPOINT_SECTOR6_ADDRESS?
            CHECKPOINT_SECTOR7_ADDRESS:CHECKPOINT_SECTOR6_ADDRESS;
        uint32_t target_number=active_sector_number==FLASH_SECTOR_6?FLASH_SECTOR_7:FLASH_SECTOR_6;
        protocol_checkpoint_debug_hal_status=erase_sector(target_number);
        if(protocol_checkpoint_debug_hal_status!=HAL_OK) return NOSTOS_IO_ERROR;
        active_sector_address=target_address;
        active_sector_number=target_number;
        next_slot=0;
    }
    flash_checkpoint_t record;
    checkpoint_to_flash(checkpoint,&record,generation+1U);
    uint32_t address=slot_address(active_sector_address,next_slot);
    bool durable=false;
    nostos_result_t result=program_checkpoint(&record,address,&durable);
    if(durable) {
        generation=record.generation; ++next_slot;
        protocol_checkpoint_debug_generation=generation;
        protocol_checkpoint_debug_address=address;
        while(next_slot<CHECKPOINT_SLOTS && !slot_erased(slot_address(active_sector_address,next_slot)))
            ++next_slot;
    } else if(!slot_erased(address)) {
        ++next_slot;
        while(next_slot<CHECKPOINT_SLOTS && !slot_erased(slot_address(active_sector_address,next_slot)))
            ++next_slot;
    }
    return result;
}

static message_protocol_checkpoint_t initial_checkpoint(void)
{
    message_protocol_checkpoint_t checkpoint={.source_id=NOSTOS_V2_LOCAL_SOURCE,
        .session_id=NOSTOS_V2_DEPLOYMENT_SESSION,.next_sequence=0,.next_incident=1};
    for(size_t i=0;i<NOSTOS_NODE_COUNT;++i) checkpoint.windows[i]=(nostos_rx_window_t){
        .session_id=NOSTOS_V2_DEPLOYMENT_SESSION,.approved=true};
    return checkpoint;
}

nostos_result_t message_protocol_service_boot(UART_HandleTypeDef *uart,vs1003b_status_t audio_status)
{
    flash_checkpoint_t stored={0}; uint32_t address=0;
    message_protocol_checkpoint_t checkpoint;
    storage_prepare();
    if(scan_latest(&stored,&address)) checkpoint_from_flash(&stored,&checkpoint);
    else checkpoint=initial_checkpoint();
    if(checkpoint.source_id!=NOSTOS_V2_LOCAL_SOURCE) return NOSTOS_CONFLICT;
    nostos_result_t result=message_protocol_service_restore(uart,audio_status,&checkpoint);
    if(result==NOSTOS_OK) {
        result=message_protocol_service_checkpoint(&checkpoint);
        if(result==NOSTOS_OK) result=message_protocol_service_checkpoint_commit(&checkpoint);
    }
    if(result!=NOSTOS_OK) message_protocol_service_shutdown();
    return result;
}

#endif
