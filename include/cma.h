/**
 * @file  cma.h
 * @brief constants for neuroanatomical structures.
 *
 * constants and macros for neuroanatomical and some vascular structures.
 * Originally it was just cma labels, but it has been generalized.
 */
/*
 * Original Author: Bruce Fischl
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2011/01/23 17:33:23 $
 *    $Revision: 1.52.2.1 $
 *
 * Copyright (C) 2002-2010,
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


#ifndef CMA_H
#define CMA_H


#if defined(__cplusplus)
extern "C" {
#endif



/*
 * colors for these labels defined in distribution/FreeSurferColorLUT.txt
 */

#define Unknown                        0
#define Left_Cerebral_Exterior         1
#define Left_Cerebral_White_Matter     2
#define Left_Cerebral_Cortex           3
#define Left_Lateral_Ventricle         4
#define Left_Inf_Lat_Vent              5
#define Left_Cerebellum_Exterior       6
#define Left_Cerebellum_White_Matter   7
#define Left_Cerebellum_Cortex         8
#define Left_Thalamus                  9
#define Left_Thalamus_Proper          10
#define Left_Caudate                  11
#define Left_Putamen                  12
#define Left_Pallidum                 13
#define Third_Ventricle               14
#define Fourth_Ventricle              15
#define Brain_Stem                    16
#define Left_Hippocampus              17
#define Left_Amygdala                 18
#define Left_Insula                   19
#define Left_Operculum                20
#define Line_1                        21
#define Line_2                        22
#define Line_3                        23
#define CSF                           24
#define Left_Lesion                   25
#define Left_Accumbens_area           26
#define Left_Substancia_Nigra         27
#define Left_VentralDC                28
#define Left_undetermined             29
#define Left_vessel                   30
#define Left_choroid_plexus           31
#define Left_F3orb                    32
#define Left_lOg                      33
#define Left_aOg                      34
#define Left_mOg                      35
#define Left_pOg                      36
#define Left_Stellate                 37
#define Left_Porg                     38
#define Left_Aorg                     39
#define Right_Cerebral_Exterior       40
#define Right_Cerebral_White_Matter   41
#define Right_Cerebral_Cortex         42
#define Right_Lateral_Ventricle       43
#define Right_Inf_Lat_Vent            44
#define Right_Cerebellum_Exterior     45
#define Right_Cerebellum_White_Matter 46
#define Right_Cerebellum_Cortex       47
#define Right_Thalamus                48
#define Right_Thalamus_Proper         49
#define Right_Caudate                 50
#define Right_Putamen                 51
#define Right_Pallidum                52
#define Right_Hippocampus             53
#define Right_Amygdala                54
#define Right_Insula                  55
#define Right_Operculum               56
#define Right_Lesion                  57
#define Right_Accumbens_area          58
#define Right_Substancia_Nigra        59
#define Right_VentralDC               60
#define Right_undetermined            61
#define Right_vessel                  62
#define Right_choroid_plexus          63
#define Right_F3orb                   64
#define Right_lOg                     65
#define Right_aOg                     66
#define Right_mOg                     67
#define Right_pOg                     68
#define Right_Stellate                69
#define Right_Porg                    70
#define Right_Aorg                    71
#define Fifth_Ventricle               72
#define Left_Interior                 73
#define Right_Interior                74
#define Left_Lateral_Ventricles       75
#define Right_Lateral_Ventricles      76
#define WM_hypointensities            77
#define Left_WM_hypointensities       78
#define Right_WM_hypointensities      79
#define non_WM_hypointensities        80
#define Left_non_WM_hypointensities   81
#define Right_non_WM_hypointensities  82
#define Left_F1                       83
#define Right_F1                      84
#define Optic_Chiasm                  85
#define Left_Amygdala_Anterior        96
#define Right_Amygdala_Anterior       97

/*
 * no brain labels after this please unless you fix the IS_BRAIN macro
 */

#define Dura              98
#define Epidermis        118
#define Conn_Tissue      119
#define SC_FAT_MUSCLE    120
#define Cranium          121
#define CSF_SA           122
#define Muscle           123
#define Ear              124
#define Fatty_Tissue     125
#define Spinal_Cord      126
#define Soft_Tissue      127
#define Nerve            128
#define Bone             129
#define Air              130
#define Orbit            131
#define Tongue           132
#define Nasal_Structures 133
#define Globe            134
#define Teeth            135

#define Right_Temporal_Cerebral_White_Matter 186
#define Left_Temporal_Cerebral_White_Matter  187
#define Fat              189
#define Bright_Unknown   190
#define Dark_Unknown     191
#define Corpus_Callosum  192

#define alveus                    201
#define perforant_pathway         202
#define parasubiculum             203
#define presubiculum              204
#define subiculum                 205
#define CA1                       206
#define CA2                       207
#define CA3                       208
#define CA4                       209
#define GC_DG                     210
#define HATA                      211
#define fimbria                   212
#define lateral_ventricle         213
#define molecular_layer_HP        214
#define hippocampal_fissure       215
#define entorhinal_cortex         216
#define molecular_layer_subiculum 217
#define Amygdala                  218
#define Cerebral_White_Matter     219
#define Cerebral_Cortex           220
#define Inf_Lat_Vent              221

#if 1
#define Left_hippocampal_fissure  193
#define Left_CADG_head            194
#define Left_subiculum            195
#define Left_fimbria              196
#define Right_hippocampal_fissure 197
#define Right_CADG_head           198
#define Right_subiculum           199
#define Right_fimbria             200
#endif

#define Fornix            250
#define CC_Posterior      251
#define CC_Mid_Posterior  252
#define CC_Central        253
#define CC_Mid_Anterior   254
#define CC_Anterior       255

#define IS_CC(l) (l >= CC_Posterior && l <= CC_Anterior)

#define VOXEL_UNCHANGED 256

// vascular and lymph labels (from Alex G)
#define Aorta                    331
#define Left_Common_IliacA       332
#define Right_Common_IliacA      333
#define Left_External_IliacA     334
#define Right_External_IliacA    335
#define Left_Internal_IliacA     336
#define Right_Internal_IliacA    337
#define Left_Lateral_SacralA     338
#define Right_Lateral_SacralA    339
#define Left_ObturatorA          340
#define Right_ObturatorA         341
#define Left_Internal_PudendalA  342
#define Right_Internal_PudendalA 343
#define Left_UmbilicalA          344
#define Right_UmbilicalA         345
#define Left_Inf_RectalA         346
#define Right_Inf_RectalA        347
#define Left_Common_IliacV       348
#define Right_Common_IliacV      349
#define Left_External_IliacV     350
#define Right_External_IliacV    351
#define Left_Internal_IliacV     352
#define Right_Internal_IliacV    353
#define Left_ObturatorV          354
#define Right_ObturatorV         355
#define Left_Internal_PudendalV  356
#define Right_Internal_PudendalV 357
#define Pos_Lymph                358
#define Neg_Lymph                359

// Brodmann Areas
#define BA17 400  //                                    206 62  78  0
#define BA18 401  //                                    121 18  134 0
#define BA44 402  //                                    199 58  250 0
#define BA45 403  //                                    1   148 0   0
#define BA4a 404  //                                    221 248 164 0
#define BA4p 405  //                                    231 148 34  0
#define BA6 406  //                                     1   118 14  0
#define BA2 407  //                                     120 118 14  0
#define BAun1 408  //                                   123 186 220 0
#define BAun2 409  //                                   238 13  176 0

// HiRes Hippocampus labeling
#define right_CA2_3 500  //                             17  85  136 0
#define right_alveus 501  //                            119 187 102 0
#define right_CA1 502  //                               204 68  34  0
#define right_fimbria 503  //                           204 0   255 0
#define right_presubiculum 504  //                      221 187 17  0
#define right_hippocampal_fissure 505  //               153 221 238 0
#define right_CA4_DG 506  //                            51  17  17  0
#define right_subiculum 507  //                         0   119 85  0
#define right_fornix 508  //                            20  100 200 0

#define left_CA2_3 550  //                              17  85  137 0
#define left_alveus 551  //                             119 187 103 0
#define left_CA1 552  //                                204 68  35  0
#define left_fimbria 553  //                            204 0   254 0
#define left_presubiculum 554  //                       221 187 16  0
#define left_hippocampal_fissure 555  //                153 221 239 0
#define left_CA4_DG 556  //                             51  17  18  0
#define left_subiculum 557  //                          0   119 86  0
#define left_fornix 558  //                             20  100 201 0

#define Tumor       600 //                              253 253 253 0
#define SUSPICIOUS 999 //                               255 100 100 0

// be sure to update MAX_LABEL if additional labels are added!

#define MAX_LABEL SUSPICIOUS
#define MAX_CMA_LABEL (MAX_LABEL)
#define MAX_CMA_LABELS (MAX_CMA_LABEL+1)


#define IS_UNKNOWN(label)  (((label) == Unknown) || (label == 255) || (label == Bright_Unknown) || (label == Dark_Unknown))

#define IS_BRAIN(label)  (!IS_UNKNOWN(label) && label < Dura)

#define IS_WM(label) (((label) == Left_Cerebral_White_Matter) || ((label) == Right_Cerebral_White_Matter) || ((label) == Left_Temporal_Cerebral_White_Matter) || ((label) == Right_Temporal_Cerebral_White_Matter))
#define IS_HYPO(label) (((label) == WM_hypointensities)  || ((label) == Left_WM_hypointensities)  || ((label) == Right_WM_hypointensities))
#define IS_WMH(label) (IS_WM(label) || IS_HYPO(label))
#define IS_THALAMUS(label)  (((label) == Left_Thalamus) || ((label) == Left_Thalamus_Proper) || ((label) == Right_Thalamus) || ((label) == Right_Thalamus_Proper))
#define IS_GM(label) (((label) == Left_Cerebral_Cortex) || ((label) == Right_Cerebral_Cortex))


#define IS_CEREBELLAR_WM(label) (((label) == Left_Cerebellum_White_Matter) || ((label) == Right_Cerebellum_White_Matter))
#define IS_CEREBELLAR_GM(label) (((label) == Left_Cerebellum_Cortex) || ((label) == Right_Cerebellum_Cortex))

#define IS_HIPPO(l) (((l) == Left_Hippocampus) || ((l) == Right_Hippocampus))
#define IS_AMYGDALA(l) (((l) == Left_Amygdala) || ((l) == Right_Amygdala))
#define IS_CORTEX(l) (((l) == Left_Cerebral_Cortex) || \
                      ((l) == Right_Cerebral_Cortex))
#define IS_LAT_VENT(l) (((l) == Left_Lateral_Ventricle) || \
                        ((l) == Right_Lateral_Ventricle) || \
                        ((l) == Right_Inf_Lat_Vent) || \
                        ((l) == Left_Inf_Lat_Vent))
#define IS_CSF(l) (IS_LAT_VENT(l) || ((l) == CSF) || ((l) == CSF_SA) || ((l) == Third_Ventricle) || ((l) == Fourth_Ventricle))

#define IS_INF_LAT_VENT(l)  (((l) == Left_Inf_Lat_Vent) || ((l) == Right_Inf_Lat_Vent))
#define IS_CAUDATE(l) (((l) == Left_Caudate) || ((l) == Right_Caudate))
#define IS_PUTAMEN(l) (((l) == Left_Putamen) || ((l) == Right_Putamen))
#define IS_PALLIDUM(l) (((l) == Left_Pallidum) || ((l) == Right_Pallidum))

#define LABEL_WITH_NO_TOPOLOGY_CONSTRAINT(l) (\
   ((l) == Right_non_WM_hypointensities) || \
   ((l) == Left_non_WM_hypointensities) || \
   ((l) == Left_WM_hypointensities) || \
   ((l) == Right_WM_hypointensities) || \
   ((l) == Left_choroid_plexus) || \
   ((l) == Right_choroid_plexus) || \
   ((l) == WM_hypointensities) || \
   ((l) == Dura) || \
   ((l) == Bone) || \
   ((l) == CSF_SA) || \
   ((l) == Epidermis) || \
   ((l) == SC_FAT_MUSCLE) || \
   ((l) == non_WM_hypointensities))

/* --- below: see cma.c --- */

#define MAX_OUTLINE_CLAIMS 10

#define CMA_FILL_NONE     0
#define CMA_FILL_OUTLINE  1
#define CMA_FILL_INTERIOR 2

typedef struct
{
  int n_claims;
  int interior_claim_flag;
  short claim_labels[MAX_OUTLINE_CLAIMS];
  float claim_values[MAX_OUTLINE_CLAIMS];
  float no_label_claim;
}
CMAoutlineClaim;

typedef struct
{
  int width, height;
  CMAoutlineClaim **claim_field;
  //unsigned char **fill_field;
  short **fill_field;
  unsigned char **outline_points_field;
}
CMAoutlineField;

CMAoutlineField *CMAoutlineFieldAlloc(int width, int height);
int CMAfreeOutlineField(CMAoutlineField **of);

int CMAclearFillField(CMAoutlineField *field);
int CMAfill(CMAoutlineField *field, short seed_x, short seed_y);

int CMAclaimPoints(CMAoutlineField *field,
                   short label, short *points,
                   int n_points, short seed_x, short seed_y);
int CMAassignLabels(CMAoutlineField *field);

int CMAvalueClaims(CMAoutlineClaim *claim);
int CMAvalueAllClaims(CMAoutlineField *field);

/* the weights to give claims of nearby points -- see cma.c */
#define OTL_CLAIM_WEIGHT_CENTER   1.000
#define OTL_CLAIM_WEIGHT_SIDE     0.500
#define OTL_CLAIM_WEIGHT_DIAGONAL 0.707

short CMAtotalClaims(CMAoutlineField *field, int x, int y);
int CMAaddWeightedTotals(CMAoutlineClaim *claim,
                         float weight, float *claim_totals);

int CMAzeroOutlines(CMAoutlineField *field);
char *cma_label_to_name(int label) ;
int IsSubCorticalGray(int SegId);
#include "mri.h"
double SupraTentorialVolCorrection(MRI *aseg, MRI *ribbon);

#define IS_FIMBRIA(l) ((l) == left_fimbria || (l) == right_fimbria || (l) == fimbria)
#define CSF_CLASS        0
#define GM_CLASS         1
#define WM_CLASS         2
#define NTISSUE_CLASSES  3
#define LH_CLASS         4
#define RH_CLASS         5
#define FLUID_CLASS      6
#define OTHER_CLASS      7

#define IS_GRAY_MATTER(l) (         \
   ((l) == Left_Cerebral_Cortex) || \
   ((l) == Left_Hippocampus) || \
   ((l) == Left_Amygdala) || \
   ((l) == Left_Putamen) || \
   ((l) == Left_Caudate) || \
   ((l) == Left_Cerebellum_Cortex) || \
   ((l) == Left_Accumbens_area) || \
   ((l) == Right_Cerebral_Cortex) || \
   ((l) == Right_Hippocampus) || \
   ((l) == Right_Amygdala) || \
   ((l) == Right_Putamen) || \
   ((l) == Right_Caudate) || \
   ((l) == Right_Cerebellum_Cortex) || \
   ((l) == Right_Accumbens_area))

#define IS_WHITE_MATTER(l) (         \
   ((l) == Left_Cerebellum_White_Matter) || \
   ((l) == Left_Cerebral_White_Matter) || \
   ((l) == Right_Cerebellum_White_Matter) || \
   ((l) == Right_Cerebral_White_Matter))

#define IS_FLUID(l) (         \
   ((l) == Left_Lateral_Ventricle) || \
   ((l) == Left_Inf_Lat_Vent) || \
   ((l) == Right_Lateral_Ventricle) || \
   ((l) == Right_Inf_Lat_Vent) || \
   ((l) == Unknown))


#define IS_LH_CLASS(l) (\
   ((l) == Left_Cerebral_Cortex) || \
   ((l) == Left_Cerebral_White_Matter) || \
   ((l) == Left_Hippocampus) || \
   ((l) == Left_Amygdala) || \
   ((l) == Left_Putamen) || \
   ((l) == Left_Pallidum) || \
   ((l) == Left_Caudate) || \
   ((l) == Left_Lateral_Ventricle) || \
   ((l) == Left_Inf_Lat_Vent) || \
   ((l) == Left_Cerebellum_Cortex) || \
   ((l) == Left_Cerebellum_White_Matter) || \
   ((l) == Left_Thalamus_Proper) || \
   ((l) == Left_vessel) || \
   ((l) == Left_choroid_plexus) || \
   ((l) == Left_VentralDC) || \
   ((l) == Left_Accumbens_area))

#define IS_RH_CLASS(l) (\
   ((l) == Right_Cerebral_Cortex) || \
   ((l) == Right_Cerebral_White_Matter) || \
   ((l) == Right_Hippocampus) || \
   ((l) == Right_Amygdala) || \
   ((l) == Right_Putamen) || \
   ((l) == Right_Pallidum) || \
   ((l) == Right_Caudate) || \
   ((l) == Right_Lateral_Ventricle) || \
   ((l) == Right_Inf_Lat_Vent) || \
   ((l) == Right_Cerebellum_Cortex) || \
   ((l) == Right_Cerebellum_White_Matter) || \
   ((l) == Right_Thalamus_Proper) || \
   ((l) == Right_vessel) || \
   ((l) == Right_choroid_plexus) || \
   ((l) == Right_VentralDC) || \
   ((l) == Right_Accumbens_area))


// only structures that are 'pure' of one tissue type
#define IS_GRAY_CLASS(l) (IS_GM(l) || ((l) == Left_Caudate) || ((l) == Right_Caudate))

#define IS_WHITE_CLASS(l) (((l) == Left_Cerebral_White_Matter) || ((l) == Right_Cerebral_White_Matter))

#define IS_CSF_CLASS(l) (((l) == Left_Lateral_Ventricle) || ((l) == Right_Lateral_Ventricle) || ((l) == CSF) || ((l) == CSF_SA) || ((l) == Third_Ventricle) || ((l) == Fourth_Ventricle) || ((l) == Fifth_Ventricle) || ((l) == left_hippocampal_fissure) || ((l) == right_hippocampal_fissure) || ((l) == hippocampal_fissure))

#define IS_CLASS(l,c) (c == CSF_CLASS ? IS_CSF_CLASS(l) : c == GM_CLASS ? IS_GRAY_CLASS(l) : IS_WHITE_CLASS(l))

#include "mrisurf.h"
int insert_ribbon_into_aseg(MRI *mri_src_aseg, MRI *mri_aseg, 
                            MRI_SURFACE *mris_white, MRI_SURFACE *mris_pial, 
                            int hemi) ;


#if defined(__cplusplus)
};
#endif



#endif
