# MountainAnim 爬坡动画模块 · 使用说明

> 在宿主屏幕上叠加"小人爬山"动画：只占用宿主指定的一个区域（默认屏幕下方 1/3），
> 区域之外的内容（宿主 UI）完全不受影响。支持自动爬坡、手动定位（进度条场景）、任意动画区尺寸。

---

## 1. 交付物

| 文件 | 说明 |
|---|---|
| `src/MountainAnim.h` | 模块接口（必须） |
| `src/MountainAnim.cpp` | 模块实现（必须） |
| （可选）`LCD-simulator.html` | 浏览器预览工具，接入前先看效果 |

**依赖库：LovyanGFX**（模块内部使用 `lgfx::LGFX_Device` / `lgfx::LGFX_Sprite`）。

> ⚠️ 本模块要求项目使用 **LovyanGFX**。若你的项目用 **TFT_eSPI** 或其他显示库，
> 接口不兼容，需要先移植（见第 7 节）。

---

## 2. 快速接入（3 行）

```cpp
#include "MountainAnim.h"      // ① 引入头文件

MountainAnim anim;             // ② 声明动画对象（全局）

void setup() {
    tft.init();                // 你自己的屏幕初始化（引脚等你在自己的 LGFX 里配）
    anim.init(&tft);           // ③ 传入屏幕，动画区默认屏幕下方 1/3
}

void loop() {
    drawMyUI(&tft);            // 你的内容照画（上方 2/3，动画不碰）
    anim.tick();               // 动画自动爬坡 + 透明推屏（每帧调一次）
    delay(30);                 // 约 33fps
}
```

就这样。动画从山脚自动爬到山顶，然后循环；上方你的 UI 完全不受影响。

---

## 3. 接口一览

| 方法 | 作用 | 何时用 |
|---|---|---|
| `init(&tft, x=0, y=160, w=240, h=80)` | 初始化，指定动画区位置/尺寸 | `setup()` 调一次 |
| `tick(stepPx=2)` | 自动爬坡一帧（每 tick 前进 stepPx 像素） | 自动循环动画 |
| `drawFrame(x)` | 手动定位：画一帧，小人站 x 处 | 进度条/受外部控制的场景 |
| `setPersonX(x)` / `personX()` | 手动设置 / 查询小人位置 | 调试、状态读取 |
| `progressToX(0.0~1.0)` | 进度比例 → 小人位置 | 播放进度 → 小人位置 |
| `minX()` / `maxX()` | 小人可走范围（局部坐标） | 进度映射参考 |

---

## 4. 场景一：自动循环动画（默认用法）

```cpp
anim.init(&tft);       // 下方 1/3
anim.tick(2);          // 每帧前进 2px（调大更快，调小更慢）
```

小人自动从左下角爬到右上平台，到达后回到起点循环。

---

## 5. 场景二：小人 = 进度条（手动定位）

宿主完全控制小人位置 —— 比如播放音乐时，把播放进度映射到小人位置：

```cpp
// 每帧：拿播放进度（0.0 ~ 1.0），算出小人位置，画上去
float progress = player.getPosition() / player.getDuration();  // 0~1
int x = anim.progressToX(progress);   // 0 = 山脚，1 = 山顶
anim.drawFrame(x);
```

特点：
- 进度到哪，小人就到哪（会"瞬移"——跟进度条行为一致，符合预期）
- 暂停/跳转都由宿主控制，模块不干预
- 想平滑滑动：宿主每帧把 x 往目标值挪一点即可，模块不管

```cpp
// 平滑滑动示例（宿主自己写）
int target = anim.progressToX(progress);
current += (target - current) * 0.2f;   // 插值逼近
anim.drawFrame((int)current);
```

---

## 6. 调整动画区位置 / 尺寸

`init` 的 4 个参数：左上角 `(x, y)`、宽 `w`、高 `h`（均为屏幕坐标）。

```cpp
anim.init(&tft);                           // 默认：屏幕下方 1/3（y=160 起，高 80）
anim.init(&tft, 0, 200, 240, 40);          // 屏幕最底部一条小动画
anim.init(&tft, 170, 160, 70, 80);         // 只占右下角一小块
anim.init(&tft, 0, 160, 240, 120);         // 更大一点的动画区
```

山体、雪帽、小人会**按动画区尺寸自动缩放**，任意分辨率/尺寸都适用。

---

## 7. 注意事项（重要）

1. **透明色 = 纯黑（0x0000）**
   动画画布中颜色为纯黑的像素会被视为"透明"而跳过（不覆盖屏幕）。
   因此：动画画面本身不含纯黑；**宿主也不要在动画区域内画纯黑**（否则会被顶掉）。

2. **动画区下方的底色由宿主画**
   动画只画山 + 小人，山体以外的区域靠"透出"显示。宿主应在动画区画好背景
   （例如和上方 UI 一致的底色），透明区会自动露出它。

3. **只支持 LovyanGFX**
   模块依赖 `LovyanGFX.hpp`。用 TFT_eSPI 的项目需要移植（见下）。

4. **引脚与模块无关**
   SPI/背光引脚在宿主自己的 `LGFX` 配置类里设置，模块不碰硬件配置。

5. **每帧调用一次**
   `tick` / `drawFrame` 每帧调一次即可；应避免同一帧调用两次（会重复推屏）。

---

## 8. 如果宿主项目用 TFT_eSPI（移植指引）

模块与 LovyanGFX 的耦合点只有两处，移植成本很低：

1. `lgfx::LGFX_Device*` → TFT_eSPI 实例类型（TFT_eSPI 也是单例，直接全局传指针）
2. `lgfx::LGFX_Sprite` → TFT_eSPI sprite（TFT_eSPI 同名 `createSprite` / `pushSprite(x, y, transp)`，
   同样支持透明色按键，语义一致）

其余逻辑（山脊折线、插值、画山、画小人）全部是纯数值计算，可原样搬移。

---

## 9. 完整最小示例（main.cpp 参考）

```cpp
#include <LovyanGFX.hpp>
#include "MountainAnim.h"

// ---- 你自己的屏幕配置类（引脚在此）----
class LGFX : public lgfx::LGFX_Device { /* ...你的引脚配置... */ };
LGFX tft;
MountainAnim anim;

void setup() {
    tft.init();
    tft.setRotation(0);
    tft.setBrightness(255);
    anim.init(&tft);          // 默认下方 1/3
}

void loop() {
    // 宿主内容（示例：上方画一行字）
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(10, 40);
    tft.print("MY APP");

    anim.tick();              // 动画自动爬坡
    delay(30);
}
```

---

## 10. 常见问题

| 问题 | 原因 / 解决 |
|---|---|
| 动画区显示出黑色方块 | 动画区下方宿主没画底色，透明区露出的是屏幕初始黑 → 宿主先 fillRect 画背景 |
| 上方 UI 被覆盖了 | 检查 `init` 的 y 是否传小（如 0）——动画区应放在 UI 之外的区域 |
| 小人到顶后立刻跳回山脚 | 这是自动循环的默认行为；想"到顶停一下"，宿主改用 `drawFrame` 手动控制 |
| 小人不显示 | 确认 `drawFrame`/`tick` 每帧被调用，且动画区内没有纯黑背景盖住 |

---

*生成自 LCD-move 项目 · 步骤 A/B/C（局部画布 → 透明推屏 → 模块封装）*