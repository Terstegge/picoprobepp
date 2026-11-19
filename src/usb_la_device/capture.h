///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
/**
 * @file capture.h
 * @brief High-speed Logic Analyzer and Mixed-Signal capture engine for RP2040.
 *
 * Implements a dual-core capture pipeline using PIO0, the internal 12-bit SAR ADC,
 * and four ping-pong DMA channels. Supports continuous RLE-compressed streaming
 * up to 15 MHz, high-speed burst capture up to 200 MHz with hardware triggers,
 * and 4-channel 12-bit analog / mixed-signal acquisition using a zero-overhead
 * buffer overlay.
 */
#ifndef CAPTURE_H
#define CAPTURE_H

#include "trigger.h"
#include "config.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>

/// Size in bytes of each of the two primary ping-pong capture buffers
#define SAMPLE_BUFFER_SIZE 100000

/**
 * @brief Configuration parameters for a single capture session.
 */
struct capture_config_t {
    uint8_t     digital_channels_base {0};  ///< First GPIO pin used for digital channels
    uint32_t    digital_channels_mask {0};  ///< Bitmask of enabled digital channels
    uint8_t     analog_channel_base {0};    ///< First GPIO pin used for analog channels (GPIO 26)
    uint8_t     analog_channels_mask {0};   ///< Bitmask of enabled analog channels (bits 0..3)
    uint32_t    sample_rate {0};            ///< Requested sample rate in Hz
    uint32_t    pre_trigger_samples {0};    ///< Number of pre-trigger samples
    uint32_t    post_trigger_samples {0};   ///< Total number of samples to capture after trigger
    std::array<trigger, LA_D_CHANNEL_COUNT> trigger_stages; ///< Hardware trigger configurations
};

/**
 * @brief Dual-core Logic Analyzer and Mixed-Signal acquisition controller.
 */
class capture {
public:
    capture();

    /**
     * @brief Configure peripherals and start a new capture session.
     * @param c Pointer to the capture configuration structure.
     * @return true if capture was started successfully, false if no channels are enabled.
     */
    bool start(capture_config_t * c);

    /**
     * @brief Immediately abort any running capture, DMA transfers, PIO, and ADC.
     */
    void abort();

    /**
     * @brief Check if a capture session is currently running or waiting for a trigger.
     * @return true while active, false once completed or aborted.
     */
    bool is_active() const { return _is_active; }

    /**
     * @brief Check if a buffer overflow occurred because
     *        USB transmission fell behind.
     * @return true if an overflow was detected during the last capture.
     */
    bool has_overflow() const { return _overflow; }

    /// Callback invoked from Core 1 whenever a compressed or
    /// mixed data block is ready for USB TX
    std::function<void(uint8_t *, uint32_t)> data_handler;

private:
    enum capture_status_t {
        IDLE,
        WAIT_FOR_TRIGGER,
        STREAMING
    };

    // Zero-RAM-overhead buffer overlay layout inside _buffer_A / _buffer_B for analog/mixed mode:
    //   [0     ... 15999] : Digital samples (up to 4000 samples * 4 bytes)
    //   [16000 ... 47999] : 12-bit ADC samples (up to 4000 samples * 4 channels * 2 bytes)
    //   [48000 ... 95999] : Packed sigrok slice TX output (up to 4000 slices * 12 bytes)
    static constexpr uint32_t MAX_MIXED_SAMPLES      = 4000;
    static constexpr uint32_t ADC_BUF_BYTE_OFFSET    = 16000;
    static constexpr uint32_t TX_BUF_BYTE_OFFSET     = 48000;

    // Rate thresholds and hardware clock constants
    static constexpr uint32_t MAX_STREAMING_RATE_HZ  = 15000000;
    static constexpr uint32_t MAX_TOTAL_ADC_RATE_HZ  = 400000;
    static constexpr uint32_t ADC_CLOCK_HZ           = 48000000;

    /**
     * @brief Check if any hardware trigger stage is configured.
     */
    bool has_triggers() const {
        for (const auto & t : _capture_config->trigger_stages) {
            if (t.is_set()) return true;
        }
        return false;
    }

    /**
     * @brief Busy-wait on Core 1 until all configured hardware
     *        triggers have fired or aborted.
     */
    void COPY_TO_RAM wait_for_triggers() {
        for (auto & t : _capture_config->trigger_stages) {
            t.wait(_abort_trigger);
        }
    }

    /**
     * @brief Reset and configure the RP2040 ADC for round-robin 12-bit DMA sampling.
     * @param analog_mask Bitmask of active ADC channels (bits 0..3 -> GPIO 26..29).
     * @param sample_rate Per-channel sample rate in Hz.
     */
    void configure_adc(uint8_t analog_mask, uint32_t sample_rate);

    /**
     * @brief Stop continuous ADC conversions and flush remaining FIFO entries.
     */
    void stop_adc();

    /**
     * @brief Configure a digital DMA channel for the next ping-pong transfer block.
     */
    void COPY_TO_RAM setup_dma_channel(dma_rp2xxx * dma,
                                       const uint8_t * buffer,
                                       bool is_streaming);

    /**
     * @brief Configure an ADC DMA channel for the next ping-pong transfer block.
     */
    void COPY_TO_RAM setup_adc_dma_channel(dma_rp2xxx * dma,
                                           uint16_t * buffer,
                                           bool is_streaming);

    /**
     * @brief DMA completion interrupt handler for ping-pong buffers A and B.
     * @param is_A true for buffer A (IRQ0), false for buffer B (IRQ1).
     */
    void COPY_TO_RAM dma_irq_handler(bool is_A);

    volatile bool               _abort_trigger {false};
    volatile capture_status_t   _state {IDLE};
    capture_config_t *          _capture_config {nullptr};
    volatile bool               _is_active {false};
    volatile uint32_t           _core1_jobs_pushed {0};
    volatile uint32_t           _core1_jobs_popped {0};
    volatile bool               _overflow {false};

    // Digital input pins
    gpio_rp2xxx _d_channels[LA_D_CHANNEL_COUNT];

    // PIO state machine sampling the digital input pins
    std::unique_ptr<SM> _capture_sm;

    // Ping-pong DMA channels for digital PIO FIFO and analog ADC FIFO
    std::unique_ptr<dma_rp2xxx> _dma_A;
    std::unique_ptr<dma_rp2xxx> _dma_B;
    std::unique_ptr<dma_rp2xxx> _dma_adc_A;
    std::unique_ptr<dma_rp2xxx> _dma_adc_B;

    // Primary 2x 100 KB ping-pong buffers (shared via overlay in analog/mixed mode)
    alignas(4) uint8_t _buffer_A[SAMPLE_BUFFER_SIZE] { };
    alignas(4) uint8_t _buffer_B[SAMPLE_BUFFER_SIZE] { };

    uint8_t  _sample_size {0};
    uint32_t _samples_per_buffer {0};
    uint32_t _samples_read {0};
    uint32_t _planned_samples {0};

    /**
     * @brief Pack raw digital and 12-bit ADC samples into uncompressed sigrok slices.
     * @param dig_data Pointer to raw digital samples (or nullptr in analog-only mode).
     * @param adc_data Pointer to interleaved 12-bit ADC samples.
     * @param out_buf  Destination buffer for packed 7-bit-encoded slice bytes (MSB set).
     * @param num_samples Number of sample slices to encode.
     * @return Number of bytes written to out_buf.
     */
    size_t COPY_TO_RAM write_mixed_data(const uint8_t * dig_data,
                                        const uint16_t * adc_data,
                                        uint8_t * out_buf,
                                        size_t num_samples);

    // In-place Run-Length Encoding (RLE) routines for
    // 1-byte (<=4 ch), 2-byte (<=11 ch), and 4-byte (<=25 ch) modes
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
