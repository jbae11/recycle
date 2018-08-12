#ifndef AMSRA_TESTS_H_
#define AMSRA_TESTS_H_

#include <gtest/gtest.h>

#include "amsra.h"

#include "context.h"
#include "facility_tests.h"
#include "agent_tests.h"

namespace amsra{
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class AmsraTest : public ::testing::Test {
 protected:
  cyclus::TestContext tc_;
  Amsra* src_facility_;

};
} // namespace amsra
#endif // AMSRA_TESTS_H_

