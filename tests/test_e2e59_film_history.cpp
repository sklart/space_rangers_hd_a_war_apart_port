#include "units/aEFilm.hpp"
#include "units/fFilmFile.hpp"
#include "units/EC_Buf.hpp"
#include "units/GlobalsV.hpp"
#include "types/aGalaxy.hpp"
#include <cassert>
#include <atomic>
#include <thread>
#include <cstdio>
#include <array>
#include <chrono>

int main() {
  auto* film = pas::construct_call<aEFilm::TEFilm>(aEFilm::TEFilm_Create);
  auto* history = pas::construct_call<fFilmFile::TFilmFile>(fFilmFile::TFilmFile_Create);
  auto* buffer = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
  GlobalsV::FilmHistoryLimit = 2;
  auto* object = film->AllocateObject();
  const auto address = reinterpret_cast<std::uintptr_t>(object);
  assert(address > UINT32_MAX); // The old narrowing must actually lose address bits.
  aGalaxy::TJumpGateEntry gate{};
  aGalaxy::THole hole{};
  gate.GateFilmId = gate.EffectFilmId = hole.FilmObjectId = address;
  assert(gate.GateFilmId == address && gate.EffectFilmId == address && hole.FilmObjectId == address);
  film->ReleaseObject(0, reinterpret_cast<aEFilm::TEFilmObj*>(gate.GateFilmId));
  film->SetHoleState(1, reinterpret_cast<aEFilm::TEFilmObj*>(hole.FilmObjectId), 2);
  film->Turn = 10;
  history->AddFilm(film);
  assert(history->GetCount() == 1);
  // Reproduce the old corrupted command without dereferencing its truncated pointer.
  film->ReleaseObject(2, reinterpret_cast<aEFilm::TEFilmObj*>(std::uint32_t(address)));
  std::atomic<bool> failed{};
  std::thread worker([&] { try { history->AddFilm(film); } catch (...) { failed = true; } });
  worker.join();
  assert(failed);
  // The UI thread must be able to acquire the lock after a different worker failed.
  assert(history->GetCount() == 1 && history->GetEntry(0)->Turn == 10);
  bool invalid = false;
  try { history->GetEntry(9); } catch (...) { invalid = true; }
  assert(invalid && history->GetCount() == 1);
  // A truncated imported entry must not publish a partial record.
  buffer->AddIntegerValue(99);
  buffer->AddIntegerValue(4096);
  buffer->SetPosition(0);
  invalid = false;
  try { history->LoadEntryFromBuffer(buffer); } catch (...) { invalid = true; }
  assert(invalid && history->GetCount() == 1);
  film->Clear();
  for (int turn : {11, 12, 13}) { film->Turn = turn; history->AddFilm(film); }
  assert(history->GetCount() == 2);
  assert(history->GetEntry(0)->Turn == 12 && history->GetEntry(1)->Turn == 13);
  std::array<aEFilm::TEFilmObj*, 96> objects{};
  for (auto& obj : objects) obj = film->AllocateObject();
  for (int i = 0; i < 30000; ++i) {
    film->SetObjectPosition(i / 96, objects[i % 96], {float(i % 1000), float(i % 17)});
    film->SetObjectAlpha(i / 96, objects[i % 96], i % 256);
  }
  const auto began = std::chrono::steady_clock::now();
  for (int repeat = 0; repeat < 20; ++repeat) film->SaveToBuffer(buffer);
  const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - began).count();
  std::uint64_t hash = UINT64_C(14695981039346656037);
  for (int i = 0; i < buffer->DataSize; ++i) { hash ^= static_cast<const unsigned char*>(buffer->Data)[i]; hash *= UINT64_C(1099511628211); }
  std::printf("serialization bytes=%d checksum=%016llx repeat=20 us=%lld\n", buffer->DataSize,
      static_cast<unsigned long long>(hash), static_cast<long long>(us));
  pas::free(buffer); pas::free(film); pas::free(history);
  std::puts("64-bit film references, original serializer failure, cross-thread lock recovery, history transaction PASS");
}
