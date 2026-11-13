/*
 * lcd.h
 *
 *  Created on: 2023年12月22日
 *      Author: love
 */

#ifndef _LCD_H_
#define _LCD_H_

#include "driver/gpio.h"
#include <unistd.h>

/////////////////////////////////////用户配置区///////////////////////////////////
//支持横竖屏快速定义切换
#define USE_LCM_DIR  	  0   	//定义液晶屏顺时针旋转方向 	0-0度旋转，1-180度旋转，2-270度旋转，3-90度旋转
#define u32 unsigned int
#define u16 unsigned short int
#define u8 unsigned char

//-----------------LCD引脚定义----------------//
#define PIN_CS     GPIO_NUM_21
#define PIN_SDI    GPIO_NUM_17
#define PIN_SDI_RD GPIO_NUM_NC
#define PIN_SDO    GPIO_NUM_NC
#define PIN_RST    GPIO_NUM_15
#define PIN_SCK    GPIO_NUM_18
#define PIN_DC     GPIO_NUM_16
#define PIN_LED    GPIO_NUM_15

//--------------LCD引脚输入输出定义--------------//
#define SPI_CS(x)     gpio_set_level(PIN_CS,x)      //片选信号
#define SPI_SDI(x)    gpio_set_level(PIN_SDI,x)     //数据输入
#define SPI_SDI_RD()  gpio_get_level(PIN_SDI_RD)    //SDA单通道时候 作为数据输出
#define SPI_SDO(x)    gpio_set_level(PIN_SDO,x)     //数据输出
#define SPI_RST(x)    gpio_set_level(PIN_RST,x)     //复位
#define SPI_SCK(x)    gpio_set_level(PIN_SCK,x)     //时钟
#define SPI_DC(x)     gpio_set_level(PIN_DC,x)      //数据/命令
#define	LCD_LED(x)    gpio_set_level(PIN_LED,x)     //背光 开关

#define delay_ms(x) usleep(x * 1000)

//LCD重要参数集
typedef struct
{
	u16 width;			//LCD 宽度
	u16 height;			//LCD 高度
	u16 id;				//LCD ID
	u8  dir;			//横屏还是竖屏控制：0，竖屏；1，横屏。
	u16	wramcmd;		//开始写gram指令
	u16 setxcmd;		//设置x坐标指令
	u16  setycmd;		//设置y坐标指令
	u16 *farme_buff;
}_lcd_dev;

//LCD参数
extern _lcd_dev lcddev;	//管理LCD重要参数
//LCD的画笔颜色和背景色
extern u16  POINT_COLOR;//默认红色
extern u16  BACK_COLOR; //背景颜色.默认为白色

//画笔颜色
#define WHITE         	 0xFFFF
#define BLACK         	 0x0000
#define BLUE         	 0x001F
#define BRED             0XF81F
#define GRED 			 0XFFE0
#define GBLUE			 0X07FF
#define RED           	 0xF800
#define MAGENTA       	 0xF81F
#define GREEN         	 0x07E0
#define CYAN          	 0x7FFF
#define YELLOW        	 0xFFE0
#define BROWN 			 0XBC40 //棕色
#define BRRED 			 0XFC07 //棕红色
#define GRAY  			 0X8430 //灰色
//GUI颜色

#define DARKBLUE      	 0X01CF	//深蓝色
#define LIGHTBLUE      	 0X7D7C	//浅蓝色
#define GRAYBLUE       	 0X5458 //灰蓝色
//以上三色为PANEL的颜色

#define LIGHTGREEN     	 0X841F //浅绿色
#define LGRAY 					 0XC618 //浅灰色(PANNEL),窗体背景色

#define LGRAYBLUE        0XA651 //浅灰蓝色(中间层颜色)
#define LBBLUE           0X2B12 //浅棕蓝色(选择条目的反色)

void lcd_gpio_config(void);
void LCD_Init(void);													   	//初始化
void LCD_DisplayOn(void);													//开显示
void LCD_DisplayOff(void);													//关显示
void LCD_Clear(u16 Color);	 												//清屏
void LCD_DMA_Clear(u16 color);                                              //ESP32 - SPI - DMA 清屏
void LCD_SetCursor(u16 Xpos, u16 Ypos);										//设置光标
void LCD_DrawPoint(u16 x,u16 y);											//画点
void LCD_Fast_DrawPoint(u16 x,u16 y,u16 color);								//快速画点
void LCD_Draw_Circle(u16 x0,u16 y0,u8 r);						 			//画圆
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2);							//画线
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2);		   				//画矩形
void LCD_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u16 color);		   				//填充单色
void LCD_Color_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u16 *color);				//填充指定颜色
void LCD_ShowChar(u16 x,u16 y,u8 num,u8 size,u8 mode);						//显示一个字符
void LCD_ShowNum(u16 x,u16 y,u32 num,u8 len,u8 size);  						//显示一个数字
void LCD_ShowxNum(u16 x,u16 y,u32 num,u8 len,u8 size,u8 mode);				//显示 数字
void LCD_ShowString(u16 x,u16 y,u16 width,u16 height,u8 size,u8 *p);		//显示一个字符串,12/16字体

void LCD_WriteReg(u16 LCD_Reg, u16 LCD_RegValue);
void LCD_WriteRAM_Prepare(void);
void LCD_WriteRAM(u16 RGB_Code);
void LCD_WriteRAM_DMA(u16 *data, u16 len);                                  //ESP32 - SPI - DMA写DRAM
void Load_Drow_Dialog(void);
void LCD_Set_Window(u16 sx,u16 sy,u16 width,u16 height);	//设置窗口
void Show_Str(u16 x, u16 y,u8 *str,u8 size,u8 mode);//显示中文
void Gui_Drawbmp16(u16 x,u16 y,const unsigned char *p); //显示40*40 图片
void Gui_StrCenter(u16 x, u16 y,u8 *str,u8 size,u8 mode);//居中显示
void LCD_Display_Dir(u8 dir); //设置LCD显示方向
void lcd_draw_bline(u16 x1, u16 y1, u16 x2, u16 y2,u8 size,u16 color);//画一条粗线
void gui_fill_circle(u16 x0,u16 y0,u16 r,u16 color);//画实心圆
void gui_draw_hline(u16 x0,u16 y0,u16 len,u16 color);//画水平线 电容触摸屏专有部分
u16 LCD_ReadPoint(u16 x,u16 y);//读取指定坐标颜色
u16 LCD_Read_ID(u8 reg);//读ID

#endif /* COMPONENTS_LCD_INCLUDE_LCD_H_ */
