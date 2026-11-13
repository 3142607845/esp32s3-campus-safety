/*
 * esp_spi_lcd.c
 *
 *  Created on: 2024年1月7日
 *      Author: love
 */
#include "esp_spi_lcd.h"
#include <malloc.h>
#include "esp_heap_caps.h"


esp_err_t ret;
spi_device_handle_t spi;

// PIN_CS | PIN_SDI | PIN_SDI_RD | PIN_SDO | PIN_RST | PIN_SCK | PIN_DC | PIN_LED

spi_bus_config_t buscfg={

    .miso_io_num = -1,         //主入从出，这个引脚我们没有配置
    .mosi_io_num = PIN_SDI,    //主出从入，我们使用的是GPIO17
    .sclk_io_num = PIN_SCK,    //主机时钟，我们使用的是GPIO18
    .quadhd_io_num = -1,       // HD引脚不设置，这个引脚配置Quad SPI的时候才有用
    .quadwp_io_num = -1,       // WP引脚不设置，这个引脚配置Quad SPI的时候才有用

    .max_transfer_sz = 128 * 80 * 16,
    //设置传输数据的最大值。非DMA最大64bytes,DMA最大4096bytes
    //buscfg.intr_flags = 0;  //这个用于设置SPI通讯中相关的中断函数的中断优先级，0是默认。
    //这组中断函数包括SPI通讯前中断和SPI通讯后中断两个函数。

    .flags = SPICOMMON_BUSFLAG_MASTER, // @suppress("Symbol is not resolved")
    //这个用于设置初始化的时候要检测哪些选项。比如这里设置的是spi初始化为主机模式是否成功。
    //检测结果通过spi_bus_initialize函数的
    //返回值进行返回。如果初始化为主机模式成功，就会返回esp_ok
};

spi_device_interface_config_t devcfg={
        .address_bits = 0,
        //1.如果设置为0，在通讯的时候就不会发送地址位。
        //2.如果设置了非零值，就会在spi通讯的地址发送阶段发送指定长度的address数据。
        //如果设置了非零值并且在后面数据发送结构体中没有定义addr的值，会默认发送指定长度0值
        //3.我们后面发送数据会使用到spi_transaction_t结构体，这个结构体会使用spi_device_interface_config_t中定义好address、command和dummy的长度
        //如果想使用非固定长度，就要使用spi_transaction_ext_t结构体了。这个结构体包括了四个部分，包含了一个spi_transaction_t和address、command、dummy的长度。
        //我们要做的就是在spi_transaction_ext_t.base.flags中设置SPI_TRANS_VARIABLE_ADDR/CMD/DUMMY
        //然后定义好这三部分数据的长度，然后用spi_transaction_ext_t.base的指针代替spi_transaction_t的指针即可
        .command_bits = 0,
        //与address_bits是一样的
        .dummy_bits = 0,
        //这里的配置方法与address_bits是一样的。但是要着重说一下这个配置的意义，后面会再说一遍
        //1.dummy_bits是用来用来补偿输入延迟。
        //在read phase开始阶段之前被插入进去。在dummy_bits的时钟下，并不进行数据读取的工作
        //相当于这段时间发送的clock都是虚拟的时钟，并没有功能。在输入延迟最大允许时间不够的时候，可以通过这种方法进行配置，从而
        //能够使得系统工作在更高的时钟频率下。
        //3.如果主机设备只进行write操作，可以在flags中设置SPI_DEVICE_NO_DUMMY，关闭dummy bits的发送。只有写操作的话，即使使用了gpio交换矩阵,时钟周期也可以工作在80MHZ

        .input_delay_ns = 0,
        //时钟发出信号到miso进行输入直接会有延迟，这个参数就是配置这个允许的最大延迟时间。
        //如果主机接收到从机时钟，但是超过这个时间没有收到miso发来的输入信号，就会返回通讯失败。
        //这个时间即使设置为0，也能正常工作，但是最好通过手册或逻辑分析仪进行估算。能够实现更好的通讯。
        //超过8M的通讯都应该认真设置这个数字
        .clock_speed_hz = SPI_MASTER_FREQ_40M, // @suppress("Symbol is not resolved")
        //配置通讯的时钟频率。
        //这个频率受到io_mux和input_delay_ns限制。
        //如果是io直连的，时钟上限是80MHZ，如果是gpio交换矩阵连接进来的,时钟上限是40MHZ。
        //如果是全双工，时钟上限是26MHZ。并且还要考虑输入延时。在相同输入延时的条件下，使用gpio交换矩阵会比使用io mux最大允许的时钟频率小。可以通过
        //spi_get_freq_limit()来计算能够允许的最大时钟频率是多少
        //有关SPI通讯时钟极限和配置的问题，后面会详细说一下。
        .mode = 0, //设置SPI通讯的相位特性和采样边沿。包括了mode0-3四种。要看从设备能够使用哪种模式
        .spics_io_num = -1, //配置片选线
        .duty_cycle_pos = 0,
        //设置时钟的占空比，比例是 pos*1/256,默认为0，也就是50%占空比
        //.cs_ena_pretrans; //在传输之前，片选线应该保持激活状态多少个时钟，只有全双工的时候才需要配置
        //.cs_ena_posttrans; //在传输之后，片选线应该保持激活状态多少个时钟，只有全双工的时候才需要配置
        .queue_size = 6, //传输队列的长度，表示可以在通讯的时候挂起多少个spi通讯。在中断通讯模式的时候会把当前spi通讯进程挂起到队列中
        //.flags, //配置与从机有关的一些参数，比如MSB还是LSB，使不使用三线SPI
        //.pre_cb,
        //配置通讯前中断。比如不在这里配置cs片选线，把片选线作为自行控制的线，把片选线拉低放在通讯前中断中

        //.post_cb,
        //配置通讯后中断。比如不在这里配置cs片选线，把片选线作为自行控制的线，把片选线拉高放在通讯前中断中
};

void lcd_spi_init(void)
{
    ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK)
    {
        printf("bus initialize failed!\n");
    }
    else
    {
        printf("bus initialize successes!\n");
    }
    ret = spi_bus_add_device(SPI3_HOST, &devcfg, &spi);
    if (ret != ESP_OK)
    {
        printf("device config error\n");
    }
    else
    {
        printf("device config success\n");
    }

}

esp_err_t spi_write(spi_device_handle_t spi, uint8_t *data, uint16_t len)
{
    esp_err_t ret;
    spi_transaction_t t;
    if (len==0) return 0;             //no need to send anything
    memset(&t, 0, sizeof(t));       //Zero out the transaction


    t.length=len*8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer=data;               //Data
    t.user=(void*)1;                //D/C needs to be set to 1
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.

    return ret;
}

esp_err_t spi_read(spi_device_handle_t spi, uint8_t *data)
{
    spi_transaction_t t;

    memset(&t, 0, sizeof(t));
    t.length=8;
    t.flags = SPI_TRANS_USE_RXDATA; // @suppress("Symbol is not resolved")
    t.rx_buffer = data;
    t.user = (void*)1;
    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    assert( ret == ESP_OK );


    return ret;
}

void* spi_memory_malloc(int size)
{
	return heap_caps_malloc(size, MALLOC_CAP_DMA);
}
void spi_memory_free(void *ptr)
{
	heap_caps_free(ptr);
}
