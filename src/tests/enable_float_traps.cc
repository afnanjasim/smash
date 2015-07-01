/*
 *
 *    Copyright (c) 2015
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "unittest.h"
#include "../include/fpenvironment.h"

#include <stdexcept>
#include <csignal>
#include <csetjmp>

float blackhole = 0.f;
float divisor = 0.f;

std::jmp_buf jump_buffer;

struct fpe_exception : public std::exception {
  const char *what() const noexcept override { return "Floating Point Exception"; }
};

static void handle_fpe(int s) {
  if (s == SIGFPE) {
    std::longjmp(jump_buffer, 1);
  }
}

static void do_division(float x) {
  if (setjmp(jump_buffer) == 0) {
    // normally goes here
    blackhole = x / divisor;
  } else {
    // longjmp goes here
    throw fpe_exception();
  }
}

// this test cannot be merged with the without_float_traps.cc test because a
// single process can only handle a single SIGFPE. Actually program resumption
// after the SIGFPE handler is undefined behavior. But we're still in compiler
// extension land, so the test works.
TEST(enable_float_traps) {
  std::signal(SIGFPE, &handle_fpe);

  VERIFY(!std::fetestexcept(FE_DIVBYZERO));  // flag not set yet
  Smash::enable_float_traps(FE_DIVBYZERO);   // now it must trap
  VERIFY(!std::fetestexcept(FE_DIVBYZERO));  // flag not set yet

  bool got_exception = false;
  try {
    do_division(3.f);  // traps must be enabled now
    FAIL() << "may never be reached";
  } catch (fpe_exception &) {  // the signal handler produces an exception via
                               // longjmp
    got_exception = true;
  }
  VERIFY(!std::fetestexcept(
             FE_DIVBYZERO));  // flag must not be set because it trapped
  VERIFY(got_exception);
}