r"""Calls the free-tier Gemini API directly via REST to rewrite a file.

Uses the gemini-3.5-flash-lite model, matching the exact endpoint shape:

    POST https://generativelanguage.googleapis.com/v1beta/models/
         gemini-3.5-flash-lite:generateContent
    header: X-goog-api-key: <GEMINI_API_KEY>

No SDKs, no third-party GitHub Actions - one HTTP call, used because
Claude/OpenAI do not offer a free API tier and this repo is a
free-tier-only learning exercise.

This is a fully self-healing step. After each Gemini rewrite it runs,
in order: `clang-format -i` (autofix), a comment-wrap pass (clang-
format cannot reflow // comments), `cpplint` (the hard Google C++
Style gate), a full CMake configure+build+test+lcov run enforcing
100% LINE coverage (see scripts/check_coverage.sh for why branch
coverage isn't gated -- gcov's branch metric includes compiler-
generated paths this codebase can't realistically reach), and a real
benchmark comparison against the original code. If anything fails --
style, build, tests, OR the change isn't actually faster -- it sends
Gemini the exact error/benchmark output and asks for a corrected
version, up to MAX_ATTEMPTS times. It only exits 0 once the code is
genuinely clean AND measurably faster; the calling workflow must not
commit/push/open a PR unless this script succeeds, so a run that
can't self-heal leaves no broken branch behind.

Usage:
    python scripts/ai_improve.py \
        --instruction "improve algorithm and performance" \
        --file src/word_analyzer.cc \
        --tests tests/word_analyzer_test.cc
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import urllib.error
import urllib.request

API_URL = (
    "https://generativelanguage.googleapis.com/v1beta/models/"
    "gemini-3.5-flash-lite:generateContent"
)

MAX_ATTEMPTS = 4
BENCH_WORDS = 20_000
BENCH_VOCAB = 15_000
BUILD_DIR = "build"

# gemini-3.5-flash-lite is a small, free model: giving it the exact
# target implementation up front (rather than leaving the approach
# open-ended) drastically improves first-attempt success and keeps
# token usage low, since it doesn't have to "discover" the right C++
# idiom on its own.
INITIAL_PROMPT = """\
You are refactoring a C++26 CLI tool. Task: {instruction}

This file has three known inefficiencies. Fix them using EXACTLY
these standard-library idioms (no third-party dependencies):

1. `CountWords`: replace the vector + linear-search loop with a
   `std::unordered_map<std::string, int>`, then convert to the
   existing `std::vector<WordCount>` return shape.
2. `TopWords`: replace the manual per-slot max-scan with
   `std::partial_sort` or `std::nth_element` on a copy of the counts
   (descending by count), taking the first n -- pick whichever keeps
   the function signature `std::vector<WordCount> TopWords(...)`
   unchanged.
3. `Tokenize`: replace the character-by-character loop with a single
   pass using `std::isalpha`/`std::tolower` via iterators, or
   `<regex>` if you prefer -- either is fine as long as it's not
   character-by-character string concatenation.

Constraints:
- Follow the Google C++ Style Guide: 2-space indentation, K&R brace
  style, 80-column line limit including comments.
- Keep every public function/struct name, signature, and CLI behavior
  identical -- only the internal implementation changes.
- Do not add third-party dependencies.
- Do not add unused #include directives.
- The test file must keep 100% line coverage; update it as needed for
  any renamed internals, but keep all existing behavior covered.
- Do not add unnecessary complexity, extra abstraction layers, or
  extra work inside the hot path -- the idioms above are sufficient
  and are the fastest correct approach.

You must return TWO files, each in its own fenced block, in this exact
format and nothing else outside the blocks:

FILE: {file_path}
```cpp
<full new content of {file_path}>
```

FILE: {tests_path}
```cpp
<full new content of {tests_path}>
```

Current {file_path}:
```cpp
{file_content}
```

Current {tests_path}:
```cpp
{tests_content}
```
"""

RETRY_PROMPT = """\
Your previous rewrite of {file_path} and {tests_path} failed
verification. Fix it. Do not explain -- return only the two corrected
files in the same FILE: / fenced-block format as before.

Reminder of the required approach:
- `CountWords` must use `std::unordered_map<std::string, int>`.
- `TopWords` must use `std::partial_sort` or `std::nth_element`.
- `Tokenize` must use a single-pass character/iterator transform, not
  character-by-character string concatenation.
These are the fastest correct standard-library approach and must not
be replaced with anything slower or more complex.

Constraints (same as before):
- Every line, including comments, must be 80 characters or fewer.
- K&R brace style, 2-space indent (Google C++ Style Guide).
- Keep public signatures and CLI behavior identical.
- Do not add third-party dependencies or unused #include directives.
- 100% line test coverage must be maintained.

Verification output that must be fixed:
```
{error_output}
```

FILE: {file_path}
```cpp
<full corrected content of {file_path}>
```

FILE: {tests_path}
```cpp
<full corrected content of {tests_path}>
```

Your previous {file_path}:
```cpp
{file_content}
```

Your previous {tests_path}:
```cpp
{tests_content}
```
"""


def call_gemini(prompt: str, api_key: str) -> str:
    """Sends one generateContent request to the free-tier Gemini endpoint.

    Args:
        prompt: The full text prompt.
        api_key: Gemini API key (from the GEMINI_API_KEY secret).

    Returns:
        The concatenated text of the model's response parts.

    Raises:
        RuntimeError: If the HTTP call fails or the response has no
            candidates.
    """
    body = json.dumps({"contents": [{"parts": [{"text": prompt}]}]}).encode()
    request = urllib.request.Request(
        API_URL,
        data=body,
        method="POST",
        headers={
            "Content-Type": "application/json",
            "X-goog-api-key": api_key,
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=120) as response:
            payload = json.loads(response.read())
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode(errors="replace")
        raise RuntimeError(f"Gemini API error {exc.code}: {detail}") from exc

    candidates = payload.get("candidates", [])
    if not candidates:
        raise RuntimeError(f"No candidates in Gemini response: {payload}")

    parts = candidates[0]["content"]["parts"]
    return "".join(part.get("text", "") for part in parts)


def extract_files(response_text: str) -> dict[str, str]:
    """Extracts FILE: <path> / ```cpp fenced blocks from a response.

    Args:
        response_text: Raw model output.

    Returns:
        Mapping of file path to new file content.

    Raises:
        ValueError: If no fenced file blocks are found.
    """
    pattern = re.compile(
        r"FILE:\s*(\S+)\s*```(?:cpp|c\+\+)?\n(.*?)```", re.DOTALL
    )
    matches = pattern.findall(response_text)
    if not matches:
        raise ValueError(
            "Could not parse any FILE blocks from Gemini response:\n"
            + response_text
        )
    return {path.strip(): content for path, content in matches}


def write_files(files: dict[str, str], allowed_paths: tuple[str, ...]) -> None:
    """Writes extracted file contents to disk, ignoring unexpected paths.

    Args:
        files: Mapping of file path to new content.
        allowed_paths: The only paths this run is permitted to write.
    """
    for path, content in files.items():
        if path not in allowed_paths:
            print(f"warning: ignoring unexpected file in response: {path}")
            continue
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(content.rstrip() + "\n")
        print(f"wrote {path}")


def run_command(cmd: list[str]) -> tuple[bool, str]:
    """Runs a subprocess and captures combined output.

    Args:
        cmd: Command and arguments to run.

    Returns:
        (succeeded, combined_stdout_and_stderr).
    """
    result = subprocess.run(
        cmd, capture_output=True, text=True, timeout=300, check=False
    )
    output = (result.stdout or "") + (result.stderr or "")
    return result.returncode == 0, output


def measure_benchmark() -> float:
    """Builds (Release, no coverage) and runs the benchmark binary.

    A fresh Release build is used for benchmarking (not the
    coverage-instrumented Debug build, which would report misleading
    timings) so the measurement always reflects real optimized
    performance.

    Returns:
        Elapsed seconds reported by the benchmark binary.

    Raises:
        RuntimeError: If the build or run fails, or output can't be
            parsed.
    """
    configure_ok, configure_output = run_command(
        [
            "cmake", "-S", ".", "-B", "build_bench",
            "-DCMAKE_CXX_COMPILER=g++-14", "-DCMAKE_C_COMPILER=gcc-14",
            "-DCMAKE_BUILD_TYPE=Release", "-DWORDSTAT_BUILD_TESTS=OFF",
        ]
    )
    if not configure_ok:
        raise RuntimeError(f"cmake configure failed:\n{configure_output}")

    build_ok, build_output = run_command(
        ["cmake", "--build", "build_bench", "--target", "benchmark", "-j2"]
    )
    if not build_ok:
        raise RuntimeError(f"benchmark build failed:\n{build_output}")

    ok, output = run_command(
        [
            "./build_bench/benchmark",
            "--words", str(BENCH_WORDS), "--vocab", str(BENCH_VOCAB),
        ]
    )
    if not ok:
        raise RuntimeError(f"benchmark run failed:\n{output}")
    try:
        return float(output.strip().splitlines()[-1].split("elapsed_seconds=")[1])
    except (IndexError, ValueError) as exc:
        raise RuntimeError(
            f"could not parse benchmark output:\n{output}"
        ) from exc


def autofix_and_verify(
    *paths: str, baseline_seconds: float
) -> tuple[bool, str]:
    """Auto-fixes trivial issues, then runs all verification gates.

    Order: `clang-format -i` (safe autofix: whitespace, brace style,
    include ordering), a custom comment-line wrapper (clang-format
    cannot reflow over-long // comments), one more `clang-format -i`
    pass (formatting can shift after the wrap), `cpplint` (the hard
    Google C++ Style gate), a full build+test+100%-line-coverage run
    via scripts/check_coverage.sh, and finally a real benchmark
    comparison against `baseline_seconds`.

    Args:
        *paths: File paths to auto-fix and verify.
        baseline_seconds: The original (pre-AI) benchmark timing that
            the new code must beat.

    Returns:
        (all_passed, combined_error_output_if_any).
    """
    subprocess.run(
        ["clang-format", "--style=Google", "-i", *paths],
        check=False, timeout=60,
    )
    subprocess.run(
        ["python3", "scripts/wrap_comments.py", "--max-length", "80", *paths],
        check=False, timeout=60,
    )
    subprocess.run(
        ["clang-format", "--style=Google", "-i", *paths],
        check=False, timeout=60,
    )

    header = [p for p in paths if p.startswith("include/")]
    non_header = [p for p in paths if not p.startswith("include/")]
    if header:
        lint_ok, lint_output = run_command(
            ["cpplint", "--root=include", *header]
        )
        if not lint_ok:
            return False, f"cpplint failed:\n{lint_output}"
    if non_header:
        lint_ok, lint_output = run_command(["cpplint", *non_header])
        if not lint_ok:
            return False, f"cpplint failed:\n{lint_output}"

    coverage_ok, coverage_output = run_command(
        ["bash", "scripts/check_coverage.sh", BUILD_DIR]
    )
    if not coverage_ok:
        return False, (
            f"build/test/100%-line-coverage gate failed:\n{coverage_output}"
        )

    try:
        after_seconds = measure_benchmark()
    except RuntimeError as exc:
        return False, str(exc)

    if after_seconds >= baseline_seconds:
        return False, (
            "Performance regression: the change is not faster.\n"
            f"before={baseline_seconds:.4f}s after={after_seconds:.4f}s\n"
            "Use std::unordered_map for counting and std::partial_sort "
            "or std::nth_element for the top-n selection -- do not add "
            "extra work in the hot path."
        )

    print(
        f"benchmark: before={baseline_seconds:.4f}s "
        f"after={after_seconds:.4f}s "
        f"({baseline_seconds / after_seconds:.1f}x faster)"
    )
    return True, ""


def main() -> int:
    """CLI entry point. Returns 0 only once the code is verified clean."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--instruction", required=True)
    parser.add_argument("--file", required=True)
    parser.add_argument("--tests", required=True)
    args = parser.parse_args()

    api_key = os.environ.get("GEMINI_API_KEY")
    if not api_key:
        print("error: GEMINI_API_KEY not set", file=sys.stderr)
        return 1

    allowed_paths = (args.file, args.tests)
    original_file = open(args.file, encoding="utf-8").read()
    original_tests = open(args.tests, encoding="utf-8").read()

    baseline_seconds = measure_benchmark()
    print(f"baseline: {baseline_seconds:.4f}s")

    prompt = INITIAL_PROMPT.format(
        instruction=args.instruction,
        file_path=args.file,
        tests_path=args.tests,
        file_content=original_file,
        tests_content=original_tests,
    )

    for attempt in range(1, MAX_ATTEMPTS + 1):
        print(f"--- attempt {attempt}/{MAX_ATTEMPTS} ---")
        try:
            response_text = call_gemini(prompt, api_key)
            files = extract_files(response_text)
        except (RuntimeError, ValueError) as exc:
            print(f"attempt {attempt} failed to get a usable response: {exc}")
            if attempt == MAX_ATTEMPTS:
                break
            continue

        write_files(files, allowed_paths)
        passed, error_output = autofix_and_verify(
            *allowed_paths, baseline_seconds=baseline_seconds
        )

        if passed:
            print("verification passed: style clean, tests green, faster.")
            return 0

        print(f"attempt {attempt} did not pass verification:\n{error_output}")
        if attempt == MAX_ATTEMPTS:
            break

        current_file = open(args.file, encoding="utf-8").read()
        current_tests = open(args.tests, encoding="utf-8").read()
        prompt = RETRY_PROMPT.format(
            file_path=args.file,
            tests_path=args.tests,
            # Keep the TAIL, not the head: verbose cmake/test-run/lcov
            # preamble can easily exceed 4000 chars before ever
            # reaching the actual failure signal (e.g. the specific
            # uncovered file:line list check_coverage.sh prints last),
            # so truncating from the start would silently hide the one
            # thing Gemini actually needs to fix the failure.
            error_output=error_output[-4000:],
            file_content=current_file,
            tests_content=current_tests,
        )

    print(
        f"error: could not produce a passing change in {MAX_ATTEMPTS} "
        "attempts; restoring original files. No commit/PR will be made.",
        file=sys.stderr,
    )
    with open(args.file, "w", encoding="utf-8") as handle:
        handle.write(original_file)
    with open(args.tests, "w", encoding="utf-8") as handle:
        handle.write(original_tests)
    return 1


if __name__ == "__main__":
    sys.exit(main())
