/**
 * @file  gifti_local.c
 * @brief local utilities for GIFTI library
 *
 * This file has some some extra functions for use with the GIFTI
 * utilities. The official utilities reside in gifti_io.c and gifti_xml.c
 *
 */
/*
 * Original Authors: Kevin Teich and Nick Schmansky
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2010/10/27 18:16:16 $
 *    $Revision: 1.24.2.1 $
 *
 * Copyright (C) 2007-2010,
 * The General Hospital Corporation (Boston, MA).
 * All rights reserved.
 *
 * Distribution, usage and copying of this software is covered under the
 * terms found in the License Agreement file named 'COPYING' found in the
 * FreeSurfer source code root directory, and duplicated here:
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferOpenSourceLicense
 *
 * General inquiries: freesurfer@nmr.mgh.harvard.edu
 *
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <pwd.h>
#include <time.h>

#include "error.h" // return codes
#include "utils.h" // strcpyalloc
#include "nifti1.h"
#include "gifti_local.h"

/*
 *
 */
static giiDataArray* gifti_alloc_and_add_darray (gifti_image* image)
{
  if (!image)
  {
    fprintf (stderr,"** gifti_alloc_and_add_darray: NULL image\n");
    return NULL;
  }

  /* Try to add an empty array. */
  if (gifti_add_empty_darray(image,1))
  {
    fprintf (stderr,"** gifti_alloc_and_add_darray: gifti_add_empty_darray "
             "failed\n");
    return NULL;
  }

  /* Return the array we just allocated. */
  return image->darray[image->numDA-1];
}


/*
 *
 */
static double gifti_get_DA_value_2D (giiDataArray* da, int row, int col)
{
  int dim0_index, dim1_index;
  int dims_0=0, dims_1=0;

  if (!da || !da->data)
  {
    fprintf (stderr,"** gifti_get_DA_value_2D, invalid params: data=%p\n",
             da);
    exit(1);
  }

  if (da->num_dim == 1)
  {
    // support for using this routine to read 1D data, under one condition...
    if (col != 0)
    {
      fprintf (stderr,"** gifti_get_DA_value_2D, array dim is 1 "
               "but trying to access 2D data element (col=%d)\n",col);
      exit(1);
    }
    dims_0 = da->dims[0];
    dims_1 = 1; // 1D data
  }
  else if (da->num_dim != 2)
  {
    fprintf (stderr,"** gifti_get_DA_value_2D, array dim is %d\n",
             da->num_dim);
    exit(1);
  }
  else
  {
    dims_0 = da->dims[0];
    dims_1 = da->dims[1];
  }

  /* Get the dim0 and dims[1] indices based on our order. */
  if (GIFTI_IND_ORD_ROW_MAJOR == da->ind_ord)
  {
    dim0_index = row;
    dim1_index = col;
  }
  else if (GIFTI_IND_ORD_COL_MAJOR == da->ind_ord)
  {
    dim0_index = col;
    dim1_index = row;
  }
  else
  {
    fprintf (stderr,"** gifti_get_DA_value_2D, unknown ind_ord: %d\n",
             da->ind_ord);
    exit(1);
  }
  if (da->num_dim == 1) // support for using this routine to read 1D data
  {
    dim0_index = row;
    dim1_index = col;
  }

  /* Check the indices. */
  if (dim0_index < 0 || dim0_index >= dims_0 ||
      dim1_index < 0 || dim1_index >= dims_1)
  {
    fprintf(stderr,"** gifti_get_DA_value_2D, invalid params: "
            "dim0_index=%d (max=%d), dim1_index=%d (max=%d)\n",
            dim0_index, dims_0, dim1_index, dims_1);
    exit(1);
  }

  /* Switch on the data type and return the appropriate
     element. Indexing depends on the data order. */
  switch (da->datatype)
  {
  default :
    fprintf(stderr,"** gifti_get_DA_value_2D, unsupported type %d-"
            "unknown, or can't convert to double\n",da->datatype);
    exit(1);
  case NIFTI_TYPE_UINT8:
  {
    if ( GIFTI_IND_ORD_ROW_MAJOR == da->ind_ord )
      return (double)*((unsigned char*)
                       (da->data) + (dim0_index*dims_1) + dim1_index);
    else
                              return (double)*((unsigned char*)
                       (da->data) + dim0_index + (dim1_index*dims_0));
    break;
  }
  case NIFTI_TYPE_INT16:
  {
    if ( GIFTI_IND_ORD_ROW_MAJOR == da->ind_ord )
      return (double)*((short*)
                       (da->data) + (dim0_index*dims_1) + dim1_index);
    else
      return (double)*((short*)
                       (da->data) + dim0_index + (dim1_index*dims_0));
    break;
  }
  case NIFTI_TYPE_INT32:
  {
    if ( GIFTI_IND_ORD_ROW_MAJOR == da->ind_ord )
      return (double)*((int*)
                       (da->data) + (dim0_index*dims_1) + dim1_index);
    else
      return (double)*((int*)
                       (da->data) + dim0_index + (dim1_index*dims_0));
    break;
  }
  case NIFTI_TYPE_FLOAT32:
  {
    if ( GIFTI_IND_ORD_ROW_MAJOR == da->ind_ord )
      return (double)*((float*)
                       (da->data) + (dim0_index*dims_1) + dim1_index);
    else
      return (double)*((float*)
                       (da->data) + dim0_index + (dim1_index*dims_0));
    break;
  }
  case NIFTI_TYPE_INT8:
  {
    if ( GIFTI_IND_ORD_ROW_MAJOR == da->ind_ord )
      return (double)*((char*)
                       (da->data) + (dim0_index*dims_1) + dim1_index);
    else
      return (double)*((char*)
                       (da->data) + dim0_index + (dim1_index*dims_0));
    break;
  }
  case NIFTI_TYPE_UINT16:
  {
    if ( GIFTI_IND_ORD_ROW_MAJOR == da->ind_ord )
      return (double)*((unsigned short*)
                       (da->data) + (dim0_index*dims_1) + dim1_index);
    else
      return (double)*((unsigned short*)
                       (da->data) + dim0_index + (dim1_index*dims_0));
    break;
  }
  case NIFTI_TYPE_UINT32:
  {
    if ( GIFTI_IND_ORD_ROW_MAJOR == da->ind_ord )
      return (double)*((unsigned int*)
                       (da->data) + (dim0_index*dims_1) + dim1_index);
    else
      return (double)*((unsigned int*)
                       (da->data) + dim0_index + (dim1_index*dims_0));
    break;
  }
  }

  exit(1);
}


/*
 *
 */
static void gifti_set_DA_value_2D (giiDataArray* da,
                                   int row, int col, double value)
{
  int dim0_index, dim1_index;
  int dims_0=0, dims_1=0;

  if (!da || !da->data)
  {
    fprintf (stderr,"** gifti_set_DA_value_2D, invalid params: data=%p\n",
             da);
    exit(1);
  }

  if (da->num_dim == 1)
  {
    // support for using this routine to write 1D data, under one condition...
    if (col != 0)
    {
      fprintf (stderr,"** gifti_set_DA_value_2D, array dim is 1 "
               "but trying to access 2D data element (col=%d)\n",col);
      exit(1);
    }
    dims_0 = da->dims[0];
    dims_1 = 1; // 1D data
  }
  else if (da->num_dim != 2)
  {
    fprintf (stderr,"** gifti_set_DA_value_2D, array dim is %d\n",
             da->num_dim);
    exit(1);
  }
  else
  {
    dims_0 = da->dims[0];
    dims_1 = da->dims[1];
  }

  /* Get the dim0 and dims[1] indices based on our order. */
  if (GIFTI_IND_ORD_ROW_MAJOR == da->ind_ord)
  {
    dim0_index = row;
    dim1_index = col;
  }
  else
  {
    dim0_index = col;
    dim1_index = row;
  }
  if (da->num_dim == 1) // support for using this routine to read 1D data
  {
    dim0_index = row;
    dim1_index = col;
  }

  /* Check the indices. */
  if (dim0_index < 0 || dim0_index >= dims_0 ||
      dim1_index < 0 || dim1_index >= dims_1)
  {
    fprintf(stderr,"** gifti_set_DA_value_2D, invalid params: "
            "dim0_index=%d (max=%d), dim1_index=%d (max=%d)\n",
            dim0_index, dims_0, dim1_index, dims_1);
    return;
  }

  /* Switch on the data type and write the appropriate
     element. Indexing depends on the data order. */
  switch (da->datatype)
  {
  default :
    fprintf(stderr,"** gifti_set_DA_value_2D, unsupported type %d-"
            "unknown, or can't convert to double\n",da->datatype);
    return;
  case NIFTI_TYPE_UINT8:
  {
    if ( GIFTI_IND_ORD_ROW_MAJOR == da->ind_ord )
      *((unsigned char*)(da->data) + (dim0_index*dims_1) + dim1_index) =
        (unsigned char)value;
    else
      *((unsigned char*)(da->data) + dim0_index + (dim1_index*dims_0)) =
        (unsigned char)value;
    break;
  }
  case NIFTI_TYPE_INT16:
  {
    if ( GIFTI_IND_ORD_ROW_MAJOR == da->ind_ord )
      *((short*)(da->data) + (dim0_index*dims_1) + dim1_index) =
        (short)value;
    else
      *((short*)(da->data) + dim0_index + (dim1_index*dims_0)) =
        (short)value;
    break;
  }
  case NIFTI_TYPE_INT32:
  {
    if ( GIFTI_IND_ORD_ROW_MAJOR == da->ind_ord )
      *((int*)(da->data) + (dim0_index*dims_1) + dim1_index) =
        (int)value;
    else
      *((int*)(da->data) + dim0_index + (dim1_index*dims_0)) =
        (int)value;
    break;
  }
  case NIFTI_TYPE_FLOAT32:
  {
    if ( GIFTI_IND_ORD_ROW_MAJOR == da->ind_ord )
      *((float*)(da->data) + (dim0_index*dims_1) + dim1_index) =
        (float)value;
    else
      *((float*)(da->data) + dim0_index + (dim1_index*dims_0)) =
        (float)value;
    break;
  }
  case NIFTI_TYPE_INT8:
  {
    if ( GIFTI_IND_ORD_ROW_MAJOR == da->ind_ord )
      *((char*)(da->data) + (dim0_index*dims_1) + dim1_index) =
        (char)value;
    else
      *((char*)(da->data) + dim0_index + (dim1_index*dims_0)) =
        (char)value;
    break;
  }
  case NIFTI_TYPE_UINT16:
  {
    if ( GIFTI_IND_ORD_ROW_MAJOR == da->ind_ord )
      *((unsigned short*)(da->data) + (dim0_index*dims_1) + dim1_index) =
        (unsigned short)value;
    else
      *((unsigned short*)(da->data) + dim0_index + (dim1_index*dims_0)) =
        (unsigned short)value;
    break;
  }
  case NIFTI_TYPE_UINT32:
  {
    if ( GIFTI_IND_ORD_ROW_MAJOR == da->ind_ord )
      *((unsigned int*)(da->data) + (dim0_index*dims_1) + dim1_index) =
        (unsigned int)value;
    else
      *((unsigned int*)(da->data) + dim0_index + (dim1_index*dims_0)) =
        (unsigned int)value;
    break;
  }
  }

  return;
}



/*-------------------------------------------------------------------
  Parameters:    input file name of GIFTI file
                 optional mris structure to store data found
                 optional data array number to read

  Returns value: freesurfer surface structure

  Description:   reads a GIFTI file, putting vertices, 
                 and faces into an MRIS_SURFACE structure,
                 along with any other data, like labels,
                 colors, curv data, stats or values.
                 if daNum is not -1, then read only the 
                 data in data array number daNum
  -------------------------------------------------------------------*/
MRIS *mrisReadGIFTIdanum(const char *fname, MRIS *mris, int daNum)
{
  /* 
   * attempt to read the file
   */
  gifti_image* image = gifti_read_image (fname, 1);
  if (NULL == image)
  {
    fprintf (stderr,"mrisReadGIFTIfile: gifti_read_image() returned NULL\n");
    return NULL;
  }

  /* 
   * check for compliance 
   */
  int valid = gifti_valid_gifti_image (image, 1);
  if (valid == 0)
  {
    fprintf (stderr,"mrisReadGIFTIfile: GIFTI file %s is invalid!\n", fname);
    gifti_free_image (image);
    return NULL;
  }

  /*
   * check for 'LabelTable' data and read into our colortable if exists
   */
  COLOR_TABLE* ct = NULL;
  if (image->labeltable.length > 0)
  {
    /* check validity of labeltable data */
    if (!gifti_valid_LabelTable(&image->labeltable,1))
    {
      fprintf 
        (stderr,
         "mrisReadGIFTIfile: invalid labeltable found in file %s\n", 
         fname);
      gifti_free_image (image);
      return NULL;
    }

    /* copy label table contents to our color_table struct */
    ct = (COLOR_TABLE *)calloc(1, sizeof(COLOR_TABLE));
    if (ct == NULL)
    {
      fprintf 
        (stderr,
         "mrisReadGIFTIfile: could not alloc colortable memory\n");
      gifti_free_image (image);
      return NULL;
    }
    memset(ct,0,sizeof(COLOR_TABLE));
    ct->nentries = image->labeltable.length;
    ct->version = 2;
    ct->entries = (COLOR_TABLE_ENTRY**)
      calloc(ct->nentries+1, sizeof(COLOR_TABLE_ENTRY*));
    if (ct->entries == NULL)
    {
      fprintf 
        (stderr,
         "mrisReadGIFTIfile: could not alloc colortable entries\n");
      gifti_free_image (image);
      return NULL;
    }
    memset(ct->entries,0,sizeof(ct->entries));
    strncpy(ct->fname, fname, sizeof(ct->fname));

    float* rgba = image->labeltable.rgba;
    if (NULL == rgba)
    {
      // optional rgba values are missing, so we must create colors for 
      // the labels
      image->labeltable.rgba = 
        (float *)calloc(image->labeltable.length, 4*sizeof(float *));
      if (NULL == image->labeltable.rgba)
      {
        fprintf (stderr,"mrisReadGIFTIfile: "
                 "couldn't allocate memory for labeltable.rgba\n");
        return NULL;
      }
      rgba = image->labeltable.rgba;
      setRandomSeed(12);// so that color generation is consistent
      int label_index;
      for (label_index = 0; 
           label_index < image->labeltable.length;
           label_index++)
      {
        rgba[0] = (float)randomNumber(0.0f,1.0f);
        rgba[1] = (float)randomNumber(0.0f,1.0f);
        rgba[2] = (float)randomNumber(0.0f,1.0f);
        rgba[3] = 1.0f;
        rgba += 4;
      }
    }

    rgba = image->labeltable.rgba;
    int label_index;
    for (label_index = 0; 
         label_index < image->labeltable.length; 
         label_index++)
    {
      ct->entries[label_index] = (CTE*) malloc(sizeof(CTE));
      if (ct->entries[label_index] == NULL)
      {
        fprintf 
          (stderr,
           "mrisReadGIFTIfile: could not alloc colortable entry\n");
        gifti_free_image (image);
        return NULL;
      }
      strncpy(ct->entries[label_index]->name,
              image->labeltable.label[label_index],
              sizeof(ct->entries[label_index]->name));

      ct->entries[label_index]->rf = rgba[0];
      ct->entries[label_index]->ri = floor((rgba[0])*256);
      if (ct->entries[label_index]->ri > 255) ct->entries[label_index]->ri=255;
      ct->entries[label_index]->gf = rgba[1];
      ct->entries[label_index]->gi = floor((rgba[1])*256);
      if (ct->entries[label_index]->gi > 255) ct->entries[label_index]->gi=255;
      ct->entries[label_index]->bf = rgba[2];
      ct->entries[label_index]->bi = floor((rgba[2])*256);
      if (ct->entries[label_index]->bi > 255) ct->entries[label_index]->bi=255;
      ct->entries[label_index]->af = rgba[3];
      ct->entries[label_index]->ai = floor((rgba[3])*256);
      if (ct->entries[label_index]->ai > 255) ct->entries[label_index]->ai=255;
      rgba += 4;
      /*
        printf("RGBA: %d %d %d %d %f %f %f %f\n",
           ct->entries[label_index]->ri,
           ct->entries[label_index]->gi,
           ct->entries[label_index]->bi,
           ct->entries[label_index]->ai,
           ct->entries[label_index]->rf,
           ct->entries[label_index]->gf,
           ct->entries[label_index]->bf,
           ct->entries[label_index]->af);
      */
    }
    ct->entries[label_index] = NULL;
    CTABfindDuplicateNames(ct);

    // we're done, except that the colortable struct 'ct' will get stored
    // in the mris structure element 'ct' at the end of this routine, after
    // mris is known to exist
  }
  // end of LabelTable parsing (into colortable)


  /*
   * Now parse the DataArrays looking for coordinate and face data arrays,
   * so that we can create our mris structure
   */
  giiDataArray* coords = NULL;
  giiDataArray* faces = NULL;
  int numDA;
  for (numDA = 0; numDA < image->numDA; numDA++)
  {
    if (image->darray[numDA]->intent == NIFTI_INTENT_POINTSET)
    {
      coords = image->darray[numDA];
    }
    else if (image->darray[numDA]->intent == NIFTI_INTENT_TRIANGLE)
    {
      faces = image->darray[numDA];
    }
  }

  /*
   * if we found coordinate and face data....create mris struct and fill it
   */
  if (coords && faces)
  {    
    /* Check the number of vertices and faces. */
    long long num_vertices = 0;
    long long num_cols = 0;
    gifti_DA_rows_cols (coords, &num_vertices, &num_cols);
    if (num_vertices <= 0 || num_cols != 3)
    {
      fprintf (stderr,"mrisReadGIFTIfile: malformed coords data array in file "
               "%s: num_vertices=%d num_cols=%d\n",
               fname, (int)num_vertices, (int)num_cols);
      gifti_free_image (image);
      return NULL;
    }
    long long num_faces = 0;
    num_cols = 0;
    gifti_DA_rows_cols (faces, &num_faces, &num_cols);
    if (num_faces <= 0 || num_cols != 3)
    {
      fprintf (stderr,"mrisReadGIFTIfile: malformed faces data array in file "
               "%s: num_faces=%d num_cols=%d\n",
               fname, (int)num_faces, (int)num_cols);
      gifti_free_image (image);
      return NULL;
    }

    /* Try to allocate a surface. */
    mris = MRISalloc (num_vertices, num_faces);
    if (NULL == mris)
    {
      fprintf (stderr,"mrisReadGIFTIfile: failed to allocate an MRIS with "
               "%d vertices and %d faces\n",
               (int)num_vertices,(int) num_faces);
      gifti_free_image (image);
      return NULL;
    }

    /* Set some meta data in the mris. */
    strcpy(mris->fname,fname);
    mris->type = MRIS_TRIANGULAR_SURFACE;
    char* hemi = gifti_get_meta_value(&coords->meta,
                                      "AnatomicalStructurePrimary'");
    if (hemi && (strcmp(hemi,"CortexRight")==0))
      mris->hemisphere = RIGHT_HEMISPHERE ;
    else if (hemi && (strcmp(hemi,"CortexLeft")==0))
      mris->hemisphere = LEFT_HEMISPHERE ;
    else
      mris->hemisphere = NO_HEMISPHERE;

    /* Copy in the vertices. */
    float x, y, z, xhi, xlo, yhi, ylo, zhi, zlo ;
    xhi=yhi=zhi= -1000000;
    xlo=ylo=zlo= 1000000;
    int vertex_index;
    for (vertex_index = 0; vertex_index < num_vertices; vertex_index++)
    {
      mris->vertices[vertex_index].x =
        (float) gifti_get_DA_value_2D (coords, vertex_index, 0);
      mris->vertices[vertex_index].y =
        (float) gifti_get_DA_value_2D (coords, vertex_index, 1);
      mris->vertices[vertex_index].z =
        (float) gifti_get_DA_value_2D (coords, vertex_index, 2);
      mris->vertices[vertex_index].num = 0;
      mris->vertices[vertex_index].origarea = -1;
      x = mris->vertices[vertex_index].x;
      y = mris->vertices[vertex_index].y;
      z = mris->vertices[vertex_index].z;
      if (x>xhi) xhi=x;
      if (x<xlo) xlo=x;
      if (y>yhi) yhi=y;
      if (y<ylo) ylo=y;
      if (z>zhi) zhi=z;
      if (z<zlo) zlo=z;
    }
    mris->xlo = xlo ;
    mris->ylo = ylo ;
    mris->zlo = zlo ;
    mris->xhi = xhi ;
    mris->yhi = yhi ;
    mris->zhi = zhi ;
    mris->xctr = (xhi+xlo)/2;
    mris->yctr = (yhi+ylo)/2;
    mris->zctr = (zhi+zlo)/2;

    /* Copy in the faces. */
    int face_index;
    for (face_index = 0; face_index < num_faces; face_index++)
    {
      int face_vertex_index;
      for (face_vertex_index = 0;
           face_vertex_index < VERTICES_PER_FACE;
           face_vertex_index++)
      {
        vertex_index =
          (int) gifti_get_DA_value_2D (faces, face_index, face_vertex_index);
        mris->faces[face_index].v[face_vertex_index] = vertex_index;
        mris->vertices[vertex_index].num++;
      }
    }
    // each vertex has a face list (faster than face list in some operations)
    for (vertex_index = 0; vertex_index < num_vertices; vertex_index++)
    {
      mris->vertices[vertex_index].f =
        (int *)calloc(mris->vertices[vertex_index].num,sizeof(int));
      mris->vertices[vertex_index].n =
        (uchar *)calloc(mris->vertices[vertex_index].num,sizeof(uchar));
      mris->vertices[vertex_index].num = 0; // this gets re-calc'd next...
    }
    for (face_index = 0 ; face_index < mris->nfaces ; face_index++)
    {
      FACE *face = &mris->faces[face_index] ;
      int n;
      for (n = 0 ; n < VERTICES_PER_FACE ; n++)
        mris->vertices[face->v[n]].f[mris->vertices[face->v[n]].num++]
          = face_index; // note that .num is auto-incremented!
    }
    for (vertex_index = 0; vertex_index < num_vertices; vertex_index++)
    {
      int n,m;
      for (n=0;n<mris->vertices[vertex_index].num;n++)
      {
        for (m=0;m<VERTICES_PER_FACE;m++)
        {
          if (mris->faces[mris->vertices[vertex_index].f[n]].v[m] == 
              vertex_index)
            mris->vertices[vertex_index].n[n] = m;
        }
      }
    }

    /* other data structure essentials, namely:
     *  mrisFindNeighbors(mris);
     *  mrisComputeVertexDistances(mris);
     *  mrisReadTransform(mris, fname) ;
     *  mris->radius = MRISaverageRadius(mris) ;
     *  MRIScomputeMetricProperties(mris) ;
     *  MRISstoreCurrentPositions(mris) ;
     */
    MRIScomputeNormals(mris);
    UpdateMRIS(mris,fname);
  }
  // completed parsing of coordinate and face data

  // sanity-check, we ought to have an mris struct (either passed-in as a
  // parameter, or created when we found coord and face data)
  if (NULL == mris)
  {
    fprintf 
      (stderr,
       "mriseadGIFTIfile: mris is NULL! found when parsing file %s\n",fname);
    gifti_free_image (image);
    return NULL;
  }

  /*
   * and dont forget to store the colortable (if one was found)
   */
  if (ct)
  {
    mris->ct = ct;
    // sanity-check
    int numEntries=0;
    CTABgetNumberOfValidEntries(mris->ct,&numEntries);
    if (numEntries != image->labeltable.length)
    {
      fprintf 
        (stderr,
         "mrisReadGIFTIfile: ct_entries:%d != labeltable_entries:%d\n", 
         numEntries, image->labeltable.length);
      gifti_free_image (image);
      return NULL;
    }
  }


  /*
   * Now re-parse the DataArrays looking for all the other data type (except
   * coordinate and face data arrays) and fill-in mris structure as needed.
   */
  int found_curv_data = 0; // track if multiple shape data arrays exist
  int found_statval_data = 0; // track if multiple stat/val data arrays exist
  giiDataArray* node_index = NULL; // support for sparse data storage
  long long num_index_nodes = 0; // support for sparse data storage
  int startDAnum = 0;
  int endDAnum = image->numDA;
  if (daNum != -1) // support for extracting one particular data array
  {
    startDAnum = daNum;
    endDAnum = daNum+1;
  }
  for (numDA = startDAnum; numDA < endDAnum; numDA++)
  {
    giiDataArray* darray = image->darray[numDA];

    // did these already
    if ((darray->intent == NIFTI_INTENT_POINTSET) ||
        (darray->intent == NIFTI_INTENT_TRIANGLE)) continue;

    /* support for sparse data storage.  this array contains a list of node 
       numbers and it should be the first data array in the file. The remaining
       data arrays in the file that contain data assigned to nodes must contain
       the same number of elements as the NIFTI_INTENT_NODE_INDEX array. */
    if (darray->intent == NIFTI_INTENT_NODE_INDEX)
    {
      if (numDA != 0)
      {
        fprintf 
          (stderr,
           "mrisReadGIFTIfile: NODE_INDEX data array found but its not the "
           "first data array in file %s\n", fname);
        gifti_free_image (image);
        return NULL;
      }
      long long num_cols = 0;
      gifti_DA_rows_cols (darray, &num_index_nodes, &num_cols);
      if (num_index_nodes <= 0 ||
          num_index_nodes > mris->nvertices ||
          num_cols > 1)
      {
        fprintf 
          (stderr,
           "mrisReadGIFTIfile: malformed NODE_INDEX data array in file %s: "
           "num_index_nodes=%d num_cols=%d max nvertices=%d, num_cols>1\n",
           fname, (int)num_index_nodes, (int)num_cols, mris->nvertices);
        gifti_free_image (image);
        return NULL;
      }
      // else good to do, so store this node index info
      node_index = darray;
      continue;
    }
    else
    {
      /* Check the number of vertices, so we dont trounce the mris struct */
      long long num_vertices = 0;
      long long num_cols = 0;
      long long expected_num_cols = 1;
      if (darray->intent == NIFTI_INTENT_VECTOR)
        expected_num_cols = 3;
      else if (darray->intent == NIFTI_INTENT_RGB_VECTOR)
        expected_num_cols = 3;
      else if (darray->intent == NIFTI_INTENT_RGBA_VECTOR)
        expected_num_cols = 4;
      else if (darray->intent == NIFTI_INTENT_GENMATRIX)
        expected_num_cols = 9;
      gifti_DA_rows_cols (darray, &num_vertices, &num_cols);
      if (num_vertices <= 0 ||
          num_vertices != mris->nvertices ||
          num_cols > expected_num_cols)
      {
        fprintf 
          (stderr,
           "mrisReadGIFTIfile: malformed data array [%d] in file %s: "
           "num_vertices=%d num_cols=%d expected nvertices=%d, num_cols=%d\n",
           numDA, fname, (int)num_vertices, 
           (int)num_cols, mris->nvertices, (int)expected_num_cols);
        gifti_free_image (image);
        return NULL;
      }
    }


    /* 
     * parse each intent type 
     */
    if (darray->intent == NIFTI_INTENT_SHAPE)
    {
      // 'shape' data goes in our 'curv' data element of mris
      if (found_curv_data)
      {
        fprintf(stderr,
                "WARNING: a prior data array of shape data has already "
                "been read!  Skipping data in array #%d in file %s\n",
                numDA, fname);
      }
      else
      {
        found_curv_data++;

        if (node_index) // sparse data storage
        {
          int nindex;
          for (nindex = 0; nindex < num_index_nodes; nindex++)
          {
            int vno = gifti_get_DA_value_2D (node_index, nindex, 0);
            if (mris->vertices[vno].ripflag) continue;
            mris->vertices[vno].curv = 
              (float) gifti_get_DA_value_2D (darray, nindex, 0);
          }
        }
        else // regular indexing
        {
          int vno;
          for (vno = 0; vno < mris->nvertices; vno++)
          {
            if (mris->vertices[vno].ripflag) continue;
            mris->vertices[vno].curv = 
              (float) gifti_get_DA_value_2D (darray, vno, 0);
          }
        }
      }
    }
    else if (darray->intent == NIFTI_INTENT_LABEL)
    {
      // 'label' data goes into the 'annotation' data element of mris
      if ((NULL == mris->ct) || (NULL == ct)) // sanity-check
      {
        fprintf(stderr,"mrisReadGIFTIfile: NULL colortable\n");
        gifti_free_image (image);
        return NULL;
      }
      unsigned int* label_data = darray->data;
      int nindex = 0; // index into node_index (if sparse data storage is used)
      int da_index = 0; // index into the data array at hand
      int vno = 0; // index into the mris struct (vertex number)
      while (vno < mris->nvertices)
      {
        if (node_index) // sparse data storage support
        {
          vno = gifti_get_DA_value_2D (node_index, nindex, 0);
          da_index = nindex;
        }
        else // regular indexing
        {
          da_index = vno;
        }

        if (mris->vertices[vno].ripflag) continue;
        int table_key = *(label_data + da_index);
        int table_index = 0;
        for (table_index = 0; table_index < ct->nentries; table_index++)
        {
          if (table_key == image->labeltable.key[table_index])
          {
            // found the label key for this node
            break;
          }
        }
        int annotation = 0; // default to no label found
        if ((table_index < ct->nentries) && (table_index >= 0)) 
        {
          //printf("vno: %d, tidx: %d, name: %s\n",
          //     vno,table_index,ct->entries[table_index]->name);
          annotation = CTABrgb2Annotation(ct->entries[table_index]->ri,
                                          ct->entries[table_index]->gi,
                                          ct->entries[table_index]->bi);
        }
        mris->vertices[vno].annotation = annotation;

        // cross-check:
        int index = -1;
        int result = CTABfindAnnotation(mris->ct,
                                        mris->vertices[vno].annotation,
                                        &index);
        if ((result != NO_ERROR) || 
            (index < 0) || 
            (index > image->labeltable.length))
        {
          fprintf 
            (stderr,
             "mrisReadGIFTIfile: label node data not found in colortable! "
             "vno: %d, annot: %8.8X\n",
             vno, mris->vertices[vno].annotation);
          gifti_free_image (image);
          return NULL;
        }

        if (node_index) // sparse data storage support
        {
          if (++nindex >= num_index_nodes) break;
        }
        else // regular indexing
        {
          vno++;
        }
      }
    }
    else if (darray->intent == NIFTI_INTENT_VECTOR)
    {
      // 'vector' data goes in our 'dx,dy,dz' data element of mris
      int vno;
      for (vno = 0; vno < mris->nvertices; vno++)
      {
        if (mris->vertices[vno].ripflag) continue;
        mris->vertices[vno].dx =
          (float) gifti_get_DA_value_2D (darray, vno, 0);
        mris->vertices[vno].dy =
          (float) gifti_get_DA_value_2D (darray, vno, 1);
        mris->vertices[vno].dz =
          (float) gifti_get_DA_value_2D (darray, vno, 2);
      }
    }
    else if ((darray->intent == NIFTI_INTENT_RGB_VECTOR) ||
             (darray->intent == NIFTI_INTENT_RGBA_VECTOR))
    {
      // 'rgba' data goes in our 'annotation' data element of mris
      int vno;
      for (vno = 0; vno < mris->nvertices; vno++)
      {
        if (mris->vertices[vno].ripflag) continue;
        int r,g,b;

        float red = (float) gifti_get_DA_value_2D (darray, vno, 0);
        float green = (float) gifti_get_DA_value_2D (darray, vno, 0);
        float blue = (float) gifti_get_DA_value_2D (darray, vno, 0);

        if (red > 1) r = (int)red; else r = (int)floor(red*256);
        if (r > 255) r = 255;
        if (green > 1) g = (int)green; else g = (int)floor(green*256);
        if (g > 255) g = 255;
        if (blue > 1) b = (int)blue; else b = (int)floor(blue*256);
        if (b > 255) b = 255;

        MRISRGBToAnnot(r,g,b,mris->vertices[vno].annotation);
      }
    }
    else if (darray->intent == NIFTI_INTENT_GENMATRIX)
    {
      fprintf(stderr,
              "WARNING: ignoring unsupported data array NIFTI_INTENT_GENMATRIX"
              " in file %s\n", fname);
    }
    else
    {
      // 'statistics' and all other kinds of data we'll put in both our 
      // 'stat' and 'val' data elements of the mris structure
      if (found_statval_data)
      {
        fprintf(stderr,
                "WARNING: a prior data array of stat/val data has already "
                "been read!  Skipping data in array #%d in file %s\n",
                numDA, fname);
      }
      else
      {
        found_statval_data++;

        if (node_index) // sparse data storage
        {
          int nindex;
          for (nindex = 0; nindex < num_index_nodes; nindex++)
          {
            int vno = gifti_get_DA_value_2D (node_index, nindex, 0);
            if (mris->vertices[vno].ripflag) continue;
            mris->vertices[vno].val = 
              (float) gifti_get_DA_value_2D (darray, nindex, 0);
            mris->vertices[vno].stat = 
              (float) gifti_get_DA_value_2D (darray, nindex, 0);
          }
        }
        else // regular indexing
        {
          int vno;
          for (vno = 0; vno < mris->nvertices; vno++)
          {
            if (mris->vertices[vno].ripflag) continue;
            mris->vertices[vno].val = 
              (float) gifti_get_DA_value_2D (darray, vno, 0);
            mris->vertices[vno].stat = 
              (float) gifti_get_DA_value_2D (darray, vno, 0);
          }
        }
      }
    }
  }

  /* 
   * And we're done. 
   */
  gifti_free_image (image);

  return mris;
}

MRI_SURFACE * mrisReadGIFTIfile(const char *fname, MRI_SURFACE *mris)
{
  // default read routine (read all data arrays)
  return mrisReadGIFTIdanum(fname, mris, -1);
}


/*-----------------------------------------------------------
  MRISreadGiftiAsMRI() - reads GIFTI functional frames into
  an MRI volume struct, which is a retro-fit usage to store
  multiple frames of data (where in this case, a frame is one
  complete vector of vertices).
  This routine will only read NIFTI_INTENT_TIME_SERIES data
  arrays.
  -----------------------------------------------------------*/
MRI *MRISreadGiftiAsMRI(const char *fname, int read_volume)
{
  /* Attempt to read the file. */
  gifti_image* image = gifti_read_image (fname, 1);
  if (NULL == image)
  {
    fprintf 
      (stderr,
       "MRISreadGiftiAsMRI: gifti_read_image() returned NULL\n");
    return NULL;
  }

  /* check for compliance */
  int valid = gifti_valid_gifti_image (image, 1);
  if (valid == 0)
  {
    fprintf 
      (stderr,
       "MRISreadGiftiAsMRI: GIFTI file %s is invalid!\n", fname);
    gifti_free_image (image);
    return NULL;
  }

  /* check for overlay data */
  giiDataArray* scalars = NULL;
  int da_num = 0;
  int frame_count = 0;
  long long num_vertices = -1;
  long long num_cols = 0;
  do
  {
    scalars = gifti_find_DA (image, NIFTI_INTENT_TIME_SERIES, da_num);
    if (NULL == scalars)
    {
      if (++da_num >= image->numDA) break;
      else continue;
    }
    frame_count++;
    long long nvertices=0, ncols=0;
    gifti_DA_rows_cols (scalars, &nvertices, &ncols);
    if (num_vertices == -1)
    {
      num_vertices=nvertices;
      num_cols=ncols;
    }
    else
    {
      if (num_vertices <= 0 ||
          num_vertices != nvertices ||
          ncols != 1)
      {
        fprintf 
          (stderr,
           "MRISreadGiftiAsMRI: malformed time-series data array in file "
           "%s: nvertices=%d ncols=%d expected num_vertices=%d\n",
           fname, (int)nvertices, (int)num_cols, (int)num_vertices);
        gifti_free_image (image);
        return NULL;
      }
    }
    if (++da_num >= image->numDA) break;
  } while ( scalars );

  if (frame_count == 0)
  {
    fprintf 
      (stderr,
       "MRISreadGiftiAsMRI: no overlay data found in file %s\n", 
       fname);
    gifti_free_image (image);
    return NULL;
  }

  /* if we don't need to read the volume, just return a header */
  MRI *mri;
  if (!read_volume)
  {
    mri = MRIallocHeader(num_vertices,1,1,MRI_FLOAT);
    mri->nframes = frame_count;
    return(mri);
  }

  /* Copy in each scalar frame to 'volume' frame. */
  mri =  MRIallocSequence(num_vertices,1,1,MRI_FLOAT,frame_count);
  frame_count = 0;
  for (da_num = 0; da_num < image->numDA; da_num++)
  {
    scalars = gifti_find_DA (image, NIFTI_INTENT_TIME_SERIES, da_num);
    if (NULL == scalars) continue;
    int vno;
    for (vno = 0; vno < num_vertices; vno++)
    {
      float val = (float) gifti_get_DA_value_2D (scalars, vno, 0);
      MRIsetVoxVal(mri,vno,0,0,frame_count,val);
    }
    //printf("frame #%d\n",frame_count);
    frame_count++;
  }

  /* And we're done. */
  gifti_free_image (image);
  return(mri) ;
}


/*
 * insert username and current date into meta data
 */
static void insertCommonMetaData(giiMetaData* md)
{
#if 0
#include <uuid/uuid.h>
  uuid_t uuid;
  char uuidstr[2048];
  uuid_generate(uuid);
  uuid_unparse(uuid, uuidstr);
  gifti_add_to_meta( md, "UniqueID", uuidstr, 1 );
#endif

  struct passwd *pw = getpwuid(geteuid());
  if ((pw != NULL) && (pw->pw_name != NULL))
  {
    gifti_add_to_meta( md, "UserName", pw->pw_name, 1 );
  }

  time_t tyme = time(NULL);
  struct tm *lt = localtime(&tyme);
  char *date = asctime(lt);
  char *chr = strchr(date,'\r');
  if (chr) *chr = 0; // remove carriage return
  chr = strchr(date,'\n');
  if (chr) *chr = 0; // remove linefeed
  gifti_add_to_meta( md, "Date", date, 1 );
}


/*-------------------------------------------------------------------------
  Parameters:    mris - MRIS_SURFACE structure
                 intent_code - NIFTI_INTENT_* indicating
                   type of data in mris to write
                 out_fname - output file name of GIFTI file
                 curv_fname - if intent code is NIFTI_INTENT_SHAPE,
                   then this file contains the data to load into .curv field
                   prior to writing shape data to gifti output

  Returns value: 0 if passed, else error code

  Description:   writes a GIFTI file, if intent code is...
                 NIFTI_INTENT_POINTSET or _TRIANGLE, then write vertices and
                 face data
                 if NIFTI_INTENT_LABEL, then write LabelTable and annotations
                 if NIFTI_INTENT_<statistic>, then write .stats data
  ------------------------------------------------------------------------*/
int MRISwriteGIFTI(MRIS* mris,
                   int intent_code,
                   const char *out_fname,
                   const char *curv_fname)
{
  if (NULL == mris || NULL == out_fname)
  {
    fprintf (stderr,"MRISwriteGIFTI: invalid parameter\n");
    return ERROR_BADPARM;
  }

  if (intent_code == NIFTI_INTENT_SHAPE && NULL == curv_fname)
  {
    fprintf (stderr,"MRISwriteGIFTI: invalid parameter: curv_fname is NULL\n");
    return ERROR_BADPARM;
  }

  gifti_image* image = (gifti_image *)calloc(1,sizeof(gifti_image));
  if (NULL == image)
  {
    fprintf (stderr,"MRISwriteGIFTI: couldn't allocate image\n");
    return ERROR_NOMEMORY;
  }
  image->version = strcpyalloc(GIFTI_XML_VERSION);

  insertCommonMetaData(&image->meta);
  if (strlen(mris->subject_name))
  {
    gifti_add_to_meta( &image->meta, "SubjectID", mris->subject_name, 1 );
  }

  /* -------------------------------------------------------
   * Surface file
   */
  if (intent_code == NIFTI_INTENT_POINTSET || 
      intent_code == NIFTI_INTENT_TRIANGLE)
  { 
    /* 
     * Coordinates
     */
    giiDataArray* coords = gifti_alloc_and_add_darray (image);
    if (NULL == coords)
    {
      fprintf (stderr,"MRISwriteGIFTI: couldn't allocate giiDataArray\n");
      gifti_free_image (image);
      return ERROR_NOMEMORY;
    }

    /* Set its attributes. */
    coords->intent = NIFTI_INTENT_POINTSET;
    coords->datatype = NIFTI_TYPE_FLOAT32;
    coords->ind_ord = GIFTI_IND_ORD_ROW_MAJOR;
    coords->num_dim = 2;
    coords->dims[0] = mris->nvertices; /* In highest first, dim0 = rows */
    coords->dims[1] = 3;               /* In highest first, dim1 = cols */
    coords->encoding = GIFTI_ENCODING_B64GZ; // data stored in gzip'd base64
#if (BYTE_ORDER == LITTLE_ENDIAN)
    coords->endian = GIFTI_ENDIAN_LITTLE;
#else
    coords->endian = GIFTI_ENDIAN_BIG;
#endif

    coords->coordsys = NULL; // empty, unless we find something here...
    MRISreadTransform(mris, out_fname) ; // tries to get xform from out_fname
    if (mris->SRASToTalSRAS_ &&
        mris->SRASToTalSRAS_->rows==4 &&
        mris->SRASToTalSRAS_->cols==4)
    { // found a valid xform, so use it...
      gifti_add_empty_CS( coords );
      coords->coordsys[0]->dataspace = strcpyalloc("NIFTI_XFORM_UNKNOWN");
      coords->coordsys[0]->xformspace = strcpyalloc("NIFTI_XFORM_TALAIRACH");
      MATRIX *xform = mris->SRASToTalSRAS_;
      int r,c;
      for (r=1; r <= 4; r++)
        for (c=1; c <= 4; c++)
          coords->coordsys[0]->xform[r-1][c-1] = xform->rptr[r][c];
    }

    coords->nvals = gifti_darray_nvals (coords);
    gifti_datatype_sizes (coords->datatype, &coords->nbyper, NULL);

    /* Allocate the data array. */
    coords->data = NULL;
    coords->data = (void*) calloc (coords->nvals, coords->nbyper);
    if (NULL == coords->data)
    {
      fprintf (stderr,"MRISwriteGIFTI: couldn't allocate coords data of "
               "length %d, element size %d\n",
               (int)coords->nvals, coords->nbyper);
      gifti_free_image (image);
      return ERROR_NOMEMORY;
    }

    /* Copy in all our data. */
    int vertex_index;
    for (vertex_index = 0; vertex_index < mris->nvertices; vertex_index++)
    {
      if (mris->vertices[vertex_index].ripflag) continue;
      gifti_set_DA_value_2D (coords, vertex_index, 0,
                             mris->vertices[vertex_index].x);
      gifti_set_DA_value_2D (coords, vertex_index, 1,
                             mris->vertices[vertex_index].y);
      gifti_set_DA_value_2D (coords, vertex_index, 2,
                             mris->vertices[vertex_index].z);
    }

    /* 
     * Faces
     */
    giiDataArray* faces = gifti_alloc_and_add_darray (image);
    if (NULL == faces)
    {
      fprintf (stderr,"MRISwriteGIFTI: couldn't allocate giiDataArray\n");
      gifti_free_image (image);
      return ERROR_NOMEMORY;
    }

    /* count the real number of faces (the ones that dont have a vertex
       with a ripflag set) */
    int numFaces = 0;
    int face_index;
    for (face_index = 0; face_index < mris->nfaces; face_index++)
    {
      if (mris->vertices[mris->faces[face_index].v[0]].ripflag) continue;
      if (mris->vertices[mris->faces[face_index].v[1]].ripflag) continue;
      if (mris->vertices[mris->faces[face_index].v[2]].ripflag) continue;
      numFaces++;
    }

    /* Set its attributes. */
    faces->intent = NIFTI_INTENT_TRIANGLE;
    faces->datatype = NIFTI_TYPE_INT32;
    faces->ind_ord = GIFTI_IND_ORD_ROW_MAJOR;
    faces->num_dim = 2;
    faces->dims[0] = numFaces;    /* In highest first, dim0 = rows */
    faces->dims[1] = 3;               /* In highest first, dim1 = cols */
    faces->encoding = GIFTI_ENCODING_B64GZ; // data stored in gzip'd base64
#if (BYTE_ORDER == LITTLE_ENDIAN)
    faces->endian = GIFTI_ENDIAN_LITTLE;
#else
    faces->endian = GIFTI_ENDIAN_BIG;
#endif
    faces->coordsys = NULL;
    faces->nvals = gifti_darray_nvals (faces);
    gifti_datatype_sizes (faces->datatype, &faces->nbyper, NULL);

    /* Allocate the data array. */
    faces->data = NULL;
    faces->data = (void*) calloc (faces->nvals, faces->nbyper);
    if (NULL == faces->data)
    {
      fprintf (stderr,"MRISwriteGIFTI: couldn't allocate faces data of "
               "length %d, element size %d\n",
               (int)faces->nvals, faces->nbyper);
      gifti_free_image (image);
      return ERROR_NOMEMORY;
    }

    /* Copy in all our face data (remembering to ignore faces which
       have a vertex with the ripflag set). */
    int faceNum = 0;
    for (face_index = 0; face_index < mris->nfaces; face_index++)
    {
      if (mris->vertices[mris->faces[face_index].v[0]].ripflag) continue;
      if (mris->vertices[mris->faces[face_index].v[1]].ripflag) continue;
      if (mris->vertices[mris->faces[face_index].v[2]].ripflag) continue;

      gifti_set_DA_value_2D (faces, faceNum, 0,
                             mris->faces[face_index].v[0]);
      gifti_set_DA_value_2D (faces, faceNum, 1,
                             mris->faces[face_index].v[1]);
      gifti_set_DA_value_2D (faces, faceNum, 2,
                             mris->faces[face_index].v[2]);
      faceNum++;
    }

    /* standard meta data for surfaces */
    if (mris->fname)
    {
      const char *primary=NULL, *secondary=NULL, *geotype=NULL;
      char *name = mris->fname;
      char *topotype="Closed";
      if (strstr(name, "lh.")) primary = "CortexLeft";
      if (strstr(name, "rh.")) primary = "CortexRight";
      if (strstr(name, ".orig"))     secondary = "GrayWhite";
      if (strstr(name, ".smoothwm")) secondary = "GrayWhite";
      if (strstr(name, ".white"))    secondary = "GrayWhite";
      if (strstr(name, ".graymid"))  secondary = "MidThickness";
      if (strstr(name, ".gray"))     secondary = "Pial";
      if (strstr(name, ".pial"))     secondary = "Pial";
      if (strstr(name, ".orig"))     geotype = "Reconstruction";
      if (strstr(name, ".smoothwm")) geotype = "Anatomical";
      if (strstr(name, ".white"))    geotype = "Anatomical";
      if (strstr(name, ".gray"))     geotype = "Anatomical";
      if (strstr(name, ".graymid"))  geotype = "Anatomical";
      if (strstr(name, ".pial"))     geotype = "Anatomical";
      if (strstr(name, ".inflated")) geotype = "Inflated";
      if (strstr(name, ".sphere"))   geotype = "Sphere";
      if (strstr(name, ".qsphere"))  geotype = "Sphere";
      if (strstr(name,"pial-outer")) geotype = "Hull";
      if (mris->patch)
      {
        geotype = "Flat";
        topotype = "Cut";
      }
      
      if (primary) gifti_add_to_meta( &coords->meta,
                                      "AnatomicalStructurePrimary",
                                      primary,
                                      1 );
      if (secondary) gifti_add_to_meta( &coords->meta,
                                        "AnatomicalStructureSecondary",
                                        secondary,
                                        1 );
      if (geotype) gifti_add_to_meta( &coords->meta,
                                      "GeometricType",
                                      geotype,
                                      1 );
      gifti_add_to_meta( &faces->meta, "TopologicalType", topotype, 1 );
      gifti_add_to_meta( &coords->meta, "Name", name, 1 );
      gifti_add_to_meta( &faces->meta, "Name", name, 1 );
    }
  } // end of if NIFTI_INTENT_POINTSET or NIFTI_INTENT_TRIANGLE


  /* -------------------------------------------------------
   * Shape file
   */
  if (intent_code == NIFTI_INTENT_SHAPE)
  {
    if (MRISreadCurvatureFile(mris, curv_fname))
    {
      fprintf (stderr,"MRISwriteGIFTI: couldn't read %s\n",curv_fname);
      gifti_free_image (image);
      return ERROR_BADFILE;
    }

    giiDataArray* shape = gifti_alloc_and_add_darray (image);
    if (NULL == shape)
    {
      fprintf (stderr,"MRISwriteGIFTI: couldn't allocate giiDataArray\n");
      gifti_free_image (image);
      return ERROR_NOMEMORY;
    }

    /* Set its attributes. */
    shape->intent = NIFTI_INTENT_SHAPE;
    shape->datatype = NIFTI_TYPE_FLOAT32;
    shape->ind_ord = GIFTI_IND_ORD_ROW_MAJOR;
    shape->num_dim = 1;
    shape->dims[0] = mris->nvertices;
    shape->dims[1] = 0;
    shape->encoding = GIFTI_ENCODING_B64GZ; // data stored in gzip'd base64
#if (BYTE_ORDER == LITTLE_ENDIAN)
    shape->endian = GIFTI_ENDIAN_LITTLE;
#else
    shape->endian = GIFTI_ENDIAN_BIG;
#endif
    shape->coordsys = NULL;
    shape->nvals = gifti_darray_nvals (shape);
    gifti_datatype_sizes (shape->datatype, &shape->nbyper, NULL);

    /* include some metadata describing this shape */
    gifti_add_to_meta( &shape->meta, "Name", curv_fname, 1 );
    char *meta=NULL;
    if (strstr(curv_fname, ".thickness")) meta = "Thickness";
    if (strstr(curv_fname, ".curv"))      meta = "CurvatureRadial";
    if (strstr(curv_fname, ".sulc"))      meta = "SulcalDepth";
    if (strstr(curv_fname, ".area"))      meta = "Area";
    if (strstr(curv_fname, ".volume"))    meta = "Volume";
    if (strstr(curv_fname, ".jacobian"))  meta = "Jacobian";
    if (meta) gifti_add_to_meta( &shape->meta, "ShapeDataType", meta, 1 );

    /* Allocate the data array. */
    shape->data = NULL;
    shape->data = (void*) calloc (shape->nvals, shape->nbyper);
    if (NULL == shape->data)
    {
      fprintf (stderr,"MRISwriteGIFTI: couldn't allocate shape data of "
               "length %d, element size %d\n",
               (int)shape->nvals,shape->nbyper);
      gifti_free_image (image);
      return ERROR_NOMEMORY;
    }

    /* Copy in all our data. */
    int vno;
    for (vno = 0; vno < mris->nvertices; vno++)
    {
      if (mris->vertices[vno].ripflag) continue;
      gifti_set_DA_value_2D (shape, vno, 0,
                             mris->vertices[vno].curv);
    }
  } // end of if NIFTI_INTENT_SHAPE


  /* -------------------------------------------------------
   * Label file
   */
  if (intent_code == NIFTI_INTENT_LABEL)
  {
    /*
     * Writes .annot data to a label table data gifti file:
     * puts the freesurfer colortable struct into a LabelTable,
     * and puts the .annotation field from each vertex into a 
     * DataArray
     */
    /*
     * LabelTable struct, fill it in with our colortable stuff
     */
    giiLabelTable labeltable;
    labeltable.length = mris->ct->nentries;
    if (labeltable.length == 0)
    {
      fprintf(stderr, "MRISwriteGIFTI: colortable is empty!\n");
      return ERROR_BADFILE;
    }
    labeltable.key = (int *)calloc(labeltable.length, sizeof(int *));
    labeltable.label = (char **)calloc(labeltable.length, sizeof(char *));
    labeltable.rgba = (float *)calloc(labeltable.length, 4*sizeof(float *));
    if ((NULL == labeltable.key) ||
        (NULL == labeltable.label) ||
        (NULL == labeltable.rgba))
    {
      fprintf (stderr,"MRISwriteGIFTI: couldn't allocate giftiLabelTable\n");
      return ERROR_NOMEMORY;
    }
    float* rgba = labeltable.rgba;
    int idx;
    for (idx=0; idx < labeltable.length; idx++)
    {
      labeltable.key[idx] = CTABrgb2Annotation(mris->ct->entries[idx]->ri,
                                               mris->ct->entries[idx]->gi,
                                               mris->ct->entries[idx]->bi);
      //printf("%8.8X\n",labeltable.key[idx]);

      labeltable.label[idx] = strcpyalloc(mris->ct->entries[idx]->name);

      rgba[0] = mris->ct->entries[idx]->rf;
      rgba[1] = mris->ct->entries[idx]->gf;
      rgba[2] = mris->ct->entries[idx]->bf;
      rgba[3] = 1.0f;
      if ((0 == strcmp(labeltable.label[idx],"unknown")) ||
          (0 == strcmp(labeltable.label[idx],"Unknown")))
      {
        // make certain unknown region is completely empty, invisible
        rgba[0] = rgba[1] = rgba[2] = rgba[3] = 0.0f;
      }
      rgba += 4; // next color
      /*
        printf("RGBA: %d %d %d %d %f %f %f %f\n",
        mris->ct->entries[idx]->ri,
        mris->ct->entries[idx]->gi,
        mris->ct->entries[idx]->bi,
        mris->ct->entries[idx]->ai,
        mris->ct->entries[idx]->rf,
        mris->ct->entries[idx]->gf,
        mris->ct->entries[idx]->bf,
        mris->ct->entries[idx]->af);
      */
    }
    // dont forget to stick us in the image
    image->labeltable = labeltable;
    //gifti_disp_LabelTable(NULL,&image->labeltable);

    /*
     * Labels array
     */
    giiDataArray* labels = gifti_alloc_and_add_darray (image);
    if (NULL == labels)
    {
      fprintf (stderr,"MRISwriteGIFTI: couldn't allocate giiDataArray\n");
      gifti_free_image (image);
      return ERROR_NOMEMORY;
    }

    /* Set its attributes. */
    labels->intent = NIFTI_INTENT_LABEL;
    labels->datatype = NIFTI_TYPE_INT32;
    labels->ind_ord = GIFTI_IND_ORD_ROW_MAJOR;
    labels->num_dim = 1;
    labels->dims[0] = mris->nvertices;
    labels->dims[1] = 0;
    labels->encoding = GIFTI_ENCODING_B64GZ; // data stored in gzip'd base64
#if (BYTE_ORDER == LITTLE_ENDIAN)
    labels->endian = GIFTI_ENDIAN_LITTLE;
#else
    labels->endian = GIFTI_ENDIAN_BIG;
#endif
    labels->coordsys = NULL;
    labels->nvals = gifti_darray_nvals (labels);
    gifti_datatype_sizes (labels->datatype, &labels->nbyper, NULL);

    /* include some metadata describing this as a label */
    gifti_add_to_meta( &labels->meta, "Name", "node label", 1 );
    if (labeltable.length == 2)
    {
      // in the special case of a label table consisting of one label
      // (assuming the first label is 'unknown') use this one label as name,
      // for instance in the case of the V1 label
      gifti_add_to_meta( &labels->meta, "Name", labeltable.label[1], 1 );
    }

    /* Allocate the data array. */
    labels->data = NULL;
    labels->data = (void*) calloc (labels->nvals, labels->nbyper);
    if (NULL == labels->data)
    {
      fprintf (stderr,"MRISwriteGIFTI: couldn't allocate labels data of "
               "length %d, element size %d\n",
               (int)labels->nvals,labels->nbyper);
      gifti_free_image (image);
      return ERROR_NOMEMORY;
    }

    /* Copy our 'annotation' data for each vertex */
    unsigned int* label_data = labels->data;
    int label_index;
    for (label_index = 0; label_index < mris->nvertices; label_index++)
    {
      if (mris->vertices[label_index].ripflag) continue;
      *(label_data + label_index) = mris->vertices[label_index].annotation;
      //printf("%8.8X ", *(label_data + label_index));
    }
  } // end of if NIFTI_INTENT_LABEL


  /* -------------------------------------------------------
   * Statistics file
   */
  if (intent_code == NIFTI_INTENT_CORREL ||
      intent_code == NIFTI_INTENT_TTEST ||
      intent_code == NIFTI_INTENT_FTEST ||
      intent_code == NIFTI_INTENT_ZSCORE ||
      intent_code == NIFTI_INTENT_CHISQ ||
      intent_code == NIFTI_INTENT_BETA ||
      intent_code == NIFTI_INTENT_BINOM ||
      intent_code == NIFTI_INTENT_GAMMA ||
      intent_code == NIFTI_INTENT_POISSON ||
      intent_code == NIFTI_INTENT_NORMAL ||
      intent_code == NIFTI_INTENT_FTEST_NONC ||
      intent_code == NIFTI_INTENT_CHISQ_NONC ||
      intent_code == NIFTI_INTENT_LOGISTIC ||
      intent_code == NIFTI_INTENT_LAPLACE ||
      intent_code == NIFTI_INTENT_UNIFORM ||
      intent_code == NIFTI_INTENT_TTEST_NONC ||
      intent_code == NIFTI_INTENT_WEIBULL ||
      intent_code == NIFTI_INTENT_CHI ||
      intent_code == NIFTI_INTENT_INVGAUSS ||
      intent_code == NIFTI_INTENT_EXTVAL ||
      intent_code == NIFTI_INTENT_PVAL ||
      intent_code == NIFTI_INTENT_LOGPVAL ||
      intent_code == NIFTI_INTENT_LOG10PVAL ||
      intent_code == NIFTI_INTENT_ESTIMATE)
  {
    giiDataArray* stats = gifti_alloc_and_add_darray (image);
    if (NULL == stats)
    {
      fprintf (stderr,"MRISwriteGIFTI: couldn't allocate giiDataArray\n");
      gifti_free_image (image);
      return ERROR_NOMEMORY;
    }

    /* Set its attributes. */
    stats->intent = intent_code;
    stats->datatype = NIFTI_TYPE_FLOAT32;
    stats->ind_ord = GIFTI_IND_ORD_ROW_MAJOR;
    stats->num_dim = 1;
    stats->dims[0] = mris->nvertices;
    stats->dims[1] = 0;
    stats->encoding = GIFTI_ENCODING_B64GZ; // data stored in gzip'd base64
#if (BYTE_ORDER == LITTLE_ENDIAN)
    stats->endian = GIFTI_ENDIAN_LITTLE;
#else
    stats->endian = GIFTI_ENDIAN_BIG;
#endif
    stats->coordsys = NULL;
    stats->nvals = gifti_darray_nvals (stats);
    gifti_datatype_sizes (stats->datatype, &stats->nbyper, NULL);

    /* include some metadata describing this thing */
    gifti_add_to_meta( &stats->meta,
                       "Intent_code",
                       gifti_intent_to_string(intent_code),
                       1 );
    if (intent_code == NIFTI_INTENT_UNIFORM)
    {
      gifti_add_to_meta( &stats->meta, "Intent_p1", "0", 1 ); // lower end
      gifti_add_to_meta( &stats->meta, "Intent_p2", "1", 1 ); // upper end
    }

    /* Allocate the data array. */
    stats->data = NULL;
    stats->data = (void*) calloc (stats->nvals, stats->nbyper);
    if (NULL == stats->data)
    {
      fprintf (stderr,"MRISwriteGIFTI: couldn't allocate stats data of "
               "length %d, element size %d\n",
               (int)stats->nvals,stats->nbyper);
      gifti_free_image (image);
      return ERROR_NOMEMORY;
    }

    /* Copy in all our data. */
    int vno;
    for (vno = 0; vno < mris->nvertices; vno++)
    {
      if (mris->vertices[vno].ripflag) continue;
      gifti_set_DA_value_2D (stats, vno, 0, mris->vertices[vno].stat);
    }
  } // end of if NIFTI_INTENT_<stats>


  /* check for compliance */
  int valid = gifti_valid_gifti_image (image, 1);
  if (valid == 0)
  {
    fprintf (stderr,"MRISwriteGIFTI: GIFTI file %s is invalid!\n", out_fname);
    gifti_free_image (image);
    return ERROR_BADFILE;
  }

  /* Write the file. */
  if (gifti_write_image (image, out_fname, 1))
  {
    fprintf (stderr,"MRISwriteGIFTI: couldn't write image\n");
    gifti_free_image (image);
    return ERROR_BADFILE;
  }

  gifti_free_image (image);

  return ERROR_NONE;
}


/*-----------------------------------------------------------
  Parameters:    MRI structure (surface-encoded volume),
                 output file name of GIFTI file,

  Returns value: 0 if passed, else error code

  Description:   writes a GIFTI file containing functional or
                 timeseries data
  -----------------------------------------------------------*/
int mriWriteGifti(MRI* mri, const char *out_fname)
{
  if (NULL == mri || NULL == out_fname)
  {
    fprintf (stderr,"mriWriteGifti: invalid input parameters\n");
    return ERROR_BADPARM;
  }

  gifti_image* image = (gifti_image *)calloc(1,sizeof(gifti_image));
  if (NULL == image)
  {
    fprintf (stderr,"mriWriteGifti: couldn't allocate gifti_image\n");
    return ERROR_NOMEMORY;
  }
  image->version = strcpyalloc(GIFTI_XML_VERSION);

  /* include some metadata describing this thing */
  insertCommonMetaData(&image->meta);

  /* -------------------------------------------------------
   * One DataArray for each 'frame' in the 'volume' data
   */
  int frame;
  for (frame=0; frame < mri->nframes; frame++)
  {
    giiDataArray* scalars = gifti_alloc_and_add_darray (image);
    if (NULL == scalars)
    {
      fprintf (stderr,"mriWriteGifti: couldn't allocate giiDataArray\n");
      gifti_free_image (image);
      return ERROR_NOMEMORY;
    }

    /* Set its attributes. */
    scalars->intent = NIFTI_INTENT_NONE;
    if (mri->nframes > 1) scalars->intent = NIFTI_INTENT_TIME_SERIES;
    if (scalars->intent == NIFTI_INTENT_TIME_SERIES)
    {
      // add TR (repetition time) to metadata:
      char buf[STRLEN];
      sprintf(buf,"%f",mri->tr);
      gifti_add_to_meta( &scalars->meta, "TimeStep", buf, 1 );
    }
    scalars->datatype = NIFTI_TYPE_FLOAT32;
    scalars->ind_ord = GIFTI_IND_ORD_ROW_MAJOR;
    scalars->num_dim = 1;
    scalars->dims[0] = mri->width;
    scalars->dims[1] = 0;
    scalars->encoding = GIFTI_ENCODING_B64GZ; // data stored in gzip'd base64
#if (BYTE_ORDER == LITTLE_ENDIAN)
    scalars->endian = GIFTI_ENDIAN_LITTLE;
#else
    scalars->endian = GIFTI_ENDIAN_BIG;
#endif
    scalars->coordsys = NULL;
    scalars->nvals = gifti_darray_nvals (scalars);
    gifti_datatype_sizes (scalars->datatype, &scalars->nbyper, NULL);

    /* Allocate the data array. */
    scalars->data = NULL;
    scalars->data = (void*) calloc (scalars->nvals, scalars->nbyper);
    if (NULL == scalars->data)
    {
      fprintf (stderr,"mriWriteGifti: couldn't allocate scalars data of "
               "length %d, element size %d\n",
               (int)scalars->nvals,scalars->nbyper);
      gifti_free_image (image);
      return ERROR_NOMEMORY;
    }

    /* Copy in all our data. */
    int scalar_index;
    for (scalar_index = 0; scalar_index < mri->width; scalar_index++)
    {
      float val = MRIgetVoxVal(mri,scalar_index,0,0,frame);
      gifti_set_DA_value_2D (scalars, scalar_index, 0, val);
    }

    // next frame
  }

  /* check for compliance */
  int valid = gifti_valid_gifti_image (image, 1);
  if (valid == 0)
  {
    fprintf (stderr,"mriWriteGifti: GIFTI file %s is invalid!\n", 
             out_fname);
    gifti_free_image (image);
    return ERROR_BADFILE;
  }

  /* Write the file. */
  if (gifti_write_image (image, out_fname, 1))
  {
    fprintf (stderr,"mriWriteGifti: couldn't write image\n");
    gifti_free_image (image);
    return ERROR_BADFILE;
  }

  gifti_free_image (image);

  return ERROR_NONE;
}
