#include <iostream>
#include <string>
#include <nlohmann/json.hpp>

/**
 * @brief Main entry point for the MAGDA C++ library
 * @return Exit code
 */
int main() {
    std::cout << "MAGDA C++ Library - Multi Agent Generative DAW API" << std::endl;
    std::cout << "Version: 0.1.0" << std::endl;
    std::cout << "Two-Stage Pipeline Implementation" << std::endl;
    std::cout << "=================================" << std::endl;

    try {
        // Get API key from environment or user input
        std::string api_key;
        const char* env_key = std::getenv("OPENAI_API_KEY");
        if (env_key) {
            api_key = env_key;
        } else {
            std::cout << "Enter your OpenAI API key (or set OPENAI_API_KEY environment variable): ";
            std::getline(std::cin, api_key);
        }

        if (api_key.empty()) {
            std::cout << "No API key provided. Exiting." << std::endl;
            return 1;
        }

        // Create pipeline
        MAGDAPipeline pipeline(api_key);

        // Demo prompts
        std::vector<std::string> demo_prompts = {
            "create a track for bass and add a compressor with 4:1 ratio",
            "add a reverb effect to the current track",
            "create a MIDI note C4 with velocity 100",
            "add a clip starting from bar 5"
        };

        for (const auto& prompt : demo_prompts) {
            std::cout << "\n\nProcessing prompt: \"" << prompt << "\"" << std::endl;
            std::cout << "================================================" << std::endl;

            auto result = pipeline.processPrompt(prompt);

            std::cout << "\nFinal Result:" << std::endl;
            std::cout << result.dump(2) << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
