#pragma once
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include "qcarcam.h"
#include "qcarcam_types.h"
#include "screen_window.hpp"
namespace qnx_screen_camera {
    struct capture_attr {
        std::string name;
        qcarcam_input_desc_t input_id = QCARCAM_INPUT_MAX;
        qcarcam_color_fmt_t format = QCARCAM_FMT_MAX;
        int width = -1;                  // < Output buffer width
        int height = -1;                 // < Output buffer height
        int num_buffers = 5;             // < Number of buffers for output of ISP
    };
    enum camera_state {
        CAM_STATE_ERROR = -1,
        CAM_STATE_START,
        CAM_STATE_STOP,
        CAM_STATE_INIT,
        CAM_STATE_OPEN
    };
    enum camera_frame_param : int {
        CAM_FRAME_TIMEOUT = 500000000,
    };
    enum camera_ctl_command {
        CAM_CMD_START = 0,
        CAM_CMD_STOP,
    };
    class camera_controller
    {
    public:
        explicit camera_controller(const capture_attr& attr) : attr_(attr) {}
        virtual ~camera_controller() {
            if (cap_thread_ != nullptr) {
                if (cap_thread_->joinable()) {
                    cap_thread_->join();
                }
                delete cap_thread_;
                cap_thread_ = nullptr;
            }
            if (cap_buf_ != nullptr) {
                if (cap_buf_->buffers != nullptr) {
                    free(cap_buf_->buffers);
                    cap_buf_->buffers = nullptr;
                }
                delete cap_buf_;
                cap_buf_ = nullptr;
            }
            if (qcarcam_ctx_) {
                qcarcam_close(qcarcam_ctx_);
                qcarcam_ctx_ = nullptr;
            }
        }
        bool create_window(const screen_attribute &screenAttr) {
            win_ptr_ = std::make_shared<screen_window>(std::make_shared<screen_context>());
            return win_ptr_ && win_ptr_->init(screenAttr);
        }
        qcarcam_hndl_t init(screen_attribute &screenAttr, void *eventCallback) {
            screenAttr.buffer_size = { attr_.width, attr_.height };
            screenAttr.format = SCREEN_FORMAT_UYVY;
            LOG_D("creat window buffer: %d, size:%d*%d, format:%d", attr_.num_buffers, attr_.width, attr_.height, attr_.format);
            if (false == create_window(screenAttr)) {
                return nullptr;
            }
            window_buffer_attr bufferAttr;
            bufferAttr.num = attr_.num_buffers + 1;
            bufferAttr.size[0] = attr_.width;
            bufferAttr.size[1] = attr_.height;
            if (!win_ptr_->init_buffer(bufferAttr)) {
                LOG_E("win ptr init buffer error!");
                return nullptr;
            }
            cap_buf_ = new qcarcam_buffers_t;
            if (nullptr == cap_buf_) {
                LOG_E("new qcarcam_buffers_t error!");
                return nullptr;
            }
            cap_buf_->n_buffers = attr_.num_buffers;
            cap_buf_->color_fmt = attr_.format;
            cap_buf_->flags |= QCARCAM_BUFFER_FLAG_OS_HNDL;
            cap_buf_->buffers = (qcarcam_buffer_t*)calloc(cap_buf_->n_buffers, sizeof(qcarcam_buffer_t));
            if (nullptr == cap_buf_->buffers) {
                LOG_E("calloc mem error!");
                return nullptr;
            }
            memset(cap_buf_->buffers, 0, cap_buf_->n_buffers * sizeof(qcarcam_buffers_t));
            for (unsigned int i = 0;i < cap_buf_->n_buffers;++i) {
                cap_buf_->buffers[i].n_planes = 1;
                cap_buf_->buffers[i].planes[0].width = attr_.width;
                cap_buf_->buffers[i].planes[0].height = attr_.height;
            }
            auto& buffer = win_ptr_->get_win_buf();
            for (unsigned int i = 0;i < buffer.handles.size();++i) {
                cap_buf_->buffers[i].planes[0].p_buf = buffer.handles[i].mem_handle;
                cap_buf_->buffers[i].planes[0].stride = buffer.stride[0];
                cap_buf_->buffers[i].planes[0].size = buffer.stride[0] * cap_buf_->buffers[i].planes[0].height;
                LOG_D("[%d] %p %dx%d %d, size:%d", i, cap_buf_->buffers[i].planes[0].p_buf,
                               cap_buf_->buffers[i].planes[0].width, cap_buf_->buffers[i].planes[0].height,
                               cap_buf_->buffers[i].planes[0].stride, cap_buf_->buffers[i].planes[0].size);
            }
            if (!setup_capture(eventCallback)) {
                LOG_E("setupCapture failed!");
                return nullptr;
            }
            return qcarcam_ctx_;
    }
    bool setup_capture(void *eventCallback) {
        qcarcam_ctx_ = qcarcam_open(attr_.input_id);
        LOG_I("open camera id:%d", attr_.input_id);
        if (nullptr == qcarcam_ctx_) {
            LOG_E("qcarcam_open failed!");
            return false;
        }
        int ret = qcarcam_s_buffers(qcarcam_ctx_, cap_buf_);
        if (ret != QCARCAM_RET_OK) {
            LOG_E("associate buffer failed! %d nbuffer: %u", ret, cap_buf_->n_buffers);
            return false;
        }
        qcarcam_param_value_t param;
        param.ptr_value = eventCallback;
        ret = qcarcam_s_param(qcarcam_ctx_, QCARCAM_PARAM_EVENT_CB, &param);
        if (ret != QCARCAM_RET_OK) {
            LOG_E("qcarcam_s_param setcb failed!");
            return false;
        }
        param.uint_value = QCARCAM_EVENT_FRAME_READY | QCARCAM_EVENT_INPUT_SIGNAL | QCARCAM_EVENT_ERROR;
        ret = qcarcam_s_param(qcarcam_ctx_, QCARCAM_PARAM_EVENT_MASK, &param);
        if (ret != QCARCAM_RET_OK) {
            LOG_E("qcarcam_s_param setmask failed!");
            return false;
        }
        camera_state_ = CAM_STATE_OPEN;
        return true;
    }
    void notify_new_frame() {
        frame_cv_.notify_all();
    }
    void handle_new_frame() {
        uint32_t width = 0;
        uint32_t height = 0;
        win_ptr_->get_buffer_size(width, height);
        uint8_t *yuvBuffer = NULL;
        while (true) {
            std::unique_lock<std::mutex> lk(frame_mutex_);
            frame_cv_.wait(lk);
            if (!keep_running_) {
                break;
            }
            qcarcam_ret_t ret;
            qcarcam_frame_info_t frameInfo;
            ret = qcarcam_get_frame(qcarcam_ctx_, &frameInfo, CAM_FRAME_TIMEOUT, 0);
            if (QCARCAM_RET_TIMEOUT == ret) {
                LOG_E("get frame timeout!");
                continue;
            }
            if (QCARCAM_RET_OK != ret) {
                LOG_E("get frame failed %d", ret);
                continue;
            }
            if (frameInfo.idx >= attr_.num_buffers) {
                continue;
            }
            LOG_D("=== get frame num: %d", frameInfo.seq_no);
            LOG_D("=== get frame timestamp: %llu", frameInfo.timestamp);
            LOG_D("=== get frame width: %d, height: %d", width, height);
            win_ptr_->get_yuv_buffer(frameInfo.idx, &yuvBuffer);
            win_ptr_->handle_new_buffer(frameInfo.idx);
            if (pre_buffer_idx_ != -1 && pre_buffer_idx_ < attr_.num_buffers) {
                ret = qcarcam_release_frame(qcarcam_ctx_, pre_buffer_idx_);
                if (QCARCAM_RET_OK != ret) {
                    LOG_E("release frame failed %d", ret);
                    pre_buffer_idx_  = frameInfo.idx;
                    continue;
                }
            }
            pre_buffer_idx_ = frameInfo.idx;
        }
        yuvBuffer = NULL;
    }
    void handle_signals(qcarcam_input_signal_t signalType) {
        switch (signalType) {
        case QCARCAM_INPUT_SIGNAL_VALID:
            start_capture();
            break;
        case QCARCAM_INPUT_SIGNAL_LOST: 
            break;
        }
    }
    bool start_capture() {
        if (CAM_STATE_START == camera_state_) {
            return true;
        }
        if (CAM_STATE_INIT == camera_state_) {
            return false;
        }
        keep_running_ = true;
        if (nullptr == cap_thread_) {
            cap_thread_ = new std::thread([&]() {
                this->handle_new_frame();
            });
        }
        qcarcam_ret_t ret = qcarcam_start(qcarcam_ctx_);
        if (ret != QCARCAM_RET_OK) {
            LOG_E("start capture failed %d", ret);
            return false;
        }
        camera_state_ = CAM_STATE_START;
        win_ptr_->set_visible(1);
        return true;
    }
    bool control_command(int command) {
        std::lock_guard<std::mutex> guard(control_mutex_);
        bool ret = true;
        LOG_I("controlCamera: %d", command);
        switch (command) {
            case CAM_CMD_START:
                start_capture();
                win_ptr_->set_visible(1);
                break;
            case CAM_CMD_STOP:
                stop_capture();
                win_ptr_->set_visible(0);
                break;
            default: 
                break;
        }
        return ret;
    }
    bool stop_capture() {
        if (CAM_STATE_STOP == camera_state_) {
            return true;
        }
        if (CAM_STATE_INIT == camera_state_) {
            return false;
        }
        keep_running_ = false;
        frame_cv_.notify_all();
        if (cap_thread_ != nullptr){
            if (cap_thread_->joinable()) {
                cap_thread_->join();
            }
            delete cap_thread_;
            cap_thread_ = nullptr;
        }
        win_ptr_->set_visible(0);
        qcarcam_ret_t ret = qcarcam_stop(qcarcam_ctx_);
        if (ret != QCARCAM_RET_OK) {
            LOG_E("start capture failed %d", ret);
            return false;
        }
        camera_state_ = CAM_STATE_STOP;
        return true;
    }
    private:
        capture_attr attr_;
        std::shared_ptr<screen_window>win_ptr_;
        qcarcam_buffers_t *cap_buf_ = nullptr;
        qcarcam_hndl_t qcarcam_ctx_ = nullptr;
        camera_state camera_state_ = CAM_STATE_INIT;
        std::condition_variable frame_cv_;
        std::mutex frame_mutex_;
        std::mutex control_mutex_;
        std::thread* cap_thread_ = nullptr;
        std::atomic<bool>keep_running_;
        int pre_buffer_idx_ = -1;
        unsigned int frame_num_ = 0;
    };
}