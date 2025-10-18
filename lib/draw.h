
// draw.h : draw in a memory buffer

//--------------------------------------------------------------------------

struct DRAW_CONTEXT;

//--------------------------------------------------------------------------

/* initialize the DRAW_CONTEXT structure */
/* user must allocate a buffer of (4*width*height) bytes */
/* assertion: the buffer must have 4*width*height bytes */

void init_draw (out DRAW_CONTEXT dc,
                    byte[]^      buffer,     /* -> (4*width*height) bytes */
                    uint         width,
                    uint         height);

//--------------------------------------------------------------------------

/* build a color from red, green and blue components (each 0..255) */
uint rgb  (byte r, byte g, byte b);

//--------------------------------------------------------------------------

/* draw a line from (x1,y1) to (x2,y2) */

void draw_line (ref DRAW_CONTEXT dc,
                    int          x1,
                    int          y1,
                    int          x2,
                    int          y2,
                    uint         color,    /* = rgb(red,green,blue) */
                    int          thick,    /* >= 1                  */
                    bool         draw_last_pixel = true);
                    
//--------------------------------------------------------------------------

void draw_circle (ref DRAW_CONTEXT dc,
                      int          x,
                      int          y,
                      int          radius,
                      uint         color,
                      int          thick);

//--------------------------------------------------------------------------

void draw_solid_circle (ref DRAW_CONTEXT dc,
                            int          x,
                            int          y,
                            int          radius,
                            uint         color);

//--------------------------------------------------------------------------

/* returns width of text (in pixels), or a negative value in case of error */

int draw_text (ref DRAW_CONTEXT  dc,
                   string        text,           /* string to display */
                   string        font_name,      /* ex: "Times New Roman" */
                   int           font_height,    /* ex: 12 */
                   int           x,              /* lower left corner */
                   int           y,
                   uint          color,
                   uint          style);         /* = 0 */

int draw_wtext (ref DRAW_CONTEXT  dc,
                   wstring       text,           /* string to display */
                   string        font_name,      /* ex: "Times New Roman" */
                   int           font_height,    /* ex: 12 */
                   int           x,              /* lower left corner */
                   int           y,
                   uint          color,
                   uint          style);         /* = 0 */

/* style constants : */
const uint _STYLE_UNDERLINED = 0x0001;
const uint _STYLE_ITALIC     = 0x0002;
const uint _STYLE_BOLD       = 0x0004;

//--------------------------------------------------------------------------

void draw_image (ref DRAW_CONTEXT  dc,
                     int           x,            /* target (upper-left corner) */
                     int           y,
                     byte[]        image,
                     uint          width,
                     uint          height,
                     uint          clip_x,       /* =0 for all image */
                     uint          clip_y,       /* =0 for all image */
                     uint          clip_size_x,  /* =width for all image */
                     uint          clip_size_y,  /* =height for all image */
                     uint          flags = 0);

//--------------------------------------------------------------------------

/* set origin of coordinates.                         */
/* examples:                                          */
/*                                                    */
/*   ox       oy        origin                        */
/*   --       ---       ------                        */
/*   0        0         upper_left_corner (default)   */
/*   width/2  height/2  center                        */
/*   0        height-1  lower_left_corner             */
/*                                                    */
/*   dx = +1 : x increases when going right (default) */
/*   dx = -1 : x increases when going left            */
/*   dy = +1 : y increases when going down (default)  */
/*   dy = -1 : y increases when going up              */

void set_draw_origin (ref DRAW_CONTEXT dc,
                          int          ox,
                          int          oy,
                          int          dx,   /* direction of x-axis */
                          int          dy);  /* direction of y-axis */

//--------------------------------------------------------------------------

/* limit the area on which drawing can occur */

void set_draw_clip (ref DRAW_CONTEXT dc,
                        int          min_x,
                        int          min_y,
                        int          max_x,
                        int          max_y);

//--------------------------------------------------------------------------
