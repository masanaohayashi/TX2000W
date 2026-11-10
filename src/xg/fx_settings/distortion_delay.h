// license:BSD-3-Clause
// S-MU2000: distortion_delay 回路の設定。doc/fx-flow/parameters.md の distortion_delay。
// 圧縮 → 歪み → 音色の補正 → 遅れ。CMP+DT+DLY、CMP+OD+DLY は圧縮を動かし、DT+DELAY、OD+DELAY は圧縮を固定する。

#ifndef S_MU2000_XG_FX_SETTINGS_DISTORTION_DELAY_H
#define S_MU2000_XG_FX_SETTINGS_DISTORTION_DELAY_H

#include "xg/fx_settings/drive_parts.h"

namespace smu2000::fx_settings {

namespace distortion_delay {

using namespace drive_parts;

// --- この回路 ------------------------------------------------------------
constexpr int compressor_overdrive_make[2] = {2, 12}, plain_overdrive_make[2] = {4, 10};
// 歪みの前後のフィルタと飽和の段（種類によらず同じ）
inline void set_drive_path(const effect_request &in, distortion_delay_params &p, const drive_voicing &voice, int drive, int output_level, int low_gain, int mid_gain)
{
	put(second_order_filter(in.rom, highpass_shape, range_2x, voice.highpass_frequency), p.highpass_current, p.highpass_previous, p.highpass_feedback);
	put(first_order_filter(in.rom, lowpass_shape, range_2x, voice.shape_frequency), p.shape_filter_current, p.shape_filter_previous, p.shape_filter_feedback);
	set_clip_stages(drive_levels(in, drive, output_level), voice.clip_edge, &p.clip_gain[1], p.clip_mix, p.post_gain);
	put(second_order_filter(in.rom, steep_lowpass_shape, range_2x, voice.post_lowpass_frequency, flat_gain, butterworth_width), p.post_lowpass_current, p.post_lowpass_x1, p.post_lowpass_x2, p.post_lowpass_y1, p.post_lowpass_y2);
	put(second_order_filter(in.rom, low_shelf_shape, range_16x, voice.post_low_frequency, low_gain), p.post_low_current, p.post_low_previous, p.post_low_feedback);
	put(second_order_filter(in.rom, peaking_shape, range_2x, voice.post_mid_frequency, mid_gain, mid_band_width), p.post_mid_current, p.post_mid_x1, p.post_mid_x2, p.post_mid_y1, p.post_mid_y2);
}

inline void set_mix(const effect_request &in, distortion_delay_params &p, int dry_wet_value)
{
	const auto g = dry_wet(in, dry_wet_value);
	p.mix_left_from_input = p.mix_right_from_input = scaled(g.dry, 4.0);
	p.mix_left_from_post = p.mix_right_from_post = scaled(g.wet, 4.0);
}

inline void compressor_drive_delay(const effect_request &in, distortion_delay_params &p)
{
	const u16 *P = in.P;
	const int delay = P[0], feedback_level = P[1], delay_mix = P[2], drive = P[3], output_level = P[4], low_gain = P[5], mid_gain = P[6];
	const int dry_wet_value = P[9], attack = P[10], release = P[11], threshold = P[12], ratio = P[13];
	const bool overdrive = in.make == compressor_overdrive_make[0] || in.make == compressor_overdrive_make[1];
	p.delay_feedback_time = p.delay_left_time = p.delay_right_time = delay_samples(delay);
	set_delay_level(in, p, feedback_level, delay_mix);
	set_delay_damp(in, p);
	set_drive_path(in, p, overdrive ? overdrive_voicing : distortion_voicing, drive, output_level, low_gain, mid_gain);
	set_compressor_path(in, p.shape_gain, p.detector);
	set_mix(in, p, dry_wet_value);
	set_compressor_attack(in, p, attack);
	set_release(in, p, release);
	set_compressor_threshold(in, p, threshold);
	set_ratio(in, p, ratio);
}

inline void drive_delay(const effect_request &in, distortion_delay_params &p)
{
	const u16 *P = in.P;
	const int left_delay = P[0], right_delay = P[1], feedback_delay = P[2], feedback_level = P[3], delay_mix = P[4];
	const int drive = P[5], output_level = P[6], low_gain = P[7], mid_gain = P[8], dry_wet_value = P[9];
	const bool overdrive = in.make == plain_overdrive_make[0] || in.make == plain_overdrive_make[1];
	p.delay_left_time = delay_samples(left_delay);
	p.delay_right_time = delay_samples(right_delay);
	p.delay_feedback_time = delay_samples(feedback_delay);
	set_delay_level(in, p, feedback_level, delay_mix);
	set_delay_damp(in, p);
	set_drive_path(in, p, overdrive ? overdrive_voicing : distortion_voicing, drive, output_level, low_gain, mid_gain);
	set_gate_path(in, p.shape_gain, p.shape_dry);
	set_mix(in, p, dry_wet_value);
	set_built_in_gate(in, p);
}

} // namespace distortion_delay

inline bool settle(const effect_request &in, distortion_delay_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	switch (in.kind) {
	case tables::effect_compressor_drive_delay: distortion_delay::compressor_drive_delay(in, p); return true;
	case tables::effect_drive_delay: distortion_delay::drive_delay(in, p); return true;
	default: return false;
	}
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_DISTORTION_DELAY_H
