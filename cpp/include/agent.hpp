#pragma once
#include <string>
#include <memory>
#include <map>

/**
 * @brief Abstract base class for all MAGDA agents (C++).
 *
 * Agents are responsible for handling specific DAW operations.
 */
class Agent {
public:
    virtual ~Agent() = default;

    /**
     * @brief Execute the agent's operation.
     * @param prompt The natural language prompt or operation description.
     * @param context Contextual information (track IDs, etc).
     * @return A string representing the DAW command or result.
     */
    virtual std::string execute(const std::string& prompt, const std::map<std::string, std::string>& context) = 0;

    /**
     * @brief Get the agent's name/type.
     * @return The agent type as a string.
     */
    virtual std::string name() const = 0;
}; 