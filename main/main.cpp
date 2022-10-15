#include "screen_window.hpp"
#include "camera_controller.hpp"
int main() {
   //auto ptr = std::make_shared<qnx_screen::screen_context>();
   qnx_screen_camera::screen_window qnx_win(std::make_shared<qnx_screen_camera::screen_context>());

   return 0; 
}