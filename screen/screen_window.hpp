#pragma once
#include <errno.h>
#include <memory>
#include <vector>
#include "screen_attribute.hpp"
#include "screen_context.hpp"
namespace qnx_screen_camera {
    struct window_buffer_attr {     // all buffers size is the same 
        int size[2] = { 0 };
        int num = 0;                // is same with qcarcam buffers
    };
    struct window_size_attr {
        int size[2] = { 0 };
        int pos[2] = { 0 };
        int source_size[2] = { 0 };
        int source_pos[2] = { 0 };
        inline void set(const screen_attribute &attr, const display_property *display_pro) {
            size[0] = attr.window_size.x * display_pro->size[0];     // Scale multiplied by screen size
            size[1] = attr.window_size.y * display_pro->size[1];

            pos[0] = attr.window_pos.x * display_pro->size[0];
            pos[1] = attr.window_pos.y * display_pro->size[1];

            source_size[0] = attr.window_source_size.x * attr.buffer_size.x;
            source_size[1] = attr.window_source_size.y * attr.buffer_size.y;

            LOG_D("win size:(%d,%d)w/h,pos:(%d,%d)x/y,attr size:(%f,%f)w/h,pos:(%f,%f)x/y",
                    size[0], size[1], pos[0], pos[1],
                    attr.window_size.x, attr.window_size.y, attr.window_pos.x, attr.window_pos.y);

            LOG_D("win source size:(%d,%d)w/h,pos:(%d,%d)x/y,attr size:(%f,%f)w/h,pos:(%f,%f)x/y",
                 source_size[0], source_size[1], source_pos[0], source_pos[1],
                 attr.window_source_size.x, attr.window_source_size.y, attr.window_source_pos.x, attr.window_source_pos.y);

            LOG_D("buffer size:(%d,%d)x/y", attr.buffer_size.x, attr.buffer_size.y);
        }
        inline void check(const screen_attribute &attr) {
            if ((source_size[0] <= 0) || (source_size[1] <= 0) ||
                (source_size[0] + source_pos[0] > attr.buffer_size.x) ||
                (source_size[1] + source_pos[1] > attr.buffer_size.y)) {
                source_size[0] = attr.buffer_size.x - source_pos[0];
                source_size[1] = attr.buffer_size.y - source_pos[1];
            }
        }
        inline void get(int *rect) {
            rect[0] = pos[0];
            rect[1] = pos[1];
            rect[2] = size[0];
            rect[3] = size[1];
        }
    };
    struct window_buffer_handle {
        void *mem_handle = nullptr;
        void *ptr[2] = { 0 };       // buffer address
        uint32_t size = 0;          // buffer size
        long long phys_addr = 0;
    };
    struct window_buffer {
        screen_buffer_t *screen_buffers = nullptr;
        int stride[2] = { 0 };
        int offset[3] = { 0 };
        std::vector<window_buffer_handle>handles;
        int buffer_size[2] = { 0 };
    };
    class screen_window : public std::enable_shared_from_this<screen_window> {
    public:
        screen_window(std::shared_ptr<screen_context> ptr) : screen_ctx_(ptr) {
        }
        bool init(const screen_attribute &attr) {
            if (false == screen_ctx_->init()) {
                return false;
            }
            int rc = screen_create_window(&win_ctx_, screen_ctx_->get_screen_ctx());
            if (rc) {
                LOG_E("screen_create_window error:%d", errno);
                return false;
            }
            int val = SCREEN_USAGE_WRITE | SCREEN_USAGE_VIDEO | SCREEN_USAGE_CAPTURE;
            rc = screen_set_window_property_iv(win_ctx_, SCREEN_PROPERTY_USAGE, &val);
            if (rc) {
                LOG_E("set SCREEN_PROPERTY_USAGE error!");
                return false;
            }
            display_id_ = attr.display_id;
            LOG_I("display id:%d", display_id_);
            if (display_id_ < 0) {
                LOG_E("display id error!");
                return false;
            }
            const display_property *display_pro = screen_ctx_->get_display_property(display_id_);   // find the screen
            if (nullptr == display_pro) {
               LOG_E("get display property for id:%d error!", display_id_);
               return false;
            }
            const screen_display_t *displays = screen_ctx_->get_displays();
            if (nullptr == displays) {
                LOG_E("get displays error!");
                return false;
            }
            rc = screen_set_window_property_pv(win_ctx_, SCREEN_PROPERTY_DISPLAY, (void **)&(displays[display_id_]));
            if (rc) {
                LOG_E("set SCREEN_PROPERTY_DISPLAY error:%d", errno);
                return false;
            }
            format_ = attr.format;
            window_size_attr windowSizeAttr;
            windowSizeAttr.set(attr, display_pro);
            /*display size*/
            if ((windowSizeAttr.size[0] <= 0) || (windowSizeAttr.size[1] <= 0)) {
                rc = screen_get_window_property_iv(win_ctx_, SCREEN_PROPERTY_SIZE, windowSizeAttr.size);
                if (rc) {
                    LOG_E("get SCREEN_PROPERTY_SIZE error:%d", errno);
                    return false;
                }
            }
            rc = screen_set_window_property_iv(win_ctx_, SCREEN_PROPERTY_SIZE, windowSizeAttr.size);
            if (rc) {
                LOG_E("set SCREEN_PROPERTY_SIZE error:%d", errno);
                return false;
            }
            /*display position*/
            if (windowSizeAttr.pos[0] != 0 || windowSizeAttr.pos[1] != 0) {
                rc = screen_set_window_property_iv(win_ctx_, SCREEN_PROPERTY_POSITION, windowSizeAttr.pos);
                if (rc) {
                    LOG_E("set SCREEN_PROPERTY_POSITION error:%d", errno);
                    return false;
                }
            }
            int zorder = attr.zorder;
            if (zorder != -1) {
                rc = screen_set_window_property_iv(win_ctx_, SCREEN_PROPERTY_ZORDER, &zorder);
                if (rc) {
                    LOG_E("set SCREEN_PROPERTY_ZORDER error:%d", errno);
                    return false;
                }
            }
            int visible = attr.visibility;
            rc = screen_set_window_property_iv(win_ctx_, SCREEN_PROPERTY_VISIBLE, &visible);
            if (rc) {
                LOG_E("set SCREEN_PROPERTY_VISIBLE error:%d", errno);
                return false;
            }
            windowSizeAttr.check(attr);
            rc = screen_set_window_property_iv(win_ctx_, SCREEN_PROPERTY_SOURCE_SIZE, windowSizeAttr.source_size);
            if (rc) {
                LOG_E("set SCREEN_PROPERTY_SOURCE_SIZE error%d", errno);
                return false;
            }
            if (windowSizeAttr.source_pos[0] != 0 || windowSizeAttr.source_pos[1] != 0) {
                rc = screen_set_window_property_iv(win_ctx_, SCREEN_PROPERTY_SOURCE_POSITION, windowSizeAttr.source_pos);
                if (rc) {
                    LOG_E("set SCREEN_PROPERTY_SOURCE_POSITION error%d", errno);
                    return false;
                }
            }
            windowSizeAttr.get(rect_);
            LOG_I("init window done. win rect:(%d,%d),(%d*%d)", rect_[0], rect_[1], rect_[2], rect_[3]);
            return true;
        }
        bool init_buffer(const window_buffer_attr &attr) {
            LOG_D("num buffer:%d,size(%d*%d)", attr.num, attr.size[0], attr.size[1]);
            int rc = screen_set_window_property_iv(win_ctx_, SCREEN_PROPERTY_FORMAT, &format_);
            if (rc) {
                LOG_E("set SCREEN_PROPERTY_FORMAT error:%d", errno);
                return false;
            }
            rc = screen_set_window_property_iv(win_ctx_, SCREEN_PROPERTY_BUFFER_SIZE, attr.size);
            if (rc) {
                LOG_E("set SCREEN_PROPERTY_BUFFER_SIZE error:%d", errno);
                return false;
            }
            rc = screen_create_window_buffers(win_ctx_, attr.num);
            if (rc) {
                LOG_E("screen_create_window_buffers error:%d", errno);
                return false;
            }
            int nPointers = 0;
            rc = screen_get_window_property_iv(win_ctx_, SCREEN_PROPERTY_RENDER_BUFFER_COUNT, &nPointers);
            if (rc) {
                LOG_E("get SCREEN_PROPERTY_RENDER_BUFFER_COUNT error:%d", errno);
                return false;
            }
            win_buf_.buffer_size[0] = attr.size[0];
            win_buf_.buffer_size[1] = attr.size[1];
            win_buf_.screen_buffers = (screen_buffer_t *)calloc(nPointers, sizeof(screen_buffer_t));
            if (nullptr == win_buf_.screen_buffers) {
                LOG_E("calloc error!");
                return false;
            }
            win_buf_.handles.resize(nPointers);
            rc = screen_get_window_property_pv(win_ctx_, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)win_buf_.screen_buffers);
            if (rc) {
                LOG_E("get SCREEN_PROPERTY_RENDER_BUFFERS error:%d", errno);
                return false;
            }
            rc = screen_get_buffer_property_iv(win_buf_.screen_buffers[0], SCREEN_PROPERTY_STRIDE, &win_buf_.stride[0]);
            if (rc) {
                LOG_E("get SCREEN_PROPERTY_STRIDE error:%d", errno);
                return false;
            }
            LOG_I("stride[0]: %d,stride[1]: %d", win_buf_.stride[0], win_buf_.stride[1]);
            // offset for each plane from start of buffer
            rc = screen_get_buffer_property_iv(win_buf_.screen_buffers[0], SCREEN_PROPERTY_PLANAR_OFFSETS, &win_buf_.offset[0]);
            if (rc) {
                LOG_E("get SCREEN_PROPERTY_PLANAR_OFFSETS error:%d", errno);
                return false;
            }
            LOG_I("offset[0]:%d,offset[1]:%d,offset[2]:%d", win_buf_.offset[0], win_buf_.offset[1], win_buf_.offset[2]);
            for (int i = 0;i < nPointers;i++) {
                rc = screen_get_buffer_property_pv(win_buf_.screen_buffers[i], SCREEN_PROPERTY_EGL_HANDLE, &win_buf_.handles[i].mem_handle);
                if (rc) {
                    LOG_E("get SCREEN_PROPERTY_EGL_HANDLE error:%d", errno);
                    return false;
                }
                // obtain the pointer of the buffers, for the capture use
                rc = screen_get_buffer_property_pv(win_buf_.screen_buffers[i], SCREEN_PROPERTY_POINTER, &win_buf_.handles[i].ptr[0]);
                if (rc) {
                    LOG_E("get SCREEN_PROPERTY_POINTER error:%d", errno);
                    return false;
                }
                rc = screen_get_buffer_property_llv(win_buf_.screen_buffers[i], SCREEN_PROPERTY_PHYSICAL_ADDRESS, &win_buf_.handles[i].phys_addr);
                if (rc) {
                    LOG_E("get SCREEN_PROPERTY_PHYSICAL_ADDRESS error:%d", errno);
                    return false;
                }
                win_buf_.handles[i].size = win_buf_.stride[0] * attr.size[1];
                LOG_I("buffer %d egl handle:0x%p, point:0x%p pyhaddr:0x%p size:%d", i, win_buf_.handles[i].mem_handle,
                        win_buf_.handles[i].ptr[0], win_buf_.handles[i].phys_addr, win_buf_.handles[i].size);
            }
            return true;
        }
        void handle_new_buffer(int idx) {
            if (idx >= win_buf_.handles.size()) {
                return;
            }
            int rc = screen_post_window(win_ctx_, win_buf_.screen_buffers[idx], 1, rect_, SCREEN_WAIT_IDLE);
            if (rc) {
                LOG_E("screen_post_window error:%d", errno);
            }
        }
        inline void get_yuv_buffer(int idx, uint8_t **data) {
            if (idx < win_buf_.handles.size()) {
                *data = (uint8_t *)win_buf_.handles[idx].ptr[0];
            }
            *data = nullptr;
        }
        inline void get_buffer_size(uint32_t& width, uint32_t& height) {
            width = win_buf_.buffer_size[0];
            height = win_buf_.buffer_size[1];
        }
        bool change_win_attr(DVECT &size, DVECT &pos) {
            const display_property *display_pro = screen_ctx_->get_display_property(display_id_);
            if (nullptr == display_pro) {
                LOG_E("can not get display property!");
                return false;
            }
            window_size_attr windowProperty;
            windowProperty.size[0] = size.x * display_pro->size[0];
            windowProperty.size[1] = size.y * display_pro->size[1];
            windowProperty.pos[0] = pos.x * display_pro->size[0];
            windowProperty.pos[1] = pos.y * display_pro->size[1];
            int rc = screen_set_window_property_iv(win_ctx_, SCREEN_PROPERTY_SIZE, windowProperty.size);
            if (rc) {
                LOG_E("set SCREEN_PROPERTY_SIZE error:%d", errno);
                return false;
            }
            rc = screen_set_window_property_iv(win_ctx_, SCREEN_PROPERTY_POSITION, windowProperty.pos);
            if (rc) {
                LOG_E("set SCREEN_PROPERTY_POSITION error:%d", errno);
                return false;
            }
            screen_flush_context(screen_ctx_->get_screen_ctx(), SCREEN_WAIT_IDLE);
            return true;
        }
        bool set_visible(int visible) {
            int rc = screen_set_window_property_iv(win_ctx_, SCREEN_PROPERTY_VISIBLE, &visible);
            if (rc)
            {
                LOG_E("set SCREEN_PROPERTY_VISIBLE error:%d", errno);
                return false;
            }
            screen_flush_context(screen_ctx_->get_screen_ctx(), SCREEN_WAIT_IDLE);
            return true;
        }
        const window_buffer &get_win_buf() const {
            return win_buf_;
        }
        virtual ~screen_window() {
            destroy();
        }
        void destroy() {
            if (win_buf_.screen_buffers != nullptr) {
                free(win_buf_.screen_buffers);
                win_buf_.screen_buffers = nullptr;
            }
            if (win_ctx_ != nullptr) {
                screen_destroy_window(win_ctx_);
                win_ctx_ = nullptr;
            }
        }
        std::shared_ptr<screen_window> get_window_ptr() {
            return shared_from_this();
        }
    private:
        int format_ = -1;
        window_buffer win_buf_;
        screen_window_t win_ctx_ = nullptr;
        std::shared_ptr<screen_context>screen_ctx_;
        int rect_[4] = { 0 };
        int display_id_ = -1;
    };
}