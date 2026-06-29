# poker-solver

A from-scratch **counterfactual regret minimization (CFR)** solver for two
imperfect-information poker games, in header-only C++20. It self-plays each game
to convergence, recovers an approximate **Nash equilibrium**, and proves how
close it got by measuring **exploitability** — the amount a worst-case opponent
could still win.

- **Kuhn poker** — the 3-card benchmark with a known analytical solution, used to
  validate the solver against ground truth.
- **Leduc Hold'em** — a substantially harder game (two betting rounds, a public
  card, fold/call/raise with a raise cap, ~288 information sets) with no tidy
  closed form, where exploitability is the *only* way to prove correctness.

## Results

CFR drives exploitability toward zero in both games (chips per game; the same
harness computes an exact best response against the average strategy):

| iterations | Kuhn exploitability | Leduc exploitability |
|-----------:|--------------------:|---------------------:|
| 100        | 0.0233              | 0.1846               |
| 1,000      | 0.0065              | 0.0362               |
| 10,000     | 0.0015              | 0.0093               |
| 100,000    | 0.00063             | 0.0029               |
| 1,000,000  | 0.00015             | —                    |

The solutions match the known benchmarks for both games:

| | information sets | game value (to player 0) | known value |
|---|---:|---:|---:|
| Kuhn  | 12  | −0.0558 | −1/18 ≈ −0.0556 |
| Leduc | 288 | −0.0857 | ≈ −0.0856 |

For Kuhn, the recovered strategy reproduces the analytical equilibrium family
(player 0 bluffs a Jack with probability α and bets a King with 3α; player 1
calls and bluffs at the textbook 1/3 frequencies). For Leduc, the strategy is
sensible poker: weak hands check and fold to aggression, strong hands raise, and
a King never folds to a raise.

## How it works

- **CFR + regret matching.** Each information set accumulates counterfactual
  regret per action; the current strategy is the regret-matched distribution, and
  the *average* strategy over all iterations converges to a Nash equilibrium.
  Training enumerates the full game tree deterministically (all deals, all public
  cards) with exact chance probabilities, so runs are reproducible.
- **Exploitability.** `(BR0 + BR1) / 2`, where `BRt` is a player's best-response
  value against the average strategy. For Kuhn, the best response is found by
  brute force over pure strategies; for Leduc that is infeasible, so the best
  response decomposes per private card and propagates a belief over the
  opponent's hidden card — exact, and fast.

## Build & run

Requires a C++20 compiler. The library is header-only (`include/`).

With CMake:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure   # runs all five test suites
./build/poker_solver_app        # Kuhn
./build/leduc_solver_app        # Leduc
```

Or compile a demo directly:

```sh
g++ -std=c++20 -O2 -Iinclude src/leduc_main.cpp -o leduc && ./leduc 100000
```

Both demos take an optional iteration count, e.g. `./poker_solver_app 1000000`.

## Layout

```
include/poker_solver/
  kuhn.hpp            Kuhn game model
  cfr.hpp             CFR trainer for Kuhn
  exploitability.hpp  best response + exploitability for Kuhn
  leduc.hpp           Leduc game model
  leduc_cfr.hpp       CFR trainer for Leduc (chance nodes, variable actions)
  leduc_exploit.hpp   per-card best response + exploitability for Leduc
src/
  main.cpp            Kuhn demo (convergence table + strategy)
  leduc_main.cpp      Leduc demo
tests/                model, convergence, and exploitability tests for both games
```

## References

- Zinkevich, Johanson, Bowling, Piccione, *Regret Minimization in Games with
  Incomplete Information* (NeurIPS 2007) — CFR.
- Neller & Lanctot, *An Introduction to Counterfactual Regret Minimization* (2013).
- Southey et al., *Bayes' Bluff: Opponent Modelling in Poker* (UAI 2005) — Leduc.
