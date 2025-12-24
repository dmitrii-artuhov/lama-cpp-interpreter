# Bytecode analyser for Lama language

This is a homework #3 for Virtual Machines course at HSE 2025'.

The analyser counts the number of occurances of bytecode instruction sequences of length 1 and 2 in each basic block of the bytecode CFG.

*All commands assume that you are in the `/analyser` folder relative to the project root.*

# Dependencies

- `lamac`
- `clang` (I used `18.1.3`)

_______

- Installation of `lamac` compiler can be done as described in the `README.md` at the root of the project.
- I decided to use clang because with it asan properly prints all line numbers when error appears, but you can change `CXX` variables at `Makefile` and `../runtime/Makefile` to `g++`/`gcc` accordingly.

# Build

The build consists of building 1 target: `analyser`. Run the following commands:

```bash
make # build the analyser
```

There will be an executable at `build/analyser` location. By default the analyser is built in debug config, to build release version run `make release` instead.

The executable is run in the following way:

```bash
./build/analyser <bytecode_file.bc> [log_file]
```

where bytecode file is the output of `lamac -b path/to/source.lama` command and log file will be created by the interpreter and bytecode execution logs will be printed there (note that logging is only enabled in debug config of interpreter).

# Test

There is a `run_tests.sh` script which run tests. It can a single test and all tests from a folder. First you have to build the analyser, see the "Build" section above.

The scripts is run in the following way:

```bash
./run_tests.sh [--silent] <test_folder> [test_name]
```

By default it writes all produced artifacts to the `test_results` folder, which include bytecode file for the specific test and the analyser log (if debug config was used to build it) and results file. To clear out this folder run `make clean_test`.

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

1. Primary test is the `performace/Sort.lama`, its results are the following:
    ```
    Analyzing bytecode...
    Counts of 1 instructions:
      31  DROP
      28  DUP
      21  ELEM
      16  CONST 1
      11  CONST 0
      7  LD A(0)
      5  END
      5  JMP 0x000002fa
      4  SEXP cons 2
      3  ST L(0)
      3  LD L(3)
      3  LD L(0)
      3  ARRAY 2
      3  CALL 0x0000015f 1
      3  JMP 0x0000015e
      3  CALL_ARRAY 2
      2  CALL 0x0000002b 1
      2  JMP 0x00000074
      2  BEGIN 1 0
      2  BINOP ==
      2  LD L(1)
      2  CALL 0x00000097 1
      2  TAG cons 2
      1  FAIL 7 17
      1  LINE 3
      1  FAIL 14 9
      1  LINE 16
      1  CALL 0x00000075 1
      1  BEGIN 2 0
      1  BEGIN 1 6
      1  LINE 5
      1  LINE 6
      1  LINE 7
      1  LINE 9
      1  LINE 14
      1  LINE 15
      1  CJMP_Z 0x0000006a
      1  LINE 18
      1  LINE 20
      1  LINE 24
      1  LINE 25
      1  LINE 27
      1  <end>
      1  ST L(3)
      1  BINOP >
      1  CONST 10000
      1  JMP 0x00000106
      1  JMP 0x00000150
      1  JMP 0x00000182
      1  JMP 0x000002cb
      1  JMP 0x000002de
      1  LD L(2)
      1  LD L(4)
      1  LD L(5)
      1  ST L(1)
      1  ST L(2)
      1  BEGIN 1 1
      1  ST L(4)
      1  ST L(5)
      1  CJMP_Z 0x00000112
      1  CJMP_Z 0x00000258
      1  BINOP -
      1  CJMP_Z 0x000000bf
      1  CJMP_NZ 0x00000118
      1  CJMP_NZ 0x0000027d
      1  CJMP_NZ 0x00000188
      1  CJMP_NZ 0x000001ac
      1  CJMP_NZ 0x000000c5

    Counts of 2 instructions:
      13  CONST 1 | ELEM
      11  DUP | CONST 1
      11  DROP | DUP
      10  DROP | DROP
      8  CONST 0 | ELEM
      7  DUP | CONST 0
      7  ELEM | DROP
      4  DUP | DUP
      3  CALL_ARRAY 2 | JMP 0x000002fa
      3  DUP | ARRAY 2
      3  ELEM | ST L(0)
      3  ST L(0) | DROP
      2  DUP | TAG cons 2
      2  SEXP cons 2 | CALL_ARRAY 2
      2  CALL 0x0000015f 1 | DUP
      2  ELEM | CONST 0
      2  ELEM | CONST 1
      1  CALL 0x0000002b 1 | CALL 0x00000075 1
      1  LD A(0) | LD A(0)
      1  LD L(2) | CALL 0x0000015f 1
      1  LD A(0) | CJMP_Z 0x0000006a
      1  LD A(0) | CALL 0x0000015f 1
      1  CALL 0x0000015f 1 | CONST 1
      1  LD A(0) | CALL 0x00000097 1
      1  LD A(0) | CALL_ARRAY 2
      1  ST L(2) | DROP
      1  ST L(1) | DROP
      1  CALL 0x0000002b 1 | SEXP cons 2
      1  BEGIN 2 0 | LINE 25
      1  BEGIN 1 6 | LINE 3
      1  BEGIN 1 1 | LINE 14
      1  BEGIN 1 0 | LINE 24
      1  BEGIN 1 0 | LINE 18
      1  ST L(5) | DROP
      1  ST L(4) | DROP
      1  ST L(3) | DROP
      1  LINE 5 | LD L(3)
      1  LINE 27 | CONST 10000
      1  LINE 25 | LINE 27
      1  LINE 24 | LD A(0)
      1  LINE 20 | LD A(0)
      1  LINE 18 | LINE 20
      1  LINE 16 | LD L(0)
      1  LINE 15 | LD L(0)
      1  LINE 14 | LD A(0)
      1  LINE 9 | LD A(0)
      1  LINE 7 | LD L(2)
      1  LINE 6 | LD L(1)
      1  CALL 0x00000075 1 | END
      1  LINE 3 | LD A(0)
      1  FAIL 14 9 | JMP 0x0000015e
      1  FAIL 7 17 | JMP 0x000002fa
      1  ARRAY 2 | CJMP_NZ 0x000000c5
      1  ARRAY 2 | CJMP_NZ 0x0000027d
      1  ARRAY 2 | CJMP_NZ 0x00000118
      1  TAG cons 2 | CJMP_NZ 0x000001ac
      1  TAG cons 2 | CJMP_NZ 0x00000188
      1  CALL 0x00000097 1 | END
      1  CALL 0x00000097 1 | JMP 0x0000015e
      1  SEXP cons 2 | JMP 0x00000074
      1  DROP | LINE 5
      1  DROP | LD L(5)
      1  DROP | JMP 0x000002de
      1  DROP | JMP 0x000002cb
      1  DROP | JMP 0x00000182
      1  DROP | JMP 0x00000150
      1  DROP | JMP 0x00000106
      1  DROP | CONST 0
      1  END | <end>
      1  JMP 0x000002fa | JMP 0x000002fa
      1  SEXP cons 2 | CALL 0x0000015f 1
      1  DROP | LINE 15
      1  CONST 10000 | CALL 0x0000002b 1
      1  CONST 1 | LINE 6
      1  CONST 1 | BINOP ==
      1  CONST 1 | BINOP -
      1  CONST 0 | LINE 9
      1  CONST 0 | JMP 0x00000074
      1  CONST 0 | BINOP ==
      1  BINOP == | CJMP_Z 0x000000bf
      1  BINOP == | CJMP_Z 0x00000112
      1  BINOP > | CJMP_Z 0x00000258
      1  LD L(0) | JMP 0x0000015e
      1  LD A(0) | CONST 1
      1  LD L(5) | LD L(3)
      1  LD L(4) | SEXP cons 2
      1  LD L(3) | LD L(4)
      1  LD L(3) | LD L(1)
      1  LD L(3) | LD L(0)
      1  BINOP - | CALL 0x0000002b 1
      1  LD L(1) | LD L(3)
      1  LD L(1) | BINOP >
      1  LD L(0) | CALL 0x00000097 1
      1  LD A(0) | DUP
      1  LD L(0) | SEXP cons 2
      1  ELEM | ST L(5)
      1  ELEM | ST L(4)
      1  ELEM | ST L(3)
      1  ELEM | ST L(2)
      1  ELEM | ST L(1)
      1  ELEM | DUP
      1  ELEM | SEXP cons 2
      1  DUP | DROP
      1  DROP | LINE 16
    ```