# cpp-cli-ai-demo -- C++26 AI-refactor teaching example

A deliberately **sub-optimal** CLI (`wordstat`) plus a fully automated,
human-gated pipeline for having an AI agent improve it, benchmark the
improvement, and ship it through PR review, changelog, and semver
release -- using **only free-tier tooling**. This is the C++/CMake
counterpart of `py-cli-ai-demo`, `csharp-cli-ai-demo`, and
`java-cli-ai-demo`, built like-for-like from the same design, with
every lesson learned across those three repos already applied here
from the start (see "Lessons carried over," below).

## What's sub-optimal here (on purpose)

`src/word_analyzer.cc`:
- `CountWords` uses a `std::vector<WordCount>` with linear search per
  word → **O(n²)**.
- `TopWords` does a linear max-scan per output slot → **O(n·k)**.
- `Tokenize` builds strings character-by-character instead of a
  single pass.

This is the "before" state an AI agent is asked to optimize (target:
`std::unordered_map<std::string, int>` + `std::partial_sort`/
`std::nth_element`, i.e. O(n log k)).

## Stack

| Concern | Tool |
|---|---|
| Runtime | C++26 (built with GCC 14, the first GCC release with real `-std=c++26` support) |
| Build | **CMake** (`cmake --build`) |
| Tests | **GoogleTest**, fetched via `FetchContent` from `github.com/google/googletest`, **100% line coverage gate** via `gcov`/`lcov` |
| Style | Google C++ Style Guide: `clang-format --style=Google` (autofix) + `cpplint` (Google's own linter, the hard gate) |
| CI | GitHub Actions |
| AI agent (free tier) | **Google Gemini `gemini-3.5-flash-lite`**, called directly via the REST `generateContent` endpoint |

### Why Gemini only, and why raw REST instead of an SDK/Action

Claude's and OpenAI's APIs are both pay-as-you-go with no ongoing free
tier, so neither fits a "free tier for learning" repo. Gemini's
`gemini-3.5-flash-lite` is available free via Google AI Studio, so
it's the only agent wired into automation here. Rather than depend on
a third-party GitHub Action, `scripts/ai_improve.py` (a small Python
orchestration script -- tooling only, not part of the C++ deliverable
itself, exactly analogous to the same script in the other three
reference repos) calls the endpoint directly:

```bash
curl "https://generativelanguage.googleapis.com/v1beta/models/gemini-3.5-flash-lite:generateContent" \
  -H "Content-Type: application/json" \
  -H "X-goog-api-key: $GEMINI_API_KEY" \
  -X POST -d '{"contents": [{"parts": [{"text": "..."}]}]}'
```

`scripts/ai_improve.py` is that same call in Python (`urllib`, no
dependencies), with a prompt asking for the full new content of the
source file and its test file, which it then parses and writes to
disk, then verifies with `clang-format` + `cpplint` +
`scripts/check_coverage.sh` (CMake build + GoogleTest + 100%-line
gate) + a real benchmark, retrying with Gemini up to 4 times on any
failure.

> **GitHub Copilot** and **OpenAI Codex** are covered as manual/optional
> comparison points -- see `docs/copilot-agent-note.md`. Codex CLI has
> no free API tier, so it isn't wired into any automation.

## Lessons carried over from the Python/C#/Java reference repos

Every one of these was a real bug found and fixed in an earlier
language's repo; all are addressed here from the start instead of
waiting to rediscover them:

- **Tokenize-safe benchmark corpus** (Python bug): the corpus
  generator uses distinct alphabetic words (`a`, `b`, ..., `z`, `aa`,
  ...) instead of `word0`, `word1`, etc. -- `Tokenize` keeps only
  letters, so a numeric-suffix vocabulary would silently collapse into
  one token and defeat `--vocab` entirely.
- **`Run(...)` name collision** (C++-specific, found while building
  this repo): calling the free function `Run` unqualified inside a
  `TEST()` body resolves to `testing::Test`'s own private `Run()`
  method instead, since class-scope lookup beats namespace-scope. All
  test call sites use `wordstat::Run(...)` explicitly.
- **A trivial `main()` cannot be unit tested without killing the test
  process** (Java's `Main.java`/C#'s launcher pattern): `main.cc` and
  `benchmark_main.cc` are kept to a single delegating call each, with
  all real logic in fully-tested library functions. Rather than
  excluding whole files/classes from coverage (the Java/C# approach),
  this repo uses precise `// LCOV_EXCL_START` / `// LCOV_EXCL_STOP`
  markers around just the `main()` body.
- **The exit-code-swallowed-by-a-pipe bug** (`ai-improve.yml`, first
  hit in the Python repo): output is redirected to a file with
  explicit `$?` capture, never piped through `tee` (which always
  exits 0 regardless of the real command's failure).
- **The default-branch-name assumption** (`main` vs `master`, first
  hit in the Python repo): every workflow uses `github.ref_name` /
  `gh api .../default_branch` instead of hardcoding `main`.
- **MSBuild-style multi-value flags** (C#'s `ThresholdType=line,branch`
  bug) has no C++ equivalent here, but the underlying lesson --
  verify third-party tool flag syntax against real documentation
  before shipping -- is why `scripts/check_coverage.sh`'s 100%-line-
  only gate (see below) is deliberate, not an oversight.
- **Toolchain version support gaps** (JaCoCo needing 0.8.14+ for Java
  25 bytecode): checked here too -- GCC 13 (Ubuntu 24.04's default)
  does not support `-std=c++26` at all; GCC 14 does. `ci.yml` installs
  `g++-14` explicitly rather than assuming the runner's default GCC
  is new enough.

## Why the coverage gate is 100% LINE, not line+branch

Unlike the JUnit/JaCoCo and xUnit/coverlet setups in the Java and C#
repos, `gcov`'s branch metric counts **compiler-generated branches**
this codebase's tests can never realistically reach -- exception-
cleanup landing pads for every `std::string`/`std::vector` operation,
template instantiation paths, etc. -- not just the branches actually
written here. Measured on this exact codebase: 100% line coverage
corresponds to only ~68% raw gcov branch coverage, entirely from
STL/compiler-generated paths, not missing tests. `scripts/
check_coverage.sh` documents this in its own header comment and
enforces line coverage at 100% (the literal "100% test coverage"
requirement), reporting branch coverage informationally rather than
silently ignoring the discrepancy or gating on a number that doesn't
mean what it means in the managed-runtime repos.

## Local setup

```bash
sudo apt-get install -y g++-14 cmake lcov clang-format
pip3 install --break-system-packages cpplint

cmake -S . -B build -DCMAKE_CXX_COMPILER=g++-14 -DCMAKE_C_COMPILER=gcc-14
cmake --build build -j"$(nproc)"
./build/wordstat_tests

clang-format --style=Google --dry-run --Werror include/wordstat/*.h src/*.cc tests/*.cc
cpplint --root=include include/wordstat/*.h
cpplint src/*.cc tests/*.cc

bash scripts/check_coverage.sh build   # build + test + 100% line coverage gate

./build/benchmark --words 20000 --vocab 15000
./build/wordstat some_file.txt -n 10

# try the AI rewrite locally (needs a free Gemini API key from
# https://aistudio.google.com/apikey)
export GEMINI_API_KEY=your_key_here
python3 scripts/ai_improve.py \
  --instruction "improve algorithm and performance" \
  --file src/word_analyzer.cc \
  --tests tests/word_analyzer_test.cc
```

## The automated workflows

### 1) `ai-improve.yml` -- "improve algorithm and performance"

Manual-trigger only (`workflow_dispatch`) -- this **is** the
supervision gate. Runs a baseline build+test, calls Gemini directly
via REST to rewrite `word_analyzer.cc` + its tests, then a self-heal
loop (`clang-format` → hard `cpplint` gate → 100%-line-coverage
build/test gate → real benchmark comparison), retrying with Gemini on
any failure up to 4 times, **refusing to open a PR if the change isn't
measurably faster**. Never merges anything -- branch protection
enforces that even if it tried.

```bash
gh workflow run ai-improve.yml -f instruction="improve algorithm and performance"
```

**Required repo secret:** `GEMINI_API_KEY` (free tier from Google AI
Studio -- https://aistudio.google.com/apikey).

### 2) `open-pr.yml` -- for your own hand-written changes

For changes you write yourself, no AI involved: create a feature
branch, commit and push it as usual -- `ci.yml` runs style+test
feedback on every branch push, not just on `main`/`master`. When
you're done:

```bash
gh workflow run open-pr.yml -f branch=feature/my-change -f title="My change"
```

Re-verifies clang-format + cpplint + 100% line coverage on that exact
branch before opening anything; never approves or merges.

### 3) `changelog-release.yml` -- "changelog"

Manual-trigger only. Takes a PR number and generates a `CHANGELOG.md`
section from the commit log, bumps semver (`patch`/`minor`/`major`),
tags, and cuts a GitHub Release. No AI involved -- deterministic on
purpose. Runs in two phases across two invocations because branch
protection blocks direct pushes to the protected branch: phase 1
writes the changelog entry on its own branch and prints the exact
`open-pr.yml` command to open a PR for it; after you approve and merge
that PR, phase 2 (re-running the same command) tags and publishes the
release. See the workflow file's header comment for the full detail.

```bash
gh workflow run changelog-release.yml -f pr_number=42 -f bump=patch
```

## Human review is enforced, not just convention

Same model as the other reference repos: branch protection (set up
once via repo Settings → Branches, or `setup-branch-protection.yml`
with a fine-grained PAT -- see that workflow's header comment) requires
at least one human approval and a passing `ci.yml` check before
anything merges, with no bypass even for the repo owner.

```bash
gh pr review <number> --approve
gh pr merge <number> --squash
```

## Economical AI usage (avoiding token-maxxing)

- The improve-workflow prompt is short, scoped to exactly two files,
  names the exact target STL idioms (`std::unordered_map`,
  `std::partial_sort`/`std::nth_element`) instead of leaving the
  approach open-ended, and `gemini-3.5-flash-lite` is the
  smallest/cheapest tier that can do this reliably.
- The self-heal retry loop (format → style → build/test → benchmark,
  up to 4 attempts) only calls Gemini again when something actually
  failed, feeding it the specific error instead of re-explaining the
  whole task.
- The workflow **fails fast** so a bad or wasteful AI run never
  reaches PR review, and branch protection means a bad PR can't reach
  the default branch even if review is skipped.
- Free-tier key only -- this repo is for building intuition before
  spending money on a paid plan/product.

## Layout

```
include/wordstat/word_analyzer.h   public API declarations
include/wordstat/benchmark.h       benchmark harness declarations
src/word_analyzer.cc               sub-optimal CLI logic (the "before")
src/main.cc                        tiny CLI launcher (LCOV-excluded)
src/benchmark.cc                   benchmark harness implementation (fully tested)
src/benchmark_main.cc              tiny benchmark launcher (LCOV-excluded)
tests/word_analyzer_test.cc        100%-line-coverage GoogleTest suite
tests/benchmark_test.cc            100%-line-coverage GoogleTest suite for the benchmark
CMakeLists.txt                     library + 2 executables + GoogleTest via FetchContent
CPPLINT.cfg                        disables 2 known-outdated cpplint checks (see file)
scripts/check_coverage.sh          build + test + 100%-line-coverage gate (see rationale above)
scripts/ai_improve.py              Gemini REST caller + self-heal retry loop
scripts/wrap_comments.py           auto-fixer for over-long // comment lines
.github/workflows/ci.yml                       clang-format + cpplint + 100%-line-coverage gate
.github/workflows/ai-improve.yml               manual: Gemini rewrite -> self-heal -> PR
.github/workflows/open-pr.yml                  manual: open a PR for your own branch
.github/workflows/changelog-release.yml        manual: merge PR -> changelog -> semver tag
.github/workflows/setup-branch-protection.yml  manual, one-time: enforce human review
```
