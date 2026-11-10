// license:BSD-3-Clause
// S-MU2000: DYNA RING。doc/fx-flow/dyna.md の DYNA RING。
// 音量から搬送波の周波数を作り、帯域を絞った入力に掛ける。サイン波の表は遅延メモリに置く。

#ifndef S_MU2000_XG_FX_SETTINGS_DYNA_RING_H
#define S_MU2000_XG_FX_SETTINGS_DYNA_RING_H

#include "xg/fx_settings/dyna_parts.h"

namespace smu2000::fx_settings {

namespace dyna_ring {

constexpr double sensitivity_by_direction[2] = {12010.6435546875, 14650.974609375};	// Direction 0 とそれ以外。1024 で割ると Sensitivty 最大の倍率（11.7 倍、14.3 倍）
constexpr int band_width = 7;

} // namespace dyna_ring

inline bool settle(const effect_request &in, dyna_ring_params &p, effect_extras &x)
{
	using namespace dyna_ring;
	start_settle(in, p, x);
	if (in.kind != tables::effect_dyna_ring)
		return false;
	const u16 *P = in.P;
	const int sensitivity_value = P[0], highpass_cutoff = P[1], lowpass_cutoff = P[2], direction = P[6], threshold_level = P[7], level_offset_value = P[8], dry_wet_value = P[9];

	set_sine_carrier(in, p, x);

	p.sensitivity = dyna::sensitivity(in, dyna::level_curve, sensitivity_value, sensitivity_by_direction[direction != 0]);
	p.level_offset = scaled(dyna::level_offset(level_offset_value));
	dyna::set_follower(in, p, P[3], P[4], P[5], threshold_level);
	dyna::set_direction(p, direction == 0);
	dyna::set_eq(in, p);
	set_dry_wet(in, p, dry_wet_value);

	put(second_order_filter(in.rom, steep_highpass_shape, range_2x, highpass_cutoff, flat_gain, band_width), p.highpass_current, p.highpass_x1, p.highpass_x2, p.highpass_y1, p.highpass_y2);
	put(second_order_filter(in.rom, steep_lowpass_shape, range_2x, lowpass_cutoff, flat_gain, band_width), p.lowpass_current, p.lowpass_x1, p.lowpass_x2, p.lowpass_y1, p.lowpass_y2);
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_DYNA_RING_H
