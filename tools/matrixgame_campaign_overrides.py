"""Native-width allocation for original new-campaign storage records."""
def apply(text: str) -> str:
 old='                pas::get_mem_at(&Entry, 12);'
 if text.count(old)!=1 or 'aPlayer::PStorageEntry Entry{};' not in text:
  raise RuntimeError('fGameSettings storage-entry allocation boundary changed')
 return text.replace(old,'                pas::get_mem_at(&Entry, sizeof(aPlayer::TStorageEntry));',1)
