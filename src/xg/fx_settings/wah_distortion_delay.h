// license:BSD-3-Clause
// S-MU2000: wah_distortion_delay 回路の設定。doc/fx-flow/parameters.md の wah_distortion_delay。
// 入口の EQ → ワウ → 歪み → 音色の補正 → 遅れ。

#ifndef S_MU2000_XG_FX_SETTINGS_WAH_DISTORTION_DELAY_H
#define S_MU2000_XG_FX_SETTINGS_WAH_DISTORTION_DELAY_H

#include "xg/fx_settings/drive_parts.h"

namespace smu2000::fx_settings {

namespace wah_distortion_delay {

using namespace drive_parts;

constexpr int overdrive_make[2] = {2, 6};
constexpr int eq_low_frequency = 28, eq_low_gain = 66, eq_high_frequency = 46;
constexpr int detector_highpass_frequency = 0, envelope_lowpass_frequency = 54;

// 入口の EQ と検出のフィルタ（固定）
inline void set_fixed_filters(const effect_request &in, wah_distortion_delay_params &p)
{
	set_first_order_shelves(in.rom, p, eq_low_frequency, eq_low_gain, eq_high_frequency, flat_gain, range_4x);
	put(first_order_filter(in.rom, highpass_shape, range_2x, detector_highpass_frequency), p.detector_filter_current, p.detector_filter_previous, p.detector_filter_feedback);
	put(first_order_filter(in.rom, lowpass_shape, range_1x, envelope_lowpass_frequency), p.envelope_filter_current, p.envelope_filter_previous, p.envelope_filter_feedback);
}

inline void set_wah(const effect_request &in, wah_distortion_delay_params &p, int sensitivity, int cutoff, int resonance, int release)
{
	p.sensitivity = scaled(sensitivity * wah_sensitivity_step, 16.0);
	p.cutoff_base = scaled(wah_cutoff_base(cutoff));
	p.resonance = scaled(wah_resonance(resonance));
	set_wah_release(in, p, release);
}

// 左右を 1 つに混ぜる利得。和の大きさを保つ 1/√2 で、2 つは丸めだけ違う
// SYSTEM 接続は 1 倍なので中央の音は 3 dB 強く飽和へ入り、飽和のあとで 3 dB 下げる
inline void set_inputs(const effect_request &in, wah_distortion_delay_params &p)
{
	constexpr int drive_stereo_to_mono = sum_to_mono(0x8000), wah_stereo_to_mono = sum_to_mono(0x7FFF), full_level = 0x7FFF;
	const bool to_system = in.connected_to_system();
	p.drive_from_left = p.drive_from_right = scaled(to_system ? full_level : drive_stereo_to_mono);
	p.wah_from_left = p.wah_from_right = scaled(to_system ? full_level : wah_stereo_to_mono);
}

inline void set_dry_wet_around_drive(const effect_request &in, wah_distortion_delay_params &p, int drive, int dry_wet_value)
{
	const wah_mix_gains mix = wah_output_mix(in, drive, dry_wet_value);
	p.wah_left_from_eq = p.wah_right_from_eq = scaled(mix.from_eq, 4.0);
	p.wah_left_from_band = p.wah_right_from_band = scaled(mix.from_band, 4.0);
	p.mix_left_from_wah = p.mix_right_from_wah = scaled(mix.from_wah, 4.0);
	p.mix_left_from_post = p.mix_right_from_post = scaled(mix.from_post, 4.0);
}

inline void wah_drive_delay(const effect_request &in, wah_distortion_delay_params &p)
{
	const u16 *P = in.P;
	const int delay = P[0], feedback_level = P[1], delay_mix = P[2], drive = P[3], output_level = P[4], low_gain = P[5], mid_gain = P[6];
	const int dry_wet_value = P[9], sensitivity = P[10], cutoff = P[11], resonance = P[12], release = P[13];
	const drive_voicing &voice = (in.make == overdrive_make[0] || in.make == overdrive_make[1]) ? overdrive_voicing : distortion_voicing;
	p.delay_feedback_time = p.delay_left_time = p.delay_right_time = delay_samples(delay);
	set_delay_level(in, p, feedback_level, delay_mix);
	set_delay_damp(in, p);
	set_fixed_filters(in, p);
	set_wah(in, p, sensitivity, cutoff, resonance, release);
	set_inputs(in, p);
	set_dry_wet_around_drive(in, p, drive, dry_wet_value);
	// 歪みと音色の補正
	put(first_order_filter(in.rom, highpass_shape, range_2x, voice.highpass_frequency), p.drive_low_current, p.drive_low_previous, p.drive_low_feedback);
	put(first_order_filter(in.rom, lowpass_shape, range_2x, voice.shape_frequency), p.drive_high_current, p.drive_high_previous, p.drive_high_feedback);
	set_clip_stages(drive_levels(in, drive, output_level), voice.clip_edge, p.clip_gain, p.clip_mix, p.post_gain);
	put(first_order_filter(in.rom, lowpass_shape, range_2x, voice.post_lowpass_frequency), p.post_first_current, p.post_first_previous, p.post_first_feedback);
	put(second_order_filter(in.rom, peaking_shape, range_2x, voice.post_mid_frequency, mid_gain, mid_band_width), p.post_second_current, p.post_second_x1, p.post_second_x2, p.post_second_y1, p.post_second_y2);
	put(first_order_filter(in.rom, low_shelf_shape, range_4x, voice.post_low_frequency, low_gain), p.post_third_current, p.post_third_previous, p.post_third_feedback);
}

} // namespace wah_distortion_delay

inline bool settle(const effect_request &in, wah_distortion_delay_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	if (in.kind != tables::effect_wah_drive_delay)
		return false;
	wah_distortion_delay::wah_drive_delay(in, p);
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_WAH_DISTORTION_DELAY_H
