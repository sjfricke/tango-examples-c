#include "tango_data.h"

TangoData::TangoData():config_(nullptr){
  is_relocalized = false;
  is_learning_mode_enabled = false;
  for (int i = 0; i<4; i++) {
    current_timestamp_[i] = 0.0;
  }
}

// This callback function is called when new POSE updates become available.
static void onPoseAvailable(void* context, const TangoPoseData* pose) {
  int current_index = -1;
  // Set pose for device wrt start.
  if (pose->frame.base == TANGO_COORDINATE_FRAME_START_OF_SERVICE &&
      pose->frame.target == TANGO_COORDINATE_FRAME_DEVICE){
    current_index = 0;
  }
  
  // Set pose for device wrt ADF.
  else if (pose->frame.base == TANGO_COORDINATE_FRAME_AREA_DESCRIPTION &&
           pose->frame.target == TANGO_COORDINATE_FRAME_DEVICE) {
    current_index = 1;
    TangoData::GetInstance().is_relocalized = true;
  }
  // Set pose for start wrt ADF.
  else if (pose->frame.base == TANGO_COORDINATE_FRAME_START_OF_SERVICE &&
           pose->frame.target == TANGO_COORDINATE_FRAME_AREA_DESCRIPTION){
    current_index = 2;
  }
  // Set pose for ADF wrt start.
  else if (pose->frame.base == TANGO_COORDINATE_FRAME_AREA_DESCRIPTION &&
           pose->frame.target == TANGO_COORDINATE_FRAME_START_OF_SERVICE){
    current_index = 3;
  }
  else {
    return;
  }
  TangoData::GetInstance().tango_position_[current_index] =
    glm::vec3(pose->translation[0], pose->translation[1],
              pose->translation[2]);
  
  TangoData::GetInstance().tango_rotation_[current_index] =
    glm::quat(pose->orientation[3], pose->orientation[0],
              pose->orientation[1], pose->orientation[2]);
    
  TangoData::GetInstance().current_timestamp_[current_index] =
    pose->timestamp;
}

bool TangoData::Initialize() {
  // Initialize Tango Service.
  if (TangoService_initialize() != TANGO_SUCCESS) {
    LOGE("TangoService_initialize(): Failed");
    return false;
  }
  return true;
}

bool TangoData::SetConfig(int is_recording) {
  // Allocate a TangoConfig object.
  if ((config_ = TangoConfig_alloc()) == NULL) {
    LOGE("TangoService_allocConfig(): Failed");
    return false;
  }

  // Get the default TangoConfig.
  if (TangoService_getConfig(TANGO_CONFIG_DEFAULT, config_) != TANGO_SUCCESS) {
    LOGE("TangoService_getConfig(): Failed");
    return false;
  }

  // Define is recording or loading a map.
  if (is_recording) {
    is_learning_mode_enabled = true;
    if (TangoConfig_setBool(config_, "config_enable_learning_mode", true) != TANGO_SUCCESS) {
      LOGI("config_enable_learning_mode Failed");
      return false;
    }
  }
  else {
    is_learning_mode_enabled = false;
    UUID_list uuid_list;
    TangoService_getAreaDescriptionUUIDList(&uuid_list);
    if (TangoConfig_setString(config_, "config_load_area_description_UUID",
                              uuid_list.uuid[uuid_list.count-1].data) != TANGO_SUCCESS) {
      LOGI("config_load_area_description_uuid Failed");
      return false;
    }
    LOGI("Loaded map: %s", uuid_list.uuid[uuid_list.count-1].data);
    memcpy(uuid_, uuid_list.uuid[uuid_list.count-1].data, 5*sizeof(char));
  }
  
  // Set listening pairs. Connenct pose callback.
  TangoCoordinateFramePair pairs[4] =
  {
    {TANGO_COORDINATE_FRAME_START_OF_SERVICE, TANGO_COORDINATE_FRAME_DEVICE},
    {TANGO_COORDINATE_FRAME_AREA_DESCRIPTION, TANGO_COORDINATE_FRAME_DEVICE},
    {TANGO_COORDINATE_FRAME_AREA_DESCRIPTION, TANGO_COORDINATE_FRAME_START_OF_SERVICE},
    {TANGO_COORDINATE_FRAME_START_OF_SERVICE, TANGO_COORDINATE_FRAME_AREA_DESCRIPTION}
  };
  if (TangoService_connectOnPoseAvailable(4, pairs, onPoseAvailable) != TANGO_SUCCESS) {
    LOGI("TangoService_connectOnPoseAvailable(): Failed");
    return false;
  }
  return true;
}

bool TangoData::LockConfig() {
  // Lock in this configuration.
  if (TangoService_lockConfig(config_) != TANGO_SUCCESS) {
    LOGE("TangoService_lockConfig(): Failed");
    return false;
  }
  return true;
}

bool TangoData::UnlockConfig() {
  // Unlock current configuration.
  if (TangoService_unlockConfig() != TANGO_SUCCESS) {
    LOGE("TangoService_unlockConfig(): Failed");
    return false;
  }
  return true;
}

// Connect to Tango Service, service will start running, and
// POSE can be queried.
bool TangoData::Connect() {
  if (TangoService_connect(nullptr) != TANGO_SUCCESS) {
    LOGE("TangoService_connect(): Failed");
    return false;
  }
  LogAllUUIDs();
  return true;
}

bool TangoData::SaveADF(){
  UUID uuid;
  if (TangoService_saveAreaDescription(&uuid) != TANGO_SUCCESS) {
    LOGE("TangoService_saveAreaDescription(): Failed");
    return false;
  }
  LOGI("ADF Saved, uuid: %s", uuid.data);
}

void TangoData::RemoveAllAdfs(){
  LOGI("Removing all ADFs");
  UUID_list uuid_list;
  TangoService_getAreaDescriptionUUIDList(&uuid_list);
  if (&uuid_list != nullptr) {
    TangoService_destroyAreaDescriptionUUIDList(&uuid_list);
  }
}

void TangoData::Disconnect() {
  // Disconnect Tango Service.
  TangoService_disconnect();
}

void TangoData::LogAllUUIDs()
{
  UUID_list uuid_list;
  TangoService_getAreaDescriptionUUIDList(&uuid_list);
  LOGI("List of maps: ");
  for (int i = 0; i<uuid_list.count; i++) {
    LOGI("%d: %s", i, uuid_list.uuid[i].data);
  }
}
