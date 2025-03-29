#!/usr/bin/env bash

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 XCC_PATH"
  exit 1
fi

# Configurable params
TEST_COUNT="${TEST_COUNT:=25}"
STOP_ON_FAILURE="${STOP_ON_FAILURE:=0}"
QUIET="${QUIET:=1}"

# Don't touch
XCC_PATH=$(realpath $1)
DIR=$(cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd)

# Constants
C_RED="\033[31m"
C_GREEN="\033[32m"
C_RESET="\033[0m"

# Runs single test ($1 - test number)
run_test() {
  test_num=$1

  if [ $QUIET -eq 0 ]; then
    echo "======================================== TEST $test_num ========================================"
    $XCC_PATH $test_num.txt
  else
    $XCC_PATH $test_num.txt >/dev/null
  fi

  exit_code=$?

  if [ $QUIET -eq 1 ]; then
    printf "TEST %03d -- " $test_num
  fi

  if [ $exit_code -ne 0 ]; then
    if [ $STOP_ON_FAILURE -eq 1 ]; then
      echo "Test $i failed with exit code $exit_code"
      exit 1
    else
      [ $QUIET -eq 1 ] && echo -e "${C_RED}FAIL${C_RESET}"
    fi
  else
    [ $QUIET -eq 1 ] && echo -e "${C_GREEN}OK${C_RESET}"
  fi
}

# Runs all tests
run_tests() {
  echo "Running $TEST_COUNT tests using $XCC_PATH (QUIET=$QUIET STOP_ON_FAILURE=$STOP_ON_FAILURE)"
  cd $DIR

  for i in $(seq $TEST_COUNT); do
    run_test $i
  done

  exit 0
}

run_tests
