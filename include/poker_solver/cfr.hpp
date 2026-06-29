#pragma once

// Vanilla counterfactual regret minimization (CFR) with regret matching.
//
// Each information set keeps cumulative counterfactual regrets and a cumulative
// strategy. Regret matching turns positive regrets into the current strategy;
// the *average* strategy over all iterations converges to a Nash equilibrium.
//
// The deal (chance node) is enumerated over all six equally likely card
// assignments every iteration, so training is fully deterministic. The uniform
// 1/6 chance factor is a global constant: it cancels under per-info-set regret
// matching and is correctly excluded from the player-reach averaging weight, so
// it is omitted from the recursion and only divided back out of the game value.

#include "poker_solver/kuhn.hpp"

#include <array>
#include <map>
#include <string>

namespace poker_solver::cfr {

struct Node {
    std::array<double, kuhn::kNumActions> regret_sum{};
    std::array<double, kuhn::kNumActions> strategy_sum{};

    // Current strategy from regret matching, accumulated into strategy_sum
    // weighted by the acting player's reach probability.
    std::array<double, kuhn::kNumActions> strategy(double realization_weight) {
        std::array<double, kuhn::kNumActions> strat{};
        double norm = 0.0;
        for (int a = 0; a < kuhn::kNumActions; ++a) {
            strat[a] = regret_sum[a] > 0.0 ? regret_sum[a] : 0.0;
            norm += strat[a];
        }
        for (int a = 0; a < kuhn::kNumActions; ++a) {
            strat[a] = norm > 0.0 ? strat[a] / norm : 1.0 / kuhn::kNumActions;
            strategy_sum[a] += realization_weight * strat[a];
        }
        return strat;
    }

    // The average strategy over training — this is what converges to equilibrium.
    std::array<double, kuhn::kNumActions> average_strategy() const {
        std::array<double, kuhn::kNumActions> avg{};
        double norm = 0.0;
        for (double s : strategy_sum) norm += s;
        for (int a = 0; a < kuhn::kNumActions; ++a) {
            avg[a] = norm > 0.0 ? strategy_sum[a] / norm : 1.0 / kuhn::kNumActions;
        }
        return avg;
    }
};

class Trainer {
public:
    // Run `iterations` of CFR; returns the average game value to player 0
    // (Kuhn theory: -1/18 ≈ -0.0556).
    double train(int iterations) {
        double util = 0.0;
        const auto deals = kuhn::deals();
        for (int i = 0; i < iterations; ++i) {
            for (const auto& deal : deals) {
                util += cfr(deal, "", 1.0, 1.0);
            }
        }
        return util / (static_cast<double>(iterations) * deals.size());
    }

    const std::map<std::string, Node>& nodes() const { return nodes_; }

private:
    std::map<std::string, Node> nodes_;

    // Returns the expected value of `history` to the player about to act.
    double cfr(const std::array<int, kuhn::kNumPlayers>& cards,
               const std::string& history, double p0, double p1) {
        const int player = kuhn::current_player(history);
        const int opponent = 1 - player;

        if (kuhn::is_terminal(history)) {
            return kuhn::payoff(history, cards[player], cards[opponent]);
        }

        Node& node = nodes_[kuhn::info_set_key(cards[player], history)];
        const double reach = (player == 0) ? p0 : p1;
        const auto strat = node.strategy(reach);

        std::array<double, kuhn::kNumActions> util{};
        double node_util = 0.0;
        for (int a = 0; a < kuhn::kNumActions; ++a) {
            const std::string next = history + kuhn::action_char(a);
            // The child's value is from the opponent's perspective, so negate.
            util[a] = (player == 0) ? -cfr(cards, next, p0 * strat[a], p1)
                                    : -cfr(cards, next, p0, p1 * strat[a]);
            node_util += strat[a] * util[a];
        }

        const double counterfactual_reach = (player == 0) ? p1 : p0;
        for (int a = 0; a < kuhn::kNumActions; ++a) {
            node.regret_sum[a] += counterfactual_reach * (util[a] - node_util);
        }
        return node_util;
    }
};

}  // namespace poker_solver::cfr
