// Trains CFR on Leduc Hold'em: shows exploitability shrinking with iterations,
// the game value, the information-set count, and a few sample strategies.

#include "poker_solver/leduc.hpp"
#include "poker_solver/leduc_cfr.hpp"
#include "poker_solver/leduc_exploit.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>

namespace leduc = poker_solver::leduc;
namespace cfr = poker_solver::leduc_cfr;
namespace exploit = poker_solver::leduc_exploit;

int main(int argc, char** argv) {
    int iterations = (argc > 1) ? std::atoi(argv[1]) : 100000;
    if (iterations <= 0) iterations = 100000;

    std::cout << "poker-solver — Leduc Hold'em CFR\n\n";

    std::cout << "iterations   game value   exploitability\n";
    for (int n : {100, 1000, 10000, 100000}) {
        cfr::Trainer t;
        const double v = t.train(n);
        const double e = exploit::exploitability(t);
        std::cout << std::setw(10) << n << "   " << std::showpos << std::fixed
                  << std::setprecision(5) << std::setw(9) << v << std::noshowpos
                  << "   " << std::setprecision(6) << std::setw(10) << e << '\n';
    }

    cfr::Trainer trainer;
    const double v = trainer.train(iterations);
    const double e = exploit::exploitability(trainer);
    std::cout << "\nconverged at " << iterations << " iterations  (value "
              << std::showpos << std::setprecision(5) << v << std::noshowpos
              << ", exploitability " << std::setprecision(6) << e << ")\n";
    std::cout << "information sets: " << trainer.nodes().size() << "\n\n";

    std::cout << "sample average strategies   [ fold  call raise]\n";
    for (const char* key : {"J", "Q", "K", "Jr", "Kr"}) {
        const auto it = trainer.nodes().find(key);
        if (it == trainer.nodes().end()) continue;
        const auto a = it->second.average_strategy();
        std::cout << "  " << std::left << std::setw(6) << key << std::right
                  << std::setprecision(3) << std::setw(7) << a[leduc::kFold]
                  << std::setw(7) << a[leduc::kCall] << std::setw(7)
                  << a[leduc::kRaise] << '\n';
    }
    return 0;
}
