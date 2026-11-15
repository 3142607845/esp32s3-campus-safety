#ifndef _APP_UART_H_
#define _APP_UART_H_

// 初始化 UART1（接收 K230 数据），波特率 115200
void app_uart_init(void);

// 读取 K230 发来的火焰检测结果
// 返回值：1 = 识别到火焰，0 = 无火焰，-1 = 尚未收到数据或链路超时
int app_uart_get_fire(void);

#endif
