# LLMCPP Library Model Update

## Available OpenAI Models (API Verified)

### O3 Series
- o3-mini
- o3-mini-2025-01-31

### O1 Series
- o1
- o1-2024-12-17
- o1-mini
- o1-mini-2024-09-12
- o1-preview
- o1-preview-2024-09-12
- o1-pro
- o1-pro-2025-03-19

### O4 Series
- o4-mini
- o4-mini-2025-04-16
- o4-mini-deep-research
- o4-mini-deep-research-2025-06-26

### GPT-4.1 Series
- gpt-4.1
- gpt-4.1-2025-04-14
- gpt-4.1-mini
- gpt-4.1-mini-2025-04-14
- gpt-4.1-nano
- gpt-4.1-nano-2025-04-14

### GPT-4o Series
- gpt-4o
- gpt-4o-2024-05-13
- gpt-4o-2024-08-06
- gpt-4o-2024-11-20
- gpt-4o-mini
- gpt-4o-mini-2024-07-18

### GPT-4.5 Series
- gpt-4.5-preview
- gpt-4.5-preview-2025-02-27

### GPT-3.5 Series
- gpt-3.5-turbo
- gpt-3.5-turbo-0125
- gpt-3.5-turbo-1106
- gpt-3.5-turbo-16k
- gpt-3.5-turbo-instruct
- gpt-3.5-turbo-instruct-0914

## Required Enum Updates

Add to `enum class Model` in `OpenAITypes.h`:

```cpp
// O3 series (Latest - 2025)
O3,       // o3 - Latest reasoning model
O3_Mini,  // o3-mini - Cost-effective reasoning model

// O1 series (2024-2025)
O1,       // o1 - Advanced reasoning model
O1_Mini,  // o1-mini - Cost-effective O1 model
O1_Preview, // o1-preview - Preview version
O1_Pro,   // o1-pro - Professional version

// O4 series (Latest - 2025)
O4_Mini,  // o4-mini - Latest mini model
O4_Mini_Deep_Research, // o4-mini-deep-research - Research focused
```

## Update Required Functions

1. `toString()` - Add cases for new models
2. `modelFromString()` - Add string mappings
3. `supportsStructuredOutputs()` - Add O1/O3/O4 models as supported
4. `getRecommendedModel()` - Add recommendations for reasoning tasks
