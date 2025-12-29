#pragma once

#include "../WDL/WDL/wdlstring.h"
#include "reaper_plugin.h"
#include <cmath>
#include <vector>

// Forward declarations (use class to match REAPER SDK)
class MediaTrack;
class MediaItem;
class MediaItem_Take;

// DSP Analysis configuration
struct DSPAnalysisConfig {
  int fftSize = 4096;           // FFT window size
  int hopSize = 2048;           // FFT hop size (overlap)
  float analysisLength = 10.0f; // Max seconds to analyze (0 = full item)
  bool analyzeFullItem = true;  // Analyze entire item vs. selection

  // What to analyze
  bool analyzeFrequency = true;
  bool analyzeResonances = true;
  bool analyzeLoudness = true;
  bool analyzeDynamics = true;
  bool analyzeStereo = true;
  bool analyzeTransients = true;
  bool analyzeSpectralFeatures = true;
};

// Frequency band energy levels (dB)
struct FrequencyBands {
  float sub = -96.0f;        // 20-60 Hz
  float bass = -96.0f;       // 60-250 Hz
  float lowMid = -96.0f;     // 250-500 Hz
  float mid = -96.0f;        // 500-2000 Hz
  float highMid = -96.0f;    // 2000-4000 Hz
  float presence = -96.0f;   // 4000-6000 Hz
  float brilliance = -96.0f; // 6000-20000 Hz
};

// Detected peak in frequency spectrum
struct FrequencyPeak {
  float frequency; // Hz
  float magnitude; // dB
  float q;         // Q factor (bandwidth)
};

// Detected resonance
struct Resonance {
  float frequency;      // Hz
  float magnitude;      // dB above surrounding
  float q;              // Q factor
  const char *severity; // "low", "medium", "high"
  const char *type;     // "ringing", "room_mode", "harmonic", "equipment"
};

// Spectral features
struct SpectralFeatures {
  float spectralCentroid; // "Center of mass" frequency (Hz)
  float spectralRolloff;  // Frequency below which 85% of energy exists
  float spectralSlope;    // Overall tilt (dB/octave)
  float spectralFlatness; // 0=tonal, 1=noise-like
  float spectralContrast; // Difference between peaks and valleys
  float lowFreqEnergy;    // % of energy below 250Hz
  float midFreqEnergy;    // % of energy 250Hz-4kHz
  float highFreqEnergy;   // % of energy above 4kHz
};

// Loudness analysis
struct LoudnessAnalysis {
  float rms;           // dB RMS
  float lufs;          // Integrated LUFS (approximation)
  float lufsShortTerm; // Short-term LUFS
  float peak;          // dB peak
  float truePeak;      // dB True Peak (interpolated)
};

// Dynamics analysis
struct DynamicsAnalysis {
  float dynamicRange;     // dB
  float crestFactor;      // Peak/RMS ratio (dB)
  float compressionRatio; // Estimated compression
};

// Stereo analysis
struct StereoAnalysis {
  float width;       // 0=mono, 1=full stereo
  float correlation; // L/R correlation (-1 to 1)
  float balance;     // -1=L, 0=center, 1=R
};

// Transient analysis
struct TransientAnalysis {
  float attackTime;      // seconds
  float transientEnergy; // 0-1
};

// Raw audio data (for passing between threads)
struct RawAudioData {
  bool valid = false;
  std::vector<float> samples;
  int sampleRate = 0;
  int channels = 0;
};

// Complete analysis result
struct DSPAnalysisResult {
  bool success = false;
  WDL_FastString errorMessage;

  // Sample info
  int sampleRate = 0;
  int channels = 0;
  double lengthSeconds = 0.0;

  // Frequency analysis
  std::vector<float> fftFrequencies; // Frequency bins (Hz)
  std::vector<float> fftMagnitudes;  // Magnitude per bin (dB)
  std::vector<float> eqProfileFreqs; // EQ profile frequencies (1/3 octave)
  std::vector<float> eqProfileMags;  // EQ profile magnitudes (dB)
  FrequencyBands bands;
  std::vector<FrequencyPeak> peaks;

  // Resonance analysis
  std::vector<Resonance> resonances;

  // Other analysis
  SpectralFeatures spectralFeatures;
  LoudnessAnalysis loudness;
  DynamicsAnalysis dynamics;
  StereoAnalysis stereo;
  TransientAnalysis transients;
};

// DSP Analyzer class
class MagdaDSPAnalyzer {
public:
  // Analyze a specific track (pre-FX or post-FX audio)
  static DSPAnalysisResult AnalyzeTrack(int trackIndex,
                                        const DSPAnalysisConfig &config);

  // Analyze a specific media item
  static DSPAnalysisResult AnalyzeItem(MediaItem *item,
                                       const DSPAnalysisConfig &config);

  // Analyze the master track
  static DSPAnalysisResult AnalyzeMaster(const DSPAnalysisConfig &config);

  // Read raw audio samples from track (MUST be called from main thread)
  static RawAudioData ReadTrackSamples(int trackIndex,
                                       const DSPAnalysisConfig &config);

  // Analyze pre-loaded samples (can be called from background thread)
  static DSPAnalysisResult AnalyzeSamples(const RawAudioData &audioData,
                                          const DSPAnalysisConfig &config);

  // Convert analysis result to JSON string
  static void ToJSON(const DSPAnalysisResult &result, WDL_FastString &json);

  // Get FX info for a track
  static void GetTrackFXInfo(int trackIndex, WDL_FastString &json);

private:
  // Audio buffer helpers
  static bool GetAudioSamples(MediaItem_Take *take, std::vector<float> &samples,
                              int &sampleRate, int &channels,
                              const DSPAnalysisConfig &config);

  // FFT analysis
  static void PerformFFT(const std::vector<float> &samples, int sampleRate,
                         int fftSize, std::vector<float> &frequencies,
                         std::vector<float> &magnitudes);

  // Simple DFT for when REAPER FFT isn't available
  static void SimpleDFT(const float *input, int N, float *realOut,
                        float *imagOut);

  // Calculate frequency bands from FFT
  static void CalculateFrequencyBands(const std::vector<float> &frequencies,
                                      const std::vector<float> &magnitudes,
                                      FrequencyBands &bands);

  // Calculate 1/3 octave EQ profile
  static void CalculateEQProfile(const std::vector<float> &frequencies,
                                 const std::vector<float> &magnitudes,
                                 std::vector<float> &eqFreqs,
                                 std::vector<float> &eqMags);

  // Detect peaks in spectrum
  static void DetectPeaks(const std::vector<float> &frequencies,
                          const std::vector<float> &magnitudes,
                          std::vector<FrequencyPeak> &peaks,
                          float thresholdDb = -60.0f);

  // Detect resonances (problematic peaks)
  static void DetectResonances(const std::vector<FrequencyPeak> &peaks,
                               const std::vector<float> &eqMags,
                               std::vector<Resonance> &resonances);

  // Calculate spectral features
  static SpectralFeatures
  CalculateSpectralFeatures(const std::vector<float> &frequencies,
                            const std::vector<float> &magnitudes);

  // Calculate loudness metrics
  static LoudnessAnalysis CalculateLoudness(const std::vector<float> &samples,
                                            int sampleRate, int channels);

  // Calculate dynamics metrics
  static DynamicsAnalysis CalculateDynamics(const std::vector<float> &samples,
                                            int channels);

  // Calculate stereo metrics
  static StereoAnalysis CalculateStereo(const std::vector<float> &samples,
                                        int channels);

  // Calculate transient metrics
  static TransientAnalysis
  CalculateTransients(const std::vector<float> &samples, int sampleRate,
                      int channels);

  // Utility: dB conversion
  static float LinearToDb(float linear) {
    if (linear <= 0.0f)
      return -96.0f;
    return 20.0f * log10f(linear);
  }

  static float DbToLinear(float db) { return powf(10.0f, db / 20.0f); }

  // Utility: Hann window
  static float HannWindow(int n, int N) {
    return 0.5f * (1.0f - cosf(2.0f * M_PI * n / (N - 1)));
  }

  // JSON helpers
  static void AppendFloatArray(WDL_FastString &json, const char *name,
                               const std::vector<float> &arr,
                               bool first = false);
};
