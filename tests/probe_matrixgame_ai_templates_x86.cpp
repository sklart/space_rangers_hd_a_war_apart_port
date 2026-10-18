// Native engine integration probe: initializes the real D3D map/configuration.
// No L3GRun, input injection, victory or campaign result is simulated.
#include "MatrixGame.h"
#include "MatrixConfig.hpp"
#include "MatrixLogic.hpp"
#include "Interface/CConstructor.h"
#include "Cache.hpp"
#include "3g.hpp"
#include <cstdio>
#include <exception>

int main() {
    try {
        CGame::Init(GetModuleHandleW(nullptr), nullptr, nullptr, 12345u);
        const int count = SSpecialBot::m_AIRobotTypeCnt;
        int heads = 0, invalid_types = 0, invalid_prices = 0, invalid_totals = 0;
        for (int i = 0; i < count; ++i) {
            const auto& bot = SSpecialBot::m_AIRobotTypeList[i];
            if (bot.m_Head.m_nType != MRT_HEAD) ++invalid_types;
            if (bot.m_Head.m_nKind != RUK_UNKNOWN) ++heads;
            for (int r = 0; r < MAX_RESOURCES; ++r) {
                const int price = bot.m_Head.m_nKind == RUK_UNKNOWN ? 0 :
                    g_Config.m_Price[HEAD1_TITAN + (int(bot.m_Head.m_nKind)-1)*4 + r];
                if (bot.m_Head.m_Price.m_Resources[r] != price) ++invalid_prices;
                int total = bot.m_Chassis.m_Price.m_Resources[r] +
                    bot.m_Armor.m_Unit.m_Price.m_Resources[r] + price;
                for (const auto& weapon : bot.m_Weapon)
                    total += weapon.m_Unit.m_Price.m_Resources[r];
                if (bot.m_Resources[r] != total) ++invalid_totals;
            }
        }
        std::printf("{\"scope\":\"REAL X86 D3D MAP AI TEMPLATES\",\"seed\":12345,\"templates\":%d,\"with_head\":%d,\"invalid_types\":%d,\"invalid_prices\":%d,\"invalid_totals\":%d,\"battle_loop_executed\":false}\n",
            count, heads, invalid_types, invalid_prices, invalid_totals);
        std::fflush(stdout);
        CGame::Deinit();
        g_Cache->Clear();
        L3GDeinit();
        CacheDeinit();
        CMain::BaseDeInit();
        return count > 0 && heads > 0 && !invalid_types && !invalid_prices && !invalid_totals ? 0 : 3;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "native probe exception: %s\n", e.what()); return 10;
    } catch (const Base::CException& e) {
        std::fwprintf(stderr, L"native probe engine exception: %ls\n", e.Info().c_str()); return 11;
    } catch (...) { std::fprintf(stderr, "native probe unknown exception\n"); return 12; }
}
