// license:BSD-3-Clause
// S-MU2000: DYNA FLANG。doc/fx-flow/dyna.md の DYNA FLANG。

#ifndef S_MU2000_XG_FX_SETTINGS_DYNA_FLANGER_H
#define S_MU2000_XG_FX_SETTINGS_DYNA_FLANGER_H

#include "xg/fx_settings/dyna_parts.h"

namespace smu2000::fx_settings {

namespace dyna_flanger {

constexpr int sensitivity_curve = rom_at::dyna_flanger_sensitivity_curve;
constexpr double sensitivity_by_direction[2] = {38196.01171875, 23401.783203125};	// Direction 0 とそれ以外。1024 で割り曲線の最大 0.731 を掛けると Sensitivty 最大の倍率（27.3 倍、16.7 倍）
constexpr int voice_depth_samples = 112;

} // namespace dyna_flanger

inline bool settle(const effect_request &in, dyna_flanger_params &p, effect_extras &x)
{
	using namespace dyna_flanger;
	start_settle(in, p, x);
	if (in.kind != tables::effect_dyna_flanger)
		return false;
	const u16 *P = in.P;
	const int sensitivity_value = P[0], delay_offset = P[1], feedback_level = P[2], direction = P[6], threshold_level = P[7], level_offset_value = P[8], dry_wet_value = P[9];
	p.sensitivity = dyna::sensitivity(in, sensitivity_curve, sensitivity_value, sensitivity_by_direction[direction != 0]);
	p.level_offset = scaled(dyna::level_offset(level_offset_value));
	dyna::set_follower(in, p, P[3], P[4], P[5], threshold_level);
	dyna::set_direction(p, direction != 0);
	dyna::set_eq(in, p);
	set_dry_wet(in, p, dry_wet_value);
	const int delay = chorus_delay_samples(in.rom, delay_offset) + 1;
	for (int ch = 0; ch < 2; ch++) {
		p.line_feedback[ch] = scaled(dyna::feedback_gain(feedback_level));
		p.voice_base_delay[ch] = delay;
		p.voice_depth[ch] = scaled(voice_depth_samples, 32768.0);
	}
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_DYNA_FLANGER_H
