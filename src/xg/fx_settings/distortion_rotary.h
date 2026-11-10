// license:BSD-3-Clause
// S-MU2000: DT/OD +RTRY、AMP+RTRY、DT/OD +2RTRY、AMP+2RTRY。doc/fx-flow/parameters.md の distortion_rotary。
// モノラルにして飽和の段に通し、帯に分けて線へ入れ、4 本の声で読む。

#ifndef S_MU2000_XG_FX_SETTINGS_DISTORTION_ROTARY_H
#define S_MU2000_XG_FX_SETTINGS_DISTORTION_ROTARY_H

#include "xg/fx_settings/drive_parts.h"

namespace smu2000::fx_settings {

namespace distortion_rotary {

using namespace drive_parts;

// 飽和の前後の音色
struct tone_setting { filter_setting pre_first, pre_second, post_mid, post_low; };

// DT と OD の +RTRY、+2RTRY の音色
constexpr tone_setting dt_rotary_tone = {{highpass_shape, 0, flat_gain, 0}, {lowpass_shape, 47, flat_gain, 0}, {peaking_shape, 34, 68, 10}, {peaking_shape, 40, 68, 15}};
constexpr tone_setting od_rotary_tone = {{highpass_shape, 0, flat_gain, 0}, {lowpass_shape, 56, flat_gain, 0}, {peaking_shape, 41, 72, 10}, {low_shelf_shape, 24, 68, 0}};
constexpr tone_setting dt_two_way_tone = {{highpass_shape, 0, flat_gain, 0}, {lowpass_shape, 43, flat_gain, 0}, {peaking_shape, 32, 68, 10}, {low_shelf_shape, 27, 69, 0}};
constexpr tone_setting od_two_way_tone = {{through_shape, 0, flat_gain, 0}, {lowpass_shape, 57, flat_gain, 0}, {peaking_shape, 34, 70, 11}, {low_shelf_shape, 26, 71, 0}};
// AmpType のうち、表と違う音色を使うもの
constexpr tone_setting amp_rotary_stack_tone = {{highpass_shape, 25, flat_gain, 0}, {lowpass_shape, 46, flat_gain, 0}, {low_shelf_shape, 25, 76, 0}, {peaking_shape, 34, 67, 10}};
constexpr tone_setting amp_two_way_tube_tone = {{through_shape, 0, flat_gain, 0}, {lowpass_shape, 43, flat_gain, 0}, {steep_low_shelf_shape, 25, 73, 0}, {peaking_shape, 40, 72, 10}};

inline tone_setting amp_table_tone(const effect_request &in, int amp_type)
{
	return { amp_voice(in, amp_type, amp_entry_filter), amp_voice(in, amp_type, amp_shape_filter), amp_voice(in, amp_type, amp_mid_filter), amp_voice(in, amp_type, amp_low_filter) };
}

inline void set_tone(const effect_request &in, distortion_rotary_params &p, const tone_setting &t)
{
	put(first_order_from(in.rom, range_2x, t.pre_first), p.pre_first_current, p.pre_first_previous, p.pre_first_feedback);
	put(first_order_from(in.rom, range_2x, t.pre_second), p.pre_second_current, p.pre_second_previous, p.pre_second_feedback);
	put(second_order_from(in.rom, range_2x, t.post_mid), p.post_mid_current, p.post_mid_x1, p.post_mid_x2, p.post_mid_y1, p.post_mid_y2);
	put(second_order_from(in.rom, range_16x, t.post_low), p.post_low_current, p.post_low_x1, p.post_low_x2, p.post_low_y1, p.post_low_y2);
}

inline void set_post_lowpass(distortion_rotary_params &p, const second_order &f) { put(f, p.post_lowpass_current, p.post_lowpass_x1, p.post_lowpass_x2, p.post_lowpass_y1, p.post_lowpass_y2); }

// 種類で決まる歪みの硬さ
constexpr int dt_edge = 48, od_edge = 104, amp_edge = 112;

// 2 スピードの種類は、接続を求めたときの値で出口の大きさを決める
inline void set_drive(const effect_request &in, distortion_rotary_params &p, int drive, int output_level, int edge)
{
	const bool two_way = in.kind == tables::effect_drive_two_way_rotary || in.kind == tables::effect_amp_two_way_rotary;
	const bool to_system = two_way ? in.variation && (in.held_drive_connection & 1) : in.connected_to_system();
	set_clip_stages(drive_levels(in, drive, output_level, to_system), edge, p.clip_gain, p.clip_mix, p.post_gain);
}

// --- +RTRY: ローター 1 組 ----------------------------------------------------

inline void set_single_rotor_parts(const effect_request &in, distortion_rotary_params &p, effect_extras &x, int edge)
{
	const u16 *P = in.P;
	const int lfo_frequency = P[0], lfo_depth = P[1], dry_wet_value = P[9], drive = P[13], output_level = P[15];
	const int low_eq_frequency = P[5], low_eq_gain = P[6], high_eq_frequency = P[7], high_eq_gain = P[8];
	set_lfo_speeds(x.lfo, 4, in.rom, lfo_frequency);
	for (float &am : p.voice_am_depth)
		am = am_depth(lfo_depth);
	const int depth = lfo_depth & 0xff;
	p.voice_depth[0] = p.voice_depth[1] = modulation_depth(depth >> 5);
	p.voice_depth[2] = p.voice_depth[3] = modulation_depth(depth / 15);

	const auto low_eq = second_order_filter(in.rom, low_shelf_shape, range_2x, low_eq_frequency, low_eq_gain);
	put(low_eq, p.first_band_a_current, p.first_band_a_x1, p.first_band_a_x2, p.first_band_a_y1, p.first_band_a_y2);
	put(low_eq, p.second_band_a_current, p.second_band_a_x1, p.second_band_a_x2, p.second_band_a_y1, p.second_band_a_y2);
	const auto high_eq = second_order_filter(in.rom, steep_high_shelf_shape, range_16x, high_eq_frequency, high_eq_gain);
	put(high_eq, p.first_band_b_current, p.first_band_b_x1, p.first_band_b_x2, p.first_band_b_y1, p.first_band_b_y2);
	put(high_eq, p.second_band_b_current, p.second_band_b_x1, p.second_band_b_x2, p.second_band_b_y1, p.second_band_b_y2);

	const auto gains = dry_wet(in, dry_wet_value);
	p.left_from_first_band = p.right_from_second_band = scaled(gains.dry, 4.0);
	for (int voice : {0, 2, 3})
		p.left_from_voice[voice] = scaled(gains.wet, 4.0);
	for (int voice : {1, 2, 3})
		p.right_from_voice[voice] = scaled(gains.wet, 4.0);
	set_drive(in, p, drive, output_level, edge);
}

inline void drive_rotary(const effect_request &in, distortion_rotary_params &p, effect_extras &x)
{
	constexpr int dt_make = 1, lowpass_width = 7;
	const bool dt = in.make == dt_make;
	const int lowpass_cutoff = in.P[14];
	set_tone(in, p, dt ? dt_rotary_tone : od_rotary_tone);
	set_post_lowpass(p, second_order_filter(in.rom, steep_lowpass_shape, range_2x, lowpass_cutoff, flat_gain, lowpass_width));
	set_single_rotor_parts(in, p, x, dt ? dt_edge : od_edge);
}

inline void amp_rotary(const effect_request &in, distortion_rotary_params &p, effect_extras &x)
{
	const int amp_type = in.P[2], lowpass_cutoff = in.P[14];
	set_tone(in, p, amp_type == stack_amp ? amp_rotary_stack_tone : amp_table_tone(in, amp_type));
	set_post_lowpass(p, amp_post_lowpass(in, lowpass_cutoff, amp_type));
	set_single_rotor_parts(in, p, x, amp_edge);
}

// --- +2RTRY: 線 2 本で低い音と高い音に分ける ----------------------------------
// 声 0、2 が低い音、声 1、3 が高い音

struct crossover_setting { int low_offset, low_width, high_offset, high_width; };
constexpr crossover_setting dt_crossover = {-2, 10, 3, 15};
constexpr crossover_setting od_amp_crossover = {1, 10, -1, 13};

inline void set_two_rotor_parts(const effect_request &in, distortion_rotary_params &p, effect_extras &x, const crossover_setting &crossover, int edge)
{
	const u16 *P = in.P;
	const int rotor_speed = P[0], low_drive = P[1], high_drive = P[2], low_high = P[3];
	const int low_eq_frequency = P[5], low_eq_gain = P[6], high_eq_frequency = P[7], high_eq_gain = P[8];
	const int crossover_frequency = P[10], mic_angle = P[11], drive = P[13], output_level = P[15];
	set_lfo_speeds(x.lfo, 4, in.rom, rotor_speed);
	set_two_way_mic_angle(in, x, mic_angle);

	p.voice_am_depth[0] = p.voice_am_depth[2] = am_depth(low_drive);
	p.voice_depth[0] = p.voice_depth[2] = modulation_depth((low_drive & 0xff) >> 2);
	p.voice_am_depth[1] = p.voice_am_depth[3] = am_depth(high_drive);
	p.voice_depth[1] = p.voice_depth[3] = modulation_depth((high_drive & 0xff) >> 3);

	// 左右とも、片方の声だけを使う
	const auto [low_level, high_level] = crossfade(in.rom, low_high);
	p.left_from_voice[0] = p.right_from_voice[2] = scaled(low_level, 4.0);
	p.left_from_voice[3] = p.right_from_voice[1] = scaled(high_level, 4.0);
	p.left_from_voice[2] = p.right_from_voice[3] = 0.0f;

	put(second_order_filter(in.rom, steep_lowpass_shape, range_2x, (crossover_frequency + crossover.low_offset) & 0xff, flat_gain, crossover.low_width),
		p.first_band_a_current, p.first_band_a_x1, p.first_band_a_x2, p.first_band_a_y1, p.first_band_a_y2);
	put(second_order_filter(in.rom, steep_highpass_shape, range_2x, (crossover_frequency + crossover.high_offset) & 0xff, flat_gain, crossover.high_width),
		p.second_band_a_current, p.second_band_a_x1, p.second_band_a_x2, p.second_band_a_y1, p.second_band_a_y2);
	put(second_order_filter(in.rom, low_shelf_shape, range_16x, low_eq_frequency, low_eq_gain),
		p.first_band_b_current, p.first_band_b_x1, p.first_band_b_x2, p.first_band_b_y1, p.first_band_b_y2);
	put(second_order_filter(in.rom, steep_high_shelf_shape, range_16x, high_eq_frequency, high_eq_gain),
		p.second_band_b_current, p.second_band_b_x1, p.second_band_b_x2, p.second_band_b_y1, p.second_band_b_y2);
	set_drive(in, p, drive, output_level, edge);
}

inline void drive_two_way_rotary(const effect_request &in, distortion_rotary_params &p, effect_extras &x)
{
	constexpr int dt_make = 4, lowpass_width = 8;
	const bool dt = in.make == dt_make;
	const int lowpass_cutoff = in.P[14];
	set_tone(in, p, dt ? dt_two_way_tone : od_two_way_tone);
	set_post_lowpass(p, second_order_filter(in.rom, steep_lowpass_shape, range_2x, lowpass_cutoff, flat_gain, lowpass_width));
	set_two_rotor_parts(in, p, x, dt ? dt_crossover : od_amp_crossover, dt ? dt_edge : od_edge);
}

inline void amp_two_way_rotary(const effect_request &in, distortion_rotary_params &p, effect_extras &x)
{
	const int amp_type = in.P[12], lowpass_cutoff = in.P[14];
	set_tone(in, p, amp_type == tube_amp ? amp_two_way_tube_tone : amp_table_tone(in, amp_type));
	set_post_lowpass(p, amp_post_lowpass(in, lowpass_cutoff, amp_type));
	set_two_rotor_parts(in, p, x, od_amp_crossover, amp_edge);
}

} // namespace distortion_rotary

inline bool settle(const effect_request &in, distortion_rotary_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	switch (in.kind) {
	case tables::effect_drive_rotary: distortion_rotary::drive_rotary(in, p, x); return true;
	case tables::effect_amp_rotary: distortion_rotary::amp_rotary(in, p, x); return true;
	case tables::effect_drive_two_way_rotary: distortion_rotary::drive_two_way_rotary(in, p, x); return true;
	case tables::effect_amp_two_way_rotary: distortion_rotary::amp_two_way_rotary(in, p, x); return true;
	default: return false;
	}
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_DISTORTION_ROTARY_H
