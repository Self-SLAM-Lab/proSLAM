#include "world_map.h"

#include <fstream>
#include <iomanip>

namespace proslam {
using namespace srrg_core;

WorldMap::WorldMap(const WorldMapParameters* parameters_): _parameters(parameters_) {
  LOG_INFO(std::cerr << "WorldMap::WorldMap|constructing" << std::endl)
  clear();
  LOG_INFO(std::cerr << "WorldMap::WorldMap|constructed" << std::endl)
}

WorldMap::~WorldMap() {
  LOG_INFO(std::cerr << "WorldMap::~WorldMap|destroying" << std::endl)
  clear();
  LOG_INFO(std::cerr << "WorldMap::~WorldMap|destroyed" << std::endl)
}

//ds clears all internal structures
void WorldMap::clear() {

  //ds free landmarks
  LOG_INFO(std::cerr << "WorldMap::clear|deleting landmarks: " << _landmarks.size() << std::endl)
  for(LandmarkPointerMap::iterator it = _landmarks.begin(); it != _landmarks.end(); ++it) {
    delete it->second;
  }

  //ds free all frames
  LOG_INFO(std::cerr << "WorldMap::clear|deleting frames: " << _frames.size() << std::endl)
  for(FramePointerMap::iterator it = _frames.begin(); it != _frames.end(); ++it) {
    delete it->second;
  }

  //ds free all local maps
  LOG_INFO(std::cerr << "WorldMap::clear|deleting local maps: " << _local_maps.size() << std::endl)
  for(const LocalMap* local_map: _local_maps) {
    delete local_map;
  }

  //ds clear containers
  _frame_queue_for_local_map.clear();
  _landmarks.clear();
  _frames.clear();
  _local_maps.clear();
  _currently_tracked_landmarks.clear();
}

Frame* WorldMap::createFrame(const TransformMatrix3D& robot_to_world_,
                             const double& timestamp_image_left_seconds_){

  //ds update current frame
  _previous_frame = _current_frame;
  _current_frame  = new Frame(this, _previous_frame, 0, robot_to_world_, timestamp_image_left_seconds_);

  //ds check if the frame has a predecessor
  if (_previous_frame) {
    _previous_frame->setNext(_current_frame);
  } else {

    //ds we have a new root frame
    _root_frame = _current_frame; // 아마 시작 프레임 얘기하는듯? (Start라고 하지 왜 root라고)
    _current_frame ->setRoot(_root_frame);
  }

  //ds bookkeeping
  _frames.insert(std::make_pair(_current_frame->identifier(), _current_frame));
  _frame_queue_for_local_map.push_back(_current_frame);

  //ds done
  return _current_frame;
}

Landmark* WorldMap::createLandmark(FramePoint* origin_) {
  Landmark* landmark = new Landmark(origin_, _parameters->landmark);
  _landmarks.insert(std::make_pair(landmark->identifier(), landmark));
  return landmark;
}

const bool WorldMap::createLocalMap(const bool& drop_framepoints_) {
  if (_previous_frame == 0) {
    return false;
  }

  //ds reset closure status
  _relocalized = false;

  //ds update distance traveled and last pose
  const TransformMatrix3D robot_pose_last_to_current = _previous_frame->worldToRobot()*_current_frame->robotToWorld();
  _distance_traveled_window += robot_pose_last_to_current.translation().norm();
  _degrees_rotated_window   += toOrientationRodrigues(robot_pose_last_to_current.linear()).norm();

  //ds check if we can generate a keyframe - if generated by translation only a minimum number of frames in the buffer is required - or a new tracking context
  if (_degrees_rotated_window   > _parameters->minimum_degrees_rotated_for_local_map                                    ||
     (_distance_traveled_window > _parameters->minimum_distance_traveled_for_local_map       &&
      _frame_queue_for_local_map.size() > _parameters->minimum_number_of_frames_for_local_map)                          ||
     (_frame_queue_for_local_map.size() > _parameters->minimum_number_of_frames_for_local_map && _local_maps.size() < 5)) {

    //ds create the new keyframe and add it to the keyframe database
    _current_local_map = new LocalMap(_frame_queue_for_local_map,
                                      _parameters->local_map,
                                      _root_local_map,
                                      _current_local_map);
    _local_maps.push_back(_current_local_map);
    assert(_current_frame->isKeyframe());
    assert(_current_frame->localMap() == _current_local_map);

    //ds set local map root
    if (!_root_local_map) {
      _root_local_map = _current_local_map;
      _root_local_map->setRoot(_current_local_map);
    }

    //ds reset generation properties
    resetWindowForLocalMapCreation(drop_framepoints_);

    //ds local map generated
    return true;
  } else {

    //ds no local map generated
    return false;
  }
}

//ds resets the window for the local map generation
void WorldMap::resetWindowForLocalMapCreation(const bool& drop_framepoints_) {
  _distance_traveled_window = 0;
  _degrees_rotated_window   = 0;

  //ds free memory if desired (saves a lot of memory costs a little computation)
  if (drop_framepoints_) {

    //ds the last frame we'll need for the next tracking step
    _frame_queue_for_local_map.pop_back();

    //ds the pre-last frame is needed for visualization only (optical flow)
    _frame_queue_for_local_map.pop_back();

    //ds purge the rest
    for (Frame* frame: _frame_queue_for_local_map) {
      frame->clear();
    }
  }
  _frame_queue_for_local_map.clear();
}

void WorldMap::addLoopClosure(LocalMap* query_,
                              const LocalMap* reference_,
                              const TransformMatrix3D& query_to_reference_,
                              const Closure::CorrespondencePointerVector& landmark_correspondences_,
                              const real& information_) {

  //ds check if we relocalized after a lost track
  if (_frames.at(0)->root() != _current_frame->root()) {
    assert(_current_frame->localMap() == query_);

    //ds rudely link the current frame into the list (proper map merging will be coming soon!)
    setTrack(_current_frame);
  }

  //ds add loop closure information to the world map
  query_->addCorrespondence(reference_, query_to_reference_, landmark_correspondences_, information_);
  _relocalized = true;

  //ds informative only
  ++_number_of_closures;
}

void WorldMap::writeTrajectoryKITTI(const std::string& filename_) const {

  //ds construct filename
  std::string filename_kitti(filename_);

  //ds if not set
  if (filename_ == "") {

    //ds generate generic filename with timestamp
    filename_kitti = "trajectory_kitti-"+std::to_string(static_cast<uint64_t>(std::round(srrg_core::getTime())))+".txt";
  }

  //ds open file stream for kitti (overwriting)
  std::ofstream outfile_trajectory(filename_kitti, std::ifstream::out);
  assert(outfile_trajectory.good());
  outfile_trajectory << std::fixed;
  outfile_trajectory << std::setprecision(9);

  //ds for each frame (assuming continuous, sequential indexing)
  for (const FramePointerMapElement frame: _frames) {

    //ds buffer transform
    const TransformMatrix3D& robot_to_world = frame.second->robotToWorld();

    //ds dump transform according to KITTI format
    for (uint8_t u = 0; u < 3; ++u) {
      for (uint8_t v = 0; v < 4; ++v) {
        outfile_trajectory << robot_to_world(u,v) << " ";
      }
    }
    outfile_trajectory << "\n";
  }
  outfile_trajectory.close();
  LOG_INFO(std::cerr << "WorldMap::WorldMap|saved trajectory (KITTI format) to: " << filename_kitti << std::endl)
}

void WorldMap::writeTrajectoryTUM(const std::string& filename_) const {

  //ds construct filename
  std::string filename_tum(filename_);

  //ds if not set
  if (filename_ == "") {

    //ds generate generic filename with timestamp
    filename_tum = "trajectory_tum-"+std::to_string(static_cast<uint64_t>(std::round(srrg_core::getTime())))+".txt";
  }

  //ds open file stream for tum (overwriting)
  std::ofstream outfile_trajectory(filename_tum, std::ifstream::out);
  assert(outfile_trajectory.good());
  outfile_trajectory << std::fixed;
  outfile_trajectory << std::setprecision(9);

  //ds for each frame (assuming continuous, sequential indexing)
  for (const FramePointerMapElement frame: _frames) {

    //ds buffer transform
    const TransformMatrix3D& robot_to_world = frame.second->robotToWorld();
    const Quaternion orientation = Quaternion(robot_to_world.linear());

    //ds dump transform according to TUM format
    outfile_trajectory << frame.second->timestampImageLeftSeconds() << " ";
    outfile_trajectory << robot_to_world.translation().x() << " ";
    outfile_trajectory << robot_to_world.translation().y() << " ";
    outfile_trajectory << robot_to_world.translation().z() << " ";
    outfile_trajectory << orientation.x() << " ";
    outfile_trajectory << orientation.y() << " ";
    outfile_trajectory << orientation.z() << " ";
    outfile_trajectory << orientation.w() << " ";
    outfile_trajectory << "\n";
  }
  outfile_trajectory.close();
  LOG_INFO(std::cerr << "WorldMap::WorldMap|saved trajectory (TUM format) to: " << filename_tum << std::endl)
}

void WorldMap::breakTrack(Frame* frame_) {

  //ds if the track is not already broken
  if (_last_frame_before_track_break == 0)
  {
    _last_frame_before_track_break     = _previous_frame;
    _last_local_map_before_track_break = _current_local_map;
  }
  frame_->breakTrack();

  //ds purge previous and set new root - this will trigger a new start
  _previous_frame = 0;
  _root_frame     = frame_;
  _root_local_map = 0;

  //ds reset current head
  _currently_tracked_landmarks.clear();
  resetWindowForLocalMapCreation();
  setRobotToWorld(frame_->robotToWorld());
}

void WorldMap::setTrack(Frame* frame_) {
  assert(frame_->localMap());
  assert(_last_local_map_before_track_break);
  LOG_INFO(std::printf("WorldMap::setTrack|RELOCALIZED - connecting [Frame] < [LocalMap]: [%06lu] < [%06lu] with [%06lu] < [%06lu]\n",
              _last_frame_before_track_break->identifier(), _last_local_map_before_track_break->identifier(), frame_->identifier(), frame_->localMap()->identifier()))

  //ds return to original roots
  _root_frame = _last_frame_before_track_break->root();
  frame_->setRoot(_root_frame);
  _root_local_map = _last_local_map_before_track_break->root();
  frame_->localMap()->setRoot(_root_local_map);

  //ds connect the given frame to the last one before the track broke and vice versa
  _last_frame_before_track_break->setNext(frame_);
  frame_->setPrevious(frame_);

  //ds connect local maps
  _last_local_map_before_track_break->setNext(frame_->localMap());
  frame_->localMap()->setPrevious(_last_local_map_before_track_break);

  _last_frame_before_track_break     = 0;
  _last_local_map_before_track_break = 0;
}

void WorldMap::mergeLandmarks(const LocalMap::ClosureConstraintVector& closures_) {
  CHRONOMETER_START(landmark_merging)

  //ds keep track of the best merged references
  //ds we need to do this since we're processing multiple closures here,
  //ds which possibly contain different query-reference correspondences
  std::map<Identifier, std::pair<Identifier, Count>> landmark_queries_to_references_filtered;
  std::map<Identifier, std::pair<Identifier, Count>> landmark_references_to_queries_filtered;

  //ds determine landmark merge configuration
  for (const LocalMap::ClosureConstraint& closure: closures_) {
    for (const Closure::Correspondence* correspondence: closure.landmark_correspondences) {
      Identifier identifier_query     = correspondence->query->identifier();
      Identifier identifier_reference = correspondence->reference->identifier();

      //ds query is always greater than reference (merging into old landmarks)
      if (identifier_query < identifier_reference) {
        std::swap(identifier_query, identifier_reference);
      }

      //ds if the correspondence is valid and not a redundant merge for an already merged landmark
      if (correspondence->is_inlier && identifier_query != identifier_reference) {

        //ds potential correspondance candidates
        const Count& matching_count = correspondence->matching_count;
        std::pair<Identifier, Count> candidate_query(identifier_query, matching_count);
        std::pair<Identifier, Count> candidate_reference(identifier_reference, matching_count);

        //ds evaluate current situation for the proposed query-reference pair
        std::map<Identifier, std::pair<Identifier, Count>>::iterator iterator_query     = landmark_queries_to_references_filtered.find(identifier_query);
        std::map<Identifier, std::pair<Identifier, Count>>::iterator iterator_reference = landmark_references_to_queries_filtered.find(identifier_reference);

        //ds if there is not entry for the query nor the reference
        if (iterator_query == landmark_queries_to_references_filtered.end() &&
            iterator_reference == landmark_references_to_queries_filtered.end()) {

          //ds we add new entries
          landmark_queries_to_references_filtered.insert(std::make_pair(identifier_query, candidate_reference));
          landmark_references_to_queries_filtered.insert(std::make_pair(identifier_reference, candidate_query));
        }

        //ds if we have a new reference for an already added query
        else if (iterator_query != landmark_queries_to_references_filtered.end() &&
                 iterator_reference == landmark_references_to_queries_filtered.end()) {

          //ds check if the reference is better than the added one
          if (matching_count > iterator_query->second.second) {

            //ds remove previous entry
            landmark_references_to_queries_filtered.erase(iterator_query->second.first);

            //ds update entries
            iterator_query->second = candidate_query;
            landmark_references_to_queries_filtered.insert(std::make_pair(identifier_reference, candidate_query));
          }
        }

        //ds if we have a new query for and already added reference
        else if (iterator_query == landmark_queries_to_references_filtered.end() &&
                 iterator_reference != landmark_references_to_queries_filtered.end()) {

          //ds check if the query is better than the added one
          if (matching_count > iterator_reference->second.second) {

            //ds remove previous entry
            landmark_queries_to_references_filtered.erase(iterator_reference->second.first);

            //ds update entries
            iterator_reference->second = candidate_reference;
            landmark_queries_to_references_filtered.insert(std::make_pair(identifier_query, candidate_reference));
          }
        }
      }
    }
  }

  //ds map of merged landmark identfiers in case of multi-merges
  std::map<Identifier, Identifier> merged_landmark_identifiers;

  //ds for each entry: <query, reference>
  for (const std::pair<Identifier, std::pair<Identifier, Count>>& pair: landmark_queries_to_references_filtered) {
    Landmark* landmark_query     = nullptr;
    Landmark* landmark_reference = nullptr;

    //ds try to retrieve landmarks from map, ignoring queries that have been merged already
    try {
      landmark_query = _landmarks.at(pair.first);
    } catch (const std::out_of_range& /*ex*/) {

      //ds this means the query has already been merged, we skip further processing
      LOG_WARNING(std::cerr << "WorldMap::mergeLandmarks|already merged landmark ID: " << landmark_query->identifier() << std::endl)
      continue;
    }

    //ds check for reference landmark, route to absorbing one if the reference has been a query earlier
    try {
      landmark_reference = _landmarks.at(pair.second.first);
    } catch (const std::out_of_range& /*ex*/) {
      landmark_reference = _landmarks.at(merged_landmark_identifiers.at(pair.second.first));
    }

    //ds skip processing for identical calls
    if (landmark_query->identifier() == landmark_reference->identifier()) {
      continue;
    }

    //ds if the landmarks share a local map, we cannot merge them (this has to be done within the local map)
    //ds TODO perform merge and handle colliding framepoints
    bool shared_local_map = false;
    for (const LocalMap* local_map_query: landmark_query->_local_maps) {
      for (const LocalMap* local_map_reference: landmark_reference->_local_maps) {
        if (local_map_query == local_map_reference) {
          shared_local_map = true;
          break;
        }
      }
      if (shared_local_map) {
        break;
      }
    }

    //ds skip merging
    if (shared_local_map) {
      continue;
    }

    //ds check if the landmark is in the currently tracked ones and update it accordingly
    for (Index index = 0; index < _currently_tracked_landmarks.size(); ++index) {
      const Identifier& tracked_identifier = _currently_tracked_landmarks[index]->identifier();
      if (tracked_identifier == landmark_query->identifier()    ||
          tracked_identifier == landmark_reference->identifier()) {
        _currently_tracked_landmarks[index] = landmark_reference;
      }
    }

    //ds perform merge (does not free landmark memory)
    landmark_reference->merge(landmark_query);

    //ds update bookkeeping and free absorbed landmark
    merged_landmark_identifiers.insert(std::make_pair(landmark_query->identifier(), landmark_reference->identifier()));
    if (1 != _landmarks.erase(landmark_query->identifier())) {

      //ds do not free landmark memory
      LOG_WARNING(std::cerr << "WorldMap::mergeLandmarks|invalid erase of landmark ID: " << landmark_query->identifier() << std::endl)
    } else {

      //ds free landmark memory
      delete landmark_query;
    }
  }
  LOG_DEBUG(std::cerr << "WorldMap::mergeLandmarks|merged landmarks: " << merged_landmark_identifiers.size() << std::endl)
  _number_of_merged_landmarks += merged_landmark_identifiers.size();
  CHRONOMETER_STOP(landmark_merging)
}
}
