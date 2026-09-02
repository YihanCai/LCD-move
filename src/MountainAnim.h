// ============================================================
// 爬坡动画模块 · 对外接口
// 在宿主屏幕上叠加"小人爬山"动画：只占用宿主指定的一个区域
// （默认屏幕下方 1/3），区域之外的内容（宿主 UI）完全不受影响。
//
// 用法（宿主只需 3 行）：
//   MountainAnim anim;
//   anim.init(&tft);        // 初始化（默认下方 1/3）
//   anim.tick();            // 每帧调用：自动爬坡 + 透明推屏
//
// 手动定位（如"小人=音乐进度条"）：
//   int x = anim.progressToX(播放进度比例);  // 0.0~1.0 → 小人位置
//   anim.drawFrame(x);                       // 直接画一帧，位置由宿主决定
// ============================================================
#pragma once
#include <LovyanGFX.hpp>

class MountainAnim
{
public:
    // ① 初始化：传入屏幕 + 动画区位置/尺寸（默认屏幕下方 1/3）
    //    x,y = 动画区左上角（屏幕坐标）；w,h = 动画区宽高
    void init(lgfx::LGFX_Device* tft,
              int x = 0, int y = 160,
              int w = 240, int h = 80);

    // ② 每帧调用：自动爬坡（从山脚到山顶），到顶后举手庆祝片刻再循环
    //    stepPx 控制每 tick 前进的像素数（速度），默认 2
    void tick(int stepPx = 2);

    // ③ 手动定位：直接画一帧，小人站在 x 处（宿主决定位置）
    //    适合"小人=进度条"等场景；x 为动画布局部坐标（0~w）
    void drawFrame(int x);

    // ④ 手动设置/读取小人当前位置（局部坐标）
    void setPersonX(int x)            { _personX = x; }
    int  personX() const              { return _personX; }

    // ⑤ 进度条辅助：把 0.0~1.0 的进度映射到小人可走范围 [minX, maxX]
    //    0 = 山脚，1 = 山顶平台
    int  progressToX(float progress) const;

    // ⑥ 可走范围（局部坐标），宿主做进度映射时可参考
    int  minX() const                 { return _minX; }
    int  maxX() const                 { return _maxX; }

private:
    // ---- 内部实现（使用方无需关心）----
    lgfx::LGFX_Device* _tft = nullptr;
    lgfx::LGFX_Sprite  _canvas;    // 离屏画布（透明按键推屏）

    // 动画区矩形（屏幕坐标）
    int _x = 0, _y = 160, _w = 240, _h = 80;

    // 小人可走范围（局部坐标，由 w 换算）
    int _minX = 20, _maxX = 205;

    // 当前小人位置（局部坐标）
    int _personX = 100;

    // 登顶庆祝状态：到终点后举起双手欢呼，停留片刻再循环
    bool _celebrating = false;
    int  _celebrateTicks = 0;

    // 帧计数（极光流动相位推进）
    int _frame = 0;

    // 山脊折线（局部坐标，运行时按 w/h 缩放后填充）
    static const int RIDGE_N = 11;
    int _ridge[RIDGE_N][2];

    // 岩层向下延展量
    static const int ROCK_DROP[10];

    // 岩石裂缝（局部坐标）
    static const int CRACKS[6][4];

    // 颜色
    static const uint16_t _mountain, _snow, _crack, _ridgeColor, _person, _cape, _transp;
    static const uint16_t _rock[6];

    // 内部绘制
    int  groundAt(int x) const;   // 沿山脊插值求坡面高度（局部坐标）
    void drawScene();             // 画山体（透明打底 + 山 + 岩层 + 雪帽 + 裂缝 + 棱线）
    void drawPerson();            // 画小人（站在 _personX，双脚贴合坡面）
};