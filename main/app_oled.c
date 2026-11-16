#include "app_oled.h"

#include <stdio.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_log.h"

#include "i2c_bus.h"
#include "app_oled_font.h"

#define OLED_I2C_HZ       400000
#define OLED_ADDR_PRIMARY 0x3C
#define OLED_ADDR_ALT     0x3D

#define OLED_W            128
#define OLED_H            64
#define OLED_PAGES        (OLED_H / 8)
#define FB_LEN            (OLED_W * OLED_PAGES)
/* 1.3 寸模块多数是 SH1106（132 列显存），需要偏移 2 列；
 * 如果显示内容整体偏左/左边被切掉，把它改成 0（纯 SSD1306） */
#define OLED_COL_OFFSET   2

/* 上电初始化命令序列 */
static const uint8_t INIT_CMDS[] = {
    0xAE,             /* 关显示 */
    0xD5, 0x80,       /* 时钟分频 */
    0xA8, 0x3F,       /* 复用率 = 64 行 */
    0xD3, 0x00,       /* 显示垂直偏移 0 */
    0x40,             /* 起始行 = 0 */
    0x8D, 0x14,       /* 电荷泵开启（模块自带升压必须开） */
    0x20, 0x02,       /* 页寻址模式（SSD1306 用；SH1106 忽略该命令） */
    0xA1,             /* 段重映射：左右正常 */
    0xC8,             /* COM 扫描反向：上下正常 */
    0xDA, 0x12,       /* COM 引脚配置 */
    0x81, 0xCF,       /* 对比度 */
    0xD9, 0xF1,       /* 预充电周期 */
    0xDB, 0x40,       /* VCOMH 电压 */
    0xA4,             /* 显示跟随显存 */
    0xA6,             /* 正常显示（不反色） */
    0xAF,             /* 开显示 */
};

static const char *TAG = "oled";

static i2c_master_dev_handle_t s_dev = NULL;
static bool    s_ready = false;
static uint8_t s_fb[FB_LEN];

/* 上一次刷新时的内容，用于变化检测，避免每次主循环都刷屏 */
static int s_last_fan = -1;
static int s_last_win = -1;
static int s_last_cnt = -1;

/* ---------------- I2C 底层 ---------------- */

static esp_err_t oled_write_cmd(const uint8_t *cmd, size_t len)
{
    uint8_t buf[1 + sizeof(INIT_CMDS)];
    if (len + 1 > sizeof(buf)) {
        return ESP_ERR_INVALID_SIZE;
    }
    buf[0] = 0x00;              /* 控制字节：后续都是命令 */
    memcpy(buf + 1, cmd, len);
    return i2c_master_transmit(s_dev, buf, len + 1, 1000);
}

static esp_err_t oled_write_data(const uint8_t *data, size_t len)
{
    static uint8_t buf[1 + OLED_W];
    if (len + 1 > sizeof(buf)) {
        return ESP_ERR_INVALID_SIZE;
    }
    buf[0] = 0x40;              /* 控制字节：后续都是显存数据 */
    memcpy(buf + 1, data, len);
    return i2c_master_transmit(s_dev, buf, len + 1, 1000);
}

/* 把整帧 1024 字节显存按页写入屏幕 */
static void oled_flush(void)
{
    for (int page = 0; page < OLED_PAGES; page++) {
        uint8_t cmd[3] = {
            (uint8_t)(0xB0 | page),                              /* 选择页 */
            (uint8_t)(OLED_COL_OFFSET & 0x0F),                   /* 列地址低 4 位 */
            (uint8_t)(0x10 | ((OLED_COL_OFFSET >> 4) & 0x0F)),   /* 列地址高 4 位 */
        };
        oled_write_cmd(cmd, sizeof(cmd));
        oled_write_data(&s_fb[page * OLED_W], OLED_W);
    }
}

/* ---------------- 显存绘制 ---------------- */

/* 往显存里叠加一个字形：w=8（ASCII）或 16（汉字），高固定 16 像素 */
static void fb_draw_glyph(int x, int page, const uint8_t *bitmap, int w)
{
    for (int p = 0; p < 2; p++) {
        int pg = page + p;
        if (pg >= OLED_PAGES) {
            break;
        }
        for (int c = 0; c < w; c++) {
            int px = x + c;
            if (px < 0 || px >= OLED_W) {
                continue;
            }
            s_fb[pg * OLED_W + px] |= bitmap[p * w + c];
        }
    }
}

/* 取下一个 UTF-8 字符的 Unicode 码点 */
static const uint8_t *utf8_next(const uint8_t *p, uint16_t *cp)
{
    if (p[0] < 0x80) {
        *cp = p[0];
        return p + 1;
    }
    if ((p[0] & 0xE0) == 0xC0 && p[1]) {
        *cp = (uint16_t)(((p[0] & 0x1F) << 6) | (p[1] & 0x3F));
        return p + 2;
    }
    if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) {
        *cp = (uint16_t)(((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F));
        return p + 3;
    }
    *cp = '?';
    return p + 1;
}

static const oled_cjk_glyph_t *find_cjk(uint16_t code)
{
    for (size_t i = 0; i < OLED_CJK_COUNT; i++) {
        if (oled_cjk_table[i].code == code) {
            return &oled_cjk_table[i];
        }
    }
    return NULL;
}

/* 在 (x, y) 处画一串 UTF-8 文字，y 必须是 8 的倍数 */
static void fb_draw_text(int x, int y, const char *text)
{
    int page = y / 8;
    const uint8_t *p = (const uint8_t *)text;

    while (*p) {
        uint16_t cp = 0;
        p = utf8_next(p, &cp);

        if (cp < 0x80) {
            if (cp >= 32 && cp <= 126) {
                fb_draw_glyph(x, page, oled_ascii_8x16[cp - 32], 8);
            }
            x += 8;             /* ASCII 半角固定 8 像素宽 */
        } else {
            const oled_cjk_glyph_t *g = find_cjk(cp);
            if (g != NULL) {
                fb_draw_glyph(x, page, g->bitmap, 16);
            }
            x += 16;            /* 汉字全角固定 16 像素宽 */
        }
    }
}

/* 计算文字像素宽度，用于居中 */
static int text_width(const char *text)
{
    int w = 0;
    const uint8_t *p = (const uint8_t *)text;

    while (*p) {
        uint16_t cp = 0;
        p = utf8_next(p, &cp);
        w += (cp < 0x80) ? 8 : 16;
    }
    return w;
}

/* ---------------- 对外接口 ---------------- */

esp_err_t app_oled_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    esp_err_t err = i2c_bus_init();
    if (err != ESP_OK) {
        return err;
    }

    i2c_master_bus_handle_t bus = i2c_bus_get();
    if (bus == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t addr = 0;
    if (i2c_master_probe(bus, OLED_ADDR_PRIMARY, 100) == ESP_OK) {
        addr = OLED_ADDR_PRIMARY;
    } else if (i2c_master_probe(bus, OLED_ADDR_ALT, 100) == ESP_OK) {
        addr = OLED_ADDR_ALT;
    } else {
        ESP_LOGE(TAG, "未找到 OLED（0x3C / 0x3D 无应答），检查 SDA=GPIO41 SCL=GPIO42 与供电");
        return ESP_ERR_NOT_FOUND;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = OLED_I2C_HZ,
    };
    err = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "添加 I2C 设备失败：%s", esp_err_to_name(err));
        return err;
    }

    err = oled_write_cmd(INIT_CMDS, sizeof(INIT_CMDS));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "初始化命令发送失败：%s", esp_err_to_name(err));
        return err;
    }

    memset(s_fb, 0, sizeof(s_fb));
    oled_flush();

    s_ready = true;
    ESP_LOGI(TAG, "初始化完成，地址 0x%02X，分辨率 %dx%d", addr, OLED_W, OLED_H);
    return ESP_OK;
}

void app_oled_show_status(bool fan_on, bool window_open, int card_count)
{
    if (!s_ready) {
        return;
    }
    if ((int)fan_on == s_last_fan && (int)window_open == s_last_win && card_count == s_last_cnt) {
        return;             /* 内容没变，不刷屏 */
    }

    s_last_fan = fan_on;
    s_last_win = window_open;
    s_last_cnt = card_count;

    memset(s_fb, 0, sizeof(s_fb));

    const char *title = "实验室安全监测";
    fb_draw_text((OLED_W - text_width(title)) / 2, 0, title);

    char line[32];
    snprintf(line, sizeof(line), "风扇: %s", fan_on ? "开" : "关");
    fb_draw_text(16, 16, line);

    snprintf(line, sizeof(line), "窗户: %s", window_open ? "开" : "关");
    fb_draw_text(16, 32, line);

    snprintf(line, sizeof(line), "刷卡: %d 次", card_count);
    fb_draw_text(16, 48, line);

    oled_flush();
}
