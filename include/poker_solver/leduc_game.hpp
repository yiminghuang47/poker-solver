#pragma once

// Adapts the Leduc Hold'em model to the Game concept.
//
// Leduc exercises every part of the interface: a chance node in the middle of
// the tree (the public card), a variable set of legal actions (the raise cap
// and the meaningless fold), and non-uniform deal probabilities.

#include "poker_solver/game.hpp"
#include "poker_solver/leduc.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace poker_solver {

struct LeducGame {
    static constexpr int kNumActions = leduc::kNumActions;  // fold, call, raise
    static constexpr int kNumCards   = leduc::kNumRanks;    // J, Q, K
    static constexpr int kNumDeals   = kNumCards * kNumCards;  // 9 rank pairs

    // The deck holds two of each rank, so there are 6*5 = 30 ordered deals: a
    // matched pair arises 2 ways, a mismatched pair 4.
    static std::array<game::Deal, kNumDeals> deals() {
        std::array<game::Deal, kNumDeals> out{};
        std::size_t i = 0;
        for (int p0 = 0; p0 < kNumCards; ++p0) {
            for (int p1 = 0; p1 < kNumCards; ++p1) {
                out[i++] = {p0, p1, (p0 == p1 ? 2.0 : 4.0) / 30.0};
            }
        }
        return out;
    }

    static int current_player(std::string_view h) { return leduc::current_player(h); }
    static bool is_terminal(std::string_view h) { return leduc::is_terminal(h); }
    static double terminal_utility_p0(std::string_view h, int p0c, int p1c) {
        return leduc::terminal_utility_p0(h, p0c, p1c);
    }

    static bool is_chance(std::string_view h) { return leduc::needs_public_card(h); }

    // Four cards remain after the deal. How many of rank k are among them
    // depends on both private cards, which is why this takes them both.
    static std::array<double, kNumCards> chance_probabilities(std::string_view,
                                                              int p0c, int p1c) {
        std::array<double, kNumCards> p{};
        for (int k = 0; k < kNumCards; ++k) {
            const int remaining = 2 - (k == p0c) - (k == p1c);
            p[k] = remaining > 0 ? remaining / 4.0 : 0.0;
        }
        return p;
    }
    static std::string chance_child(std::string_view h, int card) {
        std::string next(h);
        next.push_back('/');
        next.push_back(leduc::rank_char(card));
        return next;
    }

    static std::array<bool, kNumActions> legal_actions(std::string_view h) {
        return leduc::legal_actions(h);
    }
    static std::string action_child(std::string_view h, int a) {
        std::string next(h);
        next.push_back(leduc::action_char(a));
        return next;
    }

    static std::string info_set_key(int card, std::string_view h) {
        return leduc::info_set_key(card, h);
    }
};

static_assert(game::Game<LeducGame>);

}  // namespace poker_solver
