#pragma once
#include <screen/screen.h>
namespace qnx_screen_camera {
    template <typename T>
    struct vect {
        T x;
        T y;
    };
    using IVECT = vect<int>;
    using DVECT = vect<double>;
    struct screen_attribute {
        DVECT       window_size = { 1.0, 1.0 };                 // < Output window size [width, height] ratio
        DVECT       window_pos = { 0, 0 };                      // < Output window position [x, y] ratio
        DVECT       window_source_size = { 1.0, 1.0 };          // < Source window size [width, height] ratio
        DVECT       window_source_pos = { 0, 0 };               // < Source window position [x, y]
        IVECT       buffer_size = { 0, 0 };                     // < buffer  size
        int         display_id = -1;                            // < Specific display output ID
        int         zorder = -1;                                // < Window position in Z plane
        int         visibility = 1;                             // < Window visibility (0 - not visible, else visible)
        int         format = SCREEN_FORMAT_UYVY;                // < Displayable format if need to convert
        int         num_buffers_display = 5;                    // < Number of buffers if we need to convert to
        int         input_id = -1;                              // < QCamera input id
    };
}