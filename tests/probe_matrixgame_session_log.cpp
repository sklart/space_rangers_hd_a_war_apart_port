// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_session_log.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

int main(int argc,char** argv){
 assert(argc==2);
 {std::ofstream file(argv[1],std::ios::binary);file<<"earlier evidence\n";assert(file.good());}
 srhd_awa::matrixgame::InitializeRangersLog(argv[1]);
 GR_Main::AppendLogLineThreadSafe("original game marker"_a);
 srhd_awa::matrixgame::InitializeRangersLog(argv[1]);
 std::ifstream file(argv[1],std::ios::binary);std::string text((std::istreambuf_iterator<char>(file)),{});
 assert(text.find("earlier evidence")!=text.npos && text.find("original game marker")!=text.npos);
 const auto first=text.find("integrated application: startup");assert(first!=text.npos && text.find("integrated application: startup",first+1)!=text.npos);
 bool rejected=false;try{srhd_awa::matrixgame::InitializeRangersLog("");}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
 std::puts("RANGERS_SESSION_LOG original_writer_append_existing=PASS repeated_start=PASS empty_path_rejected=PASS");
}
