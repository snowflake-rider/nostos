#include "vs1003b.h"

#include "main.h"

#include <stdbool.h>
#include <stddef.h>

#define VS1003B_SCI_READ_OPCODE 0x03U
#define VS1003B_SCI_WRITE_OPCODE 0x02U
#define VS1003B_SCI_MODE_ADDRESS 0x00U
#define VS1003B_SCI_MODE_RESET_VALUE 0x0800U
#define VS1003B_SM_TESTS 0x0020U

#define VS1003B_RESET_LOW_MS 2U
#define VS1003B_DREQ_TIMEOUT_MS 100U
#define VS1003B_PLAY_STALL_TIMEOUT_MS 2000U
#define VS1003B_SPI_TIMEOUT_MS 10U
#define VS1003B_SDI_CHUNK_SIZE 32U
#define VS1003B_END_FILL_SIZE 32U

static SPI_HandleTypeDef *vs1003b_spi = NULL;

static const uint8_t *play_data = NULL;
static uint32_t play_size = 0U;
static uint32_t play_position = 0U;
static uint32_t play_progress_at = 0U;
static bool play_finishing = false;
static bool play_active = false;

static const uint8_t vs1003b_sine_start_command[8] = {
    0x53U, 0xEFU, 0x6EU, 0x03U, 0x00U, 0x00U, 0x00U, 0x00U,
};

static const uint8_t vs1003b_sine_stop_command[8] = {
    0x45U, 0x78U, 0x69U, 0x74U, 0x00U, 0x00U, 0x00U, 0x00U,
};

bool vs1003b_is_ready(void)
{
    return HAL_GPIO_ReadPin(VS_DREQ_GPIO_Port, VS_DREQ_Pin) == GPIO_PIN_SET;
}

static bool vs1003b_wait_ready(uint32_t timeout_ms)
{
    uint32_t started_at_ms = HAL_GetTick();

    while (!vs1003b_is_ready())
    {                                               
        if ((uint32_t)(HAL_GetTick() - started_at_ms) >= timeout_ms)
        {
            return false;
        }
    }

    return true;
}

static void vs1003b_hardware_reset(void)
{
    /* 리셋 중에는 두 SPI 선택 신호를 모두 비활성(High)으로 유지합니다. */
    HAL_GPIO_WritePin(VS_XCS_GPIO_Port, VS_XCS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(VS_XDCS_GPIO_Port, VS_XDCS_Pin, GPIO_PIN_SET);

    HAL_GPIO_WritePin(VS_RST_GPIO_Port, VS_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(VS1003B_RESET_LOW_MS);
    HAL_GPIO_WritePin(VS_RST_GPIO_Port, VS_RST_Pin, GPIO_PIN_SET);
}

vs1003b_status_t vs1003b_read_register(uint8_t address, uint16_t *value)
{
    uint8_t tx_data[4] = {
        VS1003B_SCI_READ_OPCODE,
        address,
        0xFFU,
        0xFFU,
    };
    uint8_t rx_data[4] = {0U};

    if ((vs1003b_spi == NULL) || (value == NULL))
    {
        return VS1003B_STATUS_INVALID_ARGUMENT;
    }

    if (!vs1003b_wait_ready(VS1003B_DREQ_TIMEOUT_MS))
    {
        return VS1003B_STATUS_DREQ_TIMEOUT;
    }

    HAL_GPIO_WritePin(VS_XCS_GPIO_Port, VS_XCS_Pin, GPIO_PIN_RESET);

    HAL_StatusTypeDef hal_status = HAL_SPI_TransmitReceive(
        vs1003b_spi,
        tx_data,
        rx_data,
        sizeof(tx_data),
        VS1003B_SPI_TIMEOUT_MS
    );

    HAL_GPIO_WritePin(VS_XCS_GPIO_Port, VS_XCS_Pin, GPIO_PIN_SET);

    if (hal_status != HAL_OK)
    {
        return VS1003B_STATUS_SPI_ERROR;
    }

    *value = ((uint16_t)rx_data[2] << 8U) | rx_data[3];
    return VS1003B_STATUS_OK;
}

vs1003b_status_t vs1003b_write_register(uint8_t address, uint16_t value)
{
    uint8_t tx_data[4] = {
        VS1003B_SCI_WRITE_OPCODE,
        address,
        (uint8_t)(value >> 8U),
        (uint8_t)value,
    };

    if (vs1003b_spi == NULL)
    {
        return VS1003B_STATUS_INVALID_ARGUMENT;
    }

    if (!vs1003b_wait_ready(VS1003B_DREQ_TIMEOUT_MS))
    {
        return VS1003B_STATUS_DREQ_TIMEOUT;
    }

    HAL_GPIO_WritePin(VS_XCS_GPIO_Port, VS_XCS_Pin, GPIO_PIN_RESET);

    HAL_StatusTypeDef hal_status = HAL_SPI_Transmit(
        vs1003b_spi,
        tx_data,
        sizeof(tx_data),
        VS1003B_SPI_TIMEOUT_MS
    );

    HAL_GPIO_WritePin(VS_XCS_GPIO_Port, VS_XCS_Pin, GPIO_PIN_SET);

    if (hal_status != HAL_OK)
    {
        return VS1003B_STATUS_SPI_ERROR;
    }

    /*
     * 마지막 SCLK 직후 DREQ가 Low로 전환되기 전에 이전 High를 읽는 경쟁을 피합니다.
     * 특히 SCI_CLOCKF는 내부 클럭이 바뀌므로 짧은 안정화 시간을 둡니다.
     */
    HAL_Delay(1U);

    if (!vs1003b_wait_ready(VS1003B_DREQ_TIMEOUT_MS))
    {
        return VS1003B_STATUS_DREQ_TIMEOUT;
    }

    return VS1003B_STATUS_OK;
}

static vs1003b_status_t vs1003b_send_sdi(const uint8_t *data, uint16_t size)
{
    uint8_t rx_data[VS1003B_SDI_CHUNK_SIZE] = {0U};

    if ((vs1003b_spi == NULL) || (data == NULL) ||
        (size == 0U) || (size > sizeof(rx_data)))
    {
        return VS1003B_STATUS_INVALID_ARGUMENT;
    }

    if (!vs1003b_wait_ready(VS1003B_DREQ_TIMEOUT_MS))
    {
        return VS1003B_STATUS_DREQ_TIMEOUT;
    }

    HAL_GPIO_WritePin(VS_XCS_GPIO_Port, VS_XCS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(VS_XDCS_GPIO_Port, VS_XDCS_Pin, GPIO_PIN_RESET);

    HAL_StatusTypeDef hal_status = HAL_SPI_TransmitReceive(
        vs1003b_spi,
        (uint8_t *)data,
        rx_data,
        size,
        VS1003B_SPI_TIMEOUT_MS
    );

    HAL_GPIO_WritePin(VS_XDCS_GPIO_Port, VS_XDCS_Pin, GPIO_PIN_SET);

    if (hal_status != HAL_OK)
    {
        return VS1003B_STATUS_SPI_ERROR;
    }

    return VS1003B_STATUS_OK;
}

vs1003b_status_t vs1003b_play_start(const uint8_t *data, uint32_t size)
{
    if ((data == NULL) || (size == 0U))
    {
        return VS1003B_STATUS_INVALID_ARGUMENT;
    }

    if (play_active)
    {
        return VS1003B_STATUS_BUSY;
    }

    play_data = data;
    play_size = size;
    play_position = 0U;
    play_progress_at = HAL_GetTick();
    play_finishing = false;
    play_active = true;

    return VS1003B_STATUS_OK;
}

vs1003b_status_t vs1003b_play_process(void)
{
    static const uint8_t end_fill[VS1003B_END_FILL_SIZE] = {0U};

    if (!play_active)
    {
        return VS1003B_STATUS_OK;
    }

    /* DREQ LOW는 보통 흐름 제어지만, 계속 LOW면 무한 BUSY로 남기지 않습니다. */
    if (!vs1003b_is_ready())
    {
        if ((uint32_t)(HAL_GetTick() - play_progress_at) >= VS1003B_PLAY_STALL_TIMEOUT_MS)
        {
            play_active = false;
            play_finishing = false;
            return VS1003B_STATUS_DREQ_TIMEOUT;
        }
        return VS1003B_STATUS_OK;
    }

    if (play_finishing)
    {
        vs1003b_status_t status = vs1003b_send_sdi(end_fill, sizeof(end_fill));
        play_active = false;
        play_finishing = false;
        return status;
    }

    uint32_t remaining = play_size - play_position;
    uint16_t chunk_size = (remaining > VS1003B_SDI_CHUNK_SIZE)
        ? VS1003B_SDI_CHUNK_SIZE
        : (uint16_t)remaining;

    vs1003b_status_t status = vs1003b_send_sdi(
        &play_data[play_position],
        chunk_size
    );

    if (status != VS1003B_STATUS_OK)
    {
        play_active = false;
        return status;
    }

    play_position += chunk_size;
    play_progress_at = HAL_GetTick();
    if (play_position >= play_size)
    {
        play_finishing = true;
    }

    return VS1003B_STATUS_OK;
}

bool vs1003b_is_playing(void)
{
    return play_active;
}

uint32_t vs1003b_play_position(void)
{
    return play_position;
}

vs1003b_status_t vs1003b_sine_test_start(void)
{
    uint16_t mode = 0U;
    vs1003b_status_t status = vs1003b_read_register(
        VS1003B_SCI_MODE_ADDRESS,
        &mode
    );

    if (status != VS1003B_STATUS_OK)
    {
        return status;
    }

    status = vs1003b_write_register(
        VS1003B_SCI_MODE_ADDRESS,
        mode | VS1003B_SM_TESTS
    );

    if (status != VS1003B_STATUS_OK)
    {
        return status;
    }

    return vs1003b_send_sdi(
        vs1003b_sine_start_command,
        sizeof(vs1003b_sine_start_command)
    );
}

vs1003b_status_t vs1003b_sine_test_stop(void)
{
    return vs1003b_send_sdi(
        vs1003b_sine_stop_command,
        sizeof(vs1003b_sine_stop_command)
    );
}

vs1003b_status_t vs1003b_sdi_test(uint16_t *echo)
{
    /* VS1003 datasheet 9.8.4: SDI로 받은 명령이 SCI_MODE를 HDAT0에 복사합니다. */
    static const uint8_t command[8] = {0x53U, 0x70U, 0xEEU, 0U, 0U, 0U, 0U, 0U};
    if (echo == NULL) return VS1003B_STATUS_INVALID_ARGUMENT;
    if (play_active) return VS1003B_STATUS_BUSY;
    uint16_t mode = 0U;
    vs1003b_status_t status = vs1003b_read_register(VS1003B_SCI_MODE_ADDRESS, &mode);
    if (status != VS1003B_STATUS_OK) return status;
    mode |= VS1003B_SM_TESTS;
    status = vs1003b_write_register(VS1003B_SCI_MODE_ADDRESS, mode);
    if (status != VS1003B_STATUS_OK) return status;
    status = vs1003b_send_sdi(command, sizeof(command));
    if (status != VS1003B_STATUS_OK) return status;
    HAL_Delay(1U);
    status = vs1003b_read_register(0x08U, echo);
    if (status != VS1003B_STATUS_OK) return status;
    return (*echo == mode) ? VS1003B_STATUS_OK : VS1003B_STATUS_REGISTER_MISMATCH;
}

vs1003b_status_t vs1003b_init(SPI_HandleTypeDef *hspi, uint16_t *mode_value)
{
    vs1003b_status_t status;

    if ((hspi == NULL) || (mode_value == NULL))
    {
        return VS1003B_STATUS_INVALID_ARGUMENT;
    }

    vs1003b_spi = hspi;
    /* 하드웨어 리셋은 이전 스트림을 취소합니다. 재초기화 실패 시에도 같습니다. */
    play_active = false;
    play_finishing = false;
    play_data = NULL;
    play_size = play_position = 0U;
    play_progress_at = HAL_GetTick();
    vs1003b_hardware_reset();

    if (!vs1003b_wait_ready(VS1003B_DREQ_TIMEOUT_MS))
    {
        return VS1003B_STATUS_DREQ_TIMEOUT;
    }

    status = vs1003b_read_register(VS1003B_SCI_MODE_ADDRESS, mode_value);
    if (status != VS1003B_STATUS_OK)
    {
        return status;
    }

    if (*mode_value != VS1003B_SCI_MODE_RESET_VALUE)
    {
        return VS1003B_STATUS_MODE_MISMATCH;
    }

    return VS1003B_STATUS_OK;
}
