// license:BSD-3-Clause
#pragma once

#include "compat/types.h"
#include "xg/voice_rom_layout.h"

#include <algorithm>

namespace xg {
namespace gs {

// 40 1x のパートの番号（x）→ XG のパート（0 が 10 番、1-9 が 1-9 番、A-F が 11-16 番）
constexpr int part_of(int x) { return x == 0 ? 9 : x <= 9 ? x - 1 : x; }

constexpr int XG_NONE = -1;
constexpr int XG_RX_BANK = -2;

// 40 1x cc → XG の 08 pp xx
constexpr int part_address(int cc)
{
	if (cc == 0x02) return 0x04;                         // 受けるチャンネル
	if (cc >= 0x03 && cc <= 0x12) return 0x30 + cc - 0x03;   // 受信スイッチ
	if (cc >= 0x40 && cc <= 0x4b) return 0x41 + cc - 0x40;   // スケールチューニング
	switch (cc) {
	case 0x13: return 0x05;   // Mono/Poly
	case 0x14: return 0x06;   // Assign Mode
	case 0x16: return 0x08;   // Pitch Key Shift
	case 0x17: return 0x09;   // Pitch Offset Fine（2 バイト。XG の Detune と同じ形）
	case 0x18: return 0x0a;
	case 0x19: return 0x0b;   // Volume
	case 0x1a: return 0x0c;   // Velocity Sense Depth
	case 0x1b: return 0x0d;   // Velocity Sense Offset
	case 0x1c: return 0x0e;   // Pan
	case 0x1d: return 0x0f;   // Keyboard Range Low
	case 0x1e: return 0x10;   // Keyboard Range High
	case 0x1f: return 0x59;   // CC1 の番号
	case 0x20: return 0x60;   // CC2 の番号
	case 0x21: return 0x12;   // Chorus Send
	case 0x22: return 0x13;   // Reverb Send
	case 0x23: return XG_RX_BANK;
	case 0x30: return 0x15;   // Vibrato Rate
	case 0x31: return 0x16;   // Vibrato Depth
	case 0x32: return 0x18;   // Cutoff
	case 0x33: return 0x19;   // Resonance
	case 0x34: return 0x1a;   // Attack
	case 0x35: return 0x1b;   // Decay
	case 0x36: return 0x1c;   // Release
	case 0x37: return 0x17;   // Vibrato Delay
	default: return XG_NONE;
	}
}

// 40 2x cc（つまみの割り当て）→ XG の 08 pp xx。上の桁がつまみ、下の桁が行き先
constexpr int controller_address(int cc)
{
	const int controller = cc >> 4, target = cc & 0x0f;
	if (target == 3 || target > 6 || controller > 5)
		return XG_NONE;
	constexpr int FIRST[6] = { 0x1d, 0x23, 0x4d, 0x53, 0x5a, 0x61 };   // Mod、Bend、CAf、PAf、CC1、CC2
	return FIRST[controller] + (target > 3 ? target - 1 : target);
}

// 値の範囲。音程の行き先は ±24 半音、ベンドの音程は上だけ
constexpr int NOTE_RANGE_LOW = 0x28, NOTE_RANGE_HIGH = 0x58, CENTER = 0x40;

// リバーブのキャラクターごとの、XG のリバーブのパラメータ（02 01 03-05、10-14）
struct reverb_character { u8 diffusion, initial_delay, highpass_cutoff, reverb_delay, density, early_balance, high_damp, feedback_level; };
inline reverb_character reverb_character_of(const u8 *rom, int character)
{
	const u8 *r = rom + rom_position::GS_REVERB_CHARACTERS + 16 * character;
	return { r[1], r[2], r[3], r[10], r[11], std::max<u8>(r[12], 1), r[13], r[14] };	// Er/Rev の下限は 1
}
// リバーブのマクロ（0-7）: XG の種類と、キャラクター・Pre-LPF・Time の既定。キャラクターはマクロと同じ番号
struct reverb_macro { u8 msb, lsb, pre_lpf, time; };
inline reverb_macro reverb_macro_of(const u8 *rom, int macro)
{
	const u8 *type = rom + rom_position::GS_REVERB_MACRO_TYPES + 2 * macro, *r = rom + rom_position::GS_REVERB_MACROS + 6 * macro;
	return { type[0], type[1], r[1], r[3] };
}
// Reverb Time → XG の Reverb Time。キャラクターが 0 か 5 なら短い方の表
inline u8 reverb_pre_lpf(const u8 *rom, int pre_lpf) { return rom[rom_position::GS_REVERB_PRE_LPF + pre_lpf]; }
inline u8 reverb_time(const u8 *rom, int character, int time)
{
	return rom[(character == 0 || character == 5 ? rom_position::GS_REVERB_TIME_SHORT : rom_position::GS_REVERB_TIME_LONG) + time];
}

// コーラスのマクロ（0-7）が覚える Depth と Delay。Depth か Delay を送ると 2 つとも書く
struct chorus_macro { u8 depth, delay; };
constexpr u8 chorus_rate(int v) { return u8(std::min(v * 8 / 3, 127)); }
constexpr u8 chorus_depth(int v) { return u8(std::min(v * 4 / 3, 127)); }
constexpr u8 chorus_feedback(int v) { return u8(CENTER + v / 2); }
inline chorus_macro chorus_macro_of(const u8 *rom, int macro)
{
	const u8 *r = rom + rom_position::GS_CHORUS_MACROS + 7 * macro;
	return { chorus_depth(r[5]), r[3] };
}

// リセット直後の GS の値
constexpr int DEFAULT_CHARACTER = 4, DEFAULT_PRE_LPF = 0, DEFAULT_TIME = 64, DEFAULT_CHORUS_MACRO = 2;
constexpr int DRUM_KIT_DEFAULT = 37;

}
}
