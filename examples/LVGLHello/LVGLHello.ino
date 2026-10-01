#include <LVGL.h>

static lv_obj_t *counter;

void setup() {
  Serial.begin(115200);
  if (!LVGL.begin()) {
    Serial.println("display not ready");
    return;
  }
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x102040), 0);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Hello LVGL on F101 + Arduino");
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

  counter = lv_label_create(scr);
  lv_obj_set_style_text_color(counter, lv_color_hex(0x80ff80), 0);
  lv_obj_set_style_text_font(counter, &lv_font_montserrat_24, 0);
  lv_obj_align(counter, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t *bar = lv_bar_create(scr);
  lv_obj_set_size(bar, 600, 30);
  lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -60);
  lv_bar_set_range(bar, 0, 100);
  lv_obj_set_user_data(bar, nullptr);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, bar);
  lv_anim_set_values(&a, 0, 100);
  lv_anim_set_duration(&a, 2000);
  lv_anim_set_playback_duration(&a, 2000);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_exec_cb(&a, [](void *o, int32_t v) { lv_bar_set_value((lv_obj_t *)o, v, LV_ANIM_OFF); });
  lv_anim_start(&a);
  Serial.println("lvgl ready");
}

void loop() {
  static unsigned long last;
  if (millis() - last >= 500) {
    last = millis();
    lv_label_set_text_fmt(counter, "uptime %lu ms", millis());
  }
  LVGL.handle();
}
