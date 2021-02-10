/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2020 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#ifndef SRSLTE_CHANNEL_H
#define SRSLTE_CHANNEL_H

#include "ch_awgn.h"
#include "delay.h"
#include "fading.h"
#include "hst.h"
#include "rlf.h"
#include "srslte/common/log_filter.h"
#include "srslte/phy/common/phy_common.h"
#include <memory>
#include <string>

namespace srslte {

class channel
{
public:
  typedef struct {
    // General
    bool enable = false;

    // AWGN options
    bool  awgn_enable            = false;
    float awgn_signal_power_dBfs = 0.0f;
    float awgn_snr_dB            = 30.0f;

    // Fading options
    bool        fading_enable = false;
    std::string fading_model  = "none";

    // High Speed Train options
    bool  hst_enable      = false;
    float hst_fd_hz       = 750.0f;
    float hst_period_s    = 7.2f;
    float hst_init_time_s = 0.0f;

    // Delay options
    bool  delay_enable      = false;
    float delay_min_us      = 10;
    float delay_max_us      = 100;
    float delay_period_s    = 3600;
    float delay_init_time_s = 0;

    // RLF options
    bool     rlf_enable   = false;
    uint32_t rlf_t_on_ms  = 10000;
    uint32_t rlf_t_off_ms = 2000;
  } args_t;

  channel(const args_t& channel_args, uint32_t _nof_channels, srslog::basic_logger& logger);
  ~channel();
  void set_srate(uint32_t srate);
  void set_signal_power_dBfs(float power_dBfs);
  void run(cf_t* in[SRSLTE_MAX_CHANNELS], cf_t* out[SRSLTE_MAX_CHANNELS], uint32_t len, const srslte_timestamp_t& t);

private:
  srslog::basic_logger&    logger;
  float                    hst_init_phase              = 0.0f;
  srslte_channel_fading_t* fading[SRSLTE_MAX_CHANNELS] = {};
  srslte_channel_delay_t*  delay[SRSLTE_MAX_CHANNELS]  = {};
  srslte_channel_awgn_t*   awgn                        = nullptr;
  srslte_channel_hst_t*    hst                         = nullptr;
  srslte_channel_rlf_t*    rlf                         = nullptr;
  cf_t*                    buffer_in                   = nullptr;
  cf_t*                    buffer_out                  = nullptr;
  uint32_t                 nof_channels                = 0;
  uint32_t                 current_srate               = 0;
  args_t                   args                        = {};
};

typedef std::unique_ptr<channel> channel_ptr;

} // namespace srslte

#endif // SRSLTE_CHANNEL_H
