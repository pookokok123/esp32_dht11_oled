# ESP32 + DHT11 温湿度 + OLED 显示

ESP32 读取 DHT11 温湿度传感器，实时显示在 0.96" SSD1306 OLED 屏幕上，同时通过 WiFi + Web 网页查看数据。

## 功能

- 每 2 秒读取一次 DHT11 温湿度，刷新 OLED 显示（`Temp: xx.x C` / `Humi: xx.x %`）
- WiFi + ESPAsyncWebServer 网页实时显示温湿度（`/` 首页）
- REST 接口：`/temperature`、`/humidity`
- OLED 或传感器读取失败时显示 `--`，不影响程序运行

## 硬件接线

| 元件 | 引脚 | 说明 |
|------|------|------|
| DHT11 | GPIO 4 | VCC → 3.3V，DATA → GPIO4，GND → GND（DATA 上拉 10kΩ 到 VCC） |
| OLED (I2C) | SDA → GPIO 21 | 4 线 I2C 接口 |
| OLED (I2C) | SCL → GPIO 22 | ESP32 默认 I2C 引脚 |
| OLED (I2C) | VCC → 3.3V/5V | 按模块标注 |
| OLED (I2C) | GND → GND | |
| OLED (I2C) | RES | 接 3.3V 或 VCC（按模块说明） |
| OLED (I2C) | DC | 悬空（I2C 模式） |

> 接线不同或 I2C 地址是 0x3D 时，改 `src/main.cpp` 顶部宏：`OLED_SDA` / `OLED_SCL` / `OLED_ADDRESS`

## 项目结构

```
esp32_dht11_oled/
├── platformio.ini        # PlatformIO 配置（esp32dev, COM13, 115200）
├── src/
│   └── main.cpp          # 全部代码：DHT11 读取 + OLED 显示 + Web 服务
└── .pio/                 # 编译输出与依赖库（自动生成）
```

## 依赖库（已在 platformio.ini 声明，自动下载）

- `adafruit/Adafruit SSD1306` + `Adafruit GFX Library` — OLED 驱动
- `adafruit/DHT sensor library` — 温湿度传感器
- `ESP32Async/ESPAsyncWebServer` + `AsyncTCP` — Web 服务

## 编译 / 烧录

```powershell
# 编译
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run --project-dir E:\test_holo\test_esp32\esp32_dht11_oled

# 编译并烧录（板子接 COM13）
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload --project-dir E:\test_holo\test_esp32\esp32_dht11_oled

# 串口监视
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor --project-dir E:\test_holo\test_esp32\esp32_dht11_oled
```

## 关键配置（src/main.cpp 顶部）

```cpp
#define DHTPIN 4              // DHT11 数据脚
#define DHTTYPE DHT11         // 换成 DHT22 / DHT21 需改此行

#define OLED_SDA  21          // I2C SDA
#define OLED_SCL  22          // I2C SCL
#define OLED_ADDRESS 0x3C     // OLED I2C 地址
#define OLED_WIDTH  128       // 0.96" 屏幕
#define OLED_HEIGHT 64
#define OLED_REFRESH_MS 2000  // 刷新周期（ms），DHT11 采样周期约 1~2s

const char* ssid = "REPLACE_WITH_YOUR_SSID";        // 你的 Wi-Fi 名称
const char* password = "REPLACE_WITH_YOUR_PASSWORD"; // 你的 Wi-Fi 密码
```

## ⚠️ 注意事项

1. **WiFi 阻塞问题**：`setup()` 中 `while (WiFi.status() != WL_CONNECTED)` 在 WiFi 连不上时会**无限阻塞**，`loop()`（OLED 刷新）不会执行。要么填上正确的 WiFi，要么把等待改成限时（例如 15 秒后继续）。
2. 首次上电 OLED 显示 `OLED Ready` 测试画面，然后进入温湿度刷新。
3. 传感器读数失败时 OLED 显示 `--`，串口会打印 `Failed to read from DHT sensor!`。
4. 编译时有 ESPAsyncWebServer `send_P` 弃用警告，属原有 Web 代码，不影响运行。

## 代码逻辑流程

```
setup():
  Serial 115200 → DHT.begin() → Wire.begin(21,22) → OLED 初始化并显示 "OLED Ready"
  → WiFi 连接 → 注册 Web 路由 → server.begin()

loop():
  读温度/湿度 → 读失败打印提示 → showReading() 刷新 OLED → delay(2000ms)
```
