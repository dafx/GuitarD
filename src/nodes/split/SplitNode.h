#pragma once
#include "Split.h"

namespace guitard {
  class SplitNode final : public FaustGenerated::Split {
  public:
    SplitNode(const std::string pType) {
      shared.type = pType;
    }

    void setupUi(iplug::igraphics::IGraphics* pGrahics) override {
      Node::setupUi(pGrahics);
      mUi->setColor(Theme::Categories::TOOLS);
    }
  };
}