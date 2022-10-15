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
        qcarcam_input_desc_t inputId = QCARCAM_INPUT_MAX;
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
                delete cap_buf_;
                cap_buf_ = nullptr;
            }
            if (qcarcam_ctx_) {
                qcarcam_close(qcarcam_ctx_);
                qcarcam_ctx_ = nullptr;
            }
        }
        qcarcam_hndl_t init(screen_attribute &screenAttr, void *eventCallback) {
            screenAttr.buffer_size = { attr_.width, attr_.height };
            screenAttr.format = SCREEN_FORMAT_UYVY;
            LOG_D("creat window buffer: %d, size:%d*%d, format:%d",mAttr.nBuffers,mAttr.width,mAttr.height,mAttr.format);
        mWinPtr = QCameraManager::getInstance().GetScreenCtx()->creatWindow(screenAttr);
        if(mWinPtr == nullptr){
            LOGE("creat win ptr fail");
            return nullptr;
        }
        QScreen::QScreenWindow::WindowBufferAttr bufferAttr{};
        bufferAttr.nBuffer = mAttr.nBuffers + 1;
        bufferAttr.size[0] = mAttr.width;
        bufferAttr.size[1] = mAttr.height;
        if (!mWinPtr->initBuffer(bufferAttr)) {
            LOGE("win ptr init buffer error.");
            return nullptr;
        }

        mCapBuf = new qcarcam_buffers_t;
        if(mCapBuf == nullptr){
            LOGE("new qcarcam_buffers_t error.");
            return nullptr;
        }
        mCapBuf->n_buffers = mAttr.nBuffers;
        mCapBuf->color_fmt = mAttr.format;
        mCapBuf->flags |= QCARCAM_BUFFER_FLAG_OS_HNDL;;
        mCapBuf->buffers = (qcarcam_buffer_t*)calloc(mCapBuf->n_buffers, sizeof(qcarcam_buffer_t));
        if(mCapBuf->buffers == nullptr){
            LOGE("calloc mem error.");
            return nullptr;
        }
        ::memset(mCapBuf->buffers,0,mCapBuf->n_buffers*sizeof(qcarcam_buffers_t));
        for (unsigned int i = 0; i < mCapBuf->n_buffers; ++i) {
            mCapBuf->buffers[i].n_planes = 1;
            mCapBuf->buffers[i].planes[0].width = mAttr.width;
            mCapBuf->buffers[i].planes[0].height = mAttr.height;
        }

        const QScreen::QScreenWindow::WindowBuffer& buffer = mWinPtr->getWinBuf();
        for (unsigned int i = 0; i < buffer.handles.size(); ++i) {
            //mCapBuf->buffers[i].planes[0].p_buf = buffer.handles[i].ptr[0];
            mCapBuf->buffers[i].planes[0].p_buf = buffer.handles[i].memHandle;
            mCapBuf->buffers[i].planes[0].stride = buffer.stride[0];
            mCapBuf->buffers[i].planes[0].size = buffer.stride[0] * mCapBuf->buffers[i].planes[0].height;
            LOGD("[%d] %p %dx%d %d, size:%d", i, mCapBuf->buffers[i].planes[0].p_buf,
                               mCapBuf->buffers[i].planes[0].width, mCapBuf->buffers[i].planes[0].height,
                               mCapBuf->buffers[i].planes[0].stride, mCapBuf->buffers[i].planes[0].size);
        }
        if (!setupCapture(eventCallback)) {
            LOGE("setupCapture failed");
            return nullptr;
        }

        return mQcarcamCtx;
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