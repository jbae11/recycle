#include <gtest/gtest.h>

#include "corrm_tests.h"

namespace corrm {


}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
cyclus::Agent* CorrmConstructor(cyclus::Context* ctx) {
  return new corrm::Corrm(ctx);
}

// required to get functionality in cyclus agent unit tests library
#ifndef CYCLUS_AGENT_TESTS_CONNECTED
int ConnectAgentTests();
static int cyclus_agent_tests_connected = ConnectAgentTests();
#define CYCLUS_AGENT_TESTS_CONNECTED cyclus_agent_tests_connected
#endif // CYCLUS_AGENT_TESTS_CONNECTED

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
INSTANTIATE_TEST_CASE_P(CorrmFac, FacilityTests,
                        ::testing::Values(&CorrmConstructor));

INSTANTIATE_TEST_CASE_P(CorrmFac, AgentTests,
                        ::testing::Values(&CorrmConstructor));