#include <Arduino.h>
#include <string>

#include <esp_now.h>
#include <WiFi.h>

#include "esp_now_task.h"

#include "my_config.h"

typedef struct struct_message
{
    float value;
} struct_message;
struct_message inputPosition;
struct_message outputPosition;
esp_now_peer_info_t peerInfo;

static uint8_t peer_mac[6] = {0};

float thatMotor = 0x7FFFFFFE;

#if (MACHINE_NUMBER == 1)
std::string mac_str = PEERMAC_TWO;
int mac_str_len = mac_str.length();
#elif (MACHINE_NUMBER == 2)
std::string mac_str = PEERMAC_ONE;
int mac_str_len = mac_str.length();
#endif

/// @brief 获取对方电机
/// @return
float EspNowTask::getPeerMotor()
{
    return thatMotor;
}

float espNowGetPeerMotor()
{
    return thatMotor;
}

void espNowSendData(float val)
{
    outputPosition.value = val;
    esp_err_t result = esp_now_send(peer_mac, (uint8_t *)&outputPosition, sizeof(outputPosition));
}

/// @brief 发送数据
/// @param val
void EspNowTask::sendData(float val)
{
    outputPosition.value = val;
    esp_err_t result = esp_now_send(peer_mac, (uint8_t *)&outputPosition, sizeof(outputPosition));
}

/// @brief 接收数据
/// @param mac
/// @param incomingData
/// @param len
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    memcpy(&inputPosition, incomingData, sizeof(inputPosition));
    thatMotor = inputPosition.value;
}

void EspNowTask::Init()
{
    uint8_t mac_index = 0;
    char mac_str_hex[3] = {'\0'};
    int mac_str_hex_index = 0;

    for (int i = 0; i < mac_str_len; i++)
    {
        if (mac_str[i] != ':')
        {
            mac_str_hex[mac_str_hex_index] = mac_str[i];
            mac_str_hex_index++;
            if (mac_str_hex_index >= sizeof(mac_str_hex) / sizeof(mac_str_hex[0]))
                mac_str_hex_index = 0;
        }

        if (mac_str[i] == ':' || i == (mac_str_len - 1))
        {
            peer_mac[mac_index] = strtol(mac_str_hex, NULL, 16);
            mac_str_hex_index = 0;
            mac_index++;
            continue;
        }

        if (mac_index >= sizeof(peer_mac) / sizeof(peer_mac[0]))
        {
            break;
        }
    }

    // Init ESP-NOW
    WiFi.mode(WIFI_STA);

    log("MyMac:");
    String myMacStr = WiFi.macAddress();
    log(myMacStr.c_str());

    char peerMacStr[18];
    snprintf(peerMacStr, sizeof(peerMacStr), "%02X:%02X:%02X:%02X:%02X:%02X", peer_mac[0], peer_mac[1], peer_mac[2], peer_mac[3], peer_mac[4], peer_mac[5]);
    log("PeerMAC:");
    log(peerMacStr);

    if (esp_now_init() != ESP_OK)
    {
        log("Error initializing ESP-NOW");
        return;
    }

    esp_now_register_recv_cb(OnDataRecv);

    // Register peer
    // esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, peer_mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        log("Failed to add peer");
        return;
    }

    log("ESP-NOW Motor Ready.");
}

EspNowTask::EspNowTask()
{
}

EspNowTask::~EspNowTask()
{
}

void EspNowTask::run()
{
}

void EspNowTask::setLogger(Logger *logger)
{
    logger_ = logger;
}

void EspNowTask::log(const char *msg)
{
    if (logger_ != nullptr)
    {
        logger_->log(msg);
    }
}