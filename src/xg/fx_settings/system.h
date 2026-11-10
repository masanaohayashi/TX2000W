// license:BSD-3-Clause
// S-MU2000: システムのリバーブ、コーラス、マスター EQ、戻りの設定。doc/fx-flow/parameters.md。

#ifndef S_MU2000_XG_FX_SETTINGS_SYSTEM_H
#define S_MU2000_XG_FX_SETTINGS_SYSTEM_H

#include "xg/fx_settings/reverb_parts.h"

namespace smu2000::fx_settings {

// システムのエフェクトへの指示
struct system_request {
	const u8 *rom = nullptr;
	int reverb_make = 0;			// リバーブの種類（0 なら無し）
	int chorus_circuit = 0, chorus_make = 0;
	u16 reverb[16] = {}, chorus[16] = {};	// KEEP を種類の既定値に置き換えたパネルの値
	u8 eq[20] = {}, returns[5] = {};
};

inline system_request make_system_request(const u8 *rom, const tables::Input &in)
{
	using tables::detail::read_u16;
	system_request r;
	r.rom = rom;
	r.reverb_make = tables::reverb_kind(rom, in.rev_msb, in.rev_lsb);
	tables::chorus_kind(rom, in.cho_msb, in.cho_lsb, r.chorus_circuit, r.chorus_make);
	const int reverb_defaults = tables::REV_DEFAULTS + r.reverb_make * 32;
	const int chorus_defaults = tables::system_chorus_records[r.chorus_circuit].defaults + r.chorus_make * 32;
	for (int k = 0; k < 16; k++) {
		r.reverb[k] = in.rev[k] != tables::KEEP ? in.rev[k] : u16(read_u16(rom, reverb_defaults + 2 * k));
		r.chorus[k] = in.cho[k] != tables::KEEP ? in.cho[k] : u16(read_u16(rom, chorus_defaults + 2 * k));
	}
	std::copy(in.eq, in.eq + 20, r.eq);
	std::copy(in.mix, in.mix + 5, r.returns);
	return r;
}

// --- マスター EQ ---------------------------------------------------------
inline void settle_master_eq(const system_request &in, master_eq_params &p)
{
	constexpr int band_count = 5, top_band = band_count - 1;
	constexpr int output_level = -sum_to_mono(0x2000) - 1;	// 負の側へ切り捨て			// 位相を反転して 2.83 倍（目盛りは 16 倍）
	for (int band = 0; band < band_count; band++) {
		const u8 *knobs = in.eq + 4 * band;
		const int gain = knobs[0], frequency = knobs[1], q = knobs[2], peak_instead_of_shelf = knobs[3];
		int shape = peaking_shape;
		if (band == 0 && !peak_instead_of_shelf)
			shape = low_shelf_shape;
		if (band == top_band && !peak_instead_of_shelf)
			shape = steep_high_shelf_shape;
		// 上の帯だけ 16 倍の目盛り、ほかは 4 倍
		put(second_order_filter(in.rom, shape, band == top_band ? range_16x : range_4x, frequency, gain, q), p.b0[band], p.b1[band], p.b2[band], p.a1[band], p.a2[band]);
	}
	p.output_gain_left = p.output_gain_right = scaled(output_level, 16.0);
}

// --- リバーブ ------------------------------------------------------------
namespace system_reverb {

using namespace tables::detail;
using namespace reverb_parts;

constexpr int comb_count = 6, early_tap_count = 4;

// 種類ごとの持ち物。組番号で引く
constexpr int make_record_number = tables::REV_REC_NO;
constexpr int stored_record = tables::REV_REC, stored_record_size = 0x122;
constexpr int stored_diffuse_filter_feedback = 12, stored_diffuse_filter_previous = 13, stored_diffuse_filter_current = 14;
constexpr int line_layouts[3] = {rom_at::reverb_standard_line_layout, rom_at::reverb_second_line_layout, rom_at::reverb_dim_line_layout};	// 線の置き方（組番号 1-3）
constexpr int layout_fourth_comb_start = 10, layout_fifth_comb_start = 11;	// 線の置き方の中の、4 本目と 5 本目の櫛形の線の始まり

constexpr int return_levels = rom_at::system_return_levels;	// 種類ごとの戻りの大きさ
constexpr int last_room_make = 12;		// 部屋の種類は WHITE ROOM、TUNNEL、BASEMENT、CANYON

// 初期反射の 4 タップと、後部へ渡す遅れ、初期反射と櫛形の割合。どの遅れも遅延メモリの端には届かない
inline void set_early_part(const system_request &in, reverb_params &p, int initial_delay, int reverb_delay, int diffusion, int balance)
{
	const u8 *rom = in.rom;
	const int table = early_tap_table + size_class(rom, in.reverb_make, early_size) * early_table_stride;
	const int predelay = reverb_delay_samples(rom, initial_delay), spread = diffusion_offset(rom, diffusion);
	const crossfade_gains shares = crossfade(rom, balance, full_at_center_curve);
	auto early_gain = [&](int position) { return sx(tr(f32(f32(double(shares.falling) * double(read_s16(rom, position))) / 32768.0))) >> 1; };
	for (int i = 0; i < early_tap_count; i++) {
		const int close_side = i & 1;		// 偶数は左、奇数は右が先に鳴る
		const int delay = (predelay + mulhi(read_u16(rom, table + early_tap_time + 2 * i))) & 0xffff;
		p.early_delay[close_side][i] = delay;
		p.early_delay[1 - close_side][i] = delay + spread;
		p.early_gain[0][i] = scaled(early_gain(table + early_left_gain + 2 * i));
		p.early_gain[1][i] = scaled(early_gain(table + early_right_gain + 2 * i));
	}
	p.late_send_delay[0] = p.late_send_delay[1] = predelay + reverb_delay_samples(rom, reverb_delay);
	const float comb_share = scaled(shares.rising);
	for (auto &side : p.comb_out_gain)
		for (float &gain : side)
			gain = comb_share;
}

// 櫛形 1 本の長さと取り出し。片側の取り出しは左右の差だけ短くする（1 より短くしない）
inline void set_comb_taps(reverb_params &p, int comb, int length, int tap, int spread)
{
	const int close_side = comb & 1;		// 偶数は左、奇数は右が長い
	p.comb_delay[comb] = length & 0xffff;
	p.comb_out_delay[close_side][comb] = tap & 0xffff;
	p.comb_out_delay[1 - close_side][comb] = (tap > spread ? tap - spread : 1) & 0xffff;
}

// HALL、ROOM、STAGE、PLATE の櫛形: 長さは級で決まる
inline void set_sized_combs(const system_request &in, reverb_params &p, int reverb_time, int diffusion, int high_damp)
{
	const u8 *rom = in.rom;
	const int size = size_class(rom, in.reverb_make, comb_size), spread = diffusion_offset(rom, diffusion);
	const auto damping = damping_filter(rom, range_2x, high_damp);
	const int decay = int(read_u8(rom, decay_steps + reverb_time));
	for (int comb = 0; comb < comb_count; comb++) {
		const int entry = comb_table + size * comb_table_stride + 4 * comb;
		set_comb_taps(p, comb, mulhi(read_u16(rom, entry + 2)), mulhi(read_u16(rom, entry)), spread);
		const int offset = int(read_u8(rom, comb_decay_offsets + size * comb_decay_offsets_stride + comb));
		set_comb_damping(p, comb, damping, offset ? comb_level_at(rom, decay + offset) : comb_level{0, 0});
	}
}

// WHITE ROOM、TUNNEL、BASEMENT、CANYON の櫛形: 長さと減衰は部屋の寸法で決まり、
// 寸法ごとの 2 本目は Wall Vary のぶんだけ違う
inline void set_room_combs(const system_request &in, reverb_params &p, int reverb_time, int diffusion, int high_damp, const int *room_size, int wall_vary)
{
	const u8 *rom = in.rom;
	const int spread = diffusion_offset(rom, diffusion);
	const auto damping = damping_filter(rom, range_2x, high_damp);
	for (int dimension = 0; dimension < comb_count / 2; dimension++) {
		const int decay = int(read_u8(rom, decay_steps + nearest_time(rom, reverb_time, room_time_scale(rom, in.reverb_make, dimension))));
		for (int wall = 0; wall < 2; wall++) {
			const int size = room_size[dimension] + (wall ? wall_vary : 0), comb = 2 * dimension + wall;
			const u32 length = read_u16(rom, room_comb_lengths + 2 * size);
			set_comb_taps(p, comb, mulhi(length), mulhi(length >> 1), spread);
			set_comb_damping(p, comb, damping, comb_level_at(rom, int(read_u8(rom, room_decay_offsets + size)) + decay));
		}
	}
}

// 棚の無い種類は持ち物の値
inline void set_diffuse_filter(const system_request &in, reverb_params &p, int record)
{
	if (set_diffuse_shelf(in.rom, p, in.reverb_make))
		return;
	const u8 *rom = in.rom;
	const int start = stored_record + record * stored_record_size;
	p.diffuse_filter_current = scaled(read_s16(rom, start + 2 * stored_diffuse_filter_current), 4.0);
	p.diffuse_filter_previous = scaled(read_s16(rom, start + 2 * stored_diffuse_filter_previous), 4.0);
	p.diffuse_filter_feedback = scaled(read_s16(rom, start + 2 * stored_diffuse_filter_feedback), 4.0);
}

} // namespace system_reverb

inline void settle_reverb(const system_request &in, reverb_params &p)
{
	using namespace system_reverb;
	p = reverb_params{};
	if (!in.reverb_make)
		return;
	const u16 *P = in.reverb;
	const int reverb_time = P[0], diffusion = P[1], initial_delay = P[2], hpf_cutoff = P[3], lpf_cutoff = P[4];
	const int room_size[3] = {P[5], P[6], P[7]};
	const int wall_vary = P[8], reverb_delay = P[10], density = P[11], early_late_balance = P[12], high_damp = P[13], feedback_level = P[14];
	const int record = int(read_u8(in.rom, make_record_number + in.reverb_make));

	p.send_left_to_left = p.send_right_to_right = 1.0f;
	p.diffuse_left = p.diffuse_right = 0.5f;
	set_reverb_input_filters(in.rom, p, hpf_cutoff, lpf_cutoff);
	p.predelay_feedback[0] = p.predelay_feedback[1] = scaled(centered_feedback(feedback_level));
	set_early_part(in, p, initial_delay, reverb_delay, diffusion, early_late_balance);
	set_diffuse_filter(in, p, record);
	set_reverb_allpasses(in.rom, p, size_class(in.rom, in.reverb_make, allpass_size), density, diffusion);
	if (in.reverb_make >= first_room_make && in.reverb_make <= last_room_make)
		set_room_combs(in, p, reverb_time, diffusion, high_damp, room_size, wall_vary);
	else
		set_sized_combs(in, p, reverb_time, diffusion, high_damp);
	// PLATE は 5 本目の櫛形が無い。その取り出しは 4 本目の線を、2 本の線の始まりの距離だけ前で読む
	const int layout = line_layouts[record - 1];
	p.unused_comb_distance = (int(read_u16(in.rom, layout + 2 * layout_fifth_comb_start)) - int(read_u16(in.rom, layout + 2 * layout_fourth_comb_start))) & 0xffff;
	p.return_gain[0] = p.return_gain[1] = scaled((0x4000 * int(read_u8(in.rom, return_levels + in.reverb_make))) >> 6, 4.0);
}

// --- コーラス ------------------------------------------------------------
namespace system_chorus {

using namespace tables::detail;

// 種類ごとの持ち物の形
constexpr int record_size = 112, stored_coefficients = 40, stored_positions = 12;
constexpr int stored_positions_start = 80, stored_lfo_start = 104;
constexpr int lfo_count = 4;

constexpr int output_levels = rom_at::out_gain_table, output_levels_circuit_stride = 25;

constexpr int stereo_second_line_write = 0x4000;

// 持ち物の値を読み込む。LFO もここから始まる
template<class Params>
inline void load_stored(const system_request &in, Params &p, u16 *lfo)
{
	const int number = int(read_u8(in.rom, tables::system_chorus_records[in.chorus_circuit].numbers + in.chorus_make));
	const int start = tables::system_chorus_records[in.chorus_circuit].first + (in.chorus_circuit == 0 ? number * record_size : (number * record_size) & 0xff);
	float coef[96] = {};
	int32_t positions[32] = {};
	for (int k = 0; k < stored_coefficients; k++)
		coef[k] = float(read_s16(in.rom, start + 2 * k)) * (1.0f / 32768.0f);
	for (int k = 0; k < stored_positions; k++)
		positions[k + 1] = int(read_u16(in.rom, start + stored_positions_start + 2 * k));
	for (int k = 0; k < lfo_count; k++)
		lfo[k] = u16(read_u16(in.rom, start + stored_lfo_start + 2 * k));
	p.load(coef, positions);
}

inline int output_level(const system_request &in)
{
	return (0x4000 * int(read_u8(in.rom, output_levels + in.chorus_circuit * output_levels_circuit_stride + in.chorus_make))) >> 6;
}

// 低域と高域の棚（左右同じ）
template<class Params>
inline void set_stereo_shelves(const system_request &in, Params &p)
{
	const u16 *P = in.chorus;
	set_first_order_shelves(in.rom, p, P[5], P[6], P[7], P[8], range_4x);
}

// CHORUS、GM CHORUS、FB CHORUS、CELESTE
inline void chorus_celeste(const system_request &in, chorus_params &p, u16 *lfo, bool celeste)
{
	constexpr int feedback_step = 376;
	const u16 *P = in.chorus;
	const int lfo_frequency = P[0], lfo_depth = P[1], feedback_level = P[2], delay_offset = P[3];
	const bool stereo = P[14] == 1;
	set_lfo_speeds(lfo, lfo_count, in.rom, lfo_frequency);
	for (float &d : p.voice_depth)
		d = modulation_depth(lfo_depth * 2);
	p.left_line_feedback = p.right_line_feedback = scaled(centered_feedback(feedback_level, feedback_step));
	set_first_line_input(p.left_line_from_left, p.left_line_from_right, stereo);
	p.stereo_input = stereo;
	const int delay = chorus_delay_samples(in.rom, delay_offset);
	p.voice_base_delay[0] = p.voice_base_delay[3] = delay + 1;
	p.voice_base_delay[1] = p.voice_base_delay[2] = delay + 65;
	const float gain = scaled(output_level(in), 4.0);
	p.wet_gain[0][0] = p.wet_gain[1][0] = gain;
	if (celeste)
		p.wet_gain[0][1] = p.wet_gain[1][1] = gain;
}

// FLANGER: 声 0 と声 3 が左右の線を読む。出口へ混ぜない声 1・2 は線の先を指したまま
inline void flanger(const system_request &in, chorus_params &p, u16 *lfo)
{
	constexpr int unused_voice_1_delay = 32767, unused_voice_2_delay = 16384;
	const u16 *P = in.chorus;
	const int lfo_frequency = P[0], lfo_depth = P[1], feedback_level = P[2], delay_offset = P[3], phase_difference = P[13];
	set_lfo_speed(lfo[0], in.rom, lfo_frequency);
	set_lfo_speed(lfo[3], in.rom, lfo_frequency);
	set_lfo_start_phase(lfo[3], in.rom, phase_difference);
	p.voice_depth[0] = p.voice_depth[3] = modulation_depth(lfo_depth);
	p.left_line_feedback = p.right_line_feedback = scaled(centered_feedback(feedback_level));
	p.stereo_input = 1;
	p.voice_base_delay[0] = p.voice_base_delay[3] = chorus_delay_samples(in.rom, delay_offset) + 1;
	p.voice_base_delay[1] = unused_voice_1_delay;
	p.voice_base_delay[2] = unused_voice_2_delay;
	p.wet_gain[0][0] = p.wet_gain[1][0] = scaled(output_level(in), 4.0);
}

// SYMPHONIC: 4 つの声を少しずつずらす。声 2 だけ速い LFO で揺れ幅が狭く、
// Delay Offset を動かしても種類の既定の遅れのまま
inline void symphonic(const system_request &in, chorus_params &p, u16 *lfo)
{
	constexpr int delay_offset_knob = 2;
	const u16 *P = in.chorus;
	const int lfo_frequency = P[0], lfo_depth = P[1], delay_offset = P[delay_offset_knob];
	const int default_delay_offset = int(read_u16(in.rom, tables::system_chorus_records[in.chorus_circuit].defaults + in.chorus_make * 32 + 2 * delay_offset_knob));
	set_lfo_speeds(lfo, lfo_count, in.rom, lfo_frequency);
	p.voice_depth[0] = p.voice_depth[1] = p.voice_depth[3] = modulation_depth((lfo_depth * 5) >> 3);
	p.voice_depth[2] = modulation_depth((lfo_depth * 3) >> 3);
	const int delay = chorus_delay_samples(in.rom, delay_offset);
	p.voice_base_delay[0] = delay + 1;
	p.voice_base_delay[1] = delay + 65;
	p.voice_base_delay[2] = chorus_delay_samples(in.rom, default_delay_offset) + 65;
	p.voice_base_delay[3] = delay + 129;
	const float gain = scaled(output_level(in), 4.0);
	for (auto &side : p.wet_gain)
		for (float &g : side)
			g = gain;
}

} // namespace system_chorus

inline void settle_chorus(const system_request &in, chorus_params &p, u16 lfo[4])
{
	using namespace system_chorus;
	load_stored(in, p, lfo);
	switch (in.chorus_make) {
	case 1: case 2: case 3: case 4: case 14: case 15: case 16: case 17: case 18:
		chorus_celeste(in, p, lfo, false);
		break;
	case 5: case 6: case 7: case 8: case 20: case 21:
		chorus_celeste(in, p, lfo, true);
		break;
	case 9: case 10: case 11: case 19: case 22: case 23:
		flanger(in, p, lfo);
		break;
	case 12: case 24:
		symphonic(in, p, lfo);
		break;
	default:
		return;		// 回路の無い種類は持ち物のまま
	}
	set_stereo_shelves(in, p);
}

inline void settle_chorus(const system_request &in, system_phaser_params &p, u16 lfo[4])
{
	using namespace system_chorus;
	load_stored(in, p, lfo);
	if (in.chorus_make != 1)
		return;
	const u16 *P = in.chorus;
	const int lfo_frequency = P[0], lfo_depth = P[1], phase_shift_offset = P[2], feedback_level = P[3];
	const int stage = P[10], diffusion = P[11];
	set_lfo_speed(lfo[1], in.rom, lfo_frequency);
	const int center = (phase_shift_offset * panel_value_step) & 0xffff;
	p.sweep_center = scaled(center);
	p.sweep_depth = scaled(int(fdiv((0x7fff - s16(u16(center))) * (long long)lfo_depth, 127)));
	p.chain_feedback = scaled(centered_feedback(feedback_level));
	set_stereo_shelves(in, p);
	// Stage は音が通る段の数で 4、5、6。2 つ目の取り出しがその段、1 つ目は 1 段手前
	const int chosen_row = stage <= 4 ? 0 : stage == 5 ? 1 : 2;
	for (int k = 0; k < 3; k++)
		p.first_tap[k] = p.second_tap[k] = scaled(k == chosen_row ? 0x4000 : 0, 2.0);
	// Diffusion が mono なら左右とも同じ音。stereo は左を 1 段手前から取り、右を逆相にして広げる
	const int level = output_level(in);
	p.left_from_first = scaled(diffusion ? level : 0, 4.0);
	p.left_from_second = scaled(diffusion ? 0 : level, 4.0);
	p.right_from_second = scaled(diffusion ? -level : level, 4.0);
}

inline void settle_chorus(const system_request &in, system_detune_params &p, u16 lfo[4])
{
	using namespace system_chorus;
	load_stored(in, p, lfo);
	if (in.chorus_make != 1)
		return;
	constexpr int pitch_ratios = rom_at::pitch_shift_detune_ratio_table;	// 値 → 周波数の比
	constexpr int no_detune = 64, ratio_up_start = 114, ratio_down_start = 14;
	const u16 *P = in.chorus;
	const int detune = P[0], initial_delay_left = P[1], initial_delay_right = P[2];
	const double depth_scale = ratio_one_saw_depth;
	auto ratio = [&](int i) { return read_float(in.rom, pitch_ratios + 4 * i); };
	// 積を丸めてから引く（FMA にさせない）
	auto scale = [&](double r) { volatile double x = depth_scale * r; return double(x); };
	long long left_depth = 0, right_depth = 0;
	if (detune < no_detune) {
		left_depth = tr(scale(ratio(ratio_up_start - detune)) - depth_scale);
		right_depth = tr(depth_scale - scale(ratio(detune - ratio_down_start)));
	} else if (detune > no_detune) {
		left_depth = tr(depth_scale - scale(ratio(ratio_up_start - detune)));
		right_depth = tr(scale(ratio(detune - ratio_down_start)) - depth_scale);
		// 持ち物は下げる向き。上げるときは左右ののこぎり波の向きを入れ替える
		set_lfo_waveform(lfo[0], lfo_rising_saw);
		set_lfo_waveform(lfo[1], lfo_falling_saw);
	}
	p.head_a_depth[0] = p.head_b_depth[0] = scaled(int(left_depth), 32768.0);
	p.head_a_depth[1] = p.head_b_depth[1] = scaled(int(right_depth), 32768.0);
	p.head_a_base_delay[0] = p.head_b_base_delay[0] = chorus_delay_samples(in.rom, initial_delay_left) + 1;
	p.head_a_base_delay[1] = p.head_b_base_delay[1] = chorus_delay_samples(in.rom, initial_delay_right) + 1;
	p.out_gain[0] = p.out_gain[1] = scaled(output_level(in), 4.0);
}

inline void settle_chorus(const system_request &in, system_chorus3_params &p, u16 lfo[4])
{
	using namespace system_chorus;
	load_stored(in, p, lfo);
	if (in.chorus_make != 1)
		return;
	constexpr int wide_voice_depths = rom_at::chorus3_wide_depth_curve, narrow_voice_depths = rom_at::chorus3_narrow_depth_curve;	// LFO Depth の値 → 揺れ幅
	constexpr int feedback_step = 250;
	const u16 *P = in.chorus;
	const int lfo_frequency = P[0], lfo_depth = P[1], feedback_level = P[2], delay_offset = P[3];
	const bool stereo = P[14] == 1;
	set_lfo_speeds(lfo, lfo_count, in.rom, lfo_frequency);
	const float wet = scaled(output_level(in));
	p.wet_mix[0][0] = p.wet_mix[0][1] = p.wet_mix[1][0] = p.wet_mix[1][1] = wet;
	const float feedback = scaled(centered_feedback(feedback_level, feedback_step));
	p.first_line_feedback[0] = p.first_line_feedback[1] = p.second_line_feedback[0] = p.second_line_feedback[1] = feedback;
	const int wide = read_s16(in.rom, wide_voice_depths + 2 * (lfo_depth & 0xff)), narrow = read_s16(in.rom, narrow_voice_depths + 2 * (lfo_depth & 0xff));
	p.voice_depth[0] = p.voice_depth[2] = scaled(wide, 32768.0);
	p.voice_depth[1] = p.voice_depth[3] = scaled(narrow, 32768.0);
	set_first_line_input(p.first_line_from_left, p.first_line_from_right, stereo);
	// 揺れ幅の差の半分だけ、狭いほうの声を遅らせて揺れの中心を揃える
	const int center_shift = (wide - narrow) >> 1;
	const int delay = chorus_delay_samples(in.rom, delay_offset);
	const int second_line_write = stereo ? stereo_second_line_write : 0;
	const int first_read = first_line_write + delay;
	// 遅れがまったく無いときは、書き込み位置を読まないように全部の声を 1 つ先へずらす
	const bool touching = stereo && delay == 0 && center_shift == 0;
	set_voice(p, 0, first_read + 1 + touching, first_line_write, second_line_write);
	set_voice(p, 1, first_read + center_shift + 1 + touching, first_line_write, second_line_write);
	const int second_read = stereo ? stereo_second_line_write + delay : first_read;
	set_voice(p, 2, second_read + 1 + touching, first_line_write, second_line_write);
	set_voice(p, 3, second_read + center_shift + touching, first_line_write, second_line_write);
	set_stereo_shelves(in, p);
}

// --- 戻り ---------------------------------------------------------------
// リバーブとコーラスの戻りの {左右の減衰, 送りの減衰}
inline void system_returns(const system_request &in, u16 returns[4][2])
{
	using tables::detail::read_u8;
	const u8 *rom = in.rom;
	const int reverb_pan = in.returns[0], reverb_return = in.returns[1], chorus_pan = in.returns[2], chorus_return = in.returns[3], chorus_to_reverb = in.returns[4];
	auto pan_left = [&](int v) { return u16(read_u8(rom, tables::PAN_TAB + 0x80 - v) << 8 | 0xff); };
	auto pan_right = [&](int v) { return u16(read_u8(rom, tables::PAN_TAB + v) | 0xff00); };
	const u16 reverb_sends = u16(read_u8(rom, tables::RET_TAB + reverb_return) << 8 | 0xff);
	const u16 chorus_sends = u16(read_u8(rom, tables::RET_TAB + chorus_return) << 8 | read_u8(rom, tables::RET_TAB + chorus_to_reverb));
	const u16 values[4][2] = {{pan_left(reverb_pan), reverb_sends}, {pan_right(reverb_pan), reverb_sends},
		{pan_left(chorus_pan), chorus_sends}, {pan_right(chorus_pan), chorus_sends}};
	std::copy(&values[0][0], &values[0][0] + 8, &returns[0][0]);
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_SYSTEM_H
