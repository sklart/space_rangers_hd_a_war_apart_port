"""Exception-safe original DAT file/buffer ownership; no format/check changes."""
def apply(text, filename):
    owner={'EC_BlockPar.cpp':'TBlockParEC','EC_Data.cpp':'TDataEC'}.get(filename)
    if owner is None:return text
    marker=f'    void {owner}::LoadFromEncryptedDatFile('
    assert text.count(marker)==1,filename
    start=text.index(marker);end=text.index('\n    }',start)+len('\n    }')
    original=text[start:end];body=original
    allocation='        EC_File::TFileEC* FileObj = pas::construct_call<EC_File::TFileEC>(EC_File::TFileEC_Create);'
    assert body.count(allocation)==1
    scope='''
        // File open/read/decode may throw. Preserve original explicit release
        // points while reclaiming both resources on every exceptional exit.
        struct DatResourceScope {
            EC_Buf::TBufEC*& buffer; EC_File::TFileEC*& file;
            ~DatResourceScope(){if(buffer)pas::free(buffer);if(file)pas::free(file);}
        } resource_scope{Buf,FileObj};'''
    assert 'resource_scope' not in body
    body=body.replace(allocation,allocation+scope,1)
    for name in ('Buf','FileObj'):
        release=f'pas::free({name});'
        # Keep guards' releases intact; only original method statements change.
        original_count=original.count(release);assert original_count>0
        body=body.replace(release,release+f' {name}=nullptr;')
    return text[:start]+body+text[end:]
