/*********************************************************************
Statistics - Statistical analysis on input dataset.
Statistics is part of GNU Astronomy Utilities (Gnuastro) package.

Original author:
     Mohammad Akhlaghi <akhlaghi@gnu.org>
Contributing author(s):
Copyright (C) 2015, Free Software Foundation, Inc.

Gnuastro is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version.

Gnuastro is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with Gnuastro. If not, see <http://www.gnu.org/licenses/>.
**********************************************************************/
#include <config.h>

#include <stdio.h>
#include <errno.h>
#include <error.h>
#include <string.h>
#include <stdlib.h>

#include <gnuastro/fits.h>
#include <gnuastro/qsort.h>
#include <gnuastro/blank.h>
#include <gnuastro/threads.h>
#include <gnuastro/convolve.h>
#include <gnuastro/statistics.h>
#include <gnuastro/interpolate.h>

#include <gnuastro-internal/timing.h>
#include <gnuastro-internal/checkset.h>

#include "main.h"





static void *
sky_on_thread(void *in_prm)
{
  struct gal_threads_params *tprm=(struct gal_threads_params *)in_prm;
  struct statisticsparams *p=(struct statisticsparams *)tprm->params;

  double *darr;
  int stype=p->sky_t->type;
  void *tblock=NULL, *tarray=NULL;
  gal_data_t *tile, *mode, *sigmaclip;
  size_t i, tind, twidth=gal_type_sizeof(stype);


  /* Find the Sky and its standard deviation on the tiles given to this
     thread. */
  for(i=0; tprm->indexs[i] != GAL_BLANK_SIZE_T; ++i)
    {
      /* Set the tile and copy its values into the array we'll be using. */
      tind = tprm->indexs[i];
      tile = &p->cp.tl.tiles[tind];


      /* If we have a convolved image, temporarily (only for finding the
         mode) change the tile's pointers so we can work on the convolved
         image for the mode. */
      if(p->kernel)
        {
          tarray=tile->array; tblock=tile->block;
          tile->array=gal_tile_block_relative_to_other(tile, p->convolved);
          tile->block=p->convolved;
        }
      mode=gal_statistics_mode(tile, p->mirrordist, 1);
      if(p->kernel) { tile->array=tarray; tile->block=tblock; }


      /* Check the mode value. Note that if the mode is in-accurate, then
         the values will be NaN and all conditionals will fail. So, we'll
         go onto finding values for this tile */
      darr=mode->array;
      if( fabs(darr[1]-0.5f) < p->modmedqdiff )
        {
          /* Get the sigma-clipped mean and standard deviation. `inplace'
             is irrelevant here because this is a tile and it will be
             copied anyway. */
          sigmaclip=gal_statistics_sigma_clip(tile, p->sclipparams[0],
                                              p->sclipparams[1], 1, 1);

          /* Put the mean and its standard deviation into the respective
             place for this tile. */
          sigmaclip=gal_data_copy_to_new_type_free(sigmaclip, stype);
          memcpy(gal_data_ptr_increment(p->sky_t->array, tind, stype),
                 gal_data_ptr_increment(sigmaclip->array, 2, stype), twidth);
          memcpy(gal_data_ptr_increment(p->std_t->array, tind, stype),
                 gal_data_ptr_increment(sigmaclip->array, 3, stype), twidth);

          /* Clean up. */
          gal_data_free(sigmaclip);
        }
      else
        {
          gal_blank_write(gal_data_ptr_increment(p->sky_t->array, tind,
                                                 stype), stype);
          gal_blank_write(gal_data_ptr_increment(p->std_t->array, tind,
                                                 stype), stype);
        }

      /* Clean up. */
      gal_data_free(mode);
    }


  /* Wait for all threads to finish and return. */
  if(tprm->b) pthread_barrier_wait(tprm->b);
  return NULL;
}





void
sky(struct statisticsparams *p)
{
  char *msg, *outname;
  struct timeval t0, t1;
  gal_data_t *num, *tmp;
  struct gal_options_common_params *cp=&p->cp;
  struct gal_tile_two_layer_params *tl=&cp->tl;


  /* Print basic information */
  if(!cp->quiet)
    {
      gettimeofday(&t0, NULL);
      printf("%s\n", PROGRAM_STRING);
      printf("Estimating Sky (reference value) and its STD.\n");
      printf("-----------\n");
      printf("  - Using %zu CPU thread%s.\n", cp->numthreads,
             cp->numthreads==1 ? "" : "s");
      printf("  - Input: %s (hdu: %s)\n", p->inputname, cp->hdu);
      if(p->kernelname)
        printf("  - Kernel: %s (hdu: %s)\n", p->kernelname, p->khdu);
    }

  /* When checking steps, the input image is the first extension. */
  if(p->checksky)
    gal_fits_img_write(p->input, p->checkskyname, NULL, PROGRAM_STRING);


  /* Convolve the image (if desired). */
  if(p->kernel)
    {
      if(!cp->quiet) gettimeofday(&t1, NULL);
      p->convolved=gal_convolve_spatial(tl->tiles, p->kernel,
                                        cp->numthreads, 1, tl->workoverch);
      if(p->checksky)
        gal_fits_img_write(p->convolved, p->checkskyname, NULL,
                           PROGRAM_STRING);
      if(!cp->quiet)
        gal_timing_report(&t1, "Input convolved with kernel.", 1);
    }


  /* Make the arrays keeping the Sky and Sky standard deviation values. */
  p->sky_t=gal_data_alloc(NULL, GAL_TYPE_FLOAT32, p->input->ndim,
                          tl->numtiles, NULL, 0, p->input->minmapsize,
                          "SKY", p->input->unit, NULL);
  p->std_t=gal_data_alloc(NULL, GAL_TYPE_FLOAT32, p->input->ndim,
                          tl->numtiles, NULL, 0, p->input->minmapsize,
                          "SKY STD", p->input->unit, NULL);



  /* Find the Sky and Sky standard deviation on the tiles. */
  if(!cp->quiet) gettimeofday(&t1, NULL);
  gal_threads_spin_off(sky_on_thread, p, tl->tottiles, cp->numthreads);
  if(!cp->quiet)
    {
      num=gal_statistics_number(p->sky_t);
      asprintf(&msg, "Sky and its STD found on %zu/%zu tiles.",
               (size_t)(*((uint64_t *)(num->array))), tl->tottiles );
      gal_timing_report(&t1, msg, 1);
      gal_data_free(num);
      free(msg);
    }
  if(p->checksky)
    {
      gal_tile_full_values_write(p->sky_t, tl, p->checkskyname, NULL,
                                 PROGRAM_STRING);
      gal_tile_full_values_write(p->std_t, tl, p->checkskyname, NULL,
                                 PROGRAM_STRING);
    }


  /* Interpolate the Sky and its standard deviation. */
  if(!cp->quiet) gettimeofday(&t1, NULL);
  p->sky_t->next=p->std_t;
  tmp=gal_interpolate_close_neighbors(p->sky_t, tl, cp->interpnumngb,
                                      cp->numthreads, cp->interponlyblank, 1);
  gal_data_free(p->sky_t);
  gal_data_free(p->std_t);
  p->sky_t=tmp;
  p->std_t=tmp->next;
  p->sky_t->next=p->std_t->next=NULL;
  if(!cp->quiet)
    gal_timing_report(&t1, "All blank tiles filled (interplated).", 1);
  if(p->checksky)
    {
      gal_tile_full_values_write(p->sky_t, tl, p->checkskyname, NULL,
                                 PROGRAM_STRING);
      gal_tile_full_values_write(p->std_t, tl, p->checkskyname, NULL,
                                 PROGRAM_STRING);
    }


  /* Smooth the Sky and Sky STD arrays. */
  if(p->smoothwidth>1)
    {
      if(!cp->quiet) gettimeofday(&t1, NULL);
      tmp=gal_tile_full_values_smooth(p->sky_t, tl, p->smoothwidth,
                                      p->cp.numthreads);
      gal_data_free(p->sky_t);
      p->sky_t=tmp;
      tmp=gal_tile_full_values_smooth(p->std_t, tl, p->smoothwidth,
                                      p->cp.numthreads);
      gal_data_free(p->std_t);
      p->std_t=tmp;
      if(!cp->quiet)
        gal_timing_report(&t1, "Smoothed Sky and Sky STD values on tiles.",
                          1);
      if(p->checksky)
        {
          gal_tile_full_values_write(p->sky_t, tl, p->checkskyname, NULL,
                                     PROGRAM_STRING);
          gal_tile_full_values_write(p->std_t, tl, p->checkskyname, NULL,
                                     PROGRAM_STRING);
        }
    }


  /* Save the Sky and its standard deviation */
  outname=gal_checkset_automatic_output(&p->cp,
                                        ( p->cp.output
                                          ? p->cp.output
                                          : p->inputname ), "_sky.fits");
  gal_tile_full_values_write(p->sky_t, tl, outname, NULL, PROGRAM_STRING);
  gal_tile_full_values_write(p->std_t, tl, outname, NULL, PROGRAM_STRING);
  if(!cp->quiet)
    printf("  - Written to `%s'.\n", outname);


  /* Clean up and return. */
  free(outname);
  gal_data_free(p->sky_t);
  gal_data_free(p->std_t);
  gal_data_free(p->convolved);

  if(!cp->quiet)
    {
      printf("-----------\n");
      gal_timing_report(&t0, "Completed in:", 0);
      printf("-----------\n");
    }
}
