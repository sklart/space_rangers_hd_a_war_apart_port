// Copy the decoded original script definition into the isolated build output.
for (int index = 0; index < pas::list_count(aGalaxy::Galaxy->Scripts); ++index) {
 auto* script = pas::list_at<aScript::TScript>(aGalaxy::Galaxy->Scripts, index);
 if (script->ScriptFileName != u"Script.PC_fem_rangers"_w) continue;
 auto* control = pas::construct_call<EC_CacheBuf::TCBufControlEC>(EC_Cache::TCacheControlEC_Create);
 EC_Cache::TCacheEC::ResetControl(control);
 control->SetCacheKey(script->ScriptFileName);
 auto* cached = EC_CacheBuf::AcquireOrCreateBuffer(control);
 auto* buffer = cached->Buffer;
 const auto size = buffer->DataSize;
 const auto path = std::filesystem::path(output_root) / "pc_fem_rangers.script.bin";
 std::ofstream stream(path, std::ios::binary);
 stream.write(static_cast<const char*>(buffer->Data), size);
 if (!stream) throw std::runtime_error("isolated script copy failed");
 stream.close(); control->Release();
 std::printf("SAVE_PREFLIGHT script_copy_bytes=%d\n", size);
 break;
}
