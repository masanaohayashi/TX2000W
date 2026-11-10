// license:BSD-3-Clause

#ifndef S_MU2000_XG_SYNTH_STATE_H
#define S_MU2000_XG_SYNTH_STATE_H

#pragma once

#include <array>
#include <cstddef>

#include "xg/model.h"

namespace xg {
namespace settings {

constexpr int PARTS = 64;

// 全体の設定（XG の 00 00 nn）
constexpr u32 SYSTEM_SIZE = 7;
constexpr u32 SYS_VOLUME = 4, SYS_ATTENUATOR = 5, SYS_TRANSPOSE = 6;

// ドラムセットアップ（XG の 3n rr pp）
constexpr u32 DRUM_SETUP_PARAM = 23;
constexpr u32 DRUM_SETUP_NOTES = 79;
constexpr int DRUM_SETUP_NOTE0 = 13;
constexpr int DRUM_SETUP_SETS  = 4;

namespace drum_param {
constexpr int coarse = 0x00, fine = 0x01, level = 0x02, alternate_group = 0x03, pan = 0x04;
constexpr int reverb = 0x05, chorus = 0x06, variation = 0x07, receive_note_off = 0x09, receive_note_on = 0x0a;
constexpr int cutoff = 0x0b, resonance = 0x0c, attack = 0x0d, first_decay = 0x0e, second_decay = 0x0f;
constexpr int eq_low_gain = 0x20, eq_high_gain = 0x21, eq_low_freq = 0x24, eq_high_freq = 0x25;
constexpr int highpass = 0x50, pitch_velocity = 0x60, cutoff_velocity = 0x61;
}

constexpr int DRUM_INDEX_EQ_LOW_FREQ = 18, DRUM_INDEX_EQ_HIGH_FREQ = 19;
constexpr int DRUM_EQ_LOW_FREQ_DEFAULT = 0x0c, DRUM_EQ_HIGH_FREQ_DEFAULT = 0x36;

// XG の番号 pp から、鍵ごとの項目の番号（0-22）
inline int drum_setup_index(int addr)
{
	if (addr >= 0 && addr < 16)
		return addr;
	switch (addr) {
	case drum_param::eq_low_gain:     return 16;
	case drum_param::eq_high_gain:    return 17;
	case drum_param::eq_low_freq:     return DRUM_INDEX_EQ_LOW_FREQ;
	case drum_param::eq_high_freq:    return DRUM_INDEX_EQ_HIGH_FREQ;
	case drum_param::highpass:        return 20;
	case drum_param::pitch_velocity:  return 21;
	case drum_param::cutoff_velocity: return 22;
	default:   return -1;
	}
}
// その項目の既定値（リセットした直後の値。EQ の周波数のほかは 64）
inline int drum_setup_default(int index)
{
	return index == DRUM_INDEX_EQ_LOW_FREQ ? DRUM_EQ_LOW_FREQ_DEFAULT : index == DRUM_INDEX_EQ_HIGH_FREQ ? DRUM_EQ_HIGH_FREQ_DEFAULT : 64;
}

// パートの項目（XG の 08 pp nn の nn）
constexpr u32 PART_ELEMENT_RESERVE = 0x00, PART_BANK_MSB = 0x01, PART_BANK_LSB = 0x02, PART_PROGRAM = 0x03, PART_CHANNEL = 0x04, PART_POLY = 0x05,
              PART_SAME_NOTE = 0x06, PART_MODE = 0x07, PART_NOTE_SHIFT = 0x08, PART_DETUNE = 0x09, PART_DETUNE_LOW = 0x0a, PART_VOLUME = 0x0b,
              PART_VELOCITY_DEPTH = 0x0c, PART_VELOCITY_OFFSET = 0x0d, PART_PAN = 0x0e, PART_NOTE_LOW = 0x0f, PART_NOTE_HIGH = 0x10, PART_DRY = 0x11,
              PART_CHORUS = 0x12, PART_REVERB = 0x13, PART_VARIATION = 0x14, PART_VIBRATO_RATE = 0x15, PART_VIBRATO_DEPTH = 0x16, PART_VIBRATO_DELAY = 0x17,
              PART_CUTOFF = 0x18, PART_RESONANCE = 0x19, PART_ATTACK = 0x1a, PART_DECAY = 0x1b, PART_RELEASE = 0x1c, PART_MW_LFO_PITCH = 0x20,
              PART_MW_LFO_FILTER = 0x21, PART_BEND_RANGE = 0x23, PART_RECEIVE_NOTE = 0x35, PART_SCALE = 0x41, PART_AFTERTOUCH_FILTER = 0x4e, PART_AFTERTOUCH_AMP = 0x4f,
              PART_AFTERTOUCH_FILTER_LFO = 0x51, PART_AC1_CC = 0x59, PART_AC1_FILTER = 0x5b, PART_AC1_AMP = 0x5c, PART_AC1_FILTER_LFO = 0x5e,
              PART_PORTAMENTO_SWITCH = 0x67, PART_PORTAMENTO_TIME = 0x68, PART_PITCH_EG_INITIAL = 0x69, PART_PITCH_EG_ATTACK = 0x6a,
              PART_PITCH_EG_RELEASE_LEVEL = 0x6b, PART_PITCH_EG_RELEASE_TIME = 0x6c, PART_VELOCITY_LOW = 0x6d, PART_VELOCITY_HIGH = 0x6e,
              PART_EQ_BASS = 0x72, PART_EQ_TREBLE = 0x73, PART_EQ_BASS_FREQUENCY = 0x76, PART_EQ_TREBLE_FREQUENCY = 0x77;

// XG の番地の上位
constexpr u8 XG_PART = 0x08, XG_HIGHPASS = 0x0a, XG_EFFECT = 0x02, XG_INSERTION = 0x03;
// XG の番地の中位と、パートを指さない値
constexpr u8 XG_SYSTEM_EFFECT = 0x01, XG_MASTER_EQ = 0x40, XG_NO_PART = 0x7f;
constexpr u8 XG_HIGHPASS_VALUE = 0x20;
// XG の番地の下位（効果）
constexpr u8 XG_REVERB_TYPE = 0x00, XG_REVERB_VALUES = 0x02, XG_REVERB_TENTH_VALUE = 0x0b, XG_REVERB_RETURN = 0x0c, XG_REVERB_PAN = 0x0d;
constexpr u8 XG_CHORUS_TYPE = 0x20, XG_CHORUS_VALUES = 0x22, XG_CHORUS_TENTH_VALUE = 0x2b, XG_CHORUS_RETURN = 0x2c, XG_CHORUS_PAN = 0x2d, XG_CHORUS_TO_REVERB = 0x2e;
constexpr u8 XG_VARIATION = XG_SYSTEM_EFFECT, XG_VARIATION_TYPE = 0x40, XG_VARIATION_VALUES = 0x42, XG_VARIATION_TENTH_VALUE = 0x54, XG_VARIATION_RETURN = 0x56, XG_VARIATION_PAN = 0x57,
             XG_VARIATION_TO_REVERB = 0x58, XG_VARIATION_TO_CHORUS = 0x59, XG_VARIATION_CONNECTION = 0x5a, XG_VARIATION_PART = 0x5b, XG_VARIATION_LATER_VALUES = 0x70;
constexpr u8 XG_INSERTION_TYPE = 0x00, XG_INSERTION_VALUES = 0x02, XG_INSERTION_TENTH_VALUE = 0x0b, XG_INSERTION_PART = 0x0c, XG_INSERTION_LATER_VALUES = 0x20,
             XG_INSERTION_WIDE_VALUES = 0x30, XG_INSERTION_WIDE_TENTH_VALUE = 0x42;
constexpr u8 XG_EQ_TYPE_VALUES = 0x01, XG_EQ_GAIN_STRIDE = 4;
constexpr std::size_t MASTER_EQ_SIZE = 0x15;

// 値を覚える効果の番地の範囲
struct effect_range { u8 high, middle, low, size; };
constexpr effect_range EFFECT_RANGES[] = {
	{ XG_EFFECT, XG_SYSTEM_EFFECT, 0x00, 0x0e }, { XG_EFFECT, XG_SYSTEM_EFFECT, 0x10, 0x06 },   // リバーブ
	{ XG_EFFECT, XG_SYSTEM_EFFECT, 0x20, 0x0f },                                                // コーラス
	{ XG_EFFECT, XG_SYSTEM_EFFECT, 0x40, 0x02 }, { XG_EFFECT, XG_SYSTEM_EFFECT, 0x56, 0x0b }, { XG_EFFECT, XG_SYSTEM_EFFECT, 0x70, 0x06 },   // バリエーション
	{ XG_INSERTION, 0x00, 0x00, 0x12 }, { XG_INSERTION, 0x00, 0x20, 0x06 },
	{ XG_INSERTION, 0x01, 0x00, 0x12 }, { XG_INSERTION, 0x01, 0x20, 0x06 },
	{ XG_INSERTION, 0x02, 0x00, 0x12 }, { XG_INSERTION, 0x02, 0x20, 0x06 },
	{ XG_INSERTION, 0x03, 0x00, 0x12 }, { XG_INSERTION, 0x03, 0x20, 0x06 },
	{ XG_EFFECT, XG_MASTER_EQ, 0x00, u8(MASTER_EQ_SIZE) },
};

} // namespace settings

// 1 つのパートの設定
struct part_state
{
	std::array<u8, 128> values;     // XG の 08 pp nn を nn で引く
	u32 voice_record;               // 選んでいる音色の記録の位置（xg/voices.h）
	s16 fine;                       // RPN の微調。符号つきで 8192 が 100 セント
	s8 coarse;                      // RPN の粗調。符号つきの半音
	u8 highpass;                    // XG の 0A pp 20
	u8 modulation;                  // CC1
	u8 expression;                  // CC11
	u8 bend_half;                   // ピッチベンドの上位 7 ビットの半分（0x20 が真ん中）
	u8 receive_bank;                // バンク選択を受けるか
	u8 kit;                         // ドラムキットの番号
	u8 level;                       // 音量 × エクスプレッション × マスター音量
	u8 gain;                        // level にインサーションの有無を入れた、声へ掛ける大きさ

	u8 &operator[](u32 number) { return values[number]; }
	const u8 &operator[](u32 number) const { return values[number]; }

	// XG の番地（上位と下位）の値を置く場所。覚えない番地は nullptr
	const u8 *setting(u8 high, u8 low) const
	{
		if (high == settings::XG_HIGHPASS)
			return low == settings::XG_HIGHPASS_VALUE ? &highpass : nullptr;
		if (high != settings::XG_PART)
			return nullptr;
		const bool kept = low < 0x29 || (low >= 0x30 && low < 0x40) || (low >= 0x41 && low < 0x6f) || (low >= 0x72 && low < 0x78);
		return kept ? &values[low] : nullptr;
	}
	u8 *setting(u8 high, u8 low) { return const_cast<u8 *>(static_cast<const part_state &>(*this).setting(high, low)); }
};

// 音源の設定の全部（doc/voice-flow/settings.md）
struct synth_state
{
	std::array<part_state, settings::PARTS> parts;                    // 添字は XG のパート番号
	std::array<u8, settings::SYSTEM_SIZE> system;                     // XG の 00 00 nn
	u8 voice_mode;                                                    // 音色の引き方。0 が GS、1 が XG
	u8 voice_set;                                                     // 音色の組。1 が MU2000 の音色
	std::array<u8, 128> effect;                                       // XG の 02 01 nn
	std::array<u8, settings::MASTER_EQ_SIZE> master_eq;               // XG の 02 40 nn
	std::array<std::array<u8, 128>, 4> insertion;                     // XG の 03 0n nn
	std::array<std::array<std::array<u8, settings::DRUM_SETUP_PARAM>, settings::DRUM_SETUP_NOTES>, settings::DRUM_SETUP_SETS> drum;
	u8 pan_random;                                                    // パンの Rnd の乱数の種

	u8 &drum_setup(int set, int note, int index) { return drum[std::size_t(set)][std::size_t(note - settings::DRUM_SETUP_NOTE0)][std::size_t(index)]; }
	u8 drum_setup(int set, int note, int index) const { return drum[std::size_t(set)][std::size_t(note - settings::DRUM_SETUP_NOTE0)][std::size_t(index)]; }

	// XG の番地の値を置く場所。覚えない番地は nullptr
	const u8 *find(u32 address) const
	{
		const u8 high = u8(address >> 14), middle = u8((address >> 7) & 0x7f), low = u8(address & 0x7f);
		if (high == 0x00 && middle == 0x00)
			return low < settings::SYSTEM_SIZE ? &system[low] : nullptr;
		if ((high == settings::XG_PART || high == settings::XG_HIGHPASS) && middle < settings::PARTS)
			return parts[middle].setting(high, low);
		for (const settings::effect_range &range : settings::EFFECT_RANGES)
			if (high == range.high && middle == range.middle && low >= range.low && low < range.low + range.size)
				return high == settings::XG_INSERTION ? &insertion[middle][low] : middle == settings::XG_MASTER_EQ ? &master_eq[low] : &effect[low];
		return nullptr;
	}
	u8 *find(u32 address) { return const_cast<u8 *>(static_cast<const synth_state &>(*this).find(address)); }
};

} // namespace xg

#endif // S_MU2000_XG_SYNTH_STATE_H
