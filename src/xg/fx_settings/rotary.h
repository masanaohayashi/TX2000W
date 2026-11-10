// license:BSD-3-Clause
// S-MU2000: ROTARY SP、2WAY ROTRY、TREMOLO、AUTO PAN、AMBIENCE。doc/fx-flow/parameters.md の rotary。
// 遅延線を 4 本の声で読み、遅れと音量を LFO で揺らす。

#ifndef S_MU2000_XG_FX_SETTINGS_ROTARY_H
#define S_MU2000_XG_FX_SETTINGS_ROTARY_H

namespace smu2000::fx_settings {

namespace rotary {

constexpr int stored_first_line_write = 21, stored_second_line_write = 22;	// ROM の初期の値の中の位置
constexpr int stored_voice_read[4] = {1, 4, 6, 8};

constexpr int full_scale = 0x7fff, negative_full_scale = -0x8000;
constexpr int left_lfo = 0, right_lfo = 1;	// 左の声 0 と右の声 3 を揺らす LFO
constexpr int left_voice = 0, right_voice = 3;

inline void set_wet_gain(const effect_request &in, rotary_params &p) { p.wet_gain[0] = p.wet_gain[1] = nominal_output_gain(in); }

inline void rotary_speaker(const effect_request &in, rotary_params &p, effect_extras &x)
{
	const u16 *P = in.P;
	const int lfo_frequency = P[0], lfo_depth = P[1];
	set_lfo_speeds(x.lfo, 4, in.rom, lfo_frequency);
	// 音量の揺れは早く飽和する。声 2 は種類の値のまま
	const float am = am_depth(lfo_depth >= 32 ? 64 : s8(u8(lfo_depth)) << 1);
	p.voice_am_depth[0] = p.voice_am_depth[1] = p.voice_am_depth[3] = am;
	const float doppler = modulation_depth((lfo_depth & 0xff) >> 2);
	for (float &d : p.voice_depth)
		d = doppler;
	set_shelves(in.rom, p, P[5], P[6], P[7], P[8]);
	set_mid_band(in.rom, p, P[10], P[11], P[12]);
	set_dry_wet(in, p, P[9]);
}

// 線 2 本で低い音と高い音に分ける。声 0、1 が低い音、声 2、3 が高い音
inline void two_way_rotary(const effect_request &in, rotary_params &p, effect_extras &x)
{
	const u16 *P = in.P;
	const int rotor_speed = P[0], low_drive = P[1], high_drive = P[2], low_high = P[3];
	const int low_eq_frequency = P[5], low_eq_gain = P[6], high_eq_frequency = P[7], high_eq_gain = P[8];
	const int crossover = P[10], mic_angle = P[11];
	set_lfo_speeds(x.lfo, 4, in.rom, rotor_speed);
	set_two_way_mic_angle(in, x, mic_angle);

	p.voice_am_depth[0] = p.voice_am_depth[1] = am_depth(low_drive);
	p.voice_depth[0] = p.voice_depth[1] = modulation_depth((low_drive & 0xff) >> 2);
	p.voice_am_depth[3] = p.voice_am_depth[2] = am_depth(high_drive);
	p.voice_depth[3] = p.voice_depth[2] = modulation_depth((high_drive & 0xff) >> 3);

	const auto [low_level, high_level] = crossfade(in.rom, low_high);
	p.wet_left[0] = p.wet_right[1] = scaled(low_level);
	p.wet_left[2] = p.wet_right[3] = scaled(high_level);
	set_wet_gain(in, p);

	// 低い音の道と高い音の道: 帯域分割 → 固定の整形 → EQ
	constexpr int band_width = 7, low_cut_frequency = 8, presence_frequency = 45, presence_gain = 70, presence_width = 10;
	put(second_order_filter(in.rom, lowpass_shape, range_2x, crossover, flat_gain, band_width), p.low_shelf_current[0], p.low_shelf_previous[0], p.low_shelf_feedback[0]);
	put(second_order_filter(in.rom, highpass_shape, range_2x, crossover, flat_gain, band_width), p.low_shelf_current[1], p.low_shelf_previous[1], p.low_shelf_feedback[1]);
	put(second_order_filter(in.rom, steep_highpass_shape, range_2x, low_cut_frequency, flat_gain, band_width), p.mid_current[0], p.mid_x1[0], p.mid_x2[0], p.mid_y1[0], p.mid_y2[0]);
	put(second_order_filter(in.rom, peaking_shape, range_2x, presence_frequency, presence_gain, presence_width), p.mid_current[1], p.mid_x1[1], p.mid_x2[1], p.mid_y1[1], p.mid_y2[1]);
	put(second_order_filter(in.rom, low_shelf_shape, range_16x, low_eq_frequency, low_eq_gain), p.high_shelf_current[0], p.high_shelf_previous[0], p.high_shelf_feedback[0]);
	put(second_order_filter(in.rom, high_shelf_shape, range_16x, high_eq_frequency, high_eq_gain), p.high_shelf_current[1], p.high_shelf_previous[1], p.high_shelf_feedback[1]);
}

// TREMOLO と AUTO PAN で同じ部分: 左右 2 本の声の揺れと EQ
inline void set_left_right_motion(const effect_request &in, rotary_params &p, effect_extras &x, int doppler_value)
{
	const u16 *P = in.P;
	const int lfo_frequency = P[0], am_depth_value = P[1];
	for (int lfo : {left_lfo, right_lfo})
		set_lfo_speed(x.lfo[lfo], in.rom, lfo_frequency);
	for (int voice : {left_voice, right_voice}) {
		p.voice_am_depth[voice] = am_depth(am_depth_value);
		p.voice_depth[voice] = modulation_depth(doppler_value);
	}
	set_wet_gain(in, p);
	set_shelves(in.rom, p, P[5], P[6], P[7], P[8]);
	set_mid_band(in.rom, p, P[10], P[11], P[12]);
}

inline void tremolo(const effect_request &in, rotary_params &p, effect_extras &x)
{
	const u16 *P = in.P;
	const int pm_depth = P[2], lfo_phase = P[13], stereo_input = P[14];
	set_left_right_motion(in, p, x, (pm_depth & 0xff) >> 2);
	set_lfo_start_phase(x.lfo[right_lfo], in.rom, lfo_phase);

	// モノラル入力なら 1 本の線に左右を混ぜて右の声もそれを読む。ステレオなら右は 2 本目の線
	constexpr int stereo_second_line_write = 0x400;
	set_first_line_input(p.first_line_from_left, p.first_line_from_right, stereo_input);
	const int first_write = in.stored_read_position(stored_first_line_write);
	const int second_write = stereo_input ? stereo_second_line_write : 0;
	for (int voice : {0, 1, 2})
		set_voice(p, voice, in.stored_read_position(stored_voice_read[voice]), first_write, second_write);
	set_voice(p, right_voice, (stereo_input ? second_write : first_write) + 1, first_write, second_write);
}

inline void auto_pan(const effect_request &in, rotary_params &p, effect_extras &x)
{
	const u16 *P = in.P;
	const int front_rear_depth = P[2], pan_direction = P[3];
	// 向きごとの左右の LFO の始まり。4 より大きい値は左右を急に切り替える
	constexpr int direction_phase[5][2] = {{64, 124}, {84, 64}, {64, 84}, {94, 64}, {64, 94}};
	constexpr int smooth_linear = 0x0600, smooth_cubic = -0x2000, abrupt_linear = 0x3c00;
	constexpr int front_rear_first = 3, front_rear_last = 4;	// 前後へ動かす向き
	const bool abrupt = pan_direction >= 5;
	const bool moves_front_rear = pan_direction == front_rear_first || pan_direction == front_rear_last;
	set_left_right_motion(in, p, x, moves_front_rear ? (front_rear_depth & 0xff) >> 2 : 0);
	const int *phase = direction_phase[abrupt ? 0 : pan_direction];
	set_lfo_start_phase(x.lfo[left_lfo], in.rom, phase[0]);
	set_lfo_start_phase(x.lfo[right_lfo], in.rom, phase[1]);
	for (int shaper : {0, 3}) {	// 声 0 と声 3 の波形
		p.shaper_linear[shaper] = scaled(abrupt ? abrupt_linear : smooth_linear, 16.0);
		p.shaper_cubic[shaper] = scaled(abrupt ? 0 : smooth_cubic);
	}
}

inline void ambience(const effect_request &in, rotary_params &p, effect_extras &)
{
	const u16 *P = in.P;
	const int delay_time = P[0], out_phase = P[1];
	constexpr int inverse_out_phase = 1;	// OutPhase の invers は左を逆相にする。normal は右を逆相にする
	const int first_write = in.stored_read_position(stored_first_line_write), second_write = in.stored_read_position(stored_second_line_write);
	const int read = first_write + chorus_delay_samples(in.rom, delay_time) + 1;
	set_voice(p, left_voice, read, first_write, second_write);
	set_voice(p, right_voice, read, first_write, second_write);
	p.wet_left[0] = scaled(out_phase == inverse_out_phase ? negative_full_scale : full_scale);
	p.wet_right[3] = scaled(out_phase == inverse_out_phase ? full_scale : negative_full_scale);
	set_shelves(in.rom, p, P[5], P[6], P[7], P[8]);
	set_dry_wet(in, p, P[9]);
}

} // namespace rotary

inline bool settle(const effect_request &in, rotary_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	switch (in.kind) {
	case tables::effect_rotary_speaker: rotary::rotary_speaker(in, p, x); return true;
	case tables::effect_two_way_rotary: rotary::two_way_rotary(in, p, x); return true;
	case tables::effect_tremolo: rotary::tremolo(in, p, x); return true;
	case tables::effect_auto_pan: rotary::auto_pan(in, p, x); return true;
	case tables::effect_ambience: rotary::ambience(in, p, x); return true;
	default: return false;
	}
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_ROTARY_H
