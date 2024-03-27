#if SK_DISPLAY
#include "display_task.h"
#include "semaphore_guard.h"
#include "util.h"

#include "font/roboto_light_60.h"

// 太空人gif
#include "tkr/img1.h"
#include "tkr/img2.h"
#include "tkr/img3.h"
#include "tkr/img4.h"
#include "tkr/img5.h"
#include "tkr/img6.h"
#include "tkr/img7.h"
#include "tkr/img9.h"
#include "tkr/img11.h"
#include "tkr/img13.h"
#include "tkr/img14.h"
#include "tkr/img16.h"
#include "tkr/img18.h"
#include "tkr/img20.h"
#include "tkr/img22.h"
#include "tkr/img24.h"

#include "icon/Volume.h"
#include "icon/MusicBox.h"
#include "icon/Spin.h"
#include "icon/EspNow.h"
#include "icon/myCat.h"
#include "icon/MusicBoxHome.h"
#include "icon/SpinHome.h"

static const uint8_t LEDC_CHANNEL_LCD_BACKLIGHT = 0;

DisplayTask::DisplayTask(const uint8_t task_core) : Task{"Display", 2048 * 2, 1, task_core}
{
  knob_state_queue_ = xQueueCreate(1, sizeof(PB_SmartKnobState));
  assert(knob_state_queue_ != NULL);

  mutex_ = xSemaphoreCreateMutex();
  assert(mutex_ != NULL);
}

DisplayTask::~DisplayTask()
{
  vQueueDelete(knob_state_queue_);
  vSemaphoreDelete(mutex_);
}

static void drawPlayButton(TFT_eSprite &spr, int x, int y, int width, int height, uint16_t color)
{
  spr.fillTriangle(
      x, y - height / 2,
      x, y + height / 2,
      x + width, y,
      color);
}

int delat_time = 30;
int animation_index = 0;
int animation_tick = 0;

/// @brief 太空人动画
/// @param
void DisplayTask::tkr_animation()
{
  int start_x = 0, start_y = 0;
  delay(5);
  animation_tick += 5;
  if (animation_tick < delat_time)
    return;
  animation_tick = 0;
  switch (animation_index)
  {
  case 0:
    tft_.pushImage(start_x, start_y, 240, 240, img2);
    break;
  case 1:
    tft_.pushImage(start_x, start_y, 240, 240, img3);
    break;
  case 2:
    tft_.pushImage(start_x, start_y, 240, 240, img5);
    break;
  case 3:
    tft_.pushImage(start_x, start_y, 240, 240, img7);
    break;
  case 4:
    tft_.pushImage(start_x, start_y, 240, 240, img9);
    break;
  case 5:
    tft_.pushImage(start_x, start_y, 240, 240, img11);
    break;
  case 6:
    tft_.pushImage(start_x, start_y, 240, 240, img13);
    break;
  case 7:
    tft_.pushImage(start_x, start_y, 240, 240, img14);
    break;
  case 8:
    tft_.pushImage(start_x, start_y, 240, 240, img16);
    break;
  case 9:
    tft_.pushImage(start_x, start_y, 240, 240, img18);
    break;
  case 10:
    tft_.pushImage(start_x, start_y, 240, 240, img20);
    break;
  case 11:
    tft_.pushImage(start_x, start_y, 240, 240, img22);
    break;
  case 12:
    tft_.pushImage(start_x, start_y, 240, 240, img24);
    break;
  default:
    break;
  }

  animation_index++;
  if (animation_index > 12)
    animation_index = 0;
}

static int32_t last_position = 0;
static int32_t curr_position = 0;
static int8_t menu_index = 0;
static uint8_t first_show_menu = 0;
void DisplayTask::show_menu()
{
  if (last_position == curr_position && first_show_menu == 1)
    return;

  if (curr_position > last_position)
  {
    menu_index++;
    if (menu_index > 3)
      menu_index = 0;
  }
  else if (curr_position < last_position)
  {
    menu_index--;
    if (menu_index < 0)
      menu_index = 3;
  }

  if (first_show_menu == 0)
  {
    menu_index = 0;
  }
  first_show_menu = 1;

  last_position = curr_position;

  tft_.fillScreen(TFT_BLACK);

  int start_x = 56, start_y = 56;
  switch (menu_index)
  {
  case 0:
    tft_.pushImage(start_x, start_y, VOLUME_WIDTH, VOLUME_HEIGHT, Volume);
    break;
  case 1:
    tft_.pushImage(start_x, start_y, SPIN_WIDTH, SPIN_HEIGHT, Spin);
    break;
  case 2:
    tft_.pushImage(start_x, start_y, MUSICBOX_WIDTH, MUSICBOX_HEIGHT, MusicBox);
    break;
  case 3:
    tft_.pushImage(start_x, start_y, ESPNOW_WIDTH, ESPNOW_HEIGHT, EspNow);
    break;
  default:
    break;
  }
}

void reset_menu_index()
{
  menu_index = 0;
  first_show_menu = 0;
  last_position = 0;
  curr_position = 0;
}

int8_t get_menu_index()
{
  return menu_index;
}

void DisplayTask::run()
{
  tft_.begin();
  tft_.invertDisplay(1);
  tft_.setRotation(SK_DISPLAY_ROTATION);
  tft_.fillScreen(TFT_DARKGREEN);

#if (PIN_LCD_BACKLIGHT != -1)
  ledcSetup(LEDC_CHANNEL_LCD_BACKLIGHT, 5000, SK_BACKLIGHT_BIT_DEPTH);
  ledcAttachPin(PIN_LCD_BACKLIGHT, LEDC_CHANNEL_LCD_BACKLIGHT);
  ledcWrite(LEDC_CHANNEL_LCD_BACKLIGHT, (1 << SK_BACKLIGHT_BIT_DEPTH) - 1);
#endif

  spr_.setColorDepth(8);

  if (spr_.createSprite(TFT_WIDTH, TFT_HEIGHT) == nullptr)
  {
    log("ERROR: sprite allocation failed!");
    tft_.fillScreen(TFT_RED);
  }
  else
  {
    log("Sprite created!");
    tft_.fillScreen(TFT_PURPLE);
  }
  spr_.setTextColor(0xFFFF, TFT_BLACK);

  PB_SmartKnobState state;

  const int RADIUS = TFT_WIDTH / 2;
  const uint16_t FILL_COLOR = spr_.color565(90, 18, 151);
  const uint16_t DOT_COLOR = spr_.color565(80, 100, 200);

  spr_.setTextDatum(CC_DATUM);
  spr_.setTextColor(TFT_WHITE);

  uint8_t img_show_sta = 0;

  while (1)
  {
    if (xQueueReceive(knob_state_queue_, &state, portMAX_DELAY) == pdFALSE)
    {
      continue;
    }

    // 休眠动画
    if (strncmp(state.config.text, "Sleep", 5) == 0)
    {
      img_show_sta = 0;
      reset_menu_index();
      tkr_animation();
      continue;
    }
    // 多档开关切换菜单
    if (strncmp(state.config.text, "Rotary switch", 13) == 0)
    {
      img_show_sta = 0;
      curr_position = state.current_position;
      show_menu();
      continue;
    }
    // 音乐盒界面
    if (strncmp(state.config.text, "MusicBox", 8) == 0)
    {
      if (!img_show_sta)
      {
        img_show_sta = 1;
        tft_.fillScreen(TFT_BLACK);
        tft_.pushImage(0, 0, MUSICBOXHOME_WIDTH, MUSICBOXHOME_HEIGHT, MusicBoxHome);
      }
      continue;
    }
    // 指尖陀螺界面
    if (strncmp(state.config.text, "Spin", 4) == 0)
    {
      if (!img_show_sta)
      {
        img_show_sta = 1;
        tft_.fillScreen(TFT_BLACK);
        tft_.pushImage(0, 0, SPINHOME_WIDTH, SPINHOME_HEIGHT, SpinHome);
      }
      continue;
    }

    animation_index = 0;
    animation_tick = 0;
    first_show_menu = 0;
    img_show_sta = 0;

    int32_t num_positions = state.config.max_position - state.config.min_position + 1;
    float adjusted_sub_position = state.sub_position_unit * state.config.position_width_radians;
    if (num_positions > 0)
    {
      if (state.current_position == state.config.min_position && state.sub_position_unit < 0)
      {
        adjusted_sub_position = -logf(1 - state.sub_position_unit * state.config.position_width_radians / 5 / PI * 180) * 5 * PI / 180;
      }
      else if (state.current_position == state.config.max_position && state.sub_position_unit > 0)
      {
        adjusted_sub_position = logf(1 + state.sub_position_unit * state.config.position_width_radians / 5 / PI * 180) * 5 * PI / 180;
      }
    }

    float left_bound = PI / 2;
    float right_bound = 0;
    if (num_positions > 0)
    {
      float range_radians = (state.config.max_position - state.config.min_position) * state.config.position_width_radians;
      left_bound = PI / 2 + range_radians / 2;
      right_bound = PI / 2 - range_radians / 2;
    }
    float raw_angle = left_bound - (state.current_position - state.config.min_position) * state.config.position_width_radians;
    float adjusted_angle = raw_angle - adjusted_sub_position;

    raw_angle = -raw_angle;
    adjusted_angle = -adjusted_angle;
    bool sk_demo_mode = strncmp(state.config.text, "SKDEMO_", 7) == 0; // 比较字符是否相等

    if (!sk_demo_mode)
    {
      spr_.fillSprite(TFT_BLACK);

      if (strncmp(state.config.text, "EspNow", 6) == 0)
      {
        spr_.pushImage(56, 56, MYCAT_WIDTH, MYCAT_HEIGHT, myCat);
      }
      else if (strncmp(state.config.text, "MusicBox", 8) == 0)
      {
      }
      else if (strncmp(state.config.text, "Spin", 4) == 0)
      {
      }
      else
      {
        if (strncmp(state.config.text, "Volume", 6) == 0)
        {
          state.current_position = 100 - state.current_position;
        }
        if (num_positions > 1)
        {
          int32_t height = (state.current_position - state.config.min_position) * TFT_HEIGHT / (state.config.max_position - state.config.min_position);
          spr_.fillRect(0, TFT_HEIGHT - height, TFT_WIDTH, height, FILL_COLOR);
        }

        spr_.setFreeFont(&Roboto_Light_60);
        spr_.drawNumber(state.current_position, TFT_WIDTH / 2, TFT_HEIGHT / 2 - VALUE_OFFSET, 1);
        spr_.setFreeFont(&DESCRIPTION_FONT);
        int32_t line_y = TFT_HEIGHT / 2 + DESCRIPTION_Y_OFFSET;
        char *start = state.config.text;
        char *end = start + strlen(state.config.text);
        while (start < end)
        {
          char *newline = strchr(start, '\n');
          if (newline == nullptr)
            newline = end;

          char buf[sizeof(state.config.text)] = {};
          strncat(buf, start, min(sizeof(buf) - 1, (size_t)(newline - start)));
          spr_.drawString(String(buf), TFT_WIDTH / 2, line_y, 1);
          start = newline + 1;
          line_y += spr_.fontHeight(1);
        }

        if (num_positions > 0)
        {
          // spr_.drawLine(TFT_WIDTH / 2 + RADIUS * cosf(left_bound), TFT_HEIGHT / 2 - RADIUS * sinf(left_bound),
          //               TFT_WIDTH / 2 + (RADIUS - 10) * cosf(left_bound), TFT_HEIGHT / 2 - (RADIUS - 10) * sinf(left_bound), TFT_WHITE);
          // spr_.drawLine(TFT_WIDTH / 2 + RADIUS * cosf(right_bound), TFT_HEIGHT / 2 - RADIUS * sinf(right_bound),
          //               TFT_WIDTH / 2 + (RADIUS - 10) * cosf(right_bound), TFT_HEIGHT / 2 - (RADIUS - 10) * sinf(right_bound), TFT_WHITE);
        }

        if (DRAW_ARC)
        {
          spr_.drawCircle(TFT_WIDTH / 2, TFT_HEIGHT / 2, RADIUS, TFT_DARKGREY);
        }
      }

      if (num_positions > 0 && ((state.current_position == state.config.min_position && state.sub_position_unit < 0) ||
                                (state.current_position == state.config.max_position && state.sub_position_unit > 0)))
      {
        spr_.fillCircle(TFT_WIDTH / 2 + (RADIUS - 10) * cosf(raw_angle), TFT_HEIGHT / 2 - (RADIUS - 10) * sinf(raw_angle), 5, DOT_COLOR);
        if (raw_angle < adjusted_angle)
        {
          for (float r = raw_angle; r <= adjusted_angle; r += 2 * PI / 180)
          {
            spr_.fillCircle(TFT_WIDTH / 2 + (RADIUS - 10) * cosf(r), TFT_HEIGHT / 2 - (RADIUS - 10) * sinf(r), 2, DOT_COLOR);
          }
          spr_.fillCircle(TFT_WIDTH / 2 + (RADIUS - 10) * cosf(adjusted_angle), TFT_HEIGHT / 2 - (RADIUS - 10) * sinf(adjusted_angle), 2, DOT_COLOR);
        }
        else
        {
          for (float r = raw_angle; r >= adjusted_angle; r -= 2 * PI / 180)
          {
            spr_.fillCircle(TFT_WIDTH / 2 + (RADIUS - 10) * cosf(r), TFT_HEIGHT / 2 - (RADIUS - 10) * sinf(r), 2, DOT_COLOR);
          }
          spr_.fillCircle(TFT_WIDTH / 2 + (RADIUS - 10) * cosf(adjusted_angle), TFT_HEIGHT / 2 - (RADIUS - 10) * sinf(adjusted_angle), 2, DOT_COLOR);
        }
      }
      else
      {
        spr_.fillCircle(TFT_WIDTH / 2 + (RADIUS - 10) * cosf(adjusted_angle), TFT_HEIGHT / 2 - (RADIUS - 10) * sinf(adjusted_angle), 5, DOT_COLOR);
      }
    }

    spr_.pushSprite(0, 0);

    {
      SemaphoreGuard lock(mutex_);
#if (PIN_LCD_BACKLIGHT != -1)
      ledcWrite(LEDC_CHANNEL_LCD_BACKLIGHT, brightness_);
#endif
    }

    delay(5);
  }
}

/// @brief 获取消息队列
/// @return
QueueHandle_t DisplayTask::getKnobStateQueue()
{
  return knob_state_queue_;
}

/// @brief 设置亮度
/// @param brightness
void DisplayTask::setBrightness(uint16_t brightness)
{
  SemaphoreGuard lock(mutex_);
  brightness_ = brightness >> (16 - SK_BACKLIGHT_BIT_DEPTH);
}

/// @brief 注册log
/// @param logger
void DisplayTask::setLogger(Logger *logger)
{
  logger_ = logger;
}

/// @brief log
/// @param msg
void DisplayTask::log(const char *msg)
{
  if (logger_ != nullptr)
  {
    logger_->log(msg);
  }
}

#endif