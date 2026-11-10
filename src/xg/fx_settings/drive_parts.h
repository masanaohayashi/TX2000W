// license:BSD-3-Clause
// S-MU2000: 歪みの回路の組で同じ部品。飽和の段、アンプの種類、圧縮とゲート、ワウ、歪みのあとの遅れ。

#ifndef S_MU2000_XG_FX_SETTINGS_DRIVE_PARTS_H
#define S_MU2000_XG_FX_SETTINGS_DRIVE_PARTS_H

namespace smu2000::fx_settings {

namespace drive_parts {

constexpr int output_level_table = rom_at::drive_parts_output_level_table;
constexpr int butterworth_width = 7, mid_band_width = 10;

// フィルタ 1 段の決まり
struct filter_setting { int shape, frequency, gain, width; };
inline first_order first_order_from(const u8 *rom, int bits, const filter_setting &f) { return first_order_filter(rom, f.shape, bits, f.frequency, f.gain); }
inline second_order second_order_from(const u8 *rom, int bits, const filter_setting &f) { return second_order_filter(rom, f.shape, bits, f.frequency, f.gain, f.width); }

// --- 飽和の段 ------------------------------------------------------------
// DT と OD の音色
struct drive_voicing { int highpass_frequency, shape_frequency, post_lowpass_frequency, post_low_frequency, post_mid_frequency, clip_edge; };
constexpr drive_voicing distortion_voicing = {0, 54, 53, 20, 43, 120};
constexpr drive_voicing overdrive_voicing = {8, 44, 45, 24, 41, 104};

// Drive から、前半の段の利得と、音量をそろえる飽和のあとの利得を求める
// 前半は 2 段なので小さい音は利得の 2 乗で大きくなる。その分を飽和のあとで割り、音量を Drive によらずそろえる
struct drive_gains { int stage, post; };
inline drive_gains drive_levels(const effect_request &in, int drive, int output_level, bool to_system)
{
	using namespace tables::detail;
	constexpr int weakest_stage = 0x2C0, stage_range = 0x7FFF - weakest_stage;
	constexpr int full_compensation_drive = 10;		// これより上は補正を増やさない
	constexpr double weakest_stage_square = double(weakest_stage) * weakest_stage;
	constexpr double post_gain_at_full_level = 8192.0;	// OutputLvl が最大で 1/4 倍
	const double full_compensation = f32(21.0327);	// Drive 10 の (利得 / 一番弱い利得)²
	constexpr double square_root_of_two = 1.4142135623730949;
	const int stage = (weakest_stage + (drive * stage_range) / 127) & 0xffff;
	const double t = f32(double(s16(u16(stage))));
	const double compensation = drive > full_compensation_drive ? full_compensation : f32(f32(t * t) / weakest_stage_square);
	double post = f32(f32(read_float(in.rom, output_level_table + 4 * output_level) * post_gain_at_full_level) / compensation);
	post = f32(post * 0.5);
	if (!to_system)	// 3 dB 上げる。ワウと歪みの多くはその分を入口の左右の和で 1/√2 にしている
		post = f32(post * square_root_of_two);
	return { stage, (post != 0.0 && post < 1.0) ? 1 : int(tr(post)) };
}
inline drive_gains drive_levels(const effect_request &in, int drive, int output_level) { return drive_levels(in, drive, output_level, in.connected_to_system()); }

// Edge から後半の段の利得を求める。5 段の平均が最大になるよう、最初の段で大きさをそろえる
inline void set_edge_stages(int edge, float *stages, float *mix)
{
	using namespace tables::detail;
	constexpr int one_fifth = 0x8000 / 5;
	const int slope = (0xff - (edge & 0xff)) * 16;
	const double stage_gain = f32(double(slope) / 2048.0);
	double sum = f32(stage_gain + 1.0);
	for (int k = 0; k < 3; k++)
		sum = f32(f32(sum * stage_gain) + 1.0);
	stages[0] = scaled(int(tr(f32(32767.0 / f32(sum / 5.0)))), 16.0);
	for (int k = 1; k < 5; k++)
		stages[k] = scaled(slope, 16.0);
	for (int k = 0; k < 5; k++)
		mix[k] = scaled(one_fifth);
}

// 飽和の段: Drive が 2 段、そのあとに Edge の 5 段。stages はその先頭
inline void set_clip_stages(const drive_gains &gains, int edge, float *stages, float *mix, float &post_gain)
{
	stages[0] = stages[1] = scaled(gains.stage, 16.0);
	set_edge_stages(edge, stages + 2, mix);
	post_gain = scaled(gains.post);
}

// --- アンプの種類 ----------------------------------------------------------
// 種類ごとに 4 つのフィルター（入口、形のあと、中域、低域）を表で決める。表は形、利得、周波数の順で、低域だけ幅が続く
enum amp_filter_position { amp_entry_filter, amp_shape_filter, amp_mid_filter, amp_low_filter };
constexpr int stack_amp = 1, combo_amp = 2, tube_amp = 3;

inline filter_setting amp_voice(const effect_request &in, int amp_type, amp_filter_position filter)
{
	using tables::detail::read_u8;
	constexpr int amp_voice_table = rom_at::amp_type_table, amp_voice_size = 14, filter_size = 3;
	const int at = amp_voice_table + amp_voice_size * amp_type + filter_size * filter;
	return { int(read_u8(in.rom, at)), int(read_u8(in.rom, at + 2)), int(read_u8(in.rom, at + 1)), filter == amp_low_filter ? int(read_u8(in.rom, at + 3)) : 0 };
}
inline first_order amp_shape(const effect_request &in, int amp_type) { return first_order_from(in.rom, range_2x, amp_voice(in, amp_type, amp_shape_filter)); }
inline second_order amp_mid(const effect_request &in, int amp_type) { return second_order_from(in.rom, range_2x, amp_voice(in, amp_type, amp_mid_filter)); }
inline second_order amp_low(const effect_request &in, int amp_type) { return second_order_from(in.rom, range_16x, amp_voice(in, amp_type, amp_low_filter)); }
// Stack は箱の EQ の 2 段が +12 dB なので、飽和のあとのローパスで 8 dB 下げる
constexpr int stack_lowpass_gain = 56;
constexpr int amp_lowpass_gain(int amp_type) { return amp_type == stack_amp ? stack_lowpass_gain : flat_gain; }
// モノラルの回路の、飽和のあとのローパス
inline second_order amp_post_lowpass(const effect_request &in, int cutoff, int amp_type)
{
	constexpr int resonant_lowpass_width = 15;
	return second_order_filter(in.rom, steep_lowpass_shape, range_2x, cutoff, amp_lowpass_gain(amp_type),
		amp_type == stack_amp || amp_type == combo_amp ? resonant_lowpass_width : butterworth_width);
}

// --- 圧縮とゲート ----------------------------------------------------------
constexpr int release_feedback_table = rom_at::drive_parts_release_feedback_table;

inline int attack_feedback(const effect_request &in, int value) { return s16_at(in, rom_at::set_clip_rise_table, value); }

// ゲートの音量検出。折れ線の開いた値は 1/64 なので、ならしの直流の利得を 64 倍にして 1 にする
template<class Params>
inline void set_gate_attack(const effect_request &in, Params &p, int value)
{
	const int feedback = attack_feedback(in, value);
	p.gain_current = p.gain_previous = scaled((0x8000 - feedback) << 5);	// 2 つの和が 64 × (1 - 帰還)
	p.gain_feedback = scaled(feedback);
}
// Threshold の係数は負で、値 55 が -72 dB で 1 倍。値 1 つで 1 dB
template<class Params>
inline void set_gate_threshold(const effect_request &in, Params &p, int value)
{
	const int steps = (value & 0xff) - 56;
	p.threshold_gain = scaled(steps < 0 ? 0x8000 : -level_down(in, steps));
}
// コンプレッサーの音量検出。ならしの直流の利得は 1。Ratio は表の 1 行を折れ線の傾きにする
template<class Params>
inline void set_compressor_attack(const effect_request &in, Params &p, int value)
{
	const int feedback = attack_feedback(in, value);
	p.gain_current = p.gain_previous = scaled((0x8000 - feedback) >> 1);
	p.gain_feedback = scaled(feedback);
}
// 値 79 が -48 dB で 1 倍。値 1 つで 1 dB
template<class Params>
inline void set_compressor_threshold(const effect_request &in, Params &p, int value)
{
	const int steps = (value & 0xff) - 80;
	p.threshold_gain = scaled(steps < 0 ? 0x7FFF : level_down(in, steps));
}
template<class Params>
inline void set_ratio(const effect_request &in, Params &p, int value)
{
	static constexpr double slope_scale[8] = {128.0, 16.0, 64.0, 32.0, 8.0, 4.0, 1.0, 1.0};
	constexpr int row_size = 16;
	for (int k = 0; k < 8; k++)
		p.knee_slope[k] = scaled(tables::detail::read_u16(in.rom, rom_at::comp_row_table + row_size * value + 2 * k), slope_scale[k]);
}
inline void set_release(const effect_request &in, int value, float &peak, float &smooth, float &smooth_input)
{
	const int feedback = s16_at(in, release_feedback_table, value);
	peak = smooth = scaled(feedback);
	smooth_input = scaled(-32768 - feedback);
}
inline void set_release(const effect_request &in, distortion_params &p, int value) { set_release(in, value, p.peak_release, p.smooth_release, p.smooth_input); }
inline void set_release(const effect_request &in, distortion_delay_params &p, int value) { set_release(in, value, p.peak_release, p.smooth_feedback, p.smooth_from_peak); }

// 歪みの種類に組み込みのゲート。検出の音量が -54 dB より小さいと約 70 ms で閉じる
template<class Params>
inline void set_built_in_gate(const effect_request &in, Params &p)
{
	constexpr int fastest_attack = 0, release_about_70ms = 11, threshold_minus_54db = 73;
	set_gate_attack(in, p, fastest_attack);
	set_release(in, p, release_about_70ms);
	set_gate_threshold(in, p, threshold_minus_54db);
}
// SYSTEM 接続ではゲートを通さず 1 倍で通す。それ以外はゲートの利得を掛け、左右の和を 1/√2 にする
inline void set_gate_path(const effect_request &in, float &shape_gain, float &shape_dry)
{
	constexpr int through_level = 0x800, gated_sum_to_mono = sum_to_mono(through_level);
	const bool system = in.connected_to_system();
	shape_gain = scaled(system ? 0 : gated_sum_to_mono, 16.0);
	shape_dry = scaled(system ? through_level : 0, 16.0);
}
// 圧縮の種類: 左右の和に掛ける値。SYSTEM 接続は 1 倍と検出 10 倍、それ以外はどちらも 1/√2 にする
inline void set_compressor_path(const effect_request &in, float &shape_gain, float &detector)
{
	constexpr int sum_level = 0x2000, sum_to_mono_level = sum_to_mono(sum_level), sum_detector = 0x5000, sum_to_mono_detector = sum_to_mono(sum_detector);
	const bool system = in.connected_to_system();
	shape_gain = scaled(system ? sum_level : sum_to_mono_level, 16.0);
	detector = scaled(system ? sum_detector : sum_to_mono_detector, 16.0);
}

// --- ワウ ----------------------------------------------------------------
constexpr int wah_sensitivity_step = panel_value_step;
inline int wah_cutoff_base(int value) { return ((value * 0xC2D) >> 4) + 0x1F45; }
// 帰還は値に反比例し、-1 で止まる
inline int wah_resonance(int value) { return value <= 10 ? 0x8000 : int(tables::detail::tr(-327680.0 / double(value))); }

template<class Params>
inline void set_wah_release(const effect_request &in, Params &p, int value)
{
	constexpr int shortest_release = 52, release_steps = 15;
	const int feedback = s16_at(in, release_feedback_table, std::clamp(value - shortest_release, 0, release_steps));
	p.peak_release = p.smooth_feedback = scaled(feedback);
	p.smooth_from_peak = scaled(-2 * feedback);
}

// Dry/Wet はワウの出口と、ワウと歪みを混ぜる所の 2 か所で決まる。Drive が 0 なら歪みの道を通さない
struct wah_mix_gains { int from_eq, from_band, from_wah, from_post; };
inline wah_mix_gains wah_output_mix(const effect_request &in, int drive, int dry_wet_value)
{
	constexpr int wah_only_level = 0x4000;
	const auto [dry, wet] = crossfade(in.rom, in.connected_to_system() ? 127 : dry_wet_value);
	if (drive == 0)
		return { dry >> 2, output_gain(in, wet >> 2), wah_only_level, 0 };
	return { dry >> 2, wet >> 2, dry >> 1, output_gain(in, wet >> 1) };
}

// --- 歪みのあとの遅れ ------------------------------------------------------
inline int delay_samples(int value) { return tenths_to_samples(value) & 0xffff; }

template<class Params>
inline void set_delay_damp(const effect_request &in, Params &p)
{
	constexpr int damp_frequency = 60;
	put(first_order_filter(in.rom, lowpass_shape, range_2x, damp_frequency), p.delay_damp_current, p.delay_damp_previous, p.delay_damp_feedback);
}
template<class Params>
inline void set_delay_level(const effect_request &in, Params &p, int feedback_level, int delay_mix)
{
	using namespace tables::detail;
	p.delay_feedback = scaled(centered_feedback(feedback_level));
	p.wet_gain[0] = p.wet_gain[1] = scaled(int(tr(f32(8192.0 * read_float(in.rom, output_level_table + 4 * delay_mix)))), 4.0);
}

} // namespace drive_parts

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_DRIVE_PARTS_H
