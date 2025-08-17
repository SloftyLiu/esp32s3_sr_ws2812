#pragma once


void app_led_strip_set_one_color(uint32_t red, uint32_t green, uint32_t blue);
void app_led_strip_blink_task_start(uint32_t red, uint32_t green, uint32_t blue);
void app_led_strip_breath_task_start(uint32_t red, uint32_t green, uint32_t blue);
void app_led_strip_color_breath_task_start(void);
void app_led_strip_rainbow_flow_task_start(void);
void app_led_strip_task_clear(void);

void app_led_strip_init(void);