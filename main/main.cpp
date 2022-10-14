#include "screen_window.hpp"
int main() {
   //auto ptr = std::make_shared<qnx_screen::screen_context>();
   qnx_screen::screen_window qnx_win(std::make_shared<qnx_screen::screen_context>());

   return 0; 
}