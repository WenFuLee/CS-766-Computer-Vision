
/* -------------------------------------------------------------------------
  Minimal (unoptimized) example of PatchMatch. Requires that ImageMagick be installed.

  To improve generality you can:
   - Use whichever distance function you want in dist(), e.g. compare SIFT descriptors computed densely.
   - Search over a larger search space, such as rotating+scaling patches (see MATLAB mex for examples of both)
  
  To improve speed you can:
   - Turn on optimizations (/Ox /Oi /Oy /fp:fast or -O6 -s -ffast-math -fomit-frame-pointer -fstrength-reduce -msse2 -funroll-loops)
   - Use the MATLAB mex which is already tuned for speed
   - Use multiple cores, tiling the input. See our publication "The Generalized PatchMatch Correspondence Algorithm"
   - Tune the distance computation: manually unroll loops for each patch size, use SSE instructions (see readme)
   - Precompute random search samples (to avoid using rand, and mod)
   - Move to the GPU
  -------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <limits.h>
#include <sstream>
#include <assert.h>

#include <iostream>

#ifndef MAX
#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))
#endif

#define _DEBUG

/* -------------------------------------------------------------------------
   BITMAP: Minimal image class
   ------------------------------------------------------------------------- */

class BITMAP { public:
  int w, h;
  int *data;
  BITMAP(int w_, int h_) :w(w_), h(h_) { data = new int[w*h]; }
  BITMAP(BITMAP* bm) {
    w = bm->w;
    h = bm->h;
    data = new int[w*h];
    for (int i = 0; i < w*h; ++i) {
        data[i] = bm->data[i];
    }
  }
  ~BITMAP() { delete[] data; }
  int *operator[](int y) { return &data[y*w]; }
};

void check_im() {
  int i;
  i = system("identify ../test-images/test1.jpg");
  printf ("The value returned was: %d.\n",i);
  if (i != 0) {
    fprintf(stderr, "ImageMagick must be installed, and 'convert' and 'identify' must be in the path\n"); exit(1);
  }
}

BITMAP *load_bitmap(const char *filename) {
  // check_im();
  char rawname[256], txtname[256];
  strcpy(rawname, filename);
  strcpy(txtname, filename);
  if (!strstr(rawname, ".")) { fprintf(stderr, "Error reading image '%s': no extension found\n", filename); exit(1); }
  sprintf(strstr(rawname, "."), ".raw");
  sprintf(strstr(txtname, "."), ".txt");
  char buf[256];
  sprintf(buf, "convert %s rgba:%s", filename, rawname);
  if (system(buf) != 0) { fprintf(stderr, "Error reading image '%s': ImageMagick convert gave an error\n", filename); exit(1); }
  sprintf(buf, "identify -format \"%%w %%h\" %s > %s", filename, txtname);
  if (system(buf) != 0) { fprintf(stderr, "Error reading image '%s': ImageMagick identify gave an error\n", filename); exit(1); }
  FILE *f = fopen(txtname, "rt");
  if (!f) { fprintf(stderr, "Error reading image '%s': could not read output of ImageMagick identify\n", filename); exit(1); }
  int w = 0, h = 0;
  if (fscanf(f, "%d %d", &w, &h) != 2) { fprintf(stderr, "Error reading image '%s': could not get size from ImageMagick identify\n", filename); exit(1); }
  fclose(f);
  printf("(w, h) = (%d, %d)\n", w, h);
  f = fopen(rawname, "rb");
  BITMAP *ans = new BITMAP(w, h);
  unsigned char *p = (unsigned char *) ans->data;
  for (int i = 0; i < w*h*4; i++) {
    int ch = fgetc(f);
    if (ch == EOF) { fprintf(stderr, "Error reading image '%s': raw file is smaller than expected size %dx%dx%d\n", filename, w, h, 4); exit(1); }
    *p++ = ch;
  }
  fclose(f);
  return ans;
}

void save_bitmap(BITMAP *bmp, const char *filename) {
  // check_im();
  char rawname[256];
  strcpy(rawname, filename);
  if (!strstr(rawname, ".")) { fprintf(stderr, "Error writing image '%s': no extension found\n", filename); exit(1); }
  sprintf(strstr(rawname, "."), ".raw");
  char buf[256];
  //printf("rawname = %s\n", rawname);
  FILE *f = fopen(rawname, "wb");
  if (!f) { fprintf(stderr, "Error writing image '%s': could not open raw temporary file\n", filename); exit(1); }
  unsigned char *p = (unsigned char *) bmp->data;
  for (int i = 0; i < bmp->w*bmp->h*4; i++) {
    fputc(*p++, f);
  }
  fclose(f);
  sprintf(buf, "convert -size %dx%d -depth 8 rgba:%s %s", bmp->w, bmp->h, rawname, filename);
  //printf("system returned value = %d\n", system(buf));
  if (system(buf) != 0) { fprintf(stderr, "Error writing image '%s': ImageMagick convert gave an error\n", filename); exit(1); }
}

/* -------------------------------------------------------------------------
   PatchMatch, using L2 distance between upright patches that translate only
   ------------------------------------------------------------------------- */

int patch_w  = 7;
int pm_iters = 6;
int rs_max   = INT_MAX; // random search

#define XY_TO_INT(x, y) (((y)<<12)|(x))
#define INT_TO_X(v) ((v)&((1<<12)-1))
#define INT_TO_Y(v) ((v)>>12)

bool isHole(BITMAP *mask, int x, int y) {
  int c = (*mask)[y][x];
  int r = c&255;
  int g = (c>>8)&255;
  int b = (c>>16)&255;

  // hole means non-black pixels in mask
  if (r != 0 || g != 0 || b != 0) { return true; }
  return false;
}

/* check if a pixel x, y is in the bounding box or not */
bool inBox(int x, int y, int box_xmin, int box_xmax, int box_ymin, int box_ymax) {
  if (x >= box_xmin && x <= box_xmax+patch_w && y >= box_ymin && y <= box_ymax+patch_w) {
    return true;
  }
  return false;
}

/* Measure distance between 2 patches with upper left corners (ax, ay) and (bx, by), terminating early if we exceed a cutoff distance.
   You could implement your own descriptor here. */
int dist(BITMAP *a, BITMAP *b, int ax, int ay, int bx, int by, BITMAP *mask, int cutoff=INT_MAX) {
  int ans = 0;
  for (int dy = 0; dy < patch_w; dy++) {
    int *arow = &(*a)[ay+dy][ax];
    int *brow = &(*b)[by+dy][bx];
    for (int dx = 0; dx < patch_w; dx++) {
      int ac = arow[dx];
      int bc = brow[dx];
      int dr = (ac&255)-(bc&255);
      int dg = ((ac>>8)&255)-((bc>>8)&255);
      int db = (ac>>16)-(bc>>16);
      ans += dr*dr + dg*dg + db*db;
    }
    if (ans >= cutoff) { return cutoff; }
  }
  if (ans < 0) return INT_MAX;
  return ans;
}

void improve_guess(BITMAP *a, BITMAP *b, int ax, int ay, int &xbest, int &ybest, int &dbest, int bx, int by, BITMAP *mask, int type) {
  int d = dist(a, b, ax, ay, bx, by, mask, dbest);
  if ((d < dbest) && (ax != bx || ay != by) ) {
#ifdef DEBUG
      if (type == 0)
        printf("  Prop x: improve (%d, %d) old nn (%d, %d) new nn (%d, %d) old dist %d, new dist %d\n", ax, ay, xbest, ybest, bx, by, dbest, d);
      else if (type == 1)
        printf("  Prop y: improve (%d, %d) old nn (%d, %d) new nn (%d, %d) old dist %d, new dist %d\n", ax, ay, xbest, ybest, bx, by, dbest, d);
      else
        printf("  Random: improve (%d, %d) old nn (%d, %d) new nn (%d, %d) old dist %d, new dist %d\n", ax, ay, xbest, ybest, bx, by, dbest, d);
#endif
    dbest = d;
    xbest = bx;
    ybest = by;
  }
}

/* Get the bounding box of hole */
void getBox(BITMAP *mask, int& xmin, int& xmax, int& ymin, int& ymax) {
  for (int h = 0; h < mask->h; h++) {
    for (int w = 0; w < mask->w; w++) {
      int c = (*mask)[h][w];
      int r = c&255;
      int g = (c>>8)&255;
      int b = (c>>16)&255;
      // hole means non-black pixels in mask
      if (r != 0 || g != 0 || b != 0) {
          if (h < ymin)
            ymin = h;
          if (h > ymax)
            ymax = h;
          if (w < xmin)
            xmin = w;
          if (w > xmax)
            xmax = w;
      }
    }
  }
  xmin = xmin - patch_w + 1;
  ymin = ymin - patch_w + 1;
  xmin = (xmin < 0) ? 0 : xmin;
  ymin = (ymin < 0) ? 0 : ymin;
  
  xmax = (xmax > mask->w - patch_w + 1) ? mask->w - patch_w +1 : xmax;
  ymax = (ymax > mask->h - patch_w + 1) ? mask->h - patch_w +1 : ymax;

  printf("Hole's bounding box is x (%d, %d), y (%d, %d)\n", xmin, xmax, ymin, ymax);
}

BITMAP *norm_image(double *accum, int w, int h, BITMAP *ainit=NULL) {
  BITMAP *ans = new BITMAP(w, h);
  for (int y = 0; y < h; y++) {
    int *row = (*ans)[y];
    int *arow = NULL;
    if (ainit)
      arow = (*ainit)[y];
    double *prow = &accum[4*(y*w)];
    for (int x = 0; x < w; x++) {
      double *p = &prow[4*x];
      int c = p[3] ? p[3]: 1;
      int c2 = c>>1;             /* Changed: round() instead of floor. */
      if (ainit)
        row[x] = p[3] ? int((p[0]+c2)/c)|(int((p[1]+c2)/c)<<8)|(int((p[2]+c2)/c)<<16)|(255<<24) : arow[x];
      else
        row[x] = int((p[0]+c2)/c)|(int((p[1]+c2)/c)<<8)|(int((p[2]+c2)/c)<<16)|(255<<24);
    }
  }
  return ans;
}

/* Match image a to image b, returning the nearest neighbor field mapping a => b coords, stored in an RGB 24-bit image as (by<<12)|bx. */
void patchmatch(BITMAP *a, BITMAP *mask, BITMAP *&ans, BITMAP *&ann, BITMAP *&annd) {
  /* Initialize with random nearest neighbor field (NNF). */
  ann = new BITMAP(a->w, a->h);
  annd = new BITMAP(a->w, a->h);
  //int aew = a->w - patch_w+1, aeh = a->h - patch_w + 1;       /* Effective width and height (possible upper left corners of patches). */
  int mew = mask->w - patch_w+1, meh = mask->h - patch_w + 1;
  memset(ann->data, 0, sizeof(int)*a->w*a->h);
  memset(annd->data, 0, sizeof(int)*a->w*a->h);


  int box_xmin, box_xmax, box_ymin, box_ymax;
  box_xmin = box_ymin = INT_MAX;
  box_xmax = box_ymax = 0;

  getBox(mask, box_xmin, box_xmax, box_ymin, box_ymax);

  int bx, by;
  // Initialization
  for (int ay = box_ymin; ay < box_ymax; ay++) {
    for (int ax = box_xmin; ax < box_xmax; ax++) {
      bool valid = false;
      while (!valid) {
        bx = rand()%mew;
        by = rand()%meh;
        // should find patches outside bounding box
        if (inBox(bx, by, box_xmin, box_xmax, box_ymin, box_ymax)) {
        // or outside the hole
        //if (isHole(mask, bx, by) && isHole(mask, bx+patch_w, by+patch_w)) {
          valid = false;
        } else {
          valid = true;
        }
      }
      (*ann)[ay][ax] = XY_TO_INT(bx, by);
      (*annd)[ay][ax] = dist(a, a, ax, ay, bx, by, mask);
    }
  }



  save_bitmap(ann, "ann_before.jpg");
  save_bitmap(annd, "annd_before.jpg");
  save_bitmap(mask, "mask_before.jpg");

  // in each iter we have two mask, the old one and updated new one
  int w = 1;
  for (int iter = 0; iter < pm_iters; iter++) {
    printf("iter = %d\n", iter);
    /* In each iteration, improve the NNF, by looping in scanline or reverse-scanline order. */
    int ystart = box_ymin, yend = box_ymax, ychange = 1;
    int xstart = box_xmin, xend = box_xmax, xchange = 1;
    if (iter % 2 == 1) {
      xstart = box_xmax-1; xend = box_xmin-1; xchange = -1;
      ystart = box_ymax-1; yend = box_ymin-1; ychange = -1;
    }
    for (int ay = ystart; ay != yend; ay += ychange) {
      for (int ax = xstart; ax != xend; ax += xchange) { 
        /* Current (best) guess. */
        int v = (*ann)[ay][ax];
        int xbest = INT_TO_X(v), ybest = INT_TO_Y(v);
        int dbest = (*annd)[ay][ax];

        /* Propagation: Improve current guess by trying instead correspondences from left and above (below and right on odd iterations). */
        if ((unsigned) (ax - xchange) < (unsigned) mew) {
        //if (inBox(ax - xchange, ay, box_xmin, box_xmax, box_ymin, box_ymax)) {
          int vp = (*ann)[ay][ax-xchange];
          int xp = INT_TO_X(vp) + xchange, yp = INT_TO_Y(vp);
          if (((unsigned) xp < (unsigned) mew) && !inBox(xp, yp, box_xmin, box_xmax, box_ymin, box_ymax)) {
          //if (((unsigned) xp < (unsigned) mew)) {
            //printf("Propagation x\n");
            improve_guess(a, a, ax, ay, xbest, ybest, dbest, xp, yp, mask, 0);
          }
        }

        if ((unsigned) (ay - ychange) < (unsigned) meh) {
        //if (inBox(ax, ay - ychange, box_xmin, box_xmax, box_ymin, box_ymax)) {
          int vp = (*ann)[ay-ychange][ax];
          int xp = INT_TO_X(vp), yp = INT_TO_Y(vp) + ychange;
          if (((unsigned) yp < (unsigned) meh) && !inBox(xp, yp, box_xmin, box_xmax, box_ymin, box_ymax)) {
          //if (((unsigned) yp < (unsigned) meh)) {
            //printf("Propagation y\n");
            improve_guess(a, a, ax, ay, xbest, ybest, dbest, xp, yp, mask, 1);
          }
        }

        /* Random search: Improve current guess by searching in boxes of exponentially decreasing size around the current best guess. */
        int rs_start = rs_max;
        if (rs_start > MAX(a->w, a->h)) { rs_start = MAX(a->w, a->h); }
        for (int mag = rs_start; mag >= 1; mag /= 2) {
          /* Sampling window */
          int xmin = MAX(xbest-mag, 0), xmax = MIN(xbest+mag+1, mew);
          int ymin = MAX(ybest-mag, 0), ymax = MIN(ybest+mag+1, meh);
          int xp = xmin+rand()%(xmax-xmin);
          int yp = ymin+rand()%(ymax-ymin);
          if (!inBox(xp, yp, box_xmin, box_xmax, box_ymin, box_ymax)) {
            //printf("Random\n");
            improve_guess(a, a, ax, ay, xbest, ybest, dbest, xp, yp, mask, 2);
          }
        }

        (*ann)[ay][ax] = XY_TO_INT(xbest, ybest);
        (*annd)[ay][ax] = dbest;
      }
    }

    std::stringstream ss;
    ss << iter;

    std::string annd_file = "annd_iter_" + ss.str() + ".jpg";
    const char* annd_ptr = annd_file.c_str();
    save_bitmap(annd, annd_ptr);
    
  } 

  // to store pixels in the new patch
  int sz = a->w*a->h; sz = sz << 2; // 4*w*h
  double* accum = new double[sz];
  memset(accum, 0, sizeof(double)*sz );

  // fill in missing pixels
  for (int ay = box_ymin; ay < box_ymax; ay++) {
    for (int ax = box_xmin; ax < box_xmax; ax++) {
      int vp = (*ann)[ay][ax];
      int xp = INT_TO_X(vp), yp = INT_TO_Y(vp);
      for (int dy = 0; dy < patch_w; dy++) {
        int* arow = (*a)[yp+dy] + xp;
        double* prow = &accum[4*((ay+dy)*a->w + ax)];
        for(int dx = 0; dx < patch_w; dx++) {
          if ((*annd)[yp+dy][xp+dx] == INT_MAX) { continue; }
          int c = arow[dx];
          double* p = &prow[4*dx];
          p[0] += (c&255)*w;
          p[1] += ((c>>8)&255)*w;
          p[2] += ((c>>16)&255)*w;
          p[3] += w;
        }
      }
    }
  } 
  ans = norm_image(accum, a->w, a->h, NULL);
  delete[] accum;

  // join with original picture
  for (int h = 0; h < a->h; h++) {
    for (int w = 0; w < a->w; w++) {
      if (!isHole(mask, w, h)) {
        (*ans)[h][w] = 0 | (*a)[h][w];
      }
    }
  }
  
  save_bitmap(ans, "final_picture.jpg");

  save_bitmap(ann, "ann_final.jpg");
  save_bitmap(annd, "annd_final.jpg");
  
  
}

void tryIt(BITMAP* a, BITMAP *&ans, BITMAP *mask) {
    // fill in missing pixels
    //int ymin = 0, ymax = a->h - patch_w + 1;
    //int xmin = 0, xmax = a->w - patch_w + 1;
    int ymin = 200, ymax = 400;
    int xmin = 200, xmax = 400;
    // to store new pixels
    int sz = a->w*a->h; sz = sz << 2; // 4*w*h
    double* accum = new double[sz];
    memset(accum, 0, sizeof(double)*sz );

    int w = 1;
    for (int ay = ymin; ay < ymax; ay++) {
      for (int ax = xmin; ax < xmax; ax++) {
        int xp = ax - 200, yp = ay - 200;
        for (int dy = 0; dy < patch_w; dy++) {
          int* arow = (*a)[yp+dy] + xp;
          double* prow = &accum[4*((ay+dy)*a->w + ax)];
          for(int dx = 0; dx < patch_w; dx++) {
            // if (!isHole(mask, ax+dx, ay+dy)) { continue; }
            int c = arow[dx];
            double* p = &prow[4*dx];
            p[0] += (c&255)*w;
            p[1] += ((c>>8)&255)*w;
            p[2] += ((c>>16)&255)*w;
            p[3] += w;
            // change mask
            // (*mask)[ay+dy][ax+dx] = 0;
          }
        }
      }
    } 
    ans = norm_image(accum, a->w, a->h, a);
    save_bitmap(ans, "try_ans.jpg");

    // join with original picture
    for (int h = 0; h < a->h; h++) {
      for (int w = 0; w < a->w; w++) {
        if (!isHole(mask, w, h)) {
        //if (!inBox(w, h, box_xmin, box_xmax, box_ymin, box_ymax)) {
          (*ans)[h][w] = 0 | (*a)[h][w];
        }
      }
    }
    save_bitmap(ans, "try_ans_join.jpg");
}

void reconstruct(BITMAP *a, BITMAP *b, BITMAP *ann, BITMAP *&r) {
  r = new BITMAP(a->w, a->h);
  memset(r->data, 0, sizeof(int)*a->w*a->h);

  for (int h = 0; h < a->h; h++)
  {
  	for (int w = 0; w < a->w; w++)
  	{
	  int v = (*ann)[h][w];
      int xbest = INT_TO_X(v), ybest = INT_TO_Y(v);
      (*r)[h][w] = (*b)[ybest][xbest];
  	}
  }
}


int main(int argc, char *argv[]) {
  argc--;
  argv++;
  if (argc != 3) { fprintf(stderr, "im_complete a mask result\n"
                                   "Given input image a and mask outputs result\n"
                                   "These are stored as RGB 24-bit images, with a 24-bit int at every pixel. For the NNF we store (by<<12)|bx."); exit(1); }
  printf("(1) Loading input images\n");
  BITMAP *a = load_bitmap(argv[0]);
  BITMAP *mask = load_bitmap(argv[1]);
  BITMAP *ans = NULL, *ann = NULL, *annd = NULL;

  //BITMAP *ans2 = NULL;
  //tryIt(a, ans2, mask);

  printf("\n(2) Running PatchMatch\n");
  patchmatch(a, mask, ans, ann, annd);
  printf("\n(3) Saving output images: ans\n");
  save_bitmap(ans, argv[2]);

  return 0;
}
