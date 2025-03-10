//
// Draw-to-image code for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2021 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//
// Please see the following page on how to report bugs and issues:
//
//     https://www.fltk.org/bugs.php
//

#include <FL/Fl_Image_Surface.H>

#include <FL/fl_draw.H> // necessary for FL_EXPORT fl_*_offscreen()

#include <stdlib.h>     // realloc()

/** Constructor with optional high resolution.
 \param w,h Width and height of the resulting image. The value of the \p high_res
 parameter controls whether \p w and \p h are interpreted as pixels or FLTK units.

 \param high_res If zero, the created image surface is sized at \p w x \p h pixels.
 If non-zero, the pixel size of the created image surface depends on
 the value of the display scale factor (see Fl::screen_scale(int)):
 the resulting image has the same number of pixels as an area of the display of size
 \p w x \p h expressed in FLTK units.

 \param off If not null, the image surface is constructed around a pre-existing
 Fl_Offscreen. The caller is responsible for both construction and destruction of this Fl_Offscreen object.
 Is mostly intended for internal use by FLTK.
 \version 1.3.4 (1.3.3 without the \p highres parameter)
 */
Fl_Image_Surface::Fl_Image_Surface(int w, int h, int high_res, Fl_Offscreen off) : Fl_Widget_Surface(NULL) {
  platform_surface = Fl_Image_Surface_Driver::newImageSurfaceDriver(w, h, high_res, off);
  platform_surface->image_surface_ = this;
  driver(platform_surface->driver());
}


/** The destructor. */
Fl_Image_Surface::~Fl_Image_Surface() {
  if (is_current()) platform_surface->end_current();
  delete platform_surface;
}

void Fl_Image_Surface::origin(int x, int y) {platform_surface->origin(x, y);}

void Fl_Image_Surface::origin(int *x, int *y) {
  if (platform_surface) platform_surface->origin(x, y);
}

void Fl_Image_Surface::set_current() {
  if (platform_surface) platform_surface->set_current();
}

bool Fl_Image_Surface::is_current() {
  return surface() == platform_surface;
}

void Fl_Image_Surface::translate(int x, int y) {
  if (platform_surface) platform_surface->translate(x, y);
}

void Fl_Image_Surface::untranslate() {
  if (platform_surface) platform_surface->untranslate();
}

/** Returns the Fl_Offscreen object associated to the image surface.
 The returned Fl_Offscreen object is deleted when the Fl_Image_Surface object is deleted,
 unless the Fl_Image_Surface was constructed with non-null Fl_Offscreen argument.
 */
Fl_Offscreen Fl_Image_Surface::offscreen() {
  return platform_surface ? platform_surface->offscreen : (Fl_Offscreen)0;
}

int Fl_Image_Surface::printable_rect(int *w, int *h)  {return platform_surface->printable_rect(w, h);}

/**
 \cond DriverDev
 \addtogroup DriverDeveloper
 \{
 */
int Fl_Image_Surface_Driver::printable_rect(int *w, int *h) {
  *w = width; *h = height;
  return 0;
}

// used by the Windows and X11(no Cairo) platforms
void Fl_Image_Surface_Driver::copy_with_mask(Fl_RGB_Image* mask, uchar *dib_dst,
                                             uchar *dib_src, int line_size,
                                             bool bottom_to_top) {
  int w = mask->data_w(), h = mask->data_h();
  for (int i = 0; i < h; i++) {
    const uchar* alpha = (const uchar*)mask->array + 
      (bottom_to_top ? (h-i-1) : i) * w;
    uchar *src = dib_src + i * line_size;
    uchar *dst = dib_dst + i * line_size;
    for (int j = 0; j < w; j++) {
      // mix src and dst into dst weighted by mask pixel's value
      uchar u = *alpha++, v = 255 - u;
      *dst = ((*dst) * v + (*src) * u)/255;
      dst++; src++;
      *dst = ((*dst) * v + (*src) * u)/255;
      dst++; src++;
      *dst = ((*dst) * v + (*src) * u)/255;
      dst++; src++;
    }
  }
}


Fl_RGB_Image *Fl_Image_Surface_Driver::RGB3_to_RGB1(const Fl_RGB_Image *rgb3, int W, int H) {
  bool need_copy = false;
  if (W != rgb3->data_w() || H != rgb3->data_h()) {
    rgb3 = (Fl_RGB_Image*)rgb3->copy(W, H);
    need_copy = true;
  }
  uchar *data = new uchar[W * H];
  int i, j, ld = rgb3->ld();
  if (!ld) ld = 3 * W;
  uchar *p = data;
  for (i = 0; i < H; i++) {
    const uchar* alpha = rgb3->array + i * ld;
    for (j = 0; j < W; j++) {
      *p++ = (*alpha + *(alpha+1) + *(alpha+2)) / 3;
      alpha += 3;
    }
  }
  Fl_RGB_Image *rgb1 = new Fl_RGB_Image(data, W, H, 1);
  rgb1->alloc_array = 1;
  if (need_copy) delete rgb3;
  return rgb1;
}

/**
 \}
 \endcond
 */

/** Returns a depth-3 image made of all drawings sent to the Fl_Image_Surface object.
 The returned object contains its own copy of the RGB data;
 the caller is responsible for deleting it.
 
 \see Fl_Image_Surface::mask(Fl_RGB_Image*)
 */
Fl_RGB_Image *Fl_Image_Surface::image() {
  bool need_push = (Fl_Surface_Device::surface() != platform_surface);
  if (need_push) Fl_Surface_Device::push_current(platform_surface);
  Fl_RGB_Image *img = platform_surface->image();
  if (need_push) Fl_Surface_Device::pop_current();
  img->scale(platform_surface->width, platform_surface->height, 1, 1);
  return img;
}

/** Returns a possibly high resolution image made of all drawings sent to the Fl_Image_Surface object.
 The Fl_Image_Surface object should have been constructed with Fl_Image_Surface(W, H, 1).
 The returned Fl_Shared_Image object is scaled to a size of WxH FLTK units and may have a
 pixel size larger than these values.
 The returned object should be deallocated with Fl_Shared_Image::release() after use.
 \deprecated Use image() instead.
 \version 1.4 (1.3.4 for MacOS platform only)
 */
Fl_Shared_Image* Fl_Image_Surface::highres_image()
{
  if (!platform_surface) return NULL;
  Fl_Shared_Image *s_img = Fl_Shared_Image::get(image());
  int width, height;
  platform_surface->printable_rect(&width, &height);
  s_img->scale(width, height, 1, 1);
  return s_img;
}

// Allows to delete the Fl_Image_Surface object while keeping its underlying Fl_Offscreen
Fl_Offscreen Fl_Image_Surface::get_offscreen_before_delete_() {
  Fl_Offscreen keep = platform_surface->offscreen;
  platform_surface->offscreen = 0;
  return keep;
}

/** Adapts the Fl_Image_Surface object to the new value of the GUI scale factor.
 The Fl_Image_Surface object must not be the current drawing surface.
 This function is useful only for an object constructed with non-zero \p high_res parameter.
 \version 1.4
 */
void Fl_Image_Surface::rescale() {
  Fl_RGB_Image *rgb = image();
  int w, h;
  printable_rect(&w, &h);
  delete platform_surface;
  platform_surface = Fl_Image_Surface_Driver::newImageSurfaceDriver(w, h, 1, 0);
  Fl_Surface_Device::push_current(this);
  rgb->draw(0,0);
  Fl_Surface_Device::pop_current();
  delete rgb;
}


/** Defines a mask applied to drawings made after use of this function.
 The mask is an Fl_RGB_Image made of a white scene drawn on a solid black
 background; the drawable part of the image surface is reduced to the white areas of the mask
 after this member function gets called. If necessary, the \p mask image is internally
 replaced by a copy resized to the surface's pixel size.
 Overall, the image returned by Fl_Image_Surface::image() contains all drawings made
 until the mask() method assigned a mask, at which point subsequent drawing operations
 to the image surface were passed through the white areas of the mask.
 On some platforms, shades of gray in the mask image control the blending of
 foreground and background pixels; mask pixels closer in color to white produce image pixels
 closer to the image surface pixel, those closer to black produce image pixels closer to what the
 image surface pixel was before the call to mask().
 
 The mask is easily constructed using an Fl_Image_Surface object,
 drawing white areas on a black background there, and calling Fl_Image_Surface::image().
 \param mask A depth-3 image determining the drawable areas of the image surface.
 The \p mask object is not used after return from this member function.
 \note 
 - The image surface must not be the current drawing surface when this function
 gets called.
 - The mask can have any size but is best when it has the size of the image surface.
 - It's possible to use several masks in succession on the same image surface provided
 member function Fl_Image_Surface::image() is called between successive calls to
 Fl_Image_Surface::mask(const Fl_RGB_Image*).
 
 Example of procedure to construct a masked image:
 \code
 int W = …, H = …; // width and height of the image under construction
 Fl_Image_Surface *surf = new Fl_Image_Surface(W, H, 1);
 // first, construct the mask
 Fl_Surface_Device::push_current(surf);
 fl_color(FL_BLACK); // draw a black background
 fl_rectf(0, 0, W, H);
 fl_color(FL_WHITE); // next, draw in white what the mask should not filter out
 fl_pie(0, 0, W, H, 0, 360); // here, an ellipse with axes lengths WxH
 Fl_RGB_Image *mask = surf->image(); // get the mask
 // second, draw the image background
 fl_color(FL_YELLOW); // here, draw a yellow background
 fl_rectf(0, 0, W, H);
 // third, apply the mask
 Fl_Surface_Device::pop_current();
 surf->mask(mask);
 delete mask; // the mask image can be safely deleted at this point
 Fl_Surface_Device::push_current(surf);
 // fourth, draw the image foreground, part of which will be filtered out by the mask
 surf->draw(widget, 0, 0); // here the foreground is a drawn widget
 // fifth, get the final result, masked_image, as a depth-3 Fl_RGB_Image
 Fl_RGB_Image *masked_image = surf->image();
 // Only the part of the foreground, here a drawn widget, that has not been
 // filtered out by the mask, here the white ellipse, is in masked_image;
 // the background, here solid yellow, shows up in the remaining areas of masked_image.
 Fl_Surface_Device::pop_current();
 delete surf;
 \endcode

 \since 1.4.0
 */
void Fl_Image_Surface::mask(const Fl_RGB_Image *mask) {
  platform_surface->mask(mask);
}


// implementation of the fl_XXX_offscreen() functions

static Fl_Image_Surface **offscreen_api_surface = NULL;
static int count_offscreens = 0;

static int find_slot(void) { // return an available slot to memorize an Fl_Image_Surface object
  static int max = 0;
  for (int num = 0; num < count_offscreens; num++) {
    if (!offscreen_api_surface[num]) return num;
  }
  if (count_offscreens >= max) {
    max += 20;
    offscreen_api_surface = (Fl_Image_Surface**)realloc(offscreen_api_surface, max * sizeof(void *));
  }
  return count_offscreens++;
}

/** \addtogroup fl_drawings
   @{
   */

/**
   Creation of an offscreen graphics buffer.
   \param w,h     width and height in FLTK units of the buffer.
   \return    the created graphics buffer.

 The pixel size of the created graphics buffer is equal to the number of pixels
 in an area of the screen containing the current window sized at \p w,h FLTK units.
 This pixel size varies with the value of the scale factor of this screen.
 \note Work with the fl_XXX_offscreen() functions is equivalent to work with
 an Fl_Image_Surface object, as follows :
 <table>
 <tr> <th>Fl_Offscreen-based approach</th><th>Fl_Image_Surface-based approach</th> </tr>
 <tr> <td>Fl_Offscreen off = fl_create_offscreen(w, h)</td><td>Fl_Image_Surface *surface = new Fl_Image_Surface(w, h, 1)</td> </tr>
 <tr> <td>fl_begin_offscreen(off)</td><td>Fl_Surface_Device::push_current(surface)</td> </tr>
 <tr> <td>fl_end_offscreen()</td><td>Fl_Surface_Device::pop_current()</td> </tr>
 <tr> <td>fl_copy_offscreen(x,y,w,h, off, sx,sy)</td><td>fl_copy_offscreen(x,y,w,h, surface->offscreen(), sx,sy)</td> </tr>
 <tr> <td>fl_rescale_offscreen(off)</td><td>surface->rescale()</td> </tr>
 <tr> <td>fl_delete_offscreen(off)</td><td>delete surface</td> </tr>
 </table>
   */
Fl_Offscreen fl_create_offscreen(int w, int h) {
  int rank = find_slot();
  offscreen_api_surface[rank] = new Fl_Image_Surface(w, h, 1/*high_res*/);
  return offscreen_api_surface[rank]->offscreen();
}

/**  Deletion of an offscreen graphics buffer.
   \param ctx     the buffer to be deleted.
   \note The \p ctx argument must have been created by fl_create_offscreen().
   */
void fl_delete_offscreen(Fl_Offscreen ctx) {
  if (!ctx) return;
  for (int i = 0; i < count_offscreens; i++) {
    if (offscreen_api_surface[i] && offscreen_api_surface[i]->offscreen() == ctx) {
      delete offscreen_api_surface[i];
      offscreen_api_surface[i] = NULL;
      return;
    }
  }
}

/**  Send all subsequent drawing commands to this offscreen buffer.
   \param ctx     the offscreen buffer.
   \note The \p ctx argument must have been created by fl_create_offscreen().
   */
void fl_begin_offscreen(Fl_Offscreen ctx) {
  for (int i = 0; i < count_offscreens; i++) {
    if (offscreen_api_surface[i] && offscreen_api_surface[i]->offscreen() == ctx) {
      Fl_Surface_Device::push_current(offscreen_api_surface[i]);
      return;
    }
  }
}

/** Quit sending drawing commands to the current offscreen buffer.
   */
void fl_end_offscreen() {
  Fl_Surface_Device::pop_current();
}

/** Adapts an offscreen buffer to a changed value of the scale factor.
 The \p ctx argument must have been created by fl_create_offscreen()
 and the calling context must not be between fl_begin_offscreen() and fl_end_offscreen().
 The graphical content of the offscreen is preserved. The current scale factor
 value is given by <tt>Fl_Graphics_Driver::default_driver().scale()</tt>.
 \version 1.4
 */
void fl_rescale_offscreen(Fl_Offscreen &ctx) {
  int i;
  for (i = 0; i < count_offscreens; i++) {
    if (offscreen_api_surface[i] && offscreen_api_surface[i]->offscreen() == ctx) {
      break;
    }
  }
  if (i >= count_offscreens) return;
  offscreen_api_surface[i]->rescale();
  ctx = offscreen_api_surface[i]->offscreen();
}

/** @} */
