#pragma once

#include "Arduino.h"
#include "logger.h"

float espNowGetPeerMotor();
void espNowSendData(float val);

class EspNowTask
{
public:
    EspNowTask();
    ~EspNowTask();

    void Init();

    void sendData(float val);

    float getPeerMotor();

    void setLogger(Logger *logger);

protected:
    void run();

private:
    Logger *logger_;
    void log(const char *msg);
};
