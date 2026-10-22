# OV RandomSynth (VCV Rack)

**语言 / Language:** [中文](#zh) | [English](#en)

---

<a id="zh"></a>

## 中文

一个**粒子碰撞驱动的 VCV Rack 生成式音序器**。

小球在封闭场地内运动、碰撞。每次碰撞都会交换音高、包络和效果属性，生成波、场和火花，并输出 V/OCT、GATE、TRIG、FX 和 PAN 信号。PAN 取触发小球的 X 位置（-5V…+5V）。它不是固定序列，而是持续变化、随机但不会跑调的生成式音乐。

这是 VCV Rack 模块的专用源码仓库。包含 JUCE 独立版和其他原型的完整项目在：
https://github.com/ovikveber/RandomSynth

### 功能

- 默认 12 个小球（最多 32 个），在封闭场地内运动并碰撞
- 三类小球，按比例分布：Pitch、ADSR、Effect
- 碰撞会交换属性，并生成波、场和火花
- 音高范围 0.5–8 八度，带 `FINE` 开关；关闭 FINE 时按 0.5 八度步进
- 音阶量化：
  - `FREE`：完全随机音高，可输出微分音
  - Major、Minor、Dorian、Phrygian、Lydian、Mixolydian、Locrian、Harmonic Minor、Melodic Minor
  - 五声音阶、Blues、Whole Tone、Diminished、Chromatic
  - 5-limit 纯律：Just Major / Just Minor
  - 微分音 EDO：24、31、48、72 EDO
- 场地形状：圆形、矩形、椭圆、六边形
- 场地缩放、小球大小、小球速度、水波速度、水波范围
- 附近小球之间会共享速度
- Booster 加速器
- 输出：**V/OCT**、**GATE**、**TRIG**、**FX**、**PAN**（-5V…+5V，随小球 X 位置）
- 波规则：只有初始波能在墙上生成 **1 个子波**，子波不再继续生成
- 面板 5 个输出插座平均间距排列；PAN 在最右侧

### 安装

在 [Releases](https://github.com/ovikveber/RandomSynth-VCV/releases) 页面下载 `.vcvplugin`，直接拖进 VCV Rack 2.x 窗口。

注意：源码文件夹不能直接拖进 Rack，Rack 只识别编译好的 `.vcvplugin`，或包含 `plugin.dll` + `plugin.json` 的插件文件夹。

### 从源码构建

需要 VCV Rack SDK 2.x、`make`、C++17 编译器（Windows 下推荐 MSYS2 MinGW-w64）。

```sh
make RACK_DIR=/path/to/Rack-SDK
make dist RACK_DIR=/path/to/Rack-SDK
```

Windows + MSYS2 示例：

```sh
make RACK_DIR=/d/Rack-SDK/Rack-SDK
```

### AI 辅助说明

本项目在开发过程中使用了 AI 辅助，包括代码编写、重构、调试和文档撰写。使用的模型为 **DeepSeek**。所有代码均已由作者审核、测试并最终负责。

### 许可证

GPL-3.0-or-later，见 [LICENSE](LICENSE)。

本项目是独立的开源模块，与 VCV 官方无关。

---

<a id="en"></a>

## English

A **particle-collision generative sequencer for VCV Rack**.

Balls move and collide inside a bounded field. Each collision exchanges pitch, envelope and effect properties, spawns waves/fields/sparks, and outputs V/OCT, GATE, TRIG, FX and PAN CV. PAN follows the triggering ball's X position (-5 V…+5 V). It is not a fixed sequence but an endlessly evolving, random yet tonal stream of generative music.

This is the dedicated source repository for the VCV Rack module. The full project containing the JUCE standalone version and other prototypes is at:
https://github.com/ovikveber/RandomSynth

### Features

- 12 balls by default (up to 32), moving and colliding inside a closed field
- Three ball types with weighted ratios: Pitch, ADSR, Effect
- Collisions exchange properties and create waves, fields and sparks
- Pitch range 0.5–8 octaves with a `FINE` toggle; without fine mode the knob steps in 0.5 octave increments
- Scale quantizer with:
  - `FREE` mode: continuous random pitch, including microtones
  - Major, Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Harmonic Minor, Melodic Minor
  - Pentatonic, Blues, Whole Tone, Diminished, Chromatic
  - 5-limit Just Major / Just Minor
  - Microtonal EDO grids: 24, 31, 48 and 72 EDO
- Field shapes: circle, rectangle, ellipse, hexagon
- Field scale, ball size, ball speed, wave speed, wave range
- Speed sharing between nearby balls
- Booster objects that kick balls on contact
- Outputs: **V/OCT**, **GATE**, **TRIG**, **FX**, **PAN** (-5 V…+5 V, from ball X position)
- Wave rule: only the initial wave may create **one child wave** at a wall; child waves
  never spawn further waves
- Five output jacks are evenly spaced; PAN is on the right

### Installation

Download the `.vcvplugin` from the [Releases](https://github.com/ovikveber/RandomSynth-VCV/releases) page and drag it into the VCV Rack 2.x window.

Note: the source folder cannot be dragged into Rack — only the built `.vcvplugin` file (or a plugin folder containing `plugin.dll` and `plugin.json`) can be loaded.

### Build from source

Requires VCV Rack SDK 2.x, `make` and a C++17 compiler (MSYS2 MinGW-w64 on Windows).

```sh
make RACK_DIR=/path/to/Rack-SDK
make dist RACK_DIR=/path/to/Rack-SDK
```

On Windows with MSYS2:

```sh
make RACK_DIR=/d/Rack-SDK/Rack-SDK
```

### AI Assistance

This project was developed with AI assistance, including code generation, refactoring, debugging and documentation. The model used was **DeepSeek**. All code was reviewed, tested and is maintained by the author.

### License

GPL-3.0-or-later. See [LICENSE](LICENSE).

This project is an independent open-source module and is not affiliated with VCV.
