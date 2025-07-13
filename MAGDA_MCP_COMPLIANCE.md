# Making MAGDA MCP-Compliant (Model Context Protocol)

## Overview

MAGDA is currently a domain-specific multi-agent system for DAW operations. To make it MCP-compliant (in the sense of a Model Context Protocol server), MAGDA should support richer context management, dynamic agent/model routing, inter-agent communication, and a more general, extensible API.

This document outlines the architectural changes, features to add, and concrete next steps to evolve MAGDA into an MCP-compliant system.

---

## Key MCP Features to Implement

### 1. Rich Context Management
- Maintain per-session/user context (not just prompt context)
- Store/retrieve conversation history, user preferences, and intermediate results
- Support context injection and retrieval via API

### 2. Generalized Model/Agent Routing
- Allow dynamic selection of models/tools/agents based on task, user, or context
- Support tool/function calls (like OpenAI tool-calling or LangChain routing)
- Enable agents to call other agents, not just pipeline-style

### 3. Unified, Extensible API
- Expose a general-purpose API (e.g., `/v1/mcp/invoke`) that accepts a task, context, and tool/model hints
- Return structured results, tool outputs, and updated context

### 4. Inter-Agent Communication
- Allow agents to invoke each other, pass messages, or share state
- Support agent delegation (if one agent can’t handle a request, it can pass it to another)

### 5. Plugin/Tooling Support
- Allow new tools, plugins, or models to be registered at runtime or via config
- Support arbitrary tool schemas, not just DAW operations

### 6. Session and State Management
- Track sessions, user state, and long-running workflows
- Allow clients to resume, branch, or inspect sessions

---

## Concrete Steps for MAGDA

### 1. Refactor Pipeline for Generalized Routing
- Replace the fixed pipeline (operation identifier → specialized agent) with a router that can:
  - Inspect the task/context
  - Dynamically select which agent/model/tool to invoke
  - Support fallback, delegation, or chaining

### 2. Implement a Context Store
- Use a database, in-memory store, or file-based system to persist:
  - User/session context
  - Conversation history
  - Intermediate results

### 3. Expand the API
- Add endpoints for:
  - Creating/resuming sessions
  - Invoking arbitrary tools/agents with context
  - Querying/updating context

### 4. Support Tool/Agent Registration
- Allow new agents/tools to be registered via config or API
- Each tool/agent should declare:
  - Its capabilities/schema
  - How it should be invoked
  - What context it needs/produces

### 5. Enable Inter-Agent Calls
- Allow agents to call each other (e.g., via a shared router or message bus)
- Support tool call patterns (agent A can ask agent B for a subtask)

### 6. Adopt MCP-like API Patterns
- Use OpenAI’s MCP API as inspiration:
  - Accept a `messages` array (history), `tools` array (available tools), and `context` object
  - Return a `choices` array with tool calls, results, and updated context

---

## Example: MCP-like API for MAGDA

**Request:**
```json
POST /v1/mcp/invoke
{
  "session_id": "abc123",
  "messages": [
    {"role": "user", "content": "Create a bass track and add reverb"}
  ],
  "tools": [
    {"name": "track_agent", "schema": {...}},
    {"name": "effect_agent", "schema": {...}}
  ],
  "context": {
    "user_preferences": {...},
    "daw_state": {...}
  }
}
```

**Response:**
```json
{
  "choices": [
    {
      "tool_call": {"name": "track_agent", "arguments": {...}},
      "result": {...}
    },
    {
      "tool_call": {"name": "effect_agent", "arguments": {...}},
      "result": {...}
    }
  ],
  "context": {...}
}
```

---

## Summary Table

| MCP Feature                | MAGDA Now         | MCP-Compliant MAGDA (Goal)         |
|----------------------------|-------------------|-------------------------------------|
| Multi-agent orchestration  | Yes (fixed)       | Yes (dynamic, extensible)           |
| Context management         | Basic             | Full session/context/history        |
| Model/tool routing         | Fixed pipeline    | Dynamic, tool-calling, delegation   |
| Inter-agent comms          | Pipeline only     | Arbitrary, tool-calling             |
| API surface                | DAW-focused       | General, session/context-aware      |
| Plugin/tool registration   | Static            | Dynamic/Configurable                |

---

## Next Steps

1. Decide which MCP features are most important for your use case.
2. Refactor the pipeline to use a router and context store.
3. Expand the API to support sessions, context, and tool registration.
4. Iterate and test with real-world multi-agent workflows.

---

**This document will guide the implementation of MCP compliance in MAGDA.**
