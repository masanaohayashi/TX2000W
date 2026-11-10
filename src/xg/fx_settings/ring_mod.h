// license:BSD-3-Clause
// S-MU2000: RING MOD。doc/fx-flow/ring_mod.md、parameters.md の ring_mod。
// 入力にサイン波の搬送波を掛ける。搬送波の遅れを LFO で揺らして周波数を動かす。

#ifndef S_MU2000_XG_FX_SETTINGS_RING_MOD_H
#define S_MU2000_XG_FX_SETTINGS_RING_MOD_H

namespace smu2000::fx_settings {

namespace ring_mod {

constexpr int carrier_line_delay = 0x1000;			// 搬送波の線の中心の遅れ

// FreqCourse ごとの増分の範囲。Freq Fine でこの間を動く
constexpr int carrier_lowest_increment = rom_at::ring_mod_carrier_lowest_increment;
constexpr int carrier_highest_increment = rom_at::ring_mod_carrier_highest_increment;

constexpr int carrier_lfo = 1;
constexpr int triangle_wave = 0;				// LFO Wave
constexpr int lfo_smooth_frequency = 6;			// 40 Hz の 1 次のローパス

constexpr int band_width = 7;

inline void set_carrier(const effect_request &in, ring_mod_params &p, effect_extras &x)
{
	using namespace tables::detail;
	const u16 *P = in.P;
	const int coarse = P[0], fine = P[1], lfo_wave = P[2], lfo_depth = P[3], lfo_speed = P[4];
	set_sine_carrier(in, p, x);
	p.carrier_center_delay = carrier_line_delay;
	const int lowest = read_s16(in.rom, carrier_lowest_increment + 2 * coarse);
	const int highest = read_s16(in.rom, carrier_highest_increment + 2 * coarse);
	p.carrier_increment = scaled(lowest + (highest - lowest) * fine / 127);
	u16 &lfo = x.lfo[carrier_lfo];
	set_lfo_waveform(lfo, lfo_wave == triangle_wave ? lfo_triangle : lfo_sine);	// それ以外は正弦波
	set_lfo_speed(lfo, in.rom, lfo_speed);
	p.lfo_depth = scaled(lfo_depth << 5);
	const double smooth = tables::detail::read_float(in.rom, rom_at::filter_frequency_table + 4 * lfo_smooth_frequency);
	const int smooth_input = q15_floor(smooth / (1.0 + smooth));
	p.lfo_smooth_current = scaled(smooth_input);
	p.lfo_smooth_previous = scaled(smooth_input);
	p.lfo_smooth_feedback = scaled(q15_floor((1.0 - smooth) / (1.0 + smooth)));
}

inline void set_filters(const effect_request &in, ring_mod_params &p)
{
	const u16 *P = in.P;
	const int highpass_cutoff = P[5], lowpass_cutoff = P[6], low_frequency = P[12], low_gain = P[13], high_frequency = P[14], high_gain = P[15];
	put(second_order_filter(in.rom, highpass_shape, range_4x, highpass_cutoff, flat_gain, band_width), p.band_tone_current, p.band_tone_previous, p.band_tone_feedback);
	put(second_order_filter(in.rom, steep_lowpass_shape, range_4x, lowpass_cutoff, flat_gain, band_width), p.band_current, p.band_x1, p.band_x2, p.band_y1, p.band_y2);
	set_first_order_shelves(in.rom, p, low_frequency, low_gain, high_frequency, high_gain, range_4x);
}

} // namespace ring_mod

inline bool settle(const effect_request &in, ring_mod_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	if (in.kind != tables::effect_ring_mod)
		return false;
	ring_mod::set_carrier(in, p, x);
	ring_mod::set_filters(in, p);
	const auto g = dry_wet(in, in.P[9]);
	for (int ch = 0; ch < 2; ch++) {
		p.wet_mix[ch] = scaled(g.dry, 4.0);		// 乾いた音はここを通る
		p.wet_gain[ch] = scaled(g.wet, 4.0);
	}
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_RING_MOD_H
