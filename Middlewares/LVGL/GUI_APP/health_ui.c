#include "health_ui.h"
#include "app_data.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdint.h>

/* =========================================================
 *  原始设计尺寸：480 x 272
 *  实际显示尺寸：运行时从 LVGL 获取
 * ========================================================= */

#define UI_BASE_W    480
#define UI_BASE_H    272

/*
 * 统一字体：
 * 只使用 lv_font_montserrat_14，避免多个字体导致 Flash/RAM 占用过大。
 *
 * lv_conf.h 中必须开启：
 * #define LV_FONT_MONTSERRAT_14 1
 */
#define UI_FONT    (&lv_font_montserrat_14)

/* ===================== 屏幕尺寸 ===================== */

static lv_coord_t ui_scr_w = UI_BASE_W;
static lv_coord_t ui_scr_h = UI_BASE_H;

/**
 * @brief X方向按屏幕宽度缩放
 */
static lv_coord_t ui_sx(int v)
{
    return (lv_coord_t)((int32_t)v * ui_scr_w / UI_BASE_W);
}

/**
 * @brief Y方向按屏幕高度缩放
 */
static lv_coord_t ui_sy(int v)
{
    return (lv_coord_t)((int32_t)v * ui_scr_h / UI_BASE_H);
}

/* ===================== UI对象 ===================== */

static lv_obj_t *label_temp_value;
static lv_obj_t *label_humi_value;
static lv_obj_t *label_hr_value;
static lv_obj_t *label_spo2_value;
static lv_obj_t *label_posture_value;

static lv_obj_t *label_wifi_value;
static lv_obj_t *label_mqtt_value;
static lv_obj_t *label_upload_value;

static lv_obj_t *card_posture;

static lv_obj_t *chart_vital;
static lv_chart_series_t *series_hr;
static lv_chart_series_t *series_spo2;


/**
 * @brief 创建一个圆角卡片
 */
static lv_obj_t *ui_create_card(lv_obj_t *parent,
                                lv_coord_t x,
                                lv_coord_t y,
                                lv_coord_t w,
                                lv_coord_t h,
                                lv_color_t bg_color)
{
    lv_obj_t *card;

    card = lv_obj_create(parent);

    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);

    lv_obj_set_style_radius(card, ui_sy(8), 0);
    lv_obj_set_style_bg_color(card, bg_color, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);

    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2E6FA3), 0);

    lv_obj_set_style_shadow_width(card, ui_sy(6), 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x001A33), 0);

    lv_obj_set_style_pad_all(card, ui_sy(5), 0);

    /*
     * 禁止卡片滚动，避免小卡片出现滚动条
     */
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    return card;
}


/**
 * @brief 在卡片中创建标题和值
 */
static void ui_card_add_text(lv_obj_t *card,
                             const char *title,
                             lv_obj_t **value_label)
{
    lv_obj_t *label_title;

    label_title = lv_label_create(card);
    lv_label_set_text(label_title, title);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0xB8DFFF), 0);
    lv_obj_set_style_text_font(label_title, UI_FONT, 0);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, -ui_sy(2));

    *value_label = lv_label_create(card);
    lv_label_set_text(*value_label, "--");
    lv_obj_set_style_text_color(*value_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(*value_label, UI_FONT, 0);
    lv_obj_align(*value_label, LV_ALIGN_CENTER, 0, ui_sy(10));
}


/**
 * @brief 创建顶部标题栏
 */
static void ui_create_top_bar(lv_obj_t *scr)
{
    lv_obj_t *bar;
    lv_obj_t *title;
    lv_obj_t *time_label;

    bar = lv_obj_create(scr);
    lv_obj_set_size(bar, ui_scr_w, ui_sy(36));
    lv_obj_set_pos(bar, 0, 0);

    lv_obj_set_style_bg_color(bar, lv_color_hex(0x06284A), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    title = lv_label_create(bar);
    lv_label_set_text(title, "Elder Health Monitor");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, UI_FONT, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    time_label = lv_label_create(bar);
    lv_label_set_text(time_label, "12:30");
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xB8DFFF), 0);
    lv_obj_set_style_text_font(time_label, UI_FONT, 0);
    lv_obj_align(time_label, LV_ALIGN_RIGHT_MID, -ui_sx(10), 0);
}


/**
 * @brief 创建顶部五个数据卡片
 */
static void ui_create_data_cards(lv_obj_t *scr)
{
    lv_obj_t *card_temp;
    lv_obj_t *card_humi;
    lv_obj_t *card_hr;
    lv_obj_t *card_spo2;

    card_temp = ui_create_card(scr,
                               ui_sx(4),
                               ui_sy(38),
                               ui_sx(88),
                               ui_sy(72),
                               lv_color_hex(0x064C8C));
    ui_card_add_text(card_temp, "Temp", &label_temp_value);

    card_humi = ui_create_card(scr,
                               ui_sx(97),
                               ui_sy(38),
                               ui_sx(88),
                               ui_sy(72),
                               lv_color_hex(0x006D75));
    ui_card_add_text(card_humi, "Humi", &label_humi_value);

    card_hr = ui_create_card(scr,
                             ui_sx(190),
                             ui_sy(38),
                             ui_sx(88),
                             ui_sy(72),
                             lv_color_hex(0x9B2F42));
    ui_card_add_text(card_hr, "Heart", &label_hr_value);

    card_spo2 = ui_create_card(scr,
                               ui_sx(283),
                               ui_sy(38),
                               ui_sx(88),
                               ui_sy(72),
                               lv_color_hex(0x4B3A91));
    ui_card_add_text(card_spo2, "SpO2", &label_spo2_value);

    card_posture = ui_create_card(scr,
                                  ui_sx(376),
                                  ui_sy(38),
                                  ui_sx(92),
                                  ui_sy(72),
                                  lv_color_hex(0x0A7A3D));
    ui_card_add_text(card_posture, "Posture", &label_posture_value);

    /*
     * NORMAL 字符较长，仍然使用统一14号字体
     */
    lv_obj_set_style_text_font(label_posture_value, UI_FONT, 0);
    lv_obj_align(label_posture_value, LV_ALIGN_CENTER, 0, ui_sy(10));
}


/**
 * @brief 创建心率 + 血氧双曲线趋势图
 */
static void ui_create_chart(lv_obj_t *scr)
{
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *legend_hr;
    lv_obj_t *legend_spo2;

    panel = ui_create_card(scr,
                           ui_sx(4),
                           ui_sy(118),
                           ui_sx(292),
                           ui_sy(106),
                           lv_color_hex(0x08233F));

    title = lv_label_create(panel);
    lv_label_set_text(title, "HR & SpO2 Trend");
    lv_obj_set_style_text_color(title, lv_color_hex(0xB8DFFF), 0);
    lv_obj_set_style_text_font(title, UI_FONT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, ui_sx(4), -ui_sy(2));

    /*
     * 图例：HR
     */
    legend_hr = lv_label_create(panel);
    lv_label_set_text(legend_hr, "HR");
    lv_obj_set_style_text_color(legend_hr, lv_color_hex(0xFF5A5A), 0);
    lv_obj_set_style_text_font(legend_hr, UI_FONT, 0);
    lv_obj_align(legend_hr, LV_ALIGN_TOP_RIGHT, -ui_sx(52), -ui_sy(2));

    /*
     * 图例：SpO2
     */
    legend_spo2 = lv_label_create(panel);
    lv_label_set_text(legend_spo2, "SpO2");
    lv_obj_set_style_text_color(legend_spo2, lv_color_hex(0x00FF99), 0);
    lv_obj_set_style_text_font(legend_spo2, UI_FONT, 0);
    lv_obj_align(legend_spo2, LV_ALIGN_TOP_RIGHT, -ui_sx(8), -ui_sy(2));

    chart_vital = lv_chart_create(panel);
    lv_obj_set_size(chart_vital, ui_sx(272), ui_sy(76));
    lv_obj_align(chart_vital, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_chart_set_type(chart_vital, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_vital, 20);

    /*
     * 主Y轴用于 HR：50 ~ 120 bpm
     */
    lv_chart_set_range(chart_vital, LV_CHART_AXIS_PRIMARY_Y, 50, 120);

    /*
     * 次Y轴用于 SpO2：90 ~ 100 %
     */
    lv_chart_set_range(chart_vital, LV_CHART_AXIS_SECONDARY_Y, 90, 100);

    /*
     * 网格线
     */
    lv_chart_set_div_line_count(chart_vital, 4, 6);

    lv_obj_set_style_bg_color(chart_vital, lv_color_hex(0x061A30), 0);
    lv_obj_set_style_bg_opa(chart_vital, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chart_vital, 0, 0);

    lv_obj_set_style_line_width(chart_vital, 2, LV_PART_ITEMS);

    /*
     * HR 曲线：红色，使用主Y轴
     */
    series_hr = lv_chart_add_series(chart_vital,
                                    lv_color_hex(0xFF5A5A),
                                    LV_CHART_AXIS_PRIMARY_Y);

    /*
     * SpO2 曲线：绿色，使用次Y轴
     */
    series_spo2 = lv_chart_add_series(chart_vital,
                                      lv_color_hex(0x00FF99),
                                      LV_CHART_AXIS_SECONDARY_Y);
}


/**
 * @brief 创建系统信息区域
 */
static void ui_create_system_info(lv_obj_t *scr)
{
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *label;

    panel = ui_create_card(scr,
                           ui_sx(304),
                           ui_sy(118),
                           ui_sx(164),
                           ui_sy(106),
                           lv_color_hex(0x08233F));

    title = lv_label_create(panel);
    lv_label_set_text(title, "System Info");
    lv_obj_set_style_text_color(title, lv_color_hex(0xB8DFFF), 0);
    lv_obj_set_style_text_font(title, UI_FONT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, ui_sx(4), -ui_sy(2));

    label = lv_label_create(panel);
    lv_label_set_text(label, "WiFi:");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, UI_FONT, 0);
    lv_obj_set_pos(label, ui_sx(8), ui_sy(28));

    label_wifi_value = lv_label_create(panel);
    lv_label_set_text(label_wifi_value, "Connected");
    lv_obj_set_style_text_color(label_wifi_value, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_text_font(label_wifi_value, UI_FONT, 0);
    lv_obj_set_pos(label_wifi_value, ui_sx(72), ui_sy(28));

    label = lv_label_create(panel);
    lv_label_set_text(label, "MQTT:");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, UI_FONT, 0);
    lv_obj_set_pos(label, ui_sx(8), ui_sy(52));

    label_mqtt_value = lv_label_create(panel);
    lv_label_set_text(label_mqtt_value, "Connected");
    lv_obj_set_style_text_color(label_mqtt_value, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_text_font(label_mqtt_value, UI_FONT, 0);
    lv_obj_set_pos(label_mqtt_value, ui_sx(72), ui_sy(52));

    label = lv_label_create(panel);
    lv_label_set_text(label, "Upload:");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, UI_FONT, 0);
    lv_obj_set_pos(label, ui_sx(8), ui_sy(76));

    label_upload_value = lv_label_create(panel);
    lv_label_set_text(label_upload_value, "OK");
    lv_obj_set_style_text_color(label_upload_value, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_text_font(label_upload_value, UI_FONT, 0);
    lv_obj_set_pos(label_upload_value, ui_sx(72), ui_sy(76));
}


/**
 * @brief 创建底部导航栏
 */
static void ui_create_bottom_bar(lv_obj_t *scr)
{
    lv_obj_t *bar;
    lv_obj_t *label;

    bar = lv_obj_create(scr);
    lv_obj_set_size(bar, ui_scr_w, ui_sy(38));
    lv_obj_set_pos(bar, 0, ui_scr_h - ui_sy(38));

    lv_obj_set_style_bg_color(bar, lv_color_hex(0x06284A), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    label = lv_label_create(bar);
    lv_label_set_text(label, "Real-time Data   |   Trend   |   Alarm   |   Setting");
    lv_obj_set_style_text_color(label, lv_color_hex(0xB8DFFF), 0);
    lv_obj_set_style_text_font(label, UI_FONT, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}


/**
 * @brief 刷新真实数据
 */
static void ui_update_real_data(lv_timer_t *timer)
{
    char buf[32];

    LV_UNUSED(timer);

    /*
     * 温度
     */
    snprintf(buf, sizeof(buf), "%d C", g_temperature);
    lv_label_set_text(label_temp_value, buf);

    /*
     * 湿度
     */
    snprintf(buf, sizeof(buf), "%d%%", g_humidity);
    lv_label_set_text(label_humi_value, buf);

    /*
     * 心率
     */
    if (g_max30102_ok)
    {
        snprintf(buf, sizeof(buf), "%d", g_heart_rate);
        lv_label_set_text(label_hr_value, buf);
    }
    else
    {
        lv_label_set_text(label_hr_value, "--");
    }

    /*
     * 血氧
     */
    if (g_max30102_ok)
    {
        snprintf(buf, sizeof(buf), "%d%%", g_spo2);
        lv_label_set_text(label_spo2_value, buf);
    }
    else
    {
        lv_label_set_text(label_spo2_value, "--");
    }

    /*
     * 姿态状态
     */
    if (g_posture_status == POSTURE_FALL)
    {
        lv_label_set_text(label_posture_value, "FALL");
        lv_obj_set_style_bg_color(card_posture, lv_color_hex(0x8A1E1E), 0);
    }
    else
    {
        lv_label_set_text(label_posture_value, "NORMAL");
        lv_obj_set_style_bg_color(card_posture, lv_color_hex(0x0A7A3D), 0);
    }

    /*
     * WiFi状态
     */
    if (g_wifi_ok)
    {
        lv_label_set_text(label_wifi_value, "Connected");
        lv_obj_set_style_text_color(label_wifi_value, lv_color_hex(0x00FF66), 0);
    }
    else
    {
        lv_label_set_text(label_wifi_value, "Offline");
        lv_obj_set_style_text_color(label_wifi_value, lv_color_hex(0xFF5555), 0);
    }

    /*
     * MQTT状态
     */
    if (g_mqtt_ok)
    {
        lv_label_set_text(label_mqtt_value, "Connected");
        lv_obj_set_style_text_color(label_mqtt_value, lv_color_hex(0x00FF66), 0);
    }
    else
    {
        lv_label_set_text(label_mqtt_value, "Offline");
        lv_obj_set_style_text_color(label_mqtt_value, lv_color_hex(0xFF5555), 0);
    }

    /*
     * 上传状态
     */
    if (g_upload_ok)
    {
        lv_label_set_text(label_upload_value, "OK");
        lv_obj_set_style_text_color(label_upload_value, lv_color_hex(0x00FF66), 0);
    }
    else
    {
        lv_label_set_text(label_upload_value, "--");
        lv_obj_set_style_text_color(label_upload_value, lv_color_hex(0xB8DFFF), 0);
    }

    /*
     * 趋势曲线
     */
    if (g_max30102_ok)
    {
        lv_chart_set_next_value(chart_vital, series_hr, g_heart_rate);
        lv_chart_set_next_value(chart_vital, series_spo2, g_spo2);
    }
}


/**
 * @brief 创建健康监测主界面
 */
void health_ui_create(void)
{
    lv_obj_t *scr;
    lv_disp_t *disp;

    disp = lv_disp_get_default();

    if (disp != NULL)
    {
        ui_scr_w = lv_disp_get_hor_res(disp);
        ui_scr_h = lv_disp_get_ver_res(disp);
    }

    if (ui_scr_w <= 0)
    {
        ui_scr_w = UI_BASE_W;
    }

    if (ui_scr_h <= 0)
    {
        ui_scr_h = UI_BASE_H;
    }

    scr = lv_scr_act();

    /*
     * 清空当前屏幕，避免残留正点原子原来的 demo 控件
     */
    lv_obj_clean(scr);

    /*
     * 设置屏幕大小和背景
     */
    lv_obj_set_size(scr, ui_scr_w, ui_scr_h);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x031426), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    ui_create_top_bar(scr);
    ui_create_data_cards(scr);
    ui_create_chart(scr);
    ui_create_system_info(scr);
    ui_create_bottom_bar(scr);

    ui_update_real_data(NULL);
    lv_timer_create(ui_update_real_data, 1000, NULL);
}