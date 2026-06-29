#pragma once

// Kuhn poker game model.
//
// Two players, a three-card deck {J, Q, K}, each antes 1. Each player is dealt
// one private card; the third is unused. Player 0 acts first.
//
// The public betting history is a string of 'p' (pass) and 'b' (bet):
//   'p' = check when no bet is pending, or fold when facing a bet
//   'b' = bet   when no bet is pending, or call when facing a bet
//
// Terminal histories and their net result for the showdown/pot winner:
//   "pp"  check, check       -> showdown for the antes      (+/-1)
//   "bp"  bet, fold          -> bettor wins the pot          (+1)
//   "bb"  bet, call          -> showdown, one extra chip in (+/-2)
//   "pbp" check, bet, fold   -> bettor wins the pot          (+1)
//   "pbb" check, bet, call   -> showdown, one extra chip in (+/-2)

#include <array>
#include <string>
#include <string_view>

namespace poker_solver::kuhn {

inline constexpr int kNumPlayers = 2;
inline constexpr int kNumActions = 2;  // PASS, BET
inline constexpr int kNumCards   = 3;  // J=0, Q=1, K=2

enum Action : int { kPass = 0, kBet = 1 };

// 'J' < 'Q' < 'K'; a higher card index is a stronger hand.
inline char card_name(int card) {
    constexpr char names[kNumCards] = {'J', 'Q', 'K'};
    return names[card];
}

inline char action_char(int action) { return action == kBet ? 'b' : 'p'; }

// The player to act after `history`. Player 0 acts on the empty history.
inline int current_player(std::string_view history) {
    return static_cast<int>(history.size()) % kNumPlayers;
}

// Betting closes (the hand ends) on "pp", "bp", or "bb" as the last two actions.
// "pb" is the only non-terminal two-action history (a bet still faces a decision).
inline bool is_terminal(std::string_view history) {
    if (history.size() < 2) return false;
    const std::string_view last2 = history.substr(history.size() - 2);
    return last2 == "pp" || last2 == "bp" || last2 == "bb";
}

// Net chips won by current_player(history), given both private cards.
// Precondition: is_terminal(history).
inline int payoff(std::string_view history, int card_current, int card_opponent) {
    const std::string_view last2 = history.substr(history.size() - 2);
    const bool current_wins_showdown = card_current > card_opponent;

    // Opponent just folded to a bet -> the player to act takes the pot.
    if (last2 == "bp") return +1;

    // Check-check -> showdown contesting only the antes.
    if (last2 == "pp") return current_wins_showdown ? +1 : -1;

    // last2 == "bb": a bet was called -> showdown for one extra chip each.
    return current_wins_showdown ? +2 : -2;
}

// Information-set key: the acting player's private card followed by the public
// betting history, e.g. card Q after "pb" -> "Qpb".
inline std::string info_set_key(int card, std::string_view history) {
    std::string key;
    key.reserve(1 + history.size());
    key.push_back(card_name(card));
    key.append(history);
    return key;
}

// The six equally likely deals as {player0 card, player1 card} (distinct cards).
inline constexpr std::array<std::array<int, kNumPlayers>, 6> deals() {
    return {{ {0, 1}, {0, 2}, {1, 0}, {1, 2}, {2, 0}, {2, 1} }};
}

}  // namespace poker_solver::kuhn
