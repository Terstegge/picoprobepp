//
// Created by andreas on 30.10.26.
//

#ifndef LA_CAPTURE_H
#define LA_CAPTURE_H

#include "la_trigger.h"
#include "config.h"
#include <array>

struct capture_config_t {
    uint8_t     digital_channels_base {0};
    uint8_t     number_of_digital_channels {0};
    uint8_t     analog_channel_base {0};
    uint8_t     number_of_analog_channels {0};
    uint32_t    sample_rate {0};
    uint32_t    pre_trigger_samples {0};
    uint32_t    post_trigger_samples {0};
    std::array<la_trigger, 4> trigger_stages;
};

class la_capture {
public:
    la_capture();

    bool start(capture_config_t * c);
    void abort();
    bool is_active() const {return _is_active;}

    // Callback handlers for Sample processing
    std::function<void(uint8_t *, uint32_t)> data_handler;

private:

    enum capture_status_t {
        IDLE,
        PRE_TRIGGER_SAMPLES,
        WAIT_FOR_TRIGGER,
        STREAMING
    };

    /**
     * @brief Wait for all triggers. The method returns when all
     * triggers stages have fired. The method is moved to RAM so
     * we have a fast and deterministic execution
     */
    void COPY_TO_RAM wait_for_triggers() {
        for (auto & t: _capture_config->trigger_stages) t.wait();
    }

    void COPY_TO_RAM dma_irq_handler(bool is_A);

    capture_status_t            _status {IDLE};
    const capture_config_t *    _capture_config {nullptr};
    bool                        _is_active {false};

    // Digital input pins
    gpio_rp2xxx _d_channels[LA_D_CHANNEL_COUNT];

    // PIO State Machines
    std::unique_ptr<SM> _capture_sm;

    // DMA channels
    std::unique_ptr<dma_rp2xxx> _dma_A;
    std::unique_ptr<dma_rp2xxx> _dma_B;

    // Sample data buffers
    uint8_t * _buffer_A {nullptr};
    uint8_t * _buffer_B {nullptr};

    uint32_t _samples_per_buffer {0};

    uint32_t _samples_read {0};
};


#endif //PICOPROBE_LA_CAPTURE_H