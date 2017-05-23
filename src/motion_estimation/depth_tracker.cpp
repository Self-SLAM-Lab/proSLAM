#include "depth_tracker.h"

namespace proslam {
  using namespace srrg_core;

  //ds the tracker assumes a constant stereo camera configuration
  DepthTracker::DepthTracker(): _depth_camera(0),
                                _depth_image(0),
                                _depth_framepoint_generator(0),
                                _parameters(0) {
    std::cerr << "DepthTracker::DepthTracker|constructed" << std::endl;
  }

  void DepthTracker::configure(BaseTrackerParameters* parameters_) {
    std::cerr << "DepthTracker::setup|configuring" << std::endl;
    _parameters = dynamic_cast<DepthTrackerParameters*>(parameters_);
    BaseTracker::configure(parameters_);
    assert(_depth_camera);
    _depth_framepoint_generator = dynamic_cast<DepthFramePointGenerator*>(_framepoint_generator);
    assert(_depth_framepoint_generator);
    std::cerr << "DepthTracker::setup|configured" << std::endl;
  }

  //ds dynamic cleanup
  DepthTracker::~DepthTracker() {
    std::cerr << "DepthTracker::DepthTracker|destroying" << std::endl;
    std::cerr << "DepthTracker::DepthTracker|destroyed" << std::endl;
  }

  Frame* DepthTracker::_createFrame(){
    Frame* current_frame = _context->createFrame(_context->robotToWorld(), _framepoint_generator->maximumDepthNearMeters());
    current_frame->setCameraLeft(_camera_left);
    current_frame->setIntensityImageLeft(_intensity_image_left);
    current_frame->setCameraRight(_depth_camera);
    current_frame->setIntensityImageRight(_depth_image);
    return current_frame;
  }
  
  //ds creates a new Frame for the given images, retrieves the correspondences relative to the previous Frame, optimizes the current frame pose and updates landmarks
  void DepthTracker::compute() {
    assert(_depth_image);
    BaseTracker::compute();
  }

  //ds attempts to recover framepoints in the current image using the more precise pose estimate, retrieved after pose optimization
  void DepthTracker::_recoverPoints(Frame* current_frame_) {
    return;
  }
}
