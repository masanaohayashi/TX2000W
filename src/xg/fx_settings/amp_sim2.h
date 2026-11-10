// license:BSD-3-Clause
// S-MU2000: amp_sim2 回路の設定。doc/fx-flow/parameters.md の amp_sim2。
// モノラルにした入力を、硬い飽和、2 乗の音、柔らかい飽和、箱の EQ に通す。

#ifndef S_MU2000_XG_FX_SETTINGS_AMP_SIM2_H
#define S_MU2000_XG_FX_SETTINGS_AMP_SIM2_H

namespace smu2000::fx_settings {

namespace amp_sim2 {

constexpr int soft_clip_curve = rom_at::amp_sim2_table, soft_clip_curve_words = 4096 + 1;	// 柔らかい飽和の曲線（遅延メモリに置く）
constexpr int drive_curve = rom_at::amp_sim2_drive_curve;		// Drive → 硬い飽和の利得
constexpr int low_drive_cabinet_levels = rom_at::amp_sim2_low_drive_cabinet_level;	// Drive が小さい時の箱の前の利得。飽和の利得の逆数で小さい音の音量をそろえる
constexpr int last_low_drive = 6, cabinet_level_floor = 0x1000;	// それより上は飽和で大きさが決まるので 1/8 で一定
constexpr int output_level_curve = rom_at::amp_sim2_output_level_curve;	// OutputLvl → 箱の前の利得
constexpr int amp_type_table = rom_at::amp_sim2_param2_table, amp_type_row_size = 18;	// AmpType → 歪みの前 2 段と箱の 3 段の形
constexpr int zero_drive_soft_gain = 0x800, soft_gain = 0x1000;	// Drive が 0 の時は柔らかい飽和の前を半分にする
constexpr int system_cabinet_output = 0x4000, cabinet_output = q15_floor(0.354);	// SYSTEM 接続のバリエーションは箱の出口を 3 dB 上げる
constexpr int stack_amp = 1, british_amp = 6;
constexpr int stack_lowpass_gain = 56, british_lowpass_gain = 67;	// 箱の EQ で持ち上げる Stack は -8 dB、下げる British は +3 dB
constexpr int input_half = 0x4000, hard_mix_value = 0x2000, tube_smooth_value = 128;

// 0-127 の値を表の 0-100 の番号へ
inline int percent_of(int value) { return int(tables::detail::fdiv((long long)value * 100, 127)); }

inline void set_drive(const effect_request &in, amp_sim2_params &p, int drive, int output_level)
{
	using namespace tables::detail;
	const int drive_level = drive <= last_low_drive ? read_s16(in.rom, low_drive_cabinet_levels + 2 * drive) : cabinet_level_floor;
	const int level = read_s16(in.rom, output_level_curve + 2 * percent_of(output_level));
	p.overdrive = scaled(read_s16(in.rom, drive_curve + 2 * percent_of(drive)), 16.0);
	p.soft_input_gain = scaled(drive == 0 ? zero_drive_soft_gain : soft_gain, 16.0);
	p.cabinet_input_gain = scaled(sx((drive_level * level + 0x4000) >> 15) >> 5);
}

inline void set_amp_type(const effect_request &in, amp_sim2_params &p, int amp_type)
{
	using namespace tables::detail;
	const int row = amp_type_table + amp_type_row_size * amp_type;
	const auto entry = [&](int k) { return int(read_u8(in.rom, row + k)); };
	// 1 段目は 2 次の丸めで求めて、1 次の係数だけを使う
	put(second_order_filter(in.rom, entry(0), range_4x, entry(2), entry(1)), p.pre_first_current, p.pre_first_previous, p.pre_first_feedback);
	put(first_order_filter(in.rom, entry(3), range_2x, entry(5), entry(4)), p.pre_second_current, p.pre_second_previous, p.pre_second_feedback);
	const auto cabinet = [&](int first, int bits) { return second_order_filter(in.rom, entry(first), bits, entry(first + 2), entry(first + 1), entry(first + 3)); };
	put(cabinet(6, range_2x), p.cabinet_1_current, p.cabinet_1_x1, p.cabinet_1_x2, p.cabinet_1_y1, p.cabinet_1_y2);
	put(cabinet(10, range_4x), p.cabinet_2_current, p.cabinet_2_x1, p.cabinet_2_x2, p.cabinet_2_y1, p.cabinet_2_y2);
	put(cabinet(14, range_16x), p.cabinet_3_current, p.cabinet_3_x1, p.cabinet_3_x2, p.cabinet_3_y1, p.cabinet_3_y2);
}

// LPF Cutoff は 1 次のローパス
inline void set_cutoff(const effect_request &in, amp_sim2_params &p, int cutoff, int amp_type)
{
	const int gain = amp_type == stack_amp ? stack_lowpass_gain : amp_type == british_amp ? british_lowpass_gain : flat_gain;
	put(first_order_filter(in.rom, lowpass_shape, range_4x, cutoff, gain), p.cabinet_0_current, p.cabinet_0_previous, p.cabinet_0_feedback);
}

} // namespace amp_sim2

inline bool settle(const effect_request &in, amp_sim2_params &p, effect_extras &x)
{
	using namespace amp_sim2;
	start_settle(in, p, x);
	if (in.kind != tables::effect_amp_sim2)
		return false;
	const u16 *P = in.P;
	const int drive = P[0], amp_type = P[1], cutoff = P[2], output_level = P[3];
	copy_curve(in.rom, x, in.delay_memory_start(), soft_clip_curve, soft_clip_curve_words);
	p.input_from_left = p.input_from_right = scaled(input_half);
	p.hard_mix = scaled(hard_mix_value);
	p.tube_smooth = scaled(tube_smooth_value);
	set_drive(in, p, drive, output_level);
	set_amp_type(in, p, amp_type);
	set_cutoff(in, p, cutoff, amp_type);
	p.cabinet_output_gain = scaled(in.connected_to_system() ? system_cabinet_output : cabinet_output, 16.0);
	const auto g = dry_wet(in, P[9]);
	p.dry_gain[0] = p.dry_gain[1] = scaled(g.dry, 4.0);
	p.wet_gain = scaled(g.wet, 4.0);
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_AMP_SIM2_H
