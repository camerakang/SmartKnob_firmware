#pragma once

class Logger
{
public:
    Logger(){};
    virtual ~Logger(){};
    virtual void log(const char *msg) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size) = 0;
};