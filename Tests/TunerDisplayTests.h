#pragma once
#include "TunerControls.h"
#include <limits>

namespace tunerDisplayTests
{
inline bool run(juce::String& report)
{
    using namespace juce;
    using namespace tonegrid::tuning;
    int checks = 0; bool ok = true;
    auto check = [&](bool pass, const String& text)
    { ++checks; ok &= pass; report += String(pass ? "PASS " : "FAIL ") + "tuner display " + text + "\n"; };
    auto pitch = [](double midi) { return Reading{440 * std::pow(2., (midi - 69) / 12), .99, -15}; };
    Display display;
    check(!display.valid && !display.inTune(), "starts empty without a green indicator");
    display.advance(pitch(31.035), 1. / 60);
    check(display.valid && display.note == 31 && std::abs(display.textCents - 3.5) < .001 && display.inTune(), "G1 +3.5 cents follows reference and five-cent tolerance");
    const double initialIndicator = display.indicatorCents;
    display.advance(pitch(31.035), 1. / 60);
    check(display.indicatorCents > 0 && display.indicatorCents < initialIndicator, "indicator glides into the green centre instead of snapping");
    display.reset(); display.advance(pitch(57), 1. / 60);
    display.advance(pitch(57.2), 1. / 60);
    check(display.cents > 0 && display.cents < 5 && display.textCents == 0 && !display.inTune(), "step is eased, text held, and fresh disagreement cannot show green");
    double previous = display.cents; bool monotonic = true;
    for (int i = 0; i < 45; ++i)
    { display.advance(pitch(57.2), 1. / 60); monotonic &= display.cents >= previous && display.cents <= 20.001; previous = display.cents; }
    check(monotonic && std::abs(display.cents - 20) < .05 && std::abs(display.textCents - 20) < .11, "settles accurately without overshoot");
    Display slower; slower.advance(pitch(57), 1. / 30);
    for (int i = 0; i < 23; ++i) slower.advance(pitch(57.2), 1. / 30);
    check(std::abs(slower.cents - display.cents) < .0001, "same elapsed time gives the same smoothing at 30 and 60 Hz");
    display.reset(); display.advance(pitch(57), 1. / 60);
    double biggest = 0;
    for (int i = 0; i < 60; ++i)
    { display.advance(pitch(57 + (i % 2 ? -.04 : .04)), 1. / 60); biggest = std::max(biggest, std::abs(display.cents)); }
    check(biggest < 1, "alternating four-cent jitter is strongly damped");
    display.reset(); display.advance(pitch(57.49), 1. / 60);
    for (int i = 0; i < 30; ++i) display.advance(pitch(57 + (i % 2 ? .49 : .51)), 1. / 60);
    check(display.note == 57, "semitone boundary cannot flicker between note labels");
    display.advance(pitch(69), 1. / 60); display.advance(pitch(57), 1. / 60);
    check(display.note == 57, "one-frame octave error is rejected");
    for (int i = 0; i < 5; ++i) display.advance(pitch(64.02), 1. / 60);
    check(display.note == 64 && std::abs(display.cents - 2) < .001, "new string is accepted in 83 ms with no cross-note sweep");
    display.advance({110, .69, -15}, 1. / 60);
    check(!display.valid && !display.inTune(), "low confidence clears note and all tuning indicators");
    display.advance(pitch(57), .02); display.advance({}, .02);
    check(!display.valid, "silence clears the display");
    display.advance({std::numeric_limits<double>::quiet_NaN(), .99, -15}, .02);
    check(!display.valid, "non-finite frequency is rejected");
    display.advance({110, std::numeric_limits<double>::quiet_NaN(), -15}, .02);
    check(!display.valid, "non-finite confidence is rejected");
    display.advance({1e300, .99, -15}, .02);
    check(!display.valid, "out-of-range pitch is rejected before integer conversion");
    display.advance(pitch(57), .02); display.reset();
    check(!display.valid && display.cents == 0 && display.textCents == 0, "reopening cannot retain a previous note");
    Engine engine; AudioProcessorPlayer player; AudioTap tap(player, engine); Controls controls(engine, tap);
    controls.open();
    auto* mute = dynamic_cast<Button*>(controls.findChildWithID("tuner-mute"));
    check(mute && mute->getToggleState() && tap.isMuted() && engine.active(), "opening retains the original input analysis and mute behavior");
    if (mute) { mute->setToggleState(false, dontSendNotification); mute->onClick(); }
    check(!tap.isMuted(), "mute button restores output");
    if (mute) { mute->setToggleState(true, dontSendNotification); mute->onClick(); }
    check(tap.isMuted(), "mute button can silence output again");
    controls.close();
    check(!tap.isMuted() && !engine.active(), "closing restores output and stops the timer/analysis");
    report += String(checks) + " tuner display checks; " + (ok ? "ALL PASSED" : "FAILURES") + "\n";
    return ok;
}
}
