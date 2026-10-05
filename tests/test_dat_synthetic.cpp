#include "ec_file_adapter.hpp"
#include "runtime_settings_slice.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Data.hpp"
#include "units/GR_Main.hpp"
#include "units/aPacket.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

#include <zlib.h>

namespace {
constexpr std::uint32_t kCrcKey1 = 0x7db6c99du;
constexpr std::uint32_t kCrcKey2 = 0xc83fcbf3u;
constexpr std::uint32_t kBlockSeedKey = 0xb1e8c689u;
constexpr std::uint32_t kDataSeedKey = 0xea8f3f37u;

void AppendU32(std::vector<std::uint8_t>* out, std::uint32_t value) {
  for (int byte = 0; byte != 4; ++byte) out->push_back(static_cast<std::uint8_t>(value >> (byte * 8)));
}

void AppendWide(std::vector<std::uint8_t>* out, const char16_t* text) {
  for (const char16_t* it = text;; ++it) {
    out->push_back(static_cast<std::uint8_t>(*it));
    out->push_back(static_cast<std::uint8_t>(*it >> 8));
    if (*it == 0) return;
  }
}

void AppendString(std::vector<std::uint8_t>* out, const char16_t* name, const char16_t* value) {
  out->push_back(EC_BlockPar::bpkString);
  AppendWide(out, name);
  AppendWide(out, value);
}

void AppendBlock(std::vector<std::uint8_t>* out, const char16_t* name,
                 const std::vector<std::uint8_t>& child) {
  out->push_back(EC_BlockPar::bpkBlock);
  AppendWide(out, name);
  out->insert(out->end(), child.begin(), child.end());
}

std::vector<std::uint8_t> BlockWith(const std::vector<std::uint8_t>& entries, std::uint32_t count) {
  std::vector<std::uint8_t> block{0};  // UseSortedIndex=false.
  AppendU32(&block, count);
  block.insert(block.end(), entries.begin(), entries.end());
  return block;
}

void ApplyDatCipher(std::vector<std::uint8_t>* bytes, std::int32_t seed) {
  constexpr std::int32_t kMultiplier = 16807;
  constexpr std::int32_t kQuotient = 127773;
  constexpr std::int32_t kRemainder = 2836;
  constexpr std::int32_t kModulus = 2147483647;
  std::int32_t state = seed;
  for (auto& value : *bytes) {
    state = kMultiplier * (state % kQuotient) - kRemainder * (state / kQuotient);
    if (state <= 0) state += kModulus;
    value ^= static_cast<std::uint8_t>(state - 1);
  }
}

std::vector<std::uint8_t> Compress(const std::vector<std::uint8_t>& input) {
  uLongf size = compressBound(static_cast<uLong>(input.size()));
  std::vector<std::uint8_t> output(size);
  if (compress2(output.data(), &size, input.data(), static_cast<uLong>(input.size()), Z_BEST_COMPRESSION) != Z_OK)
    return {};
  output.resize(size);
  return output;
}

std::vector<std::uint8_t> BuildDat(const std::vector<std::uint8_t>& decoded, std::uint32_t seed_key) {
  auto encoded = Compress(decoded);
  const std::uint32_t inner_crc = crc32(0, encoded.data(), static_cast<uInt>(encoded.size()));
  constexpr std::int32_t seed = 12345;
  ApplyDatCipher(&encoded, seed);
  std::vector<std::uint8_t> payload;
  AppendU32(&payload, inner_crc);
  AppendU32(&payload, static_cast<std::uint32_t>(seed) ^ seed_key);
  payload.insert(payload.end(), encoded.begin(), encoded.end());
  const std::uint32_t first_crc = crc32(0, payload.data(), static_cast<uInt>(payload.size())) ^ kCrcKey1;
  std::vector<std::uint8_t> outer_crc_input;
  AppendU32(&outer_crc_input, first_crc);
  outer_crc_input.insert(outer_crc_input.end(), payload.begin(), payload.end());
  const std::uint32_t outer_crc = crc32(0, outer_crc_input.data(), static_cast<uInt>(outer_crc_input.size())) ^ kCrcKey2;
  std::vector<std::uint8_t> result;
  AppendU32(&result, static_cast<std::uint32_t>(payload.size()) ^ kCrcKey1 ^ kCrcKey2);
  AppendU32(&result, outer_crc);
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}

std::vector<std::uint8_t> BuildRawDat(std::vector<std::uint8_t> encoded, std::uint32_t seed_key) {
  const std::uint32_t inner_crc = crc32(0, encoded.data(), static_cast<uInt>(encoded.size()));
  constexpr std::int32_t seed = 12345;
  ApplyDatCipher(&encoded, seed);
  std::vector<std::uint8_t> payload;
  AppendU32(&payload, inner_crc);
  AppendU32(&payload, static_cast<std::uint32_t>(seed) ^ seed_key);
  payload.insert(payload.end(), encoded.begin(), encoded.end());
  const std::uint32_t first_crc = crc32(0, payload.data(), static_cast<uInt>(payload.size())) ^ kCrcKey1;
  std::vector<std::uint8_t> outer_crc_input;
  AppendU32(&outer_crc_input, first_crc);
  outer_crc_input.insert(outer_crc_input.end(), payload.begin(), payload.end());
  const std::uint32_t outer_crc = crc32(0, outer_crc_input.data(), static_cast<uInt>(outer_crc_input.size())) ^ kCrcKey2;
  std::vector<std::uint8_t> result;
  AppendU32(&result, static_cast<std::uint32_t>(payload.size()) ^ kCrcKey1 ^ kCrcKey2);
  AppendU32(&result, outer_crc);
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}

bool WriteFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  if (FILE* file = std::fopen(path.string().c_str(), "wb")) {
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    std::fclose(file);
    return ok;
  }
  return false;
}

bool LoadRejectedDatKeepsBlock(const std::filesystem::path& path,
                               const std::vector<std::uint8_t>& bytes,
                               EC_BlockPar::TBlockParEC* block) {
  if (!WriteFile(path, bytes)) return false;
  GR_Main::CCInterface->SetResourceChecksumFailed(false);
  try {
    block->LoadFromEncryptedDatFile(pas::WideString(path.filename().string().c_str()));
  } catch (...) {
    return false;
  }
  return GR_Main::CCInterface->GetResourceChecksumFailed() &&
      block->GetParam(u"Name"sv) == u"Value";
}

bool RejectsMalformedDat(const std::filesystem::path& path,
                         const std::vector<std::uint8_t>& bytes) {
  if (!WriteFile(path, bytes)) return false;
  GR_Main::CCInterface->SetResourceChecksumFailed(false);
  bool raised = false;
  try { block->LoadFromEncryptedDatFile(pas::WideString(path.filename().string().c_str())); }
  catch (...) { raised = true; }
  return raised || GR_Main::CCInterface->GetResourceChecksumFailed();
}

void FreeDatRoots() {
  GR_Main::FreeDatConfigRoots();
  pas::free(GR_Main::LanguageInstallConfig);
  GR_Main::LanguageInstallConfig = nullptr;
}
}  // namespace

int main() {
  const std::filesystem::path root = "build/m9-dat";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root);
  // TBlockPar: UseSortedIndex=false, one bpkString Name=Value.
  std::vector<std::uint8_t> block = {0};
  AppendU32(&block, 1);
  block.push_back(EC_BlockPar::bpkString);
  AppendWide(&block, u"Name");
  AppendWide(&block, u"Value");
  // TData: one dekFile entry with an interned filename.
  std::vector<std::uint8_t> data;
  AppendU32(&data, 1);
  data.push_back(EC_Data::dekFile);
  AppendWide(&data, u"Node");
  AppendWide(&data, u"file.bin");
  const auto good_block = BuildDat(block, kBlockSeedKey);
  if (!WriteFile(root / "block.dat", good_block) ||
      !WriteFile(root / "data.dat", BuildDat(data, kDataSeedKey))) return 1;

  srhd_awa::platform::ec_file::SetGameRoot(root.string());
  if (!aPacket::InitializePackageCollection()) return 1;
  GR_Main::CCInterface = pas::construct_call<GR_Main::TCCInterface>(GR_Main::TCCInterface_Create);
  bool ok = true;
  auto* loaded_block = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  auto* loaded_data = pas::construct_call<EC_Data::TDataEC>(EC_Data::TDataEC_Create);
  loaded_block->LoadFromEncryptedDatFile(u"block.dat");
  loaded_data->LoadFromEncryptedDatFile(u"data.dat");
  ok = loaded_block->GetParam(u"Name"sv) == u"Value" && !loaded_data->IsEmpty();
  auto bad_outer = good_block;
  bad_outer[4] ^= 0x80;
  const bool outer_crc_rejected = LoadRejectedDatKeepsBlock(root / "block.dat", bad_outer, loaded_block);
  if (!outer_crc_rejected) std::fputs("bad outer CRC was not rejected\n", stderr);
  ok = ok && outer_crc_rejected;
  auto bad_inner = good_block;
  bad_inner[16] ^= 0x80;
  const bool inner_crc_rejected = LoadRejectedDatKeepsBlock(root / "block.dat", bad_inner, loaded_block);
  if (!inner_crc_rejected) std::fputs("bad inner CRC was not rejected\n", stderr);
  ok = ok && inner_crc_rejected;
  auto bad_outer_size = good_block;
  bad_outer_size[0] ^= 0x01;
  const bool outer_size_rejected = LoadRejectedDatKeepsBlock(root / "block.dat", bad_outer_size, loaded_block);
  if (!outer_size_rejected) std::fputs("bad outer size was not rejected\n", stderr);
  ok = ok && outer_size_rejected;
  auto truncated = good_block;
  truncated.resize(11);
  const bool truncation_rejected = RejectsMalformedDat(root / "block.dat", truncated);
  if (!truncation_rejected) std::fputs("truncated DAT was not rejected\n", stderr);
  ok = ok && truncation_rejected;
  const bool invalid_stream_rejected = RejectsMalformedDat(root / "block.dat",
      BuildRawDat({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}, kBlockSeedKey));
  if (!invalid_stream_rejected) std::fputs("invalid compressed DAT was not rejected\n", stderr);
  ok = ok && invalid_stream_rejected;
  pas::free(loaded_block);
  pas::free(loaded_data);

  // Exercise the actual GR_Main loader with base assets followed by selected-mod overrides.
  const auto data_base = BlockWith([&] { std::vector<std::uint8_t> e; AppendString(&e, u"Origin", u"base"); return e; }(), 1);
  const auto data_mod = BlockWith([&] { std::vector<std::uint8_t> e; AppendString(&e, u"Origin", u"mod"); return e; }(), 1);
  const auto ml = BlockWith([&] { std::vector<std::uint8_t> e; AppendString(&e, u"Tag", u"base"); return e; }(), 1);
  const auto zpos = BlockWith([&] {
    std::vector<std::uint8_t> e;
    for (const auto* name : {u"Planet", u"UnitPathShip", u"UnitPathEndShip", u"UnitPath", u"UnitPathEnd",
                             u"ButtonAction", u"GalaxyStar", u"GalaxyStarName", u"GalaxyWar",
                             u"ConstellationLine", u"ConstellationColor"}) AppendString(&e, name, u"1");
    return e;
  }(), 11);
  std::vector<std::uint8_t> main_entries;
  AppendBlock(&main_entries, u"Data", data_base); AppendBlock(&main_entries, u"ML", ml); AppendBlock(&main_entries, u"ZPos", zpos);
  std::vector<std::uint8_t> mod_main_entries; AppendBlock(&mod_main_entries, u"Data", data_mod);
  const auto case_conv = BlockWith([&] { std::vector<std::uint8_t> e; AppendString(&e, u"a", u"A"); return e; }(), 1);
  const auto planet_quest = BlockWith({}, 0);
  std::vector<std::uint8_t> lang_entries; AppendBlock(&lang_entries, u"CaseConv", case_conv); AppendBlock(&lang_entries, u"PlanetQuest", planet_quest);
  std::vector<std::uint8_t> mod_lang_entries; AppendString(&mod_lang_entries, u"Override", u"yes");
  std::vector<std::uint8_t> cache_base; AppendU32(&cache_base, 1); cache_base.push_back(EC_Data::dekFile); AppendWide(&cache_base, u"Base"); AppendWide(&cache_base, u"base.bin");
  std::vector<std::uint8_t> cache_mod; AppendU32(&cache_mod, 1); cache_mod.push_back(EC_Data::dekFile); AppendWide(&cache_mod, u"Mod"); AppendWide(&cache_mod, u"mod.bin");
  std::filesystem::create_directories(root / "CFG" / "russian");
  std::filesystem::create_directories(root / "Mods" / "TestMod" / "CFG" / "russian");
  std::ofstream(root / "Mods" / "ModCFG.txt") << "CurrentMod=TestMod\n";
  std::ofstream(root / "base.bin") << "base";
  std::ofstream(root / "mod.bin") << "mod";
  ok = ok && WriteFile(root / "CFG" / "Main.dat", BuildDat(BlockWith(main_entries, 3), kBlockSeedKey)) &&
      WriteFile(root / "Mods" / "TestMod" / "CFG" / "Main.dat", BuildDat(BlockWith(mod_main_entries, 1), kBlockSeedKey)) &&
      WriteFile(root / "CFG" / "russian" / "Lang.dat", BuildDat(BlockWith(lang_entries, 2), kBlockSeedKey)) &&
      WriteFile(root / "Mods" / "TestMod" / "CFG" / "russian" / "Lang.dat", BuildDat(BlockWith(mod_lang_entries, 1), kBlockSeedKey)) &&
      WriteFile(root / "CFG" / "CacheData.dat", BuildDat(cache_base, kDataSeedKey)) &&
      WriteFile(root / "Mods" / "TestMod" / "CFG" / "CacheData.dat", BuildDat(cache_mod, kDataSeedKey));
  GR_Main::LanguageInstallConfig = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  GR_Main::LanguageInstallConfig->AddParam(u"Lang"_wref.get(), u"russian"_wref.get());
  GR_Main::SkipModsOnReload = false;
  try { GR_Main::LoadDatConfigAndModOverrides(); } catch (...) {
    std::fputs("DAT root load raised\n", stderr);
    ok = false;
  }
  const bool roots_present = GR_Main::MainDataConfig && GR_Main::LanguageDataConfig && GR_Main::CacheDataRoot;
  if (!roots_present) std::fputs("DAT root load returned missing root\n", stderr);
  if (roots_present) {
    try {
      const bool merged = GR_Main::MainDataConfig->GetBlockByPath(u"Data"_wref.get())->GetParam(u"Origin"sv) == u"mod" &&
          GR_Main::LanguageDataConfig->GetParam(u"Override"sv) == u"yes" &&
          GR_Main::CacheDataRoot->FileExistsByPath(u"Base"_wref.get()) && GR_Main::CacheDataRoot->FileExistsByPath(u"Mod"_wref.get());
      if (!merged) std::fputs("DAT root merge invariant failed\n", stderr);
      ok = ok && merged;
    } catch (...) {
      std::fputs("DAT root merge assertion raised\n", stderr);
      ok = false;
    }
  } else {
    ok = false;
  }
  FreeDatRoots();
  ok = ok && !GR_Main::MainDataConfig && !GR_Main::LanguageDataConfig && !GR_Main::CacheDataRoot;
  pas::free(GR_Main::CCInterface);
  GR_Main::CCInterface = nullptr;
  srhd_awa::platform::ec_file::SetUserRoot((root / "user").string());
  std::ofstream(root / "cfg.txt") << "VSync=True\nRenderModeScale=True\nDisableFrameLimit=False\n";
  GR_Main::LanguageInstallConfig = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  GR_Main::LanguageInstallConfig->AddParam(u"Lang"_wref.get(), u"russian"_wref.get());
  std::string runtime_error;
  const bool runtime_once = srhd_awa::platform::runtime_settings_slice::Initialize(&runtime_error);
  if (!runtime_once) std::fprintf(stderr, "runtime initialization failed: %s\n", runtime_error.c_str());
  const bool runtime_state = runtime_once && GR_Main::GameDataConfig && GR_Main::UiStyleConfig && GR_Main::UiDepthConfig &&
      GR_Main::WideCaseTable.length() == 1 && GR_Main::VSyncEnabled &&
      std::filesystem::is_regular_file(root / "user" / "config" / "CFG.TXT");
  if (!runtime_state) std::fprintf(stderr, "runtime state failed: game=%d style=%d depth=%d case=%d vsync=%d cfg=%d\n",
      GR_Main::GameDataConfig != nullptr, GR_Main::UiStyleConfig != nullptr, GR_Main::UiDepthConfig != nullptr,
      GR_Main::WideCaseTable.length(), GR_Main::VSyncEnabled,
      std::filesystem::is_regular_file(root / "user" / "config" / "CFG.TXT"));
  ok = ok && runtime_state;
  srhd_awa::platform::runtime_settings_slice::Shutdown();
  const bool runtime_twice = srhd_awa::platform::runtime_settings_slice::Initialize(&runtime_error);
  if (!runtime_twice) std::fprintf(stderr, "runtime reinitialization failed: %s\n", runtime_error.c_str());
  ok = ok && runtime_twice;
  srhd_awa::platform::runtime_settings_slice::Shutdown();
  FreeDatRoots();
  const bool runtime_cleanup = !GR_Main::CCInterface && !GR_Main::UserSettingsConfig && !GR_Main::NewGameSettingsConfig &&
      !GR_Main::MainDataConfig && !GR_Main::LanguageDataConfig && !GR_Main::CacheDataRoot &&
      GR_Main::WideCaseTable.length() == 0;
  if (!runtime_cleanup) std::fprintf(stderr, "runtime cleanup failed: cc=%d user=%d newgame=%d main=%d lang=%d cache=%d case=%d\n",
      GR_Main::CCInterface != nullptr, GR_Main::UserSettingsConfig != nullptr, GR_Main::NewGameSettingsConfig != nullptr,
      GR_Main::MainDataConfig != nullptr, GR_Main::LanguageDataConfig != nullptr, GR_Main::CacheDataRoot != nullptr,
      GR_Main::WideCaseTable.length());
  ok = ok && runtime_cleanup;
  aPacket::FinalizePackageCollection();
  std::filesystem::remove_all(root, error);
  if (ok) std::puts("encrypted DAT synthetic regression passed");
  return ok ? 0 : 1;
}
