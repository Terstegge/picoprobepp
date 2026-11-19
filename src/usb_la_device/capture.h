///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
/**
 * @file capture.h
 * @brief
 *
 */
#ifndef CAPTURE_H
#define CAPTURE_H

#include "trigger.h"
#include "config.h"
#include <array>

#define SAMPLE_BUFFER_SIZE 100000

struct capture_config_t {
    uint8_t     digital_channels_base {0};
    uint8_t     number_of_digital_channels {0};
    uint8_t     analog_channel_base {0};
    uint8_t     number_of_analog_channels {0};
    uint32_t    sample_rate {0};
    uint32_t    pre_trigger_samples {0};
    uint32_t    post_trigger_samples {0};
    std::array<trigger, LA_D_CHANNEL_COUNT> trigger_stages;
};

class capture {
public:
    capture();

    bool start(capture_config_t * c);
    void abort();
    bool is_active() const {return _is_active;}
    bool has_overflow() const { return _overflow; }

    // Callback handlers for Sample processing
    std::function<void(uint8_t *, uint32_t)> data_handler;

private:

    enum capture_status_t {
        IDLE,
        PRE_TRIGGER_SAMPLES,
        WAIT_FOR_TRIGGER,
        STREAMING
    };

    bool has_triggers() const {
        for (const auto & t : _capture_config->trigger_stages) {
            if (t.is_set()) return true;
        }
        return false;
    }

    /**
     * @brief Wait for all triggers. The method returns when all
     * triggers stages have fired. The method is moved to RAM so
     * we have a fast and deterministic execution
     */
    void COPY_TO_RAM wait_for_triggers() {
        for (auto & t: _capture_config->trigger_stages) {
            t.wait(_abort_trigger);
        }
    }

    volatile bool _abort_trigger {false};

    void COPY_TO_RAM setup_dma_channel(dma_rp2xxx * dma, const uint8_t * buffer, bool is_streaming);
    void COPY_TO_RAM dma_irq_handler(bool is_A);

    volatile capture_status_t   _state {IDLE};
    capture_config_t *          _capture_config {nullptr};
    volatile bool               _is_active {false};
    volatile uint32_t           _core1_jobs_pushed {0};
    volatile uint32_t           _core1_jobs_popped {0};
    volatile bool               _overflow {false};

    // Digital input pins
    gpio_rp2xxx _d_channels[LA_D_CHANNEL_COUNT];

    // PIO State Machine reading the samples.
    std::unique_ptr<SM> _capture_sm;

    // DMA channels
    std::unique_ptr<dma_rp2xxx> _dma_A;
    std::unique_ptr<dma_rp2xxx> _dma_B;

    // Sample data buffers
    uint8_t  _buffer_A[SAMPLE_BUFFER_SIZE] { };
    uint8_t  _buffer_B[SAMPLE_BUFFER_SIZE] { };

    uint8_t  _sample_size {0};
    uint32_t _samples_per_buffer {0};
    uint32_t _samples_read {0};
    uint32_t _planned_samples {0};

    static size_t COPY_TO_RAM write_rle4_data  (uint8_t * data, size_t len);
    static void   COPY_TO_RAM write_rle4_sample(uint8_t * & write_ptr,
                                                size_t rle_count,
                                                uint8_t last);

    static size_t COPY_TO_RAM write_rle11_data  (uint8_t * data, size_t len);
    static void   COPY_TO_RAM write_rle11_sample(uint8_t * & write_ptr,
                                                 size_t rle_count,
                                                 uint16_t last);

    static size_t COPY_TO_RAM write_rle25_data  (uint8_t * data, size_t len);
    static void   COPY_TO_RAM write_rle25_sample(uint8_t * & write_ptr,
                                                 size_t rle_count,
                                                 uint32_t last);
};

#endif // CAPTURE_H
