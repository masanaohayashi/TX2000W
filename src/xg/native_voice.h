// license:BSD-3-Clause

#ifndef S_MU2000_XG_NATIVE_VOICE_H
#define S_MU2000_XG_NATIVE_VOICE_H

#pragma once

#include "compat/types.h"
#include "xg/synth_state.h"
#include "xg/voice_rom_layout.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace xg {
namespace nv {

using namespace rom_position;


constexpr u32 SET_COUNT  = 0x1F8;
// **EG のつまみ（CC72/73/75）で速さの目盛りをどう動かすか**。
// 128 段ぜんぶ測って、3 音色で突き合わせた。
//
//   つまみ <= 64 : 目盛り = min(63, 素の目盛り + (65 - つまみ) / 2)
//   つまみ >  64 : 目盛り = min(素の目盛り, 表[つまみ - 64])
//
// 上の向き（遅くする側）は音色ごとの目盛りからの足し算だが、**下の向き
// （速くする側）は音色によらない**。3 音色（Strings1・GrandPno・Flute）の
// どれも、つまみ 72 で目盛り 28、つまみ 127 で 1 になる。
// 表は ROM にそのまま入っていた（頭が 63 ＝「変えない」）

inline int eg_rate_cc(const u8 *rom, int base, int cc)
{
	if (!rom || cc < 0 || cc == 64)
		return base > 63 ? 63 : base;
	const int c = cc > 127 ? 127 : cc;
	if (c < 64) {
		const int v = base + (65 - c) / 2;
		return v > 63 ? 63 : v;
	}
	const int cap = int(rom[EG_RATE_CC + u32(c - 64)]);
	return cap < base ? cap : base;
}

// **減衰のつまみ（CC75）は足し算**（立ち上がりと違って表を使わない）。
// 3 音色とも同じずれで、下は 0 で止まる（実機の値がそこで飽和する）。
//
//   つまみ <= 64 : 目盛り + (67 - つまみ) / 4
//   つまみ >  64 : 目盛り - (つまみ - 64) × 7 / 16
//
// 1/4 と 7/16 は 128 段ぜんぶ測って合わせた
inline int eg_rate_cc_add(int base, int cc)
{
	if (cc < 0 || cc == 64)
		return base;
	const int c = cc > 127 ? 127 : cc;
	return c < 64 ? base + (67 - c) / 4
	              : base - (c - 64) * 7 / 16;
}

inline int eg_dec1_cc(int base, int cc)
{
	if (cc <= 64)
		return base;
	const int c = cc > 127 ? 127 : cc;
	const int cap = ((127 - c) >> 2) + 2;
	return cap < base ? cap : base;
}
// **離しは別の表**。途中までは減衰の表と同じ並びだが、
// 速い側（目盛り 48 以上）だけ違う（減衰 95 98 100 102 104 104 108 108 112… /
// 離し 95 95 96 96 96 96 101 101 104…）。DECAY_TAB を引いていたので、CC72 を
// 下げ切ったときだけこちらの離しが速すぎた（Strings で 0xEC 対 0xE5）


inline int s8(u8 v) { return v >= 128 ? int(v) - 256 : int(v); }
inline u16 rd16(const u8 *rom, u32 a) { return u16(rom[a] << 8 | rom[a + 1]); }
inline u32 rd32(const u8 *rom, u32 a)
{ return u32(rom[a]) << 24 | u32(rom[a + 1]) << 16 | u32(rom[a + 2]) << 8 | rom[a + 3]; }

// 音色記録の並び
constexpr u32 VOICE_ELEMENT_MASK = 0, VOICE_LEVEL = 1, VOICE_ELEMENTS = 12;
constexpr int ELEM_BYTES = 84;

// 要素の記録（ELEM_BYTES バイト）の並び
namespace elem_at {
constexpr int wave_set_high = 2, wave_set_low = 3;
constexpr int key_low = 4, key_high = 5, velocity_low = 6, velocity_high = 7;
constexpr int filter_eg_velocity_table = 8;
constexpr int lfo_wave = 9;              // 0 はのこぎり、1 は三角、2 以上はサンプル＆ホールド
constexpr int lfo_phase_reset = 10;      // 0 なら押すたびに位相が乱数
constexpr int lfo_rate = 11, lfo_delay = 12, lfo_fade = 13;
constexpr int lfo_pitch_depth = 14, lfo_filter_depth = 15, lfo_level_depth = 16;
constexpr int coarse_tune = 17, fine_tune = 18, key_follow = 19, key_pivot = 20;
constexpr int pitch_eg_range = 21, pitch_eg_velocity_depth = 22, pitch_eg_rate_velocity = 23;
constexpr int pitch_eg_rate_key_depth = 24, pitch_eg_rate_key_center = 25;
constexpr int pitch_eg_rates = 26, pitch_eg_release_rate = 29;       // 速さが 4 段（最後が離し）
constexpr int pitch_eg_start_level = 30;
constexpr int pitch_eg_levels = 31, pitch_eg_last_level = 33, pitch_eg_release_level = 34;   // 高さが 4 段（最後が離し）
constexpr int resonance = 35, velocity_lift = 36, cutoff = 37;
constexpr int cutoff_curve_kind = 38, cutoff_curve_row_high = 44, cutoff_curve_row_low = 45;
constexpr int filter_eg_velocity_depth = 46, filter_eg_rate_velocity = 47;
constexpr int filter_eg_rate_key_depth = 48, filter_eg_rate_key_center = 49;
constexpr int filter_eg_attack_rate = 50, filter_eg_first_rate = 51, filter_eg_second_rate = 52, filter_eg_release_rate = 53;
constexpr int filter_eg_start_level = 54, filter_eg_attack_level = 55, filter_eg_first_level = 56, filter_eg_second_level = 57, filter_eg_release_level = 58;
constexpr int level = 59, level_curve_kind = 60, level_curve_row_high = 66, level_curve_row_low = 67;
constexpr int velocity_curve = 68, pan_select = 69, rate_key_depth = 70, rate_key_center = 71;
constexpr int key_delay = 72, attack_rate = 73, first_decay_rate = 74, second_decay_rate = 75, release_rate = 76;
constexpr int first_decay_level = 77, second_decay_level = 78;
constexpr int wave_skip_high = 79, wave_skip_low = 80, resonance_velocity = 81, highpass = 82;
}

// ドラムの 1 打の記録の並び。頭の 23 バイトはドラムセットアップと同じ順
namespace drum_at {
constexpr int gs_base_key = 0, fine_tune = 1, second_decay_level = 10;
constexpr int cutoff = 11, resonance = 12, attack_rate = 13, first_decay_rate = 14, second_decay_rate = 15;
constexpr int highpass = 20, sfx_pitch_key = 23, sfx_voice_high = 24, sfx_voice_low = 25;
constexpr int wave = 26, wave_base_key = 26, gs_pitch_offset = 27, pitch_offset = 28, level_offset = 29, wave_format = 38;
}

// 語の中の決まった値
constexpr u16 LOWPASS_WORD_FLAG = 0x1000, HIGHPASS_WORD_FLAG = 0x8000;
constexpr u16 PITCH_FORMAT3_FLAG = 0x4000;    // 形式 3 の波形の音程の語に立てる
constexpr u16 LFO_RESET_FLAG = 0x4000;        // 音程の包絡線の語に立てる。押鍵で LFO の位相を揃える
constexpr u16 NO_PITCH_TARGET = 0xffff;       // 音程の包絡線に書く行き先が無い
constexpr u16 PAN_CENTER_WORD = 0x0808;
constexpr u16 DRUM_LFO_WORD = 0x7000;
constexpr u16 MOVING_LEVEL_LFO = 0xaa00;      // 音量の LFO を動かしているときの語の上位
constexpr u16 HARD_CUT_RATE = 0xf000;         // オールサウンドオフの離しの速さ
constexpr u16 ROUTED_CHORUS_LOW = 0x10;       // インサーションへ送る声の、コーラス送りの語の下位
constexpr int ATTACK_FASTEST_RATE = 0x3f, ATTACK_START_LEVEL = 0x7e;
constexpr int SILENT_ATT = 0xfe;
constexpr int LFO_TRIANGLE_FLAG = 0x40;       // LFO の語の上位。三角波
constexpr int RELEASE_KEY_FLAG = 0x80;        // 離しの語の上位。離せの印
constexpr int DRUM_FIRST_DECAY_LEVEL = 0x04;
constexpr int BEND_CENTER = 0x2000;

// 音色の記録の 84 バイト（要素 1 つぶん）。rec は xg::voice_rom::lookup の戻り値
inline const u8 *element(const u8 *rom, u32 rec, int index = 0)
{
	return rom + rec + VOICE_ELEMENTS + u32(index) * ELEM_BYTES;
}
// 記録の先頭のバイトは**要素のビットマスク**（1/3/7/15 ＝ 1〜4 要素）。
// 数ではないので、立っているビットを数える
inline int element_count(const u8 *rom, u32 rec)
{
	int n = 0;
	for (int i = 0; i < 4; i++)
		if (rom[rec + VOICE_ELEMENT_MASK] & (1 << i))
			n++;
	return n;
}

// その要素が、この鍵と強さで鳴るか（鍵の下限,5 が鍵の範囲、強さの下限,7 が強さの範囲）
inline bool element_active(const u8 *elem, int note, int vel)
{
	return note >= elem[elem_at::key_low] && note <= elem[elem_at::key_high] && vel >= elem[elem_at::velocity_low] && vel <= elem[elem_at::velocity_high];
}

// 波形の組の番号（7bit が 2 つ）
inline int wave_set(const u8 *elem) { return (elem[elem_at::wave_set_high] << 7) | (elem[elem_at::wave_set_low] & 0x7f); }

// その鍵で使う波形の記録（16 バイト）。無ければ nullptr
inline const u8 *wave_entry(const u8 *rom, int setno, int note)
{
	if (setno < 0 || setno >= int(SET_COUNT))
		return nullptr;
	u32 s = WAVE_BASE + rd16(rom, SET_TABLE + u32(setno) * 2);
	for (int i = 0; i < 80; i++) {
		if (rom[s + 3] >= note || rom[s + 3] == 0x7f)
			return rom + s;
		s += 16;
	}
	return nullptr;
}

// 波形の記録の中身
struct wave_info {
	int level;         // この波形ぶんの減衰（0.375dB 目盛り。多段サンプルで段ごとに違う）
	int base_key;      // もとの音程（半音）
	int fine_cents;    // その細かい調整（セント。引く）
	int key_max;       // この記録を使う鍵の上限
	u32 pre_loop;      // ループ前のサンプル数（語 lead_high/lead_low）
	u32 loop_len;      // ループの長さ（loop_high/loop_low）
	u32 format_addr;
};

inline wave_info read_wave(const u8 *e)
{
	wave_info w{};
	w.level      = e[0];
	w.base_key   = e[1];
	w.fine_cents = e[2] >= 128 ? int(e[2]) - 256 : int(e[2]);
	w.key_max    = e[3];
	w.pre_loop   = u32(e[4]) << 24 | u32(e[5]) << 16 | u32(e[6]) << 8 | e[7];
	w.loop_len   = u32(e[8]) << 24 | u32(e[9]) << 16 | u32(e[10]) << 8 | e[11];
	w.format_addr = u32(e[12]) << 24 | u32(e[13]) << 16 | u32(e[14]) << 8 | e[15];
	return w;
}

inline void wave_backwards_swap(u32 &pre, u32 &loop)
{
	if (!(loop & 0x80000000))
		return;
	const u32 a = pre & 0xffffff, b = loop & 0xffffff;
	pre  = (pre  & 0xff000000) | b;
	loop = (loop & 0xff000000) | a;
}

constexpr int KEY_FOLLOW_N   = 6;

inline int key_follow(const u8 *rom, const u8 *elem)
{
	const int i = int(elem[elem_at::key_follow]);
	if (!rom || i < 0 || i >= KEY_FOLLOW_N)
		return 100;
	return int(rom[KEY_FOLLOW_TAB + u32(i)]);
}

// **鍵の追従の支点**（追従の支点）。ほとんどの要素は 60（中央のド）だが、
// Bottle の 75 や Applause の 57 のように別の鍵を支点にするものがある。
// 支点が 60 でないと、追従が 100 でない音色では鍵 60 でも値がずれる
inline int key_pivot(const u8 *elem) { return elem[elem_at::key_pivot]; }

// 要素を**遅らせて鳴らす**段（鍵の遅れ）。鍵の遅れ が n なら、書き始めてから
//
//   待ち = 441 × 刻みの数(n)   サンプル（0 なら遅れない）
//   刻みの数 = 1, 2, 4, 8 のあとは 12, 16, 24, 32, 48, 64 … と 1.5 倍・4/3 倍を交互に
//
// あと、そのつぎの 2.5ms の刻みで鍵を押す（押す時刻は native_driver の delayed_key_offset）。
// 2 つの要素のうなりで音ができている音色（FrenchHorn）は、ここが数十サンプルずれると波形が丸ごと変わる。
// 実機で測れたのは 1-5 と 10（5 で 12、10 で 64）。倍々のままだと 5 で 40 ms、10 で 0.6 秒遅かった
// ponytail: 6-9 と 11 以上は測れる音色が無く、同じ並びを延ばした値。違えば表を直す
constexpr int KEY_DELAY_STEPS_MAX = 23;   // これより先は同じ値（u32 に収める）
constexpr u32 key_delay_steps(int n)
{
	const int k = (n > KEY_DELAY_STEPS_MAX ? KEY_DELAY_STEPS_MAX : n) - 1;
	return k < 3 ? (1u << k) : ((2u + u32((k - 3) & 1)) << (((k - 3) >> 1) + 2));
}
inline u32 elem_wait(const u8 *elem)
{
	const int n = elem[elem_at::key_delay] & 0x7f;
	if (n <= 0)
		return 0;
	return 441 * key_delay_steps(n);
}
static_assert(key_delay_steps(1) == 1 && key_delay_steps(4) == 8 && key_delay_steps(5) == 12 && key_delay_steps(10) == 64);

// **波形を選ぶときの鍵**。実機は「その要素が実際に出す高さ」で選ぶので、
// 粗調だけでなく**鍵の追従と支点**も入る。
//
//   選ぶ鍵 = 支点 + (鍵 - 支点) * 追従 / 100 + (粗調 - 64)
//
// * GtHarmonics（31）は 粗調 が 12 半音下、追従 100 で、鍵 84 のときに
//   鍵 72 のぶんの波形を鳴らす
// * Rain（96）は 粗調 が +24、**追従 20**、支点 60。鍵 84 なら
//   60 + 24*20/100 + 24 = 88 で、上限鍵 96 の記録に入る。粗調だけで
//   数えると 108 になって、1 つ先の記録（上限鍵 108）を取ってしまう
// **ノートシフトと移調（shift）は追従を掛けたあとで足す**。音程と同じで、
// 押した鍵ぶんだけに追従が掛かる。追従の小さい要素に +24 のノートシフトを
// 掛けると、追従を掛けてから足すほうの記録を実機は選んでいた
// （音色 SFX 88 の第 2 要素は追従が小さく粗調 +32。鍵 60 で上限鍵 127 の記録）
inline int elem_key(const u8 *rom, const u8 *elem, int note, int shift)
{
	const int piv = key_pivot(elem);
	return piv + (note - shift - piv) * key_follow(rom, elem) / 100 + shift + int(elem[elem_at::coarse_tune]) - 64;
}

// note はずらしたあとの鍵、shift はそのうちのずらしたぶん
inline int wave_note(const u8 *rom, const u8 *elem, int note, int shift = 0)
{
	const int n = elem_key(rom, elem, note, shift);
	return n < 0 ? 0 : (n > 127 ? 127 : n);
}

// **鍵が 0-127 からはみ出した半音数**。`wave_note` が丸めているぶん
inline int note_overflow(const u8 *rom, const u8 *elem, int note, int shift = 0)
{
	const int n = elem_key(rom, elem, note, shift);
	return n > 127 ? n - 127 : (n < 0 ? n : 0);
}

// 要素ぶんの音程のずらし（セント）。粗調 が半音、離調 がセント
inline int elem_tune(const u8 *elem)
{
	return (int(elem[elem_at::coarse_tune]) - 64) * 100 + (int(elem[elem_at::fine_tune]) - 64);
}

constexpr u32 PORTA_TICK = 441;

inline int porta_step(const u8 *rom, int cc5)
{
	if (!rom || cc5 < 0 || cc5 > 127)
		return 0;
	const int raw = int(rd16(rom, PORTA_TAB + u32(cc5) * 2));
	return cc5 < 24 ? raw * 512 : raw * 2;
}

// **滑りの残りをセントに直す**。`glide` はセント × 256 で、
// 上へ滑るときは負。C++ の `/` は 0 の側へ切り捨てるので、
// そのままだと**上へ滑るときだけ 1 セント高め**になる。
// 実機は下へ落とす（CC5=32 の滑りで、語が 1 だけ高かった）
inline int glide_cents(int glide)
{
	return glide >= 0 ? glide / 256 : -((-glide + 255) / 256);
}

inline u16 pitch_word(const wave_info &w, int note, int follow = 100,
                     int cents_extra = 0, int pivot = 60)
{
	const int cents = (note - pivot) * follow + (pivot - w.base_key) * 100
	                + w.fine_cents + cents_extra;
	const int v = cents * 1024 / 1200;
	// ビット 14 は波形の**形式**で決まる（形式 3 のときだけ立つ。402 組で確かめた）
	const u16 flag = ((w.format_addr >> 30) & 3) == 3 ? PITCH_FORMAT3_FLAG : 0;
	return u16((v & 0x3fff) | flag);
}


// ---- コントローラ
//
// 実機が何を書くかは `nativeplay --ccwatch` で見た:
//   CC7・CC11 → 語 release の下位バイト（減衰）
//   CC10      → 語 pan_mix（上が左・下が右の減衰）
//   ベンド    → 語 pitch（音程）
//   CC1       → 語 lfo の下位バイト（LFO の深さ）

inline int vol_gain(int vol, int expr)
{
	const int v = vol < 0 ? 100 : (vol > 127 ? 127 : vol);
	const int e = expr < 0 ? 127 : (expr > 127 ? 127 : expr);
	// **音量 0 は素通しで 0**。式どおりなら ((0+1)*(127+1))>>7 = 1 になるが、
	// CC7=0 では掛け値も 0 になる（実測）
	if (v == 0)
		return 0;
	return ((v + 1) * (e + 1)) >> 7;        // 0-128
}

inline int level_with_gain(int level, int gain)
{
	if (gain <= 0 || level <= 0)
		return 0;
	int v = (level * (gain > 128 ? 128 : gain)) >> 7;
	if (v <= 0)
		v = 1;
	return v > 128 ? 128 : v;
}

// その線形の値（0-128）を減衰に直す
// つまみ 1 つぶんの、目盛りへの足し分
inline int amp_assign(const u8 *rom, int depth, int value)
{
	const int d = depth - 64;
	if (!d || !value)
		return 0;
	const int t = int(rom[AMP_ASSIGN_TAB + u32(d < 0 ? -d : d)]);
	const int v = int(u16(t * value) >> 8);
	return d < 0 ? -v : v;
}

inline int gain_att(const u8 *rom, int gain)
{
	if (gain <= 0)
		return 255;
	return 2 * int(rom[LEVEL_TAB + u32(std::min(128, gain) - 1)]);
}


inline int pan_att(const u8 *rom, int x)
{
	if (x <= 0)
		return 0;
	if (x >= 127)
		return 255;
	return int(rom[PAN_ATT_TAB + u32(x)]);
}

// 明るさ（CC74）→ 語 lowpass_cut の下 12bit（切る高さ）。
// 実測（`nativeplay --ccfilter`）は **16 × (値 - 64)** でまっすぐ動き、1984 で頭打ち
constexpr int CUTOFF_MAX = 1984;
inline int bright_shift(int cc, bool gs = false)
{
	if (!gs)
		return 16 * (cc - 64);
	const int d = cc > 64 ? cc - 64 : 64 - cc;
	const int shift = d <= 16 ? 8 * d : d <= 20 ? 128 + (d - 16) * 43 / 4 : 171 + (d - 20) * 64 / 3;
	return cc > 64 ? shift : -shift;
}

// 共振（CC71）→ 語 resonance の上 5bit。実測は **2 きざみで 1 段**
// （64 まで 0、67 で 1、127 で 31）
inline int reso_shift(int cc) { return (cc - 64) / 2; }


inline int send_att(const u8 *rom, int cc)
{
	if (cc <= 0)
		return 255;
	return int(rom[SEND_TAB + u32(std::min(127, cc) - 1)]);
}

// **ビブラートのつまみ**（08 pp 15 速さ・16 深さ ＝ NRPN 01 08/09）。
// どちらも語 lfo を動かす（上位が速さ、下位が深さ）。
// 128 段ぜんぶ測り、2 音色で突き合わせた
// 。
//
//   速さ: つまみ < 64 なら min(素の目盛り, 表)、> 64 なら max(素の目盛り, 表)
//         （表の 63 と 0 は「基準を残す」印になる）
//   深さ: 素の深さに表の値を足す
// つまみの中央のまわりは「基準を残す」印（63 と 0）で、それ以外は ROM の曲線
inline int vib_rate_mark(const u8 *rom, int knob)
{
	return knob == 63 || knob == 64 ? 63 : knob >= 65 && knob <= 74 ? 0 : int(rom[VIB_RATE_CURVE + u32(knob)]);
}

constexpr u8 VIB_DEPTH_TAB[128] = {
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,  29,  40,  52,  69,  86,  86,  86,  86,  86, 145, 145, 145, 145, 145, 150,
	150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 160, 160, 160, 160,
	160, 160, 160, 160, 160, 160, 160, 160, 163, 163, 163, 163, 163, 163, 163, 163,
	163, 163, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 173, 173, 173, 173,
};

// TG300B（GS）モードはプログラムごとのずれを足して同じ表を引く（表の 63-74 の間は 31）
constexpr int GS_VIB_RATE_SHIFT_ZERO = 14;
constexpr int XG_PROGRAM = -1;                    // gs_program に渡す。XG モード

inline int vib_rate(int base, int cc, const u8 *rom, int gs_program = XG_PROGRAM)
{
	if (cc < 0 || cc == 64)
		return base;
	const int c = cc > 127 ? 127 : cc;
	int t = vib_rate_mark(rom, c);
	if (gs_program != XG_PROGRAM) {
		const int i = std::clamp(c + int(rom[GS_VIB_RATE_SHIFT_TAB + u32(gs_program & 0x7f)]) - GS_VIB_RATE_SHIFT_ZERO, 0, 127);
		t = int(rom[VIB_RATE_CURVE + u32(i)]);
	}
	const int v = c < 64 ? (t < base ? t : base) : (t > base ? t : base);
	return v < 0 ? 0 : (v > 63 ? 63 : v);
}

constexpr u8 VIB_DEPTH_TAB_GS[128] = {
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   6,   6,  12,  12,  17,  17,  23,  29,  29,  35,  35,  40,  46,  46,  52,
	 52,  58,  58,  63,  69,  69,  75,  81,  81,  86,  86,  92,  92, 104, 104, 104,
	104, 115, 115, 115, 127, 127, 127, 127, 145, 145, 145, 147, 147, 147, 148, 148,
	148, 150, 150, 151, 154, 155, 158, 160, 165, 173, 173, 173, 173, 173, 173, 173
};

inline int vib_depth(int base, int cc, bool gs = false)
{
	if (cc < 0 || cc == 64 || (gs && cc < 64))
		return base;
	const int c = cc > 127 ? 127 : cc;
	const int t = int((gs ? VIB_DEPTH_TAB_GS : VIB_DEPTH_TAB)[c]);
	const int v = c < 64 ? (t < base ? t : base) : (t > base ? t : base);
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}


constexpr u8 GS_VIB_DELAY_CLASS_SLOW = 0x40;      // 管楽器や声の多く
constexpr u8 GS_VIB_DELAY_CLASS_MIDDLE = 0x80;

inline int vib_delay(int base, int cc, const u8 *rom, int gs_program = XG_PROGRAM)
{
	if (cc < 0 || cc == 64)
		return base;
	const int c = cc > 127 ? 127 : cc;
	u32 curve = VIB_DELAY_CURVE;
	if (gs_program != XG_PROGRAM) {
		const u8 voice_class = rom[GS_VIB_DELAY_CLASS_TAB + u32(gs_program & 0x7f)];
		if (voice_class == GS_VIB_DELAY_CLASS_SLOW)
			curve = VIB_DELAY_CURVE_GS_SLOW;
		else if (voice_class == GS_VIB_DELAY_CLASS_MIDDLE)
			curve = VIB_DELAY_CURVE_GS_MIDDLE;
	}
	const int t = int(rom[curve + u32(c)]);
	const int v = c < 64 ? (t < base ? t : base) : (t > base ? t : base);
	return v < 0 ? 0 : v;
}

// モジュレーション（CC1）→ 語 lfo の下位（LFO の深さ）に足す。
// 実測は 10 段で、**音色によらない**（GrandPno・Strings・SawLead で同じ）。
// lfo の上位は LFO の型と刻みなので触らない
inline int mod_depth(int cc)
{
	static const u8 STEP[10] = { 0, 9, 17, 26, 35, 43, 52, 60, 72, 84 };
	static const u8 EDGE[9]  = { 13, 26, 39, 52, 64, 77, 90, 103, 116 };
	int i = 0;
	while (i < 9 && cc >= int(EDGE[i]))
		i++;
	return int(STEP[i]);
}


// ピッチベンド → セント
inline int bend_cents(int bend14, int range_semitones)
{
	const int d = bend14 - 8192;
	const int stretched = d > 0 ? std::min(d * 8192 / 8190, 8192) : d;
	const int v = stretched * range_semitones * 100;
	return v >= 0 ? v / 8192 : -((-v + 8191) / 8192);
}

// 0..255 に収める
inline int clamp_att(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

// 声 1 つの設定の語。音の高さ、フィルタ、包絡線、行き先などを語で持つ
namespace word {
enum : int {
	lowpass_cut,
	highpass_cut,
	resonance,
	amplitude_lfo,
	attack,
	first_decay,
	second_decay,
	release,
	lfo,
	pitch_env_rate,
	pitch_env_target,
	pitch,
	lead_high,
	lead_low,
	loop_high,
	loop_low,
	wave_high,
	wave_low,
	eq_low_previous,
	eq_low_feedback,
	eq_low_current,
	eq_high_feedback,
	eq_high_previous,
	eq_high_current,
	pan_mix,
	reverb_send,
	chorus_send,
	route_first,
	route_second,
	route_third,
	count
};
}


// 組み立てた声の設定。write が立っている語だけ書く
struct element_words {
	u16 v[word::count];
	u64 write;         // ビット n が立っていれば語 n を書く

	element_words() { std::memset(v, 0, sizeof(v)); write = 0; }
	void set(int which, u16 value) { v[which] = value; write |= u64(1) << which; }
};

inline u16 filter2_word(int highpass, int hpf = 64)
{
	return u16(HIGHPASS_WORD_FLAG | u16(std::clamp(highpass * 16 + 24 * (hpf - 64), 0, 0x7ff)));
}

// 分かっていない所に置く値。**実機を鳴らして測った、素直な音色のときの値**で、
// これは「式が分かっていない」という印でもある
struct defaults {
	u16 lfo_amp = 0xfa00;
	// ミキサ（パート 1・音量 100・パン中央・リバーブ送り 40 のときの実測）。
	// **pan_mix-route_third だけ**。0x38-0x3d は「入力 0x40 から先」＝ エフェクト の戻りや A/D の
	// ぶんで、声のスロットのものではない。ここを書くと残響の混ざり方が変わる
	u16 mix[6] = { 0x0808, 0x182b, 0xffff, 0x4d00, 0x4800, 0x4400 };
	// 声ごとの IIR（パートの EQ）。素通しのときの実測
	u16 iir[6] = { 0xe05d, 0x1fa3, 0x2000, 0x0257, 0xfda9, 0x2000 };
};

// **ベロシティ感度**（XG の 08 pp 0C 深さ・0D ずらし。どちらも既定 64）。
// 実測（Strings1・強さ 100 と 40）で
//
//   効く強さ = clamp(強さ * 深さ / 64 + (ずらし - 64) * 2, 1, 127)
//
// 深さ 16 で 100 -> 25、32 で 50、48 で 75、80 以上で頭打ち。
// ずらし 32 で 100 -> 36、48 で 68、72 で 116、強さ 40 のときは 32 で 1
inline int vel_sense(int vel, int depth, int offset)
{
	const int d = depth  < 0 ? 64 : (depth  > 127 ? 127 : depth);
	const int o = offset < 0 ? 64 : (offset > 127 ? 127 : offset);
	int v = vel * d / 64 + (o - 64) * 2;
	if (v < 1) v = 1;
	return v > 127 ? 127 : v;
}

constexpr int VEL_CURVE_ROWS = 7;

inline int velocity_att(const u8 *rom, int vel, int curve = 0)
{
	const u32 c = u32(curve >= 0 && curve < VEL_CURVE_ROWS ? curve : 0);
	const int i = rom[VEL_CURVE + c * 128 + u32(vel & 0x7f)];
	return rom[LEVEL_TAB + u32(i & 0x7f)];
}

constexpr int LFO_RATE_STEPS = 0x40;

inline bool sh_lfo(const u8 *elem) { return int(elem[elem_at::lfo_wave]) >= 2; }
inline int  sh_ticks(const u8 *elem)
{
	const int n = LFO_RATE_STEPS - int(elem[elem_at::lfo_rate]);
	return n < 1 ? 1 : n;
}
inline int  sh_pitch_off(int rnd, const u8 *elem)
{
	return ((s8(u8(rnd)) >> 1) * int(elem[elem_at::lfo_pitch_depth])) >> 2;
}
inline bool lfo_rnd_phase(const u8 *elem) { return elem[elem_at::lfo_phase_reset] == 0; }

// その要素の強さの曲線の行（強さの曲線）
inline int vel_curve_of(const u8 *elem) { return int(elem[elem_at::velocity_curve]); }

inline int vel_shift(const u8 *elem, int vel)
{
	const int d = int(elem[elem_at::velocity_lift]);
	if (!d)
		return vel;
	const int v = vel + (((127 - vel) * (d * 36)) >> 8);
	return v > 127 ? 127 : v;
}

inline int rd16s(const u8 *rom, u32 a)
{
	const int v = int(rd16(rom, a));
	return v >= 0x8000 ? v - 0x10000 : v;
}


// 16bit に切り詰める（実機の EXTS.W）
inline int s16v(int v) { return int(s16(u16(v))); }

inline int cents_to_pitch(const u8 *rom, int cents)
{
	if (!rom || !cents)
		return 0;
	const bool neg = cents < 0;
	const int c = neg ? -cents : cents;
	const int v = c <= 4800
	            ? int(rd16s(rom, CENT_PITCH_TAB + u32(c) * 2))
	            : (c / 1200) * 256
	              + int(rd16s(rom, CENT_PITCH_TAB + u32(c % 1200) * 2));
	return neg ? -v : v;
}

inline int peg_vel_depth(const u8 *elem, int vel)
{
	const int d = int(elem[elem_at::pitch_eg_velocity_depth]) - 64;
	if (!d)
		return 0;
	const int x = 36 * (d > 0 ? d : -d);
	const int m = d > 0 ? (0x80 - (vel & 0x7f)) : (vel & 0x7f);
	return int((u32(s16v(x * m)) * 2 & 0xffff) >> 8);
}

inline int peg_cents(const u8 *elem, int level, int vel)
{
	const int d = level - 64;
	if (!d)
		return 0;
	const int k = peg_vel_depth(elem, vel);
	int m;
	if (d > 0) {
		m = d + 1;                                   // 実機は正の側だけ 1 を足す
		m -= int((u32(s16v(m * k)) & 0xffff) >> 8);
	} else {
		m = d + int((u32(-s16v(d * k)) & 0xffff) >> 8);
		m = -m;
	}
	int v = s16v(m * 75);
	switch (elem[elem_at::pitch_eg_range]) {
	case 0: v = s16v(v) >> 1; v = s16v(v) >> 1; break;
	case 1: v = s16v(v) >> 1; break;
	case 3: v = s16v(v) << 1; break;
	default: break;
	}
	return d > 0 ? v : -v;
}

// **パートの初めの高さ**（08 pp 69）のずらし。要素の高さ（0-127）に足すのではなく、
// **目盛りを ±半オクターブ（音程の包絡線の幅 = 1）に固定して別に音程へ直し、足す**。
// SquareLd（素の高さ 0 ＝ -256）で +63 が +256、GrandPno（素 64）で +63 が +512 と、
// どちらも実機の書いた pitch_env_target に一致
inline int part_peg_cents(int value)
{
	if (value == 64)
		return 0;
	u8 unit[ELEM_BYTES] = {};
	unit[elem_at::pitch_eg_range] = 1;                 // 目盛り: ±半オクターブ
	unit[elem_at::pitch_eg_velocity_depth] = 64;                // 強さは効かせない
	return peg_cents(unit, value, 100);
}

inline int peg_rate_key_adj(const u8 *elem, int note)
{
	const int d = int(elem[elem_at::pitch_eg_rate_key_depth]) - 64;
	if (!d)
		return 0;
	return s16v((note - int(elem[elem_at::pitch_eg_rate_key_center])) * (d * 16)) >> 8;   // 算術シフト（下へ丸める）
}

inline int peg_rate_vel_adj(const u8 *elem, int vel)
{
	const int d = int(elem[elem_at::pitch_eg_rate_velocity]) - 64;
	if (!d)
		return 0;
	const int a = d * 16;
	const int m = a >= 0 ? a * (vel & 0x7f) : (0x80 - (vel & 0x7f)) * (-a);
	return int((u32(s16v(m)) & 0xffff) >> 8);
}

inline int peg_rate_raw(const u8 *elem, int stage)
{
	return int(elem[elem_at::pitch_eg_rates + (stage < 0 ? 0 : (stage > 3 ? 3 : stage))]);
}

// 段ごとの行き先の高さ（段 0-2 と離しの 4 つ）
inline int peg_level_of(const u8 *elem, int stage)
{
	return int(elem[elem_at::pitch_eg_levels + (stage < 0 ? 0 : (stage > 3 ? 3 : stage))]);
}

// **立ち上がりのつまみ（CC73 / 08 pp 1B）も、パートの速さとまったく同じだけ
// 目盛りを動かす**。GrandPno・Strings1・NylonGt を 128 段測って
//   目盛り = 素 + ((64 - つまみ) >> 2)       （算術シフト。下へ丸める）
// だった。つまみ 65-68 で -1、69-72 で -2 …… と 4 段ごとに 1 目盛り。
// 下げる側も同じ式で、NylonGt（素の目盛り 54）は つまみ 48 で 58、
// つまみ 32 で 62 と、4 段ごとに 1 目盛りずつ遅くなる
inline int peg_rate_idx_of(const u8 *elem, int raw, int note, int vel, int part_rate = 64,
                           int cc_atk = 64)
{
	int r = raw + (int(s8(u8(64 - part_rate))) >> 2)
	            + (int(s8(u8(64 - cc_atk))) >> 2);
	if (s8(u8(r)) > 63) r = 63;
	if (s8(u8(r)) < 0)  r = 0;
	r += peg_rate_key_adj(elem, note);
	if (s8(u8(r)) < 0)  r = 0;
	if (s8(u8(r)) >= 63)
		return 63;
	r += peg_rate_vel_adj(elem, vel);
	if (s8(u8(r)) > 62) r = 62;
	return r;
}

// 段 stage の速さの語
inline int peg_rate_reg_stage(const u8 *rom, const u8 *elem, int stage, int note, int vel,
                              int part_rate = 64, int cc_atk = 64)
{
	const int i = peg_rate_idx_of(elem, peg_rate_raw(elem, stage), note, vel,
	                              part_rate, cc_atk);
	return rd16s(rom, PEG_RATE_TAB + u32(i) * 2);
}

// 速さの素を直に渡す版（離しの段で使う）
inline int peg_rate_reg_raw(const u8 *rom, const u8 *elem, int raw, int note, int vel,
                            int part_rate = 64, int cc_atk = 64)
{
	const int i = peg_rate_idx_of(elem, raw, note, vel, part_rate, cc_atk);
	return rd16s(rom, PEG_RATE_TAB + u32(i) * 2);
}

// 離しの速さの頭打ちの表（パートのピッチ EG の離しの時間が 64 より上のとき）

constexpr u32 VIB_TICK    = 882;        // 20ms


inline int amod_word(int sum, int base)
{
	int n = sum < 0 ? 0 : (sum > 15 ? 15 : sum);
	if (n < base)
		n = base;
	return (n * 2) & 0x7f;
}

inline int pmod_word(const u8 *rom, int sum, bool xg = true)
{
	const int s = sum < 0 ? 0 : (sum > 127 ? 127 : sum);
	const u32 tab = xg ? PMOD_CAP_XG : VIB_CAP_TAB;
	return int(rom[VIB_WORD_TAB + u32(rom[tab + u32(s)])]);
}


inline u16 lfo_step(const u8 *rom, int rate)
{ return rd16(rom, LFO_STEP_TAB + u32(rate & 0x3f) * 2); }

inline int lfo_fdep_index(const u8 *elem, int extra)
{
	int n = extra < 0 ? 0 : (extra > 22 ? 22 : extra);
	n = n == 0 ? 0 : (n == 1 ? 4 : ((n + 1) >> 1) + 4);
	const int own = int(elem[elem_at::lfo_filter_depth] & 0x7f);
	return n < own ? own : n;
}

inline int lfo_fdepth(const u8 *rom, const u8 *elem, int extra = 0)
{ return int(rom[LFO_FDEP_TAB + u32(lfo_fdep_index(elem, extra) & 0x7f)]); }

inline int lfo_fwave(u32 phase, bool tri)
{
	const int p = int(phase & 0x7fff);
	if (!tri)
		return (p <= 0x3fff ? p : p - 0x8000) >> 1;
	if (p <= 0x1fff)
		return p;
	if (p <= 0x5fff)
		return 0x4000 - p;
	return p - 0x8000;
}

// 切る高さへ足すぶん。**負のときは絶対値で掛けてから符号を戻す**
inline int lfo_fcut(int wave, int depth)
{
	if (wave >= 0)
		return int((u32(wave) * u32(depth)) >> 9);
	return -int((u32(-wave) * u32(depth)) >> 9);
}

inline u32 lfo_next(u32 phase, u16 step)
{
	const u32 p = (phase + step) & 0xffff;
	return p >= 0x8000 ? p - 0x8000 : p;
}

inline int vib_ramp_word(const u8 *rom, int counter)
{
	const int c = counter < 0 ? 0 : (counter > 63 ? 63 : counter);
	return int(rom[VIB_WORD_TAB + u32(rom[VIB_CNT_TAB + u32(c)])]);
}

inline int vib_ramp_value(const u8 *rom, int dpt, int c1, int c2, bool gs = false)
{
	auto units = [](int d) { return (d & 0x80) ? (d & 0x7f) * 8 : (d & 0x7f); };
	auto table = [&](int c) {
		const int k = c < 0 ? 0 : (c > 127 ? 127 : c);
		return int(rom[VIB_WORD_TAB + u32(rom[VIB_CNT_TAB + u32(k)])]);
	};
	if (dpt < 0)
		dpt = 64;                      // つまみに触れていない
	if (dpt > 127)
		dpt = 127;
	int own = table(c1);
	if (dpt < 64 && gs)
		dpt = 64;                      // TG300B モードでは下げられない
	if (dpt < 64) {
		const int u = units(own) - (64 - dpt) * 14;
		own = u <= 0 ? 0 : (u < 128 ? u : (0x80 | (u / 8 > 127 ? 127 : u / 8)));
	}
	const int ptop = (gs ? VIB_DEPTH_TAB_GS : VIB_DEPTH_TAB)[dpt];
	int pv = 0;
	if (ptop) {
		pv = table(c2);
		if (units(pv) >= units(ptop))
			pv = ptop;
	}
	return units(pv) > units(own) ? pv : own;
}

// Depth のぶんのカウンタが止まる所（これより先は表を引いても同じ）
constexpr int VIB_PART_CNT_END = 127;

inline int vib_delay_ticks(const u8 *elem)
{
	return elem[elem_at::lfo_delay] ? (3 * int(elem[elem_at::lfo_delay])) / 4 + 3 : 0;
}

inline int vib_ramp_target(const u8 *elem) { return int(elem[elem_at::lfo_pitch_depth]); }

// **Vib Depth のぶんのカウンタが頭打ちに着く所**。表を引いた値が Vib Depth の上限（VIB_DEPTH_TAB）に届く最初のカウンタ。
// 深さごとに決まるので、ROM を読んだときに 128 個ぶん作っておく
inline void vib_part_targets(const u8 *rom, const u8 (&depth_tab)[128], u8 (&out)[128])
{
	auto units = [](int d) { return (d & 0x80) ? (d & 0x7f) * 8 : (d & 0x7f); };
	for (int d = 0; d < 128; d++) {
		int c = 0;
		while (c < VIB_PART_CNT_END && units(rom[VIB_WORD_TAB + u32(rom[VIB_CNT_TAB + u32(c)])]) < units(depth_tab[d]))
			c++;
		out[d] = u8(c);
	}
}

// **Vib Depth のぶんのカウンタの 1 歩**。音色のせり上がり（LFO のフェード）の刻みの数で頭打ちに着くように歩む:
//
//   1 歩 = (着く所 + 1) ÷ (せり上がり + 1)  を切り捨て、1 から 5 に収める
//
// 深さ 66-127 とせり上がり 0-24 の 38 通りを実機で測って、どれも合う（深さ 70 なら、せり上がり 0-4 で 5、15 以上で 1）
inline int vib_part_step(const u8 *elem, int target)
{
	constexpr int fastest = 5;
	const int v = (target + 1) / (int(elem[elem_at::lfo_fade]) + 1);
	return v < 1 ? 1 : (v > fastest ? fastest : v);
}

inline int vib_ramp_step(const u8 *elem)
{
	const int t = int(elem[elem_at::lfo_pitch_depth]);
	if (!elem[elem_at::lfo_fade])
		return t > 1 ? t : 1;
	const int v = (t - 1) / int(elem[elem_at::lfo_fade]);   // 割り切れないときは切り上げてから 1 引いた形（LFO のせり上がり=17・LFO の音程の深さ=63 で 3）
	return v > 1 ? v : 1;
}

// **音量側の揺れ**（amplitude_lfo の下位）も同じ遅れで待つ。
// GM で遅れを持つ音色は LFO の音量の深さ がどれも 1（深さ 2）なので、
// せり上がるのか一っ飛びなのかは分からない。測れた範囲では
// 遅れが明けた瞬間に 0 から LFO の音量の深さ × 2 へ一つ飛ぶ
inline int vib_amp_depth(const u8 *elem) { return (int(elem[elem_at::lfo_level_depth]) * 2) & 0x7f; }


inline u32 eq_index(int freq, int gain, int lo, int hi)
{
	const int f = freq < lo ? lo : (freq > hi ? hi : freq);
	const int g = gain < 0 ? 0 : (gain > 127 ? 127 : gain);
	return u32(96 * (f - lo) + 3 * (g >> 2));
}

// 6 つの係数を入れる。語は eq_low_previous から 1 つ飛ばし
inline void eq_set(const u8 *rom, element_words &r, int lo_gain, int hi_gain,
                   int lo_freq, int hi_freq)
{
	if (!rom)
		return;
	const u32 li = eq_index(lo_freq, lo_gain, 4, 40);
	const u32 hi = eq_index(hi_freq, hi_gain, 28, 58);
	for (u32 i = 0; i < 3; i++) {
		r.set(word::eq_low_previous + i, rd16(rom, EQ_LOW_TAB  + (li + i) * 2));
		r.set(word::eq_high_feedback + i, rd16(rom, EQ_HIGH_TAB + (hi + i) * 2));
	}
}

// その音がせり上がりを持つか（持たないものは押した瞬間の値のまま）
inline bool vib_ramps(const u8 *elem)
{
	return elem[elem_at::lfo_wave] < 2 && (elem[elem_at::lfo_pitch_depth] || elem[elem_at::lfo_level_depth]) && (elem[elem_at::lfo_delay] || elem[elem_at::lfo_fade]);
}

inline u16 peg_word(const u8 *rom, int cents, const u8 *elem = nullptr)
{
	const u16 flag = (!elem || elem[elem_at::lfo_phase_reset]) ? LFO_RESET_FLAG : 0;
	return u16(flag | (u16(cents_to_pitch(rom, cents)) & 0x3fff));
}


// 0 の側へ丸める >>8（実機は符号で分けている）
inline int sh8(int v) { return v >= 0 ? (v >> 8) : -((-v) >> 8); }


inline int fenv_depth(const u8 *rom, const u8 *elem, int vel)
{
	if (!rom || !elem)
		return 0;
	const int d = int(elem[elem_at::filter_eg_velocity_depth]) - 64;
	const u32 tab = elem[elem_at::filter_eg_velocity_table] ? FENV_VEL_TAB1 : FENV_VEL_TAB0;
	const int t = rom[tab + u32(vel & 0x7f)];
	// 負の深さは強さの向きが逆（強いほど包絡線が浅くなる）
	const int v = d >= 0 ? (36 * d) * (0x80 - t) : (36 * -d) * t;
	return int((u32(v) * 2 & 0xffff) >> 8);
}

// レベルのバイト → 目標
inline int fenv_target(const u8 *rom, const u8 *elem, int level, int vel)
{
	const int x = (level - 64) * 2;
	return (x - sh8(x * fenv_depth(rom, elem, vel))) * 64;
}

// 速さへの足し込み。鍵のぶん（包絡線の速さの鍵の深さ が深さ・包絡線の速さの基準鍵 が基準鍵）と
// 強さのぶん（包絡線の速さの強さ が深さ）
inline int fenv_key_adj(const u8 *elem, int note)
{
	const int d = int(elem[elem_at::filter_eg_rate_key_depth]) - 64;
	return d ? sh8((note - int(elem[elem_at::filter_eg_rate_key_center])) * (d * 16)) : 0;
}
inline int fenv_vel_adj(const u8 *elem, int vel)
{
	const int d = int(elem[elem_at::filter_eg_rate_velocity]) - 64;
	if (!d)
		return 0;
	const int a = d * 16;
	return sh8(a >= 0 ? a * vel : (-a) * (0x80 - vel));   // 負の深さは弱いほど速い
}

// 速さ → 増分。63 以上は「すぐ次の段」の印
constexpr int FENV_NEXT = 0x8000;
inline int fenv_inc(const u8 *rom, int rate)
{
	if (rate >= 63)
		return FENV_NEXT;
	if (rate < 0)
		rate = 0;
	return rd16s(rom, FENV_INC_TAB + u32(rate) * 2);
}



inline int cutoff_key_curve(const u8 *rom, const u8 *elem, int note)
{
	if (!rom || !elem || elem[elem_at::cutoff_curve_kind] != 0xff)
		return 0;        // 折れ線の枝はまだ起こしていない
	const u32 row = (u32(elem[elem_at::cutoff_curve_row_high]) << 8 | elem[elem_at::cutoff_curve_row_low]) * 128;
	const u32 a = LEVEL_CURVE + row + u32(note & 0x7f);
	return s8(rom[a]) * 32;
}


// 音量の鍵による増減。記録の 音量の曲線の種類 が 0xFF のときは ROM の曲線表を引く
// （曲線の行の上位,曲線の行の下位 が行の番号）。符号付きで、鍵ごとに ±10 ほど動く
inline int level_key_curve(const u8 *rom, const u8 *elem, int note)
{
	if (elem[elem_at::level_curve_kind] != 0xff)
		return 0;                       // 折れ線の形はまだ入れていない
	const u32 idx = u32(elem[elem_at::level_curve_row_high]) << 8 | elem[elem_at::level_curve_row_low];
	const u32 a = LEVEL_CURVE + idx * 128 + u32(note & 0x7f);
	if (a >= PROGRAM_ROM_SIZE)
		return 0;
	return int(s8(rom[a]));
}

inline int wave_level(const u8 *rom, const u8 *elem, int note, int shift = 0)
{
	const u8 *we = wave_entry(rom, wave_set(elem), wave_note(rom, elem, note, shift));
	return we ? int(we[0]) : 0;
}

inline int level_curve_scaled(const u8 *rom, const u8 *elem, int note)
{
	const int v = level_key_curve(rom, elem, note) * 2;
	return int(s8(u8(v)));                  // 実機は 1 バイトに詰めて持つ
}

constexpr int VOL_GAIN_DEF = ((100 + 1) * (127 + 1)) >> 7;      // = 101


inline int voice_raw_level(const u8 *rom, u32 rec, const u8 *elem)
{
	if (!rom || !rec)
		return 64;
	// **切り捨て**。前は四捨五入（+49）にしていたが、それは減衰の表から
	// 逆に引いた値で当てていたので外していた。実機の声の音量の目盛りを
	// 直に読むと、GM の 128 音色 112 件で
	// **切り捨てが 111 件合い、四捨五入は 61 件しか合わない**
	return int(rom[rec + VOICE_LEVEL]) * int(elem[elem_at::level]) / 99;
}

inline int volume_level(const u8 *rom, u32 rec, const u8 *elem, int note)
{
	int l = voice_raw_level(rom, rec, elem) + level_curve_scaled(rom, elem, note);
	if (l < 0) l = 0;
	return l > 128 ? 128 : l;
}

// 目盛りに乗らない側の減衰（強さと、波形の段ぶん）。
// 波形の記録の先頭のバイトが、その段ぶんの減衰。多段サンプルの音色では
// 段の変わり目で 1.5dB ほど動くので、これを入れないと段ごとにずれる
inline int volume_rest(const u8 *rom, const u8 *elem, int note, int vel, int shift = 0)
{
	return velocity_att(rom, vel_shift(elem, vel), vel_curve_of(elem))
	     + wave_level(rom, elem, note, shift);
}

inline int volume_att_from(const u8 *rom, int level, int rest, int gain,
                           int add = 0)
{
	int l = level_with_gain(level, gain);
	if (add) {
		l += add;
		if (l < 0) l = 0;
		if (l > 128) l = 128;
	}
	int a = int(rom[LEVEL_TAB + 0x80 + u32(l)]) + rest;
	if (a > 127) a = 127;
	if (a < 0) a = 0;
	return a * 2;
}

inline int rate_key_corr(const u8 *elem, int note)
{
	int c = (note - int(elem[elem_at::rate_key_center])) * (int(elem[elem_at::rate_key_depth]) - 64) * 16;
	if (c < 0)
		c += 0xff;
	return c >> 8;
}

// 減衰の表の目盛りを 0-127 に収める
inline int clamp_idx(int i) { return i < 0 ? 0 : (i > 127 ? 127 : i); }

inline int rate_scale(int raw, int corr)
{
	int v = raw + corr;
	if (v <= 0) v = 1;
	if (v > 63) v = 63;
	return v * 2;
}

inline int rate_scale2(int raw, int corr)
{
	int v = raw + corr;
	if (v < 0) v = 0;
	if (v > 63) v = 63;
	return v * 2;
}

inline int reso_vel_drop(const u8 *elem, int vel)
{
	const int d = int(elem[elem_at::resonance_velocity]) - 64;
	if (!d)
		return 0;
	const int x = 18 * (d > 0 ? d : -d);
	const int m = d > 0 ? (0x80 - (vel & 0x7f)) : (vel & 0x7f);
	return int((u32(x * m) & 0xffff) >> 8);
}

// **1 ビット落とす前の値**。切る高さの頭打ちはこちらで見る
inline int reso_raw(const u8 *elem, int vel, int part_res = 64)
{
	int v = int(elem[elem_at::resonance]) - reso_vel_drop(elem, vel);
	if (v < 0)
		v = 0;
	const int p = part_res - 64;
	int r = p >= 0 ? (p >= v ? p : v) : p + v;
	return r < 0 ? 0 : r;
}

inline int reso_level(const u8 *elem, int vel, int part_res = 64)
{
	return (reso_raw(elem, vel, part_res) >> 1) & 31;
}

constexpr int REL_RATE_OFF = 1;         // 離しの速さの足し込み
// **足し込みは 1**。前は 3 としていたが、実機の値を
// 読むと 1 だった。3 だとつまみ 88 以上で
// 目盛りが 1 つ分速くなる（Strings も GrandPno も）

inline int attack_idx(int rate)
{
	const int r = (rate <= 0 ? 4 : rate) * 2;
	return r > 126 ? 126 : r;
}

// TG300B（GS）モードの上側は同じ表を、プログラムごとのずれで引く（128 音色 × 29 段で確かめた）

inline int rel_rate_cc(const u8 *rom, int base, int cc, int gs_program = XG_PROGRAM)
{
	if (cc < 0 || cc == 64)
		return base;
	const int c = cc > 127 ? 127 : cc;
	if (c < 64) {
		if (base > 55)
			return base;            // 素が速ければ下げる側は効かない
		const int v = base + (65 - c) / 2;
		return v > 55 ? 55 : v;
	}
	if (!rom)
		return base;
	const int shift = gs_program == XG_PROGRAM ? REL_RATE_OFF
	                                           : int(rom[GS_REL_RATE_SHIFT_TAB + u32(gs_program & 0x7f)]);
	const int t = int(rom[REL_RATE_CC + u32(c - 64 + shift)]);
	return t < base ? t : base;
}

// 立ち上がりの語。CC73 を動かすと、鳴っている音にもそのまま書き直す
inline u16 attack_word(const u8 *rom, const u8 *elem, int cc_atk)
{
	const int arate = eg_rate_cc(rom, int(elem[elem_at::attack_rate]), cc_atk);
	return u16(rom[ATTACK_TAB + u32(attack_idx(arate))] << 8 | (arate >= ATTACK_FASTEST_RATE ? 0 : ATTACK_START_LEVEL));
}

inline u16 release_word(const u8 *rom, const u8 *elem, int note, int att,
                       int cc_rel = 64, int gs_program = XG_PROGRAM)
{
	const int r = rom[REL_TAB + rate_scale(
	                  rel_rate_cc(rom, int(elem[elem_at::release_rate]), cc_rel, gs_program), rate_key_corr(elem, note))];
	return u16(((RELEASE_KEY_FLAG | (r & 0x7f)) << 8) | (att & 0xff));
}

// **ダンパーを踏んだとき、離している最中の音に入れる値**。
// 実機は「ペダルが拾った」形で、離しをやめて**減衰 2 の速さに戻す**
// （8 音色で、語 second_decay の上位と 1 ビット違わず同じ値だった）。
// ビット 15 の「離せ」の印は立てたまま
inline u16 damper_hold_word(const u8 *rom, const u8 *elem, int note, int att,
                           int cc_dec = 64)
{
	const int corr = rate_key_corr(elem, note);
	const int r = rom[DECAY_TAB + clamp_idx(
	                  rate_scale2(eg_rate_cc_add(int(elem[elem_at::second_decay_rate]), cc_dec), corr))];
	return u16(((RELEASE_KEY_FLAG | (r & 0x7f)) << 8) | (att & 0xff));
}

inline int fenv_rate_cc(const u8 *rom, int base, int cc)
{
	if (!rom || cc < 0)
		return base;
	const int c = cc & 0x7f;
	if (c == 64)
		return base;
	const int t = int(rom[FENV_ATK_CC + u32(c)]);
	return c > 64 ? (t < base ? t : base) : (t > base ? t : base);
}

inline int fenv_atk_rate(const u8 *rom, const u8 *elem, int cc_atk = 64)
{
	const int base = int(elem[elem_at::filter_eg_attack_rate]);
	if (!rom)
		return base;
	int cc = cc_atk & 0x7f;
	if (cc < 64 && int(elem[elem_at::filter_eg_attack_level]) < int(elem[elem_at::filter_eg_start_level]))
		cc = 64;                        // 下がる包絡線には効かない
	return fenv_rate_cc(rom, base, cc);
}

// **すぐ段 0 の行き先まで行くか**。実機は目盛りが 63 以上なら増分に
// 0x8000（＝すぐ次の段）を入れる。**鍵の補正を足したあともう一度見る**
// ので、つまみで 63 を割っていても鍵の補正で戻ることがある
inline bool fenv_atk_instant(const u8 *rom, const u8 *elem, int cc_atk = 64,
                             int note = 60)
{
	const int r = fenv_atk_rate(rom, elem, cc_atk);
	if (r >= 63)
		return true;
	int k = r + fenv_key_adj(elem, note);
	if (k < 0)
		k = 0;
	return k >= 63;
}

// **初めの高さ**。すぐ行き先まで行くなら段 0 の行き先（包絡線の段 0 の高さ）、
// そうでなければ 包絡線の初めの高さ から登る。つまみで 63 を割ると、ここが切り替わる
inline int fenv_start_level(const u8 *elem, bool instant)
{
	return instant ? elem[elem_at::filter_eg_attack_level] : elem[elem_at::filter_eg_start_level];
}

// **ソフトペダル（CC67）はフィルタの包絡線の強さを 32 下げる**。
// 実機は踏んでいるあいだ、深さを「強さ - 32」で作り直す。
// 音量・共振・減衰・立ち上がりはまったく動かない
inline int soft_vel(int vel, bool soft)
{
	if (!soft)
		return vel;
	const int v = vel - 32;
	return v < 0 ? 0 : v;
}

inline int fenv_init(const u8 *rom, const u8 *elem, int vel, int cc_atk = 64,
                     int note = 60)
{
	return fenv_target(rom, elem,
	                   fenv_start_level(elem,
	                                    fenv_atk_instant(rom, elem, cc_atk, note)),
	                   vel);
}

// `cap` を false にすると、**共振が浅いときの頭打ち（0x7C0）を掛けない**
// 値を返す。明るさのつまみ（CC74）は**頭打ちの前**に効くので、
// つまみを下げる曲では素の値から引かないと 7 ずれる
inline u16 cutoff_of(const u8 *rom, const u8 *elem, int note, int vel, int facc,
                     bool cap = true)
{
	int cut = int(rd16(rom, CUTOFF_TAB + u32(elem[elem_at::cutoff]) * 2))
	        + cutoff_key_curve(rom, elem, note);
	cut = cut < 0 ? 0 : (cut > 0xfff ? 0xfff : cut);
	cut += facc >> 2;
	if (cut <= 0x800) cut = 1;
	if (cut > 0xfff) cut = 0xfff;
	cut &= 0x7ff;
	if (cap && reso_level(elem, vel) < 4 && cut > CUTOFF_MAX)
		cut = CUTOFF_MAX;
	return u16(LOWPASS_WORD_FLAG | u16(cut));
}

// `fvel` はフィルタの包絡線の深さだけに使う強さ（ソフトペダルで下がる）。
// 共振の頭打ちは素の強さのまま
inline u16 cutoff_keyon(const u8 *rom, const u8 *elem, int note, int vel,
                        bool cap = true, int cc_atk = 64, int fvel = -1)
{
	return cutoff_of(rom, elem, note, vel,
	                 fenv_init(rom, elem, fvel < 0 ? vel : fvel, cc_atk, note), cap);
}

// 共振が浅ければ頭打ちを掛ける（つまみを効かせたあとに使う）
// **頭打ちの判定にはつまみを入れた共振を使う**。
// CC71 を上げると実機は頭打ちを外すのに、素の値で見ていたので
// SquareLead の CC71 = 96 だけ 7 だけ暗かった
inline u16 cutoff_cap(u16 v, const u8 *elem, int vel, int part_res = 64)
{
	int cut = int(v & 0xfff);
	if (reso_level(elem, vel, part_res) < 4 && cut > CUTOFF_MAX)
		cut = CUTOFF_MAX;
	return u16((v & 0xf000) | u16(cut));
}


inline int elem_pan(const u8 *rom, const u8 *elem, int note)
{
	const int i = int(elem[elem_at::pan_select]);
	return i == 15 ? int(rom[PAN_SEL_KEY + u32(note & 0x7f)])
	               : int(rom[PAN_SEL_TAB + u32(i & 0xf)]);
}

// パンの位置（0-127）
inline int voice_pan_pos(const u8 *rom, const u8 *elem, int note, int cc10 = 64)
{
	const int p = cc10 + elem_pan(rom, elem, note) - 64;
	return p < 0 ? 0 : (p > 127 ? 127 : p);
}


inline int pan_send_adj(const u8 *rom, int pan_pos)
{
	return int(rom[PAN_SEND_TAB + u32(pan_pos & 0x7f)])
	     - int(rom[PAN_SEND_TAB + 64]);
}

inline u16 pan_rnd_word(const u8 *rom, int r)
{
	if (!rom)
		return PAN_CENTER_WORD;
	const int q = r < 0 ? 0 : (r > 127 ? 127 : r);
	int l = int(rom[PAN_BASE_TAB + u32(q)]);
	int rr = int(rom[PAN_BASE_TAB + u32(0x80 - q)]);
	if (l > 255) l = 255;
	if (rr > 255) rr = 255;
	return u16((l << 8) | rr);
}

// **インサーションを通るときの pan_mix**。
// パートのパン（CC10）は**まったく見ない**で、
// 音色（打）自身のパンだけを左に入れ、右は 0
inline u16 ins_pan_word(const u8 *rom, int pan_pos)
{
	if (!rom)
		return 0;
	const int p = pan_pos < 0 ? 0 : (pan_pos > 127 ? 127 : pan_pos);
	int l = int(rom[PAN_CURVE_TAB + u32(p)]);
	int r = int(rom[PAN_CURVE_TAB + u32(0x80 - p)]);
	if (l > 255) l = 255;
	if (r > 255) r = 255;
	return u16((l << 8) | r);
}

inline u16 voice_pan_word(const u8 *rom, const u8 *elem, int note,
                         int cc10 = 64, int part_pan = 64)
{
	const int p = voice_pan_pos(rom, elem, note, cc10);
	const int q = part_pan & 0x7f;
	int l = int(rom[PAN_BASE_TAB + u32(q)]) + int(rom[PAN_CURVE_TAB + u32(p)]);
	int r = int(rom[PAN_BASE_TAB + u32(0x80 - q)])
	      + int(rom[PAN_CURVE_TAB + u32(0x80 - p)]);
	if (l > 255) l = 255;
	if (r > 255) r = 255;
	return u16((l << 8) | r);
}

constexpr u8 SILENT_KIT = 0x80;   // 打の記録を持たないキット
inline const u8 *drum_record(const u8 *rom, int kit, int note)
{
	if (!rom || (kit & SILENT_KIT))
		return nullptr;
	const u32 base = rd32(rom, DRUM_KIT_TABLE + u32(kit & 0x7f) * 4);
	if (base < DRUM_KIT_RECORDS_FIRST || base > DRUM_KIT_RECORDS_LAST)
		return nullptr;
	const u16 off = rd16(rom, base + u32(note & 0x7f) * 2);
	return off == 0xffff ? nullptr : rom + DRUM_RECORDS + off;
}

inline int drum_setup_kit_default(const u8 *rom, const u8 *rec, int idx)
{
	constexpr u32 FROM_REC = 0x7ffu & ~3u | 0xfu << 16 | 3u << 21;   // 2-10・16-19・21-22
	if (idx < 0 || idx > 22)
		return 64;
	if (!rec)
		return idx < 11 ? rom[EMPTY_DRUM_RECORD + u32(idx)] : settings::drum_setup_default(idx);
	if ((FROM_REC >> idx) & 1)
		return rec[idx];
	return settings::drum_setup_default(idx);
}

inline bool drum_rec_has_wave(const u8 *rec)
{
	return rec && rd16(rec, drum_at::sfx_voice_high) == 0xffff;
}

constexpr int SFX_NOTE        = 64;         // 波形と音程を決める鍵（固定）

inline void sfx_setup_apply(u8 *e, int cut, int reso, int atk, int dec1, int dec2)
{
	auto add = [&](int i, int d, int hi) { e[i] = u8(std::clamp(int(e[i]) + d, 0, hi)); };
	add(elem_at::cutoff, cut - 64, 127);
	add(elem_at::resonance, (reso - 64) >> 1, 63);
	add(elem_at::attack_rate, atk - 64, 63);
	add(elem_at::first_decay_rate, dec1 - 64, 63);
	add(elem_at::second_decay_rate, dec2 - 64, 63);
}


inline u32 sfx_voice_record(const u8 *rom, const u8 *drec)
{
	if (!rom || !drec || drec[drum_at::sfx_voice_high] == 0xff)
		return 0;
	const u32 idx = (u32(drec[drum_at::sfx_voice_high]) << 7) | (drec[drum_at::sfx_voice_low] & 0x7f);
	const u32 slot = SFX_VOICE_TABLE + idx * 4;
	const u32 rec = SFX_VOICES + rd32(rom, slot) * 2;
	return (rec >= SFX_VOICES && rec + 16 <= SFX_VOICES_END) ? rec : 0;
}

// 打の音程。XG は記録の pitch_offset のずれ。TG300B（GS）モードでは、gs_base_key が 64 でない打は
// その値が基準の鍵になり、ずれは gs_pitch_offset（TG300B だけの音程を持つ打）
inline int drum_cents(const u8 *rec, int coarse = 64, int fine = 64, bool gs = false)
{
	const int semi = (gs && rec[drum_at::gs_base_key] != 64 ? coarse - 64 + int(rec[drum_at::gs_base_key]) + s8(rec[drum_at::gs_pitch_offset]) : coarse + s8(rec[drum_at::pitch_offset])) - int(rec[drum_at::wave_base_key]);
	return semi * 100 + (int(rec[drum_at::fine_tune]) - 64) + (fine - 64);
}

inline int detune_cents(int hi, int lo)
{
	const int d = (hi << 8) + (lo << 4) - 0x800;
	constexpr int DETUNE_UP_SCALE = 0x63a, DETUNE_DOWN_SCALE = 0x659;
	const int m = d * (d >= 0 ? DETUNE_UP_SCALE : DETUNE_DOWN_SCALE);
	return m >= 0 ? m >> 16 : -((-m + 0xffff) >> 16);
}

constexpr int DRUM_PITCH_LOWEST = -0x1fff, DRUM_PITCH_HIGHEST = 0x1bff, DRUM_PITCH_HIGHEST_FORMAT3 = 0x7ff;

inline u16 drum_pitch_word(const u8 *rom, const u8 *rec, int cents)
{
	int c = cents < 0 ? -cents : cents;
	if (c > 9600)
		c = 9600;
	const int t = int(rd16(rom, DRUM_PITCH_TAB + u32(c) * 2) & 0x3fff);
	const u32 addr = u32(rec[drum_at::wave_format]) << 24 | u32(rec[drum_at::wave_format + 1]) << 16 | u32(rec[drum_at::wave_format + 2]) << 8 | rec[drum_at::wave_format + 3];
	const bool fmt3 = ((addr >> 30) & 3) == 3;
	// **音程には上下の頭打ちがある**（打の粗調を 0-127 と振って確かめた）。
	// 下は -0x1FFF（TknoKtLo の鍵 38）、上は 0x1BFF（7 オクターブ。SLatinKt の鍵 36）、**形式 3 の波形は上が 0x7FF**（2 オクターブ。China Kit の鍵 38）
	const int sv = std::clamp(cents < 0 ? -t : t, DRUM_PITCH_LOWEST, fmt3 ? DRUM_PITCH_HIGHEST_FORMAT3 : DRUM_PITCH_HIGHEST);
	u16 v = u16(sv & 0x3fff);
	if (fmt3)
		v = u16(v | PITCH_FORMAT3_FLAG);
	return v;
}

inline int drum_rec_idx(const u8 *rec, int i, int v)
{
	const int x = int(rec[i] & 0x7f) + (v < 0 ? 64 : v) - 64;
	return x < 0 ? 0 : (x > 127 ? 127 : x);
}

inline int drum_atk_idx(const u8 *rec, int atk) { return drum_rec_idx(rec, drum_at::attack_rate, atk); }

inline int drum_cut_idx(const u8 *rec, int cut) { return drum_rec_idx(rec, drum_at::cutoff, cut); }

// **減衰 1 と減衰 2 は別の値**（3n rr 0E・0F）。NRPN 17 は両方に同じ値を入れる
inline element_words drum_note(const u8 *rom, const u8 *rec, int att,
                           const defaults &d = defaults(),
                           int coarse = 64, int fine = 64, int atk = 64,
                           int cut = 64, int reso = 64, int dec1 = 64, int dec2 = 64,
                           int cents_extra = 0, bool gs = false,
                           int part_cut = 64, int part_reso = 64, int part_atk = 64, int part_dec = 64)
{
	element_words r;
	if (!rom || !rec)
		return r;
	// パートのつまみ（CC71・CC74）は打にも効く。旋律と同じ形で、共振は上げる側は大きいほう、下げる側は引き算
	const int reso_base = drum_rec_idx(rec, drum_at::resonance, reso) >> 2;
	const int reso_now = part_reso == 64 ? reso_base
	                   : part_reso > 64 ? std::max(reso_base, reso_shift(part_reso))
	                                    : std::max(reso_base + reso_shift(part_reso), 0);
	{
		// **共振が浅いと切る高さは 0x7C0 で頭打ち**（旋律と同じ）。つまみのあとで頭打ちする
		int c0 = int(rd16(rom, CUTOFF_TAB + u32(drum_cut_idx(rec, cut)) * 2) & 0x7ff);
		if (part_cut != 64)
			c0 = std::clamp(c0 + bright_shift(part_cut, gs), 0, 0x7ff);
		if (reso_now < 4 && c0 > CUTOFF_MAX)
			c0 = CUTOFF_MAX;
		r.set(word::lowpass_cut, u16(LOWPASS_WORD_FLAG | u16(c0)));
	}
	r.set(word::highpass_cut, filter2_word(rec[drum_at::highpass]));
	r.set(word::resonance, u16(u16(reso_now) << 11));
	r.set(word::amplitude_lfo, d.lfo_amp);
	// **速さの表は 2 倍しない**（旋律は rate_scale で 2 倍する）
	{
		// パートの立ち上がり（CC73）は旋律と同じ表。記録の値は目盛りの 2 倍で、打は目盛り 16 より遅くならない
		constexpr int DRUM_SLOWEST_ATTACK = 16;
		const int ai = part_atk == 64 ? drum_atk_idx(rec, atk) : attack_idx(std::max(eg_rate_cc(rom, drum_atk_idx(rec, atk) / 2, part_atk), DRUM_SLOWEST_ATTACK));
		r.set(word::attack, u16(u16(rom[ATTACK_TAB + u32(ai)]) << 8
		                | (ai >= 126 ? 0 : ATTACK_START_LEVEL)));
	}
	r.set(word::first_decay, u16(u16(rom[DECAY_TAB + u32(drum_rec_idx(rec, drum_at::first_decay_rate, dec1))]) << 8 | DRUM_FIRST_DECAY_LEVEL));
	// パートの減衰（CC75）は減衰 2 にだけ効く
	const int dec2_idx = part_dec == 64 ? drum_rec_idx(rec, drum_at::second_decay_rate, dec2) : rate_scale2(eg_rate_cc_add(drum_rec_idx(rec, drum_at::second_decay_rate, dec2) / 2, part_dec), 0);
	r.set(word::second_decay, u16(u16(rom[DECAY_TAB + u32(dec2_idx)]) << 8
	                | u16(((0x7f - int(rec[drum_at::second_decay_level])) * 2) & 0xff)));
	r.set(word::release, u16(att & 0xff));
	r.set(word::lfo, DRUM_LFO_WORD);
	r.set(word::pitch_env_rate, 0);
	r.set(word::pitch_env_target, 0);
	r.set(word::pitch, drum_pitch_word(rom, rec, drum_cents(rec, coarse, fine, gs) + cents_extra));
	const wave_info w = read_wave(rec + drum_at::wave);
	{
		// 逆向きのサンプルは 2 つの数を入れ替える。
		// StandKit# のタム 3 つと AnalogKit の鍵 28 がこれ
		u32 pre = w.pre_loop, loop = w.loop_len;
		wave_backwards_swap(pre, loop);
		r.set(word::lead_high, u16(pre >> 16));
		r.set(word::lead_low, u16(pre));
		r.set(word::loop_high, u16(loop >> 16));
		r.set(word::loop_low, u16(loop));
	}
	r.set(word::wave_high, u16(w.format_addr >> 16));
	r.set(word::wave_low, u16(w.format_addr));
	for (int i = 0; i < 6; i++)
		r.set(word::eq_low_previous + i, d.iir[i]);
	for (int i = 0; i < 6; i++)
		r.set(word::pan_mix + i, d.mix[i]);
	return r;
}


// **ドラムは強さの曲線が 1**（旋律は要素の 強さの曲線 で選ぶ）。
// 強さ 40/100/127 で 22/4/0。曲線 0 なら 26/5/0 で合わない
constexpr int DRUM_VEL_CURVE = 1;

constexpr int DRUM_ATT_WITHOUT_ROM = 0x40;

inline int drum_att(const u8 *rom, const u8 *rec, int level, int vel,
                    int gain = VOL_GAIN_DEF)
{
	if (!rom)
		return DRUM_ATT_WITHOUT_ROM;
	// **打の音量が 0 なら減衰をいっぱいにする**（実機は `0xFE`。StandKit の鍵 38 で確かめた）
	if (level <= 0)
		return SILENT_ATT;
	const int adj = rec ? int(s8(rec[drum_at::level_offset])) : 0;
	int l = (level + adj + 1) * (gain > 128 ? 128 : (gain < 0 ? 0 : gain)) >> 7;
	if (l < 1) l = 1;
	if (l > 128) l = 128;
	int a = int(rom[LEVEL_TAB + 0x80 + u32(l)])
	      + velocity_att(rom, vel, DRUM_VEL_CURVE);
	if (a > 127) a = 127;
	if (a < 0) a = 0;
	return a * 2;
}

// pan_mix（パン）。**57 打すべて実機と一致**。旋律の `voice_pan_word` と同じ形で、
// 音色のパンの代わりにドラムセットアップのパンを使う
inline u16 drum_pan_word(const u8 *rom, int pan, int part_pan = 64)
{
	if (!rom)
		return PAN_CENTER_WORD;
	const int p = pan < 0 ? 64 : (pan > 127 ? 127 : pan);
	const int q = (part_pan < 0 ? 64 : part_pan) & 0x7f;
	int l = int(rom[PAN_BASE_TAB + u32(q)]) + int(rom[PAN_CURVE_TAB + u32(p)]);
	int r = int(rom[PAN_BASE_TAB + u32(0x80 - q)])
	      + int(rom[PAN_CURVE_TAB + u32(0x80 - p)]);
	if (l > 255) l = 255;
	if (r > 255) r = 255;
	return u16((l << 8) | r);
}

// reverb_send・chorus_send の下位（送り）。**ドラム 57 打すべて実機と一致**。
// パートの送り（CC91/CC93）と**打ごとの送り**を掛け合わせてから表を引き、
// 真ん中で 16 の下駄、パンで振るぶん目減りする。
// **旋律にも使える**（打ごとの送りを 127 にすれば掛け算が消える）
inline int send_level_att(const u8 *rom, int part_send, int extra_send, int pan)
{
	if (!rom)
		return 0xff;
	const int ps = part_send < 0 ? 40 : (part_send > 127 ? 127 : part_send);
	const int ds = extra_send < 0 ? 127 : (extra_send > 127 ? 127 : extra_send);
	const int eff = (ps * ds) / 127;
	const int v = 16 + send_att(rom, eff) + pan_send_adj(rom, pan < 0 ? 64 : pan);
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

// 1 音ぶんの語を作る。att は release に入れる減衰（0-255。小さいほど大きい音）
inline element_words build_note(const u8 *rom, const u8 *elem, int note, int att,
                            const defaults &d = defaults(), int cents_extra = 0,
                            int vel = 100, int cc_atk = 64, int cc_dec = 64,
                            int cc_vrate = 64, int cc_vdep = 64, int wnote = -1,
                            int knote = -1, bool soft = false,
                            int part_peg_init = 64, int part_peg_atk = 64, int gs_program = XG_PROGRAM, int shift = 0)
{
	element_words r;
	// **移調・ノートシフト・粗調は「鍵の曲線」には効かない**。
	// 波形と音程はずらした鍵、切る高さ・減衰の速さ・音程の包絡線は
	// **押した鍵そのもの**で引く。GrandPno を -24 半音、SquareLd を
	// -3 半音、粗調（RPN 2）・ノートシフト（08 pp 08）・
	// マスター移調（00 00 06）の 3 通りで確かめた。どれも同じ
	const int kn = knote < 0 ? note : knote;
	// **CC84 で滑り出す音は、波形を「滑り出す鍵」で選ぶ**
	const u8 *we = wave_entry(rom, wave_set(elem),
	                          wave_note(rom, elem, wnote < 0 ? note : wnote, shift));
	if (!we)
		return r;
	const wave_info w = read_wave(we);

	r.set(word::lowpass_cut, cutoff_keyon(rom, elem, kn, vel, true, cc_atk, soft_vel(vel, soft)));
	// **鍵を押した瞬間の highpass_cut は 0xFFFF**（実機は毎回そう書いて、最初の
	// 包絡線の目で本当の値に置き換える）。14 音色を実機と突き合わせて
	// 確かめた
	r.set(word::highpass_cut, filter2_word(elem[elem_at::highpass]));
	r.set(word::resonance, u16(reso_level(elem, vel) << 11));
	r.set(word::amplitude_lfo, u16((d.lfo_amp & 0xff00)
	                | u16((elem[elem_at::lfo_delay] || elem[elem_at::lfo_fade]) ? 0 : ((elem[elem_at::lfo_level_depth] * 2) & 0x7f))));
	// LFO の型と刻み。上位は LFO の速さ に**LFO の波形 が 0 でなければ** 0x40 を足したもの
	// （Rain は LFO の波形が 0）。下位は**音程の深さ = LFO の音程の深さ × 3**
	// （PanFlute の LFO の音程の深さ=1 で 3、ChiffLead・TnklBell・Helicopter の 2 で 6）
	// 深さは amplitude_lfo と同じく、**LFO の遅れとせり上がりがどちらも 0 のとき**だけ。
	//
	// そのうえで **LFO の波形 が 2 だと音程の深さは 0** になる。音色の記録
	// 全部（962 件）で LFO の遅れ・LFO のせり上がり が 0 かつ LFO の音程の深さ が 0 でない要素は
	// BirdTweet（LFO の波形=2・LFO の音程の深さ=6）と Choral（LFO の波形=2・LFO の音程の深さ=1）の 2 つだけ
	// で、実機はどちらも 0 を書く。LFO の位相合わせ=0 の組（JumpBrss・StdiumOr）は
	// ちゃんと深さを書くので、効いているのは LFO の位相合わせ ではなく LFO の波形 のほう。
	// **音量側（amplitude_lfo）は 0 にならない**（Choral の LFO の音量の深さ=13 → 26 が一致）
	// **ビブラートのつまみ**（08 pp 15・16）で速さも深さも動く。
	// 止まっている音色（遅れ LFO の遅れ・LFO のせり上がり があるもの、LFO の波形 が 2）は
	// つまみを回しても動かない。**素の深さが 0 でも、つまみでは動く**
	// （SquareLd は素が 0 で、つまみ 96 のとき実機は 0xa0）
	// **遅れを持つ音色は 0 から始めて、20ms ごとにせり上げる**。
	// LFO の波形 が 2 以上の音色はそもそも揺れない
	const bool vgate = (elem[elem_at::lfo_delay] || elem[elem_at::lfo_fade] || elem[elem_at::lfo_wave] >= 2);
	// **bit7（8 倍の目盛り）は落とさない**。LFO の音程の深さ が 62・63 の要素は表の値が
	// 0x96 で、実機もそのまま下位に書く（SFX Kit1 の鍵 28・29 で確かめた）。
	// `& 0x7f` で落としていた。旋律の記録 1353 個のうち変わるのは 9 要素だけ
	const int plfo0 = vgate ? 0 : vib_ramp_word(rom, elem[elem_at::lfo_pitch_depth]);
	const int lrate = vib_rate(int(elem[elem_at::lfo_rate] & 0x3f), cc_vrate, rom, gs_program);
	const int plfo  = vgate ? 0 : vib_depth(plfo0, cc_vdep, gs_program != XG_PROGRAM);
	r.set(word::lfo, u16(((((elem[elem_at::lfo_wave] ? LFO_TRIANGLE_FLAG : 0) | (lrate & 0x3f)) << 8))
	                | u16(plfo & 0xff)));
	// 音程の包絡線。速さが 127（即到達）のときだけ初めの高さは 音程の包絡線の段 0 の高さ を使う
	// **パートのピッチ EG**（XG の 08 pp 69・6A）。
	// アタックの時間は、素の速さの目盛りを**立ち上がりのつまみと同じ表**（eg_rate_cc）で
	// 動かす: 64 より上は表の値で頭打ち（遅く）、下は足す（速く。63 で止まる）。
	// だから素が即到達（63）の音色では、下げても何も変わらない。
	// 初めの高さは素の 音程の包絡線の初めの高さ にずらし量を足す
	const int praw = part_peg_atk == 64 ? int(elem[elem_at::pitch_eg_rates]) : eg_rate_cc(rom, int(elem[elem_at::pitch_eg_rates]), part_peg_atk);
	const int prate = peg_rate_reg_raw(rom, elem, praw, kn, vel, 64, cc_atk);
	r.set(word::pitch_env_rate, u16(prate << 8));
	r.set(word::pitch_env_target, peg_word(rom, prate == 127 ? peg_cents(elem, elem[elem_at::pitch_eg_levels], vel)
	                                      : peg_cents(elem, elem[elem_at::pitch_eg_start_level], vel) + part_peg_cents(part_peg_init),
	                   elem));

	const int corr = rate_key_corr(elem, kn);
	// **立ち上がりのつまみ（CC73）で目盛りが動く**
	const int arate = eg_rate_cc(rom, int(elem[elem_at::attack_rate]), cc_atk);
	const u8 atk = rom[ATTACK_TAB + u32(attack_idx(arate))];
	const u8 dc1 = rom[DECAY_TAB  + clamp_idx(
	                   rate_scale(eg_dec1_cc(int(elem[elem_at::first_decay_rate]), cc_atk), corr))];
	// **減衰のつまみ（CC75）で目盛りが動く**
	const u8 dc2 = rom[DECAY_TAB  + clamp_idx(
	                   rate_scale2(eg_rate_cc_add(int(elem[elem_at::second_decay_rate]), cc_dec), corr))];
	// はじめの音量。アタックが最速（63）のときだけ 0 で、あとは 0x7e
	r.set(word::attack, u16(atk << 8 | (arate >= ATTACK_FASTEST_RATE ? 0 : ATTACK_START_LEVEL)));
	r.set(word::first_decay, u16(dc1 << 8 | (((0x7f - elem[elem_at::first_decay_level]) * 2) & 0xff)));
	r.set(word::second_decay, u16(dc2 << 8 | (((0x7f - elem[elem_at::second_decay_level]) * 2) & 0xff)));
	r.set(word::release, u16(att & 0xff));

	// --- 音程と波形
	// 要素の 粗調 は**半音単位の粗調**、離調 は**セント単位の離調**（どちらも 64 が中央）。
	// 離調は重ねの音色で 2 つの層をずらすのに使う。入れないと層がぴったり重なって
	// 打ち消し合わず、3dB ほど大きくなる
	// **鍵が 0-127 からはみ出したら、はみ出したぶんを引く**。
	// 実機は粗調を足した鍵を 0-127 に収めてから波形も音程も出す
	// （`wave_note` は丸めているのに、音程だけ丸めていなかった）。
	// Shakuhachi の第 2 要素は粗調 +12 半音・支点 53 なので、鍵 120 で
	// 53+67+12 = 132 ＝ 5 半音はみ出す。実機との差はちょうど 500 セントだった
	r.set(word::pitch, pitch_word(w, note - shift, key_follow(rom, elem),
	                      cents_extra + shift * 100 + elem_tune(elem) - note_overflow(rom, elem, note, shift) * 100,
	                      key_pivot(elem)));
	{
		u32 pre = w.pre_loop, loop = w.loop_len;
		// 逆向きなら 2 つの数を入れ替える。**下駄を引く前**に入れ替えて、
		// 下駄は lead_high/lead_low に行くほうから引く。旋律の音色で逆向きのものは
		// 見つかっていないので、この組み合わせは実機で確かめられていない
		wave_backwards_swap(pre, loop);
		const u32 skip = u32(elem[elem_at::wave_skip_high]) * 128 + elem[elem_at::wave_skip_low];
		if (pre > skip)
			pre -= skip;
		else
			pre = 0;
		r.set(word::lead_high, u16(pre >> 16));
		r.set(word::lead_low, u16(pre));
		r.set(word::loop_high, u16(loop >> 16));
		r.set(word::loop_low, u16(loop));
	}
	r.set(word::wave_high, u16(w.format_addr >> 16));
	r.set(word::wave_low, u16(w.format_addr));

	// --- 声の EQ とミキサ
	for (int i = 0; i < 6; i++)
		r.set(word::eq_low_previous + i, d.iir[i]);
	for (int i = 0; i < 6; i++)
		r.set(word::pan_mix + i, d.mix[i]);
	r.set(word::pan_mix, voice_pan_word(rom, elem, note));
	// 送りはそのパンのぶん目減りする
	{
		const int adj = pan_send_adj(rom, voice_pan_pos(rom, elem, note));
		for (int i = 0; i < 2; i++) {
			// **切ってある送り（0xff）はそのまま**。実機も頭打ちなので、
			// ここでパンのぶん引くと切ったはずの送りが開いてしまう
			if ((d.mix[1 + i] & 0xff) >= 0xff)
				continue;
			int v = int(d.mix[1 + i] & 0xff) + adj;
			v = v < 0 ? 0 : (v > 255 ? 255 : v);
			r.set(word::reverb_send + i, u16((d.mix[1 + i] & 0xff00) | u16(v)));
		}
	}

	return r;
}

} // namespace nv
} // namespace xg

#endif // S_MU2000_XG_NATIVE_VOICE_H
