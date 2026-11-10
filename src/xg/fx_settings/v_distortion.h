// license:BSD-3-Clause
// S-MU2000: v_distortion 回路の設定。doc/fx-flow/parameters.md の v_distortion。
// 歪みの前の EQ → 硬い飽和と 2 乗の音 → 柔らかい飽和 → 箱の EQ → 遅れ。doc/fx-flow/amp_simulator.md。

#ifndef S_MU2000_XG_FX_SETTINGS_V_DISTORTION_H
#define S_MU2000_XG_FX_SETTINGS_V_DISTORTION_H

#include "xg/fx_settings/drive_parts.h"

namespace smu2000::fx_settings {

namespace v_distortion {

using namespace drive_parts;

enum device { transistor, vintage, distortion1, distortion2, fuzz };	// Device の値

// Overdrive から求める表（Device で変わる）
constexpr int transistor_overdrive_table = rom_at::v_distortion_transistor_overdrive, transistor_cabinet_level_table = rom_at::v_distortion_transistor_cabinet_level;
constexpr int vintage_fuzz_overdrive_table = rom_at::amp_sim2_drive_curve, vintage_fuzz_cabinet_level_table = rom_at::amp_sim2_low_drive_cabinet_level;
constexpr int distortion_overdrive_table = rom_at::v_distortion_overdrive, distortion_soft_and_cabinet_table = rom_at::v_distortion_soft_and_cabinet;
constexpr int output_level_table = rom_at::amp_sim2_output_level_curve;
// Device ごとのフィルタの並び。1 次（形、周波数、利得）、2 次（形、周波数、利得、幅）、1 次
constexpr int pre_filter_table = rom_at::vdist_p1_table, cabinet_device_filter_table = rom_at::vdist_p0_table;
constexpr int device_row_bytes = 10, device_row_second = 3, device_row_third = 7;
// Speaker ごとのフィルタの並び
constexpr int speaker_filter_table = rom_at::v_distortion_speaker_filter_table;
constexpr int speaker_row_bytes = 17, speaker_row_cabinet_1 = 4, speaker_row_cabinet_2 = 8, speaker_row_cabinet_3 = 11;
// Speaker と Presence で引く、できあがりの係数
constexpr int speaker_presence_second_order_table = rom_at::v_distortion_speaker_presence_second_order, speaker_presence_first_order_table = rom_at::v_distortion_speaker_presence_first_order;
constexpr int second_order_words = 5, first_order_words = 3, presence_steps = 21;

inline int byte_at(const effect_request &in, int position) { return int(tables::detail::read_u8(in.rom, position)); }

// Overdrive と OutputLvl: 飽和の利得、柔らかい飽和の前の利得、箱の前の利得
// 小さい Overdrive では箱の前を飽和の利得の逆数にし、小さい音の音量をそろえる。Distortion の 2 種は箱の前と柔らかい飽和の前の積を一定にする
inline void set_overdrive(const effect_request &in, v_distortion_params &p, int overdrive, int device_value, int output_level)
{
	constexpr int cabinet_level_floor = 0x1000;	// 大きい Overdrive は飽和で大きさが決まるので 1/8 で一定
	constexpr int half_soft_input = 0x400, unity_soft_input = 0x800, double_soft_input = 0x1000, eightfold_soft_input = 0x4000;	// 柔らかい飽和の前の利得。1 倍を超えると ±1 で頭打ちになる
	int drive_gain = 0, cabinet_level = 0, soft_input = 0;
	switch (device_value) {
	case transistor:
		drive_gain = s16_at(in, transistor_overdrive_table, overdrive);
		cabinet_level = overdrive > 6 ? cabinet_level_floor : s16_at(in, transistor_cabinet_level_table, overdrive);
		soft_input = half_soft_input;
		break;
	case vintage: case fuzz:
		drive_gain = s16_at(in, vintage_fuzz_overdrive_table, overdrive);
		cabinet_level = overdrive > 6 ? cabinet_level_floor : s16_at(in, vintage_fuzz_cabinet_level_table, overdrive);
		soft_input = overdrive == 0 ? unity_soft_input : double_soft_input;
		break;
	case distortion1: case distortion2:
		drive_gain = s16_at(in, distortion_overdrive_table, overdrive);
		cabinet_level = overdrive > 4 ? cabinet_level_floor : s16_at(in, distortion_soft_and_cabinet_table, 2 * overdrive + 1);
		soft_input = overdrive > 4 ? eightfold_soft_input : s16_at(in, distortion_soft_and_cabinet_table, 2 * overdrive);
		break;
	default:
		break;
	}
	p.overdrive = scaled(drive_gain, 16.0);
	p.soft_input_gain = scaled(soft_input, 16.0);
	p.cabinet_input_gain = scaled((cabinet_level * s16_at(in, output_level_table, output_level) + 0x4000) >> 15);
}

// Device: 2 乗の音の混ぜ具合と、歪みの前と箱のフィルタ。Vintage と Fuzz は箱の 1 段が Overdrive で持ち上がる
inline void set_device(const effect_request &in, v_distortion_params &p, int device_value, int overdrive)
{
	int hard_mix = 0x7FFF, tube_bias = 0xD00, tube_smooth = 128;	// Fuzz: ほぼ 2 乗の音だけ
	if (device_value == transistor) { hard_mix = 0; tube_bias = 0; tube_smooth = 1; }
	else if (device_value == vintage) { hard_mix = 0x2000; tube_bias = 0; }
	else if (device_value == distortion1 || device_value == distortion2) { hard_mix = 0x1000; tube_bias = 0; }
	p.hard_mix = scaled(hard_mix);
	p.tube_bias = scaled(tube_bias);
	p.tube_smooth = scaled(tube_smooth);
	const int pre = pre_filter_table + device_row_bytes * device_value, post = cabinet_device_filter_table + device_row_bytes * device_value;
	put(first_order_row(in.rom, range_2x, pre), p.pre_first_current, p.pre_first_previous, p.pre_first_feedback);
	put(second_order_row(in.rom, range_4x, pre + device_row_second), p.pre_second_current, p.pre_second_x1, p.pre_second_x2, p.pre_second_y1, p.pre_second_y2);
	put(first_order_row(in.rom, range_2x, pre + device_row_third), p.pre_third_current, p.pre_third_previous, p.pre_third_feedback);
	put(first_order_row(in.rom, range_4x, post), p.cabinet_5_current, p.cabinet_5_previous, p.cabinet_5_feedback);
	const int second = post + device_row_second, third = post + device_row_third;
	const int second_gain = device_value == vintage ? (overdrive >= 57 ? 72 : (overdrive >> 3) + flat_gain) : byte_at(in, second + 2);
	const int third_gain = device_value == fuzz ? (overdrive >= 49 ? 68 : (overdrive >> 4) + flat_gain) : byte_at(in, third + 2);
	put(second_order_filter(in.rom, byte_at(in, second), range_4x, byte_at(in, second + 1), second_gain, byte_at(in, second + 3)), p.cabinet_6_current, p.cabinet_6_x1, p.cabinet_6_x2, p.cabinet_6_y1, p.cabinet_6_y2);
	put(first_order_filter(in.rom, byte_at(in, third), range_4x, byte_at(in, third + 1), third_gain), p.cabinet_7_current, p.cabinet_7_previous, p.cabinet_7_feedback);
	p.input_from_left = p.input_from_right = scaled(0x7FFF);
	p.cabinet_output_gain = scaled(0x4000, 2.0);
}

// Speaker と Presence: 箱の EQ の段 0 から段 4
inline void set_cabinet(const effect_request &in, v_distortion_params &p, int speaker, int presence)
{
	const int row = speaker_filter_table + speaker_row_bytes * speaker;
	put(second_order_row(in.rom, range_4x, row + speaker_row_cabinet_1), p.cabinet_1_current, p.cabinet_1_x1, p.cabinet_1_x2, p.cabinet_1_y1, p.cabinet_1_y2);
	put(first_order_row(in.rom, range_2x, row + speaker_row_cabinet_2), p.cabinet_2_current, p.cabinet_2_previous, p.cabinet_2_feedback);
	put(first_order_row(in.rom, range_4x, row + speaker_row_cabinet_3), p.cabinet_3_current, p.cabinet_3_previous, p.cabinet_3_feedback);
	const int choice = presence_steps * speaker + presence;
	auto second_word = [&](int k) { return s16_at(in, speaker_presence_second_order_table, choice * second_order_words + k); };
	p.cabinet_0_current = scaled(second_word(0));
	p.cabinet_0_x1 = scaled(second_word(1), 2.0);
	p.cabinet_0_x2 = scaled(second_word(2));
	p.cabinet_0_y1 = scaled(second_word(3), 2.0);
	p.cabinet_0_y2 = scaled(second_word(4));
	auto first_word = [&](int k) { return s16_at(in, speaker_presence_first_order_table, choice * first_order_words + k); };
	p.cabinet_4_current = scaled(first_word(0), 16.0);
	p.cabinet_4_previous = scaled(first_word(1), 16.0);
	p.cabinet_4_feedback = scaled(first_word(2));
}

inline void amp_part(const effect_request &in, v_distortion_params &p)
{
	const u16 *P = in.P;
	const int overdrive = P[0], device_value = P[1], speaker = P[2], presence = P[3], output_level = P[4], dry_wet_value = P[9];
	set_overdrive(in, p, overdrive, device_value, output_level);
	set_device(in, p, device_value, overdrive);
	set_cabinet(in, p, speaker, presence);
	const auto g = dry_wet(in, dry_wet_value);
	p.mix_left_from_input = p.mix_right_from_input = scaled(g.dry, 4.0);
	p.mix_from_amp = scaled(g.wet, 4.0);
	set_delay_damp(in, p);
}

inline void delay_part(const effect_request &in, v_distortion_params &p)
{
	const u16 *P = in.P;
	const int left_delay = P[5], right_delay = P[6], feedback_delay = P[7], feedback_level = P[8], delay_mix = P[10];
	p.delay_left_time = delay_samples(left_delay);
	p.delay_right_time = delay_samples(right_delay);
	p.delay_feedback_time = delay_samples(feedback_delay);
	set_delay_level(in, p, feedback_level, delay_mix);
}

} // namespace v_distortion

inline bool settle(const effect_request &in, v_distortion_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	switch (in.kind) {
	case tables::effect_v_drive_delay_tempo: v_distortion::amp_part(in, p); return true;
	case tables::effect_v_drive_delay: v_distortion::amp_part(in, p); v_distortion::delay_part(in, p); return true;
	default: return false;
	}
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_V_DISTORTION_H
