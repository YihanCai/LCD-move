// ============================================================
// 步骤 4：让小人爬坡动画（核心目标）
// 小人沿坡道从左下角移动到右上角，用 Sprite 双缓冲防闪烁
// 库：LovyanGFX（原生支持 ESP32-C6 + ST7789）
// 引脚：SCLK=7, MOSI=6, CS=14, DC=15, RST=21, BL=22
// ============================================================
#include <LovyanGFX.hpp>

// ---- 显示配置类（LovyanGFX 的标准写法）----
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

LGFX tft;   // 全局屏幕实例
lgfx::LGFX_Sprite canvas(&tft);   // 离屏缓冲（双缓冲，防闪烁）

// ---- 绘制山景背景（画到 canvas 而非 tft）----
void drawScene()
{
    // 1) 天空渐变（4 层色带，从深蓝到近白）
    canvas.fillRect(0, 0,   240, 45, 0x5FA8FF);  // 深蓝
    canvas.fillRect(0, 45,  240, 45, 0x8FCBFF);  // 蓝
    canvas.fillRect(0, 90,  240, 45, 0xB5DBFF);  // 浅蓝
    canvas.fillRect(0, 135, 240, 45, 0xDCEFFF);  // 近白（地平线）

    // 2) 太阳（放左上天空，避免被前景山坡遮挡）
    canvas.fillCircle(50, 48, 25, TFT_YELLOW);

    // 3) 云朵（白色圆叠加，避开太阳与山坡）
    canvas.fillCircle(95, 40, 10, TFT_WHITE);
    canvas.fillCircle(110, 34, 13, TFT_WHITE);
    canvas.fillCircle(126, 42, 10, TFT_WHITE);

    canvas.fillCircle(135, 78, 9, TFT_WHITE);
    canvas.fillCircle(148, 72, 12, TFT_WHITE);
    canvas.fillCircle(162, 79, 9, TFT_WHITE);

    canvas.fillCircle(30, 108, 8, TFT_WHITE);
    canvas.fillCircle(42, 103, 11, TFT_WHITE);
    canvas.fillCircle(55, 109, 8, TFT_WHITE);

    // 4) 远山（两层，颜色更浅，位于地平线）
    canvas.fillTriangle(0, 180, 70, 78, 150, 180, 0x9CD0B0);  // 远山1
    canvas.fillTriangle(95, 180, 150, 70, 245, 180, 0x86C39A); // 远山2

    // 5) 草地（地面）
    canvas.fillRect(0, 180, 240, 60, 0x2E8B57);

    // 6) 前景山坡（主角要爬的山）
    //    三角形：底边在屏幕底部，顶点在右上（205,45）
    //    左侧这条边就是"左下角爬到右上角"的坡道
    canvas.fillTriangle(0, 240, 205, 45, 240, 240, 0x228B22);
}

// ---- 画一个小人（stick figure，画到 canvas）----
// cx:     小人水平中心
// gyL/gyR:左脚 / 右脚踩的地面高度（让两脚分别贴合倾斜坡面）
// color:  小人颜色
void drawPerson(int cx, int gyL, int gyR, uint16_t color)
{
    // 取两脚高度的中点，作为身体/头部的垂直参考
    int gyMid = (gyL + gyR) / 2;

    // 头（圆）
    canvas.fillCircle(cx, gyMid - 46, 9, color);

    // 身体（竖线：从肩膀到髋部）
    canvas.drawLine(cx, gyMid - 36, cx, gyMid - 14, color);

    // 双臂（从肩部向下两侧展开）
    canvas.drawLine(cx, gyMid - 34, cx - 12, gyMid - 22, color);  // 左臂
    canvas.drawLine(cx, gyMid - 34, cx + 12, gyMid - 22, color);  // 右臂

    // 双腿（髋部到各自脚的坡面高度）
    canvas.drawLine(cx, gyMid - 14, cx - 8, gyL, color);          // 左腿
    canvas.drawLine(cx, gyMid - 14, cx + 8, gyR, color);          // 右腿
}

// ---- 坡道函数：给定 x，返回坡面高度 y ----
// 坡道是从 (0,240) 到 (205,45) 的直线
float slopeAt(float x) { return 240.0f - (195.0f / 205.0f) * x; }

// ---- 在 canvas 上画一帧：背景 + 处于位置 cx 的小人 ----
void drawFrame(int personX)
{
    drawScene();  // 先重画背景

    // 左右脚分别踩在各自 x 对应的坡面高度上
    int gyL = (int)slopeAt(personX - 8);
    int gyR = (int)slopeAt(personX + 8);
    drawPerson(personX, gyL, gyR, TFT_WHITE);

    canvas.pushSprite(0, 0);  // 把整帧一次性推到屏幕（无闪烁）
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println("[Step4] LCD init...");

    tft.init();             // 初始化屏幕
    tft.setRotation(0);     // 0-3：根据安装方向调整
    tft.setBrightness(255); // 点亮背光

    canvas.setColorDepth(16);     // 16 位色（RGB565）
    canvas.createSprite(240, 240); // 240x240 离屏缓冲
}

void loop()
{
    // 小人从左下角 (x=20) 爬到右上角 (x=195)，沿坡道逐帧移动
    for (int x = 20; x <= 195; x += 2) {
        drawFrame(x);
        delay(30);   // 每帧 30ms ≈ 33fps
    }

    // 到达山顶，停留片刻
    delay(1500);

    // 重新从左下角开始（循环播放）
}
