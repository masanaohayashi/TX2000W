// license:BSD-3-Clause
// S-MU2000: ER 1、ER 2、GATE REV、REVRS GATE、KARAOKE。doc/fx-flow/parameters.md の early_reflection。
// モノラルにした入力を主の遅延線へ入れ、左右 9 本ずつのタップで反射を作る。

#ifndef S_MU2000_XG_FX_SETTINGS_EARLY_REFLECTION_H
#define S_MU2000_XG_FX_SETTINGS_EARLY_REFLECTION_H

#include "xg/fx_settings/reverb_parts.h"

namespace smu2000::fx_settings {

namespace early_reflection {

using namespace reverb_parts;

// --- 初期反射 ---------------------------------------------------------------
constexpr int gate_reverb_make = 3, reverse_gate_make = 4;
constexpr int gate_reverb_first_pattern = 6, reverse_gate_first_pattern = 8;	// 反射の並びの表で、ゲートの種類の並びが始まる行
constexpr int stored_main_line_write = 29;	// ROM の初期の値の中の位置
constexpr int reflection_pattern = rom_at::early_reflection_reflection_pattern;	// 行ごとに 18 本の (遅れ, 大きさ)
constexpr int reflection_pattern_row = 72, reflection_tap_bytes = 4, reflection_taps = 18;
constexpr int room_size_scale = rom_at::early_reflection_room_size_scale;
constexpr int liveness_shift = rom_at::early_reflection_liveness_shift;
constexpr int allpass_row_by_type = rom_at::early_reflection_allpass_row_by_type;
constexpr int allpass_row_by_type_bytes = 12;
constexpr int allpass_rows = rom_at::early_allpass_rows, section_switches_by_density = rom_at::early_section_switches;
constexpr int allpass_rows_bytes = 16, allpass_section_bytes = 4;
constexpr int karaoke_allpass_row = 3, karaoke_diffusion = 5;	// KARAOKE は Diffusion を持たないので固定
constexpr int longest_feedback_position = 0x8000;	// 帰還を取る位置の上限（メモリの半分）
constexpr int karaoke_feedback_step = 250;

// 拡散 2 段と出口 2 段のオールパス。Diffusion が 0 なら出口の右は左と同じ長さにする
inline void set_allpasses(const effect_request &in, early_reflection_params &p, int row, int density, int diffusion)
{
	using namespace tables::detail;
	const int table = allpass_rows + row * allpass_rows_bytes;
	const int active_bits = read_s16(in.rom, section_switches_by_density + 2 * density);
	allpass_section s[4];
	for (int i = 0; i < 4; i++) {
		const int at = table + i * allpass_section_bytes;
		const int length_at = i == 3 && diffusion == 0 ? at - allpass_section_bytes : at;
		s[i] = allpass_from(in.rom, length_at, at + 2, (active_bits >> i) & 1);
	}
	for (int k = 0; k < 2; k++) {
		write_allpass(s[k], p.diffuser_delay[k], p.diffuser_direct_out[k], p.diffuser_delayed_out[k], p.diffuser_input[k], p.diffuser_feedback[k]);
		write_allpass(s[k + 2], p.out_allpass_delay[k], p.out_allpass_direct_out[k], p.out_allpass_delayed_out[k], p.out_allpass_input[k], p.out_allpass_feedback[k]);
	}
}

// 入口のハイパスとローパス
inline void set_input_filters(const effect_request &in, early_reflection_params &p, int highpass_cutoff, int lowpass_cutoff)
{
	put(second_order_filter(in.rom, highpass_shape, range_2x, highpass_cutoff), p.highpass_current, p.highpass_previous, p.highpass_feedback);
	put(second_order_filter(in.rom, lowpass_shape, range_4x, lowpass_cutoff), p.lowpass_current, p.lowpass_previous, p.lowpass_feedback);
}

// 反射のタップ。左右交互に並ぶ。遅延メモリの端を超えるタップは無音
// 大きさは櫛形の残り方の表を曲線に使い、Liveness が行を -40 から +60 までずらす
inline void set_reflection_taps(const effect_request &in, early_reflection_params &p, int early_type, int room_size, int init_delay, int liveness)
{
	using namespace tables::detail;
	const int first_pattern = in.make == gate_reverb_make ? gate_reverb_first_pattern : in.make == reverse_gate_make ? reverse_gate_first_pattern : 0;
	const int pattern = reflection_pattern + reflection_pattern_row * (early_type + first_pattern);
	const int scale = read_u8(in.rom, room_size_scale + room_size);
	const int level_shift = int(read_u8(in.rom, liveness_shift + liveness)) - 128;
	const int line_write = in.stored_read_position(stored_main_line_write);
	const int start = (line_write + reverb_delay_samples(in.rom, init_delay)) & 0xffff;
	int position = start;
	for (int i = 0; i < reflection_taps; i++) {
		const int at = pattern + i * reflection_tap_bytes;
		const int level = read_s16(in.rom, at + 2);
		position = mulhi(read_u16(in.rom, at)) * scale / 10 + start;
		int gain = 0;
		if (position > 0xffff)
			position = 0xffff;
		else if (level > 0)
			gain = read_s16(in.rom, decay_curve + decay_curve_row_bytes * level_row(level + level_shift));
		else
			gain = -read_s16(in.rom, decay_curve + decay_curve_row_bytes * level_row(level_shift - level));
		p.tap_delay[i & 1][i / 2] = (position - line_write) & 0xffff;
		p.tap_gain[i & 1][i / 2] = scaled(gain / 2);
	}
	p.feedback_delay = (std::min(position, longest_feedback_position) - line_write) & 0xffff;
	if (in.make == gate_reverb_make)
		p.tap_gain[1][7] = p.tap_gain[0][8] = p.tap_gain[1][8] = scaled(0);	// ゲートの最後の 3 本は鳴らさない
}

inline void reflection(const effect_request &in, early_reflection_params &p)
{
	const u16 *P = in.P;
	const int early_type = P[0], room_size = P[1], diffusion = P[2], init_delay = P[3], feedback_level = P[4];
	const int highpass_cutoff = P[5], lowpass_cutoff = P[6], dry_wet_value = P[9], liveness = P[10], density = P[11], high_damp = P[12];
	set_reflection_taps(in, p, early_type, room_size, init_delay, liveness);
	const int allpass_row = tables::detail::read_u16(in.rom, allpass_row_by_type + in.make * allpass_row_by_type_bytes + 2 * early_type);
	set_allpasses(in, p, allpass_row, density, diffusion);
	const int cross_delay = diffusion_offset(in.rom, diffusion) + 1;
	p.cross_delay[0] = p.cross_delay[1] = cross_delay & 0xffff;
	p.line_from_feedback = scaled(centered_feedback(feedback_level));
	set_input_filters(in, p, highpass_cutoff, lowpass_cutoff);
	put(scaled_coefficients(damping_filter(in.rom, range_1x, high_damp), range_1x), p.feedback_current, p.feedback_previous, p.feedback_feedback);
	set_dry_wet(in, p, dry_wet_value, full_at_center_curve);
}

// KARAOKE: 左右の 1 本目のタップと帰還を同じ遅れにした、やまびこ
inline void karaoke(const effect_request &in, early_reflection_params &p)
{
	const u16 *P = in.P;
	const int delay_time = P[0], feedback_level = P[1], highpass_cutoff = P[2], lowpass_cutoff = P[3], dry_wet_value = P[9];
	const int stored_density = P[10];	// パネルに無い。KARAOKE 1 は拡散 1 段と出口 2 段、2 は出口 2 段、3 は全部素通し
	const int delay = long_delay_samples(in.rom, delay_time) & 0xffff;
	p.feedback_delay = p.tap_delay[0][0] = p.tap_delay[1][0] = delay;
	p.tap_gain[0][0] = p.tap_gain[1][0] = scaled(0x4000);
	p.feedback_current = p.feedback_previous = scaled(centered_feedback(feedback_level, karaoke_feedback_step));
	set_input_filters(in, p, highpass_cutoff, lowpass_cutoff);
	set_allpasses(in, p, karaoke_allpass_row, stored_density, karaoke_diffusion);
	set_dry_wet(in, p, dry_wet_value, full_at_center_curve);
}

} // namespace early_reflection

inline bool settle(const effect_request &in, early_reflection_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	switch (in.kind) {
	case tables::effect_early_reflection: early_reflection::reflection(in, p); return true;
	case tables::effect_karaoke: early_reflection::karaoke(in, p); return true;
	default: return false;
	}
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_EARLY_REFLECTION_H
