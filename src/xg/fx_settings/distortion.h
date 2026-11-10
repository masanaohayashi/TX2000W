// license:BSD-3-Clause
// S-MU2000: DISTORTION、OVERDRIVE、AMP SIM、CMP+DT、COMPRESSOR、NOISE GATE、HM ENHNCER。doc/fx-flow/parameters.md の distortion。
// 道 A と道 B の利得を音量検出で決め、道 A を飽和の段に通す。

#ifndef S_MU2000_XG_FX_SETTINGS_DISTORTION_H
#define S_MU2000_XG_FX_SETTINGS_DISTORTION_H

#include "xg/fx_settings/drive_parts.h"

namespace smu2000::fx_settings {

namespace distortion {

using namespace drive_parts;

// --- 回路の部品 ------------------------------------------------------------
inline auto highpass_a(distortion_params &p) { return std::tie(p.highpass_a_current, p.highpass_a_x1, p.highpass_a_x2, p.highpass_a_y1, p.highpass_a_y2); }
inline auto highpass_b(distortion_params &p) { return std::tie(p.highpass_b_current, p.highpass_b_x1, p.highpass_b_x2, p.highpass_b_y1, p.highpass_b_y2); }
inline auto out_filter_a(distortion_params &p) { return std::tie(p.out_filter_a_current, p.out_filter_a_previous, p.out_filter_a_feedback); }
inline auto out_filter_b(distortion_params &p) { return std::tie(p.out_filter_b_current, p.out_filter_b_previous, p.out_filter_b_feedback); }
inline auto post_lowpass(distortion_params &p) { return std::tie(p.post_lowpass_current, p.post_lowpass_x1, p.post_lowpass_x2, p.post_lowpass_y1, p.post_lowpass_y2); }
inline auto post_mid(distortion_params &p) { return std::tie(p.post_mid_current, p.post_mid_x1, p.post_mid_x2, p.post_mid_y1, p.post_mid_y2); }
inline auto post_low(distortion_params &p) { return std::tie(p.post_low_current, p.post_low_x1, p.post_low_x2, p.post_low_y1, p.post_low_y2); }

// 道 A（右の入力）と道 B（左の入力）を分けて通す種類は、出口で左右を入れ替えて、右の入力を右へ戻す。
// 接続に関係しない（入れ替えないと、左いっぱいの入力が右に出る。2 つの要素を左右に振った音色では、インサーションで左右が逆になって 1.2 dB 違った）
inline void set_split_sides(distortion_params &p) { p.swap_sides = true; }

// 飽和の段を使わない種類の出口（道 A を左へ、道 B を右へ）
inline void set_undriven_output(const effect_request &in, distortion_params &p, int output_level)
{
	set_split_sides(p);
	using namespace tables::detail;
	const float level = scaled(output_gain(in, int(tr(double(read_float(in.rom, output_level_table + 4 * output_level)) * 16384.0))), 4.0);
	p.left_from_post = p.right_from_b = level;
	p.right_from_post = p.dry_to_left = p.dry_to_right = scaled(0);
}
inline void set_detector(distortion_params &p, int level) { p.detector_a = p.detector_b = scaled(level, 16.0); }

// 音色のフィルターを通さない
inline void set_through_tone(const effect_request &in, distortion_params &p)
{
	post_lowpass(p) = post_mid(p) = second_order_filter(in.rom, through_shape, range_2x, 0);
	post_low(p) = second_order_filter(in.rom, through_shape, range_16x, 0);
}
// COMPRESSOR と NOISE GATE: 入口は直流だけを落とし、ほかは通す
inline void set_dynamics_paths(const effect_request &in, distortion_params &p)
{
	highpass_a(p) = highpass_b(p) = second_order_filter(in.rom, highpass_shape, range_2x, 0);
	out_filter_a(p) = out_filter_b(p) = first_order_filter(in.rom, through_shape, range_2x, 0);
	set_through_tone(in, p);
}

// 歪みの道の出口
inline void set_drive_output(const effect_request &in, distortion_params &p, int dry_wet_value)
{
	const auto g = dry_wet(in, dry_wet_value);
	p.dry_to_left = p.dry_to_right = scaled(g.dry);
	p.left_from_post = p.right_from_post = scaled(g.wet, 4.0);
	p.right_from_b = scaled(0);
}
inline void set_clip_path(const effect_request &in, distortion_params &p, int drive, int output_level, int edge)
{
	set_clip_stages(drive_levels(in, drive, output_level), edge, &p.clip_gain[1], p.clip_mix, p.post_gain);
}
// 歪みの音色: 低域、ローパス、中域
inline void set_drive_tone(const effect_request &in, distortion_params &p)
{
	const u16 *P = in.P;
	const int low_frequency = P[1], low_gain = P[2], lowpass_cutoff = P[3], mid_frequency = P[6], mid_gain = P[7], mid_width = P[8];
	post_low(p) = second_order_filter(in.rom, low_shelf_shape, range_16x, low_frequency, low_gain);
	post_lowpass(p) = second_order_filter(in.rom, steep_lowpass_shape, range_2x, lowpass_cutoff, flat_gain, butterworth_width);
	post_mid(p) = second_order_filter(in.rom, peaking_shape, range_2x, mid_frequency, mid_gain, mid_width);
}
inline void set_entry(const effect_request &in, distortion_params &p, const drive_voicing &voice)
{
	highpass_a(p) = second_order_filter(in.rom, highpass_shape, range_2x, voice.highpass_frequency);
	out_filter_a(p) = first_order_filter(in.rom, lowpass_shape, range_2x, voice.shape_frequency);
}

// --- 種類 ----------------------------------------------------------------
inline void distortion_overdrive(const effect_request &in, distortion_params &p)
{
	constexpr int distortion_make = 1;
	const u16 *P = in.P;
	const int drive = P[0], output_level = P[4], dry_wet_value = P[9], edge = P[10];
	set_built_in_gate(in, p);
	set_entry(in, p, in.make == distortion_make ? distortion_voicing : overdrive_voicing);
	set_drive_tone(in, p);
	set_gate_path(in, p.shape_gain_a, p.shape_dry_a);
	set_drive_output(in, p, dry_wet_value);
	set_clip_path(in, p, drive, output_level, edge);
}

inline void amp_simulator(const effect_request &in, distortion_params &p)
{
	const u16 *P = in.P;
	const int drive = P[0], amp_type = P[1], lowpass_cutoff = P[2], output_level = P[3], dry_wet_value = P[9], edge = P[10];
	set_built_in_gate(in, p);
	highpass_a(p) = second_order_from(in.rom, range_2x, amp_voice(in, amp_type, amp_entry_filter));
	out_filter_a(p) = amp_shape(in, amp_type);
	post_mid(p) = amp_mid(in, amp_type);
	post_low(p) = amp_low(in, amp_type);
	post_lowpass(p) = amp_post_lowpass(in, lowpass_cutoff, amp_type);
	set_gate_path(in, p.shape_gain_a, p.shape_dry_a);
	set_drive_output(in, p, dry_wet_value);
	set_clip_path(in, p, drive, output_level, edge);
}

inline void compressor(const effect_request &in, distortion_params &p)
{
	constexpr int sum_detector = 0x2800, sum_to_mono_detector = sum_to_mono(sum_detector);	// 左右の和に掛ける値（5 倍と、SYSTEM 接続以外はその 1/√2）
	const u16 *P = in.P;
	const int attack = P[0], release = P[1], threshold = P[2], ratio = P[3], output_level = P[4];
	set_dynamics_paths(in, p);
	set_compressor_attack(in, p, attack);
	set_release(in, p, release);
	set_compressor_threshold(in, p, threshold);
	set_ratio(in, p, ratio);
	set_undriven_output(in, p, output_level);
	set_detector(p, in.connected_to_system() ? sum_detector : sum_to_mono_detector);
}

inline void noise_gate(const effect_request &in, distortion_params &p)
{
	const u16 *P = in.P;
	const int attack = P[0], release = P[1], threshold = P[2], output_level = P[3];
	set_dynamics_paths(in, p);
	set_gate_attack(in, p, attack);
	set_release(in, p, release);
	set_gate_threshold(in, p, threshold);
	set_undriven_output(in, p, output_level);
}

inline void compressor_drive(const effect_request &in, distortion_params &p)
{
	const u16 *P = in.P;
	const int drive = P[0], output_level = P[4], dry_wet_value = P[9], edge = P[10], attack = P[11], release = P[12], threshold = P[13], ratio = P[14];
	set_entry(in, p, distortion_voicing);
	set_drive_tone(in, p);
	set_compressor_path(in, p.shape_gain_a, p.detector_a);
	set_drive_output(in, p, dry_wet_value);
	set_clip_path(in, p, drive, output_level, edge);
	set_compressor_attack(in, p, attack);
	set_release(in, p, release);
	set_compressor_threshold(in, p, threshold);
	set_ratio(in, p, ratio);
}

inline void harmonic_enhancer(const effect_request &in, distortion_params &p)
{
	constexpr int sum_detector = 0x800, sum_to_mono_detector = sum_to_mono(sum_detector);	// 左右の和に掛ける値（1 倍と、SYSTEM 接続以外はその 1/√2）
	constexpr int drive_scale = panel_value_step, mix_level_scale = 129;
	const u16 *P = in.P;
	const int highpass_cutoff = P[0], drive = P[1], mix_level = P[2];
	set_through_tone(in, p);
	highpass_a(p) = highpass_b(p) = second_order_filter(in.rom, steep_highpass_shape, range_2x, highpass_cutoff, flat_gain, butterworth_width);
	out_filter_a(p) = out_filter_b(p) = first_order_filter(in.rom, highpass_shape, range_2x, highpass_cutoff);
	p.drive_a = p.drive_b = scaled(drive * drive_scale, 2.0);
	p.left_from_post = p.right_from_b = scaled(output_gain(in, mix_level * mix_level_scale), 4.0);
	set_split_sides(p);
	set_detector(p, in.connected_to_system() ? sum_detector : sum_to_mono_detector);
}

} // namespace distortion

inline bool settle(const effect_request &in, distortion_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	switch (in.kind) {
	case tables::effect_distortion: distortion::distortion_overdrive(in, p); return true;
	case tables::effect_amp_sim: distortion::amp_simulator(in, p); return true;
	case tables::effect_compressor: distortion::compressor(in, p); return true;
	case tables::effect_noise_gate: distortion::noise_gate(in, p); return true;
	case tables::effect_compressor_drive: distortion::compressor_drive(in, p); return true;
	case tables::effect_harmonic_enhancer: distortion::harmonic_enhancer(in, p); return true;
	default: return false;
	}
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_DISTORTION_H
