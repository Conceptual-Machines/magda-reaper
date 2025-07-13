#include <iostream>
#include <string>
#include "openai/OpenAI.hpp"

/**
 * @brief Minimal reproducible example for o3-mini temperature parameter issue
 *
 * This demonstrates the bug in the llmcpp library where it incorrectly
 * sends the temperature parameter to the OpenAI Responses API when using
 * the o3-mini model, which doesn't support this parameter.
 *
 * Expected behavior: temperature should be omitted entirely for o3-mini
 * Current behavior: temperature is included, causing API error
 */

int main() {
    try {
        std::cout << "🔧 Testing llmcpp library with o3-mini model..." << std::endl;
        std::cout << "Expected: Should work without temperature parameter" << std::endl;
        std::cout << "Current bug: Sends temperature parameter, causing API error" << std::endl;
        std::cout << std::endl;

        // Initialize OpenAI client
        openai::OpenAI client;

        // Create a responses request with o3-mini model
        openai::ResponsesRequest request;
        request.model = "o3-mini";
        request.instructions = "You are a helpful assistant. Respond briefly.";
        request.input = "What is 2+2?";

        // This is the problematic line - temperature should not be sent for o3-mini
        // The llmcpp library should check if the model supports temperature
        request.temperature = 0.5f;

        std::cout << "📤 Sending request to OpenAI API..." << std::endl;
        std::cout << "Model: " << *request.model << std::endl;
        std::cout << "Temperature: " << *request.temperature << std::endl;
        std::cout << std::endl;

        // This will fail because the llmcpp library includes temperature in the request
        // even though o3-mini doesn't support it
        auto response = client.responses.create(request);

        std::cout << "✅ Success! Response: " << response.output_text << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        std::cerr << std::endl;
        std::cerr << "🔍 This error occurs because:" << std::endl;
        std::cerr << "1. o3-mini is a reasoning model that doesn't support temperature" << std::endl;
        std::cerr << "2. The llmcpp library includes temperature in the JSON request" << std::endl;
        std::cerr << "3. OpenAI API rejects the request with 'Unsupported parameter'" << std::endl;
        std::cerr << std::endl;
        std::cerr << "🛠️  Fix needed in llmcpp library:" << std::endl;
        std::cerr << "   - Add supportsTemperature() method" << std::endl;
        std::cerr << "   - Only include temperature if model supports it" << std::endl;
        std::cerr << "   - For o3-mini, o3, o1-mini, o1, o4-mini, o4: omit temperature" << std::endl;
        return 1;
    }

    return 0;
}
