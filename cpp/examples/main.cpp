#include <iostream>
#include <string>
#include <vector>
#include "magda_cpp/agents/track_agent.h"
#include "magda_cpp/agents/volume_agent.h"

void testAgent(const std::string& agent_name, 
               magda::BaseAgent& agent, 
               const std::vector<std::string>& operations) {
    std::cout << "\n" << agent_name << " Agent Test" << std::endl;
    std::cout << std::string(agent_name.length() + 12, '=') << std::endl;

    for (const auto& operation : operations) {
        std::cout << "\nTesting operation: " << operation << std::endl;
        
        if (agent.canHandle(operation)) {
            std::cout << "✓ " << agent_name << " agent can handle this operation" << std::endl;
            
            try {
                // Execute the operation
                auto response = agent.execute(operation);
                
                std::cout << "Result:" << std::endl;
                std::cout << "  DAW Command: " << response.daw_command << std::endl;
                
                // Show result details based on agent type
                if (agent_name == "Track") {
                    std::cout << "  Track ID: " << response.result["track_id"].get<std::string>() << std::endl;
                    std::cout << "  Track Name: " << response.result["track_name"].get<std::string>() << std::endl;
                    
                    if (response.result.contains("vst") && !response.result["vst"].is_null()) {
                        std::cout << "  VST: " << response.result["vst"].get<std::string>() << std::endl;
                    }
                } else if (agent_name == "Volume") {
                    std::cout << "  Track Name: " << response.result["track_name"].get<std::string>() << std::endl;
                    std::cout << "  Volume: " << response.result["volume"].get<float>() << std::endl;
                    
                    if (response.result.contains("pan") && !response.result["pan"].is_null()) {
                        std::cout << "  Pan: " << response.result["pan"].get<float>() << std::endl;
                    }
                    
                    if (response.result.contains("mute") && !response.result["mute"].is_null()) {
                        std::cout << "  Mute: " << (response.result["mute"].get<bool>() ? "true" : "false") << std::endl;
                    }
                }
                
            } catch (const std::exception& e) {
                std::cout << "✗ Error executing operation: " << e.what() << std::endl;
            }
        } else {
            std::cout << "✗ " << agent_name << " agent cannot handle this operation" << std::endl;
        }
    }

    // Show agent capabilities
    std::cout << "\n" << agent_name << " agent capabilities:" << std::endl;
    auto capabilities = agent.getCapabilities();
    for (const auto& capability : capabilities) {
        std::cout << "  - " << capability << std::endl;
    }
}

int main() {
    std::cout << "MAGDA C++ Example - Multi-Agent System" << std::endl;
    std::cout << "=======================================" << std::endl;

    try {
        // Create agents
        magda::TrackAgent track_agent;
        magda::VolumeAgent volume_agent;

        // Test track agent
        std::vector<std::string> track_operations = {
            "create a bass track with Serum",
            "add a drum track",
            "create track for lead synth",
            "set volume to 50%"  // This should not be handled by track agent
        };
        testAgent("Track", track_agent, track_operations);

        // Test volume agent
        std::vector<std::string> volume_operations = {
            "set volume to 75%",
            "pan the track to the left",
            "mute the bass track",
            "create a track"  // This should not be handled by volume agent
        };
        testAgent("Volume", volume_agent, volume_operations);

        // List all created tracks
        std::cout << "\nCreated tracks:" << std::endl;
        auto tracks = track_agent.listTracks();
        for (const auto& track : tracks) {
            std::cout << "  - " << track["track_name"].get<std::string>() 
                      << " (ID: " << track["track_id"].get<std::string>() << ")" << std::endl;
        }

        // List all volume settings
        std::cout << "\nVolume settings:" << std::endl;
        auto volume_settings = volume_agent.listVolumeSettings();
        for (const auto& setting : volume_settings) {
            std::cout << "  - " << setting["track_name"].get<std::string>() 
                      << " (Volume: " << setting["volume"].get<float>() << ")" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Make sure OPENAI_API_KEY environment variable is set." << std::endl;
        return 1;
    }

    std::cout << "\nExample completed successfully!" << std::endl;
    return 0;
} 