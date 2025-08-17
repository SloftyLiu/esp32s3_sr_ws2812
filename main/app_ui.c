#include "app_ui.h"
#include "esp32_s3_szp.h"
#include "string.h"

static const char *TAG = "app_ui";
static lv_obj_t *text;

void app_ui_init(void)
{
    text = lv_label_create(lv_scr_act());
    lv_label_set_text(text, "Waiting for wake up ...");
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(text, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(text);
}


void app_ui_listening(void)
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x87CEEB), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(text, "I'm Listening ...");
}

void app_ui_waiting(void)
{
    //设置画面背景为白色
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(text, "Waiting for wake up ...");
}
