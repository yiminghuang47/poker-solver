// Verifies CFR on Leduc Hold'em. Leduc has no tidy closed-form solution, so the
// checks are: the canonical information-set count, a game value matching the
// known benchmark (~-0.086 to player 0), and exploitability that is nonnegative,
// decreasing, and small.

#include "poker_solver/best_response.hpp"
#include "poker_solver/leduc.hpp"
#include "poker_solver/leduc_game.hpp"
#include "poker_solver/solver.hpp"

#include <cmath>
#include <iostream>
#include <string_view>

namespace leduc = poker_solver::leduc;
namespace exploit = poker_solver::best_response;
using Trainer = poker_solver::solver::Trainer<poker_solver::LeducGame>;

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
// Regression: at very low iteration counts a few information sets are first
// reached with reach probability exactly 0 (regrets update between deals, so a
// strategy can go pure before the node is ever visited), leaving strategy_sum
// all zeros. average_strategy() must still return a probability distribution
// over the legal actions — an all-zero vector makes the best-response walk in
// leduc_exploit.hpp discard that subtree's mass and report a wrong number.
void test_average_strategy_is_always_a_distribution() {
    for (int n : {1, 2, 5, 10}) {
        Trainer t;
        t.train(n);
        for (const auto& [key, node] : t.nodes()) {
            const auto avg = node.average_strategy();
            double sum = 0.0;
            for (double p : avg) sum += p;
            CHECK(std::abs(sum - 1.0) < 1e-12);

            // Illegal actions must carry no probability.
            const auto legal =
                leduc::legal_actions(std::string_view(key).substr(1));
            for (int a = 0; a < leduc::kNumActions; ++a) {
                if (!legal[a]) CHECK(avg[a] == 0.0);
                CHECK(avg[a] >= 0.0);
            }
        }
    }
}

}  // namespace

int main() {
    test_average_strategy_is_always_a_distribution();

    Trainer low;
    low.train(1000);
    Trainer high;
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
