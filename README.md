# 智慧校园 · 实验室/教室安全监测系统（端云一体）

基于 **ESP32-S3** 的实验室安全监测终端：本地烟雾检测 + 声光报警 + 风扇联动，数据经 WiFi/MQTT 上报云端，板载 LCD 实时显示状态，并通过 UART 接收 K230 视觉模块的安全帽检测结果。

> 传智杯 AIoT 嵌入式系统创新大赛参赛作品

---

## 功能特性

| 功能 | 说明 |
|---|---|
| 烟雾检测 | MQ-2 数字量输出，检测到烟雾即触发报警 |
| 声光报警 | 红灯常亮 + 有源蜂鸣器 500ms 间歇鸣叫 |
| 排风联动 | 继电器吸合启动 5V 风扇，烟雾散去后自动关闭 |
| 人体检测 | HC-SR501 / RCWL-0516 热释电或微波雷达，状态上报云端 |
| 本地显示 | 板载 ST7735S LCD（128×160）+ LVGL，显示 WiFi/MQTT 状态、温湿度、报警状态 |
| 云端上报 | MQTT 每 5 秒上报 JSON 数据到 `broker.emqx.io` |
| 视觉联动 | UART1 接收 K230 视觉模块的安全帽检测结果 |

### 报警联动逻辑

以 MQ-2 为唯一触发源：

```
无烟雾：灯灭   + 风扇关 + 蜂鸣器静音
有烟雾：红灯亮 + 风扇开 + 蜂鸣器响 500ms / 停 500ms
```

---

## 硬件清单

| 器件 | 型号 | 数量 |
|---|---|---|
| 主控开发板 | ESP32-S3-DevKitC-1（N16R8，板载 ST7735S LCD） | 1 |
| 烟雾传感器 | MQ-2 模块（LM393 比较器，DO 输出） | 1 |
| 人体感应 | HC-SR501（热释电）或 RCWL-0516（微波雷达） | 1 |
| 继电器模块 | 1 路 5V 光耦继电器（高电平触发） | 1 |
| 风扇 | 5V 直流小风扇 | 1 |
| 蜂鸣器 | 有源蜂鸣器模块（S8050 驱动，高电平触发） | 1 |
| 指示灯 | 共阴双色 LED（红/绿） | 1 |
| 限流电阻 | 220Ω | 2 |
| 温湿度 | DHT22 / AM2302（选配，主程序暂未接入） | 1 |
| 视觉模块 | 正点原子 K230D（CanMV MicroPython，选配） | 1 |

---

## 引脚分配

| ESP32-S3 引脚 | 连接对象 | 说明 |
|---|---|---|
| GPIO4 | MQ-2 `DO` | 低电平 = 检测到烟雾，内部上拉 |
| GPIO5 | HC-SR501 `OUT` / RCWL-0516 `OUT` | 高电平 = 有人，内部下拉 |
| GPIO6 | 继电器 `IN` | 高电平吸合 |
| GPIO7 | 蜂鸣器 `I/O` | 高电平鸣叫 |
| GPIO10 | 220Ω → 双色 LED 红阳极 | 高电平点亮 |
| GPIO47 | 220Ω → 双色 LED 绿阳极 | 高电平点亮 |
| GPIO1 | K230 `IO40`（TX） | UART1 RX，115200 8N1 |
| GPIO2 | K230 `IO41`（RX） | UART1 TX，115200 8N1 |
| 5V / GND | 各模块 VCC / GND | 独立供电需与开发板共地 |

### 接线要点

- **MQ-2**：`VCC`→5V、`GND`→GND、`DO`→GPIO4，`AO` 悬空
- **人体模块**：`VCC`→5V、`OUT`→GPIO5、`GND`→GND；RCWL-0516 的 `CDS` 接 3V3 可屏蔽光敏，`3V3` 脚悬空
- **继电器**：控制端 `DC+`→5V、`DC−`→GND、`IN`→GPIO6；负载端 `COM`→5V、`NO`→风扇正极，风扇负极→GND（**低边接 GND，不要接 ESP32**）
- **双色 LED**：公共脚（阴极）→GND，`R` 和 `G` 各串 220Ω 接 GPIO
- 所有模块建议由**独立 5V 电源（充电宝）供电**，并**与开发板共地**，避免 USB 口过流

---

## 软件结构

```
.
├── CMakeLists.txt
├── sdkconfig
├── main/
│   ├── main.c                  # 主程序：初始化 + 传感器读取 + 报警联动 + 上报
│   ├── main_mqtt.c/.h          # MQTT 客户端封装
│   ├── app_uart.c/.h           # UART1，接收 K230 的 HAT:0/HAT:1
│   └── wifi_config.h.example   # WiFi 账号密码模板（复制为 wifi_config.h 使用）
└── components/
    ├── app_wifi/               # WiFi 连接与 IP 获取
    ├── lcd/                    # ST7735S 屏驱动
    ├── lvgl/                   # LVGL 图形库
    ├── lvgl_port/              # LVGL 显示移植层
    ├── lvgl_gui/               # GUI Guider 生成的界面（guider_ui）
    ├── app_ethernet/           # W5500 以太网（当前未启用）
    ├── app_sd_card/            # SD 卡（当前未启用）
    └── app_gpio/               # 工厂测试 GPIO（当前未启用）
```

---

## 编译与烧录

### 1. 环境要求

- ESP-IDF **v5.5.x**
- VSCode + ESP-IDF 插件（或用 `idf.py` 命令行）

### 2. 配置 WiFi

仓库不包含真实 WiFi 密码，首次编译前必须先创建配置文件：

```powershell
Copy-Item "main\wifi_config.h.example" "main\wifi_config.h"
```

然后编辑 `main/wifi_config.h`，填入自己的账号密码：

```c
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASS "your-wifi-password"
```

> `wifi_config.h` 已被 `.gitignore` 忽略，不会上传到仓库。

### 3. 设置目标芯片并编译烧录

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p COM端口 flash monitor
```

VSCode 中也可直接点击底部状态栏的 Build / Flash / Monitor 按钮。

---

## MQTT 数据格式

| 项目 | 值 |
|---|---|
| Broker | `mqtt://broker.emqx.io:1883` |
| 发布主题 | `aibot/esp32s3/status` |
| 上报周期 | 5 秒 |

```json
{
  "temp": 26.3,
  "hum": 55.4,
  "smoke": 0,
  "hat": 1,
  "pir": 0
}
```

| 字段 | 含义 |
|---|---|
| `temp` | 温度（℃） |
| `hum` | 相对湿度（%RH） |
| `smoke` | 1 = 检测到烟雾，0 = 正常 |
| `hat` | 1 = 检测到佩戴安全帽，0 = 未佩戴，-1 = K230 无数据 |
| `pir` | 1 = 检测到人体活动，0 = 无人 |

### K230 串口协议

K230 通过 UART 发送 ASCII 行：

```
HAT:1\n   → 已佩戴安全帽
HAT:0\n   → 未佩戴安全帽
```

---

## 已知限制

- **温湿度目前为模拟值**（`esp_random()` 生成），DHT22 驱动已单独验证通过，尚未接入主程序
- 无烟雾时绿灯不点亮（按需求设定为"灯灭 + 红灯报警"的双态指示）
- W5500 以太网、SD 卡、工厂 GPIO 测试功能已注释，未启用
- 人体模块缺失时 GPIO5 保持下拉读 0，不影响报警逻辑

---

## 许可证

比赛作品，代码仅供学习参考。
