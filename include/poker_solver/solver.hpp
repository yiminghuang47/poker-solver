#pragma once

// Vanilla counterfactual regret minimization (CFR) with regret matching, for any
// type satisfying the Game concept.
//
// Each information set accumulates counterfactual regret per action. Regret
// matching turns the positive regrets into the current strategy; the *average*
// strategy over all iterations is what converges to a Nash equilibrium.
//
// The tree is enumerated exhaustively -- every deal and every chance outcome,
// each weighted by its exact probability -- rather than sampled, so training is
// deterministic and two runs agree bit for bit. The cost is that one iteration
// touches the whole tree.
//
// Note on where the reach probabilities go, since it is the part that is easy to
// get wrong:
//   * the acting player's own reach weights the average strategy;
//   * the opponent's reach TIMES the chance reach weights the regret. Excluding
//     the actor's own reach is the "counterfactual" in CFR -- we are asking what
//     each action would have been worth had the actor steered play here.

#include "poker_solver/game.hpp"

#include <array>
#include <map>
#include <string>

namespace poker_solver::solver {

template <game::Game G>
struct Node {
    std::array<double, G::kNumActions> regret_sum{};
    std::array<double, G::kNumActions> strategy_sum{};
    // The legal-action mask, recorded on every visit. It is a fixed property of
    // the information set; average_strategy() needs it for the degenerate case.
    std::array<bool, G::kNumActions> legal{};

    // Regret matching, accumulated into strategy_sum weighted by the acting
    // player's reach probability.
    std::array<double, G::kNumActions> strategy(
            double realization_weight,
            const std::array<bool, G::kNumActions>& legal_actions) {
        legal = legal_actions;
        std::array<double, G::kNumActions> strat{};
        double norm = 0.0;
        int num_legal = 0;
        for (int a = 0; a < G::kNumActions; ++a) {
            if (legal[a]) {
                strat[a] = regret_sum[a] > 0.0 ? regret_sum[a] : 0.0;
                norm += strat[a];
                ++num_legal;
            }
        }
        for (int a = 0; a < G::kNumActions; ++a) {
            if (!legal[a]) continue;
            strat[a] = norm > 0.0 ? strat[a] / norm : 1.0 / num_legal;
            strategy_sum[a] += realization_weight * strat[a];
        }
        return strat;
    }

    // The average strategy over training -- this is what converges.
    std::array<double, G::kNumActions> average_strategy() const {
        std::array<double, G::kNumActions> avg{};
        double norm = 0.0;
        for (double s : strategy_sum) norm += s;
        if (norm > 0.0) {
            for (int a = 0; a < G::kNumActions; ++a) avg[a] = strategy_sum[a] / norm;
            return avg;
        }
        // strategy_sum is all zeros: every visit to this information set had
        // reach probability exactly 0, which happens for a handful of nodes at
        // very low iteration counts. Returning zeros would not be a probability
        // distribution, and the best-response walk would silently drop this
        // subtree's mass instead of exploring it.
        int num_legal = 0;
        for (bool b : legal) num_legal += b;
        if (num_legal == 0) return avg;  // node never visited; nothing to say
        for (int a = 0; a < G::kNumActions; ++a) {
            avg[a] = legal[a] ? 1.0 / num_legal : 0.0;
        }
        return avg;
    }
};

template <game::Game G>
class Trainer {
public:
    // Run `iterations` of CFR; returns the average game value to player 0.
    double train(int iterations) {
        double util = 0.0;
        const auto deals = G::deals();
        for (int i = 0; i < iterations; ++i) {
            for (const auto& d : deals) {
                util += d.probability *
                        cfr(d.p0_card, d.p1_card, "", 1.0, 1.0, d.probability);
            }
        }
        return util / iterations;
    }

    const std::map<std::string, Node<G>>& nodes() const { return nodes_; }

private:
    std::map<std::string, Node<G>> nodes_;

    // Returns the expected value of `history` to player 0.
    double cfr(int p0card, int p1card, const std::string& history,
               double p0, double p1, double chance) {
        if (G::is_terminal(history)) {
            return G::terminal_utility_p0(history, p0card, p1card);
        }
        if (G::is_chance(history)) {
            const auto probs = G::chance_probabilities(history, p0card, p1card);
            double value = 0.0;
            for (int k = 0; k < G::kNumCards; ++k) {
                if (probs[k] == 0.0) continue;
                // The probability both weights the child's value and enters the
                // chance reach carried below; forgetting either is a classic bug.
                value += probs[k] * cfr(p0card, p1card, G::chance_child(history, k),
                                        p0, p1, chance * probs[k]);
            }
            return value;
        }

        const int player = G::current_player(history);
        const int card = player == 0 ? p0card : p1card;
        const auto legal = G::legal_actions(history);
        Node<G>& node = nodes_[G::info_set_key(card, history)];
        const auto strat = node.strategy(player == 0 ? p0 : p1, legal);

        std::array<double, G::kNumActions> util{};
        double node_util = 0.0;
        for (int a = 0; a < G::kNumActions; ++a) {
            if (!legal[a]) continue;
            const std::string next = G::action_child(history, a);
            util[a] = player == 0
                ? cfr(p0card, p1card, next, p0 * strat[a], p1, chance)
                : cfr(p0card, p1card, next, p0, p1 * strat[a], chance);
            node_util += strat[a] * util[a];
        }

        // Values above are in player 0's units; regret must be in the acting
        // player's, so flip once here rather than negating at every level.
        const double cf_reach = (player == 0 ? p1 : p0) * chance;
        for (int a = 0; a < G::kNumActions; ++a) {
            if (!legal[a]) continue;
            const double action_util = player == 0 ? util[a] : -util[a];
            const double base_util = player == 0 ? node_util : -node_util;
            node.regret_sum[a] += cf_reach * (action_util - base_util);
        }
        return node_util;
    }
};

}  // namespace poker_solver::solver
