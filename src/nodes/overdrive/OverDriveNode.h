#pragma once
#include "OverDrive.h"
namespace guitard {
  class OverDriveNode final : public FaustGenerated::OverDrive {
  public:
    OverDriveNode(const std::string pType) {
      shared.type = pType;
    }

#ifndef GUITARD_HEADLESS
    void setupUi(iplug::igraphics::IGraphics* pGrahics) override {
      Node::setupUi(pGrahics);
      mUi->setColor(Theme::Categories::DISTORTION);
    }
#endif
  };
}