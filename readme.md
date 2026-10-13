<p align="center">
  <img src="https://image-osb.obs.cn-east-3.myhuaweicloud.com/blog/2023-12-13/print-led/r01/cover/cover-wechat-b04496e1ff55.webp" alt="深色封面：左侧是带彩色料盘的打印机和正在走近的人，右侧大字是竹子智控增高架，下面三枚标签为多色打印、任务检测、人体检测" width="100%">
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-111111?style=flat-square" alt="MIT License"></a>
  <img src="https://img.shields.io/badge/ESP8266-111111?style=flat-square&logo=espressif&logoColor=white" alt="ESP8266">
  <img src="https://img.shields.io/badge/C%2B%2B-111111?style=flat-square&logo=cplusplus&logoColor=white" alt="C++">
  <img src="https://img.shields.io/badge/Arduino-111111?style=flat-square&logo=arduino&logoColor=white" alt="Arduino">
  <img src="https://img.shields.io/badge/PlatformIO-111111?style=flat-square&logo=platformio&logoColor=white" alt="PlatformIO">
  <img src="https://img.shields.io/badge/ArduinoJson-7-111111?style=flat-square" alt="ArduinoJson 7">
</p>

<p align="center">
  <b>拓竹 P1S 增高架的灯带控制固件。</b><br>
  换上自己的 Wi-Fi、静态 IP 和 MQTT 服务器，烧进 ESP-01，灯带会按机内动作和机外人体自动开关，也能远程控制。
</p>

<p align="center">
  <a href="#功能">功能</a> ·
  <a href="#跑起来">跑起来</a> ·
  <a href="#改成你的">改成你的</a> ·
  <a href="#目录">目录</a> ·
  <a href="#遇到问题时">遇到问题时</a> ·
  <a href="#交流与贡献">交流与贡献</a>
</p>

## 这是什么

这个仓库是拓竹 P1S 智控增高架的灯带固件，给已经接好 ESP-01、继电器和两路红外的人编译上传。板型是 PlatformIO 的 `esp01_1m`。

仓库交付的是固件工程：控制逻辑在 `src/main.cpp`，Wi-Fi 示例在 `include/config.example.h`。用 PlatformIO 编译，通过串口烧进模块。固件驱动灯带继电器。GPIO0 目前只定义了常量 `Relay2PIN`。

配套的制作说明放在文末资料入口。

## 功能

继电器接在 GPIO2。`State` 为 0 时引脚拉低，灯带打开；为 1 时引脚拉高，灯带关闭。上电后会先打开灯带。

| 能力 | 结果 |
| --- | --- |
| **机内动作** | GPIO1 连续读到高电平就开灯，并刷新机内计时 |
| **延时后再看人** | 机内连续 30 秒没有高电平之后，才读取 GPIO3 |
| **机外人体** | 此时 GPIO3 为高就开灯；从未触发过机外检测则关灯 |
| **人离开** | 机外最后一次高电平后再过 20 秒，关灯 |
| **远程开灯** | 指令为 `on` 时灯保持打开，自动检测暂停 |
| **远程关灯** | 指令为 `off` 时灯关闭，并立刻回到自动检测 |
| **状态上报** | 每 5 秒向 `3D_client/sub` 发布一则 JSON |
| **网页升级** | 浏览器打开 `http://设备IP/update`，上传新固件 |
| **定时重启** | 连续运行满 48 小时后调用 `ESP.restart()` |

两路红外都先等 50 毫秒再读一次，用来避开毛刺。GPIO1、GPIO3 分别是串口的 TX、RX，检测期间串口日志和烧录会和模块抢这两根线。

远程指令有两种格式。

微信小程序主题 `Wechat/pub` 接收 JSON。`info.name` 需要是 `Wechat`，`equipment.switch1` 为 `on` 或 `off`：

```json
{"info":{"name":"Wechat"},"equipment":{"switch1":"on"}}
```

Home Assistant 主题 `HA/3D_Control` 接收纯文本：`on` 开灯，其他内容关灯。

上报内容里，`info.name` 固定为 `3D_Print`。`param.relay_pin` 按 GPIO2 的电平填写：低电平是 `on`，高电平是 `off`。

```json
{"info":{"name":"3D_Print"},"param":{"relay_pin":"on"}}
```

## 跑起来

需要本机已安装 [PlatformIO](https://platformio.org/)。`platformio.ini` 使用 `espressif8266` 平台和 Arduino 框架，平台版本由 PlatformIO 在编译时解析。库依赖是 ArduinoJson `^7.0.3` 和 PubSubClient `^2.8`。用 VS Code 打开仓库时，按提示安装扩展 `platformio.platformio-ide`。

```bash
git clone https://github.com/zhanghongchen1213/DIY_3DPrint_Parts.git
cd DIY_3DPrint_Parts
cp include/config.example.h include/config.h
```

`src/main.cpp` 会包含 `config.h`。这个文件被 `.gitignore` 忽略，缺了它编译会停在包含这一步。填好 Wi-Fi、静态 IP 和 MQTT 服务器后，在仓库根目录执行：

```bash
pio run -e esp01_1m
pio run -e esp01_1m --target upload
```

环境名是 `esp01_1m`。`platformio.ini` 把 `upload_resetmethod` 设为 `nodemcu`。上传端口按本机串口适配器指定。串口日志波特率是 `9600`，对应 `Serial.begin(9600)`。

烧录时让模块进入下载模式。接在 GPIO1、GPIO3 上的红外模块先断开，避免和下载串口抢线。上传完成后接回模块，再看灯带和远程指令。

## 改成你的

示例里的地址属于 `192.168.1.0/24`。设备用静态 IP 入网，地址、网关和子网要改成你自己局域网里的值。

| 要改的内容 | 文件与当前值 |
| --- | --- |
| Wi-Fi 名称、密码 | `include/config.h` 的 `WIFI_SSID`、`WIFI_PASSWD`；示例写成 `你的WiFi名称`、`你的WiFi密码` |
| 静态 IP | 同文件的 `local_IP(192, 168, 1, 111)` |
| 网关、子网、DNS | `gateway(192, 168, 1, 1)`、`subnet(255, 255, 255, 0)`、`primaryDNS(192, 168, 1, 1)`、`secondaryDNS(8, 8, 8, 8)` |
| MQTT 服务器 | `src/main.cpp` 的 `mqtt_server`，当前是 `www.zhcmqtt.top`，端口 `1883` |
| 客户端 ID | 同文件的 `User_ID`，当前是 `3D_Print`；同一服务器上每台设备使用不同 ID |
| 三个主题 | `publish_topic` 为 `3D_client/sub`，`subscribe_topic` 为 `Wechat/pub`，`subscribe_topicb` 为 `HA/3D_Control` |

MQTT 连接只提交客户端 ID。网页升级页在浏览器里直接打开。服务器和升级页放在你自己的网络里。

改完后重新上传。串口出现 `MQTT connected` 后，用浏览器打开 `http://` 加上你写入的静态 IP，再加 `/update`。示例地址对应 `http://192.168.1.111/update`。

## 目录

```text
.
├── include/config.example.h
├── src/main.cpp
├── platformio.ini
└── .vscode/extensions.json
```

| 入口 | 位置 |
| --- | --- |
| Wi-Fi 与静态 IP | `include/config.h`，由示例文件复制，不进入版本库 |
| 引脚、MQTT 与灯控 | `src/main.cpp` |
| 板型、上传方式和库版本 | `platformio.ini` 的环境 `esp01_1m` |
| 状态上报 | 主题 `3D_client/sub` |
| 小程序指令 | 主题 `Wechat/pub` |
| Home Assistant 指令 | 主题 `HA/3D_Control` |
| 固件升级页 | `http://设备IP/update` |

引脚常量：`RelayPIN` 是 GPIO2，`TXDPIN` 是 GPIO1，`RXDPIN` 是 GPIO3，`Relay2PIN` 是 GPIO0。

## 遇到问题时

| 现象 | 先检查 |
| --- | --- |
| 串口反复打印 `Connecting to MQTT...`，浏览器打不开升级页 | `config.h` 的 Wi-Fi 和静态 IP，以及 `mqtt_server` 的 1883 端口。`mqttconnect()` 在连上之前不会返回，升级页要等这一步结束才启动 |
| 断线恢复后，远程指令没有反应 | 重连时只调用了 `mqttconnect()`，没有再次调用 `mqttSub()` |
| 刚发了关闭，灯又亮了 | 收到 `off` 后会清除云端控制标志。GPIO1 或 GPIO3 仍为高电平时，自动检测会再次开灯 |
| 上传失败，或上电后一直停在下载模式 | 上电时 GPIO0 为低会进入下载。烧录前断开 GPIO1、GPIO3 上的红外模块 |

源码里定义了 `Debug`。Wi-Fi 等待循环写在 `#ifndef Debug` 中，所以当前编译结果不会在 `setup()` 里空等网络。连接结果以串口上的 MQTT 文本为准。

## 交流与贡献

部署和使用时遇到问题，可以到 [Issues](https://github.com/zhanghongchen1213/DIY_3DPrint_Parts/issues) 提问。用这套源码做出自己的成果后，也欢迎在 Issue 里留下地址。

发现 Bug 或有明确的改动建议，请 [提交 Issue](https://github.com/zhanghongchen1213/DIY_3DPrint_Parts/issues/new)。本仓库没有单独的贡献说明和安全政策，改代码前先用 Issue 说明改动范围。不要在公开 Issue 里贴密钥、令牌或环境变量文件。

**本文资料 · 接着动手**

**鸿尘客公众号**  
扫码关注鸿尘客公众号，回复 **“查看原文”**，获取本文开源资料与配套内容。

![鸿尘客公众号二维码](https://image-osb.obs.cn-east-3.myhuaweicloud.com/wemedia/wxlogo.webp)

**鸿尘客独立站**

点击下方获取本文**开源资料**与**配套内容**  
[开源资料：https://zhcmqtt.top/article/print-led](https://zhcmqtt.top/article/print-led)

![官网](https://image-osb.obs.cn-east-3.myhuaweicloud.com/wemedia/honchic_logo.webp)

## 许可

本仓库以 **MIT License** 授权。SPDX 标识符为 `MIT`，版权人是鸿尘客（Honchic），版权年份为 2024。具有法律效力的许可全文在仓库根目录的 [LICENSE](LICENSE)。

在所有副本或实质性部分中保留上述版权声明和许可声明的前提下，可以免费使用、复制、修改、合并、发布、分发、再许可或销售本软件及其配套文档。软件按「原样」提供，不包含适销性、特定用途适用性、不侵权等任何明示或暗示担保。作者与版权人不对因使用或无法使用本软件引起的索赔、损害或其他责任负责。

本授权覆盖本仓库中的源代码、配置、脚本和文档。第三方依赖保留各自许可证，不因本仓库采用 MIT 而改变。
