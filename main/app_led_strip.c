#include "esp32_s3_szp.h"
#include "led_strip.h"
#include "app_led_strip.h"

uint8_t led_ctrl = 0;

static led_strip_handle_t led_strip;
static const char *TAG = "app_led_strip";
#define LED_STRIP_GPIO 11
#define MAX_LEDS 100

typedef struct {
    uint32_t red;
    uint32_t green;
    uint32_t blue;
} strip_color_t;

static TaskHandle_t led_blink_task_handle = NULL;
static TaskHandle_t led_breath_task_handle = NULL;
static TaskHandle_t led_color_breath_task_handle = NULL;
static TaskHandle_t led_rainbow_flow_task_handle = NULL;

// 添加互斥锁保护SPI访问
static SemaphoreHandle_t led_strip_mutex = NULL;

// 使用全局变量存储颜色参数
static strip_color_t blink_color = {0};
static strip_color_t breath_color = {0};

// 任务退出标志
static volatile bool blink_task_should_exit = false;
static volatile bool breath_task_should_exit = false;
static volatile bool color_breath_task_should_exit = false;
static volatile bool rainbow_flow_task_should_exit = false;

//led strip闪烁任务
static void led_strip_blink_task(void *arg)
{    
    uint8_t led_blink_state = 1;
    ESP_LOGI(TAG, "LED blink task start");
    
    while (!blink_task_should_exit) {
        // 使用互斥锁保护LED操作
        if (xSemaphoreTake(led_strip_mutex, portMAX_DELAY) == pdTRUE) {
            if (led_blink_state) {
                app_led_strip_set_one_color(blink_color.red, blink_color.green, blink_color.blue);
            } else {
                led_strip_clear(led_strip);
            }
            xSemaphoreGive(led_strip_mutex);
        }
        led_blink_state = !led_blink_state;
        
        // 检查退出标志，使用较短的延时便于及时响应
        for (int i = 0; i < 10 && !blink_task_should_exit; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    ESP_LOGI(TAG, "LED blink task exit");
    led_blink_task_handle = NULL;
    vTaskDelete(NULL); // 删除自己
}

//led strip呼吸任务
static void led_strip_breath_task(void *arg)
{
    float led_power_state = 0.02;
    float step = 0.01; // 每次增减的步长
    bool increasing = true; // 当前是否在增长
    ESP_LOGI(TAG, "LED breath task start");
    
    while (!breath_task_should_exit) {
        // led_power_state从0.02开始增长，到达1后开始减小，达到0.02后重新开始增加
        if (increasing) {
            led_power_state += step;
            if (led_power_state >= 1.0) {
                led_power_state = 1.0;
                increasing = false; // 开始减小
            }
        } else {
            led_power_state -= step;
            if (led_power_state <= 0.02) {
                led_power_state = 0.02;
                increasing = true; // 开始增长
            }
        }

        // 使用互斥锁保护LED操作
        if (xSemaphoreTake(led_strip_mutex, portMAX_DELAY) == pdTRUE) {
            app_led_strip_set_one_color(breath_color.red * led_power_state, 
                                       breath_color.green * led_power_state, 
                                       breath_color.blue * led_power_state);
            xSemaphoreGive(led_strip_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(16));
    }
    
    ESP_LOGI(TAG, "LED breath task exit");
    led_breath_task_handle = NULL;
    vTaskDelete(NULL); // 删除自己
}

// HSV转RGB的辅助函数
static void hsv_to_rgb(float h, float s, float v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    float c = v * s;
    float x = c * (1 - fabs(fmod(h / 60.0, 2) - 1));
    float m = v - c;
    
    float r_f, g_f, b_f;
    
    if (h < 60) {
        r_f = c; g_f = x; b_f = 0;
    } else if (h < 120) {
        r_f = x; g_f = c; b_f = 0;
    } else if (h < 180) {
        r_f = 0; g_f = c; b_f = x;
    } else if (h < 240) {
        r_f = 0; g_f = x; b_f = c;
    } else if (h < 300) {
        r_f = x; g_f = 0; b_f = c;
    } else {
        r_f = c; g_f = 0; b_f = x;
    }
    
    *r = (uint32_t)((r_f + m) * 255);
    *g = (uint32_t)((g_f + m) * 255);
    *b = (uint32_t)((b_f + m) * 255);
}

//led strip彩色呼吸任务
static void led_strip_color_breath_task(void *arg)
{
    float hue = 0; // 色调，0-360度
    float hue_step = 0.5; // 色调变化步长
    
    ESP_LOGI(TAG, "LED color breath task start");
    
    while (!color_breath_task_should_exit) {
        
        // 色调循环变化
        hue += hue_step;
        if (hue >= 360) {
            hue = 0;
        }
        
        // 转换HSV到RGB
        uint32_t red, green, blue;
        hsv_to_rgb(hue, 1.0, 0.8, &red, &green, &blue);
        
        // 使用互斥锁保护LED操作
        if (xSemaphoreTake(led_strip_mutex, portMAX_DELAY) == pdTRUE) {
            app_led_strip_set_one_color(red, green, blue);
            xSemaphoreGive(led_strip_mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(16)); // 控制更新频率
    }
    
    ESP_LOGI(TAG, "LED color breath task exit");
    led_color_breath_task_handle = NULL;
    vTaskDelete(NULL);
}

//led strip彩色流光任务
static void led_strip_rainbow_flow_task(void *arg)
{
    float hue_offset = 0; // 色调偏移量
    float hue_step = 1.0; // 色调移动步长
    float hue_density = 360.0 / MAX_LEDS; // 每个LED间的色调间隔
    
    ESP_LOGI(TAG, "LED rainbow flow task start");
    
    while (!rainbow_flow_task_should_exit) {
        // 使用互斥锁保护LED操作
        if (xSemaphoreTake(led_strip_mutex, portMAX_DELAY) == pdTRUE) {
            // 为每个LED设置不同的颜色
            for (int i = 0; i < MAX_LEDS; i++) {
                // 计算当前LED的色调
                float current_hue = hue_offset + (i * hue_density);
                
                // 确保色调在0-360范围内
                while (current_hue >= 360) {
                    current_hue -= 360;
                }
                while (current_hue < 0) {
                    current_hue += 360;
                }
                
                // 转换HSV到RGB (饱和度100%, 亮度80%)
                uint32_t red, green, blue;
                hsv_to_rgb(current_hue, 1.0, 0.8, &red, &green, &blue);
                
                // 设置LED颜色
                led_strip_set_pixel(led_strip, i, red, green, blue);
            }
            
            // 刷新LED显示
            led_strip_refresh(led_strip);
            xSemaphoreGive(led_strip_mutex);
        }
        
        // 移动色调偏移，产生流动效果
        hue_offset += hue_step;
        if (hue_offset >= 360) {
            hue_offset = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(16)); // 控制流动速度
    }
    
    ESP_LOGI(TAG, "LED rainbow flow task exit");
    led_rainbow_flow_task_handle = NULL;
    vTaskDelete(NULL);
}


void app_led_strip_set_one_color(uint32_t red, uint32_t green, uint32_t blue)
{
    /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
    for (int i = 0; i < MAX_LEDS; i++) {
        led_strip_set_pixel(led_strip, i, red, green, blue);
    }

    /* Refresh the strip to send data */
    led_strip_refresh(led_strip);
}

//创建led_strip_blink_task任务
void app_led_strip_blink_task_start(uint32_t red, uint32_t green, uint32_t blue)
{    
    // 设置颜色参数
    blink_color.red = red;
    blink_color.green = green;
    blink_color.blue = blue;
    
    // 重置退出标志
    blink_task_should_exit = false;

    xTaskCreate(led_strip_blink_task, "led_strip_blink_task", 4096, NULL, 5, &led_blink_task_handle);
}

//创建led_strip_breath_task任务
void app_led_strip_breath_task_start(uint32_t red, uint32_t green, uint32_t blue)
{
    // 设置颜色参数
    breath_color.red = red;
    breath_color.green = green;
    breath_color.blue = blue;
    
    // 重置退出标志
    breath_task_should_exit = false;

    xTaskCreate(led_strip_breath_task, "led_strip_breath_task", 4096, NULL, 5, &led_breath_task_handle);
}

//创建led_strip_color_breath_task任务
void app_led_strip_color_breath_task_start(void)
{
    // 重置退出标志
    color_breath_task_should_exit = false;

    xTaskCreate(led_strip_color_breath_task, "led_strip_color_breath_task", 4096, NULL, 5, &led_color_breath_task_handle);
}

//创建led_strip_rainbow_flow_task任务
void app_led_strip_rainbow_flow_task_start(void)
{
    // 重置退出标志
    rainbow_flow_task_should_exit = false;

    xTaskCreate(led_strip_rainbow_flow_task, "led_strip_rainbow_flow_task", 4096, NULL, 5, &led_rainbow_flow_task_handle);
}

//删除已经存在的任务
void app_led_strip_task_clear(void)
{
    // 设置退出标志，让任务自己退出
    if (led_blink_task_handle != NULL) {
        ESP_LOGI(TAG, "Stopping blink task...");
        blink_task_should_exit = true;
        
        // 等待任务自己退出
        while (led_blink_task_handle != NULL) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        ESP_LOGI(TAG, "Blink task stopped");
    }
    
    if (led_breath_task_handle != NULL) {
        ESP_LOGI(TAG, "Stopping breath task...");
        breath_task_should_exit = true;
        
        // 等待任务自己退出
        while (led_breath_task_handle != NULL) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        ESP_LOGI(TAG, "Breath task stopped");
    }
    
    if (led_color_breath_task_handle != NULL) {
        ESP_LOGI(TAG, "Stopping color breath task...");
        color_breath_task_should_exit = true;
        
        // 等待任务自己退出
        while (led_color_breath_task_handle != NULL) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        ESP_LOGI(TAG, "Color breath task stopped");
    }
    
    if (led_rainbow_flow_task_handle != NULL) {
        ESP_LOGI(TAG, "Stopping rainbow flow task...");
        rainbow_flow_task_should_exit = true;
        
        // 等待任务自己退出
        while (led_rainbow_flow_task_handle != NULL) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        ESP_LOGI(TAG, "Rainbow flow task stopped");
    }
    
    // 清除LED（使用互斥锁保护）
    if (led_strip_mutex != NULL && xSemaphoreTake(led_strip_mutex, portMAX_DELAY) == pdTRUE) {
        led_strip_clear(led_strip);
        xSemaphoreGive(led_strip_mutex);
        ESP_LOGI(TAG, "LED strip clear");
    }
}
void app_led_strip_init(void)
{
    ESP_LOGI(TAG, "LED strip initialization start");
    
    // 创建互斥锁
    led_strip_mutex = xSemaphoreCreateMutex();
    if (led_strip_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create LED strip mutex");
        return;
    }
    
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = MAX_LEDS, // at least one LED on board
    };

    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));

    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);
    
    ESP_LOGI(TAG, "LED strip initialization complete");
}