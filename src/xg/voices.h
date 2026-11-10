// license:BSD-3-Clause

#ifndef S_MU2000_XG_VOICES_H
#define S_MU2000_XG_VOICES_H

#pragma once

#include "compat/types.h"
#include "xg/voice_rom_layout.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace xg {

using namespace rom_position;


class voice_rom
{
public:
	explicit voice_rom(std::shared_ptr<const std::vector<u8>> rom) : m_rom(std::move(rom))
	{
		// 版の確かめ。どれか 1 つでも違えば使わない
		m_ok = m_rom && m_rom->size() == PROGRAM_ROM_SIZE && names_hash() == ROM_NAMES_HASH && word(ICONS) == 0xfffc;
	}

	bool ok() const { return m_ok; }
	// ROM の中身（音色の要素の記録を読むとき。xg/native_voice.h の element）
	const u8 *data() const { return m_ok ? m_rom->data() : nullptr; }
	// パネルの文字。ROM が無ければ空
	std::string text(rom_text t) const
	{
		if (!m_ok)
			return {};
		std::string s(reinterpret_cast<const char *>(at(t.at)), t.length);
		return s.substr(0, s.find('\0'));
	}
	// 一覧の語。後ろの詰め物を除く
	std::string value(rom_text t) const { return trim(text(t)); }
	// ラベル。「=」まで
	std::string label(rom_text t) const
	{
		const std::string s = value(t);
		const size_t equal = s.find('=');
		return equal == std::string::npos ? s : s.substr(0, equal + 1);
	}
	int kit_number(int msb, int prog) const
	{
		if (!m_ok || kit_name(msb, prog).empty())
			return -1;
		struct pk { u8 prog, kit; };
		static constexpr pk XG127[] = {
			{ 0, 36 }, { 1, 1 }, { 2, 9 }, { 3, 14 }, { 4, 18 }, { 5, 19 }, { 6, 23 }, { 7, 24 }, { 8, 2 }, { 9, 15 },
			{ 16, 3 }, { 17, 16 }, { 24, 4 }, { 25, 5 }, { 26, 11 }, { 27, 13 }, { 28, 12 }, { 29, 10 }, { 30, 25 },
			{ 31, 26 }, { 32, 6 }, { 33, 17 }, { 40, 7 }, { 41, 27 }, { 48, 8 }, { 56, 34 }, { 57, 35 }, { 64, 20 },
			{ 65, 21 }, { 66, 22 }, { 126, 36 }, { 127, 0 },
		};
		static constexpr pk XG126[] = {
			{ 0, 47 }, { 1, 48 }, { 16, 28 }, { 17, 29 }, { 18, 30 }, { 32, 31 }, { 33, 32 }, { 34, 33 },
		};
		if (msb == 127) {
			for (const pk &x : XG127)
				if (x.prog == prog)
					return x.kit;
		} else {
			for (const pk &x : XG126)
				if (x.prog == prog)
					return x.kit;
		}
		return -1;
	}
	struct tg_kit { int prog, kit; };
	static constexpr tg_kit TG300B_KITS[] = {
		{ 0, 37 }, { 8, 38 }, { 16, 39 }, { 24, 40 }, { 25, 41 }, { 32, 42 }, { 40, 43 }, { 48, 44 }, { 56, 46 }, { 127, 45 },
	};
	// TG300B モードのキットの名前。表に無いプログラムは Standard Kit
	std::string tg300b_kit_name(int prog) const
	{
		int index = 0;
		for (int i = 0; i < int(std::size(TG300B_KITS)); i++)
			if (TG300B_KITS[size_t(i)].prog == prog)
				index = i;
		return value(NAMES_TG300B_KIT[index]);
	}
	// TG300B モードのプログラム → キットの番号。表に無いプログラムは Standard Kit
	static int tg300b_kit_number(int prog)
	{
		for (const tg_kit &k : TG300B_KITS)
			if (k.prog == prog)
				return k.kit;
		return TG300B_KITS[0].kit;
	}
	// **絵の番号**（プログラムで決まる。効果音のバンク 64 は 57、ドラムは -2）と、その絵
	static constexpr int ICON_DRUM = -2;
	int icon_index(int msb, int prog) const
	{
		if (!m_ok)
			return -1;
		if (msb == 127 || msb == 126)
			return ICON_DRUM;
		return msb == 64 ? 57 : int(byte(ICON_OF_PROGRAM + u32(prog & 0x7f)));
	}
	bool icon_rows(int index, u16 rows[16]) const
	{
		if (!m_ok || (index < 0 && index != ICON_DRUM) || index > 57)
			return false;
		for (int y = 0; y < 16; y++)
			rows[y] = index == ICON_DRUM ? word(DRUM_ICON + u32(y) * 2) : word(ICONS + u32(index) * 32 + u32(y) * 2);
		return true;
	}

	bool lsb_ok(int mode, int set, int msb, int lsb) const
	{
		if (!m_ok || mode != 1)
			return true;
		msb &= 0x7f; lsb &= 0x7f;
		if (lsb == 0)
			return true;
		const u8 kind = byte(GROUP_XG + u32(msb));
		u32 tab;
		if (kind == 0)
			tab = set == 0 ? GROUP_LSB0 : GROUP_LSB1;
		else if (kind == 77)
			tab = GROUP_LSB77;
		else if (kind == 0xc9)
			tab = set == 0 ? GROUP_LSBC9_0 : GROUP_LSBC9_1;
		else
			return true;              // LSB を見ない組
		return byte(tab + u32(lsb)) != byte(tab);
	}

	u32 lookup(int mode, int set, int msb, int lsb, int prog) const
	{
		if (!m_ok)
			return 0;
		msb &= 0x7f; lsb &= 0x7f; prog &= 0x7f;
		u32 group = 76;
		if (mode == 0) {
			const u8 g = byte(GROUP_GM + msb);
			group = g == 0xff ? 76 : g;
		} else if (mode == 1) {
			if (msb == 16 && lsb < 2)
				return 0;
			const u8 kind = byte(GROUP_XG + msb);
			if (kind == 0)
				group = byte((set == 0 ? GROUP_LSB0 : GROUP_LSB1) + lsb);
			else if (kind == 77)
				group = byte(GROUP_LSB77 + lsb);
			else if (kind == 0xc9)
				group = byte((set == 0 ? GROUP_LSBC9_0 : GROUP_LSBC9_1) + lsb);
			else
				group = kind;
		}
		const u32 slot = VOICE_TABLE + group * 512 + u32(prog) * 4;
		if (slot + 4 > m_rom->size())
			return 0;
		const u32 off = u32(byte(slot)) << 24 | u32(byte(slot + 1)) << 16 | u32(byte(slot + 2)) << 8 | byte(slot + 3);
		const u32 rec = VOICES + off * 2;
		return rec >= VOICES && rec + 16 <= VOICES_END ? rec : 0;
	}

	u32 drum_record(int kit, int note) const
	{
		if (!m_ok || (kit & 0x80))
			return 0;
		const u32 base = rd32(DRUM_KIT_TABLE + u32(kit & 0x7f) * 4);
		if (base < NAME_TABLES_FIRST || base + 256 > m_rom->size())
			return 0;
		const u16 off = word(base + u32(note & 0x7f) * 2);
		return off == 0xffff ? 0 : DRUM_RECORDS + off;
	}

	// 内蔵の波形の組（0-502）ごとに、それを要素で使っている音色の名前（重ねない）。
	// 音色の記録は VOICES から「14 + 84 × 要素の数」バイトずつ並ぶ（先頭のバイトが要素のビットマスク）。
	// 要素の byte2・3 が組の番号（7bit が 2 つ。xg/native_voice.h の wave_set）。
	// サンプリングの窓で、サンプル音色に内蔵ウェーブを選ぶときの名前代わり
	static constexpr int WAVE_SETS = 503;
	std::vector<std::vector<std::string>> wave_users() const
	{
		std::vector<std::vector<std::string>> out(WAVE_SETS);
		if (!m_ok)
			return out;
		for (u32 a = VOICES; a + 14 <= VOICES_END;) {
			const int mask = byte(a) & 15;
			int n = 0;
			for (int i = 0; i < 4; i++)
				n += (mask >> i) & 1;
			if (!n)
				break;
			const std::string name = trim(std::string(reinterpret_cast<const char *>(at(a + 2)), 10));
			for (int e = 0; e < n; e++) {
				const u32 el = a + 12 + u32(e) * 84;
				const int set = (byte(el + 2) << 7) | (byte(el + 3) & 0x7f);
				if (set < WAVE_SETS && std::find(out[size_t(set)].begin(), out[size_t(set)].end(), name) == out[size_t(set)].end())
					out[size_t(set)].push_back(name);
			}
			a += 14 + 84 * u32(n);
		}
		return out;
	}

	// 記録の名前（lookup の戻り値から）
	std::string record_name(u32 rec) const
	{
		if (!m_ok || rec < VOICES || rec + 12 > VOICES_END)
			return {};
		return trim(std::string(reinterpret_cast<const char *>(at(rec + 2)), 10));
	}

	// ドラムキットの名前。無いキット（SilenKit を指すもの）は空
	std::string kit_name(int msb, int prog) const
	{
		if (!m_ok || (msb != 127 && msb != 126))
			return {};
		const u32 map = msb == 127 ? KIT_MAP : SFX_MAP;
		const u32 names = msb == 127 ? KIT_NAMES : SFX_NAMES;
		const u8 idx = byte(map + (prog & 0x7f));
		const u32 position = names + idx * 12;
		return std::memcmp(at(position), at(NAME_SILENT_KIT), 8) ? trim(std::string(reinterpret_cast<const char *>(at(position)), 8)) : std::string();
	}

	// **ドラムの鍵ごとの楽器名**（12 文字。LCD のドラムセットアップの画面に出る名前）。
	// キットの記録（名前 8 文字の後ろの 4 バイト）が、鍵 13-91 の名前の表（12 文字 × 79）を指している。
	// バンク 127 の 32 キットとバンク 126（SFX）の 9 キットすべてで引ける。音の無い鍵は
	// 「************」なので空を返す。キットでない（msb が 126・127 でない）ときも空
	std::string drum_key_name(int msb, int prog, int key) const
	{
		if (!m_ok || (msb != 127 && msb != 126) || key < DRUM_KEY0 || key >= DRUM_KEY0 + DRUM_KEYS)
			return {};
		const u32 map = msb == 127 ? KIT_MAP : SFX_MAP;
		const u32 names = msb == 127 ? KIT_NAMES : SFX_NAMES;
		const u32 table = rd32(names + u32(byte(map + u32(prog & 0x7f))) * 12 + 8);
		if (table < NAME_TABLES_FIRST || table + DRUM_KEYS * 12 > m_rom->size())
			return {};
		std::string s(reinterpret_cast<const char *>(at(table + u32(key - DRUM_KEY0) * 12)), 12);
		for (char c : s)
			if (c < 0x20 || c > 0x7e)
				return {};
		if (s.find_first_not_of('*') == std::string::npos)
			return {};
		return trim(s);
	}

	std::string screen_name(u32 rec, int msb, int prog) const
	{
		if (!m_ok)
			return {};
		if (msb == 127 || msb == 126) {
			const u32 map = msb == 127 ? KIT_MAP : SFX_MAP;
			const u32 names = msb == 127 ? KIT_NAMES : SFX_NAMES;
			const u8 idx = (*m_rom)[map + u32(prog & 0x7f)];
			return std::string(
			    reinterpret_cast<const char *>(at(names + u32(idx) * 12)), 8);
		}
		if (rec < VOICES || rec + 16 > VOICES_END)
			return {};
		return std::string(reinterpret_cast<const char *>(at(rec + 2)), 8);
	}

	bool icon_of(u32 rec, int msb, int prog, u16 rows[16]) const
	{
		if (!m_ok)
			return false;
		if (msb == 127 || msb == 126) {
			if (word(DRUM_ICON) != 0x000e)
				return false;
			for (int y = 0; y < 16; y++)
				rows[y] = word(DRUM_ICON + u32(y) * 2);
			return true;
		}
		if (rec < VOICES || rec + 16 > VOICES_END)
			return false;
		int index;
		if (!std::memcmp(at(rec + 2), at(NAME_SILENT_VOICE), 7))
			index = 56;
		else if (msb == 64)
			index = 57;
		else
			index = (*m_rom)[ICON_OF_PROGRAM + u32(prog & 0x7f)];
		const u32 p = ICONS + u32(index) * 32;
		for (int y = 0; y < 16; y++)
			rows[y] = word(p + u32(y) * 2);
		return true;
	}

	bool icon_at(u32 address, u16 rows[16]) const
	{
		if (!m_ok || address + 32 > m_rom->size())
			return false;
		for (int y = 0; y < 16; y++)
			rows[y] = word(address + u32(y) * 2);
		return true;
	}

	// 本体のパートではないもの（プラグインボードのチャンネル）の絵: GM の番号の楽器の絵か、ドラムの絵
	bool icon_gm(int prog, bool drum, u16 rows[16]) const
	{
		if (!m_ok)
			return false;
		if (drum) {
			if (word(DRUM_ICON) != 0x000e)
				return false;
			for (int y = 0; y < 16; y++)
				rows[y] = word(DRUM_ICON + u32(y) * 2);
			return true;
		}
		const u32 p = ICONS + u32((*m_rom)[ICON_OF_PROGRAM + u32(prog & 0x7f)]) * 32;
		for (int y = 0; y < 16; y++)
			rows[y] = word(p + u32(y) * 2);
		return true;
	}

private:
	static constexpr int DRUM_KEY0 = 13, DRUM_KEYS = 79;   // 鍵ごとの名前の表の範囲

	u32 names_hash() const
	{
		u32 hash = 2166136261u;
		for (const u32 first : { VOICES + 2, KIT_NAMES, SFX_NAMES })
			for (u32 i = 0; i < 8; i++)
				hash = (hash ^ byte(first + i)) * 16777619u;
		return hash;
	}

	u8 byte(u32 a) const { return (*m_rom)[a]; }
	const u8 *at(u32 a) const { return m_rom->data() + a; }
	u16 word(u32 a) const { return u16((*m_rom)[a] << 8 | (*m_rom)[a + 1]); }
	u32 rd32(u32 a) const
	{
		return u32((*m_rom)[a]) << 24 | u32((*m_rom)[a + 1]) << 16 |
		       u32((*m_rom)[a + 2]) << 8 | (*m_rom)[a + 3];
	}

	static std::string trim(std::string s)
	{
		while (!s.empty() && (s.back() == ' ' || s.back() == 0))
			s.pop_back();
		return s;
	}

	std::shared_ptr<const std::vector<u8>> m_rom;
	bool m_ok = false;
};

} // namespace xg

#endif // S_MU2000_XG_VOICES_H
