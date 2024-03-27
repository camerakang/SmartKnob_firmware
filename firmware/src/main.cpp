#include <Arduino.h>

#include "configuration.h"
#include "display_task.h"
#include "interface_task.h"
#include "motor_task.h"

#include "esp_now_task.h"

Configuration config;

#if SK_DISPLAY
static DisplayTask display_task(1);
static DisplayTask *display_task_p = &display_task;
#else
static DisplayTask *display_task_p = nullptr;
#endif

static MotorTask motor_task(1, config);

InterfaceTask interface_task(1, motor_task, display_task_p);

#define USE_ESPNOW 1

#if USE_ESPNOW
EspNowTask espNow_task;
#else

#include <WiFi.h>
#include <PubSubClient.h>

// WiFi
const char *ssid = "DOUZHI";        // Enter your WiFi name
const char *password = "douzhi123"; // Enter WiFi password

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io"; // broker address
const char *mqtt_username = "emqx";         // username for authentication
const char *mqtt_password = "public";       // password for authentication
const int mqtt_port = 1883;                 // port of MQTT over TCP

// TODO: 注意---两台机器烧录时修改
#define USE_MOTOR_1 2

#if (USE_MOTOR_1 == 1)
const char *publish_topic = "knob/motor1";
const char *subscribe_topic = "knob/motor2";
#elif (USE_MOTOR_1 == 2)
const char *publish_topic = "knob/motor2";
const char *subscribe_topic = "knob/motor1";
#endif

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

static uint8_t mqtt_connected = 0;

float thatMotor_ = 0;

void emqx_callback(char *topic, byte *payload, unsigned int length)
{
  // interface_task.log("Message arrived in topic: ");
  // interface_task.log(topic);
  // interface_task.log("Message:");

  String recv_str = "";
  for (int i = 0; i < length; i++)
    recv_str += (char)payload[i];

  thatMotor_ = recv_str.toFloat();

  // char buf_temp[256] = {'\0'};
  // snprintf(buf_temp, sizeof(buf_temp), "%f", thatMotor_);
  // interface_task.log(buf_temp);
}

/// @brief MQTT发送数据
/// @param payload
void mqtt_publish(const char *payload)
{
  if (!mqtt_connected)
    return;
  mqtt_client.publish(publish_topic, payload);
}

float espNowGetPeerMotor()
{
  return thatMotor_;
}

float last_val = 0;
void espNowSendData(float val)
{
  if (last_val == val)
    return;
  last_val = val;
  String val_str = String(val);
  char payload[128] = {'\0'};
  val_str.toCharArray(payload, 128);
  mqtt_publish(payload);
}

void mqtt_init()
{
  // connecting to a WiFi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    interface_task.log("Connecting to WiFi..");
  }
  interface_task.log("Connected to the WiFi network");

  // connecting to a mqtt broker
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(emqx_callback);
  while (!mqtt_client.connected())
  {
    String client_id = "esp32-mqtt_client-";
    client_id += String(WiFi.macAddress());

    char buf_temp[256] = {'\0'};
    snprintf(buf_temp, sizeof(buf_temp), "The mqtt_client %s connects to the public mqtt broker", client_id);
    interface_task.log(buf_temp);

    if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password))
    {
      interface_task.log("Public emqx mqtt broker connected");
    }
    else
    {
      memset(buf_temp, '\0', sizeof(buf_temp));
      snprintf(buf_temp, sizeof(buf_temp), "failed with state %d", mqtt_client.state());
      interface_task.log(buf_temp);
      delay(2000);
    }
  }
  mqtt_client.subscribe(subscribe_topic);
  mqtt_connected = 1;
}

#endif

void setup()
{
#if SK_DISPLAY
  display_task.setLogger(&interface_task);
  display_task.begin();
  // Connect display to motor_task's knob state feed
  // 观察者模式
  // Publisher -> Listener
  // motor_task: Publisher
  // display_task: Listener
  motor_task.addListener(display_task.getKnobStateQueue());
#endif

  interface_task.begin();

  config.setLogger(&interface_task);
  config.loadFromDisk();

  interface_task.setConfiguration(&config);

  motor_task.setLogger(&interface_task);
  motor_task.begin();

#if USE_ESPNOW
  espNow_task.setLogger(&interface_task);
  espNow_task.Init();
#else
  mqtt_init();
#endif

  // Free up the Arduino loop task
  vTaskDelete(NULL);
}

void loop()
{
  // mqtt_client.loop();
}

// case1：太空人旋转屏保（动画就一直转就行，电机不用管）
// case2：多档模式（smartknob中的一个效果）显示界面改一下，变成转一档切换一张图片（logo）
// case3：音量调节模式，到头变为弹簧（与smartknob一致不用改）

// case4：指尖陀螺模式，转一下后持续旋转缓慢减速
// case5：音乐盒模式
// case6：双机联动模式，需要两个旋钮，共享位置和扭矩