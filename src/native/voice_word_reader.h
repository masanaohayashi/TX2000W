// license:BSD-3-Clause
// S-MU2000: 声の制御（xg::native_driver）が出す書き込みを、声の名前付きのパラメータへ直す。
// 書き込みの番号と声のパラメータの対応は、このファイルだけが持つ。
#ifndef S_MU2000_NATIVE_VOICE_WORD_READER_H
#define S_MU2000_NATIVE_VOICE_WORD_READER_H

#include "native/voice_bank.h"
#include "xg/native_voice.h"

#include <cstdint>

namespace native_voice {

class voice_word_reader
{
public:
	explicit voice_word_reader(voice_bank &bank) : m_bank(bank) {}

	void key_on(int group, uint64_t mask)
	{
		for (int i = 0; i < first_group_size; i++)
			if ((mask >> i) & 1)
				m_bank.key_on(group * first_group_size + i);
	}

	void write(int v, int word_kind, uint16_t value)
	{
		namespace word = xg::nv::word;
		held_words &h = m_held[v];
		switch (word_kind) {
		case word::lowpass_cut: m_bank.set_lp_cut(v, value & 0x7ff); break;
		case word::highpass_cut: m_bank.set_hp_cut(v, value & 0x7ff); break;
		case word::resonance: m_bank.set_resonance(v, value >> 11); break;
		case word::amplitude_lfo: m_bank.set_lfo_amp_depth(v, value & 0x7f); break;
		case word::attack: m_bank.set_attack(v, value >> 8, value & 0xff); break;
		case word::first_decay: m_bank.set_decay1(v, value >> 8, value & 0xff); break;
		case word::second_decay: m_bank.set_decay2(v, value >> 8, value & 0xff); break;
		case word::release: m_bank.set_release(v, (value >> 8) & 0x7f, (value >> 15) != 0, value & 0xff); break;
		case word::lfo: m_bank.set_lfo(v, value >> 14, (value >> 8) & 0x3f, ((value >> 7) & 1) != 0, value & 0x7f); break;
		case word::pitch_env_rate: m_bank.set_peg_rate(v, (value >> 8) & 0x7f); break;
		case word::pitch_env_target: m_bank.set_peg_target(v, signed_14(value)); break;
		case word::pitch: m_bank.set_pitch_base(v, signed_14(value)); break;
		case word::lead_high: h.lead_hi = value; lead_words(v, h); break;
		case word::lead_low: h.lead_lo = value; lead_words(v, h); break;
		case word::loop_high: h.loop_hi = value; loop_words(v, h); break;
		case word::loop_low: h.loop_lo = value; loop_words(v, h); break;
		case word::wave_high: h.base_hi = value; base_words(v, h); break;
		case word::wave_low: h.base_lo = value; base_words(v, h); break;
		case word::eq_low_previous: h.eq[0][1] = int16_t(value); eq_words(v, h, 0); break;
		case word::eq_low_feedback: h.eq[0][2] = int16_t(value); eq_words(v, h, 0); break;
		case word::eq_low_current: h.eq[0][0] = int16_t(value); eq_words(v, h, 0); break;
		case word::eq_high_feedback: h.eq[1][2] = int16_t(value); eq_words(v, h, 1); break;
		case word::eq_high_previous: h.eq[1][1] = int16_t(value); eq_words(v, h, 1); break;
		case word::eq_high_current: h.eq[1][0] = int16_t(value); eq_words(v, h, 1); break;
		case word::pan_mix: h.mix[0] = value; mix_words(v, h); break;
		case word::reverb_send: h.mix[1] = value; mix_words(v, h); break;
		case word::chorus_send: h.mix[2] = value; mix_words(v, h); break;
		case word::route_first: h.route[0] = value; route_words(v, h); break;
		case word::route_second: h.route[1] = value; route_words(v, h); break;
		case word::route_third: h.route[2] = value; route_words(v, h); break;
		default: break;
		}
	}

private:
	struct held_words
	{
		uint16_t lead_hi = 0, lead_lo = 0, loop_hi = 0, loop_lo = 0, base_hi = 0, base_lo = 0;
		int eq[2][3] = {};                  // [段][current, previous, feedback]
		uint16_t mix[3] = {}, route[3] = {};
	};

	static int signed_14(uint16_t v) { return (int(v & 0x3fff) ^ 0x2000) - 0x2000; }

	void lead_words(int v, const held_words &h)
	{
		m_bank.set_lead_length(v, uint32_t(h.lead_hi & 0xff) << 16 | h.lead_lo);
		m_bank.set_loop_fraction(v, (h.lead_hi >> 8) & 63);
		m_bank.set_one_shot(v, ((h.lead_hi >> 14) & 1) != 0);
	}

	void loop_words(int v, const held_words &h)
	{
		m_bank.set_loop_length(v, uint32_t(h.loop_hi & 0xff) << 16 | h.loop_lo);
		const int fine = (h.loop_hi >> 8) & 0x7f;
		m_bank.set_fine_tune(v, (fine ^ 0x40) - 0x40);
		m_bank.set_reverse(v, (h.loop_hi >> 15) != 0);
	}

	void base_words(int v, const held_words &h)
	{
		m_bank.set_wave_base(v, uint32_t(h.base_hi & 0x1ff) << 16 | h.base_lo);
		m_bank.set_compress_decay(v, (h.base_hi >> 9) & 3);
		m_bank.set_compress_scale(v, (h.base_hi >> 11) & 7);
		m_bank.set_wave_format(v, h.base_hi >> 14);
	}

	void eq_words(int v, const held_words &h, int stage)
	{
		m_bank.set_eq(v, stage, h.eq[stage][0], h.eq[stage][1], h.eq[stage][2]);
	}

	void mix_words(int v, const held_words &h)
	{
		m_bank.set_mix(v, h.mix[0] >> 8, h.mix[0] & 0xff, h.mix[1] >> 8, h.mix[1] & 0xff, h.mix[2] >> 8, h.mix[2] & 0xff);
	}

	void route_words(int v, const held_words &h)
	{
		destination_id d = dest_none;
		if (h.route[0] == 0x4d00 && h.route[1] == 0x4800 && h.route[2] == 0x4400)
			d = dest_normal;
		else if (h.route[0] == h.route[1] && h.route[1] == h.route[2])
			switch (h.route[0]) {
			case 0x4000: d = dest_variation; break;
			case 0x1000: d = dest_ins1; break;
			case 0x0001: d = dest_ins2; break;
			case 0x0002: d = dest_ins3; break;
			case 0x0004: d = dest_ins4; break;
			default: break;
			}
		m_bank.set_destination(v, d);
	}

	voice_bank &m_bank;
	held_words m_held[voice_count];
};

} // namespace native_voice

#endif // S_MU2000_NATIVE_VOICE_WORD_READER_H
