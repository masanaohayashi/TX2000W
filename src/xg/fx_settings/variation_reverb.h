// license:BSD-3-Clause
// S-MU2000: HALL、ROOM、STAGE、PLATE と、WHITE ROOM、TUNNEL、CANYON、BASEMENT。doc/fx-flow/parameters.md の variation_reverb。
// 違いは櫛形の長さと減衰の決め方だけ。前者は種類ごとの表、後者は部屋の寸法から求める。

#ifndef S_MU2000_XG_FX_SETTINGS_VARIATION_REVERB_H
#define S_MU2000_XG_FX_SETTINGS_VARIATION_REVERB_H

#include "xg/fx_settings/reverb_parts.h"

namespace smu2000::fx_settings {

namespace variation_reverb {

using namespace reverb_parts;

constexpr int tunnel_make = 10, canyon_make = 12;

// 櫛形 1 本の長さと、そこから左右の出口へのタップ。片方は Diffusion の分だけ短い
inline void set_comb_lengths(const effect_request &in, variation_reverb_params &p, int comb, int length, int tap, int diffusion)
{
	int shorter = tap - diffusion_offset(in.rom, diffusion);
	if (s16(u16(shorter)) <= 0)
		shorter = 1;
	p.comb_delay[comb] = length & 0xffff;
	p.out_delay[comb & 1][comb + 1] = tap & 0xffff;
	p.out_delay[(comb & 1) ^ 1][comb + 1] = shorter & 0xffff;
}

// --- HALL、ROOM、STAGE、PLATE ------------------------------------------------
inline void table_combs(const effect_request &in, variation_reverb_params &p, int reverb_time, int diffusion, int high_damp)
{
	using namespace tables::detail;
	const int row = size_class(in.rom, in.make, comb_size);
	const auto damping = damping_filter(in.rom, range_2x, high_damp);
	const int base_level = read_u8(in.rom, decay_steps + reverb_time);
	for (int comb = 0; comb < 4; comb++) {
		const int at = comb_table + row * comb_table_stride + comb * 4;
		set_comb_lengths(in, p, comb, mulhi(read_u16(in.rom, at + 2)), mulhi(read_u16(in.rom, at)), diffusion);
		const int decay = read_u8(in.rom, comb_decay_offsets + row * comb_decay_offsets_stride + comb);
		set_comb_damping(p, comb, damping, decay ? comb_level_at(in.rom, base_level + decay) : comb_level{0, 0});
	}
}

// --- WHITE ROOM、TUNNEL、CANYON、BASEMENT --------------------------------------
enum room_axis { width_axis, height_axis, depth_axis };

inline void room_combs(const effect_request &in, variation_reverb_params &p, int reverb_time, int diffusion, int high_damp)
{
	using namespace tables::detail;
	const u16 *P = in.P;
	const int size[3] = {P[5], P[6], P[7]}, wall_vary = P[8];
	const int varied_axis = in.make == tunnel_make ? depth_axis : in.make == canyon_make ? width_axis : height_axis;
	const auto damping = damping_filter(in.rom, range_2x, high_damp);
	const auto set_comb = [&](int comb, int room_size, int axis) {
		const int length = read_u16(in.rom, room_comb_lengths + 2 * room_size);
		set_comb_lengths(in, p, comb, mulhi(length), mulhi(length >> 1), diffusion);
		const int level = int(read_u8(in.rom, room_decay_offsets + room_size)) + read_u8(in.rom, decay_steps + nearest_time(in.rom, reverb_time, room_time_scale(in.rom, in.make, axis)));
		set_comb_damping(p, comb, damping, comb_level_at(in.rom, level));
	};
	for (int axis : {width_axis, height_axis, depth_axis})
		set_comb(axis, size[axis], axis);
	set_comb(3, size[varied_axis] + wall_vary, varied_axis);	// 4 本目は壁で変わる 1 辺
}

// --- 共通 ---------------------------------------------------------------------
// Er/Rev: 初期の音の大きさと、櫛形のタップの大きさ。初期反射は 1 本だけで、システムのリバーブの 1 本目の左の大きさを使う
inline void set_out_gains(const effect_request &in, variation_reverb_params &p, int er_rev)
{
	using namespace tables::detail;
	const auto [early_level, late] = crossfade(in.rom, er_rev, full_at_center_curve);
	const double early_share = f32(double(early_level) / 32768.0);
	const int early = sx(tr(f32(double(read_s16(in.rom, early_tap_table + early_left_gain + early_table_stride * size_class(in.rom, in.make, early_size))) * early_share)));
	for (int ch = 0; ch < 2; ch++) {
		p.out_gain[ch][0] = scaled(early);
		for (int comb = 0; comb < 4; comb++)
			p.out_gain[ch][comb + 1] = scaled(late);
	}
}

inline void set_shared_parts(const effect_request &in, variation_reverb_params &p)
{
	const u16 *P = in.P;
	const int diffusion = P[1], init_delay = P[2], highpass_cutoff = P[3], lowpass_cutoff = P[4], dry_wet_value = P[9], reverb_delay = P[10], density = P[11], er_rev = P[12], feedback_level = P[14];
	set_reverb_input_filters(in.rom, p, highpass_cutoff, lowpass_cutoff);

	const int early_delay = reverb_delay_samples(in.rom, init_delay);
	p.predelay = (early_delay + reverb_delay_samples(in.rom, reverb_delay)) & 0xffff;
	p.predelay_feedback = scaled(centered_feedback(feedback_level));
	p.out_delay[0][0] = early_delay & 0xffff;
	p.out_delay[1][0] = (early_delay + diffusion_offset(in.rom, diffusion)) & 0xffff;

	set_diffuse_shelf(in.rom, p, in.make);
	set_reverb_allpasses(in.rom, p, size_class(in.rom, in.make, allpass_size), density, diffusion);
	set_out_gains(in, p, er_rev);
	set_dry_wet(in, p, dry_wet_value, full_at_center_curve);
}

} // namespace variation_reverb

inline bool settle(const effect_request &in, variation_reverb_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	const int reverb_time = in.P[0], diffusion = in.P[1], high_damp = in.P[13];
	switch (in.kind) {
	case tables::effect_hall_room_plate: variation_reverb::table_combs(in, p, reverb_time, diffusion, high_damp); break;
	case tables::effect_large_space_reverb: variation_reverb::room_combs(in, p, reverb_time, diffusion, high_damp); break;
	default: return false;
	}
	variation_reverb::set_shared_parts(in, p);
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_VARIATION_REVERB_H
