#pragma once

#include <Arduino.h>

/*
 * 复制本文件为 include/config.h 后填写。
 * include/config.h 已加入 .gitignore，不要提交。
 */

#define WIFI_SSID "你的WiFi名称"
#define WIFI_PASSWD "你的WiFi密码"

IPAddress local_IP(192, 168, 1, 111);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 1, 1);
IPAddress secondaryDNS(8, 8, 8, 8);
