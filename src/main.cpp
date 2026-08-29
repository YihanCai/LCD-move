// ============================================================
// 步骤 A：局部画布 —— 动画只占屏幕下方 1/3（y 160~240）
// 画布从全屏 240x240 缩小为 240x80；RIDGE 等改用【局部坐标】
// （相当于屏幕 y 减去 160）。画面不再自带天空，山体以上为透明
// 打底色（黑）。推屏 pushSprite(0, 160)，上方宿主内容不受影响。
// 库：LovyanGFX（原生支持 ESP32-C6 + ST7789）
// 引脚：SCLK=7, MOSI=6, CS=14, DC=15, RST=21, BL=22
// ============================================================
#include <LovyanGFX.hpp>

// ---- 动画区常量 ----
static const int ANIM_Y = 160;   // 动画区在屏幕上的起始 y（下方 1/3）
static const int ANIM_H = 80;    // 动画区高度 = 240 - 160
static const uint16_t C_TRANSP = 0x0000;  // 透明打底色（黑），动画画面内不出现

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
static const uint16_t C_MOUNTAIN  = 0x2B47;   // #2B6B3C
static const uint16_t C_ROCK[6]   = { 0x33C8, 0x3C29, 0x448B, 0x54CC, 0x5D4E, 0x6DAF };
static const uint16_t C_SNOW      = 0xF7BE;   // #F2F7F4
static const uint16_t C_CRACK     = 0x1A45;   // #1E4A2E
static const uint16_t C_RIDGE     = 0xFFFF;   // #FFFFFF
static const uint16_t C_PERSON    = 0xE1C6;   // #E53935 红色小人

// ---- 山脊折线（局部坐标：山脚 = 画布底 y=80，山顶平台 y=38）----
// 屏幕坐标 = 局部坐标 + 160（如平台 (202,198) → 局部 (202,38)）
// 前 n-1 个点是爬坡路径，最后 [240,80] 只是山体右缘（用于闭合，不作为路径）
static const int RIDGE[][2] = {
    {  0,  80},   // 山脚（左下角）
    { 45,  73},
    { 90,  65},
    {130,  58},
    {150,  60},   // 小凹（真实山脊有起伏）
    {175,  48},
    {190,  40},
    {202,  38},   // 进入山顶（左侧）
    {215,  38},   // ← 山顶平坦段（水平台地）
    {225,  48},   // 右侧下坡
    {240,  80},   // 山体右缘（闭合用）
};
static const int RIDGE_N = sizeof(RIDGE) / sizeof(RIDGE[0]);
// 岩层向下延展量（每段山脊往下 drop 的厚度）
static const int ROCK_DROP[10] = { 10, 9, 9, 8, 8, 7, 6, 6, 5, 5 };
// 岩石裂缝：一组从山脊向下的深色短线 [x1,y1,x2,y2]（局部坐标）
static const int CRACKS[][4] = {
    { 70,  60,  74,  71},
    {115,  48, 120,  60},
    {150,  50, 155,  60},
    {178,  24, 182,  35},
    {196,  10, 200,  22},
    { 95,  54,  99,  64},
};
static const int CRACKS_N = sizeof(CRACKS) / sizeof(CRACKS[0]);

// ---- 沿山脊折线插值：给定 x 求坡面高度 y（局部坐标）----
static int groundAt(int x)
{
    // 用前 RIDGE_N-1 个点（不含右缘 [240,80]）作为爬坡路径
    for (int i = 0; i < RIDGE_N - 2; i++) {
        int x1 = RIDGE[i][0], y1 = RIDGE[i][1];
        int x2 = RIDGE[i+1][0], y2 = RIDGE[i+1][1];
        if (x >= x1 && x <= x2) {
            long t = (long)(x - x1) * (y2 - y1);
            return y1 + (int)(t / (x2 - x1));
        }
    }
    return 80;
}

// ---- 画山景背景（画到 canvas 而非 tft，全部局部坐标）----
static void drawScene()
{
    // 0) 整张动画画布先填透明色（替代原来动画自带的天空）
    canvas.fillRect(0, 0, 240, ANIM_H, C_TRANSP);

    // 1) 山体主轮廓（沿折线山脊 → 每段向下填到底边 y=80 闭合）
    for (int i = 0; i < RIDGE_N - 1; i++) {
        int x1 = RIDGE[i][0],   y1 = RIDGE[i][1];
        int x2 = RIDGE[i+1][0], y2 = RIDGE[i+1][1];
        canvas.fillTriangle(x1, y1, x2, y2, x2, 80, C_MOUNTAIN);
        canvas.fillTriangle(x1, y1, x2, 80, x1, 80, C_MOUNTAIN);
    }

    // 2) 岩层：沿每段山脊往下延展的梯形岩石带（受光面逐层变浅）
    for (int i = 0; i < RIDGE_N - 2; i++) {
        int x1 = RIDGE[i][0],   y1 = RIDGE[i][1];
        int x2 = RIDGE[i+1][0], y2 = RIDGE[i+1][1];
        int d = ROCK_DROP[i];
        uint16_t c = C_ROCK[i < 6 ? i : 5];
        canvas.fillTriangle(x1, y1, x2, y2, x2, y2 + d, c);
        canvas.fillTriangle(x1, y1, x2, y2 + d, x1, y1 + d, c);
    }

    // 3) 雪帽（贴合山顶台地：局部 y≈28~44，底缘压 y≈42~44，不悬空）
    canvas.fillTriangle(191,  44, 197,  33, 205,  28, C_SNOW);
    canvas.fillTriangle(205,  28, 214,  28, 221,  36, C_SNOW);
    canvas.fillTriangle(221,  36, 215,  42, 203,  43, C_SNOW);
    canvas.fillTriangle(191,  44, 205,  28, 221,  36, C_SNOW); // 顶部整体罩白，避免接缝
    canvas.fillTriangle(191,  44, 221,  36, 203,  43, C_SNOW);

    // 4) 岩石裂缝：几条从山脊向下的深色短线（局部坐标）
    for (int i = 0; i < CRACKS_N; i++) {
        canvas.drawLine(CRACKS[i][0], CRACKS[i][1], CRACKS[i][2], CRACKS[i][3], C_CRACK);
    }

    // 5) 山脊受光棱线（沿整条爬坡山脊描白）
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

// ---- 画一帧：局部画布 + 处于位置 cx 的小人（全部局部坐标）----
static void drawFrame(int personX)
{
    drawScene();   // 先重画动画画布（透明打底 + 山体）

    // 左右脚分别踩在各自 x 对应的坡面高度上（局部坐标）
    int gyL = groundAt(personX - 6);
    int gyR = groundAt(personX + 6);
    drawPerson(personX, gyL, gyR, C_PERSON);

    // 推屏：只推到屏幕下方 1/3 区域 (0,160)，上方宿主内容不受影响
    // 步骤 A：暂不带透明跳色（整块 240x80 覆盖），步骤 B 再加
    canvas.pushSprite(0, ANIM_Y);
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println("[Step4] LCD init...");

    tft.init();             // 初始化屏幕
    tft.setRotation(0);     // 0-3：根据安装方向调整
    tft.setBrightness(255); // 点亮背光

    canvas.setColorDepth(16);        // 16 位色（RGB565）
    canvas.createSprite(240, ANIM_H); // 局部画布：240x80，只覆盖屏幕下方 1/3
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
