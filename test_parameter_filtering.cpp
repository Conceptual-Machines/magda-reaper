#include <iostream>
#include <string>
#include "openai/OpenAITypes.h"

int main() {
    std::cout << "🔧 Testing parameter filtering for o3-mini model..." << std::endl;

    // Create a ResponsesRequest with o3-mini model
    OpenAI::ResponsesRequest request;
    request.model = "o3-mini";
    request.instructions = "You are a helpful assistant. Respond briefly.";
    request.input = OpenAI::ResponsesInput::fromText("What is 2+2?");

    // Set temperature (should be filtered out)
    request.temperature = 0.5;

    std::cout << "📤 Model: " << request.model << std::endl;
    std::cout << "📤 Temperature set: " << *request.temperature << std::endl;
    std::cout << "📤 Is temperature supported: " << (request.isParameterSupported("temperature") ? "YES" : "NO") << std::endl;

    // Convert to JSON
    auto json = request.toJson();

    std::cout << "📤 JSON output:" << std::endl;
    std::cout << json.dump(2) << std::endl;

    // Check if temperature is in the JSON
    if (json.contains("temperature")) {
        std::cout << "❌ ERROR: Temperature is still in JSON!" << std::endl;
        return 1;
    } else {
        std::cout << "✅ SUCCESS: Temperature correctly filtered out!" << std::endl;
    }

    return 0;
}
