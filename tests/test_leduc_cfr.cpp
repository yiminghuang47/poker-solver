// Verifies CFR on Leduc Hold'em. Leduc has no tidy closed-form solution, so the
// checks are: the canonical information-set count, a game value matching the
// known benchmark (~-0.086 to player 0), and exploitability that is nonnegative,
// decreasing, and small.

#include "poker_solver/leduc_cfr.hpp"
#include "poker_solver/leduc_exploit.hpp"

#include <iostream>

namespace cfr = poker_solver::leduc_cfr;
namespace exploit = poker_solver::leduc_exploit;

namespace {
int g_failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::cerr << "FAIL: " << #cond << "  (" << __FILE__ << ':'    \
                      << __LINE__ << ")\n";                               \
            ++g_failures;                                                 \
        }                                                                 \
    } while (0)
}  // namespace

int main() {
    cfr::Trainer low;
    low.train(1000);
    cfr::Trainer high;
    const double value = high.train(10000);

    // Leduc has exactly 288 information sets.
    CHECK(high.nodes().size() == 288);

    // Known Leduc game value to player 0 is about -0.086.
    CHECK(value > -0.11 && value < -0.06);

    const double eps_low = exploit::exploitability(low);
    const double eps_high = exploit::exploitability(high);

    CHECK(eps_high >= 0.0);
    CHECK(eps_high < eps_low);
    CHECK(eps_high < 0.02);

    if (g_failures == 0) {
        std::cout << "Leduc CFR checks passed (info sets = " << high.nodes().size()
                  << ", value = " << value << ", eps at 1e4 = " << eps_high << ")\n";
        return 0;
    }
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
}
