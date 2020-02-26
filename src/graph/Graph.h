#pragma once
#include <chrono>
#include "../../thirdparty/soundwoofer/soundwoofer.h"
#include "../misc/constants.h"
#include "../types/gmutex.h"
#include "../types/types.h"
#include "../types/pointerList.h"
#include "../misc/MessageBus.h"
#include "../nodes/io/InputNode.h"
#include "../nodes/io/OutputNode.h"
#include "./Serializer.h"
#include "../parameter/ParameterManager.h"

namespace guitard {
  /**
   * This is the "god object" which will handle all the nodes
   * and interactions with the graph
   * It's not a IControl itself but owns a few which make up the GUI
   */
  class Graph {
    MessageBus::Bus* mBus = nullptr;


    ParameterManager* mParamManager = nullptr;

    /**
     * Holds all the nodes in the processing graph
     */
    PointerList<Node> mNodes;

    /**
     * Mutex to keep changes to the graph like adding/removing or rerouting from crashing
     */
    Mutex mAudioMutex;

    /**
     * Acts as a semaphore since the mAudioMutex only needs to be locked once to stop the audio thread
     */
    int mPauseAudio = 0;

    /**
     * Dummy nodes to get the audio blocks in and out of the graph
     */
    InputNode* mInputNode;
    OutputNode* mOutputNode;

    /**
     * This is the channel count to be used internally
     * All nodes will allocate buffers to according to this
     * Using anything besides stereo will cause problems with the faust DSP code
     */
    int mChannelCount = 0;

    /**
     * This is the actual input channel count provided by the DAW
     * The input channel count can be 1 if the plugin is on a mono track
     * But internal processing will still happen in stereo
     */
    int mInPutChannelCount = 0;

    int mSampleRate = 0;

    /**
     * The max blockSize which can be used to minimize round trip delay when having cycles in the graph
     */
    int mMaxBlockSize = MAX_BUFFER;

    GraphStats mStats;

    /**
     * Used to slice the dsp block in smaller slices
     * Only does stereo for now
     */
    sample** mSliceBuffer[2] = { nullptr };

    float mScale = 1.0; // This is the zoom level of the graph

  public:

    explicit Graph(MessageBus::Bus* pBus, ParameterManager* pParamManager) {
      mBus = pBus; // Goal is to get this out of all the non UI classes
      mParamManager = pParamManager; // we'll keep this around to let nodes register parameters

      mInputNode = new InputNode(mBus);
      mOutputNode = new OutputNode(mBus);
      mOutputNode->connectInput(mInputNode->shared.socketsOut[0]);
    }

    ~Graph() {
      removeAllNodes();
      // TODOG get rid of all the things
    }

    void lockAudioThread() {
      if (mPauseAudio == 0) {
        mAudioMutex.lock();
      }
      mPauseAudio++;
    }

    void unlockAudioThread() {
      if (mPauseAudio == 1) {
        mAudioMutex.unlock();
      }
      mPauseAudio--;
    }

    void OnReset(const int pSampleRate, const int pOutputChannels = 2, const int pInputChannels = 2) {
      if (pSampleRate != mSampleRate || pOutputChannels != mChannelCount || pInputChannels != mInPutChannelCount) {
        lockAudioThread();
        mSampleRate = pSampleRate;
        {
          if (mSliceBuffer[0] != nullptr) {
            for (int c = 0; c < pOutputChannels; c++) {
              delete mSliceBuffer[0];
              mSliceBuffer[0] = nullptr;
              delete mSliceBuffer[1];
              mSliceBuffer[1] = nullptr;
            }
          }
          mSliceBuffer[0] = new sample * [pOutputChannels];
          mSliceBuffer[1] = new sample * [pOutputChannels];
          /**
           * There's no need to create a buffer inside since it will use the pointer from the
           * scratch buffer offset by the sub-block position
           */
        }
        mChannelCount = pOutputChannels;
        mInPutChannelCount = pInputChannels;
        mInputNode->setInputChannels(pInputChannels);
        mInputNode->OnReset(pSampleRate, pOutputChannels);
        mOutputNode->OnReset(pSampleRate, pOutputChannels);
        for (int i = 0; i < mNodes.size(); i++) {
          mNodes[i]->OnReset(pSampleRate, pOutputChannels);
        }
        unlockAudioThread();
      }
      else {
        /**
         * If nothing has changed we'll assume a transport
         */
        OnTransport();
      }
    }

    /**
     * Will clear all the DSP buffers to kill reverbs and so on
     */
    void OnTransport() {
      lockAudioThread();
      mInputNode->OnTransport();
      mOutputNode->OnTransport();
      for (size_t i = 0; i < mNodes.size(); i++) {
        mNodes[i]->OnTransport();
      }
      unlockAudioThread();
    }

    /**
     * Will set the max blocksize to change roundtrip delay inside the graph
     */
    void setBlockSize(const int size) {
      if (size == mMaxBlockSize || size > MAX_BUFFER) { return; }
      lockAudioThread();
      mMaxBlockSize = size;
      for (int i = 0; i < mNodes.size(); i++) {
        Node* n = mNodes[i];
        n->shared.maxBlockSize = size;
        n->OnReset(mSampleRate, mChannelCount, true);
      }
      unlockAudioThread();
    }

    /**
     * Main entry point for the DSP
     */
    void ProcessBlock(sample** in, sample** out, const int nFrames) {
      /**
       * Process the block in smaller bits since it's too large
       * Also abused to lower the delay a feedback node creates
       */
      if (nFrames > mMaxBlockSize) {
        const int overhang = nFrames % mMaxBlockSize;
        int s = 0;
        while (true) {
          for (int c = 0; c < mChannelCount; c++) {
            mSliceBuffer[0][c] = &in[c][s];
            mSliceBuffer[1][c] = &out[c][s];
          }
          s += mMaxBlockSize;
          if (s <= nFrames) {
            ProcessBlock(mSliceBuffer[0], mSliceBuffer[1], mMaxBlockSize);
          }
          else {
            if (overhang > 0) {
              ProcessBlock(mSliceBuffer[0], mSliceBuffer[1], overhang);
            }
            return;
          }
        }
      }


      const int nodeCount = mNodes.size();
      const int maxAttempts = 10;

      {
        if (mPauseAudio > 0) {
          /**
           * Skip the block if the mutex is locked, waiting will most likely result in an under-run anyways
           */
          for (int c = 0; c < mChannelCount; c++) {
            for (int i = 0; i < nFrames; i++) {
              out[c][i] = 0;
            }
          }
          return;
        }

        const auto start = std::chrono::high_resolution_clock::now();
        LockGuard lock(mAudioMutex);
        mInputNode->CopyIn(in, nFrames);
        for (int n = 0; n < nodeCount; n++) {
          mNodes[n]->BlockStart();
        }
        mOutputNode->BlockStart();
        // The List is pre sorted so the attempts are only needed to catch circular dependencies and other edge cases
        int attempts = 0;
        while (!mOutputNode->mIsProcessed && attempts < maxAttempts) {
          for (int n = 0; n < nodeCount; n++) {
            mNodes[n]->ProcessBlock(nFrames);
          }
          mOutputNode->ProcessBlock(nFrames);
          attempts++;
        }

        // This extra iteration makes sure the feedback loops get data from their previous nodes
        if (attempts < maxAttempts) {
          for (int n = 0; n < nodeCount; n++) {
            mNodes[n]->ProcessBlock(nFrames);
          }
          if (!mStats.valid) {
            mStats.valid = true;
            MessageBus::fireEvent(mBus, MessageBus::GraphStatsChanged, &mStats);
          }
        }
        else {
          // failed processing
          if (mStats.valid) {
            mStats.valid = false;
            MessageBus::fireEvent(mBus, MessageBus::GraphStatsChanged, &mStats);
          }
        }

        mOutputNode->CopyOut(out, nFrames);
        const auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::high_resolution_clock::now() - start
          );
        mStats.executionTime = duration.count();
      }
    }

    /**
     * Used to add nodes and pause the audio thread
     */
    void addNode(
        Node* node, Node* pInput = nullptr, const float x = 0, const float y = 0,
        const int outputIndex = 0, const int inputIndex = 0, Node* clone = nullptr
    ) {
      if (mNodes.find(node) != -1) {
        assert(false); // In case node is already in the list
        return;
      }
      node->shared.X = x;
      node->shared.Y = y;
      node->setup(mBus, mSampleRate, mMaxBlockSize);

      if (clone != nullptr) {
        node->copyState(clone);
      }

      if (mParamManager != nullptr) {
        mParamManager->claimNode(node);
      }

      if (pInput != nullptr) {
        node->connectInput(pInput->shared.socketsOut[outputIndex], inputIndex);
      }

      lockAudioThread();
      // Allocating the node is thread safe, but not the node list itself
      mNodes.add(node);
      sortGraphWithoutLock();
      unlockAudioThread();
    }

    /**
     * Will re route the connections the node provided
     * Only takes care of the first input and first output
     */
    void byPassConnection(Node* node) const {
      if (node->shared.inputCount > 0 && node->shared.outputCount > 0) {
        NodeSocket* inSock = node->shared.socketsIn[0];
        NodeSocket* outSock = node->shared.socketsOut[0];
        NodeSocket* prevSock = inSock->mConnectedTo[0];
        if (prevSock != nullptr) { // make sure there's a previous node
          int nextSocketCount = 0;
          NodeSocket* nextSockets[MAX_SOCKET_CONNECTIONS] = { nullptr };
          for (int i = 0; i < MAX_SOCKET_CONNECTIONS; i++) { // Gather all the sockets connected to the output of this node
            NodeSocket* nextSock = outSock->mConnectedTo[i];
            if (nextSock != nullptr) {
              bool duplicate = false;
              for (int j = 0; j < nextSocketCount; j++) {
                if (nextSockets[j] == nextSock) {
                  duplicate = true;
                }
              }
              if (!duplicate) {
              nextSockets[nextSocketCount] = nextSock;
              nextSocketCount++;
            }
          }
          }

          outSock->disconnectAll();
          inSock->disconnectAll();

          for (int i = 0; i < nextSocketCount; i++) {
            prevSock->connect(nextSockets[i]);
          }
        }
      }
    }

    /**
     * Adds a combine node after provided node to act as a dry wet mix
     * and returns a pointer to the new combine
     */
    Node* spliceInCombine(Node* node) {
      if (node->shared.inputCount > 0 && node->shared.outputCount > 0) {
        NodeSocket* inSock = node->shared.socketsIn[0];
        NodeSocket* outSock = node->shared.socketsOut[0];
        NodeSocket* source = inSock->mConnectedTo[0];
        NodeSocket* target = outSock->mConnectedTo[0];
        if (target == nullptr || source == nullptr) {
          return nullptr;
        }
        Node* combine = NodeList::createNode("CombineNode");
        addNode(combine, nullptr, node->shared.X, node->shared.Y);

        for (int i = 0; i < MAX_SOCKET_CONNECTIONS; i++) {
          if (outSock->mConnectedTo[i] != nullptr) {
            combine->shared.socketsOut[0]->connect(outSock->mConnectedTo[i]);
          }
        }
        combine->shared.socketsIn[0]->connect(outSock);
        combine->shared.socketsIn[1]->connect(source);
        return combine;
      }
      return nullptr;
    }

    void removeAllNodes() {
      while (mNodes.size()) {
        removeNode(0);
      }
    }

    /**
     * Removes the node and pauses the audio thread
     * Can also bridge the connection if possible
     */
    void removeNode(Node* node, const bool reconnect = false) {
      if (node == mInputNode || node == mOutputNode) { return; }
      lockAudioThread();
      if (reconnect) {
        byPassConnection(node);
      }

      if (mParamManager != nullptr) {
        mParamManager->releaseNode(node);
      }
      
      node->cleanUp();
      mNodes.remove(node);
      if (mNodes.find(node) != -1) {
        assert(false);
      }
      delete node;
      sortGraphWithoutLock();
      // mMaxBlockSize = hasFeedBackNode() ? MIN_BLOCK_SIZE : MAX_BUFFER;
      unlockAudioThread();
    }

    void removeNode(const int index) {
      removeNode(mNodes[index]);
    }

    void serialize(WDL_String& serialized) {
      nlohmann::json json;
      serialize(json);
      try {
        serialized.Set(json.dump(4).c_str());
      }
      catch (...) {
        assert(false); // Failed to dump json
      }
    }

    void serialize(nlohmann::json& json) {
      try {
        json = {
          { "version", PLUG_VERSION_HEX },
        };
        json["maxBlockSize"] = mMaxBlockSize;
        Serializer::serialize(json, &mNodes, mInputNode, mOutputNode);
      }
      catch (...) {
        assert(false); // Failed to serialize json
      }
    }

    void deserialize(const char* data) {
      try {
        nlohmann::json json = nlohmann::json::parse(data);
        deserialize(json);
      }
      catch (...) {
        return;
        // assert(false); // Failed to parse JSON
      }
    }

    void deserialize(nlohmann::json& json) {
      try {
        removeAllNodes();
        if (json.contains("maxBlockSize")) {
          mMaxBlockSize = json["maxBlockSize"];
        }
        lockAudioThread();
        Serializer::deserialize(json, &mNodes, mOutputNode, mInputNode, mSampleRate, mMaxBlockSize, mParamManager, mBus);
#ifndef GUITARD_HEADLESS
        //if (mGraphics != nullptr && mGraphics->WindowIsOpen()) {
        //  for (int i = 0; i < mNodes.size(); i++) {
        //    mNodes[i]->setupUi(mGraphics);
        //  }
        //  scaleUi();
        //}
#endif
        sortGraphWithoutLock();
        unlockAudioThread();
      }
      catch (...) {
        // assert(false); // To load graph with json
      }
    }

    float getScale() const {
      return mScale;
    }

    void setScale(float scale) {
      mScale = scale;
    }

    PointerList<Node> getNodes() const {
      return mNodes;
    }

    Node* getInputNode() const {
      return mInputNode;
    }

    Node* getOutputNode() const {
      return mOutputNode;
    }

    void sortGraph() {
      lockAudioThread();
      sortGraphWithoutLock();
      unlockAudioThread();
    }

  private:
    /**
     * Does some sorting on the mNodes list so the graph can be computed with fewer attempts
     * Does not touch the positions of the nodes
     */
    void sortGraphWithoutLock() {
      PointerList<Node> sorted;
      for (int i = 0; i < MAX_SOCKET_CONNECTIONS; i++) {
        // Put in the nodes which directly follow the input node
        NodeSocket* con = mInputNode->shared.socketsOut[0]->mConnectedTo[i];
        if (con != nullptr) {
          sorted.add(con->mParentNode);
        }
      }

      // Arbitrary depth
      for (int tries = 0; tries < 100; tries++) {
        for (int i = 0; i < sorted.size(); i++) {
          Node* node = sorted[i];
          for (int out = 0; out < node->shared.outputCount; out++) {
            NodeSocket* outSocket = node->shared.socketsOut[out];
            if (outSocket == nullptr) { continue; }
            for (int next = 0; next < MAX_SOCKET_CONNECTIONS; next++) {
              NodeSocket* nextSocket = outSocket->mConnectedTo[next];
              if (nextSocket == nullptr) { continue; }
              Node* nextNode = nextSocket->mParentNode;
              // Don't want to add duplicates or the output node
              if (sorted.find(nextNode) != -1) { continue; }
              sorted.add(nextNode);
            }
          }
        }
      }

      // Add in all the nodes which might not be connected or were missed because of the depth limit
      for (int i = 0; i < mNodes.size(); i++) {
        Node* nextNode = mNodes[i];
        if (sorted.find(nextNode) != -1) { continue; }
        sorted.add(nextNode);
      }

      mNodes.clear();
      for (int i = 0; i < sorted.size(); i++) {
        Node* n = sorted[i];
        if (n == mOutputNode) { continue; }
        bool dupli = false;
        for (int j = 0; j < mNodes.size(); j++) {
          if (mNodes[j] == n) {
            dupli = true;
          }
        }
        if (dupli) { continue; } // TODOG there shouldn't be any dupes, but it happens for some reason
        mNodes.add(n);
      }
    }
  };
}
