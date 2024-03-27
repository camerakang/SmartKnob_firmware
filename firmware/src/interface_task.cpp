#if SK_LEDS
#include <FastLED.h>
#endif

#define SK_STRAIN 1

#if SK_STRAIN
#include <HX711.h>
#endif

#if SK_ALS
#include <Adafruit_VEML7700.h>
#endif

#include "interface_task.h"
#include "semaphore_guard.h"
#include "util.h"

#include "my_config.h"

static uint8_t last_menu_index = 0;
static uint8_t sleep_sta = 0;
static uint32_t sleep_ticks = 0;
int32_t sleep_last_position = 0;

#define USE_RGB 1

#if SK_LEDS
CRGB leds[NUM_LEDS];
#endif

#if SK_STRAIN
HX711 scale;
#endif

#if SK_ALS
Adafruit_VEML7700 veml = Adafruit_VEML7700();
#endif

static PB_SmartKnobConfig configs[] = {
// int32_t position;
// float sub_position_unit;
// uint8_t position_nonce;
// int32_t min_position;
// int32_t max_position;
// float position_width_radians;
// float detent_strength_unit;
// float endstop_strength_unit;
// float snap_point;
// char text[51];
// pb_size_t detent_positions_count;
// int32_t detent_positions[5];
// float snap_point_bias;
// int8_t led_hue;
#if MY_KNOB_CONFIGS
    {
        0,                   // position
        0,                   // sub_position_unit
        0,                   // position_nonce
        0,                   // min_position
        -1,                  // max position < min position indicates no bounds
        10 * PI / 180,       // position_width_radians
        0,                   // detent_strength_unit
        1,                   // endstop_strength_unit
        1.1,                 // snap_point
        "Sleep\nNo detents", // text
        0,                   // detent_positions_count
        {},                  // detent_positions
        0,                   // snap_point_bias
        200,                 // led_hue
    },                       // 休眠模式

    {
        0,
        0,
        3,
        0,
        -1,
        60 * PI / 180,
        1,
        1,
        0.55,
        "Rotary switch\nStrong detent",
        0,
        {},
        0,
        157,
    }, // 多档开关

    {
        50,                     // position
        0,                      // sub_position_unit
        6,                      // position_nonce
        0,                      // min_position 0
        100,                    // max_position 100
        3.6 * PI / 180,         // position_width_radians
        0.2,                    // detent_strength_unit
        1.1,                    // endstop_strength_unit
        1.1,                    // snap_point
        "Volume\nWeak detents", // text
        0,                      // detent_positions_count
        {},                     // detent_positions
        0,                      // snap_point_bias
        0,                      // led_hue
    },                          // 音量调节旋钮

    {
        0,                  // position
        0,                  // sub_position_unit
        0,                  // position_nonce
        0,                  // min_position
        -1,                 // max position < min position indicates no bounds
        10 * PI / 180,      // position_width_radians
        0,                  // detent_strength_unit
        1,                  // endstop_strength_unit
        1.1,                // snap_point
        "Spin\nNo detents", // text
        0,                  // detent_positions_count
        {},                 // detent_positions
        0,                  // snap_point_bias
        200,                // led_hue
    },                      // TODO:指尖陀螺

    {
        0,                          // position
        0,                          // sub_position_unit
        0,                          // position_nonce
        0,                          // min_position
        100,                        // max position < min position indicates no bounds
        3.6 * PI / 180,             // position_width_radians
        1,                          // detent_strength_unit
        1,                          // endstop_strength_unit
        1.1,                        // snap_point
        "MusicBox\nStrong detents", // text
        0,                          // detent_positions_count
        {},                         // detent_positions
        0,                          // snap_point_bias
        200,                        // led_hue
    },                              // TODO: 音乐盒---棘轮手感拧过去，顺滑慢慢转回来

    {
        0,                    // position
        0,                    // sub_position_unit
        0,                    // position_nonce
        0,                    // min_position
        -1,                   // max position < min position indicates no bounds
        10 * PI / 180,        // position_width_radians
        0,                    // detent_strength_unit
        1,                    // endstop_strength_unit
        1.1,                  // snap_point
        "EspNow\nNo detents", // text
        0,                    // detent_positions_count
        {},                   // detent_positions
        0,                    // snap_point_bias
        200,                  // led_hue
    },                        // TODO: 双机联动
#else
    {
               0,
        0,
        6,
        0,
        87,
        8.225806452 * PI / 180 /2,
        3,
        1,
        1.1,
        "Coarse values\nkkkk",
        0,
        {},
        0,
        200,
    },
        {
        0,                          // position
        0,                          // sub_position_unit
        0,                          // position_nonce
        0,                          // min_position
        100,                        // max position < min position indicates no bounds
        3.6 * PI / 180,             // position_width_radians
        1,                          // detent_strength_unit
        1,                          // endstop_strength_unit
        1.1,                        // snap_point
        "MusicBox\nStrong detents", // text
        0,                          // detent_positions_count
        {},                         // detent_positions
        0,                          // snap_point_bias
        200,                        // led_hue
    },                              // TODO: 音乐盒---棘轮手感拧过去，顺滑慢慢转回来,
    {
        0,
        0,
        2,
        0,
        72,
        10 * PI / 180,
        0,
        1,
        1.1,
        "Multi-rev\nNo detents",
        0,
        {},
        0,
        73,
    },
    {
        0,
        0,
        3,
        0,
        1,
        60 * PI / 180,
        1,
        1,
        0.55, // Note the snap point is slightly past the midpoint (0.5); compare to normal detents which use a snap point *past* the next value (i.e. > 1)
        "On/off\nStrong detent",
        0,
        {},
        0,
        157,
    },
    {
        0,
        0,
        4,
        0,
        0,
        60 * PI / 180,
        0.01,
        0.6,
        1.1,
        "Return-to-center",
        0,
        {},
        0,
        45,
    },
    {
        127,
        0,
        5,
        0,
        255,
        1 * PI / 180,
        0,
        1,
        1.1,
        "Fine values\nNo detents",
        0,
        {},
        0,
        219,
    },
    {
        127,
        0,
        5,
        0,
        255,
        1 * PI / 180,
        1,
        1,
        1.1,
        "Fine values\nWith detents",
        0,
        {},
        0,
        25,
    },
    {
        0,
        0,
        6,
        0,
        -1,
        8.225806452 * PI / 180 / 3,
        2,
        1,
        1.6,
        "Coarse values\nStrong detents",
        0,
        {},
        0,
        200,
    },
    {
        0,
        0,
        6,
        0,
        -1,
        8.225806452 * PI / 180 * 2,
        4,
        1,
        1.1,
        "Coarse values\nWeak detents",
        0,
        {},
        0,
        0,
    },
    {
        0,
        0,
        7,
        0,
        31,
        7 * PI / 180,
        2.5,
        1,
        0.7,
        "Magnetic detents",
        4,
        {2, 10, 21, 22},
        0,
        73,
    },
    {
        0,
        0,
        8,
        -6,
        6,
        60 * PI / 180,
        1,
        1,
        0.55,
        "Return-to-center\nwith detents",
        0,
        {},
        0.4,
        157,
    },
#endif
};

InterfaceTask::InterfaceTask(const uint8_t task_core, MotorTask &motor_task, DisplayTask *display_task) : Task("Interface", 3400, 1, task_core),
                                                                                                          stream_(),
                                                                                                          motor_task_(motor_task),
                                                                                                          display_task_(display_task),
                                                                                                          plaintext_protocol_(stream_, [this]()
                                                                                                                              { motor_task_.runCalibration(); }),
                                                                                                          proto_protocol_(stream_, [this](PB_SmartKnobConfig &config)
                                                                                                                          { applyConfig(config, true); })
{
#if SK_DISPLAY
    assert(display_task != nullptr);
#endif

    log_queue_ = xQueueCreate(10, sizeof(std::string *));
    assert(log_queue_ != NULL);

    knob_state_queue_ = xQueueCreate(1, sizeof(PB_SmartKnobState));
    assert(knob_state_queue_ != NULL);

    mutex_ = xSemaphoreCreateMutex();
    assert(mutex_ != NULL);
}

InterfaceTask::~InterfaceTask()
{
    vSemaphoreDelete(mutex_);
}

void InterfaceTask::run()
{
    stream_.begin();

#if SK_LEDS
    FastLED.addLeds<SK6812, PIN_LED_DATA, GRB>(leds, NUM_LEDS);
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        leds[i] = CRGB::Black;
    }
    FastLED.show();
#endif

#if SK_ALS && PIN_SDA >= 0 && PIN_SCL >= 0
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000);
#endif

#if SK_STRAIN
    scale.begin(PIN_STRAIN_DO, PIN_STRAIN_SCK);
#endif

#if SK_ALS
    if (veml.begin())
    {
        veml.setGain(VEML7700_GAIN_2);
        veml.setIntegrationTime(VEML7700_IT_400MS);
    }
    else
    {
        log("ALS sensor not found!");
    }
#endif
    // 默认处于第一个
    applyConfig(configs[0], false);

    motor_task_.addListener(knob_state_queue_);

    // [this]() 表示一个匿名函数
    plaintext_protocol_.init([this]()
                             { changeConfig(true); }, /* 按 空格键改变模式 */
                             [this]()
                             {
                                 if (!configuration_loaded_)
                                 {
                                     return;
                                 }
                                 if (strain_calibration_step_ == 0)
                                 {
                                     log("Strain calibration step 1: Don't touch the knob, then press 'S' again");
                                     strain_calibration_step_ = 1;
                                 }
                                 else if (strain_calibration_step_ == 1)
                                 {
                                     configuration_value_.strain.idle_value = strain_reading_;
                                     snprintf(buf_, sizeof(buf_), "  idle_value=%d", configuration_value_.strain.idle_value);
                                     log(buf_);
                                     log("Strain calibration step 2: Push and hold down the knob with medium pressure, and press 'S' again");
                                     strain_calibration_step_ = 2;
                                 }
                                 else if (strain_calibration_step_ == 2)
                                 {
                                     configuration_value_.strain.press_delta = strain_reading_ - configuration_value_.strain.idle_value;
                                     configuration_value_.has_strain = true;
                                     snprintf(buf_, sizeof(buf_), "  press_delta=%d", configuration_value_.strain.press_delta);
                                     log(buf_);
                                     log("Strain calibration complete! Saving...");
                                     strain_calibration_step_ = 0;
                                     if (configuration_->setStrainCalibrationAndSave(configuration_value_.strain))
                                     {
                                         log("  Saved!");
                                     }
                                     else
                                     {
                                         log("  FAILED to save config!!!");
                                     }
                                 }
                             });

    // Start in legacy protocol mode
    current_protocol_ = &plaintext_protocol_;

    // 匿名函数
    ProtocolChangeCallback protocol_change_callback = [this](uint8_t protocol)
    {
        switch (protocol)
        {
        case SERIAL_PROTOCOL_LEGACY:
            current_protocol_ = &plaintext_protocol_;
            break;
        case SERIAL_PROTOCOL_PROTO:
            current_protocol_ = &proto_protocol_;
            break;
        default:
            log("Unknown protocol requested");
            break;
        }
    };

    plaintext_protocol_.setProtocolChangeCallback(protocol_change_callback);
    proto_protocol_.setProtocolChangeCallback(protocol_change_callback);

    // Interface loop:
    while (1)
    {
        // 打印状态
        if (xQueueReceive(knob_state_queue_, &latest_state_, 0) == pdTRUE)
        {
            if (strncmp(latest_state_.config.text, "Rotary switch", 13) == 0)
            {
                if (get_menu_index() != last_menu_index)
                {
                    // TODO: 菜单音效
                    last_menu_index = get_menu_index();
                    uint8_t cx800_buf[3] = {0xFE, 0x31, 0x02};
                    cx800_buf[2] = (uint8_t)MENU_SOUND;
                    current_protocol_->write(cx800_buf, 3);
                }
            }
            else if (strncmp(latest_state_.config.text, "EspNow", 6) == 0)
            {
                // TODO: 逗猫音效
                static uint8_t last_espnow_state = 0;
                if (int(latest_state_.current_angle) != 0 && int(latest_state_.current_angle) % 7 == 0)
                {
                    if (last_espnow_state == 0)
                    {
                        last_espnow_state = 1;
                        uint8_t cx800_buf[3] = {0xFE, 0x31, 0x06};
                        cx800_buf[2] = (uint8_t)CAT_SOUND;
                        current_protocol_->write(cx800_buf, 3);
                    }
                }
                else
                {
                    last_espnow_state = 0;
                }
            }
            else if (strncmp(latest_state_.config.text, "MusicBox", 8) == 0)
            {
                // TODO: 八音盒上发条音效
                static int32_t last_position = 0;
                static uint32_t last_millis = 0;
                static uint8_t last_musicbox_state = 0;
                if (latest_state_.current_position > last_position)
                {
                    // TODO: 1秒播一次
                    if (millis() - last_millis > 1000)
                    {
                        uint8_t cx800_buf[3] = {0xFE, 0x31, 0x04};
                        cx800_buf[2] = (uint8_t)MUSICBOX_SPRING_SOUND;
                        current_protocol_->write(cx800_buf, 3);
                        last_millis = millis();
                    }
                    last_musicbox_state = 0;
                }
                last_position = latest_state_.current_position;

                extern uint8_t get_music_box_sta();
                if (get_music_box_sta())
                {
                    // 八音盒：开始播放
                    if (last_musicbox_state == 0)
                    {
                        last_musicbox_state = 1;
                        uint8_t cx800_buf[3] = {0xFE, 0x31, 0x05};
                        cx800_buf[2] = (uint8_t)MUSICBOX_SOUND;
                        current_protocol_->write(cx800_buf, 3);
                    }
                }
                else
                {
                    // 八音盒：停止播放
                    if (last_musicbox_state == 1)
                    {
                        uint8_t cx800_buf[3] = {0xFE, 0x11, 0x01};
                        current_protocol_->write(cx800_buf, 3);
                    }
                    last_musicbox_state = 0;
                }
            }
#if PRINT_LOG
            // publishState();
#endif
        }

        current_protocol_->loop();

        // 打印log
        std::string *log_string;
        while (xQueueReceive(log_queue_, &log_string, 0) == pdTRUE)
        {
#if PRINT_LOG
            current_protocol_->log(log_string->c_str());
#endif
            delete log_string;
        }

        // 更新硬件状态
        // 应变传感器
        // RGB状态
        updateHardware();

        // 如果没有加载配置,先获取配置
        if (!configuration_loaded_)
        {
            SemaphoreGuard lock(mutex_);
            if (configuration_ != nullptr)
            {
                configuration_value_ = configuration_->get();
                configuration_loaded_ = true;
            }
        }

        // TODO: sleep
        if (strncmp(latest_state_.config.text, "Sleep", 5) == 0)
        {
            sleep_ticks = 0;
            sleep_sta = 0;
            sleep_last_position = latest_state_.current_position;
        }
        else
        {
            sleep_ticks++;

            if (latest_state_.current_position != sleep_last_position)
            {
                sleep_last_position = latest_state_.current_position;
                sleep_ticks = 0;
                sleep_sta = 0;
            }
            else
            {
                if (sleep_ticks >= SLEEP_TIME)
                {
                    if (sleep_sta == 0)
                    {
                        log("Sleeping...");
                        sleep_sta = 1;
                        sleep_ticks = 0;
                        current_config_ = 0;
                        applyConfig(configs[0], false);
                    }
                }
            }
        }

        delay(1);
    }
}

void InterfaceTask::log(const char *msg)
{
    // Allocate a string for the duration it's in the queue; it is free'd by the queue consumer
    std::string *msg_str = new std::string(msg);

    // Put string in queue (or drop if full to avoid blocking)
    xQueueSendToBack(log_queue_, &msg_str, 0);
}

size_t InterfaceTask::write(const uint8_t *buffer, size_t size)
{
    return current_protocol_->write(buffer, size);
}

extern int8_t get_menu_index();
// void InterfaceTask::changeConfig(bool next)
// {
//     // current_config_ 模式索引
//     if (current_config_ == 1)
//     {
//         int8_t menu_index = get_menu_index();
//         switch (menu_index)
//         {
//         case 0: // Volume
//             current_config_ = 2;
//             break;
//         case 1: // Spin
//             current_config_ = 3;
//             break;
//         case 2: // MusicBox
//             current_config_ = 4;
//             break;
//         case 3: // EspNow
//             current_config_ = 5;
//             break;
//         default:
//             break;
//         }
//     }
//     else
//     {
//         current_config_ = 1;
//         // if (next)
//         // {
//         //     current_config_ = (current_config_ + 1) % COUNT_OF(configs);
//         // }
//         // else
//         // {
//         //     if (current_config_ == 0)
//         //     {
//         //         current_config_ = COUNT_OF(configs) - 1;
//         //     }
//         //     else
//         //     {
//         //         current_config_--;
//         //     }
//         // }
//     }

//     snprintf(buf_, sizeof(buf_), "Changing config to %d -- %s", current_config_, configs[current_config_].text);
//     log(buf_);

//     applyConfig(configs[current_config_], false);
// }

void InterfaceTask::changeConfig(bool next)
{
#if MY_KNOB_CONFIGS
        // current_config_ 模式索引
        if (current_config_ == 1)
    {
        int8_t menu_index = get_menu_index();
        switch (menu_index)
        {
        case 0: // Volume
            current_config_ = 2;
            break;
        case 1: // Spin
            current_config_ = 3;
            break;
        case 2: // MusicBox
            current_config_ = 4;
            break;
        case 3: // EspNow
            current_config_ = 5;
            break;
        default:
            break;
        }
    }
    else
    {
        current_config_ = 1;
    }
#else
    if (next)
    {
        current_config_ = (current_config_ + 1) % COUNT_OF(configs);
    }
    else
    {
        if (current_config_ == 0)
        {
            current_config_ = COUNT_OF(configs) - 1;
        }
        else
        {
            current_config_--;
        }
    }
#endif

    snprintf(buf_, sizeof(buf_), "Changing config to %d -- %s", current_config_, configs[current_config_].text);
    log(buf_);
    applyConfig(configs[current_config_], false);
}
void InterfaceTask::updateHardware()
{
    // How far button is pressed, in range [0, 1]
    float press_value_unit = 0;

#if SK_ALS
    const float LUX_ALPHA = 0.005;
    static float lux_avg;
    float lux = veml.readLux();
    lux_avg = lux * LUX_ALPHA + lux_avg * (1 - LUX_ALPHA);
    static uint32_t last_als;
    if (millis() - last_als > 1000 && strain_calibration_step_ == 0)
    {
        snprintf(buf_, sizeof(buf_), "millilux: %.2f", lux * 1000);
        log(buf_);
        last_als = millis();
    }
#endif

    static bool pressed;

#if SK_STRAIN
    if (scale.wait_ready_timeout(100))
    {
        strain_reading_ = scale.read();

        static uint32_t last_reading_display;
        if (millis() - last_reading_display > 1000 && strain_calibration_step_ == 0)
        {
#if PRINT_STRAIN
            snprintf(buf_, sizeof(buf_), "HX711 reading: %d", strain_reading_);
            log(buf_);
#endif
            last_reading_display = millis();
        }
        // if (configuration_loaded_ && configuration_value_.has_strain && strain_calibration_step_ == 0)
        if (strain_calibration_step_ == 0)
        {
            // TODO: calibrate and track (long term moving average) idle point (lower)
            // press_value_unit = lerp(strain_reading_, configuration_value_.strain.idle_value, configuration_value_.strain.idle_value + configuration_value_.strain.press_delta, 0, 1);
            press_value_unit = lerp(strain_reading_, STRAIN_MIN, STRAIN_MAX, 0, 1);
            // Ignore readings that are way out of expected bounds
            if (-1 < press_value_unit && press_value_unit < 2)
            {
                static uint8_t press_readings;
                if (!pressed && press_value_unit > 1)
                {
                    press_readings++;
                    if (press_readings > 2)
                    {
                        // 按下时 抖动电机
                        motor_task_.playHaptic(true);
                        pressed = true;
                        press_count_++;
                        // 打印状态
                        // publishState();
                        // TODO: 按下时播放音效
                        uint8_t cx800_buf[3] = {0xFE, 0x31, 0x01};
                        cx800_buf[2] = (uint8_t)REPRESSED_SOUND;
                        current_protocol_->write(cx800_buf, 3);
                        current_protocol_->write(cx800_buf, 3);
                        last_menu_index = get_menu_index();
                        if (!remote_controlled_)
                        {
                            changeConfig(true);
                        }
                    }
                }
                else if (pressed && press_value_unit < 0.5)
                {
                    press_readings++;
                    if (press_readings > 2)
                    {
                        // 释放时 抖动电机
                        motor_task_.playHaptic(false);
                        pressed = false;
                        uint8_t cx800_buf[3] = {0xFE, 0x31, 0x01};
                        cx800_buf[2] = (uint8_t)PRESSED_SOUND7;
                        current_protocol_->write(cx800_buf, 3);
                        current_protocol_->write(cx800_buf, 3);
                    }
                }
                else
                {
                    press_readings = 0;
                }
            }
        }
    }
    else
    {
        log("HX711 not found.");
#if SK_LEDS
        for (uint8_t i = 0; i < NUM_LEDS; i++)
        {
            leds[i] = CRGB::Red;
        }
#if USE_RGB
        FastLED.show();
#endif
#endif
    }
#endif

    uint16_t brightness = UINT16_MAX;
// TODO: brightness scale factor should be configurable (depends on reflectivity of surface)
#if SK_ALS
    brightness = (uint16_t)CLAMP(lux_avg * 13000, (float)1280, (float)UINT16_MAX);
#endif

#if SK_DISPLAY
    display_task_->setBrightness(brightness); // TODO: apply gamma correction
#endif

#if SK_LEDS
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        leds[i].setHSV(latest_config_.led_hue, 255 - 180 * CLAMP(press_value_unit, (float)0, (float)1) - 75 * pressed, brightness >> 8);

        // Gamma adjustment
        leds[i].r = dim8_video(leds[i].r);
        leds[i].g = dim8_video(leds[i].g);
        leds[i].b = dim8_video(leds[i].b);
    }
#if USE_RGB
    FastLED.show();
#endif
#endif
}

void InterfaceTask::setConfiguration(Configuration *configuration)
{
    SemaphoreGuard lock(mutex_);
    configuration_ = configuration;
}

void InterfaceTask::publishState()
{
    // Apply local state before publishing to serial
    latest_state_.press_nonce = press_count_;
    current_protocol_->handleState(latest_state_);
}

void InterfaceTask::applyConfig(PB_SmartKnobConfig &config, bool from_remote)
{
    remote_controlled_ = from_remote;
    latest_config_ = config;
    motor_task_.setConfig(config);
}
