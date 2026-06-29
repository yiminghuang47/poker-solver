#pragma once

// Leduc Hold'em game model.
//
// Deck: 6 cards = two each of {J, Q, K}. Two players, each antes 1. Each is dealt
// one private card; then two betting rounds with a public card revealed between
// them. Fixed-limit betting: raise size 2 in round 1, 4 in round 2, at most two
// raises per round. Showdown: a private card matching the public card's rank is a
// pair and wins; otherwise the higher private rank wins; equal ranks split.
//
// The public history is a string:
//   round-1 actions, then (when round 1 closes without a fold) '/', the public
//   card rank char ('J'/'Q'/'K'), then round-2 actions.
// Action chars: 'f' fold, 'c' check/call, 'r' bet/raise.
//   "rc"          P0 bets, P1 calls   -> round 1 closed, deal public card
//   "rc/Qcc"      ...then on a Q public card, both check -> showdown
//   "crrf"        P0 checks, P1 bets, P0 raises, P1 folds -> P0 wins

#include <array>
#include <string>
#include <string_view>

namespace poker_solver::leduc {

inline constexpr int kNumPlayers = 2;
inline constexpr int kNumActions = 3;  // fold, call, raise
inline constexpr int kNumRanks   = 3;  // J=0, Q=1, K=2
inline constexpr int kMaxRaises  = 2;
inline constexpr int kRaiseSize[2] = {2, 4};  // round 1, round 2

enum Action : int { kFold = 0, kCall = 1, kRaise = 2 };

inline char rank_char(int r) {
    constexpr char names[kNumRanks] = {'J', 'Q', 'K'};
    return names[r];
}
inline int char_rank(char c) { return c == 'J' ? 0 : c == 'Q' ? 1 : 2; }
inline char action_char(int a) { return a == kFold ? 'f' : a == kCall ? 'c' : 'r'; }

// --- History parsing ---------------------------------------------------------

inline bool has_public(std::string_view h) {
    return h.find('/') != std::string_view::npos;
}
inline std::string_view round1_actions(std::string_view h) {
    const auto s = h.find('/');
    return h.substr(0, s == std::string_view::npos ? h.size() : s);
}
inline int public_rank(std::string_view h) {
    const auto s = h.find('/');
    return s == std::string_view::npos ? -1 : char_rank(h[s + 1]);
}
inline std::string_view round2_actions(std::string_view h) {
    const auto s = h.find('/');
    return s == std::string_view::npos ? std::string_view{} : h.substr(s + 2);
}
inline std::string_view current_actions(std::string_view h) {
    return has_public(h) ? round2_actions(h) : round1_actions(h);
}

// A round's betting is closed by a fold, a mutual check, or a call of a raise.
inline bool round_closed(std::string_view a) {
    if (a.empty()) return false;
    if (a.back() == 'f') return true;
    if (a == "cc") return true;
    return a.back() == 'c' && a.find('r') != std::string_view::npos;
}

inline bool ends_fold(std::string_view h) { return !h.empty() && h.back() == 'f'; }

inline bool is_terminal(std::string_view h) {
    if (h.empty()) return false;
    if (ends_fold(h)) return true;
    return has_public(h) && round_closed(round2_actions(h));
}

// True at the chance node where the public card is about to be dealt: round 1 has
// closed (by call/check, not fold) and no public card exists yet.
inline bool needs_public_card(std::string_view h) {
    if (has_public(h)) return false;
    const auto r1 = round1_actions(h);
    return round_closed(r1) && r1.back() != 'f';
}

// Player to act (player 0 acts first in each round). Valid only at decision nodes.
inline int current_player(std::string_view h) {
    return static_cast<int>(current_actions(h).size()) % kNumPlayers;
}

inline int num_raises(std::string_view a) {
    int n = 0;
    for (char c : a) n += (c == 'r');
    return n;
}

// Legal actions at a (non-terminal, non-chance) decision node.
inline std::array<bool, kNumActions> legal_actions(std::string_view h) {
    std::array<bool, kNumActions> legal{false, false, false};
    const auto a = current_actions(h);
    const bool facing_bet = !a.empty() && a.back() == 'r';
    const bool can_raise = num_raises(a) < kMaxRaises;
    legal[kCall] = true;                 // check or call is always available
    legal[kFold] = facing_bet;           // only meaningful facing a bet
    legal[kRaise] = can_raise;
    return legal;
}

// Information set: the acting player's private rank + the public history (which
// already encodes the public card and all betting; the opponent's card is not).
inline std::string info_set_key(int rank, std::string_view h) {
    std::string key;
    key.reserve(1 + h.size());
    key.push_back(rank_char(rank));
    key.append(h);
    return key;
}

// --- Payoffs -----------------------------------------------------------------

// Total chips each player has committed (antes + bets), parsed from the history.
inline std::array<int, kNumPlayers> contributions(std::string_view h) {
    std::array<int, kNumPlayers> total{1, 1};  // antes
    auto sim = [&](std::string_view a, int unit) {
        int level = 0;
        std::array<int, kNumPlayers> put{0, 0};
        int actor = 0;
        for (char ch : a) {
            if (ch == 'c') {
                put[actor] += level - put[actor];          // check (0) or call
            } else if (ch == 'r') {
                level += unit;
                put[actor] += level - put[actor];          // call up + raise
            }
            actor ^= 1;
        }
        total[0] += put[0];
        total[1] += put[1];
    };
    sim(round1_actions(h), kRaiseSize[0]);
    if (has_public(h)) sim(round2_actions(h), kRaiseSize[1]);
    return total;
}

// The player who made the final (folding) action.
inline int folder(std::string_view h) {
    const auto a = current_actions(h);
    return static_cast<int>(a.size() - 1) % kNumPlayers;
}

// Net chips won by player 0 at a terminal history. Precondition: is_terminal(h).
inline double terminal_utility_p0(std::string_view h, int p0rank, int p1rank) {
    const auto c = contributions(h);
    if (ends_fold(h)) {
        const int winner = 1 - folder(h);
        return winner == 0 ? +static_cast<double>(c[1]) : -static_cast<double>(c[0]);
    }
    // Showdown.
    const int pub = public_rank(h);
    int winner;
    if (p0rank == pub) winner = 0;
    else if (p1rank == pub) winner = 1;
    else if (p0rank > p1rank) winner = 0;
    else if (p0rank < p1rank) winner = 1;
    else return 0.0;  // equal ranks, no pair: split
    return winner == 0 ? +static_cast<double>(c[1]) : -static_cast<double>(c[0]);
}

}  // namespace poker_solver::leduc
