// Cross-validates the generic best response against an independent oracle.
//
// This is the test the Game concept exists for. best_response.hpp computes an
// exact best response by carrying a belief over the opponent's hidden card down
// the tree -- the only approach that scales to Leduc, and therefore the code
// that produces the headline Leduc exploitability number. It is also the most
// intricate code in the project: unnormalized Bayesian belief updates, card
// removal that varies per opponent holding, and a maximization that has to be
// taken over a belief-weighted sum.
//
// Nothing validates it on Leduc, because Leduc has no closed-form solution to
// check against. But now that it is generic it can be run on KUHN, where
// exploitability.hpp provides ground truth by brute force over all 2^6 pure
// strategies per player -- an approach that shares no code with the belief walk
// and is almost impossible to get subtly wrong.
//
// If the two agree to machine precision on Kuhn, across trained profiles and
// across the analytical equilibrium family, the belief machinery is sound, and
// the Leduc number inherits that confidence.

#include "poker_solver/best_response.hpp"
#include "poker_solver/exploitability.hpp"
#include "poker_solver/kuhn.hpp"
#include "poker_solver/kuhn_game.hpp"
#include "poker_solver/leduc_game.hpp"
#include "poker_solver/solver.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace exploit = poker_solver::exploit;
namespace br = poker_solver::best_response;
using KuhnGame = poker_solver::KuhnGame;
using LeducGame = poker_solver::LeducGame;
using Trainer = poker_solver::solver::Trainer<KuhnGame>;
using LeducTrainer = poker_solver::solver::Trainer<LeducGame>;

namespace {
int g_failures = 0;

void check_near(const char* what, double got, double want, double tol) {
    if (!(std::abs(got - want) <= tol)) {
        std::cerr << "FAIL: " << what << " = " << got << ", expected " << want
                  << " (tol " << tol << ", diff " << std::abs(got - want) << ")\n";
        ++g_failures;
    }
}

// The known Kuhn equilibrium profile for a given alpha in [0, 1/3].
exploit::Profile analytical_equilibrium(double alpha) {
    auto bet = [](double p) { return exploit::Strategy{1.0 - p, p}; };
    return {
        {"J", bet(alpha)},   {"Q", bet(0.0)},   {"K", bet(3.0 * alpha)},
        {"Jpb", bet(0.0)},   {"Qpb", bet(alpha + 1.0 / 3)}, {"Kpb", bet(1.0)},
        {"Jp", bet(1.0 / 3)},{"Qp", bet(0.0)},  {"Kp", bet(1.0)},
        {"Jb", bet(0.0)},    {"Qb", bet(1.0 / 3)}, {"Kb", bet(1.0)},
    };
}

// The two Profile types are the same map type, since KuhnGame::kNumActions is
// kuhn::kNumActions -- so a profile can be handed to either implementation.
static_assert(std::is_same_v<exploit::Profile, br::Profile<KuhnGame>>);

// Brute force reports (max_x u0 - min_y u0)/2; the belief walk reports
// (BR0 + BR1)/2 where BR1 is already in player 1's units. Those are the same
// quantity, so the two totals are directly comparable.
void check_profile(const char* what, const exploit::Profile& p) {
    check_near(what, br::exploitability<KuhnGame>(p), exploit::exploitability(p),
               1e-12);

    // Compare the two best-response values individually as well, not just their
    // average -- an error that cancelled between the players would otherwise
    // slip through. min_y u0 is player 1's best response expressed in player 0's
    // units, so it is the negation of BR1.
    check_near("  BR0", br::best_response_value<KuhnGame>(p, 0),
               exploit::best_response_value_p0(p, 0, /*maximize=*/true), 1e-12);
    check_near("  BR1", br::best_response_value<KuhnGame>(p, 1),
               -exploit::best_response_value_p0(p, 1, /*maximize=*/false), 1e-12);
}

// --- An independent value computation, for the chance-node path -------------
//
// The cross-check above cannot reach best_response.hpp's chance-node code at
// all: Kuhn's is_chance() is always false. Confirmed by mutation -- deleting the
// card-removal factor from the chance belief update leaves every Kuhn assertion
// passing.
//
// So: a direct expectimax that knows BOTH private cards and therefore carries no
// belief at all. Different algorithm, same answer required. Comparing it against
// expected_value() exercises the chance-node belief update on Leduc, where it
// actually runs.
template <class G>
double direct_value_p0(const br::Profile<G>& p, int p0c, int p1c,
                       const std::string& h) {
    if (G::is_terminal(h)) return G::terminal_utility_p0(h, p0c, p1c);
    if (G::is_chance(h)) {
        const auto probs = G::chance_probabilities(h, p0c, p1c);
        double v = 0.0;
        for (int k = 0; k < G::kNumCards; ++k) {
            if (probs[k] == 0.0) continue;
            v += probs[k] * direct_value_p0<G>(p, p0c, p1c, G::chance_child(h, k));
        }
        return v;
    }
    const int player = G::current_player(h);
    const auto legal = G::legal_actions(h);
    const auto& s = p.at(G::info_set_key(player == 0 ? p0c : p1c, h));
    double v = 0.0;
    for (int a = 0; a < G::kNumActions; ++a) {
        if (!legal[a] || s[a] == 0.0) continue;
        v += s[a] * direct_value_p0<G>(p, p0c, p1c, G::action_child(h, a));
    }
    return v;
}

template <class G>
double direct_game_value(const br::Profile<G>& p) {
    double v = 0.0;
    for (const auto& d : G::deals()) {
        v += d.probability * direct_value_p0<G>(p, d.p0_card, d.p1_card, "");
    }
    return v;
}

template <class G>
void check_value_agreement(const char* what, const br::Profile<G>& p) {
    const double direct = direct_game_value<G>(p);
    check_near(what, br::expected_value<G>(p, 0), direct, 1e-12);
    // Zero sum: player 1's expected value is player 0's negated.
    check_near("  zero-sum", br::expected_value<G>(p, 1), -direct, 1e-12);
}

}  // namespace

int main() {
    // 1) Agreement across the analytical equilibrium family, where both should
    //    report exactly zero.
    for (double alpha : {0.0, 1.0 / 6, 1.0 / 3}) {
        check_profile("equilibrium profile", analytical_equilibrium(alpha));
    }

    // 2) Agreement on strategies CFR actually produces, at several points along
    //    convergence -- these are far from equilibrium early on, which exercises
    //    branches a zero-exploitability profile never reaches.
    for (int n : {1, 10, 100, 1000, 100000}) {
        Trainer t;
        t.train(n);
        check_profile("trained profile", br::average_profile(t));
    }

    // 3) A deliberately lopsided profile: player 0 always bets, player 1 always
    //    passes. Wildly exploitable, and it forces the belief walk through
    //    fold-heavy subtrees the trained profiles barely touch.
    exploit::Profile skewed;
    for (const char* k : {"J", "Q", "K", "Jpb", "Qpb", "Kpb"})
        skewed[k] = exploit::Strategy{0.0, 1.0};
    for (const char* k : {"Jp", "Qp", "Kp", "Jb", "Qb", "Kb"})
        skewed[k] = exploit::Strategy{1.0, 0.0};
    check_profile("skewed profile", skewed);

    // 4) Chance-node coverage. The checks above never enter the chance branch,
    //    since Kuhn has no chance node below the deal. Compare the belief walk's
    //    expected value against a direct expectimax on LEDUC, where the public
    //    card makes that branch live.
    for (int n : {1, 10, 1000, 10000}) {
        LeducTrainer t;
        t.train(n);
        check_value_agreement<LeducGame>("leduc value (belief vs direct)",
                                         br::average_profile(t));
    }
    // Same check on Kuhn -- cheap, and it pins the non-chance path too.
    for (double alpha : {0.0, 1.0 / 3}) {
        check_value_agreement<KuhnGame>("kuhn value (belief vs direct)",
                                        analytical_equilibrium(alpha));
    }

    if (g_failures == 0) {
        std::cout << "belief-propagation best response matches the brute-force "
                     "oracle on Kuhn, and its value walk matches a direct "
                     "expectimax on Leduc\n";
        return 0;
    }
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
}
