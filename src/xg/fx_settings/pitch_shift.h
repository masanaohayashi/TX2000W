// license:BSD-3-Clause
// S-MU2000: pitch_shift 回路の設定。doc/fx-flow/parameters.md の pitch_shift。
// EQ 3 段と、読み頭 2 本のピッチシフトが左右に 1 組ずつある。EQ だけの種類はピッチシフトを使わない。

#ifndef S_MU2000_XG_FX_SETTINGS_PITCH_SHIFT_H
#define S_MU2000_XG_FX_SETTINGS_PITCH_SHIFT_H

namespace smu2000::fx_settings {

namespace pitch_shift {

constexpr int saw_lfo[2] = {4, 5};			// 左右ののこぎり LFO
constexpr int saw_speed_bits = 0x03ff;
constexpr int second_pitch_make = 2;			// PITCH CNG2
constexpr int second_semitone_up_saw_speed = 111;	// PITCH CNG2 の半音上げの速さ。1 周 22307 サンプル（表は 104 で 26214 サンプル）
constexpr int unison = 64;				// Pitch、Fine、Detune の中央

constexpr int detune_ratio_table = rom_at::pitch_shift_detune_ratio_table;			// Fine の値 → 周波数の比
constexpr int detune_ratio_first_value = 14;					// その表の先頭の値
constexpr int fine_depth_table = rom_at::pitch_shift_fine_depth_table;			// PITCH CNG1: 半音ずらさない時の揺れ幅（Fine の中央からの距離で引く）
constexpr int semitone_depth_table = rom_at::pitch_shift_semitone_depth_table;		// PITCH CNG1: 半音ずらす時の揺れ幅
constexpr int ratio_one_depth_table = rom_at::pitch_shift_ratio_one_depth_table;	// PITCH CNG2: 周波数の比の差 1 に当たる揺れ幅（のこぎりの速さで変わる）
constexpr int exact_semitone_depth_table = rom_at::pitch_shift_exact_semitone_depth_table;	// PITCH CNG2: 半音ちょうどの揺れ幅
constexpr int saw_speed_table = rom_at::pitch_shift_saw_speed_table;		// Pitch → のこぎりの速さ
constexpr int pitch_table_first_value = 40;					// 上の 4 つの表の先頭の Pitch
constexpr double unison_depth_scale = ratio_one_saw_depth;			// 半音ずらさない時の、周波数の比の差 1 に当たる揺れ幅
// 揺れ幅は (周波数の比 - 1) × のこぎりの 1 周の長さ。PITCH CNG2 の半音上げは 1 周が 44100 / 1.977 で、半音の比を掛けた値と引く値
constexpr double semitone_up_saw_length = 44100.0 / 1.977;
constexpr double semitone_up_depth_scale = semitone_up_saw_length * 1.0594630943592953, semitone_up_depth_offset = -semitone_up_saw_length;

constexpr int head_b_extra_delay = 384;					// PITCH CNG で読み頭 B は A よりこれだけ（8.7 ms）遅れて読む

// 2 つの窓の差が大きいほど、足した大きさが 1 を超える。帰還が大きい時は差を詰めて 1 に近づける
constexpr int window_center = -0x2000, window_widest_spread = 0x0800;
constexpr int strong_negative_feedback_below = 32, strong_positive_feedback_above = 96;
constexpr double window_spread_ramp = 31.0;

constexpr int level_table = rom_at::drive_parts_output_level_table;	// OutputLvl → 倍率
constexpr int full_output = 0x4000;			// 利得 1 倍

enum band_stage { low_stage, mid_stage, high_stage };

constexpr int low_adjust_frequency_table = rom_at::pitch_shift_low_adjust_frequency_table;	// Low Adjust → 周波数の番号
constexpr int high_adjust_frequency_offset = 34, high_adjust_off = 26;		// HighAdjust の周波数の番号、その最大は素通し
constexpr int cancel_from_other_channel = 0xc000;				// VOIC CANCL: もう片方の音を引く

// EQ の段 1 つ（左右同じ）
inline void set_band(const effect_request &in, pitch_shift_params &p, band_stage stage, int shape, int frequency, int gain, int width = 0)
{
	const second_order f = second_order_filter(in.rom, shape, stage == high_stage ? range_16x : range_2x, frequency, gain, width);
	for (int ch = 0; ch < 2; ch++)
		put(f, p.band_current[ch][stage], p.band_x1[ch][stage], p.band_x2[ch][stage], p.band_y1[ch][stage], p.band_y2[ch][stage]);
}

// のこぎりの向き
inline void set_saw_direction(u16 &lfo, bool pitch_up) { set_lfo_waveform(lfo, pitch_up ? lfo_falling_saw : lfo_rising_saw); }

inline void set_head_depths(pitch_shift_params &p, int ch, int depth)
{
	p.head_a_depth[ch] = p.head_b_depth[ch] = scaled(depth, 32768.0);
}

inline double detune_ratio(const effect_request &in, int fine) { return tables::detail::read_float(in.rom, detune_ratio_table + 4 * (fine - detune_ratio_first_value)); }

// 半音ずらさず、周波数の比だけ読み頭を動かす時の揺れ幅
inline int detune_depth(const effect_request &in, int fine)
{
	const double ratio = detune_ratio(in, fine);
	return int(tables::detail::tr(fine >= unison ? unison_depth_scale * ratio + -unison_depth_scale : unison_depth_scale - unison_depth_scale * ratio));
}

// 半音（Pitch）と端数（Fine）から読み頭の揺れ幅を求める
inline int pitch_depth(const effect_request &in, int pitch, int fine)
{
	using namespace tables::detail;
	const int semitones = pitch - unison, fine_offset = fine - unison;
	const int row = pitch - pitch_table_first_value;
	if (in.make != second_pitch_make) {
		if (semitones == 0)
			return read_s16(in.rom, fine_depth_table + 2 * std::abs(fine_offset));
		const int depth = read_s16(in.rom, semitone_depth_table + 2 * row);
		return semitones < 0 ? depth - fine_offset : depth + fine_offset;
	}
	if (semitones == 0)
		return detune_depth(in, fine);
	const double ratio = detune_ratio(in, fine);
	if (semitones == 1)
		return int(tr(semitone_up_depth_scale * ratio + semitone_up_depth_offset));
	const double one = read_float(in.rom, ratio_one_depth_table + 4 * row), exact = read_float(in.rom, exact_semitone_depth_table + 4 * row);
	return int(tr(semitones < 0 ? f32(f32(f32(exact - one) * ratio) + one) : f32(f32(f32(exact + one) * ratio) - one)));
}

// Pan と OutputLvl から、ピッチの音 1 つの左右の量を求める
inline void set_pitch_output(const effect_request &in, pitch_shift_params &p, int voice, int pan, int level)
{
	using namespace tables::detail;
	const double gain = read_float(in.rom, level_table + 4 * level);
	const auto [left, right] = crossfade(in.rom, pan);
	p.wet_left[voice] = scaled(int(tr(f32(double(left) * gain))));
	p.wet_right[voice] = scaled(int(tr(f32(double(right) * gain))));
}

inline void pitch_change(const effect_request &in, pitch_shift_params &p, effect_extras &x)
{
	using namespace tables::detail;
	const u16 *P = in.P;
	const int pitch = P[0], initial_delay = P[1], feedback = P[4];
	const int fine[2] = {P[2], P[3]};

	int saw_speed = read_u16(in.rom, saw_speed_table + 2 * (pitch - pitch_table_first_value));
	if (in.make == second_pitch_make && pitch == unison + 1)
		saw_speed = second_semitone_up_saw_speed;
	for (int ch : {0, 1}) {
		set_head_depths(p, ch, pitch_depth(in, pitch, fine[ch]));
		u16 &lfo = x.lfo[saw_lfo[ch]];
		set_saw_direction(lfo, (pitch - unison) * 100 + fine[ch] - unison > 0);
		lfo = u16((lfo & ~saw_speed_bits) | saw_speed);
	}

	const int delay = long_delay_samples(in.rom, initial_delay) + 1;
	for (int ch : {0, 1}) {
		p.head_a_base_delay[ch] = delay & 0xffff;
		p.head_b_base_delay[ch] = (delay + head_b_extra_delay) & 0xffff;
	}

	p.first_line_feedback = p.second_line_feedback = scaled(centered_feedback(feedback));
	int spread = window_widest_spread;
	if (feedback < strong_negative_feedback_below)
		spread = int(tr(window_widest_spread * (feedback - 1.0) / window_spread_ramp));
	else if (feedback > strong_positive_feedback_above)
		spread = int(tr(window_widest_spread * (127.0 - feedback) / window_spread_ramp));
	for (int ch : {0, 1}) {
		p.window_a_offset[ch] = scaled(window_center + spread);
		p.window_b_offset[ch] = scaled(window_center - spread);
	}

	const auto g = dry_wet(in, P[9]);
	p.out_left_from_input = p.out_right_from_input = scaled(g.dry, 4.0);
	p.out_left_from_wet = p.out_right_from_wet = scaled(g.wet, 4.0);
	set_pitch_output(in, p, 0, P[10], P[11]);
	set_pitch_output(in, p, 1, P[12], P[13]);
}

inline void ensemble_detune(const effect_request &in, pitch_shift_params &p, effect_extras &x)
{
	const u16 *P = in.P;
	const int detune = P[0];
	// 左右は中央をはさんで逆の向きへずらす。中央は揺れ幅が 0 なので向きは初期のまま
	set_head_depths(p, 0, detune_depth(in, 128 - detune));
	set_head_depths(p, 1, detune_depth(in, detune));
	if (in.held_detune != unison) {
		set_saw_direction(x.lfo[saw_lfo[0]], in.held_detune < unison);
		set_saw_direction(x.lfo[saw_lfo[1]], in.held_detune > unison);
	}
	for (int ch : {0, 1}) {
		const int delay = chorus_delay_samples(in.rom, P[1 + ch]) + 1;
		p.head_a_base_delay[ch] = p.head_b_base_delay[ch] = delay & 0xffff;
	}
	const auto g = dry_wet(in, P[9]);
	p.out_left_from_left = p.out_right_from_right = scaled(g.dry, 4.0);
	p.out_left_from_wet = p.out_right_from_wet = scaled(g.wet, 4.0);
	set_band(in, p, low_stage, low_shelf_shape, P[10], P[11]);
	set_band(in, p, high_stage, high_shelf_shape, P[12], P[13]);
}

// VOIC CANCL: 入力の左右の差を取る
inline void voice_cancel(const effect_request &in, pitch_shift_params &p)
{
	const u16 *P = in.P;
	const int low_adjust = P[10], high_adjust = P[11];
	p.out_left_from_right = p.out_right_from_left = scaled(output_gain(in, cancel_from_other_channel), 4.0);
	// SYSTEM 接続のバリエーションは元の音を混ぜない
	p.out_left_from_input = p.out_right_from_input = scaled(in.connected_to_system() ? 0 : full_output, 4.0);
	set_band(in, p, low_stage, highpass_shape, tables::detail::read_u8(in.rom, low_adjust_frequency_table + (low_adjust & 0xff)), flat_gain);
	set_band(in, p, high_stage, high_adjust == high_adjust_off ? through_shape : lowpass_shape, (high_adjust & 0xff) + high_adjust_frequency_offset, flat_gain);
}

inline void two_band_eq(const effect_request &in, pitch_shift_params &p)
{
	const u16 *P = in.P;
	p.out_left_from_left = p.out_right_from_right = nominal_output_gain(in);
	set_band(in, p, low_stage, low_shelf_shape, P[0], P[1]);
	set_band(in, p, high_stage, high_shelf_shape, P[2], P[3]);
}

inline void three_band_eq(const effect_request &in, pitch_shift_params &p)
{
	const u16 *P = in.P;
	const int low_gain = P[0], mid_frequency = P[1], mid_gain = P[2], mid_width = P[3], high_gain = P[4], low_frequency = P[5], high_frequency = P[6];
	const bool stereo_input = P[14];
	set_band(in, p, low_stage, steep_low_shelf_shape, low_frequency, low_gain);
	set_band(in, p, mid_stage, peaking_shape, mid_frequency, mid_gain, mid_width);
	set_band(in, p, high_stage, steep_high_shelf_shape, high_frequency, high_gain);
	// ステレオは左右そのまま、モノラルは右の道に左右を半分ずつ入れて両方の出口へ出す
	p.input_from_left[0] = scaled(stereo_input ? full_output : 0, 2.0);
	p.input_from_left[1] = scaled(stereo_input ? 0 : full_output / 2, 2.0);
	p.input_from_right[1] = scaled(stereo_input ? full_output : full_output / 2, 2.0);
	p.out_left_from_left = scaled(stereo_input ? output_gain(in, full_output) : 0, 4.0);
	p.out_left_from_right = scaled(stereo_input ? 0 : output_gain(in, full_output), 4.0);
	p.out_right_from_right = nominal_output_gain(in);
}

} // namespace pitch_shift

inline bool settle(const effect_request &in, pitch_shift_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	switch (in.kind) {
	case tables::effect_pitch_change: pitch_shift::pitch_change(in, p, x); return true;
	case tables::effect_ensemble_detune: pitch_shift::ensemble_detune(in, p, x); return true;
	case tables::effect_pass_through: pitch_shift::voice_cancel(in, p); return true;
	case tables::effect_two_band_eq: pitch_shift::two_band_eq(in, p); return true;
	case tables::effect_three_band_eq: pitch_shift::three_band_eq(in, p); return true;
	default: return false;
	}
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_PITCH_SHIFT_H
