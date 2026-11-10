// license:BSD-3-Clause
// S-MU2000: CHORUS 3。doc/fx-flow/parameters.md の chorus3。
// 遅延線 2 本を声 4 つで読む。揺れの小さい声は、揺れの中心が揃うように読む位置をずらす。

#ifndef S_MU2000_XG_FX_SETTINGS_CHORUS3_H
#define S_MU2000_XG_FX_SETTINGS_CHORUS3_H

namespace smu2000::fx_settings {

namespace chorus3 {

constexpr int stored_first_line_write = 20;		// ROM の初期の値の中の位置
constexpr int wide_depth_curve = rom_at::chorus3_wide_depth_curve, narrow_depth_curve = rom_at::chorus3_narrow_depth_curve;
constexpr int stereo_input_mode = 1;
constexpr int feedback_step = 250;

} // namespace chorus3

inline bool settle(const effect_request &in, chorus3_params &p, effect_extras &x)
{
	using namespace chorus3;
	start_settle(in, p, x);
	if (in.kind != tables::effect_chorus3)
		return false;
	const u16 *P = in.P;
	const int lfo_frequency = P[0], lfo_depth = P[1] & 0xff, feedback_level = P[2], delay_offset = P[3];
	const bool stereo = P[14] == stereo_input_mode;

	set_lfo_speeds(x.lfo + 2, 4, in.rom, lfo_frequency);
	const int wide_depth = tables::detail::read_u16(in.rom, wide_depth_curve + 2 * lfo_depth);
	const int narrow_depth = tables::detail::read_u16(in.rom, narrow_depth_curve + 2 * lfo_depth);
	p.voice_depth[0] = p.voice_depth[3] = scaled(wide_depth, 32768.0);
	p.voice_depth[1] = p.voice_depth[2] = scaled(narrow_depth, 32768.0);

	const int center_shift = (s16(u16(wide_depth)) - s16(u16(narrow_depth))) >> 1;
	const int delay = chorus_delay_samples(in.rom, delay_offset);
	const int first_write = in.stored_read_position(stored_first_line_write);
	const int second_write = stereo ? half_memory : 0;
	const int first_read = s16(u16(first_write + delay));
	const int second_read = stereo ? half_memory + delay : first_read;
	set_voice(p, 0, first_read + 1, first_write, second_write);
	set_voice(p, 1, first_read + center_shift, first_write, second_write);
	set_voice(p, 2, second_read + center_shift + 1, first_write, second_write);
	set_voice(p, 3, second_read + 1, first_write, second_write);
	set_first_line_input(p.first_line_from_left, p.first_line_from_right, stereo);

	const float feedback = scaled(centered_feedback(feedback_level, feedback_step));
	for (int k = 0; k < 2; k++)
		p.first_line_feedback[k] = p.second_line_feedback[k] = feedback;
	set_shelves(in.rom, p, P[5], P[6], P[7], P[8]);
	set_mid_band(in.rom, p, P[10], P[11], P[12]);
	set_dry_wet(in, p, P[9]);
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_CHORUS3_H
