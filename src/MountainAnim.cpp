// ============================================================
// 爬坡动画模块 · 实现
// 设计基准：内部以"宽 w、高 h"的局部坐标系绘制（默认 240x80）。
// 山脊折线按 h 比例缩放：山脚在画布底（y=h），山顶平台在顶部附近。
// 推屏用透明色按键 pushSprite(x, y, transp)，透出宿主内容。
// ============================================================
#include "MountainAnim.h"
#include <math.h>   // sinf：摆臂相位

// ---- 颜色（RGB565，与模拟器 hex 对应）----
const uint16_t MountainAnim::_mountain   = 0x2B47;   // #2B6B3C 山体深岩绿
const uint16_t MountainAnim::_rock[6]    = { 0x33C8, 0x3C29, 0x448B, 0x54CC, 0x5D4E, 0x6DAF };
const uint16_t MountainAnim::_snow       = 0xF7BE;   // #F2F7F4 雪帽
const uint16_t MountainAnim::_crack      = 0x1A45;   // #1E4A2E 岩石裂缝
const uint16_t MountainAnim::_ridgeColor = 0xFFFF;   // 山脊受光棱线
const uint16_t MountainAnim::_person     = 0xE1C6;   // #E53935 红色小人
const uint16_t MountainAnim::_cape       = 0x1338;   // #1565C0 披风（深蓝）
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

// ---- 画小人（站在 _personX，双脚贴合坡面，侧身 + 摆臂）----
void MountainAnim::drawPerson()
{
    // 缩放系数
    float sx = (float)_w / 240.0f;
    float scale = (float)_h / 80.0f;

    // 左右脚分别踩在各自 x 的坡面高度上
    int gyL = groundAt(_personX - (int)(6 * sx));
    int gyR = groundAt(_personX + (int)(6 * sx));
    int gyMid = (gyL + gyR) / 2;

    // 基准尺寸（高 80 时）：头半径 7、头中心 gyMid-24、身 gyMid-15~-8
    int hr = (int)(7 * scale);
    if (hr < 2) hr = 2;

    // —— 披风（画在身体后面）：锚在脖子后，向后下方展开，随错相位飘动 ——
    float capePh = (float)(_personX - _minX) / 24.0f * 6.2831853f + 0.9f;   // 与摆臂错开 ~0.9 相位
    int   capeFlap = (int)(sinf(capePh) * 3 * sx);                          // 飘动幅度（横向）
    _canvas.fillTriangle(
        _personX + (int)(2 * sx),                          gyMid - (int)(16 * scale),          // 锚点（脖子后）
        _personX - (int)(9 * sx) + capeFlap,               gyMid - (int)(10 * scale) + (int)(capeFlap * 0.4),   // 后上角
        _personX - (int)(7 * sx) + capeFlap,               gyMid - (int)(3 * scale) + (int)(capeFlap * 0.6),    // 后下角
        _cape);

    // —— 登顶庆祝：右手高举握拳（拳头在头顶上方），左手自然下垂，身体挺直，双脚站定 ——
    if (_celebrating) {
        int hx = _personX + (int)(4 * sx), hy = gyMid - (int)(26 * scale);
        int n2x = _personX + (int)(2 * sx), n2y = gyMid - (int)(17 * scale);
        int t2x = _personX - (int)(1 * sx), t2y = gyMid - (int)(9 * scale);
        // 右手高举握拳：上臂→前臂→拳头小圆（抬到头顶上方 gyMid-35，半径2，不重叠头）
        int fu1x = _personX + (int)(7 * sx), fu1y = gyMid - (int)(27 * scale);   // 上臂肘
        int fu2x = _personX + (int)(8 * sx), fu2y = gyMid - (int)(35 * scale);   // 前臂末端（拳头中心）
        int fistR = (int)(2 * scale);                                             // 拳头半径
        if (fistR < 1) fistR = 1;
        // 左手自然下垂（两段）
        int dl1x = _personX - (int)(3 * sx), dl1y = gyMid - (int)(15 * scale);   // 左上臂
        int dl2x = _personX - (int)(4 * sx), dl2y = gyMid - (int)(12 * scale);   // 左前臂
        _canvas.fillCircle(hx, hy, hr, _person);                                  // 头
        _canvas.drawLine(hx, gyMid - (int)(19 * scale), n2x, n2y, _person);       // 脖子
        _canvas.drawLine(n2x, n2y, t2x, t2y, _person);                            // 躯干（挺直）
        _canvas.drawLine(n2x, n2y, fu1x, fu1y, _person);                          // 右上臂
        _canvas.drawLine(fu1x, fu1y, fu2x, fu2y, _person);                        // 右前臂
        _canvas.fillCircle(fu2x, fu2y, fistR, _person);                           // 握拳（头顶上方）
        _canvas.drawLine(n2x, n2y, dl1x, dl1y, _person);                          // 左上臂
        _canvas.drawLine(dl1x, dl1y, dl2x, dl2y, _person);                        // 左前臂
        _canvas.drawLine(t2x, t2y, _personX - (int)(4 * sx), gyL, _person);       // 左腿（站定）
        _canvas.drawLine(t2x, t2y, _personX + (int)(4 * sx), gyR, _person);       // 右腿（站定）
        return;
    }

    // 摆臂相位：随移动距离推进（每走 24px 完成一个周期），停止即静止
    float ph = (float)(_personX - _minX) / 24.0f * 6.2831853f;   // 0~2π
    float s  = sinf(ph);                                          // -1~1，决定前后摆

    // 侧身攀爬姿态（面朝右）：头略前探、躯干挺拔微前倾、手臂绕肩前后弧线摆（钟摆式）、双腿跨步交替
    int hx  = _personX + (int)(4 * sx);                   // 头中心（略前探）
    int hy  = gyMid - (int)(24 * scale);
    int n2x = _personX + (int)(2 * sx), n2y = gyMid - (int)(15 * scale);  // 脖子下端（肩）
    int t1x = _personX + (int)(2 * sx), t1y = gyMid - (int)(15 * scale);  // 躯干上端（肩）
    int t2x = _personX - (int)(1 * sx), t2y = gyMid - (int)(8 * scale);   // 躯干下端（髋）

    // —— 前后摆臂：手绕肩做钟摆式弧线摆动（不是水平平移）——
    float L       = 9.0f * sx;             // 臂长（缩小）
    float SWING_A = 0.6f;                  // 摆动角幅度 (弧度) ≈ ±34°
    float BASE_A  = 0.2f;                  // 自然下垂略前倾 (弧度)
    int   sxj = t1x, syj = t1y;            // 肩
    // 前臂角：BASE + SWING*sin；后臂角：-BASE - SWING*sin（反相 180°）
    float aF = BASE_A + SWING_A * s;
    int hfX = sxj + (int)(L * sinf(aF)), hfY = syj + (int)(L * cosf(aF));   // 前手（弧线位）
    float aB = -BASE_A - SWING_A * s;
    int hbX = sxj + (int)(L * sinf(aB)), hbY = syj + (int)(L * cosf(aB));   // 后手（弧线位）
    // 肘：肩→手 50% 处（两段式，随弧线跟随）
    int efX = sxj + (hfX - sxj) / 2, efY = syj + (hfY - syj) / 2;
    int ebX = sxj + (hbX - sxj) / 2, ebY = syj + (hbY - syj) / 2;

    // —— 迈腿（阶段2 v2）：双腿从髋向两侧大幅交替，跨步腿抬起，支撑腿踩坡 ——
    int SWING_LEG = (int)(5 * sx);                         // 脚摆动幅度 (px)，跨越身体中线（缩小）
    int LIFT      = (int)(3 * scale);                      // 迈步腿抬升 (px)
    // 左腿：x = 髋 + 6*sin(ph)（s=+1 最前）；右腿反相（s=+1 最后）
    int lFootX = t2x + (int)(SWING_LEG * s);
    int rFootX = t2x - (int)(SWING_LEG * s);
    // 抬腿：正在向前摆的那条腿抬起（cos ph>0 左腿抬，<0 右腿抬），另一条踩坡
    float cph = cosf(ph);
    int lLift = (cph > 0 ? (int)(cph * LIFT) : 0);
    int rLift = (cph < 0 ? (int)(-cph * LIFT) : 0);
    int lFootY = groundAt(lFootX) - lLift;
    int rFootY = groundAt(rFootX) - rLift;
    // 膝：髋→脚中点，微微上抬保持弯曲（两段式）
    int lKneeX = (t2x + lFootX) / 2, lKneeY = (t2y + lFootY) / 2 - (int)(2 * scale);
    int rKneeX = (t2x + rFootX) / 2, rKneeY = (t2y + rFootY) / 2 - (int)(2 * scale);

    _canvas.fillCircle(hx, hy, hr, _person);                                  // 头
    _canvas.drawLine(hx, gyMid - (int)(17 * scale), n2x, n2y, _person);       // 脖子（下巴→肩）
    _canvas.drawLine(t1x, t1y, t2x, t2y, _person);                            // 躯干（挺拔，微前倾）
    _canvas.drawLine(sxj, syj, efX, efY, _person);                            // 前臂上段
    _canvas.drawLine(efX, efY, hfX, hfY, _person);                            // 前臂下段→前手
    _canvas.drawLine(sxj, syj, ebX, ebY, _person);                            // 后臂上段
    _canvas.drawLine(ebX, ebY, hbX, hbY, _person);                            // 后臂下段→后手
    _canvas.drawLine(t2x, t2y, lKneeX, lKneeY, _person);                      // 左腿上段
    _canvas.drawLine(lKneeX, lKneeY, lFootX, lFootY, _person);                // 左腿下段→脚
    _canvas.drawLine(t2x, t2y, rKneeX, rKneeY, _person);                      // 右腿上段
    _canvas.drawLine(rKneeX, rKneeY, rFootX, rFootY, _person);                // 右腿下段→脚
}

// ---- 自动爬坡：每 tick 前进 stepPx，到山顶后庆祝片刻再循环 ----
void MountainAnim::tick(int stepPx)
{
    if (_celebrating) {
        _celebrateTicks++;
        if (_celebrateTicks >= 50) {          // 庆祝约 50 帧（≈1.7s）
            _celebrating = false;
            _celebrateTicks = 0;
            _personX = _minX;                 // 回到山脚重新爬
        }
    } else {
        _personX += stepPx;
        if (_personX >= _maxX) {              // 到达山顶 → 进入庆祝
            _personX = _maxX;
            _celebrating = true;
            _celebrateTicks = 0;
        }
    }
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