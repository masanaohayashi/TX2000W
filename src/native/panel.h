// license:BSD-3-Clause
#ifndef S_MU2000_NATIVE_PANEL_H
#define S_MU2000_NATIVE_PANEL_H

#include "native/engine.h"
#include "native/panel_fx_tables.h"
#include "xg/fx_types.h"
#include "xg/voices.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace smu2000 {

namespace rom_at = ::xg::rom_position;

class native_panel
{
	using settings_array = std::array<short, 20>;
public:
	enum button : int {
		button_play = 0, button_edit = 1, button_util = 2, button_effect = 3, button_mode = 4, button_eq = 5,
		button_mute = 6, button_part_minus = 7, button_part_plus = 8, button_enter = 9,
		button_select_minus = 10, button_select_plus = 11, button_exit = 12,
		button_value_minus = 13, button_value_plus = 14,
		button_audition = 15,
		button_count
	};
	static constexpr int LINE_LENGTH = 24, ROWS = 2, CGRAM_SIZE = 64;
	static constexpr int MIDI_PARTS = 64, AD_PARTS = 2, ALL_PARTS = MIDI_PARTS + AD_PARTS;
	static constexpr u16 LED_PLAY = 1 << 0, LED_EDIT = 1 << 1, LED_UTIL = 1 << 2, LED_EFFECT = 1 << 3, LED_MODE = 1 << 4, LED_EQ = 1 << 5, LED_MU = 1 << 6;

	enum screen_id : u8 { screen_play, screen_menu, screen_page, screen_confirm, screen_sound_module, screen_message, screen_performance_name, screen_performance_copy, screen_performance_store, screen_performance_recall };
	enum play_layout : u8 { layout_voice, layout_header, layout_all_parts, layout_count };
	// 演奏画面の項目。先頭は音色（バンクとプログラム）で、続きは各パートの値
	enum play_item : u8 { item_voice, item_volume, item_expression, item_pan, item_reverb, item_chorus, item_variation, item_note_shift, play_item_count };
	// 音色の欄: プログラム、同じプログラムのバンクを順に、全音色を通しで
	enum voice_field : u8 { field_program, field_bank, field_every_voice };
	enum menu_id : u8 { menu_edit, menu_util, menu_dump, menu_init, menu_effect, menu_insertion, menu_performance, menu_count };
	enum page_id : u8 {
		page_filter, page_eg, page_eq, page_vibrato, page_others, page_drum,
		page_system, page_card, page_reverb, page_chorus, page_variation, page_insertion, page_master_eq, page_performance_common, page_count
	};
	enum confirm_id : u8 { confirm_dump_all, confirm_dump_multi, confirm_dump_perform, confirm_factory, confirm_init_xg, confirm_init_drum };
	enum mute_view : u8 { mute_normal, mute_solo };

	// 液晶へ渡す中身。text は DDRAM の窓に出ている 24 桁、cgram は外字 8 個ぶん
	struct lcd_frame
	{
		u8 text[ROWS][LINE_LENGTH] = {};
		u8 cgram[CGRAM_SIZE] = {};
	};

	struct view
	{
		screen_id screen = screen_play;
		play_layout layout = layout_voice;
		u8 item = item_voice;
		voice_field field = field_program;
		menu_id menu = menu_edit;
		page_id page = page_filter;
		confirm_id confirm = confirm_dump_all;
		u8 menu_cursor = 0;
		u8 page_item = 0;
		u8 confirm_choice = 0;
		u8 message = 0;
		u8 mode_cursor = 0;
		bool blink_off = false;
		u8 part = 0;
		u8 bank_msb = 0, bank_lsb = 0, program = 0;
		u8 part_mode = 0;
		u8 ad_preset = 0;
		u8 ad1_bank = 0;
		bool fallback_voice = false;        // このバンクに音色が無く、既定の音色が鳴っている
		bool gs = false;
		u8 voice_set = 1;
		u8 contrast = 1;                    // UTIL の Contrast（0 が表示の 1）
		bool solo = false;
		u64 muted = 0;
		u8 muted_ad = 0;
		u8 solo_part = 0;
		u8 volume = 0, expression = 0, pan = 0, reverb = 0, chorus = 0, variation = 0, note_shift = 0;
		u8 bars[ALL_PARTS] = {};            // いま出している項目の各パートの値（音色の項目では音量の目盛り）
		bool bars_are_meter = true;
		int value = 0;                      // 開いている項目の値
		u8 drum_key = 0;
		int effect_type = 0;
		int effect_part = 0;                // 掛かり先。無ければ負
		bool variation_system = false;
		u8 eq_gains[5] = {};
		u8 eq_frequencies[5] = {};
		u8 eq_type = 0;
		u8 fx_block = 0;
		bool performance = false;
		bool performance_names = false;     // PLAY で 4 パートの音色名へ切り替えた
		u8 performance_bank = 0, performance_number = 0;
		u8 performance_name[12] = {};
		u8 performance_flags[4] = {};
		u8 performance_voice[4][3] = {};    // MSB・LSB・プログラム
		u8 performance_volume = 0, performance_pan = 0, performance_reverb = 0, performance_chorus = 0, performance_variation = 0, performance_transpose = 0;
		u8 performance_part_volume = 0;
		bool performance_focus = false;     // 1 つのパートを選んでいる（パートの欄に番号が出る）
		u8 performance_part_item = 0;       // 選んだパートのいまの項目の値
		u8 performance_recall_bank = 0, performance_recall_number = 0;
		u8 name_cursor = 0;
		screen_id executing_title = screen_confirm;
		u8 copy_bank = 0, copy_from = 0, copy_to = 0, copy_field = 0, store_to = 0;
		bool performance_muted = false;
		bool detail = false;                // ENTER で開く「MIDI のメッセージ」の画面
		bool notice = false;
		u8 sysex_error = 0;                 // 受けた SysEx のエラーの知らせ
		u8 checksum = 0;
		u8 progress = 0;                    // 送信の進み（8 段）
		u8 exclusive_address[3] = {};
		u8 exclusive_data[4] = {};
		u8 exclusive_count = 0;             // 0 なら SysEx にならない項目
		bool exclusive_model_performance = false, exclusive_model_mu = false;
		u8 control_number = 0;              // 演奏画面の詳細: CC の番号
		u16 leds() const
		{
			static constexpr u16 MODE_LED[] = { LED_EDIT, LED_UTIL, LED_UTIL, LED_UTIL, LED_EFFECT, LED_EFFECT, LED_EDIT };
			u16 lit = LED_MU;
			if (screen == screen_play)
				return lit | LED_PLAY;
			if (screen == screen_menu)
				return lit | MODE_LED[menu];
			if (screen == screen_performance_name || screen == screen_performance_copy || screen == screen_performance_store || screen == screen_performance_recall)
				return lit | LED_EDIT;
			if (screen == screen_message && (message == MESSAGE_NO_RECALL || executing_title == screen_performance_copy || executing_title == screen_performance_store || executing_title == screen_performance_recall))
				return lit | LED_EDIT;
			if (screen == screen_sound_module)
				return lit | LED_MODE;
			if (screen == screen_confirm || screen == screen_message)
				return lit | LED_UTIL;
			switch (page) {
			case page_system: case page_card: return lit | LED_UTIL;
			case page_reverb: case page_chorus: case page_variation: case page_insertion: return lit | LED_EFFECT;
			case page_master_eq: return lit | LED_EQ;
			default: return lit | LED_EDIT;
			}
		}
	};

	void press(native_engine &engine, int index)
	{
		if (index < 0 || index >= button_count)
			return;
		m_held_button = index;
		m_held_samples = 0;
		m_repeat_at = REPEAT_DELAY;
		if (index == button_audition) {
			audition(engine, true);
			return;
		}
		// 塊を受けた直後のパネルは止まっている
		if (m_key_lock > 0)
			return;
		// エラーの知らせは次のキーで消える（そのキーの働きはしない）
		if (m_sysex_error != sysex_error_none) {
			m_sysex_error = sysex_error_none;
			return;
		}
		act(engine, index);
	}

	void release(native_engine &engine, int index)
	{
		if (index == m_held_button)
			m_held_button = -1;
		if (index == button_audition)
			audition(engine, false);
	}

	void turn(native_engine &engine, int delta)
	{
		if (m_key_lock > 0)
			return;
		if (m_sysex_error != sysex_error_none) {
			m_sysex_error = sysex_error_none;
			return;
		}
		change(engine, delta);
	}

	// 実時間の進み。frames サンプルぶん、点滅と押しっぱなしの連続を進める
	void tick(native_engine &engine, int frames)
	{
		m_blink_samples = (m_blink_samples + u32(frames)) % (BLINK_HALF_PERIOD * 2);
		m_enter_samples = std::min(m_enter_samples + frames, DOUBLE_ENTER_WINDOW);
		meter_tick(engine, frames);
		take_notices(engine);
		if (m_key_lock > 0)
			m_key_lock -= frames;
		if (engine.external_resets() != m_external_resets) {
			m_external_resets = engine.external_resets();
			// 外からのリセット。ロックしていなければミュートと A/D の品書きも戻る
			if (m_settings[SETTING_MUTE_LOCK] == 0) {
				// A/D をロックしていると、そのとき黙っていた A/D はそのまま（ソロで黙っていたものも）
				const bool solo_ad = m_solo && m_solo_part >= MIDI_PARTS;
				const u8 ad_now = m_solo ? (solo_ad ? u8(3 & ~(1 << (m_solo_part - MIDI_PARTS))) : u8(3)) : m_muted_ad;
				const u8 ad_kept = m_settings[SETTING_AD_LOCK] != 0 ? ad_now : u8(0);
				clear_mute(engine);
				m_muted_ad = ad_kept;
				apply_mute(engine);
			}
			if (m_settings[SETTING_AD_LOCK] == 0)
				m_ad_preset = m_ad_bank = {};
		}
		if (engine.performance_mode() && engine.performance_loads() != m_performance_loads) {
			m_performance_loads = engine.performance_loads();
			m_muted = engine.performance_part_silence();
			m_muted_ad = 0;
			m_solo = false;
			m_last_mute_part = -1;
			m_performance_muted = false;
		}
		if (m_message_samples > 0 && (m_message_samples -= frames) <= 0)
			message_done();
		if (m_screen == screen_message && m_message == MESSAGE_TRANSMITTING)
			m_dump_wait -= frames;
		if (m_held_button < 0 || !repeats(m_held_button))
			return;
		m_held_samples += frames;
		// 押しっぱなし: 間隔を縮めながら連続する（測った時刻で）
		while (m_held_samples >= m_repeat_at) {
			act(engine, m_held_button);
			m_repeat_at += m_repeat_at < REPEAT_SLOW_UNTIL ? REPEAT_INTERVAL_SLOW : m_repeat_at < REPEAT_MIDDLE_UNTIL ? REPEAT_INTERVAL_MIDDLE : REPEAT_INTERVAL_FAST;
		}
	}

	view capture(const native_engine &engine) const
	{
		view v;
		v.screen = m_screen;
		v.layout = m_layout;
		v.item = m_item;
		v.field = m_field;
		v.menu = m_menu;
		v.page = m_page;
		v.confirm = m_confirm;
		v.menu_cursor = m_menu_cursor[m_menu];
		v.page_item = m_page_item[m_page];
		v.confirm_choice = u8(m_confirm_choice);
		v.message = m_message;
		v.mode_cursor = m_mode_cursor;
		v.blink_off = m_blink_samples >= BLINK_HALF_PERIOD;
		v.part = u8(m_part);
		v.gs = engine.gs_mode();
		v.voice_set = u8(engine.voice_set());
		v.contrast = u8(m_settings[SETTING_CONTRAST]);
		v.solo = m_solo;
		v.muted = m_muted;
		v.muted_ad = m_muted_ad;
		v.solo_part = u8(m_solo_part);
		v.drum_key = u8(m_drum_key);
		v.detail = m_detail;
		v.executing_title = m_executing_title;
		v.performance = engine.performance_mode();
		if (v.performance) {
			const u8 *record = engine.performance_record();
			v.performance_names = m_performance_names;
			v.performance_bank = u8(engine.performance_bank());
			v.performance_number = u8(engine.performance_number());
			std::copy_n(record + native_engine::PERF_NAME, native_engine::PERF_NAME_SIZE, v.performance_name);
			for (int part = 0; part < native_engine::PERFORMANCE_PARTS; part++) {
				const u8 *q = record + part * native_engine::PERFORMANCE_PART_SIZE;
				v.performance_flags[part] = q[native_engine::PERF_PART_FLAGS];
				v.performance_voice[part][0] = q[native_engine::PERF_PART_BANK_MSB];
				v.performance_voice[part][1] = q[native_engine::PERF_PART_BANK_LSB];
				v.performance_voice[part][2] = q[native_engine::PERF_PART_PROGRAM];
			}
			v.performance_volume = record[native_engine::PERF_VOLUME];
			v.performance_part_volume = record[std::min(m_part, native_engine::PERFORMANCE_PARTS - 1) * native_engine::PERFORMANCE_PART_SIZE + 2];
			v.performance_pan = record[native_engine::PERF_PAN];
			v.performance_reverb = record[native_engine::PERF_REVERB_RETURN];
			v.performance_chorus = record[native_engine::PERF_CHORUS_RETURN];
			v.performance_variation = record[native_engine::PERF_VARIATION_RETURN];
			v.performance_transpose = engine.system_transpose();
			v.performance_muted = m_performance_muted;
			v.performance_focus = m_performance_focus;
			v.performance_part_item = performance_part_item(engine, std::min(m_part, native_engine::PERFORMANCE_PARTS - 1), m_item);
			v.performance_recall_bank = u8(engine.performance_recall_bank());
			v.performance_recall_number = u8(engine.performance_recall_number());
			v.name_cursor = u8(m_name_cursor);
			v.copy_bank = u8(m_copy_bank);
			v.copy_from = u8(m_copy_from);
			v.copy_to = u8(m_copy_to);
			v.copy_field = u8(m_copy_field);
			v.store_to = u8(m_store_to);
		}
		v.notice = m_notice;
		v.sysex_error = u8(m_sysex_error);
		v.checksum = m_error_checksum;
		v.progress = u8(m_transmit_cells);
		if (m_part < MIDI_PARTS) {
			const ::xg::part_state &block = engine.part_settings(m_part);
			v.bank_msb = u8(engine.part_bank_msb(m_part));
			v.bank_lsb = u8(engine.part_bank_lsb(m_part));
			v.program = u8(engine.part_program(m_part));
			v.part_mode = block[BLOCK_PART_MODE];
			if (v.bank_msb == 0 && v.bank_lsb != 0 && v.part_mode == 0 && !v.gs) {
				const ::xg::voice_rom voices(engine.program_rom_shared());
				v.fallback_voice = !bank_has_voice(voices, v.voice_set, 0, v.bank_lsb, v.program);
			}
			// GS は MSB がバンク
			if (v.gs && v.part_mode == 0 && v.bank_msb != 0 && v.bank_msb < 126) {
				const ::xg::voice_rom voices(engine.program_rom_shared());
				v.fallback_voice = voices.lookup(0, 1, v.bank_msb, 0, v.program) == voices.lookup(0, 1, 0, 0, v.program);
			}
		}
		v.ad1_bank = u8(m_ad_bank[0]);
		if (m_part >= MIDI_PARTS) {
			v.ad_preset = u8(m_ad_preset[m_part - MIDI_PARTS]);
			v.program = v.ad_preset;
			v.bank_lsb = u8(m_ad_bank[m_part - MIDI_PARTS]);
		}
		v.volume = part_value(engine, m_part, item_volume);
		v.expression = part_value(engine, m_part, item_expression);
		v.pan = part_value(engine, m_part, item_pan);
		v.reverb = part_value(engine, m_part, item_reverb);
		v.chorus = part_value(engine, m_part, item_chorus);
		v.variation = part_value(engine, m_part, item_variation);
		v.note_shift = part_value(engine, m_part, item_note_shift);
		v.variation_system = engine.variation_is_system();
		v.bars_are_meter = bars_show_meter();
		for (int part = 0; part < ALL_PARTS; part++)
			v.bars[part] = v.bars_are_meter ? m_meter_smooth[part] : bar_value(engine, part);
		if (m_screen == screen_page) {
			v.value = page_value(engine);
			v.fx_block = u8(m_fx_block);
			if (fx_page(m_page)) {
				v.effect_type = effect_type_now(engine);
				v.effect_part = effect_part_now(engine);
				v.variation_system = engine.variation_is_system();
			}
			if (m_page == page_master_eq) {
				v.eq_type = u8(eq_type_now(engine));
				for (int band = 0; band < 5; band++) {
					v.eq_gains[band] = u8(engine.effect_value(native_engine::block_master_eq, 1 + band * 4));
					v.eq_frequencies[band] = u8(engine.effect_value(native_engine::block_master_eq, 2 + band * 4));
				}
			}
			if (m_detail)
				fill_exclusive(engine, v);
		}
		return v;
	}

	// 液晶の中身を作る
	static void compose(const view &v, const ::xg::voice_rom &voices, lcd_frame &out)
	{
		compose_screen(v, voices, out);
		if (v.sysex_error == sysex_error_none)
			return;
		char line[20];
		for (auto &row : out.text)
			std::memset(row, ' ', 17);
		if (v.sysex_error == sysex_error_checksum) {
			put(out.text[0], 17, voices.text(rom_at::TEXT_CHECK_SUM_ERROR));
			std::snprintf(line, sizeof(line), "%s%02X", voices.text(rom_at::TEXT_CHECK_SUM_VALUE).c_str(), v.checksum);
			put(out.text[1], 17, line);
		} else
			put(out.text[0], 17, voices.text(v.sysex_error == sysex_error_data ? rom_at::TEXT_SYSEX_DATA_ERROR : rom_at::TEXT_SYSEX_ADDRESS_ERROR));
	}

	static void compose_screen(const view &v, const ::xg::voice_rom &voices, lcd_frame &out)
	{
		std::memset(out.text, ' ', sizeof(out.text));
		std::memset(out.cgram, 0, sizeof(out.cgram));
		compose_part_field(v, voices, out);
		compose_icon(v, voices, out);
		compose_segments(v, out);
		switch (v.screen) {
		case screen_play: compose_play(v, voices, out); break;
		case screen_menu: compose_menu(v, voices, out); break;
		case screen_page: compose_page(v, voices, out); break;
		case screen_confirm: compose_confirm(v, voices, out); break;
		case screen_sound_module: compose_sound_module(v, voices, out); break;
		case screen_message: compose_message(v, voices, out); break;
		case screen_performance_name:
			put(out.text[0], 17, voices.text(rom_at::TEXT_PERFORMANCE_NAME));
			out.text[1][2] = '[';
			for (int i = 0; i < 12; i++)
				out.text[1][3 + i] = v.blink_off && i == v.name_cursor ? (v.performance_name[i] == ' ' ? 0x1f : ' ') : v.performance_name[i];
			out.text[1][15] = ']';
			break;
		case screen_performance_copy: {
			char line[20];
			put(out.text[0], 17, voices.text(rom_at::TEXT_PERFORMANCE_COPY));
			std::snprintf(line, sizeof(line), "%s%c%03d%s%c%03d", voices.text(rom_at::TEXT_COPY_FROM).c_str(), char(v.copy_bank == 0 ? 0x1d : 0x1e), v.copy_from + 1, voices.text(rom_at::TEXT_COPY_TO).c_str(), char(0x1e), v.copy_to + 1);
			put(out.text[1], 17, line);
			if (v.blink_off) {
				// 点滅するのは印、番号（元）、番号（先）のどれか
				const int first = v.copy_field == 0 ? 5 : v.copy_field == 1 ? 6 : 14, count = v.copy_field == 0 ? 1 : 3;
				for (int i = 0; i < count; i++)
					out.text[1][first + i] = ' ';
			}
			break;
		}
		case screen_performance_recall: {
			char line[20];
			std::snprintf(line, sizeof(line), "%s%c%03d", voices.text(rom_at::TEXT_RECALL_PERFORMANCE).c_str(), char(v.performance_recall_bank == 0 ? 0x1d : 0x1e), v.performance_recall_number + 1);
			put(out.text[0], 17, line);
			put(out.text[1], 17, voices.text(rom_at::TEXT_ARE_YOU_SURE));
			break;
		}
		case screen_performance_store: {
			char line[20];
			if (v.blink_off) std::snprintf(line, sizeof(line), "%s", voices.text(rom_at::TEXT_STORE_PERFORMANCE).c_str());
			else std::snprintf(line, sizeof(line), "%s%c%03d", voices.text(rom_at::TEXT_STORE_PERFORMANCE).c_str(), char(0x1e), v.store_to + 1);
			put(out.text[0], 17, line);
			put(out.text[1], 17, voices.text(rom_at::TEXT_ARE_YOU_SURE));
			break;
		}
		}
	}

	static constexpr int DUMP_MESSAGE_LIMIT = 128;
	int next_dump_message(const native_engine &engine, u8 *out)
	{
		// 「送ってほしい」の返事が先
		while (m_request_read != m_request_write) {
			const request r = m_requests[m_request_read];
			m_request_read = (m_request_read + 1) % REQUESTS;
			if (const int size = answer_request(engine, r, out))
				return size;
		}
		if (m_screen != screen_message || m_message != MESSAGE_TRANSMITTING || m_dump_wait > 0)
			return 0;
		const dump_plan plan = dump_plan_of(m_confirm, m_confirm_choice);
		const int size = dump_message(engine, plan, m_dump_position, out);
		if (size == 0) {
			message_done();
			return 0;
		}
		m_dump_wait += size * SAMPLES_PER_SECOND / DUMP_BYTES_PER_SECOND;
		if (m_dump_buffered + size > DUMP_BUFFER_BYTES) {
			m_dump_buffered = 0;
			m_dump_wait += (DUMP_INTERVAL_MS[std::clamp<int>(m_settings[SETTING_DUMP_INTERVAL], 0, 4)] + DUMP_REST_MS) * SAMPLES_PER_SECOND / 1000;
		}
		m_dump_buffered += size;
		m_dump_position++;
		m_dump_sent += size;
		m_transmit_cells = std::min(int(s64(m_dump_sent) * DUMP_PROGRESS_CELLS / dump_total_bytes(plan)), DUMP_PROGRESS_CELLS);
		return size;
	}

	// 電源を切っても残る設定（UTIL の SYSTEM・ミュート・A/D の品書きなど）
	static constexpr size_t SAVED_SIZE = sizeof(settings_array) + sizeof(u64) + 1 + 7 + 2 * 2 * sizeof(int) + 2;
	void save(const native_engine &engine, u8 *to) const
	{
		const auto put_bytes = [&](const void *from, size_t size) { std::memcpy(to, from, size); to += size; };
		put_bytes(m_settings.data(), sizeof(settings_array));
		put_bytes(&m_muted, sizeof(u64));
		*to++ = m_muted_ad;
		put_bytes(m_controller_depth.data(), 7);
		put_bytes(m_ad_preset.data(), 2 * sizeof(int));
		put_bytes(m_ad_bank.data(), 2 * sizeof(int));
		*to++ = u8(engine.master_eq_type());
		*to++ = u8(m_part);
	}
	void load(native_engine &engine, const u8 *from)
	{
		const auto get_bytes = [&](void *into, size_t size) { std::memcpy(into, from, size); from += size; };
		get_bytes(m_settings.data(), sizeof(settings_array));
		get_bytes(&m_muted, sizeof(u64));
		m_muted_ad = *from++;
		get_bytes(m_controller_depth.data(), 7);
		get_bytes(m_ad_preset.data(), 2 * sizeof(int));
		get_bytes(m_ad_bank.data(), 2 * sizeof(int));
		engine.set_master_eq_type(*from++);
		m_part = std::min(int(*from++), ALL_PARTS - 1);
		m_solo = false;
		m_external_resets = engine.external_resets();
		m_performance_loads = engine.performance_loads();
		apply_settings(engine);
	}

private:
	// ---- 液晶の文字
	static constexpr u8 CH_RIGHT = 0x10, CH_LEFT = 0x11, CH_END_LEFT = 0x12, CH_ARROW_RIGHT = 0x13, CH_MENU = 0x14, CH_END = 0x15, CH_SILENT_PART = 0x16;
	static constexpr u8 performance_item_transpose = item_variation;
	// 棒の字: 左の棒 a・右の棒 b（0-8 点）で 0x7f + 9a + b。両方 0 は空白
	static constexpr u8 BAR_BASE = 0x7f, BAR_STEP = 9;
	// パンの棒の字: 下の行で、真ん中から左へ何段かを 0-6 で
	static constexpr u8 PAN_BAR_BASE = 0xcf, PAN_BAR_STEP = 7;

	static constexpr u32 BLINK_HALF_PERIOD = 8820;   // 点滅の半周期 0.2 秒
	static constexpr int REPEAT_DELAY = 20727, REPEAT_SLOW_UNTIL = 42000, REPEAT_MIDDLE_UNTIL = 66150;
	static constexpr int REPEAT_INTERVAL_SLOW = 7500, REPEAT_INTERVAL_MIDDLE = 3660, REPEAT_INTERVAL_FAST = 3130;
	static constexpr int DOUBLE_ENTER_WINDOW = 17640;   // ENTER の 2 度押しは 0.4 秒以内
	static constexpr int EXECUTING_SAMPLES = 61740;  // 「Executing...」は 1.4 秒
	static constexpr int COPY_SAMPLES = 67400, STORE_SAMPLES = 69000, FACTORY_SAMPLES = 74970;
	static constexpr u8 MESSAGE_TRANSMITTING = 1, MESSAGE_EXECUTING = 2, MESSAGE_CANCELED = 3, MESSAGE_NO_RECALL = 4, MESSAGE_CARD_FIRST = 5;
	static constexpr int CANCELED_SAMPLES = 55125, NO_RECALL_SAMPLES = 44100;
	static constexpr int METER_STEP_SAMPLES = 1102;  // 25ms ごとに目盛りをなます

	// ---- パートの項目（XG の 08 pp xx の番号）
	static constexpr int BLOCK_CHANNEL = 0x04, BLOCK_PART_MODE = 0x07, BLOCK_NOTE_SHIFT = 0x08, BLOCK_VOLUME = 0x0b, BLOCK_PAN = 0x0e,
	                     BLOCK_CHORUS = 0x12, BLOCK_REVERB = 0x13, BLOCK_VARIATION = 0x14;

	// 棒の刻み: 2 行で 16 段、1 行で 8 段、全パートの面は 7 段
	static constexpr int DIVISOR_TWO_ROWS = 8, DIVISOR_ONE_ROW = 16, DIVISOR_ALL_PARTS = 19;
	static constexpr int NOTE_SHIFT_LOWEST = 0x28;

	// ---- 値の見せ方
	enum value_format : u8 {
		format_signed_center,   // 64 が +00
		format_decimal3,        // 000
		format_decimal2,        // 00
		format_decimal_plain,   // 1 桁などそのまま
		format_receive_channel, // off・A01-D16
		format_detune,          // +00.0
		format_part_mode,
		format_mono_poly,
		format_on_off,
		format_note,            // C-2 ...
		format_bass_frequency,
		format_treble_frequency,
		format_pan,
		format_bend_range,
		format_master_tune,
		format_device,
		format_channel_plus1,
		format_midi_in,
		format_contrast,
		format_dump_interval,
		format_thru_port,
		format_display_bank,
		format_map,
		format_out,
		format_digital_gain,
		format_command,         // 値の無い項目（カード）
		format_ac1_control,     // 00-95、その上が CAT
	};
	struct part_param { ::xg::rom_text field; u8 high, low; value_format format; short lowest, highest; };

#define NP_CENTER(field, high, low) { field, high, low, format_signed_center, 0, 127 }
	static constexpr part_param FILTER_PARAMS[] = {
		NP_CENTER(rom_at::LABELS_PART_SOUND[0], 0x08, 0x18), NP_CENTER(rom_at::LABELS_PART_SOUND[1], 0x08, 0x19), NP_CENTER(rom_at::LABELS_PART_SOUND[2], 0x0a, 0x20),
	};
	static constexpr part_param EG_PARAMS[] = {
		NP_CENTER(rom_at::LABELS_PART_SOUND[3], 0x08, 0x1a), NP_CENTER(rom_at::LABELS_PART_SOUND[4], 0x08, 0x1b), NP_CENTER(rom_at::LABELS_PART_SOUND[5], 0x08, 0x1c),
		NP_CENTER(rom_at::LABELS_PART_SOUND[6], 0x08, 0x69), NP_CENTER(rom_at::LABELS_PART_SOUND[7], 0x08, 0x6a), NP_CENTER(rom_at::LABELS_PART_SOUND[8], 0x08, 0x6b), NP_CENTER(rom_at::LABELS_PART_SOUND[9], 0x08, 0x6c),
	};
	static constexpr part_param EQ_PARAMS[] = {
		{ rom_at::LABELS_PART_SOUND[10], 0x08, 0x76, format_bass_frequency, 4, 40 }, NP_CENTER(rom_at::LABELS_PART_SOUND[11], 0x08, 0x72),
		{ rom_at::LABELS_PART_SOUND[12], 0x08, 0x77, format_treble_frequency, 28, 58 }, NP_CENTER(rom_at::LABELS_PART_SOUND[13], 0x08, 0x73),
	};
	static constexpr part_param VIBRATO_PARAMS[] = {
		NP_CENTER(rom_at::LABELS_PART_OTHERS[0], 0x08, 0x15), NP_CENTER(rom_at::LABELS_PART_OTHERS[1], 0x08, 0x16), NP_CENTER(rom_at::LABELS_PART_OTHERS[2], 0x08, 0x17),
	};
	static constexpr part_param OTHER_PARAMS[] = {
		{ rom_at::LABELS_PART_OTHERS[3], 0x08, 0x04, format_receive_channel, -1, 63 },
		{ rom_at::LABELS_PART_OTHERS[4], 0x08, 0x09, format_detune, 0, 255 },
		{ rom_at::LABELS_PART_OTHERS[5], 0x08, 0x07, format_part_mode, 0, 5 },
		{ rom_at::LABELS_PART_OTHERS[6], 0x08, 0x05, format_mono_poly, 0, 1 },
		{ rom_at::LABELS_PART_OTHERS[7], 0x08, 0x67, format_on_off, 0, 1 },
		{ rom_at::LABELS_PART_OTHERS[8], 0x08, 0x68, format_decimal3, 0, 127 },
		{ rom_at::LABELS_PART_OTHERS[9], 0x08, 0x0f, format_note, 0, 127 },
		{ rom_at::LABELS_PART_OTHERS[10], 0x08, 0x10, format_note, 0, 127 },
		{ rom_at::LABELS_PART_OTHERS[12], 0x08, 0x0c, format_decimal3, 0, 127 },
		{ rom_at::LABELS_PART_OTHERS[13], 0x08, 0x0d, format_decimal3, 0, 127 },
		{ rom_at::LABELS_PART_OTHERS[14], 0x08, 0x6d, format_decimal3, 1, 127 },
		{ rom_at::LABELS_PART_OTHERS[15], 0x08, 0x6e, format_decimal3, 1, 127 },
		{ rom_at::LABELS_PART_OTHERS[16], 0x08, 0x23, format_bend_range, 0x28, 0x58 },
		{ rom_at::LABELS_PART_OTHERS[17], 0x08, 0x20, format_decimal3, 0, 127 },
		{ rom_at::LABELS_PART_OTHERS[18], 0x08, 0x59, format_decimal2, 0, 95 },
		NP_CENTER(rom_at::LABELS_PART_OTHERS[19], 0x08, 0x5b),
		NP_CENTER(rom_at::LABELS_PART_OTHERS[20], 0x08, 0x5c),
	};
	// ドラムのセットアップ 3n rr xx。high は使わず low が xx
	static constexpr part_param DRUM_PARAMS[] = {
		{ rom_at::LABELS_DRUM[0], 0x30, 0x00, format_signed_center, 0, 127 }, { rom_at::LABELS_DRUM[1], 0x30, 0x01, format_signed_center, 0, 127 },
		{ rom_at::LABELS_DRUM[2], 0x30, 0x60, format_signed_center, 0, 127 }, { rom_at::LABELS_DRUM[3], 0x30, 0x02, format_decimal3, 0, 127 },
		{ rom_at::LABELS_DRUM[4], 0x30, 0x04, format_pan, 0, 127 }, { rom_at::LABELS_DRUM[5], 0x30, 0x05, format_decimal3, 0, 127 },
		{ rom_at::LABELS_DRUM[6], 0x30, 0x06, format_decimal3, 0, 127 }, { rom_at::LABELS_DRUM[7], 0x30, 0x07, format_on_off, 0, 1 },
		{ rom_at::LABELS_DRUM[8], 0x30, 0x0b, format_signed_center, 0, 127 }, { rom_at::LABELS_DRUM[9], 0x30, 0x0c, format_signed_center, 0, 127 },
		{ rom_at::LABELS_DRUM[10], 0x30, 0x61, format_signed_center, 0, 127 }, { rom_at::LABELS_DRUM[11], 0x30, 0x50, format_signed_center, 0, 127 },
		{ rom_at::LABELS_DRUM[12], 0x30, 0x24, format_bass_frequency, 4, 40 }, { rom_at::LABELS_DRUM[13], 0x30, 0x20, format_signed_center, 0, 127 },
		{ rom_at::LABELS_DRUM[14], 0x30, 0x25, format_treble_frequency, 28, 58 }, { rom_at::LABELS_DRUM[15], 0x30, 0x21, format_signed_center, 0, 127 },
		{ rom_at::LABELS_DRUM[16], 0x30, 0x0d, format_signed_center, 0, 127 }, { rom_at::LABELS_DRUM[17], 0x30, 0x0e, format_signed_center, 0, 127 },
		{ rom_at::LABELS_DRUM[18], 0x30, 0x0f, format_signed_center, 0, 127 }, { rom_at::LABELS_DRUM[19], 0x30, 0x03, format_decimal_plain, 0, 127 },
	};
	static constexpr part_param SYSTEM_PARAMS[] = {
		{ rom_at::LABELS_SYSTEM[0], 0, 0, format_master_tune, 0, 2047 },
		{ rom_at::LABELS_SYSTEM[1], 0, 1, format_device, 0, 16 },
		{ rom_at::LABELS_SYSTEM[2], 0, 2, format_channel_plus1, 0, 15 },
		{ rom_at::LABELS_SYSTEM[3], 0, 3, format_midi_in, 0, 1 },
		{ rom_at::LABELS_SYSTEM[4], 0, 4, format_note, 13, 91 },
		{ rom_at::LABELS_SYSTEM[5], 0, 5, format_note, 13, 91 },
		{ rom_at::LABELS_SYSTEM[6], 0, 6, format_decimal3, 1, 127 },
		{ rom_at::LABELS_SYSTEM[7], 0, 7, format_on_off, 0, 1 },
		{ rom_at::LABELS_SYSTEM[8], 0, 8, format_on_off, 0, 1 },
		{ rom_at::LABELS_SYSTEM[9], 0, 9, format_on_off, 0, 1 },
		{ rom_at::LABELS_SYSTEM[10], 0, 10, format_on_off, 0, 1 },
		{ rom_at::LABELS_SYSTEM[11], 0, 11, format_on_off, 0, 1 },
		{ rom_at::LABELS_SYSTEM[12], 0, 12, format_on_off, 0, 1 },
		{ rom_at::LABELS_SYSTEM[13], 0, 13, format_contrast, 0, 7 },
		{ rom_at::LABELS_SYSTEM[14], 0, 14, format_dump_interval, 0, 4 },
		{ rom_at::LABELS_SYSTEM[15], 0, 15, format_thru_port, 0, 7 },
		{ rom_at::LABELS_SYSTEM[16], 0, 16, format_display_bank, 0, 1 },
		{ rom_at::LABELS_SYSTEM[17], 0, 17, format_map, 0, 1 },
		{ rom_at::LABELS_SYSTEM[18], 0, 18, format_out, 0, 2 },
		{ rom_at::LABELS_SYSTEM[19], 0, 19, format_digital_gain, 0, 2 },
	};
	static constexpr part_param CARD_ITEMS[] = {
		{ rom_at::LABELS_CARD[0], 0, 0, format_command, 0, 0 }, { rom_at::LABELS_CARD[1], 0, 0, format_command, 0, 0 }, { rom_at::LABELS_CARD[2], 0, 0, format_command, 0, 0 },
		{ rom_at::LABELS_CARD[3], 0, 0, format_command, 0, 0 }, { rom_at::LABELS_CARD[4], 0, 0, format_command, 0, 0 },
	};
#undef NP_CENTER

	// パフォーマンスのパートの OTHERS。受信チャンネルやパートモードは持たない
	static constexpr part_param PERFORMANCE_OTHERS[] = {
		{ rom_at::LABELS_PART_OTHERS[4], 0x08, 0x09, format_detune, 0, 255 },
		{ rom_at::LABELS_PART_OTHERS[6], 0x08, 0x05, format_mono_poly, 0, 1 },
		{ rom_at::LABELS_PART_OTHERS[9], 0x08, 0x0f, format_note, 0, 127 },
		{ rom_at::LABELS_PART_OTHERS[10], 0x08, 0x10, format_note, 0, 127 },
		{ rom_at::LABELS_PART_OTHERS[11], 0x08, 0x11, format_decimal3, 0, 127 },
		{ rom_at::LABELS_PART_OTHERS[12], 0x08, 0x0c, format_decimal3, 0, 127 },
		{ rom_at::LABELS_PART_OTHERS[13], 0x08, 0x0d, format_decimal3, 0, 127 },
		{ rom_at::LABELS_PART_OTHERS[14], 0x08, 0x6d, format_decimal3, 1, 127 },
		{ rom_at::LABELS_PART_OTHERS[15], 0x08, 0x6e, format_decimal3, 1, 127 },
	};

	// パフォーマンスの COMMON。low は記録の中の位置
	static constexpr part_param PERFORMANCE_COMMON[] = {
		{ rom_at::TEXT_PERFORM_NAME_ITEM, 0, 0, format_command, 0, 0 },
		{ rom_at::LABELS_PERFORMANCE_COMMON[0], 0, native_engine::PERF_PORTA_SWITCH, format_on_off, 0, 1 },
		{ rom_at::LABELS_PERFORMANCE_COMMON[1], 0, native_engine::PERF_PORTA_TIME, format_decimal3, 0, 127 },
		{ rom_at::LABELS_PERFORMANCE_COMMON[2], 0, native_engine::PERF_MW_PITCH, format_decimal3, 0, 127 },
		{ rom_at::LABELS_PERFORMANCE_COMMON[3], 0, native_engine::PERF_MW_FILTER, format_decimal3, 0, 127 },
		{ rom_at::LABELS_PERFORMANCE_COMMON[4], 0, native_engine::PERF_BEND, format_bend_range, 0x28, 0x58 },
		{ rom_at::LABELS_PERFORMANCE_COMMON[5], 0, 0x9f, format_on_off, 0, 1 },
		{ rom_at::LABELS_PERFORMANCE_COMMON[6], 0, native_engine::PERF_AC1_CC, format_ac1_control, 0, 0x60 },
		{ rom_at::LABELS_PERFORMANCE_COMMON[7], 0, native_engine::PERF_AC1_FILTER, format_signed_center, 0, 127 },
		{ rom_at::LABELS_PERFORMANCE_COMMON[8], 0, native_engine::PERF_AC1_AMP, format_signed_center, 0, 127 },
		{ rom_at::LABELS_PERFORMANCE_COMMON[9], 0, native_engine::PERF_AC1_FILTER_LFO, format_decimal3, 0, 127 },
	};

	struct param_list { const part_param *items; int count; };
	static param_list params_of(page_id page, bool performance = false)
	{
		if (performance && page == page_others)
			return { PERFORMANCE_OTHERS, int(std::size(PERFORMANCE_OTHERS)) };
		switch (page) {
		case page_filter: return { FILTER_PARAMS, int(std::size(FILTER_PARAMS)) };
		case page_eg: return { EG_PARAMS, int(std::size(EG_PARAMS)) };
		case page_eq: return { EQ_PARAMS, int(std::size(EQ_PARAMS)) };
		case page_vibrato: return { VIBRATO_PARAMS, int(std::size(VIBRATO_PARAMS)) };
		case page_others: return { OTHER_PARAMS, int(std::size(OTHER_PARAMS)) };
		case page_drum: return { DRUM_PARAMS, int(std::size(DRUM_PARAMS)) };
		case page_system: return { SYSTEM_PARAMS, int(std::size(SYSTEM_PARAMS)) };
		case page_card: return { CARD_ITEMS, int(std::size(CARD_ITEMS)) };
		case page_performance_common: return { PERFORMANCE_COMMON, int(std::size(PERFORMANCE_COMMON)) };
		default: return { nullptr, 0 };
		}
	}

	// ---- 品書き
	struct menu_layout { ::xg::rom_text rows[2]; u8 cursor_row[5]; u8 cursor_column[5]; u8 count; };
	static const menu_layout &menu_of(menu_id menu, bool drum, bool performance = false, bool gs = false)
	{
		static constexpr menu_layout EDIT = { { rom_at::MENU_EDIT[0], rom_at::MENU_EDIT[1] }, { 0, 0, 0, 1, 1 }, { 0, 9, 13, 0, 9 }, 5 };
		static constexpr menu_layout EDIT_DRUM = { { rom_at::MENU_EDIT_DRUM[0], rom_at::MENU_EDIT_DRUM[1] }, { 0, 0, 0, 1, 1 }, { 0, 8, 12, 0, 9 }, 5 };
		static constexpr menu_layout UTIL = { { rom_at::MENU_UTIL[0], rom_at::MENU_UTIL[1] }, { 0, 0, 0, 1, 1 }, { 0, 6, 12, 0, 6 }, 5 };
		static constexpr menu_layout DUMP = { { rom_at::MENU_DUMP[0], rom_at::MENU_DUMP[1] }, { 0, 0, 1 }, { 0, 9, 0 }, 3 };
		static constexpr menu_layout DUMP_PERFORMANCE = { { rom_at::MENU_DUMP[1], {} }, { 0 }, { 0 }, 1 };
		static constexpr menu_layout INIT = { { rom_at::MENU_INIT[0], rom_at::MENU_INIT[1] }, { 0, 0, 1 }, { 0, 9, 0 }, 3 };
		static constexpr menu_layout EFFECT = { { rom_at::MENU_EFFECT[0], rom_at::MENU_EFFECT[1] }, { 0, 0, 0, 1, 1 }, { 0, 6, 12, 0, 6 }, 5 };
		static constexpr menu_layout PERFORMANCE = { { rom_at::MENU_PERFORMANCE[0], rom_at::MENU_PERFORMANCE[1] }, { 0, 0, 0, 1, 1 }, { 0, 5, 11, 0, 9 }, 5 };
		static constexpr menu_layout INIT_PERFORMANCE = { { rom_at::MENU_INIT_PERFORMANCE[0], rom_at::MENU_INIT_PERFORMANCE[1] }, { 0, 0 }, { 0, 9 }, 2 };
		static constexpr menu_layout INIT_GS = { { rom_at::MENU_INIT_GS[0], rom_at::MENU_INIT_GS[1] }, { 0, 0, 1 }, { 0, 9, 0 }, 3 };
		static constexpr menu_layout INSERTION = { { rom_at::MENU_INSERTION[0], rom_at::MENU_INSERTION[1] }, { 0, 0, 1, 1 }, { 0, 7, 0, 7 }, 4 };
		switch (menu) {
		case menu_edit: return drum ? EDIT_DRUM : EDIT;
		case menu_util: return UTIL;
		case menu_dump: return performance ? DUMP_PERFORMANCE : DUMP;
		case menu_init: return performance ? INIT_PERFORMANCE : gs ? INIT_GS : INIT;
		case menu_effect: return EFFECT;
		case menu_performance: return PERFORMANCE;
		default: return INSERTION;
		}
	}

	// ---- 目盛り
	void meter_tick(native_engine &engine, int frames)
	{
		m_meter_samples += frames;
		if (m_meter_samples < METER_STEP_SAMPLES)
			return;
		m_meter_samples %= METER_STEP_SAMPLES;
		u8 now[MIDI_PARTS];
		engine.fill_meter(now, MIDI_PARTS);
		for (int part = 0; part < MIDI_PARTS; part++) {
			// 25ms ごとに半分ずつ寄せる
			const int current = m_meter_smooth[part];
			m_meter_smooth[part] = u8(current + (now[part] - current) / 2);
		}
	}

	bool bars_show_meter() const { return m_screen == screen_play && m_item == item_voice; }


	static constexpr u8 MODEL_XG = 0x4c, MODEL_MU = 0x49, MODEL_PERFORMANCE = 0x59;
	static constexpr int DUMP_LOWS = 4;
	struct dump_group { u8 model, high, highs, middle, middles, lows; u8 low[DUMP_LOWS]; u8 size[DUMP_LOWS]; };
	static constexpr int DUMP_GROUP_LIMIT = 28;
	struct dump_plan { dump_group groups[DUMP_GROUP_LIMIT]; int count = 0; };
	static constexpr int AUDITION_VELOCITY_BOOST = 16, MU_ADDRESS_MAP = 0x12;
	enum sysex_error_id { sysex_error_none, sysex_error_data, sysex_error_address, sysex_error_checksum };
	static constexpr int KEY_LOCK_SAMPLES = 66150, KEY_LOCK_MU_SAMPLES = 88200;
	static constexpr int OUT_AD_DIRECT = 1, MAP_NOT_APPLICABLE = 2;
	static constexpr int DUMP_PART_CHOICES[] = { 64, 32, 16, 64, 32, 16 };
	static constexpr int DUMP_FIRST_AD_CHOICE = 3;
	static constexpr int DUMP_BUFFER_BYTES = 512, DUMP_BYTES_PER_SECOND = 3125, DUMP_REST_MS = 90;
	static constexpr int SAMPLES_PER_SECOND = 44100;
	static constexpr short DUMP_INTERVAL_MS[] = { 50, 100, 150, 200, 300 };
	static constexpr int DUMP_PROGRESS_CELLS = 8;
	static constexpr int DUMP_MESSAGE_OVERHEAD = 11;

	static dump_plan dump_plan_of(confirm_id confirm, int choice)
	{
		dump_plan plan;
		const auto add = [&](u8 model, u8 high, u8 highs, u8 middle, u8 middles, std::initializer_list<std::pair<u8, u8>> items) {
			dump_group &g = plan.groups[plan.count++];
			g = { model, high, highs, middle, middles, u8(items.size()), {}, {} };
			int i = 0;
			for (const auto &item : items) {
				g.low[i] = item.first;
				g.size[i++] = item.second;
			}
		};
		// 1 つのパフォーマンスだけのときは、その記録だけ
		if (!(confirm == confirm_dump_perform && choice > 0))
			add(MODEL_XG, 0x00, 1, 0x00, 1, { { 0x00, 7 } });
		const auto performances = [&](u8 number, u8 count) {
			add(MODEL_PERFORMANCE, 0x30, 4, number, count, { { 0x00, 34 }, { 0x30, 2 } });
			add(MODEL_PERFORMANCE, 0x40, 1, number, count, { { 0x00, 24 }, { 0x20, 40 }, { 0x70, 6 } });
			add(MODEL_PERFORMANCE, 0x50, 2, number, count, { { 0x00, 15 } });
			add(MODEL_PERFORMANCE, 0x60, 3, number, count, { { 0x00, 100 } });
		};
		const auto settings = [&] { add(MODEL_MU, 0x00, 1, 0x00, 1, { { 0x00, 10 }, { 0x10, 1 }, { 0x12, 1 }, { 0x14, 2 } }); };
		if (confirm == confirm_dump_perform) {
			if (choice > 0) {
				performances(u8(choice - 1), 1);
				return plan;
			}
			settings();
			performances(0, native_engine::PERFORMANCES);
			return plan;
		}
		const int parts = confirm == confirm_dump_multi ? DUMP_PART_CHOICES[choice] : MIDI_PARTS;
		add(MODEL_XG, 0x02, 1, 0x01, 1, { { 0x00, 14 }, { 0x10, 6 }, { 0x20, 15 }, { 0x30, 6 } });
		add(MODEL_XG, 0x02, 1, 0x01, 1, { { 0x40, 33 }, { 0x70, 6 } });
		add(MODEL_XG, 0x02, 1, 0x40, 1, { { 0x00, 21 } });
		add(MODEL_XG, 0x03, 1, 0x00, 4, { { 0x00, 18 }, { 0x20, 6 }, { 0x30, 20 } });
		add(MODEL_XG, 0x08, 1, 0x00, u8(parts), { { 0x00, 41 }, { 0x30, 63 }, { 0x70, 4 }, { 0x74, 12 } });
		add(MODEL_XG, 0x0a, 1, 0x00, u8(parts), { { 0x20, 2 } });
		add(MODEL_XG, 0x11, 1, 0x00, 1, { { 0x00, 1 } });
		if (confirm == confirm_dump_all || choice >= DUMP_FIRST_AD_CHOICE)
			add(MODEL_XG, 0x10, 1, 0x00, AD_PARTS, { { 0x00, 21 }, { 0x30, 49 } });
		add(MODEL_XG, 0x30, u8(::xg::settings::DRUM_SETUP_SETS), u8(::xg::settings::DRUM_SETUP_NOTE0), u8(::xg::settings::DRUM_SETUP_NOTES), { { 0x00, 16 }, { 0x20, 14 }, { 0x50, 2 }, { 0x60, 2 } });
		if (confirm == confirm_dump_all)
			performances(0, native_engine::PERFORMANCES);
		settings();
		return plan;
	}

	static int dump_total_bytes(const dump_plan &plan)
	{
		int total = 0;
		for (int i = 0; i < plan.count; i++) {
			const dump_group &g = plan.groups[i];
			for (int low = 0; low < g.lows; low++)
				total += g.highs * g.middles * (g.size[low] + DUMP_MESSAGE_OVERHEAD);
		}
		return total;
	}

	// コントローラの深さ 5 つ（MW・ベンド・CAT・AC1・AC2）。パネルで動かせるのは AC1 だけ
	void dump_controllers(int block, u8 *data) const
	{
		std::fill_n(data, 5, u8(0x40));
		data[3] = m_controller_depth[size_t(block)];
	}

	void dump_read_effect(const native_engine &engine, u8 middle, u8 low, u8 *data) const
	{
		using E = native_engine;
		const auto type_of = [&](int type) { data[0] = u8(type >> 7); data[1] = u8(type & 0x7f); };
		const auto values = [&](int block, int number_first, int count, int at) {
			for (int i = 0; i < count; i++)
				data[at + i] = u8(engine.effect_value(block, number_first + i));
		};
		const auto raw = [&](int block, int address_first, int count, int at) {
			for (int i = 0; i < count; i++)
				data[at + i] = u8(engine.effect_value(block, 0x100 | (address_first + i)));
		};
		const auto controllers = [&](int block, int at) { dump_controllers(block, data + at); };
		if (middle == 0x40) {
			data[0] = u8(eq_type_now(engine));
			values(E::block_master_eq, 1, 20, 1);
		} else if (low == 0x00) {
			type_of(engine.reverb_type());
			values(E::block_reverb, 1, 10, 2);
			raw(E::block_reverb, 0x0c, 2, 12);
			if (!engine.reverb_type_written())
				data[11] = 0x7f;
		} else if (low == 0x10)
			values(E::block_reverb, 11, 6, 0);
		else if (low == 0x20) {
			type_of(engine.chorus_type());
			values(E::block_chorus, 1, 10, 2);
			raw(E::block_chorus, 0x2c, 3, 12);
			if (!engine.chorus_type_written())
				data[11] = 0x7f;
		} else if (low == 0x30) {
			values(E::block_chorus, 11, 6, 0);
			if (!engine.chorus_type_written()) {
				data[0] = 0x30;
				data[2] = 0x14;
			}
		}
		else if (low == 0x40) {
			type_of(engine.variation_type());
			for (int n = 0; n < 10; n++) {
				const int value = engine.effect_value(E::block_variation, n + 1);
				data[2 + n * 2] = u8((value >> 7) & 0x7f);
				data[3 + n * 2] = u8(value & 0x7f);
			}
			raw(E::block_variation, 0x56, 6, 22);
			controllers(E::block_variation, 28);
		} else
			values(E::block_variation, 11, 6, 0);
	}

	void dump_read_insertion(const native_engine &engine, int n, u8 low, u8 *data) const
	{
		const int block = native_engine::block_insertion_1 + n;
		if (low == 0x00) {
			const int type = engine.effect_type(block);
			data[0] = u8(type >> 7);
			data[1] = u8(type & 0x7f);
			for (int i = 0; i < 10; i++)
				data[2 + i] = u8(engine.effect_value(block, i + 1));
			data[12] = u8(engine.effect_value(block, 0x100 | 0x0c));
			dump_controllers(block, data + 13);
		} else if (low == 0x20) {
			for (int i = 0; i < 6; i++)
				data[i] = u8(engine.effect_value(block, 11 + i));
		} else {
			for (int n_wide = 0; n_wide < 10; n_wide++) {
				const int value = engine.effect_value(block, n_wide + 1);
				data[n_wide * 2] = u8((value >> 7) & 0x7f);
				data[1 + n_wide * 2] = u8(value & 0x7f);
			}
		}
	}

	// 機種 49: UTIL の設定
	void dump_read_settings(const native_engine &engine, u8 low, u8 *data) const
	{
		if (low == 0x00) {
			static constexpr u8 FIELDS[] = { SETTING_MUTE_LOCK, SETTING_AD_LOCK, SETTING_EQ_LOCK, SETTING_RECEIVE_SYSTEM_ON, SETTING_RECEIVE_BANK, SETTING_DUMP_INTERVAL, SETTING_PERFORMANCE_CHANNEL };
			for (size_t i = 0; i < std::size(FIELDS); i++)
				data[i] = u8(m_settings[FIELDS[i]]);
			data[7] = 0x40;
			data[8] = u8(m_settings[SETTING_CONTRAST]);
			data[9] = u8(m_settings[SETTING_THRU_PORT]);
		} else if (low == 0x10)
			data[0] = 1;
		else if (low == 0x12)
			data[0] = u8(engine.voice_set());
		else {
			data[0] = u8(m_settings[SETTING_OUT]);
			data[1] = u8(m_settings[SETTING_DIGITAL_GAIN]);
		}
	}

	// 機種 59: 中位がパフォーマンスの番号（Int の側）
	static void dump_read_performance(const native_engine &engine, u8 high, u8 number, u8 low, u8 size, u8 *data)
	{
		const u8 *record = engine.stored_performance(1, number);
		if (high < 0x40)
			std::copy_n(record + (high - 0x30) * native_engine::PERFORMANCE_PART_SIZE + (low == 0 ? 0 : 34), size, data);
		else if (high == 0x40)
			std::copy_n(record + (low == 0x00 ? 0x90 : low == 0x20 ? 0xa8 : 0xd0), size, data);
		else if (high < 0x60)
			std::copy_n(record + (high == 0x50 ? 0xd6 : 0xe5), size, data);
		else {
			std::fill_n(data, size, u8(0));
			std::fill_n(data, 3, u8(0x7f));
		}
	}

	static u8 dump_part_value(const native_engine &engine, int part, u8 high, u8 low)
	{
		if (const u8 *value = engine.part_settings(part).setting(high, low))
			return *value;
		if (high == 0x0a)
			return 0x40;
		switch (low) {
		case 0x40: return 0x01;
		case 0x70: return 0x3e;
		case 0x74: case 0x75: case 0x71: return 0x40;
		case 0x78: return 0x22;
		case 0x79: return 0x2e;
		case 0x7a: case 0x7b: case 0x7c: case 0x7d: return 0x07;
		default: return 0x00;
		}
	}

	static u8 dump_drum_value(const native_engine &engine, int set, int key, int low)
	{
		if (::xg::settings::drum_setup_index(low) >= 0)
			return u8(engine.drum_setup_value(set, key, low));
		switch (low) {
		case 0x22: case 0x23: case 0x51: return 0x40;
		case 0x26: return 0x22;
		case 0x27: return 0x2e;
		case 0x28: case 0x29: case 0x2a: case 0x2b: return 0x07;
		default: return 0x00;
		}
	}

	void dump_read(const native_engine &engine, const dump_group &g, u8 high, u8 middle, u8 low, u8 size, u8 *data) const
	{
		std::fill_n(data, size, u8(0));
		if (g.model == MODEL_MU) {
			dump_read_settings(engine, low, data);
			return;
		}
		if (g.model == MODEL_PERFORMANCE) {
			dump_read_performance(engine, high, middle, low, size, data);
			return;
		}
		switch (high) {
		case 0x00:
			for (int i = 0; i < size; i++)
				data[i] = u8(engine.system_value(i));
			break;
		case 0x02: dump_read_effect(engine, middle, low, data); break;
		case 0x03: dump_read_insertion(engine, middle, low, data); break;
		case 0x08: case 0x0a:
			for (int i = 0; i < size; i++)
				data[i] = dump_part_value(engine, middle, high, u8(low + i));
			break;
		case 0x10: engine.ad_block(middle, low, size, data); break;
		case 0x11: break;
		default:
			for (int i = 0; i < size; i++)
				data[i] = dump_drum_value(engine, high - 0x30, middle, low + i);
			break;
		}
	}

	static int bulk(u8 *out, u8 model, int high, int middle, int low, const u8 *data, int count)
	{
		int sum = (count >> 7) + (count & 0x7f) + high + middle + low;
		int at = 0;
		out[at++] = 0xf0; out[at++] = 0x43; out[at++] = 0x00; out[at++] = model;
		out[at++] = u8(count >> 7); out[at++] = u8(count & 0x7f);
		out[at++] = u8(high); out[at++] = u8(middle); out[at++] = u8(low);
		for (int i = 0; i < count; i++) {
			out[at++] = u8(data[i] & 0x7f);
			sum += data[i] & 0x7f;
		}
		out[at++] = u8((128 - (sum & 0x7f)) & 0x7f);
		out[at++] = 0xf7;
		return at;
	}

	// position 番目のメッセージ。無ければ 0
	int dump_message(const native_engine &engine, const dump_plan &plan, int position, u8 *out) const
	{
		for (int i = 0; i < plan.count; i++) {
			const dump_group &g = plan.groups[i];
			const int messages = g.highs * g.middles * g.lows;
			if (position >= messages) {
				position -= messages;
				continue;
			}
			const u8 low = g.low[position % g.lows], size = g.size[position % g.lows];
			const int rest = position / g.lows;
			const u8 middle = u8(g.middle + rest % g.middles), high = u8(g.high + rest / g.middles);
			u8 data[DUMP_MESSAGE_LIMIT] = {};
			dump_read(engine, g, high, middle, low, size, data);
			return bulk(out, g.model, high, middle, low, data, size);
		}
		return 0;
	}

	// ---- 「送ってほしい」への返事
	struct request { bool bulk; u8 model, high, middle, low; };
	static constexpr size_t REQUESTS = 32;
	u8 reply_device() const { return u8(m_settings[SETTING_DEVICE] < 16 ? m_settings[SETTING_DEVICE] : 0); }

	int answer_request(const native_engine &engine, const request &r, u8 *out) const
	{
		if (r.model == MODEL_PERFORMANCE && !r.bulk && (r.high == 0x09 || r.high == 0x0b)) {
			const int value = performance_value(engine, r);
			return value < 0 ? 0 : value_message(out, r, value);
		}
		const dump_plan plan = dump_plan_of(confirm_dump_all, 0);
		for (int i = 0; i < plan.count; i++) {
			const dump_group &g = plan.groups[i];
			if (g.model != r.model || r.high < g.high || r.high >= g.high + g.highs || r.middle < g.middle || r.middle >= g.middle + g.middles)
				continue;
			for (int section = 0; section < g.lows; section++) {
				const int offset = r.low - g.low[section];
				if (offset < 0 || offset >= g.size[section] || (r.bulk && offset != 0))
					continue;
				u8 data[DUMP_MESSAGE_LIMIT] = {};
				dump_read(engine, g, r.high, r.middle, g.low[section], g.size[section], data);
				if (r.bulk) {
					const int size = bulk(out, g.model, r.high, r.middle, r.low, data, g.size[section]);
					out[2] = reply_device();
					return size;
				}
				return value_message(out, r, data[offset]);
			}
		}
		return 0;
	}

	int value_message(u8 *out, const request &r, int value) const
	{
		const u8 message[] = { 0xf0, 0x43, u8(0x10 | reply_device()), r.model, r.high, r.middle, r.low, u8(value & 0x7f), 0xf7 };
		std::copy(std::begin(message), std::end(message), out);
		return int(std::size(message));
	}

	static int performance_value(const native_engine &engine, const request &r)
	{
		const u8 *record = engine.performance_record();
		if (r.high == 0x09) {
			if (r.middle >= native_engine::PERFORMANCE_PARTS || r.low == 0x09 || r.low > 0x25 || r.low == 0x18)
				return -1;
			const int offset = r.low <= 0x08 ? r.low : r.low <= 0x16 ? r.low - 1 : r.low - 2;
			return record[r.middle * native_engine::PERFORMANCE_PART_SIZE + offset];
		}
		const int offset = r.low <= 0x17 ? native_engine::PERF_NAME + r.low : (r.low >= 0x20 ? 0x88 + r.low : -1);
		return r.middle == 0 && offset >= 0 && offset < native_engine::PERFORMANCE_SIZE ? record[offset] : -1;
	}

	void take_notices(native_engine &engine)
	{
		native_engine::panel_notice notice;
		while (engine.take_notice(notice)) {
			switch (notice.kind) {
			case native_engine::notice_data_error: show_sysex_error(sysex_error_data); break;
			case native_engine::notice_address_error: show_sysex_error(sysex_error_address); break;
			case native_engine::notice_checksum:
				m_error_checksum = notice.a;
				show_sysex_error(sysex_error_checksum);
				break;
			case native_engine::notice_bulk:
				m_key_lock = notice.a == MODEL_MU ? KEY_LOCK_MU_SAMPLES : KEY_LOCK_SAMPLES;
				break;
			case native_engine::notice_setting: receive_setting(engine, notice.a, notice.b); break;
			case native_engine::notice_request_bulk:
			case native_engine::notice_request_value: {
				const size_t next = (m_request_write + 1) % REQUESTS;
				if (next != m_request_read) {
					m_requests[m_request_write] = { notice.kind == native_engine::notice_request_bulk, notice.a, notice.b, notice.c, notice.d };
					m_request_write = next;
				}
				break;
			}
			}
		}
	}

	void show_sysex_error(int error)
	{
		// 送信中の画面には出ない
		if (m_screen == screen_message && m_message == MESSAGE_TRANSMITTING)
			return;
		m_sysex_error = error;
	}

	void receive_setting(native_engine &engine, int address, int value)
	{
		if (address == MU_ADDRESS_MAP) {
			if (!engine.performance_mode()) {
				m_settings[SETTING_MAP] = short(value != 0);
				apply_settings(engine);
			}
			return;
		}
		for (int item = 1; item < int(std::size(SETTING_ADDRESS)); item++) {
			if (SETTING_ADDRESS[item] != address)
				continue;
			m_settings[size_t(item)] = short(std::clamp(value, int(SYSTEM_PARAMS[item].lowest), int(SYSTEM_PARAMS[item].highest)));
			apply_settings(engine);
			if (item == SETTING_PERFORMANCE_CHANNEL)
				engine.set_performance_channel(m_settings[size_t(item)]);
			return;
		}
	}

	// ---- パートの値
	static u8 part_value(const native_engine &engine, int part, u8 item)
	{
		if (part >= MIDI_PARTS) {
			const int which = part - MIDI_PARTS;
			switch (item) {
			case item_volume: return u8(engine.ad_value(which, BLOCK_VOLUME));
			case item_expression: return u8(engine.ad_value(which, native_engine::AD_EXPRESSION));
			case item_pan: return u8(engine.ad_value(which, BLOCK_PAN));
			case item_reverb: return u8(engine.ad_value(which, BLOCK_REVERB));
			case item_chorus: return u8(engine.ad_value(which, BLOCK_CHORUS));
			case item_variation:
				if (!engine.variation_is_system())
					return engine.effect_value(native_engine::block_variation, FX_VARIATION_PART) == part ? 127 : 0;
				return u8(engine.ad_value(which, BLOCK_VARIATION));
			default: return 0x40;
			}
		}
		if (item == item_variation && !engine.variation_is_system())
			return engine.effect_value(native_engine::block_variation, FX_VARIATION_PART) == part ? 127 : 0;
		const ::xg::part_state &block = engine.part_settings(part);
		switch (item) {
		case item_volume: return block[BLOCK_VOLUME];
		case item_expression: return block.expression;
		case item_pan: return block[BLOCK_PAN];
		case item_reverb: return block[BLOCK_REVERB];
		case item_chorus: return block[BLOCK_CHORUS];
		case item_variation: return block[BLOCK_VARIATION];
		default: return block[BLOCK_NOTE_SHIFT];
		}
	}

	static u8 part_setting(const ::xg::part_state &block, u8 high, u8 low)
	{
		const u8 *value = block.setting(high, low);
		return value ? *value : block[0];
	}

	u8 bar_value(const native_engine &engine, int part) const
	{
		if (m_screen == screen_play && engine.performance_mode())
			return part < native_engine::PERFORMANCE_PARTS ? performance_part_item(engine, part, m_item) : 0;
		if (m_screen == screen_play)
			return part_value(engine, part, m_item);
		if (m_screen == screen_page && part < MIDI_PARTS) {
			const param_list list = params_of(m_page, engine.performance_mode());
			if (list.items && m_page <= page_others) {
				// 項目の範囲を 8 段に割る。bar_level が 16 で割るので 128 の目盛りに直す
				const part_param &p = list.items[m_page_item[m_page]];
				const ::xg::part_state &block = engine.part_settings(part);
				int value = p.format == format_detune
				          ? (part_setting(block, 0x08, 0x09) & 0x0f) << 4 | (part_setting(block, 0x08, 0x0a) & 0x0f)
				          : part_setting(block, p.high, p.low);
				// 受信チャンネルは口の中の 16 チャンネルぶんの目盛り
				if (p.format == format_receive_channel)
					return value >= MIDI_PARTS ? 0 : u8((value % 16) * 8);
				// 周波数の項目は 5 目ごとに 1 段
				if (p.format == format_bass_frequency || p.format == format_treble_frequency)
					return u8((value - p.lowest) / 5 * DIVISOR_ONE_ROW);
				return u8((value - p.lowest) * 128 / (p.highest - p.lowest + 1));
			}
		}
		return 0;
	}

	int page_value(const native_engine &engine) const
	{
		const int item = m_page_item[m_page];
		switch (m_page) {
		case page_filter: case page_eg: case page_eq: case page_vibrato: case page_others: {
			if (m_part >= MIDI_PARTS)
				return -1;
			const part_param &p = params_of(m_page, engine.performance_mode()).items[item];
			const ::xg::part_state &block = engine.part_settings(m_part);
			if (p.format == format_detune)
				return (part_setting(block, 0x08, 0x09) & 0x0f) << 4 | (part_setting(block, 0x08, 0x0a) & 0x0f);
			const int raw = part_setting(block, p.high, p.low);
			if (p.format == format_receive_channel && raw >= MIDI_PARTS)
				return -1;
			return raw;
		}
		case page_drum:
			return drum_set_of(engine) < 0 ? engine.drum_kit_value(m_part, m_drum_key, DRUM_PARAMS[item].low)
			                               : engine.drum_setup_value(drum_set_of(engine), m_drum_key, DRUM_PARAMS[item].low);
		case page_system:
			if (item == 0)
				return engine.master_tune();
			if (item == SETTING_MAP && engine.performance_mode())
				return MAP_NOT_APPLICABLE;
			return m_settings[size_t(item)];
		case page_card:
			return 0;
		case page_reverb: case page_chorus: case page_variation: case page_insertion:
			if (item == 0)
				return effect_type_index(engine);
			return fx_item_value(engine, item - 1);
		case page_master_eq:
			return item == 0 ? eq_type_now(engine) : engine.effect_value(native_engine::block_master_eq, 1 + (item - 1) * 4);
		case page_performance_common:
			return item == 0 ? 0 : engine.performance_record()[PERFORMANCE_COMMON[item].low];
		default:
			return 0;
		}
	}

	int drum_set_of(const native_engine &engine) const
	{
		const int mode = engine.part_settings(m_part)[BLOCK_PART_MODE];
		return mode >= 2 ? mode - 2 : -1;
	}

	// ENTER で見せる SysEx の中身
	void fill_exclusive(const native_engine &engine, view &v) const
	{
		const int item = m_page_item[m_page];
		const auto set = [&](int high, int middle, int low) {
			v.exclusive_address[0] = u8(high);
			v.exclusive_address[1] = u8(middle);
			v.exclusive_address[2] = u8(low);
		};
		const auto one = [&](int value) {
			v.exclusive_data[0] = u8(value);
			v.exclusive_count = 1;
		};
		switch (m_page) {
		case page_filter: case page_eg: case page_eq: case page_vibrato: case page_others: {
			const part_param &p = params_of(m_page, engine.performance_mode()).items[item];
			if (engine.performance_mode()) {
				v.exclusive_model_performance = true;
				const int offset = native_engine::performance_part_offset(p.high, p.low);
				set(0x09, m_part, p.format == format_detune ? 0x12 : p.format == format_mono_poly ? 0x18 : native_engine::performance_part_address(offset));
			} else
				set(p.high, m_part, p.low);
			if (p.format == format_detune) {
				v.exclusive_data[0] = u8(v.value >> 4);
				v.exclusive_data[1] = u8(v.value & 0x0f);
				v.exclusive_count = 2;
			} else
				one(v.value < 0 ? 127 : v.value);
			break;
		}
		case page_drum:
			if (drum_set_of(engine) < 0)
				break;
			set(0x30 | drum_set_of(engine), m_drum_key, DRUM_PARAMS[item].low);
			one(v.value);
			break;
		case page_performance_common:
			if (item > 0) {
				v.exclusive_model_performance = true;
				set(0x0b, 0x00, native_engine::performance_common_address(PERFORMANCE_COMMON[item].low));
				one(v.value);
			}
			break;
		case page_system:
			if (item == 0) {
				set(0, 0, 0);
				for (int i = 0; i < 4; i++)
					v.exclusive_data[i] = u8((v.value >> (12 - i * 4)) & 0x0f);
				v.exclusive_count = 4;
			} else if (const int address = SETTING_ADDRESS[item]; address >= 0 && !(item == SETTING_MAP && engine.performance_mode())) {
				v.exclusive_model_mu = true;
				set(0, 0, address);
				one(item == SETTING_MAP ? engine.voice_set() : v.value);
			}
			break;
		case page_master_eq:
			if (item > 0) {
				set(0x02, 0x40, 1 + (item - 1) * 4);
				one(v.value);
			}
			break;
		case page_reverb: case page_chorus: case page_variation: case page_insertion:
			fill_fx_exclusive(engine, v, item);
			break;
		default:
			break;
		}
	}

	// パフォーマンスの記録の中の位置。無ければ負
	int performance_effect_offset(const panel_fx::param *param, const panel_fx::tail_item *tail) const
	{
		const bool ins = m_page == page_insertion;
		const int base = ins ? (m_fx_block == 0 ? 0xd6 : 0xe5) : 0;
		if (tail) {
			if (m_page == page_reverb) return 0xb0;
			if (m_page == page_chorus) return tail->address == (0x100 | 0x2d) ? 0xb9 : 0xba;
			if (m_page == page_variation) {
				switch (tail->address & 0x7f) {
				case 0x57: return 0xca;
				case 0x58: return 0xcb;
				case 0x59: return 0xcc;
				case 0x5a: return 0xce;
				case 0x5b: return 0xcf;
				default: return 0xcd;
				}
			}
			return base + ((tail->address & 0x7f) == 0x0c ? 0x0e : 0x0d);
		}
		if (!param)                  // 種類
			return m_page == page_reverb ? 0xa8 : m_page == page_chorus ? 0xb1 : m_page == page_variation ? 0xbb : base;
		const int number = param->address;
		switch (m_page) {
		case page_reverb: return 0xaa + number - 1;
		case page_chorus: return 0xb3 + number - 1;
		case page_variation: return number == 10 ? 0xc7 : 0xbd + (number - 1) * 2;
		default: return number == 10 ? base + 0x0c : base + 2 + (number - 1) * 2;
		}
	}

	void fill_fx_exclusive(const native_engine &engine, view &v, int item) const
	{
		if (engine.performance_mode()) {
			fill_performance_fx_exclusive(engine, v, item);
			return;
		}
		const int block = fx_block_of(m_page);
		const bool insertion = m_page == page_insertion;
		const int high = insertion ? 0x03 : 0x02, middle = insertion ? m_fx_block : 0x01;
		v.exclusive_address[0] = u8(high);
		v.exclusive_address[1] = u8(middle);
		if (item == 0) {
			const int type = effect_type_now(engine);
			v.exclusive_address[2] = u8(m_page == page_reverb ? 0x00 : m_page == page_chorus ? 0x20 : m_page == page_variation ? 0x40 : 0x00);
			v.exclusive_data[0] = u8(type >> 7);
			v.exclusive_data[1] = u8(type & 0x7f);
			v.exclusive_count = 2;
			return;
		}
		const panel_fx::type_table table = panel_fx::find(fx_table_of(m_page), effect_type_now(engine));
		const bool performance = engine.performance_mode();
		const int shown = visible_count(table, performance, fx_table_of(m_page));
		const int index = item - 1;
		if (index < shown) {
			const panel_fx::param &p = visible_param(table, index, performance, fx_table_of(m_page));
			const int number = p.address;
			v.exclusive_address[2] = u8(native_engine::effect_param_address(block, number, p.wide));
			if (p.wide || (m_page == page_variation && number <= 10)) {
				v.exclusive_data[0] = u8(v.value >> 7);
				v.exclusive_data[1] = u8(v.value & 0x7f);
				v.exclusive_count = 2;
			} else {
				v.exclusive_data[0] = u8(v.value);
				v.exclusive_count = 1;
			}
			return;
		}
		const panel_fx::tail_list tail = panel_fx::tail_of(fx_table_of(m_page), engine.variation_is_system());
		v.exclusive_address[2] = u8(tail.items[index - shown].address & 0x7f);
		v.exclusive_data[0] = u8(v.value);
		v.exclusive_count = 1;
	}

	// パフォーマンスの記録にあるのはパラメータ 1-5 と 10 だけで、ページにもそれしか出ない
	static bool fx_param_visible(const panel_fx::param &p, bool performance, panel_fx::table_id table)
	{
		if (!performance)
			return true;
		return p.address <= 5 || (p.address == 10 && (table == panel_fx::table_variation || table == panel_fx::table_insertion));
	}
	static int visible_count(const panel_fx::type_table &t, bool performance, panel_fx::table_id table)
	{
		int count = 0;
		for (int i = 0; i < t.count; i++)
			count += fx_param_visible(t.params[i], performance, table);
		return count;
	}
	static const panel_fx::param &visible_param(const panel_fx::type_table &t, int nth, bool performance, panel_fx::table_id table)
	{
		for (int i = 0; i < t.count; i++)
			if (fx_param_visible(t.params[i], performance, table) && nth-- == 0)
				return t.params[i];
		return t.params[0];
	}

	void fill_performance_fx_exclusive(const native_engine &engine, view &v, int item) const
	{
		const panel_fx::type_table table = panel_fx::find(fx_table_of(m_page), effect_type_now(engine));
		const int shown = visible_count(table, true, fx_table_of(m_page));
		const panel_fx::tail_list tail = panel_fx::tail_of(fx_table_of(m_page), engine.variation_is_system());
		int offset, count = 1;
		if (item == 0) {
			offset = performance_effect_offset(nullptr, nullptr);
			count = 2;
		} else if (item - 1 < shown) {
			const panel_fx::param &p = visible_param(table, item - 1, true, fx_table_of(m_page));
			offset = performance_effect_offset(&p, nullptr);
			count = m_page == page_variation || m_page == page_insertion ? 2 : 1;
		} else
			offset = performance_effect_offset(nullptr, &tail.items[item - 1 - shown]);
		v.exclusive_model_performance = true;
		v.exclusive_address[0] = 0x0b;
		v.exclusive_address[1] = 0x00;
		v.exclusive_address[2] = u8(offset - PERFORMANCE_ADDRESS_BASE);
		const u8 *record = engine.performance_record();
		for (int i = 0; i < count; i++)
			v.exclusive_data[i] = record[offset + i];
		v.exclusive_count = u8(count);
	}

	// ---- エフェクトのページ
	static bool fx_page(page_id page) { return page == page_reverb || page == page_chorus || page == page_variation || page == page_insertion; }

	int fx_block_of(page_id page) const
	{
		switch (page) {
		case page_reverb: return native_engine::block_reverb;
		case page_chorus: return native_engine::block_chorus;
		case page_variation: return native_engine::block_variation;
		default: return native_engine::block_insertion_1 + m_fx_block;
		}
	}

	int effect_type_now(const native_engine &engine) const { return engine.effect_type(fx_block_of(m_page)); }

	int effect_part_now(const native_engine &engine) const
	{
		if (m_page == page_variation)
			return engine.effect_value(native_engine::block_variation, FX_VARIATION_PART);
		if (m_page == page_insertion)
			return engine.effect_value(fx_block_of(m_page), FX_INSERTION_PART);
		return -1;
	}

	const std::vector<::xg::fx_type> &types_of(page_id page) const
	{
		return page == page_reverb ? ::xg::rev_types() : page == page_chorus ? ::xg::cho_types() : ::xg::ins_types();
	}

	int effect_type_index(const native_engine &engine) const
	{
		const std::vector<::xg::fx_type> &types = types_of(m_page);
		const int current = effect_type_now(engine);
		for (size_t i = 0; i < types.size(); i++)
			if ((types[i].msb << 7 | types[i].lsb) == current)
				return int(i);
		return 0;
	}

	int fx_item_value(const native_engine &engine, int index) const
	{
		const panel_fx::type_table table = panel_fx::find(fx_table_of(m_page), effect_type_now(engine));
		const bool performance = engine.performance_mode();
		const int count = visible_count(table, performance, fx_table_of(m_page));
		if (index < count)
			return engine.effect_value(fx_block_of(m_page), visible_param(table, index, performance, fx_table_of(m_page)).address);
		const panel_fx::tail_list tail = panel_fx::tail_of(fx_table_of(m_page), engine.variation_is_system());
		const panel_fx::tail_item &t = tail.items[index - count];
		if (t.format == panel_fx::tail_center)
			return m_controller_depth[size_t(fx_block_of(m_page))];
		return engine.effect_value(fx_block_of(m_page), t.address);
	}

	static panel_fx::table_id fx_table_of(page_id page)
	{
		return page == page_reverb ? panel_fx::table_reverb : page == page_chorus ? panel_fx::table_chorus : page == page_variation ? panel_fx::table_variation : panel_fx::table_insertion;
	}

	int fx_item_count(const native_engine &engine) const
	{
		const panel_fx::type_table table = panel_fx::find(fx_table_of(m_page), effect_type_now(engine));
		return 1 + visible_count(table, engine.performance_mode(), fx_table_of(m_page)) + panel_fx::tail_of(fx_table_of(m_page), engine.variation_is_system()).count;
	}

	// ---- ボタン
	static bool repeats(int index)
	{
		return index == button_value_minus || index == button_value_plus || index == button_part_minus || index == button_part_plus
		    || index == button_select_minus || index == button_select_plus;
	}

	void act(native_engine &engine, int index)
	{
		// 「drumS1-4 を選べ」の知らせは次のボタンで消える（そのボタンの働きはしない）
		if (m_notice) {
			m_notice = false;
			return;
		}
		// SOUND MODULE は、面を離れるときに選んだものへ切り替わる
		if (m_screen == screen_sound_module && !m_detail
		    && (index == button_exit || index == button_play || index == button_edit || index == button_util || index == button_effect || index == button_eq))
			choose_sound_module(engine);
		// 実行と送信のあいだは EXIT（送信の取り消し）だけを受ける
		if (m_screen == screen_message && m_message <= MESSAGE_NO_RECALL) {
			if (index == button_exit && m_message == MESSAGE_TRANSMITTING) {
				m_message = MESSAGE_CANCELED;
				m_message_samples = CANCELED_SAMPLES;
			}
			return;
		}
		switch (index) {
		case button_play: press_play(engine); break;
		case button_edit: open_menu(engine.performance_mode() ? menu_performance : menu_edit); break;
		case button_util: open_menu(menu_util); break;
		case button_effect: open_menu(menu_effect); break;
		case button_mode: open_sound_module(engine); break;
		case button_eq: open_page(page_master_eq); m_return_menu = menu_effect; break;
		case button_mute: press_mute(engine); break;
		case button_part_minus: change_part(engine, -1); break;
		case button_part_plus: change_part(engine, 1); break;
		case button_enter: press_enter(engine); break;
		case button_exit: press_exit(engine.performance_mode()); break;
		case button_select_minus: select(engine, -1); break;
		case button_select_plus: select(engine, 1); break;
		case button_value_minus: change(engine, -1); break;
		case button_value_plus: change(engine, 1); break;
		default: break;
		}
	}

	void press_play(native_engine &engine)
	{
		if (m_screen == screen_play && m_detail) {
			m_detail = false;
			return;
		}
		m_detail = false;
		if (m_screen == screen_play && engine.performance_mode())
			m_performance_names = !m_performance_names;
		else if (m_screen == screen_play)
			m_layout = play_layout((m_layout + 1) % layout_count);
		else {
			m_screen = screen_play;
			// パフォーマンスでは、詳しい画面から品書きへ移ったときだけ項目が残る
			if (!(m_item_kept && engine.performance_mode())) {
				m_item = item_voice;
				m_field = field_program;
			}
		}
	}

	void open_sound_module(const native_engine &engine)
	{
		m_screen = screen_sound_module;
		m_mode_cursor = engine.performance_mode() ? 2 : engine.gs_mode() ? 1 : 0;
		m_mode_cursor_moved = false;
	}

	void open_menu(menu_id menu)
	{
		m_item_kept = m_screen == screen_play ? m_detail : m_item_kept && m_screen != screen_play;
		m_detail = false;
		m_screen = screen_menu;
		m_menu = menu;
	}

	void open_page(page_id page)
	{
		m_detail = false;
		m_screen = screen_page;
		m_page = page;
	}

	void clear_mute(native_engine &engine)
	{
		m_muted = 0;
		m_muted_ad = 0;
		m_solo = false;
		m_last_mute_part = -1;
		m_performance_muted = false;
		apply_mute(engine);
	}

	void audition(native_engine &engine, bool down)
	{
		if (down == m_audition_on || m_part >= MIDI_PARTS)
			return;
		m_audition_on = down;
		if (down) {
			const bool drum = !engine.performance_mode() && engine.part_settings(m_part)[BLOCK_PART_MODE] != 0;
			m_audition_note = m_settings[drum ? SETTING_AUDITION_DRUM_NOTE : SETTING_AUDITION_VOICE_NOTE];
			m_audition_part = m_part;
			const int velocity = m_settings[SETTING_AUDITION_VELOCITY];
			engine.panel_note(m_part, m_audition_note, std::min(velocity + velocity / AUDITION_VELOCITY_BOOST, 127));
		} else
			engine.panel_note(m_audition_part, m_audition_note, 0);
	}

	void press_mute(native_engine &engine)
	{
		if (m_screen != screen_play)
			return;
		if (!engine.performance_mode())
			m_detail = false;
		// パフォーマンスの全体の面: ソロを抜け、次からは全部のミュートを出し入れする（各パートの印は残る）
		if (engine.performance_mode() && !m_performance_focus) {
			if (m_solo) {
				m_solo = false;
				m_last_mute_part = -1;
			} else
				m_performance_muted = !m_performance_muted;
			apply_mute(engine);
			note_performance_mute(engine);
			return;
		}
		// 全部のミュートのまま 1 つのパートを押すと、そのパートだけ鳴る（ソロと同じ）
		if (m_performance_muted) {
			m_performance_muted = false;
			m_solo = true;
			m_solo_part = m_part;
			m_last_mute_part = m_part;
			set_part_muted(m_part, false);
			apply_mute(engine);
			note_performance_mute(engine);
			return;
		}
		// ソロ中: 同じパートなら抜け、別のパートならソロが移る
		if (m_solo) {
			if (m_part == m_solo_part) {
				m_solo = false;
				m_last_mute_part = -1;
			} else {
				m_solo_part = m_part;
				set_part_muted(m_part, false);
			}
		} else if (m_part == m_last_mute_part) {
			// 続けて押すとソロ
			m_solo = true;
			m_solo_part = m_part;
			set_part_muted(m_part, false);
		} else {
			// ミュートならはずし、そうでなければミュート
			set_part_muted(m_part, !part_is_muted(m_part));
			m_last_mute_part = m_part;
		}
		apply_mute(engine);
		if (engine.performance_mode())
			note_performance_mute(engine);
	}

	// パフォーマンスの記録の「鳴らす」印は、いま鳴っている通り
	void note_performance_mute(native_engine &engine) const
	{
		const u64 mask = m_performance_muted ? ~u64(0) : m_solo ? ~(u64(1) << m_solo_part) : m_muted;
		engine.set_performance_part_silence(u8(mask & 0xf));
	}

	bool part_is_muted(int part) const
	{
		return part < MIDI_PARTS ? (m_muted >> part & 1) != 0 : (m_muted_ad >> (part - MIDI_PARTS) & 1) != 0;
	}

	void set_part_muted(int part, bool muted)
	{
		if (part < MIDI_PARTS)
			m_muted = muted ? m_muted | u64(1) << part : m_muted & ~(u64(1) << part);
		else
			m_muted_ad = u8(muted ? m_muted_ad | 1 << (part - MIDI_PARTS) : m_muted_ad & ~(1 << (part - MIDI_PARTS)));
	}

	void apply_mute(native_engine &engine)
	{
		// Out=AD_Direct: A/D の音はミックスへ入らない
		const u8 direct = m_settings[SETTING_OUT] == OUT_AD_DIRECT ? u8(3) : u8(0);
		if (m_performance_muted) {
			engine.set_part_mute(~u64(0), 3);
			return;
		}
		const bool solo_ad = m_solo && m_solo_part >= MIDI_PARTS;
		const u64 solo_mask = m_solo && !solo_ad ? ~(u64(1) << m_solo_part) : solo_ad ? ~u64(0) : 0;
		const u8 solo_ad_mask = m_solo ? (solo_ad ? u8(3 & ~(1 << (m_solo_part - MIDI_PARTS))) : u8(3)) : u8(0);
		engine.set_part_mute(m_solo ? solo_mask : m_muted, u8((m_solo ? solo_ad_mask : m_muted_ad) | direct));
	}

	void change_part(native_engine &engine, int delta)
	{
		if (engine.performance_mode()) {
			// パートを選んでいるあいだは 4 パートを回る
			if ((m_screen == screen_page && m_page <= page_others) || (m_screen == screen_menu && m_menu == menu_edit) || (m_screen == screen_play && m_performance_focus) || (m_performance_focus && ((m_screen == screen_menu && m_menu == menu_performance) || (m_screen == screen_page && m_page == page_performance_common)))) {
				m_part = (m_part + delta + native_engine::PERFORMANCE_PARTS) % native_engine::PERFORMANCE_PARTS;
				return;
			}
			if (!(m_screen == screen_page && (fx_page(m_page) || m_page == page_master_eq)))
				return;
		}
		if (m_screen == screen_page && m_page == page_drum) {
			m_drum_key = std::clamp(m_drum_key + delta, int(::xg::settings::DRUM_SETUP_NOTE0), int(::xg::settings::DRUM_SETUP_NOTE0) + int(::xg::settings::DRUM_SETUP_NOTES) - 1);
			return;
		}
		if (m_screen == screen_page && (fx_page(m_page) || m_page == page_master_eq)) {
			step_fx_page(engine, delta);
			return;
		}
		// UTIL と EFFECT の面ではパートは動かない
		if (m_screen != screen_play && !(m_screen == screen_menu && m_menu == menu_edit) && !(m_screen == screen_page && m_page <= page_others))
			return;
		// 演奏画面だけ A/D パートまで回る
		const int count = m_screen == screen_page ? MIDI_PARTS : ALL_PARTS;
		m_part = (std::min(m_part, count - 1) + delta + count) % count;
		// A/D パートに全音色の欄は無く、バンクの欄になる
		if (m_part >= MIDI_PARTS && m_field == field_every_voice)
			m_field = field_bank;
		(void)engine;
	}

	// REV → CHO → VAR → INS1-4 → EQ の順に送る。項目はパラメータの番号を持ち越す（無ければ次にある番号）
	void step_fx_page(native_engine &engine, int delta)
	{
		const int from = m_page == page_reverb ? 0 : m_page == page_chorus ? 1 : m_page == page_variation ? 2 : m_page == page_insertion ? 3 + m_fx_block : 7;
		const int to = std::clamp(from + delta, 0, 7);
		if (to != from)
			m_enter_samples = DOUBLE_ENTER_WINDOW;   // 別のページでは数え直す
		move_fx_page(engine, to, delta);
	}

	void move_fx_page(native_engine &engine, int position, int direction)
	{
		int number = 0, tail_index = -1;
		if (fx_page(m_last_fx_page)) {
			const page_id here = m_page;
			const int block = m_fx_block;
			m_page = m_last_fx_page;
			m_fx_block = m_last_fx_block;
			const int item = m_page_item[m_page];
			const panel_fx::type_table from = panel_fx::find(fx_table_of(m_page), effect_type_now(engine));
			const bool performance = engine.performance_mode();
			const int shown = visible_count(from, performance, fx_table_of(m_page));
			if (item > shown)
				tail_index = item - 1 - shown;
			else if (item > 0)
				number = visible_param(from, item - 1, performance, fx_table_of(m_page)).address;
			m_page = here;
			m_fx_block = block;
		}
		m_page = position == 0 ? page_reverb : position == 1 ? page_chorus : position == 2 ? page_variation : position == 7 ? page_master_eq : page_insertion;
		if (position >= 3 && position < 7)
			m_fx_block = position - 3;
		m_menu_cursor[menu_effect] = u8(position == 7 ? 4 : std::min(position, 3));
		if (position >= 3 && position < 7) {
			m_menu_cursor[menu_insertion] = u8(position - 3);
			m_return_menu = menu_insertion;
		} else
			m_return_menu = menu_effect;
		if (m_page == page_master_eq)
			return;
		const panel_fx::type_table to = panel_fx::find(fx_table_of(m_page), effect_type_now(engine));
		const int tail_count = panel_fx::tail_of(fx_table_of(m_page), engine.variation_is_system()).count;
		int next = 0;
		const bool performance = engine.performance_mode();
		const int shown = visible_count(to, performance, fx_table_of(m_page));
		if (tail_index >= 0)
			next = performance ? 1 + shown + std::min(tail_index, tail_count - 1)
			                   : 1 + shown + (m_page == page_reverb || m_page == page_chorus ? tail_count - 1 : 0);
		else if (number > 0) {
			next = 1 + shown;
			for (int i = 0; i < shown; i++)
				if (visible_param(to, i, performance, fx_table_of(m_page)).address >= number) {
					next = 1 + i;
					break;
				}
		}
		m_page_item[m_page] = u8(next);
		m_last_fx_page = m_page;
		m_last_fx_block = m_fx_block;
	}

	void press_enter(native_engine &engine)
	{
		switch (m_screen) {
		case screen_menu:
			enter_menu(engine);
			m_enter_samples = DOUBLE_ENTER_WINDOW;   // 開いたページでは数え直す
			break;
		case screen_confirm: run_confirm(engine); break;
		case screen_sound_module:
			choose_sound_module(engine);
			if (m_mode_cursor != 2)      // PFM に SysEx の画面は無い
				enter_detail();
			else
				m_enter_samples = 0;
			break;
		case screen_page:
			if (m_page == page_card)
				open_card_item();
			else if (m_page == page_performance_common && m_page_item[m_page] == 0) {
				m_screen = screen_performance_name;
				m_name_cursor = 0;
			} else
				enter_detail();
			break;
		case screen_play:
			enter_detail();
			break;
		case screen_performance_copy:
			engine.copy_performance(m_copy_bank, m_copy_from, m_copy_to);
			start_executing(menu_performance);
			break;
		case screen_performance_store:
			engine.store_performance(m_store_to);
			start_executing(menu_performance);
			break;
		case screen_performance_recall:
			engine.restore_performance_recall();
			start_executing(menu_performance);
			break;
		default:
			break;
		}
	}

	void start_executing(menu_id back_to)
	{
		m_message = MESSAGE_EXECUTING;
		// 記録を書く長さ。STORE がいちばん長い
		m_message_samples = m_screen == screen_performance_store ? STORE_SAMPLES : m_screen == screen_performance_copy || m_screen == screen_performance_recall ? COPY_SAMPLES : EXECUTING_SAMPLES;
		m_confirm_return = back_to;
		m_executing_title = m_screen;
		m_screen = screen_message;
	}

	// ENTER を続けて 2 回押すと詳しい画面を開く
	void enter_detail()
	{
		const bool second = m_enter_samples < DOUBLE_ENTER_WINDOW;
		if (second)
			m_detail = true;
		m_enter_samples = second ? DOUBLE_ENTER_WINDOW : 0;
	}

	void enter_menu(native_engine &engine)
	{
		const int cursor = m_menu_cursor[m_menu];
		switch (m_menu) {
		case menu_performance:
			m_return_menu = menu_performance;
			if (cursor == 0)
				open_page(page_performance_common);
			else if (cursor == 1) {
				m_part = std::min(m_part, native_engine::PERFORMANCE_PARTS - 1);
				m_performance_focus = true;
				open_menu(menu_edit);
			} else if (cursor == 2)
				m_screen = screen_performance_copy;
			else if (cursor == 3) {
				m_store_to = engine.performance_number();
				m_screen = screen_performance_store;
			} else
				recall_edit_buffer(engine);
			break;
		case menu_edit: {
			if (m_part >= MIDI_PARTS) {
				// A/D パートは受信チャンネルの項目だけ
				if (cursor == 4) {
					open_page(page_others);
					m_page_item[page_others] = 0;
					m_return_menu = menu_edit;
				}
				return;
			}
			static constexpr page_id PAGES[] = { page_filter, page_eg, page_eq, page_vibrato, page_others };
			const bool drum = engine.part_settings(m_part)[BLOCK_PART_MODE] != 0;
			open_page(cursor == 2 && drum ? page_drum : PAGES[cursor]);
			m_return_menu = menu_edit;
			break;
		}
		case menu_util:
			if (cursor == 0) { open_page(page_system); m_return_menu = menu_util; }
			else if (cursor == 1) open_menu(menu_dump);
			else if (cursor == 2) open_menu(menu_init);
			else if (cursor == 3) open_sound_module(engine);
			else { open_page(page_card); m_return_menu = menu_util; }
			break;
		case menu_dump:
			open_confirm(engine.performance_mode() ? confirm_dump_perform : cursor == 0 ? confirm_dump_all : cursor == 1 ? confirm_dump_multi : confirm_dump_perform);
			break;
		case menu_init:
			open_confirm(cursor == 0 ? confirm_factory : cursor == 1 ? confirm_init_xg : confirm_init_drum);
			break;
		case menu_effect:
			if (cursor == 3)
				open_menu(menu_insertion);
			else {
				m_screen = screen_page;
				move_fx_page(engine, cursor == 4 ? 7 : cursor, 1);
			}
			break;
		case menu_insertion:
			m_screen = screen_page;
			move_fx_page(engine, 3 + cursor, 1);
			break;
		default:
			break;
		}
	}

	void recall_edit_buffer(native_engine &engine)
	{
		if (!engine.performance_recall_valid()) {
			m_message = MESSAGE_NO_RECALL;
			m_screen = screen_message;
			m_message_samples = NO_RECALL_SAMPLES;
			m_confirm_return = menu_performance;
			return;
		}
		m_screen = screen_performance_recall;
	}

	void open_confirm(confirm_id id)
	{
		m_screen = screen_confirm;
		m_confirm = id;
		m_confirm_choice = 0;
		m_confirm_return = m_menu;
	}

	void run_confirm(native_engine &engine)
	{
		switch (m_confirm) {
		case confirm_factory:
			engine.factory_reset();
			reset_settings();
			m_ad_preset = m_ad_bank = {};
			m_part = 0;
			apply_settings(engine);
			apply_mute(engine);
			break;
		case confirm_init_xg:
			if (engine.performance_mode())
				engine.initialize_performance();
			else {
				if (engine.gs_mode())
					engine.select_sound_module(true);
				else
					engine.initialize_xg();
				m_ad_preset = m_ad_bank = {};
				clear_mute(engine);
			}
			break;
		case confirm_init_drum:
			engine.initialize_drum_setup(std::max(0, m_confirm_choice));
			break;
		default:
			break;
		}
		m_screen = screen_message;
		m_executing_title = screen_confirm;
		if (m_confirm <= confirm_dump_perform) {
			m_message = MESSAGE_TRANSMITTING;
			m_dump_position = 0;
			m_dump_wait = 0;
			m_dump_buffered = 0;
			m_dump_sent = 0;
			m_transmit_cells = 0;
			m_message_samples = 0;
		} else {
			m_message = MESSAGE_EXECUTING;
			m_message_samples = m_confirm == confirm_factory ? FACTORY_SAMPLES : EXECUTING_SAMPLES;
		}
	}

	void message_done()
	{
		m_screen = screen_menu;
		m_menu = m_confirm_return;
	}

	void open_card_item()
	{
		m_message = u8(MESSAGE_CARD_FIRST + m_page_item[page_card]);
		m_screen = screen_message;
		m_message_samples = 0;
	}

	void choose_sound_module(native_engine &engine)
	{
		// 同じ所で押し直しても何もしない
		// カーソルを動かしていなければ何もしない（動かして戻したときは選び直しになる）
		if (!m_mode_cursor_moved)
			return;
		m_mode_cursor_moved = false;
		const int current = engine.performance_mode() ? 2 : engine.gs_mode() ? 1 : 0;
		if (m_mode_cursor != current)
			m_performance_names = false;
		// パフォーマンスの選んだパートと、ほかのモードのパートは別に覚えている
		if (current == 2 && m_mode_cursor != 2)
			std::swap(m_part, m_other_mode_part);
		else if (current != 2 && m_mode_cursor == 2) {
			std::swap(m_part, m_other_mode_part);
			m_part = std::min(m_part, native_engine::PERFORMANCE_PARTS - 1);
		}
		m_performance_focus = false;
		m_page_item[page_others] = 0;
		m_item = item_voice;
		m_field = field_program;
		m_ad_preset = m_ad_bank = {};
		clear_mute(engine);
		if (m_mode_cursor == 2) {
			engine.enter_performance(m_settings[SETTING_PERFORMANCE_CHANNEL]);
			return;
		}
		engine.leave_performance();
		engine.select_sound_module(m_mode_cursor == 1);
		apply_settings(engine);
	}

	void press_exit(bool engine_in_performance)
	{
		if (m_detail) {
			m_detail = false;
			return;
		}
		switch (m_screen) {
		case screen_play:
			// パートを選んでいて項目も先頭なら、全体の面へ戻る
			if (m_item == item_voice && m_field == field_program)
				m_performance_focus = false;
			m_item = item_voice;
			m_field = field_program;
			break;
		case screen_menu:
			if (m_menu == menu_edit && engine_in_performance)
				open_menu(menu_performance);
			else if (m_menu == menu_dump || m_menu == menu_init)
				open_menu(menu_util);
			else if (m_menu == menu_insertion)
				open_menu(menu_effect);
			else {
				m_screen = screen_play;
				if (!(m_item_kept && engine_in_performance)) {
					m_item = item_voice;
					m_field = field_program;
				}
			}
			break;
		case screen_page:
			open_menu(m_return_menu);
			break;
		case screen_performance_name:
			m_screen = screen_page;
			m_page = page_performance_common;
			break;
		case screen_performance_copy:
		case screen_performance_store:
		case screen_performance_recall:
			open_menu(menu_performance);
			break;
		case screen_confirm:
			open_menu(m_confirm_return);
			break;
		case screen_sound_module:
			m_performance_names = false;
			open_menu(menu_util);
			break;
		case screen_message:
			if (m_page == page_card && m_message >= MESSAGE_CARD_FIRST)
				m_screen = screen_page;
			else
				open_menu(m_confirm_return);
			break;
		}
	}

	void move_mode_cursor(int delta)
	{
		const u8 moved = u8(std::clamp(m_mode_cursor + delta, 0, 2));
		m_mode_cursor_moved = m_mode_cursor_moved || moved != m_mode_cursor;
		m_mode_cursor = moved;
	}

	void select(native_engine &engine, int delta)
	{
		if (m_detail && m_screen != screen_play)
			return;
		switch (m_screen) {
		case screen_play:
			if (engine.performance_mode() && !m_performance_focus)
				select_performance(delta);
			else
				select_play(delta, engine.performance_mode() ? int(performance_item_transpose) : int(play_item_count) - 1);
			break;
		case screen_menu: {
			const int count = engine.performance_mode() && m_menu == menu_insertion ? 2 : menu_of(m_menu, m_part < MIDI_PARTS && engine.part_settings(m_part)[BLOCK_PART_MODE] != 0, engine.performance_mode(), engine.gs_mode()).count;
			m_menu_cursor[m_menu] = u8(std::clamp(m_menu_cursor[m_menu] + delta, 0, count - 1));
			break;
		}
		case screen_page: {
			const int count = fx_page(m_page) ? fx_item_count(engine) : m_page == page_master_eq ? 6 : m_page == page_performance_common ? int(std::size(PERFORMANCE_COMMON)) : m_part >= MIDI_PARTS && m_page == page_others ? 1 : params_of(m_page, engine.performance_mode()).count;
			m_page_item[m_page] = u8(std::clamp(m_page_item[m_page] + delta, 0, count - 1));
			if (fx_page(m_page)) {
				m_last_fx_page = m_page;
				m_last_fx_block = m_fx_block;
			}
			break;
		}
		case screen_sound_module:
			move_mode_cursor(delta);
			break;
		case screen_performance_name:
			m_name_cursor = std::clamp(m_name_cursor + delta, 0, native_engine::PERF_NAME_SIZE - 1);
			break;
		case screen_performance_copy:
			m_copy_field = std::clamp(m_copy_field + delta, 0, 2);
			break;
		default:
			break;
		}
	}

	void select_performance(int delta)
	{
		if (m_item == item_voice) {
			if (delta < 0) {
				m_field = field_bank;
				return;
			}
			if (m_field == field_bank) {
				m_field = field_program;
				return;
			}
			m_item = item_volume;
			return;
		}
		m_item = u8(std::clamp(m_item + delta, int(item_voice), int(performance_item_transpose)));
	}

	void select_play(int delta, int last_item)
	{
		if (m_item == item_voice) {
			if (delta < 0) {
				m_field = m_field == field_program || m_part >= MIDI_PARTS ? field_bank : field_every_voice;
				return;
			}
			if (m_field == field_every_voice) {
				m_field = field_bank;
				return;
			}
			if (m_field == field_bank) {
				m_field = field_program;
				return;
			}
			m_item = item_volume;
			return;
		}
		if (delta < 0 && m_item == item_volume)
			m_field = field_program;
		m_item = u8(std::clamp(m_item + delta, int(item_voice), last_item));
	}

	// ---- 値を動かす（VALUE −/+ とダイヤル）
	void change(native_engine &engine, int delta)
	{
		if (delta == 0)
			return;
		switch (m_screen) {
		case screen_play:
			if (engine.performance_mode() && m_performance_focus)
				change_performance_part(engine, delta);
			else if (engine.performance_mode())
				change_performance(engine, delta);
			else
				change_play(engine, delta);
			break;
		case screen_page: change_page(engine, delta); break;
		case screen_performance_name:
			change_name(engine, delta);
			break;
		case screen_performance_copy:
			if (m_copy_field == 0)
				m_copy_bank = std::clamp(m_copy_bank + delta, 0, 1);
			else
				(m_copy_field == 1 ? m_copy_from : m_copy_to) = std::clamp((m_copy_field == 1 ? m_copy_from : m_copy_to) + delta, 0, native_engine::PERFORMANCES - 1);
			break;
		case screen_performance_store:
			m_store_to = std::clamp(m_store_to + delta, 0, native_engine::PERFORMANCES - 1);
			break;
		case screen_confirm: change_confirm(delta); break;
		case screen_sound_module:
			if (!m_detail)
				move_mode_cursor(delta);
			break;
		default: break;
		}
	}

	void change_name(native_engine &engine, int delta)
	{
		const u8 *record = engine.performance_record();
		const int current = record[native_engine::PERF_NAME + m_name_cursor];
		engine.set_performance_name_char(m_name_cursor, u8(std::clamp(current + delta, 0x20, 0x7d)));
	}

	void change_performance(native_engine &engine, int delta)
	{
		const u8 *record = engine.performance_record();
		switch (m_item) {
		case item_voice:
		{
			const int bank = m_field == field_bank ? std::clamp(engine.performance_bank() + delta, 0, 1) : engine.performance_bank();
			const int number = m_field == field_bank ? engine.performance_number() : std::clamp(engine.performance_number() + delta, 0, native_engine::PERFORMANCES - 1);
			if (bank == engine.performance_bank() && number == engine.performance_number())
				break;
			engine.recall_performance(bank, number);
			break;
		}
		case item_volume: engine.set_performance_volume(std::clamp(record[native_engine::PERF_VOLUME] + delta, 0, 127)); engine.note_performance_edited(); break;
		case item_expression: engine.set_performance_pan(std::clamp(record[native_engine::PERF_PAN] + delta, 0, 127)); engine.note_performance_edited(); break;
		case item_pan: set_return(engine, native_engine::PERF_REVERB_RETURN, 0x0c, delta); break;
		case item_reverb: set_return(engine, native_engine::PERF_CHORUS_RETURN, 0x2c, delta); break;
		case item_chorus: set_return(engine, native_engine::PERF_VARIATION_RETURN, 0x56, delta); break;
		default: {
			const u8 value = u8(std::clamp(engine.system_transpose() + delta, 0x28, 0x58));
			engine.panel_bytes(0x00, 0x00, 0x06, &value, 1);
			break;
		}
		}
	}

	// 選んだパートの項目: 音色・音量・パン・送り 3 つ・ノートシフト。記録の中の位置
	static constexpr int PERFORMANCE_PART_ITEM[] = { 0x00, 0x02, 0x03, 0x06, 0x05, 0x07, 0x08 };
	static constexpr int PERFORMANCE_PART_ITEM_LOW[] = { 0x00, 0x0b, 0x0e, 0x13, 0x12, 0x14, 0x08 };
	static u8 performance_part_item(const native_engine &engine, int part, int item)
	{
		return engine.performance_record()[part * native_engine::PERFORMANCE_PART_SIZE + PERFORMANCE_PART_ITEM[std::min<int>(item, performance_item_transpose)]];
	}

	void change_performance_part(native_engine &engine, int delta)
	{
		if (m_item == item_voice) {
			step_voice(engine, delta);
			engine.note_performance_part_voice(m_part);
			return;
		}
		const int offset = m_part * native_engine::PERFORMANCE_PART_SIZE + PERFORMANCE_PART_ITEM[m_item];
		const bool shift = m_item == performance_item_transpose;
		const int value = std::clamp(engine.performance_record()[offset] + delta, shift ? 0x28 : 0, shift ? 0x58 : 127);
		engine.set_performance_byte(offset, value);
		if (m_item == item_volume)
			engine.set_performance_volume(engine.performance_record()[native_engine::PERF_VOLUME]);
		else if (m_item == item_expression)
			engine.set_performance_pan(engine.performance_record()[native_engine::PERF_PAN]);
		else
			send_xg(engine, 0x08, u8(m_part), u8(PERFORMANCE_PART_ITEM_LOW[m_item]), u8(value));
	}

	void set_return(native_engine &engine, int offset, int low, int delta)
	{
		const int value = std::clamp(engine.performance_record()[offset] + delta, 0, 127);
		engine.set_performance_byte(offset, value);
		send_xg(engine, 0x02, 0x01, u8(low), u8(value));
	}

	void change_confirm(int delta)
	{
		const int count = m_confirm == confirm_dump_multi ? 5 : m_confirm == confirm_dump_perform ? 100 : m_confirm == confirm_init_drum ? 3 : 0;
		m_confirm_choice = std::clamp(m_confirm_choice + delta, 0, count);
	}

	void change_play(native_engine &engine, int delta)
	{
		if (m_item == item_voice) {
			if (m_part >= MIDI_PARTS)
				step_ad_voice(engine, delta);
			else
				step_voice(engine, delta);
			return;
		}
		const int current = part_value(engine, m_part, m_item);
		switch (m_item) {
		case item_volume: send_part(engine, m_part, BLOCK_VOLUME, std::clamp(current + delta, 0, 127)); break;
		case item_expression:
			if (m_part >= MIDI_PARTS)
				engine.panel_ad_expression(m_part - MIDI_PARTS, std::clamp(current + delta, 0, 127));
			else
				engine.panel_control(m_part, 11, std::clamp(current + delta, 0, 127));
			break;
		case item_pan: send_part(engine, m_part, BLOCK_PAN, std::clamp(current + delta, 0, 127)); break;
		case item_reverb: send_part(engine, m_part, BLOCK_REVERB, std::clamp(current + delta, 0, 127)); break;
		case item_chorus: send_part(engine, m_part, BLOCK_CHORUS, std::clamp(current + delta, 0, 127)); break;
		case item_variation:
			if (engine.variation_is_system())
				send_part(engine, m_part, BLOCK_VARIATION, std::clamp(current + delta, 0, 127));
			else
				send_xg(engine, 0x02, 0x01, 0x5b, delta > 0 ? u8(m_part) : u8(127));
			break;
		default: send_part(engine, m_part, BLOCK_NOTE_SHIFT, std::clamp(current + delta, 0x28, 0x58)); break;
		}
	}

	static constexpr int AD2_BANKS = 4, AD2_PRESETS = 5;
	static constexpr int AD_BANK_LINE = 2, AD_BANK_AUDIO = 3;
	static constexpr u8 PAN_HARD_LEFT = 1, PAN_HARD_RIGHT = 127;
	static constexpr u8 THIRD_INSERTION = 2, FOURTH_INSERTION = 3;

	// A/D パートの「音色」は入力の使い方の品書き（バンクごとに別の組）。選ぶと送りとエフェクトがその組になる
	void step_ad_voice(native_engine &engine, int delta)
	{
		const int which = m_part - MIDI_PARTS;
		if (m_field != field_program) {
			const int banks = which == 0 ? int(std::size(panel_fx::AD_BANK_COUNT)) : AD2_BANKS;
			const int bank = std::clamp(m_ad_bank[which] + delta, 0, banks - 1);
			if (bank != m_ad_bank[which]) {
				m_ad_bank[which] = bank;
				m_ad_preset[which] = 0;
				apply_ad_preset(engine, which);
			}
			return;
		}
		// A/D 2 はエフェクトを使う品書きを持たない
		const int count = which == 0 ? panel_fx::AD_BANK_COUNT[m_ad_bank[which]] : AD2_PRESETS;
		const int preset = std::clamp(m_ad_preset[which] + delta, 0, count - 1);
		if (preset == m_ad_preset[which])
			return;
		m_ad_preset[which] = preset;
		apply_ad_preset(engine, which);
	}

	void apply_ad_preset(native_engine &engine, int which)
	{
		namespace at = ::xg::rom_position;
		const u8 *preset = engine.program_rom() + panel_fx::ad_preset_at(m_ad_bank[which], m_ad_preset[which]);
		const u8 part = u8(which);
		send_xg(engine, 0x10, part, BLOCK_VOLUME, preset[at::AD_VOLUME]);
		// パンを決めない記録はステレオの入力で、左右に振り分ける
		const bool stereo = preset[at::AD_PAN] == at::AD_NOT_SET;
		send_xg(engine, 0x10, part, BLOCK_PAN, stereo ? (which == 0 ? PAN_HARD_LEFT : PAN_HARD_RIGHT) : preset[at::AD_PAN]);
		send_xg(engine, 0x10, part, BLOCK_REVERB, preset[at::AD_REVERB]);
		send_xg(engine, 0x10, part, BLOCK_CHORUS, preset[at::AD_CHORUS]);
		// 効果を持つのは A/D 1 の記録だけ。A/D 2 を選んでも A/D 1 の効果は動かない
		if (which != 0)
			return;
		const int effects = preset[at::AD_EFFECTS_USED];
		if (effects == at::ad_variation)
			write_ad_variation(engine, preset);
		else
			send_xg(engine, ::xg::settings::XG_EFFECT, ::xg::settings::XG_VARIATION, ::xg::settings::XG_VARIATION_PART, ::xg::settings::XG_NO_PART);
		if (effects == at::ad_third_and_fourth_insertions)
			write_ad_insertion(engine, preset, THIRD_INSERTION, at::AD_THIRD_INSERTION);
		else
			send_xg(engine, ::xg::settings::XG_INSERTION, THIRD_INSERTION, ::xg::settings::XG_INSERTION_PART, ::xg::settings::XG_NO_PART);
		if (effects == at::ad_fourth_insertion || effects == at::ad_third_and_fourth_insertions)
			write_ad_insertion(engine, preset, FOURTH_INSERTION, at::AD_FOURTH_INSERTION);
		else
			send_xg(engine, ::xg::settings::XG_INSERTION, FOURTH_INSERTION, ::xg::settings::XG_INSERTION_PART, ::xg::settings::XG_NO_PART);
	}

	static void write_ad_variation(native_engine &engine, const u8 *preset)
	{
		namespace at = ::xg::rom_position;
		engine.panel_bytes(::xg::settings::XG_EFFECT, ::xg::settings::XG_VARIATION, ::xg::settings::XG_VARIATION_TYPE, preset + at::AD_VARIATION_TYPE, 2);
		for (int n = 0; n < at::AD_VARIATION_VALUE_COUNT; n++) {
			const u8 value = preset[at::AD_VARIATION_VALUES + n];
			if (value != at::AD_NOT_SET) {
				const u8 bytes[2] = { 0, value };
				engine.panel_bytes(::xg::settings::XG_EFFECT, ::xg::settings::XG_VARIATION, u8(::xg::settings::XG_VARIATION_VALUES + n * 2), bytes, 2);
			}
		}
		// 戻り、パン、リバーブとコーラスへの送り、繋ぎ方（インサーション）、掛かり先
		const u8 routing[6] = { 64, 64, 0, 0, 0, u8(MIDI_PARTS) };
		engine.panel_bytes(::xg::settings::XG_EFFECT, ::xg::settings::XG_VARIATION, ::xg::settings::XG_VARIATION_RETURN, routing, 6);
	}

	void write_ad_insertion(native_engine &engine, const u8 *preset, u8 insertion, const ::xg::rom_position::ad_insertion_record &record)
	{
		namespace at = ::xg::rom_position;
		engine.panel_bytes(::xg::settings::XG_INSERTION, insertion, ::xg::settings::XG_INSERTION_TYPE, preset + record.type, 2);
		const u8 *values = preset + record.values;
		// 値を 2 バイトで持つ種類と 1 バイトで持つ種類で、書く先が違う
		const bool wide = native_engine::fx_type_is_wide(preset[record.type], preset[record.type + 1]);
		for (int n = 0; n < at::AD_INSERTION_VALUE_COUNT; n++) {
			if (values[n * 2 + 1] == at::AD_NOT_SET)
				continue;
			if (wide) {
				const int value = values[n * 2] << 8 | values[n * 2 + 1];
				const u8 bytes[2] = { u8((value >> 7) & 0x7f), u8(value & 0x7f) };
				engine.panel_bytes(::xg::settings::XG_INSERTION, insertion, u8(::xg::settings::XG_INSERTION_WIDE_VALUES + n * 2), bytes, 2);
			} else {
				send_xg(engine, ::xg::settings::XG_INSERTION, insertion, u8(::xg::settings::XG_INSERTION_VALUES + n), values[n * 2 + 1]);
			}
		}
		for (int n = 0; n < at::AD_INSERTION_LATER_VALUE_COUNT; n++) {
			const u8 value = values[at::AD_INSERTION_LATER_VALUES + n];
			if (value != at::AD_NOT_SET)
				send_xg(engine, ::xg::settings::XG_INSERTION, insertion, u8(::xg::settings::XG_INSERTION_LATER_VALUES + 1 + n), value);
		}
		send_xg(engine, ::xg::settings::XG_INSERTION, insertion, ::xg::settings::XG_INSERTION_PART, u8(MIDI_PARTS));
	}

	// 音色のあるバンクだけを通る。旋律は MSB 0 → 48 → 64 → 121 の順
	static constexpr u8 BANK_MSB_ORDER[] = { 0, 48, 64, 121 };
	static constexpr u8 PLUGIN_STYLE_LSBS[] = { 0, 8, 16, 24, 48, 56, 64, 72, 80, 88, 96, 104, 120 };
	static constexpr int GM_BANK_LAST_LSB = 9;
	static constexpr int VOICE_FAMILY_SIZE = 8;
	static constexpr int GS_LAST_MELODY_BANK = 125;

	static bool bank_has_voice(const ::xg::voice_rom &voices, int set, int msb, int lsb, int program)
	{
		switch (msb) {
		case 0:
			return lsb == 0 || lsb >= 126 || voices.lookup(1, set, 0, lsb, program) != voices.lookup(1, set, 0, 0, program);
		case 48:
			return std::find(std::begin(PLUGIN_STYLE_LSBS), std::end(PLUGIN_STYLE_LSBS), u8(lsb)) != std::end(PLUGIN_STYLE_LSBS);
		case 64:
			return lsb == 0;
		default:
			return lsb == 0 || (lsb <= GM_BANK_LAST_LSB && voices.lookup(1, set, msb, lsb, program) != voices.lookup(1, set, msb, 0, program));
		}
	}

	static bool step_bank(const ::xg::voice_rom &voices, int set, int &msb, int &lsb, int program, int direction, bool main_banks_only)
	{
		int group = 0;
		for (size_t i = 0; i < std::size(BANK_MSB_ORDER); i++)
			if (BANK_MSB_ORDER[i] == msb)
				group = int(i);
		int position = lsb;
		while (true) {
			position += direction;
			if (position < 0 || position > 127) {
				do {
					group += direction;
				} while (main_banks_only && group >= 0 && group < int(std::size(BANK_MSB_ORDER)) && BANK_MSB_ORDER[group] != 0 && BANK_MSB_ORDER[group] != 121);
				if (group < 0 || group >= int(std::size(BANK_MSB_ORDER)))
					return false;
				position = direction > 0 ? 0 : 127;
			}
			if (bank_has_voice(voices, set, BANK_MSB_ORDER[group], position, program)) {
				msb = BANK_MSB_ORDER[group];
				lsb = position;
				return true;
			}
		}
	}

	struct kit_entry { u8 msb, program; };
	static bool kit_exists(const ::xg::voice_rom &voices, int msb, int program)
	{
		if (msb == 120)
			return gm_kit_index(program) >= 0;
		return !voices.kit_name(msb, program).empty();
	}

	// GM のキット: プログラム → 名前の一覧の番号。無ければ -1
	static int gm_kit_index(int program)
	{
		switch (program) {
		case 0: return 8;
		case 8: return 0;
		case 16: return 1;
		case 24: return 2;
		case 25: return 3;
		case 32: return 4;
		case 40: return 5;
		case 48: return 6;
		case 56: return 7;
		default: return -1;
		}
	}

	void step_kit(native_engine &engine, const ::xg::voice_rom &voices, int direction)
	{
		static constexpr u8 KIT_MSB_ORDER[] = { 127, 126, 120 };
		int msb = engine.part_bank_msb(m_part), program = engine.part_program(m_part);
		int group = msb == 126 ? 1 : msb == 120 ? 2 : 0;
		while (true) {
			program += direction;
			if (program < 0 || program > 127) {
				group += direction;
				if (group < 0 || group > 2)
					return;
				program = direction > 0 ? 0 : 127;
			}
			if (kit_exists(voices, KIT_MSB_ORDER[group], program))
				break;
		}
		engine.panel_apply_voice(m_part, KIT_MSB_ORDER[group], 0, program);
	}

	void step_voice(native_engine &engine, int delta)
	{
		const ::xg::voice_rom voices(engine.program_rom_shared());
		const int direction = delta > 0 ? 1 : -1;
		const bool drum = engine.part_settings(m_part)[BLOCK_PART_MODE] != 0;
		const bool gs = engine.gs_mode();
		for (int steps = std::abs(delta); steps > 0; steps--) {
			int msb = engine.part_bank_msb(m_part), lsb = engine.part_bank_lsb(m_part), program = engine.part_program(m_part);
			if (drum && gs) {
				if (m_field == field_program) {
					int index = 0;
					const int kits = int(std::size(::xg::voice_rom::TG300B_KITS));
					for (int i = 0; i < kits; i++)
						if (::xg::voice_rom::TG300B_KITS[size_t(i)].prog == program)
							index = i;
					engine.panel_apply_voice(m_part, msb, lsb, ::xg::voice_rom::TG300B_KITS[size_t(std::clamp(index + direction, 0, kits - 1))].prog);
				}
				continue;
			}
			if (drum) {
				if (m_field == field_program)
					step_kit(engine, voices, direction);
				continue;
			}
			// GS は LSB を使わず、MSB に音色のあるものを順に通る
			if (gs && m_field != field_program) {
				const auto has_voice = [&](int bank, int number) { return bank >= 126 || bank == 0 || voices.lookup(0, 1, bank, 0, number) != voices.lookup(0, 1, 0, 0, number); };
				int next = msb;
				while (true) {
					next += direction;
					if (next < 0 || next > 127)
						break;
					if (has_voice(next, program))
						break;
				}
				if (next < 0 || next > 127) {
					if (m_field != field_every_voice)
						return;
					// 全音色の欄: 8 音色の組の中で次の音色へ
					const int family = program / VOICE_FAMILY_SIZE * VOICE_FAMILY_SIZE;
					program = family + (program - family + direction + VOICE_FAMILY_SIZE) % VOICE_FAMILY_SIZE;
					next = 0;
					if (direction < 0)
						for (int bank = GS_LAST_MELODY_BANK; bank > 0; bank--)
							if (has_voice(bank, program)) {
								next = bank;
								break;
							}
				}
				engine.panel_apply_voice(m_part, next, 0, program);
				continue;
			}
			if (m_field == field_program) {
				const int next = std::clamp(program + direction, 0, 127);
				if (next == program)
					return;
				program = next;
			} else if (m_field == field_bank) {
				if (!step_bank(voices, engine.voice_set(), msb, lsb, program, direction, false))
					return;
			} else if (!step_bank(voices, engine.voice_set(), msb, lsb, program, direction, true)) {
				// 8 音色ずつの組の中で回る
				const int family = program / VOICE_FAMILY_SIZE * VOICE_FAMILY_SIZE;
				program = family + (program - family + direction + VOICE_FAMILY_SIZE) % VOICE_FAMILY_SIZE;
				msb = direction > 0 ? 0 : 121;
				lsb = 0;
				if (direction < 0)
					for (int candidate = GM_BANK_LAST_LSB; candidate > 0; candidate--)
						if (bank_has_voice(voices, engine.voice_set(), 121, candidate, program)) {
							lsb = candidate;
							break;
						}
			}
				engine.panel_apply_voice(m_part, msb, lsb, program);
		}
	}

	void change_page(native_engine &engine, int delta)
	{
		const int item = m_page_item[m_page];
		switch (m_page) {
		case page_filter: case page_eg: case page_eq: case page_vibrato: case page_others: {
			if (m_part >= MIDI_PARTS)
				return;
			const part_param &p = params_of(m_page, engine.performance_mode()).items[item];
			const int value = std::clamp(page_value(engine) + delta, int(p.lowest), int(p.highest));
			if (p.format == format_detune) {
				const u8 detune[2] = { u8((value >> 4) & 0x0f), u8(value & 0x0f) };
				engine.panel_bytes(0x08, u8(m_part), 0x09, detune, 2);
			} else
				send_xg(engine, p.high, u8(m_part), p.low, u8(value < 0 ? 127 : value));
			if (engine.performance_mode() && m_part < native_engine::PERFORMANCE_PARTS)
				engine.note_performance_part_edit(m_part, p.high, p.low, value);
			break;
		}
		case page_drum: {
			const int set = drum_set_of(engine);
			if (set < 0) {
				m_notice = true;     // 組を持たないパートは編集できない
				return;
			}
			const part_param &p = DRUM_PARAMS[item];
			const int value = std::clamp(page_value(engine) + delta, int(p.lowest), int(p.highest));
			send_xg(engine, u8(0x30 | set), u8(m_drum_key), p.low, u8(value));
			break;
		}
		case page_performance_common: {
			if (item == 0)
				break;
			const part_param &p = PERFORMANCE_COMMON[item];
			const int value = std::clamp(page_value(engine) + delta, int(p.lowest), int(p.highest));
			engine.set_performance_byte(p.low, value);
			engine.apply_performance();
			break;
		}
		case page_system: {
			const part_param &p = SYSTEM_PARAMS[item];
			if (item == SETTING_MAP && engine.performance_mode())
				break;
			const int value = std::clamp(page_value(engine) + delta, int(p.lowest), int(p.highest));
			if (item == 0) {
				engine.set_master_tune(value);
				break;
			}
			m_settings[size_t(item)] = short(value);
			apply_settings(engine);
			if (item == SETTING_PERFORMANCE_CHANNEL)
				engine.set_performance_channel(value);
			break;
		}
		case page_reverb: case page_chorus: case page_variation: case page_insertion:
			change_fx(engine, item, delta);
			break;
		case page_master_eq:
			if (item == 0) {
				const int type = std::clamp(eq_type_now(engine) + delta, 0, 4);
				if (type != eq_type_now(engine)) {
					engine.set_master_eq_type(type);
					engine.panel_bytes(0x02, 0x40, 0x01, engine.eq_preset(type), 20);
					if (engine.performance_mode()) {
						engine.set_performance_byte(native_engine::PERF_EQ_TYPE, type);
						for (int band = 0; band < 5; band++)
							engine.set_performance_byte(native_engine::PERF_EQ_TYPE + 1 + band, engine.eq_preset(type)[band * 4]);
					}
				}
			} else {
				const int gain = std::clamp(engine.effect_value(native_engine::block_master_eq, 1 + (item - 1) * 4) + delta, 52, 76);
				send_xg(engine, 0x02, 0x40, u8(1 + (item - 1) * 4), u8(gain));
				if (engine.performance_mode())
					engine.set_performance_byte(native_engine::PERF_EQ_TYPE + item, gain);
			}
			break;
		default:
			break;
		}
	}

	int eq_type_now(const native_engine &engine) const
	{
		return engine.performance_mode() ? engine.performance_record()[native_engine::PERF_EQ_TYPE] : engine.master_eq_type();
	}

	void change_fx(native_engine &engine, int item, int delta)
	{
		const panel_fx::table_id table_id = fx_table_of(m_page);
		const int block = fx_block_of(m_page);
		const int type = effect_type_now(engine);
		if (item == 0) {
			const std::vector<::xg::fx_type> &types = types_of(m_page);
			if (types.empty())
				return;
			// インサーションの並びは THRU が先頭で NO EFFECT が無い（ROM の表は NO EFFECT が先頭で THRU が末尾）
			const int last = int(types.size()) - 1;
			const bool insertion = m_page == page_insertion;
			const int table_index = effect_type_index(engine);
			const int position = insertion ? (table_index == last ? 0 : table_index) : table_index;
			const int moved = std::clamp(position + delta, 0, insertion ? last - 1 : last);
			const int index = insertion && moved == 0 ? last : moved;
			const u8 message[2] = { types[size_t(index)].msb, types[size_t(index)].lsb };
			engine.panel_effect_write(block, 0, message, 2);
			if (engine.performance_mode()) {
				const int offset = performance_effect_offset(nullptr, nullptr);
				engine.set_performance_byte(offset, message[0]);
				engine.set_performance_byte(offset + 1, message[1]);
			}
			return;
		}
		const panel_fx::type_table table = panel_fx::find(table_id, type);
		const bool performance = engine.performance_mode();
		const int shown = visible_count(table, performance, table_id);
		const int index = item - 1;
		if (index < shown) {
			const panel_fx::param &p = visible_param(table, index, performance, table_id);
			const int value = std::clamp(engine.effect_value(block, p.address) + delta, int(p.lowest), int(p.highest));
			if (performance) {
				const int offset = performance_effect_offset(&p, nullptr);
				if (m_page == page_variation || m_page == page_insertion) {
					engine.set_performance_byte(offset, (value >> 7) & 0x7f);
					engine.set_performance_byte(offset + 1, value & 0x7f);
				} else
					engine.set_performance_byte(offset, value);
			}
			if (p.wide) {
				const u8 bytes[2] = { u8((value >> 7) & 0x7f), u8(value & 0x7f) };
				engine.panel_effect_write(block, p.address, bytes, 2);
			} else {
				const u8 byte = u8(value);
				engine.panel_effect_write(block, p.address, &byte, 1);
			}
			return;
		}
		const panel_fx::tail_list tail = panel_fx::tail_of(table_id, engine.variation_is_system());
		const panel_fx::tail_item &t = tail.items[index - shown];
		if (t.format == panel_fx::tail_center) {
			// コントローラの深さは音源の状態ではなくパネルが持つ（音は変わらない）
			u8 &depth = m_controller_depth[size_t(block)];
			depth = u8(std::clamp(depth + delta, int(t.lowest), int(t.highest)));
			return;
		}
		if (performance) {
			const int change_offset = performance_effect_offset(nullptr, &t);
			engine.set_performance_byte(change_offset, std::clamp(engine.performance_record()[change_offset] + delta, 0, 127));
		}
		// 掛かり先の並びは off・パート 1-64・AD1・AD2
		const int current = engine.effect_value(block, t.address);
		if (t.format == panel_fx::tail_part || t.format == panel_fx::tail_part_wide) {
			const int position = current >= 127 ? 0 : current + 1;
			const int next = std::clamp(position + delta, int(t.lowest), int(t.highest));
			const u8 byte = u8(next == 0 ? 127 : next - 1);
			engine.panel_effect_write(block, t.address, &byte, 1);
			return;
		}
		const u8 byte = u8(std::clamp(current + delta, int(t.lowest), int(t.highest)));
		engine.panel_effect_write(block, t.address, &byte, 1);
	}

	void send_part(native_engine &engine, int part, int address, int value)
	{
		if (part >= MIDI_PARTS)
			send_xg(engine, 0x10, u8(part - MIDI_PARTS), u8(address), u8(value));
		else
			send_xg(engine, 0x08, u8(part), u8(address), u8(value));
	}

	void send_xg(native_engine &engine, u8 high, u8 middle, u8 low, u8 value)
	{
		engine.panel_write(high, middle, low, value);
	}

	void reset_settings()
	{
		m_settings = DEFAULT_SETTINGS;
		m_muted = 0;
		m_solo = false;
	}

	void apply_settings(native_engine &engine)
	{
		engine.set_locks(m_settings[SETTING_AD_LOCK] != 0, m_settings[SETTING_EQ_LOCK] != 0);
		apply_mute(engine);
		engine.set_receive_options(m_settings[SETTING_DEVICE], m_settings[SETTING_RECEIVE_SYSTEM_ON] != 0, m_settings[SETTING_RECEIVE_EXCLUSIVE] != 0, m_settings[SETTING_RECEIVE_BANK] != 0);
		engine.set_voice_map(m_settings[SETTING_MAP] != 0);
	}

	// ---- 液晶を組む
	static void put(u8 *to, int room, const char *text)
	{
		for (int i = 0; i < room && text[i]; i++)
			to[i] = u8(text[i]);
	}
	static void put(u8 *to, int room, const std::string &text) { put(to, room, text.c_str()); }

	// UTIL と EFFECT はパートに依らない面で、パートの欄が空く
	static bool common_screen(const view &v)
	{
		return (v.leds() & (LED_UTIL | LED_EFFECT | LED_MODE | LED_EQ)) != 0;
	}

	static void compose_part_field(const view &v, const ::xg::voice_rom &voices, lcd_frame &out)
	{
		char text[4];
		for (int i = 0; i < 4; i++) {
			out.text[0][20 + i] = u8(i);
			out.text[1][20 + i] = u8(4 + i);
		}
		if (common_screen(v))
			return;
		if (v.part >= MIDI_PARTS && !v.performance) {
			out.text[0][17] = ' ';
			out.text[0][18] = 'A';
			out.text[1][17] = '/';
			out.text[1][18] = 'D';
			out.text[1][19] = u8('1' + v.part - MIDI_PARTS);
		} else {
			std::snprintf(text, sizeof(text), "%02d", (v.part + 1) % 100);
			out.text[0][17] = u8(text[0]);
			out.text[0][18] = u8(text[1]);
			std::snprintf(text, sizeof(text), "%c%02d", 'A' + v.part / 16, v.part % 16 + 1);
			put(&out.text[1][17], 3, text);
		}
		if (v.performance) {
			out.text[0][17] = out.text[0][18] = ' ';
			// パートを選んでいるときはその番号、ほかは全体
			if (v.performance_focus) {
				std::snprintf(text, sizeof(text), "%02d ", v.part + 1);
				put(&out.text[1][17], 3, text);
			} else
				put(&out.text[1][17], 3, voices.text(rom_at::TEXT_ALL_PARTS));
		}
		// VOL と EXP の棒（左が音量、右がエクスプレッション）
		const bool whole_performance = v.performance && !v.performance_focus;
		const int shown_volume = !v.performance ? v.volume : whole_performance ? v.performance_volume : v.performance_part_volume;
		const int volume_level = (shown_volume + 15) / 16, expression_level = ((v.performance ? 127 : v.expression) + 15) / 16;
		out.text[0][19] = u8(BAR_BASE + volume_level * BAR_STEP + expression_level);
	}

	static void compose_icon(const view &v, const ::xg::voice_rom &voices, lcd_frame &out)
	{
		u16 rows[16];
		if (const u32 address = fixed_icon(v) ? fixed_icon(v) : screen_icon(v)) {
			if (!voices.icon_at(address, rows))
				return;
			// マスター EQ は帯域ごとの絵を重ね、目盛りの行を抜く
			if (address == rom_at::ICON_MASTER_EQ) {
				for (u32 band = 1; band < 5; band++) {
					u16 other[16];
					voices.icon_at(address + band * 32, other);
					for (int y = 0; y < 16; y++)
						rows[y] &= other[y];
				}
				rows[9] = 0x7fff;
			}
		} else if (v.part >= MIDI_PARTS) {
			if (!voices.icon_at(rom_at::ICON_AD_BANKS[v.bank_lsb], rows))
				return;
		} else if (v.bank_msb == BANK_MSB_PLUGIN_STYLE || v.bank_msb == BANK_MSB_GM2) {
			if (!voices.icon_at(v.bank_msb == BANK_MSB_GM2 ? ::xg::rom_position::ICON_GM2 : ::xg::rom_position::ICON_PLUGIN_STYLE, rows))
				return;
		} else {
			const int family_msb = v.gs ? 0 : v.bank_msb;
			const u32 record = family_msb >= 126 ? 0 : voices.lookup(v.gs ? 0 : 1, v.voice_set, v.bank_msb, v.bank_lsb, v.program);
			if (!voices.icon_of(record, family_msb, v.program, rows))
				return;
		}
		for (int y = 0; y < 16; y++) {
			for (int c = 0; c < 3; c++)
				out.cgram[(y < 8 ? c : 4 + c) * 8 + (y & 7)] = u8((rows[y] >> (11 - c * 5)) & 0x1f);
			out.cgram[(y < 8 ? 3 : 7) * 8 + (y & 7)] = u8((rows[y] & 1) << 4);
		}
	}

	static u32 fixed_icon(const view &v)
	{
		if (v.detail)
			return rom_at::ICON_MIDI_PLUG;
		if (v.screen == screen_page && v.page == page_insertion)
			return rom_at::ICON_INSERTION + u32(v.fx_block & 3) * 32;
		if (v.screen == screen_page && v.page == page_variation && !v.variation_system)
			return rom_at::ICON_VARIATION;
		if (v.screen == screen_page && v.page == page_master_eq)
			return rom_at::ICON_MASTER_EQ;
		if ((v.screen == screen_confirm || v.screen == screen_message) && (v.screen == screen_confirm ? v.confirm <= confirm_dump_perform : v.message == MESSAGE_TRANSMITTING || v.message == MESSAGE_CANCELED))
			return rom_at::ICON_DUMP_CONFIRM;
		return 0;
	}

	static u32 screen_icon(const view &v)
	{
		if (v.performance && (v.screen == screen_play || (v.screen == screen_menu && v.menu == menu_performance)))
			return ::xg::rom_position::ICON_PERFORMANCE;
		switch (v.screen) {
		case screen_play: return 0;
		case screen_menu:
			switch (v.menu) {
			case menu_edit: return 0;
			case menu_util: return ::xg::rom_position::ICON_UTIL;
			case menu_dump: return ::xg::rom_position::ICON_DUMP;
			case menu_init: return ::xg::rom_position::ICON_INIT;
			default: return ::xg::rom_position::ICON_EFFECT;
			}
		case screen_sound_module: return v.mode_cursor == 0 ? ::xg::rom_position::ICON_SOUND_MODULE : v.mode_cursor == 1 ? ::xg::rom_position::ICON_SOUND_MODULE_GS : ::xg::rom_position::ICON_SOUND_MODULE_PFM;
		case screen_confirm: return v.confirm <= confirm_dump_perform ? ::xg::rom_position::ICON_DUMP : ::xg::rom_position::ICON_INIT_CONFIRM;
		case screen_performance_name: case screen_performance_copy: case screen_performance_store: case screen_performance_recall: return ::xg::rom_position::ICON_INIT_CONFIRM;
		case screen_message: return v.message >= MESSAGE_CARD_FIRST ? ::xg::rom_position::ICON_CARD_RUNNING : v.message == MESSAGE_EXECUTING ? ::xg::rom_position::ICON_INIT_CONFIRM : ::xg::rom_position::ICON_DUMP;
		case screen_page:
			switch (v.page) {
			case page_system: return ::xg::rom_position::ICON_SYSTEM;
			case page_card: return ::xg::rom_position::ICON_CARD;
			case page_reverb: return ::xg::rom_position::ICON_REVERB;
			case page_chorus: return ::xg::rom_position::ICON_CHORUS;
			case page_variation: return ::xg::rom_position::ICON_VARIATION;
			case page_insertion: return ::xg::rom_position::ICON_EFFECT;
			case page_master_eq: return ::xg::rom_position::ICON_EFFECT;
			default: return 0;
			}
		}
		return 0;
	}

	// 決まった形のセグメント。doc/lcd-segments.md の表（外字 3・7 の下位 4 ビット）
	static void segment(lcd_frame &out, int row, int column, bool on)
	{
		if (!on)
			return;
		const int character = row < 8 ? 3 : 7;
		out.cgram[character * 8 + (row & 7)] |= u8(8 >> column);
	}

	static void compose_segments(const view &v, lcd_frame &out)
	{
		enum { A, B, C, D };
		const bool play = v.screen == screen_play;
		// 札 MIC と LINE は、いまの A/D の品書きの入力（バンク 1・2 以外は MIC）
		const int ad_bank = v.part >= MIDI_PARTS ? v.bank_lsb : v.ad1_bank;
		segment(out, 1, D, !v.performance && ad_bank != AD_BANK_LINE && ad_bank != AD_BANK_AUDIO);
		segment(out, 2, D, ad_bank == AD_BANK_LINE || ad_bank == AD_BANK_AUDIO);
		const bool gs = v.screen == screen_sound_module ? v.mode_cursor == 1 : v.gs;
		const bool performance = v.screen == screen_sound_module && v.mode_cursor == 2;
		segment(out, 5, B, !gs && !performance && !v.performance);   // XG
		segment(out, 4, A, gs);                          // GS
		segment(out, 5, A, performance || v.performance);            // PERFORM
		if (common_screen(v))
			return;
		const bool voice = play && v.item == item_voice;
		segment(out, 3, D, true);                        // A1 A2
		segment(out, 4, D, true);                        // 1-32
		// 札「BANK」「PGM#」は音色の番号が出ている側だけ
		segment(out, 0, D, voice && v.layout == layout_header && !v.detail && !v.performance_names);
		segment(out, 5, D, voice && v.layout == layout_voice && !v.detail && !v.performance_names);
		// いまの項目のカーソルは点滅する
		const bool cursor = play && !v.blink_off;
		segment(out, 0, C, cursor && voice && v.field != field_bank);
		segment(out, 1, C, cursor && voice && v.field != field_program);
		// PFM の項目には Expression がなく、Volume の次から 1 つずつ前に詰まっている
		const int cursor_item = v.performance && v.item > item_volume ? v.item + 1 : v.item;
		segment(out, 3, C, cursor && cursor_item == item_volume);
		segment(out, 4, C, cursor && cursor_item == item_expression);
		segment(out, 5, C, cursor && cursor_item == item_pan);
		segment(out, 6, C, cursor && cursor_item == item_reverb);
		segment(out, 7, C, cursor && cursor_item == item_chorus);
		segment(out, 6, D, cursor && cursor_item == item_variation);
		segment(out, 7, D, cursor && cursor_item == item_note_shift);

		// ノートシフト。中心は 0x40。A/D パートには無い
		// PFM の全体の面では SysTran
		const bool whole_performance = v.performance && !v.performance_focus;
		const int shift = (whole_performance ? int(v.performance_transpose) : int(v.note_shift)) - 0x40;
		if (whole_performance || v.part < MIDI_PARTS) {
		const int size = std::abs(shift), tens = size / 10, ones = size % 10;
		segment(out, 0, A, shift >= 0);
		segment(out, 0, B, true);
		static constexpr u8 SEVEN[10] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f };   // bit0=a ... bit6=g
		const u8 one = SEVEN[ones];
		segment(out, 6, B, one & 0x01);                  // a
		segment(out, 6, A, one & 0x02);                  // b
		segment(out, 3, A, one & 0x04);                  // c
		segment(out, 2, A, one & 0x08);                  // d
		segment(out, 2, B, one & 0x10);                  // e
		segment(out, 7, B, one & 0x20);                  // f
		segment(out, 3, B, one & 0x40);                  // g
		segment(out, 1, A, tens == 2);                   // 十の位 a/d/e/g
		segment(out, 1, B, tens >= 1);                   // 十の位 b
		segment(out, 7, A, tens == 1);                   // 十の位 c
		}

		// 送りの扇（8 段）: 1 / 17 / 33 ... で 1 段ずつ
		const auto fan = [&](int value, int column) {
			const int steps = (value + 15) / 16;
			for (int step = 0; step < 8; step++)
				segment(out, 15 - step, column, step < steps);
		};
		// PFM の全体の面ではパートではなく Performance の RevRtn・ChoRtn・VarRtn と Pfm Pan
		fan(whole_performance ? v.performance_reverb : v.reverb, A);
		fan(whole_performance ? v.performance_chorus : v.chorus, B);
		fan(whole_performance ? v.performance_variation : v.variation, C);
		// パンの 7 か所
		const int pan = whole_performance ? v.performance_pan : v.pan;
		const int pan_row = pan < 22 ? 15 : pan < 43 ? 14 : pan < 64 ? 13 : pan == 64 ? 12 : pan < 86 ? 11 : pan < 107 ? 10 : 9;
		segment(out, pan_row, D, true);
	}

	// 棒の目盛り。layout ごとの刻みと、パート 2 つで 1 マス
	static int bar_level(const view &v, int part, int divisor)
	{
		if (!v.bars_are_meter && part == v.part && v.blink_off)
			return 0;
		if (v.screen == screen_play) {
			if (v.solo ? part != v.solo_part : (part < MIDI_PARTS ? (v.muted >> part & 1) != 0 : (v.muted_ad >> (part - MIDI_PARTS) & 1) != 0))
				return 0;
		}
		int value = v.bars[part];
		if (v.screen == screen_play && v.item == item_note_shift && !v.bars_are_meter) {
			// ノートシフトは ±24 を目盛りいっぱいに
			value = std::max(value - NOTE_SHIFT_LOWEST, 0);
			divisor = divisor == DIVISOR_TWO_ROWS ? 3 : divisor == DIVISOR_ONE_ROW ? 6 : 7;
		}
		return value / divisor + 1;
	}

	static u8 bar_code(int left, int right)
	{
		return left == 0 && right == 0 ? u8(' ') : u8(BAR_BASE + left * BAR_STEP + right);
	}

	static void bars_one_row(const view &v, u8 *row, int divisor)
	{
		// 0 桁目が A/D パート、1-16 桁目がいまのパートの側の 32 パート（A・B か C・D）
		row[0] = bar_code(std::min(bar_level(v, MIDI_PARTS, divisor), 8), std::min(bar_level(v, MIDI_PARTS + 1, divisor), 8));
		const int first = divisor == DIVISOR_ALL_PARTS || v.part >= MIDI_PARTS ? 0 : v.part / 32 * 32;
		for (int cell = 0; cell < 16; cell++)
			row[1 + cell] = bar_code(std::min(bar_level(v, first + cell * 2, divisor), 8), std::min(bar_level(v, first + cell * 2 + 1, divisor), 8));
	}

	static void bars_two_rows(const view &v, lcd_frame &out)
	{
		// いまのパートの口の 16 パートを、上下 2 行で 16 段に
		const bool pan = v.screen == screen_play && v.item == item_pan;
		const int port_first = (v.part >= MIDI_PARTS ? 0 : v.part / 16) * 16;
		const auto bar_part = [&](int cell, int side) { return port_first + cell * 2 + side; };
		const auto put_cell = [&](int column, int left_part, int right_part) {
			if (pan) {
				const int depth_left = pan_depth(v, left_part), depth_right = pan_depth(v, right_part);
				out.text[1][column] = depth_left || depth_right ? u8(PAN_BAR_BASE + depth_left * PAN_BAR_STEP + depth_right) : u8(' ');
				const int top_left = pan_rise(v, left_part), top_right = pan_rise(v, right_part);
				out.text[0][column] = top_left || top_right ? u8(BAR_BASE + top_left * BAR_STEP + top_right) : u8(' ');
				return;
			}
			const int left = bar_level(v, left_part, DIVISOR_TWO_ROWS), right = bar_level(v, right_part, DIVISOR_TWO_ROWS);
			out.text[1][column] = bar_code(std::min(left, 8), std::min(right, 8));
			const int top_left = std::max(left - 8, 0), top_right = std::max(right - 8, 0);
			out.text[0][column] = top_left || top_right ? u8(BAR_BASE + top_left * BAR_STEP + top_right) : u8(' ');
		};
		put_cell(0, MIDI_PARTS, MIDI_PARTS + 1);
		for (int cell = 0; cell < 8; cell++)
			put_cell(1 + cell, bar_part(cell, 0), bar_part(cell, 1));
	}

	static int pan_depth(const view &v, int part)
	{
		if (part == v.part && v.blink_off)
			return 0;
		if (v.solo ? part != v.solo_part : (part < MIDI_PARTS ? (v.muted >> part & 1) != 0 : (v.muted_ad >> (part - MIDI_PARTS) & 1) != 0))
			return 0;
		const int pan = v.bars[part];
		return pan < 16 ? 6 : pan < 28 ? 5 : pan < 40 ? 4 : pan < 52 ? 3 : pan < 64 ? 2 : 1;
	}

	static int pan_rise(const view &v, int part)
	{
		const int pan = v.bars[part];
		return pan <= 64 ? 0 : pan < 77 ? 1 : pan < 89 ? 2 : pan < 101 ? 3 : pan < 114 ? 4 : 5;
	}

	static void bars_all_parts(const view &v, lcd_frame &out)
	{
		// 下の行に A/D とパート 1-32、上の行にパート 33-64
		bars_one_row(v, out.text[1], DIVISOR_ALL_PARTS);
		for (int cell = 0; cell < 16; cell++)
			out.text[0][1 + cell] = bar_code(std::min(bar_level(v, 32 + cell * 2, DIVISOR_ALL_PARTS), 8), std::min(bar_level(v, 33 + cell * 2, DIVISOR_ALL_PARTS), 8));
	}

	static std::string play_label(const ::xg::voice_rom &voices, u8 item)
	{
		static constexpr ::xg::rom_text LABELS[play_item_count] = { {}, rom_at::LABELS_PLAY[0], rom_at::LABELS_PLAY[1], rom_at::LABELS_PLAY[2], rom_at::LABELS_PLAY_SEND[0], rom_at::LABELS_PLAY_SEND[1], rom_at::LABELS_PLAY_SEND[2], rom_at::LABELS_PLAY_SEND[3] };
		return voices.text(LABELS[item]);
	}

	static void play_value_text(const view &v, const ::xg::voice_rom &voices, char *text, size_t size)
	{
		switch (v.item) {
		case item_volume: std::snprintf(text, size, "%03d", v.volume); break;
		case item_expression: std::snprintf(text, size, "%03d", v.expression); break;
		case item_pan: format_pan_text(voices, v.pan, text, size); break;
		case item_reverb: std::snprintf(text, size, "%03d", v.reverb); break;
		case item_chorus: std::snprintf(text, size, "%03d", v.chorus); break;
		case item_variation:
			if (v.variation_system) std::snprintf(text, size, "%03d", v.variation);
			else std::snprintf(text, size, "%s", voices.value(rom_at::TEXTS_ON_OFF[v.variation ? 1 : 0]).c_str());
			break;
		default: std::snprintf(text, size, "%+03d", int(v.note_shift) - 0x40); break;
		}
	}

	static void format_pan_text(const ::xg::voice_rom &voices, int pan, char *text, size_t size)
	{
		std::snprintf(text, size, "%s", voices.value(rom_at::TEXTS_PAN[std::clamp(pan, 0, 127)]).c_str());
	}

	// いまのパートをミュートしていると、項目の矢印が左を向く
	static u8 muted_arrow(const view &v)
	{
		if (v.performance_muted)
			return CH_LEFT;
		if (v.solo)
			return v.part == v.solo_part ? CH_RIGHT : CH_LEFT;
		const bool muted = v.part < MIDI_PARTS ? (v.muted >> v.part & 1) != 0 : (v.muted_ad >> (v.part - MIDI_PARTS) & 1) != 0;
		return muted ? CH_LEFT : CH_RIGHT;
	}

	static void compose_voice_numbers(const view &v, const ::xg::voice_rom &voices, u8 *at)
	{
		char text[4];
		const bool muted = muted_arrow(v) == CH_LEFT;
		// A/D パートに全音色の欄は無く、バンクの欄として出る
		const voice_field field = v.part >= MIDI_PARTS && v.field == field_every_voice ? field_bank : v.field;
		at[0] = field == field_program || muted ? CH_LEFT : CH_RIGHT;
		if (v.bank_msb == 64)
			std::snprintf(text, sizeof(text), "%s", voices.text(rom_at::TEXT_BANK_SFX).c_str());
		else if (v.gs && v.part < MIDI_PARTS && v.part_mode != 0)
			std::snprintf(text, sizeof(text), "000");   // GS のキットにバンクの番号は無い
		else
			std::snprintf(text, sizeof(text), "%03d", v.part < MIDI_PARTS && (v.bank_msb >= 126 || v.gs) ? v.bank_msb : v.bank_lsb);
		put(at + 1, 3, text);
		// バンクの欄か、いまのパートをミュートしていると左向き
		at[4] = v.fallback_voice && muted ? CH_SILENT_PART : field == field_bank ? CH_LEFT : v.fallback_voice ? CH_END : muted ? CH_LEFT : CH_RIGHT;
		std::snprintf(text, sizeof(text), "%03d", v.program + 1);
		put(at + 5, 3, text);
	}

	static void voice_name(const view &v, const ::xg::voice_rom &voices, u8 *at)
	{
		if (v.part >= MIDI_PARTS) {
			put(at, 8, voices.value(panel_fx::ad_preset_name(v.bank_lsb, v.ad_preset)));
			return;
		}
		if (v.gs && v.part_mode != 0) {
			put(at, 8, voices.tg300b_kit_name(v.program));
			return;
		}
		if (v.bank_msb == 120 && v.part_mode != 0) {
			if (const int index = gm_kit_index(v.program); index >= 0)
				put(at, 8, voices.text(rom_at::NAMES_GM_KIT[index]));
			return;
		}
		const int family_msb = v.gs ? 0 : v.bank_msb;
		const u32 record = family_msb >= 126 ? 0 : voices.lookup(v.gs ? 0 : 1, v.voice_set, v.bank_msb, v.bank_lsb, v.program);
		put(at, 8, voices.screen_name(record, family_msb, v.program).c_str());
	}

	static void compose_play_detail(const view &v, const ::xg::voice_rom &voices, lcd_frame &out)
	{
		// CC とその値、右に画面の値
		static constexpr u8 CONTROL[play_item_count] = { 0, 7, 11, 10, 91, 93, 94, 0 };
		char line[24], shown[8];
		if (v.part >= MIDI_PARTS) {
			// A/D パートは CC を受けない。音色の欄だけ SysEx になる
			if (v.item != item_voice) {
				put(out.text[0], 17, voices.text(rom_at::TEXT_RECEIVE_OFF));
				return;
			}
			std::snprintf(line, sizeof(line), "=F0 43104C 10%02X%02X", v.part - MIDI_PARTS, v.field == field_program ? 3 : 2);
			put(out.text[0], 17, line);
			std::snprintf(line, sizeof(line), " %02X F7", v.field == field_program ? v.program : v.bank_lsb);
			put(out.text[1], 17, line);
			return;
		}
		if (v.item == item_voice && v.field != field_program && v.gs) {
			std::snprintf(line, sizeof(line), "MIDI    %cBank=%03d", muted_arrow(v), v.bank_msb);
			put(out.text[0], 17, line);
			std::snprintf(line, sizeof(line), "=000 %03d%cPGM#=%03d", v.bank_msb, muted_arrow(v), v.program + 1);
			put(out.text[1], 17, line);
			return;
		}
		if (v.item == item_voice && v.field != field_program) {
			std::snprintf(line, sizeof(line), "MSB=000 %03d %cPGM#", v.bank_msb, muted_arrow(v));
			put(out.text[0], 17, line);
			std::snprintf(line, sizeof(line), "LSB=032 %03d  =%03d", v.bank_lsb, v.program + 1);
			put(out.text[1], 17, line);
			return;
		}
		put(&out.text[0][1], 4, voices.text(rom_at::TEXT_MIDI));
		out.text[0][9] = muted_arrow(v);
		if (v.item == item_voice) {
			put(&out.text[0][10], 7, voices.text(rom_at::TEXT_PROGRAM));
			std::snprintf(line, sizeof(line), " =%03d       =%03d", v.program, v.program + 1);
			put(out.text[1], 17, line);
			return;
		}
		put(&out.text[0][10], 7, play_label(voices, v.item));
		play_value_text(v, voices, shown, sizeof(shown));
		std::snprintf(line, sizeof(line), " =%03d %03d   =%s", CONTROL[v.item], play_raw_value(v), shown);
		put(out.text[1], 17, line);
	}

	static int play_raw_value(const view &v)
	{
		switch (v.item) {
		case item_volume: return v.volume;
		case item_expression: return v.expression;
		case item_pan: return v.pan;
		case item_reverb: return v.reverb;
		case item_chorus: return v.chorus;
		case item_variation: return v.variation;
		default: return v.note_shift;
		}
	}

	static bool performance_part_silent(const view &v, int part)
	{
		return v.performance_muted || (v.solo ? part != v.solo_part : (v.muted >> part & 1) != 0);
	}

	static void put_model_59(lcd_frame &out, int high, int middle, int low, int first, int second = -1)
	{
		char line[24];
		std::snprintf(line, sizeof(line), "=F0 431059 %02X%02X%02X", high, middle, low);
		put(out.text[0], 17, line);
		if (second < 0)
			std::snprintf(line, sizeof(line), " %02X F7", first);
		else
			std::snprintf(line, sizeof(line), " %02X%02X F7", first, second);
		put(out.text[1], 17, line);
	}

	static void compose_performance_detail(const view &v, const ::xg::voice_rom &voices, lcd_frame &out)
	{
		char line[24];
		if (v.performance_focus) {
			if (v.item != item_voice)
				put_model_59(out, 0x09, v.part, native_engine::performance_part_address(PERFORMANCE_PART_ITEM[v.item]), v.performance_part_item);
			else if (v.field == field_every_voice) {
				put(out.text[0], 17, voices.text(rom_at::TEXT_SELECT_BANK));
				put(out.text[1], 17, voices.text(rom_at::TEXT_SELECT_PROGRAM));
			} else if (v.field == field_bank)
				put_model_59(out, 0x09, v.part, native_engine::performance_part_address(0x22), v.bank_msb, v.bank_lsb);
			else
				put_model_59(out, 0x09, v.part, 0x00, v.program);
			return;
		}
		if (v.item == performance_item_transpose) {
			put(out.text[0], 17, "=F0 431049 000007");
			std::snprintf(line, sizeof(line), " %02X F7", v.performance_transpose);
			put(out.text[1], 17, line);
			return;
		}
		if (v.item >= item_pan) {
			static constexpr int RETURNS[] = { native_engine::PERF_REVERB_RETURN, native_engine::PERF_CHORUS_RETURN, native_engine::PERF_VARIATION_RETURN };
			put_model_59(out, 0x0b, 0x00, native_engine::performance_common_address(RETURNS[v.item - item_pan]), performance_raw_value(v));
			return;
		}
		static constexpr ::xg::rom_text LABELS[] = { rom_at::TEXT_PROGRAM, rom_at::LABELS_PERFORMANCE_DETAIL[0], rom_at::LABELS_PERFORMANCE_DETAIL[1] };
		static constexpr u8 CONTROL[] = { 0, 7, 10 };
		put(&out.text[0][1], 4, voices.text(rom_at::TEXT_MIDI));
		out.text[0][9] = v.performance_muted ? CH_LEFT : CH_RIGHT;
		if (v.item == item_voice && v.field == field_bank) {
			put(&out.text[0][10], 7, voices.text(rom_at::TEXT_BANK));
			std::snprintf(line, sizeof(line), " =000 %03d   =%s", v.performance_bank + 1, voices.text(rom_at::TEXTS_PERFORMANCE_BANK[v.performance_bank == 0 ? 0 : 1]).c_str());
			put(out.text[1], 17, line);
			return;
		}
		put(&out.text[0][10], 7, voices.text(LABELS[v.item]));
		if (v.item == item_voice)
			std::snprintf(line, sizeof(line), " =%03d       =%03d", v.performance_number, v.performance_number + 1);
		else {
			char shown[8];
			performance_value_text(v, voices, shown, sizeof(shown));
			std::snprintf(line, sizeof(line), " =%03d %03d   =%s", CONTROL[v.item], performance_raw_value(v), shown);
		}
		put(out.text[1], 17, line);
	}

	static void compose_performance(const view &v, const ::xg::voice_rom &voices, lcd_frame &out)
	{
		char line[24];
		if (v.detail) {
			compose_performance_detail(v, voices, out);
			return;
		}
		if (v.performance_names) {
			for (int part = 0; part < native_engine::PERFORMANCE_PARTS; part++) {
				u8 *at = &out.text[part / 2][part % 2 * 9];
				const bool silent = performance_part_silent(v, part);
				// 選んでいるパートの印は点滅する
				at[0] = v.performance_focus && part == v.part && !v.blink_off ? (silent ? CH_LEFT : CH_RIGHT) : silent ? CH_SILENT_PART : CH_END;
				const u8 *voice = v.performance_voice[part];
				const u32 record = voice[0] >= 126 ? 0 : voices.lookup(1, v.voice_set, voice[0], voice[1], voice[2]);
				const std::string name = voices.screen_name(record, voice[0], voice[2]);
				put(at + 1, 7, name.c_str());
			}
			return;
		}
		// 4 パートの棒は下の行の 1-2 桁
		out.text[1][1] = bar_code(performance_bar(v, 0), performance_bar(v, 1));
		out.text[1][2] = bar_code(performance_bar(v, 2), performance_bar(v, 3));
		const u8 bank_mark = v.performance_bank == 0 ? 0x1d : 0x1e;
		out.text[0][0] = bank_mark;
		std::snprintf(line, sizeof(line), "%03d", v.performance_number + 1);
		put(&out.text[0][1], 3, line);
		char shown[8];
		if (v.performance_focus) {
			if (v.item == item_voice) {
				voice_name(v, voices, &out.text[0][9]);
				compose_voice_numbers(v, voices, &out.text[1][9]);
				return;
			}
			static constexpr ::xg::rom_text PART_LABELS[] = { {}, rom_at::LABELS_PERFORMANCE_PART[0], rom_at::LABELS_PERFORMANCE_PART[1], rom_at::LABELS_PERFORMANCE_PART[2], rom_at::LABELS_PERFORMANCE_PART[3], rom_at::LABELS_PERFORMANCE_PART[4], rom_at::LABELS_PERFORMANCE_PART[5] };
			out.text[0][9] = muted_arrow(v);
			put(&out.text[0][10], 7, voices.text(PART_LABELS[v.item]));
			out.text[1][9] = '=';
			if (v.item == item_expression)
				format_pan_text(voices, v.performance_part_item, shown, sizeof(shown));
			else if (v.item == performance_item_transpose)
				std::snprintf(shown, sizeof(shown), "%+03d", int(v.performance_part_item) - 0x40);
			else
				std::snprintf(shown, sizeof(shown), "%03d", v.performance_part_item);
			put(&out.text[1][10], 6, shown);
			return;
		}
		if (v.item == item_voice) {
			out.text[0][4] = ':';
			for (int i = 0; i < native_engine::PERF_NAME_SIZE; i++)
				out.text[0][5 + i] = v.performance_name[i];
			out.text[1][9] = v.field == field_program || v.performance_muted ? CH_LEFT : CH_RIGHT;
			put(&out.text[1][10], 3, voices.text(rom_at::TEXTS_PERFORMANCE_BANK[v.performance_bank == 0 ? 0 : 1]));
			out.text[1][13] = v.field == field_bank || v.performance_muted ? CH_LEFT : CH_RIGHT;
			put(&out.text[1][14], 3, line);
			return;
		}
		static constexpr ::xg::rom_text LABELS[] = { {}, rom_at::LABELS_PERFORMANCE[0], rom_at::LABELS_PERFORMANCE[1], rom_at::LABELS_PERFORMANCE[2], rom_at::LABELS_PERFORMANCE[3], rom_at::LABELS_PERFORMANCE[4], rom_at::LABELS_PERFORMANCE[5] };
		out.text[0][9] = v.performance_muted ? CH_LEFT : CH_RIGHT;
		put(&out.text[0][10], 7, voices.text(LABELS[v.item]));
		out.text[1][9] = '=';
		performance_value_text(v, voices, shown, sizeof(shown));
		put(&out.text[1][10], 6, shown);
	}

	static int performance_bar(const view &v, int part)
	{
		if (performance_part_silent(v, part))
			return 0;
		// 選んだパートの棒は項目を動かすあいだ点滅する
		if (v.performance_focus && v.item != item_voice && part == v.part && v.blink_off)
			return 0;
		const int value = v.item == item_voice || v.performance_focus ? v.bars[part] : performance_raw_value(v);
		return std::min(value / DIVISOR_ONE_ROW + 1, 8);
	}

	static int performance_raw_value(const view &v)
	{
		switch (v.item) {
		case item_volume: return v.performance_volume;
		case item_expression: return v.performance_pan;
		case item_pan: return v.performance_reverb;
		case item_reverb: return v.performance_chorus;
		case item_chorus: return v.performance_variation;
		default: return v.performance_transpose;
		}
	}

	static void performance_value_text(const view &v, const ::xg::voice_rom &voices, char *text, size_t size)
	{
		if (v.item == item_expression)
			format_pan_text(voices, v.performance_pan, text, size);
		else if (v.item == performance_item_transpose)
			std::snprintf(text, size, "%+03d", int(v.performance_transpose) - 0x40);
		else
			std::snprintf(text, size, "%03d", performance_raw_value(v));
	}

	static void compose_play(const view &v, const ::xg::voice_rom &voices, lcd_frame &out)
	{
		char value[8];
		if (v.performance) {
			compose_performance(v, voices, out);
			return;
		}
		if (v.detail) {
			compose_play_detail(v, voices, out);
			return;
		}
		if (v.layout == layout_all_parts) {
			bars_all_parts(v, out);
			return;
		}
		if (v.layout == layout_header) {
			bars_one_row(v, out.text[1], DIVISOR_ONE_ROW);
			if (v.item == item_voice) {
				compose_voice_numbers(v, voices, &out.text[0][0]);
				voice_name(v, voices, &out.text[0][9]);
			} else {
				out.text[0][0] = muted_arrow(v);
				put(&out.text[0][1], 7, play_label(voices, v.item));
				out.text[0][9] = '=';
				play_value_text(v, voices, value, sizeof(value));
				put(&out.text[0][10], 6, value);
			}
			return;
		}
		bars_two_rows(v, out);
		if (v.item == item_voice) {
			voice_name(v, voices, &out.text[0][9]);
			compose_voice_numbers(v, voices, &out.text[1][9]);
		} else {
			out.text[0][9] = muted_arrow(v);
			put(&out.text[0][10], 7, play_label(voices, v.item));
			out.text[1][9] = '=';
			play_value_text(v, voices, value, sizeof(value));
			put(&out.text[1][10], 6, value);
		}
	}

	static std::string menu_row(const ::xg::voice_rom &voices, ::xg::rom_text row)
	{
		std::string s = voices.text(row);
		std::replace(s.begin(), s.end(), '*', char(0x14));
		return s;
	}

	static void compose_menu(const view &v, const ::xg::voice_rom &voices, lcd_frame &out)
	{
		const bool drum = v.part < MIDI_PARTS && v.part_mode != 0;
		const menu_layout &layout = menu_of(v.menu, drum, v.performance, v.gs);
		put(out.text[0], 17, menu_row(voices, layout.rows[0]));
		// パフォーマンスのインサーションは 2 つだけ
		if (!(v.performance && v.menu == menu_insertion))
			put(out.text[1], 17, menu_row(voices, layout.rows[1]));
		if (v.blink_off)
			out.text[layout.cursor_row[v.menu_cursor]][layout.cursor_column[v.menu_cursor]] = ' ';
	}

	static void page_markers(u8 *row, int item, int count)
	{
		row[0] = item == 0 ? CH_END : CH_ARROW_RIGHT;
		row[16] = item == count - 1 ? CH_END : CH_END_LEFT;
	}

	static void format_value(const ::xg::voice_rom &voices, const part_param &p, int value, char *text, size_t size)
	{
		switch (p.format) {
		case format_signed_center: std::snprintf(text, size, "%+03d", value - 64); break;
		case format_decimal3: std::snprintf(text, size, "%03d", value); break;
		case format_decimal2: std::snprintf(text, size, "%02d", value); break;
		case format_decimal_plain: if (value) std::snprintf(text, size, "%d", value);
			else std::snprintf(text, size, "%s", voices.value(rom_at::TEXTS_ON_OFF[0]).c_str());
			break;
		case format_receive_channel:
			if (value < 0) std::snprintf(text, size, "%s", voices.value(rom_at::TEXTS_ON_OFF[0]).c_str());
			else std::snprintf(text, size, "%c%02d", 'A' + value / 16, value % 16 + 1);
			break;
		case format_detune: {
			const int tenths = value - 128;
			std::snprintf(text, size, "%c%02d.%d", tenths < 0 ? '-' : '+', std::abs(tenths) / 10, std::abs(tenths) % 10);
			break;
		}
		case format_part_mode: std::snprintf(text, size, "%s", voices.value(rom_at::TEXTS_PART_MODE[std::clamp(value, 0, 5)]).c_str()); break;
		case format_mono_poly: std::snprintf(text, size, "%s", voices.value(rom_at::TEXTS_MONO_POLY[value ? 1 : 0]).c_str()); break;
		case format_on_off: std::snprintf(text, size, "%s", voices.value(rom_at::TEXTS_ON_OFF[value ? 1 : 0]).c_str()); break;
		case format_note: std::snprintf(text, size, "%s", voices.value(rom_at::TEXTS_NOTE[std::clamp(value, 0, 127)]).c_str()); break;
		case format_bass_frequency: std::snprintf(text, size, "%s", voices.value(rom_at::FX_FREQUENCY[std::clamp(value, 4, 40)]).c_str()); break;
		case format_treble_frequency: std::snprintf(text, size, "%s", voices.value(rom_at::FX_FREQUENCY[std::clamp(value, 28, 58)]).c_str()); break;
		case format_pan: format_pan_text(voices, value, text, size); break;
		case format_bend_range: std::snprintf(text, size, "%+03d", value - 0x40); break;
		case format_master_tune: {
			const int tenths = value - 1024;
			std::snprintf(text, size, "%c%03d.%d", tenths < 0 ? '-' : '+', std::abs(tenths) / 10, std::abs(tenths) % 10);
			break;
		}
		case format_device:
			if (value >= 16) std::snprintf(text, size, "%s", voices.text(rom_at::TEXT_DEVICE_ALL).c_str());
			else std::snprintf(text, size, "%02d", value + 1);
			break;
		case format_channel_plus1: std::snprintf(text, size, "%02d", value + 1); break;
		case format_midi_in: std::snprintf(text, size, "%s", voices.value(rom_at::TEXTS_MIDI_IN[value ? 1 : 0]).c_str()); break;
		case format_contrast: case format_thru_port: case format_display_bank: std::snprintf(text, size, "%d", value + 1); break;
		case format_dump_interval: std::snprintf(text, size, "%s", voices.value(rom_at::TEXTS_DUMP_INTERVAL[std::clamp(value, 0, 4)]).c_str()); break;
		case format_map: std::snprintf(text, size, "%s", voices.value(value == MAP_NOT_APPLICABLE ? rom_at::TEXT_MAP_NOT_APPLICABLE : rom_at::TEXTS_MAP[value ? 1 : 0]).c_str()); break;
		case format_out: std::snprintf(text, size, "%s", voices.value(rom_at::TEXTS_OUT[std::clamp(value, 0, 2)]).c_str()); break;
		case format_digital_gain: std::snprintf(text, size, "%s", voices.value(rom_at::TEXTS_DIGITAL_GAIN[std::clamp(value, 0, 2)]).c_str()); break;
		case format_command: text[0] = 0; break;
		case format_ac1_control:
			if (value >= 0x60) std::snprintf(text, size, "%s", voices.text(rom_at::TEXT_CHANNEL_AFTERTOUCH).c_str());
			else std::snprintf(text, size, "%02d", value);
			break;
		}
	}

	static void compose_part_line(const view &v, const ::xg::voice_rom &voices, const part_param &p, int count, u8 *row)
	{
		char value[12];
		page_markers(row, v.page_item, count);
		const std::string field = voices.label(p.field);
		put(row + 1, 15, field);
		format_value(voices, p, v.value, value, sizeof(value));
		put(row + 1 + int(field.size()), 15 - int(field.size()), value);
	}

	static void compose_exclusive(const view &v, const ::xg::voice_rom &voices, lcd_frame &out)
	{
		char line[24];
		if (v.exclusive_count == 0) {
			if (v.page == page_drum || (v.page == page_system && v.performance && v.page_item == SETTING_MAP)) {
				put(out.text[0], 17, voices.text(rom_at::TEXT_NO_PARAMETER));
				return;
			}
			put(out.text[0], 17, voices.text(rom_at::TEXT_THIS_PARAMETER));
			put(out.text[1], 17, voices.text(rom_at::TEXT_NOT_EXCLUSIVE));
			return;
		}
		std::snprintf(line, sizeof(line), "=F0 4310%s %02X%02X%02X", v.exclusive_model_performance ? "59" : v.exclusive_model_mu ? "49" : "4C", v.exclusive_address[0], v.exclusive_address[1], v.exclusive_address[2]);
		put(out.text[0], 17, line);
		int at = 0;
		line[at++] = ' ';
		for (int i = 0; i < v.exclusive_count; i++)
			at += std::snprintf(line + at, sizeof(line) - size_t(at), "%02X", v.exclusive_data[i]);
		std::snprintf(line + at, sizeof(line) - size_t(at), " F7");
		put(out.text[1], 17, line);
	}

	static void compose_page(const view &v, const ::xg::voice_rom &voices, lcd_frame &out)
	{
		if (v.notice) {
			put(out.text[0], 17, voices.text(rom_at::TEXT_SELECT_DRUM));
			put(out.text[1], 17, voices.text(rom_at::TEXT_WHEN_YOU_EDIT));
			return;
		}
		if (v.detail) {
			compose_exclusive(v, voices, out);
			return;
		}
		const param_list list = params_of(v.page, v.performance);
		switch (v.page) {
		case page_filter: case page_eg: case page_eq: case page_vibrato: case page_others:
			compose_part_line(v, voices, list.items[v.page_item], v.part >= MIDI_PARTS ? 1 : list.count, out.text[0]);
			bars_one_row(v, out.text[1], DIVISOR_ONE_ROW);
			out.text[1][0] = ' ';
			// パフォーマンスの棒は 4 パートぶんだけ
			if (v.performance)
				std::memset(&out.text[1][3], ' ', 14);
			return;
		case page_drum: {
			out.text[0][0] = CH_RIGHT;
			put(&out.text[0][1], 3, voices.value(rom_at::TEXTS_NOTE[std::clamp(int(v.drum_key), 0, 127)]));
			out.text[0][4] = ':';
			put(&out.text[0][5], 12, voices.drum_key_name(v.bank_msb, v.program, v.drum_key).c_str());
			compose_part_line(v, voices, list.items[v.page_item], list.count, out.text[1]);
			return;
		}
		case page_performance_common:
			put(out.text[0], 16, voices.text(rom_at::TEXT_COMMON_TITLE));
			compose_part_line(v, voices, list.items[v.page_item], list.count, out.text[1]);
			return;
		case page_system:
			put(out.text[0], 16, voices.text(rom_at::TEXT_SYSTEM_TITLE));
			compose_part_line(v, voices, list.items[v.page_item], list.count, out.text[1]);
			return;
		case page_card:
			put(out.text[0], 16, voices.text(rom_at::TEXT_CARD_TITLE));
			compose_part_line(v, voices, list.items[v.page_item], list.count, out.text[1]);
			return;
		case page_master_eq:
			compose_master_eq(v, voices, out);
			return;
		default:
			compose_fx_page(v, voices, out);
			return;
		}
	}

	static std::string fx_page_name(const view &v, const ::xg::voice_rom &voices)
	{
		if (v.page == page_reverb) return voices.text(rom_at::TEXT_REVERB);
		if (v.page == page_chorus) return voices.text(rom_at::TEXT_CHORUS);
		if (v.page == page_variation) return voices.text(rom_at::TEXT_VARIATION);
		return voices.text(rom_at::TEXTS_CONNECTION[0]) + std::to_string(v.fx_block + 1);
	}

	static void compose_fx_page(const view &v, const ::xg::voice_rom &voices, lcd_frame &out)
	{
		char line[20];
		const std::string page_name = fx_page_name(v, voices);
		const char *name = page_name.c_str();
		const panel_fx::type_table table = panel_fx::find(fx_table_of(v.page), v.effect_type);
		const panel_fx::tail_list tail = panel_fx::tail_of(fx_table_of(v.page), v.variation_system);
		const int shown = visible_count(table, v.performance, fx_table_of(v.page));
		const int count = 1 + shown + tail.count;
		out.text[0][0] = CH_RIGHT;
		const std::string type_name = ::xg::fx_name(v.effect_type);
		const bool assigned_block = v.page == page_variation || v.page == page_insertion;
		if (v.page_item == 0) {
			if (assigned_block) {
				char part_text[8];
				effect_part_text(voices, v.effect_part, part_text, sizeof(part_text));
				// システム接続のバリエーションには掛かり先が無い
				if (v.page == page_variation && v.variation_system)
					std::snprintf(line, sizeof(line), "%s", name);
				else
					std::snprintf(line, sizeof(line), "%s<%s>", name, part_text);
				put(&out.text[0][1], 16, line);
				if (v.page == page_variation)
					put(&out.text[0][12], 5, voices.text(v.variation_system ? rom_at::TEXT_VARIATION_SYSTEM : rom_at::TEXT_VARIATION_INSERTION));
			} else
				put(&out.text[0][1], 16, name);
			page_markers(out.text[1], 0, count);
			put(&out.text[1][1], 5, voices.text(rom_at::TEXT_TYPE_FIELD));
			put(&out.text[1][6], 10, type_name.c_str());
			return;
		}
		std::snprintf(line, sizeof(line), assigned_block ? "%s %s" : "%s:%s", name, type_name.c_str());
		put(&out.text[0][1], 16, line);
		page_markers(out.text[1], v.page_item, count);
		const int index = v.page_item - 1;
		char value[16];
		if (index < shown) {
			const panel_fx::param &p = visible_param(table, index, v.performance, fx_table_of(v.page));
			panel_fx::format(voices, p, v.value, value, sizeof(value));
			const std::string field = voices.label(p.field());
			put(&out.text[1][1], 15, field);
			const int length = int(field.size());
			put(&out.text[1][1 + length], 15 - length, value);
		} else {
			const panel_fx::tail_item &t = tail.items[index - shown];
			panel_fx::format_tail(voices, t, v.value, value, sizeof(value));
			std::string field = voices.label(t.field);
			// インサーションの番号は * に入る
			if (const size_t number = field.find('*'); number != std::string::npos)
				field[number] = char('1' + v.fx_block);
			put(&out.text[1][1], 15, field);
			const int length = int(field.size());
			put(&out.text[1][1 + length], 15 - length, value);
		}
	}

	static void effect_part_text(const ::xg::voice_rom &voices, int part, char *text, size_t size)
	{
		if (part < 0 || part >= 127)
			std::snprintf(text, size, "%s", voices.value(rom_at::TEXTS_ON_OFF[0]).c_str());
		else if (part >= MIDI_PARTS)
			std::snprintf(text, size, "AD%02d", part - MIDI_PARTS + 1);
		else
			std::snprintf(text, size, "PART%02d", part + 1);
	}

	static void eq_bar(u8 &code, int side, int level)
	{
		const int current = code == ' ' ? 0 : code - BAR_BASE;
		code = u8(BAR_BASE + (side == 0 ? level * BAR_STEP + current % BAR_STEP : current / BAR_STEP * BAR_STEP + level));
	}

	static void compose_master_eq(const view &v, const ::xg::voice_rom &voices, lcd_frame &out)
	{
		char line[20];
		out.text[0][0] = CH_RIGHT;
		out.text[1][0] = v.page_item == 0 ? CH_END : CH_ARROW_RIGHT;
		out.text[1][9] = v.page_item == 5 ? CH_END : CH_END_LEFT;
		if (v.page_item == 0) {
			put(&out.text[0][1], 16, "EQ:Type");
			std::snprintf(line, sizeof(line), "=%s", voices.value(rom_at::TEXTS_EQ_TYPE[v.eq_type]).c_str());
		} else {
			std::string frequency = voices.value(rom_at::FX_FREQUENCY[std::clamp(int(v.eq_frequencies[v.page_item - 1]), 0, 52)]);
			frequency.erase(0, frequency.find_first_not_of(' '));
			if (!frequency.empty() && frequency.back() == 'k')
				std::snprintf(line, sizeof(line), "EQ:%.*skHz", int(frequency.size() - 1), frequency.c_str());
			else
				std::snprintf(line, sizeof(line), "EQ:%3s Hz", frequency.c_str());
			put(&out.text[0][1], 16, line);
			std::snprintf(line, sizeof(line), "=%+03ddB", v.value - 64);
		}
		put(&out.text[1][1], 8, line);
		// 5 つの帯域の量を 13 本の棒に。周波数と Q は絵に入らない。帯域のあいだは直線でつなぐ
		// 平らで下の行が満杯（8 段）。持ち上がる分は上の行へ
		const auto peak = [&](int band) {
			const int gain = std::clamp(int(v.eq_gains[band]) - 64, -EQ_MAX_GAIN, EQ_MAX_GAIN);
			return gain < 0 ? -EQ_BAR_OF_GAIN[-gain] : EQ_BAR_OF_GAIN[gain];
		};
		for (int bar = 0; bar < EQ_BARS; bar++) {
			const int band = std::min(bar / EQ_BARS_PER_BAND, 3), t = bar - band * EQ_BARS_PER_BAND;
			const int rise = peak(band + 1) - peak(band);
			// 3 で割って四捨五入（負の側も同じ形）
			const int between = (rise * t + (rise < 0 ? -1 : 1)) / EQ_BARS_PER_BAND;
			const int level = EQ_FLAT_LEVEL + peak(band) + between;
			const int cell = (bar + 1) / 2, side = (bar + 1) % 2;
			eq_bar(out.text[1][10 + cell], side, std::min(level, EQ_FLAT_LEVEL));
			if (level > EQ_FLAT_LEVEL)
				eq_bar(out.text[0][10 + cell], side, level - EQ_FLAT_LEVEL);
		}
	}

	static std::string initialize_title(const view &v, const ::xg::voice_rom &voices)
	{
		return voices.text(v.performance ? rom_at::TEXT_INITIALIZE_PERFORMANCE : v.gs ? rom_at::TEXT_RESET_GS : rom_at::TEXT_INITIALIZE_XG);
	}

	static void compose_confirm(const view &v, const ::xg::voice_rom &voices, lcd_frame &out)
	{
		char line[20];
		switch (v.confirm) {
		case confirm_dump_all: std::snprintf(line, sizeof(line), "%s", voices.text(rom_at::TEXT_DUMP_ALL).c_str()); break;
		case confirm_dump_multi:
			std::snprintf(line, sizeof(line), "%s %s", voices.text(rom_at::TEXT_DUMP_MULTI).c_str(), v.blink_off ? "" : voices.value(rom_at::TEXTS_DUMP_RANGE[v.confirm_choice]).c_str());
			break;
		case confirm_dump_perform:
			if (v.blink_off) std::snprintf(line, sizeof(line), "%s", voices.text(rom_at::TEXT_DUMP_PERFORMANCE).c_str());
			else if (v.confirm_choice == 0) std::snprintf(line, sizeof(line), "%s %s", voices.text(rom_at::TEXT_DUMP_PERFORMANCE).c_str(), voices.text(rom_at::TEXT_ALL_PARTS).c_str());
			else std::snprintf(line, sizeof(line), "%s %c%03d", voices.text(rom_at::TEXT_DUMP_PERFORMANCE).c_str(), char(0x1e), v.confirm_choice);
			break;
		case confirm_factory: std::snprintf(line, sizeof(line), "%s", voices.text(rom_at::TEXT_FACTORY_SET).c_str()); break;
		case confirm_init_xg: std::snprintf(line, sizeof(line), "%s", initialize_title(v, voices).c_str()); break;
		case confirm_init_drum:
			if (v.blink_off) std::snprintf(line, sizeof(line), "%s", voices.text(rom_at::TEXT_INITIALIZE_DRUM).c_str());
			else std::snprintf(line, sizeof(line), "%sS%d", voices.text(rom_at::TEXT_INITIALIZE_DRUM).c_str(), v.confirm_choice + 1);
			break;
		}
		put(out.text[0], 17, line);
		put(out.text[1], 17, voices.text(rom_at::TEXT_ARE_YOU_SURE));
	}

	static void compose_sound_module(const view &v, const ::xg::voice_rom &voices, lcd_frame &out)
	{
		if (v.detail) {
			put(out.text[0], 17, v.mode_cursor == 0 ? "=F0 43104C 00007E" : "=F0 411042 124000");
			put(out.text[1], 17, v.mode_cursor == 0 ? " 00 F7" : " 7F0041 F7");
			return;
		}
		put(out.text[0], 17, voices.text(rom_at::TEXT_SOUND_MODULE));
		put(out.text[1], 17, voices.text(rom_at::TEXT_SOUND_MODULE_CHOICES));
		if (!v.blink_off)
			out.text[1][v.mode_cursor * 6] = CH_RIGHT;
	}

	static void compose_message(const view &v, const ::xg::voice_rom &voices, lcd_frame &out)
	{
		switch (v.message) {
		case MESSAGE_TRANSMITTING:
			put(out.text[0], 17, voices.text(rom_at::TEXT_TRANSMITTING));
			for (int cell = 0; cell < 8; cell++)
				out.text[1][cell] = cell < v.progress ? CH_RIGHT : CH_LEFT;
			break;
		case MESSAGE_EXECUTING: {
			char line[20];
			if (v.executing_title == screen_performance_recall) std::snprintf(line, sizeof(line), "%s%c%03d", voices.text(rom_at::TEXT_RECALL_PERFORMANCE).c_str(), char(v.performance_recall_bank == 0 ? 0x1d : 0x1e), v.performance_recall_number + 1);
			else if (v.executing_title == screen_performance_copy) std::snprintf(line, sizeof(line), "%s", voices.text(rom_at::TEXT_PERFORMANCE_COPY).c_str());
			else if (v.executing_title == screen_performance_store) std::snprintf(line, sizeof(line), "%s%c%03d", voices.text(rom_at::TEXT_STORE_PERFORMANCE).c_str(), char(0x1e), v.store_to + 1);
			else if (v.confirm == confirm_factory) std::snprintf(line, sizeof(line), "%s", voices.text(rom_at::TEXT_FACTORY_SET).c_str());
			else if (v.confirm == confirm_init_xg) std::snprintf(line, sizeof(line), "%s", initialize_title(v, voices).c_str());
			else std::snprintf(line, sizeof(line), "%sS%d", voices.text(rom_at::TEXT_INITIALIZE_DRUM).c_str(), v.confirm_choice + 1);
			put(out.text[0], 17, line);
			if (!v.blink_off)
				put(out.text[1], 17, voices.text(rom_at::TEXT_EXECUTING));
			break;
		}
		case MESSAGE_CARD_FIRST:
			put(out.text[0], 17, "USED:    0KB");
			put(out.text[1], 17, "FREE:    0KB");
			break;
		case MESSAGE_CANCELED:
			put(out.text[0], 17, voices.text(rom_at::TEXT_CANCELED));
			break;
		case MESSAGE_NO_RECALL:
			put(out.text[0], 17, voices.text(rom_at::TEXT_NO_RECALL));
			break;
		case MESSAGE_CARD_FIRST + 4:
			put(out.text[0], 17, voices.text(rom_at::TEXT_FORMAT_CARD));
			put(out.text[1], 17, voices.text(rom_at::TEXT_CARD_NOT_READY));
			break;
		default:
			put(out.text[0], 17, voices.text(rom_at::TEXT_CARD_ROOT));
			put(out.text[1], 17, voices.text(rom_at::TEXT_CARD_NOT_READY));
			break;
		}
	}

	// マスター EQ の絵: 帯域の量（dB）を棒の段へ
	static constexpr int EQ_BARS = 13, EQ_BARS_PER_BAND = 3, EQ_FLAT_LEVEL = 8, EQ_MAX_GAIN = 12;
	static constexpr int EQ_BAR_OF_GAIN[EQ_MAX_GAIN + 1] = { 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 7 };
	static constexpr int PERFORMANCE_ADDRESS_BASE = 0x88;

	static constexpr int BANK_MSB_PLUGIN_STYLE = 48, BANK_MSB_GM2 = 121;

	static constexpr int FX_VARIATION_PART = 0x100 | 0x5b, FX_INSERTION_PART = 0x100 | 0x0c;

	enum setting_index {
		SETTING_DEVICE = 1, SETTING_PERFORMANCE_CHANNEL = 2, SETTING_MIDI_IN = 3, SETTING_AUDITION_VOICE_NOTE = 4, SETTING_AUDITION_DRUM_NOTE = 5, SETTING_AUDITION_VELOCITY = 6,
		SETTING_MUTE_LOCK = 7, SETTING_AD_LOCK = 8, SETTING_EQ_LOCK = 9, SETTING_RECEIVE_SYSTEM_ON = 10, SETTING_RECEIVE_EXCLUSIVE = 11, SETTING_RECEIVE_BANK = 12,
		SETTING_CONTRAST = 13, SETTING_DUMP_INTERVAL = 14, SETTING_THRU_PORT = 15, SETTING_DISPLAY_BANK = 16, SETTING_MAP = 17, SETTING_OUT = 18, SETTING_DIGITAL_GAIN = 19,
	};
	static constexpr int SETTING_ADDRESS[20] = { -1, -1, 0x06, -1, -1, -1, -1, 0x00, 0x01, 0x02, 0x03, -1, 0x04, 0x08, 0x05, 0x09, -1, 0x12, 0x14, 0x15 };
	static constexpr std::array<short, 20> DEFAULT_SETTINGS = {
		0, 16, 0, 0, 60, 38, 100, 0, 0, 0, 1, 1, 1, 1, 2, 0, 0, 1, 0, 0,
	};

	screen_id m_screen = screen_play;
	play_layout m_layout = layout_voice;
	u8 m_item = item_voice;
	voice_field m_field = field_program;
	menu_id m_menu = menu_edit;
	menu_id m_return_menu = menu_edit;
	menu_id m_confirm_return = menu_util;
	page_id m_page = page_filter;
	confirm_id m_confirm = confirm_dump_all;
	int m_confirm_choice = 0;
	u8 m_message = 0;
	u8 m_mode_cursor = 0;
	bool m_mode_cursor_moved = false;
	std::array<u8, menu_count> m_menu_cursor = {};
	std::array<u8, page_count> m_page_item = {};
	settings_array m_settings = DEFAULT_SETTINGS;
	int m_part = 0, m_other_mode_part = 0;
	int m_drum_key = 24;
	int m_fx_block = 0;
	page_id m_last_fx_page = page_reverb;
	int m_last_fx_block = 0;
	u64 m_muted = 0;
	u8 m_muted_ad = 0;
	int m_last_mute_part = -1;
	bool m_solo = false;
	int m_solo_part = 0;
	u8 m_meter_smooth[ALL_PARTS] = {};
	int m_meter_samples = 0;
	u32 m_blink_samples = 0;
	int m_held_button = -1;
	int m_held_samples = 0, m_repeat_at = 0;
	int m_message_samples = 0;
	std::array<u8, 7> m_controller_depth = { 64, 64, 64, 64, 64, 64, 64 };
	std::array<int, AD_PARTS> m_ad_preset = {}, m_ad_bank = {};
	bool m_detail = false, m_notice = false, m_item_kept = false;
	int m_sysex_error = sysex_error_none, m_key_lock = 0;
	u8 m_error_checksum = 0;
	request m_requests[REQUESTS] = {};
	size_t m_request_read = 0, m_request_write = 0;
	int m_enter_samples = DOUBLE_ENTER_WINDOW;
	bool m_performance_names = false, m_performance_muted = false, m_performance_focus = false;
	u32 m_performance_loads = 0, m_external_resets = 0;
	bool m_audition_on = false;
	int m_audition_note = 0, m_audition_part = 0;
	int m_name_cursor = 0, m_copy_bank = 0, m_copy_from = 0, m_copy_to = 0, m_copy_field = 0, m_store_to = 0;
	screen_id m_executing_title = screen_confirm;
	int m_dump_position = 0, m_dump_wait = 0;
	int m_dump_buffered = 0, m_dump_sent = 0;
	int m_transmit_cells = 0;
};

} // namespace smu2000

#endif // S_MU2000_NATIVE_PANEL_H
