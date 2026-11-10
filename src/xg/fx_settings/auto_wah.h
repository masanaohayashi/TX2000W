// license:BSD-3-Clause
// S-MU2000: AUTO WAH、A-WAH+DT、A-WAH+OD、TOUCH WAH、T-WAH+DIST、T-WAH+ODRV。doc/fx-flow/parameters.md の auto_wah。
// LFO か入力の音量で遮断周波数を動かす共振フィルターと、飽和の道。

#ifndef S_MU2000_XG_FX_SETTINGS_AUTO_WAH_H
#define S_MU2000_XG_FX_SETTINGS_AUTO_WAH_H

#include "xg/fx_settings/drive_parts.h"

namespace smu2000::fx_settings {

namespace auto_wah {

using namespace drive_parts;

// 飽和の道の作りの違い
struct voicing {
	bool overdrive_entry_band;	// 飽和の前を OVERDRIVE の帯域に
	bool overdrive_tone;		// 飽和のあとを OVERDRIVE の音色に
	bool panel_tone;			// 飽和のあとの音色をパネルで決める
	int fixed_output_level;		// パネルで決めない種類の出力レベル
};

inline bool is_one_of(int make, const int (&makes)[2]) { return make == makes[0] || make == makes[1]; }

inline void set_resonance(auto_wah_params &p, int value) { p.resonance[0] = p.resonance[1] = scaled(wah_resonance(value)); }

inline void set_output_mix(const effect_request &in, auto_wah_params &p, int drive, int dry_wet_value)
{
	const wah_mix_gains mix = wah_output_mix(in, drive, dry_wet_value);
	for (int ch = 0; ch < 2; ch++) {
		p.wah_from_eq[ch] = scaled(mix.from_eq, 4.0);
		p.wah_from_band[ch] = scaled(mix.from_band, 4.0);
	}
	p.left_from_wah = p.right_from_wah = scaled(mix.from_wah, 4.0);
	p.left_from_post = p.right_from_post = scaled(mix.from_post, 4.0);
}

// AUTO WAH と TOUCH WAH で同じ部分: EQ、飽和の道、出口
inline void set_shared_parts(const effect_request &in, auto_wah_params &p, const voicing &v)
{
	constexpr int fixed_low_gain = 72, fixed_mid_gain = 74, fixed_lowpass_cutoff = 53;
	constexpr int stereo_to_mono = sum_to_mono(0x8000), full_level = 0x7FFF;	// 左右の和を 1/√2 にする。SYSTEM 接続は 1 倍
	const u16 *P = in.P;
	const int low_frequency = P[5], low_gain = P[6], high_frequency = P[7], high_gain = P[8], dry_wet_value = P[9], drive = P[10];
	const int tone_low_gain = v.panel_tone ? P[11] : fixed_low_gain;
	const int tone_mid_gain = v.panel_tone ? P[12] : fixed_mid_gain;
	const int lowpass_cutoff = v.panel_tone ? P[13] : fixed_lowpass_cutoff;
	const int output_level = v.panel_tone ? P[14] : v.fixed_output_level;

	set_first_order_shelves(in.rom, p, low_frequency, low_gain, high_frequency, high_gain, range_4x);

	const drive_voicing &entry = v.overdrive_entry_band ? overdrive_voicing : distortion_voicing;
	const drive_voicing &tone = v.overdrive_tone ? overdrive_voicing : distortion_voicing;
	put(first_order_filter(in.rom, highpass_shape, range_2x, entry.highpass_frequency), p.drive_low_current, p.drive_low_previous, p.drive_low_feedback);
	put(first_order_filter(in.rom, lowpass_shape, range_2x, entry.shape_frequency), p.drive_high_current, p.drive_high_previous, p.drive_high_feedback);
	set_clip_stages(drive_levels(in, drive, output_level), entry.clip_edge, p.clip_gain, p.clip_mix, p.post_gain);
	p.drive_from_left = p.drive_from_right = scaled(in.connected_to_system() ? full_level : stereo_to_mono);
	set_output_mix(in, p, drive, dry_wet_value);

	put(second_order_filter(in.rom, steep_low_shelf_shape, range_16x, tone.post_low_frequency, tone_low_gain), p.post_low_current, p.post_low_x1, p.post_low_x2, p.post_low_y1, p.post_low_y2);
	put(second_order_filter(in.rom, peaking_shape, range_2x, tone.post_mid_frequency, tone_mid_gain, mid_band_width), p.post_mid_current, p.post_mid_x1, p.post_mid_x2, p.post_mid_y1, p.post_mid_y2);
	put(second_order_filter(in.rom, steep_lowpass_shape, range_2x, lowpass_cutoff, flat_gain, butterworth_width), p.post_lowpass_current, p.post_lowpass_x1, p.post_lowpass_x2, p.post_lowpass_y1, p.post_lowpass_y2);
}

inline void lfo_wah(const effect_request &in, auto_wah_params &p, effect_extras &x)
{
	constexpr int auto_wah_makes[2] = {1, 8}, overdrive_makes[2] = {4, 10};
	constexpr int fixed_output_level = 57;
	const u16 *P = in.P;
	const int lfo_frequency = P[0], lfo_depth = P[1], cutoff = P[2], resonance_value = P[3];
	const bool overdrive = is_one_of(in.make, overdrive_makes);
	set_lfo_speed(x.lfo[0], in.rom, lfo_frequency);
	const int base = wah_cutoff_base(cutoff);
	p.cutoff_base = scaled(base);
	p.cutoff_from_lfo = scaled((0x7FFF - base) * lfo_depth / 127);	// 基準から最大までの幅を LFO で動かす
	set_resonance(p, resonance_value);
	set_shared_parts(in, p, {overdrive, overdrive, !is_one_of(in.make, auto_wah_makes), fixed_output_level});
}

inline void touch_wah(const effect_request &in, auto_wah_params &p)
{
	constexpr int touch_wah_1_make = 2;
	constexpr int touch_wah_2_make = 7;
	constexpr int overdrive_makes[2] = {6, 12};
	constexpr int overdrive_entry_only_makes[2] = {13, 14};	// TOUCH WAH2 の形で、飽和の前だけ OVERDRIVE の帯域。初期の Drive は 0
	constexpr int touch_wah_1_output_level = 57, fixed_level = 48;
	constexpr int detector_stereo_to_mono = sum_to_mono(0x7FFF), full_level = 0x7FFF;
	// TOUCH WAH1 は検出を +6 dB、包絡を +9 dB にする
	constexpr int touch_wah_1_detector_gain = 70, touch_wah_1_envelope_frequency = 51, touch_wah_1_envelope_gain = 73, envelope_frequency = 54;
	const u16 *P = in.P;
	const int sensitivity = P[0], cutoff = P[1], resonance_value = P[2], release_value = P[15];
	const bool touch_wah_1 = in.make == touch_wah_1_make;
	const bool overdrive_tone = is_one_of(in.make, overdrive_makes);
	const bool overdrive_band = overdrive_tone || is_one_of(in.make, overdrive_entry_only_makes);
	const bool panel_tone = overdrive_band || in.make == touch_wah_2_make;

	p.sensitivity = scaled(sensitivity * wah_sensitivity_step, 16.0);
	p.cutoff_base = scaled(wah_cutoff_base(cutoff));
	set_resonance(p, resonance_value);
	p.detector_from_left = p.detector_from_right = scaled(in.connected_to_system() ? full_level : detector_stereo_to_mono);

	put(first_order_filter(in.rom, highpass_shape, range_2x, 0, touch_wah_1 ? touch_wah_1_detector_gain : flat_gain), p.detector_filter_current, p.detector_filter_previous, p.detector_filter_feedback);
	put(first_order_filter(in.rom, lowpass_shape, range_1x, touch_wah_1 ? touch_wah_1_envelope_frequency : envelope_frequency, touch_wah_1 ? touch_wah_1_envelope_gain : flat_gain), p.envelope_filter_current, p.envelope_filter_previous, p.envelope_filter_feedback);
	if (touch_wah_1) {	// Release のパネルが無い。ピークを使わず、ならした値がほぼ 1 の帰還で約 0.74 秒で戻る
		p.peak_release = p.smooth_from_peak = scaled(0);
		p.smooth_feedback = p.cutoff_from_envelope = scaled(full_level);
	}
	if (panel_tone)
		set_wah_release(in, p, release_value);
	set_shared_parts(in, p, {overdrive_band, overdrive_tone, panel_tone, touch_wah_1 ? touch_wah_1_output_level : fixed_level});
}

} // namespace auto_wah

inline bool settle(const effect_request &in, auto_wah_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	switch (in.kind) {
	case tables::effect_auto_wah: auto_wah::lfo_wah(in, p, x); return true;
	case tables::effect_touch_wah: auto_wah::touch_wah(in, p); return true;
	default: return false;
	}
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_AUTO_WAH_H
