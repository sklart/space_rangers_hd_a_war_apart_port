  // The real GI was loaded/decoded/drawn above. Seed only the manager's LRU;
  // all control bindings and retains below go through the actual acquire API.
  const char* mode=std::getenv("E2E555_CASE");assert(mode);
  const bool wrong=std::strcmp(mode,"wrong-class")==0;
  const bool bound_wrong=std::strcmp(mode,"bound-wrong-class")==0;
  const bool own=std::strcmp(mode,"caller-retained")==0;
  const bool trim=std::strcmp(mode,"trim")==0;
  const bool uaf=std::strcmp(mode,"uaf")==0;
  auto* manager=pas::construct_call<EC_Cache::TCacheEC>(EC_Cache::TCacheEC_Create);
  GR_Main::GlobalCache=manager;manager->ResidentByteLimit=INT32_MAX;
  manager->AddDataToLruHead(cache);manager->ResidentBytes=cache->ResidentBytes;
  auto* held=pas::construct_call<EC_CacheGI::TCGiControlEC>(EC_Cache::TCacheControlEC_Create);
  auto* temporary=pas::construct_call<EC_CacheGI::TCGiControlEC>(EC_Cache::TCacheControlEC_Create);
  held->SetCacheKey(cache->CacheKey);temporary->SetCacheKey(cache->CacheKey);
  auto* acquired=EC_CacheGI::AcquireCachedGi(held);
  assert(acquired==cache && EC_CacheGI::AcquireCachedGi(held)==cache);
  assert(held->RetainCount==2 && held->BoundData==cache && cache->FirstBoundControl==held && cache->LastBoundControl==held);
  if(own || bound_wrong){assert(EC_CacheGI::AcquireCachedGi(temporary)==cache);if(bound_wrong)temporary->Release();}
  if(trim)manager->TrimToBudget(0);
  else if(wrong || bound_wrong)temporary->EvictData(pas::class_ref<EC_Cache::TCacheDataEC>());
  else temporary->EvictData(pas::class_ref<EC_CacheGI::TCGiEC>());
  const bool kept=held->BoundData==acquired && manager->MostRecentData==acquired && manager->ResidentBytes>0;
  assert(kept==(wrong || own || trim));assert(held->RetainCount==2);
  std::printf("CACHE_CONTRACT case=%s acquired_through_original_API=1 nested_retains=2 kept=%d other_retain_invalidated=%d\n",mode,kept,!kept);std::fflush(stdout);
  if(uaf){
      // This is the raw pointer returned by AcquireCachedGi, still retained.
      // ASan must diagnose this member read after the explicit forced eviction.
      const auto size=acquired->Image->GetContentSize();
      std::printf("unexpected stale retained GI size=%d/%d\n",size.X,size.Y);return 49;
  }
  if(kept){assert(acquired->Image && acquired->Image->Data && acquired->Image->GetContentSize().X==1280);}
  held->Release();held->Release();assert(held->RetainCount==0);
  if(own)temporary->Release();
  if(trim){manager->TrimToBudget(0);assert(!held->BoundData && !manager->MostRecentData && manager->ResidentBytes==0);}
  cache=nullptr;pas::free(temporary);pas::free(held);pas::free(manager);GR_Main::GlobalCache=nullptr;
