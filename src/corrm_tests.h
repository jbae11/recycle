#ifndef CORRM_TESTS_H_
#define CORRM_TESTS_H_

#include <gtest/gtest.h>

#include "corrm.h"

#include "context.h"
#include "facility_tests.h"
#include "agent_tests.h"

namespace corrm {
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class CorrmTest : public ::testing::Test {
 protected:
  cyclus::TestContext tc_;
  Corrm* src_facility_;
};
} // namespace corrm
#endif // CORRM_TESTS_H_

