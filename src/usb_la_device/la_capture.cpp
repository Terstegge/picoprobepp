///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
#include "la_capture.h"
#include "capture.pio.h"
#include "usb_log.h"
#include <task.h>

using enum usb_log::log_level;

#define SAMPLE_BUFFER_SIZE 50000

la_capture::la_capture() {
    // Allocate sample buffers
    _buffer_A = new uint8_t[SAMPLE_BUFFER_SIZE];
    _buffer_B = new uint8_t[SAMPLE_BUFFER_SIZE];

    // Load the sample SM
    _capture_sm = pio_rp2xxx::pio0.loadProgram(capture_program);

    // Allocate DMA channels
    _dma_A = dma_rp2xxx::allocateChannel();
    _dma_B = dma_rp2xxx::allocateChannel();

    // Initialize the digital inputs
    for (uint8_t ch=0; ch < LA_D_CHANNEL_COUNT; ch++) {
        _d_channels[ch].setGpio(LA_D_CHANNEL_START + ch);
        _d_channels[ch].gpioMode(GPIO::INPUT | GPIO::PULLUP);
    }
    // Initialize the analog inputs
    // TBD

}

bool la_capture::start(capture_config_t * c) {
    // Check if capture is inactive
    if (_status != IDLE) return false;
    _capture_config = c;
    _is_active = true;

    // Pre trigger samples müssen weniger als 1-2 * BUFFER_SIZE sein!!

    // Configure the capture PIO state machine
    configure_capture(_capture_sm,
                      _capture_config->digital_channels_base,
                      _capture_config->number_of_digital_channels);
    _capture_sm->setClock(_capture_config->sample_rate);

    // Pre-configure the DMA channels
    _dma_A->READ_ADDR       = (uint32_t)&_capture_sm->pio.RXF[_capture_sm->sm_index];
    _dma_A->CTRL.TREQ_SEL   = CH_CTRL_TRIG_TREQ_SEL__PIO0_RX0+_capture_sm->sm_index;
    _dma_A->CTRL.INCR_READ  = false;
    _dma_A->CTRL.INCR_WRITE = true;
    _dma_A->CTRL.CHAIN_TO   = _dma_B->getIndex();
    _dma_A->attachIrq0([&] { dma_irq_handler(true); });

    _dma_B->READ_ADDR       = (uint32_t)&_capture_sm->pio.RXF[_capture_sm->sm_index];
    _dma_B->CTRL.TREQ_SEL   = CH_CTRL_TRIG_TREQ_SEL__PIO0_RX0+_capture_sm->sm_index;
    _dma_B->CTRL.INCR_READ  = false;
    _dma_B->CTRL.INCR_WRITE = true;
    _dma_B->CTRL.CHAIN_TO   = _dma_A->getIndex();
    _dma_B->attachIrq1([&] { dma_irq_handler(false); });

    // Calculate total size of a single sample
    uint8_t sample_size = 1;
    // if (_capture_config->number_of_digital_channels) {
    //     sample_size += _capture_config->digital_sample_size;
    // }
    // if (_capture_config->number_of_analog_channels) {
    //     sample_size += _capture_config->analog_sample_size;
    // }
    _samples_per_buffer = SAMPLE_BUFFER_SIZE / sample_size;
    switch (sample_size) {
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
    TUPP_LOG(LOG_INFO, "Waiting for Triggers");
    wait_for_triggers();
    TUPP_LOG(LOG_INFO, "Triggers detected!!");

    _status = IDLE;
    dma_irq_handler(true);
    dma_irq_handler(false);

    _samples_read = 0;

    _status = PRE_TRIGGER_SAMPLES;
    _dma_A->trigger();

    _capture_sm->enable();

    return true;
}

void la_capture::abort() {
    uint32_t mask = (1 << _dma_A->getIndex()) |
                    (1 << _dma_B->getIndex());
    // Abort DMA channels
    DMA.CHAN_ABORT = mask;
    // Wait until DMA channels are deactivated
    while (_dma_A->CTRL.BUSY ||
           _dma_B->CTRL.BUSY) { }
    // Disable the channels
    _dma_A->CTRL.EN = false;
    _dma_B->CTRL.EN = false;
    // Clear interrupts
    DMA_SET.INTS0 = mask;

    // Disable the data PIO SM
    _capture_sm->disable();
    _capture_sm->pio_set.CTRL.CLKDIV_RESTART = (1 << _capture_sm->sm_index);
    // Clear FIFO
    _capture_sm->regs.SM_SHIFTCTRL.FJOIN_RX = false;
    task::sleep_ms(1);
    _capture_sm->regs.SM_SHIFTCTRL.FJOIN_RX = true;
    // Set PC
    //_capture_sm->setRegister(out_dest_t::PC, _capture_sm->load_addr);
}

void COPY_TO_RAM la_capture::dma_irq_handler(bool is_A) {
    //TUPP_LOG(LOG_INFO, "dma_irq_handler(%b)", is_A);
    if (is_A) {
        if (_status != IDLE) {
            // Process data
            data_handler(_buffer_A, _samples_per_buffer);
            _samples_read += _samples_per_buffer;
        }

        if (_samples_read == _capture_config->post_trigger_samples) {
            _dma_A->CTRL.EN     = false;
            _is_active          = false;
            _capture_sm.reset();
        } else {
            // Buffer A is ready. Prepare it for another round
            _dma_A->WRITE_ADDR  = (uint32_t)_buffer_A;
            _dma_A->TRANS_COUNT = _samples_per_buffer;
            _dma_A->CTRL.EN     = true;
        }
    } else {
        if (_status != IDLE) {
            // Process data
            data_handler(_buffer_B, _samples_per_buffer);
            _samples_read += _samples_per_buffer;
        }

        if (_samples_read == _capture_config->post_trigger_samples) {
            _dma_B->CTRL.EN     = false;
            _is_active          = false;
            _capture_sm.reset();
        } else {
            // Buffer B is ready. Prepare it for another round
            _dma_B->WRITE_ADDR  = (uint32_t)_buffer_B;
            _dma_B->TRANS_COUNT = _samples_per_buffer;
            _dma_B->CTRL.EN     = true;
        }
    }
}

//    is_capturing = is_aborting = capture_completed = false;
//    triggered_channel= -1;
//
//    // Read Trigger type
//    gpio_rp2xxx type_pin(17);
//    type_pin.gpioMode(GPIO::INPUT | GPIO::PULLUP);
//    config.trigger_edge = type_pin.gpioRead();

//void capture::capture_start() {
//    apply_trigger_config();
//
//    LA_LOG(LA_log::LOG_DEBUG, "Sys Clk: %d Clk div: %d", CLK_SYS, CLK_SYS / config.rate);
//
//    // Load mux program on to SM
//    mux_sm = pio_rp2xxx::pio1.loadProgram(mux_program);
//    configure_pio_mux(mux_sm, [this](){
//        // Get Triggered Channel
//        triggered_channel = mux_sm->readRxFifo();
//        // Clear Interrupt
//        mux_sm->pio.IRQ = 1;
//    });
//    LA_LOG(LA_log::LOG_DEBUG, "Mux SM initialized");
//
//    // Allocate DMA channel for disable mux and triggers
//    dma_pio1_ctrl = dma_rp2xxx::allocateChannel();
//    dma_pio1_ctrl->CTRL.EN = true;
//    // Set size of bus transfer
//    dma_pio1_ctrl->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_WORD;
//    // Set read and write increment
//    dma_pio1_ctrl->CTRL.INCR_READ = false;
//    dma_pio1_ctrl->CTRL.INCR_WRITE = false;
//    // Set number of transfers
//    dma_pio1_ctrl->TRANS_COUNT = 1;
//    // Set read and write addresses
//    dma_pio1_ctrl->READ_ADDR = (uint32_t)&pio1_ctrl;
//    dma_pio1_ctrl->WRITE_ADDR = (uint32_t)&_PIO1_::PIO1.CTRL;
//    LA_LOG(LA_log::LOG_DEBUG, "PIO1 control DMA initialized");
//
//    // Allocate DMA channel for disable pre trigger and enable post trigger
//    dma_pio0_ctrl = dma_rp2xxx::allocateChannel();
//    dma_pio0_ctrl->CTRL.EN = true;
//    // Set size of bus transfer
//    dma_pio0_ctrl->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_WORD;
//    // Set read and write increment
//    dma_pio0_ctrl->CTRL.INCR_READ = false;
//    dma_pio0_ctrl->CTRL.INCR_WRITE = false;
//    // Set DREQ
//    // Calculate correct DREQ like the following: PIO + TX + Index
//        // PIO: PIO0 = 0; PIO1 = 8
//        // TX: Send to SM (TX) = 0; Read from SM (RX) = 4
//        // Index: Index of the SM e.g. 0-3
//    // Here: PIO0 + RX + Index = 4 + Index
//    dma_pio0_ctrl->CTRL.TREQ_SEL = (4 + mux_sm->sm_index);
//    // Chain to different DMA channel
//    dma_pio0_ctrl->CTRL.CHAIN_TO = dma_pio1_ctrl->getIndex();
//    // Set number of transfers
//    dma_pio0_ctrl->TRANS_COUNT = 1;
//    // Set read and write addresses
//    dma_pio0_ctrl->READ_ADDR = (uint32_t)&pio0_ctrl;
//    dma_pio0_ctrl->WRITE_ADDR = (uint32_t)&_PIO0_::PIO0.CTRL;
//    // Start channel
//    dma_pio0_ctrl->trigger();
//    LA_LOG(LA_log::LOG_DEBUG, "PIO0 control DMA initialized");
//
//    // Init pre trigger samples
//    // Load capture program for pre trigger samples on to SM
//    pre_trigger_sm = pio_rp2xxx::pio1.loadProgram(capture_program);
//    configure_capture(pre_trigger_sm, config);
//    LA_LOG(LA_log::LOG_DEBUG, "Pre trigger SM initialized");
//
//    // Allocate DMA channel for pre trigger samples
//    dma_pre_trigger = dma_rp2xxx::allocateChannel();
//    dma_pre_trigger->CTRL.EN = true;
//    // Set size of bus transfer
//    dma_pre_trigger->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_HALFWORD;
//    // Enable ring buffer
//    dma_pre_trigger->CTRL.RING_SEL = true;
//    dma_pre_trigger->CTRL.RING_SIZE = PRE_TRIGGER_RING_BITS + 1;
//    // Set read and write increment
//    dma_pre_trigger->CTRL.INCR_READ = false;
//    dma_pre_trigger->CTRL.INCR_WRITE = true;
//    // Set DREQ
//    dma_pre_trigger->CTRL.TREQ_SEL = (4 + pre_trigger_sm->sm_index);
//    // Restart channel at the end of each transfer sequence
//    dma_pre_trigger->attachIrq0([this](){
//        // Clear Interrupt
//        DMA_SET.INTS0 = DMA.INTS0;
//        // Restart Pre-Trigger DMA
//        dma_pre_trigger->trigger();
//    });
//    // Set number of transfers
//    dma_pre_trigger->TRANS_COUNT = PRE_TRIGGER_RING_TRANSFER_COUNT;
//    // Set read and write addresses
//    dma_pre_trigger->READ_ADDR = (uint32_t)&pre_trigger_sm->pio.RXF[pre_trigger_sm->sm_index];
//    dma_pre_trigger->WRITE_ADDR = (uint32_t)&pre_trigger_buffer[0];
//    // Trigger channel
//    dma_pre_trigger->trigger();
//
//    LA_LOG(LA_log::LOG_DEBUG, "Pre trigger DMA initialized");
//
//
//    // Init post trigger samples
//    // Load capture program for post trigger samples on to SM
//    post_trigger_sm = pio_rp2xxx::pio1.loadProgram(capture_program);
//    configure_capture(post_trigger_sm, config);
//    // Set pio0_ctrl
//    pio0_ctrl = (1 << post_trigger_sm->sm_index);
//    LA_LOG(LA_log::LOG_DEBUG, "Post trigger SM initialized");
//
//    // Allocate DMA channel for post trigger samples
//    dma_post_trigger = dma_rp2xxx::allocateChannel();
//    dma_post_trigger->CTRL.EN = true;
//    // Set size of bus transfer
//    dma_post_trigger->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_HALFWORD;
//    // Set read and write increment
//    dma_post_trigger->CTRL.INCR_READ = false;
//    dma_post_trigger->CTRL.INCR_WRITE = true;
//    // Set DREQ
//    dma_post_trigger->CTRL.TREQ_SEL = (4 + post_trigger_sm->sm_index);
//    // Set interrupt for capture complete
//    dma_post_trigger->attachIrq0([this](){capture_complete_handler();});
//    // Set number of transfers
//    //dma_post_trigger->TRANS_COUNT = config.total_samples - config.pre_trigger_samples;
//    dma_post_trigger->TRANS_COUNT = 500;
//    // Set read and write addresses
//    dma_post_trigger->READ_ADDR = (uint32_t)&post_trigger_sm->pio.RXF[post_trigger_sm->sm_index];
//    dma_post_trigger->WRITE_ADDR = (uint32_t)&post_trigger_buffer[0];
//    // Trigger channel
//    dma_post_trigger->trigger();
//    LA_LOG(LA_log::LOG_DEBUG, "Post trigger DMA initialized");
//
//
//    // Init triggers
//    uint i = 0;
//    while (config.triggers[i].is_enabled) {
//        set_trigger(i);
//        i++;
//    }
//
//    // Start state machines
//    if (!i) {
//        // Enable post trigger SM
//        post_trigger_sm->enable();
//        LA_LOG(LA_log::LOG_INFO, "No trigger enabled. Started post trigger SM");
//    } else {
//        // Enable pre trigger SM
//        pre_trigger_sm->enable();
//        // Enable mux SM
//        mux_sm->enable();
//        // Enable trigger SMs
//        for (auto&& sm : trigger_sms) {
//            if (sm != nullptr) {
//                sm->enable();
//            } else {
//                break;
//            }
//        }
//        LA_LOG(LA_log::LOG_INFO, "%d trigger enabled. Started pre trigger, mux and trigger SMs", i);
//    }
//
//    is_capturing = true;
//
//    LA_LOG(LA_log::LOG_INFO, "Capture start. Samples: %d Rate: %d Pre trigger samples: %d", config.total_samples, config.rate, config.pre_trigger_samples);
//}
//
//void Capture::apply_trigger_config() {
//    // Disable all trigger
//    for (int i=0; i < TRIGGER_COUNT; i++) {
//        config.triggers[i].is_enabled = false;
//    }
//
//    uint trigger = 0;
//    for (int stage=0; stage < STAGE_COUNT; stage++) {
//        trigger_stage_config_t stage_conf = config.stages[stage];
//
//        LA_LOG(LA_log::LOG_DEBUG, "Stage: %d Mask: 0x%x Values: 0x%x Configuration: 0x%x", stage, stage_conf.mask, stage_conf.values, stage_conf.configuration);
//
//        if (stage_conf.mask &&
//            ((stage_conf.configuration & TRIGGER_START) &&
//            ((stage_conf.configuration & TRIGGER_LEVEL_MASK) == 0))
//        ) {
//            if (!(stage_conf.configuration & TRIGGER_SERIAL)) {  // Level triggers (parallel trigger)
//                for (uint channel = 0; channel < config.channels; channel++) {
//                    if (((stage_conf.mask >> channel) & 1) != 0) {
//                        config.triggers[trigger].is_enabled = true;
//                        config.triggers[trigger].pin = channel;
//
//                        if (!config.trigger_edge) {
//                            if (((stage_conf.values >> channel) & 1) == 1) {
//                                config.triggers[trigger].match = TRIGGER_TYPE_LEVEL_HIGH;
//                            } else {
//                                config.triggers[trigger].match = TRIGGER_TYPE_LEVEL_LOW;
//                            }
//                        } else {
//                            if (((stage_conf.values >> channel) & 1) == 1) {
//                                config.triggers[trigger].match = TRIGGER_TYPE_EDGE_HIGH;
//                            } else {
//                                config.triggers[trigger].match = TRIGGER_TYPE_EDGE_LOW;
//                            }
//                        }
//
//                        trigger++;
//                    }
//                }
//            } else if ((stage_conf.configuration & TRIGGER_SERIAL) && (stage_conf.mask == 0b11)) {  // Edge triggers (serial, mask 0b11)
//                config.triggers[trigger].is_enabled = true;
//                config.triggers[trigger].pin = (stage_conf.configuration & TRIGGER_CHANNEL_MASK) >> 20;
//
//                if (((stage_conf.values & 1) == 0) && (((stage_conf.values >> 1) & 1) == 1)) {
//                    config.triggers[trigger].match = TRIGGER_TYPE_EDGE_HIGH;
//                    trigger++;
//                } else if (((stage_conf.values & 1) == 1) &&
//                           (((stage_conf.values >> 1) & 1) == 0)) {
//                    config.triggers[trigger].match = TRIGGER_TYPE_EDGE_LOW;
//                    trigger++;
//                } else {
//                    config.triggers[trigger].is_enabled = false;
//                }
//            } else if ((stage_conf.configuration & TRIGGER_SERIAL) && (stage_conf.mask == 0b1)) {
//                config.triggers[trigger].is_enabled = true;
//                config.triggers[trigger].pin =
//                    (stage_conf.configuration & TRIGGER_CHANNEL_MASK) >> 20;
//
//                if ((stage_conf.values & 1) == 1) {
//                    config.triggers[trigger].match = TRIGGER_TYPE_LEVEL_HIGH;
//                    trigger++;
//                } else {
//                    config.triggers[trigger].match = TRIGGER_TYPE_LEVEL_LOW;
//                    trigger++;
//                }
//            }
//
//            if (trigger > TRIGGER_COUNT - 1) {
//                LA_LOG(LA_log::LOG_INFO, "Trigger ignored. Reached maximum number of triggers (%d)", TRIGGER_COUNT);
//                return;
//            }
//        }
//    }
//
//    if (trigger < TRIGGER_COUNT) {
//        config.triggers[trigger].is_enabled = false;
//    }
//}
//
//void Capture::set_trigger(uint i) {
//    if (i < TRIGGER_COUNT) {
//        // Load trigger program on to SM
//        switch (config.triggers[i].match) {
//        case TRIGGER_TYPE_LEVEL_HIGH:
//            trigger_sms[i] = pio_rp2xxx::pio0.loadProgram(trigger_level_high_program);
//            break;
//        case TRIGGER_TYPE_LEVEL_LOW:
//            trigger_sms[i] = pio_rp2xxx::pio0.loadProgram(trigger_level_low_program);
//            break;
//        case TRIGGER_TYPE_EDGE_HIGH:
//            trigger_sms[i] = pio_rp2xxx::pio0.loadProgram(trigger_edge_high_program);
//            break;
//        case TRIGGER_TYPE_EDGE_LOW:
//            trigger_sms[i] = pio_rp2xxx::pio0.loadProgram(trigger_edge_low_program);
//            break;
//        }
//        configure_trigger(trigger_sms[i], config.rate, config.base);
//
//        // Allocate DMA channel for trigger
//        dma_trigger[i] = dma_rp2xxx::allocateChannel();
//        dma_trigger[i]->CTRL.EN = true;
//        // Set size of bus transfer
//        dma_trigger[i]->CTRL.DATA_SIZE = CH_CTRL_TRIG_DATA_SIZE__SIZE_WORD;
//        // Set read and write increment
//        dma_trigger[i]->CTRL.INCR_READ = false;
//        dma_trigger[i]->CTRL.INCR_WRITE = false;
//        // Set DREQ
//        dma_trigger[i]->CTRL.TREQ_SEL = (12 + trigger_sms[i]->sm_index);
//        // Set number of transfers
//        dma_trigger[i]->TRANS_COUNT = 1;
//        // Set read and write addresses
//        dma_trigger[i]->READ_ADDR = (uint32_t)&triggered_channel_index[i];
//        dma_trigger[i]->WRITE_ADDR = (uint32_t)&mux_sm->pio.TXF[mux_sm->sm_index];
//        // Trigger channel
//        dma_trigger[i]->trigger();
//
//        LA_LOG(
//            LA_log::LOG_DEBUG,
//            "Set trigger %d Pin: %d Match: %s %s",
//            i, config.triggers[i].pin, config.triggers[i].match, config.trigger_edge ? "(override)" : ""
//        );
//    }
//}
//
//void Capture::capture_complete_handler() {
//    if (!is_aborting) {
//        pre_trigger_first = 0;
//        pre_trigger_count = 0;
//        if (config.pre_trigger_samples) {
//            uint transfer_count = dma_pre_trigger->TRANS_COUNT;
//            pre_trigger_first = (int)(transfer_count % PRE_TRIGGER_BUFFER_SIZE) - (int)config.pre_trigger_samples;
//            pre_trigger_count = config.pre_trigger_samples;
//            if ((pre_trigger_first < 0) && (transfer_count < PRE_TRIGGER_BUFFER_SIZE)) {
//                pre_trigger_first = 0;
//                pre_trigger_count = transfer_count;
//            }
//        }
//        capture_stop();
//        is_capturing = false;
//        capture_completed = true;
//    } else {
//        is_aborting = false;
//    }
//}
//
//uint Capture::get_sample(uint index) {
//    if (index >= get_sample_count()) return 0;
//
//    if (index < pre_trigger_count) {
//        int pos = pre_trigger_first + index;
//
//        if (pos < 0) {
//            pos += PRE_TRIGGER_BUFFER_SIZE;
//        } else if (pos >= PRE_TRIGGER_BUFFER_SIZE) {
//            pos -= PRE_TRIGGER_BUFFER_SIZE;
//        }
//        return pre_trigger_buffer[pos];
//    }
//
//    return post_trigger_buffer[index - pre_trigger_count];
//   return post_trigger_buffer[index];
//}
//
//void Capture::capture_abort() {
//    is_capturing = false;
//    is_aborting = true;
//    capture_stop();
//}
//
//void Capture::capture_stop() {
//    // Disable all SMs
//    mux_sm->disable();
//    pre_trigger_sm->disable();
//    post_trigger_sm->disable();
//    for (auto&& sm : trigger_sms) {
//        if (sm != nullptr) {
//            sm->disable();
//        }
//    }
//
//    // Free DMA channels
//    dma_pio0_ctrl.reset();
//    dma_pio1_ctrl.reset();
//    dma_pre_trigger.reset();
//    dma_post_trigger.reset();
//    for (auto&& dma : dma_trigger) {
//        if (dma != nullptr) {
//            dma.reset();
//        }
//    }
//
//    // Clear SM FIFOs
//    clear_fifo(mux_sm);
//    clear_fifo(pre_trigger_sm);
//    clear_fifo(post_trigger_sm);
//    for (auto&& sm : trigger_sms) {
//        if (sm != nullptr) {
//            clear_fifo(sm);
//        }
//    }
//}
//
//void Capture::clear_fifo(std::unique_ptr<SM>& sm) {
//    sm->regs.SM_SHIFTCTRL.FJOIN_RX = !(sm->regs.SM_SHIFTCTRL.FJOIN_RX);
//    sm->regs.SM_SHIFTCTRL.FJOIN_RX = !(sm->regs.SM_SHIFTCTRL.FJOIN_RX);
//}