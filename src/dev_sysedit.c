/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file dev_sysedit.c
 *
 * @brief Handles the star system editor.
 */

#include "dev_sysedit.h"

#include "naev.h"

#include "SDL.h"

#include "space.h"
#include "toolkit.h"
#include "opengl.h"
#include "map.h"
#include "dev_system.h"
#include "unidiff.h"
#include "dialogue.h"
#include "tk/toolkit_priv.h"


#define BUTTON_WIDTH    80 /**< Map button width. */
#define BUTTON_HEIGHT   30 /**< Map button height. */


#define SYSEDIT_EDIT_WIDTH       400 /**< System editor width. */
#define SYSEDIT_EDIT_HEIGHT      300 /**< System editor height. */


#define SYSEDIT_DRAG_THRESHOLD   300   /**< Drag threshold. */
#define SYSEDIT_MOVE_THRESHOLD   10    /**< Movement threshold. */


/*
 * The editor modes.
 */
#define SYSEDIT_DEFAULT    0  /**< Default editor mode. */
#define SYSEDIT_JUMP       1  /**< Jump point toggle mode. */
#define SYSEDIT_NEWSYS     2  /**< New system editor mode. */


static StarSystem *sysedit_sys = NULL; /**< Currently opened system. */
static unsigned int sysedit_wid = 0; /**< Sysedit wid. */
static double sysedit_xpos    = 0.; /**< Viewport X position. */
static double sysedit_ypos    = 0.; /**< Viewport Y position. */
static double sysedit_zoom    = 1.; /**< Viewport zoom level. */
static int sysedit_moved      = 0;  /**< Space moved since mouse down. */
static unsigned int sysedit_dragTime = 0; /**< Tick last started to drag. */
static int sysedit_drag       = 0;  /**< Dragging viewport around. */
static int sysedit_dragSys    = 0;  /**< Dragging system around. */


/*
 * Universe editor Prototypes.
 */
/* Selection. */
/*
static void sysedit_deselect (void);
static void sysedit_selectAdd( StarSystem *sys );
static void sysedit_selectRm( StarSystem *sys );
static void sysedit_selectText (void);
*/
/* Custom system editor widget. */
static void sysedit_buttonZoom( unsigned int wid, char* str );
static void sysedit_render( double bx, double by, double w, double h, void *data );
static void sysedit_renderSprite( glTexture *gfx, double bx, double by, double x, double y, int sx, int sy );
static void sysedit_renderOverlay( double bx, double by, double bw, double bh, void* data );
static void sysedit_mouse( unsigned int wid, SDL_Event* event, double mx, double my,
      double w, double h, void *data );
/* Button functions. */
static void sysedit_close( unsigned int wid, char *wgt );
static void sysedit_save( unsigned int wid_unused, char *unused );
static void sysedit_btnNew( unsigned int wid_unused, char *unused );
/* Keybindings handling. */
static int sysedit_keys( unsigned int wid, SDLKey key, SDLMod mod );


/**
 * @brief Opens the system editor interface.
 */
void sysedit_open( StarSystem *sys )
{
   unsigned int wid;

   /* Reconstructs the jumps - just in case. */
   systems_reconstructJumps();

   /* Reset some variables. */
   sysedit_sys    = sys;
   sysedit_drag   = 0;
   sysedit_zoom   = 1.;
   sysedit_xpos   = 0.;
   sysedit_ypos   = 0.;

   /* Create the window. */
   wid = window_create( "Star System Editor", -1, -1, -1, -1 );
   window_handleKeys( wid, sysedit_keys );
   sysedit_wid = wid;

   /* Close button. */
   window_addButton( wid, -20, 20, BUTTON_WIDTH, BUTTON_HEIGHT,
         "btnClose", "Close", sysedit_close );

   /* Save button. */
   window_addButton( wid, -20, 20+(BUTTON_HEIGHT+20)*1, BUTTON_WIDTH, BUTTON_HEIGHT,
         "btnSave", "Save", sysedit_save );

   /* New system. */
   window_addButton( wid, -20, 20+(BUTTON_HEIGHT+20)*6, BUTTON_WIDTH, BUTTON_HEIGHT,
         "btnNew", "New Sys", sysedit_btnNew );

   /* Zoom buttons */
   window_addButton( wid, 40, 20, 30, 30, "btnZoomIn", "+", sysedit_buttonZoom );
   window_addButton( wid, 80, 20, 30, 30, "btnZoomOut", "-", sysedit_buttonZoom );

   /* Selected text. */
   window_addText( wid, 140, 10, SCREEN_W - 80 - 30 - 30 - BUTTON_WIDTH - 20, 30, 0,
         "txtSelected", &gl_smallFont, &cBlack, NULL );

   /* Actual viewport. */
   window_addCust( wid, 20, -40, SCREEN_W - 150, SCREEN_H - 100,
         "cstSysEdit", 1, sysedit_render, sysedit_mouse, NULL );
   window_custSetOverlay( wid, "cstSysEdit", sysedit_renderOverlay );

   /* Deselect everything. */
   /*sysedit_deselect();*/
}


/**
 * @brief Handles keybindings.
 */
static int sysedit_keys( unsigned int wid, SDLKey key, SDLMod mod )
{
   (void) wid;
   (void) mod;

   switch (key) {

      default:
         return 0;
   }
}


/**
 * @brief Closes the system editor widget.
 */
static void sysedit_close( unsigned int wid, char *wgt )
{
   /* Reconstruct jumps. */
   systems_reconstructJumps();

   /* Close the window. */
   window_close( wid, wgt );
}


/**
 * @brief Saves the systems.
 */
static void sysedit_save( unsigned int wid_unused, char *unused )
{
   (void) wid_unused;
   (void) unused;
}

/**
 * @brief Enters the editor in new system mode.
 */
static void sysedit_btnNew( unsigned int wid_unused, char *unused )
{
   (void) wid_unused;
   (void) unused;
}


/**
 * @brief System editor custom widget rendering.
 */
static void sysedit_render( double bx, double by, double w, double h, void *data )
{
   (void) data;
   int i;
   StarSystem *sys;
   Planet *p;
   JumpPoint *jp;
   double x,y;

   /* Comfort++. */
   sys = sysedit_sys;

   /* Background */
   gl_renderRect( bx, by, w, h, &cBlack );

   /* Coordinate translation. */
   x = round((bx - sysedit_xpos + w/2) * 1.);
   y = round((by - sysedit_ypos + h/2) * 1.);

   /* Render planets. */
   for (i=0; i<sys->nplanets; i++) {
      p     = sys->planets[i];
      sysedit_renderSprite( p->gfx_space, x, y, p->pos.x, p->pos.y, 0, 0 );
   }

   /* Render jump points. */
   for (i=0; i<sys->njumps; i++) {
      jp    = &sys->jumps[i];
      sysedit_renderSprite( jumppoint_gfx, x, y, jp->pos.x, jp->pos.y, jp->sx, jp->sy );
   }
}


/**
 * @brief Renders a sprite for the custom widget.
 */
static void sysedit_renderSprite( glTexture *gfx, double bx, double by, double x, double y, int sx, int sy )
{
   double tx, ty, z;

   /* Comfort. */
   z  = sysedit_zoom;

   /* Translate coords. */
   tx = bx + (x - gfx->sw/2.)*z + SCREEN_W/2.;
   ty = by + (y - gfx->sh/2.)*z + SCREEN_H/2.;

   /* Blit the planet. */
   gl_blitScaleSprite( gfx, tx, ty, sx, sy, gfx->sw*z, gfx->sh*z, NULL );
}


/**
 * @brief Renders the overlay.
 */
static void sysedit_renderOverlay( double bx, double by, double bw, double bh, void* data )
{
   (void) bx;
   (void) by;
   (void) bw;
   (void) bh;
   (void) data;
}

/**
 * @brief System editor custom widget mouse handling.
 */
static void sysedit_mouse( unsigned int wid, SDL_Event* event, double mx, double my,
      double w, double h, void *data )
{
   (void) wid;
   (void) data;
   int i;
   double x,y, t;
   SDLMod mod;

   t = 15.*15.; /* threshold */

   /* Handle modifiers. */
   mod = SDL_GetModState();

   switch (event->type) {
      
      case SDL_MOUSEBUTTONDOWN:
         /* Must be in bounds. */
         if ((mx < 0.) || (mx > w) || (my < 0.) || (my > h))
            return;

         /* Zooming */
         if (event->button.button == SDL_BUTTON_WHEELUP)
            sysedit_buttonZoom( 0, "btnZoomIn" );
         else if (event->button.button == SDL_BUTTON_WHEELDOWN)
            sysedit_buttonZoom( 0, "btnZoomOut" );

         /* selecting star system */
         else {
            mx -= w/2 - sysedit_xpos;
            my -= h/2 - sysedit_ypos;

#if 0
            for (i=0; i<systems_nstack; i++) {
               sys = system_getIndex( i );

               /* get position */
               x = sys->pos.x * sysedit_zoom;
               y = sys->pos.y * sysedit_zoom;

               if ((pow2(mx-x)+pow2(my-y)) < t) {

                  /* Try to find in selected systems - begin drag move. */
                  for (i=0; i<sysedit_nsys; i++) {
                     if (sysedit_sys[i] == sys) {
                        sysedit_dragSys   = 1;
                        sysedit_tsys      = sys;

                        /* Check modifier. */
                        if (mod & (KMOD_LCTRL | KMOD_RCTRL))
                           sysedit_tadd      = 0;
                        else
                           sysedit_tadd      = -1;
                        sysedit_dragTime  = SDL_GetTicks();
                        sysedit_moved     = 0;
                        return;
                     }
                  }

                  if (sysedit_mode == SYSEDIT_DEFAULT) {
                     /* Add the system if not selected. */
                     if (mod & (KMOD_LCTRL | KMOD_RCTRL))
                        sysedit_selectAdd( sys );
                     else {
                        sysedit_deselect();
                        sysedit_selectAdd( sys );
                     }
                     sysedit_tsys      = NULL;

                     /* Start dragging anyway. */
                     sysedit_dragSys   = 1;
                     sysedit_dragTime  = SDL_GetTicks();
                     sysedit_moved     = 0;
                  }
                  else if (sysedit_mode == SYSEDIT_JUMP) {
                     sysedit_toggleJump( sys );
                     sysedit_mode = SYSEDIT_DEFAULT;
                  }
                  return;
               }
            }
#endif

            /* Start dragging. */
            if (!(mod & (KMOD_LCTRL | KMOD_RCTRL))) {
               sysedit_drag      = 1;
               sysedit_dragTime  = SDL_GetTicks();
               sysedit_moved     = 0;
            }
            return;
         }
         break;

      case SDL_MOUSEBUTTONUP:
         if (sysedit_drag) {
#if 0
            if ((SDL_GetTicks() - sysedit_dragTime < SYSEDIT_DRAG_THRESHOLD) && (sysedit_moved < SYSEDIT_MOVE_THRESHOLD)) {
               if (sysedit_tsys == NULL)
                  sysedit_deselect();
               else
                  sysedit_selectAdd( sysedit_tsys );
            }
#endif
            sysedit_drag      = 0;
         }
#if 0
         if (sysedit_dragSys) {
            if ((SDL_GetTicks() - sysedit_dragTime < SYSEDIT_DRAG_THRESHOLD) &&
                  (sysedit_moved < SYSEDIT_MOVE_THRESHOLD) && (sysedit_tsys != NULL)) {
               if (sysedit_tadd == 0)
                  sysedit_selectRm( sysedit_tsys );
               else {
                  sysedit_deselect();
                  sysedit_selectAdd( sysedit_tsys );
               }
            }
            sysedit_dragSys   = 0;
         }
#endif
         break;

      case SDL_MOUSEMOTION:
         /* Handle dragging. */
         if (sysedit_drag) {
            /* axis is inverted */
            sysedit_xpos -= event->motion.xrel;
            sysedit_ypos += event->motion.yrel;

            /* Update mousemovement. */
            sysedit_moved += ABS( event->motion.xrel ) + ABS( event->motion.yrel );
         }
#if 0
         else if (sysedit_dragSys && (sysedit_nsys > 0)) {
            if ((sysedit_moved > SYSEDIT_MOVE_THRESHOLD) || (SDL_GetTicks() - sysedit_dragTime > SYSEDIT_DRAG_THRESHOLD)) {
               for (i=0; i<sysedit_nsys; i++) {
                  sysedit_sys[i]->pos.x += ((double)event->motion.xrel) / sysedit_zoom;
                  sysedit_sys[i]->pos.y -= ((double)event->motion.yrel) / sysedit_zoom;
               }
            }

            /* Update mousemovement. */
            sysedit_moved += ABS( event->motion.xrel ) + ABS( event->motion.yrel );
         }
#endif
         break;
   }
}


/**
 * @brief Handles the button zoom clicks.
 *
 *    @param wid Unused.
 *    @param str Name of the button creating the event.
 */
static void sysedit_buttonZoom( unsigned int wid, char* str )
{
   (void) wid;

   /* Transform coords to normal. */
   sysedit_xpos /= sysedit_zoom;
   sysedit_ypos /= sysedit_zoom;

   /* Apply zoom. */
   if (strcmp(str,"btnZoomIn")==0) {
      sysedit_zoom += (sysedit_zoom >= 1.) ? 0.5 : 0.25;
      sysedit_zoom = MIN(2.5, sysedit_zoom);
   }
   else if (strcmp(str,"btnZoomOut")==0) {
      sysedit_zoom -= (sysedit_zoom > 1.) ? 0.5 : 0.25;
      sysedit_zoom = MAX(0.25, sysedit_zoom);
   }

   /* Hack for the circles to work. */
   map_setZoom(sysedit_zoom);

   /* Transform coords back. */
   sysedit_xpos *= sysedit_zoom;
   sysedit_ypos *= sysedit_zoom;
}

