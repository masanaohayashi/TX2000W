// license:BSD-3-Clause
// S-MU2000: DYNA FLT。doc/fx-flow/dyna.md の DYNA FLT。
// 状態変数フィルタの出口のうち、Type が選ぶ 1 つだけを Wet の大きさで出す。

#ifndef S_MU2000_XG_FX_SETTINGS_DYNA_FILTER_H
#define S_MU2000_XG_FX_SETTINGS_DYNA_FILTER_H

#include "xg/fx_settings/dyna_parts.h"

namespace smu2000::fx_settings {

namespace dyna_filter {

constexpr double sensitivity_by_direction[2] = {9521.9521484375, 17103.8515625};	// Direction 0 とそれ以外。1024 で割ると Sensitivty 最大の倍率（9.3 倍、16.7 倍）
constexpr int resonance_table = rom_at::dyna_filter_resonance_table;
constexpr int damping_from_level = q15_floor(-0.1);	// 制御信号が大きいほど減衰を小さくする（-0.1 倍）
enum filter_type { lpf_12, lpf_18, lpf_24, hpf, bpf, bef };	// Type

// Type が選ぶ出口
inline float (&chosen_output(dyna_filter_params &p, int type))[2]
{
	switch (type) {
	case lpf_18: return p.from_lowpass1;
	case lpf_24: return p.from_lowpass2;
	case hpf: return p.from_high;
	case bpf: return p.from_band;
	case bef: return p.from_input;
	default: return p.from_low;
	}
}

} // namespace dyna_filter

inline bool settle(const effect_request &in, dyna_filter_params &p, effect_extras &x)
{
	using namespace dyna_filter;
	start_settle(in, p, x);
	if (in.kind != tables::effect_dyna_filter)
		return false;
	const u16 *P = in.P;
	const int type = P[0], sensitivity_value = P[1], level_offset_value = P[2], resonance_value = P[3], direction = P[7], threshold_level = P[8], dry_wet_value = P[9];
	p.sensitivity = dyna::sensitivity(in, dyna::level_curve, sensitivity_value, sensitivity_by_direction[direction != 0]);
	p.level_offset = scaled(dyna::level_offset(level_offset_value));
	p.resonance = scaled(int(tables::detail::read_u16(in.rom, resonance_table + 2 * resonance_value)));
	dyna::set_follower(in, p, P[4], P[5], P[6], threshold_level);
	dyna::set_direction(p, direction == 0);
	dyna::set_eq(in, p);
	p.damping_from_control = scaled(type == hpf || type == bef ? 0 : damping_from_level);
	const auto mix = dry_wet(in, dry_wet_value);
	for (int ch = 0; ch < 2; ch++) {
		p.dry_gain[ch] = scaled(mix.dry);
		p.from_input[ch] = p.from_high[ch] = p.from_band[ch] = p.from_low[ch] = p.from_lowpass1[ch] = p.from_lowpass2[ch] = 0.0f;
	}
	auto &output = chosen_output(p, type);
	output[0] = output[1] = scaled(mix.wet);
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_DYNA_FILTER_H
