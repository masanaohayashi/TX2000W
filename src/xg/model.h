// license:BSD-3-Clause

#ifndef S_MU2000_XG_MODEL_H
#define S_MU2000_XG_MODEL_H

#pragma once

#include "compat/types.h"

#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace xg {

// ---- 定義表

enum class area : u8 {
	system,     // 00 00 xx
	effect,     // 02 01 xx（リバーブ・コーラス・バリエーション）/ 03 nn xx（インサーション）
	part,       // 08 pp xx。pp がパート番号（0 から）
};

enum class coding : u8 {
	byte7,      // 1 バイト 7bit。2 バイトなら MSB, LSB（値 = MSB × 128 + LSB）
	nibble,     // 1 バイトに 4bit ずつ（マスタチューン、デチューン）
};

enum class view : u8 {
	raw,        // そのまま
	plus1,      // 1 から数える（プログラム番号）
	center,     // 真ん中を 0 として ±（ノートシフトなど）
	pan,        // 0 がランダム、1-127 で L63 ... C ... R63
	choice,     // 名前の表
	part_off,   // 0-63 がパート 1-64、64-65 が AD1・AD2、127 が OFF
};

struct param {
	const char *key;      // "part.volume"
	const char *label;    // "Volume"
	area  where;
	u8    hi, mid, lo;    // part のときの mid は使わない（パート番号が入る）
	u8    size;           // SysEx のデータのバイト数
	coding enc;
	int   min, max;       // 値の範囲
	int   special;        // 範囲の外で許される値（OFF の 127 など）。無ければ -1
	int   def;            // XG の初期値（資料の値。実測ではない）
	view  how;
	int   center;         // view::center のとき
	const char *const *choices;   // view::choice のとき。min から順に
};

const std::vector<param> &params();
const param *find(const std::string &key);

constexpr u32 pack(u8 hi, u8 mid, u8 lo) { return u32(hi) << 14 | u32(mid) << 7 | lo; }
u32 address(const param &p, int part = 0);

bool valid(const param &p, int value);
// バンクのように、プログラムを書いたときに初めて効くもの
bool applies_on_program(const param &p);
std::string format(const param &p, int value);


std::vector<u8> param_change(const param &p, int part, int value);
std::vector<u8> param_request(const param &p, int part = 0);
std::vector<u8> dump_request(u32 addr);
inline std::vector<u8> part_dump_request(int part) { return dump_request(pack(0x08, u8(part), 0)); }


class model
{
public:
	void feed(u8 b);

	bool get(const param &p, int part, int &value) const;

	bool get_raw(u32 addr, int size, int &value) const;
	std::vector<u8> set_raw(u32 addr, int size, int value);

	std::vector<u8> set(const param &p, int part, int value);

	using edit_listener = std::function<void(const param &p, int part, int value)>;
	void set_edit_listener(edit_listener f) { m_edit = std::move(f); }
	using raw_listener = std::function<void(u32 addr, int size, int value)>;
	void set_raw_listener(raw_listener f) { m_edit_raw = std::move(f); }

	void load(u32 addr, const u8 *data, size_t n, u64 now_ms)
	{
		m_now = now_ms;
		store(addr, data, n);
		m_accepted++;
	}

	// 塊の読み返しを頼む。同じものが並んでいれば足さない
	void want_dump(u32 addr);
	void want_part(int part) { want_dump(pack(0x08, u8(part), 0)); }

	// ときどき呼ぶ。送る問い合わせがあればそのバイトを返す。
	// **返事が戻るまで次は頼まない**。戻らなければ now_ms で時間を見て 1 回だけ頼み直す。
	// now_ms は音源の時刻で数えること（音が止まっている間は返事も来ないので）
	std::vector<u8> poll(u64 now_ms);
	bool busy() const { return m_waiting || !m_queue.empty(); }

	// 読んだメッセージの数（試験用）
	u64 accepted() const { return m_accepted; }
	u64 rejected() const { return m_rejected; }   // チェックサム違いなど

	void forget();
	void forget(const param &p, int part);

private:
	void on_sysex();
	void store(u32 addr, const u8 *data, size_t n);

	static constexpr u64 PIN_MS = 500;

	std::unordered_map<u32, u8> m_bytes;
	std::unordered_map<u32, u64> m_pinned;
	u64 m_now = 0;
	std::vector<u8> m_msg;
	bool m_in = false;

	std::deque<u32> m_queue;
	bool m_waiting = false;
	u32  m_wait_addr = 0;
	u64  m_sent_ms = 0;
	int  m_tries = 0;

	u64 m_accepted = 0, m_rejected = 0;
	edit_listener m_edit;
	raw_listener m_edit_raw;
};

} // namespace xg

#endif // S_MU2000_XG_MODEL_H
