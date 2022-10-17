#pragma once
#include <map>
#include <chrono>
#include "single_instance.hpp"
#include "camera_controller.hpp"
namespace qnx_screen_camera {
    class camera_manager;
    #define G_CAMERA_MANAGER single_instance<camera_manager>::instance()
    class camera_manager {
    public:
        bool init() {
            qcarcam_init_t qcarcam_init = { 0 };
            qcarcam_init.version = QCARCAM_VERSION;
            qcarcam_init.debug_tag = (char *) "camera_manager";
            qcarcam_ret_t ret = qcarcam_initialize(&qcarcam_init);
            if (ret != QCARCAM_RET_OK) {
                LOG_E("qcarcam_initialize failed %d", ret);
                return false;
            }
            if (!query_inputs()) {
                LOG_E("queryInputs error!");
                return false;
            }
            return true;
        }
        bool query_inputs() {
            int rc = 0;
            unsigned int queryCnt = 0, queryFill = 0;
            do {
                rc = qcarcam_query_inputs(nullptr, 0, &queryCnt);
                if (QCARCAM_RET_OK == rc && queryCnt != 0) {
                    LOG_I("query input count %d", queryCnt);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(5));
            } while (true);
            qcarcam_input_t *pInput = static_cast<qcarcam_input_t *>(calloc(queryCnt, sizeof(qcarcam_input_t)));
            if (nullptr == pInput) {
               LOG_E("calloc mem error!");
               return false;
            }
            rc = qcarcam_query_inputs(pInput, queryCnt, &queryFill);
            if (rc != QCARCAM_RET_OK || queryFill == 0) {
                free(pInput);
                LOG_E("rc:%d,query file error.", rc);
                return false;
            }
            for (int i = 0;i < queryFill;++i) {
                LOG_I("id:%d, format:%d, fps:%f, w*h:(%d*%d)  uyvy:%d", pInput[i].desc, pInput[i].color_fmt[0],
                        pInput[i].res[0].fps,
                        pInput[i].res[0].width, pInput[i].res[0].height, QCARCAM_FMT_UYVY_8);
                input_src_map_.emplace(std::make_pair(pInput[i].desc, pInput[i]));
            }
            free(pInput);
            return true;
        }
        qcarcam_hndl_t create_camera_connect(screen_attribute &sreenAttr, capture_attr &capAttr) {
            auto inputSrc = input_src_map_.find(capAttr.input_id);
            if (input_src_map_.end() == inputSrc) {
                LOG_E("cannot find input id!");
                return nullptr;
            }
            if (capAttr.height == -1 || capAttr.width == -1) {
                capAttr.width = inputSrc->second.res[0].width;
                capAttr.height = inputSrc->second.res[0].height;
            }
            if (QCARCAM_FMT_MAX == capAttr.format) {
                capAttr.format = inputSrc->second.color_fmt[0];
            }
            sreenAttr.input_id = static_cast<int>(capAttr.input_id);
            auto cameraPtr = std::make_shared<camera_controller>(capAttr);
            qcarcam_hndl_t camHandle = cameraPtr->init(sreenAttr, reinterpret_cast<void *>(qcarcamEventCb));
            if (nullptr == camHandle) {
                LOG_E("init camera handle failed!");
                return nullptr;
            }
            camera_connect_map_.emplace(std::make_pair(camHandle, cameraPtr));
            camera_handle_map_.emplace(std::make_pair(capAttr.input_id, cameraPtr));
            return camHandle;
        }
        std::shared_ptr<camera_controller> find_camera_connect(const qcarcam_hndl_t handle) {
            auto controller = camera_connect_map_.find(handle);
            if (controller != camera_connect_map_.end()) {
                return controller->second;
            }
            return nullptr;
        }
        std::shared_ptr<camera_controller> find_camera_connect_by_id(int id) {
            auto ctr = camera_handle_map_.find(id);
            if (ctr != camera_handle_map_.end()) {
                return ctr->second;
            }
            return nullptr;
        }
        bool control_camera(int id, int command) {
            auto ptr = find_camera_connect_by_id(id);
            if (nullptr == ptr) {
                LOG_E("controlCamera id:%d is not exist.", id);
                return false;
            }
            if (false == ptr->control_command(command)) {
                LOG_E("call controlCommand error!");
                return false;
            }
            return true;
        }
        static void qcarcamEventCb(qcarcam_hndl_t hndl, qcarcam_event_t event_id, qcarcam_event_payload_t *p_payload) {
            auto ptr = G_CAMERA_MANAGER.find_camera_connect(hndl);
            if (nullptr == ptr) {
                LOG_E("not find this hndl:%d", hndl);
                return;
            }
            switch (event_id) {
            case QCARCAM_EVENT_FRAME_READY: 
                ptr->notify_new_frame();
                break;
            case QCARCAM_EVENT_INPUT_SIGNAL:
                ptr->handle_signals((qcarcam_input_signal_t) p_payload->uint_payload);
                break;
            case QCARCAM_EVENT_ERROR:
                LOG_E("qcarcam event error!");
                break;
            default:
                break;
            }
        }
        void start_capture(qcarcam_hndl_t hndl) {
            auto ptr = find_camera_connect(hndl);
            if (ptr != nullptr) {
                ptr->start_capture();
            }
        }
        void stop_capture(qcarcam_hndl_t hndl) {
            auto ptr = find_camera_connect(hndl);
            if (ptr != nullptr) {
                ptr->stop_capture();
            }
        }
    private:
        std::map<qcarcam_input_desc_t, qcarcam_input_t>input_src_map_;
        std::map<qcarcam_hndl_t, std::shared_ptr<camera_controller>>camera_connect_map_;
        std::map<int, std::shared_ptr<camera_controller>>camera_handle_map_;
    };
}