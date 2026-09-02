// ============================================================
// 步骤 C：封装为 MountainAnim 模块 —— main.cpp 只负责
// 硬件初始化（引脚）+ 宿主内容，动画逻辑全部在模块里。
//
// 动画：小人从左下角爬上山坡到右上角平台，循环播放。
// 模块：MountainAnim（src/MountainAnim.h/.cpp）
//    - 只占用屏幕下方 1/3（默认 y=160 起，高 80）
//    - 透明色按键推屏，宿主上方内容完全不受影响
//    - 支持自动爬坡 tick() 或手动定位 drawFrame()（进度条场景）
// 引脚：SCLK=7, MOSI=6, CS=14, DC=15, RST=21, BL=22
// ============================================================
#include <LovyanGFX.hpp>
#include "MountainAnim.h"

// ---- 显示配置类（LovyanGFX 的标准写法，引脚在此配置）----
class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789   _panel_instance;
    lgfx::Bus_SPI        _bus_instance;
    lgfx::Light_PWM      _light_instance;

public:
    LGFX(void)
    {
        // 1) SPI 总线配置
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host   = SPI2_HOST;   // ESP32-C6 用 FSPI (SPI2)
            cfg.spi_mode   = 0;
            cfg.freq_write = 40000000;    // 40MHz
            cfg.pin_sclk   = 7;
            cfg.pin_mosi   = 6;
            cfg.pin_miso   = 5;           // ST7789 只写，MISO 未使用
            cfg.pin_dc     = 15;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // 2) 面板配置
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs      = 14;
            cfg.pin_rst     = 21;
            cfg.panel_width  = 240;
            cfg.panel_height = 240;
            cfg.offset_x    = 0;
            cfg.offset_y    = 0;
            cfg.invert      = true;    // 用户实测白屏→蓝底反色，ST7789 需开启反转
            cfg.rgb_order   = false;
            _panel_instance.config(cfg);
        }

        // 3) 背光配置
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl      = 22;
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 1;
            _light_instance.config(cfg);
        }

        setPanel(&_panel_instance);
        _panel_instance.setLight(&_light_instance);
    }
};

LGFX tft;                 // 全局屏幕实例
MountainAnim anim;        // 爬坡动画模块

// ---- 宿主模拟测试：画上方内容 + 动画区下方宿主背景 ----
// 真实项目里这段代码由宿主自己写；这里验证动画确实不影响宿主
static void drawHostSim()
{
    // 0) 宿主背景：铺满整屏（含动画区下方的底色，供透明区透出）
    tft.fillRect(0, 0,      240, 180, 0x2125);   // 上方宿主背景（深灰蓝，到 y=180）
    tft.fillRect(0, 180,    240, 60, 0x29A8);    // 动画区下方的宿主底色

    // 1) 顶部标题栏
    tft.fillRect(0, 0, 240, 22, 0x4D1F);
    tft.setTextColor(0x08C5);       // 深色文字
    tft.setTextSize(1);
    tft.setFont(&fonts::Font0);
    tft.setCursor(10, 6);
    tft.print("MY APP UI");

    // 2) 三个彩色"卡片"（宿主 UI 内容）
    tft.fillRect(14, 34, 66, 32, 0xE1C6);   // 红
    tft.fillRect(90, 34, 66, 32, 0x3DED);   // 绿
    tft.fillRect(166, 34, 66, 32, 0xF5C7);  // 黄
    tft.setTextColor(0xFFFF);
    tft.setCursor(18, 44); tft.print("TEMP");
    tft.setCursor(94, 44); tft.print("HR");
    tft.setCursor(170, 44); tft.print("STEP");

    // 3) 说明文字（宿主区）
    tft.setTextColor(0x9D37);
    tft.setCursor(14, 104);
    tft.print("host area: upper 3/4");
    tft.setCursor(14, 120);
    tft.print("animation must NOT touch");
    tft.setCursor(14, 136);
    tft.print("boundary y=180 below");

    // 4) 分界线
    tft.drawFastHLine(0, 179, 240, 0x6BB0);
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println("[StepC] LCD init...");

    tft.init();             // 初始化屏幕
    tft.setRotation(0);     // 0-3：根据安装方向调整
    tft.setBrightness(255); // 点亮背光

    anim.init(&tft, 0, 180, 240, 60);   // 动画区：屏幕下方 1/4（y 180~240，高 60）
}

void loop()
{
    drawHostSim();   // ① 宿主每帧重画上方内容 + 动画区下方底色
    anim.tick(2);    // ② 动画自动爬坡 + 透明推屏（每 tick 前进 2px）
    delay(30);       // 每帧 30ms ≈ 33fps
}