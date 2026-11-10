// license:BSD-3-Clause

#ifndef S_MU2000_NATIVE_ENGINE_H
#define S_MU2000_NATIVE_ENGINE_H

#include "compat/platform.h"
#include "native/block_mixer.h"
#include "native/fx_flow_all.h"
#include "native/voice_word_reader.h"
#include "xg/gs.h"
#include "xg/effect_tables.h"
#include "native/panel_fx_tables.h"
#include "xg/native_driver.h"
#include "xg/synth_state.h"
#include "xg/native_voice.h"
#include "xg/voices.h"

#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace smu2000 {

class native_engine
{
public:
	static constexpr int PORTS = 4;
	static constexpr int PORT_PANEL = PORTS;         // パネルが送るもの。受信の設定で止めない

	native_engine()
	{
		m_sampram.assign(0x400000, 0);
		for (int group = 0; group < 32; group++) {
			m_regions[0].flows.by_group[group] = native_fx::make_system_chorus(group);
			m_regions[1].flows.by_group[group] = native_fx::make_variation_effect(group);
			m_regions[2].flows.by_group[group] = native_fx::make_variation_effect(group);
			for (extra_insertion &si : m_extra_insertions)
				si.flows.by_group[group] = native_fx::make_variation_effect(group);
		}
	}

	// prog: プログラム ROM 4MB（表を読むだけ）。wave: 波形 ROM 4 枚（各 8MB、ic49・ic50・ic53・ic54 の順）。
	// LFO のサイン波の 1/4 周期。残りは value_at で折り返す
	void make_sine_table()
	{
		m_sin.resize(0x8000);
		for (size_t i = 0; i < m_sin.size(); i++) {
			const double angle = (double(i) + 0.5) / double(m_sin.size()) * std::acos(0.0);
			m_sin[i] = u16(std::min(65535.0, std::nearbyint(0x8000 + std::sin(angle) * 0x7fff)));
		}
	}

	bool load(std::vector<u8> prog, const std::vector<u8> *const wave[4])
	{
		if (prog.size() != ::xg::rom_position::PROGRAM_ROM_SIZE)
			return false;
		for (int i = 0; i < 4; i++)
			if (!wave[i] || wave[i]->size() != 0x800000)
				return false;
		m_prog = std::make_shared<const std::vector<u8>>(std::move(prog));
		// 4 枚を 32bit の語に交互に並べる（mu2000::load_wave_data と同じ並び）
		m_wave.assign(0x2000000, 0);
		for (int i = 0; i < 4; i++) {
			const size_t base = i >= 2 ? 0x1000000 : 0, off = (i & 1) ? 2 : 0;
			const std::vector<u8> &w = *wave[i];
			for (size_t j = 0; j < w.size(); j += 2) {
				m_wave[base + j * 2 + off] = w[j];
				m_wave[base + j * 2 + off + 1] = w[j + 1];
			}
		}
		make_sine_table();
		// EQ の型 0 は 1 バイトずつ、1-4 は 2 バイトずつ ROM に並ぶ
		std::copy_n(m_prog->data() + ::xg::rom_position::EQ_FLAT_PRESET, 20, m_eq_presets[0]);
		for (int type = 1; type < 5; type++)
			for (int k = 0; k < 20; k++)
				m_eq_presets[type][k] = (*m_prog)[::xg::rom_position::EQ_SHAPED_PRESETS + u32(type - 1) * 40 + u32(k) * 2 + 1];
		m_voices.set_wave_memory(m_wave.data(), m_wave.size(), m_sampram.data(), SAMPLE_RAM_FIRST_WORD, m_sampram.size());
		m_drv.reset();
		m_drv.set_rom(m_prog->data());
		m_drv.set_state(&m_state);
		m_drv.set_poke([this](int voice, int word, u16 value) { m_word_reader.write(voice, word, value); });
		m_drv.set_key_on([this](int group, u64 mask) { m_word_reader.key_on(group, mask); });
		m_drv.set_peg_peek([this](int v) { return m_voices.peg_reached(v); });
		m_drv.set_slot_peek([this](int v) { return m_voices.sounding(v); });
		m_perf_internal.assign(m_prog->begin() + ::xg::rom_position::PERFORMANCE_INTERNAL, m_prog->begin() + ::xg::rom_position::PERFORMANCE_INTERNAL + PERF_INTERNAL_BYTES);
		m_drv.set_slot_held([this](int v) { return m_voices.slot_held(v); });
		m_drv.set_level_peek([this](int v) { return m_voices.eg_level(v); });
		set_eg_phase_now();
		xg_reset();
		lfo_power_on();
		// 電源を入れた直後だけ、バリエーションのパラメーター 12 は 0（リセットで種類の既定へ戻る）
		m_var.request.P[11] = 0;
		return true;
	}

	// 1 バイト受ける。DIN（31250 baud）の線を流れたものとして、受け終えた時刻を数える
	void midi(u8 byte, int port = 0)
	{
		if (port < 0 || port > PORT_PANEL || byte >= 0xf8)
			return;
		u64 &rx = m_rx_at[port];
		rx = std::max(rx, m_clock * RX_UNIT) + RX_BYTE;
		parser &p = m_in[port];
		if (byte & 0x80) {
			if (byte == 0xf0) {
				p.sx_len = 0;
				p.in_sx = true;
			} else if (byte == 0xf7) {
				if (p.in_sx)
					queue(ev_sysex, port, 0, 0, rx, p.sx, p.sx_len);
				p.in_sx = false;
			} else if (byte < 0xf0) {
				p.status = byte;
				p.have = 0;
				p.in_sx = false;
			}
			return;
		}
		if (p.in_sx) {
			// XG のパラメータより長いもの（バルクダンプ）は受けない
			if (p.sx_len < sizeof(p.sx))
				p.sx[p.sx_len++] = byte;
			else
				p.in_sx = false;
			return;
		}
		if (!p.status)
			return;
		const u8 kind = p.status & 0xf0;
		const int chan = port * 16 + (p.status & 0x0f);   // 口 x 16 + チャンネル。聞くパートは効く時に決める（08 pp 04）
		const int need = (kind == 0xc0 || kind == 0xd0) ? 1 : 2;
		p.d[p.have++] = byte;
		if (p.have < need)
			return;
		p.have = 0;
		switch (kind) {
		case 0x80: queue(ev_off, chan, p.d[0], 0, rx); break;
		case 0x90: queue(p.d[1] ? ev_on : ev_off, chan, p.d[0], p.d[1], rx); break;
		case 0xb0: queue(ev_cc, chan, p.d[0], p.d[1], rx); break;
		case 0xc0: queue(ev_prog, chan, p.d[0], 0, rx); break;
		case 0xe0: queue(ev_bend, chan, p.d[0], p.d[1], rx); break;
		case 0xa0: queue(ev_pat, chan, p.d[0], p.d[1], rx); break;
		case 0xd0: queue(ev_cat, chan, p.d[0], 0, rx); break;
		default: break;
		}
	}

	// frames サンプル作る。全振幅は ±1.0。イベントごとにブロックを区切る（doc/voice-flow/README.md）。
	// MIDI の入力（midi()）は、この呼び出しのあいだには入らない。
	// ad_1, ad_2: A/D INPUT（全振幅 ±1.0）。A/D パートの音量を上げると鳴る。無ければ nullptr
	void render(float *left, float *right, int frames, const float *ad_1 = nullptr, const float *ad_2 = nullptr)
	{
		while (frames > 0) {
			// 時計はサンプルの頭で進める（MIDI の時刻は進める前の時計で数えている）
			m_clock++;
			m_drv.set_clock(m_clock - 1);     // 入力は 1 つ前の刻みのあとに届く
			apply_due_events();
			m_drv.tick(m_clock);
			m_lfo.due(m_clock);
			m_extra_insertion_lfo.due(m_clock);
			u64 next = std::min(m_drv.next_tick(), std::min(m_lfo.next_due(), m_extra_insertion_lfo.next_due()));
			for (const fx_region &g : m_regions)
				if (g.active_at > m_clock)
					next = std::min(next, g.active_at);
			for (const extra_insertion &si : m_extra_insertions)
				if (si.active_at > m_clock)
					next = std::min(next, si.active_at);
			if (m_q_count)
				next = std::min(next, queued(0).at);
			const int n = next <= m_clock ? 1 : int(std::min<u64>(next - m_clock, u64(std::min(frames, native_voice::max_block))));
			render_block(n, left, right, ad_1, ad_2);
			m_clock += u64(n - 1);
			left += n;
			right += n;
			if (ad_1)
				ad_1 += n;
			if (ad_2)
				ad_2 += n;
			frames -= n;
		}
	}

	const u8 *program_rom() const { return m_prog->data(); }
	std::shared_ptr<const std::vector<u8>> program_rom_shared() const { return m_prog; }
	// パネルが読む: パートの設定と、選んでいる音色
	const ::xg::part_state &part_settings(int part) const { return m_state.parts[size_t(part)]; }
	int part_bank_msb(int part) const { return m_sel[part].msb; }
	int part_bank_lsb(int part) const { return m_sel[part].lsb; }
	int part_program(int part) const { return m_sel[part].prog; }
	bool reverb_type_written() const { return m_reverb_type_written; }
	bool chorus_type_written() const { return m_chorus_type_written; }
	int reverb_type() const { return m_fxin.rev_msb << 7 | m_fxin.rev_lsb; }
	int chorus_type() const { return m_fxin.cho_msb << 7 | m_fxin.cho_lsb; }
	int variation_type() const { return m_var.msb << 7 | m_var.lsb; }
	enum effect_block : int { block_reverb, block_chorus, block_variation, block_insertion_1, block_insertion_2, block_insertion_3, block_insertion_4, block_master_eq };
	bool gs_mode() const { return m_state.voice_mode == 0; }
	bool variation_is_system() const { return m_state.effect[::xg::settings::XG_VARIATION_CONNECTION] == 1; }
	void fill_meter(u8 *levels, int parts) const { m_drv.fill_meter(levels, parts); }
	// パラメータを 2 バイトで持つ種類か（ディレイの類）
	static bool fx_type_is_wide(int msb, int lsb)
	{
		const panel_fx::type_table table = panel_fx::find(panel_fx::table_insertion, msb << 7 | lsb);
		return table.count > 0 && table.params[0].wide;
	}

	static constexpr int AD_EXPRESSION = 0x80;   // XG のパートの番号の外
	int ad_value(int which, int address) const
	{
		const ad_part &a = m_ad[which];
		switch (address) {
		case ::xg::settings::PART_VOLUME: return a.volume;
		case ::xg::settings::PART_PAN: return a.pan;
		case ::xg::settings::PART_DRY: return a.dry;
		case ::xg::settings::PART_CHORUS: return a.chorus;
		case ::xg::settings::PART_REVERB: return a.reverb;
		case ::xg::settings::PART_VARIATION: return a.variation;
		case AD_EXPRESSION: return m_ad_expression[which];
		default: return 0;
		}
	}
	void panel_ad_expression(int which, int value) { m_ad_expression[which] = u8(value); }
	int system_value(int index) const { return m_state.system[size_t(index)]; }
	void ad_block(int which, int low, int size, u8 *data) const
	{
		static constexpr u8 RECEIVE_FLAGS[49] = {
			0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10, 0, 0, 0, 0, 0, 0, 0x11,
		};
		if (low == 0x30) {
			std::copy_n(RECEIVE_FLAGS, std::min(size, 49), data);
			return;
		}
		std::fill_n(data, size, u8(0));
		data[::xg::settings::PART_CHANNEL] = ::xg::settings::XG_NO_PART;
		for (u32 address : { ::xg::settings::PART_VOLUME, ::xg::settings::PART_PAN, ::xg::settings::PART_DRY, ::xg::settings::PART_CHORUS, ::xg::settings::PART_REVERB, ::xg::settings::PART_VARIATION })
			data[address] = u8(ad_value(which, int(address)));
	}
	int master_tune() const
	{
		const auto &tune = m_state.system;
		return (tune[0] & 0x0f) << 12 | (tune[1] & 0x0f) << 8 | (tune[2] & 0x0f) << 4 | (tune[3] & 0x0f);
	}
	void set_master_tune(int value)
	{
		const u8 nibbles[4] = { u8((value >> 12) & 0x0f), u8((value >> 8) & 0x0f), u8((value >> 4) & 0x0f), u8(value & 0x0f) };
		panel_bytes(0x00, 0x00, 0x00, nibbles, 4);
	}
	int drum_setup_value(int set, int key, int address) const
	{
		const int index = ::xg::settings::drum_setup_index(address);
		if (set < 0 || set >= int(::xg::settings::DRUM_SETUP_SETS) || index < 0 || key < ::xg::settings::DRUM_SETUP_NOTE0 || key >= ::xg::settings::DRUM_SETUP_NOTE0 + int(::xg::settings::DRUM_SETUP_NOTES))
			return 0;
		return m_state.drum_setup(set, key, index);
	}
	// 組を持たない「drum」のパートは、キットの値をそのまま見せる
	int drum_kit_value(int part, int key, int address) const
	{
		const int index = ::xg::settings::drum_setup_index(address);
		if (index < 0)
			return 0;
		const u8 *record = ::xg::nv::drum_record(m_prog->data(), part_settings(part).kit, key);
		return ::xg::nv::drum_setup_kit_default(m_prog->data(), record, index);
	}
	int effect_type(int block) const
	{
		switch (block) {
		case block_reverb: return reverb_type();
		case block_chorus: return chorus_type();
		case block_variation: return variation_type();
		case block_insertion_1: return m_ins1.msb << 7 | m_ins1.lsb;
		case block_insertion_2: case block_insertion_3: case block_insertion_4: {
			const fx_block &b = m_extra_insertions[block - block_insertion_2].b;
			return b.msb << 7 | b.lsb;
		}
		default: return 0;
		}
	}
	int effect_value(int block, int address) const
	{
		using smu2000::xg::effect_tables::KEEP;
		if (block == block_master_eq)
			return m_fxin.eq[std::clamp(address - 1, 0, 19)];
		if (address >= 0x100) {
			const int low = address - 0x100;
			if (block == block_reverb)
				return low == 0x0c ? m_fxin.mix[1] : m_fxin.mix[0];
			if (block == block_chorus)
				return low == 0x2c ? m_fxin.mix[3] : low == 0x2d ? m_fxin.mix[2] : m_fxin.mix[4];
			const u32 xg = block == block_variation ? (0x02u << 14 | 0x01u << 7 | u32(low)) : (0x03u << 14 | u32(block - block_insertion_1) << 7 | u32(low));
			const u8 *value = m_state.find(xg);
			return value ? *value : 0;
		}
		const int n = std::clamp(address, 1, 16) - 1;
		if (block == block_reverb) {
			return smu2000::fx_settings::make_system_request(m_prog->data(), m_fxin).reverb[n];
		}
		if (block == block_chorus)
			return smu2000::fx_settings::make_system_request(m_prog->data(), m_fxin).chorus[n];
		const fx_block &b = block == block_variation ? m_var : block == block_insertion_1 ? m_ins1 : m_extra_insertions[block - block_insertion_2].b;
		return b.request.P[n];
	}
	// パートやシステムの設定への 1 バイト（XG のパラメータチェンジと同じ）
	void panel_write(u8 high, u8 middle, u8 low, u8 value)
	{
		panel_bytes(high, middle, low, &value, 1);
	}
	void panel_effect_write(int block, int address, const u8 *bytes, int count)
	{
		u8 high = 0x02, middle = 0x01, low = 0;
		const int xg = address >= 0x100 ? address - 0x100 : address;
		// コーラスのパラメータ 11-16 は MIDI では捨てるが、パネルからは書ける
		if (block == block_chorus && address >= 11 && address <= 16) {
			m_fxin.cho[address - 1] = bytes[0];
			fx_update(LFO_DELAY_PARAM);
			return;
		}
		if (address == 0)
			low = block == block_reverb ? 0x00 : block == block_chorus ? 0x20 : block == block_variation ? 0x40 : 0x00;
		else if (address >= 0x100)
			low = u8(xg);
		else if (block == block_reverb)
			low = u8(xg <= 10 ? xg + 1 : xg - 11 + 0x10);
		else if (block == block_chorus)
			low = u8(xg <= 10 ? xg + 0x21 : xg - 11 + 0x30);
		else if (block == block_variation)
			low = u8(xg <= 10 ? 0x42 + (xg - 1) * 2 : 0x70 + xg - 11);
		else
			low = u8(xg <= 10 ? (count == 2 ? 0x30 + (xg - 1) * 2 : xg + 1) : 0x20 + xg - 11);
		if (block >= block_insertion_1 && block <= block_insertion_4) {
			high = 0x03;
			middle = u8(block - block_insertion_1);
		}
		if (block == block_variation && address != 0 && address < 0x100 && count == 1) {
			const u8 wide[2] = { 0, bytes[0] };
			if (xg <= 10) { panel_bytes(high, middle, low, wide, 2); return; }
		}
		panel_bytes(high, middle, low, bytes, count);
	}
	static int effect_param_address(int block, int number, bool wide)
	{
		if (block == block_reverb) return number <= 10 ? number + 1 : number - 11 + 0x10;
		if (block == block_chorus) return number <= 10 ? number + 0x21 : number - 11 + 0x30;
		if (block == block_variation) return number <= 10 ? 0x42 + (number - 1) * 2 : 0x70 + number - 11;
		return number <= 10 ? (wide ? 0x30 + (number - 1) * 2 : number + 1) : 0x20 + number - 11;
	}
	static constexpr int PANEL_MESSAGE_LIMIT = 64;
	void panel_sysex(const u8 *bytes, int count)
	{
		m_sysex_from_panel = true;
		sysex(sysex_bytes{ bytes, u32(count) });
		m_sysex_from_panel = false;
	}
	void panel_bytes(u8 high, u8 middle, u8 low, const u8 *bytes, int count)
	{
		// パネルはオーディオスレッドから呼ぶので、その場で効かせる（MIDI の線の時間は掛からない）
		u8 message[PANEL_MESSAGE_LIMIT] = { 0x43, 0x10, 0x4c, high, middle, low };
		std::copy(bytes, bytes + std::min(count, PANEL_MESSAGE_LIMIT - 6), message + 6);
		panel_sysex(message, 6 + count);
	}
	void panel_control(int part, int cc, int value)
	{
		const int channel = pb(part)[::xg::settings::PART_CHANNEL];
		if (channel < PORTS * 16) {
			midi(u8(0xb0 | channel % 16), channel / 16);
			midi(u8(cc), channel / 16);
			midi(u8(value), channel / 16);
		}
	}
	// パネルで音色を替える。受信スイッチ（バンク）を見ない道なので、値を先に入れてからプログラムを書く
	void panel_apply_voice(int part, int msb, int lsb, int program)
	{
		m_sel[part].msb = u8(msb);
		m_sel[part].lsb = u8(lsb);
		panel_select_voice(part, msb, lsb, program);
	}
	void panel_select_voice(int part, int msb, int lsb, int program)
	{
		const u8 values[3] = { u8(msb), u8(lsb), u8(program) };
		panel_bytes(0x08, u8(part), 0x01, values, 3);
	}
	// マスター EQ の型: 帯ごとの量・周波数・幅・形
	const u8 *eq_preset(int type) const { return m_eq_presets[type]; }

	// ---- パフォーマンス（PFM）。doc/panel-flow.md
	static constexpr int PERFORMANCES = 100, PERFORMANCE_SIZE = 244, PERFORMANCE_PARTS = 4, PERFORMANCE_PART_SIZE = 36;
	// 共通の項目（記録の中の位置）
	static constexpr int PERF_NAME = 0x90, PERF_NAME_SIZE = 12, PERF_VOLUME = 0x9c, PERF_PAN = 0x9d, PERF_AC1_CC = 0x9e, PERF_MW_PITCH = 0xa0, PERF_MW_FILTER = 0xa1,
	                     PERF_BEND = 0xa2, PERF_AC1_FILTER = 0xa3, PERF_AC1_AMP = 0xa4, PERF_AC1_FILTER_LFO = 0xa5, PERF_PORTA_SWITCH = 0xa6, PERF_PORTA_TIME = 0xa7,
	                     PERF_REVERB_RETURN = 0xaf, PERF_CHORUS_RETURN = 0xb8, PERF_VARIATION_RETURN = 0xc9;
	static constexpr int PERF_REVERB_TYPE = 0xa8, PERF_REVERB_VALUES = 0xaa, PERF_REVERB_PAN = 0xb0, PERF_CHORUS_TYPE = 0xb1, PERF_CHORUS_VALUES = 0xb3,
	                     PERF_CHORUS_PAN = 0xb9, PERF_CHORUS_TO_REVERB = 0xba, PERF_VARIATION_TYPE = 0xbb, PERF_VARIATION_VALUES = 0xbd, PERF_VARIATION_TENTH_VALUE = 0xc7,
	                     PERF_VARIATION_PAN = 0xca, PERF_VARIATION_TO_REVERB = 0xcb, PERF_VARIATION_TO_CHORUS = 0xcc, PERF_VARIATION_CONNECTION = 0xce, PERF_VARIATION_PART = 0xcf;
	static constexpr int PERF_SYSTEM_VALUES = 5, PERF_EQ_GAINS = 0xd1, PERF_EQ_BANDS = 5;
	static constexpr int PERF_INSERTIONS = 2, PERF_FIRST_INSERTION = 0xd6, PERF_SECOND_INSERTION = 0xe5, PERF_INSERTION_VALUES = 0x02, PERF_INSERTION_TENTH_VALUE = 0x0c, PERF_INSERTION_PART = 0x0e;
	static constexpr int PERF_AC1_AFTERTOUCH = 0x60, PERF_DEPTH_CENTER = 64;
	static constexpr int PERF_EQ_TYPE = 0xd0;
	// パートごとの項目
	static constexpr int PERF_PART_PROGRAM = 0x00, PERF_PART_VOLUME = 0x02, PERF_PART_PAN = 0x03, PERF_PART_DRY = 0x04, PERF_PART_CHORUS = 0x05, PERF_PART_REVERB = 0x06,
	                     PERF_PART_VARIATION = 0x07, PERF_PART_NOTE_SHIFT = 0x08, PERF_PART_CUTOFF = 0x09, PERF_PART_RESONANCE = 0x0a, PERF_PART_ATTACK = 0x0b, PERF_PART_DECAY = 0x0c,
	                     PERF_PART_RELEASE = 0x0d, PERF_PART_VIBRATO_RATE = 0x0e, PERF_PART_VIBRATO_DEPTH = 0x0f, PERF_PART_VIBRATO_DELAY = 0x10, PERF_PART_DETUNE_LOW = 0x12,
	                     PERF_PART_PITCH_EG_INITIAL = 0x13, PERF_PART_PITCH_EG_ATTACK = 0x14, PERF_PART_PITCH_EG_RELEASE_LEVEL = 0x15, PERF_PART_PITCH_EG_RELEASE_TIME = 0x16,
	                     PERF_PART_VELOCITY_DEPTH = 0x17, PERF_PART_VELOCITY_OFFSET = 0x18, PERF_PART_NOTE_LOW = 0x19, PERF_PART_NOTE_HIGH = 0x1a, PERF_PART_VELOCITY_LOW = 0x1b,
	                     PERF_PART_VELOCITY_HIGH = 0x1c, PERF_PART_EQ_BASS = 0x1d, PERF_PART_EQ_TREBLE = 0x1e, PERF_PART_EQ_BASS_FREQUENCY = 0x1f, PERF_PART_EQ_TREBLE_FREQUENCY = 0x20,
	                     PERF_PART_HIGHPASS = 0x21, PERF_PART_BANK_MSB = 0x22, PERF_PART_BANK_LSB = 0x23;
	static constexpr int PERF_PART_FLAGS = 0x11, PERF_PART_RECEIVE_NOTE = 0x40, PERF_PART_POLY = 0x20;
	static constexpr int PERF_INTERNAL_BYTES = PERFORMANCES * PERFORMANCE_SIZE;

	bool performance_mode() const { return m_perf_on; }
	int performance_bank() const { return m_perf_bank; }
	int performance_number() const { return m_perf_number; }
	bool performance_edited() const { return m_perf_edited; }
	const u8 *performance_record() const { return m_perf; }
	const u8 *stored_performance(int bank, int number) const
	{
		if (bank == 1 && !m_perf_internal.empty())
			return m_perf_internal.data() + size_t(number) * PERFORMANCE_SIZE;
		return m_prog->data() + (bank == 0 ? ::xg::rom_position::PERFORMANCE_PRESET : ::xg::rom_position::PERFORMANCE_INTERNAL) + u32(number) * PERFORMANCE_SIZE;
	}
	void enter_performance(int channel)
	{
		panel_universal_on();
		m_perf_on = true;
		m_perf_channel = channel;
		m_state.voice_set = 0;
		const u8 attenuator = PERF_ATTENUATOR;
		panel_bytes(0x00, 0x00, 0x05, &attenuator, 1);
		m_drv.set_global_attenuation(PERF_VOICE_ATTENUATION);
		m_drv.set_performance_mode(true);
		m_perf_entering = true;
		recall_performance(m_perf_bank, m_perf_number);
		m_perf_entering = false;
	}
	void leave_performance()
	{
		m_perf_on = false;
		m_drv.set_global_attenuation(0);
		m_drv.set_performance_mode(false);
	}
	void recall_performance(int bank, int number)
	{
		// 編集したものを捨てる前に控える（EDIT の RECALL で戻せる）
		if (m_perf_held && std::memcmp(m_perf, stored_performance(m_perf_bank, m_perf_number), PERFORMANCE_SIZE) != 0) {
			std::copy_n(m_perf, PERFORMANCE_SIZE, m_perf_recall);
			m_perf_recall_bank = m_perf_bank;
			m_perf_recall_number = m_perf_number;
			m_perf_recall_valid = true;
		}
		m_perf_bank = bank;
		m_perf_number = std::clamp(number, 0, PERFORMANCES - 1);
		std::copy_n(stored_performance(bank, m_perf_number), PERFORMANCE_SIZE, m_perf);
		m_perf_held = true;
		m_perf_edited = false;
		performance_loaded();
	}
	bool performance_recall_valid() const { return m_perf_recall_valid; }
	int performance_recall_bank() const { return m_perf_recall_bank; }
	int performance_recall_number() const { return m_perf_recall_number; }
	void restore_performance_recall()
	{
		m_perf_bank = m_perf_recall_bank;
		m_perf_number = m_perf_recall_number;
		std::copy_n(m_perf_recall, PERFORMANCE_SIZE, m_perf);
		m_perf_edited = true;
		performance_loaded();
	}
	// 記録を読み込んだ印。パネルはこれを見てミュートを読み直す
	u32 performance_loads() const { return m_perf_loads; }
	u8 performance_part_silence() const
	{
		u8 silent = 0;
		for (int part = 0; part < PERFORMANCE_PARTS; part++)
			if (!(m_perf[part * PERFORMANCE_PART_SIZE + PERF_PART_FLAGS] & PERF_PART_RECEIVE_NOTE))
				silent |= u8(1 << part);
		return silent;
	}
	// パートのミュートは記録の「鳴らす」印そのもの
	void set_performance_part_silence(u8 silent)
	{
		for (int part = 0; part < PERFORMANCE_PARTS; part++) {
			u8 &flags = m_perf[part * PERFORMANCE_PART_SIZE + PERF_PART_FLAGS];
			flags = u8((silent >> part & 1) ? flags & ~PERF_PART_RECEIVE_NOTE : flags | PERF_PART_RECEIVE_NOTE);
			panel_write(::xg::settings::XG_PART, u8(part), ::xg::settings::PART_RECEIVE_NOTE, (silent >> part & 1) ? 0 : 1);
		}
		m_perf_edited = true;
	}
	void note_performance_edited() { m_perf_edited = true; }
	// パートの音色を記録へ写す
	void note_performance_part_voice(int part)
	{
		u8 *q = m_perf + part * PERFORMANCE_PART_SIZE;
		q[PERF_PART_PROGRAM] = u8(part_program(part));
		q[PERF_PART_BANK_MSB] = u8(part_bank_msb(part));
		q[PERF_PART_BANK_LSB] = u8(part_bank_lsb(part));
		m_perf_edited = true;
	}
	static int performance_part_address(int record_offset) { return record_offset <= 0x08 ? record_offset : record_offset <= 0x16 ? record_offset + 1 : record_offset + 2; }
	static int performance_common_address(int record_offset) { return record_offset <= 0xa7 ? record_offset - PERF_NAME : record_offset - 0x88; }
	static bool performance_clears_param(int msb, int lsb, int number)
	{
		if (msb >= 0x05 && msb <= 0x08)
			return number == 12;
		if (msb == 0x54)
			return number == 11;
		if (msb == 0x56)
			return number == 13;
		if (msb == 0x48)
			return number == (lsb == 0x08 ? 12 : 13);
		return msb == 0x4d && lsb == 0x00;
	}
	void performance_loaded()
	{
		apply_performance();
		// 読むたびに格子が 1 目後ろへ。パフォーマンスのモードに入るときはもう 1 目
		move_eg_grid(m_perf_entering ? 2 : 1);
		// 種類や値が同じでも、LFO は書き直されて位相が 0 から始まる
		m_lfo_load_delay = m_perf_entering ? LFO_DELAY_PERFORMANCE_ENTRY : LFO_DELAY_PERFORMANCE;
		m_var.lfo_restart = m_ins1.lfo_restart = true;
		for (extra_insertion &insertion : m_extra_insertions)
			insertion.b.lfo_restart = true;
		fx_update(m_lfo_load_delay);
		m_lfo_load_delay = 0;
		set_part_mute(performance_part_silence(), 0);
		m_perf_loads++;
	}
	// 記録を XG の値へ広げる
	void apply_performance()
	{
		const u8 *r = m_perf;
		const auto one = [&](int high, int middle, int low, int value) {
			const u8 byte = u8(value);
			panel_bytes(u8(high), u8(middle), u8(low), &byte, 1);
		};
		const auto two = [&](int high, int middle, int low, int first, int second) {
			const u8 bytes[2] = { u8(first), u8(second) };
			panel_bytes(u8(high), u8(middle), u8(low), bytes, 2);
		};
		namespace settings = ::xg::settings;
		two(settings::XG_EFFECT, settings::XG_SYSTEM_EFFECT, settings::XG_REVERB_TYPE, r[PERF_REVERB_TYPE], r[PERF_REVERB_TYPE + 1]);
		for (int i = 0; i < PERF_SYSTEM_VALUES; i++)
			one(settings::XG_EFFECT, settings::XG_SYSTEM_EFFECT, settings::XG_REVERB_VALUES + i, r[PERF_REVERB_VALUES + i]);
		// 記録に無い項目のいくつかは、種類の既定ではなく 0 になる
		one(settings::XG_EFFECT, settings::XG_SYSTEM_EFFECT, settings::XG_REVERB_TENTH_VALUE, 0);
		one(settings::XG_EFFECT, settings::XG_SYSTEM_EFFECT, settings::XG_REVERB_RETURN, r[PERF_REVERB_RETURN]);
		one(settings::XG_EFFECT, settings::XG_SYSTEM_EFFECT, settings::XG_REVERB_PAN, r[PERF_REVERB_PAN]);
		two(settings::XG_EFFECT, settings::XG_SYSTEM_EFFECT, settings::XG_CHORUS_TYPE, r[PERF_CHORUS_TYPE], r[PERF_CHORUS_TYPE + 1]);
		for (int i = 0; i < PERF_SYSTEM_VALUES; i++)
			one(settings::XG_EFFECT, settings::XG_SYSTEM_EFFECT, settings::XG_CHORUS_VALUES + i, r[PERF_CHORUS_VALUES + i]);
		one(settings::XG_EFFECT, settings::XG_SYSTEM_EFFECT, settings::XG_CHORUS_TENTH_VALUE, 0);
		one(settings::XG_EFFECT, settings::XG_SYSTEM_EFFECT, settings::XG_CHORUS_RETURN, r[PERF_CHORUS_RETURN]);
		one(settings::XG_EFFECT, settings::XG_SYSTEM_EFFECT, settings::XG_CHORUS_PAN, r[PERF_CHORUS_PAN]);
		one(settings::XG_EFFECT, settings::XG_SYSTEM_EFFECT, settings::XG_CHORUS_TO_REVERB, r[PERF_CHORUS_TO_REVERB]);
		two(settings::XG_EFFECT, settings::XG_VARIATION, settings::XG_VARIATION_TYPE, r[PERF_VARIATION_TYPE], r[PERF_VARIATION_TYPE + 1]);
		for (int i = 0; i < PERF_SYSTEM_VALUES; i++)
			two(settings::XG_EFFECT, settings::XG_VARIATION, settings::XG_VARIATION_VALUES + 2 * i, r[PERF_VARIATION_VALUES + 2 * i], r[PERF_VARIATION_VALUES + 1 + 2 * i]);
		two(settings::XG_EFFECT, settings::XG_VARIATION, settings::XG_VARIATION_TENTH_VALUE, r[PERF_VARIATION_TENTH_VALUE], r[PERF_VARIATION_TENTH_VALUE + 1]);
		for (int number = 11; number <= 13; number++)
			if (performance_clears_param(r[PERF_VARIATION_TYPE], r[PERF_VARIATION_TYPE + 1], number))
				one(settings::XG_EFFECT, settings::XG_VARIATION, settings::XG_VARIATION_LATER_VALUES + number - 11, 0);
		static constexpr u8 VARIATION_MIX[][2] = {
			{ settings::XG_VARIATION_RETURN, PERF_VARIATION_RETURN }, { settings::XG_VARIATION_PAN, PERF_VARIATION_PAN },
			{ settings::XG_VARIATION_TO_REVERB, PERF_VARIATION_TO_REVERB }, { settings::XG_VARIATION_TO_CHORUS, PERF_VARIATION_TO_CHORUS },
			{ settings::XG_VARIATION_CONNECTION, PERF_VARIATION_CONNECTION }, { settings::XG_VARIATION_PART, PERF_VARIATION_PART },
		};
		for (const auto &item : VARIATION_MIX)
			one(settings::XG_EFFECT, settings::XG_VARIATION, item[0], r[item[1]]);
		for (int n = 0; n < PERF_INSERTIONS; n++) {
			const u8 *insertion = r + (n == 0 ? PERF_FIRST_INSERTION : PERF_SECOND_INSERTION);
			two(settings::XG_INSERTION, n, settings::XG_INSERTION_TYPE, insertion[0], insertion[1]);
			// 記録は 2 バイトで持つ。1 バイトの種類へは下位だけ
			const bool wide = fx_type_is_wide(insertion[0], insertion[1]);
			for (int i = 0; i < PERF_SYSTEM_VALUES; i++) {
				const u8 *value = insertion + PERF_INSERTION_VALUES + 2 * i;
				if (wide)
					two(settings::XG_INSERTION, n, settings::XG_INSERTION_WIDE_VALUES + 2 * i, value[0], value[1]);
				else
					one(settings::XG_INSERTION, n, settings::XG_INSERTION_VALUES + i, value[1]);
			}
			const u8 *tenth = insertion + PERF_INSERTION_TENTH_VALUE;
			if (wide)
				two(settings::XG_INSERTION, n, settings::XG_INSERTION_WIDE_TENTH_VALUE, tenth[0], tenth[1]);
			else
				one(settings::XG_INSERTION, n, settings::XG_INSERTION_TENTH_VALUE, tenth[1]);
			one(settings::XG_INSERTION, n, settings::XG_INSERTION_PART, insertion[PERF_INSERTION_PART]);
			for (int number = 11; number <= 13; number++)
				if (performance_clears_param(insertion[0], insertion[1], number))
					one(settings::XG_INSERTION, n, settings::XG_INSERTION_LATER_VALUES + number - 11, 0);
		}
		panel_bytes(settings::XG_EFFECT, settings::XG_MASTER_EQ, settings::XG_EQ_TYPE_VALUES, m_eq_presets[std::min<int>(r[PERF_EQ_TYPE], 4)], 20);
		for (int band = 0; band < PERF_EQ_BANDS; band++)
			one(settings::XG_EFFECT, settings::XG_MASTER_EQ, settings::XG_EQ_TYPE_VALUES + band * settings::XG_EQ_GAIN_STRIDE, r[PERF_EQ_GAINS + band]);
		const bool aftertouch = r[PERF_AC1_CC] == PERF_AC1_AFTERTOUCH;
		static constexpr u8 PART_FIELDS[][2] = {
			{ settings::PART_NOTE_SHIFT, PERF_PART_NOTE_SHIFT },
			{ settings::PART_VELOCITY_DEPTH, PERF_PART_VELOCITY_DEPTH },
			{ settings::PART_VELOCITY_OFFSET, PERF_PART_VELOCITY_OFFSET },
			{ settings::PART_PAN, PERF_PART_PAN },
			{ settings::PART_NOTE_LOW, PERF_PART_NOTE_LOW },
			{ settings::PART_NOTE_HIGH, PERF_PART_NOTE_HIGH },
			{ settings::PART_DRY, PERF_PART_DRY },
			{ settings::PART_CHORUS, PERF_PART_CHORUS },
			{ settings::PART_REVERB, PERF_PART_REVERB },
			{ settings::PART_VARIATION, PERF_PART_VARIATION },
			{ settings::PART_VIBRATO_RATE, PERF_PART_VIBRATO_RATE },
			{ settings::PART_VIBRATO_DEPTH, PERF_PART_VIBRATO_DEPTH },
			{ settings::PART_VIBRATO_DELAY, PERF_PART_VIBRATO_DELAY },
			{ settings::PART_CUTOFF, PERF_PART_CUTOFF },
			{ settings::PART_RESONANCE, PERF_PART_RESONANCE },
			{ settings::PART_ATTACK, PERF_PART_ATTACK },
			{ settings::PART_DECAY, PERF_PART_DECAY },
			{ settings::PART_RELEASE, PERF_PART_RELEASE },
			{ settings::PART_PITCH_EG_INITIAL, PERF_PART_PITCH_EG_INITIAL },
			{ settings::PART_PITCH_EG_ATTACK, PERF_PART_PITCH_EG_ATTACK },
			{ settings::PART_PITCH_EG_RELEASE_LEVEL, PERF_PART_PITCH_EG_RELEASE_LEVEL },
			{ settings::PART_PITCH_EG_RELEASE_TIME, PERF_PART_PITCH_EG_RELEASE_TIME },
			{ settings::PART_VELOCITY_LOW, PERF_PART_VELOCITY_LOW },
			{ settings::PART_VELOCITY_HIGH, PERF_PART_VELOCITY_HIGH },
			{ settings::PART_EQ_BASS, PERF_PART_EQ_BASS },
			{ settings::PART_EQ_TREBLE, PERF_PART_EQ_TREBLE },
			{ settings::PART_EQ_BASS_FREQUENCY, PERF_PART_EQ_BASS_FREQUENCY },
			{ settings::PART_EQ_TREBLE_FREQUENCY, PERF_PART_EQ_TREBLE_FREQUENCY },
		};
		for (int part = 0; part < PERFORMANCE_PARTS; part++) {
			const u8 *q = r + part * PERFORMANCE_PART_SIZE;
			one(settings::XG_PART, part, settings::PART_CHANNEL, m_perf_channel);
			m_sel[part].msb = q[PERF_PART_BANK_MSB];
			m_sel[part].lsb = q[PERF_PART_BANK_LSB];
			pb(part)[settings::PART_BANK_MSB] = q[PERF_PART_BANK_MSB];
			pb(part)[settings::PART_BANK_LSB] = q[PERF_PART_BANK_LSB];
			one(settings::XG_PART, part, settings::PART_PROGRAM, q[PERF_PART_PROGRAM]);
			for (const auto &item : PART_FIELDS)
				one(settings::XG_PART, part, item[0], q[item[1]]);
			one(settings::XG_PART, part, settings::PART_POLY, (q[PERF_PART_FLAGS] & PERF_PART_POLY) != 0);
			two(settings::XG_PART, part, settings::PART_DETUNE, q[PERF_PART_FLAGS] & 0x0f, q[PERF_PART_DETUNE_LOW]);
			one(settings::XG_PART, part, settings::PART_RECEIVE_NOTE, (q[PERF_PART_FLAGS] & PERF_PART_RECEIVE_NOTE) != 0);
			one(settings::XG_PART, part, settings::PART_VOLUME, performance_part_volume(part));
			one(settings::XG_HIGHPASS, part, settings::XG_HIGHPASS_VALUE, q[PERF_PART_HIGHPASS]);
			one(settings::XG_PART, part, settings::PART_MW_LFO_PITCH, r[PERF_MW_PITCH]);
			one(settings::XG_PART, part, settings::PART_MW_LFO_FILTER, r[PERF_MW_FILTER]);
			one(settings::XG_PART, part, settings::PART_BEND_RANGE, r[PERF_BEND]);
			one(settings::XG_PART, part, settings::PART_AC1_CC, std::min<int>(r[PERF_AC1_CC], 0x5f));
			// 切る高さの効きは選んだ方にだけ入り、他方は中央。音量の効きはいつも AC1 へ、LFO の効きはいつもアフタータッチへ入る
			one(settings::XG_PART, part, settings::PART_AFTERTOUCH_FILTER, aftertouch ? r[PERF_AC1_FILTER] : PERF_DEPTH_CENTER);
			one(settings::XG_PART, part, settings::PART_AC1_FILTER, aftertouch ? PERF_DEPTH_CENTER : r[PERF_AC1_FILTER]);
			one(settings::XG_PART, part, settings::PART_AFTERTOUCH_AMP, PERF_DEPTH_CENTER);
			one(settings::XG_PART, part, settings::PART_AC1_AMP, r[PERF_AC1_AMP]);
			one(settings::XG_PART, part, settings::PART_AFTERTOUCH_FILTER_LFO, r[PERF_AC1_FILTER_LFO]);
			one(settings::XG_PART, part, settings::PART_AC1_FILTER_LFO, 0);
			// パフォーマンスのバンク選択は記録を選ぶ。パート自身は受けない
			pb(part).receive_bank = 0;
			one(settings::XG_PART, part, settings::PART_PORTAMENTO_SWITCH, r[PERF_PORTA_SWITCH]);
			one(settings::XG_PART, part, settings::PART_PORTAMENTO_TIME, r[PERF_PORTA_TIME]);
		}
		for (int part = PERFORMANCE_PARTS; part < 16; part++)
			one(settings::XG_PART, part, settings::PART_CHANNEL, settings::XG_NO_PART);
	}
	int performance_part_volume(int part) const
	{
		// パートの音量か演奏の音量が 0 なら 0。ほかは 1 を足すので、小さくても 1 になる
		// （0 にも 1 を足していたので、音量 0 のパートが音量 1 と同じに漏れていた。PFM 31 で右に -84 dB、実機は -95 dB）
		const int master = m_perf[PERF_VOLUME], volume = m_perf[part * PERFORMANCE_PART_SIZE + PERF_PART_VOLUME];
		return master && volume ? (volume * master >> 7) + 1 : 0;
	}
	// INIT の PFMInit: いまの番号へ初期の記録を読む
	void initialize_performance()
	{
		std::copy_n(m_prog->data() + ::xg::rom_position::PERFORMANCE_INITIAL, PERFORMANCE_SIZE, m_perf);
		m_perf_edited = true;
		performance_loaded();
	}
	// パネルと CC から: 記録の値を替えて、効く先へ
	void set_performance_byte(int offset, int value)
	{
		m_perf[offset] = u8(value);
		m_perf_edited = true;
	}
	void set_performance_volume(int value)
	{
		m_perf[PERF_VOLUME] = u8(value);
		for (int part = 0; part < PERFORMANCE_PARTS; part++)
			panel_write(::xg::settings::XG_PART, u8(part), u8(::xg::settings::PART_VOLUME), u8(performance_part_volume(part)));
	}
	void set_performance_pan(int value)
	{
		m_perf[PERF_PAN] = u8(value);
		for (int part = 0; part < PERFORMANCE_PARTS; part++) {
			const int own = m_perf[part * PERFORMANCE_PART_SIZE + 3];
			panel_write(::xg::settings::XG_PART, u8(part), u8(::xg::settings::PART_PAN), u8(own == 0 ? 0 : std::clamp(own + std::max(value, 1) - 64, 1, 127)));
		}
	}
	void set_performance_channel(int channel)
	{
		m_perf_channel = channel;
		if (m_perf_on)
			for (int part = 0; part < PERFORMANCE_PARTS; part++)
				panel_write(::xg::settings::XG_PART, u8(part), u8(::xg::settings::PART_CHANNEL), u8(channel));
	}
	void store_performance(int number)
	{
		if (m_perf_internal.empty())
			return;
		std::copy_n(m_perf, PERFORMANCE_SIZE, m_perf_internal.data() + size_t(number) * PERFORMANCE_SIZE);
		m_perf_bank = 1;
		m_perf_number = number;
		m_perf_edited = false;
	}
	void copy_performance(int from_bank, int from, int to)
	{
		if (!m_perf_internal.empty())
			std::memmove(m_perf_internal.data() + size_t(to) * PERFORMANCE_SIZE, stored_performance(from_bank, from), PERFORMANCE_SIZE);
	}
	int system_transpose() const { return m_state.system[::xg::settings::SYS_TRANSPOSE]; }
	// パートのページで動かした値を記録へも写す（STORE で残るように）
	void note_performance_part_edit(int part, int high, int low, int value)
	{
		m_perf_edited = true;
		const int base = part * PERFORMANCE_PART_SIZE;
		if (low == 0x09) {
			m_perf[base + PERF_PART_FLAGS] = u8((m_perf[base + PERF_PART_FLAGS] & 0xf0) | ((value >> 4) & 0x0f));
			m_perf[base + 0x12] = u8(value & 0x0f);
			return;
		}
		if (low == 0x05) { m_perf[base + PERF_PART_FLAGS] = u8((m_perf[base + PERF_PART_FLAGS] & ~PERF_PART_POLY) | (value ? PERF_PART_POLY : 0)); return; }
		const int offset = performance_part_offset(high, low);
		if (offset >= 0)
			m_perf[base + offset] = u8(value);
	}
	static int performance_part_offset(int high, int low)
	{
		static constexpr u8 FIELDS[][2] = {
			{ 0x08, 0x08 }, { 0x0c, 0x17 }, { 0x0d, 0x18 }, { 0x0e, 0x03 }, { 0x0f, 0x19 }, { 0x10, 0x1a }, { 0x11, 0x04 }, { 0x12, 0x05 }, { 0x13, 0x06 }, { 0x14, 0x07 },
			{ 0x15, 0x0e }, { 0x16, 0x0f }, { 0x17, 0x10 }, { 0x18, 0x09 }, { 0x19, 0x0a }, { 0x1a, 0x0b }, { 0x1b, 0x0c }, { 0x1c, 0x0d }, { 0x69, 0x13 }, { 0x6a, 0x14 },
			{ 0x6b, 0x15 }, { 0x6c, 0x16 }, { 0x6d, 0x1b }, { 0x6e, 0x1c }, { 0x72, 0x1d }, { 0x73, 0x1e }, { 0x76, 0x1f }, { 0x77, 0x20 },
		};
		if (high == 0x0a && low == 0x20)
			return 0x21;
		for (const auto &item : FIELDS)
			if (item[0] == low)
				return item[1];
		return -1;
	}
	static int performance_part_low(int offset)
	{
		for (int low = 0; low < 0x80; low++)
			if (performance_part_offset(0x08, low) == offset)
				return low;
		return -1;
	}
	void set_performance_name_char(int index, u8 value) { set_performance_byte(PERF_NAME + index, value); }
	const std::vector<u8> &performance_internal() const { return m_perf_internal; }
	void restore_performance_internal(const u8 *bytes) { m_perf_internal.assign(bytes, bytes + PERF_INTERNAL_BYTES); }

	// UTIL の受信の設定（device は 0-15、16 が all）
	void set_receive_options(int device, bool system_on, bool exclusive, bool bank_select)
	{
		m_receive_device = device;
		m_receive_system_on = system_on;
		m_receive_exclusive = exclusive;
		m_receive_bank_select = bank_select;
	}
	void set_locks(bool ad_part, bool master_eq) { m_ad_locked = ad_part; m_eq_locked = master_eq; }
	u32 external_resets() const { return m_external_resets; }
	int master_eq_type() const { return m_eq_type; }
	void set_master_eq_type(int type) { m_eq_type = u8(type); }
	// AUDITION のキー: いまのパートのチャンネルでノートを鳴らす
	void panel_note(int part, int note, int velocity)
	{
		const int channel = pb(part)[::xg::settings::PART_CHANNEL];
		if (channel < PORTS * 16) {
			midi(u8(0x90 | channel % 16), channel / 16);
			midi(u8(note), channel / 16);
			midi(u8(velocity), channel / 16);
		}
	}
	// ---- 音源からパネルへの知らせ（音源のスレッドの中で渡す）
	enum notice_kind : u8 { notice_data_error, notice_address_error, notice_checksum, notice_bulk, notice_setting, notice_request_bulk, notice_request_value };
	struct panel_notice { notice_kind kind; u8 a, b, c, d; };
	bool take_notice(panel_notice &out)
	{
		if (m_notice_read == m_notice_write)
			return false;
		out = m_notices[m_notice_read];
		m_notice_read = (m_notice_read + 1) % NOTICES;
		return true;
	}
	void push_notice(const panel_notice &notice)
	{
		const size_t next = (m_notice_write + 1) % NOTICES;
		if (next == m_notice_read)
			return;                  // ponytail: 溢れたら捨てる。パネルは毎ブロック読むので溜まらない
		m_notices[m_notice_write] = notice;
		m_notice_write = next;
	}
	int receive_device() const { return m_receive_device; }
	void set_voice_map(bool mu100_native) { m_state.voice_set = mu100_native ? 1 : 0; }
	int voice_set() const { return m_state.voice_set; }
	void set_part_mute(u64 mask, u8 ad_mask = 0)
	{
		// 新たにミュートしたパートの音は止める。解除しても戻らない
		for (u64 newly = mask & ~m_part_mute; newly != 0; newly &= newly - 1)
			m_drv.control(std::countr_zero(newly), CC_ALL_SOUND_OFF, 0);
		m_part_mute = mask;
		m_ad_mute = ad_mask;
		ad_refresh();
	}
	void factory_reset() { panel_universal_on(); }
	void initialize_xg() { panel_universal_on(); }
	void initialize_drum_setup(int set) { const u8 value = u8(set); panel_bytes(0x00, 0x00, 0x7d, &value, 1); }
	void select_sound_module(bool gs)
	{
		if (gs) {
			static constexpr u8 GS_RESET[] = { 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7f, 0x00, 0x41 };
			panel_sysex(GS_RESET, int(sizeof(GS_RESET)));
		} else
			panel_universal_on();
	}
	void panel_universal_on()
	{
		const u8 zero = 0;
		panel_bytes(0x00, 0x00, 0x7e, &zero, 1);
	}
	bool midi_overflowed() const { return m_midi_overflowed; }
	void clear_midi_overflow() { m_midi_overflowed = false; }
	// 鳴っている音を止めて、受け取り途中の MIDI も捨てる（設定を読み込んだあと・サンプルレートを替えたあと）
	void discard_sound()
	{
		m_q_head = m_q_count = m_sx_used = 0;
		for (parser &p : m_in)
			p = parser{};
		m_drv.stop_all_sound();
		m_voices.reset();
		m_mixer.reset();
		fx_mixer_refresh();              // 戻りの利得も消えたので置き直す
		m_exit_held[0] = m_exit_held[1] = 0.0f;
		for (float (&held)[2] : m_ad_held)
			held[0] = held[1] = 0.0f;
		for (float (&filter)[2] : m_ad_filter)
			filter[0] = filter[1] = 0.0f;
		// エフェクトの残響も消す
		m_eq.reset();
		m_reverb.reset();
		const auto silence_effect = [](auto &effect) {
			if (effect.flow)
				effect.flow->reset();
			effect.quiet = effect.hold = 0;
			effect.active_at = 0;
		};
		for (fx_region &g : m_regions)
			silence_effect(g);
		for (extra_insertion &si : m_extra_insertions)
			silence_effect(si);
		m_reverb_quiet = 0;
		// 時計を 0 から数え直す（書き換えを待っていた LFO は今の値にする）。同じ入力には、いつでも同じ音を返す
		lfo_power_on();
		m_clock = 0;
		for (u64 &received : m_rx_at)
			received = 0;
	}

	// XG の状態の保存。鳴っている音は入れない（読み込むと静かな状態から始まる）
	std::vector<u8> save_state() const
	{
		std::vector<u8> out;
		auto put = [&out](const void *p, size_t n) {
			const u8 *b = static_cast<const u8 *>(p);
			out.insert(out.end(), b, b + n);
		};
		put(STATE_MAGIC, sizeof(STATE_MAGIC));
		put(&m_state, sizeof(m_state));
		put(&m_fxin, sizeof(m_fxin));
		put(m_sel, sizeof(m_sel));
		put(m_rpn, sizeof(m_rpn));
		put(&m_gs_effects, sizeof(m_gs_effects));
		put(&m_eg_phase_quarters, sizeof(m_eg_phase_quarters));
		put(&m_resets_received, sizeof(m_resets_received));
		put(&m_last_reset_gs, sizeof(m_last_reset_gs));
		put(m_bank_drum, sizeof(m_bank_drum));
		put(m_ad, sizeof(m_ad));
		const u8 performance_flags[4] = { u8(m_perf_on), u8(m_perf_bank), u8(m_perf_number), u8(m_perf_channel) };
		put(performance_flags, sizeof(performance_flags));
		put(m_perf, sizeof(m_perf));
		put(m_perf_internal.data(), m_perf_internal.size());
		for (const fx_block *b : { &m_var, &m_ins1, &m_extra_insertions[0].b, &m_extra_insertions[1].b, &m_extra_insertions[2].b }) {
			const u8 kind[2] = { b->msb, b->lsb };
			put(kind, 2);
			put(b->P, sizeof(b->P));
			put(b->M, sizeof(b->M));
		}
		return out;
	}

	bool load_state(const u8 *data, size_t size)
	{
		static_assert(std::is_trivially_copyable_v<smu2000::xg::effect_tables::Input>);
		const size_t block = 2 + sizeof(fx_block::P) + sizeof(fx_block::M);
		const size_t expected = sizeof(STATE_MAGIC) + sizeof(m_state) + sizeof(m_fxin) + sizeof(m_sel) + sizeof(m_rpn) + sizeof(m_gs_effects) + sizeof(m_eg_phase_quarters) + sizeof(m_resets_received) + sizeof(m_last_reset_gs) + sizeof(m_bank_drum) + sizeof(m_ad) + 4 + sizeof(m_perf) + PERF_INTERNAL_BYTES + 5 * block;
		// パフォーマンスを足す前に保存したものも読む（パフォーマンスの分だけ短い）
		const size_t performance_bytes = 4 + sizeof(m_perf) + PERF_INTERNAL_BYTES;
		const bool has_performance = size == expected;
		if ((!has_performance && size != expected - performance_bytes) || std::memcmp(data, STATE_MAGIC, sizeof(STATE_MAGIC)) != 0)
			return false;
		xg_reset();
		const u8 *p = data + sizeof(STATE_MAGIC);
		auto get = [&p](void *to, size_t n) {
			std::memcpy(to, p, n);
			p += n;
		};
		get(&m_state, sizeof(m_state));
		get(&m_fxin, sizeof(m_fxin));
		get(m_sel, sizeof(m_sel));
		get(m_rpn, sizeof(m_rpn));
		get(&m_gs_effects, sizeof(m_gs_effects));
		get(&m_eg_phase_quarters, sizeof(m_eg_phase_quarters));
		get(&m_resets_received, sizeof(m_resets_received));
		get(&m_last_reset_gs, sizeof(m_last_reset_gs));
		set_eg_phase_now();
		get(m_bank_drum, sizeof(m_bank_drum));
		get(m_ad, sizeof(m_ad));
		m_perf_on = false;
		if (has_performance) {
			u8 performance_flags[4];
			get(performance_flags, sizeof(performance_flags));
			m_perf_on = performance_flags[0] != 0;
			m_perf_bank = performance_flags[1];
			m_perf_number = performance_flags[2];
			m_perf_channel = performance_flags[3];
			get(m_perf, sizeof(m_perf));
			m_perf_internal.resize(PERF_INTERNAL_BYTES);
			get(m_perf_internal.data(), PERF_INTERNAL_BYTES);
		}
		m_drv.set_global_attenuation(m_perf_on ? PERF_VOICE_ATTENUATION : 0);
		m_drv.set_performance_mode(m_perf_on);
		extra_insertion *const slaves = m_extra_insertions;
		fx_block *const blocks[5] = { &m_var, &m_ins1, &slaves[0].b, &slaves[1].b, &slaves[2].b };
		// インサーション 2-4 の出口の利得はインサーション 1 の種類で決まるので、1 を先に読む
		for (fx_block *b : blocks) {
			u16 params[16], mix[16];
			u8 kind[2];
			get(kind, 2);
			get(params, sizeof(params));
			get(mix, sizeof(mix));
			b->msb = kind[0];
			b->lsb = kind[1];
			std::copy(mix, mix + 16, b->M);
			fx_block_load(*b, b == &m_var ? smu2000::xg::effect_tables::FX_VAR : smu2000::xg::effect_tables::FX_INS);
			if (b == &m_var)
				b->request.set_connection(b->M[9]);
			std::copy(params, params + 16, b->P);
			for (int n = 1; n <= 16; n++)
				if (b->P[n - 1] != 0xffff)
					b->request.set_param(n, b->P[n - 1]);
		}
		for (int part = 0; part < 64; part++)
			part_gain(part);
		m_drv.reset_parts();
		for (int part = 0; part < 64; part++)
			select_voice(part);
		for (int part = 0; part < 64; part++)
			m_drv.bend(part, int(pb(part).bend_half) << 8);
		ad_refresh();
		fx_update(LFO_DELAY_RESET);
		return true;
	}

	std::FILE *trace = nullptr;
	// 試験: render が出口の代わりに声の乾いた音（render --tap-dry と同じもの。母線の単位）を返す
	bool dry_tap = false;

private:
	enum : u8 { ev_off, ev_on, ev_cc, ev_prog, ev_bend, ev_sysex, ev_cat, ev_pat };
	struct event { u64 at; u8 kind, chan, d0, d1; u32 sx_at, sx_len; };
	struct parser { u8 status = 0, d[2] = {}; int have = 0; bool in_sx = false; u32 sx_len = 0; u8 sx[1024]; };
	struct sysex_bytes
	{
		const u8 *data;
		size_t count;
		size_t size() const { return count; }
		u8 operator[](size_t i) const { return data[i]; }
	};
	struct prog_sel { u8 msb = 0, lsb = 0, prog = 0; };
	struct gs_effects { u8 character = ::xg::gs::DEFAULT_CHARACTER, pre_lpf = ::xg::gs::DEFAULT_PRE_LPF, time = ::xg::gs::DEFAULT_TIME,
	                     chorus_depth = 0, chorus_delay = 0; };
	struct rpn_sel { u8 msb = 0x7f, lsb = 0x7f; bool nrpn = false; u8 fine_data = 0; };

	// 線の刻み: 1/8000 サンプル。DIN の 1 バイトは 10 ビット / 31250 baud ＝ ちょうど 14.112 サンプル
	static constexpr u64 RX_UNIT = 8000;
	static constexpr u64 RX_BYTE = RX_UNIT * 44100 * 10 / 31250;	// 1 バイト 10 ビットの時間（31250 baud）
	// 受け終えてから効くまで（サンプル）。押しは 32（音色を引いて要素を組む）、離しは 2。
	// ドラムの押しは 3 サンプル早い（1 打を引く道が短い）
	static constexpr u64 ON_PROC = 32, OFF_PROC = 2, DRUM_LEAD = 3;
	// 第 2 群は書くのが速く、受けてから 8 サンプルで鍵を押せる。押しはそれに間に合うように早めに読んでおく
	static constexpr u64 NOTE_READ_AHEAD = 24;
	// 10ms の格子の位相（サンプル）。起動のときは 413 で、リセットを受けるたびに 2.5ms 戻る
	static constexpr u32 EG_PHASE_AT_BOOT_QUARTERS = 413 * 4, EG_GRID_STEP_QUARTERS = 441, EG_TICK_QUARTERS = 441 * 4;
	static constexpr int COARSE_MAX = 24;
	enum : u32 { RECEIVE_BEND = 0x30, RECEIVE_CHANNEL_PRESSURE, RECEIVE_PROGRAM, RECEIVE_CONTROL, RECEIVE_POLY_PRESSURE, RECEIVE_NOTE, RECEIVE_RPN, RECEIVE_NRPN,
	             RECEIVE_MODULATION, RECEIVE_VOLUME, RECEIVE_PAN, RECEIVE_EXPRESSION, RECEIVE_HOLD, RECEIVE_PORTAMENTO, RECEIVE_SOSTENUTO, RECEIVE_SOFT };
	static constexpr int CC_PORTAMENTO = 65, CC_PORTAMENTO_TIME = 5;
	static constexpr u8 NOTE_SHIFT_MIN = 0x28, NOTE_SHIFT_MAX = 0x58;
	static constexpr int GS_CHORUS_MACRO = 0x38;
	static constexpr u8 GS_LEVEL_DEFAULT = 0x40;
	// コーラスのマクロで戻るパラメータ（02 01 xx）
	static constexpr u8 GS_RESET_CHORUS[][2] = { { 0x22, 0x06 }, { 0x23, 0x36 }, { 0x24, 0x4d }, { 0x25, 0x6a } };
	// GS リセットのあとのリバーブのパラメータ（02 01 xx）
	static constexpr u8 GS_RESET_REVERB[][2] = { { 0x02, 0x12 }, { 0x03, 0x0a }, { 0x04, 0x08 }, { 0x05, 0x0d }, { 0x06, 0x31 }, { 0x07, 0 }, { 0x08, 0 }, { 0x09, 0 }, { 0x0a, 0 }, { 0x0b, 0 },
		{ 0x10, 0x00 }, { 0x11, 0x04 }, { 0x12, 0x32 }, { 0x13, 0x08 }, { 0x14, 0x40 }, { 0x15, 0x00 } };
	enum : u8 { XG_MONO_POLY = 0x05, XG_PART_ASSIGN = 0x06, XG_PART_MODE = 0x07, XG_NOTE_SHIFT = 0x08, XG_DETUNE_HIGH = 0x09, XG_DETUNE_LOW = 0x0a };
	enum : int { CC_ALL_SOUND_OFF = 120, CC_MONO = 126, CC_POLY = 127 };
	static constexpr int BEND_RANGE_ZERO = 64, BEND_RANGE_MAX = 24;   // 08 pp 23 の 64 が 0 半音。上は 24 半音まで
	static constexpr u32 SAMPLE_RAM_FIRST_WORD = 0x1000000;
	static constexpr u8 INSERTION_DEFAULT_MSB = 0x49;         // XG の既定のインサーション（DISTORTION）。後ろのインサーションの出口の利得の表もこれで引く
	static constexpr int INSERTION_LEVEL_PARAM = 5;           // DISTORTION の Output Level
	static constexpr u16 INSERTION_DEFAULT_LEVEL = 64;
	// 出口の語。母線の値を 24 ビットの語にして（0 の側へ切り捨て）、4 ビット右へずらし（床の側）、18 ビットに収める。
	// 全振幅は母線の単位の 0.25。浮動小数のまま出すと、実機なら 0 になる小さい余韻（-110 dB の下）が続いていた
	static constexpr float DAC_FULL = 131072.0f, DAC_STEPS_PER_BUS = 4.0f * DAC_FULL;
	static float dac_word(float bus)
	{
		const float steps = std::floor(native_fx::fx_flow::sat(bus) * DAC_STEPS_PER_BUS);
		return std::clamp(steps, -DAC_FULL, DAC_FULL - 1.0f) * (1.0f / DAC_FULL);
	}

	// 入力の待ち行列はオーディオスレッドで動くので、固定の領域に置く。溢れたら捨てて印を立てる
	void queue(u8 kind, int chan, u8 d0, u8 d1, u64 rx8, const u8 *sx = nullptr, u32 sx_len = 0)
	{
		if (m_q_count == QUEUE_SIZE || m_sx_used + sx_len > SYSEX_BYTES) {
			m_midi_overflowed = true;
			return;
		}
		// 受け終えた時刻 + 処理の時間を切り捨て。離しは押しの処理を待たない
		u64 at = (rx8 + ON_PROC * RX_UNIT) / RX_UNIT - (kind == ev_off ? ON_PROC - OFF_PROC : kind == ev_on ? NOTE_READ_AHEAD : 0);
		if (kind == ev_on)
			for (int part = 0; part < 64; part++)
				if (pb(part)[::xg::settings::PART_CHANNEL] == chan && m_drv.is_drum(part)) {
					at -= DRUM_LEAD;
					break;
				}
		const u32 sx_at = m_sx_used;
		if (sx_len)
			std::memcpy(m_sx_bytes.data() + sx_at, sx, sx_len);
		m_sx_used += sx_len;
		// 時刻の順に並べる（離しは押しより早く効くので、後ろから探して入れる）
		u32 i = m_q_count;
		while (i > 0 && queued(i - 1).at > at) {
			queued(i) = queued(i - 1);
			i--;
		}
		queued(i) = event{ at, kind, u8(chan), d0, d1, sx_at, sx_len };
		m_q_count++;
	}
	event &queued(u32 i) { return m_q_items[(m_q_head + i) % QUEUE_SIZE]; }
	void apply_due_events()
	{
		while (m_q_count && queued(0).at <= m_clock) {
			const event e = queued(0);
			m_q_head = (m_q_head + 1) % QUEUE_SIZE;
			m_q_count--;
			apply(e);
			// 待ちが空になったら SysEx の領域を頭から使い直す
			if (!m_q_count)
				m_sx_used = 0;
		}
	}

	void apply(const event &e)
	{
		if (e.kind == ev_sysex) {
			m_sysex_from_panel = e.chan == PORT_PANEL;
			m_sysex_inner = false;
			sysex(sysex_bytes{ m_sx_bytes.data() + e.sx_at, e.sx_len });
			m_sysex_from_panel = false;
		} else if (m_perf_on && e.chan == m_perf_channel && performance_event(e)) {
			// パフォーマンスのチャンネルで、記録を動かす CC とプログラムチェンジ
		} else
			// そのチャンネルを聞いているパート全部（08 pp 04。127 は OFF なので、どのチャンネルとも合わない）
			for (int part = 0; part < 64; part++)
				if (pb(part)[::xg::settings::PART_CHANNEL] == e.chan)
					apply_part(e, part);
		if (e.kind == ev_on) {
			if (const int lost = m_drv.note_writes_lost_ticks())
				move_eg_grid(lost);
			m_drv.settle_note_steps();
		}
	}

	// パフォーマンスの受け方: CC7 と CC10 は全体、音を作る CC は記録からの相対、プログラムは呼び出し
	bool performance_event(const event &e)
	{
		if (e.kind == ev_prog) {
			recall_performance(m_perf_latched_bank, e.d0);
			return true;
		}
		if (e.kind != ev_cc)
			return false;
		switch (e.d0) {
		case 0:
			if (e.d1 == 1 || e.d1 == 2)
				m_perf_latched_bank = e.d1 - 1;
			return true;
		case 32:
			return true;
		case 7:
			set_performance_volume(e.d1);
			return true;
		case 10:
			set_performance_pan(e.d1);
			return true;
		default:
			break;
		}
		static constexpr struct { u8 cc, part_offset; u32 xg; } RELATIVE_CONTROLS[] = {
			{ 71, PERF_PART_RESONANCE, ::xg::settings::PART_RESONANCE }, { 72, PERF_PART_RELEASE, ::xg::settings::PART_RELEASE }, { 73, PERF_PART_ATTACK, ::xg::settings::PART_ATTACK }, { 74, PERF_PART_CUTOFF, ::xg::settings::PART_CUTOFF }, { 75, PERF_PART_DECAY, ::xg::settings::PART_DECAY }, { 76, PERF_PART_VIBRATO_RATE, ::xg::settings::PART_VIBRATO_RATE }, { 77, PERF_PART_VIBRATO_DEPTH, ::xg::settings::PART_VIBRATO_DEPTH }, { 78, PERF_PART_VIBRATO_DELAY, ::xg::settings::PART_VIBRATO_DELAY },
			{ 91, PERF_PART_REVERB, ::xg::settings::PART_REVERB }, { 93, PERF_PART_CHORUS, ::xg::settings::PART_CHORUS }, { 94, PERF_PART_VARIATION, ::xg::settings::PART_VARIATION },
		};
		for (const auto &item : RELATIVE_CONTROLS)
			if (item.cc == e.d0) {
				for (int part = 0; part < PERFORMANCE_PARTS; part++)
					panel_write(::xg::settings::XG_PART, u8(part), u8(item.xg), u8(std::clamp(int(m_perf[part * PERFORMANCE_PART_SIZE + item.part_offset]) + e.d1 - 64, 0, 127)));
				return true;
			}
		return false;
	}

	// ベロシティの範囲（XG の 08 pp 6D-6E）
	bool velocity_in_range(int part, int velocity) const
	{
		const ::xg::part_state &p = m_state.parts[size_t(part)];
		const int lo = p[::xg::settings::PART_VELOCITY_LOW], hi = p[::xg::settings::PART_VELOCITY_HIGH];
		return lo <= hi ? velocity >= lo && velocity <= hi : velocity >= lo || velocity <= hi;
	}

	// 受信スイッチ（XG の 08 pp 30-3F）
	bool receives(int part, u32 xg_addr) const { return m_state.parts[size_t(part)][xg_addr] != 0; }

	// そのコントロールチェンジをパートが受けるか。バンク選択とチャンネルモード（120 以上）はスイッチに関係なく受ける
	bool receives_control(int part, int cc) const
	{
		if (cc == 0 || cc == 32 || cc >= 120)
			return true;
		if (!receives(part, RECEIVE_CONTROL))
			return false;
		switch (cc) {
		case 1: return receives(part, RECEIVE_MODULATION);
		case 7: return receives(part, RECEIVE_VOLUME);
		case 10: return receives(part, RECEIVE_PAN);
		case 11: return receives(part, RECEIVE_EXPRESSION);
		case 64: return receives(part, RECEIVE_HOLD);
		case 65: return receives(part, RECEIVE_PORTAMENTO);
		case 66: return receives(part, RECEIVE_SOSTENUTO);
		case 67: return receives(part, RECEIVE_SOFT);
		case 6: case 38: case 96: case 97: return receives(part, m_rpn[part].nrpn ? RECEIVE_NRPN : RECEIVE_RPN);
		default: return true;
		}
	}

	void apply_part(const event &e, int part)
	{
		switch (e.kind) {
		case ev_off: m_drv.note_off(part, e.d0); break;
		case ev_on:
			if (!receives(part, RECEIVE_NOTE) || (m_part_mute >> part & 1))
				break;
			// 鍵の範囲の外（08 pp 0F/10）と、押しを受けないドラムの打（3n rr 0A = 0）は鳴らさない
			if (!m_drv.note_in_range(part, e.d0) || !velocity_in_range(part, e.d1) || (m_drv.is_drum(part) && !m_drv.drum_rcv_note_on(part, e.d0)))
				break;
			m_drv.begin_read_ahead(NOTE_READ_AHEAD);
			m_drv.note_on(part, e.d0, e.d1);
			m_drv.end_read_ahead();
			break;
		case ev_cc:
			if (receives_control(part, e.d0))
				control(part, e.d0, e.d1);
			break;
		case ev_prog:
			if (!receives(part, RECEIVE_PROGRAM))
				break;
			m_sel[part].prog = e.d0;
			pb(part)[::xg::settings::PART_BANK_MSB] = m_sel[part].msb;
			pb(part)[::xg::settings::PART_BANK_LSB] = m_sel[part].lsb;
			pb(part)[::xg::settings::PART_PROGRAM] = e.d0;
			select_voice(part);
			break;
		case ev_bend:
			if (!receives(part, RECEIVE_BEND))
				break;
			pb(part).bend_half = e.d1 >> 1;
			m_drv.bend(part, int(e.d1) << 7 | e.d0);
			break;
		// アフタータッチ。割り当て（08 pp 4D-58）に従って音程・切る高さ・音量・LFO に効く（native_driver の refresh_assign_*）
		case ev_cat:
			if (!receives(part, RECEIVE_CHANNEL_PRESSURE))
				break;
			m_drv.chan_press(part, e.d0);
			break;
		case ev_pat:
			if (!receives(part, RECEIVE_POLY_PRESSURE))
				break;
			m_drv.poly_at(part, e.d0, e.d1);
			break;
		default: break;
		}
	}


	// XG の 1 バイト。種類を替えたら、そのエフェクトのパラメータは種類の既定に戻る
	void fx_param(u8 mm, u8 ll, u8 v)
	{
		using smu2000::xg::effect_tables::KEEP;
		smu2000::xg::effect_tables::Input &in = m_fxin;
		v &= 0x7f;
		if (mm == 0x40) {
			// Mlt EQ Lock のあいだ、外からの EQ は受けない
			if (m_eq_locked && !m_sysex_from_panel)
				return;
			if (ll == 0 && v < 5) {
				m_eq_type = v;
				std::copy_n(m_eq_presets[v], 20, in.eq);
				return;
			}
			if (ll < 1 || ll > 20)
				return;
			const int k = (ll - 1) % 4;
			if (k == 0) v = std::clamp<u8>(v, 52, 76);
			if (k == 2) v = std::max<u8>(v, 1);
			in.eq[ll - 1] = v;
			return;
		}
		auto clear = [](u16 *p) { std::fill(p, p + 16, KEEP); };
		if (ll == 0x00) { if (in.rev_msb != v) clear(in.rev); in.rev_msb = v; m_reverb_type_written = true; }
		else if (ll == 0x01) { if (in.rev_lsb != v) clear(in.rev); in.rev_lsb = v; m_reverb_type_written = true; }
		else if (ll >= 0x02 && ll <= 0x0b) in.rev[ll - 0x02] = v;
		else if (ll == 0x0c) in.mix[1] = v;
		else if (ll == 0x0d) in.mix[0] = v;
		else if (ll >= 0x10 && ll <= 0x15) in.rev[ll - 0x10 + 10] = v;
		else if (ll == 0x20) { if (in.cho_msb != v) clear(in.cho); in.cho_msb = v; m_chorus_type_written = true; }
		else if (ll == 0x21) { if (in.cho_lsb != v) clear(in.cho); in.cho_lsb = v; m_chorus_type_written = true; }
		else if (ll >= 0x22 && ll <= 0x2b) in.cho[ll - 0x22] = v;
		else if (ll == 0x2c) in.mix[3] = v;
		else if (ll == 0x2d) in.mix[2] = v;
		else if (ll == 0x2e) in.mix[4] = v;
	}

	static constexpr u64 EFFECT_START_LEAD_EXTRA_INSERTION = 10;
	static u64 effect_start_lead(int msb, int lsb) { return msb == 0x47 && lsb == 1 ? 57 : 70; }
	static constexpr u64 LFO_GRID = 882, LFO_GRID_PHASE_MASTER = 837, LFO_GRID_PHASE_SLAVE = 447;
	static constexpr u32 LFO_DELAY_RESET = 8362, LFO_DELAY_TYPE = 7464, LFO_DELAY_PARAM = 130;
	// GS: リセットから 8723、コーラスのマクロから 7545。パラメータの LFO はメッセージを受け終える前に書くので、すぐ
	static constexpr u32 LFO_DELAY_QUEUED_RESET = 765;
	// パフォーマンスの読み込みで LFO を書くまで。モードに入るときはリセットと一緒に書く。記録ごとに ±28 ms ばらつく中央の値
	static constexpr u32 LFO_DELAY_PERFORMANCE = 11500, LFO_DELAY_PERFORMANCE_ENTRY = 8200;
	static constexpr u32 LFO_DELAY_VARIATION = 415, LFO_DELAY_FIRST_INSERTION = 850, LFO_DELAY_EXTRA_INSERTIONS = 960;
	static constexpr u32 LFO_DELAY_GS_RESET = 8723, LFO_DELAY_GS_CHORUS_MACRO = 7545, LFO_DELAY_GS_PARAM = 0;

	// エフェクト の出口をミキサが拾う結線と減衰（リバーブ・コーラスの戻り、バリエーション・インサーション 1 の戻り）
	// 同じパートに重なったエフェクトの次の段。インサーション 1-4、バリエーションの順に通す。
	// 戻り値は 0-3 がインサーション、4 がバリエーション、無ければ負
	int chain_next(int insertion) const
	{
		const int part = m_state.insertion[insertion][::xg::settings::XG_INSERTION_PART];
		if (part >= 64)
			return -1;
		for (int n = insertion + 1; n < 4; n++)
			if (m_state.insertion[n][::xg::settings::XG_INSERTION_PART] == part)
				return n;
		if (m_state.effect[::xg::settings::XG_VARIATION_CONNECTION] == 0 && m_state.effect[::xg::settings::XG_VARIATION_PART] == part)
			return 4;
		return -1;
	}

	void apply_chain(native_voice::effect_id effect, int insertion)
	{
		using namespace native_voice;
		const int next = chain_next(insertion);
		if (next < 0) {
			m_mixer.set_chain(effect, -1, 0.0f);
			return;
		}
		static constexpr u16 PAN_LEFT = 0x10ff, PAN_RIGHT = 0xff10;
		if (next == 4) {
			// バリエーションへは送りの線で届く
			set_return(effect, PAN_LEFT, PAN_RIGHT, 0xffff, 0xff00);
			m_mixer.set_chain(effect, -1, 0.0f);
			return;
		}
		set_return(effect, PAN_LEFT, PAN_RIGHT, 0xffff, 0xffff);
		m_mixer.set_chain(effect, bus_ins2 + next - 1, att_gain(CHAIN_LINK_ATTENUATION));
	}

	void fx_mixer_refresh()
	{
		extra_insertion_mixer_refresh();
		const bool var_sys = m_state.effect[::xg::settings::XG_VARIATION_CONNECTION] == 1;
		auto ins_words = [&](int part, u16 w[2][3]) {
			if (part < 0 || part >= 64)
				return;
			const ::xg::part_state &p = pb(part);
			u16 out[4];
			smu2000::xg::effect_tables::fx_insert_mixer(m_prog->data(), p.level, p[::xg::settings::PART_PAN], p[::xg::settings::PART_REVERB], p[::xg::settings::PART_CHORUS], p[::xg::settings::PART_VARIATION], p[::xg::settings::PART_DRY], var_sys, out);
			w[0][0] = out[0]; w[1][0] = out[1];
			w[0][1] = w[1][1] = out[2];
			w[0][2] = w[1][2] = out[3];
		};
		u16 var_mix[2][3] = { { 0x19ff, 0x1326, 0xffff }, { 0xff19, 0x1326, 0xffff } };
		u16 ins_mix[2][3] = { { 0x19ff, 0x1326, 0xffff }, { 0xff19, 0x1326, 0xffff } };
		ins_words(m_state.insertion[0][::xg::settings::XG_INSERTION_PART], ins_mix);
		if (m_var.M[9] & 1) {
			u16 mo[4][3];
			smu2000::xg::effect_tables::fx_mixer(m_prog->data(), m_var.M, mo);
			for (int k = 0; k < 3; k++) {
				var_mix[0][k] = mo[0][k];
				var_mix[1][k] = mo[1][k];
			}
		} else
			ins_words(m_state.effect[::xg::settings::XG_VARIATION_PART], var_mix);
		set_return(native_voice::effect_reverb, m_system_returns[0][0], m_system_returns[1][0], (m_system_returns[0][1] & 0xff00) | 0xff, 0xffff);
		set_return(native_voice::effect_chorus, m_system_returns[2][0], m_system_returns[3][0], m_system_returns[2][1], 0xffff);
		set_return(native_voice::effect_ins1, ins_mix[0][0], ins_mix[1][0], ins_mix[0][1], ins_mix[0][2]);
		set_return(native_voice::effect_variation, var_mix[0][0], var_mix[1][0], var_mix[0][1], var_mix[0][2] | 0xff);
		apply_chain(native_voice::effect_ins1, 0);
	}

	// 戻りの減衰。pan_* は上位が左へ・下位が右へ、level の上位が乾いた音・下位がリバーブ、send の上位がコーラス・下位がバリエーション
	void set_return(native_voice::effect_id effect, u16 pan_left, u16 pan_right, u16 level, u16 send)
	{
		const int pan[2][2] = { { pan_left >> 8, pan_left & 0xff }, { pan_right >> 8, pan_right & 0xff } };
		const int level_att[4] = { level >> 8, level & 0xff, send >> 8, send & 0xff };
		m_mixer.set_return(effect, pan, level_att);
	}

	// 種類は演奏中に変わり、そこはオーディオスレッドなので、形を全部先に作っておく。
	// ponytail: 遅延線の分で全部で約 24 MB。効くようなら、1 区画の形で遅延線を分け合う
	struct effect_flow_set {
		std::unique_ptr<native_fx::effect_flow> by_group[32];
		native_fx::effect_flow *choose(int group)
		{
			native_fx::effect_flow *flow = group >= 0 && group < 32 ? by_group[group].get() : nullptr;
			if (flow)
				flow->reset();
			return flow;
		}
	};

	struct fx_block {
		u8 msb = 0, lsb = 0;
		u16 P[16] = {};                 // パラメータ 1-16 の今の値（種類の読み込み後に送り直す用。0xffff は送っていない）
		u16 M[16] = {};
		smu2000::fx_settings::effect_request request;
		u8 pend_hi = 0;                 // 2 バイトのパラメータの上位（XG は MSB・LSB の 2 バイト）
		int insertion_part = 0;                  // 1-3: インサーション 2-4（第 2 群）
		bool lfo_restart = false;       // 種類を読み込んだので、LFO を位相 0 から数え直す
	};

	void extra_insertion_mixer_refresh()
	{
		const bool var_sys = m_state.effect[::xg::settings::XG_VARIATION_CONNECTION] == 1;
		static constexpr native_voice::effect_id EFFECT[3] = { native_voice::effect_ins2, native_voice::effect_ins3, native_voice::effect_ins4 };
		for (int k = 0; k < 3; k++) {
			const int part = m_state.insertion[k + 1][::xg::settings::XG_INSERTION_PART];
			u16 w[4] = { 0x19ff, 0xff19, 0x1326, 0xffff };   // 掛かり先が無いときは既定のパートの値（音は来ない）
			if (part >= 0 && part < 64) {
				const ::xg::part_state &p = pb(part);
				smu2000::xg::effect_tables::fx_insert_mixer(m_prog->data(), p.level, p[::xg::settings::PART_PAN], p[::xg::settings::PART_REVERB], p[::xg::settings::PART_CHORUS], p[::xg::settings::PART_VARIATION], p[::xg::settings::PART_DRY], var_sys, w);
			}
			set_return(EFFECT[k], w[0], w[1], w[2], w[3]);
			apply_chain(EFFECT[k], k + 1);
		}
	}

	// ---- 第 2 群のエフェクト（インサーション 2-4）
	//
	// 親のインサーション 1 と同じ形。
	// 入出力と LFO の番号をずらして回す
	struct extra_insertion {
		int n;                 // 0-2（インサーション 2-4）
		fx_block b;
		int group = -1;
		native_fx::effect_flow *flow = nullptr;
		effect_flow_set flows;
		u32 quiet = 0, hold = 0;
		u64 active_at = 0;     // 種類を読み込み終える時刻。それまでは止まっていて音も出ない
	};
	static int extra_insertion_lfo(int n) { return 6 + n * 6; }           // その区画の LFO の先頭

	void extra_insertion_load(extra_insertion &si)
	{
		si.b.insertion_part = si.n + 1;
		fx_block_load(si.b, smu2000::xg::effect_tables::FX_INS);
	}

	void extra_insertion_update(extra_insertion &si, u32 lfo_delay)
	{
		// 形の組と出口の利得の表は親（インサーション 1）の並びで求める
		if (si.b.request.circuit != si.group) {
			si.group = si.b.request.circuit;
			si.flow = si.flows.choose(si.group);
		}
		if (!si.flow)
			return;
		smu2000::fx_settings::effect_extras extras;
		si.flow->configure(si.b.request, extras);
		const int lb = extra_insertion_lfo(si.n);
		const bool restart = si.b.lfo_restart || lfo_delay == LFO_DELAY_RESET;
		const u64 at = m_lfo_load_delay ? m_clock + m_lfo_load_delay + LFO_DELAY_EXTRA_INSERTIONS : si.b.lfo_restart ? lfo_load_time(si.b, true) : m_clock + (lfo_delay == LFO_DELAY_RESET ? lfo_delay + m_repeat_reset_delay : LFO_DELAY_PARAM);
		if (si.b.lfo_restart)
			si.active_at = at - EFFECT_START_LEAD_EXTRA_INSERTION;
		si.b.lfo_restart = false;
		for (int k = 0; k < 6; k++)
			if (extras.lfo[k] != m_extra_insertion_lfo.setting[lb + k] || restart) {
				m_extra_insertion_lfo.next[lb + k] = extras.lfo[k];
				m_extra_insertion_lfo.at[lb + k] = at;
			}
		si.quiet = 0;
		si.hold = REVERB_QUIET_HOLD;
		// 表は第 2 群の 音源 のリバーブ RAM へ
		table_put(m_extra_insertion_table, extras.ram);
	}

	static void table_put(std::vector<float> &table, const ::smu2000::xg::effect_tables::RamTable &words)
	{
		words.for_each([&table](u32 addr, u32 w) {
			if (addr < table.size())
				table[addr] = native_fx::fx_flow::word(float(s32(w) >> 8) * (1.0f / 8388608.0f));
		});
	}


	void fx_block_load(fx_block &b, int which)
	{
		// XG の層の M5-M9: SYSTEM はそのまま、INSERTION は 0・127・0・0
		std::pair<int, int> mix[5];
		const bool sys = which == smu2000::xg::effect_tables::FX_VAR && (b.M[9] & 1);
		mix[0] = { 5, sys ? b.M[5] : 0 };
		mix[1] = { 6, sys ? b.M[6] : 127 };
		mix[2] = { 7, sys ? b.M[7] : 0 };
		mix[3] = { 8, sys ? b.M[8] : 0 };
		mix[4] = { 9, b.M[9] };
		b.request = smu2000::fx_settings::make_request(m_prog->data(), which, b.msb, b.lsb, mix, 5, b.insertion_part, b.insertion_part ? m_ins1.msb : -1, b.insertion_part ? m_ins1.lsb : -1);
		std::fill(b.P, b.P + 16, u16(0xffff));
	}

	void fx_block_sysex(fx_block &b, int which, u8 a0, const sysex_bytes &x)
	{
		const bool var = which == smu2000::xg::effect_tables::FX_VAR;
		size_t first = 6;
		// 種類を 2 バイトで送ると、同じ種類でも既定値を読み込み直す（1 バイトだけなら、変わったときだけ）
		if (a0 == 0 && x.size() >= 8) {
			b.msb = x[6] & 0x7f;
			b.lsb = x[7] & 0x7f;
			fx_block_load(b, which);
			b.lfo_restart = true;
			first = 8;
		}
		for (size_t i = first; i < x.size(); i++) {
			const int a = a0 + int(i - 6), v = x[i] & 0x7f;
			// バリエーション（40 からの差）: 02-15 = 42-55 がパラメータ 1-10（2 バイト）、16 戻り・17 パン・18 リバーブへ・
			// 19 コーラスへ・1A 繋ぎ方・1B パート、30-35 = 70-75 がパラメータ 11-16。
			// インサーション: 02-0B がパラメータ 1-10（1 バイト）か 30-43（2 バイト。種類による）、0C パート、20-25 がパラメータ 11-16
			//
			if (a == 0) { if (b.msb != v) { b.msb = u8(v); fx_block_load(b, which); b.lfo_restart = true; } continue; }
			if (a == 1) { if (b.lsb != v) { b.lsb = u8(v); fx_block_load(b, which); b.lfo_restart = true; } continue; }
			int n = 0, val = v;
			const int w0 = var ? 0x02 : 0x30, w1 = var ? 0x16 : 0x44;   // 2 バイトの並び
			if (a >= w0 && a < w1) {
				// インサーションは種類ごとに 2 バイトか 1 バイトのどちらかしか効かない。バリエーションは下位だけでは効かない
				if (!var && !fx_type_is_wide(b.msb, b.lsb))
					continue;
				if (!((a - w0) & 1)) { b.pend_hi = u8(v); continue; }
				if (i == first)
					continue;
				n = (a - w0) / 2 + 1;
				val = b.pend_hi << 7 | v;
			} else if (!var && a >= 0x02 && a <= 0x0b) {
				if (fx_type_is_wide(b.msb, b.lsb))
					continue;
				n = a - 1;
			} else if (var && a >= 0x16 && a <= 0x1a) {
				static const u8 MI[5] = { 6, 5, 7, 8, 9 };        // 戻り → M6、パン → M5、送り 2 つ → M7・M8、繋ぎ方 → M9
				b.M[MI[a - 0x16]] = u16(v);
				if (a == 0x1a) b.request.set_connection(v);
				continue;
			} else if (var ? (a >= 0x30 && a <= 0x35) : (a >= 0x20 && a <= 0x25))
				n = a - (var ? 0x30 : 0x20) + 11;
			if (n) {
				val = clamp_fx_param(b.msb, b.lsb, n, val);
				b.P[n - 1] = u16(val);
				b.request.set_param(n, u16(val));
			}
		}
	}

	u64 lfo_load_time(const fx_block &b, bool second_group) const
	{
		u32 load = second_group ? 7000 : 7400;
		if (second_group) {
			if (b.msb >= 0x74 && b.msb <= 0x76)
				load = 8800;
		} else if ((b.msb == 0x47 || b.msb == 0x4b) && b.lsb == 1)
			load = 12700;
		else if (b.msb == 0x68 || b.msb == 0x72)
			load = 10100;
		else if (b.msb == 0x69)
			load = 19800;
		else if (b.msb >= 0x74 && b.msb <= 0x76)
			load = 21500;
		const u64 phase = second_group ? LFO_GRID_PHASE_SLAVE : LFO_GRID_PHASE_MASTER;
		const u64 ready = m_clock + load;
		return ready + (phase + LFO_GRID * 4 - ready % LFO_GRID) % LFO_GRID;
	}

	static int clamp_fx_param(int msb, int lsb, int n, int value)
	{
		for (const panel_fx::receive_range &r : panel_fx::RECEIVE_RANGES)
			if (r.msb == msb && r.lsb == lsb && r.number == n)
				return std::clamp<int>(value, r.lowest, r.highest);
		const panel_fx::type_table table = panel_fx::find(panel_fx::table_insertion, msb << 7 | lsb);
		for (int i = 0; i < table.count; i++)
			if (table.params[i].address == n)
				return std::clamp<int>(value, table.params[i].lowest, table.params[i].highest);
		return value;
	}

	void fx_update(u32 lfo_delay)
	{
		namespace settings = smu2000::fx_settings;
		const settings::system_request system = settings::make_system_request(m_prog->data(), m_fxin);
		u16 wanted_lfo[24] = {};
		// システムコーラス
		u16 chorus_lfo[4] = {};
		fx_region &chorus = m_regions[0];
		choose_flow(chorus, system.chorus_circuit);
		chorus.flow->configure(system, chorus_lfo);
		std::copy(chorus_lfo, chorus_lfo + 4, wanted_lfo + chorus.first_lfo);
		restart_region_tail(chorus);
		// インサーション 1 とバリエーション
		for (const fx_block *b : { &m_var, &m_ins1 }) {
			fx_region &region = m_regions[b == &m_ins1 ? 1 : 2];
			settings::effect_extras extras;
			choose_flow(region, b->request.circuit);
			if (region.flow) {
				region.flow->configure(b->request, extras);
				restart_region_tail(region);
			} else {
				settings::start_extras(b->request, extras);
			}
			std::copy(extras.lfo, extras.lfo + 6, wanted_lfo + region.first_lfo);
			table_put(m_table, extras.ram);
		}
		// 全部の区画の設定が揃ってから LFO を書く。種類を読み込み直した区画だけ数え直す（ほかの区画の揺れは止めない）
		const u64 var_at = m_lfo_load_delay ? m_clock + m_lfo_load_delay + LFO_DELAY_VARIATION : m_var.lfo_restart ? lfo_load_time(m_var, false) : 0,
		          ins1_at = m_lfo_load_delay ? m_clock + m_lfo_load_delay + LFO_DELAY_FIRST_INSERTION : m_ins1.lfo_restart ? lfo_load_time(m_ins1, false) : 0;
		if (m_var.lfo_restart)
			m_regions[2].active_at = var_at - effect_start_lead(m_var.msb, m_var.lsb);
		if (m_ins1.lfo_restart)
			m_regions[1].active_at = ins1_at - effect_start_lead(m_ins1.msb, m_ins1.lsb);
		for (int i = 0; i < 24; i++) {
			const fx_block *block = i >= m_regions[2].first_lfo ? &m_var : i >= m_regions[1].first_lfo ? &m_ins1 : nullptr;
			const bool reset_keeps_running = (lfo_delay == LFO_DELAY_RESET || lfo_delay == LFO_DELAY_GS_RESET) && m_reset_keeps_chorus_lfo;
			const bool restart = block ? block->lfo_restart || lfo_delay == LFO_DELAY_RESET : lfo_delay != LFO_DELAY_PARAM && lfo_delay != LFO_DELAY_GS_PARAM && !reset_keeps_running;
			if (reset_keeps_running && !block && m_lfo.at[i] > m_clock && m_lfo.next[i] == wanted_lfo[i]) {
				// まだ書いていない 1 回目のリセットのぶん。2 回目を受けると書くのが遅れる
				if (m_lfo_queued_reset_pending)
					m_lfo.at[i] += LFO_DELAY_QUEUED_RESET;
				continue;
			}
			if (wanted_lfo[i] == m_lfo.setting[i] && !restart)
				continue;
			m_lfo.next[i] = wanted_lfo[i];
			// 種類を読み込んでいるあいだに来た変更は、読み込みのあとで効く（予定を早めない）
			m_lfo.at[i] = std::max(m_lfo.at[i], block && block->lfo_restart ? (block == &m_var ? var_at : ins1_at)
			            : m_clock + (block && lfo_delay != LFO_DELAY_RESET ? LFO_DELAY_PARAM : lfo_delay)
			              + (lfo_delay == LFO_DELAY_RESET || lfo_delay == LFO_DELAY_GS_RESET ? m_repeat_reset_delay : 0));
		}
		m_lfo_queued_reset_pending = false;
		m_var.lfo_restart = m_ins1.lfo_restart = false;
		settings::system_returns(system, m_system_returns);
		fx_mixer_refresh();
		for (extra_insertion &si : m_extra_insertions)
			extra_insertion_update(si, lfo_delay);
		native_fx::master_eq_params eq_params;
		settings::settle_master_eq(system, eq_params);
		m_eq.configure(eq_params);
		m_master_output_gain[0] = eq_params.output_gain_left;
		m_master_output_gain[1] = eq_params.output_gain_right;
		native_fx::reverb_params reverb_params;
		settings::settle_reverb(system, reverb_params);
		m_reverb.configure(reverb_params);
		output_gain_refresh();
		m_reverb_quiet = 0;
		for (int i = 0; i < 24; i++)
			m_lfo.inc[i] = lfo_increment(m_lfo.setting[i]);
		m_fx_on = true;
	}

	// 形ごとのフロー（src/native/fx_flow_*.h）を、ミキサーの順（doc/voice-flow/mixer.md の 6 章）で回す
	struct fx_region;
	void choose_flow(fx_region &region, int group)
	{
		region.group = group;
		if (region.group != region.made) {
			region.made = region.group;
			region.flow = region.flows.choose(region.group);
		}
	}
	void restart_region_tail(fx_region &region)
	{
		region.quiet = 0;
		region.hold = REVERB_QUIET_HOLD;
	}

	// エフェクト の LFO の 1 サンプルの増え方
	// **電源を入れたときの LFO**。実機は起動の途中でエフェクトの LFO（既定のコーラスの 4 本）を書き、
	// MIDI を受けられるようになるまでに LFO_RUN_AT_POWER_ON サンプル回している。
	// 0 から回すと、電源を入れた直後のコーラスの揺れが最大 4 dB ずれた（リセットのあとは一致していた）
	static constexpr int LFO_RUN_AT_POWER_ON = 52100;
	void lfo_power_on()
	{
		m_chorus_type_loaded = false;
		m_lfo.restart();
		m_extra_insertion_lfo.restart();
		m_lfo.step(LFO_RUN_AT_POWER_ON);
		m_extra_insertion_lfo.step(LFO_RUN_AT_POWER_ON);
	}

	static u32 lfo_increment(u16 setting)
	{
		static constexpr int DT[8] = { 0, 32, 64, 128, 256, 512, 1024, 2048 };
		static constexpr int SH[8] = { 0, 0, 1, 2, 3, 4, 5, 6 };
		const int i = setting & 0xff;
		return u32(DT[i >> 5] + ((i & 31) << SH[i >> 5]));
	}

	// エフェクト の LFO 24 本（1 群ぶん）。書かれた所から位相 0 で数え直す
	struct lfo_bank {
		u16 setting[24] = {}, next[24] = {};
		u32 cnt[24] = {}, inc[24] = {};
		u64 at[24] = {};                   // その LFO を書き換える時刻（0 なら予定なし）

		u64 next_due() const
		{
			u64 soonest = ~u64(0);
			for (int i = 0; i < 24; i++)
				if (at[i] && at[i] < soonest)
					soonest = at[i];
			return soonest;
		}

		void due(u64 clock)
		{
			for (int i = 0; i < 24; i++)
				if (at[i] && clock >= at[i]) {
					at[i] = 0;
					setting[i] = next[i];
					inc[i] = lfo_increment(setting[i]);
					cnt[i] = 0;
				}
		}
		void restart()
		{
			for (int i = 0; i < 24; i++) {
				if (at[i]) {
					at[i] = 0;
					setting[i] = next[i];
					inc[i] = lfo_increment(setting[i]);
				}
				cnt[i] = 0;
			}
		}
		void step(int samples)
		{
			for (int i = 0; i < 24; i++)
				cnt[i] = (cnt[i] + inc[i] * u32(samples)) & 0x3fffff;
		}
		// LFO i の n サンプルぶん
		void values(int i, int n, const u16 *sin, float *out) const
		{
			if (!inc[i]) {
				std::fill(out, out + n, value_at(i, cnt[i], sin));
				return;
			}
			u32 c = cnt[i];
			for (int k = 0; k < n; k++) {
				out[k] = value_at(i, c, sin);
				c = (c + inc[i]) & 0x3fffff;
			}
		}
		// その位相の値（0 から 1 未満）
		float value_at(int n, u32 phase, const u16 *sin) const
		{
			// 1 周の 1/4 ごとに、0、1/12、1/8、1/6 周を足した位相。コンパイル時に決まる
			static constexpr std::array<u32, 16> OFFSETS = [] {
				constexpr u32 cycle = 0x20000;
				const u32 within_quarter[4] = { 0, cycle / 12, cycle / 8, cycle / 6 };
				std::array<u32, 16> offsets{};
				for (u32 n = 0; n < 16; n++)
					offsets[n] = (n / 4) * (cycle / 4) + within_quarter[n % 4];
				return offsets;
			}();
			const u16 r = setting[n];
			u32 base = (phase >> 5) << ((r >> 8) & 3);
			base = (base + OFFSETS[r >> 12]) & 0x1ffff;
			u32 res = 0;
			switch ((r >> 10) & 3) {
			case 0:
				res = base < 0x8000 ? sin[base] : base < 0x10000 ? sin[(base & 0x7fff) ^ 0x7fff]
				    : base < 0x18000 ? sin[base & 0x7fff] ^ 0xffff : sin[(base & 0x7fff) ^ 0x7fff] ^ 0xffff;
				break;
			case 1: res = (base + 0x8000) & 0x1ffff; if (res & 0x10000) res ^= 0x1ffff; break;
			case 2: res = base >> 1; break;
			case 3: res = (base ^ 0x1ffff) >> 1; break;
			}
			return float(res << 7) * (1.0f / 8388608.0f);
		}
	};


	// エフェクトを 1 ブロック回す。**送りが 0 で出口も 0 のまま、遅延の窓 + 0.1 秒たったら回さない**
	void run_effect(native_fx::effect_flow &fx, u32 &quiet, u32 hold, const lfo_bank &bank, int first_lfo, const float *table,
	                const float *in_left, const float *in_right, float *out_left, float *out_right, int n)
	{
		const bool no_input = all_zero(in_left, n) && all_zero(in_right, n);
		if (no_input && quiet >= hold) {
			std::fill(out_left, out_left + n, 0.0f);
			std::fill(out_right, out_right + n, 0.0f);
			return;
		}
		alignas(32) float lfo[6][native_voice::max_block];
		const float *const lfos[6] = { lfo[0], lfo[1], lfo[2], lfo[3], lfo[4], lfo[5] };
		for (int k = 0; k < 6; k++)
			bank.values(first_lfo + k, n, m_sin.data(), lfo[k]);
		fx.process(in_left, in_right, lfos, table, out_left, out_right, n);
		quiet = no_input && all_silent(out_left, n) && all_silent(out_right, n) ? quiet + u32(n) : 0;
	}

	static bool all_zero(const float *x, int n)
	{
		float any = 0.0f;
		for (int i = 0; i < n; i++)
			any = std::max(any, std::abs(x[i]));
		return any == 0.0f;
	}
	// 出口の語（24 ビット）で 0 に見えるくらい小さい
	static bool all_silent(const float *x, int n)
	{
		float any = 0.0f;
		for (int i = 0; i < n; i++)
			any = std::max(any, std::abs(x[i]));
		return any < 1.0f / 8388608.0f;
	}

	void run_reverb(const float *in_left, const float *in_right, float *out_left, float *out_right, int n)
	{
		const bool no_input = all_zero(in_left, n) && all_zero(in_right, n);
		if (no_input && m_reverb_quiet >= REVERB_QUIET_HOLD) {
			std::fill(out_left, out_left + n, 0.0f);
			std::fill(out_right, out_right + n, 0.0f);
			return;
		}
		m_reverb.process(in_left, in_right, out_left, out_right, n);
		m_reverb_quiet = no_input && all_silent(out_left, n) && all_silent(out_right, n) ? m_reverb_quiet + u32(n) : 0;
	}

	// ---- A/D INPUT（doc/voice-flow/mixer.md の 7 章）
	struct ad_part { u8 volume = 0, pan = 64, reverb = 0, chorus = 0, variation = 0, dry = 127; };
	void ad_param(int which, u8 address, u8 value)
	{
		ad_part &a = m_ad[which];
		switch (address) {
		case 0x0b: a.volume = value; break;
		case 0x0e: a.pan = value; break;
		case 0x11: a.dry = value; break;
		case 0x12: a.chorus = value; break;
		case 0x13: a.reverb = value; break;
		case 0x14: a.variation = value; break;
		default: return;
		}
		ad_refresh();
	}
	void ad_refresh()
	{
		using namespace native_voice;
		const bool var_sys = m_state.effect[::xg::settings::XG_VARIATION_CONNECTION] == 1;
		const u8 *rom = m_prog->data();
		for (int k = 0; k < 2; k++) {
			const ad_part &a = m_ad[k];
			u16 words[4];
			smu2000::xg::effect_tables::fx_insert_mixer(rom, a.volume && !(m_ad_mute >> k & 1) ? a.volume + 1 : 0, 64, a.reverb, a.chorus, a.variation, a.dry, var_sys, words);
			const int pan = a.pan ? a.pan : 64;
			const int pan_att[2] = { rom[::xg::nv::PAN_BASE_TAB + u32(pan)], rom[::xg::nv::PAN_BASE_TAB + u32(0x80 - pan)] };
			const int level_att[bus_ins1] = { words[2] >> 8, words[2] & 0xff, words[3] >> 8, words[3] & 0xff };
			for (int bus = 0; bus < bus_ins1; bus++)
				for (int ch = 0; ch < 2; ch++)
					m_ad_gain[k][bus][ch] = att_gain(pan_att[ch] + level_att[bus]) * AD_INPUT_GAIN;
		}
	}
	static float att_gain(int att)
	{
		return att >= 255 ? 0.0f : (1.0f - float(att & 15) / 32.0f) / float(1 << (att >> 4));
	}
	// 第 2 群の母線へ足す。入力から母線まで 2 サンプル遅れる
	void add_audio_input(int n, const float *ad_1, const float *ad_2)
	{
		using namespace native_voice;
		const float *const in[2] = { ad_1, ad_2 };
		for (int k = 0; k < 2; k++) {
			float delayed[max_block];
			delayed[0] = m_ad_held[k][0];
			if (n > 1)
				delayed[1] = m_ad_held[k][1];
			for (int i = 2; i < n; i++)
				delayed[i] = in[k] ? in[k][i - 2] : 0.0f;
			const float last = in[k] ? in[k][n - 1] : 0.0f;
			m_ad_held[k][0] = n > 1 ? (in[k] ? in[k][n - 2] : 0.0f) : m_ad_held[k][1];
			m_ad_held[k][1] = last;
			// 入口の直流止め
			float x1 = m_ad_filter[k][0], y1 = m_ad_filter[k][1];
			for (int i = 0; i < n; i++) {
				const float y = AD_HIGHPASS_CURRENT * (delayed[i] - x1) + AD_HIGHPASS_FEEDBACK * y1;
				x1 = delayed[i];
				y1 = y;
				delayed[i] = y;
			}
			m_ad_filter[k][0] = x1;
			m_ad_filter[k][1] = y1;
			if (m_ad[k].volume == 0)
				continue;
			for (int bus = 0; bus < bus_ins1; bus++)
				for (int ch = 0; ch < 2; ch++) {
					const float gain = m_ad_gain[k][bus][ch];
					if (gain == 0.0f)
						continue;
					float *out = m_second.s[bus][ch];
					for (int i = 0; i < n; i++)
						out[i] += gain * delayed[i];
				}
		}
	}

	void render_block(int n, float *left, float *right, const float *ad_1, const float *ad_2)
	{
		using namespace native_voice;
		m_voices.render(n, u32(m_clock - 1), m_first, m_second);
		add_audio_input(n, ad_1, ad_2);
		if (dry_tap) {
			for (int i = 0; i < n; i++) {
				left[i] = m_first.s[bus_dry][0][i] + m_second.s[bus_dry][0][i];
				right[i] = m_first.s[bus_dry][1][i] + m_second.s[bus_dry][1][i];
			}
			return;
		}
		alignas(32) float dry_left[max_block], dry_right[max_block];
		m_mixer.process(n, m_first, m_second, [&](effect_id e, const float *in_l, const float *in_r, float *out_l, float *out_r, int len) {
			fx_region *region = e == effect_chorus ? &m_regions[0] : e == effect_ins1 ? &m_regions[1] : e == effect_variation ? &m_regions[2] : nullptr;
			if (e == effect_reverb) {
				if (m_fx_on)
					run_reverb(in_l, in_r, out_l, out_r, len);
				else {
					std::fill(out_l, out_l + len, 0.0f);
					std::fill(out_r, out_r + len, 0.0f);
				}
			} else if (region && region->flow && m_clock >= region->active_at)
				run_effect(*region->flow, region->quiet, region->hold, m_lfo, region->first_lfo, m_table.data(), in_l, in_r, out_l, out_r, len);
			else if (!region && m_extra_insertions[e - effect_ins2].flow && m_clock >= m_extra_insertions[e - effect_ins2].active_at) {
				extra_insertion &si = m_extra_insertions[e - effect_ins2];
				run_effect(*si.flow, si.quiet, si.hold, m_extra_insertion_lfo, extra_insertion_lfo(si.n), m_extra_insertion_table.data(), in_l, in_r, out_l, out_r, len);
			} else {
				std::fill(out_l, out_l + len, 0.0f);
				std::fill(out_r, out_r + len, 0.0f);
			}
		}, dry_left, dry_right);
		m_lfo.step(n);
		m_extra_insertion_lfo.step(n);
		if (!m_fx_on) {
			std::fill(left, left + n, 0.0f);
			std::fill(right, right + n, 0.0f);
			return;
		}
		m_eq.process(dry_left, dry_right, n);
		// 出口。スピーカーは 1 サンプル遅れで、全振幅は母線の単位の 0.25（doc/voice-flow/mixer.md の 6 章）
		left[0] = m_exit_held[0];
		right[0] = m_exit_held[1];
		for (int i = 0; i < n - 1; i++) {
			left[i + 1] = dac_word(m_output_gain[0] * dry_left[i]);
			right[i + 1] = dac_word(m_output_gain[1] * dry_right[i]);
		}
		m_exit_held[0] = dac_word(m_output_gain[0] * dry_left[n - 1]);
		m_exit_held[1] = dac_word(m_output_gain[1] * dry_right[n - 1]);
	}

	::xg::part_state &pb(int part) { return m_state.parts[size_t(part)]; }

	void part_gain(int part)
	{
		::xg::part_state &p = pb(part);
		const int vol = p[::xg::settings::PART_VOLUME], expr = p.expression, mv = m_state.system[::xg::settings::SYS_VOLUME];
		int g = 0;
		if (vol && expr && mv) {
			g = std::max(1, ((vol + 1) * (expr + 1)) >> 7);
			g = std::max(1, (g * (mv + 1)) >> 7);
		}
		p.level = u8(g);
		p.gain = u8(smu2000::xg::effect_tables::fx_part_gain(g, part_inserted(part)));
	}

	// そのパートはインサーション 1-4 か、INSERTION で繋いだバリエーションに掛かっているか
	bool part_inserted(int part) const
	{
		for (const auto &insertion : m_state.insertion)
			if (insertion[::xg::settings::XG_INSERTION_PART] == part)
				return true;
		return m_state.effect[::xg::settings::XG_VARIATION_CONNECTION] == 0 && m_state.effect[::xg::settings::XG_VARIATION_PART] == part;
	}

	void control(int part, int cc, int v)
	{
		::xg::part_state &p = pb(part);
		switch (cc) {
		case 0:
			if (p.receive_bank && m_receive_bank_select)
				m_sel[part].msb = u8(v);
			break;
		case 32:
			if (p.receive_bank && m_receive_bank_select && ::xg::voice_rom(m_prog).lsb_ok(m_state.voice_mode, m_state.voice_set, m_sel[part].msb, v))
				m_sel[part].lsb = u8(v);
			break;
		case 1: p.modulation = u8(v); break;
		case 7: p[::xg::settings::PART_VOLUME] = u8(v); part_gain(part); break;
		case 11: p.expression = u8(v); part_gain(part); break;
		// パン 0 は Rnd だが、CC10 では Rnd にならない。CC10=0 は左いっぱい（1）として覚える
		case 10: p[::xg::settings::PART_PAN] = u8(std::max(v, 1)); break;
		case 71: p[::xg::settings::PART_RESONANCE] = u8(v); break;
		case 72: p[::xg::settings::PART_RELEASE] = u8(v); break;
		case 73: p[::xg::settings::PART_ATTACK] = u8(v); break;
		case 74: p[::xg::settings::PART_CUTOFF] = u8(v); break;
		case 75: p[::xg::settings::PART_DECAY] = u8(v); break;
		case 76: p[::xg::settings::PART_VIBRATO_RATE] = u8(v); m_drv.set_part_param(part, ::xg::settings::PART_VIBRATO_RATE, u8(v)); break;
		case 77: p[::xg::settings::PART_VIBRATO_DEPTH] = u8(v); break;
		case 78: p[::xg::settings::PART_VIBRATO_DELAY] = u8(v); break;
		case 91: p[::xg::settings::PART_REVERB] = u8(v); break;
		case 93: p[::xg::settings::PART_CHORUS] = u8(v); break;
		case 94: p[::xg::settings::PART_VARIATION] = u8(v); break;
		case 98: m_rpn[part].lsb = u8(v); m_rpn[part].nrpn = true; break;
		case 99: m_rpn[part].msb = u8(v); m_rpn[part].nrpn = true; break;
		case 100: m_rpn[part].lsb = u8(v); m_rpn[part].nrpn = false; break;
		case 101: m_rpn[part].msb = u8(v); m_rpn[part].nrpn = false; break;
		case 6: data_entry(part, v, -1); break;
		case 38: data_entry(part, -1, v); break;
		case 121:   // コントローラーのリセット
			p.modulation = 0;
			p.expression = 127;
			p.bend_half = PART_DEFAULT.bend_half;
			part_gain(part);
			break;
		case 96: data_step(part, 1); break;
		case 97: data_step(part, -1); break;
		default: break;
		}
		m_drv.control(part, cc, v);
		if (cc == 7 || cc == 10 || cc == 11 || cc == 91 || cc == 93 || cc == 94 || cc == 121)
			fx_mixer_refresh();
	}

	// データインクリメント・デクリメント（CC96/97）。データの値は見ず 1 段動かす。NRPN には効かない
	void data_step(int part, int delta)
	{
		rpn_sel &s = m_rpn[part];
		if (s.nrpn || s.msb != 0)
			return;
		::xg::part_state &p = pb(part);
		if (s.lsb == 0)
			p[::xg::settings::PART_BEND_RANGE] = u8(BEND_RANGE_ZERO + std::clamp(int(p[::xg::settings::PART_BEND_RANGE]) - BEND_RANGE_ZERO + delta, 0, BEND_RANGE_MAX));
		else if (s.lsb == 1) {
			if (int(s.fine_data) + delta >= 0)
				data_entry(part, std::min(int(s.fine_data) + delta, 127), -1);
		} else if (s.lsb == 2)
			p.coarse = s8(std::clamp(int(p.coarse) + delta, -COARSE_MAX, COARSE_MAX));
	}

	// RPN / NRPN（データエントリ）。XG のパートの NRPN はパートの設定（08 pp ll）と同じ所へ入る
	void data_entry(int part, int msb, int lsb)
	{
		rpn_sel &s = m_rpn[part];
		::xg::part_state &p = pb(part);
		if (!s.nrpn) {
			if (s.msb != 0)
				return;
			if (s.lsb == 0 && msb >= 0)
				p[::xg::settings::PART_BEND_RANGE] = u8(BEND_RANGE_ZERO + std::min(msb, BEND_RANGE_MAX));   // ベンドの幅（08 pp 23。RPN は半音の数そのまま）
			else if (s.lsb == 1) {
				// 微調: 16bit 符号つき、8192 で 100 セント
				int v = p.fine;
				if (msb >= 0)
					s.fine_data = u8(msb);
				int m = msb >= 0 ? msb : ((v + 8192) >> 7) & 0x7f, l = lsb >= 0 ? lsb : 0;
				v = ((m << 7) | l) - 8192;
				p.fine = s16(v);
			} else if (s.lsb == 2 && msb >= 0)
				p.coarse = s8(std::clamp(msb - 64, -COARSE_MAX, COARSE_MAX));   // 粗調（符号つきの半音）
			return;
		}
		if (msb < 0)
			return;
		// ドラムの NRPN（14-1F。LSB が鍵）。その組のセットアップを書く（17 は減衰 1 と 2 の両方）
		if (s.msb >= 0x14 && s.msb <= 0x1f) {
			static const s8 ADDR[12] = { 0x0b, 0x0c, 0x0d, 0x0e, 0x00, 0x01, 0x02, -1, 0x04, 0x05, 0x06, 0x07 };
			const int a = ADDR[s.msb - 0x14], set = drum_set(part), note = s.lsb;
			if (a < 0 || set < 0 || note < ::xg::settings::DRUM_SETUP_NOTE0
			    || note >= ::xg::settings::DRUM_SETUP_NOTE0 + int(::xg::settings::DRUM_SETUP_NOTES))
				return;
			m_state.drum_setup(set, note, a) = u8(msb);
			if (a == ::xg::settings::drum_param::first_decay)
				m_state.drum_setup(set, note, ::xg::settings::drum_param::second_decay) = u8(msb);
			return;
		}
		if (s.msb != 0x01)
			return;
		int addr = -1;
		switch (s.lsb) {
		case 0x08: addr = 0x15; break;   // ビブラートの速さ
		case 0x09: addr = 0x16; break;   // 深さ
		case 0x0a: addr = 0x17; break;   // 遅れ
		case 0x20: addr = 0x18; break;   // フィルタの切る高さ
		case 0x21: addr = 0x19; break;   // 共振
		case 0x63: addr = 0x1a; break;   // アタック
		case 0x64: addr = 0x1b; break;   // ディケイ
		case 0x66: addr = 0x1c; break;   // リリース
		default: break;
		}
		if (addr >= 0) {
			p[addr] = u8(msb);
			m_drv.set_part_param(part, u8(addr), u8(msb));
		}
	}

	static constexpr u8 GM2_REVERB_TYPE_MESSAGE[] = { 0x43, 0x10, 0x4c, 0x02, 0x01, 0x00, 0x01, 0x07 };

	// ---- 機種 4C・49・59 のメッセージの入口（doc/panel-flow.md の「SysEx の受信」）
	static constexpr u8 MODEL_XG = 0x4c, MODEL_MU = 0x49, MODEL_PERFORMANCE = 0x59;
	static constexpr int MU_SETTING_LAST = 0x15;

	bool checksum_ok(const sysex_bytes &x)
	{
		u32 sum = 0;
		for (size_t i = 3; i < x.size(); i++)
			sum += x[i];
		if (sum & 0x7f) {
			// 正しい和を画面に出す
			const u8 expected = u8((x[x.size() - 1] - sum) & 0x7f);
			push_notice({ notice_checksum, expected, 0, 0, 0 });
			return false;
		}
		return true;
	}

	static int xg_param_size(u8 hh, u8 mm, u8 ll)
	{
		if (hh == 0x00)
			return ll == 0x00 ? 4 : ll < 0x04 ? -1 : (ll == 0x04 || ll == 0x05) ? 1 : 0;
		if (hh == 0x02 && mm == 0x01) {
			if (ll == 0x00 || ll == 0x20 || ll == 0x40)
				return 2;
			if (ll == 0x01 || ll == 0x21 || ll == 0x41)
				return -1;
			if (ll >= 0x42 && ll <= 0x55)
				return (ll & 1) == 0 ? 2 : -1;
			return 1;
		}
		if (hh == 0x03) {
			if (ll >= 0x30 && ll <= 0x43)
				return 0;
			return ll == 0x00 ? 2 : ll == 0x01 ? -1 : 1;
		}
		if (hh == 0x08 && mm < 64)
			return ll == 0x09 ? 2 : ll == 0x0a ? -1 : 1;
		if (hh == 0x02 || hh == 0x0a || hh == 0x10 || (hh >= 0x30 && hh < 0x34))
			return 1;
		return 0;
	}

	bool yamaha_message(const sysex_bytes &x)
	{
		if (x.size() < 3 || x[0] != 0x43)
			return false;
		const u8 model = x[2];
		if (model != MODEL_XG && model != MODEL_MU && model != MODEL_PERFORMANCE)
			return false;
		if (m_receive_device < 16 && (x[1] & 0x0f) != m_receive_device)
			return true;
		const int kind = x[1] >> 4;
		if (kind == 2 || kind == 3) {        // 送ってほしい: 2 は塊、3 は 1 つの値
			if (x.size() == 6 && !(model == MODEL_XG && m_perf_on) && !(model == MODEL_PERFORMANCE && kind == 3 && !m_perf_on))
				push_notice({ kind == 2 ? notice_request_bulk : notice_request_value, model, x[3], x[4], x[5] });
			return true;
		}
		if (model == MODEL_XG) {
			if (m_perf_on)
				return true;
			if (kind == 0 && x.size() >= 9) {
				if (checksum_ok(x))
					push_notice({ notice_bulk, model, 0, 0, 0 });
				bulk_unchecked(x);
				return true;
			}
			if (kind == 1 && x.size() >= 7) {
				const int size = xg_param_size(x[3], x[4], x[5]);
				if (size < 0 || (size > 0 && int(x.size()) - 6 != size))
					push_notice({ notice_data_error, 0, 0, 0, 0 });
				if (size > 0 && int(x.size()) - 6 > size) {
					sysex(sysex_bytes{ x.data, size_t(6 + size) });
					return true;
				}
			}
			return false;
		}
		if (kind == 0) {
			if (x.size() < 9 || x.size() != (size_t(x[3]) << 7 | x[4]) + 9)
				return true;
			const bool sum_ok = checksum_ok(x);
			const u8 *data = x.data + 8;
			const int count = int(x.size()) - 9;
			if (model == MODEL_MU)
				mu_bulk(x[7], data, count);
			else
				performance_bulk(x[5], x[6], x[7], data, count);
			if (sum_ok)
				push_notice({ notice_bulk, model, 0, 0, 0 });
			return true;
		}
		if (kind == 1 && x.size() >= 7) {
			if (model == MODEL_MU) {
				if (x[3] != 0 || x[4] != 0 || x[5] > MU_SETTING_LAST)
					return true;
				// 数が違うと知らせを出すが、先頭の値は受けてしまう
				if (x.size() != 7)
					push_notice({ notice_data_error, 0, 0, 0, 0 });
				push_notice({ notice_setting, x[5], x[6], 0, 0 });
			} else if (m_perf_on)
				performance_param(x[3], x[4], x[5], x.data + 6, int(x.size()) - 6);
		}
		return true;
	}

	void bulk_unchecked(const sysex_bytes &x)
	{
		const size_t count = size_t(x[3]) << 7 | x[4];
		if (x.size() != count + 9)           // 長さはデータだけを数える
			return;
		u8 change[sizeof(parser::sx)];
		change[0] = 0x43;
		change[1] = 0x10;
		change[2] = 0x4c;
		for (size_t i = 5; i + 1 < x.size(); i++)
			change[i - 2] = x[i];
		sysex(sysex_bytes{ change, x.size() - 3 });
	}

	void mu_bulk(u8 low, const u8 *data, int count)
	{
		if (!((low == 0x00 && count == 10) || (low == 0x10 && count == 1) || (low == 0x12 && count == 1) || (low == 0x14 && count == 2)))
			return;
		for (int i = 0; i < count; i++)
			push_notice({ notice_setting, u8(low + i), data[i], 0, 0 });
	}

	// 機種 59 の塊: Int の記録へ書く。いま読み込んでいる記録は変わらない
	void performance_bulk(u8 high, u8 number, u8 low, const u8 *data, int count)
	{
		if (number >= PERFORMANCES || m_perf_internal.empty())
			return;
		u8 *record = m_perf_internal.data() + size_t(number) * PERFORMANCE_SIZE;
		int at = -1;
		if (high >= 0x30 && high < 0x34) {
			if (low == 0x00 && count == 34) at = (high - 0x30) * PERFORMANCE_PART_SIZE;
			else if (low == 0x30 && count == 2) at = (high - 0x30) * PERFORMANCE_PART_SIZE + 34;
		} else if (high == 0x40) {
			if (low == 0x00 && count == 24) at = 0x90;
			else if (low == 0x20 && count == 40) at = 0xa8;
			else if (low == 0x70 && count == 6) at = 0xd0;
		} else if ((high == 0x50 || high == 0x51) && low == 0x00 && count == 15)
			at = high == 0x50 ? 0xd6 : 0xe5;
		if (at >= 0)
			std::copy_n(data, count, record + at);
	}

	// 機種 59 のパラメータチェンジ: いま読み込んでいる記録を替える
	void performance_param(u8 high, u8 middle, u8 low, const u8 *data, int count)
	{
		const auto size_error = [&] { push_notice({ notice_data_error, 0, 0, 0, 0 }); };
		const auto address_error = [&] { push_notice({ notice_address_error, 0, 0, 0, 0 }); };
		if (high == 0x09 && middle < PERFORMANCE_PARTS) {
			const int part = middle;
			u8 *q = m_perf + part * PERFORMANCE_PART_SIZE;
			if (low == 0x09 || low > 0x25)
				return;
			if (low == 0x25 || low == 0x13) {
				address_error();
				return;
			}
			const int wanted = low == 0x24 || low == 0x12 ? 2 : 1;
			if (count != wanted) {
				size_error();
				if (wanted != 1)
					return;
			}
			if (low == 0x24)
				return;
			m_perf_edited = true;
			if (low == 0x12) {
				q[PERF_PART_FLAGS] = u8((q[PERF_PART_FLAGS] & 0xf0) | (data[0] & 0x0f));
				q[PERF_PART_DETUNE_LOW] = u8(data[1] & 0x0f);
				const u8 detune[2] = { u8(q[PERF_PART_FLAGS] & 0x0f), q[PERF_PART_DETUNE_LOW] };
				panel_bytes(::xg::settings::XG_PART, u8(part), ::xg::settings::PART_DETUNE, detune, 2);
				return;
			}
			if (low == 0x18) {
				q[PERF_PART_FLAGS] = u8((q[PERF_PART_FLAGS] & ~PERF_PART_POLY) | (data[0] ? PERF_PART_POLY : 0));
				panel_write(::xg::settings::XG_PART, u8(part), ::xg::settings::PART_POLY, data[0] ? 1 : 0);
				return;
			}
			const int offset = low <= 0x08 ? low : low <= 0x16 ? low - 1 : low - 2;
			q[offset] = data[0];
			if (offset == PERF_PART_PROGRAM)
				panel_apply_voice(part, q[PERF_PART_BANK_MSB], q[PERF_PART_BANK_LSB], q[PERF_PART_PROGRAM]);
			else if (offset == PERF_PART_VOLUME)
				set_performance_volume(m_perf[PERF_VOLUME]);
			else if (offset == PERF_PART_PAN)
				set_performance_pan(m_perf[PERF_PAN]);
			else if (offset == PERF_PART_HIGHPASS)
				panel_write(::xg::settings::XG_HIGHPASS, u8(part), ::xg::settings::XG_HIGHPASS_VALUE, data[0]);
			else if (const int xg = performance_part_low(offset); xg >= 0)
				panel_write(::xg::settings::XG_PART, u8(part), u8(xg), data[0]);
			return;
		}
		if (high != 0x0b || middle != 0x00)
			return;
		const int offset = low <= 0x17 ? PERF_NAME + low : (low >= 0x20 ? 0x88 + low : -1);
		if (offset < 0 || offset >= PERFORMANCE_SIZE)
			return;
		if (offset >= PERF_NAME && offset < PERF_NAME + PERF_NAME_SIZE) {
			// 名前はまとめて送れる
			for (int i = 0; i < count && offset + i < PERF_NAME + PERF_NAME_SIZE; i++)
				m_perf[offset + i] = data[i];
			m_perf_edited = true;
			return;
		}
		const int wanted = performance_common_size(offset);
		if (wanted == 0) {
			address_error();
			return;
		}
		if (count != wanted) {
			size_error();
			if (wanted != 1)
				return;
			count = 1;
		}
		for (int i = 0; i < count; i++)
			m_perf[offset + i] = data[i];
		m_perf_edited = true;
		if (offset == PERF_VOLUME)
			set_performance_volume(data[0]);
		else if (offset == PERF_PAN)
			set_performance_pan(data[0]);
		else
			apply_performance();
	}

	// 共通の項目の大きさ。2 バイトの項目の後ろ半分は 0
	static int performance_common_size(int offset)
	{
		const auto wide_at = [&](int first, int last) {
			return offset >= first && offset <= last ? ((offset - first) % 2 == 0 ? 2 : 0) : -1;
		};
		for (const auto &range : { std::array<int, 2>{ 0xa8, 0xa9 }, std::array<int, 2>{ 0xb1, 0xb2 }, std::array<int, 2>{ 0xbb, 0xc8 }, std::array<int, 2>{ 0xd6, 0xe3 }, std::array<int, 2>{ 0xe5, 0xf2 } }) {
			const int size = wide_at(range[0], range[1]);
			if (size >= 0)
				return size;
		}
		return 1;
	}

	void sysex(const sysex_bytes &x)
	{
		// UTIL の受信の設定。パネルが送るものは止めない
		if (!m_sysex_from_panel) {
			if (!m_receive_exclusive)
				return;
			// 入口を見るのは外から届いたメッセージだけ（中で組み直したものは見ない）
			if (!m_sysex_inner) {
				m_sysex_inner = true;
				if (yamaha_message(x))
					return;
			}
			if (m_perf_on)
				return;
			const bool xg_message = x.size() >= 3 && x[0] == 0x43 && x[2] == 0x4c;
			if (xg_message && m_receive_device < 16 && (x[1] & 0x0f) != m_receive_device)
				return;
			const bool reset = (x.size() == 4 && x[0] == 0x7e && x[3] >= 1 && x[3] <= 3)
			                || (xg_message && x.size() >= 6 && x[3] == 0 && x[4] == 0 && x[5] == 0x7e);
			if (reset && !m_receive_system_on)
				return;
		}
		// GM システムオン・オフ・GM2 オン 7E 7F 09 01-03（どれもリセット）
		if (x.size() == 4 && x[0] == 0x7e && x[2] == 0x09 && x[3] >= 0x01 && x[3] <= 0x03) {
			xg_reset();
			// GM オンのあとは NRPN とバンク選択を受けない
			if (x[3] == 0x01)
				for (int part = 0; part < 64; part++) {
					::xg::part_state &p = pb(part);
					p[RECEIVE_NRPN] = 0;
					p.receive_bank = 0;
				}
			// GM2 オンの既定のリバーブは 01 07
			if (x[3] == 0x03)
				sysex(sysex_bytes{ GM2_REVERB_TYPE_MESSAGE, sizeof(GM2_REVERB_TYPE_MESSAGE) });
			return;
		}
		// GS 41 dd 42 12 hh mm ll dd… 和
		if (x.size() >= 7 && x[0] == 0x41 && (x[1] & 0xf0) == 0x10 && x[2] == 0x42 && x[3] == 0x12) {
			gs_message(x);
			return;
		}
		// ユニバーサルのマスター音量 7F 7F 04 01 下位 上位
		if (x.size() == 6 && x[0] == 0x7f && x[2] == 0x04 && x[3] == 0x01) {
			m_state.system[::xg::settings::SYS_VOLUME] = x[5];
			master_volume_changed();
			return;
		}
		// XG のバルク 43 0n 4C 長さの上位 下位 hh mm ll dd… 和。パラメータチェンジを連ねたものと同じ
		if (x.size() >= 9 && x[0] == 0x43 && (x[1] & 0xf0) == 0x00 && x[2] == 0x4c) {
			bulk_unchecked(x);
			return;
		}
		// XG のパラメータチェンジ 43 1n 4C hh mm ll dd…
		if (x.size() < 7 || x[0] != 0x43 || (x[1] & 0xf0) != 0x10 || x[2] != 0x4c)
			return;
		const u8 hh = x[3], mm = x[4], ll = x[5];
		if (hh == 0x00 && mm == 0x00 && (ll == 0x7e || ll == 0x7f)) {
			xg_reset();
			return;
		}
		if ((hh == 0x02 && mm == 0x01 && ((ll >= 0x40 && ll <= 0x5b) || (ll >= 0x70 && ll <= 0x75))) || (hh == 0x03 && mm < 4)) {
			for (size_t i = 6; i < x.size(); i++) {
				const u32 a = u32(ll + i - 6);
				if (u8 *value = m_state.find(u32(hh) << 14 | u32(mm) << 7 | a))
					*value = x[i];        // 繋ぎ方・掛かり先のパートは native_driver が設定から読む
			}
			if (hh == 0x02 || mm == 0x00)
				fx_block_sysex(hh == 0x02 ? m_var : m_ins1, hh == 0x02 ? smu2000::xg::effect_tables::FX_VAR : smu2000::xg::effect_tables::FX_INS,
				               u8(hh == 0x02 ? ll - 0x40 : ll), x);
			else {
				// インサーション 2-4（03 01-03）は第 2 群。種類を替えたら slave を付け直す
				extra_insertion &si = m_extra_insertions[mm - 1];
				fx_block_sysex(si.b, smu2000::xg::effect_tables::FX_INS, ll, x);
			}
			for (int pt = 0; pt < 64; pt++)
				part_gain(pt);           // 掛かり先が変われば音量の目盛りも変わる
			fx_update(LFO_DELAY_PARAM);
			return;
		}
		if (hh == 0x02 && (mm == 0x01 || mm == 0x40)) {
			const smu2000::xg::effect_tables::Input before = m_fxin;
			for (size_t i = 6; i < x.size(); i++)
				fx_param(mm, u8(ll + i - 6), x[i]);
			const bool type = before.rev_msb != m_fxin.rev_msb || before.rev_lsb != m_fxin.rev_lsb
			               || before.cho_msb != m_fxin.cho_msb || before.cho_lsb != m_fxin.cho_lsb;
			if (!type)
				for (size_t i = 6; i < x.size(); i++) {
					const u8 a = u8(ll + i - 6);
					if (mm == 0x01 && a <= 0x01)
						return;            // 種類の書き直し（パラメータも既定に戻さない）
					// **電源を入れてから初めてのコーラスの種類は、同じ種類でも読み込み直す**。
					// 既定には戻さないが、LFO は種類を替えたときと同じに書き直す。リセットや種類を読んだあとの書き直しは何もしない
					if (mm == 0x01 && (a == 0x20 || a == 0x21)) {
						if (!m_chorus_type_loaded) {
							m_chorus_type_loaded = true;
							fx_update(LFO_DELAY_TYPE);
						}
						return;
					}
				}
			const bool chorus_type = before.cho_msb != m_fxin.cho_msb || before.cho_lsb != m_fxin.cho_lsb;
			if (chorus_type)
				m_chorus_type_loaded = true;
			fx_update(chorus_type ? LFO_DELAY_TYPE : LFO_DELAY_PARAM);
			return;
		}
		// A/D パート（10 00 xx が AD1、10 01 xx が AD2）
		if (hh == 0x10 && mm < 2) {
			for (size_t i = 6; i < x.size(); i++)
				ad_param(mm, u8(ll + i - 6), x[i] & 0x7f);
			return;
		}
		// ドラムのセットアップのリセット（00 00 7D nn）: その組をキットの既定に戻す
		if (hh == 0x00 && mm == 0x00 && ll == 0x7d) {
			const int set = x[6];
			if (set < int(::xg::settings::DRUM_SETUP_SETS)) {
				fill_drum_setup(set, set_kit(set));
			}
			return;
		}
		// ドラムのセットアップ（3n rr pp dd）。n が組、rr が鍵
		if ((hh & 0xf0) == 0x30) {
			const int set = hh & 0x0f, note = mm;
			for (size_t i = 6; i < x.size(); i++) {
				const int idx = ::xg::settings::drum_setup_index(int(ll + i - 6));
				if (set >= int(::xg::settings::DRUM_SETUP_SETS) || idx < 0)
					continue;
				if (note >= ::xg::settings::DRUM_SETUP_NOTE0 && note < ::xg::settings::DRUM_SETUP_NOTE0 + int(::xg::settings::DRUM_SETUP_NOTES))
					m_state.drum_setup(set, note, idx) = x[i];
			}
			return;
		}
		for (size_t i = 6; i < x.size(); i++) {
			const u32 addr = u32(hh) << 14 | u32(mm) << 7 | u32(ll + i - 6);
			u8 *value = m_state.find(addr);
			if (!value)
				continue;
			const u8 before = *value;
			*value = x[i];
			if (hh == 0x08 && mm < 64) {
				const u8 a = u8(ll + i - 6);
				if (a == XG_NOTE_SHIFT)
					*value = std::clamp(x[i], NOTE_SHIFT_MIN, NOTE_SHIFT_MAX);
				// Detune は 2 バイトで 1 つの値。1 つのメッセージで 2 つ揃わないと効かない
				if ((a == XG_DETUNE_HIGH && i + 1 >= x.size()) || (a == XG_DETUNE_LOW && i == 6))
					*value = before;
				m_drv.set_part_param(mm, a, x[i]);
				// ポルタメントのスイッチと時間（08 pp 67・68）は CC65・CC5 と同じ所へ
				if (a == ::xg::settings::PART_PORTAMENTO_SWITCH)
					m_drv.control(mm, CC_PORTAMENTO, x[i] ? 127 : 0);
				else if (a == ::xg::settings::PART_PORTAMENTO_TIME)
					m_drv.control(mm, CC_PORTAMENTO_TIME, x[i]);
				if (a == 0x0b)
					part_gain(mm);
				// 音量・パン・送りは鳴っている音にも効く
				if (a == 0x0b || a == 0x0e || (a >= 0x12 && a <= 0x14))
					m_drv.refresh_part(mm);
				// Mono/Poly（同じ値でも。CC126/127 と同じ）、Part Mode の変更、Rcv Note を切ると、鳴っている音を急停止する
				if (a == XG_MONO_POLY)
					m_drv.control(mm, x[i] ? CC_POLY : CC_MONO, 0);
				if ((a == XG_PART_MODE && x[i] != before) || (a == RECEIVE_NOTE && !x[i]))
					m_drv.control(mm, CC_ALL_SOUND_OFF, 0);
				if (a == 0x0b || a == 0x0e || (a >= 0x11 && a <= 0x14))
					fx_mixer_refresh();
				if (a == 0x07 && x[i] == 0) {
					m_sel[mm].msb = 0;
					pb(mm)[::xg::settings::PART_BANK_MSB] = 0;
				}
				if (a == 0x01) m_sel[mm].msb = x[i];
				if (a == 0x02) m_sel[mm].lsb = x[i];
				if (a == 0x03) m_sel[mm].prog = x[i];
				// Bank は CC0/CC32 と同じく、次のプログラムチェンジで効く
				if (a == 0x03 || a == 0x07)
					select_voice(mm);
			}
			if (hh == 0x00 && mm == 0x00 && ll + i - 6 == 4)
				master_volume_changed();
			if (hh == 0x00 && mm == 0x00 && ll + i - 6 == 5)
				output_gain_refresh();
		}
	}

	void master_volume_changed()
	{
		for (int part = 0; part < 64; part++) {
			part_gain(part);
			m_drv.refresh_part(part);
		}
	}

	// 出口の利得（マスターアッテネータ 00 00 05 は ((127 - 値) / 127)^2 で絞る）
	void output_gain_refresh()
	{
		const float open = float(127 - std::min<int>(m_state.system[::xg::settings::SYS_ATTENUATOR], 127)) / 127.0f;
		m_output_gain[0] = m_master_output_gain[0] * open * open;
		m_output_gain[1] = m_master_output_gain[1] * open * open;
	}

	// リセットを受けた。格子が 2.5ms 戻り、エフェクトの LFO を書く時刻は前のリセットの種類と回数で変わる
	void note_reset(bool gs)
	{
		m_eg_phase_quarters = (m_eg_phase_quarters + EG_TICK_QUARTERS - EG_GRID_STEP_QUARTERS) % EG_TICK_QUARTERS;
		set_eg_phase_now();
		m_repeat_reset_delay = repeat_reset_delay(m_resets_received, m_last_reset_gs, gs);
		// XG のリセットのあとのリセットは、コーラスの LFO を書き直さない（値が変わらなければ動いたまま）
		m_reset_keeps_chorus_lfo = m_resets_received > FIRST_EXTERNAL_RESET && !m_last_reset_gs;
		m_lfo_queued_reset_pending = true;
		m_resets_received++;
		m_last_reset_gs = gs;
	}
	static u32 repeat_reset_delay(u32 resets_before, bool last_gs, bool gs)
	{
		// ponytail: 8 回目以降は 7 回目の値のまま
		static constexpr u32 AFTER_GS[] = { 9289, 9179, 9069, 9310, 9220, 9109 };
		static constexpr u32 AFTER_XG[] = { 9520, 9410, 9301, 9191, 9101, 8990 };
		static constexpr u32 FIRST_GS = 8915, FIRST_XG = 8526;
		if (resets_before < FIRST_EXTERNAL_RESET + 1)
			return 0;
		const u32 i = std::min(resets_before - FIRST_EXTERNAL_RESET - 1, u32(std::size(AFTER_GS) - 1));
		return (last_gs ? AFTER_GS : AFTER_XG)[i] - (gs ? FIRST_GS : FIRST_XG);
	}
	void set_eg_phase_now()
	{
		m_drv.force_eg_phase(m_eg_phase_quarters);
	}
	// 格子を 2.5ms ずつ後ろへ動かす（doc/voice-flow/voice.md の 10 ms の格子）
	void move_eg_grid(int ticks)
	{
		const u32 before = m_drv.eg_phase();
		m_eg_phase_quarters = (m_eg_phase_quarters + u32(ticks) * EG_GRID_STEP_QUARTERS) % EG_TICK_QUARTERS;
		set_eg_phase_now();
		m_drv.delay_voice_steps((m_drv.eg_phase() + EG_TICK_QUARTERS / 4 - before) % (EG_TICK_QUARTERS / 4));
	}


	void xg_part_change(int part, int address, u8 value)
	{
		const u8 change[] = { 0x43, 0x10, 0x4c, 0x08, u8(part), u8(address), value };
		sysex(sysex_bytes{ change, sizeof(change) });
	}

	void gs_message(const sysex_bytes &x)
	{
		// 実機は和を見ない
		const u8 mm = x[5], ll = x[6];
		const size_t first = 7, count = x.size() - first - 1;
		if (!count)
			return;
		// SC-88 の Mode Set（00 00 7F）も GS リセットと同じ
		if (x[4] == 0x00 && mm == 0x00 && ll == 0x7f) {
			gs_reset();
			return;
		}
		if (x[4] != 0x40)
			return;
		// リセット（40 00 7F）は XG のままでも受ける。ほかは TG300B モードの間だけ
		if (mm == 0x00 && ll == 0x7f) {
			gs_reset();
			return;
		}
		if (m_state.voice_mode != 0)
			return;
		if (mm == 0x00) {
			if (ll == 0x00 && count == 4) {
				const u8 change[] = { 0x43, 0x10, 0x4c, 0x00, 0x00, 0x00, x[7], x[8], x[9], x[10] };
				sysex(sysex_bytes{ change, sizeof(change) });
			} else if (ll == 0x04) {
				m_state.system[::xg::settings::SYS_VOLUME] = x[first];
				master_volume_changed();
			} else if (ll == 0x05)
				m_state.system[::xg::settings::SYS_TRANSPOSE] = std::clamp<u8>(x[first], ::xg::gs::NOTE_RANGE_LOW, ::xg::gs::NOTE_RANGE_HIGH);
			return;
		}
		if (mm == 0x01) {
			bool chorus_macro = false;
			for (size_t i = 0; i < count; i++) {
				gs_effect(ll + int(i), x[first + i]);
				chorus_macro |= ll + i == GS_CHORUS_MACRO;
			}
			fx_update(chorus_macro ? LFO_DELAY_GS_CHORUS_MACRO : LFO_DELAY_GS_PARAM);
			return;
		}
		const int part = ::xg::gs::part_of(mm & 0x0f);
		if ((mm & 0xf0) == 0x10) {
			// Tone Number は 2 バイト揃って効く（バンク、プログラム）
			if (ll == 0x00) {
				if (count >= 2)
					gs_select_tone(part, x[first], x[first + 1]);
				return;
			}
			// Pitch Offset Fine も 2 バイトで XG の Detune と同じ形
			if (ll == 0x17) {
				if (count >= 2) {
					const u8 change[] = { 0x43, 0x10, 0x4c, 0x08, u8(part), XG_DETUNE_HIGH, x[first], x[first + 1] };
					sysex(sysex_bytes{ change, sizeof(change) });
				}
				return;
			}
			for (size_t i = 0; i < count; i++)
				gs_part_param(part, ll + int(i), x[first + i]);
		} else if ((mm & 0xf0) == 0x20) {
			for (size_t i = 0; i < count; i++) {
				const int cc = ll + int(i), address = ::xg::gs::controller_address(cc);
				if (address == ::xg::gs::XG_NONE)
					continue;
				u8 v = x[first + i];
				if ((cc & 0x0f) == 0)
					v = std::clamp<u8>(v, (cc >> 4) == 1 ? ::xg::gs::CENTER : ::xg::gs::NOTE_RANGE_LOW, ::xg::gs::NOTE_RANGE_HIGH);
				xg_part_change(part, address, v);
			}
		}
	}

	void gs_part_param(int part, int cc, u8 v)
	{
		if (cc == 0x15) {
			// Use for Rhythm Part: 0 で旋律、2 で Map2（DRUMS2）、ほかは Map1（DRUMS1）
			xg_part_change(part, XG_PART_MODE, v == 0 ? 0 : v == 2 ? 3 : 2);
			return;
		}
		const int address = ::xg::gs::part_address(cc);
		if (address == ::xg::gs::XG_NONE)
			return;
		if (address == ::xg::gs::XG_RX_BANK) {
			pb(part).receive_bank = v ? 1 : 0;
			return;
		}
		if (address >= int(RECEIVE_BEND) && address <= int(RECEIVE_SOFT))
			v = v ? 1 : 0;
		else if (address == XG_PART_ASSIGN)
			v = v ? 1 : 0;
		else if (address == 0x04 && v > 15)
			v = 0x7f;                        // OFF
		xg_part_change(part, address, v);
	}

	void gs_select_tone(int part, u8 bank, u8 program)
	{
		m_sel[part].msb = bank;
		m_sel[part].prog = program;
		::xg::part_state &p = pb(part);
		p[::xg::settings::PART_BANK_MSB] = bank;
		p[::xg::settings::PART_PROGRAM] = program;
		select_voice(part);
	}

	void gs_effect(int address, u8 v)
	{
		using namespace ::xg::gs;
		const auto write_character = [this] {
			const reverb_character c = reverb_character_of(m_prog->data(), m_gs_effects.character);
			const u8 values[][2] = { { 0x02, reverb_time(m_prog->data(), m_gs_effects.character, m_gs_effects.time) }, { 0x03, c.diffusion }, { 0x04, c.initial_delay }, { 0x05, c.highpass_cutoff },
				{ 0x06, reverb_pre_lpf(m_prog->data(), m_gs_effects.pre_lpf) }, { 0x10, c.reverb_delay }, { 0x11, c.density }, { 0x12, c.early_balance }, { 0x13, c.high_damp }, { 0x14, c.feedback_level } };
			for (const auto &value : values)
				fx_param(0x01, value[0], value[1]);
		};
		switch (address) {
		case 0x30: {   // Reverb Macro
			const int macro = std::min<int>(v, 7);
			const reverb_macro m = reverb_macro_of(m_prog->data(), macro);
			m_gs_effects.character = u8(macro);
			m_gs_effects.pre_lpf = m.pre_lpf;
			m_gs_effects.time = m.time;
			fx_param(0x01, 0x0c, GS_LEVEL_DEFAULT);
			fx_param(0x01, 0x00, m.msb);
			fx_param(0x01, 0x01, m.lsb);
			// 種類を替えても、マクロが決めないパラメータは種類の既定に戻らない
			for (const u8 (&value)[2] : GS_RESET_REVERB)
				fx_param(0x01, value[0], value[1]);
			write_character();
			break;
		}
		case 0x31:     // Reverb Character
			m_gs_effects.character = u8(std::min<int>(v, 7));
			write_character();
			break;
		case 0x32:     // Reverb Pre-LPF
			m_gs_effects.pre_lpf = u8(std::min<int>(v, 7));
			fx_param(0x01, 0x06, reverb_pre_lpf(m_prog->data(), m_gs_effects.pre_lpf));
			break;
		case 0x33: fx_param(0x01, 0x0c, v); break;   // Reverb Level
		case 0x34:     // Reverb Time
			m_gs_effects.time = v;
			fx_param(0x01, 0x02, reverb_time(m_prog->data(), m_gs_effects.character, v));
			break;
		case GS_CHORUS_MACRO: {
			// 実機のマクロはコーラスのパラメータを既定へ戻し、マクロの Depth と Delay は次に送るときまで覚えているだけ
			const chorus_macro m = chorus_macro_of(m_prog->data(), std::min<int>(v, 7));
			m_gs_effects.chorus_depth = m.depth;
			m_gs_effects.chorus_delay = m.delay;
			for (const u8 (&value)[2] : GS_RESET_CHORUS)
				fx_param(0x01, value[0], value[1]);
			break;
		}
		case 0x3a: fx_param(0x01, 0x2c, v); break;   // Chorus Level
		case 0x3b: fx_param(0x01, 0x24, chorus_feedback(v)); break;
		case 0x3c:     // Chorus Delay
			m_gs_effects.chorus_delay = v;
			fx_param(0x01, 0x23, m_gs_effects.chorus_depth);
			fx_param(0x01, 0x25, v);
			break;
		case 0x3d: fx_param(0x01, 0x22, chorus_rate(v)); break;
		case 0x3e:     // Chorus Depth
			m_gs_effects.chorus_depth = chorus_depth(v);
			fx_param(0x01, 0x23, m_gs_effects.chorus_depth);
			fx_param(0x01, 0x25, m_gs_effects.chorus_delay);
			break;
		case 0x3f: fx_param(0x01, 0x2e, v); break;   // Chorus Send Level To Reverb
		default: break;
		}
	}

	// GS リセット。XG のリセットのあと、TG300B モードの既定へ
	void gs_reset()
	{
		xg_reset(true);
		m_state.voice_mode = 0;
		for (int part = 0; part < 64; part++) {
			::xg::part_state &p = pb(part);
			const int ch = part % 16;
			if (ch == 9) {
				p[::xg::settings::PART_ELEMENT_RESERVE] = 2;
				p[::xg::settings::PART_SAME_NOTE] = 0;
				p[::xg::settings::PART_MODE] = 2;      // どの口の 10 番も DRUMS1
				p.kit = ::xg::gs::DRUM_KIT_DEFAULT;
			} else
				p[::xg::settings::PART_ELEMENT_RESERVE] = ch == 0 ? 6 : 0;
		}
		for (int set = 0; set < int(::xg::settings::DRUM_SETUP_SETS); set++)
			fill_drum_setup(set, ::xg::gs::DRUM_KIT_DEFAULT);
		// HALL 2。パラメータは種類の既定に戻らず、HALL 1 の値のまま
		fx_param(0x01, 0x01, 0x01);
		for (const u8 (&value)[2] : GS_RESET_REVERB)
			fx_param(0x01, value[0], value[1]);
		fx_param(0x01, 0x2b, 0x00);
		fx_update(LFO_DELAY_GS_RESET);
		for (int part = 0; part < 64; part++)
			select_voice(part);
	}

	void select_voice(int part)
	{
		const prog_sel &s = m_sel[part];
		// TG300B モードではバンクでドラムにならない（パートのモードだけ）
		const bool gs_mode = m_state.voice_mode == 0;
		const bool drum = (!gs_mode && (s.msb == 127 || s.msb == 126)) || pb(part)[::xg::settings::PART_MODE] != 0;
		if (drum) {
			if (pb(part)[::xg::settings::PART_MODE] == 0) {
				bool used[4] = {};
				for (int q = 0; q < 64; q++)
					if (q != part && pb(q)[::xg::settings::PART_MODE] >= 2 && pb(q)[::xg::settings::PART_MODE] < 6)
						used[pb(q)[::xg::settings::PART_MODE] - 2] = true;
				int set = 0;
				while (set < 3 && used[set])
					set++;
				pb(part)[::xg::settings::PART_MODE] = u8(set + 2);
				m_bank_drum[part] = true;
			}
			// キットの番号と、その組のセットアップ（キットの 1 打ずつの既定値）
			const int kit = gs_mode ? ::xg::voice_rom::tg300b_kit_number(s.prog) : ::xg::voice_rom(m_prog).kit_number(s.msb == 126 ? 126 : 127, s.prog);
			// バンク 126 のキットが無いプログラムは Silent Kit。127 は Standard Kit に落ちる
			pb(part).kit = u8(kit >= 0 ? kit : s.msb == 126 ? ::xg::nv::SILENT_KIT : 36);
			fill_drum_setup(drum_set(part), pb(part).kit);
			return;
		}
		// バンクでドラムにしていたパートを旋律に戻したら、付けた組も外す
		if (m_bank_drum[part]) {
			pb(part)[::xg::settings::PART_MODE] = 0;
			m_bank_drum[part] = false;
		}
		const ::xg::voice_rom vr(m_prog);
		const u32 rec = vr.lookup(m_state.voice_mode, m_state.voice_set, s.msb, s.lsb, s.prog);
		pb(part).voice_record = rec;
	}

	// XG のリセット（電源を入れたときも）。値は XG システムオンのあとの姿
	void xg_reset(bool gs_follows = false)
	{
		m_chorus_type_loaded = true;
		note_reset(gs_follows);
		for (int part = 0; part < 64; part++) {
			::xg::part_state &p = pb(part);
			p = PART_DEFAULT;
			const int port = part / 16, ch = part % 16;
			p[::xg::settings::PART_CHANNEL] = u8(part);                                   // 受けるチャンネル
			if (ch == 9) {
				// 各口の 10 番目はドラム。種類は A が DRUMS1、B が DRUMS3（測った値）、C・D は DRUM
				static const u8 MODE[4] = { 2, 4, 1, 1 };
				p[::xg::settings::PART_ELEMENT_RESERVE] = 0;
				p[::xg::settings::PART_BANK_MSB] = 127;
				p[::xg::settings::PART_MODE] = MODE[port];
				p.kit = 36;   // Standard Kit
			}
			m_sel[part] = prog_sel{ p[::xg::settings::PART_BANK_MSB], 0, 0 };
			m_rpn[part] = rpn_sel();
			m_bank_drum[part] = false;
		}
		static const u8 SYS[7] = { 0x00, 0x04, 0x00, 0x00, 0x7f, 0x00, 0x40 };   // 調律 0x400、音量 127、移調 64
		std::copy_n(SYS, sizeof(SYS), m_state.system.begin());
		m_state.voice_mode = 1;
		m_state.voice_set = 1;
		m_gs_effects = gs_effects{};
		{
			const ::xg::gs::chorus_macro chorus = ::xg::gs::chorus_macro_of(m_prog->data(), ::xg::gs::DEFAULT_CHORUS_MACRO);
			m_gs_effects.chorus_depth = chorus.depth;
			m_gs_effects.chorus_delay = chorus.delay;
		}
		// バリエーションはインサーションの繋ぎで、掛かり先のパートは無し。インサーション 1-4 も無し
		m_state.effect[::xg::settings::XG_VARIATION_RETURN] = 0x40;
		m_state.effect[::xg::settings::XG_VARIATION_PAN] = 0x40;
		m_state.effect[::xg::settings::XG_VARIATION_CONNECTION] = 0;
		m_state.effect[::xg::settings::XG_VARIATION_PART] = 0x7f;
		for (auto &insertion : m_state.insertion) {
			insertion[::xg::settings::XG_INSERTION_PART] = ::xg::settings::XG_NO_PART;
			// コントローラーの効きは中央
			std::fill_n(&insertion[::xg::settings::XG_INSERTION_PART + 1], 5, u8(64));
		}
		for (int set = 0; set < int(::xg::settings::DRUM_SETUP_SETS); set++)
			fill_drum_setup(set, 36);
		// エフェクトも電源を入れたときの姿（HALL 1・CHORUS 1・EQ 平ら）
		const bool external = !m_sysex_from_panel;
		if (external && m_reset_seen)
			m_external_resets++;
		const smu2000::xg::effect_tables::Input before_reset = m_fxin;
		m_fxin = smu2000::xg::effect_tables::Input();
		// 外からのリセットでも、ロック中のものは残す（パネルの初期化は全部戻す）
		if (external && m_eq_locked)
			std::copy_n(before_reset.eq, 20, m_fxin.eq);
		else
			m_eq_type = 0;
		if (m_reset_seen)
			m_reverb_type_written = m_chorus_type_written = true;
		m_reset_seen = true;
		m_var = fx_block();
		m_var.msb = 0x05;                // XG の既定: DELAY L,C,R、繋ぎ方 INSERTION（パート無し）、パン・戻り 64
		m_var.M[5] = 64;
		m_var.M[6] = 64;
		m_ins1 = fx_block();
		m_ins1.msb = INSERTION_DEFAULT_MSB;
		fx_block_load(m_var, smu2000::xg::effect_tables::FX_VAR);
		fx_block_load(m_ins1, smu2000::xg::effect_tables::FX_INS);
		// 電源を入れたときの Output Level は、種類を選び直したときの既定値（48）と違う
		m_ins1.P[INSERTION_LEVEL_PARAM - 1] = INSERTION_DEFAULT_LEVEL;
		m_ins1.request.set_param(INSERTION_LEVEL_PARAM, INSERTION_DEFAULT_LEVEL);
		for (extra_insertion &si : m_extra_insertions) {
			si.b = fx_block();
			si.b.msb = INSERTION_DEFAULT_MSB;
			extra_insertion_load(si);
			si.b.P[INSERTION_LEVEL_PARAM - 1] = INSERTION_DEFAULT_LEVEL;
			si.b.request.set_param(INSERTION_LEVEL_PARAM, INSERTION_DEFAULT_LEVEL);
		}
		fx_update(LFO_DELAY_RESET);
		m_drv.reset_parts();
		for (int part = 0; part < 64; part++)
			select_voice(part);
		if (!(external && m_ad_locked))
			m_ad[0] = m_ad[1] = ad_part{};
		ad_refresh();
	}

	// パートが使うドラムのセットアップの組（08 pp 07 が 2-5 なら 0-3。1 の DRUM は組を持たない。native_driver::drum_set_of と同じ）
	int drum_set(int part) { const int m = pb(part)[::xg::settings::PART_MODE]; return m >= 2 ? m - 2 : -1; }

	// その組を使っているパートのキット（無ければ Standard）
	int set_kit(int set)
	{
		for (int part = 0; part < 64; part++)
			if (pb(part)[::xg::settings::PART_MODE] != 0 && drum_set(part) == set)
				return pb(part).kit;
		return 36;
	}

	// **ドラムのセットアップをキットの既定に戻す**。鍵 13-91 の 23 個ずつ（値は ::xg::nv::drum_setup_kit_default）
	void fill_drum_setup(int set, int kit)
	{
		if (set < 0 || set >= int(::xg::settings::DRUM_SETUP_SETS))
			return;
		for (int note = ::xg::settings::DRUM_SETUP_NOTE0; note < ::xg::settings::DRUM_SETUP_NOTE0 + int(::xg::settings::DRUM_SETUP_NOTES); note++) {
			const u8 *rec = ::xg::nv::drum_record(m_prog->data(), kit, note);
			for (int i = 0; i < int(::xg::settings::DRUM_SETUP_PARAM); i++)
				m_state.drum_setup(set, note, i) = u8(::xg::nv::drum_setup_kit_default(m_prog->data(), rec, i));
		}
	}

	// パートの既定値。チャンネルと音色の記録は xg_reset() が入れる
	static constexpr ::xg::part_state PART_DEFAULT = {
		.values = {
			0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x40, 0x08, 0x00, 0x64, 0x40, 0x40, 0x40, 0x00,
			0x7f, 0x7f, 0x00, 0x28, 0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
			0x0a, 0x00, 0x00, 0x42, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
			0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
			0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x10, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00,
			0x11, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x01, 0x7f, 0x00,
			0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x0c, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		.highpass = 0x40,
		.expression = 0x7f,
		.bend_half = 0x20,
		.receive_bank = 0x01,
		.level = 0x65,
		.gain = 0x65,
	};

	std::shared_ptr<const std::vector<u8>> m_prog;
	::xg::synth_state m_state{};
	std::vector<u8> m_wave, m_sampram;
	std::vector<u16> m_sin;
	native_voice::voice_bank m_voices;
	native_voice::voice_word_reader m_word_reader { m_voices };
	native_voice::block_mixer m_mixer;
	native_voice::bus_block m_first, m_second;     // 第 1 群・第 2 群の声の母線
	float m_exit_held[2] = {};                     // 出口の 1 サンプルの遅れ
	static constexpr u8 STATE_MAGIC[8] = { 'S', 'M', 'U', 'N', 'A', 'T', '0', '2' };
	static constexpr float AD_INPUT_GAIN = 0.25f;  // 入力を母線へ入れる固定の減衰
	ad_part m_ad[2];
	float m_ad_gain[2][native_voice::bus_ins1][2] = {};
	static constexpr float AD_HIGHPASS_CURRENT = 0.998596f, AD_HIGHPASS_FEEDBACK = 0.997132f;
	float m_ad_held[2][2] = {};
	float m_ad_filter[2][2] = {};
	::xg::native_driver m_drv;
	static constexpr u32 QUEUE_SIZE = 4096, SYSEX_BYTES = 1 << 17;
	std::vector<event> m_q_items = std::vector<event>(QUEUE_SIZE);
	std::vector<u8> m_sx_bytes = std::vector<u8>(SYSEX_BYTES);
	u32 m_q_head = 0, m_q_count = 0, m_sx_used = 0;
	bool m_midi_overflowed = false;
	parser m_in[PORTS + 1];                         // 最後の 1 つはパネルの口
	u64 m_rx_at[PORTS + 1] = {};
	prog_sel m_sel[64];
	rpn_sel m_rpn[64];
	gs_effects m_gs_effects;
	u32 m_eg_phase_quarters = EG_PHASE_AT_BOOT_QUARTERS + EG_GRID_STEP_QUARTERS;
	bool m_chorus_type_loaded = false;     // 電源を入れたあと、リセットかコーラスの種類で読み込んだか
	static constexpr u32 FIRST_EXTERNAL_RESET = 1;   // 0 番目は起動のときの xg_reset
	u32 m_resets_received = 0;
	bool m_last_reset_gs = false;
	u32 m_repeat_reset_delay = 0;
	bool m_reset_keeps_chorus_lfo = false;
	bool m_lfo_queued_reset_pending = false;
	bool m_bank_drum[64] = {};
	// エフェクトの状態と、区画ごとのフロー
	bool m_fx_on = false;
	float m_master_output_gain[2] = {};
	u8 m_eq_presets[5][20] = {};
	lfo_bank m_lfo, m_extra_insertion_lfo;                // マスタと第 2 群のエフェクト の LFO
	native_fx::master_eq_flow m_eq;
	native_fx::reverb_flow m_reverb;
	float m_output_gain[2] = {};
	static constexpr u32 REVERB_QUIET_HOLD = 0x10000 + 4410;
	u32 m_reverb_quiet = 0;
	fx_block m_var, m_ins1;
	extra_insertion m_extra_insertions[3] = { { 0 }, { 1 }, { 2 } };
	std::vector<float> m_extra_insertion_table = std::vector<float>(0x40000, 0.0f);
	u16 m_system_returns[4][2] = {};   // リバーブ・コーラスの戻りの減衰（fx_mixer_refresh が使う）
	struct fx_region {
		int first_lfo;
		int group = -1, made = -1;
		native_fx::effect_flow *flow = nullptr;
		effect_flow_set flows;
		u32 quiet = 0, hold = 0;
		u64 active_at = 0;     // 種類を読み込み終える時刻。それまでは止まっていて音も出ない
	};
	fx_region m_regions[3] = { { 8 }, { 12 }, { 18 } };
	std::vector<float> m_table = std::vector<float>(0x40000, 0.0f);
	u64 m_part_mute = 0;                            // パネルの MUTE/SOLO
	int m_receive_device = 16;
	bool m_receive_system_on = true, m_receive_exclusive = true, m_receive_bank_select = true;
	bool m_sysex_from_panel = false;
	u8 m_ad_mute = 0;
	static constexpr u8 PERF_ATTENUATOR = 0x20;
	static constexpr int CHAIN_LINK_ATTENUATION = 0x10;
	static constexpr int PERF_VOICE_ATTENUATION = 8;
	bool m_perf_on = false, m_perf_edited = false, m_perf_entering = false;
	u32 m_lfo_load_delay = 0;                       // パフォーマンスの読み込みで LFO を書くまで（読み込みの間だけ 0 以外）
	int m_perf_bank = 0, m_perf_number = 0, m_perf_channel = 0, m_perf_latched_bank = 0;
	u8 m_perf[PERFORMANCE_SIZE] = {};
	u8 m_perf_recall[PERFORMANCE_SIZE] = {};
	bool m_perf_recall_valid = false;
	int m_perf_recall_bank = 0, m_perf_recall_number = 0;
	static constexpr size_t NOTICES = 64;
	panel_notice m_notices[NOTICES] = {};
	size_t m_notice_read = 0, m_notice_write = 0;
	bool m_sysex_inner = false;
	u32 m_perf_loads = 0;
	u32 m_external_resets = 0;
	bool m_ad_locked = false, m_eq_locked = false;
	u8 m_eq_type = 0;
	bool m_reset_seen = false;
	bool m_reverb_type_written = false, m_chorus_type_written = false;   // 電源を入れてから種類か初期化を受けたか
	bool m_perf_held = false;
	std::vector<u8> m_perf_internal;
	u8 m_ad_expression[2] = { 127, 127 };
	smu2000::xg::effect_tables::Input m_fxin;            // バンクでドラムにしたので、組を付けたパート
	u64 m_clock = 0;
};

} // namespace smu2000

#endif // S_MU2000_NATIVE_ENGINE_H
