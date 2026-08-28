// ============================================================
// 步骤 4：让小人爬坡动画（与 LCD-simulator.html 同步）
// 小人沿折线山脊从左下角爬到右上角平台，用 Sprite 双缓冲防闪烁
// 库：LovyanGFX（原生支持 ESP32-C6 + ST7789）
// 引脚：SCLK=7, MOSI=6, CS=14, DC=15, RST=21, BL=22
// 画面：只画"要爬的那座山"，山+人物整体落在屏幕下端 1/3 (y 160~240)
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

// ---- 颜色（RGB565，与模拟器 hex 对应）----
static const uint16_t C_SKY_TOP   = 0x5D5F;   // #5FA8FF
static const uint16_t C_SKY_2     = 0x8E5F;   // #8FCBFF
static const uint16_t C_SKY_3     = 0xB6DF;   // #B5DBFF
static const uint16_t C_SKY_BASE  = 0xDF7F;   // #DCEFFF
static const uint16_t C_MOUNTAIN  = 0x2B47;   // #2B6B3C
static const uint16_t C_ROCK[6]   = { 0x33C8, 0x3C29, 0x448B, 0x54CC, 0x5D4E, 0x6DAF };
static const uint16_t C_SNOW      = 0xF7BE;   // #F2F7F4
static const uint16_t C_CRACK     = 0x1A45;   // #1E4A2E
static const uint16_t C_RIDGE     = 0xFFFF;   // #FFFFFF
static const uint16_t C_PERSON    = 0xE1C6;   // #E53935 红色小人

// ---- 山脊折线（矮山：山+人物整体落在屏幕下端 1/3，y 160~240）----
// 前 n-1 个点是爬坡路径，最后 [240,240] 只是山体右缘（用于闭合，不作为路径）
static const int RIDGE[][2] = {
    {  0, 240},   // 山脚（左下角）
    { 45, 233},
    { 90, 225},
    {130, 218},
    {150, 220},   // 小凹（真实山脊有起伏）
    {175, 208},
    {190, 200},
    {202, 198},   // 进入山顶（左侧）
    {215, 198},   // ← 山顶平坦段（水平台地）
    {225, 208},   // 右侧下坡
    {240, 240},   // 山体右缘（闭合用）
};
static const int RIDGE_N = sizeof(RIDGE) / sizeof(RIDGE[0]);
// 岩层向下延展量（每段山脊往下 drop 的厚度）
static const int ROCK_DROP[10] = { 10, 9, 9, 8, 8, 7, 6, 6, 5, 5 };
// 岩石裂缝：一组从山脊向下的深色短线 [x1,y1,x2,y2]
static const int CRACKS[][4] = {
    { 70, 220,  74, 231},
    {115, 208, 120, 220},
    {150, 210, 155, 220},
    {178, 184, 182, 195},
    {196, 170, 200, 182},
    { 95, 214,  99, 224},
};
static const int CRACKS_N = sizeof(CRACKS) / sizeof(CRACKS[0]);

// ---- 沿山脊折线插值：给定 x 求坡面高度 y ----
static int groundAt(int x)
{
    // 用前 RIDGE_N-1 个点（不含右缘 [240,240]）作为爬坡路径
    for (int i = 0; i < RIDGE_N - 2; i++) {
        int x1 = RIDGE[i][0], y1 = RIDGE[i][1];
        int x2 = RIDGE[i+1][0], y2 = RIDGE[i+1][1];
        if (x >= x1 && x <= x2) {
            long t = (long)(x - x1) * (y2 - y1);
            return y1 + (int)(t / (x2 - x1));
        }
    }
    return 240;
}

// ---- 画山景背景（画到 canvas 而非 tft）----
static void drawScene()
{
    // 1) 天空渐变（4 层色带，从深蓝到近白，与模拟器一致）
    canvas.fillRect(0, 0,   240, 45, C_SKY_TOP);
    canvas.fillRect(0, 45,  240, 45, C_SKY_2);
    canvas.fillRect(0, 90,  240, 45, C_SKY_3);
    canvas.fillRect(0, 135, 240, 45, C_SKY_BASE);

    // 2) 山体主轮廓（沿折线山脊 → 每段向下填到底边闭合）
    for (int i = 0; i < RIDGE_N - 1; i++) {
        int x1 = RIDGE[i][0],   y1 = RIDGE[i][1];
        int x2 = RIDGE[i+1][0], y2 = RIDGE[i+1][1];
        canvas.fillTriangle(x1, y1, x2, y2, x2, 240, C_MOUNTAIN);
        canvas.fillTriangle(x1, y1, x2, 240, x1, 240, C_MOUNTAIN);
    }

    // 3) 岩层：沿每段山脊往下延展的梯形岩石带（受光面逐层变浅）
    for (int i = 0; i < RIDGE_N - 2; i++) {
        int x1 = RIDGE[i][0],   y1 = RIDGE[i][1];
        int x2 = RIDGE[i+1][0], y2 = RIDGE[i+1][1];
        int d = ROCK_DROP[i];
        uint16_t c = C_ROCK[i < 6 ? i : 5];
        canvas.fillTriangle(x1, y1, x2, y2, x2, y2 + d, c);
        canvas.fillTriangle(x1, y1, x2, y2 + d, x1, y1 + d, c);
    }

    // 4) 雪帽（贴合山顶台地：底缘压在 y≈198~204，不悬空）
    canvas.fillTriangle(191, 204, 197, 193, 205, 188, C_SNOW);
    canvas.fillTriangle(205, 188, 214, 188, 221, 196, C_SNOW);
    canvas.fillTriangle(221, 196, 215, 202, 203, 203, C_SNOW);
    canvas.fillTriangle(191, 204, 205, 188, 221, 196, C_SNOW); // 顶部整体罩白，避免接缝
    canvas.fillTriangle(191, 204, 221, 196, 203, 203, C_SNOW);

    // 5) 岩石裂缝：几条从山脊向下的深色短线
    for (int i = 0; i < CRACKS_N; i++) {
        canvas.drawLine(CRACKS[i][0], CRACKS[i][1], CRACKS[i][2], CRACKS[i][3], C_CRACK);
    }

    // 6) 山脊受光棱线（沿整条爬坡山脊描白）
    for (int i = 0; i < RIDGE_N - 2; i++) {
        canvas.drawLine(RIDGE[i][0], RIDGE[i][1], RIDGE[i+1][0], RIDGE[i+1][1], C_RIDGE);
    }
}

// ---- 画一个小人（stick figure，与模拟器 drawPerson 同步）----
// cx: 小人水平中心；gyL/gyR: 左脚/右脚踩的地面高度；color: 颜色
static void drawPerson(int cx, int gyL, int gyR, uint16_t color)
{
    int gyMid = (gyL + gyR) / 2;   // 两脚高度中点，身体/头部的垂直参考

    // 头（圆，缩小）
    canvas.fillCircle(cx, gyMid - 25, 8, color);
    // 身体（竖线：从肩到髋）
    canvas.drawLine(cx, gyMid - 18, cx, gyMid - 10, color);
    // 双臂（从肩部两侧微微展开）
    canvas.drawLine(cx, gyMid - 17, cx - 9, gyMid - 9, color);   // 左臂
    canvas.drawLine(cx, gyMid - 17, cx + 9, gyMid - 9, color);   // 右臂
    // 双腿（髋部到各自脚的坡面高度，贴合倾斜坡面）
    canvas.drawLine(cx, gyMid - 10, cx - 6, gyL, color);         // 左腿
    canvas.drawLine(cx, gyMid - 10, cx + 6, gyR, color);         // 右腿
}

// ---- 画一帧：背景 + 处于位置 cx 的小人 ----
static void drawFrame(int personX)
{
    drawScene();   // 先重画背景

    // 左右脚分别踩在各自 x 对应的坡面高度上
    int gyL = groundAt(personX - 6);
    int gyR = groundAt(personX + 6);
    drawPerson(personX, gyL, gyR, C_PERSON);

    canvas.pushSprite(0, 0);   // 把整帧一次性推到屏幕（无闪烁）
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
    // 小人从左下角 (x=20) 爬到右上平台 (x=205)，沿山脊折线逐帧移动
    for (int x = 20; x <= 205; x += 2) {
        drawFrame(x);
        delay(30);   // 每帧 30ms ≈ 33fps
    }

    // 到达山顶（平台），停留片刻
    delay(1500);

    // 重新从左下角开始（循环播放）
}
