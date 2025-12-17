# C++ Interpreter for Lama language

This is a homework #2 for Virtual Machines course at HSE 2025'.

*All commands assume that you are in the `/interpreter` folder relative to the project root.*

# Dependencies

- `lamac`
- `clang` (I used `18.1.3`)

_______

- Installation of `lamac` compiler can be done as described in the `README.md` at the root of the project.
- I decided to use clang because with it asan properly prints all line numbers when error appears, but you can change `CXX` variables at `Makefile` and `../runtime/Makefile` to `g++`/`gcc` accordingly.

# Build


The build consists of building 2 targets: `runtime.a` and `intepreter`. Run the following commands:

```bash
cd ../runtime && make clean # makes sure there are no old artifacts of runtime
cd ../interpreter # go back
make # build the intepreter and runtime
```

There will be an executable at `build/interpeter` location. By default the interpreter is built in debug config, to build release version run `make release` as a last step.

The executable is run in the following way:

```bash
./build/interpreter <bytecode_file.bc> [log_file]
```

where bytecode file is the output of `lamac -b path/to/source.lama` command and log file will be created by the interpreter and bytecode execution logs will be printed there (note that logging is only enabled in debug config of interpreter).

# Test

There is a `run_tests.sh` script which run tests. It can a single test and all tests from a folder. First you have to build the intepreter, see the "Build" section above.

The scripts is run in the following way:

```bash
./run_tests.sh [--silent] <test_folder> [test_name]
```

By default it writes all produced artifacts to the `test_results` folder, which include bytecode file for the specific test, output of the lama interpreter (`lamac -i path/to/source.lama`), output of my cpp interpeter, and the interpreter log file (if debug config was used to build it). To clear out this folder run `make clean_test`.

- Running a single test:
  ```bash
  ./run_tests.sh ../regression test001 # this will run test001.lama test from regression folder
  ```
- Running all tests from a folder:
  ```bash
  ./run_tests.sh ../regression # runs all tests from regression folder
  ./run_tests.sh --silent ../regression_long/expressions/ # runs without generating testing artifacts
  ./run_tests.sh --silent ../regression_long/deep-expressions/
  ```

# Results

1. Primary regression tests from `regression` folder:
    ```
    ...
    Test Summary:
      Total:  79
      Passed: 76
      Failed: 3
    ```
    All failed tests are related to problems with lama compiler:
    - Failed to generate bytecode file: `test054`, `test110`, `test111`.

2. Expressions tests from `regression_long/expressions` folder:
    ```
    ...
    Test Summary:
      Total:  9956
      Passed: 9956
      Failed: 0
    ```

3. Deep expressions tests from `regression_long/deep-expressions` folder:
    ```
    ...
    Test Summary:
      Total:  1000
      Passed: 1000
      Failed: 0
    ```

# Benchmark

To run benchmarks use `benchmark.sh` script. It has the same arguments as the `run_tests.sh` script (except for `--silent`):

```bash
./benchmark.sh <test_folder> [test_name]
```

It also can run either a single test or all tests from a folder.

Below is the summary for all tests from `../regression` folder with release version of c++ interpreter (`make release`):

```bash
...
=========================================
Summary
=========================================
  Total tests:  79
  Passed:       74
  Skipped:      3 # failed to generate bytecode
  Failed:       2 # c++ intepeter failed, see the "Results" section for details

  Total Lama time:  16344.68ms
  Total C++ time:   5382.94ms
  Overall speedup:  3.04x
=========================================
```

You can run the script locally to see per-test results. Note, that I have not tried running benchmarking on tests from the `../regression_long`, since they take too much time to execute.