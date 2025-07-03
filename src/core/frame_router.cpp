#include "frame_router.h"
#include "core/config_manager.h"
#include "net/serial_handler.h"
#include "esp_log.h"

#define TAG "FRAME_ROUTER"

void frame_router_process(const lynk_frame_t* frame) {
    const lynk_config_t* cfg = config_get();

    if (cfg->mode == LYNK_MODE_STATIC) {
        lynk_frame_t routed = *frame;  // Frame'in kopyasını oluştur
        routed.dst_id = cfg->static_dst_id;  // dst_id'yi override et
        ESP_LOGI(TAG, "STATIC: Overriding dst_id → %u", routed.dst_id);
        serial_handler_send_to_module(&routed);
        return;
    }

    // DYNAMIC MOD: Frame yalnızca hedef cihaz ID'sine ya da broadcast (0xFF) adresine sahipse iletilir
    if (frame->dst_id != cfg->device_id && frame->dst_id != 0xFF) {
        ESP_LOGI(TAG, "DYNAMIC: Frame ignored (dst_id: %u)", frame->dst_id);
        return;
    }

    ESP_LOGI(TAG, "DYNAMIC: Forwarding frame to dst_id: %u", frame->dst_id);
    serial_handler_send_to_module(frame);
}

