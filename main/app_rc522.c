#include "app_rc522.h"

#include <stdio.h>
#include <string.h>

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "rc522";

/* ---- 接线（如与其他外设冲突，只改这几行即可）----
 * 板上已被占用的脚：1/2/4/5/6/7/8/9/10/15/16/17/18/21/38/39/41/42/47
 * 屏幕占用 SPI3：CS=21 SDI=17 RST=15 SCK=18 DC=16
 * 所以 RC522 走 SPI2，与屏幕互不干扰                          */
#define RC522_SPI_HOST   SPI2_HOST
#define RC522_SCK_PIN    12
#define RC522_MOSI_PIN   11
#define RC522_MISO_PIN   3
#define RC522_CS_PIN     48
#define RC522_SPI_HZ     (1 * 1000 * 1000)   /* 初次调试先用 1MHz，线长/接触不良也能通，通了再改回 4MHz */
/* 模块 RST 直接接 3.3V 即可，不占用 GPIO */

/* ---- MFRC522 寄存器 ---- */
#define CommandReg      0x01
#define ComIEnReg       0x02
#define ComIrqReg       0x04
#define ErrorReg        0x06
#define Status2Reg      0x08
#define FIFODataReg     0x09
#define FIFOLevelReg    0x0A
#define ControlReg      0x0C
#define BitFramingReg   0x0D
#define CollReg         0x0E
#define ModeReg         0x11
#define TxControlReg    0x14
#define TxASKReg        0x15
#define TModeReg        0x2A
#define TPrescalerReg   0x2B
#define TReloadRegH     0x2C
#define TReloadRegL     0x2D
#define VersionReg      0x37

/* ---- MFRC522 命令 ---- */
#define PCD_Idle        0x00
#define PCD_Transceive  0x0C
#define PCD_SoftReset   0x0F

#define PICC_REQA       0x26
#define PICC_ANTICOLL   0x93

#define COMIRQ_RXIRQ    0x20
#define COMIRQ_TIMERIRQ 0x01
#define ERROR_MASK      0x1B

static spi_device_handle_t s_spi;
static bool  s_card_present = false;
static int64_t s_last_poll_us = 0;
static bool  s_chip_ok = false;          /* 芯片版本寄存器是否读到正常值 */
static int64_t s_last_retry_us = 0;
static int   s_miss = 0;                 /* 连续寻卡失败次数（只用于日志节流） */
static int   s_absent = 0;               /* 连续"读不到卡"次数，用来判断卡是否真的拿开 */

/* ---------------- SPI 底层 ---------------- */

static void rc522_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { (uint8_t)((reg << 1) & 0x7E), val };
    spi_transaction_t t = {
        .length    = 16,
        .tx_buffer = tx,
    };
    spi_device_polling_transmit(s_spi, &t);
}

static uint8_t rc522_read_reg(uint8_t reg)
{
    uint8_t tx[2] = { (uint8_t)(((reg << 1) & 0x7E) | 0x80), 0x00 };
    uint8_t rx[2] = { 0 };
    spi_transaction_t t = {
        .length    = 16,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    spi_device_polling_transmit(s_spi, &t);
    return rx[1];
}

/* 读寄存器，但第二个字节发自定义"哑字节"。
   真芯片收完地址后回的是寄存器内容，不会把哑字节送回来；
   如果读到的东西正好等于哑字节，说明 MISO 和 MOSI 连在一起（回环）。 */
static uint8_t rc522_read_reg_dummy(uint8_t reg, uint8_t dummy)
{
    uint8_t tx[2] = { (uint8_t)(((reg << 1) & 0x7E) | 0x80), dummy };
    uint8_t rx[2] = { 0 };
    spi_transaction_t t = {
        .length    = 16,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    spi_device_polling_transmit(s_spi, &t);
    return rx[1];
}

static void rc522_set_bit_mask(uint8_t reg, uint8_t mask)
{
    rc522_write_reg(reg, rc522_read_reg(reg) | mask);
}

static void rc522_antenna_on(void)
{
    uint8_t v = rc522_read_reg(TxControlReg);
    if ((v & 0x03) != 0x03) {
        rc522_write_reg(TxControlReg, v | 0x03);
    }
}

/* ---------------- 卡片操作 ---------------- */

/* 等待收发完成：返回 true 表示有响应帧
   这里只用软件超时（50ms），不依赖芯片内部定时器——
   兼容芯片的定时器寄存器行为可能和原厂不同，靠 TimerIRQ 判断容易提前放弃 */
static bool rc522_wait_rx(void)
{
    for (int i = 0; i < 50; i++) {
        if (rc522_read_reg(ComIrqReg) & COMIRQ_RXIRQ) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return false;
}

/* 寻卡 REQA，成功返回 true */
static bool rc522_request(void)
{
    rc522_write_reg(BitFramingReg, 0x07);        /* 最后一位发 7 bit */
    rc522_write_reg(CommandReg, PCD_Idle);
    rc522_write_reg(ComIrqReg, 0x7F);            /* 清所有中断标志 */
    rc522_write_reg(FIFOLevelReg, 0x80);         /* 清 FIFO */
    rc522_write_reg(FIFODataReg, PICC_REQA);
    rc522_write_reg(CommandReg, PCD_Transceive);
    rc522_set_bit_mask(BitFramingReg, 0x80);     /* StartSend */

    bool ok = rc522_wait_rx();
    rc522_write_reg(BitFramingReg, 0x00);

    if (!ok) {
        return false;
    }
    if (rc522_read_reg(ErrorReg) & ERROR_MASK) {
        return false;
    }
    return (rc522_read_reg(FIFOLevelReg) & 0x7F) == 2;   /* ATQA 两字节 */
}

/* 防冲突，取 4 字节 UID */
static bool rc522_anticoll(uint8_t uid[4])
{
    uint8_t buf[5] = { 0 };

    rc522_write_reg(CommandReg, PCD_Idle);
    rc522_write_reg(ComIrqReg, 0x7F);
    rc522_write_reg(FIFOLevelReg, 0x80);
    rc522_write_reg(FIFODataReg, PICC_ANTICOLL);
    rc522_write_reg(FIFODataReg, 0x20);
    rc522_write_reg(CommandReg, PCD_Transceive);
    rc522_write_reg(BitFramingReg, 0x80);        /* 8 bit + StartSend */

    if (!rc522_wait_rx()) {
        return false;
    }
    if (rc522_read_reg(ErrorReg) & ERROR_MASK) {
        return false;
    }
    if ((rc522_read_reg(FIFOLevelReg) & 0x7F) != 5) {
        return false;
    }
    for (int i = 0; i < 5; i++) {
        buf[i] = rc522_read_reg(FIFODataReg);
    }
    /* 校验 BCC：前 4 字节异或应等于第 5 字节 */
    if ((uint8_t)(buf[0] ^ buf[1] ^ buf[2] ^ buf[3]) != buf[4]) {
        return false;
    }
    memcpy(uid, buf, 4);
    return true;
}

/* ---------------- 对外接口 ---------------- */

/* 芯片级初始化。返回 false 只代表"SPI 线不通"，版本号异常不算失败 */
static bool rc522_chip_init(void)
{
    rc522_write_reg(CommandReg, PCD_SoftReset);
    for (int i = 0; i < 100; i++) {
        if ((rc522_read_reg(CommandReg) & 0x10) == 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    /* SPI 通路自检：往 ComIEnReg(0x02) 写两个互补值再读回
       - 两个值都能原样读回，且回环判别不是 0xA5 = SPI 四根线正常
       - 读回全是 0xFF = MISO 悬空高电平
       - 读回全是 0x00 = MISO 一直低电平 */
    rc522_write_reg(ComIEnReg, 0x55);
    uint8_t r55 = rc522_read_reg(ComIEnReg);
    rc522_write_reg(ComIEnReg, 0xAA);
    uint8_t raa = rc522_read_reg(ComIEnReg);
    rc522_write_reg(ComIEnReg, 0x00);
    ESP_LOGI(TAG, "SPI 自检：写0x55读到0x%02X，写0xAA读到0x%02X（分别为0x55/0xAA=SPI 正常）",
             r55, raa);

    /* 回环判别：读版本寄存器时把哑字节改成 0xA5。
       真芯片不会把哑字节送回来；如果读回 0xA5，说明 MISO/MOSI 短接，
       上面的自检是假通过。 */
    uint8_t loop = rc522_read_reg_dummy(VersionReg, 0xA5);
    ESP_LOGI(TAG, "回环判别：哑字节发0xA5 读到0x%02X（读回0xA5=回环，接线错）", loop);

    rc522_write_reg(TModeReg, 0x80);
    rc522_write_reg(TPrescalerReg, 0xA9);
    rc522_write_reg(TReloadRegH, 0x03);
    rc522_write_reg(TReloadRegL, 0xE8);
    rc522_write_reg(TxASKReg, 0x40);
    rc522_write_reg(ModeReg, 0x3D);
    rc522_write_reg(Status2Reg, 0x00);      /* 清 MFCrypto1On，避免上次会话残留 */
    rc522_antenna_on();

    uint8_t ver = rc522_read_reg(VersionReg);
    ESP_LOGI(TAG, "MFRC522 版本寄存器=0x%02X (标准值为 0x91/0x92)", ver);
    ESP_LOGI(TAG, "天线控制寄存器 TxControlReg=0x%02X（bit0/bit1 都为 1 表示天线已开启）",
             rc522_read_reg(TxControlReg));

    if (loop == 0xA5) {
        ESP_LOGE(TAG, "MISO 和 MOSI 短接了 -> 两根线插到了模块同一个针脚，或者杜邦线短路");
        return false;
    }
    if (r55 != 0x55 || raa != 0xAA) {
        ESP_LOGE(TAG, "寄存器写进去读不回（写0x55读0x%02X，写0xAA读0x%02X）-> SCK/MOSI/MISO/CS 接触不良",
                 r55, raa);
        return false;
    }
    if (ver != 0x91 && ver != 0x92) {
        ESP_LOGW(TAG, "版本号不是标准值，可能是国产兼容芯片；继续尝试寻卡");
    }
    return true;
}

esp_err_t app_rc522_init(void)
{
    spi_bus_config_t bus = {
        .mosi_io_num     = RC522_MOSI_PIN,
        .miso_io_num     = RC522_MISO_PIN,
        .sclk_io_num     = RC522_SCK_PIN,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 16,
    };
    esp_err_t err = spi_bus_initialize(RC522_SPI_HOST, &bus, SPI_DMA_DISABLED);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI 总线初始化失败: %s", esp_err_to_name(err));
        return err;
    }

    spi_device_interface_config_t dev = {
        .clock_speed_hz = RC522_SPI_HZ,
        .mode           = 0,
        .spics_io_num   = RC522_CS_PIN,
        .queue_size     = 1,
    };
    err = spi_bus_add_device(RC522_SPI_HOST, &dev, &s_spi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI 设备注册失败: %s", esp_err_to_name(err));
        return err;
    }

    /* 软复位 + 参数配置 */
    s_chip_ok = rc522_chip_init();
    s_last_retry_us = esp_timer_get_time();
    return ESP_OK;
}

bool app_rc522_poll(char *uid_str, size_t len)
{
    if (uid_str == NULL || len < 9) {
        return false;
    }

    /* 每 200ms 查一次，避免占用过多 CPU */
    int64_t now = esp_timer_get_time();
    if (now - s_last_poll_us < 200000) {
        return false;
    }
    s_last_poll_us = now;

    /* 芯片没通时每 3 秒重试一次，持续打印版本寄存器，方便定位接线问题 */
    if (!s_chip_ok) {
        if (now - s_last_retry_us >= 3000000) {
            s_last_retry_us = now;
            s_chip_ok = rc522_chip_init();
        }
        return false;
    }

    uint8_t uid[4];

    /* 寻卡 + 防冲突。兼容芯片偶尔会有一次失败，所以不能失败一次就认定卡被拿开，
       必须连续 5 次（约 1 秒）都读不到，才算卡离开 */
    bool ok = rc522_request() && rc522_anticoll(uid);
    if (!ok) {
        if (++s_absent >= 5) {
            s_absent = 0;
            s_card_present = false;
        }
        /* 每 5 秒（25 次）打印一次寻卡状态，用来判断是"射频没发出去"还是"卡没应答" */
        s_miss++;
        if (s_miss >= 25) {
            s_miss = 0;
            ESP_LOGI(TAG, "寻卡中：未检测到卡  ErrorReg=0x%02X FIFO=%d TxControlReg=0x%02X",
                     rc522_read_reg(ErrorReg), rc522_read_reg(FIFOLevelReg) & 0x7F,
                     rc522_read_reg(TxControlReg));
        }
        return false;
    }
    s_absent = 0;
    s_miss = 0;

    /* 同一张卡一直放着时不再重复上报 */
    if (s_card_present) {
        return false;
    }
    s_card_present = true;

    snprintf(uid_str, len, "%02X%02X%02X%02X",
             (unsigned)uid[0], (unsigned)uid[1], (unsigned)uid[2], (unsigned)uid[3]);
    return true;
}
