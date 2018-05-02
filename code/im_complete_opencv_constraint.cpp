
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
#include <opencv2/opencv.hpp>

#include <iostream>
#include <vector>
#include <unordered_map>

#ifndef MAX
#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))
#endif

#define _DEBUG

using namespace cv;
using namespace std;

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


BITMAP *load_bitmap(const char *filename) {
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

// Just a simple struct for Box
struct Box {
  int xmin, xmax, ymin, ymax;
};

// a struct for constraint map
struct CMap {
  unordered_map<int, vector<pair<int, int> > > constraint_map;
  vector<int> constraint_ids;
};

void getCMap(Mat constraint, CMap* cmap) {
  unordered_map<int, vector<pair<int, int> > >::iterator got;
  for (int y = 0; y < constraint.rows; ++y) {
    for (int x = 0; x < constraint.cols; ++x) {
      int cons_pixel = (int) constraint.at<uchar>(y, x);
      if (cons_pixel == 0)
          continue;
      got = cmap->constraint_map.find(cons_pixel);
      if (got == cmap->constraint_map.end()) {
        vector<pair<int, int> > constraint_vector;
        constraint_vector.push_back(make_pair(x, y));
        cmap->constraint_map[cons_pixel] = constraint_vector;
        cmap->constraint_ids.push_back(cons_pixel);
      } else {
        got->second.push_back(make_pair(x, y));
      }
    }
  }

  cout << "Rows: " << constraint.rows << ", Cols: " << constraint.cols << endl;
  cout << constraint.rows * constraint.cols << endl;
  cout << "Map has size of " << cmap->constraint_map.size() << endl;
  for (int i = 0; i < cmap->constraint_ids.size(); ++i) {
    int id = cmap->constraint_ids[i];
    cout << "  Map id " << id << " has " << cmap->constraint_map.find(id)->second.size() << " elements " <<endl;
  }

}

/* -------------------------------------------------------------------------
   PatchMatch, using L2 distance between upright patches that translate only
   ------------------------------------------------------------------------- */

int patch_w  = 8;
int pm_iters = 5;
int rs_max   = INT_MAX; // random search
int sigma = 1 * patch_w * patch_w;

#define XY_TO_INT(x, y) (((y)<<12)|(x))
#define INT_TO_X(v) ((v)&((1<<12)-1))
#define INT_TO_Y(v) ((v)>>12)

/* Get the bounding box of hole */
Box getBox(Mat mask) {
  int xmin = INT_MAX, ymin = INT_MAX;
  int xmax = 0, ymax = 0;
  for (int h = 0; h < mask.rows; h++) {
    for (int w = 0; w < mask.cols; w++) {
      //Vec3b mask_pixel = mask.at<Vec3b>(h, w);
      int mask_pixel = (int) mask.at<uchar>(h, w);
      // hole means non-black pixels in mask
      // if (!(mask_pixel[0] == 0 && mask_pixel[1] == 0 && mask_pixel[2] == 0)) {
      if (mask_pixel == 255) {
          if (h < ymin)
            ymin = h;
          if (h > ymax)
            ymax = h;
          if (w < xmin)
            xmin = w;
          if (w > xmax)
            xmax = w;
      } else if (mask_pixel != 0) {
          cout << "SHIT happens, value " << mask_pixel << " in pos x " << w << " , y" << h << endl;
      }
    }
  }
  xmin = xmin - patch_w + 1;
  ymin = ymin - patch_w + 1;
  xmin = (xmin < 0) ? 0 : xmin;
  ymin = (ymin < 0) ? 0 : ymin;

  xmax = (xmax > mask.cols - patch_w + 1) ? mask.cols - patch_w +1 : xmax;
  ymax = (ymax > mask.rows - patch_w + 1) ? mask.rows - patch_w +1 : ymax;

  printf("Hole's bounding box is x (%d, %d), y (%d, %d)\n", xmin, xmax, ymin, ymax);
  Box box = {.xmin = xmin, .xmax = xmax, .ymin = ymin, .ymax = ymax};
  return box;
}

/* check if a pixel x, y is in the bounding box or not */
bool inBox(int x, int y, Box box) {
  if (x >= box.xmin && x <= box.xmax && y >= box.ymin && y <= box.ymax) {
    return true;
  }
  return false;
}

/* Measure distance between 2 patches with upper left corners (ax, ay) and (bx, by), terminating early if we exceed a cutoff distance.
   You could implement your own descriptor here. */
int dist(Mat a, Mat b, int ax, int ay, int bx, int by, int cutoff=INT_MAX) {
  int ans = 0;
  if (a.type() != CV_8UC3) {
    cout << "Bad things happened in dist " <<endl;
    exit(1);
  }
  for (int dy = 0; dy < patch_w; dy++) {
    for (int dx = 0; dx < patch_w; dx++) {
      Vec3b ac = a.at<Vec3b>(ay + dy, ax + dx);
      Vec3b bc = b.at<Vec3b>(by + dy, bx + dx);

      int db = ac[0] - bc[0];
      int dg = ac[1] - bc[1];
      int dr = ac[2] - bc[2];
      ans += dr*dr + dg*dg + db*db;
    }
    if (ans >= cutoff) { return cutoff; }
  }
  if (ans < 0) return INT_MAX;
  return ans;
}

void improve_guess(Mat a, Mat b, int ax, int ay, int &xbest, int &ybest, int &dbest, int bx, int by, int type) {
  int d = dist(a, b, ax, ay, bx, by, dbest);
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

/* Match image a to image b, returning the nearest neighbor field mapping a => b coords, stored in an RGB 24-bit image as (by<<12)|bx. */
void patchmatch(Mat a, Mat b, BITMAP *&ann, BITMAP *&annd, Mat dilated_mask, Mat constraint, CMap* cmap) {
  /* Initialize with random nearest neighbor field (NNF). */
  ann = new BITMAP(a.cols, a.rows);
  annd = new BITMAP(a.cols, a.rows);
   /* Effective width and height (possible upper left corners of patches). */
  int aew = a.cols - patch_w + 1, aeh = a.rows - patch_w + 1;
  int bew = b.cols - patch_w + 1, beh = b.rows - patch_w + 1;
  memset(ann->data, 0, sizeof(int) * a.cols * a.rows);
  memset(annd->data, 0, sizeof(int) * a.cols * a.rows);

  // process constraint
  //CMap *cmap_ptr, cmap;
  //cmap_ptr = &cmap;
  //getCMap(constraint, cmap_ptr);

  // Initialization
  int bx, by;
  unordered_map<int, vector<pair<int, int> > >::iterator got;
  for (int ay = 0; ay < aeh; ay++) {
    for (int ax = 0; ax < aew; ax++) {
      bool valid = false;
      int const_pixel = (int) constraint.at<uchar>(ay, ax);

      // if not having constraint
      if (const_pixel == 0) {
        while (!valid) {
          bx = rand() % bew;
          by = rand() % beh;
          int mask_pixel = (int) dilated_mask.at<uchar>(by, bx);
          // should find patches outside the hole
          if (mask_pixel == 255) {
            valid = false;
          } else {
            valid = true;
          }
        }
      } else {
        got = cmap->constraint_map.find(const_pixel);
        if (got == cmap->constraint_map.end()) {
          cout << "Something wrong in constraint map " << endl;
          exit(1);
        }
        //int debug_shit = 0;
        while (!valid) {
          //debug_shit++;
          //cout << "debug index " << debug_shit <<endl;
          //cout << "got->second.size() " << got->second.size() <<endl;
          int rand_index = rand() % got->second.size();
          //cout << "rand index " << rand_index <<endl;
          bx = got->second[rand_index].first;
          by = got->second[rand_index].second;
          int mask_pixel = (int) dilated_mask.at<uchar>(by, bx);
          if (bx >= bew || by >= beh) {
              valid = false;
          } else if (mask_pixel == 255) {
            valid = false;
          } else {
            valid = true;
          }
        }
      }
      (*ann)[ay][ax] = XY_TO_INT(bx, by);
      (*annd)[ay][ax] = dist(a, b, ax, ay, bx, by);
    }
  }

#ifdef DEBUG
  for (int ay = 0; ay < aeh; ay++ ) {
    for (int ax = 0; ax < aew; ax++) {
      int vp = (*ann)[ay][ax];
      int xp = INT_TO_X(vp);
      int yp = INT_TO_Y(vp);
      int mask_pixel = (int) dilated_mask.at<uchar>(yp, xp);
      if (mask_pixel == 255) {
         cout << "Something wrong after init  " << xp << " ,  " << yp << " pixel " << mask_pixel << endl;
      }
    }
  }
#endif

  for (int iter = 0; iter < pm_iters; iter++) {
    // printf("  pm_iter = %d\n", iter);
    /* In each iteration, improve the NNF, by looping in scanline or reverse-scanline order. */
    int ystart = 0, yend = aeh, ychange = 1;
    int xstart = 0, xend = aew, xchange = 1;
    if (iter % 2 == 1) {
      xstart = xend-1; xend = -1; xchange = -1;
      ystart = yend-1; yend = -1; ychange = -1;
    }
    for (int ay = ystart; ay != yend; ay += ychange) {
      for (int ax = xstart; ax != xend; ax += xchange) {

        int const_pixel = (int) constraint.at<uchar>(ay, ax);

        /* Current (best) guess. */
        int v = (*ann)[ay][ax];
        int xbest = INT_TO_X(v), ybest = INT_TO_Y(v);
        int dbest = (*annd)[ay][ax];

        /* Propagation: Improve current guess by trying instead correspondences from left and above (below and right on odd iterations). */
        if ((unsigned) (ax - xchange) < (unsigned) aew) {
          int vp = (*ann)[ay][ax-xchange];
          int xp = INT_TO_X(vp) + xchange, yp = INT_TO_Y(vp);

          if (((unsigned) xp < (unsigned) aew)) {
            int mask_pixel = (int) dilated_mask.at<uchar>(yp, xp);
            if (mask_pixel != 255) {
              int new_const_pixel = (int) constraint.at<uchar>(yp, xp);
              //if (const_pixel == 0) {
                improve_guess(a, b, ax, ay, xbest, ybest, dbest, xp, yp, 0);
              //} else if (const_pixel == new_const_pixel) {
              //  improve_guess(a, b, ax, ay, xbest, ybest, dbest, xp, yp, 0);
              //}
            }
          }
        }

        if ((unsigned) (ay - ychange) < (unsigned) aeh) {
          int vp = (*ann)[ay-ychange][ax];
          int xp = INT_TO_X(vp), yp = INT_TO_Y(vp) + ychange;

          if (((unsigned) yp < (unsigned) aeh)) {
            int mask_pixel = (int) dilated_mask.at<uchar>(yp, xp);
            if (mask_pixel != 255) {
              int new_const_pixel = (int) constraint.at<uchar>(yp, xp);
              //if (const_pixel == 0) {
                improve_guess(a, b, ax, ay, xbest, ybest, dbest, xp, yp, 1);
              //} else if (const_pixel == new_const_pixel) {
              //  improve_guess(a, b, ax, ay, xbest, ybest, dbest, xp, yp, 1);
              //}
            }
          }
        }

        /* Random search: Improve current guess by searching in boxes of exponentially decreasing size around the current best guess. */
        //if (const_pixel == 0) {
          int rs_start = rs_max;
          if (rs_start > MAX(b.cols, b.rows)) { rs_start = MAX(b.cols, b.rows); }
          for (int mag = rs_start; mag >= 1; mag /= 2) {
            /* Sampling window */
            int xmin = MAX(xbest-mag, 0), xmax = MIN(xbest+mag+1, bew);
            int ymin = MAX(ybest-mag, 0), ymax = MIN(ybest+mag+1, beh);
            bool do_improve = false;
            do {
              int xp = xmin + rand() % (xmax-xmin);
              int yp = ymin + rand() % (ymax-ymin);
              int mask_pixel = (int) dilated_mask.at<uchar>(yp, xp);
              if (mask_pixel != 255) {
                improve_guess(a, b, ax, ay, xbest, ybest, dbest, xp, yp, 2);
                do_improve = true;
              }
            } while (!do_improve);
          }
        /*} else {
          got = cmap->constraint_map.find(const_pixel);
          // we choose the improve times to be sqrt of the size
          int improve_times = (int) ceil(sqrt(got->second.size()));
          for (int i_t = 0; i_t < improve_times; ++i_t) {
            bool do_improve = false;
            do {
              int rand_index = rand() % got->second.size();
              int xp = got->second[rand_index].first;
              int yp = got->second[rand_index].second;
              int mask_pixel = (int) dilated_mask.at<uchar>(yp, xp);
              if (xp >= bew || yp >= beh) {
                do_improve = false;
              } else if (mask_pixel != 255) {
                improve_guess(a, b, ax, ay, xbest, ybest, dbest, xp, yp, 2);
                do_improve = true;
              }
            } while (!do_improve);
          }*/
        //}

        (*ann)[ay][ax] = XY_TO_INT(xbest, ybest);
        (*annd)[ay][ax] = dbest;
      }
    }
  }
}


/**
 * Image inpainting algorithm
 * Basic idea based on Wexler et. al 2017 Space-Time Image Completion
 *
 * @param im_orig: original image (with pixels in hole presented or not)
 * @param mask:    mask specify missing region
 * @param constraint: constraint image generate by user
 *
 * @return the completed/inpainting image
 */
void image_complete(Mat im_orig, Mat mask, Mat constraint) {

  // some parameters for scaling
  int rows = im_orig.rows;
  int cols = im_orig.cols;
  //int startscale = (int) -1*ceil(log2(MIN(rows, cols))) + 5;
  int startscale = -3;
  double scale = pow(2, startscale);

  cout << "Scaling image by " << scale << endl;

  double t1 = (double)getTickCount();

  // Resize image to starting scale
  Mat resize_img, resize_mask, resize_constraint;
  resize(im_orig, resize_img, Size(), scale, scale, INTER_AREA);
  resize(mask, resize_mask, Size(), scale, scale, INTER_AREA);
  threshold(resize_mask, resize_mask, 127, 255, 0);
  resize(constraint, resize_constraint, Size(), scale, scale, INTER_NEAREST);

  // Random starting guess for inpainted image
  rows = resize_img.rows;
  cols = resize_img.cols;
  for (int y = 0; y < rows; ++y) {
    for (int x = 0; x < cols; ++x) {
      int mask_pixel = (int) resize_mask.at<uchar>(y, x);
      if (mask_pixel != 0 && mask_pixel != 255) {
        cout << "GGGGGGGGGGGGGGG" << endl;
        exit(1);
      }
      // if not black pixel, then means white (1) pixel in mask
      // means hole, thus random init colors in hole
      if (mask_pixel != 0) {
        resize_img.at<Vec3b>(y, x)[0] = rand() % 256;
        resize_img.at<Vec3b>(y, x)[1] = rand() % 256;
        resize_img.at<Vec3b>(y, x)[2] = rand() % 256;
      }
    }
  }

  double p1 = ((double)getTickCount() - t1) / getTickFrequency();
  cout << "time for init = " << p1 << endl;

  // just for DEBUG
  int index = 0;

  // go through all scale
  for (int logscale = startscale; logscale <= 0; logscale++) {
    index++;

    scale = pow(2, logscale);

    cout << "Scaling is " << scale << endl;

    Box mask_box = getBox(resize_mask);
    // dilate the mask
    //
    // if patch_w = 3
    // kernel width = 5 , 0 1 2 is 1
    // pixel is    result should be
    // 0 0 0 0     1 1 1 0
    // 0 0 0 0     1 1 1 0
    // 0 0 1 0     1 1 1 0
    // 0 0 0 0     0 0 0 0
    Mat element = Mat::zeros(2*patch_w - 1, 2*patch_w - 1, CV_8UC1);
    element(Rect(patch_w - 1, patch_w - 1, patch_w, patch_w)) = 255;
    Mat dilated_mask;
    dilate(resize_mask, dilated_mask, element);

    /*
    imwrite("dilated_mask.png", dilated_mask);
    imwrite("mask.png", mask);
    Mat inverted_mask;
    bitwise_not(mask, inverted_mask);
    Mat mask_diff;
    bitwise_and(dilated_mask, inverted_mask, mask_diff);
    imwrite("mask_diff.png", mask_diff);
    */

    CMap cmap;
    CMap* cmap_ptr = &cmap;
    getCMap(resize_constraint, cmap_ptr);

    /*
    for (int y = 0; y < resize_mask.rows; ++y) {
      for (int x = 0; x < resize_mask.cols; ++x) {
        int const_pixel = (int) resize_constraint.at<uchar>(y, x);
        Vec3b& img_pixel = resize_img.at<Vec3b>(y, x);
        if (const_pixel != 0) {
          img_pixel[0] = 255;
          img_pixel[1] = 0;
          img_pixel[2] = 0;
        }
      }
    }


    unordered_map<int, vector<pair<int, int> > >::iterator it;
    for (it = cmap_ptr->constraint_map.begin(); it != cmap_ptr->constraint_map.end(); ++it) {
      for (int i = 0; i < it->second.size(); ++i) {
        int nx = it->second[i].first;
        int ny = it->second[i].second;
        Vec3b& img_pixel = resize_img.at<Vec3b>(ny, nx);
        img_pixel[0] = 255;
        img_pixel[1] = 0;
        img_pixel[2] = 0;

      }
    }

    stringstream ss;
    ss << index;
    string debug_file = "debug_"  + ss.str() + ".png";
    imwrite(debug_file, resize_img);
     */

    // iterations of image completion
    int im_iterations = 60;
    for (int im_iter = 0; im_iter < im_iterations; ++im_iter) {
      printf("im_iter = %d\n", im_iter);

      BITMAP *ann = NULL, *annd = NULL;

      double t2 = (double)getTickCount();

      Mat B = resize_img.clone();
      bitwise_and(resize_img, 0, B, resize_mask);

      // use patchmatch to find NN
      patchmatch(resize_img, B, ann, annd, dilated_mask, resize_constraint, cmap_ptr);

      //stringstream ss;
      //ss << im_iter;
      //string annd_file = "annd_iter_"  + ss.str() + ".jpg";
      //const char* annd_ptr = annd_file.c_str();
      //save_bitmap(ann, annd_ptr);

      double p2 = ((double)getTickCount() - t2) / getTickFrequency();
      cout << "time for PM = " << p2 << endl;

      double t3 =  (double)getTickCount();
      // create new image by letting each patch vote
      Mat R = Mat::zeros(resize_img.rows, resize_img.cols, CV_32FC3);
      Mat Rcount = Mat::zeros(resize_img.rows, resize_img.cols, CV_32FC3);
      for (int y = mask_box.ymin; y < mask_box.ymax; ++y) {
        for (int x = mask_box.xmin; x < mask_box.xmax; ++x) {
            int v = (*ann)[y][x];
            int xbest  = INT_TO_X(v), ybest = INT_TO_Y(v);
            Rect srcRect(Point(x, y), Size(patch_w, patch_w));
            Rect dstRect(Point(xbest, ybest), Size(patch_w, patch_w));
            float d = (float) (*annd)[y][x];
            float sim = exp(-d / (2*pow(sigma, 2) ));
            Mat toAssign;
            addWeighted(R(srcRect), 1.0, resize_img(dstRect), sim, 0, toAssign, CV_32FC3);
            toAssign.copyTo(R(srcRect));
            add(Rcount(srcRect), sim, toAssign, noArray(), CV_32FC3);
            toAssign.copyTo(Rcount(srcRect));
/*
            Mat debugR = Rcount.clone();
            cout << "Hole (" << x << ", " << y << ") has sim2 " << exp(-d / (2*pow(sigma, 2))) <<endl;
            cout << sum(Rcount - debugR) <<endl;
*/
        }
      }
      double p3 = ((double)getTickCount() - t3) / getTickFrequency();
      cout << "time for voting = " << p3 << endl;

      // normalize new image
      // COULD BE optimize TODO
      for (int h = 0; h < R.rows; h++) {
        for (int w = 0; w < R.cols; w++) {
          Vec3f rcount_pixel = Rcount.at<Vec3f>(h, w);
          if (rcount_pixel[0] > 0) {
            Vec3f& r_pixel = R.at<Vec3f>(h, w);
            r_pixel[0] = (r_pixel[0] / rcount_pixel[0]);
            r_pixel[1] = (r_pixel[1] / rcount_pixel[1]);
            r_pixel[2] = (r_pixel[2] / rcount_pixel[2]);
          }
        }
      }

      R.convertTo(R, CV_8UC3);

      // keep pixel outside mask
      Mat old_img = resize_img.clone();
      R.copyTo(resize_img, resize_mask);

      // measure how much image has changed, if not much then stop  TODO
      if (im_iter > 0) {
        double diff = 0;
        int mask_count_white = 0;
        int mask_count_black = 0;
        int mask_count_other = 0;
        for (int h = 0; h < resize_img.rows; h++) {
          for (int w = 0; w < resize_img.cols; w++) {
            int mask_pixel = (int) resize_mask.at<uchar>(h, w);
            // white pixel in mask is hole
            if (mask_pixel == 255) {
              Vec3b new_pixel = resize_img.at<Vec3b>(h, w);
              Vec3b old_pixel = old_img.at<Vec3b>(h, w);
              diff += pow(new_pixel[0] - old_pixel[0], 2);
              diff += pow(new_pixel[1] - old_pixel[1], 2);
              diff += pow(new_pixel[2] - old_pixel[2], 2);
              mask_count_white += 1;
            } else if (mask_pixel == 0) {
              mask_count_black += 1;
            } else {
              mask_count_other += 1;
            }
          }
        }
        assert(mask_count_other == 0);
#ifdef DEBUG
        cout << "diff is " << diff << endl;
        cout << "mask count is " << mask_count_white << endl;
        cout << "norm diff is " << diff/mask_count_white << endl;
#endif
        if (diff/mask_count_white < 0.02) {
          break;
        }
      }

      string outfile = "r_scale" + to_string(index) + "_imiter" + to_string(im_iter) + ".png";
      imwrite(outfile, R);

      delete ann;
      delete annd;
    }


    // Upsample A for the next scale
    if (logscale < 0) {
      double t4 = (double)getTickCount();
      cout << "Upscaling" << endl;
      // orig down scale to new scale
      Mat upscale_img;
      resize(im_orig, upscale_img, Size(), 2*scale, 2*scale, INTER_AREA);

      // data upscale to new scale
      int new_cols = upscale_img.cols, new_rows = upscale_img.rows;
      resize(resize_img, resize_img, Size(new_cols, new_rows), 0, 0, INTER_CUBIC);
      resize(mask, resize_mask, Size(new_cols, new_rows), 0, 0, INTER_AREA);
      resize(constraint, resize_constraint, Size(new_cols, new_rows), 0, 0, INTER_NEAREST);

      threshold(resize_mask, resize_mask, 127, 255, 0);

      Mat inverted_mask;
      bitwise_not(resize_mask, inverted_mask);
      upscale_img.copyTo(resize_img, inverted_mask);

      double p4 = ((double)getTickCount() - t4) / getTickFrequency();
      cout << "time for resize = " << p4 << endl;
    }
  }

  imwrite("final_out.png", resize_img);
}

int main(int argc, char *argv[]) {
  argc--;
  argv++;
  if (argc != 3 && argc != 4) { fprintf(stderr, "im_complete a mask result\n"
                                   "Given input image a and mask outputs result\n"
                                   "These are stored as RGB 24-bit images, with a 24-bit int at every pixel."); exit(1); }

  Mat image = imread(argv[0]);

  Mat a_matrix = image.clone();
  Mat mask_cv = imread(argv[1], CV_LOAD_IMAGE_GRAYSCALE);
  Mat const_cv = imread(argv[2], CV_LOAD_IMAGE_GRAYSCALE);

  printf("mask_cv type %d\n", mask_cv.type());
  for (int y = 0; y < mask_cv.rows; ++y) {
    for (int x = 0; x < mask_cv.cols; ++x) {
      int mask_pixel =(int) mask_cv.at<uchar>(y, x);
      if (mask_pixel != 0) {
        a_matrix.at<Vec3b>(y, x) = Vec3b(0, 0, 0);
      }
    }
  }

  image_complete(image, mask_cv, const_cv);

  return 0;
}
