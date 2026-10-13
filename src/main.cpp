//==============================================================
// 项目说明：ESP32 + DHT11 温湿度传感器 + OLED 显示屏 + Web 网页查询
// 功能：
//   1. 每隔 2 秒读取 DHT11 的温度和湿度，并显示在 OLED 屏幕上
//   2. ESP32 连接 Wi-Fi 后启动一个网页服务器，
//      在浏览器中输入 ESP32 的 IP 地址即可查看温湿度数据
//==============================================================

// ---------- 第 1 步：引入（导入）需要用到的功能库 ----------
#include "WiFi.h"                 // Wi-Fi 功能库：让 ESP32 能连接路由器
#include "ESPAsyncWebServer.h"    // 异步 Web 服务器库：让 ESP32 能当网页服务器
#include <Adafruit_Sensor.h>      // Adafruit 传感器通用库（DHT 库依赖它）
#include <DHT.h>                  // DHT 系列温湿度传感器驱动库（支持 DHT11/DHT22 等）
#include <Wire.h>                 // I2C 通信库：用来和 OLED 屏通信
#include <Adafruit_GFX.h>         // Adafruit 图形库：画字符、图形的基础库
#include <Adafruit_SSD1306.h>     // SSD1306 OLED 屏（0.96 英寸）驱动库

// ---------- 第 2 步：填写你自己的 Wi-Fi 信息 ----------
// ★ 必须修改！把下面两个值改成你家里/办公室路由器的 Wi-Fi 名称和密码 ★
const char* ssid = "HUAWEI-ETP89N_HiLink";        // 你的 Wi-Fi 名称
const char* password = "Aa123456"; // 你的 Wi-Fi 密码

// ---------- 第 3 步：硬件接线定义 ----------
// DHTPIN 是 DHT11 传感器接的那根 GPIO 引脚
// DHT11 只有 3 根线：VCC 接 3.3V，GND 接地，中间的信号线接 GPIO4
// 注意：VCC 和信号线之间要接一个 4.7K~10K 的上拉电阻
#define DHTPIN 4     // DHT 传感器信号线连接的数字引脚（GPIO4）

// 选择传感器型号（用哪种就打开哪一行的注释，# 号前的行才是生效的）：
#define DHTTYPE    DHT11     // DHT 11
//#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)

// ---------- OLED 屏（0.96 英寸 SSD1306，I2C 接口）配置 ----------
// OLED 只有 4 根线：VCC 接 3.3V，GND 接地，SDA 和 SCL 分别接下面两个引脚
#define OLED_SDA  21     // I2C 数据线 SDA，接 GPIO21
#define OLED_SCL  22     // I2C 时钟线 SCL，接 GPIO22
#define OLED_ADDRESS 0x3C   // OLED 屏的 I2C 设备地址（最常见是 0x3C，也有 0x3D）
#define OLED_WIDTH  128     // 屏幕宽度：128 个像素点
#define OLED_HEIGHT 64      // 屏幕高度：64 个像素点
#define OLED_REFRESH_MS 2000   // 刷新间隔 2000 毫秒（2 秒）。因为 DHT11 采样很慢（约 1~2 秒一次），不能读太频繁

// ---------- 第 4 步：创建各种"对象"（可以理解为创建各种硬件的"控制器"）----------
// 创建 DHT11 传感器对象：告诉库"我的传感器接在 GPIO4，型号是 DHT11"
DHT dht(DHTPIN, DHTTYPE);

// 创建 OLED 显示屏对象：告诉库"我的屏是 128x64 像素，走 I2C（&Wire）"
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// 创建 Web 服务器对象：监听 80 端口（HTTP 网页的默认端口）
AsyncWebServer server(80);

//==============================================================
// 功能函数：把温度(t)和湿度(h)显示到 OLED 屏幕上
// 参数：t = 温度（摄氏度），h = 湿度（百分比）
//==============================================================
void showReading(float t, float h) {
  display.clearDisplay();          // 先清屏（把屏幕变成空白）
  display.setTextSize(2);          // 设置字体大小为 2 号（1~5 号可选，数字越大字越大）
  display.setTextColor(SSD1306_WHITE); // 设置文字颜色为白色（OLED 屏是黑底白字）

  // ---- 显示温度：先设置打印的起始位置（0,10 表示第 10 行）----
  display.setCursor(0, 10);
  if (isnan(t)) {
    // isnan() 判断数值是不是"非数字"（NaN），读取传感器失败时会返回 NaN
    display.println("Temp: --");   // 温度读取失败，显示 "--"
  } else {
    display.print("Temp: ");       // 打印 "Temp: "
    display.print(t, 1);           // 打印温度值，保留 1 位小数（比如 25.3）
    display.println(" C");         // 打印单位 " C"，并换行
  }

  // ---- 显示湿度：起始位置（0,40 表示第 40 行）----
  display.setCursor(0, 40);
  if (isnan(h)) {
    display.println("Humi: --");   // 湿度读取失败，显示 "--"
  } else {
    display.print("Humi: ");       // 打印 "Humi: "
    display.print(h, 1);           // 打印湿度值，保留 1 位小数（比如 60.5）
    display.println(" %");         // 打印单位 " %"，并换行
  }

  display.display();               // 关键一步！把上面画的内容真正刷新显示到屏幕上
}

//==============================================================
// 功能函数：读取 DHT11 的"温度"，返回一个字符串
// 说明：DHT11 是慢速传感器，一次读取最多需要约 2 秒，
//       所以两次读取之间要间隔 1~2 秒，否则可能读取失败
//==============================================================
String readDHTTemperature() {
  // 读取温度（默认单位是摄氏度 ℃）
  float t = dht.readTemperature();
  // 如果想读华氏度（℉），把参数改成 true，例如：
  //float t = dht.readTemperature(true);

  // 检查读取是否失败（失败时 t 的值是 NaN，用 isnan() 判断）
  if (isnan(t)) {
    Serial.println("Failed to read from DHT sensor!"); // 串口打印错误信息
    return "--";   // 返回字符串 "--" 表示读取失败
  }
  else {
    Serial.println(t);   // 通过串口把读到的温度打印出来（调试用）
    return String(t);    // 把数字温度值转成字符串返回（比如 "25.3"）
  }
}

//==============================================================
// 功能函数：读取 DHT11 的"湿度"，返回一个字符串
// 用法和上面的温度函数完全一样，只是调用了 readHumidity()
//==============================================================
String readDHTHumidity() {
  // 读取湿度（单位是百分比 %）
  float h = dht.readHumidity();
  if (isnan(h)) {
    Serial.println("Failed to read from DHT sensor!"); // 读取失败
    return "--";
  }
  else {
    Serial.println(h);   // 串口打印湿度值（调试用）
    return String(h);    // 转成字符串返回（比如 "60.5"）
  }
}

//==============================================================
// Web 网页的 HTML 页面（一段很长的字符串，存到 ESP32 的 Flash 里）
// 说明1：PROGMEM 表示把这段数据存进 Flash（程序存储器），不占宝贵的 RAM 内存
// 说明2：R"rawliteral( ... )rawliteral" 是 C++ 的"原始字符串"写法，
//       里面的 HTML、引号等符号不需要逐个加转义符，代码更清晰
// 说明3：页面里的 %TEMPERATURE% 和 %HUMIDITY% 是"占位符"，
//       浏览器访问时，下面的 processor() 函数会读出传感器数据替换它们
//==============================================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">  <!-- 让手机浏览器自适应屏幕宽度 -->
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">  <!-- 引入 Font Awesome 图标库（网页上的温度计/水滴图标） -->
  <style>                             <!-- CSS 样式：控制网页外观 -->
    html {
     font-family: Arial;              /* 字体用 Arial */
     display: inline-block;
     margin: 0px auto;                /* 内容整体水平居中 */
     text-align: center;              /* 文字居中 */
    }
    h2 { font-size: 3.0rem; }         /* 标题大号字 */
    p { font-size: 3.0rem; }          /* 数据大号字 */
    .units { font-size: 1.2rem; }     /* 单位（℃、%）小号字 */
    .dht-labels{                      /* "Temperature""Humidity"标签的样式 */
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h2>ESP32 DHT Server</h2>         <!-- 网页大标题 -->
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i>   <!-- 绿色的温度计图标 -->
    <span class="dht-labels">Temperature</span>
    <span id="temperature">%TEMPERATURE%</span>   <!-- 温度占位符：稍后由 processor() 替换成真实数值 -->
    <sup class="units">&deg;C</sup>               <!-- 单位 ℃（上标小字） -->
  </p>
  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i>   <!-- 蓝色的水滴图标 -->
    <span class="dht-labels">Humidity</span>
    <span id="humidity">%HUMIDITY%</span>          <!-- 湿度占位符：稍后由 processor() 替换成真实数值 -->
    <sup class="units">&percnt;</sup>               <!-- 单位 %（上标小字） -->
  </p>
</body>
<script>
// ---- JavaScript 代码：让网页自动定时刷新 ----
// 第一个定时器：每 10 秒（10000 毫秒）向 ESP32 请求一次 /temperature 接口
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();                 // 创建一个 HTTP 请求对象
  xhttp.onreadystatechange = function() {          // 请求状态变化时的处理函数
    if (this.readyState == 4 && this.status == 200) {  // 请求完成(4)且成功(200)时：
      document.getElementById("temperature").innerHTML = this.responseText;  // 把返回的温度值填进网页
    }
  };
  xhttp.open("GET", "/temperature", true);          // 指定请求方式 GET 和网址 /temperature
  xhttp.send();                                    // 发出请求
}, 10000 ) ;

// 第二个定时器：同样每 10 秒请求一次 /humidity 接口，把湿度值填进网页
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("humidity").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/humidity", true);
  xhttp.send();
}, 10000 ) ;
</script>
</html>)rawliteral";

//==============================================================
// processor 函数：把网页里的占位符替换成 DHT11 的实时数据
// 工作原理：send_P 发送页面时会扫描 HTML，凡是 %XXX% 形式的变量，
//           都会调用这个 processor() 函数，返回它的结果
//==============================================================
String processor(const String& var){
  //Serial.println(var);   // 调试用：把每个被替换的变量名打印出来
  if(var == "TEMPERATURE"){
    return readDHTTemperature();   // 占位符是 %TEMPERATURE% → 返回实时温度
  }
  else if(var == "HUMIDITY"){
    return readDHTHumidity();      // 占位符是 %HUMIDITY% → 返回实时湿度
  }
  return String();                 // 其他不认识的占位符 → 返回空字符串
}

//==============================================================
// SETUP（初始化函数）
// 说明：setup() 只在程序上电启动时执行一次，用来做各种准备工作：
//       开串口 → 初始化 DHT11 → 初始化 OLED → 连 Wi-Fi → 设置网页路由 → 启动服务器
//==============================================================
void setup(){
  // 初始化串口，波特率 115200（调试信息从这里输出，可在串口监视器里看到）
  Serial.begin(115200);

  // 初始化 DHT11 传感器（使用前必须调用一次 begin）
  dht.begin();

  // 初始化 I2C 总线（OLED 屏通过 I2C 通信，SDA/SCL 接的是 GPIO21/GPIO22）
  Wire.begin(OLED_SDA, OLED_SCL);
  // 初始化 OLED 屏；SSD1306_SWITCHCAPVCC 表示用屏自带的升压电源
  // display.begin() 返回 false 表示没找到屏（接线/地址有问题）
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("Could not find SSD1306 OLED display, OLED disabled!");  // 串口提示：找不到屏，OLED 功能禁用
  } else {
    // 找到了屏：显示一行 "OLED Ready" 表示初始化成功
    display.clearDisplay();          // 清屏（把屏幕擦干净）
    display.setTextColor(SSD1306_WHITE);  // 文字颜色设为白色（OLED 是黑底白字）
    display.setTextSize(1);          // 字体 1 号
    display.setCursor(0, 0);         // 光标移到左上角 (0,0)
    display.println("OLED Ready");   // 打印 "OLED Ready"
    display.display();               // 关键！把内容真正刷新到屏幕上
  }

  // ---------- 连接 Wi-Fi ----------
  WiFi.begin(ssid, password);        // 用上方的 Wi-Fi 名称和密码连接路由器
  while (WiFi.status() != WL_CONNECTED) {   // 一直等待，直到连接成功
    delay(1000);                     // 每 1000 毫秒（1 秒）检查一次
    Serial.println("Connecting to WiFi..");  // 串口打印"正在连接..."
  }

  // 打印 ESP32 在局域网里分到的 IP 地址（稍后在电脑浏览器里输入它访问网页）
  Serial.println(WiFi.localIP());

  // ---------- 设置网页路由（规定浏览器访问哪个网址时，ESP32 该做什么）----------
  // 路由 1：根路径 "/"，返回温湿度网页
  // 说明：server.on(网址, 请求方式, lambda匿名函数) 是注册路由的写法，
  //       括号里 [] 开始到 } 结束的部分就是一个小函数（lambda）
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    // send_P：把存在 Flash 里的 index_html 页面发给浏览器
    // 200 是 HTTP 状态码，表示"成功"
    // 最后一个参数 processor 是"占位符替换函数"：
    // 发送时会自动把 HTML 里的 %TEMPERATURE%、%HUMIDITY% 替换成实时数据
    request->send_P(200, "text/html", index_html, processor);
  });
  // 路由 2："/temperature"，网页 JS 每 10 秒请求一次这里获取最新温度
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    // 直接把读到的温度（纯文本）返回给浏览器
    request->send_P(200, "text/plain", readDHTTemperature().c_str());
  });
  // 路由 3："/humidity"，网页 JS 每 10 秒请求一次这里获取最新湿度
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
    // 直接把读到的湿度（纯文本）返回给浏览器
    request->send_P(200, "text/plain", readDHTHumidity().c_str());
  });

  // 启动 Web 服务器（上面只是"登记规则"，这里才正式开始监听 80 端口）
  server.begin();
}

//==============================================================
// LOOP（主循环函数）
// 说明：loop() 会不停地一遍一遍地循环执行，
//       负责：读 DHT11 传感器 → 显示到 OLED 屏 → 等待 2 秒 → 再来一次
//==============================================================
void loop(){
  // 读取 DHT11 的温度（℃）和湿度（%）
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    // isnan() 判断是不是"非数字(NaN)"，|| 表示"或"
    // 温度或湿度任意一个读取失败，就在串口提示一下
    Serial.println("Failed to read from DHT sensor!");
  }

  showReading(t, h);          // 把温度和湿度显示到 OLED 屏上（内部已处理失败显示 "--"）
  delay(OLED_REFRESH_MS);     // 等待 2000 毫秒（2 秒）。DHT11 采样慢，两次读取之间必须留间隔
}
