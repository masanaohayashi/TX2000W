// license:BSD-3-Clause

#ifndef S_MU2000_XG_FX_TYPES_H
#define S_MU2000_XG_FX_TYPES_H

#pragma once

#include "compat/types.h"
#include "xg/voice_rom_layout.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace xg {

struct fx_type { const char *name; u8 msb, lsb; };

namespace detail {

struct fx_tables {
	std::vector<fx_type> rev, cho, ins;
	std::deque<std::string> names;          // fx_type::name の持ち主
};

// ROM から読んだ表。1 度だけ作って、プロセスが終わるまで消さない（読んでいる側の参照が切れないように）。
// プラグインを 2 つ同時に起動すると、2 つのスレッドから同時に set_fx_type_rom が来る（#233）
inline std::atomic<const fx_tables *> &rom_tables()
{
	static std::atomic<const fx_tables *> p{ nullptr };
	return p;
}

inline std::mutex &rom_tables_mutex()
{
	static std::mutex m;
	return m;
}

inline const fx_tables &tables()
{
	static const fx_tables no_tables;
	const fx_tables *p = rom_tables().load(std::memory_order_acquire);
	return p ? *p : no_tables;
}

} // namespace detail

inline bool set_fx_type_rom(const std::vector<u8> &rom)
{
	std::lock_guard<std::mutex> lock(detail::rom_tables_mutex());
	if (detail::rom_tables().load(std::memory_order_acquire))
		return true;
	using namespace rom_position;
	const int NREV = FX_REVERB_TYPES, NCHO = FX_CHORUS_TYPES, NVAR = FX_VARIATION_TYPES;
	if (rom.size() < FX_TYPE_NAMES + 10 * (NREV + NCHO + NVAR))
		return false;
	auto name_at = [&](int i) { return std::string(reinterpret_cast<const char *>(rom.data() + FX_TYPE_NAMES + 10 * i), 10); };
	// 形の確かめ: 3 つの表とも効果なし（00 00）の同じ名前で始まり、バリエーションの最後は 40 00
	if (name_at(0) != name_at(NREV) || name_at(0) != name_at(NREV + NCHO) || rom[FX_REVERB_CODES] || rom[FX_REVERB_CODES + 1] ||
	    rom[FX_VARIATION_CODES + 2 * (NVAR - 1)] != 0x40)
		return false;
	auto made = std::make_unique<detail::fx_tables>();
	detail::fx_tables &t = *made;
	int k = 0;
	auto load = [&](std::vector<fx_type> &out, u32 codes, int n) {
		for (int i = 0; i < n; i++, k++) {
			std::string s = name_at(k);
			while (!s.empty() && (s.back() == ' ' || s.back() == 0))
				s.pop_back();
			t.names.push_back(s);
			out.push_back({ t.names.back().c_str(), rom[codes + 2 * i], rom[codes + 2 * i + 1] });
		}
	};
	load(t.rev, FX_REVERB_CODES, NREV);
	load(t.cho, FX_CHORUS_CODES, NCHO);
	load(t.ins, FX_VARIATION_CODES, NVAR);
	detail::rom_tables().store(made.release(), std::memory_order_release);
	return true;
}

// リバーブ・コーラス・バリエーション（インサーションも同じ）に置ける種類
inline const std::vector<fx_type> &rev_types() { return detail::tables().rev; }
inline const std::vector<fx_type> &cho_types() { return detail::tables().cho; }
inline const std::vector<fx_type> &ins_types() { return detail::tables().ins; }

// 種別の値から名前。表に無ければ「TYPE 49-01」のように番号で
inline std::string fx_name(int value)
{
	const int msb = value >> 7, lsb = value & 0x7f;
	for (const auto *list : { &ins_types(), &rev_types(), &cho_types() })
		for (const fx_type &t : *list)
			if (t.msb == msb && t.lsb == lsb)
				return t.name;
	char buf[16];
	std::snprintf(buf, sizeof(buf), "TYPE %02X-%02X", msb, lsb);
	return buf;
}

// 種類を選ぶ品書きの分け方（画面の都合で決めたもの）。MSB の系統ごと
struct fx_category { const char *name; std::vector<u8> msbs; };
inline const std::vector<fx_category> &fx_categories()
{
	static const std::vector<fx_category> c = {
		{ "リバーブ",                 { 0x01, 0x02, 0x03, 0x04, 0x10, 0x11, 0x12, 0x13 } },
		{ "初期反射・ゲート",         { 0x09, 0x0a, 0x0b } },
		{ "ディレイ・エコー",         { 0x05, 0x06, 0x07, 0x08, 0x15, 0x16 } },
		{ "カラオケ",                 { 0x14 } },
		{ "コーラス・セレステ",       { 0x41, 0x42, 0x57 } },
		{ "フランジャー・フェイザー", { 0x43, 0x44, 0x48, 0x68, 0x6b, 0x6c, 0x6e, 0x6f } },
		{ "回転・トレモロ・パン",     { 0x45, 0x46, 0x47, 0x56, 0x63 } },
		{ "歪み・アンプ",             { 0x49, 0x4a, 0x4b, 0x62 } },
		{ "EQ・ワウ・フィルタ",       { 0x4c, 0x4d, 0x4e, 0x52, 0x6d } },
		{ "コンプ・ゲート",           { 0x53, 0x54, 0x69 } },
		{ "組み合わせ",               { 0x5f, 0x60, 0x61 } },
		{ "ローファイ・テクノ",       { 0x5e, 0x74, 0x75, 0x76, 0x73, 0x72 } },
		{ "ピッチ・その他",           { 0x50, 0x51, 0x55, 0x58, 0x5d, 0x70, 0x71 } },
	};
	return c;
}

} // namespace xg

#endif // S_MU2000_XG_FX_TYPES_H
