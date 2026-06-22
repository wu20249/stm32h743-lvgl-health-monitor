/**
 * @file lv_port_disp_template.c
 *
 */

#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp_template.h"
#include "../../lvgl.h"
#include "./BSP/LCD/lcd.h"

/*********************
 *      DEFINES
 *********************/

/*
 * 最大屏幕宽度。
 * 你的工程当前按 800 宽申请缓冲，保留 800。
 * 如果以后确认屏幕最大宽度只有 480，可以改成 480。
 */
#define LVGL_DISP_MAX_WIDTH     800

/*
 * LVGL 刷新缓冲行数。
 * 5 行可以明显节省 RAM。
 */
#define LVGL_DISP_BUF_LINES     5

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);
static void disp_flush(lv_disp_drv_t *disp_drv,
                       const lv_area_t *area,
                       lv_color_t *color_p);

/**********************
 *  STATIC VARIABLES
 **********************/

/*
 * LVGL 显示缓冲。
 * 注意：这里按最大宽度申请，实际使用宽度由 lcddev.width 决定。
 */
static lv_disp_draw_buf_t draw_buf_dsc_1;
static lv_color_t buf_1[LVGL_DISP_MAX_WIDTH * LVGL_DISP_BUF_LINES];

/*
 * 显示驱动描述符。
 */
static lv_disp_drv_t disp_drv;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    uint32_t buf_size;

    /*
     * 初始化 LCD，并设置方向。
     * lcd_display_dir(1) 后，lcddev.width / lcddev.height 才是横屏尺寸。
     */
    disp_init();

    /*
     * 根据 LCD 实际宽度计算 LVGL 缓冲大小。
     * 例如：
     * 800 宽，5 行：800 * 5
     * 480 宽，5 行：480 * 5
     */
    buf_size = lcddev.width * LVGL_DISP_BUF_LINES;

    /*
     * 防止 lcddev.width 异常超过静态缓冲最大宽度。
     */
    if (lcddev.width > LVGL_DISP_MAX_WIDTH)
    {
        buf_size = LVGL_DISP_MAX_WIDTH * LVGL_DISP_BUF_LINES;
    }

    /*
     * 初始化 LVGL 显示缓冲。
     * 使用单缓冲，节省 RAM。
     */
    lv_disp_draw_buf_init(&draw_buf_dsc_1,
                          buf_1,
                          NULL,
                          buf_size);

    /*
     * 初始化 LVGL 显示驱动
     */
    lv_disp_drv_init(&disp_drv);

    /*
     * 使用 LCD 驱动识别到的真实宽高。
     * 不要写死 480x272 或 800x480。
     */
    disp_drv.hor_res = lcddev.width;
    disp_drv.ver_res = lcddev.height;

    /*
     * 刷新回调函数
     */
    disp_drv.flush_cb = disp_flush;

    /*
     * 设置显示缓冲
     */
    disp_drv.draw_buf = &draw_buf_dsc_1;

    /*
     * 注册显示驱动
     */
    lv_disp_drv_register(&disp_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief 初始化 LCD
 */
static void disp_init(void)
{
    /*
     * 如果 main.c 里已经调用过 lcd_init()，
     * 这里重复调用一般也可以。
     * 为了保证 LVGL 获取到正确的 lcddev.width / height，
     * 这里仍然保留。
     */
    lcd_init();

    /*
     * 设置横屏。
     * 这个函数会更新 lcddev.width / lcddev.height。
     */
    lcd_display_dir(1);
}

/**
 * @brief LVGL 刷新回调
 */
static void disp_flush(lv_disp_drv_t *disp_drv,
                       const lv_area_t *area,
                       lv_color_t *color_p)
{
    /*
     * LVGL 传入的 area 是需要刷新的区域。
     * lcd_color_fill 会把 color_p 中的颜色数据写到 LCD 对应区域。
     */
    lcd_color_fill((uint16_t)area->x1,
                   (uint16_t)area->y1,
                   (uint16_t)area->x2,
                   (uint16_t)area->y2,
                   (uint16_t *)color_p);

    /*
     * 通知 LVGL 刷新完成。
     * 如果以后改成 DMA / DMA2D 异步刷新，
     * 这一句要放到 DMA 完成中断里。
     */
    lv_disp_flush_ready(disp_drv);
}

#else

typedef int keep_pedantic_happy;

#endif