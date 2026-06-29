// Demonstrates the Kuhn poker game model: prints the betting tree and the full
// set of information sets. The CFR solver will take over this entry point next.

#include "poker_solver/kuhn.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace kuhn = poker_solver::kuhn;

namespace {

// Recursively print the betting tree (cards aside), marking terminal nodes.
void print_tree(const std::string& history, int depth) {
    const std::string indent(static_cast<std::size_t>(depth) * 2, ' ');
    const std::string label = history.empty() ? "(start)" : history;

    if (kuhn::is_terminal(history)) {
        std::cout << indent << label << "  [terminal]\n";
        return;
    }

    std::cout << indent << label << "  -> player " << kuhn::current_player(history)
              << " to act\n";
    print_tree(history + kuhn::action_char(kuhn::kPass), depth + 1);
    print_tree(history + kuhn::action_char(kuhn::kBet), depth + 1);
}

// Collect every non-terminal history at which `player` is the one to act.
void collect_decision_histories(const std::string& history, int player,
                                std::vector<std::string>& out) {
    if (kuhn::is_terminal(history)) return;
    if (kuhn::current_player(history) == player) out.push_back(history);
    collect_decision_histories(history + kuhn::action_char(kuhn::kPass), player, out);
    collect_decision_histories(history + kuhn::action_char(kuhn::kBet), player, out);
}

}  // namespace

int main() {
    std::cout << "poker-solver — Kuhn poker model\n\n";

    std::cout << "Betting tree:\n";
    print_tree("", 0);

    std::cout << "\nInformation sets (12 total):\n";
    for (int player = 0; player < kuhn::kNumPlayers; ++player) {
        std::vector<std::string> histories;
        collect_decision_histories("", player, histories);
        std::cout << "  player " << player << ":";
        for (const auto& h : histories) {
            for (int card = 0; card < kuhn::kNumCards; ++card) {
                std::cout << ' ' << kuhn::info_set_key(card, h);
            }
        }
        std::cout << '\n';
    }

    return 0;
}
