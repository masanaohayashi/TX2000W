// license:BSD-3-Clause
// S-MU2000: パネルの値から各エフェクトの設定を求める。回路ごとの計算は xg/fx_settings/ にある。
// 流れは doc/fx-flow/parameters.md の図のとおり。

#ifndef S_MU2000_XG_FX_SETTINGS_H
#define S_MU2000_XG_FX_SETTINGS_H

#include "native/fx_params.h"
#include "xg/effect_tables.h"

#include <tuple>

namespace smu2000::fx_settings {

using namespace native_fx;
namespace tables = xg::effect_tables;
namespace rom_at = xg::effect_rom_layout;

// エフェクト 1 つへの指示
struct effect_request {
	const u8 *rom = nullptr;
	bool variation = true;		// false ならインサーション
	int insertion_part = 0;		// 1-3: インサーション 2-4
	int circuit = 0, make = 0;	// 回路の組とその中の種類
	u32 kind = tables::effect_none;
	int gain_circuit = 0, gain_make = 0;	// 出口の利得の表を引く組と種類
	u16 P[16] = {};		// パネルの値
	u16 M[16] = {};		// 接続とセンド
	// 関係するつまみを次に動かすまで、前の値で求めたままのもの
	u16 held_cross_input = 0;	// T.CRS DLY: 声 0 の位置を求めたときの入力の選択
	u16 held_drive_connection = 0;	// 2 スピードのロータリー: 出口の大きさを求めたときの接続
	u16 held_wet_phase = 0;		// V-FLANGER: Dry/Wet を求めたときの ModPhase
	bool pan_was_smooth = false;	// AUTO PAN2: なめらかな向きを 1 度でも通ったか
	u16 held_detune = 64;		// ENS DETUNE: 中央以外で最後に動かした Detune（中央では窓の向きが変わらない）
	void hold_current_values()
	{
		held_cross_input = P[3];
		held_drive_connection = M[9];
		held_wet_phase = P[13];
		pan_was_smooth = P[3] < 5;
		held_detune = P[0];
	}
	void set_param(int n, u16 value)	// n = 1..16
	{
		const int k = n - 1;
		P[k] = value;
		switch (kind) {
		case tables::effect_tempo_cross_delay:
			if (k <= 1 || k == 5)
				held_cross_input = P[3];
			break;
		case tables::effect_drive_two_way_rotary:
		case tables::effect_amp_two_way_rotary:
			if (k == 13 || k == 15)
				held_drive_connection = M[9];
			break;
		case tables::effect_v_flanger:
			if (k == 4 || k == 9)
				held_wet_phase = P[13];
			break;
		case tables::effect_ensemble_detune:
			if (k == 0 && value != 64)
				held_detune = value;
			break;
		case tables::effect_auto_pan2:
			if ((k == 3 || k == 4) && P[3] < 5)
				pan_was_smooth = true;
			break;
		default:
			break;
		}
	}
	void set_connection(int value)
	{
		M[9] = u16(value);
		if (kind == tables::effect_v_flanger)
			held_wet_phase = P[13];
	}
	bool connected_to_system() const { return variation && (M[9] & 1); }
	const tables::effect_records &record_table() const { return insertion_part ? tables::extra_insertion_records[circuit] : variation ? tables::variation_records[circuit] : tables::insertion_records[circuit]; }
	int record_start() const { const tables::effect_records &t = record_table(); return t.first + int(rom[t.numbers + make]) * tables::effect_record_size; }
	// 種類ごとに ROM に入っている初期の値
	int stored_gain(int position) const { return tables::detail::read_s16(rom, record_start() + 2 * position); }
	int stored_read_position(int position) const { return int(tables::detail::read_u16(rom, record_start() + tables::effect_record_read_positions + 2 * position)); }
	u16 stored_lfo(int k) const { return u16(tables::detail::read_u16(rom, record_start() + tables::effect_record_lfos + 2 * k)); }
	// 遅延メモリのうち、このエフェクトの持ち分の先頭
	int delay_memory_start() const { return insertion_part ? (insertion_part == 1 ? 0 : insertion_part == 2 ? 0x2500 : 0x5000) : (variation ? 0x4000 : 0); }
};

inline effect_request make_request(const u8 *rom, int which, int msb, int lsb, const std::pair<int, int> *mix = nullptr, int mixes = 0,
	int insertion_part = 0, int insertion1_msb = -1, int insertion1_lsb = -1)
{
	effect_request r;
	r.rom = rom;
	r.variation = which == tables::FX_VAR;
	r.insertion_part = insertion_part;
	std::copy(tables::FX_MIX_RESET, tables::FX_MIX_RESET + 16, r.M);
	for (int k = 0; k < mixes; k++)
		r.M[mix[k].first] = u16(mix[k].second);
	tables::fx_kind(rom, which, msb, lsb, r.circuit, r.make);
	r.kind = (r.circuit < 32 && r.make < tables::FX_DISP_N) ? tables::FX_HANDLER[r.circuit][r.make] : u32(tables::effect_none);
	r.gain_circuit = r.circuit;
	r.gain_make = r.make;
	if (insertion_part && tables::slave_gain_ins1(r.kind)) {
		if (insertion1_msb < 0) { insertion1_msb = 0x49; insertion1_lsb = 0; }
		tables::fx_kind(rom, tables::FX_INS, insertion1_msb, insertion1_lsb, r.gain_circuit, r.gain_make);
	}
	const int defaults = r.record_table().defaults;
	for (int k = 0; k < 16; k++)
		r.P[k] = u16(tables::detail::read_u16(rom, defaults + 32 * r.make + 2 * k));
	r.hold_current_values();
	return r;
}

// 設定のうち、回路の値以外
struct effect_extras {
	u16 lfo[6] = {};		// LFO の設定（engine.h の lfo_bank が読む形）
	tables::RamTable ram;		// 遅延メモリに置く曲線
};

inline void start_extras(const effect_request &in, effect_extras &x)
{
	for (int k = 0; k < 6; k++)
		x.lfo[k] = in.stored_lfo(k);
	x.ram.first = u32(in.delay_memory_start());
}

// 固定小数の値を、フローの持つ倍率の値へ
inline float scaled(int v, double scale = 1.0) { return float(double(float(s16(u16(v))) * (1.0f / 32768.0f)) * scale); }

// --- LFO ---------------------------------------------------------------
enum lfo_waveform : u16 { lfo_sine = 0, lfo_triangle = 0x0400, lfo_rising_saw = 0x0800, lfo_falling_saw = 0x0C00 };
constexpr u16 lfo_waveform_bits = 0x0C00;
constexpr u16 lfo_speed_range_bits = 0x0300, lfo_eight_times_faster = 0x0300;

inline void set_lfo_speed_value(u16 &lfo, int speed) { lfo = u16((lfo & 0xff00) | speed); }
inline void set_lfo_speed(u16 &lfo, const u8 *rom, int value) { set_lfo_speed_value(lfo, tables::detail::read_u8(rom, tables::LFO_FREQ_TAB + (value & 0xff))); }
inline void set_lfo_speeds(u16 *lfo, int count, const u8 *rom, int value)
{
	for (int k = 0; k < count; k++)
		set_lfo_speed(lfo[k], rom, value);
}
inline void set_lfo_start_phase(u16 &lfo, const u8 *rom, int value) { lfo = u16((lfo & 0x0fff) | ((tables::detail::read_u8(rom, rom_at::symphonic_detune_param14_table + (value & 0xff)) << 12) & 0xf000)); }
inline void set_lfo_waveform(u16 &lfo, lfo_waveform wave) { lfo = u16((lfo & ~lfo_waveform_bits) | wave); }
inline void set_lfo_speed_range(u16 &lfo, u16 range) { lfo = u16((lfo & ~lfo_speed_range_bits) | range); }

// +2 ローターの Mic Angle: 右の声 2 本の LFO の始まりをずらす
inline void set_two_way_mic_angle(const effect_request &in, effect_extras &x, int value)
{
	constexpr int low_right_lfo = 2, high_right_lfo = 1;
	const int angle = s8(u8(value));
	set_lfo_start_phase(x.lfo[low_right_lfo], in.rom, angle + 64);
	set_lfo_start_phase(x.lfo[high_right_lfo], in.rom, angle + 4);
}

// テンポと音符の長さから LFO の速さの値を求める（曲のテンポは 120 に固定）
constexpr int fixed_tempo = 120;
// 周波数の比の差 1 に当たる揺れ幅。のこぎりの速さ 64 の 1 周の長さ（44100 / 0.673）
constexpr double ratio_one_saw_depth = 44100.0 / 0.673;
inline int tempo_lfo_speed(const u8 *rom, int note_length)
{
	using namespace tables::detail;
	// 速さは 32 ごとに周波数が倍になる。区間の上の端と、区間の中の傾き（コンパイル時に決まる）
	static constexpr int RANGES = 7;
	static constexpr std::array<double, RANGES> RANGE_TOP = {0.67291259999999997, 0.67291259999999997 * 2, 0.67291259999999997 * 4,
		0.67291259999999997 * 8, 0.67291259999999997 * 16, 21.533203100000001, 42.393493700000001};
	static constexpr std::array<float, RANGES> RANGE_SCALE = {95.1089325f, 95.1089325f / 2, 95.1089325f / 4, 95.1089325f / 8,
		95.1089325f / 16, 95.1089325f / 32, 95.1089325f / 64};
	double hz = f32(f32(f32(double(fixed_tempo)) * 8.0) / f32(read_float(rom, rom_at::tempo_lfo_table + 4 * note_length) * 2.0));
	hz = f32(hz / 10.0);
	for (int k = 0; k < RANGES; k++)
		if ((k || hz >= 0.0) && hz < RANGE_TOP[k]) {
			volatile double x = f32(hz * double(RANGE_SCALE[k]));
			double y = x;
			if (k)
				y = f32(y + 32.0 * k);
			return int(tr(y)) & 0xff;
		}
	return 0xff;
}

// --- よく使う値 ----------------------------------------------------------
// 左右を足して 1 本にするときの倍率 √½ を掛けた大きさ（切り捨て）。コンパイル時に決まる
constexpr int sum_to_mono(int level) { return int(level * 0.70710678118654752); }
// 小数を 1 倍が 0x8000 の目盛りへ（下へ切り捨て）。コンパイル時に決まる
constexpr int q15_floor(double value)
{
	const double scaled_value = value * 0x8000;
	const int truncated = int(scaled_value);
	return truncated > scaled_value ? truncated - 1 : truncated;
}
constexpr int panel_value_step = 0x7FFF / 127;	// パネルの値 1 つぶん。127 でほぼ 1 倍（32766）
inline float modulation_depth(int value) { return scaled(((value & 0xff) << 9) / 127, 32768.0); }	// 値に比例、最大 1/64
// 音量の揺れの深さ（揺れは音量を下げる向きなので負）
inline float am_depth(int value)
{
	const int v = s8(u8(value));
	return scaled(v == -127 ? 0x7fff : -((v << 15) / 127));
}
inline int centered_feedback(int value, int step = 500) { return (value - 64) * step; }
inline int s16_at(const effect_request &in, int table, int k) { return tables::detail::read_s16(in.rom, table + 2 * k); }
inline int u16_at(const effect_request &in, int table, int k) { return int(tables::detail::read_u16(in.rom, table + 2 * k)); }
inline int level_down(const effect_request &in, int steps) { return s16_at(in, rom_at::sixth_octave_gain_table, steps % 6) >> (steps / 6); }	// 6 段で半分

inline int tenths_to_samples(int tenths_of_ms) { return tables::detail::ms_to_smp(u32(tenths_of_ms)); }	// 0.1ms → サンプル数
// 値ごとの遅れの表（0.1ms）からサンプル数へ
inline int table_delay_samples(const u8 *rom, int table, int value) { return tenths_to_samples(tables::detail::read_u16(rom, table + 2 * value)); }
inline int chorus_delay_samples(const u8 *rom, int value) { return table_delay_samples(rom, tables::CHO_DLY_TAB, value); }
inline int reverb_delay_samples(const u8 *rom, int value) { return table_delay_samples(rom, rom_at::reverb_delay_samples_table, value); }
inline int long_delay_samples(const u8 *rom, int value) { return table_delay_samples(rom, rom_at::karaoke_param1_table, value); }	// 最大 400 ms

// --- 遅延線 --------------------------------------------------------------
// メモリを 0x8000 語ずつの線 2 本に分けるときの書き込み位置。音に効くのは読む位置との差だけ
constexpr int first_line_write = 1, half_memory = first_line_write + (0x10000 >> 1);

// 声が読む位置から、近い方の線と、その線の書き込み位置からの遅れを求める
template<class Params>
inline void set_voice(Params &p, int voice, int read, int first_line_write, int second_line_write)
{
	const int from_first = (read - first_line_write) & 0xffff, from_second = (read - second_line_write) & 0xffff;
	p.voice_base_delay[voice] = std::min(from_first, from_second);
	p.voice_on_second_line[voice] = from_first > from_second;
}

// 1 本目の線へ入れる左右。ステレオなら左だけ、モノラルなら半分ずつ
inline void set_first_line_input(float &from_left, float &from_right, bool stereo)
{
	from_left = scaled(stereo ? 0x7fff : 0x4000);
	from_right = scaled(stereo ? 0 : 0x4000);
}
// 線 2 本のとき: ステレオなら右は 2 本目へ
template<class Params>
inline void set_two_line_inputs(Params &p, bool stereo)
{
	set_first_line_input(p.first_line_from_left, p.first_line_from_right, stereo);
	p.second_line_from_right = scaled(stereo ? 0x7fff : 0);
}

// 遅延メモリに置く曲線
inline void write_curve_word(effect_extras &x, int position, int value) { x.ram[u32(position)] = u32(u16(value)) << 16; }
inline void copy_curve(const u8 *rom, effect_extras &x, int position, int curve, int words)
{
	for (int k = 0; k < words; k++)
		write_curve_word(x, position + k, tables::detail::read_u16(rom, curve + 2 * k));
}
// 搬送波のサイン波を遅延メモリの持ち分の先頭に置く
template<class Params>
inline void set_sine_carrier(const effect_request &in, Params &p, effect_extras &x)
{
	constexpr int sine_curve = rom_at::dyna_ring_table, sine_words = 256 + 1, sine_center = 128;
	const int memory = in.delay_memory_start();
	copy_curve(in.rom, x, memory, sine_curve, sine_words);
	p.carrier_read_center = memory + sine_center;
}

// --- 出口 ----------------------------------------------------------------
// 回路の出口の利得（種類ごとの大きさを掛ける）
inline int output_gain(const effect_request &in, int v)
{
	const int table = in.variation ? tables::variation_output_gain_table : tables::insertion_output_gain_table;
	return (s16(u16(v)) * int(tables::detail::read_u8(in.rom, table + in.gain_circuit * tables::FX_DISP_N + in.gain_make))) >> 6;
}
inline float nominal_output_gain(const effect_request &in) { return scaled(output_gain(in, 0x4000), 4.0); }

// 値が増えると下がる方と上がる方の大きさ
// dry_wet_curve は 2 乗の和が 1 の曲線 sin((v - 1) / 126 × 90°)
constexpr int full_at_center_curve = rom_at::full_at_center_curve;	// 中央で両方とも最大で、端へ ((v - 1) / 63)² で下がる。リバーブとディレイの Dry/Wet
struct crossfade_gains { int falling, rising; };
inline crossfade_gains crossfade(const u8 *rom, int value, int curve = rom_at::dry_wet_curve)
{
	return { tables::detail::read_s16(rom, curve + 2 * (128 - value)), tables::detail::read_s16(rom, curve + 2 * value) };
}

// Dry/Wet: SYSTEM 接続のバリエーションは全部 Wet
struct dry_wet_gains { int dry, wet; };
inline dry_wet_gains dry_wet(const effect_request &in, int value, int curve = rom_at::dry_wet_curve)
{
	const auto [dry, wet] = crossfade(in.rom, in.connected_to_system() ? 127 : value, curve);
	return { dry >> 1, output_gain(in, wet >> 1) };
}
template<class Params>
inline void set_dry_wet(const effect_request &in, Params &p, int value, int curve = rom_at::dry_wet_curve)
{
	const auto g = dry_wet(in, value, curve);
	for (int ch = 0; ch < 2; ch++) {
		p.dry_gain[ch] = scaled(g.dry, 4.0);
		p.wet_gain[ch] = scaled(g.wet, 4.0);
	}
}

// --- フィルタ ------------------------------------------------------------
enum filter_shape {
	lowpass_shape = 0, highpass_shape = 1, low_shelf_shape = 2, high_shelf_shape = 3,
	steep_lowpass_shape = 4, steep_highpass_shape = 5, steep_low_shelf_shape = 6, steep_high_shelf_shape = 7,
	peaking_shape = 10, through_shape = 11
};
constexpr int range_1x = 0, range_2x = 1, range_4x = 2, range_16x = 4;	// 係数の目盛り。値は 2^bits 倍で読む
constexpr int flat_gain = 64;		// 利得の 0 dB

using first_order = std::tuple<float, float, float>;	// 今の入力、1 つ前の入力、帰還
using second_order = std::tuple<float, float, float, float, float>;	// 今の入力、1 つ前と 2 つ前の入力、1 つ前と 2 つ前の出力

inline first_order scaled_coefficients(const std::array<u16, 3> &v, int bits)
{
	const double scale = 1 << bits;
	return { scaled(v[0], scale), scaled(v[1], scale), scaled(v[2], scale) };
}
inline second_order scaled_coefficients(const std::array<u16, 5> &v, int bits)
{
	const double scale = 1 << bits;
	return { scaled(v[0], scale), scaled(v[1], scale), scaled(v[2], scale), scaled(v[3], scale), scaled(v[4], scale) };
}
inline first_order first_order_filter(const u8 *rom, int shape, int bits, int frequency, int gain = flat_gain)
{
	return scaled_coefficients(tables::detail::filt1(rom, shape, bits, frequency, gain), bits);
}
inline second_order second_order_filter(const u8 *rom, int shape, int bits, int frequency, int gain = flat_gain, int width = 0)
{
	return scaled_coefficients(tables::detail::filt2(rom, shape, bits, frequency, gain & 0xff, width & 0xff), bits);
}
// 表の 1 列（形、周波数、利得。2 次は幅が続く）から
inline first_order first_order_row(const u8 *rom, int bits, int row)
{
	using tables::detail::read_u8;
	return first_order_filter(rom, read_u8(rom, row), bits, read_u8(rom, row + 1), read_u8(rom, row + 2));
}
inline second_order second_order_row(const u8 *rom, int bits, int row)
{
	using tables::detail::read_u8;
	return second_order_filter(rom, read_u8(rom, row), bits, read_u8(rom, row + 1), read_u8(rom, row + 2), read_u8(rom, row + 3));
}

// 係数をフローの名前へ。左右の配列なら両方に同じ値を入れる
inline void store(float &to, float value) { to = value; }
inline void store(float (&to)[2], float value) { to[0] = to[1] = value; }
template<class Name>
inline void put(const first_order &f, Name &current, Name &previous, Name &feedback)
{
	store(current, std::get<0>(f));
	store(previous, std::get<1>(f));
	store(feedback, std::get<2>(f));
}
template<class Name>
inline void put(const second_order &f, Name &current, Name &x1, Name &x2, Name &y1, Name &y2)
{
	store(current, std::get<0>(f));
	store(x1, std::get<1>(f));
	store(x2, std::get<2>(f));
	store(y1, std::get<3>(f));
	store(y2, std::get<4>(f));
}
// 1 次の形を 2 次の丸めで求めたもの。1 つ前の入力と 1 つ前の出力だけを使う
template<class Name>
inline void put(const second_order &f, Name &current, Name &previous, Name &feedback)
{
	store(current, std::get<0>(f));
	store(previous, std::get<1>(f));
	store(feedback, std::get<3>(f));
}

// 低域と高域の棚（2 次の丸めで求める 1 次、高域は 16 倍の目盛り）
template<class Params>
inline void set_shelves(const u8 *rom, Params &p, int low_frequency, int low_gain, int high_frequency, int high_gain)
{
	put(second_order_filter(rom, low_shelf_shape, range_2x, low_frequency, low_gain), p.low_shelf_current, p.low_shelf_previous, p.low_shelf_feedback);
	put(second_order_filter(rom, high_shelf_shape, range_16x, high_frequency, high_gain), p.high_shelf_current, p.high_shelf_previous, p.high_shelf_feedback);
}
// 低域と高域の棚（1 次）
template<class Params>
inline void set_first_order_shelves(const u8 *rom, Params &p, int low_frequency, int low_gain, int high_frequency, int high_gain, int high_bits)
{
	put(first_order_filter(rom, low_shelf_shape, range_2x, low_frequency, low_gain), p.low_shelf_current, p.low_shelf_previous, p.low_shelf_feedback);
	put(first_order_filter(rom, high_shelf_shape, high_bits, high_frequency, high_gain), p.high_shelf_current, p.high_shelf_previous, p.high_shelf_feedback);
}
// ピーキング
template<class Params>
inline void set_mid_band(const u8 *rom, Params &p, int frequency, int gain, int width, int bits = range_2x)
{
	put(second_order_filter(rom, peaking_shape, bits, frequency, gain, width), p.mid_current, p.mid_x1, p.mid_x2, p.mid_y1, p.mid_y2);
}

// 種類ごとの初期の値をフローの名前へ
template<class Params>
inline void load_stored(const effect_request &in, Params &p)
{
	float coef[96];
	int32_t positions[32];
	for (int k = 0; k < 96; k++)
		coef[k] = float(in.stored_gain(k)) * (1.0f / 32768.0f);
	for (int k = 0; k < 32; k++)
		positions[k] = in.stored_read_position(k);
	p.load(coef, positions);
}

// 回路の設定の始まり: LFO の初期の値と、種類ごとの初期の値
template<class Params>
inline void start_settle(const effect_request &in, Params &p, effect_extras &x)
{
	start_extras(in, x);
	load_stored(in, p);
}

// まだこの形に移っていない回路
template<class Params>
inline bool settle(const effect_request &, Params &, effect_extras &) { return false; }

} // namespace smu2000::fx_settings

#include "xg/fx_settings/modulated_delay.h"
#include "xg/fx_settings/rotary.h"
#include "xg/fx_settings/dual_rotary.h"
#include "xg/fx_settings/distortion_rotary.h"
#include "xg/fx_settings/phaser.h"
#include "xg/fx_settings/early_reflection.h"
#include "xg/fx_settings/variation_reverb.h"
#include "xg/fx_settings/chorus3.h"
#include "xg/fx_settings/auto_wah.h"
#include "xg/fx_settings/distortion.h"
#include "xg/fx_settings/stereo_distortion.h"
#include "xg/fx_settings/pitch_shift.h"
#include "xg/fx_settings/talk_mod.h"
#include "xg/fx_settings/lo_fi.h"
#include "xg/fx_settings/amp_sim2.h"
#include "xg/fx_settings/wah_distortion_delay.h"
#include "xg/fx_settings/distortion_delay.h"
#include "xg/fx_settings/v_distortion.h"
#include "xg/fx_settings/v_flanger.h"
#include "xg/fx_settings/multi_comp.h"
#include "xg/fx_settings/dyna_flanger.h"
#include "xg/fx_settings/dyna_phaser.h"
#include "xg/fx_settings/dyna_filter.h"
#include "xg/fx_settings/dyna_ring.h"
#include "xg/fx_settings/auto_pan2.h"
#include "xg/fx_settings/low_reso.h"
#include "xg/fx_settings/isolator.h"
#include "xg/fx_settings/slice.h"
#include "xg/fx_settings/ring_mod.h"
#include "xg/fx_settings/turntable.h"
#include "xg/fx_settings/system.h"

#endif // S_MU2000_XG_FX_SETTINGS_H
