/*==========================================================================
 *
 *  Copyright (C) 1995-1999 Microsoft Corporation. All Rights Reserved.
 *
 *  File:       donuts.h
 *  Content:    main include file
 *
 *
 ***************************************************************************/

#ifndef __DONUTS_H__
#define __DONUTS_H__

/* bcc32 does not support nameless unions in C mode */
#if defined(__BORLANDC__) && !defined(__cplusplus)
#define NONAMELESSUNION
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <windowsx.h>

#include "ddutil.h"
#include "resource.h"
#include <ddraw.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef USE_DSOUND
#include "dsutil.h"
#endif
#include "input.h"

#define DEF_SHOW_DELAY (2000)

#define IS_NUM(c) ((c) >= '0' && (c) <= '9')
#define IS_SPACE(c) \
	((c) == ' ' || (c) == '\r' || (c) == '\n' || (c) == '\t' || (c) == 'x')

enum { OBJ_DONUT = 0, OBJ_PYRAMID, OBJ_CUBE, OBJ_SPHERE, OBJ_SHIP, OBJ_BULLET };

// program states
enum { PS_SPLASH, PS_ACTIVE, PS_BEGINREST, PS_REST };

#define MAX_SCREEN_X (ScreenX - 1)
#define MAX_SCREEN_Y (ScreenY - 1)
#define MAX_DONUT_X MAX_SCREEN_X - 64
#define MAX_DONUT_Y MAX_SCREEN_Y - 64
#define MAX_DONUT_FRAME 30
#define MAX_PYRAMID_X MAX_SCREEN_X - 32
#define MAX_PYRAMID_Y MAX_SCREEN_Y - 32
#define MAX_PYRAMID_FRAME 40
#define MAX_SPHERE_X MAX_SCREEN_X - 16
#define MAX_SPHERE_Y MAX_SCREEN_Y - 16
#define MAX_SPHERE_FRAME 40
#define MAX_CUBE_X MAX_SCREEN_X - 16
#define MAX_CUBE_Y MAX_SCREEN_Y - 16
#define MAX_CUBE_FRAME 40
#define MAX_SHIP_X MAX_SCREEN_X - 32
#define MAX_SHIP_Y MAX_SCREEN_Y - 32
#define MAX_SHIP_FRAME 40
#define MAX_BULLET_X MAX_SCREEN_X - 3;
#define MAX_BULLET_Y MAX_SCREEN_Y - 3;
#define MAX_BULLET_FRAME 400

// Offsets for the bullet bitmap
#define BULLET_X 304
#define BULLET_Y 0

/*
 * structures
 */

/*
 * DBLNODE - a node in a generic doubly-linked list
 */
typedef struct _DBLNODE {
	struct _DBLNODE FAR* next; // link to next node
	struct _DBLNODE FAR* prev; // link to previous node
	SHORT type;                // type of object
	double posx, posy;         // actual x and y position
	double velx, vely;         // x and y velocity (pixels/millisecond)
	double frame;              // current frame
	double delay;              // frame/millisecond
	RECT src, dst;             // source and destination rects
	LPDIRECTDRAWSURFACE7 surf; // surface containing bitmap
} DBLNODE;
typedef DBLNODE FAR* LPDBLNODE;

extern double Dirx[40];
extern double Diry[40];

/*
 * fn prototypes
 */
void DestroyGame(void);
BOOL InitializeGame(void);
void makeFontStuff(void);
void UpdateFrame(void);
BOOL CleanupAndExit(char* err);
BOOL RestoreSurfaces(void);
BOOL isDisplayListEmpty(void);
void initShip(BOOL delay);
void initLevel(int level);
void addObject(SHORT type, double x, double y, double vx, double vy);
void linkObject(LPDBLNODE newx);
void linkLastObject(LPDBLNODE newx);
void UpdateDisplayList(void);
void DrawDisplayList(void);
int randInt(int low, int high);
double randDouble(double low, double high);
void DeleteFromList(LPDBLNODE thisx);
void CheckForHits(void);
void bltScore(char* num, int x, int y);
void DisplayFrameRate(void);
void bltSplash(void);
void EraseScreen(void);
void FlipScreen(void);
void DisplayLevel(void);
void InitializeSound(void);
void DestroySound(void);

#endif
