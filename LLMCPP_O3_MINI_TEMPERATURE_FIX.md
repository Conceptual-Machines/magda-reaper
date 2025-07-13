# LLMCPP Library Fix: o3-mini Temperature Parameter Issue

## Problem Description

The llmcpp library is currently sending the `temperature` parameter to the OpenAI Responses API even when using the `o3-mini` model, which doesn't support this parameter. This causes API calls to fail with the error:

```
Unsupported parameter: 'temperature' is not supported with this model.
```

## Root Cause Analysis

### 1. Issue Location
The problem is in the `ResponsesRequest::toJson()` method in `include/openai/OpenAITypes.h` at line 642:

```cpp
if (temperature) j["temperature"] = *temperature;
```

### 2. Why This Happens
- The `o3-mini` model is a reasoning model that doesn't support the `temperature` parameter
- The current code includes the temperature parameter in the JSON request if it's set (even to 0.0)
- The Python SDK uses `NotGiven` to indicate truly optional parameters that should be omitted entirely

### 3. Python SDK Reference
Based on the Python SDK `client.responses.create()` method, here are the **truly optional parameters** that can be omitted entirely:

**Required Parameters:**
- `model` - The model to use (e.g., "o3-mini")
- `instructions` - System instructions
- `input` - The input prompt

**Optional Parameters (can be omitted):**
- `background` - Optional[bool] | NotGiven
- `include` - Optional[List[ResponseIncludable]] | NotGiven
- `max_output_tokens` - Optional[int] | NotGiven
- `max_tool_calls` - Optional[int] | NotGiven
- `metadata` - Optional[Metadata] | NotGiven
- `parallel_tool_calls` - Optional[bool] | NotGiven
- `previous_response_id` - Optional[str] | NotGiven
- `prompt` - Optional[ResponsePromptParam] | NotGiven
- `reasoning` - Optional[Reasoning] | NotGiven
- `service_tier` - Optional[Literal['auto', 'default', 'flex', 'scale', 'priority']] | NotGiven
- `store` - Optional[bool] | NotGiven
- `stream` - Optional[Literal[False]] | Literal[True] | NotGiven
- **`temperature`** - Optional[float] | NotGiven ← **This is the problematic one**
- `text` - ResponseTextConfigParam | NotGiven
- `tool_choice` - response_create_params.ToolChoice | NotGiven
- `tools` - Iterable[ToolParam] | NotGiven
- `top_logprobs` - Optional[int] | NotGiven
- `top_p` - Optional[float] | NotGiven
- `truncation` - Optional[Literal['auto', 'disabled']] | NotGiven
- `user` - str | NotGiven

## Solution

### Option 1: String-based Model Checking (Recommended)

Add a `supportsTemperature()` function that checks model names:

```cpp
// In OpenAITypes.h, add this function to the ResponsesRequest class:
bool supportsTemperature() const {
    if (!model) return false;

    std::string modelStr = *model;

    // Models that don't support temperature
    if (modelStr == "o3-mini" ||
        modelStr == "o3" ||
        modelStr == "o1-mini" ||
        modelStr == "o1" ||
        modelStr == "o4-mini" ||
        modelStr == "o4") {
        return false;
    }

    // Models that support temperature
    if (modelStr.find("gpt-") != std::string::npos ||
        modelStr.find("claude-") != std::string::npos) {
        return true;
    }

    // Default to true for unknown models (backward compatibility)
    return true;
}
```

Then modify the `toJson()` method:

```cpp
// In the toJson() method, replace:
if (temperature) j["temperature"] = *temperature;

// With:
if (temperature && supportsTemperature()) j["temperature"] = *temperature;
```

### Option 2: Enum-based Model Checking (More Robust)

If the library uses enums for models, create a more robust checking system:

```cpp
bool supportsTemperature() const {
    if (!model) return false;

    // Add this to the Model enum in OpenAITypes.h
    switch (*model) {
        case Model::O3_Mini:
        case Model::O3:
        case Model::O1_Mini:
        case Model::O1:
        case Model::O4_Mini:
        case Model::O4:
            return false;
        default:
            return true;
    }
}
```

## Implementation Steps

1. **Add the `supportsTemperature()` method** to the `ResponsesRequest` class in `include/openai/OpenAITypes.h`

2. **Modify the `toJson()` method** to check `supportsTemperature()` before including temperature

3. **Add unit tests** to verify the fix works correctly

4. **Update documentation** to reflect which models support which parameters

## Testing Strategy

### Unit Tests
```cpp
TEST_CASE("ResponsesRequest temperature support", "[openai][responses]") {
    // Test o3-mini doesn't include temperature
    ResponsesRequest req1;
    req1.model = "o3-mini";
    req1.temperature = 0.5f;
    auto json1 = req1.toJson();
    REQUIRE(json1.find("temperature") == json1.end());

    // Test gpt-4o-mini includes temperature
    ResponsesRequest req2;
    req2.model = "gpt-4o-mini";
    req2.temperature = 0.5f;
    auto json2 = req2.toJson();
    REQUIRE(json2.find("temperature") != json2.end());
    REQUIRE(json2["temperature"] == 0.5f);

    // Test backward compatibility - unknown model includes temperature
    ResponsesRequest req3;
    req3.model = "unknown-model";
    req3.temperature = 0.5f;
    auto json3 = req3.toJson();
    REQUIRE(json3.find("temperature") != json3.end());
}
```

### Integration Tests
- Test actual API calls with o3-mini model
- Verify no temperature parameter is sent
- Verify API calls succeed without errors

## Backward Compatibility

This fix is **backward compatible**:
- Models that support temperature will continue to work as before
- Models that don't support temperature will no longer send the parameter
- Unknown models default to including temperature (safe fallback)

## Files to Modify

1. `include/openai/OpenAITypes.h` - Add `supportsTemperature()` method and modify `toJson()`
2. `tests/` - Add unit tests for the new functionality
3. `docs/` - Update documentation about model parameter support

## Priority

**High Priority** - This is blocking the use of o3-mini models in the llmcpp library and causing API failures.
