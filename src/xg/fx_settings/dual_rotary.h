// license:BSD-3-Clause
// S-MU2000: DUAL ROTR1、DUAL ROTR2。doc/fx-flow/parameters.md の dual_rotary。
// 入力をローター（低い音）とホーン（高い音）に分け、線 2 本を 8 本の声で読む。
// 声 0-3（フローの high_band の線）がローター、声 4-7（low_band の線）がホーン。

#ifndef S_MU2000_XG_FX_SETTINGS_DUAL_ROTARY_H
#define S_MU2000_XG_FX_SETTINGS_DUAL_ROTARY_H

namespace smu2000::fx_settings {

namespace dual_rotary {

constexpr int horn_first_voice = 4, rotor_first_voice = 0;
constexpr int horn_lfos[3] = {0, 1, 2}, rotor_lfos[3] = {3, 4, 5};

// 分割のフィルタ（種類で決まる）
struct band_filters {
	int rotor_lowpass_frequency;
	int rotor_tone_shape, rotor_tone_frequency, rotor_tone_gain;
	int horn_highpass_frequency, horn_lowpass_frequency;
};
constexpr band_filters rotary_1_bands = {22, lowpass_shape, 34, 60, 32, 44};
constexpr band_filters rotary_2_bands = {28, high_shelf_shape, 38, 52, 38, 44};

inline void set_bands(const effect_request &in, dual_rotary_params &p, const band_filters &b)
{
	constexpr int rotor_width = 7, horn_width = 13;
	put(second_order_filter(in.rom, lowpass_shape, range_2x, b.rotor_lowpass_frequency, flat_gain, rotor_width), p.rotor_first_current, p.rotor_first_previous, p.rotor_first_feedback);
	put(second_order_filter(in.rom, b.rotor_tone_shape, range_2x, b.rotor_tone_frequency, b.rotor_tone_gain, rotor_width), p.rotor_second_current, p.rotor_second_previous, p.rotor_second_feedback);
	put(second_order_filter(in.rom, steep_highpass_shape, range_2x, b.horn_highpass_frequency, flat_gain, horn_width), p.horn_first_current, p.horn_first_x1, p.horn_first_x2, p.horn_first_y1, p.horn_first_y2);
	put(second_order_filter(in.rom, steep_lowpass_shape, range_2x, b.horn_lowpass_frequency, flat_gain, horn_width), p.horn_second_current, p.horn_second_x1, p.horn_second_x2, p.horn_second_y1, p.horn_second_y2);
}

// ローター 1 組の揺れ。左右に共通の声 2 本（LFO のままと、反転）と、左右別の声 2 本で音量の揺れが違う
struct rotor_motion { int shared_voices_am, side_voices_am, doppler; };
inline void set_motion(dual_rotary_params &p, int first_voice, const rotor_motion &m)
{
	for (int k = 0; k < 4; k++) {
		p.voice_depth[first_voice + k] = scaled(m.doppler / 127, 32768.0);
		p.voice_am_depth[first_voice + k] = am_depth(k < 2 ? m.shared_voices_am : m.side_voices_am);
	}
}

// DUAL ROTR1: Slow と Fast で揺れの効き方が違う
inline rotor_motion rotor_1_motion(int drive, bool fast)
{
	if (fast) {
		const int am = drive >= 34 ? 127 : (drive * 15) >> 1;
		return { am, am * 13 / 20, drive * 0x1000 };
	}
	const int am = drive >= 18 ? 127 : (drive * 57) >> 3;
	return { am, am * 25 / 38, drive * 0x1400 };
}
inline rotor_motion horn_1_motion(int drive, bool fast)
{
	if (fast) {
		const int am = drive >= 52 ? 64 : drive * 15 / 12;
		return { am, am * 26 / 15, drive * 0x1000 };
	}
	const int am = drive >= 23 ? 70 : drive * 19 / 6;
	return { am, am * 15 / 19, drive * 0x3800 / 3 };
}
// DUAL ROTR2
inline rotor_motion rotor_2_motion(int drive)
{
	const int am = drive >= 43 ? 127 : drive * 3;
	return { am, am, drive * 0x1000 };
}
inline rotor_motion horn_2_motion(int drive)
{
	const int am = drive >= 32 ? 64 : drive * 2;
	return { am, am, drive * 0x1000 };
}

// 2 種類で同じ部分: 速さ、出口、Low/High、EQ、Mic Angle
inline void set_shared_parts(const effect_request &in, dual_rotary_params &p, effect_extras &x)
{
	const u16 *P = in.P;
	const int rotor_slow = P[0], horn_slow = P[1], rotor_fast = P[2], horn_fast = P[3], low_high = P[8];
	const int low_eq_frequency = P[10], low_eq_gain = P[11], high_eq_frequency = P[12], high_eq_gain = P[13];
	const int mic_angle = P[14], speed_control = P[15];
	for (int lfo : rotor_lfos)
		set_lfo_speed(x.lfo[lfo], in.rom, speed_control ? rotor_fast : rotor_slow);
	for (int lfo : horn_lfos)
		set_lfo_speed(x.lfo[lfo], in.rom, speed_control ? horn_fast : horn_slow);

	p.out_gain[0] = p.out_gain[1] = nominal_output_gain(in);

	// 左右とも、共通の声（LFO のまま）、共通の声（LFO を反転）、左右別の声の順に混ぜる
	// ローターは 3/4、-1/8、1。ホーンは全体が 9/10 で 1/2、-1/8、1。反転の声は逆相で小さく足す
	const auto [low_level, high_level] = crossfade(in.rom, low_high);
	const int rotor_direct = low_level / 4 * 3, rotor_opposite = low_level / -8, rotor_side = low_level;
	const int horn_direct = high_level / 20 * 9, horn_opposite = high_level / -80 * 9, horn_side = high_level / 10 * 9;
	const int mix[6] = { rotor_direct, rotor_opposite, rotor_side, horn_direct, horn_opposite, horn_side };
	for (int k = 0; k < 6; k++)
		p.left_mix[k] = p.right_mix[k] = scaled(mix[k]);

	put(first_order_filter(in.rom, low_shelf_shape, range_16x, low_eq_frequency, low_eq_gain), p.rotor_third_current, p.rotor_third_previous, p.rotor_third_feedback);
	put(first_order_filter(in.rom, high_shelf_shape, range_16x, high_eq_frequency, high_eq_gain), p.horn_third_current, p.horn_third_previous, p.horn_third_feedback);

	// 左右のマイクの向き: 左右別の声の LFO の始まりを開く
	constexpr int horn_center = 64, rotor_center = 94;
	constexpr int horn_side_lfo[2] = {1, 2}, rotor_side_lfo[2] = {4, 5};
	const int half_angle = mic_angle >> 1;
	set_lfo_start_phase(x.lfo[horn_side_lfo[0]], in.rom, horn_center - half_angle);
	set_lfo_start_phase(x.lfo[horn_side_lfo[1]], in.rom, horn_center + half_angle);
	set_lfo_start_phase(x.lfo[rotor_side_lfo[0]], in.rom, rotor_center - half_angle);
	set_lfo_start_phase(x.lfo[rotor_side_lfo[1]], in.rom, rotor_center + half_angle);
}

inline void dual_rotary_1(const effect_request &in, dual_rotary_params &p, effect_extras &x)
{
	const u16 *P = in.P;
	const int rotor_drive = P[6], horn_drive = P[7];
	const bool fast = P[15];
	set_bands(in, p, rotary_1_bands);
	set_motion(p, rotor_first_voice, rotor_1_motion(rotor_drive, fast));
	set_motion(p, horn_first_voice, horn_1_motion(horn_drive, fast));
	set_shared_parts(in, p, x);
}

inline void dual_rotary_2(const effect_request &in, dual_rotary_params &p, effect_extras &x)
{
	const u16 *P = in.P;
	const int rotor_drive = P[6], horn_drive = P[7];
	set_bands(in, p, rotary_2_bands);
	set_motion(p, rotor_first_voice, rotor_2_motion(rotor_drive));
	set_motion(p, horn_first_voice, horn_2_motion(horn_drive));
	set_shared_parts(in, p, x);
}

} // namespace dual_rotary

inline bool settle(const effect_request &in, dual_rotary_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	switch (in.kind) {
	case tables::effect_dual_rotary_1: dual_rotary::dual_rotary_1(in, p, x); return true;
	case tables::effect_dual_rotary_2: dual_rotary::dual_rotary_2(in, p, x); return true;
	default: return false;
	}
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_DUAL_ROTARY_H
