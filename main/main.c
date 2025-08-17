#include <stdio.h>
#include "esp32_s3_szp.h"
#include "app_ui.h"
#include "app_sr.h"
#include "app_led_strip.h"


void app_main(void)
{
    app_led_strip_init();
    bsp_i2c_init();  // I2C初始化
    pca9557_init();  // IO扩展芯片初始化
    bsp_lvgl_start(); // 初始化液晶屏lvgl接口
    app_ui_init();    // 初始化UI界面
    bsp_codec_init(); // 音频初始化
    app_sr_init();  // 语音识别初始化   
}
