// license:BSD-3-Clause
#ifndef S_MU2000_NATIVE_PANEL_FX_TABLES_H
#define S_MU2000_NATIVE_PANEL_FX_TABLES_H

#include "compat/types.h"
#include "xg/voices.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace smu2000::panel_fx {

namespace rom_at = ::xg::rom_position;

// 値の文字の作り方
enum text_kind : u8 {
	text_rom,            // ROM の一覧
	text_rom_hertz,      // ROM の一覧 + 単位
	text_pan_direction,  // ROM の一覧。「<」の出し方だけ違う
	text_balance,        // ROM の 3 語（同じ、右寄り、左寄り）。寄りの語の ** に量
	text_number,         // pattern に (値 - zero) × step
	text_sign,           // pattern に符号と大きさ
	text_half_steps,     // pattern に符号と (値 - zero) × 22.5 の四捨五入
	text_letter,         // A から
	text_tenths,         // pattern に値 ÷ 10 の整数部と小数部
	text_tenths_free,    // 同じ。範囲に丸めない
};
struct value_text { text_kind kind; short zero, step; const char *pattern; ::xg::rom_list list; };
constexpr value_text rom_values(::xg::rom_list list, int zero = 0) { return { text_rom, short(zero), 1, nullptr, list }; }

struct param
{
	short address;        // パラメーターの番号 1-16
	u8 label;             // ラベルの一覧の中の番号
	short lowest, highest;
	bool wide;            // 2 バイトで送る
	const value_text &text;
	::xg::rom_text field() const { return rom_at::LABELS_FX[label]; }
};
struct type_table { int type; const param *params; int count; };
struct type_row { u8 msb, lsb; const param *params; int count; };
enum table_id { table_reverb, table_chorus, table_variation, table_insertion };

inline constexpr value_text reverb_time = rom_values(rom_at::FX_REVERB_TIME);
inline constexpr value_text two_digits = { text_number, 0, 1, "%02d", {} };
inline constexpr value_text initial_delay = rom_values(rom_at::FX_INITIAL_DELAY);
inline constexpr value_text frequency = rom_values(rom_at::FX_FREQUENCY);
inline constexpr value_text digit = { text_number, 0, 1, "%d", {} };
inline constexpr value_text early_reverb = { text_balance, 64, 1, nullptr, rom_at::FX_EARLY_REVERB };
inline constexpr value_text tenths_narrow = { text_tenths, 0, 1, "%d.%d", {} };
inline constexpr value_text centered = { text_number, 64, 1, "%+03d", {} };
inline constexpr value_text space_size = rom_values(rom_at::FX_SPACE_SIZE);
inline constexpr value_text lfo_hertz = { text_rom_hertz, 0, 1, nullptr, rom_at::FX_LFO_FREQUENCY };
inline constexpr value_text three_digits = { text_number, 0, 1, "%03d", {} };
inline constexpr value_text modulation_delay = rom_values(rom_at::FX_MODULATION_DELAY);
inline constexpr value_text input_mode = rom_values(rom_at::FX_INPUT_MODE);
inline constexpr value_text phase_degrees = { text_number, 64, 3, "%+04d", {} };
inline constexpr value_text centered_spaced = { text_number, 64, 1, " %+03d", {} };
inline constexpr value_text dry_wet = { text_balance, 64, 1, nullptr, rom_at::FX_DRY_WET };
inline constexpr value_text delay_tenths = { text_tenths_free, 0, 1, "%3d.%d", {} };
inline constexpr value_text input_select = rom_values(rom_at::FX_INPUT_SELECT);
inline constexpr value_text early_type = rom_values(rom_at::FX_EARLY_TYPE);
inline constexpr value_text room_size = rom_values(rom_at::FX_ROOM_SIZE);
inline constexpr value_text gate_type = rom_values(rom_at::FX_GATE_TYPE);
inline constexpr value_text long_delay = rom_values(rom_at::FX_LONG_DELAY);
inline constexpr value_text tempo_note = rom_values(rom_at::FX_TEMPO_NOTE);
inline constexpr value_text tenths = { text_tenths, 0, 1, "%2d.%d", {} };
inline constexpr value_text amp_type = rom_values(rom_at::FX_AMP_TYPE);
inline constexpr value_text pan_direction = { text_pan_direction, 0, 1, nullptr, rom_at::FX_PAN_DIRECTION };
inline constexpr value_text two_digits_from_lowest = { text_number, 4, 1, "%02d", {} };
inline constexpr value_text compressor_attack = rom_values(rom_at::FX_COMPRESSOR_ATTACK);
inline constexpr value_text compressor_release = rom_values(rom_at::FX_COMPRESSOR_RELEASE);
inline constexpr value_text below_full_scale = { text_number, 127, 1, "%03d", {} };
inline constexpr value_text compressor_ratio = rom_values(rom_at::FX_COMPRESSOR_RATIO);
inline constexpr value_text pan = rom_values(rom_at::FX_PAN);
inline constexpr value_text centered_low = { text_number, 12, 1, "%+03d", {} };
inline constexpr value_text frequency_from_value_34 = rom_values(rom_at::FX_FREQUENCY, -34);
inline constexpr value_text compressor_release_from_lowest_52 = rom_values(rom_at::FX_COMPRESSOR_RELEASE, 52);
inline constexpr value_text low_high = { text_balance, 64, 1, nullptr, rom_at::FX_LOW_HIGH };
inline constexpr value_text three_digits_by_three = { text_number, 0, 3, "%03d", {} };
inline constexpr value_text output_phase = rom_values(rom_at::FX_OUTPUT_PHASE);
inline constexpr value_text vowel = rom_values(rom_at::FX_VOWEL);
inline constexpr value_text sample_frequency = rom_values(rom_at::FX_SAMPLE_FREQUENCY);
inline constexpr value_text output_gain = { text_number, 6, 1, "%+03d", {} };
inline constexpr value_text filter_character = rom_values(rom_at::FX_FILTER_CHARACTER);
inline constexpr value_text on_off = rom_values(rom_at::FX_ON_OFF);
inline constexpr value_text percent = { text_number, 0, 1, "%3d%%", {} };
inline constexpr value_text device = rom_values(rom_at::FX_DEVICE);
inline constexpr value_text speaker = rom_values(rom_at::FX_SPEAKER);
inline constexpr value_text speed_control = rom_values(rom_at::FX_SPEED_CONTROL);
inline constexpr value_text lfo_wave = rom_values(rom_at::FX_LFO_WAVE);
inline constexpr value_text delay_offset = rom_values(rom_at::FX_DELAY_OFFSET);
inline constexpr value_text signed_percent = { text_sign, 100, 1, "%c%3d%%", {} };
inline constexpr value_text half_step_degrees = { text_half_steps, 8, 1, "%c%3d", {} };
inline constexpr value_text lofi_type = rom_values(rom_at::FX_LOFI_TYPE);
inline constexpr value_text reset = rom_values(rom_at::FX_RESET);
inline constexpr value_text filter_type = rom_values(rom_at::FX_FILTER_TYPE);
inline constexpr value_text resonance = { text_sign, 16, 1, "%c%3d", {} };
inline constexpr value_text dynamics_attack = rom_values(rom_at::FX_DYNAMICS_ATTACK);
inline constexpr value_text dynamics_release = rom_values(rom_at::FX_DYNAMICS_RELEASE);
inline constexpr value_text direction = rom_values(rom_at::FX_DIRECTION);
inline constexpr value_text coarse_frequency = rom_values(rom_at::FX_COARSE_FREQUENCY);
inline constexpr value_text input_mode_from_value_52 = rom_values(rom_at::FX_INPUT_MODE, -52);
inline constexpr value_text letter = { text_letter, 0, 1, "%c", {} };
inline constexpr value_text resolution = rom_values(rom_at::FX_RESOLUTION);
inline constexpr value_text phase_invert = rom_values(rom_at::FX_PHASE_INVERT);
inline constexpr value_text lfo_frequency = rom_values(rom_at::FX_LFO_FREQUENCY);

inline constexpr param NO_PARAMS[] = { { 0, 0, 0, 0, false, digit } };

inline constexpr param REV_HALL[] = {
	{ 1, 0, 0, 69, false, reverb_time }, { 2, 1, 0, 10, false, two_digits }, { 3, 2, 0, 63, false, initial_delay }, { 4, 3, 0, 52, false, frequency },
	{ 5, 4, 34, 60, false, frequency }, { 11, 80, 0, 63, false, initial_delay }, { 12, 81, 0, 4, false, digit }, { 13, 82, 1, 127, false, early_reverb },
	{ 14, 17, 1, 10, false, tenths_narrow }, { 15, 9, 1, 127, false, centered },
};
inline constexpr param REV_WHITE_ROOM[] = {
	{ 1, 0, 0, 69, false, reverb_time }, { 2, 1, 0, 10, false, two_digits }, { 3, 2, 0, 63, false, initial_delay }, { 4, 3, 0, 52, false, frequency },
	{ 5, 4, 34, 60, false, frequency }, { 6, 83, 0, 37, false, space_size }, { 7, 84, 0, 73, false, space_size }, { 8, 85, 0, 104, false, space_size },
	{ 9, 86, 0, 30, false, two_digits }, { 11, 80, 0, 63, false, initial_delay }, { 12, 81, 0, 4, false, digit }, { 13, 82, 1, 127, false, early_reverb },
	{ 14, 17, 1, 10, false, tenths_narrow }, { 15, 9, 1, 127, false, centered },
};
inline constexpr param CHO_CHORUS[] = {
	{ 1, 25, 0, 127, false, lfo_hertz }, { 2, 26, 0, 127, false, three_digits }, { 3, 79, 1, 127, false, centered },
	{ 4, 27, 0, 127, false, modulation_delay }, { 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered },
	{ 8, 88, 28, 58, false, frequency }, { 9, 89, 52, 76, false, centered }, { 15, 94, 0, 1, false, input_mode },
};
inline constexpr param CHO_FLANGER[] = {
	{ 1, 25, 0, 127, false, lfo_hertz }, { 2, 26, 0, 127, false, three_digits }, { 3, 79, 1, 127, false, centered },
	{ 4, 27, 0, 127, false, modulation_delay }, { 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered },
	{ 8, 88, 28, 58, false, frequency }, { 9, 89, 52, 76, false, centered }, { 13, 95, 4, 124, false, phase_degrees },
};
inline constexpr param CHO_SYMPHONIC[] = {
	{ 1, 25, 0, 127, false, lfo_hertz }, { 2, 26, 0, 127, false, three_digits }, { 3, 27, 0, 127, false, modulation_delay },
	{ 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency }, { 9, 89, 52, 76, false, centered },
};
inline constexpr param CHO_PHASER[] = {
	{ 1, 25, 0, 127, false, lfo_hertz }, { 2, 26, 0, 127, false, three_digits }, { 3, 38, 0, 127, false, three_digits },
	{ 4, 79, 1, 127, false, centered }, { 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency },
	{ 9, 89, 52, 76, false, centered }, { 10, 98, 4, 6, false, digit }, { 11, 113, 0, 1, false, input_mode },
};
inline constexpr param CHO_ENS_DETUNE[] = {
	{ 1, 28, 14, 114, false, centered_spaced }, { 2, 29, 0, 127, false, modulation_delay }, { 3, 30, 0, 127, false, modulation_delay },
};
inline constexpr param VAR_HALL[] = {
	{ 1, 0, 0, 69, false, reverb_time }, { 2, 1, 0, 10, false, two_digits }, { 3, 2, 0, 63, false, initial_delay }, { 4, 3, 0, 52, false, frequency },
	{ 5, 4, 34, 60, false, frequency }, { 10, 10, 1, 127, false, dry_wet }, { 11, 80, 0, 63, false, initial_delay }, { 12, 81, 0, 4, false, digit },
	{ 13, 82, 1, 127, false, early_reverb }, { 14, 17, 1, 10, false, tenths_narrow }, { 15, 9, 1, 127, false, centered },
};
inline constexpr param VAR_DELAY_LCR[] = {
	{ 1, 5, 1, 14860, true, delay_tenths }, { 2, 6, 1, 14860, true, delay_tenths }, { 3, 7, 1, 14860, true, delay_tenths },
	{ 4, 8, 1, 14860, true, delay_tenths }, { 5, 9, 1, 127, true, centered }, { 6, 87, 0, 127, true, three_digits },
	{ 7, 17, 1, 10, true, tenths_narrow }, { 10, 10, 1, 127, true, dry_wet }, { 13, 40, 4, 40, false, frequency }, { 14, 41, 52, 76, false, centered },
	{ 15, 88, 28, 58, false, frequency }, { 16, 89, 52, 76, false, centered },
};
inline constexpr param VAR_DELAY_L_R[] = {
	{ 1, 5, 1, 14860, true, delay_tenths }, { 2, 6, 1, 14860, true, delay_tenths }, { 3, 11, 1, 14860, true, delay_tenths },
	{ 4, 12, 1, 14860, true, delay_tenths }, { 5, 9, 1, 127, true, centered }, { 6, 17, 1, 10, true, tenths_narrow }, { 10, 10, 1, 127, true, dry_wet },
	{ 13, 40, 4, 40, false, frequency }, { 14, 41, 52, 76, false, centered }, { 15, 88, 28, 58, false, frequency }, { 16, 89, 52, 76, false, centered },
};
inline constexpr param VAR_ECHO[] = {
	{ 1, 13, 1, 7430, true, delay_tenths }, { 2, 14, 1, 127, true, centered }, { 3, 15, 1, 7430, true, delay_tenths }, { 4, 16, 1, 127, true, centered },
	{ 5, 17, 1, 10, true, tenths_narrow }, { 6, 90, 1, 7430, true, delay_tenths }, { 7, 91, 1, 7430, true, delay_tenths },
	{ 8, 92, 0, 127, true, three_digits }, { 10, 10, 1, 127, true, dry_wet }, { 13, 40, 4, 40, false, frequency }, { 14, 41, 52, 76, false, centered },
	{ 15, 88, 28, 58, false, frequency }, { 16, 89, 52, 76, false, centered },
};
inline constexpr param VAR_CROSSDELAY[] = {
	{ 1, 18, 1, 7430, true, delay_tenths }, { 2, 19, 1, 7430, true, delay_tenths }, { 3, 107, 1, 127, true, centered },
	{ 4, 20, 0, 2, true, input_select }, { 5, 17, 1, 10, true, tenths_narrow }, { 10, 10, 1, 127, true, dry_wet }, { 13, 40, 4, 40, false, frequency },
	{ 14, 41, 52, 76, false, centered }, { 15, 88, 28, 58, false, frequency }, { 16, 89, 52, 76, false, centered },
};
inline constexpr param VAR_EARLY_REFLECTION[] = {
	{ 1, 21, 0, 5, false, early_type }, { 2, 22, 0, 44, false, room_size }, { 3, 1, 0, 10, false, two_digits }, { 4, 2, 0, 127, false, initial_delay },
	{ 5, 9, 1, 127, false, centered }, { 6, 3, 0, 52, false, frequency }, { 7, 4, 34, 60, false, frequency }, { 10, 10, 1, 127, false, dry_wet },
	{ 11, 93, 0, 10, false, two_digits }, { 12, 81, 0, 3, false, digit }, { 13, 17, 1, 10, false, tenths_narrow },
};
inline constexpr param VAR_GATE_REV[] = {
	{ 1, 23, 0, 1, false, gate_type }, { 2, 22, 0, 44, false, room_size }, { 3, 1, 0, 10, false, two_digits }, { 4, 2, 0, 127, false, initial_delay },
	{ 5, 9, 1, 127, false, centered }, { 6, 3, 0, 52, false, frequency }, { 7, 4, 34, 60, false, frequency }, { 10, 10, 1, 127, false, dry_wet },
	{ 11, 93, 0, 10, false, two_digits }, { 12, 81, 0, 3, false, digit }, { 13, 17, 1, 10, false, tenths_narrow },
};
inline constexpr param VAR_WHITE_ROOM[] = {
	{ 1, 0, 0, 69, false, reverb_time }, { 2, 1, 0, 10, false, two_digits }, { 3, 2, 0, 63, false, initial_delay }, { 4, 3, 0, 52, false, frequency },
	{ 5, 4, 34, 60, false, frequency }, { 6, 83, 0, 37, false, space_size }, { 7, 84, 0, 73, false, space_size }, { 8, 85, 0, 104, false, space_size },
	{ 9, 86, 0, 30, false, two_digits }, { 10, 10, 1, 127, false, dry_wet }, { 11, 80, 0, 63, false, initial_delay }, { 12, 81, 0, 4, false, digit },
	{ 13, 82, 1, 127, false, early_reverb }, { 14, 17, 1, 10, false, tenths_narrow }, { 15, 9, 1, 127, false, centered },
};
inline constexpr param VAR_KARAOKE[] = {
	{ 1, 24, 0, 127, false, long_delay }, { 2, 9, 1, 127, false, centered }, { 3, 3, 0, 52, false, frequency }, { 4, 4, 34, 60, false, frequency },
	{ 10, 10, 1, 127, false, dry_wet },
};
inline constexpr param VAR_T_DELAY[] = {
	{ 1, 71, 0, 19, false, tempo_note }, { 2, 79, 1, 127, false, centered }, { 3, 17, 0, 10, false, tenths_narrow }, { 4, 126, 1, 127, false, centered },
	{ 5, 127, 1, 127, false, centered }, { 10, 10, 1, 127, false, dry_wet }, { 13, 40, 4, 40, false, frequency }, { 14, 41, 52, 76, false, centered },
	{ 15, 88, 28, 58, false, frequency }, { 16, 89, 52, 76, false, centered },
};
inline constexpr param VAR_T_CRS_DLY[] = {
	{ 1, 128, 0, 19, false, tempo_note }, { 2, 129, 0, 19, false, tempo_note }, { 3, 79, 1, 127, false, centered }, { 4, 20, 0, 2, false, input_select },
	{ 5, 17, 0, 10, false, tenths_narrow }, { 6, 127, 1, 127, false, centered }, { 10, 10, 1, 127, false, dry_wet }, { 13, 40, 4, 40, false, frequency },
	{ 14, 41, 52, 76, false, centered }, { 15, 88, 28, 58, false, frequency }, { 16, 89, 52, 76, false, centered },
};
inline constexpr param VAR_CHORUS[] = {
	{ 1, 25, 0, 127, false, lfo_hertz }, { 2, 26, 0, 127, false, three_digits }, { 3, 79, 1, 127, false, centered },
	{ 4, 27, 0, 127, false, modulation_delay }, { 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered },
	{ 8, 88, 28, 58, false, frequency }, { 9, 89, 52, 76, false, centered }, { 10, 10, 1, 127, false, dry_wet }, { 11, 108, 14, 54, false, frequency },
	{ 12, 109, 52, 76, false, centered }, { 13, 110, 10, 120, false, tenths }, { 15, 94, 0, 1, false, input_mode },
};
inline constexpr param VAR_FLANGER[] = {
	{ 1, 25, 0, 127, false, lfo_hertz }, { 2, 26, 0, 127, false, three_digits }, { 3, 79, 1, 127, false, centered },
	{ 4, 27, 0, 127, false, modulation_delay }, { 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered },
	{ 8, 88, 28, 58, false, frequency }, { 9, 89, 52, 76, false, centered }, { 10, 10, 1, 127, false, dry_wet }, { 11, 108, 14, 54, false, frequency },
	{ 12, 109, 52, 76, false, centered }, { 13, 110, 10, 120, false, tenths }, { 14, 95, 4, 124, false, phase_degrees },
};
inline constexpr param VAR_SYMPHONIC[] = {
	{ 1, 25, 0, 127, false, lfo_hertz }, { 2, 26, 0, 127, false, three_digits }, { 3, 27, 0, 127, false, modulation_delay },
	{ 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency }, { 9, 89, 52, 76, false, centered },
	{ 10, 10, 1, 127, false, dry_wet }, { 11, 108, 14, 54, false, frequency }, { 12, 109, 52, 76, false, centered }, { 13, 110, 10, 120, false, tenths },
};
inline constexpr param VAR_ROTARY_SP[] = {
	{ 1, 25, 0, 127, false, lfo_hertz }, { 2, 26, 0, 127, false, three_digits }, { 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered },
	{ 8, 88, 28, 58, false, frequency }, { 9, 89, 52, 76, false, centered }, { 10, 10, 1, 127, false, dry_wet }, { 11, 108, 14, 54, false, frequency },
	{ 12, 109, 52, 76, false, centered }, { 13, 110, 10, 120, false, tenths },
};
inline constexpr param VAR_DT_RTRY[] = {
	{ 1, 25, 0, 127, false, lfo_hertz }, { 2, 26, 0, 127, false, three_digits }, { 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered },
	{ 8, 88, 28, 58, false, frequency }, { 9, 89, 52, 76, false, centered }, { 10, 10, 1, 127, false, dry_wet }, { 14, 39, 0, 127, false, three_digits },
	{ 15, 4, 34, 60, false, frequency }, { 16, 61, 0, 127, false, three_digits },
};
inline constexpr param VAR_AMP_RTRY[] = {
	{ 1, 25, 0, 127, false, lfo_hertz }, { 2, 26, 0, 127, false, three_digits }, { 3, 43, 0, 3, false, amp_type }, { 6, 40, 4, 40, false, frequency },
	{ 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency }, { 9, 89, 52, 76, false, centered }, { 10, 10, 1, 127, false, dry_wet },
	{ 14, 39, 0, 127, false, three_digits }, { 15, 4, 34, 60, false, frequency }, { 16, 61, 0, 127, false, three_digits },
};
inline constexpr param VAR_TREMOLO[] = {
	{ 1, 25, 0, 127, false, lfo_hertz }, { 2, 33, 0, 127, false, three_digits }, { 3, 34, 0, 127, false, three_digits },
	{ 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency }, { 9, 89, 52, 76, false, centered },
	{ 11, 108, 14, 54, false, frequency }, { 12, 109, 52, 76, false, centered }, { 13, 110, 10, 120, false, tenths },
	{ 14, 95, 4, 124, false, phase_degrees }, { 15, 94, 0, 1, false, input_mode },
};
inline constexpr param VAR_AUTO_PAN[] = {
	{ 1, 25, 0, 127, false, lfo_hertz }, { 2, 35, 0, 127, false, three_digits }, { 3, 36, 0, 127, false, three_digits },
	{ 4, 37, 0, 5, false, pan_direction }, { 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency },
	{ 9, 89, 52, 76, false, centered }, { 11, 108, 14, 54, false, frequency }, { 12, 109, 52, 76, false, centered }, { 13, 110, 10, 120, false, tenths },
};
inline constexpr param VAR_AUTO_PAN_WAVE[] = {
	{ 1, 25, 0, 127, false, lfo_hertz }, { 2, 35, 0, 127, false, three_digits }, { 3, 36, 0, 127, false, three_digits },
	{ 4, 37, 0, 5, false, pan_direction }, { 6, 130, 4, 40, false, two_digits_from_lowest }, { 6, 40, 4, 40, false, frequency },
	{ 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency }, { 9, 89, 52, 76, false, centered }, { 11, 108, 14, 54, false, frequency },
	{ 12, 109, 52, 76, false, centered }, { 13, 110, 10, 120, false, tenths }, { 13, 94, 0, 127, false, input_mode },
};
inline constexpr param VAR_PHASER[] = {
	{ 1, 25, 0, 127, false, lfo_hertz }, { 2, 26, 0, 127, false, three_digits }, { 3, 38, 0, 127, false, three_digits },
	{ 4, 107, 1, 127, false, centered }, { 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency },
	{ 9, 89, 52, 76, false, centered }, { 10, 10, 1, 127, false, dry_wet }, { 11, 98, 4, 12, false, two_digits }, { 12, 113, 0, 1, false, input_mode },
};
inline constexpr param VAR_PHASER_DEEP[] = {
	{ 1, 25, 0, 127, false, lfo_hertz }, { 2, 26, 0, 127, false, three_digits }, { 3, 38, 0, 127, false, three_digits },
	{ 4, 107, 1, 127, false, centered }, { 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency },
	{ 9, 89, 52, 76, false, centered }, { 10, 10, 1, 127, false, dry_wet }, { 11, 98, 3, 6, false, two_digits }, { 13, 95, 4, 124, false, phase_degrees },
};
inline constexpr param VAR_DISTORTION[] = {
	{ 1, 39, 0, 127, false, three_digits }, { 2, 40, 4, 40, false, frequency }, { 3, 41, 52, 76, false, centered }, { 4, 4, 34, 60, false, frequency },
	{ 5, 61, 0, 127, false, three_digits }, { 7, 108, 14, 54, false, frequency }, { 8, 109, 52, 76, false, centered }, { 9, 110, 10, 120, false, tenths },
	{ 10, 10, 1, 127, false, dry_wet }, { 11, 99, 0, 127, false, three_digits },
};
inline constexpr param VAR_CMP_DT[] = {
	{ 1, 39, 0, 127, false, three_digits }, { 2, 40, 4, 40, false, frequency }, { 3, 41, 52, 76, false, centered }, { 4, 4, 34, 60, false, frequency },
	{ 5, 61, 0, 127, false, three_digits }, { 7, 108, 14, 54, false, frequency }, { 8, 109, 52, 76, false, centered }, { 9, 110, 10, 120, false, tenths },
	{ 10, 10, 1, 127, false, dry_wet }, { 11, 99, 0, 127, false, three_digits }, { 12, 57, 0, 19, false, compressor_attack },
	{ 13, 58, 0, 15, false, compressor_release }, { 14, 59, 79, 121, false, below_full_scale }, { 15, 60, 0, 7, false, compressor_ratio },
};
inline constexpr param VAR_AMP_SIM[] = {
	{ 1, 39, 0, 127, false, three_digits }, { 2, 43, 0, 3, false, amp_type }, { 3, 4, 34, 60, false, frequency }, { 4, 61, 0, 127, false, three_digits },
	{ 10, 10, 1, 127, false, dry_wet }, { 11, 99, 0, 127, false, three_digits },
};
inline constexpr param VAR_AMP_SIM_TUBE[] = {
	{ 1, 39, 0, 127, false, three_digits }, { 2, 43, 0, 6, false, amp_type }, { 3, 4, 34, 60, false, frequency }, { 4, 61, 0, 127, false, three_digits },
	{ 10, 10, 1, 127, false, dry_wet },
};
inline constexpr param VAR_THREE_BAND_EQ[] = {
	{ 1, 44, 52, 76, false, centered }, { 2, 45, 14, 54, false, frequency }, { 3, 46, 52, 76, false, centered }, { 4, 47, 10, 120, false, tenths },
	{ 5, 48, 52, 76, false, centered }, { 6, 78, 8, 40, false, frequency }, { 7, 49, 28, 58, false, frequency }, { 15, 94, 0, 1, false, input_mode },
};
inline constexpr param VAR_TWO_BAND_EQ[] = {
	{ 1, 78, 4, 40, false, frequency }, { 2, 44, 52, 76, false, centered }, { 3, 49, 28, 58, false, frequency }, { 4, 48, 52, 76, false, centered },
};
inline constexpr param VAR_AUTO_WAH[] = {
	{ 1, 25, 0, 127, false, lfo_hertz }, { 2, 26, 0, 127, false, three_digits }, { 3, 50, 0, 127, false, three_digits },
	{ 4, 51, 10, 120, false, tenths }, { 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency },
	{ 9, 89, 52, 76, false, centered }, { 10, 10, 1, 127, false, dry_wet }, { 11, 39, 0, 127, false, three_digits },
};
inline constexpr param VAR_A_WAH_DT[] = {
	{ 1, 25, 0, 127, false, lfo_hertz }, { 2, 26, 0, 127, false, three_digits }, { 3, 50, 0, 127, false, three_digits },
	{ 4, 51, 10, 120, false, tenths }, { 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency },
	{ 9, 89, 52, 76, false, centered }, { 10, 10, 1, 127, false, dry_wet }, { 11, 39, 0, 127, false, three_digits }, { 12, 111, 52, 76, false, centered },
	{ 13, 112, 52, 76, false, centered }, { 14, 4, 34, 60, false, frequency }, { 15, 61, 0, 127, false, three_digits },
};
inline constexpr param VAR_PITCH_CHANGE[] = {
	{ 1, 53, 40, 88, false, centered }, { 2, 2, 0, 127, false, long_delay }, { 3, 54, 14, 114, false, centered }, { 4, 55, 14, 114, false, centered },
	{ 5, 9, 1, 127, false, centered }, { 10, 10, 1, 127, false, dry_wet }, { 11, 100, 1, 127, false, pan }, { 12, 101, 0, 127, false, three_digits },
	{ 13, 102, 1, 127, false, pan }, { 14, 103, 0, 127, false, three_digits },
};
inline constexpr param VAR_HM_ENHNCER[] = {
	{ 1, 3, 28, 58, false, frequency }, { 2, 39, 0, 127, false, three_digits }, { 3, 56, 0, 127, false, three_digits },
};
inline constexpr param VAR_TOUCH_WAH[] = {
	{ 1, 52, 0, 127, false, three_digits }, { 2, 50, 0, 127, false, three_digits }, { 3, 51, 10, 120, false, tenths }, { 6, 40, 4, 40, false, frequency },
	{ 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency }, { 9, 89, 52, 76, false, centered }, { 10, 10, 1, 127, false, dry_wet },
	{ 11, 39, 0, 127, false, three_digits },
};
inline constexpr param VAR_TOUCH_WAH_EQ[] = {
	{ 1, 52, 0, 127, false, three_digits }, { 2, 50, 0, 127, false, three_digits }, { 3, 51, 10, 120, false, tenths }, { 6, 40, 4, 40, false, frequency },
	{ 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency }, { 9, 89, 52, 76, false, centered }, { 10, 10, 1, 127, false, dry_wet },
	{ 11, 39, 0, 127, false, three_digits }, { 10, 111, 0, 127, false, centered_low }, { 11, 112, 0, 127, false, centered_low },
	{ 12, 4, 0, 127, false, frequency_from_value_34 }, { 13, 61, 0, 127, false, three_digits }, { 14, 58, 0, 127, false, compressor_release },
};
inline constexpr param VAR_T_WAH_ODRV[] = {
	{ 1, 52, 0, 127, false, three_digits }, { 2, 50, 0, 127, false, three_digits }, { 3, 51, 10, 120, false, tenths }, { 6, 40, 4, 40, false, frequency },
	{ 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency }, { 9, 89, 52, 76, false, centered }, { 10, 10, 1, 127, false, dry_wet },
	{ 11, 39, 0, 127, false, three_digits }, { 12, 111, 52, 76, false, centered }, { 13, 112, 52, 76, false, centered },
	{ 14, 4, 34, 60, false, frequency }, { 15, 61, 0, 127, false, three_digits }, { 16, 58, 52, 67, false, compressor_release_from_lowest_52 },
};
inline constexpr param VAR_COMPRESSOR[] = {
	{ 1, 57, 0, 19, false, compressor_attack }, { 2, 58, 0, 15, false, compressor_release }, { 3, 59, 79, 121, false, below_full_scale },
	{ 4, 60, 0, 7, false, compressor_ratio }, { 5, 61, 0, 127, false, three_digits },
};
inline constexpr param VAR_NOISE_GATE[] = {
	{ 1, 57, 0, 19, false, compressor_attack }, { 2, 58, 0, 15, false, compressor_release }, { 3, 59, 55, 97, false, below_full_scale },
	{ 4, 61, 0, 127, false, three_digits },
};
inline constexpr param VAR_VOIC_CANCL[] = {
	{ 11, 62, 0, 26, false, two_digits }, { 12, 63, 0, 26, false, two_digits },
};
inline constexpr param VAR_TWO_WAY_ROTARY[] = {
	{ 1, 32, 0, 127, false, lfo_hertz }, { 2, 75, 0, 127, false, three_digits }, { 3, 76, 0, 127, false, three_digits },
	{ 4, 77, 1, 127, false, low_high }, { 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency },
	{ 9, 89, 52, 76, false, centered }, { 11, 96, 14, 54, false, frequency }, { 12, 97, 0, 60, false, three_digits_by_three },
};
inline constexpr param VAR_DRIVE_TWO_ROTARY[] = {
	{ 1, 32, 0, 127, false, lfo_hertz }, { 2, 75, 0, 127, false, three_digits }, { 3, 76, 0, 127, false, three_digits },
	{ 4, 77, 1, 127, false, low_high }, { 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency },
	{ 9, 89, 52, 76, false, centered }, { 11, 96, 14, 54, false, frequency }, { 12, 97, 0, 60, false, three_digits_by_three },
	{ 14, 39, 0, 127, false, three_digits }, { 15, 4, 34, 60, false, frequency }, { 16, 61, 0, 127, false, three_digits },
};
inline constexpr param VAR_AMP_TWO_ROTARY[] = {
	{ 1, 32, 0, 127, false, lfo_hertz }, { 2, 75, 0, 127, false, three_digits }, { 3, 76, 0, 127, false, three_digits },
	{ 4, 77, 1, 127, false, low_high }, { 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency },
	{ 9, 89, 52, 76, false, centered }, { 11, 96, 14, 54, false, frequency }, { 12, 97, 0, 60, false, three_digits_by_three },
	{ 13, 43, 0, 3, false, amp_type }, { 14, 39, 0, 127, false, three_digits }, { 15, 4, 34, 60, false, frequency },
	{ 16, 61, 0, 127, false, three_digits },
};
inline constexpr param VAR_ENS_DETUNE[] = {
	{ 1, 28, 14, 114, false, centered_spaced }, { 2, 29, 0, 127, false, modulation_delay }, { 3, 30, 0, 127, false, modulation_delay },
	{ 10, 10, 1, 127, false, dry_wet }, { 11, 40, 4, 40, false, frequency }, { 12, 41, 52, 76, false, centered }, { 13, 88, 28, 58, false, frequency },
	{ 14, 89, 52, 76, false, centered },
};
inline constexpr param VAR_AMBIENCE[] = {
	{ 1, 24, 0, 127, false, modulation_delay }, { 2, 31, 0, 1, false, output_phase }, { 6, 40, 4, 40, false, frequency },
	{ 7, 41, 52, 76, false, centered }, { 8, 88, 28, 58, false, frequency }, { 9, 89, 52, 76, false, centered }, { 10, 10, 1, 127, false, dry_wet },
};
inline constexpr param VAR_TALK_MOD[] = {
	{ 1, 64, 0, 4, false, vowel }, { 2, 65, 1, 62, false, two_digits }, { 3, 39, 0, 127, false, three_digits }, { 4, 61, 0, 127, false, three_digits },
};
inline constexpr param VAR_LO_FI[] = {
	{ 1, 66, 0, 127, false, sample_frequency }, { 2, 67, 1, 127, false, three_digits }, { 3, 68, 0, 18, false, output_gain },
	{ 4, 4, 10, 60, false, frequency }, { 5, 69, 0, 5, false, filter_character }, { 6, 104, 10, 120, false, tenths }, { 7, 105, 0, 6, false, digit },
	{ 8, 106, 0, 1, false, on_off }, { 10, 10, 1, 127, false, dry_wet }, { 15, 94, 0, 1, false, input_mode },
};
inline constexpr param VAR_DT_DELAY[] = {
	{ 1, 5, 1, 14860, true, delay_tenths }, { 2, 6, 1, 14860, true, delay_tenths }, { 3, 8, 1, 14860, true, delay_tenths },
	{ 4, 9, 1, 127, true, centered }, { 5, 70, 0, 127, true, three_digits }, { 6, 73, 0, 127, true, three_digits }, { 7, 74, 0, 127, true, three_digits },
	{ 8, 111, 52, 76, true, centered }, { 9, 112, 52, 76, true, centered }, { 10, 10, 1, 127, true, dry_wet },
};
inline constexpr param VAR_CMP_DT_DLY[] = {
	{ 1, 71, 1, 14860, true, delay_tenths }, { 2, 72, 1, 127, true, centered }, { 3, 70, 0, 127, true, three_digits },
	{ 4, 73, 0, 127, true, three_digits }, { 5, 74, 0, 127, true, three_digits }, { 6, 111, 52, 76, true, centered }, { 7, 112, 52, 76, true, centered },
	{ 10, 10, 1, 127, true, dry_wet }, { 11, 57, 0, 19, false, compressor_attack }, { 12, 58, 0, 15, false, compressor_release },
	{ 13, 59, 79, 121, false, below_full_scale }, { 14, 60, 0, 7, false, compressor_ratio },
};
inline constexpr param VAR_WAH_DT_DLY[] = {
	{ 1, 71, 1, 14860, true, delay_tenths }, { 2, 72, 1, 127, true, centered }, { 3, 70, 0, 127, true, three_digits },
	{ 4, 73, 0, 127, true, three_digits }, { 5, 74, 0, 127, true, three_digits }, { 6, 111, 52, 76, true, centered }, { 7, 112, 52, 76, true, centered },
	{ 10, 10, 1, 127, true, dry_wet }, { 11, 52, 0, 127, false, three_digits }, { 12, 50, 0, 127, false, three_digits },
	{ 13, 51, 10, 120, false, tenths }, { 14, 58, 52, 67, false, compressor_release_from_lowest_52 },
};
inline constexpr param VAR_V_DT_HARD[] = {
	{ 1, 114, 0, 100, true, percent }, { 2, 117, 0, 4, true, device }, { 3, 115, 0, 5, true, speaker }, { 4, 116, 0, 20, true, two_digits },
	{ 5, 61, 0, 100, true, percent }, { 10, 10, 1, 127, true, dry_wet },
};
inline constexpr param VAR_V_DT_H_DLY[] = {
	{ 1, 114, 0, 100, true, percent }, { 2, 117, 0, 4, true, device }, { 3, 115, 0, 5, true, speaker }, { 4, 116, 0, 20, true, two_digits },
	{ 5, 61, 0, 100, true, percent }, { 6, 5, 1, 14860, true, delay_tenths }, { 7, 6, 1, 14860, true, delay_tenths },
	{ 8, 8, 1, 14860, true, delay_tenths }, { 9, 9, 1, 127, true, centered }, { 10, 10, 1, 127, true, dry_wet }, { 11, 70, 0, 127, false, three_digits },
};
inline constexpr param VAR_DUAL_ROTARY[] = {
	{ 1, 119, 0, 63, false, lfo_hertz }, { 2, 120, 0, 63, false, lfo_hertz }, { 3, 121, 64, 127, false, lfo_hertz },
	{ 4, 122, 64, 127, false, lfo_hertz }, { 5, 123, 0, 127, false, three_digits }, { 6, 124, 0, 127, false, three_digits },
	{ 7, 75, 0, 127, false, three_digits }, { 8, 76, 0, 127, false, three_digits }, { 9, 77, 1, 127, false, low_high },
	{ 11, 40, 4, 40, false, frequency }, { 12, 41, 52, 76, false, centered }, { 13, 88, 28, 58, false, frequency }, { 14, 89, 52, 76, false, centered },
	{ 15, 97, 0, 60, false, three_digits_by_three }, { 15, 125, 0, 127, false, speed_control },
};
inline constexpr param VAR_V_FLANGER[] = {
	{ 1, 25, 0, 127, true, lfo_hertz }, { 2, 26, 0, 127, true, three_digits }, { 3, 130, 0, 2, true, lfo_wave }, { 4, 27, 0, 139, true, delay_offset },
	{ 5, 107, 0, 200, true, signed_percent }, { 6, 40, 4, 40, true, frequency }, { 7, 41, 52, 76, true, centered }, { 8, 88, 28, 58, true, frequency },
	{ 9, 89, 52, 76, true, centered }, { 10, 10, 1, 127, true, dry_wet }, { 11, 108, 14, 54, false, frequency }, { 12, 109, 52, 76, false, centered },
	{ 13, 110, 1, 120, false, tenths }, { 14, 131, 0, 16, false, half_step_degrees }, { 15, 17, 1, 10, false, tenths_narrow },
	{ 16, 132, 0, 10, false, two_digits },
};
inline constexpr param VAR_MULTI_COMP[] = {
	{ 1, 177, 0, 11, false, lofi_type }, { 2, 140, 32, 96, false, centered }, { 3, 178, 1, 127, false, centered }, { 4, 179, 1, 127, false, centered },
	{ 5, 180, 1, 127, false, centered },
};
inline constexpr param VAR_T_FLANGER[] = {
	{ 1, 25, 5, 21, false, tempo_note }, { 2, 26, 0, 127, false, three_digits }, { 3, 79, 1, 127, false, centered },
	{ 4, 27, 0, 127, false, modulation_delay }, { 5, 133, 0, 2, false, reset }, { 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered },
	{ 8, 88, 28, 58, false, frequency }, { 9, 89, 52, 76, false, centered }, { 10, 10, 1, 127, false, dry_wet }, { 11, 108, 14, 54, false, frequency },
	{ 12, 109, 52, 76, false, centered }, { 13, 110, 10, 120, false, tenths }, { 14, 95, 4, 124, false, phase_degrees },
};
inline constexpr param VAR_T_PHASER[] = {
	{ 1, 25, 5, 21, false, tempo_note }, { 2, 26, 0, 127, false, three_digits }, { 3, 38, 0, 127, false, three_digits },
	{ 4, 107, 1, 127, false, centered }, { 5, 133, 0, 2, false, reset }, { 6, 40, 4, 40, false, frequency }, { 7, 41, 52, 76, false, centered },
	{ 8, 88, 28, 58, false, frequency }, { 9, 89, 52, 76, false, centered }, { 10, 10, 1, 127, false, dry_wet }, { 11, 98, 3, 6, false, digit },
	{ 13, 95, 4, 124, false, phase_degrees },
};
inline constexpr param VAR_DYNA_FLT[] = {
	{ 1, 177, 0, 5, false, filter_type }, { 2, 52, 0, 127, false, three_digits }, { 3, 176, 0, 127, false, three_digits },
	{ 4, 51, 0, 127, false, resonance }, { 5, 135, 0, 127, false, dynamics_attack }, { 6, 136, 0, 127, false, dynamics_release },
	{ 7, 137, 0, 127, false, three_digits }, { 8, 138, 0, 1, false, direction }, { 9, 139, 0, 127, false, three_digits },
	{ 10, 10, 1, 127, false, dry_wet }, { 13, 40, 4, 40, false, frequency }, { 14, 41, 52, 76, false, centered }, { 15, 88, 28, 58, false, frequency },
	{ 16, 89, 52, 76, false, centered },
};
inline constexpr param VAR_DYNA_FLANG[] = {
	{ 1, 52, 0, 127, false, three_digits }, { 2, 134, 0, 127, false, three_digits }, { 3, 79, 1, 127, false, centered },
	{ 4, 135, 0, 127, false, dynamics_attack }, { 5, 136, 0, 127, false, dynamics_release }, { 6, 137, 0, 127, false, three_digits },
	{ 7, 138, 0, 1, false, direction }, { 8, 139, 0, 127, false, three_digits }, { 9, 176, 0, 127, false, three_digits },
	{ 10, 10, 1, 127, false, dry_wet }, { 13, 40, 4, 40, false, frequency }, { 14, 41, 52, 76, false, centered }, { 15, 88, 28, 58, false, frequency },
	{ 16, 89, 52, 76, false, centered },
};
inline constexpr param VAR_DYNA_PHASE[] = {
	{ 1, 52, 0, 127, false, three_digits }, { 2, 176, 0, 127, false, three_digits }, { 3, 9, 1, 127, false, centered },
	{ 4, 135, 0, 127, false, dynamics_attack }, { 5, 136, 0, 127, false, dynamics_release }, { 6, 137, 0, 127, false, three_digits },
	{ 7, 138, 0, 1, false, direction }, { 8, 139, 0, 127, false, three_digits }, { 10, 10, 1, 127, false, dry_wet }, { 11, 98, 4, 6, false, digit },
	{ 13, 40, 4, 40, false, frequency }, { 14, 41, 52, 76, false, centered }, { 15, 88, 28, 58, false, frequency }, { 16, 89, 52, 76, false, centered },
};
inline constexpr param VAR_DYNA_RING[] = {
	{ 1, 52, 0, 127, false, three_digits }, { 2, 3, 0, 52, false, frequency }, { 3, 4, 34, 60, false, frequency },
	{ 4, 135, 0, 127, false, dynamics_attack }, { 5, 136, 0, 127, false, dynamics_release }, { 6, 137, 0, 127, false, three_digits },
	{ 7, 138, 0, 1, false, direction }, { 8, 139, 0, 127, false, three_digits }, { 9, 176, 0, 127, false, three_digits },
	{ 10, 10, 1, 127, false, dry_wet }, { 13, 40, 4, 40, false, frequency }, { 14, 41, 52, 76, false, centered }, { 15, 88, 28, 58, false, frequency },
	{ 16, 89, 52, 76, false, centered },
};
inline constexpr param VAR_RING_MOD[] = {
	{ 1, 196, 0, 127, false, coarse_frequency }, { 2, 197, 0, 127, false, three_digits }, { 3, 130, 0, 1, false, lfo_wave },
	{ 4, 26, 0, 127, false, three_digits }, { 5, 198, 0, 127, false, lfo_hertz }, { 6, 3, 0, 52, false, frequency }, { 7, 4, 34, 60, false, frequency },
	{ 10, 10, 1, 127, false, dry_wet }, { 13, 40, 4, 40, false, frequency }, { 14, 41, 52, 76, false, centered }, { 15, 88, 28, 58, false, frequency },
	{ 16, 89, 52, 76, false, centered },
};
inline constexpr param VAR_SLICE[] = {
	{ 1, 187, 5, 11, false, input_mode_from_value_52 }, { 2, 188, 0, 100, false, percent }, { 3, 189, 0, 4, false, letter },
	{ 4, 190, 0, 127, false, three_digits }, { 5, 191, 1, 127, false, centered }, { 6, 192, 0, 127, false, three_digits },
	{ 7, 193, 0, 9, false, letter }, { 8, 39, 0, 127, false, three_digits }, { 9, 194, 0, 15, false, two_digits }, { 10, 10, 1, 127, false, dry_wet },
	{ 13, 40, 4, 40, false, frequency }, { 14, 41, 52, 76, false, centered }, { 15, 88, 28, 58, false, frequency }, { 16, 89, 52, 76, false, centered },
};
inline constexpr param VAR_ISOLATOR[] = {
	{ 1, 175, 0, 1, false, on_off }, { 2, 181, 0, 127, false, three_digits }, { 3, 182, 0, 127, false, three_digits },
	{ 4, 183, 0, 127, false, three_digits }, { 5, 184, 0, 1, false, on_off }, { 6, 185, 0, 1, false, on_off }, { 7, 186, 0, 1, false, on_off },
};
inline constexpr param VAR_LOW_RESO[] = {
	{ 1, 141, 0, 127, false, three_digits }, { 2, 142, 1, 127, false, three_digits }, { 3, 143, 1, 127, false, centered },
	{ 4, 144, 0, 7, false, resolution }, { 5, 145, 0, 127, false, three_digits }, { 6, 146, 0, 2, false, phase_invert },
	{ 10, 10, 1, 127, false, dry_wet },
};
inline constexpr param VAR_D_TURNTBL[] = {
	{ 1, 147, 0, 5, false, digit }, { 2, 148, 0, 127, false, three_digits }, { 3, 149, 0, 6, false, digit }, { 4, 150, 0, 127, false, lfo_frequency },
	{ 5, 151, 0, 127, false, three_digits }, { 6, 152, 0, 127, false, three_digits }, { 7, 153, 34, 60, false, frequency },
	{ 8, 154, 10, 120, false, tenths }, { 9, 155, 0, 127, false, three_digits }, { 11, 157, 0, 127, false, three_digits },
	{ 12, 156, 34, 60, false, frequency },
};
inline constexpr param VAR_D_SCRATCH[] = {
	{ 1, 158, 0, 127, true, three_digits }, { 2, 2, 1, 4600, true, delay_tenths }, { 3, 159, 1, 127, true, three_digits },
	{ 4, 160, 0, 127, true, three_digits }, { 5, 161, 0, 127, true, lfo_frequency }, { 6, 162, 0, 127, true, three_digits },
	{ 7, 163, 14, 54, true, frequency }, { 8, 164, 52, 76, true, centered }, { 9, 165, 10, 120, true, tenths }, { 10, 10, 1, 127, true, dry_wet },
	{ 11, 3, 0, 52, false, frequency },
};

inline constexpr type_row REV_TYPES[] = {
	{ 0x00, 0x00, NO_PARAMS, 0 }, { 0x01, 0x00, REV_HALL, 10 }, { 0x01, 0x01, REV_HALL, 10 }, { 0x01, 0x06, REV_HALL, 10 },
	{ 0x01, 0x07, REV_HALL, 10 }, { 0x02, 0x00, REV_HALL, 10 }, { 0x02, 0x01, REV_HALL, 10 }, { 0x02, 0x02, REV_HALL, 10 },
	{ 0x02, 0x05, REV_HALL, 10 }, { 0x02, 0x06, REV_HALL, 10 }, { 0x02, 0x07, REV_HALL, 10 }, { 0x03, 0x00, REV_HALL, 10 },
	{ 0x03, 0x01, REV_HALL, 10 }, { 0x04, 0x00, REV_HALL, 10 }, { 0x04, 0x07, REV_HALL, 10 }, { 0x10, 0x00, REV_WHITE_ROOM, 14 },
	{ 0x11, 0x00, REV_WHITE_ROOM, 14 }, { 0x12, 0x00, REV_WHITE_ROOM, 14 }, { 0x13, 0x00, REV_WHITE_ROOM, 14 },
};
inline constexpr type_row CHO_TYPES[] = {
	{ 0x00, 0x00, NO_PARAMS, 0 }, { 0x41, 0x00, CHO_CHORUS, 9 }, { 0x41, 0x01, CHO_CHORUS, 9 }, { 0x41, 0x02, CHO_CHORUS, 9 },
	{ 0x41, 0x08, CHO_CHORUS, 9 }, { 0x41, 0x03, CHO_CHORUS, 9 }, { 0x41, 0x04, CHO_CHORUS, 9 }, { 0x41, 0x05, CHO_CHORUS, 9 },
	{ 0x41, 0x06, CHO_CHORUS, 9 }, { 0x41, 0x07, CHO_CHORUS, 9 }, { 0x42, 0x00, CHO_CHORUS, 9 }, { 0x42, 0x01, CHO_CHORUS, 9 },
	{ 0x42, 0x02, CHO_CHORUS, 9 }, { 0x42, 0x08, CHO_CHORUS, 9 }, { 0x43, 0x00, CHO_FLANGER, 9 }, { 0x43, 0x01, CHO_FLANGER, 9 },
	{ 0x43, 0x08, CHO_FLANGER, 9 }, { 0x43, 0x07, CHO_FLANGER, 9 }, { 0x44, 0x00, CHO_SYMPHONIC, 7 }, { 0x48, 0x00, CHO_PHASER, 10 },
	{ 0x57, 0x00, CHO_ENS_DETUNE, 3 },
};
inline constexpr type_row VAR_TYPES[] = {
	{ 0x00, 0x00, NO_PARAMS, 0 }, { 0x01, 0x00, VAR_HALL, 11 }, { 0x01, 0x01, VAR_HALL, 11 }, { 0x01, 0x06, VAR_HALL, 11 },
	{ 0x01, 0x07, VAR_HALL, 11 }, { 0x02, 0x00, VAR_HALL, 11 }, { 0x02, 0x01, VAR_HALL, 11 }, { 0x02, 0x02, VAR_HALL, 11 },
	{ 0x02, 0x05, VAR_HALL, 11 }, { 0x02, 0x06, VAR_HALL, 11 }, { 0x02, 0x07, VAR_HALL, 11 }, { 0x03, 0x00, VAR_HALL, 11 },
	{ 0x03, 0x01, VAR_HALL, 11 }, { 0x04, 0x00, VAR_HALL, 11 }, { 0x04, 0x07, VAR_HALL, 11 }, { 0x05, 0x00, VAR_DELAY_LCR, 12 },
	{ 0x06, 0x00, VAR_DELAY_L_R, 11 }, { 0x07, 0x00, VAR_ECHO, 13 }, { 0x08, 0x00, VAR_CROSSDELAY, 10 }, { 0x09, 0x00, VAR_EARLY_REFLECTION, 11 },
	{ 0x09, 0x01, VAR_EARLY_REFLECTION, 11 }, { 0x0a, 0x00, VAR_GATE_REV, 11 }, { 0x0b, 0x00, VAR_GATE_REV, 11 }, { 0x10, 0x00, VAR_WHITE_ROOM, 15 },
	{ 0x11, 0x00, VAR_WHITE_ROOM, 15 }, { 0x12, 0x00, VAR_WHITE_ROOM, 15 }, { 0x13, 0x00, VAR_WHITE_ROOM, 15 }, { 0x14, 0x00, VAR_KARAOKE, 5 },
	{ 0x14, 0x01, VAR_KARAOKE, 5 }, { 0x14, 0x02, VAR_KARAOKE, 5 }, { 0x15, 0x00, VAR_T_DELAY, 10 }, { 0x15, 0x08, VAR_T_DELAY, 10 },
	{ 0x16, 0x00, VAR_T_CRS_DLY, 11 }, { 0x41, 0x00, VAR_CHORUS, 13 }, { 0x41, 0x01, VAR_CHORUS, 13 }, { 0x41, 0x02, VAR_CHORUS, 13 },
	{ 0x41, 0x08, VAR_CHORUS, 13 }, { 0x41, 0x03, VAR_CHORUS, 13 }, { 0x41, 0x04, VAR_CHORUS, 13 }, { 0x41, 0x05, VAR_CHORUS, 13 },
	{ 0x41, 0x06, VAR_CHORUS, 13 }, { 0x41, 0x07, VAR_CHORUS, 13 }, { 0x42, 0x00, VAR_CHORUS, 13 }, { 0x42, 0x01, VAR_CHORUS, 13 },
	{ 0x42, 0x02, VAR_CHORUS, 13 }, { 0x42, 0x08, VAR_CHORUS, 13 }, { 0x43, 0x00, VAR_FLANGER, 13 }, { 0x43, 0x01, VAR_FLANGER, 13 },
	{ 0x43, 0x08, VAR_FLANGER, 13 }, { 0x43, 0x07, VAR_FLANGER, 13 }, { 0x44, 0x00, VAR_SYMPHONIC, 11 }, { 0x45, 0x00, VAR_ROTARY_SP, 10 },
	{ 0x45, 0x01, VAR_DT_RTRY, 10 }, { 0x45, 0x02, VAR_DT_RTRY, 10 }, { 0x45, 0x03, VAR_AMP_RTRY, 11 }, { 0x46, 0x00, VAR_TREMOLO, 12 },
	{ 0x47, 0x00, VAR_AUTO_PAN, 11 }, { 0x47, 0x01, VAR_AUTO_PAN_WAVE, 13 }, { 0x48, 0x00, VAR_PHASER, 11 }, { 0x48, 0x08, VAR_PHASER_DEEP, 11 },
	{ 0x49, 0x00, VAR_DISTORTION, 10 }, { 0x49, 0x01, VAR_CMP_DT, 14 }, { 0x49, 0x08, VAR_DISTORTION, 10 }, { 0x4a, 0x00, VAR_DISTORTION, 10 },
	{ 0x4a, 0x08, VAR_DISTORTION, 10 }, { 0x4b, 0x00, VAR_AMP_SIM, 6 }, { 0x4b, 0x01, VAR_AMP_SIM_TUBE, 5 }, { 0x4b, 0x08, VAR_AMP_SIM, 6 },
	{ 0x4c, 0x00, VAR_THREE_BAND_EQ, 8 }, { 0x4d, 0x00, VAR_TWO_BAND_EQ, 4 }, { 0x4e, 0x00, VAR_AUTO_WAH, 10 }, { 0x4e, 0x01, VAR_A_WAH_DT, 14 },
	{ 0x4e, 0x02, VAR_A_WAH_DT, 14 }, { 0x50, 0x00, VAR_PITCH_CHANGE, 10 }, { 0x50, 0x01, VAR_PITCH_CHANGE, 10 }, { 0x51, 0x00, VAR_HM_ENHNCER, 3 },
	{ 0x52, 0x00, VAR_TOUCH_WAH, 9 }, { 0x52, 0x08, VAR_TOUCH_WAH_EQ, 14 }, { 0x52, 0x01, VAR_TOUCH_WAH, 9 }, { 0x52, 0x02, VAR_T_WAH_ODRV, 14 },
	{ 0x53, 0x00, VAR_COMPRESSOR, 5 }, { 0x54, 0x00, VAR_NOISE_GATE, 4 }, { 0x55, 0x00, VAR_VOIC_CANCL, 2 }, { 0x56, 0x00, VAR_TWO_WAY_ROTARY, 10 },
	{ 0x56, 0x01, VAR_DRIVE_TWO_ROTARY, 13 }, { 0x56, 0x02, VAR_DRIVE_TWO_ROTARY, 13 }, { 0x56, 0x03, VAR_AMP_TWO_ROTARY, 14 }, { 0x57, 0x00, VAR_ENS_DETUNE, 8 },
	{ 0x58, 0x00, VAR_AMBIENCE, 7 }, { 0x5d, 0x00, VAR_TALK_MOD, 4 }, { 0x5e, 0x00, VAR_LO_FI, 10 }, { 0x5f, 0x00, VAR_DT_DELAY, 10 },
	{ 0x5f, 0x01, VAR_DT_DELAY, 10 }, { 0x60, 0x00, VAR_CMP_DT_DLY, 12 }, { 0x60, 0x01, VAR_CMP_DT_DLY, 12 }, { 0x61, 0x00, VAR_WAH_DT_DLY, 12 },
	{ 0x61, 0x01, VAR_WAH_DT_DLY, 12 }, { 0x62, 0x00, VAR_V_DT_HARD, 6 }, { 0x62, 0x01, VAR_V_DT_H_DLY, 11 }, { 0x62, 0x02, VAR_V_DT_HARD, 6 },
	{ 0x62, 0x03, VAR_V_DT_H_DLY, 11 }, { 0x63, 0x00, VAR_DUAL_ROTARY, 15 }, { 0x63, 0x01, VAR_DUAL_ROTARY, 15 }, { 0x68, 0x00, VAR_V_FLANGER, 16 },
	{ 0x69, 0x00, VAR_MULTI_COMP, 5 }, { 0x6b, 0x00, VAR_T_FLANGER, 14 }, { 0x6c, 0x00, VAR_T_PHASER, 12 }, { 0x6d, 0x00, VAR_DYNA_FLT, 14 },
	{ 0x6e, 0x00, VAR_DYNA_FLANG, 14 }, { 0x6f, 0x00, VAR_DYNA_PHASE, 14 }, { 0x70, 0x00, VAR_DYNA_RING, 14 }, { 0x71, 0x00, VAR_RING_MOD, 12 },
	{ 0x72, 0x00, VAR_SLICE, 14 }, { 0x73, 0x00, VAR_ISOLATOR, 7 }, { 0x74, 0x00, VAR_LOW_RESO, 7 }, { 0x75, 0x00, VAR_D_TURNTBL, 11 },
	{ 0x76, 0x00, VAR_D_SCRATCH, 11 }, { 0x40, 0x00, NO_PARAMS, 0 },
};

inline constexpr int AD_BANK_COUNT[] = { 13, 13, 13, 5, 16, 9, 13, 9 };
inline int ad_preset_index(int bank, int preset)
{
	int index = 0;
	for (int b = 0; b < bank; b++)
		index += AD_BANK_COUNT[b];
	return index + preset;
}
inline u32 ad_preset_at(int bank, int preset)
{
	const int index = ad_preset_index(bank, preset);
	::xg::rom_position::rom_run run = rom_at::AD_PRESET_NAME_RUNS[0];
	for (const ::xg::rom_position::rom_run &r : rom_at::AD_PRESET_NAME_RUNS)
		if (r.first_index <= index)
			run = r;
	return run.at + u32(index - run.first_index) * rom_at::AD_PRESET_NAME_STRIDE;
}
inline ::xg::rom_text ad_preset_name(int bank, int preset) { return { ad_preset_at(bank, preset), 8 }; }

// 種類を表の並びで引く。無い種類はパラメーターなし
inline type_table find(table_id table, int type)
{
	const auto look_up = [&](const auto &rows) -> type_table {
		for (const type_row &r : rows)
			if ((r.msb << 7 | r.lsb) == type)
				return { type, r.params, r.count };
		return { type, NO_PARAMS, 0 };
	};
	return table == table_reverb ? look_up(REV_TYPES) : table == table_chorus ? look_up(CHO_TYPES) : look_up(VAR_TYPES);
}

// パラメーターの受け付ける範囲。画面の表と違う種類だけ
struct receive_range { u8 msb, lsb, number; short lowest, highest; };
inline constexpr receive_range RECEIVE_RANGES[] = {
	{ 0x47, 0x01, 5, 0, 28 }, { 0x47, 0x01, 15, 0, 1 },
	{ 0x52, 0x08, 12, 52, 76 }, { 0x52, 0x08, 13, 52, 76 }, { 0x52, 0x08, 14, 34, 60 }, { 0x52, 0x08, 15, 0, 127 }, { 0x52, 0x08, 16, 52, 67 },
};

enum tail_format : u8 { tail_center, tail_pan, tail_decimal3, tail_part, tail_part_wide, tail_connect };
struct tail_item { short address; ::xg::rom_text field; short lowest, highest; tail_format format; };
struct tail_list { const tail_item *items; int count; };
inline constexpr tail_item REVERB_TAIL[] = { { 0x100 | 0x0d, rom_at::LABELS_FX_TAIL[0], 1, 127, tail_pan } };
inline constexpr tail_item CHORUS_TAIL[] = {
	{ 0x100 | 0x2d, rom_at::LABELS_FX_TAIL[1], 1, 127, tail_pan }, { 0x100 | 0x2e, rom_at::LABELS_FX_TAIL[3], 0, 127, tail_decimal3 },
};
inline constexpr tail_item VARIATION_TAIL_INSERTION[] = {
	{ 0x100 | 0x5f, rom_at::LABELS_FX_TAIL[6], 0, 127, tail_center }, { 0x100 | 0x5b, rom_at::LABELS_FX_TAIL[8], 0, 66, tail_part_wide }, { 0x100 | 0x5a, rom_at::LABELS_FX_TAIL[7], 0, 1, tail_connect },
};
inline constexpr tail_item VARIATION_TAIL_SYSTEM[] = {
	{ 0x100 | 0x57, rom_at::LABELS_FX_TAIL[2], 1, 127, tail_pan }, { 0x100 | 0x59, rom_at::LABELS_FX_TAIL[4], 0, 127, tail_decimal3 },
	{ 0x100 | 0x58, rom_at::LABELS_FX_TAIL[5], 0, 127, tail_decimal3 }, { 0x100 | 0x5a, rom_at::LABELS_FX_TAIL[7], 0, 1, tail_connect },
};
inline constexpr tail_item INSERTION_TAIL[] = {
	{ 0x100 | 0x10, rom_at::LABELS_FX_TAIL[9], 0, 127, tail_center }, { 0x100 | 0x0c, rom_at::TEXT_FX_INSERTION_PART, 0, 66, tail_part },
};
inline tail_list tail_of(table_id table, bool variation_system)
{
	switch (table) {
	case table_reverb: return { REVERB_TAIL, 1 };
	case table_chorus: return { CHORUS_TAIL, 2 };
	case table_variation: return variation_system ? tail_list{ VARIATION_TAIL_SYSTEM, 4 } : tail_list{ VARIATION_TAIL_INSERTION, 3 };
	default: return { INSERTION_TAIL, 2 };
	}
}

// 値の文字
inline void format(const ::xg::voice_rom &voices, const param &p, int raw, char *text, size_t size)
{
	const value_text &t = p.text;
	const int value = std::clamp(raw, int(p.lowest), int(p.highest));
	const int n = (value - t.zero) * t.step;
	switch (t.kind) {
	case text_rom: std::snprintf(text, size, "%s", voices.value(t.list[value - t.zero]).c_str()); break;
	case text_rom_hertz: std::snprintf(text, size, "%s%s", voices.value(t.list[value]).c_str(), voices.text(rom_at::TEXT_FX_HZ).c_str()); break;
	case text_pan_direction: std::snprintf(text, size, "%s", voices.value(t.list[value]).c_str()); break;
	case text_balance: {
		// 同じなら「(E=R)」、片寄ると ** に量が入る
		std::string s = voices.value(t.list[n == 0 ? 0 : n > 0 ? 1 : 2]);
		if (const size_t at = s.find("**"); at != std::string::npos) {
			char amount[8];
			std::snprintf(amount, sizeof(amount), "%02d", std::abs(n));
			s.replace(at, 2, amount);
		}
		std::snprintf(text, size, "%s", s.c_str());
		break;
	}
	case text_number: std::snprintf(text, size, t.pattern, n); break;
	case text_sign: std::snprintf(text, size, t.pattern, n < 0 ? '-' : '+', std::abs(n)); break;
	case text_half_steps: std::snprintf(text, size, t.pattern, n < 0 ? '-' : '+', (std::abs(n) * 45 + 1) / 2); break;
	case text_letter: std::snprintf(text, size, t.pattern, 'A' + n); break;
	case text_tenths: std::snprintf(text, size, t.pattern, value / 10, value % 10); break;
	case text_tenths_free: std::snprintf(text, size, t.pattern, raw / 10, raw % 10); break;
	}
}

inline void format_tail(const ::xg::voice_rom &voices, const tail_item &t, int value, char *text, size_t size)
{
	switch (t.format) {
	case tail_center: std::snprintf(text, size, "%+03d", value - 64); break;
	case tail_pan:
		if (value == 64) std::snprintf(text, size, " C");
		else std::snprintf(text, size, "%c%02d", value < 64 ? 'L' : 'R', std::abs(value - 64));
		break;
	case tail_decimal3: std::snprintf(text, size, "%03d", value); break;
	case tail_connect: std::snprintf(text, size, "%s", voices.text(rom_at::TEXTS_CONNECTION[value ? 1 : 0]).c_str()); break;
	case tail_part:
	case tail_part_wide:
		if (value >= 127) std::snprintf(text, size, "%s", voices.value(rom_at::TEXTS_ON_OFF[0]).c_str());
		else if (value >= 64) std::snprintf(text, size, "AD%02d", value - 63);
		else std::snprintf(text, size, "%02d", value + 1);
		break;
	}
}

} // namespace smu2000::panel_fx

#endif // S_MU2000_NATIVE_PANEL_FX_TABLES_H
