// license:BSD-3-Clause
// S-MU2000: V-FLANGER。doc/fx-flow/v_flanger.md。
// 内部の三角 LFO を遅れの曲線の表で変換して読み出す。表は遅延メモリに置く。

#ifndef S_MU2000_XG_FX_SETTINGS_V_FLANGER_H
#define S_MU2000_XG_FX_SETTINGS_V_FLANGER_H

namespace smu2000::fx_settings {

namespace v_flanger {

constexpr int delay_curve = rom_at::v_flanger_table, delay_curve_words = 0x860;
constexpr int phase_rate_table = rom_at::v_flanger_phase_rate_table;
constexpr int smooth_speed_table = rom_at::v_flanger_smooth_speed_table;	// サイン波のならし
constexpr int octave_fraction_curve = rom_at::v_flanger_octave_fraction_curve;		// 2 の -k/1024 乗
constexpr int percent_curve = rom_at::v_flanger_percent_curve;				// 0-100 % の大きさ
constexpr int phase_offset_table = rom_at::v_flanger_phase_offset_table;
constexpr int damp_speed_table = rom_at::v_flanger_damp_speed_table;
constexpr int allpass_base_table = rom_at::v_flanger_allpass_base_table;
constexpr int allpass_depth_table = rom_at::v_flanger_allpass_depth_table;

constexpr int dc_block_frequency = 10, dc_block_flat_gain = 60;
constexpr int feedback_center = 100;		// FB Level の 0 %
constexpr int octave_steps_per_frequency = 76;	// Random のならしは LFO Freq 1 つで 2^(76/1024) 倍（約 5 %）
constexpr double random_smooth_at_top = 412.0;	// Random で LFO Freq が最大のときのならしの速さ
constexpr int triangle_wave = 0, sine_wave = 1, random_wave = 2;	// LFO Wave。Random は揺れない
constexpr int rate_gain = 64, random_rate_gain = 512;
constexpr int curve_steps_per_value = 8;	// 表の番号と、パネルの値 1 つぶん
// 線へ書く所で符号が反転するので、濡れた音を負にして戻す。ModPhase のパネルは 0-16 なので常に負で、これ以上はパネルの外
constexpr int positive_wet_mod_phase = 100;

// LFO をならす速さ。Random は三角の目標が 0 で、m が 0 へ戻る速さになる。LFO Freq に指数で比例する。3 以上はパネルにない値で、サイン波と同じにする
inline int lfo_smooth_speed(const effect_request &in, int lfo_frequency, int lfo_wave)
{
	if (lfo_wave == triangle_wave)
		return 0x7fff;
	if (lfo_wave != random_wave)
		return u16_at(in, smooth_speed_table, lfo_frequency);
	const int steps = (127 - lfo_frequency) * octave_steps_per_frequency;
	const double fraction = steps ? double(s16_at(in, octave_fraction_curve, steps & 1023) >> (steps >> 10)) / 32768.0 : 1.0;
	const int speed = tables::detail::sx(tables::detail::tr(fraction * random_smooth_at_top));
	return speed ? speed : 1;
}

inline int line_feedback_level(const effect_request &in, int feedback_level)
{
	const int d = feedback_level - feedback_center;
	return d >= 0 ? s16_at(in, percent_curve, d) : -s16_at(in, percent_curve, -d);
}

// 入口の棚と山、直流を取る 1 次（左右同じ）
inline void set_eq(const effect_request &in, v_flanger_params &p, int low_frequency, int low_gain, int high_frequency, int high_gain, int mid_frequency, int mid_gain, int mid_width)
{
	set_first_order_shelves(in.rom, p, low_frequency, low_gain, high_frequency, high_gain, range_16x);
	set_mid_band(in.rom, p, mid_frequency, mid_gain, mid_width, range_4x);
	put(first_order_filter(in.rom, highpass_shape, range_1x, dc_block_frequency, dc_block_flat_gain), p.dc_block_current, p.dc_block_previous, p.dc_block_feedback);
}

} // namespace v_flanger

inline bool settle(const effect_request &in, v_flanger_params &p, effect_extras &x)
{
	using namespace v_flanger;
	start_settle(in, p, x);
	if (in.kind != tables::effect_v_flanger)
		return false;
	const u16 *P = in.P;
	const int lfo_frequency = P[0], lfo_depth = P[1], lfo_wave = P[2], delay_offset = P[3], feedback_level = P[4], dry_wet_value = P[9],
		mod_phase = P[13], high_damp = P[14], analog_feel = P[15];

	const int memory = in.delay_memory_start();
	copy_curve(in.rom, x, memory, delay_curve, delay_curve_words);

	p.phase_rate = scaled(u16_at(in, phase_rate_table, lfo_frequency));
	p.lfo_smooth = scaled(lfo_smooth_speed(in, lfo_frequency, lfo_wave));
	p.phase_rate_gain = scaled(lfo_wave < random_wave ? rate_gain : lfo_wave == random_wave ? random_rate_gain : 0);
	p.right_phase_offset = scaled(u16_at(in, phase_offset_table, mod_phase));

	const int half_depth = lfo_depth * curve_steps_per_value / 2;
	p.curve_read_scale = scaled(half_depth, 32768.0);
	p.curve_read_center = scaled(delay_offset * curve_steps_per_value + half_depth, 32768.0);

	p.allpass_base = scaled(s16_at(in, allpass_base_table, analog_feel));
	const float allpass_depth = scaled((s16_at(in, percent_curve, lfo_depth) * s16_at(in, allpass_depth_table, analog_feel)) >> 15);

	const float triangle = scaled(lfo_wave < random_wave ? 0x7fff : 0);
	const float feedback = scaled(line_feedback_level(in, feedback_level));
	const float damp = scaled(u16_at(in, damp_speed_table, high_damp - 1));
	for (int ch = 0; ch < 2; ch++) {
		p.curve_table_start[ch] = memory;
		p.triangle_gain[ch] = triangle;
		p.line_feedback[ch] = feedback;
		p.damp_speed[ch] = damp;
		p.allpass_from_lfo[ch] = allpass_depth;
	}

	set_eq(in, p, P[5], P[6], P[7], P[8], P[10], P[11], P[12]);

	const auto mix = dry_wet(in, dry_wet_value);
	p.dry_gain = scaled(mix.dry, 4.0);
	p.wet_gain = scaled(in.held_wet_phase < positive_wet_mod_phase ? -mix.wet : mix.wet, 4.0);
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_V_FLANGER_H
