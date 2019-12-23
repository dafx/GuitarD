#pragma once
#include "thirdparty/fftconvolver/TwoStageFFTConvolver.h"
// #include "resample.h"
#include "src/node/Node.h"

#include <thirdparty/json.hpp>
#include <cstring>

// avoid some UNICODE issues with VST3 SDK and WDL dirscan
#if defined VST3_API && defined OS_WIN
  #ifdef FindFirstFile
    #undef FindFirstFile
    #undef FindNextFile
    #undef WIN32_FIND_DATA
    #undef PWIN32_FIND_DATA
    #define FindFirstFile FindFirstFileA
    #define FindNextFile FindNextFileA
    #define WIN32_FIND_DATA WIN32_FIND_DATAA
    #define LPWIN32_FIND_DATA LPWIN32_FIND_DATAA
  #endif
#endif

#include "dirscan.h"


#include "IPlugPaths.h"

struct MicPosition {
  WDL_String name;
  WDL_String path;
};

struct Microphone {
  WDL_String name;
  WDL_String path;
  WDL_PtrList<MicPosition> mPositions;
  ~Microphone() {
    mPositions.Empty(true);
  }

  void scanPositions() {
    WDL_DirScan dir;
    if (!dir.First(path.Get())) {
      do {
        const char* f = dir.GetCurrentFN();
        // Skip hidden files and folders
        if (f && f[0] != '.') {
          if (!dir.GetCurrentIsDirectory()) {
            // We're only interested in files, each corresponds to a IR
            MicPosition* pos = new MicPosition();
            pos->name.Append(f);
            if (strncmp(".wav", pos->name.get_fileext(), 5) == 0,
                strncmp(".WAV", pos->name.get_fileext(), 5) == 0)
            {
              dir.GetCurrentFullFN(&pos->path);
              mPositions.Add(pos);
            }
            else {
              delete pos;
            }
          }
        }
      } while (!dir.Next());
    }
  }
};

struct Cabinet {
  WDL_String name;
  WDL_String path;
  WDL_PtrList<Microphone> mMics;

  ~Cabinet() {
    mMics.Empty(true);
  }

  void scanMics() {
    WDL_DirScan dir;
    if (!dir.First(path.Get())) {
      do {
        const char* f = dir.GetCurrentFN();
        // Skip hidden files and folders
        if (f && f[0] != '.') {
          if (dir.GetCurrentIsDirectory()) {
            // We're only interested in folders, each corresponds to a mic
            Microphone* mic = new Microphone();
            mMics.Add(mic);
            mic->name.Append(f);
            dir.GetCurrentFullFN(&mic->path);
            mic->scanPositions();
          }
        }
      } while (!dir.Next());
    }
  }
};

class CabLibNodeUi : public NodeUi {
  WDL_PtrList<Cabinet> mCabinets;
public:
  CabLibNodeUi(NodeShared* param) : NodeUi(param) {
    WDL_String iniFolder;
    INIPath(iniFolder, BUNDLE_NAME);
    iniFolder.Append(PATH_DELIMITER);
    iniFolder.Append("impulses");
    buildIrTree(iniFolder.Get());
  }

  ~CabLibNodeUi() {
    mCabinets.Empty(true);
  }

  void Draw(IGraphics& g) override {
    NodeUi::Draw(g);
    //if (mCabShared != nullptr) {
    //  g.DrawText(mBlocksizeText, mCabShared->loadedIr.name.Get(), mRECT.GetVShifted(20));
    //}
  }

  void buildIrTree(const char* folder) {
    WDL_DirScan dir;
    WDBGMSG("Cabinets\n");
    if (!dir.First(folder)) {
      do {
        const char* f = dir.GetCurrentFN();
        // Skip hidden files and folders
        if (f && f[0] != '.') {
          if (dir.GetCurrentIsDirectory()) {
            // We're only interested in folders, at the top level each corresponds to a cab
            Cabinet* cab = new Cabinet();
            mCabinets.Add(cab);
            cab->name.Append(f);
            dir.GetCurrentFullFN(&cab->path);
            cab->scanMics();
            WDBGMSG(cab->name.Get());
          }
        }
      } while (!dir.Next());
    }
    else {
      WDBGMSG("IR folder doesn't exist!\n");
    }
  }
};


class CabLibNode final : public Node {
  fftconvolver::TwoStageFFTConvolver* mConvolvers[8] = { nullptr };
  WDL_RESAMPLE_TYPE** mConversionBufferIn = nullptr;
  WDL_RESAMPLE_TYPE** mConversionBufferOut = nullptr;
  bool mIRLoaded = false;

public:
  CabLibNode(const std::string pType) {
    shared.type = pType;
    mStereo = 0;
    addByPassParam();
    addStereoParam();
    shared.parameters[1]->y = -30;
  }

  void clearLoadedIR() const {
    //if (mCabShared.loadedIr.path.GetLength() != 0 && mCabShared.loadedIr.samples != nullptr) {
    //  // Check if the previous IR is custom and clear out the allocated data
    //  for (int c = 0; c < mCabShared.loadedIr.channelCount; c++) {
    //    delete[] mCabShared.loadedIr.samples[c];
    //  }
    //  delete[] mCabShared.loadedIr.samples;
    //  // TODOG check if the names leak
    //}
  }

  void resampleAndLoadIR() {
    //mIRLoaded = false;
    //clearLoadedIR();

    //if (b.path.GetLength() != 0 && b.samples == nullptr) {
    //  // Load the wav file if the to be loaded IR is custom
    //  loadWave(b);
    //}
    //for (int c = 0; c < b.channelCount; c++) {
    //  WDL_Resampler resampler;
    //  resampler.SetMode(true, 0, true);
    //  resampler.SetFilterParms();
    //  resampler.SetFeedMode(true);
    //  resampler.SetRates(b.sampleRate, mSampleRate);
    //  WDL_RESAMPLE_TYPE* inBuffer;
    //  const int inSamples = resampler.ResamplePrepare(b.sampleCount, 1, &inBuffer);
    //  for (int i = 0; i < b.sampleCount; i++) {
    //    // Adjust the volume, since higher samplerates result in louder outputs
    //    inBuffer[i] = b.samples[c][i] * (b.sampleRate / static_cast<float>(mSampleRate)) * 0.2f;
    //  }
    //  const int newSize = ceil(b.sampleCount * ((mSampleRate / static_cast<float>(b.sampleRate))));
    //  WDL_RESAMPLE_TYPE* outBuffer = new WDL_RESAMPLE_TYPE[newSize];
    //  const int outSamples = resampler.ResampleOut(outBuffer, inSamples, b.sampleCount, 1);
    //  if (b.channelCount == 1) {
    //    for (int ch = 0; ch < mChannelCount; ch++) {
    //      mConvolvers[ch]->init(128, 1024 * 4, outBuffer, outSamples);
    //    }
    //  }
    //  else if (b.channelCount == mChannelCount) {
    //    mConvolvers[c]->init(128, 1024 * 4, outBuffer, outSamples);
    //  }
    //  delete[] outBuffer;
    //}
    //mCabShared.loadedIr = b;
    //mIRLoaded = true;
  }

  static void loadWave(const char* path) {
    //drwav wav;
    //if (!drwav_init_file(&wav, b.path.Get(), nullptr)) {
    //  return;
    //}

    //float* pSampleData = static_cast<float*>(malloc(
    //  static_cast<size_t>(wav.totalPCMFrameCount)* wav.channels * sizeof(float)
    //));
    //const size_t samplesRead = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, pSampleData);
    //const float length = samplesRead / static_cast<float>(wav.sampleRate);
    //if (length > 10.f || samplesRead < 1) {
    //  assert(false);
    //}
    //b.sampleRate = wav.sampleRate;
    //b.channelCount = wav.channels;
    //b.sampleCount = samplesRead;
    //b.samples = new float* [b.channelCount];
    //for (int c = 0; c < b.channelCount; c++) {
    //  b.samples[c] = new float[b.sampleCount];
    //}
    //for (int s = 0; s < samplesRead * b.channelCount; s++) {
    //  int channel = s % b.channelCount;
    //  size_t sample = s / b.channelCount;
    //  b.samples[channel][sample] = pSampleData[s];
    //}
    //free(pSampleData);
    //drwav_uninit(&wav);
  }

  void serializeAdditional(nlohmann::json& serialized) override {
    //serialized["irName"] = mCabShared.loadedIr.name.Get();
    //bool custom = mCabShared.loadedIr.path.GetLength() != 0;
    //serialized["customIR"] = custom;
    //serialized["path"] = custom ? mCabShared.loadedIr.path.Get() : "";
  }

  void deserializeAdditional(nlohmann::json& serialized) override {
    //try {
    //  IRBundle load;
    //  if (!serialized.contains("irName")) {
    //    return;
    //  }
    //  const string name = serialized.at("irName");
    //  load.name.Set(name.c_str());
    //  const bool customIR = serialized.at("customIR");
    //  if (customIR) {
    //    const string path = serialized.at("path");
    //    load.path.Set(path.c_str());
    //  }
    //  else {
    //    for (int i = 0; i < InternalIRsCount; i++) {
    //      // Go look for the right internal IR
    //      if (strncmp(load.name.Get(), InternalIRs[i].name.Get(), 30) == 0) {
    //        load = InternalIRs[i];
    //        break;
    //      }
    //    }
    //  }
    //  resampleAndLoadIR(load);
    //}
    //catch (...) {
    //  WDBGMSG("Failed to load Cab node data!\n");
    //}
  }

  void createBuffers() override {
    Node::createBuffers();
    for (int c = 0; c < mChannelCount; c++) {
      mConvolvers[c] = new fftconvolver::TwoStageFFTConvolver();
    }

#ifdef FLOATCONV
    mConversionBufferIn = new WDL_RESAMPLE_TYPE * [mChannelCount];
    mConversionBufferOut = new WDL_RESAMPLE_TYPE * [mChannelCount];
    for (int c = 0; c < mChannelCount; c++) {
      mConversionBufferIn[c] = new WDL_RESAMPLE_TYPE[mMaxBuffer];
      mConversionBufferOut[c] = new WDL_RESAMPLE_TYPE[mMaxBuffer];
    }
#endif
    // resampleAndLoadIR(InternalIRs[0]);
  }

  void deleteBuffers() override {
    Node::deleteBuffers();
    clearLoadedIR();
    for (int c = 0; c < mChannelCount; c++) {
      delete mConvolvers[c];
    }
#ifdef FLOATCONV
    for (int c = 0; c < mChannelCount; c++) {
      delete[] mConversionBufferIn[c];
      delete[] mConversionBufferOut[c];
    }
    delete[] mConversionBufferIn;
    delete[] mConversionBufferOut;
    mConversionBufferIn = nullptr;
    mConversionBufferOut = nullptr;
#endif
  }

  void ProcessBlock(const int nFrames) override {
    outputSilence();
    return;
    if (!inputsReady() || mIsProcessed || byPass()) { return; }
    if (!mIRLoaded) {
      outputSilence();
      return;
    }
    shared.parameters[1]->update();

    iplug::sample** buffer = shared.socketsIn[0]->mConnectedTo[0]->mParentBuffer;

#ifdef FLOATCONV
    /**                           REFERENCE                           */
    for (int i = 0; i < nFrames; i++) {
      mConversionBufferIn[0][i] = static_cast<float>(buffer[0][i]);
    }
    mConvolvers[0]->process(mConversionBufferIn[0], mConversionBufferOut[0], nFrames);
    for (int i = 0; i < nFrames; i++) {
      mBuffersOut[0][0][i] = mConversionBufferOut[0][i];
    }

    if (!mStereo) {
      for (int i = 0; i < nFrames; i++) {
        mBuffersOut[0][1][i] = mBuffersOut[0][0][i];
      }
    }
    else {
      for (int i = 0; i < nFrames; i++) {
        mConversionBufferIn[1][i] = static_cast<float>(buffer[1][i]);
      }
      mConvolvers[1]->process(mConversionBufferIn[1], mConversionBufferOut[1], nFrames);
      for (int i = 0; i < nFrames; i++) {
        mBuffersOut[0][1][i] = mConversionBufferOut[1][i];
      }
    }

#else
    convolver.process(buffer[0], outputs[0][0], nFrames);
    if (mStereo) {
      convolver2.process(buffer[1], outputs[0][1], nFrames);
    }
    else {
      for (int i = 0; i < nFrames; i++) {
        outputs[0][1][i] = outputs[0][0][i];
      }
    }
#endif
    mIsProcessed = true;
  }

  void setupUi(iplug::igraphics::IGraphics* pGrahics) override {
    shared.graphics = pGrahics;
    CabLibNodeUi* ui = new CabLibNodeUi(&shared);
    mUi = ui;
    pGrahics->AttachControl(mUi);
    mUi->setColor(IColor(255, 150, 100, 100));
    mUi->setUp();
    mUiReady = true;

  }
};
