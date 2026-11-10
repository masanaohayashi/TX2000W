// The panel's own logic against the real engine: every press must reach the instrument as MIDI.
#include "compat/platform.h"
#include "native/panel.h"
#include "roms_dir.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using smu2000::native_engine;
using smu2000::native_panel;

static std::vector<u8> readAll (const std::string& path)
{
    std::ifstream file (path, std::ios::binary);
    return { std::istreambuf_iterator<char> (file), {} };
}

int main()
{
    const std::string roms = std::getenv ("S_MU2000_ROMS");
    auto prog = readAll (roms + "/mu2000_flash.bin");
    std::vector<u8> wave[4];
    for (int i = 0; i < 4; ++i) wave[i] = readAll (roms + "/dump/" + smu2000::kWaveRomNames[i]);
    const std::vector<u8>* const parts[4] = { &wave[0], &wave[1], &wave[2], &wave[3] };
    ::xg::set_fx_type_rom (prog);
    auto engine = std::make_unique<native_engine>();
    if (! engine->load (std::move (prog), parts)) return 1;
    const ::xg::voice_rom voices (engine->program_rom_shared());
    native_panel panel;
    float left[256], right[256];
    const auto lines = [&]
    {
        for (int i = 0; i < 8; ++i) engine->render (left, right, 256);   // Panel values travel as MIDI bytes.
        native_panel::lcd_frame frame;
        auto view = panel.capture (*engine);
        view.blink_off = false;
        native_panel::compose (view, voices, frame);
        std::string text;
        for (int row = 0; row < native_panel::ROWS; ++row)
        {
            for (int col = 0; col < native_panel::LINE_LENGTH; ++col)
            {
                const auto c = frame.text[row][col];
                text += c >= 0x20 && c < 0x7f ? char (c) : '~';
            }
            text += '|';
        }
        return text;
    };
    const auto has = [&] (const char* expected) { return lines().find (expected) != std::string::npos; };
    const auto tap = [&] (int button) { panel.press (*engine, button); panel.release (*engine, button); };

    // PLAY: voice screen with the same fields as the hardware.
    if (! has ("GrandP #01") || ! has ("000~001A01")) return 2;
    panel.turn (*engine, 1);
    if (! has ("BriteP #01") || engine->part_program (0) != 1 || engine->part_program (1) != 0) return 3;
    tap (native_panel::button_part_plus);
    if (! has ("A02")) return 4;
    tap (native_panel::button_part_minus);
    tap (native_panel::button_part_minus);
    if (! has ("/D2")) return 5;                                  // PART wraps through the A/D parts.
    tap (native_panel::button_part_plus);
    if (! has ("A01")) return 6;

    // PLAY items: each value reaches the engine and stops at its range.
    tap (native_panel::button_select_plus);
    if (! has ("Volume") || ! has ("=100")) return 7;
    panel.turn (*engine, -20);
    if (! has ("=080") || engine->part_settings (0)[0x0b] != 80) return 8;
    panel.turn (*engine, -1000);
    if (! has ("=000")) return 9;
    tap (native_panel::button_exit);
    if (! has ("BriteP")) return 10;

    // EDIT: menu, page, value, SysEx view by double ENTER.
    tap (native_panel::button_edit);
    if (panel.capture (*engine).leds() != (native_panel::LED_MU | native_panel::LED_EDIT) || ! has ("FILTER")) return 11;
    tap (native_panel::button_enter);
    if (! has ("LPF Cutoff=+00")) return 12;
    panel.turn (*engine, 5);
    if (! has ("LPF Cutoff=+05") || engine->part_settings (0)[0x18] != 69) return 13;
    tap (native_panel::button_enter);
    tap (native_panel::button_enter);
    if (! has ("=F0 43104C 080018") || ! has (" 45 F7")) return 14;
    tap (native_panel::button_exit);
    tap (native_panel::button_exit);
    if (! has ("VIBRATO")) return 15;

    // UTIL: system page, no card, sound module.
    tap (native_panel::button_util);
    if (! has ("SYS") || ! has ("MODE")) return 16;
    tap (native_panel::button_enter);
    if (! has ("<SYSTEM>") || ! has ("M.Tune  =+000.0")) return 17;
    panel.turn (*engine, 10);
    if (! has ("M.Tune  =+001.0") || engine->master_tune() != 1034) return 18;
    tap (native_panel::button_exit);

    // EFFECT: reverb type and its first parameter from the ROM defaults.
    tap (native_panel::button_effect);
    if (! has ("REV") || ! has ("INS")) return 19;
    tap (native_panel::button_enter);
    if (! has ("Type=HALL 1")) return 20;
    panel.turn (*engine, 1);
    if (! has ("Type=HALL 2") || engine->reverb_type() != ((::xg::rev_types()[2].msb << 7) | ::xg::rev_types()[2].lsb)) return 21;
    tap (native_panel::button_select_plus);
    if (! has ("HALL 2") || ! has ("ReverbTime=")) return 22;
    tap (native_panel::button_play);
    if (! has ("BriteP")) return 23;

    // MUTE: mute, solo, back, and the voice really goes quiet.
    engine->panel_write (0x08, 0, 0x0b, 100);   // The volume test above left this part silent.
    has ("");
    tap (native_panel::button_mute);
    engine->midi (0x90); engine->midi (60); engine->midi (127);
    float peak = 0;
    for (int i = 0; i < 40; ++i)
    {
        engine->render (left, right, 256);
        panel.tick (*engine, 256);
        for (int s = 0; s < 256; ++s) peak = std::max (peak, std::abs (left[s]));
    }
    if (peak > 1.0e-4f) return 24;
    tap (native_panel::button_mute);
    tap (native_panel::button_mute);
    // A note pressed while muted stays silent after unmuting; a new note plays.
    peak = 0;
    for (int i = 0; i < 40; ++i)
    {
        engine->render (left, right, 256);
        for (int s = 0; s < 256; ++s) peak = std::max (peak, std::abs (left[s]));
    }
    if (peak > 1.0e-4f) return 25;
    engine->midi (0x80); engine->midi (60); engine->midi (0);
    engine->midi (0x90); engine->midi (60); engine->midi (127);
    peak = 0;
    for (int i = 0; i < 40; ++i)
    {
        engine->render (left, right, 256);
        for (int s = 0; s < 256; ++s) peak = std::max (peak, std::abs (left[s]));
    }
    if (peak < 1.0e-3f) return 111;

    {
        auto machine = std::make_unique<native_engine>();
        auto program = readAll (roms + "/mu2000_flash.bin");
        const std::vector<u8>* const waves[4] = { &wave[0], &wave[1], &wave[2], &wave[3] };
        if (! machine->load (std::move (program), waves)) return 100;
        native_panel p;
        const ::xg::voice_rom names (machine->program_rom_shared());
        const auto shown = [&] {
            native_panel::lcd_frame frame;
            auto v = p.capture (*machine);
            v.blink_off = false;
            native_panel::compose (v, names, frame);
            std::string text;
            for (auto& row : frame.text)
                for (const u8 c : row) text += c >= 0x20 && c < 0x7f ? char (c) : '~';
            return text;
        };
        const auto press = [&] (int b) { p.press (*machine, b); p.release (*machine, b); };

        // マスター EQ: 帯域の量だけで絵が決まり、帯域のあいだは直線
        machine->panel_write (0x02, 0x40, 0x01, 76);
        press (native_panel::button_eq);
        {
            native_panel::lcd_frame frame;
            auto v = p.capture (*machine);
            native_panel::compose (v, names, frame);
            // 棒 0: 平ら 8 + 7 段、棒 1: + 5 段（左右で 1 桁）
            if (frame.text[0][10] != 0x7f + 7 || frame.text[0][11] != 0x7f + 5 * 9 + 2) return 101;
        }
        // MODE と EQ は、それぞれのボタンの LED が光る
        if (p.capture (*machine).leds() != (native_panel::LED_MU | native_panel::LED_EQ)) return 111;
        machine->panel_write (0x02, 0x40, 0x01, 64);
        press (native_panel::button_play);
        press (native_panel::button_mode);
        if (p.capture (*machine).leds() != (native_panel::LED_MU | native_panel::LED_MODE)) return 112;
        press (native_panel::button_exit);
        press (native_panel::button_play);

        press (native_panel::button_util);
        press (native_panel::button_select_plus);
        press (native_panel::button_enter);
        press (native_panel::button_enter);
        press (native_panel::button_enter);
        u8 message[native_panel::DUMP_MESSAGE_LIMIT];
        int messages = 0, bytes = 0;
        for (int blocks = 0; blocks < 100000; ++blocks)
        {
            p.tick (*machine, 512);
            while (const int size = p.next_dump_message (*machine, message)) { if (messages == 0 && message[3] != 0x4c) return 102; ++messages; bytes += size; }
            if (shown().find ("Transmitting") == std::string::npos) break;
        }
        if (messages != 3213 || bytes != 108734) return 103;
        press (native_panel::button_play);

        // 機種 49 と 59: 設定の受信、エラーの知らせ、値の問い合わせ
        const auto send = [&] (std::initializer_list<int> bytes) {
            for (const int b : bytes) machine->midi (u8 (b));
            for (int blocks = 0; blocks < 8; ++blocks) { machine->render (left, right, 256); p.tick (*machine, 256); }
        };
        send ({ 0xf0, 0x43, 0x10, 0x49, 0x00, 0x00, 0x00, 0x01, 0x01, 0xf7 });
        if (shown().find ("SysEx Data ERROR!") == std::string::npos) return 108;
        press (native_panel::button_value_plus);
        if (shown().find ("ERROR") != std::string::npos) return 109;
        send ({ 0xf0, 0x43, 0x30, 0x49, 0x00, 0x00, 0x00, 0xf7 });
        {
            const int size = p.next_dump_message (*machine, message);
            if (size != 9 || message[7] != 1) return 110;       // Mute Lock は先頭の値を受けて on
        }
        send ({ 0xf0, 0x43, 0x10, 0x49, 0x00, 0x00, 0x00, 0x00, 0xf7 });

        // PFM: PART の品書きを通ると 1 つのパートを選んだ面になり、EXIT で全体へ戻る
        press (native_panel::button_util);
        press (native_panel::button_select_plus);
        press (native_panel::button_select_plus);
        press (native_panel::button_enter);
        press (native_panel::button_select_plus);
        press (native_panel::button_select_plus);
        press (native_panel::button_exit);
        press (native_panel::button_exit);
        if (! machine->performance_mode() || shown().find ("ALL") == std::string::npos) return 104;
        press (native_panel::button_edit);
        press (native_panel::button_select_plus);
        press (native_panel::button_enter);
        press (native_panel::button_exit);
        press (native_panel::button_exit);
        if (shown().find ("Snow Pad") == std::string::npos) return 105;
        press (native_panel::button_part_plus);
        press (native_panel::button_part_plus);
        if (shown().find ("Fantasy") == std::string::npos) return 106;
        press (native_panel::button_exit);
        if (shown().find ("Heavens Door") == std::string::npos) return 107;
    }

    // Master EQ: a non-flat band keeps its output on the 24-bit word grid; all-flat bands leave the signal alone.
    {
        smu2000::native_fx::master_eq_params eq;
        for (int band = 0; band < 5; ++band)
        {
            eq.b0[band] = 1.0f; eq.b1[band] = 0.0f; eq.b2[band] = 0.0f; eq.a1[band] = 0.0f; eq.a2[band] = 0.0f;
        }
        smu2000::native_fx::master_eq_flow flow;
        flow.configure (eq);
        flow.reset();
        float flatLeft[4] = { 1.0e-9f, 0.1f, -0.2f, 0.3f }, flatRight[4] = { 0.25f, 0.5f, -0.75f, 1.0e-9f };
        flow.process (flatLeft, flatRight, 4);
        if (flatLeft[0] != 1.0e-9f || flatRight[3] != 1.0e-9f) return 112;
        eq.b0[0] = 0.0078125f; eq.a1[0] = 1.9921875f; eq.a2[0] = -0.9921875f;
        flow.configure (eq);
        flow.reset();
        float tail[32] {}, other[32] {};
        tail[0] = other[0] = 1.0e-5f;
        flow.process (tail, other, 32);
        for (int s = 0; s < 32; ++s)
        {
            const float word = tail[s] * 8388608.0f;
            if (word != std::trunc (word)) return 113;
        }
    }

    // The 10 ms grid: Performance 25 layers a pitch-envelope part over a plain one, so 5 ms later on the
    // grid the level falls by nearly 6 dB. A pitch-envelope target written before the key press loses that.
    {
        const auto heldLevel = [&] (int delaySamples)
        {
            auto machine = std::make_unique<native_engine>();
            auto program = readAll (roms + "/mu2000_flash.bin");
            const std::vector<u8>* const waves[4] = { &wave[0], &wave[1], &wave[2], &wave[3] };
            if (! machine->load (std::move (program), waves)) return 0.0;
            native_panel p;
            float l[32], r[32];
            double energy = 0.0;
            const auto idle = [&] (int samples, bool count)
            {
                for (int done = 0; done < samples; done += 32)
                {
                    machine->render (l, r, 32);
                    p.tick (*machine, 32);
                    if (count)
                        for (int i = 0; i < 32; ++i) energy += double (l[i]) * l[i] + double (r[i]) * r[i];
                }
            };
            const auto press = [&] (int button) { p.press (*machine, button); idle (3528, false); p.release (*machine, button); idle (3528, false); };
            idle (44100, false);
            for (const int button : { native_panel::button_util, native_panel::button_select_plus, native_panel::button_select_plus,
                                      native_panel::button_select_plus, native_panel::button_enter, native_panel::button_select_plus,
                                      native_panel::button_select_plus, native_panel::button_enter, native_panel::button_exit,
                                      native_panel::button_exit })
                press (button);
            idle (44100, false);
            for (int i = 0; i < 25; ++i) press (native_panel::button_value_plus);
            idle (13230 + delaySamples, false);
            machine->midi (0x90);
            machine->midi (0x3c);
            machine->midi (0x64);
            idle (13230, false);
            idle (22050, true);
            return 10.0 * std::log10 (energy / 22050.0 / 2.0 + 1.0e-24);
        };
        const double onGrid = heldLevel (0), late = heldLevel (220);
        std::cout << "performance 25 held level " << onGrid << " dB, 5 ms later " << late << " dB" << std::endl;
        if (onGrid - late < 4.0) return 114;
    }

    // A performance part volume of 0 is a part volume of 0, not the 1 that the scaling adds to other values.
    {
        auto machine = std::make_unique<native_engine>();
        auto program = readAll (roms + "/mu2000_flash.bin");
        const std::vector<u8>* const waves[4] = { &wave[0], &wave[1], &wave[2], &wave[3] };
        if (! machine->load (std::move (program), waves)) return 119;
        native_panel p;
        float l[32], r[32];
        const auto idle = [&] (int samples) { for (int done = 0; done < samples; done += 32) { machine->render (l, r, 32); p.tick (*machine, 32); } };
        const auto press = [&] (int button) { p.press (*machine, button); idle (3528); p.release (*machine, button); idle (3528); };
        idle (44100);
        for (const int button : { native_panel::button_util, native_panel::button_select_plus, native_panel::button_select_plus,
                                  native_panel::button_select_plus, native_panel::button_enter, native_panel::button_select_plus,
                                  native_panel::button_select_plus, native_panel::button_enter, native_panel::button_exit,
                                  native_panel::button_exit })
            press (button);
        idle (44100);
        for (const u8 byte : { 0xf0, 0x43, 0x10, 0x59, 0x09, 0x00, 0x02, 0x00, 0xf7, 0xf0, 0x43, 0x10, 0x59, 0x09, 0x01, 0x02, 0x01, 0xf7 })
            machine->midi (byte);
        idle (13230);
        if (! machine->performance_mode() || machine->part_settings (0)[0x0b] != 0 || machine->part_settings (1)[0x0b] != 1) return 119;
    }

    {
        // Program changes keep the part's settings, and reset controllers updates them.
        const auto started = [&] (const std::vector<u8>& setup)
        {
            auto machine = std::make_unique<native_engine>();
            auto program = readAll (roms + "/mu2000_flash.bin");
            const std::vector<u8>* const waves[4] = { &wave[0], &wave[1], &wave[2], &wave[3] };
            if (! machine->load (std::move (program), waves)) return std::unique_ptr<native_engine>();
            for (const u8 byte : setup) machine->midi (byte);
            for (int i = 0; i < 20; ++i) machine->render (left, right, 256);
            return machine;
        };
        const auto level = [&] (native_engine& machine)
        {
            for (const u8 byte : { u8 (0x90), u8 (60), u8 (100) }) machine.midi (byte);
            float sum = 0.0f;
            for (int i = 0; i < 40; ++i)
            {
                machine.render (left, right, 256);
                for (int s = 0; s < 256; ++s) sum += left[s] * left[s];
            }
            return sum;
        };
        const auto kept = started ({ 0xb0, 7, 40 }), selected = started ({ 0xb0, 7, 40, 0xc0, 0 });
        if (! kept || ! selected) return 115;
        const float keptLevel = level (*kept);
        // The output is the 18-bit DAC word: whole steps of 2^-17, below full scale.
        for (int i = 0; i < 256; ++i)
        {
            const float step = left[i] * 131072.0f;
            if (step != std::floor (step) || step > 131071.0f || step < -131072.0f) return 120;
        }
        if (keptLevel <= 0.0f || std::abs (level (*selected) / keptLevel - 1.0f) > 0.05f) return 115;
        const auto reset = started ({ 0xb0, 11, 40, 0xb0, 121, 0 });
        if (! reset || reset->part_settings (0).expression != 127) return 116;
        // CC10=0 is hard left, not the random pan that a part pan of 0 means.
        const auto hardLeft = started ({ 0xb0, 10, 0 });
        if (! hardLeft || hardLeft->part_settings (0)[0x0e] != 1) return 117;
        // With the variation on the SYSTEM connection, a part Dry Level of 0 removes the dry sound.
        const std::vector<u8> returnsOff = { 0xf0, 0x43, 0x10, 0x4c, 0x02, 0x01, 0x5a, 0x01, 0xf7,
                                             0xf0, 0x43, 0x10, 0x4c, 0x02, 0x01, 0x0c, 0x00, 0xf7,
                                             0xf0, 0x43, 0x10, 0x4c, 0x02, 0x01, 0x2c, 0x00, 0xf7,
                                             0xf0, 0x43, 0x10, 0x4c, 0x02, 0x01, 0x56, 0x00, 0xf7 };
        auto dryOffSetup = returnsOff;
        for (const u8 byte : { 0xf0, 0x43, 0x10, 0x4c, 0x08, 0x00, 0x11, 0x00, 0xf7 }) dryOffSetup.push_back (byte);
        const auto dryOn = started (returnsOff), dryOff = started (dryOffSetup);
        if (! dryOn || ! dryOff) return 118;
        const float dryOnLevel = level (*dryOn), dryOffLevel = level (*dryOff);
        if (dryOnLevel <= 0.0f || dryOffLevel > dryOnLevel * 1.0e-4f) return 118;
    }

    std::cout << "PASS: play, edit, util, effect and mute screens drive the engine" << std::endl;
}
