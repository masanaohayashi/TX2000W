// license:BSD-3-Clause
// S-MU2000: プログラム ROM の中の、音色と表の位置

#ifndef S_MU2000_XG_VOICE_ROM_LAYOUT_H
#define S_MU2000_XG_VOICE_ROM_LAYOUT_H

#include "compat/types.h"

namespace xg::rom_position {

inline constexpr u32 PROGRAM_ROM_SIZE = 0x400000;
inline constexpr u32 SET_TABLE = 0x200AF0;   // 波形の組 → 波形の並びの中の位置（16bit を 503 個）
inline constexpr u32 WAVE_BASE = 0x1F55A0;   // 波形の記録（16 バイトずつ）
inline constexpr u32 ATTACK_TAB = 0x1F4DB8;   // アタックの速さ（128 バイト）
inline constexpr u32 EG_RATE_CC = 0x1E54A4;    // つまみ 64-127 → 目盛り（64 バイト）
inline constexpr u32 DECAY_TAB = 0x1F4E38;   // 減衰の速さ（128 バイト）
inline constexpr u32 REL_TAB = 0x1F4EB8;   // 離しの速さ（128 バイト）
inline constexpr u32 VEL_CURVE = 0x1E5E5E;   // 強さの曲線（128 バイトの行が並ぶ。行 0 はそのまま）
inline constexpr u32 AMP_ASSIGN_TAB = 0x1E6899;
inline constexpr u32 LEVEL_TAB = 0x1E6798;
inline constexpr u32 SLOT_TABLE = 0x1F4F58;   // スロット番号 → 語の先頭（4 バイト × 64）
inline constexpr u32 CUTOFF_TAB = 0x1E5B58;   // フィルタの切る高さ（16bit。索引は記録の byte37）
inline constexpr u32 KEY_FOLLOW_TAB = 0x1E5E58;
inline constexpr u32 PORTA_TAB = 0x1E6698;
inline constexpr u32 PAN_ATT_TAB = 0x1BBAD0;
inline constexpr u32 SEND_TAB = 0x1B99B9;      // 0-127 → 送りの減衰（127 バイト。番号 0 が CC1）
inline constexpr u32 GS_VIB_RATE_SHIFT_TAB = 0x1E6A10;   // プログラム → ずれ + 14
inline constexpr u32 GS_VIB_DELAY_CLASS_TAB = 0x1E6B10;  // プログラム → 種類
inline constexpr u32 PEG_RATE_TAB = 0x1E6C94;   // 速さの目盛り → 語（64 語）
inline constexpr u32 CENT_PITCH_TAB = 0x1E6D14;  // セント → 音程の目盛り（0-4800）
inline constexpr u32 PEG_REL_TAB = 0x1E54E4;
inline constexpr u32 VIB_CAP_TAB = 0x1E6370;   // パートの深さ → 頭打ち
inline constexpr u32 VIB_CNT_TAB = 0x1E63F0;   // カウンタ → 目盛り（＝カウンタ × 2）
inline constexpr u32 VIB_WORD_TAB = 0x1E6596;   // 目盛り → 語
inline constexpr u32 PMOD_CAP_XG = 0x1E62F0;
inline constexpr u32 LFO_STEP_TAB = 0x1E6516;  // 速さ 0-63 → 10ms あたりの歩幅
inline constexpr u32 LFO_FDEP_TAB = 0x1E62E0;  // byte15 → 深さ
inline constexpr u32 EQ_LOW_TAB = 0x1EDD98;
inline constexpr u32 EQ_HIGH_TAB = 0x1F0298;
inline constexpr u32 FENV_INC_TAB = 0x1E5C58;   // 速さ → 増分（16bit 符号つき × 64）
inline constexpr u32 FENV_VEL_TAB0 = 0x1E5D58;
inline constexpr u32 FENV_VEL_TAB1 = 0x1E5DD8;
inline constexpr u32 LEVEL_CURVE = 0x23CED0;   // 鍵による音量の曲線（128 バイトの行が並ぶ）
inline constexpr u32 REL_RATE_CC = 0x1E54E4;   // 離しのつまみ（上げる側）の表
inline constexpr u32 GS_REL_RATE_SHIFT_TAB = 0x1E6A90;   // プログラム → 表の読み始め
inline constexpr u32 FENV_ATK_CC = 0x1E5CD8;   // つまみ → 立ち上がりの目盛り（128 バイト）
inline constexpr u32 PAN_SEL_TAB = 0x1E68DC;   // byte69 → パンの位置（16 個）
inline constexpr u32 PAN_SEL_KEY = 0x1E68EB;   // byte69 が 15 のとき、鍵で引く
inline constexpr u32 PAN_BASE_TAB = 0x1E6B90;   // パートのパン → 下駄（中央で 8 ＝ -3dB）
inline constexpr u32 PAN_CURVE_TAB = 0x1E6C11;   // パンの位置 → 減衰（0-128）
inline constexpr u32 PAN_SEND_TAB = 0x1F2198;
inline constexpr u32 DRUM_PITCH_TAB = 0x1E9298;   // セント → 音程の目盛り（1200 = 1024）
inline constexpr u32 DRUM_KIT_TABLE = 0x292250;   // キット → 鍵ごとのずれの表（4 バイト）
inline constexpr u32 DRUM_RECORDS = 0x283dd0;
inline constexpr u32 SFX_VOICE_TABLE = 0x283B50;
inline constexpr u32 SFX_VOICES = 0x200ee0;   // xg::rom_position::VOICES と同じ
inline constexpr u32 SFX_VOICES_END = 0x23cece;   // xg::rom_position::VOICES_END と同じ
inline constexpr u32 VOICES = 0x200ee0;   // 音色の記録の並び
inline constexpr u32 VOICES_END = 0x23cece;
inline constexpr u32 KIT_MAP = 0x299fcc;   // バンク 127: プログラム → キットの番号
inline constexpr u32 KIT_NAMES = 0x299dc0;   //   名前 8 文字 + 4 バイト
inline constexpr u32 SFX_MAP = 0x29be58;   // バンク 126
inline constexpr u32 SFX_NAMES = 0x29bdec;
inline constexpr u32 ICON_OF_PROGRAM = 0x1cd044;   // プログラム → 絵の番号
inline constexpr u32 ICONS = 0x1bbf70;   // 絵。16 ワードずつ
inline constexpr u32 DRUM_ICON = 0x1cd0e4;
inline constexpr u32 GROUP_GM = 0x283d50;   // GM モード: MSB → 組
inline constexpr u32 GROUP_XG = 0x283950;   // XG モード: MSB → 組（0 / 77 / 0xC9 は LSB で引き直す）
inline constexpr u32 GROUP_LSB0 = 0x2839d0;
inline constexpr u32 GROUP_LSB1 = 0x283a50;
inline constexpr u32 GROUP_LSB77 = 0x283ad0;
inline constexpr u32 GROUP_LSBC9_0 = 0x292640;
inline constexpr u32 GROUP_LSBC9_1 = 0x2926c0;
inline constexpr u32 VOICE_TABLE = 0x267f50;   // 組 × 128 プログラム。値の 2 倍が VOICES からの距離
inline constexpr u32 PERFORMANCE_INITIAL = 0x2a20a4;
inline constexpr u32 PERFORMANCE_INTERNAL = 0x2a2590, PERFORMANCE_PRESET = 0x2a84e0;
inline constexpr u32 ICON_UTIL = 0x1cc538, ICON_EFFECT = 0x1bce30, ICON_PLUGIN_STYLE = 0x1cd2a4, ICON_GM2 = 0x1bbf10, ICON_REVERB = 0x1bce50, ICON_CHORUS = 0x1bce70, ICON_VARIATION = 0x1bce90, ICON_SYSTEM = 0x1d5244, ICON_DUMP = 0x1d1904, ICON_INIT = 0x1bbeb0, ICON_INIT_CONFIRM = 0x1bbc90, ICON_SOUND_MODULE = 0x1bbef0, ICON_SOUND_MODULE_GS = 0x1bbf30, ICON_SOUND_MODULE_PFM = 0x1d3eb8, ICON_PERFORMANCE = 0x1bcf30, ICON_CARD = 0x1d9f02, ICON_CARD_RUNNING = 0x1d9f22;
inline constexpr u32 GS_REVERB_CHARACTERS = 0x1b95cc, GS_REVERB_MACROS = 0x1b964c, GS_REVERB_MACRO_TYPES = 0x1b983c, GS_CHORUS_MACROS = 0x1b96fc;
inline constexpr u32 GS_REVERB_PRE_LPF = 0x1b9734;   // GS の Pre-LPF → XG の LPF
inline constexpr u32 GS_REVERB_TIME_SHORT = 0x1b973c, GS_REVERB_TIME_LONG = 0x1b97bc;   // GS の Time → XG の Time
inline constexpr u32 EQ_FLAT_PRESET = 0x1b98bf;     // EQ の型 0（1 バイトずつ）
inline constexpr u32 EQ_SHAPED_PRESETS = 0x181660;  // EQ の型 1-4（2 バイトずつ 20 個）
inline constexpr u32 VIB_RATE_CURVE = 0x1e647e;     // ビブラートの速さのつまみ → 目盛り
inline constexpr u32 VIB_DELAY_CURVE = 0x1e55d4, VIB_DELAY_CURVE_GS_SLOW = 0x1e5654, VIB_DELAY_CURVE_GS_MIDDLE = 0x1e56d4;
inline constexpr u32 EMPTY_DRUM_RECORD = 0x1f530c;  // 打の記録が無いときの値

// パネルの文字。at から length 文字
struct rom_text { u32 at; u8 length; };
// 同じ間隔で並ぶ文字
struct rom_list
{
	u32 first; u16 stride; u8 length;
	constexpr rom_text operator[](int i) const { return { u32(int(first) + int(stride) * i), length }; }
};

inline constexpr rom_text TEXT_CHECK_SUM_ERROR = { 0x1d0672, 16 };
inline constexpr rom_text TEXT_CHECK_SUM_VALUE = { 0x1d0683, 13 };
inline constexpr rom_text TEXT_SYSEX_DATA_ERROR = { 0x1d062e, 17 };
inline constexpr rom_text TEXT_SYSEX_ADDRESS_ERROR = { 0x1d060c, 17 };
inline constexpr rom_text TEXT_PERFORMANCE_NAME = { 0x1d4229, 17 };
inline constexpr rom_text TEXT_PERFORMANCE_COPY = { 0x1d46b0, 12 };
inline constexpr rom_text TEXT_COPY_FROM = { 0x1d46c1, 5 };
inline constexpr rom_text TEXT_COPY_TO = { 0x1d46ca, 4 };
inline constexpr rom_text TEXT_RECALL_PERFORMANCE = { 0x1d4714, 13 };
inline constexpr rom_text TEXT_ARE_YOU_SURE = { 0x1d0503, 15 };
inline constexpr rom_text TEXT_STORE_PERFORMANCE = { 0x1d46d2, 13 };
inline constexpr rom_text TEXT_NO_RECALL = { 0x1d411e, 17 };
inline constexpr rom_text TEXT_ALL_PARTS = { 0x1d04ee, 3 };
inline constexpr rom_text TEXT_BANK_SFX = { 0x1ddba7, 3 };
inline constexpr rom_text TEXT_RECEIVE_OFF = { 0x1d071c, 15 };
inline constexpr rom_text TEXT_MIDI = { 0x1d05c8, 4 };
inline constexpr rom_text TEXT_PROGRAM = { 0x1d47f0, 4 };
inline constexpr rom_text TEXT_BANK = { 0x1d4746, 4 };
inline constexpr rom_text TEXT_SELECT_BANK = { 0x1d48d4, 15 };
inline constexpr rom_text TEXT_SELECT_PROGRAM = { 0x1d48e5, 11 };
inline constexpr rom_text TEXT_NO_PARAMETER = { 0x1d06fa, 13 };
inline constexpr rom_text TEXT_THIS_PARAMETER = { 0x1d06b6, 15 };
inline constexpr rom_text TEXT_NOT_EXCLUSIVE = { 0x1d06c7, 16 };
inline constexpr rom_text TEXT_SELECT_DRUM = { 0x1d0694, 16 };
inline constexpr rom_text TEXT_WHEN_YOU_EDIT = { 0x1d06a5, 14 };
inline constexpr rom_text TEXT_COMMON_TITLE = { 0x1d4207, 8 };
inline constexpr rom_text TEXT_SYSTEM_TITLE = { 0x1d5232, 8 };
inline constexpr rom_text TEXT_CARD_TITLE = { 0x1d9f42, 6 };
inline constexpr rom_text TEXT_REVERB = { 0x1d35c1, 3 };
inline constexpr rom_text TEXT_CHORUS = { 0x1d35c7, 3 };
inline constexpr rom_text TEXT_VARIATION = { 0x1d35cd, 3 };
inline constexpr rom_text TEXT_VARIATION_SYSTEM = { 0x1d3870, 5 };
inline constexpr rom_text TEXT_VARIATION_INSERTION = { 0x1d3868, 5 };
inline constexpr rom_text TEXT_TYPE_FIELD = { 0x1d3a1e, 5 };
inline constexpr rom_text TEXT_DUMP_ALL = { 0x1d0514, 13 };
inline constexpr rom_text TEXT_DUMP_MULTI = { 0x1d0536, 10 };
inline constexpr rom_text TEXT_DUMP_PERFORMANCE = { 0x1d04f2, 12 };
inline constexpr rom_text TEXT_FACTORY_SET = { 0x1d5741, 11 };
inline constexpr rom_text TEXT_INITIALIZE_PERFORMANCE = { 0x1d57a7, 14 };
inline constexpr rom_text TEXT_RESET_GS = { 0x1d5785, 8 };
inline constexpr rom_text TEXT_INITIALIZE_XG = { 0x1d5763, 13 };
inline constexpr rom_text TEXT_INITIALIZE_DRUM = { 0x1d57c9, 15 };
inline constexpr rom_text TEXT_SOUND_MODULE = { 0x1d3e74, 13 };
inline constexpr rom_text TEXT_SOUND_MODULE_CHOICES = { 0x1d3e85, 16 };
inline constexpr rom_text TEXT_TRANSMITTING = { 0x1d04cc, 15 };
inline constexpr rom_text TEXT_EXECUTING = { 0x1d5730, 14 };
inline constexpr rom_text TEXT_CANCELED = { 0x1d52a8, 9 };
inline constexpr rom_text TEXT_FORMAT_CARD = { 0x1da5eb, 11 };
inline constexpr rom_text TEXT_CARD_NOT_READY = { 0x1d073e, 15 };
inline constexpr rom_text TEXT_CARD_ROOT = { 0x1da5e7, 2 };
inline constexpr rom_text TEXT_DEVICE_ALL = { 0x1d016c, 3 };
inline constexpr rom_text TEXT_CHANNEL_AFTERTOUCH = { 0x1d01d8, 3 };
inline constexpr rom_text TEXT_MAP_NOT_APPLICABLE = { 0x1d51ef, 11 };
inline constexpr rom_text TEXT_PERFORM_NAME_ITEM = { 0x1d4219, 14 };
inline constexpr rom_text TEXT_FX_INSERTION_PART = { 0x1d3ad1, 11 };
inline constexpr rom_text TEXT_FX_HZ = { 0x2b341c, 2 };

// 品書きの 2 行（印は * で入っている）
inline constexpr rom_text MENU_EDIT[2] = { { 0x1d2b64, 17 }, { 0x1d2b75, 17 } };
inline constexpr rom_text MENU_EDIT_DRUM[2] = { { 0x1d2ba8, 17 }, { 0x1d2b75, 17 } };
inline constexpr rom_text MENU_UTIL[2] = { { 0x1d4f88, 17 }, { 0x1d4f99, 17 } };
inline constexpr rom_text MENU_DUMP[2] = { { 0x1d5264, 17 }, { 0x1d5275, 17 } };
inline constexpr rom_text MENU_INIT[2] = { { 0x1d56ca, 17 }, { 0x1d56db, 17 } };
inline constexpr rom_text MENU_EFFECT[2] = { { 0x1d35c0, 17 }, { 0x1d35d1, 17 } };
inline constexpr rom_text MENU_PERFORMANCE[2] = { { 0x1d40fc, 17 }, { 0x1d410d, 17 } };
inline constexpr rom_text MENU_INIT_PERFORMANCE[2] = { { 0x1d570e, 17 }, { 0x000000, 0 } };
inline constexpr rom_text MENU_INIT_GS[2] = { { 0x1d56ec, 17 }, { 0x1d56db, 17 } };
inline constexpr rom_text MENU_INSERTION[2] = { { 0x1d3876, 17 }, { 0x1d38a9, 17 } };

// 一覧。ラベルは「=」まで、ほかは後ろの詰め物を除いて使う
inline constexpr rom_list LABELS_PART_SOUND = { 0x1d2c11, 48, 12 };
inline constexpr rom_list LABELS_PART_OTHERS = { 0x1d3115, 48, 12 };
inline constexpr rom_list LABELS_DRUM = { 0x1d2eb1, 22, 12 };
inline constexpr rom_list LABELS_SYSTEM = { 0x1d5053, 24, 12 };
inline constexpr rom_list LABELS_CARD = { 0x1d9f54, 17, 14 };
inline constexpr rom_list LABELS_PERFORMANCE_COMMON = { 0x1d4171, 15, 12 };
inline constexpr rom_list LABELS_FX_TAIL = { 0x1d3a24, 17, 12 };
inline constexpr rom_list LABELS_FX = { 0x2af90c, 12, 12 };
inline constexpr rom_list LABELS_PLAY = { 0x1d4962, 7, 7 };
inline constexpr rom_list LABELS_PLAY_SEND = { 0x1d4af7, 7, 7 };
inline constexpr rom_list LABELS_PERFORMANCE_PART = { 0x1d47b4, 10, 7 };
inline constexpr rom_list LABELS_PERFORMANCE = { 0x1d475a, 10, 7 };
inline constexpr rom_list LABELS_PERFORMANCE_DETAIL = { 0x1d481a, 14, 7 };
inline constexpr rom_list TEXTS_ON_OFF = { 0x1d0154, 4, 3 };
inline constexpr rom_list TEXTS_MONO_POLY = { 0x1d015c, 8, 4 };
inline constexpr rom_list TEXTS_MIDI_IN = { 0x1d0170, 8, 5 };
inline constexpr rom_list TEXTS_DUMP_INTERVAL = { 0x1d0180, 4, 3 };
inline constexpr rom_list TEXTS_MAP = { 0x1d0198, 12, 11 };
inline constexpr rom_list TEXTS_CONNECTION = { 0x1d01b0, 4, 3 };
inline constexpr rom_list TEXTS_PERFORMANCE_BANK = { 0x1d01d0, 4, 3 };
inline constexpr rom_list TEXTS_OUT = { 0x1d0250, 12, 11 };
inline constexpr rom_list TEXTS_DIGITAL_GAIN = { 0x1d0280, 4, 3 };
inline constexpr rom_list TEXTS_PART_MODE = { 0x1cff29, 6, 6 };
inline constexpr rom_list TEXTS_NOTE = { 0x1cff4d, 4, 4 };
inline constexpr rom_list TEXTS_EQ_TYPE = { 0x1d3b54, 7, 7 };
inline constexpr rom_list TEXTS_DUMP_RANGE = { 0x1d0558, 6, 6 };
inline constexpr rom_list TEXTS_PAN = { 0x1d4977, 3, 3 };
inline constexpr rom_list NAMES_TG300B_KIT = { 0x29ddc8, 12, 8 };
inline constexpr rom_list NAMES_GM_KIT = { 0x29fcd8, 12, 8 };
inline constexpr u32 NAME_TABLES_FIRST = 0x200000;   // キットの記録が指せる先の始まり
inline constexpr u32 NAME_SILENT_KIT = 0x299f28;   // いないキットの名前
inline constexpr u32 NAME_SILENT_VOICE = 0x22d8d4;   // 音の無い音色の名前

// 絵
inline constexpr u32 ICON_MIDI_PLUG = 0x1bbc10;
inline constexpr u32 ICON_DUMP_CONFIRM = 0x1d542a;
inline constexpr u32 ICON_INSERTION = 0x1bceb0;   // INS1-4 が 16 ワードずつ
inline constexpr u32 ICON_AD_BANKS[8] = { 0x1cd504, 0x1cd524, 0x1cd544, 0x1cd564, 0x1cd5c4, 0x1bc110, 0x1cd604, 0x1cd624 };
inline constexpr u32 ICON_MASTER_EQ = 0x1d3b78;   // 帯域ごとの絵が 5 つ

// ドラムキットの記録が置かれる範囲
inline constexpr u32 DRUM_KIT_RECORDS_FIRST = 0x200000, DRUM_KIT_RECORDS_LAST = 0x2ffff0;

// A/D の設定の記録の、名前からの相対位置。0xff の欄は「決めない」
inline constexpr int AD_VOLUME = -12, AD_PAN = -10, AD_REVERB = -9, AD_CHORUS = -8;
inline constexpr int AD_VARIATION_TYPE = -7, AD_VARIATION_VALUES = -5, AD_VARIATION_VALUE_COUNT = 5;
inline constexpr int AD_EFFECTS_USED = 8;
inline constexpr u8 AD_NOT_SET = 0xff;
enum ad_effects : u8 { ad_no_effects, ad_variation, ad_fourth_insertion, ad_third_and_fourth_insertions };
// インサーションの種類（2 バイト）、パラメーター 1-10（2 バイトずつ）。その後ろは 11（使わない）と 12-16
struct ad_insertion_record { int type, values; };
inline constexpr ad_insertion_record AD_THIRD_INSERTION = { 9, 12 }, AD_FOURTH_INSERTION = { 38, 40 };
inline constexpr int AD_INSERTION_VALUE_COUNT = 10, AD_INSERTION_LATER_VALUES = 21, AD_INSERTION_LATER_VALUE_COUNT = 5;

// A/D の設定の名前。番号 first_index から同じ間隔で並ぶ
struct rom_run { short first_index; u32 at; };
inline constexpr rom_run AD_PRESET_NAME_RUNS[] = { { 0, 0x1de266 }, { 44, 0x1df5c6 }, { 59, 0x1e0746 }, { 60, 0x1dfa76 }, { 69, 0x1dfd46 }, { 80, 0x1e06f6 }, { 81, 0x1e00b6 }, { 82, 0x1e0106 } };
inline constexpr u32 AD_PRESET_NAME_STRIDE = 0x50;

// 効果の種類。(MSB, LSB) の並びと、同じ順の 10 文字の名前
inline constexpr u32 FX_REVERB_CODES = 0x2aea33, FX_CHORUS_CODES = 0x2aea59, FX_VARIATION_CODES = 0x2aea83, FX_TYPE_NAMES = 0x2aeb6f;
inline constexpr int FX_REVERB_TYPES = 19, FX_CHORUS_TYPES = 21, FX_VARIATION_TYPES = 118;

inline constexpr u32 ROM_NAMES_HASH = 0x0477b0c6;   // 版の確かめ: 音色とキットの先頭の名前

// 効果のパラメーターの値の文字。先頭が値 0
inline constexpr rom_list FX_LFO_FREQUENCY = { 0x2b1660, 4, 4 };
inline constexpr rom_list FX_MODULATION_DELAY = { 0x2b1860, 4, 4 };
inline constexpr rom_list FX_FREQUENCY = { 0x2b1a60, 4, 4 };
inline constexpr rom_list FX_REVERB_TIME = { 0x2b1b54, 4, 4 };
inline constexpr rom_list FX_INITIAL_DELAY = { 0x2b1c6c, 5, 5 };
inline constexpr rom_list FX_ROOM_SIZE = { 0x2b1eec, 3, 3 };
inline constexpr rom_list FX_LONG_DELAY = { 0x2b1f73, 5, 5 };
inline constexpr rom_list FX_COMPRESSOR_ATTACK = { 0x2b21f3, 2, 2 };
inline constexpr rom_list FX_COMPRESSOR_RELEASE = { 0x2b221b, 3, 3 };
inline constexpr rom_list FX_COMPRESSOR_RATIO = { 0x2b224b, 4, 4 };
inline constexpr rom_list FX_SPACE_SIZE = { 0x2b226b, 4, 4 };
inline constexpr rom_list FX_SAMPLE_FREQUENCY = { 0x2b240f, 5, 5 };
inline constexpr rom_list FX_TEMPO_NOTE = { 0x2b268f, 6, 6 };
inline constexpr rom_list FX_DYNAMICS_ATTACK = { 0x2b2863, 3, 3 };
inline constexpr rom_list FX_DYNAMICS_RELEASE = { 0x2b29e3, 4, 4 };
inline constexpr rom_list FX_COARSE_FREQUENCY = { 0x2b2be3, 5, 5 };
inline constexpr rom_list FX_DELAY_OFFSET = { 0x2b2e63, 5, 5 };
inline constexpr rom_list FX_PAN = { 0x2b311f, 3, 3 };
inline constexpr rom_list FX_EARLY_TYPE = { 0x2b329f, 3, 3 };
inline constexpr rom_list FX_VOWEL = { 0x2b32b1, 2, 2 };
inline constexpr rom_list FX_AMP_TYPE = { 0x2b32bb, 7, 7 };
inline constexpr rom_list FX_INPUT_SELECT = { 0x2b32f3, 3, 3 };
inline constexpr rom_list FX_FILTER_CHARACTER = { 0x2b32fc, 5, 5 };
inline constexpr rom_list FX_LOFI_TYPE = { 0x2b331a, 8, 8 };
inline constexpr rom_list FX_RESOLUTION = { 0x2b3382, 5, 5 };
inline constexpr rom_list FX_GATE_TYPE = { 0x2b33af, 5, 5 };
inline constexpr rom_list FX_FILTER_TYPE = { 0x2b33eb, 7, 7 };
inline constexpr rom_list FX_ON_OFF = { 0x2b3420, 4, 4 };
inline constexpr rom_list FX_INPUT_MODE = { 0x2b3428, 8, 8 };
inline constexpr rom_list FX_DRY_WET = { 0x2b3438, 8, 8 };
inline constexpr rom_list FX_EARLY_REVERB = { 0x2b3450, 8, 8 };
inline constexpr rom_list FX_PAN_DIRECTION = { 0x2b3468, 8, 8 };
inline constexpr rom_list FX_OUTPUT_PHASE = { 0x2b3498, 8, 8 };
inline constexpr rom_list FX_DEVICE = { 0x2b34b8, 8, 8 };
inline constexpr rom_list FX_SPEAKER = { 0x2b34e0, 8, 8 };
inline constexpr rom_list FX_LOW_HIGH = { 0x2b3510, 8, 8 };
inline constexpr rom_list FX_SPEED_CONTROL = { 0x2b3528, 8, 8 };
inline constexpr rom_list FX_LFO_WAVE = { 0x2b3538, 8, 8 };
inline constexpr rom_list FX_RESET = { 0x2b3558, 12, 12 };
inline constexpr rom_list FX_DIRECTION = { 0x2b35b8, 8, 8 };
inline constexpr rom_list FX_PHASE_INVERT = { 0x2b35d0, 8, 8 };
inline constexpr rom_list FX_DIVIDE = { 0x2b35f0, 8, 8 };

} // namespace xg::rom_position

#endif // S_MU2000_XG_VOICE_ROM_LAYOUT_H
