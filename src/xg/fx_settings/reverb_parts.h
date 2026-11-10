// license:BSD-3-Clause
// S-MU2000: リバーブの回路の組で同じ部品。櫛形の減衰、オールパス、入口のフィルタ、部屋の寸法。
// システムのリバーブ、variation_reverb、early_reflection が使う。

#ifndef S_MU2000_XG_FX_SETTINGS_REVERB_PARTS_H
#define S_MU2000_XG_FX_SETTINGS_REVERB_PARTS_H

namespace smu2000::fx_settings {

namespace reverb_parts {

// 減衰の段 → {櫛形の残り方 g、櫛形の入力}。-ln g は 17 段で半分。入力は √((1 - g²) / 24) で、g によらず櫛形の音量をそろえる
constexpr int decay_curve = rom_at::fb_gain_table, decay_curve_row_bytes = 4;
constexpr int first_decay_level = 56;		// 減衰の段（残響時間の段と櫛形の長さの段の和）のうち、表の先頭の行に当たる値
constexpr int decay_steps = rom_at::reverb_parts_decay_steps;	// Reverb Time の値 → 減衰の段
constexpr int comb_table = rom_at::reverb_diffusion_rows, comb_table_stride = 24;	// 級ごとに櫛形の {取り出しの遅れ、長さ}
constexpr int comb_decay_offsets = rom_at::reverb_comb_decay_offsets, comb_decay_offsets_stride = 6;	// 級ごとに櫛形の減衰の段の差。0 は使わない櫛形

// 大きさの級。初期反射、櫛形、オールパスで別
enum size_kind { early_size, comb_size, allpass_size };
inline int size_class(const u8 *rom, int make, size_kind which) { return int(tables::detail::read_u16(rom, tables::REV_SIZE + make * 6 + 2 * int(which))); }

inline int level_row(int value) { return std::clamp(value, 0, 255); }
inline int diffusion_offset(const u8 *rom, int diffusion) { return tables::detail::read_u8(rom, rom_at::reverb_diffusion_offsets + diffusion); }
// 帰還の減衰のローパス。High Damp の値から周波数の番号を引く
inline std::array<u16, 3> damping_filter(const u8 *rom, int bits, int high_damp) { return tables::detail::filt1(rom, lowpass_shape, bits, tables::detail::read_u8(rom, rom_at::rev_time_table + high_damp), flat_gain); }

// 櫛形の残り方と入力の大きさ
struct comb_level { int decay, input; };
inline comb_level comb_level_at(const u8 *rom, int level)
{
	const int row = decay_curve + decay_curve_row_bytes * level_row(level - first_decay_level);
	return {int(tables::detail::read_u16(rom, row)), int(tables::detail::read_u16(rom, row + 2))};
}

// 減衰のローパスの通す分に残り方を掛ける
template<class Params>
inline void set_comb_damping(Params &p, int comb, const std::array<u16, 3> &damping, const comb_level &level)
{
	using namespace tables::detail;
	const int gain = sx(tr(f32(f32(double(level.decay) * double(s16(damping[0]))) / 32768.0)));
	p.comb_input[comb] = scaled(level.input);
	p.comb_damp_current[comb] = p.comb_damp_previous[comb] = scaled(gain, 2.0);
	p.comb_damp_feedback[comb] = scaled(damping[2], 2.0);
}

// --- 初期反射の表 ---------------------------------------------------------------
constexpr int early_tap_table = rom_at::system_early_tap_table;	// 級ごとに初期反射の {遅れ、左の大きさ、右の大きさ}
constexpr int early_table_stride = 36, early_tap_time = 2, early_left_gain = 12, early_right_gain = 24;

// --- 部屋の寸法で櫛形を決める種類 -----------------------------------------------
constexpr int first_room_make = 9;
constexpr int room_comb_lengths = rom_at::reverb_parts_room_comb_lengths;	// 寸法の値 → 櫛形の長さ（半分が取り出しの遅れ）
constexpr int room_decay_offsets = rom_at::reverb_parts_room_decay_offsets;	// 寸法の値 → 減衰の段の差

// 種類と寸法ごとの残響時間の倍率（10 分率）
inline int room_time_scale(const u8 *rom, int make, int dimension) { return int(tables::detail::read_u16(rom, tables::DIM_SCALE + (make - first_room_make) * 12 + 2 * dimension)); }

// 秒数に倍率を掛けた長さに一番近い、残響時間の値
inline int nearest_time(const u8 *rom, int reverb_time, int scale)
{
	using namespace tables::detail;
	const auto seconds = [&](int i) { return read_float(rom, tables::TIME_TAB + 4 * i); };
	const double scaled_time = f32(f32(double(scale) * seconds(reverb_time)) / 10.0);
	int i = 0;
	while (scaled_time > seconds(i + 1))
		i++;
	return 0.0 > f32(f32(seconds(i) + seconds(i + 1)) - f32(2.0 * scaled_time)) ? i + 1 : i;
}

// --- オールパス ----------------------------------------------------------------
// 1 段の長さと、遅延線まわりの大きさ。Density で切った段は素通し
// 帰還 f で、線へ書く値 = (1 - f) × 入力 + f × 線、出力 = -f × 入力 + (1 + f) × 線。利得 1 のオールパス
struct allpass_section { int length, direct_out, delayed_out, input, feedback; };

inline allpass_section allpass_from(const u8 *rom, int length_at, int feedback_at, bool active)
{
	using namespace tables::detail;
	int length = mulhi(read_u16(rom, length_at));
	if ((length & 0xffff) == 0)
		length = 1;
	if (!active)
		return {length, 0x3fff, 0, 0, 0};
	const int feedback = read_s16(rom, feedback_at), direct_out = -feedback / 2;
	return {length, direct_out, 0x4000 - direct_out, 0x7fff - feedback, feedback};
}

inline void write_allpass(const allpass_section &s, int32_t &delay, float &direct_out, float &delayed_out, float &input, float &feedback)
{
	delay = s.length & 0xffff;
	direct_out = scaled(s.direct_out, 2.0);
	delayed_out = scaled(s.delayed_out, 2.0);
	input = scaled(s.input);
	feedback = scaled(s.feedback);
}

// 拡散 2 段と、出口の左右 2 段ずつ。Diffusion が 0 なら右の出口は左と同じ長さにする
template<class Params>
inline void set_reverb_allpasses(const u8 *rom, Params &p, int size, int density, int diffusion)
{
	using namespace tables::detail;
	constexpr int section_count = 6, section_bytes = 4, row_bytes = 24;
	const int table = rom_at::reverb_density_rows + size * row_bytes;
	const int active_bits = read_s16(rom, rom_at::reverb_density_active_bits + 2 * density);
	allpass_section s[section_count];
	for (int i = 0; i < section_count; i++) {
		const int at = table + i * section_bytes;
		s[i] = allpass_from(rom, (i >= 4 && diffusion == 0) ? at - 2 * section_bytes : at, at + 2, (active_bits >> i) & 1);
	}
	for (int ch = 0; ch < 2; ch++) {	// 出口の 1 段目は入力と素通しを 2 倍にし、オールパスの形のまま音量を 2 倍にする
		s[2 + 2 * ch].direct_out <<= 1;
		s[2 + 2 * ch].input <<= 1;
	}
	for (int k = 0; k < 2; k++)
		write_allpass(s[k], p.diffuser_delay[k], p.diffuser_direct_out[k], p.diffuser_delayed_out[k], p.diffuser_input[k], p.diffuser_feedback[k]);
	for (int ch = 0; ch < 2; ch++)
		for (int k = 0; k < 2; k++)
			write_allpass(s[2 + 2 * ch + k], p.out_allpass_delay[ch][k], p.out_allpass_direct_out[ch][k], p.out_allpass_delayed_out[ch][k], p.out_allpass_input[ch][k], p.out_allpass_feedback[ch][k]);
}

// --- 入口 ----------------------------------------------------------------------
template<class Params>
inline void set_reverb_input_filters(const u8 *rom, Params &p, int highpass_cutoff, int lowpass_cutoff)
{
	put(first_order_filter(rom, highpass_shape, range_1x, highpass_cutoff), p.highpass_current, p.highpass_previous, p.highpass_feedback);
	put(first_order_filter(rom, lowpass_shape, range_2x, lowpass_cutoff), p.lowpass_current, p.lowpass_previous, p.lowpass_feedback);
}

// 拡散の入口の棚。種類ごとの {周波数、利得}。利得 0 なら棚が無いので何も書かずに false を返す
template<class Params>
inline bool set_diffuse_shelf(const u8 *rom, Params &p, int make)
{
	using tables::detail::read_u8;
	const int entry = rom_at::rev_all_tail_table + 2 * make;
	const int gain = read_u8(rom, entry + 1);
	if (!gain)
		return false;
	put(first_order_filter(rom, low_shelf_shape, range_4x, read_u8(rom, entry), gain), p.diffuse_filter_current, p.diffuse_filter_previous, p.diffuse_filter_feedback);
	return true;
}

} // namespace reverb_parts

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_REVERB_PARTS_H
