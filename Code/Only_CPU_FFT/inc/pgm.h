/**
 * @file pgm.h
 * @brief Saving the simulation board as PGM images.
 *
 * PGM (Portable GrayMap, binary "P5" variant) is a very simple greyscale
 * image format: a short text header followed by one byte per pixel.
 */
#ifndef PGM_H
#define PGM_H

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Creates the output folder, including any missing parent folders.
 *
 * Does nothing if the folder already exists.
 *
 * @param folder Path to the folder, absolute or relative to the current
 *               working directory.
 * @return 0 on success, 1 on error (an error message is printed).
 */
int init_output(const char *folder);

/**
 * @brief Saves the board as `<folder>/frame_<frame_number>.pgm`.
 *
 * The frame number is padded with zeros to 5 digits (e.g. frame_00120.pgm),
 * so the files sort in the right order. A cell value of 0.0 becomes black and
 * 1.0 becomes white.
 *
 * @param folder            Output folder; it must already exist
 *                          (see init_output()).
 * @param frame_number      Number used in the file name, usually the
 *                          iteration number.
 * @param current_board_prt Pointer to the first cell of a square board stored
 *                          row by row, e.g. `&current_board[0][0]`.
 * @param size_fo_board     Width (and height) of the board in cells.
 * @return 0 on success, 1 if the file could not be created.
 */
int save_board_frame(const char *folder, int frame_number,
                     const double *current_board_prt, size_t size_fo_board);

#endif