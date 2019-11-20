#pragma once
#include "SimpleDelay.h"

class SimpleDelayNode final : public SimpleDelay {
public:
  SimpleDelayNode(const std::string pType) {
    mType = pType;
  }

  void setupUi(iplug::igraphics::IGraphics* pGrahics) override {
    Node::setupUi(pGrahics);
    mUi->setColor(Theme::Categories::SPATIAL);
  }
};
