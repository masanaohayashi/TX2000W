// license:BSD-3-Clause

#ifndef S_MU2000_XG_NATIVE_DRIVER_H
#define S_MU2000_XG_NATIVE_DRIVER_H

#pragma once

#include "xg/native_voice.h"
#include "xg/synth_state.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <vector>

namespace xg {

namespace word = nv::word;

class native_driver
{
public:
	static constexpr int PARTS = 64;
	static constexpr int SLOTS = 128;
	static constexpr int CHIP_SLOTS = 64;

	// スロットの印（128 ビット）。32 ビットの Windows でも作れるように 64 ビット 2 つで持つ
	struct slot_bits
	{
		u64 w[2] = { 0, 0 };
		static slot_bits of(int i)
		{
			slot_bits b;
			b.w[i >> 6] = u64(1) << (i & 63);
			return b;
		}
		bool test(int i) const { return (w[i >> 6] >> (i & 63)) & 1; }
		void set(int i) { w[i >> 6] |= u64(1) << (i & 63); }
		explicit operator bool() const { return (w[0] | w[1]) != 0; }
		slot_bits &operator|=(const slot_bits &o)
		{
			w[0] |= o.w[0];
			w[1] |= o.w[1];
			return *this;
		}
	};

	static constexpr int EG_LAG = -3;

	// スロット 1 つの使われ方
	struct slot_use {
		bool on = false;
		u8 attack_low = 0;             // 打鍵時の attack の下位。立ち上がりのつまみは上位しか替えない
		// **鍵をもう押したか**。要素を書き終えてから押すので、混んでいると押すのが
		// 数十 ms 遅れる（write_done）。その間に届いた離しは off_wait に預けて、
		// 押した直後に効かせる（実機は MIDI を順に処理するので、押しを終えてから
		// 離しを読む）。先に離していたので、ゲートの短いドラムが混んだ所で
		// 鳴らなかった（離した印の付いたスロットへ遅れて鍵が押されていた）
		bool keyed = false;
		bool off_wait = false;
		u64 tstart = 0;                 // 鳴らし始めた時刻
		bool held = false;              // ダンパーで離しを待たせている
		bool sost = false;              // ソステヌート（CC66）で離しを待たせている
		// **離しの最中**（on は落ちたが、まだ鳴り終わっていない）。
		// 実機はこの間もつまみの動きを反映するので、こちらも追う必要がある。
		// 追わないと、曲の終わりの CC7 のフェードアウトで、離したばかりの
		// 長い音（ストリングスなど）だけが元の音量のまま鳴り続ける
		bool rel = false;
		u64  rel_at = 0;                // 離した時刻
		int  rel_att = 0;               // 離しのときに書いた減衰（戻さないための下限）
		bool caught = false;
		u16  reverb_base = 0, chorus_base = 0;
		int  rnd_pan = -1;
		int  vel = 0;                   // 押した強さ（液晶のメーター用）
		// **キーアサインがシングルで切られた音**。離しの速さが 0xD9 になる
		bool single_cut = false;
		// **オルタネートグループで切った打の、止めを刺す時刻**（0 は無し）。
		// 実機は 0xD9 で離したあと 1010 サンプル（23ms）で 0xF0 を書く
		u64  alt_kill = 0;
		int part = -1, note = -1, att = 0;
		// **MIDI で押された鍵**。note のほうは XG のノートシフト（08 pp 08）を
		// 足した「鳴らす鍵」なので、離すときの照合はこちらで見る
		int keynote = -1;
		// **音量の目盛り**（つまみを掛ける前）と、目盛りに乗らない側の減衰。
		// 実機は目盛りに音量を掛けてから 1 回だけ表を引くので、CC7・CC11 が
		// 動いたらこの 2 つから作り直す
		int lvl0 = 0, arest = 0;
		const u8 *elem = nullptr;
		const u8 *wave = nullptr;       // ベンドで音程を作り直すのに要る
		u16 lfo = 0;                    // いま鳴らしている lfo（モジュレーションを足す前）
		u16 cut = 0;                    // いま鳴らしている lowpass_cut（明るさを足す前）
		int facc = 0, ftgt = 0, finc = 0, fstage = 0, fadj = 0, fvel = 100;
		bool hard = false;              // オールサウンドオフで切った（離しを最速に）
		u32 inst = 0;                   // 何回目の押しか（同じ鍵を重ねたとき用）
		u64 fnext = 0;                  // つぎに 1 段進める時刻
		u32 fskip = 0;                  // 押した直後の 1 目を飛ばすぶん
		u64 key_at = 0;                 // 鍵を押す時刻
		bool keeps_steps = false;       // レガートで前の音の刻みを受け継いだ
		// **サンプル＆ホールドの音程**
		u64 sh_next = ~u64(0);          // つぎの 20ms の刻み
		int sh_cnt = 0;                 // 残りの刻み（0 なら無し）
		int sh_off = 0;                 // いまのずらし
		int key_shift = 0;              // note のうち、ノートシフトと移調でずらした半音
		// **音程の包絡線の行き先**。実機はキーオンの直後にこれを書いて、
		// あとは群に任せる。
		u16 peg_tgt = nv::NO_PITCH_TARGET;
		// **遅れて掛かるビブラート**
		int vcnt = 0;          // いまのカウンタ
		int vtgt = 0;          // 止まる所
		int vstep = 1;         // 1 歩
		int vdly = 0;          // 遅れの残り（20ms の目盛り）
		// **SFX キットの打**。要素を持って旋律の道で組んだが、離しは
		// ドラムの決まり（3n rr 09 が立っていなければ鳴りきる）に従う
		bool sfx = false;
		std::array<u8, nv::ELEM_BYTES> sfx_el{};
		u16 vhi = 0;           // lfo の上位（型と刻み）
		u16 ahi = 0;           // amplitude_lfo の上位
		int vamp = 0;          // 遅れが明けたあとの、音量側の揺れ
		bool vamp_waiting = false;   // 音量側の揺れをまだ書いていない
		int fdvel = 100;       // フィルタの包絡線の深さだけに使う強さ
		u32 lph = 0;           // 位相（0-0x7fff）
		u16 lstep = 0;         // 10ms あたりの歩幅
		int lfdep = 0;         // 深さ（遅れが明けるまで 0）
		int lfull = 0;         // 遅れが明けたあとの深さ
		int lcut = 0;          // 切る高さへ足すぶん
		bool ltri = true;      // 三角波か（LFO の波形 が 0 でなければ）
		bool lrun = false;     // 遅れが明けて、位相を進める段階に入ったか
		// **つまみの割り当てで音程を書き直す印**。
		// 実機は受けた瞬間ではなく、**次の 10ms の刻み**で書く
		bool pdirty = false;
		// **Vib Depth のぶんのせり上がり**。0 から 20ms ごとに vstep2 ずつ
		int vcnt2 = 0;
		int vstep2 = 5;
		int vdep = 64;         // 押したときの Vib Depth（08 pp 16）
		bool vramp = false;    // 遅れて掛かるビブラートを動かしている
		u64 vnext = ~u64(0);   // つぎに進める時刻
		// **音程の包絡線の段**（0 が押した直後の段。3 で終わり）。
		// 群が行き先に着いたら次の段を張る
		int pstage = 3;
		int pvel = 100;
		u64 pnext = 0;              // つぎに着いたか見る時刻
		s32 glide = 0, glide_step = 0;
		u64 glide_next = 0;
		u64 age = 0;
		int legato_att = 0;    // レガートで引き継いだ包絡線のぶんの減衰
	};
	// **レガートで引き継ぐ音**（モノ + ポルタメントで、押さえたまま次の鍵を押したとき）
	struct legato_source { const u8 *elem; int level; u16 cut; int facc, ftgt, finc, fstage, fadj; u64 fnext; };

	// 声 1 つの設定を 1 語書く口
	using poke_fn = std::function<void(int voice, int word, u16 value)>;
	// 群ごとに声をまとめて鳴らす口（mask のビットが群の中の声）
	using key_on_fn = std::function<void(int group, u64 mask)>;
	void set_key_on(key_on_fn f) { m_key_on = std::move(f); }

	void set_poke(poke_fn f) { m_poke = std::move(f); }
	using peek_fn = std::function<bool(int)>;
	void set_peg_peek(peek_fn f) { m_peg_peek = std::move(f); }
	// **そのスロットがまだ鳴っているか**を群に聞く。
	// オルタネートグループで切る相手を選ぶのに使う
	void set_slot_peek(peek_fn f) { m_slot_peek = std::move(f); }
	// **実機がまだそのスロットを持っているか**。離した音の
	// 尾をどこまで追うかはこれで決める
	void set_slot_held(peek_fn f) { m_slot_held = std::move(f); }
	void set_level_peek(std::function<int(int)> f) { m_level_peek = std::move(f); }
	void set_rom(const u8 *rom)
	{
		m_rom = rom;
		if (rom) {
			nv::vib_part_targets(rom, nv::VIB_DEPTH_TAB, m_vib_part_target[0]);
			nv::vib_part_targets(rom, nv::VIB_DEPTH_TAB_GS, m_vib_part_target[1]);
		}
	}
	void set_state(synth_state *state) { m_state = state; }

	void reset()
	{
		for (auto &a : m_pat)
			a.fill(0);
		for (auto &s : m_slot)
			s = slot_use();
		for (auto &c : m_cc)
			c = part_cc();
		m_clock = 0;
		m_alt_kill_next = ~u64(0);
		m_traj = false;
		m_traj_next = 0;
		m_pend.clear();
		m_busy = 0;
		m_bend_due.fill(0);
		m_bend_next = ~u64(0);
		m_age = 0;
	}

	void stop_all_sound()
	{
		for (int p = 0; p < PARTS; p++)
			all_off(p, true);
		m_pend.clear();
		m_bend_due.fill(0);
		// 音がなくなったので時計を 0 から数え直す（同じ入力には、いつでも同じ音を返す）
		for (auto &s : m_slot)
			s = slot_use();
		m_clock = 0;
		m_alt_kill_next = ~u64(0);
		m_traj = false;
		m_traj_next = 0;
		m_busy = 0;
		m_bend_next = ~u64(0);
		m_age = 0;
	}

	void reset_parts()
	{
		for (int p = 0; p < PARTS; p++) {
			all_off(p, true);
			m_cc[p] = part_cc();
		}
		m_pend.clear();
		m_bend_due.fill(0);
		m_bend_next = ~u64(0);
	}

	// ドラムのパートか。XG の「パートモード」（08 pp 07。0 が普通、1 以上がドラム）を見る。
	// 「記録が引けない＝ドラム」では、音色を選び終える前の旋律パートまで拾ってしまう
	bool is_drum(int part) const
	{
		if (part < 0 || part >= PARTS)
			return false;
		return m_state->parts[part][settings::PART_MODE] != 0;
	}

	// そのパート・鍵のドラムの 1 打の記録（42 バイト）。無ければ nullptr
	const u8 *drum_rec_of(int part, int note) const
	{
		if (!m_rom || part < 0 || part >= PARTS)
			return nullptr;
		return nv::drum_record(m_rom, int(m_state->parts[part].kit), note);
	}

	// 書いている間に通った 2.5ms の刻みの数から、失われた刻みの増えたぶんを返す（doc/voice-flow/voice.md の 10 ms の格子）
	int note_writes_lost_ticks()
	{
		const u64 grid = u64(m_eg_phase) * 4;
		const auto ticks_until = [grid](u64 sample) { return (sample * 4 + EG_TICK_QUARTERS - grid) / EG_GRID_STEP_QUARTERS; };
		const u64 from = m_burst_start / 64;
		const u64 ticks = ticks_until(from > WRITES_BLOCK_BEFORE ? (m_busy / 64 + WRITES_BLOCK_AFTER) : 0) - ticks_until(from > WRITES_BLOCK_BEFORE ? from - WRITES_BLOCK_BEFORE : 0);
		const int lost = int(ticks > 1 ? ticks - 1 : 0) - m_burst_lost_ticks;
		if (lost <= 0)
			return 0;
		m_burst_lost_ticks += lost;
		return lost;
	}

	// 格子が動いた。鳴っている声のつぎの刻みも同じだけ遅れる
	void delay_voice_steps(u32 samples)
	{
		// 遅れて鳴る要素の待ちも同じ刻みを数えている
		const u64 last_key = (m_busy + 32) / 64;
		for (pending_key &p : m_pend)
			if (p.at > last_key)
				p.at += samples;
		for (slot_use &s : m_slot) {
			if (!s.elem)
				continue;
			if (s.key_at > last_key)
				s.key_at += samples;
			s.fnext += samples;
			s.pnext += samples;
			s.glide_next += samples;
			if (s.vnext != ~u64(0))
				s.vnext += samples;
			if (s.sh_next != ~u64(0))
				s.sh_next += samples;
		}
	}

	// 音を書き終えた。今の音の最初の刻みを決める。書いている間は刻みの仕事が動かない
	void settle_note_steps()
	{
		const u64 last_key = (m_busy + 32) / 64;
		m_steps_from = last_key + STEP_AFTER_LAST_KEY;
		if (m_eg_have)
			for (int i = 0; i < SLOTS; i++) {
				if (!m_burst_voices.test(i))
					continue;
				slot_use &s = m_slot[i];
				if (!s.elem)
					continue;
				// 書き始めより前の目は、もう前の 10ms の仕事で終わっている
				const u64 written = u64(s64(s.tstart) + EG_LAG);
				const u64 near_last_key = last_key > STEP_BEFORE_LAST_KEY ? last_key - STEP_BEFORE_LAST_KEY : 0;
				const u64 first = slot_eg_after(s.key_at > last_key ? u64(s64(s.key_at) + EG_LAG) : std::max(written, near_last_key), i);
				s.pnext = first;
				if (s.keeps_steps || !s.fnext)
					continue;
				const u64 fnext = first + s.fskip;
				const s64 moved = s64(fnext) - s64(s.fnext);
				s.fnext = fnext;
				s.glide_next = u64(s64(s.glide_next) + moved);
				if (s.sh_next != ~u64(0))
					s.sh_next = u64(s64(s.sh_next) + moved);
				if (s.vnext != ~u64(0)) {
					s.vnext = s.fnext;
					if (off_vibrato_step(s.vnext, i))
						s.vnext += FENV_TICK;
				}
			}
		m_burst_voices = {};
	}

	u32 eg_phase() const { return m_eg_phase; }

	// 格子の位相は 1/4 サンプルで受ける（2.5ms の刻みは 110.25 サンプル）
	void force_eg_phase(u32 quarters)
	{
		m_eg_quarters = quarters % EG_TICK_QUARTERS;
		m_eg_phase = ((m_eg_quarters + 2) / 4) % FENV_TICK;
		m_eg_have = true;
	}

	// x 以降でいちばん早い格子の目
	u64 eg_after(u64 x) const { return grid_after(x, m_eg_phase); }

	// **声の刻みは、スロットの組ごとに 2.5ms ずつ遅れる**。
	// 声は 32 スロットずつ 4 組に分かれていて、10ms の仕事は 2.5ms の刻みごとに 1 組ずつ回る。
	// 組 0（スロット 0-31）が格子の目、組 1 はその 2.5ms あと、組 3 は 7.5ms あと
	// （1 音ずつ弾いてスロットを 128 まで進め、組ごとの遅れを測った）
	u32 slot_eg_phase(int slot) const
	{
		const u32 group = u32(slot) / SLOTS_PER_STEP_GROUP;
		return ((m_eg_quarters + group * EG_GRID_STEP_QUARTERS + 2) / 4) % FENV_TICK;
	}
	u64 slot_eg_after(u64 x, int slot) const { return grid_after(x, slot_eg_phase(slot)); }
	static u64 grid_after(u64 x, u32 phase)
	{
		const u64 base = x - (x % FENV_TICK) + phase;
		return base >= x ? base : base + FENV_TICK;
	}
	// 20ms の刻み（ビブラート）。奇数のスロットは組の 10ms の目、偶数はその 10ms あと
	bool off_vibrato_step(u64 at, int slot) const
	{
		return (at + nv::VIB_TICK - (u64(slot_eg_phase(slot)) + (slot & 1 ? 0 : FENV_TICK)) % nv::VIB_TICK) % nv::VIB_TICK != 0;
	}

	// 時計だけ進める（tick を呼ばないサンプルがあるので）
	void set_clock(u64 clock) { m_clock = clock; }

	// 次に tick に仕事がある時刻。それまで tick は何もしないので呼ばなくてよい
	u64 next_tick() const
	{
		u64 next = std::min(m_alt_kill_next, m_bend_next);
		for (const auto &p : m_pend)
			next = std::min<u64>(next, p.at);
		if (m_traj)
			next = std::min(next, m_traj_next);
		return next;
	}

	struct small_pend
	{
		std::pair<slot_bits, u32> items[SLOTS];
		int count = 0;
		void push_back(const std::pair<slot_bits, u32> &v) { if (count < SLOTS) items[count++] = v; }
		const std::pair<slot_bits, u32> *begin() const { return items; }
		const std::pair<slot_bits, u32> *end() const { return items + count; }
	};

	// フィルタの包絡線を流し、遅らせた要素を鳴らす
	void tick(u64 clock)
	{
		m_clock = clock;
		if (!m_pend.empty()) {
			// **同じ時刻のものは 1 回で押す**。実機も要素をまとめて
			// 押すので、要素ごとに分けると合図が 2 回になってしまう
			size_t w = 0;
			slot_bits now;
			for (size_t i = 0; i < m_pend.size(); i++) {
				if (m_pend[i].at <= clock)
					now |= m_pend[i].mask;
				else
					m_pend[w++] = m_pend[i];
			}
			m_pend.resize(w);
			if (now)
				key_on(now);
		}
		// **オルタネートグループで切った打に止めを刺す**
		if (clock >= m_alt_kill_next) {
			u64 next3 = ~u64(0);
			for (int i = 0; i < SLOTS; i++) {
				slot_use &s = m_slot[i];
				if (!s.alt_kill)
					continue;
				if (s.alt_kill <= clock) {
					s.alt_kill = 0;
					m_poke(i, word::release,
					       u16(nv::HARD_CUT_RATE | u16(note_att(s, s.part) & 0xff)));
					s.on = false;
					s.rel = false;     // 音は消えたのでスロットを空ける
				} else if (s.alt_kill < next3) {
					next3 = s.alt_kill;
				}
			}
			m_alt_kill_next = next3;
		}
		// **格子に乗せたベンド**
		if (clock >= m_bend_next) {
			u64 next2 = ~u64(0);
			for (int p = 0; p < PARTS; p++) {
				if (!m_bend_due[p])
					continue;
				if (m_bend_due[p] <= clock) {
					m_bend_due[p] = 0;
					apply_bend(p);
				} else if (m_bend_due[p] < next2) {
					next2 = m_bend_due[p];
				}
			}
			m_bend_next = next2;
		}
		if (!m_traj)
			return;
		if (clock < m_traj_next)
			return;
		u64 next = ~u64(0);
		int live = 0;
		// **遅れて掛かるビブラート**の歩み。鍵を離したあとも実機は同じに進める
		const auto vib_moving = [](const slot_use &x) {
			return x.vdly > 0 || x.vcnt < x.vtgt || (x.vramp && x.vcnt2 < nv::VIB_PART_CNT_END);
		};
		const auto step_vibrato = [&](slot_use &s, int i) {
			if (vib_moving(s)) {
				bool movp = false, mova = false;
				const int count_before = s.vcnt;
				while (clock >= s.vnext && vib_moving(s)) {
					if (s.vdly > 0) {
						s.vdly--;
						if (!s.vdly && s.vamp_waiting && !vib_moving(s)) {
							s.vamp_waiting = false;
							mova = true;     // 遅れが明けて、音程の側に歩みが無い
						}
					} else {
						if (s.vamp_waiting) {
							s.vamp_waiting = false;
							mova = true;     // 音量の側も音程の 1 歩目と同じ刻みで掛かる
						}
						// **フィルタ側の LFO が掛かり出すのは、
						// せり上げの 1 歩目と同じ刻み**。
						// 遅れが 0 になった刻みではまだ掛からない
						s.lrun = true;
						s.lfdep = s.lfull;
						s.vcnt += s.vstep;
						if (s.vcnt > s.vtgt)
							s.vcnt = s.vtgt;
						s.vcnt2 = std::min(nv::VIB_PART_CNT_END, s.vcnt2 + s.vstep2);
						movp = true;
					}
					s.vnext += nv::VIB_TICK;
				}
				if (mova)
					m_poke(i, word::amplitude_lfo,
					       u16(nv::MOVING_LEVEL_LFO | u16(nv::amod_word(
					           assign_amod(s.part, s.keynote), s.vamp / 2))));
				if (movp) {
					// 音色のぶんと Vib Depth のぶんの大きいほう。モジュレーションとの大きいほうは
					// lfo_word が取る。今の値は s.lfo に置く（つまみが動いたときの元になる）
					// 音色のカウンタが止まって値も変わらない刻みは実機も書かない（Vib Depth のカウンタは表の頭打ちの先まで回る）
					const u16 ramped = u16(s.vhi | u16(nv::vib_ramp_value(m_rom, s.vdep, s.vcnt, s.vcnt2, gs_mode())));
					if (ramped != s.lfo || s.vcnt != count_before) {
						s.lfo = ramped;
						m_poke(i, word::lfo, lfo_word(s.lfo, s.part, s.keynote));
						// 実機はこの刻みでも切る高さを作り直す
						//（位相は進めない）
						if (s.lstep && s.elem) {
							s.lcut = nv::lfo_fcut(
							    nv::lfo_fwave(s.lph, s.ltri), s.lfdep);
							m_poke(i, word::lowpass_cut,
							       cut_with_cc(s, s.cut));
						}
					}
				}
				if (vib_moving(s) && s.vnext < next)
					next = s.vnext;
			}
		};
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			// **サンプル＆ホールドの音程**は、押していても
			// 離していても、声が鳴り終わってスロットが空くまで回る。
			// 早く止めると乱数の列がずれ、あとで押す音の LFO の位相まで変わる
			// （離しの長い音色では、離して 3 秒たっても引き続けていた）
			if (rel_follow(s, clock))
				if (sh_tick(i, clock, next))
					live++;          // 見回りを止めない
			// **離しの最中もフィルタを動かす**。実機は離しのあいだも
			// lowpass_cut・highpass_cut・resonance を書き続ける
			if (!s.on) {
				// **鳴り終わった声には書かない**
				if (!rel_follow(s, clock))
					continue;
				// **離しの最中も包絡線を式で動かす**
				if (s.elem) {
					bool moved = false;
					while (clock >= s.fnext) {
						fenv_step(s);
						lfo_tick(s);     // 離しのあいだも揺れる
						s.fnext += FENV_TICK;
						moved = true;
					}
					if (moved) {
						s.cut = fenv_cut(s);
						m_poke(i, word::lowpass_cut, cut_with_cc(s, s.cut));
						if (s.pdirty && s.wave) {
							s.pdirty = false;
							m_poke(i, word::pitch, pitch_of(s));
						}
					}
					if (s.finc || s.lfdep) {
						live++;
						if (s.fnext < next)
							next = s.fnext;
					}
				}
				// 実機と同じ順。包絡線の目のあとに、同じ目のビブラートの歩み
				if (vib_moving(s)) {
					live++;
					step_vibrato(s, i);
				}
				continue;
			}
			live++;
			// **音程の包絡線の段**。群が行き先に着いていたら次の段を張る
			if (s.pstage < 3 && s.elem && m_peg_peek) {
				while (clock >= s.pnext && clock >= m_steps_from) {
					if (m_peg_peek(i))
						peg_advance(i);
					s.pnext += FENV_TICK;
					if (s.pstage >= 3)
						break;
				}
				if (s.pstage < 3 && s.pnext < next)
					next = std::max(s.pnext, m_steps_from);
			}
			// **フィルタの包絡線を式で動かす**
			if (s.elem) {
				bool moved = false;
				while (clock >= s.fnext && clock >= m_steps_from) {
					fenv_step(s);
					// **フィルタ側の LFO** も同じ 10ms の刻み
					//。実機は**足すぶんを先に作ってから
					// 位相を進める**（だから 1 刻み遅れる）。
					// 遅れ（ビブラートの vdly）のあいだは位相も止まる
					lfo_tick(s);
					s.fnext += FENV_TICK;
					moved = true;
				}
				if (moved) {
					s.cut = fenv_cut(s);
					m_poke(i, word::lowpass_cut, cut_with_cc(s, s.cut));
					// **つまみの割り当ての音程もこの刻み**
					if (s.pdirty && s.wave) {
						s.pdirty = false;
						m_poke(i, word::pitch, pitch_of(s));
					}
				}
				if (s.fnext < next)
					next = std::max(s.fnext, m_steps_from);
			}
			step_vibrato(s, i);
			// ポルタメント: 10ms ごとに残りのずれを step だけ 0 へ寄せて、
			// 音程の語を書き直す
			if (s.glide && s.elem && s.wave) {
				bool moved = false;
				while (s.glide && s.glide_next <= clock) {
					if (s.glide > 0)
						s.glide = s.glide > s.glide_step ? s.glide - s.glide_step : 0;
					else
						s.glide = -s.glide > s.glide_step ? s.glide + s.glide_step : 0;
					s.glide_next += nv::PORTA_TICK;
					moved = true;
				}
				// **動いたときだけ書く**。前は段の輪が回るたびに書いていて、
				// 1 音の滑りで pitch を 26000 回以上書いていた
				if (moved)
					m_poke(i, word::pitch, pitch_of(s));
			}
			if (s.glide && s.glide_next < next)
				next = s.glide_next;
		}
		m_traj = live > 0;
		m_traj_next = next;
	}

	void refresh_part(int part) { apply_cc(part); }
	void refresh_attack(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part == part && s.on && s.elem && m_rom) {
				m_poke(i, word::attack, u16((nv::attack_word(m_rom, s.elem, part_atk(part)) & 0xff00) | s.attack_low));
				if (s.fstage == 0 && s.finc)
					fenv_attack_step(s);
			}
		}
	}
	// ビブラートの速さは押している音にも効く（フィルタ側の LFO の歩幅も）。深さと遅れは押したときの値のまま
	void refresh_vibrato_rate(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part != part || !s.on || !s.elem || !m_rom)
				continue;
			const int rate = nv::vib_rate(int(s.elem[nv::elem_at::lfo_rate] & 0x3f), part_vrate(part), m_rom, gs_program(part));
			const u16 type_and_rate = u16(((s.elem[nv::elem_at::lfo_wave] ? nv::LFO_TRIANGLE_FLAG : 0) | rate) << 8);
			s.vhi = type_and_rate;
			s.lfo = u16(type_and_rate | (s.lfo & 0xff));
			if (s.lstep)
				s.lstep = nv::lfo_step(m_rom, rate);
			m_poke(i, word::lfo, lfo_word(s.lfo, part, s.keynote));
		}
	}
	bool gs_mode() const { return m_state->voice_mode == 0; }
	int gs_program(int part) const { return gs_mode() ? int(m_state->parts[part][settings::PART_PROGRAM]) : nv::XG_PROGRAM; }

	// 押しを ahead サンプル早く読んでいる間。時計はその音の時刻を指し、鍵を押すのはまだ先のことがある
	void begin_read_ahead(u64 ahead) { m_read_ahead = ahead; m_clock += ahead; }
	void end_read_ahead() { m_clock -= m_read_ahead; m_read_ahead = 0; }

	// パフォーマンスの間、全部の声に足す減衰
	void set_global_attenuation(int att) { m_global_att = att; }
	// パフォーマンスのモードか（バリエーションへの送りの作り方が変わる）
	void set_performance_mode(bool on) { m_performance = on; }
	void set_part_param(int part, u8 addr, u8 dd)
	{
		if (part < 0 || part >= PARTS)
			return;
		switch (addr) {
		case settings::PART_VIBRATO_RATE: refresh_vibrato_rate(part); break;
		case settings::PART_ATTACK: refresh_attack(part); break;
		default: break;
		}
		const u32 bend_end = ASSIGN_BEND + TO_LFO_LEVEL;
		const u32 xg_last = ASSIGN_AC2 + TO_LFO_LEVEL;
		if ((u32(addr) >= ASSIGN_WHEEL && u32(addr) <= bend_end) || (u32(addr) >= ASSIGN_PRESSURE && u32(addr) <= xg_last))
			if (asn_index(u32(addr)) >= 0)
				refresh_assign(part);
	}

	// つまみごとの割り当て（XG の 08 pp の番号）。どのつまみも行き先が同じ順に 6 つ続く
	static constexpr u32 ASSIGN_WHEEL = 0x1d, ASSIGN_BEND = 0x23, ASSIGN_PRESSURE = 0x4d, ASSIGN_KEY_PRESSURE = 0x53;
	static constexpr u32 ASSIGN_AC1_CC = 0x59, ASSIGN_AC1 = 0x5a, ASSIGN_AC2_CC = 0x60, ASSIGN_AC2 = 0x61;
	static constexpr u32 TO_PITCH = 0, TO_CUTOFF = 1, TO_LEVEL = 2, TO_LFO_PITCH = 3, TO_LFO_FILTER = 4, TO_LFO_LEVEL = 5;
	static constexpr int ASSIGN_TARGETS = 6;

	// **こちらが見ている割り当てのバイト**。つまみ 6 つ×
	// 行き先 6 つ（音程・切る高さ・音量・LFO の音程・フィルタ・音量）の 36 を全部
	static constexpr u32 ASN_BASE[6] = {
		ASSIGN_WHEEL, ASSIGN_BEND, ASSIGN_PRESSURE, ASSIGN_KEY_PRESSURE, ASSIGN_AC1, ASSIGN_AC2,
	};

	static int asn_index(u32 off)
	{
		for (int b = 0; b < 6; b++)
			if (off >= ASN_BASE[b] && off < ASN_BASE[b] + 6)
				return b * 6 + int(off - ASN_BASE[b]);
		return -1;
	}

	int asn_byte(int part, u32 off) const
	{
		return (part >= 0 && part < PARTS) ? int(m_state->parts[part][off]) : 64;
	}

	void refresh_assign(int part)
	{
		refresh_lfo_depth(part);
		refresh_assign_pitch(part);
		refresh_assign_amp(part);
		refresh_assign_amod(part);
		refresh_pmod(part);
	}

	// パートの音色の記録
	u32 record_of(int part) const
	{
		if (part < 0 || part >= PARTS)
			return 0;
		const part_state &p = m_state->parts[part];
		const u32 r = p.voice_record;
		return (r >= rom_position::VOICES && r + 16 <= rom_position::VOICES_END) ? r : 0;
	}


	// ---- コントローラ
	//

	// パートごとの、設定に置かないつまみの状態（doc/voice-flow/settings.md）
	struct part_cc {
		// ポルタメント（CC5 速さ・CC65 入切・CC84 で滑り出す鍵を指定）。
		// last は最後に押した鍵で、つぎの音はここから滑る
		int porta_time = 0, porta_src = -1, last = -1;
		bool porta_on = false;
		int bend = nv::BEND_CENTER;            // ピッチベンド
		bool mono = false;                     // CC126 モノ / CC127 ポリ
		bool damper = false;           // CC64
		bool sost_on = false;          // CC66（ソステヌート）
		bool soft = false;             // CC67（ソフトペダル）
		// **フィルタ側 LFO の深さに乗るつまみ**。
		// 値はこちらで覚える
		int chpress = 0;               // チャンネルアフタータッチ
		int ac1 = 0, ac2 = 0;          // AC1・AC2 の値
	};

	int part_vol(int part) const  { return m_state->parts[part][settings::PART_VOLUME]; }
	int part_expr(int part) const { return m_state->parts[part].expression; }
	int part_pan(int part) const  { return m_state->parts[part][settings::PART_PAN]; }
	int part_mod(int part) const  { return m_state->parts[part].modulation; }
	int part_rev(int part) const  { return m_state->parts[part][settings::PART_REVERB]; }
	int part_var(int part) const  { return m_state->parts[part][settings::PART_VARIATION]; }
	int part_atk(int part) const  { return m_state->parts[part][settings::PART_ATTACK]; }
	int part_dec(int part) const  { return m_state->parts[part][settings::PART_DECAY]; }
	int part_rel(int part) const  { return m_state->parts[part][settings::PART_RELEASE]; }
	int part_vrate(int part) const { return m_state->parts[part][settings::PART_VIBRATO_RATE]; }
	int part_vdep(int part) const { return m_state->parts[part][settings::PART_VIBRATO_DEPTH]; }
	int part_vdly(int part) const { return m_state->parts[part][settings::PART_VIBRATO_DELAY]; }
	// ベンドの幅（08 pp 23。64 が 0 半音。-24〜+24 で、負なら逆向きに曲がる）
	int bend_range(int part) const { return std::clamp(int(m_state->parts[part][settings::PART_BEND_RANGE]) - 64, -24, 24); }
	// パートの共振つまみ（CC71 / 08 pp 19）
	int res_knob(int part) const
	{ return part_res(part); }
	int assign_amp(int part, int note = -1) const
	{
		if (!m_rom || part < 0 || part >= PARTS)
			return 0;
		const part_state &b = m_state->parts[part];
		const part_cc &c = m_cc[part];
		int sum = 0;
		sum += nv::amp_assign(m_rom, asn_byte(part, ASSIGN_WHEEL + TO_LEVEL),
		                      int(b.modulation));
		sum += nv::amp_assign(m_rom, asn_byte(part, ASSIGN_PRESSURE + TO_LEVEL), c.chpress);
		sum += nv::amp_assign(m_rom, asn_byte(part, ASSIGN_AC1 + TO_LEVEL), c.ac1);
		sum += nv::amp_assign(m_rom, asn_byte(part, ASSIGN_AC2 + TO_LEVEL), c.ac2);
		const int bv = (c.bend - nv::BEND_CENTER) >> 6;
		if (bv) {
			const int q = nv::amp_assign(m_rom, asn_byte(part, ASSIGN_BEND + TO_LEVEL),
			                             bv < 0 ? -bv : bv);
			sum += bv < 0 ? -q : q;
		}
		if (note >= 36 && note < 98)
			sum += nv::amp_assign(m_rom, asn_byte(part, ASSIGN_KEY_PRESSURE + TO_LEVEL),
			                      int(m_pat[size_t(part)][size_t(note)]));
		return sum;
	}

	int assign_cut(int part, int note) const
	{
		if (part < 0 || part >= PARTS)
			return 0;
		const part_state &b = m_state->parts[part];
		const part_cc &c = m_cc[part];
		auto term = [](int d, int v) {
			return (d == 64 || !v) ? 0 : ((d - 64) * v) >> 2;
		};
		int sum = 0;
		sum += term(asn_byte(part, ASSIGN_WHEEL + TO_CUTOFF),
		            int(b.modulation));
		sum += term(asn_byte(part, ASSIGN_PRESSURE + TO_CUTOFF), c.chpress);
		sum += term(asn_byte(part, ASSIGN_AC1 + TO_CUTOFF), c.ac1);
		sum += term(asn_byte(part, ASSIGN_AC2 + TO_CUTOFF), c.ac2);
		if (note >= 36 && note < 98)
			sum += term(asn_byte(part, ASSIGN_KEY_PRESSURE + TO_CUTOFF),
			            int(m_pat[size_t(part)][size_t(note)]));
		const int pb = asn_byte(part, ASSIGN_BEND + TO_CUTOFF);
		if (pb != 64) {
			const int v = c.bend - nv::BEND_CENTER;
			if (v)
				sum += ((pb - 64) * v) >> 8;
		}
		return sum;
	}

	int vol_gain_of(int part, int vol, int expr) const
	{
		int g = nv::vol_gain(vol, expr);
		g = (g * (int(m_state->system[settings::SYS_VOLUME]) + 1)) >> 7;
		// 設定の大きさと、設定のつまみから作った値の比で直す
		const int g_part = int(m_state->parts[part].gain);
		int g_calc = nv::vol_gain(part_vol(part), part_expr(part));
		g_calc = (g_calc * (int(m_state->system[settings::SYS_VOLUME]) + 1)) >> 7;
		if (g_calc > 0)
			g = g * g_part / g_calc;
		// **つまみの割り当てはここでは足さない**。実機は音量を
		// 掛けたあとの目盛りの索引の側に足す（volume_att_from の `add`）
		return g < 0 ? 0 : (g > 128 ? 128 : g);
	}

	// **ベロシティ感度**（08 pp 0C 深さ・0D ずらし）を掛けた強さ
	int part_vel(int part, int vel) const
	{
		const part_state &b = m_state->parts[part];
		return nv::vel_sense(vel, int(b[settings::PART_VELOCITY_DEPTH]), int(b[settings::PART_VELOCITY_OFFSET]));
	}

	// **ノートシフト**（08 pp 08。64 が 0 半音、±24 まで）。実機は鍵を移して
	// から音色を選ぶので、要素の鍵域も波形の選び方も移した鍵で決まる
	// **スケールチューニング**。音名ごとに音程をずらす
	// （XG の 08 pp 41-4C。12 個、64 が 0 セント）
	int part_scale_cents(int part, int note) const
	{
		if (part < 0 || part >= PARTS)
			return 0;
		const u32 i = u32(((note % 12) + 12) % 12);
		return int(m_state->parts[part][settings::PART_SCALE + i]) - 64;
	}

	// **マスターチューン**。全部のパートに効く。
	// 00 00 00-03 の 4 バイトの下 4bit をつないだ 12bit（0x400 が 0 セント、
	// 1 きざみ 0.1 セント）
	static constexpr int MASTER_TUNE_CENTER = 0x400;
	int master_tune_tenths() const
	{
		const auto &b = m_state->system;
		const int v = ((b[0] & 0xf) << 12) | ((b[1] & 0xf) << 8)
		            | ((b[2] & 0xf) << 4) | (b[3] & 0xf);
		return (v & 0xfff) - MASTER_TUNE_CENTER;
	}

	// **RPN 1（微調）**。fine に「14bit の値 − 8192」が
	// 入る（8192 で 100 セント）。音程の語にだけ出る
	int part_fine_cents(int part) const
	{
		if (part < 0 || part >= PARTS)
			return 0;
		const part_state &b = m_state->parts[part];
		const int fine = b.fine;
		// マスターチューン（全部のパートに効く）も一緒に足す
		return fine * 100 / 8192 + master_tune_tenths() / 10 + part_detune_cents(part);
	}

	// **パートの DETUNE**（08 pp 09・0A）。旋律もドラムも同じセントを足す
	int part_detune_cents(int part) const
	{
		if (part < 0 || part >= PARTS)
			return 0;
		const part_state &b = m_state->parts[part];
		return nv::detune_cents(int(b[settings::PART_DETUNE]), int(b[settings::PART_DETUNE_LOW]));
	}

	int drum_shift_cents(int part) const
	{
		if (part < 0 || part >= PARTS)
			return 0;
		const part_state &b = m_state->parts[part];
		return (int(b[settings::PART_NOTE_SHIFT]) - 64 + int(b.coarse)) * 100 + part_detune_cents(part);
	}

	int part_shift(int part) const
	{
		int v = int(m_state->parts[part][settings::PART_NOTE_SHIFT]) - 64;
		v += int(m_state->system[settings::SYS_TRANSPOSE]) - 64;
		// **RPN 2（粗調）**。符号つきの半音
		v += int(m_state->parts[part].coarse);
		return v;
	}
	int part_cho(int part) const  { return m_state->parts[part][settings::PART_CHORUS]; }

	void apply_part_eq(nv::element_words &r, int part) const
	{
		if (!m_rom || part < 0 || part >= PARTS)
			return;
		const part_state &b = m_state->parts[part];
		nv::eq_set(m_rom, r, int(b[settings::PART_EQ_BASS]), int(b[settings::PART_EQ_TREBLE]),
		           int(b[settings::PART_EQ_BASS_FREQUENCY]), int(b[settings::PART_EQ_TREBLE_FREQUENCY]));
	}
	void apply_drum_eq(nv::element_words &r, int part, int note) const
	{
		if (!m_rom)
			return;
		nv::eq_set(m_rom, r, drum_live(part, note, settings::drum_param::eq_low_gain), drum_live(part, note, settings::drum_param::eq_high_gain),
		           drum_live(part, note, settings::drum_param::eq_low_freq), drum_live(part, note, settings::drum_param::eq_high_freq));
	}
	// **ドラムの HPF はパートの HPF と打ごとの HPF（3n rr 50）の足し算**
	// （パート 100 ＋ 打 84 が、どちらか一方の 120 と同じ語になった）
	int drum_hpf(int part, int note) const { return part_hpf(part) + drum_live(part, note, settings::drum_param::highpass) - 64; }
	int part_bri(int part) const  { return m_state->parts[part][settings::PART_CUTOFF]; }
	int part_setting(int part, u32 off) const { return m_state->parts[part][off]; }
	// パートの HPF（0A pp 20。64 が音色のまま）
	int part_hpf(int part) const  { return m_state->parts[part].highpass; }
	int part_res(int part) const  { return m_state->parts[part][settings::PART_RESONANCE]; }


	void assignable(int part, int cc, int value)
	{
		if (part < 0 || part >= PARTS)
			return;
		const part_state &pb = m_state->parts[part];
		const u32 num[2] = { ASSIGN_AC1_CC, ASSIGN_AC2_CC };
		for (int k = 0; k < 2; k++) {
			if (cc != int(pb[num[k]]))
				continue;
			// **値も覚える**。フィルタ側 LFO の深さに乗る
			(k ? m_cc[part].ac2 : m_cc[part].ac1) = value & 0x7f;
			refresh_lfo_depth(part);
			refresh_assign_pitch(part);
			refresh_assign_amp(part);
			refresh_assign_amod(part);
		}
	}


	// MIDI のコントロールチェンジの番号
	static constexpr int CC_MODULATION = 0x01, CC_PORTAMENTO_TIME = 0x05, CC_DATA_ENTRY = 0x06, CC_VOLUME = 0x07, CC_PAN = 0x0a, CC_EXPRESSION = 0x0b;
	static constexpr int CC_DAMPER = 0x40, CC_PORTAMENTO_SWITCH = 0x41, CC_SOSTENUTO = 0x42, CC_SOFT = 0x43, CC_RESONANCE = 0x47, CC_RELEASE = 0x48;
	static constexpr int CC_ATTACK = 0x49, CC_BRIGHTNESS = 0x4a, CC_DECAY = 0x4b, CC_PORTAMENTO_FROM = 0x54, CC_REVERB = 0x5b, CC_CHORUS = 0x5d;
	static constexpr int CC_VARIATION = 0x5e, CC_NRPN_LSB = 0x62, CC_NRPN_MSB = 0x63, CC_RPN_LSB = 0x64, CC_RPN_MSB = 0x65, CC_ALL_SOUND_OFF = 0x78;
	static constexpr int CC_RESET_CONTROLLERS = 0x79, CC_ALL_NOTES_OFF = 0x7b, CC_OMNI_OFF = 0x7c, CC_OMNI_ON = 0x7d, CC_MONO = 0x7e, CC_POLY = 0x7f;

	bool control(int part, int cc, int value)
	{
		if (part < 0 || part >= PARTS)
			return false;
		part_cc &p = m_cc[part];
		switch (cc) {
		case CC_VOLUME:
		case CC_EXPRESSION:
		case CC_PAN:
			break;
		case CC_ATTACK:                             // CC73 立ち上がり。鳴っている音も書き直す
			refresh_attack(part);
			break;
		case CC_DECAY:                               // CC75 減衰
		case CC_RELEASE:                             // CC72 離し
			break;
		case CC_MODULATION:
			// **割り当てのぶんを鳴っている音に効かせる**。
			// lfo は apply_cc が書き直すので、残りをここで
			refresh_lfo_depth(part);
			refresh_assign_pitch(part);
			refresh_assign_amp(part);
			refresh_assign_amod(part);
			break;
		case CC_REVERB:
		case CC_CHORUS:
		case CC_BRIGHTNESS:
		case CC_RESONANCE:
			break;
		// **モノ / ポリ**。モノのパートは、つぎの鍵を押すと
		// 前の音を**離す**（実機は古いスロットへ離しの release を書く）。
		// 入れるまでは前の音が鳴り続けて、重なったぶん 0.8dB 大きかった
		// **モノ・ポリの切り替えも鍵を離す**。MIDI の決めで
		// CC124-127 はどれもオールノートオフを兼ねる。
		// 実機で確かめたところ、4 つとも CC123 と同じだけ音が止まる
		case CC_MONO: p.mono = true; all_off(part, true); return true;
		case CC_POLY: p.mono = false; all_off(part, true); return true;
		case CC_PORTAMENTO_TIME: p.porta_time = value; return true;     // ポルタメントの速さ
		case CC_PORTAMENTO_SWITCH: p.porta_on = value >= 64; return true; // ポルタメント 入切
		case CC_PORTAMENTO_FROM: p.porta_src = value & 0x7f; return true;   // 滑り出す鍵を指定
		case CC_DAMPER:                             // ダンパー
		{
			const bool was = p.damper;
			p.damper = value >= 64;
			if (!p.damper)
				release_held(part);
			else if (!was)
				damper_catch(part);    // 離し中の音を拾う
			return true;
		}
		case CC_SOSTENUTO:                             // ソステヌート
			// ダンパーと違って、**踏んだ時点で鳴っている音だけ**を待たせる。
			// あとから押した鍵は普通に離れる
			if (value >= 64) {
				p.sost_on = true;
				for (int i = 0; i < SLOTS; i++) {
					slot_use &s2 = m_slot[i];
					if (s2.on && s2.part == part)
						s2.sost = true;
				}
			} else {
				p.sost_on = false;
				release_sost(part);
			}
			return true;
		// **CC121 コントローラリセット**。ベンド・モジュレーション・
		// エクスプレッション・ダンパー・ポルタメントを既定へ戻す。
		// 音量とパンと送りは**戻らない**（XG も MIDI もそう決まっている）。
		// 入れるまではベンドが残って、リセット後の音が半音ずれていた
		case CC_RESET_CONTROLLERS:
			p.bend = 8192;
			p.damper = false;
			p.sost_on = false;
			p.porta_on = false;
			p.porta_src = -1;
			release_held(part);
			release_sost(part);
			apply_bend(part);
			apply_cc(part);
			return false;
		// **CC67 ソフトペダル**。踏むと、このあと押す音の
		// フィルタの包絡線が「強さ - 32」の深さになる。
		// 鳴っている音はそのまま（実機も書き直さない）
		case CC_SOFT:
			p.soft = value >= 64;
			return false;
		// RPN と NRPN はエンジンが設定へ書く。割り当てのつまみとしては受けない
		case CC_NRPN_MSB:
		case CC_NRPN_LSB:
		case CC_RPN_MSB:
		case CC_RPN_LSB:
		case CC_DATA_ENTRY:
			return false;
		case CC_ALL_SOUND_OFF:                             // CC120 オールサウンドオフ
			all_off(part, true);
			return false;
		case CC_ALL_NOTES_OFF:                             // CC123 オールノートオフ
			all_off(part);
			return false;
		case CC_OMNI_OFF:                             // CC124 オムニオフ
		case CC_OMNI_ON:                             // CC125 オムニオン
			all_off(part);
			return false;
		default: {
			if (cc == CC_VARIATION)
				return false;
			// 知らない CC は AC1・AC2 に割り当てられているかもしれない
			assignable(part, cc, value);
			return false;
		}
		}
		apply_cc(part);
		return true;
	}

	void bend(int part, int value14)
	{
		if (part < 0 || part >= PARTS)
			return;
		m_cc[part].bend = value14;
		// （下で格子に乗せて流す）
		bend_assign(part);          // 割り当てはその場で
		if (m_eg_have) {
			const u64 due = eg_after(m_clock);
			m_bend_due[part] = due;
			if (due < m_bend_next)
				m_bend_next = due;
		} else {
			apply_bend(part);
		}
	}


private:
	// **離した音を追ってよいか**。
	// 実機は声が鳴り終わった時点でスロットを空け、以後は
	// 何も書かない。こちらだけが書き続けると、鳴り続けている
	// 尾にベンドや LFO が掛かって実機と全く違う音になる
	bool rel_follow(const slot_use &s, u64 now = ~u64(0)) const
	{
		if (s.on)
			return true;
		if (now == ~u64(0))
			now = m_clock;
		if (!s.rel || now - s.rel_at > REL_FOLLOW)
			return false;
		const int ch = int(&s - m_slot.data());
		// 実機がスロットを空けたら、そこで書くのをやめる
		if (m_slot_held && !m_slot_held(ch))
			return false;
		if (!m_slot_peek)
			return true;
		return m_slot_peek(ch);
	}

	// いま鳴っているスロットに、つまみの動きを反映する
	void apply_cc(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part != part)
				continue;
			// **離しの最中の音も追う**。release の下位は「素の減衰」で、
			// 坂の位置とは別に持たれている
			// （出る値は level + (glo & 0xff) << 6）。
			// つまり書き直しても坂は引き直しにならないので、安心して追える。
			// 追わないと、曲の終わりの CC7 のフェードアウトで離したばかりの
			// 長い音だけが元の音量のまま鳴り続ける
			if (!s.on) {
				// **ドラムには離しの段が無い**（要素を持たない）ので、
				// 追わずにそのまま鳴らしきらせる
				if (!s.elem || !rel_follow(s))
					continue;
				m_poke(i, word::release, release_of(s, part, s.keynote));
				continue;
			}
			m_poke(i, word::release, u16(note_att(s, part)));
			m_poke(i, word::pan_mix,
			       ins_routed(part) ? u16(0)
			       : s.rnd_pan < 0 ? exact_pan(s, part)
			                       : nv::pan_rnd_word(m_rom, s.rnd_pan));
			if (s.lfo)
				m_poke(i, word::lfo, lfo_word(s.lfo, part, s.keynote));
			m_poke(i, word::reverb_send, exact_send(s, part, false, s.reverb_base));
			m_poke(i, word::chorus_send,
			       ins_routed(part)
			       ? u16((exact_send(s, part, true, s.chorus_base) & 0xff00) | nv::ROUTED_CHORUS_LOW)
			       : exact_send(s, part, true, s.chorus_base));
			// **包絡線の刻みと同じ `cut_with_cc` で書く**
			if (s.cut)
				m_poke(i, word::lowpass_cut, cut_with_cc(s, s.cut));
			if (s.elem)
				m_poke(i, word::resonance, u16(u16(nv::reso_level(s.elem, s.fvel, res_knob(part))) << 11));
		}
	}

	void fenv_next(slot_use &s)
	{
		const u8 *e = s.elem;
		if (!e || !m_rom) {
			s.finc = 0;
			return;
		}
		s.facc = s.ftgt;
		if (s.fstage >= 9) {             // 離しの段は進めない
			s.finc = 0;
			return;
		}
		s.fstage++;
		const int adj = s.fadj;
		int rate = 0, lvl = -1;          // 速さは鍵の補正で負になることがある（0 に収める）
		if (s.fstage == 1) {
			if (e[nv::elem_at::filter_eg_attack_level] != e[nv::elem_at::filter_eg_first_level]) { rate = int(e[nv::elem_at::filter_eg_first_rate]) + adj; lvl = e[nv::elem_at::filter_eg_first_level]; }
			else                  s.fstage = 2;
		}
		if (lvl < 0 && s.fstage == 2) {
			// **段 2 の速さには EG ディケイ（CC75）が掛かる**。
			// 実機は段 1 には 0x40 を渡していて、そこだけ掛からない
			if (e[nv::elem_at::filter_eg_first_level] != e[nv::elem_at::filter_eg_second_level]) {
				rate = nv::fenv_rate_cc(m_rom, int(e[nv::elem_at::filter_eg_second_rate]), part_dec(s.part))
				     + adj;
				lvl = e[nv::elem_at::filter_eg_second_level];
			} else {
				s.fstage = 3;
			}
		}
		if (lvl < 0) {               // もう段が無い
			s.finc = 0;
			return;
		}
		if (rate < 0) rate = 0;
		if (rate > 63) rate = 63;
		s.ftgt = nv::fenv_target(m_rom, e, lvl, s.fdvel);
		s.finc = nv::fenv_inc(m_rom, rate);
		if (s.facc > s.ftgt && s.finc != nv::FENV_NEXT)
			s.finc = -s.finc;
	}

	// 立ち上がりの段の 1 目ぶんの増分。立ち上がりのつまみは押しているあいだも効く
	void fenv_attack_step(slot_use &s) const
	{
		const int atk = part_atk(s.part);
		if (nv::fenv_atk_instant(m_rom, s.elem, atk, s.keynote)) {
			s.finc = nv::FENV_NEXT;
			return;
		}
		const int rate = std::clamp(nv::fenv_atk_rate(m_rom, s.elem, atk) + s.fadj, 0, 62);
		s.finc = nv::fenv_inc(m_rom, rate);
		if (s.facc > s.ftgt && s.finc != nv::FENV_NEXT)
			s.finc = -s.finc;
	}

	// 鍵を押したときに包絡線を張る
	void fenv_start(slot_use &s, int vel)
	{
		if (!s.elem || !m_rom)
			return;
		s.fvel = vel;
		s.fdvel = nv::soft_vel(vel, m_cc[s.part].soft);
		const int kadj = nv::fenv_key_adj(s.elem, s.keynote);
		s.fadj = kadj + nv::fenv_vel_adj(s.elem, vel);
		s.ftgt = nv::fenv_target(m_rom, s.elem, s.elem[nv::elem_at::filter_eg_attack_level], s.fdvel);
		// **立ち上がりの段**。包絡線の立ち上がりの速さ が 63（即到達）なら段 0 の行き先から
		// 始まり、そうでなければ 包絡線の初めの高さ から 包絡線の立ち上がりの速さ の速さで登る。
		// **立ち上がりのつまみでこの速さも動く**。実機は
		// 「目盛り 63 以上」を**鍵の補正を足す前と足したあとの 2 回**見る
		const int atk = part_atk(s.part);
		s.facc = nv::fenv_init(m_rom, s.elem, s.fdvel, atk, s.keynote);
		s.finc = 0;
		s.fstage = 0;
		// **押鍵のときにもう段 0 の行き先に居るか**。
		// ここで段を進めた音は、押した直後の 1 目で値が動かない
		const bool at_tgt = (s.facc == s.ftgt);
		if (at_tgt) {
			fenv_next(s);
		} else {
			fenv_attack_step(s);
		}
		const u32 skip = at_tgt ? FENV_TICK : 0;
		s.fskip = skip;
		// **遅れて鳴る要素は、その要素が鳴り出してから数える**
		//。語は要素をまとめて先に書くが、実機の
		// 10ms 刻みは**その要素が実際に打たれてから**始まる。
		// 書いた時刻から数えると、1 目ぶん早く進んでいた
		const u64 dly = s.elem ? delayed_key_offset(s.elem) : 0;
		s.fnext = m_eg_have
		        ? slot_eg_after(u64(s64(s.tstart + dly) + EG_LAG), int(&s - m_slot.data())) + skip
		        : u64(s64(s.tstart + dly + FENV_TICK + skip) + EG_LAG);
	}

	// **サンプル＆ホールドの音程の1 刻み**。
	// 20ms ごとに刻みを削って、なくなったら乱数を引いて音程をずらす。
	// **押している間も離しの間も同じに回る**（止めると乱数の列がずれる）
	bool sh_tick(int i, u64 clock, u64 &next)
	{
		slot_use &s = m_slot[i];
		if (s.sh_cnt <= 0 || !s.elem)
			return false;
		bool drew = false;
		while (clock >= s.sh_next) {
			if (--s.sh_cnt <= 0) {
				s.sh_off = nv::sh_pitch_off(rnd_next(), s.elem);
				s.sh_cnt = nv::sh_ticks(s.elem);
				drew = true;
			}
			s.sh_next += 2 * FENV_TICK;
		}
		if (drew && s.wave)
			m_poke(i, word::pitch, pitch_of(s));
		if (s.sh_next < next)
			next = s.sh_next;
		return true;
	}

	// **フィルタ側 LFO の 1 刻み**。押しているあいだも
	// 離しのあいだも同じに回る。入れ忘れていたときは、離したとたん
	// 揺れが止まって、切る高さが古いぶんだけずれたまま固まっていた
	void lfo_tick(slot_use &s)
	{
		if (!s.lstep)
			return;
		s.lcut = nv::lfo_fcut(nv::lfo_fwave(s.lph, s.ltri), s.lfdep);
		if (s.lfdep)
			s.lph = nv::lfo_next(s.lph, s.lstep);
	}

	// **離しの段**。鍵を離すと、実機はもう 1 段張って 0 へ向かう。
	// 速さは 包絡線の離しの速さ、行き先は 包絡線の離しの高さ（段 1 が 包絡線の段 1 の速さ/包絡線の段 1 の高さ、
	// 段 2 が 包絡線の段 2 の速さ/包絡線の段 2 の高さ と並んでいるので、その次）。
	// 実測（GrandPno）で増分 -28 ＝ INC_TAB[13]、包絡線の離しの速さ(13) と一致
	void fenv_release(slot_use &s)
	{
		const u8 *e = s.elem;
		if (!e || !m_rom)
			return;
		// **離しの速さには EG リリース（CC72）が掛かる**
		int rate = nv::fenv_rate_cc(m_rom, int(e[nv::elem_at::filter_eg_release_rate]), part_rel(s.part))
		         + s.fadj;
		if (rate < 0) rate = 0;
		if (rate > 63) rate = 63;
		s.fstage = 9;                    // もう段を進めない印
		s.ftgt = nv::fenv_target(m_rom, e, e[nv::elem_at::filter_eg_release_level], s.fdvel);
		s.finc = nv::fenv_inc(m_rom, rate);
		if (s.facc > s.ftgt && s.finc != nv::FENV_NEXT)
			s.finc = -s.finc;
	}

	// 10ms ぶん進める
	void fenv_step(slot_use &s)
	{
		if (s.finc == nv::FENV_NEXT) {
			fenv_next(s);
			return;
		}
		if (!s.finc)
			return;
		s.facc += s.finc;
		if ((s.finc > 0 && s.facc >= s.ftgt) || (s.finc < 0 && s.facc <= s.ftgt))
			fenv_next(s);
	}

	u16 cut_with_cc(const slot_use &s, u16 base) const
	{
		const int add = s.lcut + assign_cut(s.part, s.keynote);
		return cut_plain(base, s.part, s.elem, s.fvel, add);
	}

	// 式で出した lowpass_cut に、明るさ（CC71）だけを足す
	// **明るさのつまみは頭打ちの前に効く**。`base` は頭打ちを
	// 掛けていない値を渡すこと
	u16 cut_plain(u16 base, int part, const u8 *elem, int vel,
	              int lfo = 0) const
	{
		const int now = part_bri(part);
		int v = int(base & 0xfff);
		if (now != 64)
			v += nv::bright_shift(now, gs_mode());
		// **頭は 0x7ff**。実機は0x800 の下騄を履いたまま
		// 0xfff で止めるので、こちらでは0x7ff で止めるのと同じ。
		// 0xfff で止めていたので、明るさを上げて共振も深いと（`cutoff_cap` が
		// 通らない）`0xa3f` のような 12 ビットの値を書いて、音が丸ごと壊れていた
		v = v < 0 ? 0 : (v > 0x7ff ? 0x7ff : v);
		if (lfo) {
			v += lfo;
			v = v <= 0 ? 1 : (v > 0x7ff ? 0x7ff : v);
		}
		return nv::cutoff_cap(u16((base & 0xf000) | u16(v)), elem, vel,
		                      res_knob(part));
	}

	u16 fenv_cut(const slot_use &s) const
	{
		return nv::cutoff_of(m_rom, s.elem, s.keynote, s.fvel, s.facc, false);
	}

	int assign_cents(int part, int note) const
	{
		if (part < 0 || part >= PARTS)
			return 0;
		const part_state &b = m_state->parts[part];
		const part_cc &c = m_cc[part];
		auto term = [](int d, int v) {
			if (d == 64 || !v)
				return 0;
			return int(s16(u16((u32(v) * u32(d - 64) * 0xc947u) >> 16)));
		};
		int sum = 0;
		sum += term(asn_byte(part, ASSIGN_WHEEL + TO_PITCH),
		            int(b.modulation));
		sum += term(asn_byte(part, ASSIGN_PRESSURE + TO_PITCH), c.chpress);
		sum += term(asn_byte(part, ASSIGN_AC1 + TO_PITCH), c.ac1);
		sum += term(asn_byte(part, ASSIGN_AC2 + TO_PITCH), c.ac2);
		if (note >= 36 && note < 98)
			sum += term(asn_byte(part, ASSIGN_KEY_PRESSURE + TO_PITCH),
			            int(m_pat[size_t(part)][size_t(note)]));
		return sum;
	}

	// 押したときの語（nv::build_note）と同じ式。鍵が 0-127 からはみ出したぶんも引く。
	// 引かないと、ベンドやサンプル＆ホールドで書き直したとたん、はみ出しの半音ぶん高くなった
	u16 pitch_of(const slot_use &s) const
	{
		const part_cc &pc = m_cc[s.part];
		return nv::pitch_word(nv::read_wave(s.wave), s.note - s.key_shift, nv::key_follow(m_rom, s.elem),
		                     s.key_shift * 100 + nv::bend_cents(pc.bend, bend_range(s.part)) + nv::elem_tune(s.elem)
		                     + part_fine_cents(s.part)
		                     + part_scale_cents(s.part, s.note) + nv::glide_cents(s.glide)
		                     + assign_cents(s.part, s.keynote) + s.sh_off
		                     - nv::note_overflow(m_rom, s.elem, s.note, s.key_shift) * 100,
		                     nv::key_pivot(s.elem));
	}

	void apply_bend(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part != part || !s.elem || !s.wave)
				continue;
			// **離している音も曲げる**。戻す時刻と
			// 離す時刻が重なると、曲がったまま鳴り続けていた
			if (!rel_follow(s))
				continue;
			m_poke(i, word::pitch, pitch_of(s));
		}
	}

	// **ベンドの割り当て（音程以外）はその場で**。
	// 実機は音程だけを 10ms の格子に乗せて、こちらはすぐに書く。
	// 切る高さはフィルタの 10ms の刻みが毎回 `assign_cut` を乗せるのでここには無い
	void bend_assign(int part)
	{
		if (bend_asn_plain(part))
			return;
		refresh_lfo_depth(part);
		refresh_assign_amp(part);
		refresh_assign_amod(part);
		refresh_pmod(part);
	}

	// つまみが動いたら、鳴っている音の**LFO の音程の深さ**を書き直す。
	// CC は `apply_cc` がやっているので、ここはベンド専用
	void refresh_pmod(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part != part || !s.lfo)
				continue;
			if (!rel_follow(s))
				continue;
			m_poke(i, word::lfo, lfo_word(s.lfo, part, s.keynote));
		}
	}

	// ベンドの割り当て（音程以外の 5 つ）が全部既定か
	bool bend_asn_plain(int part) const
	{
		if (part < 0 || part >= PARTS)
			return true;
		const u8 *b = &m_state->parts[part].values[ASSIGN_BEND];
		return b[TO_CUTOFF] == 64 && b[TO_LEVEL] == 64 && !b[TO_LFO_PITCH] && !b[TO_LFO_FILTER] && !b[TO_LFO_LEVEL];
	}

	// モノのパートで、いま鳴っている別の鍵を離す。
	// **付け替えの離しは 0xD9**（キーアサインがシングルのときと同じ）。
	// 音色の離しの速さを使っていたので、弦のような尾の長い
	// 音色でこちらだけ前の鍵が鳴り続けていた（48・0・80・10・56 の
	// 5 音色で確かめて、どれも 0xD9）
	// レガートの元になる、いま押さえている音を集める
	int collect_legato(int part, int note, legato_source *out) const
	{
		const part_cc &pc = m_cc[part];
		if (!pc.mono || !pc.porta_on || pc.porta_time <= 0 || !m_level_peek)
			return 0;
		int n = 0;
		for (int i = 0; i < SLOTS && n < LEGATO_SOURCES; i++) {
			const slot_use &s = m_slot[i];
			if (s.on && s.part == part && s.keynote != note && s.elem)
				out[n++] = { s.elem, m_level_peek(i), s.cut, s.facc, s.ftgt, s.finc, s.fstage, s.fadj, s.fnext };
		}
		return n;
	}

	void apply_legato(slot_use &su, nv::element_words &r, const legato_source &from, int part)
	{
		const int level = from.level / LEGATO_LEVEL_UNIT;
		const int goal = r.v[word::second_decay] & 0xff;
		if (goal >= LEGATO_GOAL_FADING) {
			// 消えていく音: 減衰 1 は飛ばす
			// 減衰 1 と 2 の速さが離れている音色は半分の勢い。前の音がほとんど消えていれば、そこで止まる
			const int rate_gap = su.elem ? int(su.elem[nv::elem_at::second_decay_rate]) - int(su.elem[nv::elem_at::first_decay_rate]) : 0;
			const bool slow = rate_gap >= LEGATO_SLOW_RATE_GAP;
			const int over = std::max(std::min(level, LEGATO_LEVEL_GONE) - LEGATO_LEVEL_FREE, 0);
			su.legato_att = level >= LEGATO_LEVEL_GONE ? (slow ? LEGATO_GONE_SLOW : LEGATO_GONE_FAST)
			                                           : (slow ? (over / 2 + 1) & ~1 : (over + 1) & ~1);
			r.set(word::first_decay, LEGATO_DECAY_SKIP);
			r.set(word::second_decay, u16((r.v[word::second_decay] & 0xff00) | LEGATO_GOAL_NEW));
		} else {
			// 伸びる音: 伸びの高さに着いていれば、その深さを土台へ移す。途中ならそのまま
			if (from.level >= goal * LEGATO_LEVEL_UNIT && goal > 0) {
				su.legato_att = goal;
				r.set(word::second_decay, u16(r.v[word::second_decay] & 0xff00));
			}
		}
		r.set(word::attack, LEGATO_ATTACK);
		su.attack_low = u8(LEGATO_ATTACK & 0xff);
		r.set(word::release, u16(note_att(su, part) & 0xff));
		// 切る高さの包絡線は前の音の続きから
		su.cut = from.cut;
		su.facc = from.facc;
		su.ftgt = from.ftgt;
		su.finc = from.finc;
		su.fstage = from.fstage;
		su.fadj = from.fadj;
		su.fnext = from.fnext;
		su.keeps_steps = true;
		r.set(word::lowpass_cut, cut_with_cc(su, su.cut));
	}

	void mono_cut(int part, int except)
	{
		int keys[SLOTS];
		int n = 0;
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (!s.on || s.part != part || s.keynote == except)
				continue;
			s.single_cut = true;
			bool seen = false;
			for (int k = 0; k < n; k++)
				if (keys[k] == s.keynote)
					seen = true;
			if (!seen)
				keys[n++] = s.keynote;
		}
		for (int k = 0; k < n; k++)
			note_off(part, keys[k], true);
	}

	// 実機は 0xD9 で離してから **1010 サンプル**（23ms）後に 0xF0 を書く。
	// 4 回の開閉でどれも同じ間隔だった
	static constexpr u64 ALT_KILL_DELAY = 1010;

	// 同じオルタネートグループで鳴っている打を止める
	int alt_cut(int part, int except, int grp)
	{
		int wrote = 0;
		int keys[SLOTS];
		int n = 0;
		for (int i = 0; i < SLOTS; i++) {
			const slot_use &s = m_slot[i];
			if (s.part != part || s.keynote == except)
				continue;
			// 離してから長い音は追わない（実機も書かない）
			if (!rel_follow(s))
				continue;
			if (s.alt_kill)
				continue;
			// **鳴り終わった打は切らない**。実機もそうで、切ると書き込みの
			// ぶん打鍵が 1 サンプル遅れてしまう
			if (m_slot_peek && !m_slot_peek(i))
				continue;
			if (drum_alt_group(part, s.keynote) != grp)
				continue;
			bool seen = false;
			for (int k = 0; k < n; k++)
				if (keys[k] == s.keynote)
					seen = true;
			if (!seen)
				keys[n++] = s.keynote;
		}
		for (int k = 0; k < n; k++)
			for (int i = 0; i < SLOTS; i++) {
				slot_use &s = m_slot[i];
				if (s.part != part || s.keynote != keys[k] || (!s.on && !s.rel))
					continue;
				if (s.alt_kill)        // もう切ってある打は二度切らない
					continue;
				if (m_slot_peek && !m_slot_peek(i))
					continue;
				s.alt_kill = m_clock + ALT_KILL_DELAY;
				if (s.alt_kill < m_alt_kill_next)
					m_alt_kill_next = s.alt_kill;
				s.single_cut = true;
				s.on = false;
				s.rel = true;
				s.rel_at = m_clock;
				// **ドラムは要素を持たない**ので、普通の離しの式は使えない。
				// 速さだけ与えて、音量はそのときの値にする
				m_poke(i, word::release,
				       s.elem ? release_of(s, part, s.keynote)
				              : u16(SINGLE_CUT_RATE | u16(note_att(s, part) & 0xff)));
				wrote++;
			}
		return wrote;
	}

	// ダンパーを離したとき、待たせていた音を切る
	void release_held(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.on && s.held && s.part == part) {
				s.held = false;
				note_off(part, s.keynote, true);
			} else if (s.caught && s.part == part && !s.on && s.rel && s.elem) {
				// **ペダルで拾っていた音を離し直す**
				s.caught = false;
				m_poke(i, word::release, release_of(s, part, s.keynote));
			}
		}
	}

	// ソステヌートを離したとき、待たせていた音を切る
	void release_sost(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.sost && s.part == part) {
				s.sost = false;
				if (s.on)
					note_off(part, s.keynote);
			}
		}
	}


	int note_att(const slot_use &s, int part) const
	{
		const int asn = assign_amp(part, s.keynote);
		if (s.sfx && s.lvl0 == 0)
			return nv::SILENT_ATT;                 // 打の音量で目盛りが 0（sfx_level）
		if (!m_rom)
			return nv::clamp_att(s.att + s.legato_att);
		const int vol = part_vol(part), expr = part_expr(part);
		if (s.lvl0 > 0)
			return nv::clamp_att(nv::volume_att_from(m_rom, s.lvl0, s.arest,
			                                         vol_gain_of(part, vol, expr),
			                                         asn) + m_global_att + s.legato_att);
		return nv::clamp_att(s.att + nv::gain_att(m_rom, nv::vol_gain(vol, expr))
		                     - nv::gain_att(m_rom, nv::VOL_GAIN_DEF));
	}

	int assign_pmod(int part, int note, int mod_now, int bend_now = -1) const
	{
		if (part < 0 || part >= PARTS)
			return 0;
		const part_cc &c = m_cc[part];
		auto term = [](int d, int v) { return (d && v) ? (d * v) / 128 : 0; };
		int sum = 0;
		sum += term(asn_byte(part, ASSIGN_WHEEL + TO_LFO_PITCH), mod_now < 0 ? 0 : mod_now);
		sum += term(asn_byte(part, ASSIGN_PRESSURE + TO_LFO_PITCH), c.chpress);
		sum += term(asn_byte(part, ASSIGN_AC1 + TO_LFO_PITCH), c.ac1);
		sum += term(asn_byte(part, ASSIGN_AC2 + TO_LFO_PITCH), c.ac2);
		if (note >= 36 && note < 98)
			sum += term(asn_byte(part, ASSIGN_KEY_PRESSURE + TO_LFO_PITCH),
			            int(m_pat[size_t(part)][size_t(note)]));
		const int pb = asn_byte(part, ASSIGN_BEND + TO_LFO_PITCH);
		if (pb) {
			int v = bend_now < 0 ? c.bend : bend_now;
			if (v < 0)
				v += 63;
			v = (v >> 6) - 128;
			if (v)
				sum += (v < 0 ? -v : v) * pb / 128;
		}
		return sum;
	}

	// **つまみの割り当て「LFO の音量」の合計**。
	// どのつまみも `値 × 深さ / 1024`。PAT だけは鍵ごと
	int assign_amod(int part, int note) const
	{
		if (part < 0 || part >= PARTS)
			return 0;
		const part_state &b = m_state->parts[part];
		const part_cc &c = m_cc[part];
		auto term = [](int d, int v) { return (d && v) ? (d * v) / 1024 : 0; };
		int sum = 0;
		sum += term(asn_byte(part, ASSIGN_WHEEL + TO_LFO_LEVEL),
		            int(b.modulation));
		sum += term(asn_byte(part, ASSIGN_PRESSURE + TO_LFO_LEVEL), c.chpress);
		sum += term(asn_byte(part, ASSIGN_AC1 + TO_LFO_LEVEL), c.ac1);
		sum += term(asn_byte(part, ASSIGN_AC2 + TO_LFO_LEVEL), c.ac2);
		if (note >= 36 && note < 98)
			sum += term(asn_byte(part, ASSIGN_KEY_PRESSURE + TO_LFO_LEVEL),
			            int(m_pat[size_t(part)][size_t(note)]));
		const int pb = s8(u8(asn_byte(part, ASSIGN_BEND + TO_LFO_LEVEL)));
		if (pb) {
			int v = c.bend - nv::BEND_CENTER;
			if (v < 0)
				v += 0xff;
			v >>= 8;
			if (v < 0)
				v = -v;
			sum += (pb * v) / 256;
		}
		return sum;
	}

	// つまみが動いたら、鳴っている音の**LFO の音量**を書き直す
	void refresh_assign_amod(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part != part || !s.elem)
				continue;
			// 離してから長い音は追わない（実機も書かない）
			if (!rel_follow(s))
				continue;
			const int base = s.lrun ? (s.vamp / 2) : 0;
			m_poke(i, word::amplitude_lfo,
			       u16(nv::MOVING_LEVEL_LFO | u16(nv::amod_word(assign_amod(part, s.keynote),
			                                     base))));
		}
	}

	u16 lfo_word(u16 base, int part, int note = 60) const
	{
		if (!m_rom)
			return base;
		const int now = part_mod(part);
		const int sum = assign_pmod(part, note, now);
		const bool xg_curve = !gs_mode();    // TG300B（GS）モードは別の曲線（CC1 を 128 段測って一致）
		const int was = nv::pmod_word(m_rom, assign_pmod(part, note, 0, nv::BEND_CENTER), xg_curve);
		int own = int(base & 0xff);
		if (was > 0 && own == was)
			own = 0;
		const int wheel = nv::pmod_word(m_rom, sum, xg_curve);
		return u16((base & 0xff00) | (own > wheel ? own : wheel));
	}

public:
	int m_global_att = 0;
	std::function<int(int)> m_level_peek;
	static constexpr int LEGATO_SOURCES = 8, LEGATO_LEVEL_UNIT = 64, LEGATO_LEVEL_FREE = 15, LEGATO_LEVEL_GONE = 128, LEGATO_GONE_SLOW = 54, LEGATO_GONE_FAST = 182, LEGATO_SLOW_RATE_GAP = 3, LEGATO_GOAL_FADING = 0xfe;
	static constexpr int LEGATO_SILENT = 16383;
	static constexpr u16 LEGATO_ATTACK = 0x3a7e, LEGATO_DECAY_SKIP = 0x7800;
	static constexpr int LEGATO_GOAL_NEW = 0xfc;
	static constexpr u64 METER_TAIL = 44100 / 4;

	int lfo_fdep_extra(int part, int note) const
	{
		if (part < 0 || part >= PARTS)
			return 0;
		const part_state &b = m_state->parts[part];
		const part_cc &c = m_cc[part];
		auto term = [](int d, int v) { return (d && v) ? (d * v) / 1024 : 0; };
		int sum = 0;
		sum += term(s8(u8(asn_byte(part, ASSIGN_WHEEL + TO_LFO_FILTER))),
		            int(b.modulation));
		sum += term(s8(u8(asn_byte(part, ASSIGN_PRESSURE + TO_LFO_FILTER))), c.chpress);
		sum += term(s8(u8(asn_byte(part, ASSIGN_AC1 + TO_LFO_FILTER))), c.ac1);
		sum += term(s8(u8(asn_byte(part, ASSIGN_AC2 + TO_LFO_FILTER))), c.ac2);
		// **鍵ごとの PAT**。鍵 36-97 の 62 鍵ぶんの値を深さと掛ける
		if (note >= 36 && note < 98)
			sum += term(s8(u8(asn_byte(part, ASSIGN_KEY_PRESSURE + TO_LFO_FILTER))),
			            int(m_pat[size_t(part)][size_t(note)]));
		const int pb = s8(u8(asn_byte(part, ASSIGN_BEND + TO_LFO_FILTER)));
		if (pb) {
			int v = c.bend - nv::BEND_CENTER;
			if (v < 0)
				v += 0xff;
			v >>= 8;
			if (v < 0)
				v = -v;
			sum += (pb * v) / 256;
		}
		return sum;
	}

	// つまみが動いたら、鳴っている音の**音量**を書き直す
	void refresh_assign_amp(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part != part || !s.elem)
				continue;
			if (s.on)
				m_poke(i, word::release, u16(note_att(s, part)));
			else if (rel_follow(s))
				m_poke(i, word::release, release_of(s, part, s.keynote));
		}
	}

	// つまみが動いたら、鳴っている音の**音程**を書き直す
	void refresh_assign_pitch(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part != part || !s.elem || !s.wave)
				continue;
			// **離した音も追う**。押している音だけにすると
			// 割り当てを戻すところの残差が 16.9% → 47.9% に悪くなった。
			// 実機も離しの最中は音程を見直しているらしい
			if (!rel_follow(s))
				continue;
			s.pdirty = true;
			m_traj = true;
			m_traj_next = 0;
		}
	}

	// つまみが動いたら、鳴っている音の深さを作り直す
	void refresh_lfo_depth(int part)
	{
		if (!m_rom || part < 0 || part >= PARTS)
			return;
		for (slot_use &s : m_slot) {
			if (s.part != part || !s.elem || !s.lstep)
				continue;
			// 離してから長い音は追わない（実機も書かない）
			if (!rel_follow(s))
				continue;
			s.lfull = nv::lfo_fdepth(m_rom, s.elem,
			                         lfo_fdep_extra(part, s.note));
			if (s.lrun)
				s.lfdep = s.lfull;
		}
	}

	// **鍵ごとのアフタータッチ**。値を覚えて、
	// 鳴っている音のフィルタ側 LFO の深さを作り直す
	void poly_at(int part, int note, int value)
	{
		if (part < 0 || part >= PARTS || note < 0 || note > 127)
			return;
		m_pat[size_t(part)][size_t(note)] = u8(value & 0x7f);
		refresh_lfo_depth(part);
		refresh_assign_pitch(part);
		refresh_assign_amp(part);
		refresh_assign_amod(part);
	}

	// **チャンネルアフタータッチ**
	void chan_press(int part, int value)
	{
		if (part < 0 || part >= PARTS)
			return;
		m_cc[part].chpress = value & 0x7f;
		refresh_lfo_depth(part);
		refresh_assign_pitch(part);
		refresh_assign_amp(part);
		refresh_assign_amod(part);
	}

	int part_meter(int part, int vel) const { return meter_of(part, vel); }

	void fill_meter(u8 *dst, int n) const
	{
		for (int i = 0; i < n; i++)
			dst[i] = 0;
		if (!m_rom)
			return;
		for (const slot_use &s : m_slot) {
			if (s.part < 0 || s.part >= n || s.vel <= 0)
				continue;
			if (!s.on && !(s.rel && m_clock - s.rel_at < METER_TAIL))
				continue;
			const int v = meter_of(s.part, s.vel);
			if (v > int(dst[s.part]))
				dst[s.part] = u8(v);
		}
	}

private:
	// 目盛り = 強さ x パートの目盛り / 128。
	// **パートの目盛りは設定から取る**（gain。音量・
	// エクスプレッション・マスター音量・インサーションの損まで畳んである）。実機との差は 1 以内（実測 15 通り）
	int meter_of(int part, int vel) const
	{
		const int g = int(m_state->parts[part].gain) - 1;
		if (g <= 0)
			return 0;
		const int v = (vel * g) >> 7;
		return v > 127 ? 127 : v;
	}

	// **Rnd（パン 0）かどうか**。パートのパンの値がそのまま 0 のとき
	bool pan_is_rnd(int part) const { return part_pan(part) == 0; }

	static constexpr int RND_MULTIPLIER = 0xb3, RND_INCREMENT = 0x11;

	int rnd_next()
	{
		u8 &x = m_state->pan_random;
		x = u8(RND_MULTIPLIER * x + RND_INCREMENT);
		return int(x);
	}

	// パンの Rnd。要素 1 つにつき 1 回進む
	int pan_rnd_draw() { return rnd_next() >> 1; }

	bool ins_routed(int part) const { return ins_target(part) >= 0; }

	// **どのエフェクトに挿さっているか**。0 がバリエーション（インサーション接続）、
	// 1-4 がインサーション 1-4、-1 は通らない
	int ins_target(int part) const
	{
		if (part < 0 || part >= PARTS)
			return -1;
		// 同じパートに重なったら若い番号のインサーションが先。バリエーションはその後
		for (int n = 0; n < 4; n++)
			if (int(m_state->insertion[n][settings::XG_INSERTION_PART]) == part)
				return n + 1;
		if (m_state->effect[settings::XG_VARIATION_CONNECTION] == 0
		    && int(m_state->effect[settings::XG_VARIATION_PART]) == part)
			return 0;
		return -1;
	}

	// 声の出口の語。どのエフェクトの入口へ送るか
	static constexpr u16 ROUTE_VARIATION = 0x4000;
	static constexpr u16 ROUTE_INSERTION[4] = { 0x1000, 0x0001, 0x0002, 0x0004 };

	void ins_mixer(nv::element_words &r, int pan_pos, int target) const
	{
		static constexpr u16 ROUTE[5] = { ROUTE_VARIATION, ROUTE_INSERTION[0], ROUTE_INSERTION[1], ROUTE_INSERTION[2], ROUTE_INSERTION[3] };
		// **パンは音色（打）自身の分だけ残る**。
		// 真ん中の音色なら 0 なので、lofi・ins2 では見えていなかった
		r.set(word::pan_mix, nv::ins_pan_word(m_rom, pan_pos));
		r.set(word::chorus_send, u16((r.v[word::chorus_send] & 0xff00) | nv::ROUTED_CHORUS_LOW));
		const u16 v = ROUTE[target < 0 || target > 4 ? 0 : target];
		r.set(word::route_first, v);
		r.set(word::route_second, v);
		r.set(word::route_third, v);
	}

	u16 exact_pan(const slot_use &s, int part) const
	{
		const int q = part_pan(part);
		if (s.elem && s.sfx)
			return nv::voice_pan_word(m_rom, s.elem, s.note, sfx_pan(part, s.keynote), q);
		if (s.elem)
			return nv::voice_pan_word(m_rom, s.elem, s.note, 64, q);
		const int dp = drum_setup_of(part, s.keynote, settings::drum_param::pan);
		return nv::drum_pan_word(m_rom, dp < 0 ? 64 : dp, q);
	}

	// 送り（reverb_send・chorus_send）。上位はそのまま、下位を組み直す
	u16 exact_send(const slot_use &s, int part, bool cho, u16 base) const
	{
		const int now = cho ? part_cho(part) : part_rev(part);
		int extra = 127;
		int pan = 64;
		if (s.elem && s.sfx) {
			const int d = drum_setup_of(part, s.keynote, cho ? settings::drum_param::chorus : settings::drum_param::reverb);
			extra = d < 0 ? 127 : d;
			pan = nv::voice_pan_pos(m_rom, s.elem, s.note, sfx_pan(part, s.keynote));
		} else if (s.elem) {
			pan = nv::voice_pan_pos(m_rom, s.elem, s.note);
		} else {
			const int d = drum_setup_of(part, s.keynote, cho ? settings::drum_param::chorus : settings::drum_param::reverb);
			const int dp = drum_setup_of(part, s.keynote, settings::drum_param::pan);
			extra = d < 0 ? 127 : d;
			pan = dp < 0 ? 64 : dp;
		}
		// **パンが Rnd のときは、送りの表を位置 0 で引く**。
		// 当たった位置ではなく 0 なので、音色（打）の寄りは効かない
		if (s.rnd_pan >= 0)
			pan = 0;
		// **リバーブは reverb_send の下位、コーラスは chorus_send の上位**（`send_word` と同じ）。
		// ここを下位で書いていたので、コーラスを使う曲で送りが丸ごと狂っていた
		const int v = nv::send_level_att(m_rom, now,
		                                 extra, pan);
		if (!cho)
			return u16(u16(dry_att(part, base)) << 8 | u16(v));
		return u16(u16(v) << 8 | u16(var_send_att(s, part, base)));
	}

	// reverb_send の上位（直の減衰）。**バリエーションがシステム接続のときだけ**、
	// パートの Dry Level（08 pp 11）を送りの表で足す。127 でそのまま、64 で 16、0 で切れる。
	// ほかの接続では Dry Level は効かない
	u8 dry_att(int part, u16 base) const
	{
		const int full = base >> 8;
		if (m_state->effect[settings::XG_VARIATION_CONNECTION] != 1)
			return u8(full);
		const int v = full + nv::send_att(m_rom, part_setting(part, settings::PART_DRY));
		return u8(v > 255 ? 255 : v);
	}

	// chorus_send の下位（バリエーションへの送り）。システム接続のときだけ組む。
	// インサーション接続のパート（ins_mixer）と、ほかの接続のときは base のまま
	u8 var_send_att(const slot_use &s, int part, u16 base) const
	{
		if (m_state->effect[settings::XG_VARIATION_CONNECTION] != 1)
			return u8(base & 0xff);
		const int now = part_var(part);
		// **XG のモードでは、バリエーションへの送りにパンの目減りが掛からない**（リバーブ・コーラスの送りとは違う）。
		// 要素にパンのある音色でも、パンが Rnd でも中央と同じ値を送る。目減りを掛けると 6 dB まで多く送っていた。
		// パフォーマンスのモードでは、リバーブと同じく音色の位置の目減りが掛かり、Rnd は位置 0 で引く
		if (s.elem && !s.sfx)
			return u8(nv::send_level_att(m_rom, now, 127, !m_performance ? 64 : s.rnd_pan >= 0 ? 0 : nv::voice_pan_pos(m_rom, s.elem, s.note)));
		const int d = drum_setup_of(part, s.keynote, settings::drum_param::variation);
		const int ds = d < 0 ? 127 : d;
		const int eff = (now * ds) >> 7;
		const int v = 16 + nv::send_att(m_rom, eff);
		return u8(v > 255 ? 255 : v);
	}


public:
	// **キーアサインがシングルか**（XG の 08 pp 06。0 がシングル、1 がマルチ）
	bool key_assign_single(int part) const
	{
		return part >= 0 && part < PARTS
		    && m_state->parts[part][settings::PART_SAME_NOTE] == 0;
	}

	// そのパートが使うドラムの組（0-3）。パートモードから決まる
	int drum_set_of(int part) const
	{
		if (part < 0 || part >= PARTS)
			return -1;
		const int mode = int(m_state->parts[part][settings::PART_MODE]);
		const int set = mode >= 2 ? mode - 2 : -1;
		return set < settings::DRUM_SETUP_SETS ? set : -1;
	}

	int drum_live(int part, int note, int addr) const
	{
		const int set = drum_set_of(part);
		const int idx = settings::drum_setup_index(addr);
		if (idx < 0 || note < 0 || note > 127)
			return 64;
		if (set < 0)
			return nv::drum_setup_kit_default(m_rom, drum_rec_of(part, note), idx);
		if (note < settings::DRUM_SETUP_NOTE0
		    || note >= settings::DRUM_SETUP_NOTE0 + int(settings::DRUM_SETUP_NOTES))
			return settings::drum_setup_default(idx);
		return int(m_state->drum_setup(set, note, idx));
	}


	void sfx_setup_elem(int part, int note, u8 *e) const
	{
		nv::sfx_setup_apply(e, drum_live(part, note, settings::drum_param::cutoff), drum_live(part, note, settings::drum_param::resonance),
		                    drum_live(part, note, settings::drum_param::attack), drum_live(part, note, settings::drum_param::first_decay),
		                    drum_live(part, note, settings::drum_param::second_decay));
	}
	// 粗調（3n rr 00）は**鍵をずらす**。組む鍵は 64 のままなので、要素の追従
	// （SFX の音色はほとんど追従しない。SFXKit1 の鍵 36 は 1 半音で 5 セント）ぶんだけ動く
	int sfx_pitch_note(int part, int note, int pnote) const
	{
		// TG300B（GS）モードは音程を組む鍵を 1 打の記録が持つ（XG は 64 のまま）
		const u8 *hit = gs_mode() ? nv::drum_record(m_rom, int(m_state->parts[part].kit), note) : nullptr;
		const int co = drum_setup_of(part, note, settings::drum_param::coarse);
		return std::clamp((hit ? int(hit[nv::drum_at::sfx_pitch_key]) : pnote) + (co < 0 ? 0 : co - 64), 0, 127);
	}
	// 打のパン（3n rr 04）。音色のパンに CC10 の代わりに足す
	int sfx_pan(int part, int note) const
	{
		const int dp = drum_setup_of(part, note, settings::drum_param::pan);
		return dp <= 0 ? 64 : dp;
	}
	int sfx_level(int part, int note, int level) const
	{
		if (note < settings::DRUM_SETUP_NOTE0 || note >= settings::DRUM_SETUP_NOTE0 + int(settings::DRUM_SETUP_NOTES))
			return level;
		return (level * (drum_live(part, note, settings::drum_param::level) + 1)) >> 7;
	}
	// 打のパン（3n rr 04）が 0 ＝ Rnd か
	bool drum_pan_rnd(int part, int note) const { return drum_setup_of(part, note, settings::drum_param::pan) == 0; }

	// **ドラムセットアップの値**（3n rr pp）。組はパートモードから決まる
	int drum_setup_of(int part, int note, int param) const
	{
		if (note < settings::DRUM_SETUP_NOTE0
		    || note >= settings::DRUM_SETUP_NOTE0 + int(settings::DRUM_SETUP_NOTES))
			return -1;
		const int set = drum_set_of(part);
		if (set < 0)
			return nv::drum_setup_kit_default(m_rom, drum_rec_of(part, note), param);
		return int(m_state->drum_setup(set, note, u32(param)));
	}

	// **その打が離しを受けるか**（3n rr 09）。既定は 0 ＝ 受けない
	// （打ったら鳴りきる）。1 なら離しで止める。実機の離しの速さは
	// 音色によらず 0xCF（鍵 49・38・46・51 で確かめた）
	bool drum_rcv_note_off(int part, int note) const
	{ return drum_setup_of(part, note, settings::drum_param::receive_note_off) > 0; }

	// **その打が押しを受けるか**（3n rr 0A）。既定は 1。0 なら鳴らさない
	bool drum_rcv_note_on(int part, int note) const
	{ return drum_setup_of(part, note, settings::drum_param::receive_note_on) != 0; }

	static constexpr u16 DRUM_OFF_RATE = 0xcf00;

	// **その打のオルタネートグループ**（ドラムセットアップの 3n rr 03）。
	// 0 は「組なし」。同じ組の打は互いを止める（ハイハットの開閉など）
	int drum_alt_group(int part, int note) const
	{
		if (note < settings::DRUM_SETUP_NOTE0
		    || note >= settings::DRUM_SETUP_NOTE0 + int(settings::DRUM_SETUP_NOTES))
			return 0;
		// **組はパートモードから**（08 pp 07）。2-5 が DRUMS1-4 で、
		// それぞれドラムセットアップの組 0-3 にあたる。
		// キットの番号は音色の記録番号で、組ではない
		const int v = drum_setup_of(part, note, settings::drum_param::alternate_group);
		return v < 0 ? 0 : v;
	}

	// **鍵の範囲の中か**（XG の 08 pp 0F 下限・10 上限）。実機は範囲の外の
	// 鍵を鳴らさない。ここを見ていないと、**実機が黙っている所で音が出る**。
	// 下限 > 上限のときは「外側」が鳴る（XG の決まり）
	bool note_in_range(int part, int note) const
	{
		if (part < 0 || part >= PARTS)
			return true;
		const part_state &b = m_state->parts[part];
		const int lo = int(b[settings::PART_NOTE_LOW]), hi = int(b[settings::PART_NOTE_HIGH]);
		return lo <= hi ? (note >= lo && note <= hi) : (note <= hi || note >= lo);
	}

	bool note_on(int part, int note, int vel)
	{
		if (is_drum(part))
			return drum_on(part, note, vel);
		return note_on_rec(part, note, vel, record_of(part), -1);
	}

	// **記録を指定して押す**。fixed_note が 0 以上なら、要素の選択・波形・音程・
	// 鍵の曲線を**ぜんぶその鍵として**組む（SFX の打。実機は鍵 64 で組む）。
	// 押した鍵は離すときの照合（keynote）にだけ残る
	bool note_on_rec(int part, int note, int vel, u32 rec, int fixed_note)
	{
		if (!rec || !m_rom)
			return false;
		legato_source legato[LEGATO_SOURCES];
		const int legato_count = collect_legato(part, note, legato);
		// **モノなら前の音を離す**
		if (m_cc[part].mono)
			mono_cut(part, note);
		// **キーアサインがシングルなら、同じ鍵の前の音を離す**（08 pp 06）。
		// マルチ（既定）は重ねる。余韻の長い音色で同じ鍵を続けて押すと差が出る
		else if (key_assign_single(part)) {
			for (slot_use &s : m_slot)
				if (s.on && s.part == part && s.keynote == note)
					s.single_cut = true;
			note_off(part, note, true);
		}
		++m_inst;                        // この押しの番号
		const int nelem = nv::element_count(m_rom, rec);
		// **ノートシフト**（08 pp 08）。要素の鍵の範囲は押した鍵で見て、波形と音程は
		// 鍵の追従のあとでずらす。離すときの照合は押した鍵
		const int sh = part_shift(part);
		const int pn0 = note + sh;
		const int pnote = fixed_note >= 0 ? fixed_note
		                : pn0 < 0 ? 0 : (pn0 > 127 ? 127 : pn0);
		// **鍵の曲線を引く鍵**（音量・切る高さ・包絡線の速さ）。ふつうは押した鍵
		// だが、SFX の打は組む鍵（64）で引く
		const int knote = fixed_note >= 0 ? fixed_note : note;
		// **ベロシティ感度**（08 pp 0C・0D）。これも音色を選ぶ前に掛かる
		const int pvel = part_vel(part, vel);
		slot_bits keymask;
		bool any = false;
		int nwrote = 0;                  // 語を書いた要素の数
		// スロット → 鍵の遅れ の遅れ。1 音の要素の数だけ。音を鳴らす道で確保しないよう固定の領域に置く
		small_pend pend;
		// **滑る音は、押した鍵と滑り出す鍵の「高いほう」で波形を選ぶ**。
		// 多段サンプルは鍵の上限で選ぶので、滑る範囲のいちばん高い所を
		// 通せる記録でないと足りない。CC84 で 48 から 72 へ滑るときは 72、
		// 84 から 60 へ滑るときは 84 の波形を実機が使っていた
		const int gsrc = m_cc[part].porta_src;
		const int gs = gsrc < 0 ? -1
		             : std::min(127, std::max(0, gsrc + part_shift(part)));
		const int wnote = fixed_note >= 0 ? fixed_note : gs > pnote ? gs : pnote;
		const int key_shift = fixed_note >= 0 ? 0 : pnote - note;
		for (int k = 0; k < nelem; k++) {
			const u8 *el = nv::element(m_rom, rec, k);
			if (!nv::element_active(el, pnote - key_shift, pvel))
				continue;
			const u8 *we = nv::wave_entry(m_rom, nv::wave_set(el), nv::wave_note(m_rom, el, wnote, key_shift));
			const int slot = take_slot(part, note);
			if (slot < 0)
				break;
			slot_use &su = m_slot[slot];
			su.elem = el;
			su.sfx = fixed_note >= 0;        // SFX の打は離しがドラムの決まり
			if (su.sfx) {
				std::memcpy(su.sfx_el.data(), el, su.sfx_el.size());
				sfx_setup_elem(part, note, su.sfx_el.data());
				el = su.sfx_el.data();
				su.elem = el;
			}
			su.wave = we;
			su.tstart = m_clock;
			su.keeps_steps = false;
			su.note = pnote;                 // 鳴らす鍵（移調ぶんを足したもの）
			su.key_shift = key_shift;
			fenv_start(su, pvel);
			// **サンプル＆ホールドの音程**。初めの刻みは 4、
			// そのあとは `0x40 - LFO の速さ` ごとに乱数を引く
			su.sh_off = 0;
			su.sh_cnt = 0;
			su.sh_next = ~u64(0);
			if (el && nv::sh_lfo(el)) {
				su.sh_cnt  = 4;
				su.sh_next = su.fnext + FENV_TICK;
			}
			// **LFO の初めの位相も乱数**（LFO の位相合わせ が 0 の要素だけ）。
			// 引く回数を実機に合わせるので、位相を使わなくても引く
			if (el && nv::lfo_rnd_phase(el))
				su.lph = u32(rnd_next()) & 0x7fff;
			m_traj = true;
			m_traj_next = 0;       // つぎの tick で見直す
			// **鍵の曲線は押した鍵で引く**。波形の段の分
			// （volume_rest の wave_level）だけが波形を選んだ鍵（CC84 の滑り出しも含む）に付いていく
			su.lvl0  = nv::volume_level(m_rom, rec, el, knote);
			if (su.sfx)
				su.lvl0 = sfx_level(part, note, su.lvl0);
			const bool sfx_mute = su.sfx && su.lvl0 == 0;   // 打の音量で目盛りが 0 になったら鳴らさない
			su.arest = nv::volume_rest(m_rom, el, wnote, pvel, key_shift);
			su.att   = nv::clamp_att(nv::volume_att_from(
			    m_rom, su.lvl0, su.arest,
			    vol_gain_of(part,
			                part_vol(part),
			                part_expr(part)),
			    assign_amp(part, note)) + m_global_att);
			// 実機は目盛りが 0 なら減衰をいっぱい（0xFE）にする（SFXKit1 などの音量 0 で確かめた）
			if (sfx_mute)
				su.att = nv::SILENT_ATT;
			const part_cc &pc = m_cc[part];
			// **ポルタメント**。前の鍵（CC84 があればその鍵）の音程で
			// 鳴らし始めて、10ms ごとに寄せていく。残りのずれはセント × 256 で持つ。
			// 追従を掛けるのは、鍵 1 つぶんの音程がその要素の追従で決まるから
			su.glide = 0;
			su.glide_step = 0;
			const int src = pc.porta_src >= 0 ? pc.porta_src : pc.last;
			if (pc.porta_on && pc.porta_time > 0 && src >= 0 && src != note) {
				su.glide_step = nv::porta_step(m_rom, pc.porta_time);
				if (su.glide_step > 0) {
					su.glide = (src - note) * nv::key_follow(m_rom, el) * 256;
					su.glide_next = m_eg_have
					              ? slot_eg_after(u64(s64(m_clock) + EG_LAG), slot)
					              : (su.fnext > nv::PORTA_TICK
					                 ? su.fnext - nv::PORTA_TICK
					                 : (m_clock / nv::PORTA_TICK + 1) * nv::PORTA_TICK);
				}
			}
			// **移調した鍵と、感度を掛けた強さで組む**。ここに元の鍵を
			// 渡していたので、ノートシフトやマスター移調が音程・波形に効かなかった
			m_peg_flag = el[nv::elem_at::lfo_phase_reset] ? nv::LFO_RESET_FLAG : 0;
			nv::element_words sr = nv::build_note(m_rom, el, su.sfx ? sfx_pitch_note(part, note, pnote) : pnote,
			                                  note_att(su, part),
			                                  nv::defaults(),
			                                  nv::bend_cents(pc.bend, bend_range(part))
			                                  + part_fine_cents(part)
			                                  + (su.sfx ? drum_live(part, note, settings::drum_param::fine) - 64 : 0)
		                                  + part_scale_cents(part, pnote) + nv::glide_cents(su.glide)
			                                  + assign_cents(part, note),
			                                  pvel, part_atk(part), part_dec(part),
			                                  part_vrate(part), part_vdep(part), wnote, knote,
			                                  pc.soft, part_setting(part, settings::PART_PITCH_EG_INITIAL), part_setting(part, settings::PART_PITCH_EG_ATTACK), gs_program(part), key_shift);
			su.attack_low = u8(sr.v[word::attack]);
			// SFX の打はドラムと同じく打ごとの EQ（パートの EQ は掛からない）
			if (su.sfx)
				apply_drum_eq(sr, part, note);
			else
				apply_part_eq(sr, part);
			// 音程の包絡線の行き先（音程の包絡線の段 0 の高さ）。初めの高さと同じなら書かない
			{
				const u16 tgt = nv::peg_word(m_rom, nv::peg_cents(el, el[nv::elem_at::pitch_eg_levels], pvel), el);
				su.peg_tgt = tgt == sr.v[word::pitch_env_target] ? nv::NO_PITCH_TARGET : tgt;
			}
			// **遅れて掛かるビブラート**。遅れのあと
			// 20ms ごとに深さをせり上げる。lfo の上位（型と刻み）は
			// 押した瞬間のまま使い回す
			su.vcnt = su.vtgt = su.vdly = su.vcnt2 = 0;
			su.vramp = false;
			su.vnext = ~u64(0);
			su.vamp = 0;
			su.vamp_waiting = false;
			// **フィルタ側の LFO**。鍵を押すと
			// 位相は 0 に戻り、その場で 1 歩進む
			su.lstep = nv::lfo_step(m_rom, nv::vib_rate(int(el[nv::elem_at::lfo_rate] & 0x3f), part_vrate(part), m_rom, gs_program(part)));
			su.ltri  = el[nv::elem_at::lfo_wave] != 0;
			su.lph   = su.lstep;
			su.lfull = su.lstep
			         ? nv::lfo_fdepth(m_rom, el, lfo_fdep_extra(part, pnote))
			         : 0;
			su.lrun  = true;             // 遅れが無ければすぐ回り出す
			su.lfdep = su.lfull;
			su.lcut  = 0;
			// **遅れのつまみ**（08 pp 17）。速さ・深さと同じ
			// 「下は小さいほう・上は大きいほう」。つまみで遅れが付くと、
			// **自身はせり上がらない音色でも遅れて掛かるようになる**
			// （GrandPno に遅れ 100 を与えると、深さ 0 で始めて 845ms から
			//  20ms ごとにせり上げ、1066ms で元の深さに着く。Vibes の音量の LFO も同じ）
			const int vdly_t = nv::vib_delay(nv::vib_delay_ticks(el), part_vdly(part), m_rom, gs_program(part));
			if (nv::vib_ramps(el) || vdly_t > 0) {
				su.vtgt  = nv::vib_ramp_target(el);
				su.vstep = nv::vib_ramp_step(el);
				su.vdly  = vdly_t;
				if (su.vdly > 0) {
					su.lfdep = 0;        // 遅れのあいだは掛からない
					su.lrun  = false;    // 位相も止めておく
					// せり上がらない音色は押鍵の値に深さが入っている。
					// 遅れを付けたのだから、そこは 0 から始める
					sr.set(word::lfo, u16(sr.v[word::lfo] & 0xff00));
					sr.set(word::amplitude_lfo, u16(sr.v[word::amplitude_lfo] & 0xff00));
				}
				su.vhi   = u16(sr.v[word::lfo] & 0xff00);
				su.ahi   = u16(sr.v[word::amplitude_lfo] & 0xff00);
				su.vamp  = nv::vib_amp_depth(el);
				su.vamp_waiting = su.vamp > 0;
				su.vdep  = part_vdep(part);
				su.vcnt2 = 0;
				su.vstep2 = nv::vib_part_step(el, m_vib_part_target[gs_mode() ? 1 : 0][std::clamp(su.vdep, 0, 127)]);
				su.vramp = true;
				// 20ms の刻みは声の番号の偶奇で 10ms ずれる
				su.vnext = su.fnext;
				if (m_eg_have && off_vibrato_step(su.vnext, slot))
					su.vnext += FENV_TICK;
				m_traj = true;
				m_traj_next = 0;
			}
			// 押鍵でもう段 0 の行き先に居る音は、包絡線が最初の目を飛ばす。LFO はその目も進む
			if (su.fstage != 0 && su.lrun && su.lfdep)
				su.lph = nv::lfo_next(su.lph, su.lstep);
			// **段 0 から始める**。実機は 10ms ごとに「着いたか」を見て次の段へ
			su.pvel = pvel;
			su.vel = vel;                    // 液晶のメーター用
			su.pstage = 0;
			// **刻みはフィルタの包絡線と同じ**（実機はどちらも同じ 10ms の
			// タイマで動いている）
			// フィルタの包絡線は「1 目遅らせて進め始める」ので、こちらは
			// その 1 目ぶん手前が実機の格子になる（実測で 312 サンプル）
			// **音程の包絡線の段も実機の 10ms 格子**。
			su.pnext = m_eg_have
			         ? slot_eg_after(u64(s64(m_clock) + EG_LAG), slot)
			         : (su.fnext > FENV_TICK ? su.fnext - FENV_TICK
			                                 : (m_clock / FENV_TICK + 1) * FENV_TICK);
			// **打のパンが 0（Rnd）なら打つたびにでたらめ**（SFX の打も）
			su.rnd_pan = (pan_is_rnd(part) || (su.sfx && drum_pan_rnd(part, note))) ? pan_rnd_draw() : -1;
			su.reverb_base = sr.v[word::reverb_send];
			su.chorus_base = sr.v[word::chorus_send];
			sr.set(word::pan_mix, su.rnd_pan >= 0 ? nv::pan_rnd_word(m_rom, su.rnd_pan) : exact_pan(su, part));
			su.lfo = sr.v[word::lfo];
			su.cut = sr.v[word::lowpass_cut];
			sr.set(word::lfo, lfo_word(su.lfo, part, note));
			// **鍵の曲線は押した鍵で**
			sr.set(word::lowpass_cut, cut_plain(nv::cutoff_keyon(m_rom, el, knote, pvel, false,
			                                                     part_atk(part),
			                                                     nv::soft_vel(pvel, pc.soft)),
			                                    part, el, pvel,
			                                    assign_cut(part, note)));
			sr.set(word::resonance, u16(u16(sr.v[word::resonance] & 0x07ff) |
			                            u16(u16(nv::reso_level(el, pvel, res_knob(part))) << 11)));
			sr.set(word::highpass_cut, nv::filter2_word(el[nv::elem_at::highpass], su.sfx ? drum_hpf(part, note) : part_hpf(part)));
			sr.set(word::reverb_send, exact_send(su, part, false, su.reverb_base));
			sr.set(word::chorus_send, exact_send(su, part, true, su.chorus_base));
			// **インサーションを通るパートはミキサが丸ごと別**
			if (ins_routed(part))
			{
				ins_mixer(sr, nv::voice_pan_pos(m_rom, el, pnote), ins_target(part));
				// Rnd は当たった位置そのもの（素通しのときと同じ値）
				if (su.rnd_pan >= 0)
					sr.set(word::pan_mix, nv::pan_rnd_word(m_rom, su.rnd_pan));
			}
			su.legato_att = 0;
			if (legato_count > 0) {
				// 同じ要素の音がもう消えていれば、すっかり下がった所から
				legato_source from = legato[0];
				from.level = LEGATO_SILENT;
				for (int source = 0; source < legato_count; source++)
					if (legato[source].elem == el)
						from = legato[source];
				apply_legato(su, sr, from, part);
			}
			write_slot(slot, sr);
			// 鍵の遅れ が 0 でなければ、その要素は遅れて鳴る
			pend.push_back({ slot_bits::of(slot), delayed_key_offset(el) });
			nwrote++;
			any = true;
		}
		if (any) {
			m_cc[part].last = note;      // つぎの音はここから滑る
			m_cc[part].porta_src = -1;   // CC84 の指定は 1 度で使い切る
		}
		// **要素を書き終えてから鍵を押す**。SFX の打はそこから
		// さらに SFX_KEY_DELAY ぶん遅れる
		int on_second_chip = 0;
		for (const auto &p : pend)
			on_second_chip += second_chip_slots(p.first);
		const s64 at0 = s64(write_done(nwrote, on_second_chip)) + (fixed_note >= 0 ? SFX_KEY_DELAY : 0);
		const u64 at = at0 < 0 ? 0 : u64(at0);
		for (const auto &p : pend) {
			const u64 key = p.second ? std::max(at, m_clock + p.second) : at;
			for (int i = 0; i < SLOTS; i++)
				if (p.first.test(i))
					m_slot[i].key_at = key;
			m_burst_voices |= p.first;
			if (p.second)
				m_pend.push_back({ p.first, std::max(at, m_clock + p.second) });
			else if (at > m_clock - m_read_ahead)
				m_pend.push_back({ p.first, at });
			else
				keymask |= p.first;
		}
		if (!keymask)
			return any;                  // 遅らせた要素だけの音もある
		key_on(keymask);
		return true;
	}

	// 鍵を離す。鳴っていなければ false
	// **離すのは 1 回ぶんだけ**。同じ鍵を離さずに何度も押すと、
	// 実機は押したぶんだけスロットを使い、ノートオフ 1 つでは**いちばん古い
	// 1 回ぶん**しか離さない（残りには離しの速さを書かない）。こちらは
	// 同じ鍵のスロットを全部離していたので、刻みの曲で音がごっそり消えていた。
	// `all` は全部切るとき（オールノートオフ・モノ・ダンパー離し）
	bool note_off(int part, int note, bool all = false)
	{
		bool any = false;
		u32 want = 0;
		if (!all) {
			// いちばん古い押しの番号を探す
			for (int i = 0; i < SLOTS; i++) {
				const slot_use &s = m_slot[i];
				if (s.on && s.part == part && s.keynote == note &&
				    (!want || s.inst < want))
					want = s.inst;
			}
			if (!want)
				return false;
		}
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (!s.on || s.part != part || s.keynote != note)
				continue;
			if (!all && s.inst != want)
				continue;
			// **まだ鍵を押していない音の離しは、押した直後まで待たせる**（key_on）
			if (!s.keyed) {
				s.off_wait = true;
				any = true;
				continue;
			}
			release_slot(i);
			any = true;
		}
		return any;
	}

	// スロット 1 つを離す（note_off の中身。鍵を押す前に届いた離しは key_on から）
	void release_slot(int i)
	{
		slot_use &s = m_slot[i];
		const int part = s.part;
		const int note = s.keynote;
		{
			if (m_cc[part].damper && !s.hard) {       // ダンパーを踏んでいる間は切らない（急停止は別）
				s.held = true;
				return;
			}
			if (s.sost)                    // ソステヌートで待たせている音
				return;
			// 減衰は**いまのつまみで**出す。s.att は鳴らし始めたときの値なので、
			// 途中で音量を絞られた音を離すと、絞る前の大きさで鳴り終わってしまう
			// **SFX の打**は要素を持つが、離しを受けるかはドラムの決まり
			// （3n rr 09）。受けるなら**要素の離しの速さ**で離す（実機は鍵 39 で 0xCC、
			// 鍵 68 で 0xA8 と要素ごとの値を書く。ドラムの 0xCF ではない）
			if (s.elem && (!s.sfx || drum_rcv_note_off(part, note)))
				m_poke(i, word::release, release_of(s, part, s.keynote));
			// **離しを受けるドラム**（3n rr 09）。要素を持たないので
			// 速さだけ与えて、音量はそのときの値にする
			else if (!s.elem && drum_rcv_note_off(part, note))
				m_poke(i, word::release,
				       u16(DRUM_OFF_RATE | u16(note_att(s, part) & 0xff)));
			// ドラムは離しでも音を切らない（実機も打ったら鳴りきる）
			s.on = false;
			// **ドラムも「鳴っている」ことにする**。離しの段は無いが、
			// 打の尾が残っている間はスロットを空けない。
			// `s.elem != nullptr` にしていたので**ドラムだけ外れていて**、
			// 離した打のスロットをすぐ次の打で使い回していた。実機はロールの
			// 1 打ごとに別のスロットを使う（28・29・30・31…）
			s.rel = true;
			s.rel_at = m_clock;
			s.rel_att = s.att;
			fenv_release(s);
			peg_release(i);              // 音程の包絡線も離す
			if (s.elem) {
				m_traj = true;
				m_traj_next = 0;
			}
		}
	}

	void silence()
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (!s.on)
				continue;
			if (s.elem && m_rom && m_poke)
				m_poke(i, word::release, nv::release_word(m_rom, s.elem, s.keynote, s.att));
			s.on = false;
			s.held = false;
			s.sost = false;
		}
		m_pend.clear();
		m_busy = 0;
		m_bend_due.fill(0);
		m_bend_next = ~u64(0);
		m_traj = false;
		m_traj_next = 0;
	}

	// 離しの release。**オールサウンドオフ（CC120）は速さを最大にする**
	// （実機は上位に `0xf0` を書く）。ふつうの離しは音色の速さ
	// **キーアサインがシングルで切るときの離しの速さ**。実機は音色によらず 0xD9 を書く（Strings・GrandPno・
	// Square Lead・Music Box の 4 つで確かめた）。音量はそのときの値のまま
	static constexpr u16 SINGLE_CUT_RATE = 0xd900;

	u16 release_of(const slot_use &s, int part, int note) const
	{
		const u16 v = nv::release_word(m_rom, s.elem, note, note_att(s, part),
		                              part_rel(part), gs_program(part));
		if (s.hard)
			return u16(nv::HARD_CUT_RATE | (v & 0xff));
		return s.single_cut ? u16(SINGLE_CUT_RATE | (v & 0xff)) : v;
	}

	// **ダンパーを踏んだ瞬間、離している最中の音を拾う**。
	// 実機は離しをやめて減衰 2 の速さに戻す。入れていなかったので、
	// ペダルで拾ったはずの音がそのまま消えていた
	void damper_catch(int part)
	{
		if (!m_rom)
			return;
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part != part || s.on || !s.rel || !s.elem || s.hard)
				continue;
			s.caught = true;
			m_poke(i, word::release,
			       nv::damper_hold_word(m_rom, s.elem, s.keynote, note_att(s, part),
			                           part_dec(part)));
		}
	}

	// CC123（オールノートオフ）は離す。CC120（オールサウンドオフ）は
	// ダンパーも無視して、離しを最速にして切る
	void all_off(int part, bool hard = false)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part != part)
				continue;
			if (hard && (s.on || s.rel)) {
				s.hard = true;
				s.held = false;
				s.sost = false;
				if (!s.on && s.elem)       // もう離している音も切り直す
					m_poke(i, word::release, release_of(s, part, s.keynote));
			}
			if (s.on)
				note_off(part, s.keynote, true);
		}
	}

	bool drum_on(int part, int note, int vel)
	{
		if (!m_rom)
			return false;
		// **同じオルタネートグループの打を止める**。ハイハットの
		// 開いた音は、閉じた音を打った瞬間に止まる。見ていないと刻みが濁る
		const int grp = drum_alt_group(part, note);
		const int cutn = grp ? alt_cut(part, note, grp) : 0;
		wrote_words(cutn);
		const u8 *drec = drum_rec_of(part, note);
		if (!nv::drum_rec_has_wave(drec)) {
			const u32 vrec = nv::sfx_voice_record(m_rom, drec);
			return vrec ? note_on_rec(part, note, vel, vrec, nv::SFX_NOTE) : false;
		}
		++m_inst;                        // この打の番号
		const int slot = take_slot(part, note);   // 1 打 1 スロット
		if (slot < 0)
			return false;
		slot_use &su = m_slot[slot];
		su.elem = nullptr;
		su.vcnt = su.vtgt = su.vdly = su.vcnt2 = 0;   // 前の音のビブラートのせり上がりを持ち越さない
		su.vramp = false;
		su.vnext = ~u64(0);
		su.wave = nullptr;
		su.tstart = m_clock;
		su.rnd_pan = (pan_is_rnd(part) || drum_pan_rnd(part, note)) ? pan_rnd_draw() : -1;
		su.vel = vel;
		m_traj = true;
		m_traj_next = 0;
		const int lv = drum_setup_of(part, note, settings::drum_param::level);
		su.att = nv::drum_att(m_rom, drec, lv < 0 ? 127 : lv, vel,
		                      vol_gain_of(part, part_vol(part), part_expr(part)));
		su.lvl0 = 0;
		su.arest = 0;
		const int att = nv::clamp_att(su.att);
		su.lfo = 0;
		const int co = drum_setup_of(part, note, settings::drum_param::coarse);
		const int fi = drum_setup_of(part, note, settings::drum_param::fine);
		// **切る高さ・共振・立ち上がり・減衰 1・減衰 2**（3n rr 0B-0F）は
		// 表の索引をずらす。SysEx でも NRPN でも効く。
		nv::element_words dr = nv::drum_note(m_rom, drec, att, nv::defaults(),
		                                     co < 0 ? 64 : co, fi < 0 ? 64 : fi,
		                                     drum_live(part, note, settings::drum_param::attack),
		                                     drum_live(part, note, settings::drum_param::cutoff),
		                                     drum_live(part, note, settings::drum_param::resonance),
		                                     drum_live(part, note, settings::drum_param::first_decay),
		                                     drum_live(part, note, settings::drum_param::second_decay),
		                                     drum_shift_cents(part), gs_mode(),
		                                     part_bri(part), res_knob(part),
		                                     part_atk(part), part_dec(part));
		dr.set(word::pitch_env_target, m_peg_flag);
		apply_drum_eq(dr, part, note);
		// パン・送りもドラムセットアップから。
		// `su.keynote` はもう入っているので exact_* が使える
		dr.set(word::pan_mix, su.rnd_pan >= 0 ? nv::pan_rnd_word(m_rom, su.rnd_pan)
		                             : exact_pan(su, part));
		dr.set(word::reverb_send, exact_send(su, part, false, dr.v[word::reverb_send]));
		dr.set(word::chorus_send, exact_send(su, part, true, dr.v[word::chorus_send]));
		if (ins_routed(part)) {
			const int dp = drum_setup_of(part, su.keynote, settings::drum_param::pan);
			ins_mixer(dr, dp < 0 ? 64 : dp, ins_target(part));
		}
		su.reverb_base = dr.v[word::reverb_send];
		su.chorus_base = dr.v[word::chorus_send];
		for (int i = 0; i < word::count; i++)
			if (i == word::release)
				m_poke(slot, u32(i), u16(att));
			else if (dr.write & (u64(1) << i))
				m_poke(slot, u32(i), dr.v[i]);
		m_poke(slot, word::highpass_cut, nv::filter2_word(drec[nv::drum_at::highpass], drum_hpf(part, note)));
		slot_bits keymask;
		keymask.set(slot);
		const int nwrote = 1;
		// **ドラムも要素を書き終えてから押す**。そのうえで、実機は
		// ドラムの 1 打を引くのに旋律より少し手間が掛かる（キットの表 →
		// 鍵ごとのずれ → 記録の 3 段引き）。実測で 2 サンプルぶん遅い
		const int on_second_chip = second_chip_slots(keymask);
		s64 at0 = s64(write_done(nwrote, on_second_chip)) + (on_second_chip ? DRUM_KEY_LEAD : 0);
		// **切った打があると打鍵が 1 サンプル遅れる**。実機は
		// 切る書き込み（0xD9）を先に済ませてから鍵を押すので、そのぶん後ろへずれる
		if (cutn && at0 <= s64(m_clock))
			at0 = s64(m_clock) + 1;
		const u64 at = at0 < 0 ? 0 : u64(at0);
		if (at > m_clock - m_read_ahead)
			m_pend.push_back({ keymask, at });
		else
			key_on(keymask);
		return true;
	}

	static constexpr s64 DRUM_KEY_LEAD = -2;

	// **SFX の打の押鍵の遅れ**。実機はキットの表 → 打の記録 → 音色記録の表 →
	// 旋律の組み立て、と 3 段引いてから押すので、旋律の式より遅い。SFX Kit1 の鍵 36 で
	// 実機 s=384306・こちら 384302（33 本の書き込みに 30 サンプル）。ノイズ系の SFX は
	// 3 サンプルずれるだけで相関が消える（ずらせば 100%）
	static constexpr s64 SFX_KEY_DELAY = 4;

private:

	int busy() const
	{
		int n = 0;
		for (const slot_use &s : m_slot)
			if (s.on)
				n++;
		return n;
	}

	slot_use fresh(int part, int note)
	{
		slot_use s;
		s.on = true;
		s.part = part;
		s.note = note;
		s.key_shift = 0;
		s.keynote = note;
		s.tstart = m_clock;
		s.age = ++m_age;
		s.inst = m_inst;
		return s;
	}

	int take_slot(int part, int note)
	{
		int oldest = -1, oldest_rel = -1;
		u64 oldest_age = ~u64(0), oldest_rel_age = ~u64(0);
		for (int n2 = 0; n2 < SLOTS; n2++) {
			const int i = (int(m_next_slot) + n2) % SLOTS;
			const slot_use &u = m_slot[i];
			const bool ringing = u.rel && m_clock - u.rel_at <= REL_FOLLOW;
			if (!u.on && !ringing) {
				m_slot[i] = fresh(part, note);
				m_next_slot = u8((i + 1) % SLOTS);
				return i;
			}
			if (!u.on) {
				if (u.age < oldest_rel_age) {
					oldest_rel_age = u.age;
					oldest_rel = i;
				}
			} else if (u.age < oldest_age) {
				oldest_age = u.age;
				oldest = i;
			}
		}
		// 離しの古いものを先に取る（まだ押されている音は最後まで残す）
		const int taken = oldest_rel >= 0 ? oldest_rel : oldest;
		m_slot[taken] = fresh(part, note);
		m_next_slot = u8((taken + 1) % SLOTS);
		return taken;
	}

	void write_slot(int slot, const nv::element_words &r)
	{
		// 第 2 群の出口（route_first-route_third）の置き換えは m_poke の先（mu2000）でやる。
		// ドラムは write_slot を通らずに直接書くので、ここでは漏れていた
		for (int i = 0; i < word::count; i++)
			if (r.write & (u64(1) << i))
				m_poke(slot, u32(i), r.v[i]);
	}

	void peg_release(int i)
	{
		slot_use &s = m_slot[i];
		if (!s.elem || !m_rom)
			return;
		const u8 *e = s.elem;
		const part_state &b = m_state->parts[s.part];
		if (e[nv::elem_at::pitch_eg_last_level] == e[nv::elem_at::pitch_eg_release_level] && s.pstage >= 3 && b[settings::PART_PITCH_EG_RELEASE_LEVEL] == 64)
			return;
		s.pstage = 3;
		int raw = int(e[nv::elem_at::pitch_eg_release_rate]);
		const int d = int(b[settings::PART_PITCH_EG_RELEASE_TIME]) - 64;
		if (d > 0) {
			const int tb = int(m_rom[nv::PEG_REL_TAB + u32(d)]);
			if (raw > tb)
				raw = tb;
		} else {
			raw -= d >> 1;
			if (raw > 63)
				raw = 63;
		}
		const int rel = part_rel(s.part);
		const int rate = nv::peg_rate_reg_raw(m_rom, e, raw, s.keynote, s.pvel,
		                                      64, rel < 0 ? 64 : rel);
		m_poke(i, word::pitch_env_rate, u16(rate << 8));
		// **行き先は 音程の包絡線の離しの高さ にパートの離しの高さ（XG の 08 pp 6B）を足す**。
		//見ていなかったので、そこを動かした曲で
		// 離したあとの音程が丸ごと違っていた
		int lv = int(e[nv::elem_at::pitch_eg_release_level]) + (int(b[settings::PART_PITCH_EG_RELEASE_LEVEL]) - 64);
		if (lv < 0) lv = 0;
		if (lv > 127) lv = 127;
		m_poke(i, word::pitch_env_target,
		       nv::peg_word(m_rom, nv::peg_cents(e, lv, s.pvel), e));
	}

	void peg_advance(int i)
	{
		slot_use &s = m_slot[i];
		if (!s.elem || !m_rom)
			return;
		const u8 *e = s.elem;
		s.pstage++;
		if (s.pstage == 1 && nv::peg_level_of(e, 0) == nv::peg_level_of(e, 1))
			s.pstage = 2;
		if (s.pstage == 2 && nv::peg_level_of(e, 1) == nv::peg_level_of(e, 2))
			s.pstage = 3;
		if (s.pstage > 2) {
			s.pstage = 3;
			return;
		}
		const int dec = part_dec(s.part);
		const int cc = s.pstage == 2 ? (dec < 0 ? 64 : dec) : 64;
		const int rate = nv::peg_rate_reg_stage(m_rom, e, s.pstage, s.keynote, s.pvel,
		                                        64, cc);
		const int lvl  = nv::peg_level_of(e, s.pstage);
		m_poke(i, word::pitch_env_rate, u16(rate << 8));
		m_poke(i, word::pitch_env_target,
		       nv::peg_word(m_rom, nv::peg_cents(s.elem, lvl, s.pvel), s.elem));
	}

	// **要素 1 つぶんの語を書く時間**（1/64 サンプル単位）。
	// 実機は要素の語を **1 要素 34 本**書き、**全部書き終えてから**
	// 鍵を押す。音源 への書き込みは 1 本 440 サイクル待たされるので
	// （1 本あたり約 560 サイクル）、34 本でちょうど
	// **30 サンプル**かかる。要素が 1 つ増えるごとに鍵がそのぶん遅れる。
	// こちらは一度に書いて即座に押していたので、2 要素の音色
	// （Square Lead など）が 30 サンプル早く鳴っていた
	//
	static constexpr u32 ELEM_COST64 = 30 * 64;
	// **要素を全部書き終える時刻**を返す（サンプル）。実機は 1 つの
	// CPU で順番に書くので、前の音がまだ書き終わっていなければその
	// あとに並ぶ。和音や密な曲では、あとの音ほど遅れて鳴る
	// **語 1 本ぶんの時間**（要素 1 つが 34 本ぶん）。要素の外で
	// 書いたぶんも、実機では同じだけ CPU を食う
	static u64 reg_cost64() { return ELEM_COST64 / 34; }

	// 要素の外で n 本書いたぶん、つぎの書き込みを後ろへずらす。
	// オルタネートグループで打を切ると、実機は**そのぶん打鍵が 1 サンプル
	// 遅れる**（入れないと閉じたハイハットだけ 1 サンプル早く鳴る）
	void wrote_words(int n)
	{
		if (n <= 0)
			return;
		const u64 cost = ELEM_COST64;
		const u64 now = m_clock * 64;
		const u64 t0 = now > cost ? now - cost : 0;
		const u64 start = t0 > m_busy ? t0 : m_busy;
		m_busy = start + u64(n) * reg_cost64();
	}

	// 第 2 群（スロット 64-127）へは 1 要素 6.5 サンプルで書ける
	static constexpr u64 SECOND_CHIP_ELEM_COST64 = 6 * 64 + 32;
	static int second_chip_slots(const slot_bits &slots)
	{
		int count = 0;
		for (int i = CHIP_SLOTS; i < SLOTS; i++)
			count += slots.test(i);
		return count;
	}

	// こちらが呼ばれる時刻は「1 要素を書き終えた時刻」なので、書き始めはその 1 要素ぶん手前。前の音を書いている間は待つ
	u64 write_start64() const
	{
		const u64 cost = ELEM_COST64;
		const u64 now = m_clock * 64;
		const u64 t0 = now > cost ? now - cost : 0;
		return t0 > m_busy ? t0 : m_busy;
	}

	// **前の書き込みに続けて書くときは、そのあいだに 2 サンプル**（次のパートへ移る手間）。
	// 1 つの MIDI の音を 2 パート目からは 1 パートごとにこれだけ遅れて押す。
	// 入れないと 2 要素のパートは 60 サンプルおき（実機は 61-63）、1 要素は 30（実機は 32-33）で、
	// 同じ音色を重ねたパフォーマンスの響きが 1 dB ほど違った
	static constexpr u64 NEXT_PART_GAP64 = 2 * 64;
	u64 write_done(int nwrote, int on_second_chip)
	{
		const int on_first_chip = (nwrote < 1 ? 1 : nwrote) - on_second_chip;
		const u64 queued = write_start64();
		const bool follows = queued <= m_busy && m_busy > 0;
		const u64 start = follows ? queued + NEXT_PART_GAP64 : queued;
		if (!follows) {
			m_burst_start = start;
			m_burst_lost_ticks = 0;
		}
		m_busy = start + u64(on_first_chip) * ELEM_COST64 + u64(on_second_chip) * SECOND_CHIP_ELEM_COST64;
		return (m_busy + 32) / 64;
	}

	// 遅れて鳴る要素（nv::elem_wait）の鍵を押す時刻。いまの m_clock からの差（遅れない要素は 0）。
	// 実機は 2.5ms（441/4 サンプル）の刻みで待ちを数える。この刻みは 10ms の格子の上にあり、
	// 書き始めの直後の刻みが 1 目なので、待ちから 1 目ぶんを引いた時刻のつぎの刻みで押す
	u32 delayed_key_offset(const u8 *elem) const
	{
		const u32 wait = nv::elem_wait(elem);
		if (!wait)
			return 0;
		constexpr u64 QUARTERS_PER_TICK = 441;       // ここの時刻は 1/4 サンプルで数える
		constexpr u64 FIRST_TICK_QUARTERS = 440;     // 待ちから引く 1 目（110 サンプル）
		constexpr u64 KEY_WRITE_QUARTERS = 6;        // 刻みから鍵を書くまで 1.5 サンプル
		const u64 tick_phase = u64(m_eg_phase) * 4 + KEY_WRITE_QUARTERS;
		// 同じ MIDI の音で続けて書くパートは、同じ刻みから数える
		const u64 start = write_start64();
		const u64 ready = (start > m_busy ? start : m_burst_start) / 16 + u64(wait) * 4 - FIRST_TICK_QUARTERS;
		const u64 ticks = ready > tick_phase ? (ready - tick_phase + QUARTERS_PER_TICK - 1) / QUARTERS_PER_TICK : 0;
		const u64 at = (tick_phase + ticks * QUARTERS_PER_TICK + 2) / 4;
		return at > m_clock ? u32(at - m_clock) : 1;
	}

	void key_on(const slot_bits &mask)
	{
		// 群ごとに押す。押す声の無い群には触らない
		for (int group = 0; group < 2; group++)
			if (mask.w[group])
				m_key_on(group, mask.w[group]);
		// **音程の包絡線の行き先はキーオンの「あと」に書く**。群は
		// キーオンのときの pitch_env_target を初めの高さとして取り込むので、
		// 先に書いてしまうと包絡線が無くなる（実機も 15 サンプル後に書く）
		for (int i = 0; i < SLOTS; i++) {
			if (!mask.test(i))
				continue;
			if (m_slot[i].peg_tgt != nv::NO_PITCH_TARGET)
				m_poke(i, word::pitch_env_target, m_slot[i].peg_tgt);
			m_slot[i].peg_tgt = nv::NO_PITCH_TARGET;
			m_slot[i].keyed = true;
		}
		// 押す前に届いていた離しを、いま効かせる（slot_use::keyed）
		for (int i = 0; i < SLOTS; i++) {
			if (!mask.test(i) || !m_slot[i].off_wait)
				continue;
			m_slot[i].off_wait = false;
			if (m_slot[i].on)
				release_slot(i);
		}
	}

	poke_fn m_poke;
	key_on_fn m_key_on;
	peek_fn m_peg_peek;
	peek_fn m_slot_peek;
	peek_fn m_slot_held;
	const u8 *m_rom = nullptr;
	u8 m_vib_part_target[2][128] = {};       // Vib Depth の深さ → カウンタが頭打ちに着く所（XG・GS）
	synth_state *m_state = nullptr;
	std::array<slot_use, SLOTS> m_slot;
	std::array<part_cc, PARTS> m_cc;
	u16 m_peg_flag = 0;
	// **鍵ごとのアフタータッチの値**。128 鍵分持っておく
	std::array<std::array<u8, 128>, PARTS> m_pat{};
	// **ドラムのセットアップを MIDI で書いた印**。組 × 鍵 ごとに、
	// 離したあと、つまみの動きを追い続ける長さ。いちばん遅い離しでも
	// これだけあれば鳴り終わる（それ以上はスロットを取り直しているはず）
	static constexpr u64 REL_FOLLOW = 44100 * 8;
	// 実機の包絡線は 441 サンプル（10ms）の格子で進む
	static constexpr u64 FENV_TICK = 441;
	static constexpr u64 EG_TICK_QUARTERS = 441 * 4, EG_GRID_STEP_QUARTERS = 441;
	// 音を書いて 2.5ms の刻みが止まる範囲は、書き始めの少し前から最後の鍵の少しあとまで
	static constexpr u64 WRITES_BLOCK_BEFORE = 11, WRITES_BLOCK_AFTER = 9;
	// 最後の鍵の少し前までの刻みは書き終えるまで待って動き、それより前の刻みは飛ぶ
	static constexpr u64 STEP_BEFORE_LAST_KEY = 60, STEP_AFTER_LAST_KEY = 5;
	u64 m_burst_start = 0;          // 続けて書いている音の書き始め（1/64 サンプル）
	int m_burst_lost_ticks = 0;
	slot_bits m_burst_voices;
	u64 m_steps_from = 0;           // 刻みの仕事が動いてよい時刻
	u32 m_eg_phase = 0;
	u32 m_eg_quarters = 0;                 // m_eg_phase を 1/4 サンプルで
	bool m_performance = false;
	static constexpr u32 SLOTS_PER_STEP_GROUP = 32;
	bool m_eg_have = false;
	u64 m_clock = 0;
	u64 m_alt_kill_next = ~u64(0);  // つぎに止めを刺す時刻
	bool m_traj = false;
	u64 m_traj_next = 0;           // つぎに段を書く時刻
	// 遅らせて鳴らす要素（鍵の遅れ）。時が来たら key_on する
	struct pending_key { slot_bits mask; u64 at; };
	static std::vector<pending_key> reserved_pend() { std::vector<pending_key> v; v.reserve(SLOTS * 4); return v; }
	std::vector<pending_key> m_pend = reserved_pend();
	u64 m_busy = 0;
	u8 m_next_slot = 0;              // つぎに使うスロット（take_slot）
	u64 m_read_ahead = 0;            // begin_read_ahead
	u32 m_inst = 0;                 // 押した回数（同じ鍵を重ねたとき用）
	// 格子に乗せたベンドの、パートごとの流す時刻（0 は無し）
	std::array<u64, PARTS> m_bend_due{};
	u64 m_bend_next = ~u64(0);
	u64 m_age = 0;
};

} // namespace xg

#endif // S_MU2000_XG_NATIVE_DRIVER_H
