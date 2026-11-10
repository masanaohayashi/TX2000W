// license:BSD-3-Clause

#ifndef S_MU2000_XG_EFFECT_TABLES_H
#define S_MU2000_XG_EFFECT_TABLES_H

#pragma once

#include "compat/types.h"
#include "xg/effect_rom_layout.h"

#include <algorithm>
#include <array>
#include <bit>

namespace smu2000::xg::effect_tables {

using namespace effect_rom_layout;

constexpr u16 KEEP = 0xFFFF;	// Input のリバーブ・コーラスのパラメータ: 送らない（種類の既定値のまま）

// XG のパラメータ。既定値は電源投入時（HALL 1、CHORUS 1、EQ 平ら、戻り 64・パン 64）
struct Input {
	u8 rev_msb = 0x01, rev_lsb = 0x00;	// 02 01 00/01
	u16 rev[16] = {KEEP, KEEP, KEEP, KEEP, KEEP, KEEP, KEEP, KEEP,
	               KEEP, KEEP, KEEP, KEEP, KEEP, KEEP, KEEP, KEEP};	// パラメータ 1..16（KEEP 以外を番号の順に送る）
	u8 cho_msb = 0x41, cho_lsb = 0x00;	// 02 01 20/21
	u16 cho[16] = {KEEP, KEEP, KEEP, KEEP, KEEP, KEEP, KEEP, KEEP,
	               KEEP, KEEP, KEEP, KEEP, KEEP, KEEP, KEEP, KEEP};
	u8 eq[20] = {64, 12, 7, 0, 64, 28, 7, 0, 64, 34, 7, 0, 64, 46, 7, 0, 64, 52, 7, 0};	// 02 40 01..14
	// 0 リバーブのパン（02 01 0D）、1 リバーブの戻り（0C）、2 コーラスのパン（2D）、3 コーラスの戻り（2C）、
	// 4 コーラス → リバーブ（2E）
	u8 mix[5] = {64, 64, 64, 64, 0};
};

namespace detail {

inline u32 read_u8(const u8 *r, int a) { return r[a]; }
inline u32 read_u16(const u8 *r, int a) { return u32(r[a]) << 8 | r[a + 1]; }
inline int read_s16(const u8 *r, int a) { return s16(u16(read_u16(r, a))); }
inline u32 read_u32(const u8 *r, int a) { return read_u16(r, a) << 16 | read_u16(r, a + 2); }
inline u64 read_u64(const u8 *r, int a) { return u64(read_u32(r, a)) << 32 | read_u32(r, a + 4); }
inline double read_float(const u8 *r, int a) { return std::bit_cast<float>(read_u32(r, a)); }
inline double f32(double x) { return float(x); }
inline int sx(long long v) { return s16(u16(v)); }			// 16 ビットの符号つきへ
inline long long tr(double x) { return (long long)x; }		// float → int（0 の側へ）
inline long long fdiv(long long a, long long b) { long long q = a / b; return (a % b && (a < 0) != (b < 0)) ? q - 1 : q; }	// Python の //
inline int mulhi(u32 a) { return sx(((a & 0xffff) * 0x70E5u) >> 16); }	// ×0.441（表の単位 → 標本）
inline int ms_to_smp(u32 v) { return int(((v & 0xffff) * 0x468Fu) >> 12); }	// 0.1ms → 標本

constexpr int FREQ_TAB = filter_frequency_table, GAIN_TAB = filter_gain_table, SQRT_GAIN_TAB = filter_sqrt_gain_table;

struct Coefs { double v[5]; int n; };

inline Coefs filt_coefs(const u8 *rom, int kind, int idx, int gain, int q = 0)
{
	const double f = read_float(rom, FREQ_TAB + 4 * idx);
	auto T2 = [&](int i) { return read_float(rom, GAIN_TAB + 4 * i); };
	auto SQ = [&](int i) { return read_float(rom, SQRT_GAIN_TAB + 4 * i); };
	const double f2 = f32(f * f);
	switch (kind) {
	case 0: {	// LPF
		const double b0 = f32(f32(T2(gain - 52) * f) / (1.0 + f));
		return {{b0, b0, f32((1.0 - f) / (1.0 + f))}, 3};
	}
	case 1: {	// HPF
		const double b0 = f32(T2(gain - 52) / (1.0 + f));
		return {{b0, -b0, f32((1.0 - f) / (1.0 + f))}, 3};
	}
	case 2: {	// 低域シェルフ（1 次）
		double p = f, r = f;
		if (gain >= 64) p = f32(f * T2(gain - 52));
		else r = f32(f * T2(76 - gain));
		return {{f32((1.0 + p) / (1.0 + r)), f32((p - 1.0) / (1.0 + r)), f32((1.0 - r) / (1.0 + r))}, 3};
	}
	case 3: {	// 高域シェルフ（1 次、float だけ）
		double G = 1.0, A = 1.0;
		if (gain >= 64) G = T2(gain - 52);
		else A = T2(76 - gain);
		const double d = f32(A + f);
		return {{f32(f32(f + G) / d), f32(f32(f - G) / d), f32(f32(A - f) / d)}, 3};
	}
	case 7: {	// 高域シェルフ（2 次）
		const double ff = f + f;
		if (gain >= 64) {
			const double G = T2(gain - 52), S = SQ(gain - 52);
			const double D = f32(1.0 + (f2 + ff));
			const double t = f32(S + f), y = f32(S - f);
			return {{f32(f32(t * t) / D), f32(f32(f32(f32(f2 + f2) - G) - G) / D), f32(f32(y * y) / D),
			         f32(((2.0 - f2) - f2) / D), f32((-1.0 + (ff - f2)) / D)}, 5};
		}
		const double G = T2(76 - gain), S = SQ(76 - gain);
		const double t = f32(S + f);
		const double Dn = f32(t * t);
		return {{f32((1.0 + (f2 + ff)) / Dn), f32((-2.0 + f32(f2 + f2)) / Dn), f32((1.0 + ((f2 - f) - f)) / Dn),
		         f32(f32(f32(f32(G - f2) - f2) + G) / Dn), f32(f32(f32(S - f) * f32(f - S)) / Dn)}, 5};
	}
	case 4: case 5: {	// 2 次の LPF（利得つき）/ HPF
		const double w = f32((10.0 * f) / double(q));
		const double D = f32(1.0 + (f2 + w));
		double b0, b1;
		if (kind == 4) { b0 = f32(f32(T2(gain - 52) * f2) / D); b1 = f32(b0 + b0); }
		else { b0 = f32(1.0 / D); b1 = f32(-b0 - b0); }
		return {{b0, b1, b0, f32(((2.0 - f2) - f2) / D), f32((-1.0 + (w - f2)) / D)}, 5};
	}
	case 8: case 9: {	// 帯域通過 / 帯域阻止（利得は使わない）
		const double w = f32((10.0 * f) / double(q));
		const double D = f32(1.0 + (f2 + w));
		const double a2 = f32((-1.0 + (w - f2)) / D);
		if (kind == 8) { const double b0 = f32(w / D); return {{b0, 0.0, -b0, f32(((2.0 - f2) - f2) / D), a2}, 5}; }
		const double b0 = f32((1.0 + f2) / D), b1 = f32((-2.0 + f32(f2 + f2)) / D);
		return {{b0, b1, b0, -b1, a2}, 5};
	}
	case 6: {
		double g = f, g2 = f2;
		double s = f32(f * SQ(gain >= 64 ? gain - 52 : 76 - gain)), s2 = f32(s * s);
		if (gain < 64) { std::swap(g, s); std::swap(g2, s2); }
		const double D = f32(1.0 + (g2 + (g + g)));
		return {{f32((1.0 + (s2 + (s + s))) / D), f32((-2.0 + f32(s2 + s2)) / D), f32((1.0 + ((s2 - s) - s)) / D),
		         f32(((2.0 - g2) - g2) / D), f32((-1.0 + ((g + g) - g2)) / D)}, 5};
	}
	case 11:	// 素通し
		return {{T2(gain - 52), 0, 0, 0, 0}, 5};
	default: {	// 10 ピーキング
		const double w = f32((10.0 * f) / double(q));
		double D, b0, b2, a2;
		if (gain >= 64) {
			const double wg = f32(w * T2(gain - 52));
			D = f32(1.0 + (f2 + w));
			b0 = f32((1.0 + (f2 + wg)) / D);
			b2 = f32((1.0 + (f2 - wg)) / D);
			a2 = f32((-1.0 + (w - f2)) / D);
		} else {
			const double wg = f32(w * T2(76 - gain));
			D = f32(1.0 + (f2 + wg));
			b0 = f32((1.0 + (f2 + w)) / D);
			b2 = f32((1.0 + (f2 - w)) / D);
			a2 = f32((-1.0 + (wg - f2)) / D);
		}
		const double b1 = f32((-2.0 + f32(f2 + f2)) / D);
		return {{b0, b1, b2, -b1, a2}, 5};
	}
	}
}

// s*x を 0 から遠い側への四捨五入で
inline double qz(double s, double x) { return f32(f32(s * x) + (0.0 > x ? -0.5 : 0.5)); }
inline double qerr(double s, double q, double x) { return f32(double(sx(tr(q))) - f32(s * x)); }

inline std::array<u16, 3> filt1(const u8 *rom, int kind, int shift, int idx, int gain)
{
	const Coefs c = filt_coefs(rom, kind, idx, gain);
	const double s = f32(double(16 >> shift) * 2048.0);
	double q0 = qz(s, c.v[0]), q1 = qz(s, c.v[1]);
	const double q2 = qz(s, c.v[2]);
	const double e0 = qerr(s, q0, c.v[0]), e1 = qerr(s, q1, c.v[1]);
	const double e = f32(e0 + e1);
	if (-0.5 > e) {
		if (e1 >= e0) q0 = f32(q0 + 1.0);
		else q1 = f32(q1 + 1.0);
	} else if (0.5 < e) {
		if (e1 < e0) q0 = f32(q0 - 1.0);
		else q1 = f32(q1 - 1.0);
	}
	return {u16(tr(q0)), u16(tr(q1)), u16(tr(q2))};
}

inline std::array<u16, 5> filt2(const u8 *rom, int kind, int shift, int idx, int gain, int qv)
{
	Coefs c = filt_coefs(rom, kind, idx, gain, qv);
	if (c.n == 3) c = {{c.v[0], c.v[1], 0.0, c.v[2], 0.0}, 5};
	const double s = f32(double(16 >> shift) * 2048.0);
	double Q[5], e[5];
	for (int i = 0; i < 5; i++) Q[i] = qz(s, c.v[i]);
	for (int i = 0; i < 5; i++) e[i] = qerr(s, Q[i], c.v[i]);
	const double n = f32(f32(e[1] + e[0]) + e[2]);
	if (-0.5 > n) {
		const int i = e[2] >= e[1] ? (e[1] >= e[0] ? 0 : 1) : (e[2] >= e[0] ? 0 : 2);
		Q[i] = f32(Q[i] + 1.0);
	} else if (0.5 < n) {
		const int i = e[2] < e[1] ? (e[1] < e[0] ? 0 : 1) : (e[2] < e[0] ? 0 : 2);
		Q[i] = f32(Q[i] - 1.0);
	}
	const double d = f32(e[4] + e[3]);
	if (-0.5 > d) {
		const int i = e[4] >= e[3] ? 3 : 4;
		Q[i] = f32(Q[i] + 1.0);
	} else if (0.5 < d) {
		const int i = e[4] < e[3] ? 3 : 4;
		Q[i] = f32(Q[i] - 1.0);
	}
	std::array<u16, 5> o;
	for (int i = 0; i < 5; i++) o[i] = u16(tr(Q[i]));
	return o;
}

}	// namespace detail

constexpr int REV_KIND_TAB = reverb_kind_by_type, REV_KIND_OK = reverb_kind_valid, REV_KIND_TAB2 = reverb_kind_high_types;
constexpr int REV_REC_NO = reverb_record_numbers, REV_REC = reverb_records;
constexpr int REV_DEFAULTS = reverb_default_params, REV_SIZE = reverb_size_classes, TIME_TAB = reverb_time_table;
constexpr int DIM_SCALE = reverb_room_scale;
// コーラス
constexpr int LFO_FREQ_TAB = lfo_speed_table, CHO_DLY_TAB = chorus_delay_table;
// まとめ
constexpr int PAN_TAB = pan_attenuation_table, RET_TAB = return_attenuation_table;

inline int reverb_kind(const u8 *rom, int msb, int lsb)
{
	using namespace detail;
	if (lsb >= 32) return 0;
	if (msb < 5) {
		if (lsb >= 20 || read_u8(rom, REV_KIND_OK + msb * 20 + lsb) == 0) lsb = 0;
		return read_u8(rom, REV_KIND_TAB + msb * 20 + lsb);
	}
	if (16 <= msb && msb < 20) return read_u8(rom, REV_KIND_TAB2 + msb - 16);
	return 0;
}

inline void chorus_kind(const u8 *rom, int msb, int lsb, int &g, int &idx)
{
	g = 0;
	if (lsb < 32) {
		if (msb == 0x48) g = 1;
		else if (msb == 0x57) g = 2;
		else if (msb == 0x41 && lsb == 2) g = 3;
	}
	idx = 0;
	if (lsb < 32 && 64 <= msb) {
		if (msb < 69) {
			const int row_offset = (msb - 64) * 18;
			if (lsb >= 18 || detail::read_u8(rom, chorus_kind_valid + row_offset + lsb) == 0) lsb = 0;
			idx = detail::read_u8(rom, chorus_kind_makes + row_offset + lsb);
		} else if (msb == 72 || msb == 87) {
			idx = 1;
		}
	}
}




constexpr int FX_VAR = 0, FX_INS = 1;
constexpr int FX_DISP_N = 35;
enum effect_kind : u32 { effect_none, effect_compressor_drive_delay, effect_drive_delay, effect_distortion, effect_amp_sim, effect_compressor, effect_compressor_drive, effect_harmonic_enhancer, effect_noise_gate, effect_v_drive_delay_tempo, effect_v_drive_delay, effect_drive_rotary, effect_amp_rotary, effect_drive_two_way_rotary, effect_amp_two_way_rotary, effect_early_reflection, effect_karaoke, effect_lo_fi, effect_chorus_celeste, effect_flanger, effect_symphonic, effect_delay_lcr, effect_delay_lr, effect_echo, effect_cross_delay, effect_tempo_delay, effect_tempo_cross_delay, effect_rotary_speaker, effect_two_way_rotary, effect_tremolo, effect_auto_pan, effect_ambience, effect_chorus3, effect_pitch_change, effect_ensemble_detune, effect_pass_through, effect_two_band_eq, effect_three_band_eq, effect_phaser, effect_tempo_phaser, effect_hall_room_plate, effect_large_space_reverb, effect_dual_rotary_1, effect_dual_rotary_2, effect_stereo_drive, effect_stereo_amp, effect_talk_mod, effect_auto_wah, effect_touch_wah, effect_wah_drive_delay, effect_amp_sim2, effect_v_flanger, effect_multi_comp, effect_dyna_flanger, effect_dyna_phaser, effect_dyna_filter, effect_dyna_ring, effect_auto_pan2, effect_dj_scratch, effect_low_reso, effect_isolator, effect_slice, effect_ring_mod, effect_turntable };
constexpr u32 FX_HANDLER[32][FX_DISP_N] = {
	// 組 effect_none
	{effect_none, effect_none, effect_chorus_celeste, effect_chorus_celeste, effect_none, effect_chorus_celeste, effect_chorus_celeste, effect_chorus_celeste, effect_chorus_celeste, effect_chorus_celeste, effect_flanger, effect_flanger, effect_flanger, effect_symphonic,
	 effect_delay_lcr, effect_delay_lr, effect_echo, effect_cross_delay, effect_tempo_delay, effect_tempo_delay, effect_tempo_cross_delay, effect_chorus_celeste, effect_chorus_celeste, effect_chorus_celeste, effect_chorus_celeste, effect_chorus_celeste,
	 effect_flanger, effect_delay_lcr, effect_chorus_celeste, effect_chorus_celeste, effect_chorus_celeste, effect_flanger, effect_flanger, effect_symphonic, effect_flanger},
	{effect_none, effect_rotary_speaker, effect_two_way_rotary, effect_tremolo, effect_auto_pan, effect_ambience, effect_rotary_speaker, effect_tremolo, effect_auto_pan, effect_auto_pan, effect_tremolo, effect_auto_pan, effect_auto_pan,
	 effect_tremolo, effect_tremolo, effect_auto_pan, effect_auto_pan},
	{effect_none, effect_phaser, effect_tempo_phaser, effect_phaser, effect_phaser, effect_phaser, effect_tempo_phaser},
	{effect_none, effect_early_reflection, effect_early_reflection, effect_early_reflection, effect_early_reflection, effect_karaoke, effect_karaoke, effect_karaoke},
	{effect_none, effect_auto_wah, effect_touch_wah, effect_auto_wah, effect_auto_wah, effect_touch_wah, effect_touch_wah, effect_touch_wah, effect_auto_wah, effect_auto_wah, effect_auto_wah, effect_touch_wah, effect_touch_wah,
	 effect_touch_wah, effect_touch_wah},
	{effect_none, effect_distortion, effect_distortion, effect_amp_sim, effect_compressor, effect_compressor_drive, effect_harmonic_enhancer, effect_noise_gate, effect_compressor_drive, effect_amp_sim, effect_amp_sim, effect_harmonic_enhancer},
	{effect_none, effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate, effect_large_space_reverb, effect_large_space_reverb, effect_large_space_reverb, effect_large_space_reverb,
	 effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate,
	 effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate, effect_hall_room_plate},
	{effect_none, effect_pitch_change, effect_pitch_change, effect_ensemble_detune, effect_pass_through, effect_two_band_eq, effect_three_band_eq, effect_three_band_eq, effect_three_band_eq, effect_pitch_change},
	{},
	{effect_none, effect_dual_rotary_1, effect_dual_rotary_2},
	{effect_none, effect_talk_mod},
	{effect_none, effect_lo_fi},
	{},
	{effect_none, effect_wah_drive_delay, effect_wah_drive_delay, effect_none, effect_none, effect_wah_drive_delay, effect_wah_drive_delay},
	{effect_none, effect_compressor_drive_delay, effect_compressor_drive_delay, effect_drive_delay, effect_drive_delay, effect_none, effect_none, effect_none, effect_none, effect_drive_delay, effect_drive_delay, effect_compressor_drive_delay, effect_compressor_drive_delay},
	{effect_none, effect_v_drive_delay_tempo, effect_v_drive_delay_tempo, effect_v_drive_delay, effect_v_drive_delay},
	{effect_none, effect_stereo_drive, effect_stereo_drive, effect_stereo_amp, effect_stereo_amp, effect_stereo_amp, effect_stereo_amp, effect_stereo_amp},
	{effect_none, effect_drive_rotary, effect_drive_rotary, effect_amp_rotary, effect_drive_two_way_rotary, effect_drive_two_way_rotary, effect_amp_two_way_rotary},
	{effect_none, effect_chorus3},
	{effect_none, effect_amp_sim2}, {effect_none, effect_v_flanger}, {effect_none, effect_multi_comp}, {effect_none, effect_dyna_flanger}, {effect_none, effect_dyna_phaser}, {effect_none, effect_dyna_filter}, {effect_none, effect_dyna_ring},
	{effect_none, effect_auto_pan2}, {effect_none, effect_dj_scratch, effect_low_reso}, {effect_none, effect_isolator}, {effect_none, effect_slice}, {effect_none, effect_ring_mod}, {effect_none, effect_turntable}};
constexpr u16 FX_MIX_RESET[16] = {64, 64, 64, 64, 0, 0, 127, 0, 0, 0, 0, 0, 64, 64, 0, 0};

inline void fx_kind(const u8 *rom, int which, int msb, int lsb, int &g, int &i, int flag = 0)
{
	using namespace detail;
	if (lsb >= 32) g = 0;
	else if (msb == 0x41 && lsb == 2) g = 18;
	else if (msb == 0x45 && lsb >= 1 && lsb <= 3) g = 17;
	else if (msb == 0x47 && lsb == 1) g = 26;
	else if ((msb == 0x49 || msb == 0x4A) && lsb == 8) g = 16;
	else if (msb == 0x4B && (lsb == 8 || (lsb >= 18 && lsb <= 21))) g = 16;
	else if (msb == 0x4B && lsb == 1) g = 19;
	else if (msb == 0x56 && lsb >= 1 && lsb <= 3) g = 17;
	else g = int(read_u8(rom, (which == FX_VAR ? variation_group_by_msb : insertion_group_by_msb) + (msb & 0x7f)));
	if (lsb >= 32) { i = (msb >= 64 || which == FX_INS || !flag) ? 1 : 0; return; }
	if (msb >= 100 && msb <= 103) { i = 1; return; }
	const auto look_up = [&](const auto &ranges) {
		for (const effect_type_range &range : ranges)
			if (msb >= range.first_msb && msb < range.end_msb) {
				const int row = (msb - range.first_msb) * range.lsb_count;
				const int used_lsb = (lsb < range.lsb_count && read_u8(rom, range.lsb_valid + row + lsb)) ? lsb : 0;
				i = int(read_u8(rom, range.record_numbers + row + used_lsb));
				return true;
			}
		return false;
	};
	if (which == FX_VAR ? look_up(variation_type_ranges) : look_up(insertion_type_ranges))
		return;
	i = ((msb >= 12 && msb < 16) || (msb >= 23 && msb < 64)) && which == FX_VAR && flag ? 0 : 1;
}

struct RamTable {
	static constexpr u32 SPAN = 0x2500;
	u32 first = 0;
	u32 &operator[](u32 addr)
	{
		const u32 i = addr - first;
		if (i >= SPAN) {
			out_of_span = true;
			return discard;
		}
		used[i] = 1;
		return word[i];
	}
	template <class F> void for_each(F visit) const
	{
		for (u32 i = 0; i < SPAN; i++)
			if (used[i])
				visit(first + i, word[i]);
	}
	bool out_of_span = false;
private:
	u32 word[SPAN] = {}, discard = 0;
	u8 used[SPAN] = {};
};


#ifdef __clang__
#pragma clang fp contract(off)		// 積和を FMA にまとめない（double の DETUNE など）
#endif

inline bool slave_gain_ins1(u32 h)
{
	switch (h) {
	case effect_amp_sim2: case effect_v_flanger: case effect_multi_comp: case effect_dyna_flanger: case effect_dyna_phaser: case effect_dyna_filter: case effect_dyna_ring: case effect_auto_pan2:
	case effect_dj_scratch: case effect_low_reso: case effect_isolator: case effect_slice: case effect_ring_mod: case effect_turntable: return true;
	default: return false;
	}
}


inline void fx_mixer(const u8 *rom, const u16 M[16], u16 out[4][3])
{
	using namespace detail;
	auto cl = [](int v) { return v > 255 ? 255 : v; };
	int l = int(read_u8(rom, PAN_TAB + 0x80 - M[5])), r = int(read_u8(rom, PAN_TAB + M[5]));
	if (M[9] == 0) { l = cl(l + 16); r = cl(r + 16); }
	const int rv = M[9] ? int(read_u8(rom, RET_TAB + M[6])) : cl(int(read_u8(rom, RET_TAB + M[6])) + 8);
	const u16 v1 = u16(rv << 8 | read_u8(rom, RET_TAB + M[7])), v2 = u16(read_u8(rom, RET_TAB + M[8]) << 8 | 0xFF);
	out[0][0] = u16(l << 8 | 0xFF); out[1][0] = u16(0xFF00 | r);
	out[0][1] = out[1][1] = v1; out[0][2] = out[1][2] = v2;
	if (M[10]) { l = cl(int(read_u8(rom, PAN_TAB + 0x80 - M[10])) + 16); r = cl(int(read_u8(rom, PAN_TAB + M[10])) + 16); }
	else l = r = 16;
	out[2][0] = u16(l << 8 | 0xFF); out[3][0] = u16(0xFF00 | r);
	out[2][1] = out[2][2] = out[3][1] = out[3][2] = 0;
}

inline int fx_part_gain(int gain, bool inserted) { return inserted ? 128 : gain; }
inline void fx_insert_mixer(const u8 *rom, int gain, int pan, int rev, int cho, int var, int dry, bool var_system, u16 out[4])
{
	using namespace detail;
	auto T = [&](int v) { return int(read_u8(rom, send_attenuation_table + v)); };
	auto cl = [](int v) { return v > 255 ? 255 : v; };
	const int g = gain < 0 ? 0 : gain > 128 ? 128 : gain;
	int A, hi;
	if (g == 0) A = hi = 0xFF;
	else { A = int(read_u8(rom, part_gain_attenuation_table + g)); hi = cl(A + 8 + (var_system ? T(dry) : 0)); }
	if (!pan) pan = 64;
	out[0] = u16(cl(int(read_u8(rom, PAN_TAB + 0x80 - pan)) + 16) << 8 | 0xFF);
	out[1] = u16(0xFF00 | cl(int(read_u8(rom, PAN_TAB + pan)) + 16));
	out[2] = u16(hi << 8 | cl(T(rev) + A));
	out[3] = u16(cl(T(cho) + A) << 8 | (var_system ? cl(T(var) + A) : 0xFF));
}


}	// namespace smu2000::xg::effect_tables

#endif
