///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
#include "capture.h"
#include "capture.pio.h"
#include "multicore_rp2040.h"
#include "usb_log.h"

using enum usb_log::log_level;

constexpr uint32_t CORE1_CMD_WRITE_DATA       = 0xDEADBEEF;
constexpr uint32_t CORE1_CMD_WAIT_FOR_TRIGGER = 0xDEADC0DE;

capture::capture() {
    _capture_sm = pio_rp2xxx::pio0.loadProgram(capture_program);

    _dma_A = dma_rp2xxx::allocateChannel();
    _dma_B = dma_rp2xxx::allocateChannel();

    // Pre-configure the DMA channels (these values will not change!)
    _dma_A->READ_ADDR       = (uint32_t)&_capture_sm->pio.RXF[_capture_sm->sm_index];
    _dma_A->CTRL.TREQ_SEL   = CH_CTRL_TRIG_TREQ_SEL__PIO0_RX0+_capture_sm->sm_index;
    _dma_A->CTRL.INCR_READ  = false;
    _dma_A->CTRL.INCR_WRITE = true;
    _dma_A->attachIrq0([&] { dma_irq_handler(true); });

    _dma_B->READ_ADDR       = (uint32_t)&_capture_sm->pio.RXF[_capture_sm->sm_index];
    _dma_B->CTRL.TREQ_SEL   = CH_CTRL_TRIG_TREQ_SEL__PIO0_RX0+_capture_sm->sm_index;
    _dma_B->CTRL.INCR_READ  = false;
    _dma_B->CTRL.INCR_WRITE = true;
    _dma_B->attachIrq1([&] { dma_irq_handler(false); });

    for (uint8_t ch=0; ch < LA_D_CHANNEL_COUNT; ch++) {
        _d_channels[ch].setGpio(LA_D_CHANNEL_START + ch);
        _d_channels[ch].gpioMode(GPIO::INPUT | GPIO::PULLUP);
    }

    multicore_rp2040::reset_core1();
    multicore_rp2040::start_on_core1([&] {
        while (true) {
            uint32_t cmd = multicore_rp2040::fifo_pop_blocking();
            switch (cmd) {
                case CORE1_CMD_WRITE_DATA: {
                    auto * data = (uint8_t *) multicore_rp2040::fifo_pop_blocking();
                    uint32_t samples = multicore_rp2040::fifo_pop_blocking();
                    if (_state == IDLE || samples == 0) {
                        _core1_jobs_popped = _core1_jobs_popped + 1;
                        continue;
                    }
                    size_t rle_len = 1;
                    switch (_sample_size) {
                        case 1: rle_len = write_rle4_data (data, samples); break;
                        case 2: rle_len = write_rle11_data(data, samples); break;
                        case 4: rle_len = write_rle25_data(data, samples); break;
                        default: TUPP_LOG(LOG_ERROR, "Wrong sample size!!!");
                    }
                    data_handler(data, rle_len);
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
                default: TUPP_LOG(LOG_ERROR, "Unknow core1 command!");
            }
        }
    });
}

bool capture::start(capture_config_t * c) {
    while (_core1_jobs_pushed != _core1_jobs_popped) { }
    _core1_jobs_pushed = 0;
    _core1_jobs_popped = 0;

    abort();

    _capture_config  = c;
    _is_active       = true;
    _samples_read    = 0;
    _planned_samples = 0;
    _overflow        = false;
    _abort_trigger   = false;

    // Prepare DMA channels
    _dma_A->CTRL.EN       = true;
    _dma_B->CTRL.EN       = true;
    _dma_A->CTRL.CHAIN_TO = _dma_B->getIndex();
    _dma_B->CTRL.CHAIN_TO = _dma_A->getIndex();

    // Configure the PIO state machine
    _capture_sm->reset();
    configure_capture(_capture_sm,
                      _capture_config->digital_channels_base,
                      _capture_config->number_of_digital_channels);
    _capture_sm->setClock(_capture_config->sample_rate);

    // Calculate total size of a single sample
    _sample_size = 1;
    if (_capture_config->number_of_digital_channels > 4) {
        _sample_size += 1;
    }
    if (_capture_config->number_of_digital_channels > 11) {
        _sample_size += 2;
    }

    _samples_per_buffer = SAMPLE_BUFFER_SIZE / _sample_size;

    // Puffergröße begrenzen, damit spätestens alle 0,5 s (sample_rate / 2) ein Block gesendet wird
    uint32_t max_samples_half_sec = _capture_config->sample_rate / 4;
    if (max_samples_half_sec > 0 && _samples_per_buffer > max_samples_half_sec) {
        _samples_per_buffer = max_samples_half_sec;
    }

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
        default:
            return false;
    }

    // Bei hohen Frequenzen (> 15 MHz) auf die 2-Buffer-Kapazität deckeln (Burst-Modus)
    if (_capture_config->sample_rate > 15000000) {
        uint32_t max_burst_samples = 2 * _samples_per_buffer;
        if (_capture_config->post_trigger_samples > max_burst_samples) {
            _capture_config->post_trigger_samples = max_burst_samples;
        }
    }

    // Zustand setzen: Entweder auf Hardware-Trigger warten oder direkt streamen
    bool use_hw_trigger = has_triggers();
    _state = use_hw_trigger ? WAIT_FOR_TRIGGER : STREAMING;
    bool streaming = (_state == STREAMING);

    // Beide DMA-Kanäle vorladen (bei WAIT_FOR_TRIGGER im endlosen Ping-Pong)
    setup_dma_channel(_dma_A.get(), _buffer_A, streaming);
    setup_dma_channel(_dma_B.get(), _buffer_B, streaming);

    // Hardware starten
    _dma_A->trigger();
    _capture_sm->enable();

    // Falls Hardware-Trigger aktiv sind, Core 1 zum Warten losschicken
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
    _dma_A->clear_irq();
    _dma_B->clear_irq();
    _dma_A->CTRL.EN = false;
    _dma_B->CTRL.EN = false;
    _capture_sm->reset();
}

void COPY_TO_RAM capture::setup_dma_channel(dma_rp2xxx * dma, const uint8_t * buffer, bool is_streaming) {
    dma->WRITE_ADDR = (uint32_t)buffer;

    if (!is_streaming) {
        dma->TRANS_COUNT = _samples_per_buffer;
        return;
    }

    size_t samples_needed = 0;
    if (_capture_config->post_trigger_samples > _planned_samples) {
        samples_needed = _capture_config->post_trigger_samples - _planned_samples;
    }

    if (samples_needed > _samples_per_buffer) {
        dma->TRANS_COUNT = _samples_per_buffer;
        _planned_samples += _samples_per_buffer;

        // Prüfen, ob der aktuell laufende Partner-Puffer bereits für den Rest reicht
        if (_capture_config->sample_rate > 15000000) {
            size_t remaining_after_partner = samples_needed - _samples_per_buffer;
            if (remaining_after_partner <= _samples_per_buffer) {
                dma_rp2xxx * partner = (dma == _dma_A.get()) ? _dma_B.get() : _dma_A.get();
                partner->CTRL.CHAIN_TO = partner->getIndex();
            }
        }
    } else if (samples_needed > 0) {
        dma->TRANS_COUNT   = samples_needed;
        dma->CTRL.CHAIN_TO = dma->getIndex();
        _planned_samples += samples_needed;
    } else {
        dma->CTRL.EN = false;
    }
}

void COPY_TO_RAM capture::dma_irq_handler(bool is_A) {
    bool streaming = (_state == STREAMING);

    dma_rp2xxx * dma    = is_A ? _dma_A.get() : _dma_B.get();
    uint8_t *    buffer = is_A ? _buffer_A    : _buffer_B;

    auto * write_addr = (uint8_t *)dma->WRITE_ADDR;

    if (streaming) {
        uint32_t samples = (write_addr - buffer) / _sample_size;
        if (samples > 0) {
            if ((_core1_jobs_pushed - _core1_jobs_popped) > 1) {
                _overflow = true;
                abort();
                return;
            }
            _core1_jobs_pushed = _core1_jobs_pushed + 1;
            multicore_rp2040::fifo_push_blocking(CORE1_CMD_WRITE_DATA);
            multicore_rp2040::fifo_push_blocking((uint32_t)buffer);
            multicore_rp2040::fifo_push_blocking(samples);
        }
    }
    // Kanal für den nächsten Durchlauf neu programmieren
    setup_dma_channel(dma, buffer, streaming);
}


size_t COPY_TO_RAM capture::write_rle4_data(uint8_t * data, size_t len) {
    if (data == nullptr || len == 0) return 0;
    uint8_t * write_ptr = data;
    // Write the first sample
    uint8_t last = data[0];
    write_rle4_sample(write_ptr, 0, last);
    size_t rle_count = 0;
    for (size_t i=1; i<len; i++) {
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

void COPY_TO_RAM capture::write_rle4_sample(uint8_t * & write_ptr,
                                            size_t rle_count, uint8_t last) {
    if (rle_count) {
        size_t div=0, mod=0;
        while (rle_count >= 640) {
            *write_ptr++ = 0x7f;
            rle_count -= 640;
        }
        div = rle_count >> 3;
        mod = rle_count &  7;
        if (div) {
            *write_ptr++ = div + 47;
        }
        last |= mod << 4;
    }
    *write_ptr++ = 0x80 | last;
}

size_t COPY_TO_RAM capture::write_rle11_data(uint8_t * data, size_t len) {
    if (data == nullptr || len == 0) return 0;
    auto * src = static_cast<const uint16_t *>(__builtin_assume_aligned(data, 4));
    uint8_t * write_ptr = data;
    // Write the first sample
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

void COPY_TO_RAM capture::write_rle11_sample(uint8_t * & write_ptr,
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

size_t COPY_TO_RAM capture::write_rle25_data(uint8_t * data, size_t len) {
    if (data == nullptr || len == 0) return 0;
    const auto * src = static_cast<const uint32_t *>(__builtin_assume_aligned(data, 4));
    uint8_t * write_ptr = data;
    // Write the first sample
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

inline void COPY_TO_RAM capture::write_rle25_sample(uint8_t * & write_ptr, size_t rle_count, uint32_t val) {
    uint8_t b0 = 0x80 | (val & 0x0F);          // Bits 0..3   (4 Bit)
    uint8_t b1 = 0x80 | ((val >> 4)  & 0x7F);  // Bits 4..10  (7 Bit)
    uint8_t b2 = 0x80 | ((val >> 11) & 0x7F);  // Bits 11..17 (7 Bit)
    uint8_t b3 = 0x80 | ((val >> 18) & 0x7F);  // Bits 18..24 (7 Bit)

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
