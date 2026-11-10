// license:BSD-3-Clause
// S-MU2000: DYNA PHASE。doc/fx-flow/dyna.md の DYNA PHASE。

#ifndef S_MU2000_XG_FX_SETTINGS_DYNA_PHASER_H
#define S_MU2000_XG_FX_SETTINGS_DYNA_PHASER_H

#include "xg/fx_settings/dyna_parts.h"

namespace smu2000::fx_settings {

namespace dyna_phaser {

constexpr double sensitivity_by_direction[2] = {21532.595703125, 17103.8515625};	// Direction 0 とそれ以外。1024 で割ると Sensitivty 最大の倍率（21.0 倍、16.7 倍）
constexpr int reversed_coefficient_limit = 0x8060;	// Direction が逆のとき、音量が大きいほど係数 a を 0 から -0.997 へ動かす
constexpr int full_tap = 0x4000;				// 段の取り出し（+1 倍）

} // namespace dyna_phaser

inline bool settle(const effect_request &in, dyna_phaser_params &p, effect_extras &x)
{
	using namespace dyna_phaser;
	start_settle(in, p, x);
	if (in.kind != tables::effect_dyna_phaser)
		return false;
	const u16 *P = in.P;
	const int sensitivity_value = P[0], level_offset_value = P[1], feedback_level = P[2], direction = P[6], threshold_level = P[7], dry_wet_value = P[9], stage = P[10];
	p.sensitivity = dyna::sensitivity(in, dyna::level_curve, sensitivity_value, sensitivity_by_direction[direction != 0]);
	p.level_offset = scaled(dyna::level_offset(level_offset_value * 2));	// DYNA PHASE だけ刻みが倍
	dyna::set_follower(in, p, P[3], P[4], P[5], threshold_level);
	p.direction_up = scaled(direction ? reversed_coefficient_limit : 0);
	p.direction_down = scaled(direction ? 0 : 0x7fff);
	dyna::set_eq(in, p);
	set_dry_wet(in, p, dry_wet_value);
	for (int ch = 0; ch < 2; ch++) {
		p.chain_feedback[ch] = scaled(dyna::feedback_gain(feedback_level));
		p.tap_stage4[ch] = scaled(stage == 4 ? full_tap : 0, 2.0);
		p.tap_stage5[ch] = scaled(stage == 5 ? full_tap : 0, 2.0);
		p.tap_stage6[ch] = scaled(stage == 6 ? full_tap : 0, 2.0);
	}
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_DYNA_PHASER_H
