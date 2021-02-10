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

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "srslte/srslte.h"

srslte_cell_t cell = {
    6,              // nof_prb
    1,              // nof_ports
    1000,           // cell_id
    SRSLTE_CP_NORM, // cyclic prefix
    SRSLTE_PHICH_NORM,
    SRSLTE_PHICH_R_1, // PHICH length
    SRSLTE_FDD,

};

char* output_matlab = NULL;

void usage(char* prog)
{
  printf("Usage: %s [recov]\n", prog);

  printf("\t-r nof_prb [Default %d]\n", cell.nof_prb);
  printf("\t-e extended cyclic prefix [Default normal]\n");

  printf("\t-c cell_id (1000 tests all). [Default %d]\n", cell.id);

  printf("\t-o output matlab file [Default %s]\n", output_matlab ? output_matlab : "None");
  printf("\t-v increase verbosity\n");
}

void parse_args(int argc, char** argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "recov")) != -1) {
    switch (opt) {
      case 'r':
        cell.nof_prb = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'e':
        cell.cp = SRSLTE_CP_EXT;
        break;
      case 'c':
        cell.id = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'o':
        output_matlab = argv[optind];
        break;
      case 'v':
        srslte_verbose++;
        break;
      default:
        usage(argv[0]);
        exit(-1);
    }
  }
}

int main(int argc, char** argv)
{
  srslte_chest_ul_t est;
  cf_t *            input = NULL, *ce = NULL, *h = NULL;
  int               i, j, n_port = 0, sf_idx = 0, cid = 0;
  int               ret = -1;
  int               max_cid;
  FILE*             fmatlab = NULL;

  parse_args(argc, argv);

  if (output_matlab) {
    fmatlab = fopen(output_matlab, "w");
    if (!fmatlab) {
      perror("fopen");
      goto do_exit;
    }
  }

  uint32_t num_re = 2U * cell.nof_prb * SRSLTE_NRE * SRSLTE_CP_NSYMB(cell.cp);

  input = srslte_vec_cf_malloc(num_re);
  if (!input) {
    perror("srslte_vec_malloc");
    goto do_exit;
  }
  h = srslte_vec_cf_malloc(num_re);
  if (!h) {
    perror("srslte_vec_malloc");
    goto do_exit;
  }
  ce = srslte_vec_cf_malloc(num_re);
  if (!ce) {
    perror("srslte_vec_malloc");
    goto do_exit;
  }
  srslte_vec_cf_zero(ce, num_re);

  if (cell.id == 1000) {
    cid     = 0;
    max_cid = 504;
  } else {
    cid     = cell.id;
    max_cid = cell.id;
  }
  printf("max_cid=%d, cid=%d, cell.id=%d\n", max_cid, cid, cell.id);
  if (srslte_chest_ul_init(&est, cell.nof_prb)) {
    ERROR("Error initializing equalizer");
    goto do_exit;
  }
  while (cid <= max_cid) {
    cell.id = cid;
    if (srslte_chest_ul_set_cell(&est, cell)) {
      ERROR("Error initializing equalizer");
      goto do_exit;
    }

    for (int n = 6; n <= cell.nof_prb; n += 5) {
      if (srslte_dft_precoding_valid_prb(n)) {
        for (int delta_ss = 29; delta_ss < SRSLTE_NOF_DELTA_SS; delta_ss++) {
          for (int cshift = 7; cshift < SRSLTE_NOF_CSHIFT; cshift++) {
            for (int t = 2; t < 3; t++) {
              /* Setup and pregen DMRS reference signals */
              srslte_refsignal_dmrs_pusch_cfg_t pusch_cfg;

              uint32_t nof_prb         = n;
              pusch_cfg.cyclic_shift   = cshift;
              pusch_cfg.delta_ss       = delta_ss;
              bool group_hopping_en    = false;
              bool sequence_hopping_en = false;

              if (!t) {
                group_hopping_en    = false;
                sequence_hopping_en = false;
              } else if (t == 1) {
                group_hopping_en    = false;
                sequence_hopping_en = true;
              } else if (t == 2) {
                group_hopping_en    = true;
                sequence_hopping_en = false;
              }
              pusch_cfg.group_hopping_en    = group_hopping_en;
              pusch_cfg.sequence_hopping_en = sequence_hopping_en;
              srslte_chest_ul_pregen(&est, &pusch_cfg, NULL);

              // Loop through subframe idx and cyclic shifts

              for (int sf_idx = 0; sf_idx < 10; sf_idx += 3) {
                for (int cshift_dmrs = 0; cshift_dmrs < SRSLTE_NOF_CSHIFT; cshift_dmrs += 5) {
                  if (SRSLTE_VERBOSE_ISINFO()) {
                    printf("nof_prb: %d, ", nof_prb);
                    printf("cyclic_shift: %d, ", pusch_cfg.cyclic_shift);
                    printf("cyclic_shift_for_dmrs: %d, ", cshift_dmrs);
                    printf("delta_ss: %d, ", pusch_cfg.delta_ss);
                    printf("SF_idx: %d\n", sf_idx);
                  }

                  /* Generate random input */
                  srslte_vec_cf_zero(input, num_re);
                  for (i = 0; i < num_re; i++) {
                    input[i] = 0.5 - rand() / (float)RAND_MAX + I * (0.5 - rand() / (float)RAND_MAX);
                  }

                  /* Generate channel and pass input through channel */
                  for (i = 0; i < 2 * SRSLTE_CP_NSYMB(cell.cp); i++) {
                    for (j = 0; j < cell.nof_prb * SRSLTE_NRE; j++) {
                      float x = -1 + (float)i / SRSLTE_CP_NSYMB(cell.cp) +
                                cosf(2 * M_PI * (float)j / cell.nof_prb / SRSLTE_NRE);
                      h[i * cell.nof_prb * SRSLTE_NRE + j] = (3 + x) * cexpf(I * x);
                      input[i * cell.nof_prb * SRSLTE_NRE + j] *= h[i * cell.nof_prb * SRSLTE_NRE + j];
                    }
                  }

                  // Configure estimator
                  srslte_chest_ul_res_t res;
                  srslte_pusch_cfg_t    cfg;

                  ZERO_OBJECT(cfg);
                  res.ce                   = ce;
                  cfg.grant.L_prb          = n;
                  cfg.grant.n_prb_tilde[0] = 0;
                  cfg.grant.n_prb_tilde[1] = 0;
                  cfg.grant.n_dmrs         = cshift_dmrs;

                  srslte_ul_sf_cfg_t ul_sf;
                  ZERO_OBJECT(ul_sf);
                  ul_sf.tti = sf_idx;

                  // Estimate channel
                  srslte_chest_ul_estimate_pusch(&est, &ul_sf, &cfg, input, &res);

                  // Compute MSE
                  float mse = 0;
                  for (i = 0; i < num_re; i++) {
                    mse += cabsf(ce[i] - h[i]);
                  }
                  mse /= num_re;
                  INFO("MSE: %f", mse);
                  if (mse > 4) {
                    goto do_exit;
                  }
                }
              }
            }
          }
        }
      }
    }
    cid += 10;
    printf("cid=%d\n", cid);
  }

  srslte_chest_ul_free(&est);

  if (fmatlab) {
    fprintf(fmatlab, "input=");
    srslte_vec_fprint_c(fmatlab, input, num_re);
    fprintf(fmatlab, ";\n");
    fprintf(fmatlab, "h=");
    srslte_vec_fprint_c(fmatlab, h, num_re);
    fprintf(fmatlab, ";\n");
    fprintf(fmatlab, "ce=");
    srslte_vec_fprint_c(fmatlab, ce, num_re);
    fprintf(fmatlab, ";\n");
  }

  ret = 0;

do_exit:

  if (ce) {
    free(ce);
  }
  if (input) {
    free(input);
  }
  if (h) {
    free(h);
  }

  if (!ret) {
    printf("OK\n");
  } else {
    printf("Error at cid=%d, slot=%d, port=%d\n", cid, sf_idx, n_port);
  }

  exit(ret);
}
