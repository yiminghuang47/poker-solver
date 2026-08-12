#pragma once

// Adapts the Kuhn poker model to the Game concept.
//
// Kuhn is the degenerate case of the interface: its only chance event is the
// deal at the root, and both actions are always legal. Those parts of the
// concept are therefore trivial here.

#include "poker_solver/game.hpp"
#include "poker_solver/kuhn.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace poker_solver {

struct KuhnGame {
    static constexpr int kNumActions = kuhn::kNumActions;  // pass, bet
    static constexpr int kNumCards   = kuhn::kNumCards;    // J, Q, K
    static constexpr int kNumDeals   = 6;  // ordered pairs of distinct cards

    static std::array<game::Deal, kNumDeals> deals() {
        std::array<game::Deal, kNumDeals> out{};
        std::size_t i = 0;
        for (const auto& d : kuhn::deals()) {
            out[i++] = {d[0], d[1], 1.0 / kNumDeals};  // all six equally likely
        }
        return out;
    }

    static int current_player(std::string_view h) { return kuhn::current_player(h); }
    static bool is_terminal(std::string_view h) { return kuhn::is_terminal(h); }

    // kuhn::payoff is expressed in the acting player's units; the concept wants
    // player 0's, so flip when the player to act at the terminal is player 1.
    static double terminal_utility_p0(std::string_view h, int p0c, int p1c) {
        const int cp = kuhn::current_player(h);
        const int v = kuhn::payoff(h, cp == 0 ? p0c : p1c, cp == 0 ? p1c : p0c);
        return cp == 0 ? v : -v;
    }

    // Kuhn deals both cards at the root and reveals nothing afterwards.
    static bool is_chance(std::string_view) { return false; }
    static std::array<double, kNumCards> chance_probabilities(std::string_view,
                                                              int, int) {
        return {};  // unreachable: is_chance() is never true
    }
    static std::string chance_child(std::string_view h, int) {
        return std::string(h);  // unreachable, for the same reason
    }

    // Pass and bet are always both available; nothing is ever illegal.
    static std::array<bool, kNumActions> legal_actions(std::string_view) {
        return {true, true};
    }
    static std::string action_child(std::string_view h, int a) {
        std::string next(h);
        next.push_back(kuhn::action_char(a));
        return next;
    }

    static std::string info_set_key(int card, std::string_view h) {
        return kuhn::info_set_key(card, h);
    }
};

static_assert(game::Game<KuhnGame>);

}  // namespace poker_solver
