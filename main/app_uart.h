#ifndef _APP_UART_H_
#define _APP_UART_H_

// 初始化 UART1（接收 K230 数据），波特率 115200
void app_uart_init(void);

// 读取 K230 发来的安全帽状态
// 返回值：1 = 检测到戴安全帽，0 = 未检测到，-1 = 暂无数据
int app_uart_get_hat(void);

#endif
