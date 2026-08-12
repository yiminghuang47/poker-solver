#pragma once

// Exact best response and exploitability, for any type satisfying the Game
// concept.
//
// Brute force over pure strategies (see exploitability.hpp, kept for Kuhn as an
// independent oracle) is hopeless past a toy game: Leduc's 288 information sets
// would mean roughly 2^144 candidates per player. This walk instead exploits the
// fact that a player's information set always contains their own private card,
// so the best response decomposes per private card. Fix the traverser's card,
// walk the tree once, and carry a belief over the opponent's hidden card.
//
// The belief is never renormalized as it descends, so its entries stay JOINT
// probabilities -- P(opponent holds o AND play reached here). That is what lets
// the terminal sum come out as an expectation and lets sibling branches be added
// rather than averaged.
//
//   exploitability = (BR0 + BR1) / 2,  >= 0, and 0 exactly at equilibrium.

#include "poker_solver/game.hpp"
#include "poker_solver/solver.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <string>
#include <string_view>

namespace poker_solver::best_response {

template <game::Game G>
using Strategy = std::array<double, G::kNumActions>;
template <game::Game G>
using Profile = std::map<std::string, Strategy<G>>;
template <game::Game G>
using Belief = std::array<double, G::kNumCards>;  // over the opponent's card

template <game::Game G>
Profile<G> average_profile(const solver::Trainer<G>& trainer) {
    Profile<G> profile;
    for (const auto& kv : trainer.nodes()) {
        profile[kv.first] = kv.second.average_strategy();
    }
    return profile;
}

namespace detail {

template <game::Game G>
struct BestResponder {
    int traverser;
    int my_card;
    const Profile<G>& profile;
    // When false, the traverser follows the profile instead of maximizing, so
    // the walk returns the expected value of the profile rather than the value
    // of a best response. Same belief machinery either way, which is what makes
    // it useful as a self-check -- see expected_value() below.
    bool maximize = true;

    // The traverser may be either seat, so map (my card, opponent card) back to
    // the (player 0, player 1) ordering the game model expects.
    int p0_card(int opp) const { return traverser == 0 ? my_card : opp; }
    int p1_card(int opp) const { return traverser == 0 ? opp : my_card; }

    Strategy<G> opp_strategy(int opp_card, std::string_view history,
                             const std::array<bool, G::kNumActions>& legal) const {
        const auto it = profile.find(G::info_set_key(opp_card, history));
        if (it != profile.end()) return it->second;
        // Information set absent from the profile (possible only for a profile
        // assembled outside this library): assume uniform over legal actions.
        Strategy<G> s{};
        const int n = static_cast<int>(std::ranges::count(legal, true));
        for (int a = 0; a < G::kNumActions; ++a) s[a] = legal[a] ? 1.0 / n : 0.0;
        return s;
    }

    double br(const std::string& history, const Belief<G>& w) const {
        if (G::is_terminal(history)) {
            double v = 0.0;
            for (int o = 0; o < G::kNumCards; ++o) {
                if (w[o] == 0.0) continue;
                const double u0 =
                    G::terminal_utility_p0(history, p0_card(o), p1_card(o));
                v += w[o] * (traverser == 0 ? u0 : -u0);
            }
            return v;
        }

        if (G::is_chance(history)) {
            double v = 0.0;
            for (int k = 0; k < G::kNumCards; ++k) {
                Belief<G> w2{};
                bool any = false;
                for (int o = 0; o < G::kNumCards; ++o) {
                    if (w[o] == 0.0) continue;
                    // The chance probability depends on the opponent's card
                    // (card removal), so it cannot be hoisted out as a scalar
                    // the way the trainer can hoist it -- it belongs in the
                    // belief, per opponent holding.
                    const auto p =
                        G::chance_probabilities(history, p0_card(o), p1_card(o));
                    if (p[k] == 0.0) continue;
                    w2[o] = w[o] * p[k];
                    any = true;
                }
                if (!any) continue;
                v += br(G::chance_child(history, k), w2);
            }
            return v;
        }

        const int player = G::current_player(history);
        const auto legal = G::legal_actions(history);

        if (player == traverser) {
            if (!maximize) {
                // Follow the profile. The traverser's own card is known, so this
                // is a plain scalar weighting -- no belief update.
                const Strategy<G> s = opp_strategy(my_card, history, legal);
                double v = 0.0;
                for (int a = 0; a < G::kNumActions; ++a) {
                    if (!legal[a] || s[a] == 0.0) continue;
                    v += s[a] * br(G::action_child(history, a), w);
                }
                return v;
            }
            // One action must cover every opponent holding at once -- we cannot
            // tell them apart. The belief-weighted sum is exactly the expected
            // value of committing to that action, and for a fixed private card
            // each information set appears once, so a local max at every node
            // composes into a globally optimal response.
            double best = -std::numeric_limits<double>::infinity();
            for (int a = 0; a < G::kNumActions; ++a) {
                if (!legal[a]) continue;
                best = std::max(best, br(G::action_child(history, a), w));
            }
            return best;
        }

        // Opponent node: their action distribution depends on their hidden card,
        // so seeing the action reweights the belief (Bayes, unnormalized).
        double v = 0.0;
        for (int a = 0; a < G::kNumActions; ++a) {
            if (!legal[a]) continue;
            Belief<G> w2{};
            bool any = false;
            for (int o = 0; o < G::kNumCards; ++o) {
                if (w[o] == 0.0) continue;
                const double sigma = opp_strategy(o, history, legal)[a];
                if (sigma == 0.0) continue;
                w2[o] = w[o] * sigma;
                any = true;
            }
            if (!any) continue;
            v += br(G::action_child(history, a), w2);
        }
        return v;
    }
};

}  // namespace detail

namespace detail {

// Shared driver for best_response_value() and expected_value(): seed the belief
// per traverser card and sum the walks.
template <game::Game G>
double walk(const Profile<G>& profile, int traverser, bool maximize) {
    double value = 0.0;
    for (int my_card = 0; my_card < G::kNumCards; ++my_card) {
        // Seed the belief straight from the deal distribution, as the joint
        // P(I hold my_card AND opponent holds o). Because it is joint rather
        // than conditional, the outer P(my_card) factor is already folded in and
        // the per-card results simply add.
        Belief<G> w{};
        bool any = false;
        for (const auto& d : G::deals()) {
            const int mine = traverser == 0 ? d.p0_card : d.p1_card;
            const int theirs = traverser == 0 ? d.p1_card : d.p0_card;
            if (mine != my_card) continue;
            w[theirs] += d.probability;
            any = true;
        }
        if (!any) continue;
        const BestResponder<G> responder{traverser, my_card, profile, maximize};
        value += responder.br("", w);
    }
    return value;
}

}  // namespace detail

// Best-response value to `traverser` against the profile's opponent strategy.
template <game::Game G>
double best_response_value(const Profile<G>& profile, int traverser) {
    return detail::walk<G>(profile, traverser, /*maximize=*/true);
}

// Expected value to `traverser` when BOTH players follow the profile. Uses the
// same belief machinery as the best response, so comparing it against a direct
// expectimax that knows both private cards (and therefore needs no beliefs at
// all) validates the belief and card-removal math independently -- including
// the chance-node path, which Kuhn cannot exercise because it has no chance node
// below the deal. See tests/test_equivalence.cpp.
template <game::Game G>
double expected_value(const Profile<G>& profile, int traverser) {
    return detail::walk<G>(profile, traverser, /*maximize=*/false);
}

// Note: Profile<G> is an alias for a plain std::map, so G cannot be deduced from
// a profile argument and must be named explicitly -- exploitability<KuhnGame>(p).
// That is a deliberate trade. Making Profile a distinct class template would fix
// deduction, but the point of leaving it a plain map is that exploitability.hpp's
// independent brute-force oracle consumes the *same object*, which is what makes
// the cross-validation in tests/test_equivalence.cpp possible.
template <game::Game G>
double exploitability(const Profile<G>& profile) {
    return (best_response_value<G>(profile, 0) +
            best_response_value<G>(profile, 1)) / 2.0;
}

// Deduction does work from a Trainer, so this overload needs no explicit arg.
template <game::Game G>
double exploitability(const solver::Trainer<G>& trainer) {
    return exploitability<G>(average_profile(trainer));
}

}  // namespace poker_solver::best_response
