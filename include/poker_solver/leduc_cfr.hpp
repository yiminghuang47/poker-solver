#pragma once

// Vanilla CFR for Leduc Hold'em.
//
// Like the Kuhn trainer, but the tree now has chance nodes (the public card)
// partway down and a variable set of legal actions per node, so:
//   * chance reach is tracked explicitly (it is NOT a global constant here — it
//     differs between round-1 and round-2 information sets), and
//   * regret matching and the average strategy are taken over legal actions only.
// Deals are enumerated over rank pairs with their true probabilities, and the
// public card over the remaining deck, so training is deterministic and exact.

#include "poker_solver/leduc.hpp"

#include <array>
#include <map>
#include <string>

namespace poker_solver::leduc_cfr {

struct Node {
    std::array<double, leduc::kNumActions> regret_sum{};
    std::array<double, leduc::kNumActions> strategy_sum{};

    std::array<double, leduc::kNumActions> strategy(
            double realization_weight,
            const std::array<bool, leduc::kNumActions>& legal) {
        std::array<double, leduc::kNumActions> strat{};
        double norm = 0.0;
        int num_legal = 0;
        for (int a = 0; a < leduc::kNumActions; ++a) {
            if (legal[a]) {
                strat[a] = regret_sum[a] > 0.0 ? regret_sum[a] : 0.0;
                norm += strat[a];
                ++num_legal;
            }
        }
        for (int a = 0; a < leduc::kNumActions; ++a) {
            if (!legal[a]) continue;
            strat[a] = norm > 0.0 ? strat[a] / norm : 1.0 / num_legal;
            strategy_sum[a] += realization_weight * strat[a];
        }
        return strat;
    }

    std::array<double, leduc::kNumActions> average_strategy() const {
        std::array<double, leduc::kNumActions> avg{};
        double norm = 0.0;
        for (double s : strategy_sum) norm += s;
        for (int a = 0; a < leduc::kNumActions; ++a) {
            avg[a] = norm > 0.0 ? strategy_sum[a] / norm : 0.0;
        }
        return avg;
    }
};

class Trainer {
public:
    // Run `iterations` of CFR; returns the average game value to player 0.
    double train(int iterations) {
        double util = 0.0;
        for (int i = 0; i < iterations; ++i) {
            for (int p0 = 0; p0 < leduc::kNumRanks; ++p0) {
                for (int p1 = 0; p1 < leduc::kNumRanks; ++p1) {
                    const double p_deal = (p0 == p1 ? 2.0 : 4.0) / 30.0;
                    util += p_deal * cfr(p0, p1, "", 1.0, 1.0, p_deal);
                }
            }
        }
        return util / iterations;
    }

    const std::map<std::string, Node>& nodes() const { return nodes_; }

private:
    std::map<std::string, Node> nodes_;

    double cfr(int p0rank, int p1rank, const std::string& history,
               double p0, double p1, double chance) {
        if (leduc::is_terminal(history)) {
            return leduc::terminal_utility_p0(history, p0rank, p1rank);
        }
        if (leduc::needs_public_card(history)) {
            double value = 0.0;
            for (int k = 0; k < leduc::kNumRanks; ++k) {
                const int remaining = 2 - (k == p0rank) - (k == p1rank);
                if (remaining <= 0) continue;
                const double p_card = remaining / 4.0;
                std::string next = history;
                next.push_back('/');
                next.push_back(leduc::rank_char(k));
                value += p_card * cfr(p0rank, p1rank, next, p0, p1, chance * p_card);
            }
            return value;
        }

        const int player = leduc::current_player(history);
        const int rank = player == 0 ? p0rank : p1rank;
        const auto legal = leduc::legal_actions(history);
        Node& node = nodes_[leduc::info_set_key(rank, history)];
        const double reach = player == 0 ? p0 : p1;
        const auto strat = node.strategy(reach, legal);

        std::array<double, leduc::kNumActions> util{};
        double node_util = 0.0;
        for (int a = 0; a < leduc::kNumActions; ++a) {
            if (!legal[a]) continue;
            std::string next = history;
            next.push_back(leduc::action_char(a));
            util[a] = player == 0
                ? cfr(p0rank, p1rank, next, p0 * strat[a], p1, chance)
                : cfr(p0rank, p1rank, next, p0, p1 * strat[a], chance);
            node_util += strat[a] * util[a];
        }

        // Regret is in the acting player's units (player 1's payoff is -u0).
        const double cf_reach = (player == 0 ? p1 : p0) * chance;
        for (int a = 0; a < leduc::kNumActions; ++a) {
            if (!legal[a]) continue;
            const double action_util = player == 0 ? util[a] : -util[a];
            const double base_util = player == 0 ? node_util : -node_util;
            node.regret_sum[a] += cf_reach * (action_util - base_util);
        }
        return node_util;
    }
};

}  // namespace poker_solver::leduc_cfr
