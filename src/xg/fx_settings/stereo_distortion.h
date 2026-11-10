// license:BSD-3-Clause
// S-MU2000: STREO DT、STREO OD、STREO AMP。doc/fx-flow/parameters.md の stereo_distortion。
// 左右それぞれに同じ歪みの道を持つ。

#ifndef S_MU2000_XG_FX_SETTINGS_STEREO_DISTORTION_H
#define S_MU2000_XG_FX_SETTINGS_STEREO_DISTORTION_H

#include "xg/fx_settings/drive_parts.h"

namespace smu2000::fx_settings {

namespace stereo_distortion {

using namespace drive_parts;

inline void set_pre_first(stereo_distortion_params &p, const first_order &f) { put(f, p.pre_first_current, p.pre_first_previous, p.pre_first_feedback); }
inline void set_pre_second(stereo_distortion_params &p, const first_order &f) { put(f, p.pre_second_current, p.pre_second_previous, p.pre_second_feedback); }
inline void set_post_lowpass(stereo_distortion_params &p, const second_order &f) { put(f, p.post_lowpass_current, p.post_lowpass_x1, p.post_lowpass_x2, p.post_lowpass_y1, p.post_lowpass_y2); }
inline void set_post_mid(stereo_distortion_params &p, const second_order &f) { put(f, p.post_mid_current, p.post_mid_x1, p.post_mid_x2, p.post_mid_y1, p.post_mid_y2); }
inline void set_post_low(stereo_distortion_params &p, const second_order &f) { put(f, p.post_low_current, p.post_low_x1, p.post_low_x2, p.post_low_y1, p.post_low_y2); }

inline void set_shared_parts(const effect_request &in, stereo_distortion_params &p, int drive, int output_level)
{
	const u16 *P = in.P;
	const int dry_wet_value = P[9], edge = P[10];
	const auto gains = drive_levels(in, drive, output_level);
	for (int ch = 0; ch < 2; ch++)
		set_clip_stages(gains, edge, &p.clip_gain[ch][1], p.clip_mix[ch], p.post_gain[ch]);
	set_dry_wet(in, p, dry_wet_value);
}

inline void stereo_drive(const effect_request &in, stereo_distortion_params &p)
{
	constexpr int distortion_make = 1;
	constexpr int shelf_frequency = 4, shelf_gain = 61, distortion_lowpass_frequency = 50;
	constexpr int lowpass_width = 16;
	const u16 *P = in.P;
	const int drive = P[0], low_frequency = P[1], low_gain = P[2], lowpass_cutoff = P[3], output_level = P[4], mid_frequency = P[6], mid_gain = P[7], mid_width = P[8];
	if (in.make == distortion_make) {
		set_pre_first(p, first_order_filter(in.rom, low_shelf_shape, range_2x, shelf_frequency, shelf_gain));
		set_pre_second(p, first_order_filter(in.rom, lowpass_shape, range_2x, distortion_lowpass_frequency));
	} else {
		set_pre_first(p, first_order_filter(in.rom, highpass_shape, range_2x, overdrive_voicing.highpass_frequency));
		set_pre_second(p, first_order_filter(in.rom, lowpass_shape, range_2x, overdrive_voicing.shape_frequency));
	}
	set_post_low(p, second_order_filter(in.rom, low_shelf_shape, range_16x, low_frequency, low_gain));
	set_post_lowpass(p, second_order_filter(in.rom, steep_lowpass_shape, range_2x, lowpass_cutoff, flat_gain, lowpass_width));
	set_post_mid(p, second_order_filter(in.rom, peaking_shape, range_2x, mid_frequency, mid_gain, mid_width));
	set_shared_parts(in, p, drive, output_level);
}

inline void stereo_amp(const effect_request &in, stereo_distortion_params &p)
{
	constexpr int stack_lowpass_width = 15, combo_lowpass_width = 13;
	const u16 *P = in.P;
	const int drive = P[0], amp_type = P[1], lowpass_cutoff = P[2], output_level = P[3];
	set_pre_first(p, amp_type == combo_amp ? first_order_filter(in.rom, through_shape, range_2x, 0)
		: first_order_from(in.rom, range_2x, amp_voice(in, amp_type, amp_entry_filter)));
	set_pre_second(p, amp_shape(in, amp_type));
	set_post_mid(p, amp_mid(in, amp_type));
	set_post_low(p, amp_low(in, amp_type));
	set_post_lowpass(p, second_order_filter(in.rom, steep_lowpass_shape, range_2x, lowpass_cutoff, amp_lowpass_gain(amp_type),
		amp_type == stack_amp ? stack_lowpass_width : amp_type == combo_amp ? combo_lowpass_width : butterworth_width));
	set_shared_parts(in, p, drive, output_level);
}

} // namespace stereo_distortion

inline bool settle(const effect_request &in, stereo_distortion_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	switch (in.kind) {
	case tables::effect_stereo_drive: stereo_distortion::stereo_drive(in, p); return true;
	case tables::effect_stereo_amp: stereo_distortion::stereo_amp(in, p); return true;
	default: return false;
	}
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_STEREO_DISTORTION_H
