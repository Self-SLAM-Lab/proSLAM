#pragma once
#include "parameters.h"
#include "camera.h"
#include "frame_point.h"

namespace proslam {
  
  //ds forward declarations
  class LocalMap;
  class WorldMap;

  //ds this class encapsulates all data gained from the processing of a stereo image pair
  class Frame {

  //ds exported types
  public: EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    //ds defines one of the two tracker states
    enum Status {Localizing, Tracking};

  //ds object handling
  protected:

    //ds frame construction in the WorldMap context
    Frame(const WorldMap* context_,
          Frame* previous_,
          Frame* next_,
          const TransformMatrix3D& robot_to_world_,
          const real& maximum_depth_near_);

    //ds FramePoints cleanup
    ~Frame();

    //ds prohibit default construction
    Frame() = delete;

  //ds getters/setters
  public:

    //ds unique identifier for a frame instance (exists once in memory)
    const Identifier& identifier() const {return _identifier;}

    inline const Frame* root() const {return _root;}
    inline void setRoot(const Frame* root_) {_root = root_;}

    inline Frame* previous() {return _previous;}
    inline const Frame* previous() const {return _previous;}
    void setPrevious(Frame* previous_) {_previous = previous_;}

    inline Frame* next()  {return _next;}
    inline const Frame* next() const {return _next;}
    void setNext(Frame* next_) {_next = next_;}

    inline std::vector<cv::KeyPoint>& keypointsLeft() {return _keypoints_left;}
    inline std::vector<cv::KeyPoint>& keypointsRight() {return _keypoints_right;}
    inline cv::Mat& descriptorsLeft() {return _descriptors_left;}
    inline cv::Mat& descriptorsRight() {return _descriptors_right;}

    inline const Camera* cameraLeft() const {return _camera_left;}
    void setCameraLeft(const Camera* camera_) {_camera_left = camera_;}

    inline const Camera* cameraRight() const {return _camera_right;}
    void setCameraRight(const Camera* camera_) {_camera_right = camera_;}

    inline const TransformMatrix3D& robotToWorld() const {return _robot_to_world;}
    void setRobotToWorld(const TransformMatrix3D& robot_to_world_);
    inline const TransformMatrix3D& worldToRobot() const {return _world_to_robot;}
    
    inline const TransformMatrix3D& frameToLocalMap() const {return _frame_to_local_map;}
    inline const TransformMatrix3D& localMapToFrame() const {return _local_map_to_frame;}

    //ds visualization only
    void setRobotToWorldGroundTruth(const TransformMatrix3D& robot_to_world_ground_truth_) {_robot_to_world_ground_truth = robot_to_world_ground_truth_;}
    const TransformMatrix3D& robotToWorldGroundTruth() const {return _robot_to_world_ground_truth;}

    inline const FramePointPointerVector& points() const {return _active_points;}
    inline FramePointPointerVector& points() {return _active_points;}

    //ds request a new framepoint instance with an optional link to a previous point (track)
    FramePoint* createFramepoint(const cv::KeyPoint& keypoint_left_,
                       const cv::Mat& descriptor_left_,
                       const cv::KeyPoint& keypoint_right_,
                       const cv::Mat& descriptor_right_,
                       const PointCoordinates& camera_coordinates_left_,
                       FramePoint* previous_point_ = 0);

    inline const IntensityImage& intensityImageLeft() const {return *_intensity_image_left;}
    void setIntensityImageLeft(const IntensityImage* intensity_image_)  {_intensity_image_left = intensity_image_;}

    inline const IntensityImage& intensityImageRight() const {return *_intensity_image_right;}
    void setIntensityImageRight(const IntensityImage* intensity_image_)  {_intensity_image_right = intensity_image_;}

    inline const Status& status() const {return _status;}
    void setStatus(const Status& status_) {_status = status_;}

    //ds the maximum allowed depth for framepoints to become classified as near - everything above is far
    inline const real maximumDepthNear() const {return _maximum_depth_near;}

    void setLocalMap(LocalMap* local_map_) {_local_map = local_map_;}
    inline LocalMap* localMap() {return _local_map;}
    inline const LocalMap* localMap() const {return _local_map;}
    void setFrameToLocalMap(const TransformMatrix3D& frame_to_local_map_) {_frame_to_local_map = frame_to_local_map_; _local_map_to_frame = _frame_to_local_map.inverse();}
    void setIsKeyframe(const bool& is_keyframe_) {_is_keyframe = is_keyframe_;}
    inline const bool isKeyframe() const {return _is_keyframe;}

    //ds get a quick overview of the overall point status in the frame
    const Count countPoints(const Count& min_track_length_,
		                        const ThreeValued& has_landmark_ = Unknown) const;

    //ds free all point instances
    void clear();

    //ds update framepoint world coordinates
    void updateActivePoints();

  //ds attributes
  protected:

    //ds unique identifier for a landmark (exists once in memory)
    const Identifier _identifier;

    //ds tracker status at the time of creation of this instance
    Status _status = Localizing;

    //ds links to preceding and subsequent instances
    Frame* _previous = 0;
    Frame* _next     = 0;

    //! @brief detected keypoints at the time of creation of the Frame
    std::vector<cv::KeyPoint> _keypoints_left;
    std::vector<cv::KeyPoint> _keypoints_right;

    //! @brief extracted descriptors associated to the keypoints at the time of creation of the Frame
    cv::Mat _descriptors_left;
    cv::Mat _descriptors_right;

    //! @brief bookkeeping: all created framepoints for this frame (create function)
    FramePointPointerVector _created_points;

    //! @brief bookkeeping: active (used) framepoints in the pipeline (a subset of _created_points)
    FramePointPointerVector _active_points;

    //ds spatials
    TransformMatrix3D _frame_to_local_map = TransformMatrix3D::Identity();
    TransformMatrix3D _local_map_to_frame = TransformMatrix3D::Identity();
    TransformMatrix3D _robot_to_world     = TransformMatrix3D::Identity();
    TransformMatrix3D _world_to_robot     = TransformMatrix3D::Identity();

    //ds stereo camera configuration affiliated with this frame
    const Camera* _camera_left   = 0;
    const Camera* _camera_right  = 0;

    //ds to support arbitrary number of rgb/depth image combinations
    const IntensityImage* _intensity_image_left;
    const IntensityImage* _intensity_image_right;

    //ds the maximum allowed depth for framepoints to become classified as near - everything above is far
    const real _maximum_depth_near;

    //ds link to a local map if the frame is part of one
    LocalMap* _local_map;
    bool _is_keyframe  = false;

    //ds access
    friend class WorldMap;

    //ds visualization only
    TransformMatrix3D _robot_to_world_ground_truth = TransformMatrix3D::Identity();
    const Frame* _root;

    //ds class specific
    private:

      //ds inner instance count - incremented upon constructor call (also unsuccessful calls)
      static Count _instances;
  };

  typedef std::vector<Frame*> FramePointerVector;
  typedef std::map<Identifier, Frame*> FramePointerMap;
  typedef std::pair<Identifier, Frame*> FramePointerMapElement;
}
