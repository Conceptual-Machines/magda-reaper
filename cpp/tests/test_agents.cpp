#include <gtest/gtest.h>
#include "agent.hpp"

/**
 * @brief Dummy agent for testing.
 */
class DummyAgent : public Agent {
public:
    std::string execute(const std::string& prompt, const std::map<std::string, std::string>& context) override {
        return "dummy_command";
    }
    std::string name() const override {
        return "dummy";
    }
};

TEST(AgentTest, CannotInstantiateAbstract) {
    // Agent is abstract, cannot instantiate directly
    // Uncommenting the next line should fail to compile:
    // Agent a;
    SUCCEED();
}

TEST(AgentTest, DummyAgentWorks) {
    DummyAgent agent;
    std::map<std::string, std::string> ctx;
    EXPECT_EQ(agent.execute("test", ctx), "dummy_command");
    EXPECT_EQ(agent.name(), "dummy");
} 