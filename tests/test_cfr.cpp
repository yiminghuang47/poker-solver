// Verifies that CFR converges to the known Kuhn poker Nash equilibrium.
//
// Kuhn's equilibrium is a one-parameter family in alpha in [0, 1/3]: player 0
// bluffs a Jack with probability alpha and bets a King with probability 3*alpha.
// We test the alpha-independent facts and the two relationships that pin the
// family down, plus the game value (-1/18).

#include "poker_solver/cfr.hpp"
#include "poker_solver/kuhn.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace kuhn = poker_solver::kuhn;
namespace cfr = poker_solver::cfr;

namespace {
int g_failures = 0;

void check_near(const std::string& what, double got, double want, double tol) {
    if (std::abs(got - want) > tol) {
        std::cerr << "FAIL: " << what << " = " << got << ", expected ~" << want
                  << " (tol " << tol << ")\n";
        ++g_failures;
    }
}

// Probability of betting/calling at an info set under the average strategy.
double bet_prob(const cfr::Trainer& t, const std::string& key) {
    return t.nodes().at(key).average_strategy()[kuhn::kBet];
}

}  // namespace

int main() {
    cfr::Trainer trainer;
    const double value = trainer.train(100000);
    constexpr double tol = 0.03;

    // Game value to player 0 converges to -1/18.
    check_near("game value", value, -1.0 / 18.0, 0.01);

    // --- Player 0 (first to act) ---
    const double j_bluff = bet_prob(trainer, "J");   // alpha
    const double k_bet = bet_prob(trainer, "K");     // 3*alpha
    if (j_bluff < -1e-9 || j_bluff > 1.0 / 3.0 + tol)
        { std::cerr << "FAIL: J bluff " << j_bluff << " outside [0,1/3]\n"; ++g_failures; }
    check_near("K bet = 3*alpha", k_bet, 3.0 * j_bluff, tol);
    check_near("Q never bets first", bet_prob(trainer, "Q"), 0.0, tol);

    // Player 0 facing a bet after checking.
    check_near("Qpb call = alpha + 1/3", bet_prob(trainer, "Qpb"),
               j_bluff + 1.0 / 3.0, tol);
    check_near("Kpb always calls", bet_prob(trainer, "Kpb"), 1.0, tol);
    check_near("Jpb always folds", bet_prob(trainer, "Jpb"), 0.0, tol);

    // --- Player 1 (second to act), strategy is alpha-independent ---
    check_near("Jp bets 1/3", bet_prob(trainer, "Jp"), 1.0 / 3.0, tol);
    check_near("Qp checks", bet_prob(trainer, "Qp"), 0.0, tol);
    check_near("Kp always bets", bet_prob(trainer, "Kp"), 1.0, tol);
    check_near("Jb folds to bet", bet_prob(trainer, "Jb"), 0.0, tol);
    check_near("Qb calls 1/3", bet_prob(trainer, "Qb"), 1.0 / 3.0, tol);
    check_near("Kb always calls", bet_prob(trainer, "Kb"), 1.0, tol);

    if (g_failures == 0) {
        std::cout << "CFR converged to the Kuhn equilibrium (alpha = " << j_bluff
                  << ", value = " << value << ")\n";
        return 0;
    }
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
}
