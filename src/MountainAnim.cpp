// ============================================================
// 爬坡动画模块 · 实现
// 设计基准：内部以"宽 w、高 h"的局部坐标系绘制（默认 240x80）。
// 山脊折线按 h 比例缩放：山脚在画布底（y=h），山顶平台在顶部附近。
// 推屏用透明色按键 pushSprite(x, y, transp)，透出宿主内容。
// ============================================================
#include "MountainAnim.h"

// ---- 颜色（RGB565，与模拟器 hex 对应）----
const uint16_t MountainAnim::_mountain   = 0x2B47;   // #2B6B3C 山体深岩绿
const uint16_t MountainAnim::_rock[6]    = { 0x33C8, 0x3C29, 0x448B, 0x54CC, 0x5D4E, 0x6DAF };
const uint16_t MountainAnim::_snow       = 0xF7BE;   // #F2F7F4 雪帽
const uint16_t MountainAnim::_crack      = 0x1A45;   // #1E4A2E 岩石裂缝
const uint16_t MountainAnim::_ridgeColor = 0xFFFF;   // 山脊受光棱线
const uint16_t MountainAnim::_person     = 0xE1C6;   // #E53935 红色小人
const uint16_t MountainAnim::_transp     = 0x0000;   // 透明打底色（黑），画面内不出现

// ---- 岩层向下延展量 ----
const int MountainAnim::ROCK_DROP[10] = { 10, 9, 9, 8, 8, 7, 6, 6, 5, 5 };

// ---- 岩石裂缝（局部坐标，基准高 80；运行时按比例缩放 y）----
const int MountainAnim::CRACKS[6][4] = {
    { 70,  60,  74,  71},
    {115,  48, 120,  60},
    {150,  50, 155,  60},
    {178,  24, 182,  35},
    {196,  10, 200,  22},
    { 95,  54,  99,  64},
};

// ---- 初始化 ----
void MountainAnim::init(lgfx::LGFX_Device* tft, int x, int y, int w, int h)
{
    _tft = tft;
    _x = x; _y = y; _w = w; _h = h;

    // 山脊折线：以高度 h 为基准缩放（山脚在底部 y=h，平台在顶部）
    // 比例基准来自模拟器设计（高 80 时平台 y=38）
    float scale = (float)h / 80.0f;
    const float BASE_X[11]   = { 0, 45, 90, 130, 150, 175, 190, 202, 215, 225, 240 };
    const float BASE_Y[11]   = { 80, 73, 65, 58, 60, 48, 40, 38, 38, 48, 80 };
    for (int i = 0; i < RIDGE_N; i++) {
        _ridge[i][0] = (int)(BASE_X[i] * ((float)w / 240.0f));
        _ridge[i][1] = (int)(BASE_Y[i] * scale);
    }

    // 小人可走范围（局部坐标）：基准 x 20~205，按宽缩放
    _minX = (int)(20 * ((float)w / 240.0f));
    _maxX = (int)(205 * ((float)w / 240.0f));
    _personX = (_minX + _maxX) / 2;

    // 创建局部画布
    _canvas.setColorDepth(16);
    _canvas.createSprite(_w, _h);
}

// ---- 进度条换映射：0.0 ~ 1.0 → [minX, maxX] ----
int MountainAnim::progressToX(float progress) const
{
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    return _minX + (int)(progress * (_maxX - _minX));
}

// ---- 沿山脊折线插值：给定 x 求坡面高度 y（局部坐标）----
int MountainAnim::groundAt(int x) const
{
    for (int i = 0; i < RIDGE_N - 2; i++) {   // 用前 n-1 点（不含右缘）
        int x1 = _ridge[i][0],   y1 = _ridge[i][1];
        int x2 = _ridge[i+1][0], y2 = _ridge[i+1][1];
        if (x >= x1 && x <= x2) {
            long t = (long)(x - x1) * (y2 - y1);
            return y1 + (int)(t / (x2 - x1));
        }
    }
    return _h;   // 兜底：画布底
}

// ---- 画山体（画到离屏画布，局部坐标）----
void MountainAnim::drawScene()
{
    // 0) 整张动画画布先填透明色
    _canvas.fillRect(0, 0, _w, _h, _transp);

    // 1) 山体主轮廓（沿折线山脊 → 每段向下填到底边闭合）
    for (int i = 0; i < RIDGE_N - 1; i++) {
        int x1 = _ridge[i][0],   y1 = _ridge[i][1];
        int x2 = _ridge[i+1][0], y2 = _ridge[i+1][1];
        _canvas.fillTriangle(x1, y1, x2, y2, x2, _h, _mountain);
        _canvas.fillTriangle(x1, y1, x2, _h, x1, _h, _mountain);
    }

    // 2) 岩层：沿每段山脊往下延展的梯形岩石带（受光面逐层变浅）
    float scale = (float)_h / 80.0f;
    for (int i = 0; i < RIDGE_N - 2; i++) {
        int x1 = _ridge[i][0],   y1 = _ridge[i][1];
        int x2 = _ridge[i+1][0], y2 = _ridge[i+1][1];
        int d = (int)(ROCK_DROP[i] * scale);
        _canvas.fillTriangle(x1, y1, x2, y2, x2, y2 + d, _rock[i < 6 ? i : 5]);
        _canvas.fillTriangle(x1, y1, x2, y2 + d, x1, y1 + d, _rock[i < 6 ? i : 5]);
    }

    // 3) 雪帽（贴合山顶台地，局部坐标，按比例缩放）
    {
        const float SX[7] = { 191, 197, 205, 214, 221, 215, 203 };
        const float SY[7] = {  44,  33,  28,  28,  36,  42,  43 };
        // 以 (1,2) 为公共顶点分解为三角形
        int px[7], py[7];
        for (int i = 0; i < 7; i++) {
            px[i] = (int)(SX[i] * ((float)_w / 240.0f));
            py[i] = (int)(SY[i] * scale);
        }
        _canvas.fillTriangle(px[0], py[0], px[1], py[1], px[2], py[2], _snow);
        _canvas.fillTriangle(px[2], py[2], px[3], py[3], px[4], py[4], _snow);
        _canvas.fillTriangle(px[4], py[4], px[5], py[5], px[6], py[6], _snow);
        _canvas.fillTriangle(px[0], py[0], px[2], py[2], px[4], py[4], _snow);
        _canvas.fillTriangle(px[0], py[0], px[4], py[4], px[6], py[6], _snow);
    }

    // 4) 岩石裂缝（局部坐标，按比例缩放）
    for (int i = 0; i < 6; i++) {
        int x1 = (int)(CRACKS[i][0] * ((float)_w / 240.0f));
        int y1 = (int)(CRACKS[i][1] * scale);
        int x2 = (int)(CRACKS[i][2] * ((float)_w / 240.0f));
        int y2 = (int)(CRACKS[i][3] * scale);
        _canvas.drawLine(x1, y1, x2, y2, _crack);
    }

    // 5) 山脊受光棱线（沿整条爬坡山脊描白）
    for (int i = 0; i < RIDGE_N - 2; i++) {
        _canvas.drawLine(_ridge[i][0], _ridge[i][1],
                         _ridge[i+1][0], _ridge[i+1][1], _ridgeColor);
    }
}

// ---- 画小人（站在 _personX，双脚贴合坡面）----
void MountainAnim::drawPerson()
{
    // 缩放系数
    float sx = (float)_w / 240.0f;
    float scale = (float)_h / 80.0f;

    // 左右脚分别踩在各自 x 的坡面高度上
    int gyL = groundAt(_personX - (int)(6 * sx));
    int gyR = groundAt(_personX + (int)(6 * sx));
    int gyMid = (gyL + gyR) / 2;

    // 基准尺寸（高 80 时）：头半径 8、头中心 gyMid-26、身 gyMid-19~-10
    int hr = (int)(8 * scale);
    if (hr < 2) hr = 2;

    // 侧身攀爬姿态（面朝右）：头略前、躯干前倾、四肢两段弯曲
    int hx  = _personX + (int)(2 * sx);                   // 头中心
    int hy  = gyMid - (int)(26 * scale);
    int t1x = _personX + (int)(4 * sx), t1y = gyMid - (int)(19 * scale);   // 躯干上端
    int t2x = _personX - (int)(2 * sx), t2y = gyMid - (int)(10 * scale);   // 躯干下端（髋）
    int shx = _personX + (int)(4 * sx), shy = gyMid - (int)(18 * scale);   // 肩
    int fa2x = _personX + (int)(12 * sx), fa2y = gyMid - (int)(15 * scale); // 前臂肘
    int fahx = _personX + (int)(13 * sx), fahy = gyMid - (int)(13 * scale); // 前手
    int ba2x = _personX - (int)(3 * sx), ba2y = gyMid - (int)(14 * scale);  // 后臂肘
    int bahx = _personX - (int)(8 * sx), bahy = gyMid - (int)(11 * scale);  // 后手
    int fk2x = _personX + (int)(4 * sx), fk2y = gyMid - (int)(4 * scale);   // 前腿膝
    int ffx  = _personX + (int)(7 * sx);                  // 前脚（高坡 gyR）
    int bk2x = _personX - (int)(5 * sx), bk2y = gyMid - (int)(4 * scale);   // 后腿膝
    int bfx  = _personX - (int)(7 * sx);                  // 后脚（低坡 gyL）

    _canvas.fillCircle(hx, hy, hr, _person);                                  // 头
    _canvas.drawLine(t1x, t1y, t2x, t2y, _person);                            // 躯干（前倾）
    _canvas.drawLine(shx, shy, fa2x, fa2y, _person);                          // 前臂上段
    _canvas.drawLine(fa2x, fa2y, fahx, fahy, _person);                        // 前臂下段→手
    _canvas.drawLine(shx, shy, ba2x, ba2y, _person);                          // 后臂上段
    _canvas.drawLine(ba2x, ba2y, bahx, bahy, _person);                        // 后臂下段→手
    _canvas.drawLine(t2x, t2y, fk2x, fk2y, _person);                          // 前腿上段
    _canvas.drawLine(fk2x, fk2y, ffx, gyR, _person);                          // 前腿下段→脚（高坡）
    _canvas.drawLine(t2x, t2y, bk2x, bk2y, _person);                          // 后腿上段
    _canvas.drawLine(bk2x, bk2y, bfx, gyL, _person);                          // 后腿下段→脚（低坡）
}

// ---- 自动爬坡：每 tick 前进 stepPx，到终点后循环 ----
void MountainAnim::tick(int stepPx)
{
    _personX += stepPx;
    if (_personX > _maxX) _personX = _minX;   // 到山顶后回到山脚
    drawFrame(_personX);
}

// ---- 画一帧：山体 + 小人 → 透明按键推屏 ----
void MountainAnim::drawFrame(int x)
{
    _personX = x;
    drawScene();
    drawPerson();
    // 透明色按键：画布中颜色 == _transp 的像素跳过，透出宿主内容
    _canvas.pushSprite(_tft, _x, _y, _transp);
}