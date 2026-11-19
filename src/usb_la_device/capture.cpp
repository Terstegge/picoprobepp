///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
/**
 * @file capture.cpp
 * @brief Implementation of the dual-core Logic Analyzer and Mixed-Signal capture engine.
 */
#include "capture.h"
#include "capture.pio.h"
#include "multicore_rp2040.h"
#include "usb_log.h"
#include "RP2040.h"

using enum usb_log::log_level;

constexpr uint32_t CORE1_CMD_WRITE_DATA = 0xDEADBEEF;
constexpr uint32_t CORE1_CMD_WAIT_FOR_TRIGGER = 0xDEADC0DE;

capture::capture() {
    _capture_sm = pio_rp2xxx::pio0.loadProgram(capture_program);

    _dma_A = dma_rp2xxx::allocateChannel();
    _dma_B = dma_rp2xxx::allocateChannel();
    _dma_adc_A = dma_rp2xxx::allocateChannel();
    _dma_adc_B = dma_rp2xxx::allocateChannel();

    // Pre-configure digital DMA channels (PIO0 RX FIFO -> RAM buffer)
    _dma_A->READ_ADDR = (uint32_t) &_capture_sm->pio.RXF[_capture_sm->sm_index];
    _dma_A->CTRL.TREQ_SEL = CH_CTRL_TRIG_TREQ_SEL__PIO0_RX0 + _capture_sm->
                            sm_index;
    _dma_A->CTRL.INCR_READ = false;
    _dma_A->CTRL.INCR_WRITE = true;

    _dma_B->READ_ADDR = (uint32_t) &_capture_sm->pio.RXF[_capture_sm->sm_index];
    _dma_B->CTRL.TREQ_SEL = CH_CTRL_TRIG_TREQ_SEL__PIO0_RX0 + _capture_sm->
                            sm_index;
    _dma_B->CTRL.INCR_READ = false;
    _dma_B->CTRL.INCR_WRITE = true;

    // Pre-configure ADC DMA channels (16-bit halfwords from ADC FIFO -> RAM overlay)
    _dma_adc_A->READ_ADDR = (uint32_t) &_ADC_::ADC.FIFO;
    _dma_adc_A->CTRL.TREQ_SEL = CH_CTRL_TRIG_TREQ_SEL__ADC;
    _dma_adc_A->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_HALFWORD;
    _dma_adc_A->CTRL.INCR_READ = false;
    _dma_adc_A->CTRL.INCR_WRITE = true;

    _dma_adc_B->READ_ADDR = (uint32_t) &_ADC_::ADC.FIFO;
    _dma_adc_B->CTRL.TREQ_SEL = CH_CTRL_TRIG_TREQ_SEL__ADC;
    _dma_adc_B->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_HALFWORD;
    _dma_adc_B->CTRL.INCR_READ = false;
    _dma_adc_B->CTRL.INCR_WRITE = true;

    // Attach shared ping-pong IRQ handlers (enabled/disabled per mode in start())
    _dma_A->attachIrq0([&] { dma_irq_handler(true); });
    _dma_B->attachIrq1([&] { dma_irq_handler(false); });
    _dma_adc_A->attachIrq0([&] { dma_irq_handler(true); });
    _dma_adc_B->attachIrq1([&] { dma_irq_handler(false); });

    // Initialize digital input pins with internal pull-ups
    for (uint8_t ch = 0; ch < LA_D_CHANNEL_COUNT; ch++) {
        _d_channels[ch].setGpio(LA_D_CHANNEL_START + ch);
        _d_channels[ch].gpioMode(GPIO::INPUT | GPIO::PULLUP);
    }

    // Launch Core 1 worker loop for hardware trigger polling and RLE/slice encoding
    multicore_rp2040::reset_core1();
    multicore_rp2040::start_on_core1([&] {
        while (true) {
            uint32_t cmd = multicore_rp2040::fifo_pop_blocking();
            switch (cmd) {
                case CORE1_CMD_WRITE_DATA: {
                    auto *data = (uint8_t *)
                            multicore_rp2040::fifo_pop_blocking();
                    uint32_t samples = multicore_rp2040::fifo_pop_blocking();

                    if (_state == IDLE || samples == 0) {
                        _core1_jobs_popped = _core1_jobs_popped + 1;
                        continue;
                    }

                    if (_capture_config->analog_channels_mask != 0) {
                        // Mixed-signal or analog-only mode: pack uncompressed slices using buffer overlay
                        auto *adc_data = reinterpret_cast<const uint16_t *>(
                            data + ADC_BUF_BYTE_OFFSET);
                        uint8_t *tx_buf = data + TX_BUF_BYTE_OFFSET;
                        size_t tx_len = write_mixed_data(
                            data, adc_data, tx_buf, samples);
                        data_handler(tx_buf, tx_len);
                    } else {
                        // Digital-only mode: perform in-place Run-Length Encoding (RLE)
                        size_t rle_len = 1;
                        switch (_sample_size) {
                            case 1: rle_len = write_rle4_data(data, samples);
                                break;
                            case 2: rle_len = write_rle11_data(data, samples);
                                break;
                            case 4: rle_len = write_rle25_data(data, samples);
                                break;
                            default:
                                TUPP_LOG(LOG_ERROR, "Invalid sample size!");
                        }
                        data_handler(data, rle_len);
                    }

                    _samples_read += samples;
                    _core1_jobs_popped = _core1_jobs_popped + 1;

                    if (_samples_read >= _capture_config->post_trigger_samples) {
                        _state = IDLE;
                        _is_active = false;
                    }
                    break;
                }
                case CORE1_CMD_WAIT_FOR_TRIGGER: {
                    wait_for_triggers();
                    if (!_abort_trigger && _state == WAIT_FOR_TRIGGER) {
                        _state = STREAMING;
                    }
                    break;
                }
                default:
                    TUPP_LOG(LOG_ERROR, "Unknown Core 1 FIFO command!");
            }
        }
    });
}

void capture::set_sample_rate(capture_config_t * c, uint32_t rate) const {
    TUPP_LOG(LOG_INFO, "set_sample_rate(%u)", rate);
    if (rate < 5000) {
        rate = 5000;
    } else if (rate > 200000000) {
        rate = 200000000;
    }
    if (c->analog_channels_mask != 0) {
        uint8_t num_a = std::popcount(c->analog_channels_mask);
        uint32_t max_adc_rate = MAX_TOTAL_ADC_RATE_HZ / num_a;
        if (rate > max_adc_rate) {
            rate = max_adc_rate;
        }
    }
    c->sample_rate = rate;
}

void capture::configure_adc(uint8_t analog_mask, uint32_t sample_rate) {
    using namespace _IO_BANK0_;
    using namespace _RESETS_;
    using namespace _PADS_BANK0_;

    // 0. Perform a full hardware reset of the ADC peripheral for a deterministic start state
    RESETS_SET.RESET.adc = 1;
    RESETS_CLR.RESET.adc = 1;
    while (!RESETS.RESET_DONE.adc) {
    }

    // 1. Configure active GPIO 26..29 pins for high-impedance analog input
    for (uint8_t ch = 0; ch < LA_A_CHANNEL_COUNT; ch++) {
        if (analog_mask & (1u << ch)) {
            uint8_t gpio = LA_A_CHANNEL_START + ch;
            GPIO_CTRL_t *io_ctrl = &IO_BANK0.GPIO0_CTRL + (gpio << 1);
            io_ctrl->FUNCSEL = GPIO_CTRL_FUNCSEL__null;
            PADS_BANK0.GPIO[gpio].PUE = 0;
            PADS_BANK0.GPIO[gpio].PDE = 0;
            PADS_BANK0.GPIO[gpio].IE = 0;
            PADS_BANK0.GPIO[gpio].OD = 1;
        }
    }

    // 2. Enable ADC and wait until ready
    _ADC_::ADC.CS.EN = 1;
    while (!_ADC_::ADC.CS.READY) {
    }

    // 3. Configure 12-bit FIFO with DREQ enabled for every single sample
    _ADC_::ADC.FCS.EN = 1;
    _ADC_::ADC.FCS.SHIFT = 0;
    _ADC_::ADC.FCS.THRESH = 1;
    _ADC_::ADC.FCS.DREQ_EN = 1;
    while (!_ADC_::ADC.FCS.EMPTY) {
        (void) _ADC_::ADC.FIFO;
    }

    // 4. Set fractional clock divider (48 MHz ADC clock) for exact round-robin sample rate
    uint8_t num_a = std::popcount(analog_mask);
    uint32_t total_conv_rate = sample_rate * num_a;
    uint32_t div_256 = (static_cast<uint64_t>(ADC_CLOCK_HZ) * 256ULL) /
                       total_conv_rate;
    if (div_256 >= 256) div_256 -= 256;
    _ADC_::ADC.DIV = div_256;

    // 5. Select initial channel (AINSEL) before enabling round-robin mask (RROBIN)
    uint8_t first_a = std::countr_zero(analog_mask);
    _ADC_::ADC.CS.AINSEL = first_a;
    _ADC_::ADC.CS.RROBIN = analog_mask & 0x0F;
}

void capture::stop_adc() {
    if (!_ADC_::ADC.CS.EN) return;

    _ADC_::ADC.CS.START_MANY = 0;
    _ADC_::ADC.CS.RROBIN = 0;
    _ADC_::ADC.FCS.DREQ_EN = 0;

    // Flush up to 8 entries (hardware FIFO depth is 4)
    for (int i = 0; i < 8 && !_ADC_::ADC.FCS.EMPTY; ++i) {
        (void) _ADC_::ADC.FIFO;
    }
}

bool capture::start(capture_config_t *c) {
    // Wait until Core 1 has finished draining any leftover jobs from a previous run
    while (_core1_jobs_pushed != _core1_jobs_popped) { }
    _core1_jobs_pushed = 0;
    _core1_jobs_popped = 0;

    abort();

    _capture_config = c;
    _is_active = true;
    _samples_read = 0;
    _planned_samples = 0;
    _overflow = false;
    _abort_trigger = false;

    uint32_t d_mask = _capture_config->digital_channels_mask;
    uint8_t a_mask = _capture_config->analog_channels_mask;
    bool has_dig = (d_mask != 0);
    bool has_adc = (a_mask != 0);

    if (!has_dig && !has_adc) return false;

    uint8_t num_d = std::bit_width(d_mask);

    // Determine raw digital sample width (1, 2, or 4 bytes)
    if (has_dig) {
        _sample_size = 1;
        if (num_d > 4) _sample_size += 1;
        if (num_d > 11) _sample_size += 2;
    } else {
        _sample_size = 1;
    }

    // Configure PIO state machine before ADC so analog pin muxing is not overwritten
    _capture_sm->reset();
    if (has_dig) {
        configure_capture(_capture_sm, _capture_config->digital_channels_base,
                          num_d);
        _capture_sm->setClock(_capture_config->sample_rate);
    }

    if (has_adc) {
        configure_adc(a_mask, _capture_config->sample_rate);
    }

    // Determine buffer size in samples
    if (has_adc) {
        _samples_per_buffer = MAX_MIXED_SAMPLES;
    } else {
        _samples_per_buffer = SAMPLE_BUFFER_SIZE / _sample_size;
    }

    // Cap buffer size at low sample rates so PulseView receives updates at least every 0.5 s
    uint32_t max_samples_half_sec = _capture_config->sample_rate / 2;
    if (max_samples_half_sec > 0 && _samples_per_buffer >max_samples_half_sec) {
        _samples_per_buffer = max_samples_half_sec;
    }

    // Cap sample count to 2-buffer RAM capacity in high-speed burst mode (> 15 MHz)
    if (!has_adc && _capture_config->sample_rate > MAX_STREAMING_RATE_HZ) {
        uint32_t max_burst = 2 * _samples_per_buffer;
        if (_capture_config->post_trigger_samples > max_burst) {
            _capture_config->post_trigger_samples = max_burst;
        }
    }
    bool use_hw_trigger = has_triggers();
    _state = use_hw_trigger ? WAIT_FOR_TRIGGER : STREAMING;
    bool streaming = (_state == STREAMING);
    // Configure DMA master/slave roles: ADC DMA is master when analog channels are active
    if (has_adc) {
        _dma_A->disableIrq0();
        _dma_B->disableIrq1();
        _dma_adc_A->enableIrq0();
        _dma_adc_B->enableIrq1();
        _dma_adc_A->CTRL.EN = true;
        _dma_adc_B->CTRL.EN = true;
        _dma_adc_A->CTRL.CHAIN_TO = _dma_adc_B->getIndex();
        _dma_adc_B->CTRL.CHAIN_TO = _dma_adc_A->getIndex();
        auto *adc_buf_A = reinterpret_cast<uint16_t *>(
            _buffer_A + ADC_BUF_BYTE_OFFSET);
        auto *adc_buf_B = reinterpret_cast<uint16_t *>(
            _buffer_B + ADC_BUF_BYTE_OFFSET);
        setup_adc_dma_channel(_dma_adc_A.get(), adc_buf_A, streaming);
        setup_adc_dma_channel(_dma_adc_B.get(), adc_buf_B, streaming);
    } else {
        _dma_adc_A->disableIrq0();
        _dma_adc_B->disableIrq1();
        _dma_A->enableIrq0();
        _dma_B->enableIrq1();
    }
    if (has_dig) {
        _dma_A->CTRL.EN = true;
        _dma_B->CTRL.EN = true;
        _dma_A->CTRL.CHAIN_TO = _dma_B->getIndex();
        _dma_B->CTRL.CHAIN_TO = _dma_A->getIndex();
        switch (_sample_size) {
            case 1:
                _dma_A->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_BYTE;
                _dma_B->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_BYTE;
                break;
            case 2:
                _dma_A->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_HALFWORD;
                _dma_B->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_HALFWORD;
                break;
            case 4:
                _dma_A->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_WORD;
                _dma_B->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_WORD;
                break;
            default: return false;
        }
        if (!has_adc) {
            setup_dma_channel(_dma_A.get(), _buffer_A, streaming);
            setup_dma_channel(_dma_B.get(), _buffer_B, streaming);
        } else {
            // In mixed mode, digital DMA runs synchronized to ADC DMA sample counts
            uint32_t initial_samples = std::min<uint32_t>(_samples_per_buffer,
                _capture_config->post_trigger_samples);
            _dma_A->WRITE_ADDR = (uint32_t) _buffer_A;
            _dma_A->TRANS_COUNT = initial_samples;
            _dma_B->WRITE_ADDR = (uint32_t) _buffer_B;
            _dma_B->TRANS_COUNT = initial_samples;
        }
    }
    // Arm DMA channels first, then enable PIO and ADC free-running conversions
    if (has_dig) _dma_A->trigger();
    if (has_adc) _dma_adc_A->trigger();
    if (has_dig) _capture_sm->enable();
    if (has_adc) _ADC_::ADC.CS.START_MANY = 1;
    if (use_hw_trigger) {
        multicore_rp2040::fifo_push_blocking(CORE1_CMD_WAIT_FOR_TRIGGER);
    }
    return true;
}

void capture::abort() {
    _state = IDLE;
    _abort_trigger = true;
    _is_active = false;
    _dma_A->abort();
    _dma_B->abort();
    _dma_adc_A->abort();
    _dma_adc_B->abort();
    _dma_A->clear_irq();
    _dma_B->clear_irq();
    _dma_adc_A->clear_irq();
    _dma_adc_B->clear_irq();
    _dma_A->CTRL.EN = false;
    _dma_B->CTRL.EN = false;
    _dma_adc_A->CTRL.EN = false;
    _dma_adc_B->CTRL.EN = false;
    _capture_sm->reset();
    stop_adc();
}

void COPY_TO_RAM capture::setup_dma_channel(
    dma_rp2xxx *dma, const uint8_t *buffer, bool is_streaming) {
    dma->WRITE_ADDR = (uint32_t) buffer;
    if (!is_streaming) {
        dma->TRANS_COUNT = _samples_per_buffer;
        return;
    }
    size_t samples_needed = 0;
    if (_capture_config->post_trigger_samples > _planned_samples) {
        samples_needed = _capture_config->post_trigger_samples -
                         _planned_samples;
    }
    if (samples_needed > _samples_per_buffer) {
        dma->TRANS_COUNT = _samples_per_buffer;
        _planned_samples += _samples_per_buffer;
        // In high-speed burst mode (> 15 MHz), disable partner chaining one buffer early
        // so hardware chaining stops cleanly after the second buffer without overflowing.
        if (_capture_config->sample_rate > MAX_STREAMING_RATE_HZ) {
            size_t remaining_after_partner =
                    samples_needed - _samples_per_buffer;
            if (remaining_after_partner <= _samples_per_buffer) {
                dma_rp2xxx *partner = (dma == _dma_A.get())
                                          ? _dma_B.get()
                                          : _dma_A.get();
                partner->CTRL.CHAIN_TO = partner->getIndex();
            }
        }
    } else if (samples_needed > 0) {
        dma->TRANS_COUNT = samples_needed;
        dma->CTRL.CHAIN_TO = dma->getIndex();
        _planned_samples += samples_needed;
    } else {
        dma->CTRL.EN = false;
    }
}

void COPY_TO_RAM capture::setup_adc_dma_channel(
    dma_rp2xxx *dma, uint16_t *buffer, bool is_streaming) {
    uint8_t num_a = std::popcount(_capture_config->analog_channels_mask);
    dma->WRITE_ADDR = (uint32_t) buffer;
    if (!is_streaming) {
        dma->TRANS_COUNT = _samples_per_buffer * num_a;
        return;
    }
    size_t samples_needed = 0;
    if (_capture_config->post_trigger_samples > _planned_samples) {
        samples_needed = _capture_config->post_trigger_samples -
                         _planned_samples;
    }
    if (samples_needed > _samples_per_buffer) {
        dma->TRANS_COUNT = _samples_per_buffer * num_a;
        _planned_samples += _samples_per_buffer;
    } else if (samples_needed > 0) {
        dma->TRANS_COUNT = samples_needed * num_a;
        dma->CTRL.CHAIN_TO = dma->getIndex();
        _planned_samples += samples_needed;
    } else {
        dma->CTRL.EN = false;
    }
}

void COPY_TO_RAM capture::dma_irq_handler(bool is_A) {
    bool streaming = (_state == STREAMING);
    bool has_dig = (_capture_config->digital_channels_mask != 0);
    bool has_adc = (_capture_config->analog_channels_mask != 0);
    uint8_t *buffer = is_A ? _buffer_A : _buffer_B;
    dma_rp2xxx *dig_dma = is_A ? _dma_A.get() : _dma_B.get();
    dma_rp2xxx *adc_dma = is_A ? _dma_adc_A.get() : _dma_adc_B.get();
    if (streaming) {
        uint32_t samples = 0;
        if (has_adc) {
            uint8_t num_a =
                    std::popcount(_capture_config->analog_channels_mask);
            auto *adc_start = reinterpret_cast<uint16_t *>(
                buffer + ADC_BUF_BYTE_OFFSET);
            auto *write_addr = (uint16_t *) adc_dma->WRITE_ADDR;
            samples = (write_addr - adc_start) / num_a;
        } else if (has_dig) {
            auto *write_addr = (uint8_t *) dig_dma->WRITE_ADDR;
            samples = (write_addr - buffer) / _sample_size;
        }
        if (samples > 0) {
            // Detect buffer overrun if Core 1 has not finished processing the previous buffer
            if ((_core1_jobs_pushed - _core1_jobs_popped) > 1) {
                _overflow = true;
                abort();
                return;
            }
            _core1_jobs_pushed = _core1_jobs_pushed + 1;
            multicore_rp2040::fifo_push_blocking(CORE1_CMD_WRITE_DATA);
            multicore_rp2040::fifo_push_blocking((uint32_t) buffer);
            multicore_rp2040::fifo_push_blocking(samples);
        }
    }
    if (has_adc) {
        auto *adc_start = reinterpret_cast<uint16_t *>(
            buffer + ADC_BUF_BYTE_OFFSET);
        setup_adc_dma_channel(adc_dma, adc_start, streaming);
        if (has_dig) {
            dma_rp2xxx *partner_dig = is_A ? _dma_B.get() : _dma_A.get();
            if (adc_dma->CTRL.EN) {
                dig_dma->abort();
                dig_dma->WRITE_ADDR = (uint32_t) buffer;
                dig_dma->TRANS_COUNT = _samples_per_buffer;
                dig_dma->CTRL.EN = true;
                if (!partner_dig->CTRL.BUSY) {
                    partner_dig->trigger();
                }
            } else {
                dig_dma->abort();
                dig_dma->CTRL.EN = false;
                dig_dma->CTRL.CHAIN_TO = dig_dma->getIndex();
            }
        }
    } else if (has_dig) {
        setup_dma_channel(dig_dma, buffer, streaming);
    }
}

size_t COPY_TO_RAM capture::write_mixed_data(const uint8_t *dig_data,
                                             const uint16_t *adc_data,
                                             uint8_t *out_buf,
                                             size_t num_samples) {
    uint8_t *out = out_buf;
    uint32_t d_mask = _capture_config->digital_channels_mask;
    uint8_t a_mask = _capture_config->analog_channels_mask;
    uint8_t num_a = std::popcount(a_mask);
    for (size_t s = 0; s < num_samples; s++) {
        // 1. Pack enabled digital channels in 7-bit chunks (MSB = 1)
        if (d_mask != 0 && dig_data != nullptr) {
            uint32_t d_val = 0;
            if (_sample_size == 1) d_val = dig_data[s];
            else if (_sample_size == 2)
                d_val = reinterpret_cast<const uint16_t *>(dig_data)[s];
            else d_val = reinterpret_cast<const uint32_t *>(dig_data)[s];
            for (uint8_t bit = 0; bit < LA_D_CHANNEL_COUNT; bit += 7) {
                if ((d_mask >> bit) & 0x7F) {
                    *out++ = 0x80 | ((d_val >> bit) & 0x7F);
                }
            }
        }
        // 2. Pack each 12-bit ADC sample into two 7-bit bytes (LSB first, MSB = 1)
        for (uint8_t a = 0; a < num_a; a++) {
            uint16_t raw = adc_data[s * num_a + a] & 0x0FFF;
            *out++ = 0x80 | (raw & 0x7F);
            *out++ = 0x80 | ((raw >> 7) & 0x7F);
        }
    }
    return out - out_buf;
}

size_t COPY_TO_RAM capture::write_rle4_data(uint8_t *data, size_t len) {
    if (data == nullptr || len == 0) return 0;
    uint8_t *write_ptr = data;
    uint8_t last = data[0];
    write_rle4_sample(write_ptr, 0, last);
    size_t rle_count = 0;
    for (size_t i = 1; i < len; i++) {
        if (data[i] == last) {
            rle_count++;
        } else {
            last = data[i];
            write_rle4_sample(write_ptr, rle_count, last);
            rle_count = 0;
        }
    }
    if (rle_count) {
        rle_count--;
        write_rle4_sample(write_ptr, rle_count, last);
    }
    return write_ptr - data;
}

void COPY_TO_RAM capture::write_rle4_sample(uint8_t * &write_ptr,
                                            size_t rle_count, uint8_t last) {
    if (rle_count) {
        size_t div = 0, mod = 0;
        while (rle_count >= 640) {
            *write_ptr++ = 0x7F;
            rle_count -= 640;
        }
        div = rle_count >> 3;
        mod = rle_count & 7;
        if (div) {
            *write_ptr++ = div + 47;
        }
        last |= mod << 4;
    }
    *write_ptr++ = 0x80 | last;
}

size_t COPY_TO_RAM capture::write_rle11_data(uint8_t *data, size_t len) {
    if (data == nullptr || len == 0) return 0;
    const auto *src = static_cast<const uint16_t *>(
        __builtin_assume_aligned(data, 4));
    uint8_t *write_ptr = data;
    uint16_t last = *src & 0x07FF;
    write_rle11_sample(write_ptr, 0, last);
    size_t rle_count = 0;
    for (size_t i = 1; i < len; i++) {
        uint16_t curr = src[i] & 0x07FF;
        if (curr == last) {
            rle_count++;
        } else {
            last = curr;
            write_rle11_sample(write_ptr, rle_count, last);
            rle_count = 0;
        }
    }
    if (rle_count) {
        rle_count--;
        write_rle11_sample(write_ptr, rle_count, last);
    }
    return write_ptr - data;
}

void COPY_TO_RAM capture::write_rle11_sample(uint8_t * &write_ptr,
                                             size_t rle_count, uint16_t val) {
    uint8_t b0 = 0x80 | (val & 0x0F);
    uint8_t b1 = 0x80 | ((val >> 4) & 0x7F);
    if (rle_count) {
        while (rle_count >= 640) {
            *write_ptr++ = 0x7F;
            rle_count -= 640;
        }
        size_t div = rle_count >> 3;
        size_t mod = rle_count & 7;
        if (div) {
            *write_ptr++ = div + 47;
        }
        b0 |= (mod << 4);
    }
    *write_ptr++ = b0;
    *write_ptr++ = b1;
}

size_t COPY_TO_RAM capture::write_rle25_data(uint8_t *data, size_t len) {
    if (data == nullptr || len == 0) return 0;
    const auto *src = static_cast<const uint32_t *>(
        __builtin_assume_aligned(data, 4));
    uint8_t *write_ptr = data;
    uint32_t last = *src & 0x01FFFFFF;
    write_rle25_sample(write_ptr, 0, last);
    size_t rle_count = 0;
    for (size_t i = 1; i < len; i++) {
        uint32_t curr = src[i] & 0x01FFFFFF;
        if (curr == last) {
            rle_count++;
        } else {
            last = curr;
            write_rle25_sample(write_ptr, rle_count, last);
            rle_count = 0;
        }
    }
    if (rle_count) {
        rle_count--;
        write_rle25_sample(write_ptr, rle_count, last);
    }
    return write_ptr - data;
}

inline void COPY_TO_RAM capture::write_rle25_sample(uint8_t * &write_ptr,
                                                    size_t rle_count,
                                                    uint32_t val) {
    uint8_t b0 = 0x80 | (val & 0x0F);         // Bits 0..3   (4 bits)
    uint8_t b1 = 0x80 | ((val >> 4) & 0x7F);  // Bits 4..10  (7 bits)
    uint8_t b2 = 0x80 | ((val >> 11) & 0x7F); // Bits 11..17 (7 bits)
    uint8_t b3 = 0x80 | ((val >> 18) & 0x7F); // Bits 18..24 (7 bits)
    if (rle_count) {
        while (rle_count >= 640) {
            *write_ptr++ = 0x7F;
            rle_count -= 640;
        }
        size_t div = rle_count >> 3;
        size_t mod = rle_count & 7;
        if (div) {
            *write_ptr++ = div + 47;
        }
        b0 |= (mod << 4);
    }
    *write_ptr++ = b0;
    *write_ptr++ = b1;
    *write_ptr++ = b2;
    *write_ptr++ = b3;
}
