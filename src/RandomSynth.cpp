#include "rack.hpp"
#include "WorldCV.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>


using namespace rack;
using namespace rack::componentlibrary;

namespace
{
const NVGcolor bgColour = nvgRGBAf (0.055f, 0.07f, 0.09f, 1.0f);
const NVGcolor lcdColour = nvgRGBAf (0.035f, 0.045f, 0.06f, 1.0f);
float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

const NVGcolor textColour = nvgRGBAf (0.90f, 0.91f, 0.93f, 1.0f);
const NVGcolor dimColour = nvgRGBAf (0.52f, 0.56f, 0.63f, 1.0f);
const NVGcolor accentColour = nvgRGBAf (1.0f, 0.64f, 0.23f, 1.0f);
const NVGcolor pitchColour = nvgRGBAf (1.0f, 0.62f, 0.25f, 1.0f);
const NVGcolor adsrColour = nvgRGBAf (0.30f, 0.84f, 0.78f, 1.0f);
const NVGcolor effectColour = nvgRGBAf (0.81f, 0.42f, 1.0f, 1.0f);
const NVGcolor wallColour = nvgRGBAf (0.20f, 0.24f, 0.30f, 1.0f);

constexpr float panelW = 660.0f;
constexpr float panelH = 380.0f;
constexpr float displayX = 20.0f;
constexpr float displayY = 32.0f;
constexpr float displayW = 620.0f;
constexpr float displayH = 200.0f;
constexpr float groupTop = 258.0f;
constexpr float groupBottom = 336.0f;
constexpr float groupCaptionY = 266.0f;
constexpr float knobLabelY = 280.0f;
constexpr float knobY = 306.0f;
constexpr float buttonY = 236.0f;
constexpr float buttonH = 18.0f;
constexpr float buttonW = 120.0f;
constexpr float jackY = 352.0f;
constexpr float jackLabelY = 372.0f;
constexpr float fineX = 160.0f;
constexpr float fineY = 306.0f;
constexpr float fineLabelY = 330.0f;

constexpr float knobX[10] { 58.0f, 118.0f, 226.0f, 288.0f, 350.0f,
                            412.0f, 474.0f, 526.0f, 572.0f, 618.0f };
constexpr float jackX[5] { 66.0f, 198.0f, 330.0f, 462.0f, 594.0f };

NVGcolor kindColour (int kind)
{
    if (kind == 0) return pitchColour;
    if (kind == 1) return adsrColour;
    return effectColour;
}

std::string noteName (float midi)
{
    static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const auto n = (int) std::lround (midi);
    const auto clamped = n < 0 ? 0 : (n > 127 ? 127 : n);
    auto text = std::string (names[clamped % 12]) + std::to_string (clamped / 12 - 1);

    const auto cents = (int) std::lround ((midi - (float) n) * 100.0f);
    if (cents != 0)
        text += (cents > 0 ? "+" : "") + std::to_string (cents) + "c";

    return text;
}
}

struct PercentQuantity : rack::ParamQuantity
{
    std::string getUnit() override { return " %"; }
};

struct ScaleQuantity : rack::ParamQuantity
{
    std::string getDisplayValueString() override
    {
        return scaleName ((int) std::lround (getValue()));
    }

    std::string getUnit() override { return ""; }
};


struct RandomSynth : Module
{
    enum ParamIds
    {
        FIELD_PARAM,
        BALL_PARAM,
        SPEED_PARAM,
        WAVE_PARAM,
        PITCH_PARAM,
        ADSR_PARAM,
        FX_PARAM,
        PITCH_RANGE_PARAM,
        SCALE_PARAM,
        RANGE_FINE_PARAM,
        WAVE_SPEED_PARAM,
        NUM_PARAMS
    };

    enum OutputIds
    {
        VOCT_OUTPUT,
        GATE_OUTPUT,
        TRIG_OUTPUT,
        FX_OUTPUT,
        PAN_OUTPUT,
        NUM_OUTPUTS
    };

    World world;
    Snapshot snapshot;
    std::mutex snapshotMutex;

    std::atomic<int> resetRequest { 0 };
    std::atomic<int> addBallRequest { 0 };
    std::atomic<int> boosterRequest { 0 };
    std::atomic<bool> cycleShape { false };

    std::atomic<float> cvPitch { 0.0f };
    std::atomic<float> cvFx { 0.0f };
    std::atomic<bool> cvGate { false };
    std::atomic<bool> cvTrig { false };

    RandomSynth()
    {
        config (NUM_PARAMS, 0, NUM_OUTPUTS, 0);

        configParam (FIELD_PARAM, 0.5f, 1.0f, 1.0f, "Field size");
        configParam (BALL_PARAM, 0.3f, 1.6f, 0.55f, "Ball size");
        configParam (SPEED_PARAM, 0.1f, 3.0f, 1.0f, "Ball speed");
        configParam (WAVE_PARAM, 0.2f, 1.5f, 0.5f, "Wave range");
        configParam (PITCH_PARAM, 0.0f, 100.0f, 50.0f, "Pitch ratio");
        configParam (ADSR_PARAM, 0.0f, 100.0f, 25.0f, "ADSR ratio");
        configParam (FX_PARAM, 0.0f, 100.0f, 25.0f, "Effect ratio");
        configParam (PITCH_RANGE_PARAM, 1.0f, 16.0f, 8.0f, "Pitch range", " oct", 0.0f, 0.5f);
        configParam (RANGE_FINE_PARAM, 0.0f, 1.0f, 0.0f, "Range fine");
        configParam (WAVE_SPEED_PARAM, 0.2f, 2.0f, 0.65f, "Wave speed");
        auto* scaleQuantity = configParam<ScaleQuantity> (SCALE_PARAM, 0.0f,
                                                          (float) ((int) ScaleMode::Count - 1),
                                                          0.0f, "Scale");
        scaleQuantity->snapEnabled = true;

        configOutput (VOCT_OUTPUT, "V/OCT");
        configOutput (GATE_OUTPUT, "Gate");
        configOutput (TRIG_OUTPUT, "Trigger");
        configOutput (FX_OUTPUT, "FX send");
        configOutput (PAN_OUTPUT, "Pan");

        world.reset();
    }

    void requestReset() { resetRequest.fetch_add (1); }
    void requestAddBall() { addBallRequest.fetch_add (1); }
    void requestBooster() { boosterRequest.fetch_add (1); }
    void requestCycleShape() { cycleShape.store (true); }

    void applySettings()
    {
        const auto fieldScale = params[FIELD_PARAM].getValue();
        const auto ballScale = params[BALL_PARAM].getValue();

        if (std::abs (fieldScale - world.fieldScale) > 1.0e-4f)
            world.setFieldScale (fieldScale);

        if (std::abs (ballScale - world.getSettings().ballScale) > 1.0e-4f)
            world.setBallScale (ballScale);

        const auto fine = params[RANGE_FINE_PARAM].getValue() > 0.5f;
        if (auto* rangeQuantity = getParamQuantity (PITCH_RANGE_PARAM))
            rangeQuantity->snapEnabled = ! fine;

        const auto pitchRange = params[PITCH_RANGE_PARAM].getValue() * 0.5f;

        if (std::abs (pitchRange - world.getSettings().pitchRange) > 1.0e-4f)
            world.setPitchRange (pitchRange);

        const auto scale = (int) std::lround (params[SCALE_PARAM].getValue());
        if (scale != world.getSettings().scaleMode)
            world.setScale (scale);

        WorldSettings s;
        s.ballScale = ballScale;
        s.ballSpeed = params[SPEED_PARAM].getValue();
        s.waveRange = params[WAVE_PARAM].getValue();
        s.waveSpeed = params[WAVE_SPEED_PARAM].getValue();
        s.pitchRange = pitchRange;
        s.scaleMode = scale;
        s.ratio = { params[PITCH_PARAM].getValue() / 100.0f,
                    params[ADSR_PARAM].getValue() / 100.0f,
                    params[FX_PARAM].getValue() / 100.0f };
        world.setSettings (s);
    }

    void triggerCV (const SoundEvent& ev, float sampleRate)
    {
        const auto gateSeconds = clampf (0.05f + ev.release * (0.5f + rack::random::uniform() * 1.4f),
                                         0.05f, 2.5f);
        pitchVolts = (ev.note - 60.0f) / 12.0f;
        fxVolts = ev.send * 10.0f;
        panVolts = ev.pan * 5.0f;
        gateHigh = true;
        trigHigh = true;
        gateSamples = gateSeconds * sampleRate;
        trigSamples = 0.003f * sampleRate;
        outputs[VOCT_OUTPUT].setVoltage (pitchVolts);
        outputs[GATE_OUTPUT].setVoltage (10.0f);
        outputs[TRIG_OUTPUT].setVoltage (10.0f);
        outputs[FX_OUTPUT].setVoltage (fxVolts);
        outputs[PAN_OUTPUT].setVoltage (panVolts);
    }

    void process (const ProcessArgs& args) override
    {
        controlCounter += 1;
        if (controlCounter >= 32)
        {
            controlCounter = 0;

            if (resetRequest.exchange (0) > 0)
                world.reset();
            if (cycleShape.exchange (false))
                world.setShape ((world.shape + 1) % 4);
            if (addBallRequest.exchange (0) > 0)
                world.addBall();
            if (boosterRequest.exchange (0) > 0)
                world.addBooster();

            settingsCounter += 1;
            if (settingsCounter >= 2)
            {
                settingsCounter = 0;
                applySettings();
            }

            cvPitch.store (pitchVolts);
            cvGate.store (gateHigh);
            cvTrig.store (trigHigh);
            cvFx.store (fxVolts);
        }

        accumulator += args.sampleTime;
        constexpr float step = 1.0f / 60.0f;
        while (accumulator >= step)
        {
            accumulator -= step;
            world.update (step);

            for (const auto& ev : world.getEvents())
                triggerCV (ev, args.sampleRate);

            if (++snapshotCounter >= 2)
            {
                snapshotCounter = 0;
                std::unique_lock<std::mutex> lock (snapshotMutex, std::try_to_lock);
                if (lock.owns_lock())
                {
                    world.fillSnapshot (snapshot);
                }
            }
        }

        if (gateHigh)
        {
            gateSamples -= 1.0f;
            if (gateSamples <= 0.0f)
            {
                gateSamples = 0.0f;
                gateHigh = false;
                fxVolts = 0.0f;
                outputs[GATE_OUTPUT].setVoltage (0.0f);
                outputs[FX_OUTPUT].setVoltage (0.0f);
            }
        }

        if (trigHigh)
        {
            trigSamples -= 1.0f;
            if (trigSamples <= 0.0f)
            {
                trigSamples = 0.0f;
                trigHigh = false;
                outputs[TRIG_OUTPUT].setVoltage (0.0f);
            }
        }
    }

    float accumulator = 0.0f;
    float gateSamples = 0.0f;
    float trigSamples = 0.0f;
    float pitchVolts = 0.0f;
    float fxVolts = 0.0f;
    float panVolts = 0.0f;
    bool gateHigh = false;
    bool trigHigh = false;
    int snapshotCounter = 0;
    int settingsCounter = 0;
    int controlCounter = 0;
};

struct ActionButton : widget::Widget
{
    ActionButton (Vec pos, Vec size, std::string text, std::shared_ptr<Font> font,
                  std::function<void()> action)
        : label (std::move (text)), font (std::move (font)), onClick (std::move (action))
    {
        box.pos = pos;
        box.size = size;
    }

    void draw (const DrawArgs& args) override
    {
        const auto vg = args.vg;
        const auto r = box.size.y * 0.25f;
        nvgBeginPath (vg);
        nvgRoundedRect (vg, 0.0f, 0.0f, box.size.x, box.size.y, r);
        nvgFillColor (vg, nvgRGBAf (0.12f, 0.15f, 0.20f, 1.0f));
        nvgFill (vg);
        nvgStrokeColor (vg, nvgRGBAf (0.26f, 0.30f, 0.37f, 1.0f));
        nvgStrokeWidth (vg, 1.0f);
        nvgStroke (vg);


        if (font)
        {
            nvgFontFaceId (vg, font->handle);
            nvgFontSize (vg, 10.5f);
            nvgTextAlign (vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor (vg, textColour);
            nvgText (vg, box.size.x * 0.5f, box.size.y * 0.5f + 1.0f, label.c_str(), nullptr);
        }
    }

    void onButton (const ButtonEvent& e) override
    {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT)
        {
            if (onClick)
                onClick();
            e.consume (this);
        }
    }

    std::string label;
    std::shared_ptr<Font> font;
    std::function<void()> onClick;
};

struct PanelBackground : widget::Widget
{
    explicit PanelBackground (std::shared_ptr<Font> font) : font (std::move (font)) {}

    void draw (const DrawArgs& args) override
    {
        const auto vg = args.vg;
        const auto w = box.size.x;
        const auto h = box.size.y;

        NVGpaint bg = nvgLinearGradient (vg, 0.0f, 0.0f, w, h,
                                         nvgRGBAf (0.075f, 0.09f, 0.115f, 1.0f),
                                         nvgRGBAf (0.025f, 0.03f, 0.04f, 1.0f));
        nvgBeginPath (vg);
        nvgRect (vg, 0.0f, 0.0f, w, h);
        nvgFillPaint (vg, bg);
        nvgFill (vg);

        nvgBeginPath (vg);
        nvgRect (vg, 0.0f, 0.0f, w, 30.0f);
        nvgFillColor (vg, nvgRGBAf (0.12f, 0.055f, 0.02f, 0.45f));
        nvgFill (vg);

        auto drawGroup = [&] (float x, float gw)
        {
            nvgBeginPath (vg);
            nvgRoundedRect (vg, x, groupTop, gw, groupBottom - groupTop, 9.0f);
            nvgFillColor (vg, nvgRGBAf (1.0f, 1.0f, 1.0f, 0.022f));
            nvgFill (vg);
            nvgStrokeColor (vg, nvgRGBAf (0.30f, 0.35f, 0.43f, 0.55f));
            nvgStrokeWidth (vg, 1.0f);
            nvgStroke (vg);
        };

        drawGroup (20.0f, 170.0f);
        drawGroup (202.0f, 290.0f);
        drawGroup (504.0f, 136.0f);

        if (font)
        {
            nvgFontFaceId (vg, font->handle);
            nvgTextAlign (vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgFontSize (vg, 15.0f);
            nvgFillColor (vg, accentColour);
            nvgText (vg, 20.0f, 15.0f, "OV  RANDOM SYNTH", nullptr);

            nvgFontSize (vg, 10.0f);
            nvgFillColor (vg, dimColour);
            nvgTextAlign (vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            nvgText (vg, w - 20.0f, 15.0f, "CV / GATE / TRIG", nullptr);

            nvgFontSize (vg, 9.0f);
            nvgFillColor (vg, nvgRGBAf (0.62f, 0.66f, 0.73f, 1.0f));
            nvgTextAlign (vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText (vg, 32.0f, groupCaptionY, "TUNING", nullptr);
            nvgText (vg, 214.0f, groupCaptionY, "WORLD", nullptr);
            nvgText (vg, 516.0f, groupCaptionY, "VOICE", nullptr);
            nvgTextAlign (vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgText (vg, fineX, fineLabelY, "FINE", nullptr);

            const char* labels[] { "SCALE", "RANGE", "FIELD", "BALL", "SPEED",
                                   "WAVE", "W.SPD", "PITCH", "ADSR", "FX" };
            nvgFontSize (vg, 10.0f);
            nvgFillColor (vg, dimColour);
            nvgTextAlign (vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            for (int i = 0; i < 10; ++i)
                nvgText (vg, knobX[i], knobLabelY, labels[i], nullptr);

            const char* outLabels[] { "V/OCT", "GATE", "TRIG", "FX", "PAN" };
            for (int i = 0; i < 5; ++i)
                nvgText (vg, jackX[i], jackLabelY, outLabels[i], nullptr);
        }
    }

    std::shared_ptr<Font> font;
};

struct WorldDisplay : widget::TransparentWidget
{
    WorldDisplay (RandomSynth* module, Vec pos, Vec size, std::shared_ptr<Font> font)
        : module (module), font (std::move (font))
    {
        box.pos = pos;
        box.size = size;
    }

    void draw (const DrawArgs& args) override
    {
        const auto vg = args.vg;
        const auto w = box.size.x;
        const auto h = box.size.y;

        nvgBeginPath (vg);
        nvgRoundedRect (vg, 0.0f, 0.0f, w, h, 7.0f);
        nvgFillColor (vg, lcdColour);
        nvgFill (vg);
        nvgStrokeColor (vg, nvgRGBAf (0.18f, 0.21f, 0.27f, 1.0f));
        nvgStrokeWidth (vg, 1.0f);
        nvgStroke (vg);

        if (module == nullptr)
            return;

        Snapshot view;
        {
            std::lock_guard<std::mutex> lock (module->snapshotMutex);
            view = module->snapshot;
        }

        nvgSave (vg);
        nvgScale (vg, w / World::fieldW, h / World::fieldH);
        nvgScissor (vg, 0.0f, 0.0f, World::fieldW, World::fieldH);

        for (int i = 0; i < view.fieldCount; ++i)
        {
            const auto& f = view.fields[(size_t) i];
            const auto c = kindColour (f.kind);
            nvgBeginPath (vg);
            nvgCircle (vg, f.pos.x, f.pos.y, f.radius);
            nvgFillColor (vg, nvgRGBAf (c.r, c.g, c.b, f.strength * 0.12f));
            nvgFill (vg);
        }


        for (int i = 0; i < view.waveCount; ++i)
        {
            const auto& wv = view.waves[(size_t) i];
            const auto c = kindColour (wv.kind);
            const auto alpha = clampf (wv.amp, 0.0f, 1.0f) * 0.65f;
            nvgBeginPath (vg);
            nvgCircle (vg, wv.pos.x, wv.pos.y, wv.radius);
            nvgStrokeColor (vg, nvgRGBAf (c.r, c.g, c.b, alpha));
            nvgStrokeWidth (vg, 1.4f);
            nvgStroke (vg);
        }

        for (int i = 0; i < view.linkCount; ++i)
        {
            const auto& l = view.links[(size_t) i];
            nvgBeginPath (vg);
            nvgMoveTo (vg, l.a.x, l.a.y);
            nvgLineTo (vg, l.b.x, l.b.y);
            nvgStrokeColor (vg, nvgRGBAf (0.56f, 0.82f, 1.0f, 0.22f + 0.5f * l.amount));
            nvgStrokeWidth (vg, 1.2f);
            nvgStroke (vg);
        }


        if (view.segmentCount > 0)
        {
            nvgBeginPath (vg);
            nvgRect (vg, 0.0f, 0.0f, w, h);
            nvgMoveTo (vg, view.segments[0].a.x, view.segments[0].a.y);
            for (int i = 0; i < view.segmentCount; ++i)
                nvgLineTo (vg, view.segments[(size_t) i].b.x, view.segments[(size_t) i].b.y);
            nvgClosePath (vg);
            nvgPathWinding (vg, NVG_HOLE);
            nvgFillColor (vg, lcdColour);
            nvgFill (vg);

            for (int i = 0; i < view.segmentCount; ++i)
            {
                const auto& s = view.segments[(size_t) i];
                NVGcolor col = wallColour;
                if (s.hasProp && s.strength > 0.02f)
                {
                    const auto pc = kindColour (s.kind);
                    col = nvgRGBAf (0.20f + (pc.r - 0.20f) * s.strength,
                                    0.24f + (pc.g - 0.24f) * s.strength,
                                    0.30f + (pc.b - 0.30f) * s.strength, 1.0f);
                }
                nvgBeginPath (vg);
                nvgMoveTo (vg, s.a.x, s.a.y);
                nvgLineTo (vg, s.b.x, s.b.y);
                nvgStrokeColor (vg, col);
                nvgStrokeWidth (vg, s.strength > 0.25f ? 2.0f : 1.2f);
                nvgStroke (vg);
            }
        }


        for (int i = 0; i < view.boosterCount; ++i)
        {
            const auto& b = view.boosters[(size_t) i];
            const auto pulse = 0.5f + 0.5f * std::sin (view.elapsed * 4.0f + b.phase);
            const auto c = b.cooldown > 0.0f ? nvgRGBAf (0.55f, 0.52f, 0.38f, 1.0f)
                                              : nvgRGBAf (1.0f, 0.86f, 0.35f, 1.0f);
            nvgBeginPath (vg);
            nvgCircle (vg, b.pos.x, b.pos.y, b.radius + 1.0f + pulse * 2.0f);
            nvgStrokeColor (vg, c);
            nvgStrokeWidth (vg, 1.4f);
            nvgStroke (vg);
            for (int k = 0; k < 4; ++k)
            {
                const auto ang = view.elapsed * 0.8f + (float) k * 3.14159265f * 0.5f;
                nvgBeginPath (vg);
                nvgMoveTo (vg, b.pos.x + std::cos (ang) * (b.radius + 4.0f),
                               b.pos.y + std::sin (ang) * (b.radius + 4.0f));
                nvgLineTo (vg, b.pos.x + std::cos (ang) * (b.radius + 10.0f + pulse * 3.0f),
                               b.pos.y + std::sin (ang) * (b.radius + 10.0f + pulse * 3.0f));
                nvgStrokeColor (vg, c);
                nvgStrokeWidth (vg, 1.2f);
                nvgStroke (vg);
            }
        }


        for (int i = 0; i < view.ballCount; ++i)
        {
            const auto& b = view.balls[(size_t) i];
            const auto c = kindColour (b.kind);
            const auto glowRadius = b.radius * 2.6f;
            nvgBeginPath (vg);
            nvgCircle (vg, b.pos.x, b.pos.y, glowRadius);
            nvgFillColor (vg, nvgRGBAf (c.r, c.g, c.b, 0.12f));
            nvgFill (vg);

            nvgBeginPath (vg);
            nvgCircle (vg, b.pos.x, b.pos.y, b.radius);
            nvgFillColor (vg, b.flash > 0.0f
                                  ? nvgRGBAf (std::fmin (1.0f, c.r + 0.5f),
                                              std::fmin (1.0f, c.g + 0.5f),
                                              std::fmin (1.0f, c.b + 0.5f), 1.0f)
                                  : c);
            nvgFill (vg);
            nvgStrokeColor (vg, nvgRGBAf (1.0f, 1.0f, 1.0f, 0.55f));
            nvgStrokeWidth (vg, 0.8f);
            nvgStroke (vg);
        }

        if (font)
        {
            nvgFontFaceId (vg, font->handle);
            nvgFontSize (vg, 10.0f);
            nvgFillColor (vg, dimColour);
            nvgTextAlign (vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

            char line[96];
            std::snprintf (line, sizeof (line), "BALLS %02d   SPD %03.0f   FX %d   RNG %.1f oct   %s",
                           view.ballCount, view.averageSpeed, view.boosterCount,
                           module->params[RandomSynth::PITCH_RANGE_PARAM].getValue() * 0.5f,
                           scaleName ((int) std::lround (module->params[RandomSynth::SCALE_PARAM].getValue())));
            nvgText (vg, 10.0f, 9.0f, line, nullptr);

            const auto pitch = module->cvPitch.load();
            const auto midi = pitch * 12.0f + 60.0f;
            std::snprintf (line, sizeof (line), "OUT %-4s  GATE %s  TRIG %s",
                           noteName (midi).c_str(),
                           module->cvGate.load() ? "ON " : "OFF",
                           module->cvTrig.load() ? "ON" : "OFF");
            nvgFillColor (vg, textColour);
            nvgText (vg, 10.0f, World::fieldH - 18.0f, line, nullptr);
        }

        nvgRestore (vg);
    }

    RandomSynth* module = nullptr;
    std::shared_ptr<Font> font;
};

struct RandomSynthWidget : ModuleWidget
{
    RandomSynthWidget (RandomSynth* module)
    {
        setModule (module);
        box.size = Vec (panelW, panelH);

        const auto font = APP->window->loadFont (asset::system ("res/fonts/NotoSansSC-Medium.otf"));

        auto* panel = new PanelBackground (font);
        panel->box.size = box.size;
        addChild (panel);

        if (module != nullptr)
            addChild (new WorldDisplay (module, Vec (displayX, displayY),
                                        Vec (displayW, displayH), font));

        auto* shapeButton = new ActionButton (Vec (jackX[0] - buttonW * 0.5f, buttonY),
                                              Vec (buttonW, buttonH), "SHAPE", font,
                                              [module] { if (module) module->requestCycleShape(); });
        auto* resetButton = new ActionButton (Vec (jackX[1] - buttonW * 0.5f, buttonY),
                                              Vec (buttonW, buttonH), "RESET", font,
                                              [module] { if (module) module->requestReset(); });
        auto* addButton = new ActionButton (Vec (jackX[2] - buttonW * 0.5f, buttonY),
                                            Vec (buttonW, buttonH), "+ BALL", font,
                                            [module] { if (module) module->requestAddBall(); });
        auto* boostButton = new ActionButton (Vec (jackX[3] - buttonW * 0.5f, buttonY),
                                              Vec (buttonW, buttonH), "BOOST", font,
                                              [module] { if (module) module->requestBooster(); });
        addChild (shapeButton);
        addChild (resetButton);
        addChild (addButton);
        addChild (boostButton);

        const std::array<int, 10> paramIds { RandomSynth::SCALE_PARAM, RandomSynth::PITCH_RANGE_PARAM,
                                             RandomSynth::FIELD_PARAM, RandomSynth::BALL_PARAM,
                                             RandomSynth::SPEED_PARAM, RandomSynth::WAVE_PARAM,
                                             RandomSynth::WAVE_SPEED_PARAM, RandomSynth::PITCH_PARAM,
                                             RandomSynth::ADSR_PARAM, RandomSynth::FX_PARAM };
        for (size_t i = 0; i < paramIds.size(); ++i)
            addParam (createParamCentered<RoundBlackKnob> (Vec (knobX[i], knobY),
                                                           module, paramIds[i]));

        addParam (createParamCentered<CKSS> (Vec (fineX, fineY), module,
                                             RandomSynth::RANGE_FINE_PARAM));

        addOutput (createOutputCentered<PJ301MPort> (Vec (jackX[0], jackY), module,
                                                     RandomSynth::VOCT_OUTPUT));
        addOutput (createOutputCentered<PJ301MPort> (Vec (jackX[1], jackY), module,
                                                     RandomSynth::GATE_OUTPUT));
        addOutput (createOutputCentered<PJ301MPort> (Vec (jackX[2], jackY), module,
                                                     RandomSynth::TRIG_OUTPUT));
        addOutput (createOutputCentered<PJ301MPort> (Vec (jackX[3], jackY), module,
                                                     RandomSynth::FX_OUTPUT));
        addOutput (createOutputCentered<PJ301MPort> (Vec (jackX[4], jackY), module,
                                                     RandomSynth::PAN_OUTPUT));
    }
};

Model* modelRandomSynth = createModel<RandomSynth, RandomSynthWidget> ("RandomSynth");
