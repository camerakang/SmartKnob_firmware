#pragma once

#if SK_DISPLAY

#include <Arduino.h>
#include <vector>

#include <TFT_eSPI.h>

#include "configuration.h"
#include "logger.h"
#include "proto_gen/smartknob.pb.h"
#include "task.h"

int8_t get_menu_index();

class DisplayTask : public Task<DisplayTask>
{
    friend class Task<DisplayTask>; // Allow base Task to invoke protected run()

public:
    DisplayTask(const uint8_t task_core);
    ~DisplayTask();

    QueueHandle_t getKnobStateQueue();

    void setBrightness(uint16_t brightness);
    void setLogger(Logger *logger);

protected:
    void run();

private:
    TFT_eSPI tft_ = TFT_eSPI();

    /** Full-size sprite used as a framebuffer */
    TFT_eSprite spr_ = TFT_eSprite(&tft_);

    QueueHandle_t knob_state_queue_;

    PB_SmartKnobState state_;
    SemaphoreHandle_t mutex_;
    uint16_t brightness_;
    Logger *logger_;

    std::vector<QueueHandle_t> listeners_;

    void log(const char *msg);
    void tkr_animation();
    void show_menu();
};

#else

class DisplayTask
{
};

#endif
