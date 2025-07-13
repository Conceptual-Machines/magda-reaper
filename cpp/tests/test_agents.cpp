#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include <memory>
#include <string>

#include "magda_cpp/agents/base_agent.h"
#include "magda_cpp/agents/track_agent.h"
#include "magda_cpp/agents/volume_agent.h"
// #include "magda_cpp/agents/effect_agent.h"
// #include "magda_cpp/agents/clip_agent.h"
// #include "magda_cpp/agents/midi_agent.h"
#include "magda_cpp/agents/operation_identifier.h"
#include "magda_cpp/models.h"

/**
 * @brief Unit tests for MAGDA agents (Catch2)
 */
TEST_CASE("Base agent functionality", "[agents][base]") {
    // Test that base agent can be instantiated (abstract class test)
    // Note: BaseAgent is abstract, so we test through derived classes
    REQUIRE(true); // Placeholder - actual testing done through derived agents
}

TEST_CASE("Track agent unit tests", "[agents][track]") {
    magda::TrackAgent agent;

    SECTION("Can handle track operations") {
        std::string operation = "Create a new track called 'Guitar'";
        REQUIRE(agent.canHandle(operation));
    }

    SECTION("Cannot handle non-track operations") {
        std::string operation = "Set volume to -3dB";
        REQUIRE_FALSE(agent.canHandle(operation));
    }

    SECTION("Agent name is correct") {
        REQUIRE(agent.getName() == "track");
    }

    SECTION("Has track capabilities") {
        auto capabilities = agent.getCapabilities();
        REQUIRE(capabilities.size() > 0);
        REQUIRE(std::find(capabilities.begin(), capabilities.end(), "track") != capabilities.end());
    }
}

TEST_CASE("Volume agent unit tests", "[agents][volume]") {
    magda::VolumeAgent agent;

    SECTION("Can handle volume operations") {
        std::string operation = "Set the volume of track 'Guitar' to -6dB";
        REQUIRE(agent.canHandle(operation));
    }

    SECTION("Cannot handle non-volume operations") {
        std::string operation = "Create a new track";
        REQUIRE_FALSE(agent.canHandle(operation));
    }

    SECTION("Agent name is correct") {
        REQUIRE(agent.getName() == "volume");
    }

    SECTION("Has volume capabilities") {
        auto capabilities = agent.getCapabilities();
        REQUIRE(capabilities.size() > 0);
        REQUIRE(std::find(capabilities.begin(), capabilities.end(), "volume") != capabilities.end());
    }
}

/*
TEST_CASE("Effect agent unit tests", "[agents][effect]") {
    magda::EffectAgent agent;

    SECTION("Can handle effect operations") {
        std::string operation = "Add reverb to the 'Guitar' track";
        REQUIRE(agent.canHandle(operation));
    }

    SECTION("Cannot handle non-effect operations") {
        std::string operation = "Create a new track";
        REQUIRE_FALSE(agent.canHandle(operation));
    }

    SECTION("Agent name is correct") {
        REQUIRE(agent.getName() == "effect");
    }

    SECTION("Has effect capabilities") {
        auto capabilities = agent.getCapabilities();
        REQUIRE(capabilities.size() > 0);
        REQUIRE(std::find(capabilities.begin(), capabilities.end(), "effect") != capabilities.end());
    }
}
*/

/*
TEST_CASE("Clip agent unit tests", "[agents][clip]") {
    magda::ClipAgent agent;

    SECTION("Can handle clip operations") {
        std::string operation = "Create a 4-bar clip on track 'Guitar'";
        REQUIRE(agent.canHandle(operation));
    }

    SECTION("Cannot handle non-clip operations") {
        std::string operation = "Create a new track";
        REQUIRE_FALSE(agent.canHandle(operation));
    }

    SECTION("Agent name is correct") {
        REQUIRE(agent.getName() == "clip");
    }

    SECTION("Has clip capabilities") {
        auto capabilities = agent.getCapabilities();
        REQUIRE(capabilities.size() > 0);
        REQUIRE(std::find(capabilities.begin(), capabilities.end(), "clip") != capabilities.end());
    }
}
*/

/*
TEST_CASE("MIDI agent unit tests", "[agents][midi]") {
    magda::MidiAgent agent;

    SECTION("Can handle MIDI operations") {
        std::string operation = "Add a C major chord at bar 1 on 'Piano'";
        REQUIRE(agent.canHandle(operation));
    }

    SECTION("Cannot handle non-MIDI operations") {
        std::string operation = "Create a new track";
        REQUIRE_FALSE(agent.canHandle(operation));
    }

    SECTION("Agent name is correct") {
        REQUIRE(agent.getName() == "midi");
    }

    SECTION("Has MIDI capabilities") {
        auto capabilities = agent.getCapabilities();
        REQUIRE(capabilities.size() > 0);
        REQUIRE(std::find(capabilities.begin(), capabilities.end(), "midi") != capabilities.end());
    }
}
*/

TEST_CASE("Operation identifier unit tests", "[agents][operation_identifier]") {
    magda::OperationIdentifier identifier;

    SECTION("Agent name is correct") {
        REQUIRE(identifier.getName() == "operation_identifier");
    }

    SECTION("Can handle any operation type") {
        std::string track_op = "Create a new track";
        REQUIRE(identifier.canHandle(track_op));

        std::string volume_op = "Set volume to -3dB";
        REQUIRE(identifier.canHandle(volume_op));

        std::string effect_op = "Add reverb";
        REQUIRE(identifier.canHandle(effect_op));
    }

    SECTION("Has operation identification capabilities") {
        auto capabilities = identifier.getCapabilities();
        REQUIRE(capabilities.size() > 0);
    }
}
