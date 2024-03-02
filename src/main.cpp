#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

#define Debug 0 // 1:开启调试模式 0:关闭调试模式

/*Wifi名称和密码*/
#define WIFI_SSID "ZHC_Web"
#define WIFI_PASSWD "580231580231"

/*静态IP配置*/
IPAddress local_IP(192, 168, 1, 111);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(10, 10, 10, 10);   // 可选
IPAddress secondaryDNS(10, 16, 16, 16); // 可选

/*MQQT服务器设置*/
#define mqtt_server "47.94.7.69"         // EMQX服务器的域名
#define publish_topic "3D_client/sub"    // 发布消息主题
#define subscribe_topic "Wechat/pub"     // 订阅消息主题
#define subscribe_topicb "HA/3D_Control" // 订阅消息主题

/*引脚定义*/
const int RelayPIN = 2;
const int Relay2PIN = 0;
const int TXDPIN = 1;
const int RXDPIN = 3;

/*全局状态位定义*/
bool State = 0;              // 继电器状态
bool cloudControl = 0;       // 云端控制标志位
bool CloudRelayState = 0;    // 云端发送继电器状态
String User_ID = "3D_Print"; // 用户ID，必须唯一
unsigned long lastMs = 0;    // 检测时间

/*全局变量定义*/
WiFiClient ESP8266_Client;
PubSubClient ESP8266_MQTT(ESP8266_Client);
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

/*函数定义*/
void initWifi(const char *ssid, const char *password);
void callback(char *topic, byte *payload, unsigned int length);
void mqttconnect();
void mqttPub();
void mqttSub();
void parseInfo(String message);
String rootJson();
uint8_t KEY_IN(void);
uint8_t KEY_OUT(void);

void setup()
{
  Serial.begin(9600);
  pinMode(RelayPIN, OUTPUT); // 设置继电器引脚为输出模式
#if 1
  pinMode(TXDPIN, INPUT); // 设置TXD引脚为输入模式(复用串口)
  pinMode(RXDPIN, INPUT); // 设置TXD引脚为上拉输入模式(复用串口)
#endif
  digitalWrite(RelayPIN, 0); // 开机后自动开灯

  initWifi(WIFI_SSID, WIFI_PASSWD);   // 连接Wifi
  mqttconnect();                      // 连接MQEX服务器
  mqttSub();                          // 订阅消息主题
  ESP8266_MQTT.setCallback(callback); // 设置回调监听云端下发的指令
  httpUpdater.setup(&httpServer);     // 开启HTTP OTA升级
  httpServer.begin();
#if Debug
  Serial.print("HTTPUpdateServer ready! Open http://");
  Serial.print(WiFi.localIP());
  Serial.println("/update in your browser");
  uint32_t realSize = ESP.getFlashChipRealSize();
  uint32_t ideSize = ESP.getFlashChipSize();
  Serial.printf("Flash real size: %u bytes\n\n", realSize);
  Serial.printf("Flash ide  size: %u bytes\n", ideSize);
#endif
}

void loop()
{
  httpServer.handleClient();
  static unsigned long InlastActionTime = 0;  // 内部动作时间
  static unsigned long OutlastActionTime = 0; // 外部人体动作时间
  unsigned long currentTime = millis();
  if (cloudControl) // 云端控制，停止自动检测
  {
    if (CloudRelayState == 1)
    {
      State = 0; // 开灯
    }
    else
    {
      State = 1; // 关灯
      cloudControl = 0;
    }
  }
  else // 非云端控制，自动检测
  {
    // 内部动作检测
    if (KEY_IN() == 1)
    {
      InlastActionTime = currentTime; // 更新内部最后动作时间
      State = 0;                      // 开灯
      OutlastActionTime = 0;          // 重置外部最后动作时间
    }
    else if (currentTime - InlastActionTime >= 30000)
    {
      // 如果超过60秒内部无动作，检测外部动作
      if (KEY_OUT() == 1)
      {
        OutlastActionTime = currentTime; // 更新外部最后动作时间
        State = 0;                       // 开灯
      }
      else if (OutlastActionTime > 0 && currentTime - OutlastActionTime >= 20000)
      {
        // 如果外部动作后30秒内无动作，关闭继电器
        State = 1; // 关灯
      }
      else if (OutlastActionTime == 0)
      {
        // 如果外部没有任何动作，关闭继电器
        State = 1; // 关灯
      }
    }
  }
  digitalWrite(RelayPIN, State); // 写入继电器状态，控制开关灯
  if (millis() - lastMs >= 5000) // 每5s发布主题，小程序进行显示
  {
    lastMs = millis();
    mqttPub(); // 发布消息主题
  }
  ESP8266_MQTT.loop(); // 处理接收的MQTT消息
}

/*外部传感检测*/
uint8_t KEY_IN(void)
{
  uint8_t a;
  a = 0;
  if (digitalRead(TXDPIN) == HIGH)
  {
    delay(50);
    if (digitalRead(TXDPIN) == HIGH)
    {
      a = 1;
    }
  }
  return a;
}
/*内部传感检测*/
uint8_t KEY_OUT(void)
{
  uint8_t a;
  a = 0;
  if (digitalRead(RXDPIN) == HIGH)
  {
    delay(50);
    if (digitalRead(RXDPIN) == HIGH)
    {
      a = 1;
    }
  }
  return a;
}

/*连接Wifi*/
void initWifi(const char *ssid, const char *password)
{
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
  {
#if Debug
    Serial.println("STA 配置静态IP失败");
#endif
  }
  WiFi.begin(ssid, password);
#if Debug
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi does not connect, try again ...");
    delay(1000);
  }
  Serial.println("Wifi is connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
#endif
}

/*连接EMQX服务器*/
void mqttconnect()
{
  ESP8266_MQTT.setServer(mqtt_server, 1883);
  // 连接mqtt
  while (!ESP8266_MQTT.connected())
  {
    Serial.println("Connecting to MQTT...");
    if (ESP8266_MQTT.connect(User_ID.c_str()))
    {
      Serial.println("MQTT connected");
    }
    else
    {
      Serial.print("failed with state ");
      Serial.print(ESP8266_MQTT.state());
      delay(1000);
    }
  }
}

/*发布消息主题*/
void mqttPub()
{
  char jsonBuf[192]; // 存放payload主题内容
  sprintf(jsonBuf, rootJson().c_str());
  ESP8266_MQTT.publish(publish_topic, jsonBuf);
#if Debug
  Serial.println("发布主题: ");
  Serial.println(publish_topic);
  Serial.println("发布主题消息内容: ");
  Serial.println(jsonBuf);
#endif
}

/*订阅消息主题*/
void mqttSub()
{
  ESP8266_MQTT.subscribe(subscribe_topic);
  ESP8266_MQTT.subscribe(subscribe_topicb);
}

/*监听云端下发的指令*/
void callback(char *topic, byte *payload, unsigned int length)
{
#if Debug
  Serial.print("订阅主题: ");
  Serial.println(topic);
#endif
  String message;
  for (int i = 0; i < length; i++)
  {
    message += (char)payload[i]; // convert *byte to string
  }
#if Debug
  Serial.print("订阅主题消息内容: ");
  Serial.println(message);
#endif
  if (strcmp(topic, subscribe_topic) == 0)
  {
    // 解析小程序发送的json数据
    parseInfo(message);
  }
  else if (strcmp(topic, subscribe_topicb) == 0) // HA数据解析控制
  {
    cloudControl = 1; // 开启云端控制
    if (message == "on")
      CloudRelayState = 1; // 云端发送开启灯光
    else
      CloudRelayState = 0; // 云端发送关闭灯光
  }
}

/*json数据解析*/
void parseInfo(String message)
{
  // Stream& input;

  StaticJsonDocument<384> doc;

  DeserializationError error = deserializeJson(doc, message);

  if (error)
  {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  const char *info_name = doc["info"]["name"]; // "Wechat"
  const char *info_time = doc["info"]["time"]; // "2024/2/19下午7:31:56"

  JsonObject equipment = doc["equipment"];
  const char *equipment_switch1 = equipment["switch1"]; // "off"

  String name = doc["info"]["name"].as<String>();
  String pin = doc["equipment"]["switch1"].as<String>();
  if (name == "Wechat")
  {
    cloudControl = 1; // 开启云端控制
    if (pin == "on")
    {
      CloudRelayState = 1; // 云端发送开启灯光
    }
    else
    {
      CloudRelayState = 0; // 云端发送关闭灯光
    }
  }
}

/*定义json数据为字符串发送*/
String rootJson()
{
  StaticJsonDocument<96> doc;

  doc["info"]["name"] = "3D_Print";
  doc["param"]["relay_pin"] = digitalRead(RelayPIN) == HIGH ? "off" : "on";
  String jsonCode;
  serializeJson(doc, jsonCode);
  return jsonCode;
}