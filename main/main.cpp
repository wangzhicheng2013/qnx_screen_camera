#include "camera_manager.hpp"
using namespace qnx_screen_camera;
int main() {
   if (false == G_CAMERA_MANAGER.init()) {
      return -1;
   }
   screen_attribute sAttr;
   sAttr.window_pos = {.3, .2};
   sAttr.window_size = {.6, .4};
   sAttr.display_id = 1;

   capture_attr cAttr;
   cAttr.input_id = QCARCAM_INPUT_TYPE_TOF_DEPTH;
   qcarcam_hndl_t handle = G_CAMERA_MANAGER.create_camera_connect(sAttr, cAttr);
   G_CAMERA_MANAGER.start_capture(handle);
   while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(10));
   }
   G_CAMERA_MANAGER.stop_capture(handle);

   return 0; 
}