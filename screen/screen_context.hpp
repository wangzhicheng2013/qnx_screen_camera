#pragma once
#include <stdlib.h>
#include <memory>
#include <vector>
#include <screen/screen.h>
#include "color_log.hpp"
namespace qnx_screen {
    struct display_property {
        int id = 0 ;
        int size[2] = { 0 };
    };
    class screen_context : public std::enable_shared_from_this<screen_context> {
    public:
        bool init() {
            free_context();
            int rc = screen_create_context(&screen_ctx_, SCREEN_APPLICATION_CONTEXT | SCREEN_POWER_MANAGER_CONTEXT);
            if (rc) {
                LOG_E("create context error!");
                return false;
            }
            int display_num = 0;
            rc = screen_get_context_property_iv(screen_ctx_, SCREEN_PROPERTY_DISPLAY_COUNT, &display_num);
            if (rc) {
                LOG_E("get SCREEN_PROPERTY_DISPLAY_COUNT error!");
                return false;
            }
            LOG_D("display num: %d", display_num);
            displays_ = static_cast<screen_display_t *>(calloc(display_num, sizeof(screen_display_t)));
            if (nullptr == displays_) {
                LOG_E("calloc mem error!");
                return false;
            }
            rc = screen_get_context_property_pv(screen_ctx_, SCREEN_PROPERTY_DISPLAYS, (void **)displays_);
            if (rc) {
                LOG_E("get SCREEN_PROPERTY_DISPLAYS error!");
                return false;
            }
            for (int i = 0;i < display_num;++i) {
                display_property disPro;
                rc = screen_get_display_property_iv(displays_[i], SCREEN_PROPERTY_ID, &(disPro.id));
                if (rc) {
                    LOG_E("get SCREEN_PROPERTY_ID error!");
                    return false;
                }
                rc = screen_get_display_property_iv(displays_[i], SCREEN_PROPERTY_SIZE, disPro.size);
                if (rc) {
                    LOG_E("get SCREEN_PROPERTY_SIZE error!");
                    return false;
                }
                LOG_D("display id:%d, display size w/h:%d/%d", disPro.id, disPro.size[0], disPro.size[1]);
                display_pro_vec.emplace_back(disPro);
            }
            return true;
        }
        void free_context() {
            if (screen_ctx_ != nullptr) {
                screen_destroy_context(screen_ctx_);
                screen_ctx_ = nullptr;
            }
            if (displays_ != nullptr) {
                free(displays_);
                displays_ = nullptr;
            }        
        }
        virtual ~screen_context() {
            free_context();
        }
        inline screen_context_t get_screen_ctx() const {
            return screen_ctx_;
        }

        inline screen_display_t *get_displays() const {
            return displays_;
        }
        inline const display_property *get_display_property(int idx) const {
            if (idx < display_pro_vec.size()) {
                return &(display_pro_vec[idx]);
            }
            return nullptr;
        }
        std::shared_ptr<screen_context> get_context_ptr() {
            return shared_from_this();
        }
    private:
        screen_context_t screen_ctx_ = nullptr;
        screen_display_t *displays_ = nullptr;
        std::vector<display_property>display_pro_vec;
    }; 
}