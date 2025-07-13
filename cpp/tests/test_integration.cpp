#include <catch2/catch_all.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <memory>

#include "magda_cpp/pipeline.hpp"
#include "magda_cpp/models.h"

/**
 * @brief Integration tests for MAGDA C++ implementation with real API calls
 */
TEST_CASE("Basic track operations integration", "[integration][track]") {
    // Get API key from environment
    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (!api_key) {
        SKIP("OPENAI_API_KEY not set - skipping integration tests");
    }

    magda::MAGDAPipeline pipeline(api_key);

    std::vector<std::string> track_prompts = {
        "Create a new track called 'Electric Guitar'",
        "Add a new audio track named 'Acoustic Bass'",
        "Delete the track 'Old Drums'",
        "Crea una nueva pista llamada 'Piano'",
        "Ajoute une piste MIDI nommée 'Synth'"
    };

    for (const auto& prompt : track_prompts) {
        std::cout << "\nTesting track operation: " << prompt << std::endl;

        auto result = pipeline.processPrompt(prompt);
        REQUIRE(result.has_value());
        REQUIRE(result->operations.size() > 0);

        // Check for track operations
        bool has_track_op = false;
        for (const auto& op : result->operations) {
            if (op.operation_type == magda::OperationType::CREATE_TRACK) {
                has_track_op = true;
                break;
            }
        }
        REQUIRE(has_track_op);

        std::cout << "✓ Track operation passed" << std::endl;
    }
}

TEST_CASE("Volume operations integration", "[integration][volume]") {
    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (!api_key) {
        SKIP("OPENAI_API_KEY not set - skipping integration tests");
    }

    magda::MAGDAPipeline pipeline(api_key);

    std::vector<std::string> volume_prompts = {
        "Set the volume of track 'Guitar' to -6dB",
        "Increase the volume of 'Bass' by 3dB",
        "Mute the 'Drums' track",
        "Ajusta el volumen de 'Piano' a -3dB",
        "Réduis le volume de 'Synth' de 2dB"
    };

    for (const auto& prompt : volume_prompts) {
        std::cout << "\nTesting volume operation: " << prompt << std::endl;

        auto result = pipeline.processPrompt(prompt);
        REQUIRE(result.has_value());
        REQUIRE(result->operations.size() > 0);

        // Check for volume operations
        bool has_volume_op = false;
        for (const auto& op : result->operations) {
            if (op.operation_type == magda::OperationType::SET_VOLUME) {
                has_volume_op = true;
                break;
            }
        }
        REQUIRE(has_volume_op);

        std::cout << "✓ Volume operation passed" << std::endl;
    }
}

TEST_CASE("Effect operations integration", "[integration][effect]") {
    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (!api_key) {
        SKIP("OPENAI_API_KEY not set - skipping integration tests");
    }

    magda::MAGDAPipeline pipeline(api_key);

    std::vector<std::string> effect_prompts = {
        "Add reverb to the 'Guitar' track",
        "Remove the delay from 'Bass'",
        "Set the reverb wet level to 50% on 'Guitar'",
        "Füge einen Chorus-Effekt zur 'Piano'-Spur hinzu",
        "Ajoute un filtre passe-bas à la piste 'Synth'"
    };

    for (const auto& prompt : effect_prompts) {
        std::cout << "\nTesting effect operation: " << prompt << std::endl;

        auto result = pipeline.processPrompt(prompt);
        REQUIRE(result.has_value());
        REQUIRE(result->operations.size() > 0);

        // Check for effect operations
        bool has_effect_op = false;
        for (const auto& op : result->operations) {
            if (op.operation_type == magda::OperationType::ADD_EFFECT) {
                has_effect_op = true;
                break;
            }
        }
        REQUIRE(has_effect_op);

        std::cout << "✓ Effect operation passed" << std::endl;
    }
}

TEST_CASE("Clip operations integration", "[integration][clip]") {
    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (!api_key) {
        SKIP("OPENAI_API_KEY not set - skipping integration tests");
    }

    magda::MAGDAPipeline pipeline(api_key);

    std::vector<std::string> clip_prompts = {
        "Create a 4-bar clip on track 'Guitar'",
        "Delete the clip at bar 8 on 'Bass'",
        "Move the clip from bar 4 to bar 12 on 'Drums'",
        "Crea un clip de 8 compases en la pista 'Piano'",
        "Crée un clip de 2 mesures sur la pista 'Synth'"
    };

    for (const auto& prompt : clip_prompts) {
        std::cout << "\nTesting clip operation: " << prompt << std::endl;

        auto result = pipeline.processPrompt(prompt);
        REQUIRE(result.has_value());
        REQUIRE(result->operations.size() > 0);

        // Check for clip operations
        bool has_clip_op = false;
        for (const auto& op : result->operations) {
            if (op.operation_type == magda::OperationType::CREATE_CLIP) {
                has_clip_op = true;
                break;
            }
        }
        REQUIRE(has_clip_op);

        std::cout << "✓ Clip operation passed" << std::endl;
    }
}

TEST_CASE("MIDI operations integration", "[integration][midi]") {
    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (!api_key) {
        SKIP("OPENAI_API_KEY not set - skipping integration tests");
    }

    magda::MAGDAPipeline pipeline(api_key);

    std::vector<std::string> midi_prompts = {
        "Add a C major chord at bar 1 on 'Piano'",
        "Delete the note at beat 2.5 on 'Bass'",
        "Change the velocity of the note at bar 4 to 80",
        "Añade una nota Do en el compás 2 en 'Piano'",
        "Ajoute un accord de La mineur à la mesure 4 sur 'Synth'"
    };

    for (const auto& prompt : midi_prompts) {
        std::cout << "\nTesting MIDI operation: " << prompt << std::endl;

        auto result = pipeline.processPrompt(prompt);
        REQUIRE(result.has_value());
        REQUIRE(result->operations.size() > 0);

        // Check for MIDI operations
        bool has_midi_op = false;
        for (const auto& op : result->operations) {
            if (op.operation_type == magda::OperationType::CREATE_MIDI) {
                has_midi_op = true;
                break;
            }
        }
        REQUIRE(has_midi_op);

        std::cout << "✓ MIDI operation passed" << std::endl;
    }
}

TEST_CASE("Complex multi-operation integration", "[integration][complex]") {
    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (!api_key) {
        SKIP("OPENAI_API_KEY not set - skipping integration tests");
    }

    magda::MAGDAPipeline pipeline(api_key);

    std::vector<std::string> complex_prompts = {
        "Create a new track called 'Lead Guitar', set its volume to -3dB, and add reverb with 40% wet level",
        "Add a new MIDI track 'Piano', create a 4-bar clip, and add a C major chord",
        "Crea una pista llamada 'Bajo', ajusta el volumen a -6dB, y añade un efecto de compresión",
        "Ajoute une piste audio 'Batterie', crée un clip de 8 mesures, et ajoute un filtre passe-haut"
    };

    for (const auto& prompt : complex_prompts) {
        std::cout << "\nTesting complex operation: " << prompt << std::endl;

        auto result = pipeline.processPrompt(prompt);
        REQUIRE(result.has_value());
        REQUIRE(result->operations.size() > 0);
        REQUIRE(result->daw_commands.size() > 0);

        std::cout << "✓ Complex operation passed with " << result->operations.size()
                  << " operations and " << result->daw_commands.size() << " DAW commands" << std::endl;
    }
}

TEST_CASE("Multilingual support integration", "[integration][multilingual]") {
    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (!api_key) {
        SKIP("OPENAI_API_KEY not set - skipping integration tests");
    }

    magda::MAGDAPipeline pipeline(api_key);

    std::vector<std::pair<std::string, magda::OperationType>> multilingual_prompts = {
        // Spanish
        {"Crea una nueva pista llamada 'Guitarra Española'", magda::OperationType::CREATE_TRACK},
        {"Ajusta el volumen de 'Piano' a -3dB", magda::OperationType::SET_VOLUME},
        {"Añade reverb a la pista 'Bajo'", magda::OperationType::ADD_EFFECT},
        // French
        {"Ajoute une nouvelle piste audio nommée 'Batterie'", magda::OperationType::CREATE_TRACK},
        {"Réduis le volume de 'Synth' de 2dB", magda::OperationType::SET_VOLUME},
        {"Ajoute un filtre passe-bas à la piste 'Guitare'", magda::OperationType::ADD_EFFECT},
        // German
        {"Füge eine neue MIDI-Spur namens 'Klavier' hinzu", magda::OperationType::CREATE_TRACK},
        {"Erhöhe die Lautstärke der 'Bass'-Spur um 3dB", magda::OperationType::SET_VOLUME},
        {"Füge einen Chorus-Effekt zur 'Gitarre'-Spur hinzu", magda::OperationType::ADD_EFFECT},
        // Italian
        {"Crea una nuova traccia chiamata 'Chitarra Italiana'", magda::OperationType::CREATE_TRACK},
        {"Imposta il volume della traccia 'Piano' a -6dB", magda::OperationType::SET_VOLUME},
        {"Aggiungi riverbero alla traccia 'Basso'", magda::OperationType::ADD_EFFECT},
        // Portuguese
        {"Crie uma nova faixa chamada 'Guitarra Portuguesa'", magda::OperationType::CREATE_TRACK},
        {"Ajuste o volume da faixa 'Piano' para -3dB", magda::OperationType::SET_VOLUME},
        {"Adicione reverb à faixa 'Baixo'", magda::OperationType::ADD_EFFECT}
    };

    for (const auto& [prompt, expected_type] : multilingual_prompts) {
        std::cout << "\nTesting multilingual: " << prompt << std::endl;

        auto result = pipeline.processPrompt(prompt);
        REQUIRE(result.has_value());
        REQUIRE(result->operations.size() > 0);

        // Check for expected operation type
        bool has_expected_op = false;
        for (const auto& op : result->operations) {
            if (op.operation_type == expected_type) {
                has_expected_op = true;
                break;
            }
        }
        REQUIRE(has_expected_op);

        std::cout << "✓ Multilingual operation passed" << std::endl;
    }
}

TEST_CASE("Error handling integration", "[integration][error]") {
    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (!api_key) {
        SKIP("OPENAI_API_KEY not set - skipping integration tests");
    }

    magda::MAGDAPipeline pipeline(api_key);

    std::vector<std::string> invalid_prompts = {
        "This is not a valid DAW command",
        "Random text that doesn't make sense",
        "123456789",
        ""
    };

    for (const auto& prompt : invalid_prompts) {
        std::cout << "\nTesting error handling: " << prompt << std::endl;

        auto result = pipeline.processPrompt(prompt);

        // Should either return no operations or handle gracefully
        if (result.has_value()) {
            if (result->operations.empty()) {
                std::cout << "✓ No operations identified (expected)" << std::endl;
            } else {
                std::cout << "Unexpected result with operations: " << result->operations.size() << std::endl;
            }
        } else {
            std::cout << "✓ Error handled gracefully" << std::endl;
        }
    }
}
