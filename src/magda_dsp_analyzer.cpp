#include "magda_dsp_analyzer.h"
// Workaround for typo in reaper_plugin_functions.h line 6475 (Reaproject ->
// ReaProject) This is a typo in the REAPER SDK itself, not our code
typedef ReaProject Reaproject;
#include "reaper_plugin_functions.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern reaper_plugin_info_t *g_rec;

// Standard 1/3 octave center frequencies (ISO)
static const float kThirdOctaveFreqs[] = {
    20.0f,   25.0f,   31.5f,   40.0f,    50.0f,    63.0f,    80.0f,   100.0f,
    125.0f,  160.0f,  200.0f,  250.0f,   315.0f,   400.0f,   500.0f,  630.0f,
    800.0f,  1000.0f, 1250.0f, 1600.0f,  2000.0f,  2500.0f,  3150.0f, 4000.0f,
    5000.0f, 6300.0f, 8000.0f, 10000.0f, 12500.0f, 16000.0f, 20000.0f};
static const int kNumThirdOctaveBands =
    sizeof(kThirdOctaveFreqs) / sizeof(kThirdOctaveFreqs[0]);

// Helper: Show console message
static void LogMessage(const char *msg) {
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg(msg);
    }
  }
}

DSPAnalysisResult
MagdaDSPAnalyzer::AnalyzeTrack(int trackIndex,
                               const DSPAnalysisConfig &config) {
  DSPAnalysisResult result;

  if (!g_rec) {
    result.errorMessage.Set("REAPER plugin context not available");
    return result;
  }

  // Get track
  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  if (!GetTrack) {
    result.errorMessage.Set("GetTrack function not available");
    return result;
  }

  MediaTrack *track = GetTrack(nullptr, trackIndex);
  if (!track) {
    result.errorMessage.Set("Track not found");
    return result;
  }

  // Get the first media item on the track for analysis
  int (*CountTrackMediaItems)(MediaTrack *) =
      (int (*)(MediaTrack *))g_rec->GetFunc("CountTrackMediaItems");
  MediaItem *(*GetTrackMediaItem)(MediaTrack *, int) =
      (MediaItem * (*)(MediaTrack *, int)) g_rec->GetFunc("GetTrackMediaItem");

  if (!CountTrackMediaItems || !GetTrackMediaItem) {
    result.errorMessage.Set("Media item functions not available");
    return result;
  }

  int itemCount = CountTrackMediaItems(track);
  if (itemCount == 0) {
    result.errorMessage.Set("Track has no media items");
    return result;
  }

  // Analyze the first item (or could loop through all)
  MediaItem *item = GetTrackMediaItem(track, 0);
  return AnalyzeItem(item, config);
}

DSPAnalysisResult
MagdaDSPAnalyzer::AnalyzeItem(MediaItem *item,
                              const DSPAnalysisConfig &config) {
  DSPAnalysisResult result;

  if (!g_rec || !item) {
    result.errorMessage.Set("Invalid parameters");
    return result;
  }

  // Get active take
  MediaItem_Take *(*GetActiveTake)(MediaItem *) =
      (MediaItem_Take * (*)(MediaItem *)) g_rec->GetFunc("GetActiveTake");
  if (!GetActiveTake) {
    result.errorMessage.Set("GetActiveTake function not available");
    return result;
  }

  MediaItem_Take *take = GetActiveTake(item);
  if (!take) {
    result.errorMessage.Set("Item has no active take");
    return result;
  }

  // Log which take is active
  int (*CountTakes)(MediaItem *) = (int (*)(MediaItem *))g_rec->GetFunc("CountTakes");
  MediaItem_Take *(*GetTake)(MediaItem *, int) = (MediaItem_Take *(*)(MediaItem *, int))g_rec->GetFunc("GetTake");
  if (CountTakes && GetTake) {
    int numTakes = CountTakes(item);
    int activeTakeIdx = -1;
    for (int i = 0; i < numTakes; i++) {
      if (GetTake(item, i) == take) {
        activeTakeIdx = i;
        break;
      }
    }
    char logBuf[128];
    snprintf(logBuf, sizeof(logBuf), "MAGDA DSP: Active take index: %d of %d\n", activeTakeIdx, numTakes);
    LogMessage(logBuf);
  }

  // Get audio samples
  std::vector<float> samples;
  int sampleRate = 0;
  int channels = 0;

  if (!GetAudioSamples(take, samples, sampleRate, channels, config)) {
    result.errorMessage.Set("Failed to read audio samples");
    return result;
  }

  if (samples.empty()) {
    result.errorMessage.Set("No audio samples found");
    return result;
  }

  result.sampleRate = sampleRate;
  result.channels = channels;
  result.lengthSeconds = (double)samples.size() / (sampleRate * channels);

  char logBuf[256];
  snprintf(logBuf, sizeof(logBuf),
           "MAGDA DSP: Analyzing %zu samples, %d Hz, %d ch, %.2f sec\n",
           samples.size(), sampleRate, channels, result.lengthSeconds);
  LogMessage(logBuf);

  // Perform FFT analysis
  if (config.analyzeFrequency) {
    PerformFFT(samples, sampleRate, config.fftSize, result.fftFrequencies,
               result.fftMagnitudes);

    // Calculate frequency bands
    CalculateFrequencyBands(result.fftFrequencies, result.fftMagnitudes,
                            result.bands);

    // Calculate EQ profile (1/3 octave)
    CalculateEQProfile(result.fftFrequencies, result.fftMagnitudes,
                       result.eqProfileFreqs, result.eqProfileMags);

    // Detect peaks
    DetectPeaks(result.fftFrequencies, result.fftMagnitudes, result.peaks);
  }

  // Detect resonances
  if (config.analyzeResonances && !result.peaks.empty()) {
    DetectResonances(result.peaks, result.eqProfileMags, result.resonances);
  }

  // Calculate spectral features
  if (config.analyzeSpectralFeatures && !result.fftFrequencies.empty()) {
    result.spectralFeatures =
        CalculateSpectralFeatures(result.fftFrequencies, result.fftMagnitudes);
  }

  // Calculate loudness
  if (config.analyzeLoudness) {
    result.loudness = CalculateLoudness(samples, sampleRate, channels);
  }

  // Calculate dynamics
  if (config.analyzeDynamics) {
    result.dynamics = CalculateDynamics(samples, channels);
  }

  // Calculate stereo analysis
  if (config.analyzeStereo && channels >= 2) {
    result.stereo = CalculateStereo(samples, channels);
  }

  // Calculate transients
  if (config.analyzeTransients) {
    result.transients = CalculateTransients(samples, sampleRate, channels);
  }

  result.success = true;
  LogMessage("MAGDA DSP: Analysis complete\n");
  return result;
}

DSPAnalysisResult
MagdaDSPAnalyzer::AnalyzeMaster(const DSPAnalysisConfig &config) {
  DSPAnalysisResult result;
  result.errorMessage.Set("Master track analysis not yet implemented");
  // TODO: Implement master track analysis using render or audio accessor
  return result;
}

RawAudioData MagdaDSPAnalyzer::ReadTrackSamples(int trackIndex,
                                                const DSPAnalysisConfig &config) {
  RawAudioData data;

  if (!g_rec) {
    return data;
  }

  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  if (!GetTrack) {
    return data;
  }

  MediaTrack *track = GetTrack(nullptr, trackIndex);
  if (!track) {
    return data;
  }

  int (*CountTrackMediaItems)(MediaTrack *) =
      (int (*)(MediaTrack *))g_rec->GetFunc("CountTrackMediaItems");
  MediaItem *(*GetTrackMediaItem)(MediaTrack *, int) =
      (MediaItem * (*)(MediaTrack *, int)) g_rec->GetFunc("GetTrackMediaItem");

  if (!CountTrackMediaItems || !GetTrackMediaItem) {
    return data;
  }

  int itemCount = CountTrackMediaItems(track);
  if (itemCount == 0) {
    return data;
  }

  MediaItem *item = GetTrackMediaItem(track, 0);
  if (!item) {
    return data;
  }

  MediaItem_Take *(*GetActiveTake)(MediaItem *) =
      (MediaItem_Take * (*)(MediaItem *)) g_rec->GetFunc("GetActiveTake");
  if (!GetActiveTake) {
    return data;
  }

  MediaItem_Take *take = GetActiveTake(item);
  if (!take) {
    return data;
  }

  if (GetAudioSamples(take, data.samples, data.sampleRate, data.channels, config)) {
    data.valid = true;
  }

  return data;
}

DSPAnalysisResult MagdaDSPAnalyzer::AnalyzeSamples(const RawAudioData &audioData,
                                                   const DSPAnalysisConfig &config) {
  DSPAnalysisResult result;

  if (!audioData.valid || audioData.samples.empty()) {
    result.errorMessage.Set("Invalid audio data");
    return result;
  }

  result.sampleRate = audioData.sampleRate;
  result.channels = audioData.channels;
  result.lengthSeconds = (double)audioData.samples.size() / 
                         (audioData.sampleRate * audioData.channels);

  char logBuf[256];
  snprintf(logBuf, sizeof(logBuf),
           "MAGDA DSP: Analyzing %zu samples, %d Hz, %d ch, %.2f sec\n",
           audioData.samples.size(), audioData.sampleRate, audioData.channels, 
           result.lengthSeconds);
  LogMessage(logBuf);

  if (config.analyzeFrequency) {
    PerformFFT(audioData.samples, audioData.sampleRate, config.fftSize, 
               result.fftFrequencies, result.fftMagnitudes);
    CalculateFrequencyBands(result.fftFrequencies, result.fftMagnitudes,
                            result.bands);
    CalculateEQProfile(result.fftFrequencies, result.fftMagnitudes,
                       result.eqProfileFreqs, result.eqProfileMags);
    DetectPeaks(result.fftFrequencies, result.fftMagnitudes, result.peaks);
  }

  if (config.analyzeResonances && !result.peaks.empty()) {
    DetectResonances(result.peaks, result.eqProfileMags, result.resonances);
  }

  if (config.analyzeSpectralFeatures && !result.fftFrequencies.empty()) {
    result.spectralFeatures =
        CalculateSpectralFeatures(result.fftFrequencies, result.fftMagnitudes);
  }

  if (config.analyzeLoudness) {
    result.loudness = CalculateLoudness(audioData.samples, audioData.sampleRate, 
                                        audioData.channels);
  }

  if (config.analyzeDynamics) {
    result.dynamics = CalculateDynamics(audioData.samples, audioData.channels);
  }

  if (config.analyzeStereo && audioData.channels >= 2) {
    result.stereo = CalculateStereo(audioData.samples, audioData.channels);
  }

  if (config.analyzeTransients) {
    result.transients = CalculateTransients(audioData.samples, audioData.sampleRate, 
                                            audioData.channels);
  }

  result.success = true;
  LogMessage("MAGDA DSP: Analysis complete\n");
  return result;
}

bool MagdaDSPAnalyzer::GetAudioSamples(MediaItem_Take *take,
                                       std::vector<float> &samples,
                                       int &sampleRate, int &channels,
                                       const DSPAnalysisConfig &config) {
  if (!g_rec || !take) {
    return false;
  }

  // Get audio accessor
  AudioAccessor *(*CreateTakeAudioAccessor)(MediaItem_Take *) =
      (AudioAccessor * (*)(MediaItem_Take *))
          g_rec->GetFunc("CreateTakeAudioAccessor");
  void (*DestroyAudioAccessor)(AudioAccessor *) =
      (void (*)(AudioAccessor *))g_rec->GetFunc("DestroyAudioAccessor");
  int (*GetAudioAccessorSamples)(AudioAccessor *, int, int, double, int,
                                 double *) =
      (int (*)(AudioAccessor *, int, int, double, int, double *))g_rec->GetFunc(
          "GetAudioAccessorSamples");
  double (*GetAudioAccessorStartTime)(AudioAccessor *) =
      (double (*)(AudioAccessor *))g_rec->GetFunc("GetAudioAccessorStartTime");
  double (*GetAudioAccessorEndTime)(AudioAccessor *) =
      (double (*)(AudioAccessor *))g_rec->GetFunc("GetAudioAccessorEndTime");

  if (!CreateTakeAudioAccessor || !DestroyAudioAccessor ||
      !GetAudioAccessorSamples || !GetAudioAccessorStartTime ||
      !GetAudioAccessorEndTime) {
    LogMessage("MAGDA DSP: Audio accessor functions not available\n");
    return false;
  }

  // Get take source info
  PCM_source *(*GetMediaItemTake_Source)(MediaItem_Take *) =
      (PCM_source * (*)(MediaItem_Take *))
          g_rec->GetFunc("GetMediaItemTake_Source");
  if (!GetMediaItemTake_Source) {
    LogMessage("MAGDA DSP: GetMediaItemTake_Source not available\n");
    return false;
  }

  PCM_source *originalSource = GetMediaItemTake_Source(take);
  if (!originalSource) {
    LogMessage("MAGDA DSP: Take has no source\n");
    return false;
  }

  // Get source filename
  const char *(*GetMediaSourceFileName)(PCM_source *, char *, int) =
      (const char *(*)(PCM_source *, char *, int))g_rec->GetFunc("GetMediaSourceFileName");
  char filename[512] = {0};
  if (GetMediaSourceFileName) {
    GetMediaSourceFileName(originalSource, filename, sizeof(filename));
    char logBuf[600];
    snprintf(logBuf, sizeof(logBuf), "MAGDA DSP: Analyzing file: %s\n", filename[0] ? filename : "(no filename)");
    LogMessage(logBuf);
  }

  // Create a FRESH source from the file and swap it into the take
  // This forces REAPER to fully load the audio data
  PCM_source *(*PCM_Source_CreateFromFile)(const char *) =
      (PCM_source * (*)(const char *)) g_rec->GetFunc("PCM_Source_CreateFromFile");
  bool (*SetMediaItemTake_Source)(MediaItem_Take *, PCM_source *) =
      (bool (*)(MediaItem_Take *, PCM_source *))g_rec->GetFunc("SetMediaItemTake_Source");
  void (*PCM_Source_Destroy)(PCM_source *) =
      (void (*)(PCM_source *))g_rec->GetFunc("PCM_Source_Destroy");
  
  PCM_source *freshSource = nullptr;
  bool swappedSource = false;
  
  if (filename[0] && PCM_Source_CreateFromFile && SetMediaItemTake_Source) {
    freshSource = PCM_Source_CreateFromFile(filename);
    if (freshSource) {
      // Swap in the fresh source
      SetMediaItemTake_Source(take, freshSource);
      swappedSource = true;
      LogMessage("MAGDA DSP: Swapped in fresh source from file\n");
    }
  }
  
  // Use whichever source is now on the take
  PCM_source *source = GetMediaItemTake_Source(take);
  if (!source) {
    LogMessage("MAGDA DSP: Take lost its source after swap!\n");
    return false;
  }

  // Get source properties
  int (*GetMediaSourceNumChannels)(PCM_source *) =
      (int (*)(PCM_source *))g_rec->GetFunc("GetMediaSourceNumChannels");
  int (*GetMediaSourceSampleRate)(PCM_source *) =
      (int (*)(PCM_source *))g_rec->GetFunc("GetMediaSourceSampleRate");
  double (*GetMediaSourceLength)(PCM_source *, bool *) =
      (double (*)(PCM_source *, bool *))g_rec->GetFunc("GetMediaSourceLength");

  if (GetMediaSourceNumChannels) {
    channels = GetMediaSourceNumChannels(source);
  } else {
    channels = 2; // Default to stereo
  }

  if (GetMediaSourceSampleRate) {
    sampleRate = GetMediaSourceSampleRate(source);
  } else {
    sampleRate = 44100; // Default
  }

  double sourceLength = 0.0;
  if (GetMediaSourceLength) {
    sourceLength = GetMediaSourceLength(source, nullptr);
  }

  {
    char logBuf[256];
    snprintf(logBuf, sizeof(logBuf), "MAGDA DSP: Source reports: %.2f sec, %d Hz, %d ch\n", 
             sourceLength, sampleRate, channels);
    LogMessage(logBuf);
  }

  // Create audio accessor
  AudioAccessor *accessor = CreateTakeAudioAccessor(take);
  if (!accessor) {
    LogMessage("MAGDA DSP: Failed to create audio accessor\n");
    return false;
  }

  // CRITICAL: Force accessor to update after source swap
  void (*AudioAccessorUpdate)(AudioAccessor *) =
      (void (*)(AudioAccessor *))g_rec->GetFunc("AudioAccessorUpdate");
  
  // Always call update after swapping source
  if (AudioAccessorUpdate) {
    AudioAccessorUpdate(accessor);
    LogMessage("MAGDA DSP: Called AudioAccessorUpdate\n");
  }

  double startTime = GetAudioAccessorStartTime(accessor);
  double endTime = GetAudioAccessorEndTime(accessor);
  double duration = endTime - startTime;

  {
    char logBuf[256];
    snprintf(logBuf, sizeof(logBuf), "MAGDA DSP: Accessor reports: start=%.3f, end=%.3f, duration=%.3f sec\n", 
             startTime, endTime, duration);
    LogMessage(logBuf);
  }

  // Limit analysis length if configured
  if (!config.analyzeFullItem && config.analysisLength > 0 &&
      duration > config.analysisLength) {
    duration = config.analysisLength;
  }

  // Calculate number of samples to read
  int totalSamples = (int)(duration * sampleRate);
  if (totalSamples <= 0) {
    DestroyAudioAccessor(accessor);
    LogMessage("MAGDA DSP: No samples to analyze\n");
    return false;
  }

  // Limit to reasonable amount (e.g., 30 seconds max for performance)
  const int maxSamples = sampleRate * 30;
  if (totalSamples > maxSamples) {
    totalSamples = maxSamples;
    duration = (double)totalSamples / sampleRate;
  }

  {
    char logBuf[256];
    snprintf(logBuf, sizeof(logBuf), "MAGDA DSP: Requesting %d samples\n", totalSamples);
    LogMessage(logBuf);
  }

  // Allocate buffer (interleaved)
  std::vector<double> buffer(totalSamples * channels);

  // Read samples
  // NOTE: Return value is status (0=no audio, 1=success, -1=error), NOT sample count!
  int status = GetAudioAccessorSamples(
      accessor, sampleRate, channels, startTime, totalSamples, buffer.data());

  DestroyAudioAccessor(accessor);

  {
    char logBuf[256];
    snprintf(logBuf, sizeof(logBuf), "MAGDA DSP: GetAudioAccessorSamples status=%d (0=no audio, 1=success, -1=error)\n", status);
    LogMessage(logBuf);
  }

  // Restore original source if we swapped it
  if (swappedSource && SetMediaItemTake_Source && originalSource) {
    SetMediaItemTake_Source(take, originalSource);
    LogMessage("MAGDA DSP: Restored original source\n");
  }
  
  // Clean up fresh source if we created one
  if (freshSource && PCM_Source_Destroy) {
    PCM_Source_Destroy(freshSource);
  }

  if (status != 1) {
    char logBuf[128];
    snprintf(logBuf, sizeof(logBuf), "MAGDA DSP: Failed to read samples (status=%d)\n", status);
    LogMessage(logBuf);
    return false;
  }

  // Convert to float - use the REQUESTED sample count since status=1 means success
  samples.resize(totalSamples * channels);
  for (int i = 0; i < totalSamples * channels; i++) {
    samples[i] = (float)buffer[i];
  }

  char logBuf[128];
  snprintf(logBuf, sizeof(logBuf), "MAGDA DSP: Read %d samples successfully\n", totalSamples);
  LogMessage(logBuf);

  return true;
}

void MagdaDSPAnalyzer::SimpleDFT(const float *input, int N, float *realOut,
                                 float *imagOut) {
  // Simple DFT - O(N^2) but works for small windows
  for (int k = 0; k < N / 2 + 1; k++) {
    float sumReal = 0.0f;
    float sumImag = 0.0f;
    for (int n = 0; n < N; n++) {
      float angle = 2.0f * M_PI * k * n / N;
      sumReal += input[n] * cosf(angle);
      sumImag -= input[n] * sinf(angle);
    }
    realOut[k] = sumReal;
    imagOut[k] = sumImag;
  }
}

void MagdaDSPAnalyzer::PerformFFT(const std::vector<float> &samples,
                                  int sampleRate, int fftSize,
                                  std::vector<float> &frequencies,
                                  std::vector<float> &magnitudes) {
  if (samples.empty() || fftSize <= 0) {
    return;
  }

  // Number of output bins
  int numBins = fftSize / 2 + 1;

  frequencies.resize(numBins);
  magnitudes.resize(numBins, -96.0f);

  // Fill frequency bins
  for (int i = 0; i < numBins; i++) {
    frequencies[i] = (float)i * sampleRate / fftSize;
  }

  // Mix down to mono if stereo (average L+R)
  std::vector<float> mono;
  int numChannels = 2; // Assume stereo
  int numFrames = samples.size() / numChannels;

  if (numFrames < fftSize) {
    // Not enough samples
    return;
  }

  mono.resize(numFrames);
  for (int i = 0; i < numFrames; i++) {
    float sum = 0.0f;
    for (int ch = 0; ch < numChannels; ch++) {
      sum += samples[i * numChannels + ch];
    }
    mono[i] = sum / numChannels;
  }

  // Prepare windowed buffer
  std::vector<float> windowed(fftSize);
  std::vector<float> realOut(numBins);
  std::vector<float> imagOut(numBins);
  std::vector<double> magnitudeAccum(numBins, 0.0);

  // Process multiple windows and average
  int numWindows = 0;
  int hopSize = fftSize / 2;

  for (int start = 0; start + fftSize <= (int)mono.size(); start += hopSize) {
    // Apply Hann window
    for (int i = 0; i < fftSize; i++) {
      windowed[i] = mono[start + i] * HannWindow(i, fftSize);
    }

    // Perform DFT (simple version - for production, use FFTW or REAPER's FFT)
    SimpleDFT(windowed.data(), fftSize, realOut.data(), imagOut.data());

    // Accumulate magnitudes
    for (int i = 0; i < numBins; i++) {
      float mag = sqrtf(realOut[i] * realOut[i] + imagOut[i] * imagOut[i]);
      magnitudeAccum[i] += mag;
    }
    numWindows++;
  }

  // Average and convert to dB
  if (numWindows > 0) {
    for (int i = 0; i < numBins; i++) {
      float avgMag = (float)(magnitudeAccum[i] / numWindows);
      // Normalize by FFT size and convert to dB
      avgMag /= (fftSize / 2.0f);
      magnitudes[i] = LinearToDb(avgMag);
    }
  }
}

void MagdaDSPAnalyzer::CalculateFrequencyBands(
    const std::vector<float> &frequencies, const std::vector<float> &magnitudes,
    FrequencyBands &bands) {

  if (frequencies.empty() || magnitudes.empty()) {
    return;
  }

  // Accumulate energy in each band
  double subEnergy = 0, bassEnergy = 0, lowMidEnergy = 0, midEnergy = 0;
  double highMidEnergy = 0, presenceEnergy = 0, brillianceEnergy = 0;
  int subCount = 0, bassCount = 0, lowMidCount = 0, midCount = 0;
  int highMidCount = 0, presenceCount = 0, brillianceCount = 0;

  for (size_t i = 0; i < frequencies.size(); i++) {
    float freq = frequencies[i];
    float mag = magnitudes[i];
    float linear = DbToLinear(mag);

    if (freq >= 20 && freq < 60) {
      subEnergy += linear * linear;
      subCount++;
    } else if (freq >= 60 && freq < 250) {
      bassEnergy += linear * linear;
      bassCount++;
    } else if (freq >= 250 && freq < 500) {
      lowMidEnergy += linear * linear;
      lowMidCount++;
    } else if (freq >= 500 && freq < 2000) {
      midEnergy += linear * linear;
      midCount++;
    } else if (freq >= 2000 && freq < 4000) {
      highMidEnergy += linear * linear;
      highMidCount++;
    } else if (freq >= 4000 && freq < 6000) {
      presenceEnergy += linear * linear;
      presenceCount++;
    } else if (freq >= 6000 && freq <= 20000) {
      brillianceEnergy += linear * linear;
      brillianceCount++;
    }
  }

  // Convert to RMS dB
  bands.sub = subCount > 0 ? LinearToDb(sqrtf(subEnergy / subCount)) : -96.0f;
  bands.bass =
      bassCount > 0 ? LinearToDb(sqrtf(bassEnergy / bassCount)) : -96.0f;
  bands.lowMid =
      lowMidCount > 0 ? LinearToDb(sqrtf(lowMidEnergy / lowMidCount)) : -96.0f;
  bands.mid = midCount > 0 ? LinearToDb(sqrtf(midEnergy / midCount)) : -96.0f;
  bands.highMid = highMidCount > 0
                      ? LinearToDb(sqrtf(highMidEnergy / highMidCount))
                      : -96.0f;
  bands.presence = presenceCount > 0
                       ? LinearToDb(sqrtf(presenceEnergy / presenceCount))
                       : -96.0f;
  bands.brilliance = brillianceCount > 0
                         ? LinearToDb(sqrtf(brillianceEnergy / brillianceCount))
                         : -96.0f;
}

void MagdaDSPAnalyzer::CalculateEQProfile(const std::vector<float> &frequencies,
                                          const std::vector<float> &magnitudes,
                                          std::vector<float> &eqFreqs,
                                          std::vector<float> &eqMags) {

  eqFreqs.resize(kNumThirdOctaveBands);
  eqMags.resize(kNumThirdOctaveBands, -96.0f);

  for (int b = 0; b < kNumThirdOctaveBands; b++) {
    eqFreqs[b] = kThirdOctaveFreqs[b];

    // Calculate band edges (1/3 octave = 2^(1/3) ratio)
    float ratio = powf(2.0f, 1.0f / 6.0f); // Half of 1/3 octave
    float lowFreq = kThirdOctaveFreqs[b] / ratio;
    float highFreq = kThirdOctaveFreqs[b] * ratio;

    // Accumulate energy in band
    double energy = 0;
    int count = 0;

    for (size_t i = 0; i < frequencies.size(); i++) {
      if (frequencies[i] >= lowFreq && frequencies[i] <= highFreq) {
        float linear = DbToLinear(magnitudes[i]);
        energy += linear * linear;
        count++;
      }
    }

    if (count > 0) {
      eqMags[b] = LinearToDb(sqrtf(energy / count));
    }
  }
}

void MagdaDSPAnalyzer::DetectPeaks(const std::vector<float> &frequencies,
                                   const std::vector<float> &magnitudes,
                                   std::vector<FrequencyPeak> &peaks,
                                   float thresholdDb) {
  peaks.clear();

  if (frequencies.size() < 3) {
    return;
  }

  // Find local maxima above threshold
  for (size_t i = 1; i < magnitudes.size() - 1; i++) {
    if (magnitudes[i] > thresholdDb && magnitudes[i] > magnitudes[i - 1] &&
        magnitudes[i] > magnitudes[i + 1]) {

      FrequencyPeak peak;
      peak.frequency = frequencies[i];
      peak.magnitude = magnitudes[i];

      // Estimate Q factor from peak width at -3dB
      float targetLevel = magnitudes[i] - 3.0f;
      int leftIdx = i, rightIdx = i;

      // Find -3dB points
      while (leftIdx > 0 && magnitudes[leftIdx] > targetLevel) {
        leftIdx--;
      }
      while (rightIdx < (int)magnitudes.size() - 1 &&
             magnitudes[rightIdx] > targetLevel) {
        rightIdx++;
      }

      float bandwidth = frequencies[rightIdx] - frequencies[leftIdx];
      if (bandwidth > 0) {
        peak.q = peak.frequency / bandwidth;
      } else {
        peak.q = 10.0f; // Default Q
      }

      peaks.push_back(peak);
    }
  }

  // Sort by magnitude (loudest first)
  std::sort(peaks.begin(), peaks.end(),
            [](const FrequencyPeak &a, const FrequencyPeak &b) {
              return a.magnitude > b.magnitude;
            });

  // Limit to top 20 peaks
  if (peaks.size() > 20) {
    peaks.resize(20);
  }
}

void MagdaDSPAnalyzer::DetectResonances(const std::vector<FrequencyPeak> &peaks,
                                        const std::vector<float> &eqMags,
                                        std::vector<Resonance> &resonances) {
  resonances.clear();

  // Calculate average level for comparison
  float avgLevel = -96.0f;
  if (!eqMags.empty()) {
    float sum = 0;
    for (float mag : eqMags) {
      sum += mag;
    }
    avgLevel = sum / eqMags.size();
  }

  for (const auto &peak : peaks) {
    // Calculate how much this peak stands out from average
    float prominence = peak.magnitude - avgLevel;

    // High Q and high prominence = resonance
    if (peak.q > 5.0f && prominence > 6.0f) {
      Resonance res;
      res.frequency = peak.frequency;
      res.magnitude = prominence;
      res.q = peak.q;

      // Determine severity
      if (prominence > 12.0f || peak.q > 20.0f) {
        res.severity = "high";
      } else if (prominence > 9.0f || peak.q > 12.0f) {
        res.severity = "medium";
      } else {
        res.severity = "low";
      }

      // Determine type based on frequency and characteristics
      if (peak.frequency < 100) {
        res.type = "room_mode";
      } else if (peak.q > 15.0f) {
        res.type = "ringing";
      } else {
        res.type = "resonance";
      }

      resonances.push_back(res);
    }
  }

  // Limit resonances
  if (resonances.size() > 10) {
    resonances.resize(10);
  }
}

SpectralFeatures MagdaDSPAnalyzer::CalculateSpectralFeatures(
    const std::vector<float> &frequencies,
    const std::vector<float> &magnitudes) {

  SpectralFeatures features = {};

  if (frequencies.empty() || magnitudes.empty()) {
    return features;
  }

  // Calculate total energy and weighted sums
  double totalEnergy = 0;
  double weightedSum = 0;
  double lowEnergy = 0, midEnergy = 0, highEnergy = 0;

  for (size_t i = 0; i < frequencies.size(); i++) {
    float linear = DbToLinear(magnitudes[i]);
    float energy = linear * linear;
    totalEnergy += energy;
    weightedSum += frequencies[i] * energy;

    if (frequencies[i] < 250) {
      lowEnergy += energy;
    } else if (frequencies[i] < 4000) {
      midEnergy += energy;
    } else {
      highEnergy += energy;
    }
  }

  // Spectral centroid
  if (totalEnergy > 0) {
    features.spectralCentroid = weightedSum / totalEnergy;
  }

  // Energy distribution percentages
  if (totalEnergy > 0) {
    features.lowFreqEnergy = (lowEnergy / totalEnergy) * 100.0f;
    features.midFreqEnergy = (midEnergy / totalEnergy) * 100.0f;
    features.highFreqEnergy = (highEnergy / totalEnergy) * 100.0f;
  }

  // Spectral rolloff (85% energy threshold)
  double cumulativeEnergy = 0;
  double threshold = totalEnergy * 0.85;
  for (size_t i = 0; i < frequencies.size(); i++) {
    float linear = DbToLinear(magnitudes[i]);
    cumulativeEnergy += linear * linear;
    if (cumulativeEnergy >= threshold) {
      features.spectralRolloff = frequencies[i];
      break;
    }
  }

  // Spectral slope (dB/octave)
  // Simple linear regression on log-frequency axis
  if (frequencies.size() > 10) {
    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    int n = 0;
    for (size_t i = 1; i < frequencies.size(); i++) {
      if (frequencies[i] > 20 && magnitudes[i] > -90) {
        double logFreq = log2(frequencies[i]);
        sumX += logFreq;
        sumY += magnitudes[i];
        sumXY += logFreq * magnitudes[i];
        sumX2 += logFreq * logFreq;
        n++;
      }
    }
    if (n > 2) {
      features.spectralSlope =
          (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
    }
  }

  // Spectral flatness (geometric mean / arithmetic mean)
  // Higher = more noise-like, lower = more tonal
  double logSum = 0;
  double linSum = 0;
  int count = 0;
  for (size_t i = 0; i < magnitudes.size(); i++) {
    float linear = DbToLinear(magnitudes[i]);
    if (linear > 0) {
      logSum += log(linear);
      linSum += linear;
      count++;
    }
  }
  if (count > 0 && linSum > 0) {
    double geometricMean = exp(logSum / count);
    double arithmeticMean = linSum / count;
    features.spectralFlatness = geometricMean / arithmeticMean;
  }

  return features;
}

LoudnessAnalysis
MagdaDSPAnalyzer::CalculateLoudness(const std::vector<float> &samples,
                                    int sampleRate, int channels) {

  LoudnessAnalysis result = {};

  if (samples.empty()) {
    result.rms = -96.0f;
    result.peak = -96.0f;
    result.truePeak = -96.0f;
    result.lufs = -96.0f;
    result.lufsShortTerm = -96.0f;
    return result;
  }

  int numFrames = samples.size() / channels;

  // Calculate RMS and peak
  double sumSquares = 0;
  float peakSample = 0;

  for (size_t i = 0; i < samples.size(); i++) {
    float absSample = fabsf(samples[i]);
    sumSquares += samples[i] * samples[i];
    if (absSample > peakSample) {
      peakSample = absSample;
    }
  }

  float rms = sqrtf(sumSquares / samples.size());
  result.rms = LinearToDb(rms);
  result.peak = LinearToDb(peakSample);

  // True peak (simple 4x oversampling approximation)
  // For real implementation, use proper interpolation
  result.truePeak = result.peak + 0.5f; // Approximate

  // LUFS approximation
  // Real LUFS requires K-weighting filter, this is simplified
  // Roughly LUFS = RMS - 0.691 for dialogue, varies for music
  result.lufs = result.rms - 0.7f;
  result.lufsShortTerm = result.lufs; // Simplified

  return result;
}

DynamicsAnalysis
MagdaDSPAnalyzer::CalculateDynamics(const std::vector<float> &samples,
                                    int channels) {
  DynamicsAnalysis result = {};

  if (samples.empty()) {
    return result;
  }

  // Calculate peak and RMS
  double sumSquares = 0;
  float peak = 0;

  for (size_t i = 0; i < samples.size(); i++) {
    float absSample = fabsf(samples[i]);
    sumSquares += samples[i] * samples[i];
    if (absSample > peak) {
      peak = absSample;
    }
  }

  float rms = sqrtf(sumSquares / samples.size());

  // Crest factor = Peak/RMS (in dB)
  if (rms > 0) {
    result.crestFactor = LinearToDb(peak) - LinearToDb(rms);
  }

  // Dynamic range (simplified - difference between loud and quiet parts)
  // For real implementation, calculate percentiles
  result.dynamicRange = result.crestFactor * 1.5f; // Approximation

  // Compression ratio estimation (based on crest factor)
  // Lower crest factor = more compressed
  if (result.crestFactor < 6.0f) {
    result.compressionRatio = 4.0f; // Heavy compression
  } else if (result.crestFactor < 10.0f) {
    result.compressionRatio = 2.0f; // Moderate compression
  } else {
    result.compressionRatio = 1.0f; // Little/no compression
  }

  return result;
}

StereoAnalysis
MagdaDSPAnalyzer::CalculateStereo(const std::vector<float> &samples,
                                  int channels) {
  StereoAnalysis result = {};

  if (channels < 2 || samples.empty()) {
    return result;
  }

  int numFrames = samples.size() / channels;

  double sumL = 0, sumR = 0, sumLR = 0;
  double sumL2 = 0, sumR2 = 0;
  double sumMid2 = 0, sumSide2 = 0;

  for (int i = 0; i < numFrames; i++) {
    float L = samples[i * channels];
    float R = samples[i * channels + 1];
    float mid = (L + R) / 2.0f;
    float side = (L - R) / 2.0f;

    sumL += L;
    sumR += R;
    sumL2 += L * L;
    sumR2 += R * R;
    sumLR += L * R;
    sumMid2 += mid * mid;
    sumSide2 += side * side;
  }

  // Correlation coefficient
  double denom = sqrt(sumL2 * sumR2);
  if (denom > 0) {
    result.correlation = sumLR / denom;
  }

  // Stereo width (side/mid ratio)
  if (sumMid2 > 0) {
    result.width = sqrtf(sumSide2 / sumMid2);
    if (result.width > 1.0f)
      result.width = 1.0f;
  }

  // Balance
  double totalEnergy = sumL2 + sumR2;
  if (totalEnergy > 0) {
    result.balance = (sumR2 - sumL2) / totalEnergy;
  }

  return result;
}

TransientAnalysis
MagdaDSPAnalyzer::CalculateTransients(const std::vector<float> &samples,
                                      int sampleRate, int channels) {
  TransientAnalysis result = {};

  if (samples.empty()) {
    return result;
  }

  int numFrames = samples.size() / channels;

  // Mix to mono
  std::vector<float> mono(numFrames);
  for (int i = 0; i < numFrames; i++) {
    float sum = 0;
    for (int ch = 0; ch < channels; ch++) {
      sum += fabsf(samples[i * channels + ch]);
    }
    mono[i] = sum / channels;
  }

  // Simple envelope follower to detect attack
  float envelope = 0;
  float attack = 0.001f; // Fast attack
  float release = 0.01f; // Slow release

  float maxDerivative = 0;
  int attackSample = 0;

  for (int i = 1; i < numFrames; i++) {
    float input = mono[i];

    if (input > envelope) {
      envelope = envelope + attack * (input - envelope);
    } else {
      envelope = envelope + release * (input - envelope);
    }

    // Track derivative (rate of change)
    float derivative = envelope - mono[i - 1];
    if (derivative > maxDerivative) {
      maxDerivative = derivative;
      attackSample = i;
    }
  }

  // Attack time (time to reach peak from quiet)
  if (attackSample > 0) {
    result.attackTime = (float)attackSample / sampleRate;
  }

  // Transient energy (ratio of high-derivative samples)
  int transientSamples = 0;
  float threshold = maxDerivative * 0.5f;

  for (int i = 1; i < numFrames; i++) {
    float derivative = fabsf(mono[i] - mono[i - 1]);
    if (derivative > threshold) {
      transientSamples++;
    }
  }

  result.transientEnergy = (float)transientSamples / numFrames;

  return result;
}

void MagdaDSPAnalyzer::AppendFloatArray(WDL_FastString &json, const char *name,
                                        const std::vector<float> &arr,
                                        bool first) {
  if (!first) {
    json.Append(",");
  }
  json.AppendFormatted(256, "\"%s\":[", name);

  for (size_t i = 0; i < arr.size(); i++) {
    if (i > 0)
      json.Append(",");
    json.AppendFormatted(32, "%.2f", arr[i]);
  }

  json.Append("]");
}

void MagdaDSPAnalyzer::ToJSON(const DSPAnalysisResult &result,
                              WDL_FastString &json) {
  json.Append("{");

  // Success status
  json.AppendFormatted(64, "\"success\":%s", result.success ? "true" : "false");

  if (!result.success) {
    json.Append(",\"error\":");
    // Escape error message
    json.Append("\"");
    json.Append(result.errorMessage.Get());
    json.Append("\"");
    json.Append("}");
    return;
  }

  // Sample info
  json.AppendFormatted(
      128, ",\"sample_rate\":%d,\"channels\":%d,\"length\":%.3f",
      result.sampleRate, result.channels, result.lengthSeconds);

  // Frequency spectrum
  json.Append(",\"frequency_spectrum\":{");
  json.AppendFormatted(64, "\"fft_size\":%d",
                       (int)result.fftFrequencies.size() * 2);

  // Frequency bands
  json.Append(",\"bands\":{");
  json.AppendFormatted(64, "\"sub\":%.2f", result.bands.sub);
  json.AppendFormatted(64, ",\"bass\":%.2f", result.bands.bass);
  json.AppendFormatted(64, ",\"low_mid\":%.2f", result.bands.lowMid);
  json.AppendFormatted(64, ",\"mid\":%.2f", result.bands.mid);
  json.AppendFormatted(64, ",\"high_mid\":%.2f", result.bands.highMid);
  json.AppendFormatted(64, ",\"presence\":%.2f", result.bands.presence);
  json.AppendFormatted(64, ",\"brilliance\":%.2f", result.bands.brilliance);
  json.Append("}");

  // EQ Profile
  if (!result.eqProfileFreqs.empty()) {
    json.Append(",\"eq_profile\":{");
    json.Append("\"resolution\":\"1/3_octave\"");
    AppendFloatArray(json, "frequencies", result.eqProfileFreqs, false);
    AppendFloatArray(json, "magnitudes", result.eqProfileMags, false);
    json.Append("}");
  }

  // Spectral features
  json.Append(",\"spectral_features\":{");
  json.AppendFormatted(64, "\"spectral_centroid\":%.2f",
                       result.spectralFeatures.spectralCentroid);
  json.AppendFormatted(64, ",\"spectral_rolloff\":%.2f",
                       result.spectralFeatures.spectralRolloff);
  json.AppendFormatted(64, ",\"spectral_slope\":%.3f",
                       result.spectralFeatures.spectralSlope);
  json.AppendFormatted(64, ",\"spectral_flatness\":%.4f",
                       result.spectralFeatures.spectralFlatness);
  json.AppendFormatted(64, ",\"low_freq_energy\":%.2f",
                       result.spectralFeatures.lowFreqEnergy);
  json.AppendFormatted(64, ",\"mid_freq_energy\":%.2f",
                       result.spectralFeatures.midFreqEnergy);
  json.AppendFormatted(64, ",\"high_freq_energy\":%.2f",
                       result.spectralFeatures.highFreqEnergy);
  json.Append("}");

  // Peaks (top 10)
  if (!result.peaks.empty()) {
    json.Append(",\"peaks\":[");
    int peakCount = (std::min)((int)result.peaks.size(), 10);
    for (int i = 0; i < peakCount; i++) {
      if (i > 0)
        json.Append(",");
      json.AppendFormatted(128,
                           "{\"frequency\":%.2f,\"magnitude\":%.2f,\"q\":%.2f}",
                           result.peaks[i].frequency, result.peaks[i].magnitude,
                           result.peaks[i].q);
    }
    json.Append("]");
  }

  json.Append("}"); // End frequency_spectrum

  // Resonances
  if (!result.resonances.empty()) {
    json.Append(",\"resonances\":{\"resonances\":[");
    for (size_t i = 0; i < result.resonances.size(); i++) {
      if (i > 0)
        json.Append(",");
      json.AppendFormatted(
          256,
          "{\"frequency\":%.2f,\"magnitude\":%.2f,\"q\":%.2f,\"severity\":\"%"
          "s\",\"type\":\"%s\"}",
          result.resonances[i].frequency, result.resonances[i].magnitude,
          result.resonances[i].q, result.resonances[i].severity,
          result.resonances[i].type);
    }
    json.Append("]}");
  }

  // Loudness
  json.Append(",\"loudness\":{");
  json.AppendFormatted(64, "\"rms\":%.2f", result.loudness.rms);
  json.AppendFormatted(64, ",\"lufs\":%.2f", result.loudness.lufs);
  json.AppendFormatted(64, ",\"lufs_short_term\":%.2f",
                       result.loudness.lufsShortTerm);
  json.AppendFormatted(64, ",\"peak\":%.2f", result.loudness.peak);
  json.AppendFormatted(64, ",\"true_peak\":%.2f", result.loudness.truePeak);
  json.Append("}");

  // Dynamics
  json.Append(",\"dynamics\":{");
  json.AppendFormatted(64, "\"dynamic_range\":%.2f",
                       result.dynamics.dynamicRange);
  json.AppendFormatted(64, ",\"crest_factor\":%.2f",
                       result.dynamics.crestFactor);
  json.AppendFormatted(64, ",\"compression_ratio\":%.1f",
                       result.dynamics.compressionRatio);
  json.Append("}");

  // Stereo
  json.Append(",\"stereo\":{");
  json.AppendFormatted(64, "\"width\":%.3f", result.stereo.width);
  json.AppendFormatted(64, ",\"correlation\":%.3f", result.stereo.correlation);
  json.AppendFormatted(64, ",\"balance\":%.3f", result.stereo.balance);
  json.Append("}");

  // Transients
  json.Append(",\"transients\":{");
  json.AppendFormatted(64, "\"attack_time\":%.4f",
                       result.transients.attackTime);
  json.AppendFormatted(64, ",\"transient_energy\":%.3f",
                       result.transients.transientEnergy);
  json.Append("}");

  json.Append("}");
}

void MagdaDSPAnalyzer::GetTrackFXInfo(int trackIndex, WDL_FastString &json) {
  // Output only the array - caller adds the key name
  json.Append("[");

  if (!g_rec) {
    json.Append("]");
    return;
  }

  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  int (*TrackFX_GetCount)(MediaTrack *) =
      (int (*)(MediaTrack *))g_rec->GetFunc("TrackFX_GetCount");
  bool (*TrackFX_GetFXName)(MediaTrack *, int, char *, int) = (bool (*)(
      MediaTrack *, int, char *, int))g_rec->GetFunc("TrackFX_GetFXName");
  bool (*TrackFX_GetEnabled)(MediaTrack *, int) =
      (bool (*)(MediaTrack *, int))g_rec->GetFunc("TrackFX_GetEnabled");
  int (*TrackFX_GetNumParams)(MediaTrack *, int) =
      (int (*)(MediaTrack *, int))g_rec->GetFunc("TrackFX_GetNumParams");
  double (*TrackFX_GetParam)(MediaTrack *, int, int, double *, double *) =
      (double (*)(MediaTrack *, int, int, double *, double *))g_rec->GetFunc(
          "TrackFX_GetParam");
  bool (*TrackFX_GetParamName)(MediaTrack *, int, int, char *, int) =
      (bool (*)(MediaTrack *, int, int, char *, int))g_rec->GetFunc(
          "TrackFX_GetParamName");

  if (!GetTrack || !TrackFX_GetCount || !TrackFX_GetFXName) {
    json.Append("]");
    return;
  }

  MediaTrack *track = GetTrack(nullptr, trackIndex);
  if (!track) {
    json.Append("]");
    return;
  }

  int fxCount = TrackFX_GetCount(track);
  bool first = true;

  for (int fx = 0; fx < fxCount; fx++) {
    if (!first)
      json.Append(",");
    first = false;

    json.Append("{");

    // FX name
    char fxName[256] = "";
    if (TrackFX_GetFXName) {
      TrackFX_GetFXName(track, fx, fxName, sizeof(fxName));
    }
    json.Append("\"name\":\"");
    json.Append(fxName);
    json.Append("\"");

    // Index
    json.AppendFormatted(32, ",\"index\":%d", fx);

    // Enabled
    bool enabled = true;
    if (TrackFX_GetEnabled) {
      enabled = TrackFX_GetEnabled(track, fx);
    }
    json.AppendFormatted(32, ",\"enabled\":%s", enabled ? "true" : "false");

    // Parameters (limit to first 20 for size)
    if (TrackFX_GetNumParams && TrackFX_GetParam && TrackFX_GetParamName) {
      int numParams = TrackFX_GetNumParams(track, fx);
      int maxParams = (std::min)(numParams, 20);

      json.Append(",\"parameters\":[");

      for (int p = 0; p < maxParams; p++) {
        if (p > 0)
          json.Append(",");

        char paramName[128] = "";
        TrackFX_GetParamName(track, fx, p, paramName, sizeof(paramName));

        double minVal = 0, maxVal = 1;
        double value = TrackFX_GetParam(track, fx, p, &minVal, &maxVal);

        json.Append("{\"name\":\"");
        json.Append(paramName);
        json.AppendFormatted(128,
                             "\",\"value\":%.4f,\"min\":%.4f,\"max\":%.4f}",
                             value, minVal, maxVal);
      }

      json.Append("]");
    }

    json.Append("}");
  }

  json.Append("]");
}
