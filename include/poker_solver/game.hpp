#pragma once

// The Game concept: everything the generic CFR trainer (solver.hpp) and the
// generic best-response walk (best_response.hpp) need to know about a two-
// player, zero-sum, imperfect-information poker game in which each player holds
// one private card.
//
// Both Kuhn poker and Leduc Hold'em satisfy it -- see kuhn_game.hpp and
// leduc_game.hpp, each of which static_asserts conformance at the bottom.
//
// Honest note on the shape of this interface: it is the *union* of what the two
// games need, not their intersection. Kuhn has no chance node below the deal and
// no illegal actions, so KuhnGame::is_chance() is always false and its
// legal_actions() is always all-true. What the abstraction buys is not fewer
// lines -- it is one CFR implementation and one best-response implementation
// instead of two, and the cross-validation in tests/test_equivalence.cpp, which
// checks the belief-propagation best response against Kuhn's independent
// brute-force oracle. That test is impossible while the two games share no code.

#include <array>
#include <concepts>
#include <string>
#include <string_view>

namespace poker_solver::game {

// One way the private cards can be dealt, with the probability of that deal.
// The probabilities over all deals sum to 1.
struct Deal {
    int p0_card;
    int p1_card;
    double probability;
};

template <class G>
concept Game = requires(std::string_view h, int card, int p0c, int p1c, int a) {
    // --- Sizes, all compile-time constants ---------------------------------
    { G::kNumActions } -> std::convertible_to<int>;
    { G::kNumCards }   -> std::convertible_to<int>;
    { G::kNumDeals }   -> std::convertible_to<int>;

    // --- The chance event at the root: dealing the private cards -----------
    { G::deals() } -> std::same_as<std::array<Deal, G::kNumDeals>>;

    // --- Tree navigation ----------------------------------------------------
    { G::current_player(h) } -> std::same_as<int>;
    { G::is_terminal(h) }    -> std::same_as<bool>;
    // Net chips won by player 0. Precondition: is_terminal(h).
    { G::terminal_utility_p0(h, p0c, p1c) } -> std::same_as<double>;

    // --- Chance nodes inside the tree (Leduc's public card; none in Kuhn) ---
    { G::is_chance(h) } -> std::same_as<bool>;
    // Probability of each card being revealed, given both private cards --
    // it depends on both because they remove cards from the deck. Entries are
    // 0 for cards the deal has exhausted. Only called when is_chance(h).
    { G::chance_probabilities(h, p0c, p1c) }
        -> std::same_as<std::array<double, G::kNumCards>>;
    { G::chance_child(h, card) } -> std::same_as<std::string>;

    // --- Decision nodes -----------------------------------------------------
    { G::legal_actions(h) } -> std::same_as<std::array<bool, G::kNumActions>>;
    { G::action_child(h, a) } -> std::same_as<std::string>;

    // --- Information sets ---------------------------------------------------
    // The acting player's private card plus everything public. Must not encode
    // the opponent's card.
    { G::info_set_key(card, h) } -> std::same_as<std::string>;
};

}  // namespace poker_solver::game
