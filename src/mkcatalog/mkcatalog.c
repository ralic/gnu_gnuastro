/*********************************************************************
MakeCatalog - Make a catalog from an input and labeled image.
MakeCatalog is part of GNU Astronomy Utilities (Gnuastro) package.

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
along with gnuastro. If not, see <http://www.gnu.org/licenses/>.
**********************************************************************/
#include <config.h>

#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <error.h>
#include <string.h>
#include <stdlib.h>

#include "timing.h"
#include "neighbors.h"
#include "txtarrayvv.h"

#include "main.h"

#include "mkcatalog.h"










/*********************************************************************/
/*****************     Fill information tables     *******************/
/*********************************************************************/
/* In the first pass, the most basic properties (mainly about the
   objects) are found. The primary reason is that we still don't know
   how many objects there are in order to be able to put the clump
   information in the proper place. This could maybe be fixed with a
   linked list in one pass, but that would drastically bring down the
   speed. */
void
firstpass(struct mkcatalogparams *p)
{
  float imgss;
  double *thisobj;
  size_t i, s1=p->s1;
  long *objects=p->objects, *clumps=p->clumps;

  /* Go over the pixels and fill the information array. */
  for(i=0;i<p->s0*p->s1;++i)
    if(objects[i]>0)
      {
        /* thisobj is a pointer to the start of the row in the object
           information array (oinfo). It is mainly used to keep things
           short, simple, less bugy and most importantly elegant. */
        imgss = p->img[i] - p->sky[i];
        thisobj = p->oinfo + objects[i]*OCOLUMNS;

        /* Add to the flux weighted center: */
        ++thisobj[OAREA];
        thisobj[ OSKY ]     += p->sky[i];
        thisobj[ OSTD ]     += p->std[i];
        thisobj[ OTotFlux ] += imgss;
        thisobj[ OFlxWhtX ] += imgss * (i%s1);
        thisobj[ OFlxWhtY ] += imgss * (i/s1);

        if(clumps[i]>0)
          {
            /* The largest clump ID over each object is the number of
               clumps that object has. */
            thisobj[ ONCLUMPS ] = ( clumps[i] > thisobj[ONCLUMPS]
                                    ? clumps[i] : thisobj[ONCLUMPS] );

            /* Find the flux weighted center of the clumps. */
            ++thisobj [ OAREAC ];
            thisobj[ OTotFluxC ] += imgss;
            thisobj[ OFlxWhtCX ] += imgss * (i%s1);
            thisobj[ OFlxWhtCY ] += imgss * (i/s1);
          }
      }

  /* Divide the total flux by the sum of flux*positions and add with
     on to get the flux weighted center in the FITS standard. */
  for(i=1;i<=p->numobjects;++i)
    {
      thisobj = p->oinfo + i*OCOLUMNS;
      thisobj[ OFlxWhtX ] = thisobj[ OFlxWhtX ] / thisobj[ OTotFlux ] + 1;
      thisobj[ OFlxWhtY ] = thisobj[ OFlxWhtY ] / thisobj[ OTotFlux ] + 1;
      if(thisobj[ OTotFluxC ]==0.0f)
        thisobj[ OFlxWhtCX ] = thisobj[ OFlxWhtCY ] = NAN;
      else
        {
          thisobj[ OFlxWhtCX ] = ( thisobj[ OFlxWhtCX ]
                                   / thisobj[ OTotFluxC ] + 1 );
          thisobj[ OFlxWhtCY ] = ( thisobj[ OFlxWhtCY ]
                                   / thisobj[ OTotFluxC ] + 1 );
        }
    }
}





/* In the second pass, we have the number of clumps so we can find
   store the total values for the clumps. In this second round we can
   also find second order moments of the objects if we want to. */
void
secondpass(struct mkcatalogparams *p)
{
  long wngb[WNGBSIZE];
  size_t ii, *n, *nf, numngb, ngb[8];
  double *thisclump, *cinfo=p->cinfo;
  long *objects=p->objects, *clumps=p->clumps;
  float *img=p->img, *sky=p->sky, *std=p->std;
  size_t i, j, *ind, *ofcrow, row=0, is0=p->s0, is1=p->s1;



  /* The job of ofcrow (object-first-clump-row) is to give the row
     number of the first clump within an object in the clumps
     information table. This value can be added with the clumpid in
     order to give the row number in the clumps information table. */
  errno=0; ofcrow=malloc((p->numobjects+1)*sizeof *ofcrow);
  if(ofcrow==NULL)
    error(EXIT_FAILURE, errno, "%lu bytes for ofcrow in seconpass "
          "(mkcatalog.c)", (p->numobjects+1)*sizeof *ofcrow);


  /* Fill ofcrow using the known number of clumps in each object. Note
     that while ofcrow counts from zero, the clump[i] values (which
     are added with this later) are added with one. So the clump
     information will start from the second row (with index 1) of the
     clumps array, not the first (with index 0). */
  for(i=1;i<=p->numobjects;++i)
    if(p->oinfo[i*OCOLUMNS+ONCLUMPS]>0.0f)
      {
        ofcrow[i]=row;
        row+=p->oinfo[i*OCOLUMNS+ONCLUMPS];
      }

  /* Go over all the pixels in the image and fill in the clump
     information: */
  for(i=0;i<p->s0*p->s1;++i)
    {
      /* We are on a clump, save its properties. */
      if(clumps[i]>0)
        {
          /* This pointer really simplifies things below! */
          thisclump = ( cinfo
                        + ( ofcrow[objects[i]] + clumps[i] )
                        * CCOLUMNS );

          /* Fill in this clump information:  */
          ++thisclump[ CAREA ];
          thisclump[ CSKY ]      += sky[i];
          thisclump[ CSTD ]      += std[i];
          thisclump[ CTotFlux ]  += img[i];
          thisclump[ CINHOSTID ]  = clumps[i];
          thisclump[ CHOSTOID ]   = objects[i];
          thisclump[ CFlxWhtX ]  += img[i] * (i%is1);
          thisclump[ CFlxWhtY ]  += img[i] * (i/is1);
        }

      /* We are on a detected region but not a clump (with a negative
         label). This region can be used to find properties like the
         river fluxs in the vicinity of clumps. */
      else
        /* We want to check the river pixels in each object that has a
           clump. So first see if we are on an object (clumps[i]<0),
           then see if this object has any clumps at all. We don't
           want to go around the neighbors of each non-clump pixel in
           objects that don't have any clumps ;-).

           The process that keeps the labels of the clumps that have
           already been used for each river pixel in wngb is inherited
           from the getclumpinfo function in NoiseChisel's clump.c.
        */
        if(clumps[i]<0 && p->oinfo[objects[i]*OCOLUMNS+ONCLUMPS] > 0.0f)
          {
            /* Make the preparations: */
            ii=0;
            ind=&i;
            FILL_NGB_8_ALLIMG;
            nf=(n=ngb)+numngb;
            memset(wngb, 0, sizeof(wngb));

            /* Go over the neighbors and add the flux of this river
               pixel to a clump's information if it has not been added
               already. */
            do
              if(clumps[*n]>0)
                {
                  /* Go over wngb to see if this river pixel's value
                     has been added to a segment or not. */
                  for(j=0;j<ii;++j)
                    if(wngb[j]==clumps[*n])
                      /* It is already present. break out. */
                      break;
                  if(j==ii) /* First time we are seeing this clump */
                    {       /* for this river pixel.               */
                      cinfo[ ( ofcrow[objects[i]] + clumps[*n] )
                             * CCOLUMNS + CTotRivFlux ] += img[i];
                      ++cinfo[ ( ofcrow[objects[i]] + clumps[*n] )
                               * CCOLUMNS + CRivArea ];
                      wngb[ii]=clumps[*n];
                      ++ii;
                    }
                }
            while(++n<nf);
          }
    }

  /* Divide by total flux to get the flux weighted center: */
  for(i=1;i<=p->numclumps;++i)
    {
      thisclump = p->cinfo + i*CCOLUMNS;
      thisclump[ CFlxWhtX ] = thisclump[ CFlxWhtX ]/thisclump[ CTotFlux ] + 1;
      thisclump[ CFlxWhtY ] = thisclump[ CFlxWhtY ]/thisclump[ CTotFlux ] + 1;
    }

  /* Clean up: */
  free(ofcrow);
}




















/*********************************************************************/
/**************         Different operations           ****************/
/*********************************************************************/
void
idcol(struct mkcatalogparams *p)
{
  size_t i;

  p->unitp=CATUNITCOUNTER;
  sprintf(p->description, "%lu: Overall %s ID",
          p->curcol, p->name);

  for(i=0;i<p->num;++i)
    p->cat[ i * p->numcols + p->curcol ] = i+1;

  p->intcols[p->intcounter++]=p->curcol;
}





/* Store IDs related to the host object:

   o1c0==1 --> hostobjid: The ID of object hosting this clump
   o1c0==0 --> idinhostobj: The ID of clump in object
 */
void
hostobj(struct mkcatalogparams *p, int o1c0)
{
  char *des;
  double counter;
  size_t i, j, n, row=0;

  /* This function is only for clumps. */
  if(p->obj0clump1==0) return;

  p->unitp=CATUNITCOUNTER;
  des = ( o1c0 ? "ID of object hosting this clump"
          : "ID of clump in host object" );
  sprintf(p->description, "%lu: %s.",
          p->curcol, des);

  for(i=1;i<=p->numobjects;++i)
    if( (n=p->oinfo[i*OCOLUMNS+ONCLUMPS]) > 0.0f)
      {
        counter=1.0f;
        for(j=row;j<row+n;++j)
          p->cat[ j * p->numcols + p->curcol] = o1c0 ? i : counter++;
        row+=n;
      }

  p->intcols[p->intcounter++]=p->curcol;
}





void
numclumps(struct mkcatalogparams *p)
{
  size_t i;

  /* This function is only for objects. */
  if(p->obj0clump1) return;

  p->unitp=CATUNITCOUNTER;
  sprintf(p->description, "%lu: Number of clumps in this object.",
          p->curcol);

  for(i=0;i<p->numobjects;++i)
    p->cat[i * p->numcols + p->curcol ] = p->oinfo[(i+1)*OCOLUMNS+ONCLUMPS];

  p->intcols[p->intcounter++]=p->curcol;
}





void
area(struct mkcatalogparams *p, int cinobj)
{
  char *type;
  size_t i, col = p->obj0clump1 ? CAREA : OAREA;

  /* Set the proper column to use */
  if(p->obj0clump1) /* It is a clump.                            */
    {
      type="This clump";
      col = CAREA;
    }
  else
    {               /* It is an object.                         */
      if(cinobj)    /* It is the positions of clumps in object. */
        {
          type="Clumps in object";
          col = OAREAC;
        }
      else          /* It is the position of the object itsself.*/
        {
          type="Full object";
          col = OAREA;
        }
    }

  p->unitp=CATUNITPIXAREA;
  sprintf(p->description, "%lu: %s area.", p->curcol, type);

  for(i=0;i<p->num;++i)
    p->cat[i * p->numcols + p->curcol ] = p->info[(i+1)*p->icols+col];

  p->intcols[p->intcounter++]=p->curcol;
}





/* Find the positions:

   x1y1==1: X axis.

   cinobj==1: (only relevant when obj0clump1==0): clump in obj info,
              not full object.

   w1i0==1: We are dealing with world coordinates not image
            coordinates. In the world coordinates x: ra and y: dec
 */
void
position(struct mkcatalogparams *p, int w1i0, int x1y0, int cinobj)
{
  char *type;
  size_t i, pcol;

  /* Set the proper column to use */
  if(p->obj0clump1) /* It is a clump.                            */
    {
      type="This clump";
      if(w1i0)   pcol = x1y0 ? CFlxWhtRA : CFlxWhtDec;
      else       pcol = x1y0 ? CFlxWhtX : CFlxWhtY;
    }
  else
    {               /* It is an object.                         */
      if(cinobj)    /* It is the positions of clumps in object. */
        {
          type="Clumps in object";
          if(w1i0) pcol = x1y0 ? OFlxWhtCRA : OFlxWhtCDec;
          else     pcol = x1y0 ? OFlxWhtCX : OFlxWhtCY;
        }
      else          /* It is the position of the object itsself.*/
        {
          type="Full object";
          if(w1i0) pcol = x1y0 ? OFlxWhtRA : OFlxWhtDec;
          else     pcol = x1y0 ? OFlxWhtX : OFlxWhtY;
        }
    }


  p->unitp = w1i0 ? CATUNITDEGREE : CATUNITPIXPOS;
  sprintf(p->description, "%lu: %s flux weighted center (%s).",
          p->curcol, type,
          w1i0 ? (x1y0?"RA":"Dec") : (x1y0?"X":"Y") );


  for(i=0;i<p->num;++i)
    p->cat[i * p->numcols + p->curcol ] = p->info[(i+1)*p->icols+pcol];


  if(w1i0)
    p->accucols[p->accucounter++]=p->curcol;
}




















/*********************************************************************/
/*****************           Make output           *******************/
/*********************************************************************/
void
makeoutput(struct mkcatalogparams *p)
{
  size_t *cols;
  char comment[COMMENTSIZE];
  int prec[2]={p->floatprecision, p->accuprecision};
  int space[3]={p->intwidth, p->floatwidth, p->accuwidth};

  /* First make the objects catalog, then the clumps catalog. */
  for(p->obj0clump1=0;p->obj0clump1<2;++p->obj0clump1)
    {


      /* Do the preparations for this round: */
      p->intcounter=p->accucounter=p->curcol=0;
      p->name     = p->obj0clump1 ? "Clump" : "Object";
      p->icols    = p->obj0clump1 ? CCOLUMNS : OCOLUMNS;
      p->info     = p->obj0clump1 ? p->cinfo : p->oinfo;
      p->cat      = p->obj0clump1 ? p->clumpcat : p->objcat;
      p->filename = p->obj0clump1 ? p->ccatname : p->ocatname;
      cols        = p->obj0clump1 ? p->clumpcols : p->objcols;
      p->numcols  = p->obj0clump1 ? p->clumpncols : p->objncols;
      p->num      = p->obj0clump1 ? p->numclumps : p->numobjects;



      /* Allocate the integer and accuracy arrays: */
      errno=0; p->intcols=malloc(p->numcols*sizeof *p->intcols);
      if(p->intcols==NULL)
        error(EXIT_FAILURE, errno, "%lu bytes for intcols in makeoutput "
              "(mkcatalog.c)", p->numcols*sizeof *p->intcols);
      errno=0; p->accucols=malloc(p->numcols*sizeof *p->accucols);
      if(p->accucols==NULL)
        error(EXIT_FAILURE, errno, "%lu bytes for accucols in makeoutput "
              "(mkcatalog.c)", p->numcols*sizeof *p->accucols);



      /* Write the top of the comments: */
      sprintf(comment, "# %s %s Catalog.\n", SPACK_STRING, p->name);
      sprintf(p->line, "# %s started on %s", SPACK_NAME, ctime(&p->rawtime));
      strcat(comment, p->line);



      /* Fill the catalog array, in the end set the last elements in
         intcols and accucols to -1, so arraytotxt knows when to
         stop. */
      for(p->curcol=0;p->curcol<p->numcols;++p->curcol)
        {
          switch(cols[p->curcol])
            {

            case CATID:
              idcol(p);
              break;

            case CATHOSTOBJID:
              hostobj(p, 1);
              break;

            case CATIDINHOSTOBJ:
              hostobj(p, 0);
              break;

            case CATNUMCLUMPS:
              numclumps(p);
              break;

            case CATAREA:
              area(p, 0);
              break;

            case CATCLUMPSAREA:
              area(p, 1);
              break;

            case CATX:
              position(p, 0, 1, 0);
              break;

            case CATY:
              position(p, 0, 0, 0);
              break;

            case CATCLUMPSX:
              position(p, 0, 1, 1);
              break;

            case CATCLUMPSY:
              position(p, 0, 0, 1);
              break;

            case CATRA:
              position(p, 1, 1, 0);
              break;

            case CATDEC:
              position(p, 1, 0, 0);
              break;

            case CATCLUMPSRA:
              position(p, 1, 1, 1);
              break;

            case CATCLUMPSDEC:
              position(p, 1, 0, 1);
              break;

            default:
              error(EXIT_FAILURE, 0, "A bug! Please contact us at %s so we "
                    "can fix the problem. The value to cols[%lu] (%lu), is "
                    "not recognized in makeoutput (mkcatalog.c).",
                    PACKAGE_BUGREPORT, p->curcol, cols[p->curcol]);
            }

          sprintf(p->line, "# "CATDESCRIPTLENGTH"[%s]\n",
                  p->description, p->unitp);
          strcat(comment, p->line);
        }
      p->intcols[p->intcounter]=p->accucols[p->accucounter]=-1;



      /* Write the catalog to file: */
      arraytotxt(p->cat, p->num, p->numcols, comment, p->intcols,
                 p->accucols, space, prec, 'f', p->filename);

      /* Clean up: */
      free(p->intcols);
      free(p->accucols);
    }
}




















/*********************************************************************/
/*****************          Main function          *******************/
/*********************************************************************/
void
mkcatalog(struct mkcatalogparams *p)
{
  /* Allocate two arrays to keep all the basic information about each
     object and clump. Note that there should be one row more than the
     total number of objects or clumps. This is because we want each
     label to be its row number and we don't have any object label of
     zero.*/
  errno=0; p->oinfo=calloc(OCOLUMNS*(p->numobjects+1), sizeof *p->oinfo);
  if(p->oinfo==NULL)
    error(EXIT_FAILURE, errno, "%lu bytes for p->oinfo in mkcatalog "
          "(mkcatalog.c)", OCOLUMNS*(p->numobjects+1)*sizeof *p->oinfo);
  errno=0; p->cinfo=calloc(CCOLUMNS*(p->numclumps+1), sizeof *p->cinfo);
  if(p->cinfo==NULL)
    error(EXIT_FAILURE, errno, "%lu bytes for p->cinfo in mkcatalog "
          "(mkcatalog.c)", CCOLUMNS*(p->numclumps+1)*sizeof *p->cinfo);


  /* Run through the data for the first time: */
  firstpass(p);
  secondpass(p);

  /* If world coordinates are needed, then do the
     transformations. Note that we are passing the pointer to the
     first X axis value so this function can immediately start calling
     wcsp2s in wcslib. */
  if(p->up.raset || p->up.decset)
    {
      xyarraytoradec(p->wcs, p->oinfo+OCOLUMNS+OFlxWhtX,
                     p->oinfo+OCOLUMNS+OFlxWhtRA, p->numobjects,
                     OCOLUMNS);
      xyarraytoradec(p->wcs, p->cinfo+CCOLUMNS+CFlxWhtX,
                     p->cinfo+CCOLUMNS+CFlxWhtRA, p->numclumps,
                     CCOLUMNS);
    }

  if(p->up.clumpsraset || p->up.clumpsdecset)
    xyarraytoradec(p->wcs, p->oinfo+OCOLUMNS+OFlxWhtCX,
                   p->oinfo+OCOLUMNS+OFlxWhtCRA, p->numobjects,
                   OCOLUMNS);

  /* Write the output: */
  makeoutput(p);

  /* Clean up: */
  free(p->oinfo);
  free(p->cinfo);
}