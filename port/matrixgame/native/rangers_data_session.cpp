// SPDX-License-Identifier: GPL-2.0-or-later
// Configuration/package order follows original GR_Main and aPacket. Original
// loaders and MergeFrom perform the required work; temporary trees use RAII.
#include "rangers_data_session.hpp"
#include "runtime_support.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Data.hpp"
#include "units/EC_HsFile.hpp"
#include "units/EC_Str.hpp"
#include "units/GR_Main.hpp"
#include "units/SysUtilsImports.hpp"
#include "units/aPacket.hpp"
#include <stdexcept>
#include <vector>
namespace srhd_awa::matrixgame {
namespace {
template<class T>struct OriginalDelete{void operator()(T* value)const{pas::free(value);}};
template<class T>using Owner=std::unique_ptr<T,OriginalDelete<T>>;
using Block=EC_BlockPar::TBlockParEC;
Owner<Block> NewBlock(){return Owner<Block>(pas::construct_call<Block>(EC_BlockPar::TBlockParEC_Create));}
bool Exists(const pas::WideString& path){return SysUtilsImports::FileExists(static_cast<pas::AnsiString>(path));}
pas::WideString Wide(const std::u16string& value){
    if(value.find(u'\0')!=value.npos)throw std::invalid_argument("embedded NUL in Rangers configuration path");
    return pas::WideString(std::u16string_view(value));
}
Owner<Block> Text(pas::WideString path){auto result=NewBlock();result->LoadFromTextFileWithEncodingProbe(path.pchar(),false);return result;}
pas::WideString LanguageInstall(const pas::WideString& language){return pas::concat_wide({u"install_",language,u".txt"});}
}
struct RangersDataSession::Impl {
    Owner<Block> install,language_install,main,language;
    Owner<EC_Data::TDataEC> cache;
    Owner<pas::List> mod_list,mod_language_list;
    std::vector<Owner<Block>> mod_installs,mod_language_installs;
    EC_HsFile::TPackCollectionEC* packages{};
    pas::WideString old_requested=GR_Main::RequestedLanguage,old_selected=GR_Main::SelectedLanguage;
    pas::WideString old_mods=GR_Main::SelectedMods,old_suffix=GR_Main::SelectedModsDisplaySuffix;
    std::uint8_t old_skip=GR_Main::SkipModsOnReload;
    EC_Str::TWideCasePair* case_data{};int case_count{};
    explicit Impl(const RangersDataOptions& options){
        if(EC_HsFile::PackageCollection || EC_HsFile::PackageFileLock || GR_Main::InstallConfig || GR_Main::LanguageInstallConfig ||
           GR_Main::ModInstallConfigs || GR_Main::ModLanguageInstallConfigs || GR_Main::MainDataConfig || GR_Main::LanguageDataConfig ||
           GR_Main::CacheDataRoot || GR_Main::GameDataConfig || GR_Main::WideCaseTable.length() || GR_Main::GlobalCache || GR_Main::SoundManager || GR_Main::MusicManager)
            throw std::logic_error("Rangers data/package globals already owned");
        const auto requested=Wide(options.requested_language),selection_path=Wide(options.mod_selection_path);
        try{
            if(!aPacket::InitializePackageCollection())throw std::runtime_error("Rangers package initialization failed");
            packages=EC_HsFile::PackageCollection;
            install=Text(u"install.txt"_w);GR_Main::InstallConfig=install.get();
            GR_Main::RequestedLanguage=requested;
            GR_Main::SelectedLanguage=requested;
            if(GR_Main::SelectedLanguage==u"" || !Exists(LanguageInstall(GR_Main::SelectedLanguage))){
                GR_Main::RequestedLanguage=pas::WideString();GR_Main::SelectedLanguage=u"russian"_w;
            }
            const auto language_path=LanguageInstall(GR_Main::SelectedLanguage);
            if(!Exists(language_path))throw std::runtime_error("Rangers selected/fallback language not installed");
            language_install=Text(language_path);GR_Main::LanguageInstallConfig=language_install.get();
            mod_list.reset(pas::make_object<pas::List>());mod_language_list.reset(pas::make_object<pas::List>());
            GR_Main::ModInstallConfigs=mod_list.get();GR_Main::ModLanguageInstallConfigs=mod_language_list.get();
            GR_Main::SkipModsOnReload=options.skip_mods;
            pas::WideString mod_names{};
            if(Exists(selection_path)){
                auto selection=Text(selection_path);
                if(selection->CountParams(u"CurrentMod"_wref.get())>0)mod_names=EC_Str::TrimWideString(selection->GetParam(u"CurrentMod"sv));
                GR_Main::SelectedMods=mod_names;GR_Main::SelectedModsDisplaySuffix=pas::concat_wide({u", ",mod_names,u","});
            }else{
                GR_Main::SelectedMods=pas::WideString();GR_Main::SelectedModsDisplaySuffix=pas::WideString();
            }
            std::vector<pas::WideString> mod_paths;
            if(!options.skip_mods){
                const int count=EC_Str::CountDelimitedPartsW(pas::view(mod_names),u","sv);
                // The original do/while visits the empty mod path once as well.
                for(int i=0;i<(count>0?count:1);++i){
                    auto path=EC_Str::TrimWideString(EC_Str::ExtractDelimitedPartW(pas::view(mod_names),i,u","sv));
                    if(path!=u"")path=pas::concat_wide({path,u"\\"});
                    path=pas::concat_wide({u"Mods\\",path});mod_paths.push_back(path);
                    const auto install_path=pas::concat_wide({path,u"Install.txt"});
                    if(Exists(install_path)){
                        auto block=Text(install_path);mod_installs.push_back(std::move(block));
                        pas::list_add(mod_list.get(),mod_installs.back().get());
                    }
                    const auto lang_path=pas::concat_wide({path,u"Install_",GR_Main::SelectedLanguage,u".txt"});
                    if(Exists(lang_path)){
                        auto block=Text(lang_path);mod_language_installs.push_back(std::move(block));
                        pas::list_add(mod_language_list.get(),mod_language_installs.back().get());
                    }
                }
            }
            if(!aPacket::LoadConfiguredPackages())throw std::runtime_error("Rangers configured package open failed");
            main=NewBlock();GR_Main::MainDataConfig=main.get();main->LoadFromEncryptedDatFile(u"CFG\\Main.dat"_w);
            bool overrides=false;
            for(const auto& path:mod_paths){
                const auto main_path=pas::concat_wide({path,u"CFG\\Main.dat"});
                if(Exists(main_path)){auto block=NewBlock();block->LoadFromEncryptedDatFile(main_path);main->MergeFrom(block.get());overrides=true;}
            }
            language=NewBlock();GR_Main::LanguageDataConfig=language.get();
            const auto lang_folder=language_install->GetParam(u"Lang"sv);
            language->LoadFromEncryptedDatFile(pas::concat_wide({u"CFG\\",lang_folder,u"\\Lang.dat"}));
            for(const auto& path:mod_paths){
                const auto lang_path=pas::concat_wide({path,u"CFG\\",lang_folder,u"\\Lang.dat"});
                if(Exists(lang_path)){auto block=NewBlock();block->LoadFromEncryptedDatFile(lang_path);language->MergeFrom(block.get());overrides=true;}
            }
            cache.reset(pas::construct_call<EC_Data::TDataEC>(EC_Data::TDataEC_Create));GR_Main::CacheDataRoot=cache.get();
            cache->LoadFromEncryptedDatFile(u"CFG\\CacheData.dat"_w);
            for(const auto& path:mod_paths){
                const auto cache_path=pas::concat_wide({path,u"CFG\\CacheData.dat"});
                if(Exists(cache_path)){
                    Owner<EC_Data::TDataEC> data(pas::construct_call<EC_Data::TDataEC>(EC_Data::TDataEC_Create));
                    data->LoadFromEncryptedDatFile(cache_path);cache->MergeFrom(data.get());overrides=true;
                }
            }
            // Original GR_Main::InitializeRuntimeAndSettings supplies localized
            // quest files after cache/mod loading and before publishing a cache.
            cache->AddMissingFromBlock(language->GetBlockByPath(u"PlanetQuest"_wref.get()));
            if(overrides && mod_names==u""){GR_Main::SelectedMods=u"Custom mod"_w;GR_Main::SelectedModsDisplaySuffix=pas::WideString();}
            auto* cases=language->GetBlock(u"CaseConv"sv);case_count=cases->GetParamCount();
            if(case_count<=0)throw std::invalid_argument("Rangers language case table empty");
            GR_Main::WideCaseTable.set_length(case_count);case_data=&GR_Main::WideCaseTable[0];
            for(int i=0;i<case_count;++i){
                if(cases->GetParamName(i).length()!=1 || cases->GetParamValue(i).length()!=1)throw std::invalid_argument("Rangers language case pair invalid");
                GR_Main::WideCaseTable[i].LowerChar=cases->GetParamName(i).read(1);GR_Main::WideCaseTable[i].UpperChar=cases->GetParamValue(i).read(1);
            }
        }catch(...){Clear();throw;}
    }
    void Clear()noexcept{
        GR_Main::WideCaseTable=nullptr;case_data=nullptr;case_count=0;
        GR_Main::CacheDataRoot=nullptr;cache.reset();GR_Main::LanguageDataConfig=nullptr;language.reset();GR_Main::MainDataConfig=nullptr;main.reset();
        if(EC_HsFile::PackageCollection)aPacket::FinalizePackageCollection();
        else if(EC_HsFile::PackageFileLock){pas::free(EC_HsFile::PackageFileLock);EC_HsFile::PackageFileLock=nullptr;}
        packages=nullptr;
        GR_Main::ModInstallConfigs=nullptr;GR_Main::ModLanguageInstallConfigs=nullptr;
        mod_list.reset();mod_language_list.reset();mod_installs.clear();mod_language_installs.clear();
        GR_Main::LanguageInstallConfig=nullptr;language_install.reset();GR_Main::InstallConfig=nullptr;install.reset();
        GR_Main::RequestedLanguage=old_requested;GR_Main::SelectedLanguage=old_selected;
        GR_Main::SelectedMods=old_mods;GR_Main::SelectedModsDisplaySuffix=old_suffix;GR_Main::SkipModsOnReload=old_skip;
    }
    ~Impl(){
        if(EC_HsFile::PackageCollection!=packages || GR_Main::InstallConfig!=install.get() || GR_Main::LanguageInstallConfig!=language_install.get() ||
           GR_Main::MainDataConfig!=main.get() || GR_Main::LanguageDataConfig!=language.get() || GR_Main::CacheDataRoot!=cache.get() ||
           GR_Main::ModInstallConfigs!=mod_list.get() || GR_Main::ModLanguageInstallConfigs!=mod_language_list.get() ||
           GR_Main::WideCaseTable.length()!=case_count || &GR_Main::WideCaseTable[0]!=case_data ||
           GR_Main::GlobalCache || GR_Main::SoundManager || GR_Main::MusicManager)std::terminate();
        Clear();
    }
};
RangersDataSession::RangersDataSession(const RangersDataOptions& options):impl_(std::make_unique<Impl>(options)){}
RangersDataSession::~RangersDataSession()=default;
EC_BlockPar::TBlockParEC& RangersDataSession::MainConfig()const noexcept{return *impl_->main;}
EC_BlockPar::TBlockParEC& RangersDataSession::LanguageConfig()const noexcept{return *impl_->language;}
EC_Data::TDataEC& RangersDataSession::CacheData()const noexcept{return *impl_->cache;}
}
