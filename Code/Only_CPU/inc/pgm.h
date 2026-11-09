#ifndef PGM_H
#define PGM_H

#include <stdio.h>
#include <stdlib.h>

int init_output(const char *folder);
int save_board_frame(const char *folder, int frame_number,
                     const double *current_board_prt, size_t size_fo_board);

#endif