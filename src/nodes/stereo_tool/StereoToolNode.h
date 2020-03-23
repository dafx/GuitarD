#pragma once
#include "../../main/faust/generated/StereoTool.h"

namespace guitard {
  class StereoToolNode final : public FaustGenerated::StereoTool {
  public:
    StereoToolNode(NodeList::NodeInfo* info) {
      mInfo = info;
    }
  };

  GUITARD_REGISTER_NODE(StereoToolNode,
    "Stereo Tool", "Tools", "Allows panning and altering the stereo width"
  )

//#ifndef GUITARD_HEADLESS
//    void setupUi(iplug::igraphics::IGraphics* pGrahics) override {
//      //background = new NodeBackground(pGrahics, PNGSTEREOSHAPERBG_FN, L, T,
//      //  [&](float x, float y) {
//      //    this->translate(x, y);
//      //  });
//      //pGrahics->AttachControl(background);
//      //parameters[0]->x = 80;
//      //parameters[0]->y = 80;
//      //parameters[1]->x = 125;
//      //parameters[1]->y = 140;
//      //parameters[1]->w = 40;
//      //parameters[1]->h = 40;
//      //parameters[2]->x = 175;
//      //parameters[2]->y = 80;
//      Node::setupUi(pGrahics);
//      mUi->setColor(Theme::Categories::TOOLS);
//    }
//#endif
}