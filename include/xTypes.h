#ifndef xTypes_H
#define xTypes_H

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef NULL
#define NULL 0
#endif

typedef unsigned char tBoolean;
typedef long          tSignature;

typedef struct {
  int mnX, mnY;
} xPoint2n, *xPoint2nRef;

typedef struct {
  float mfX, mfY;
} xPoint2f, *xPoint2fRef;

typedef struct {
  int mnRed, mnGreen, mnBlue;
} xColor3n, *xColor3nRef;

typedef struct {
  float mfRed, mfGreen, mfBlue;
} xColor3f, *xColor3fRef;

typedef enum {
  tAxis_X = 0,
  tAxis_Y,
  tAxis_Z
} tAxis;

typedef enum {
  xColr_tComponent_None = 0,
  xColr_tComponent_Red,
  xColr_tComponent_Green,
  xColr_tComponent_Blue,
  xColr_knNumComponents
} xColr_tComponent;

void xColr_Set ( xColor3fRef iColor, 
     float ifRed, float ifGreen, float ifBlue );

void xColr_PackFloatArray ( xColor3fRef iColor,
          float*      iafColor );

void xColr_HilightComponent ( xColor3fRef      iColor,
            xColr_tComponent iComponent );

#define xColr_ExpandFloat(iColor) (iColor)->mfRed,(iColor)->mfGreen,(iColor)->mfBlue

#endif
