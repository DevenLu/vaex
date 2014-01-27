#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/times.h>
#include <unistd.h>
#include "avs_io.h"
#include "kerneldensity.h"
#include <string.h>
#define PI 3.141592654
#define TINY 1.0e-20 
#include "CImg.h"
using namespace cimg_library;
  
#ifdef __cplusplus
extern "C"
#endif
double MTfor3D(char *fname,int *nLeaves);

//extern double quick_select(double arr[], int n);
//extern double torben(double arr[], int n);
extern void pixel_qsort(double *pix_arr, int *index,int npix);
extern double MaxTreeforSpaces(char *imname,int CONNECTIVITY,int *nLeaves);
extern int combi(int N,int M,int c[][50000]);
extern void pca(double **data, int n, int m);

typedef struct { double (*Func)( double sqr_dist);
                 double normalization,
                       range;
               } Kernel;

typedef char HeaderType[20];



void init_map2d ( map2d *map,
                  double x_min,
		  double x_max,
                  int   x_bins,
                  double y_min,
		  double y_max,
                  int   y_bins)
{ short i;
  map->x_min=x_min;
  map->x_max=x_max;
  map->x_bins=x_bins;
  map->y_min=y_min;
  map->y_max=y_max;
  map->y_bins=y_bins;
  map->data_max=0.0;
  map->data_min=0.0;

  map->map=(double **) calloc(y_bins,sizeof(double *));
  for (i=0;i<y_bins;i++)
    map->map[i]=(double *) calloc(x_bins,sizeof(double));
}


void init_map3d ( map3d *map,
                  double x_min,
		  double x_max,
                  int   x_bins,
                  double y_min,
		  double y_max,
                  int   y_bins,
                  double z_min,
		  double z_max,
                  int   z_bins)
{ short i,j;
  map->x_min=x_min;
  map->x_max=x_max;
  map->x_bins=x_bins;

  map->y_min=y_min;
  map->y_max=y_max;
  map->y_bins=y_bins;

  map->z_min=z_min;
  map->z_max=z_max;
  map->z_bins=z_bins;

  printf("x_min=%f,x_max=%f, \n",map->x_min,map->x_max);
  printf("y_min=%f,y_max=%f, \n",map->y_min,map->y_max);
  printf("z_min=%f,z_max=%f, \n",map->z_min,map->z_max);

  map->data_max=0.0;
  map->data_min=0.0;

  map->map=(double ***) calloc(z_bins,sizeof(double **));
  for (j=0;j<z_bins;j++){
    map->map[j]=(double **) calloc(z_bins,sizeof(double *));
    
    for (i=0;i<y_bins;i++)
      map->map[j][i]=(double *) calloc(x_bins,sizeof(double));
  }
}


void init_regress_map2d ( regress_map2d *map,
			  double x_min,
			  double x_max,
                          int   x_bins,
			  double y_min,
			  double y_max,
                          int   y_bins )
{ short i;
  map->x_min=x_min;
  map->x_max=x_max;
  map->x_bins=x_bins;
  map->y_min=y_min;
  map->y_max=y_max;
  map->y_bins=y_bins;
  map->data_max=0.0;
  map->data_min=0.0;

  map->dens_map=(double **) calloc(y_bins,sizeof(double *));
  map->z_map=(double **) calloc(y_bins,sizeof(double *));
  for (i=0;i<y_bins;i++)
    { map->dens_map[i]=(double *) calloc(x_bins,sizeof(double));
      map->z_map[i]=(double *) calloc(x_bins,sizeof(double));
    }
}

void init_map1d ( map1d *map,
                  double x_min,
		  double x_max,
                  int   bins)
{ short i;
  map->x_min=x_min;
  map->x_max=x_max;
  map->bins=bins;
  map->data_max=0.0;
  map->data_min=0.0;

  map->map=(double *) calloc(bins,sizeof(double));
  for (i=0;i<bins;i++)
    map->map[i]=0.0;
}


void init_regress_map1d ( regress_map1d *map,
			  double x_min,
			  double x_max,
                          int           bins)
{ short i;
  map->x_min=x_min;
  map->x_max=x_max;
  map->bins=bins;
  map->data_max=0.0;
  map->data_min=0.0;

  map->x_map=(double *) calloc(bins,sizeof(double));
  map->y_map=(double *) calloc(bins,sizeof(double));
  map->sig_map=(double *) calloc(bins,sizeof(double));
  for (i=0;i<bins;i++)
    { map->x_map[i]=0.0;
      map->y_map[i]=0.0;
      map->sig_map[i]=0.0;
    }
}

void exit_map2d ( map2d *map )
{ short i;
  for (i=0;i<map->y_bins;i++)
    free(map->map[i]);
  free(map->map);
}

void exit_map3d ( map3d *map )
{ short i,j;
  for (i=0;i<map->z_bins;i++){
    for (j=0;j<map->y_bins;j++){
      free(map->map[i][j]);
    }
    free(map->map[i]);
  }
  free(map->map);
}

void exit_regress_map2d ( regress_map2d *map )
{ short i;
  for (i=0;i<map->y_bins;i++)
    { free(map->dens_map[i]);
      free(map->z_map[i]);
    }
  free(map->dens_map);
  free(map->z_map);
}

void exit_map1d ( map1d *map )
{
  free(map->map);
}

void exit_regress_map1d ( regress_map1d *map )
{
  free(map->x_map);
  free(map->y_map);
  free(map->sig_map);
}


void clean_regress_map1d ( regress_map1d *d,
                           double min_dens   )
{ int i, valid=0;
  double x=d->x_min,
        xstep=(d->x_max-
                d->x_min)/(d->bins-1);
  for (i=0;i<d->bins;i++,x+=xstep)
    if (d->x_map[i]>min_dens)
      { d->y_map[valid]=(d->y_map[i])/(d->x_map[i]);
        d->sig_map[valid]=sqrt( (d->sig_map[i])/(d->x_map[i]) -
                                (d->y_map[valid])*(d->y_map[valid])  );
        d->x_map[valid]=x;
        if (d->data_max<d->y_map[valid])
          d->data_max=d->y_map[valid];
        if (d->data_min>d->y_map[valid])
          d->data_min=d->y_map[valid];
        valid++;
      }
  d->validbins=valid;
}

void clean_regress_map2d ( regress_map2d *d,
                           double min_dens   )

{ int i,j;
  for (j=0;j<d->y_bins;j++)
    for (i=0;i<d->x_bins;j++)
      if (d->dens_map[j][i]>min_dens)
        { d->z_map[j][i]/=(d->dens_map[j][i]);
          if (d->data_max<d->z_map[j][i])
            d->data_max=d->z_map[j][i];
          if (d->data_min>d->z_map[j][i])
            d->data_min=d->z_map[j][i];
        }
      else
        d->z_map[j][i]=0;
}


void add_one_point_epan ( double       x_val,
                          double       y_val,
                          map2d       *d,
                          double       hx,
                          double       hy    )
/* 
   Add one point using Epanechnikov kernel, without normalization 
   Used for fixed kernel density estimates, which can be post-normalized
   for added speed
*/
 
{ short x,y,x_start;
  double x0,
        cur_y,
        x_step1=(d->x_max- d->x_min)/(hx*(d->x_bins-1)),
        y_step=(d->y_max-
                d->y_min)/(hy*(d->y_bins-1)),
        x_step2=x_step1*x_step1,
        cx_xs,
        dist;

  cur_y=( (d->y_min)-y_val)/hy;
  if (cur_y<(-1))
    { y=(int)((double)(fabs(cur_y)-1)/y_step);
      cur_y+=y*y_step;
    }
  else
    y=0;
  x0=( (d->x_min)-x_val)/hx;
  if (x0<(-1))
    { x_start=(int)((double)(fabs(x0)-1)/x_step1);
      x0+=x_start*x_step1;
    }
  else
    x_start=0;
  x_step1*=x0;
  while (y<d->y_bins && cur_y<=1)
    {
      cx_xs=x_step1;
      x=x_start;
      dist=cur_y*cur_y+x0*x0;
      while (cx_xs<0 && dist>1)
        { dist+=(cx_xs+cx_xs+x_step2);
          cx_xs+=x_step2;
          x++;
        }
      while ( x<d->x_bins && dist<=1)
        { (d->map[y][x])+= (1-dist);   /* no normalization for h */
          dist+=(cx_xs+cx_xs+x_step2);
          cx_xs+=x_step2;
          x++;
        }
      cur_y+=y_step;
      y++;
    }
}

void add_one_point_epan_3d ( double       x_val,
			     double       y_val,
			     double       z_val,
			     map3d       *d,
			     double       hx,
			     double       hy,
			     double       hz )
/* 
   Add one point using Epanechnikov kernel, without normalization 
   Used for fixed kernel density estimates, which can be post-normalized
*/
{ short x,y,z,x_start,y_start;
  double 
    x0,y0, cur_y, cur_z,
    x_step1=(d->x_max - d->x_min)/(hx*(d->x_bins-1)),
    y_step=(d->y_max - d->y_min)/(hy*(d->y_bins-1)),
    z_step=(d->z_max - d->z_min)/(hz*(d->z_bins-1)),
    x_step2=x_step1*x_step1,
    cx_xs,
    dist;
  
  cur_z=( (d->z_min)-z_val)/hz;
  if (cur_z<(-1)){
    z=(int)((double)(fabs(cur_z)-1)/z_step);
    cur_z+=z*z_step;
  }
  else
    z=0;

  y0=( (d->y_min)-y_val)/hy;
  if (y0<(-1)){
    y_start=(int)((double)(fabs(y0)-1)/y_step);
    y0+=y_start*y_step;
  }
  else
    y_start=0;

  x0=( (d->x_min)-x_val)/hx;
  if (x0<(-1)){
    x_start=(int)((double)(fabs(x0)-1)/x_step1);
    x0+=x_start*x_step1;
  }
  else
    x_start=0;

  x_step1*=x0;

  while (z<d->z_bins && cur_z<=1){
    cur_y = y0;
    y=y_start;
    while (y<d->y_bins && cur_y<=1){
      cx_xs=x_step1;
      x=x_start;
      dist=cur_z*cur_z+cur_y*cur_y+x0*x0;
      while (cx_xs<0 && dist>1){
	dist+=(cx_xs+cx_xs+x_step2);
	cx_xs+=x_step2;
	x++;
      }
      while ( x<d->x_bins && dist<=1){
	(d->map[z][y][x])+= (1-dist);   /* no normalization for h */
	dist+=(cx_xs+cx_xs+x_step2);
	cx_xs+=x_step2;
	x++;
      }
      cur_y+=y_step;
      y++;
    }
    cur_z+=z_step;
    z++;
  }
}

void add_one_point_epan2 ( double       x_val,
                           double       y_val,
                           map2d       *d,
                           double       hx,
                           double       hy    )
{ short x,y,x_start;
  double x0,
        h2=hx*hy,
        cur_y,
        x_step1=(d->x_max-
                d->x_min)/(hx*(d->x_bins-1)),
        y_step=(d->y_max-
                d->y_min)/(hy*(d->y_bins-1)),
        x_step2=x_step1*x_step1,
        cx_xs,
        dist;
  cur_y=( (d->y_min)-y_val)/hy;
  if (cur_y<(-1))
    { y=(int)((double)(fabs(cur_y)-1)/y_step);
      cur_y+=y*y_step;
    }
  else
    y=0;
  x0=( (d->x_min)-x_val)/hx;
  if (x0<(-1))
    { x_start=(int)((double)(fabs(x0)-1)/x_step1);
      x0+=x_start*x_step1;
    }
  else
    x_start=0;
  x_step1*=x0;
  while (y<d->y_bins && cur_y<=1)
    {
      cx_xs=x_step1;
      x=x_start;
      dist=cur_y*cur_y+x0*x0;
      while (cx_xs<0 && dist>1)
        { dist+=(cx_xs+cx_xs+x_step2);
          cx_xs+=x_step2;
          x++;
        }
      while ( x<d->x_bins && dist<=1)
        { (d->map[y][x])+= ((1-dist)/h2); /* normalized for h */
          dist+=(cx_xs+cx_xs+x_step2);
          cx_xs+=x_step2;
          x++;
        }
      cur_y+=y_step;
      y++;
    }
}

void add_one_point_epan2_3d ( double       x_val,
			      double       y_val,
			      double       z_val,
			      map3d       *d,
			      double       hx,
			      double       hy,
			      double       hz )
/* 
   Add one point using Epanechnikov kernel, with partial normalization
   (hx*hy*hz)    
   Used for adaptive kernel density estimates, which can be post-normalized
   for number of data and kernel shape, but not for bandwidth.
*/
{ short x,y,z,x_start,y_start;
  double 
    h3=hx*hy*hz,
    x0,y0, cur_y, cur_z,
    x_step1=(d->x_max - d->x_min)/(hx*(d->x_bins-1)),
    y_step=(d->y_max - d->y_min)/(hy*(d->y_bins-1)),
    z_step=(d->z_max - d->z_min)/(hz*(d->z_bins-1)),
    x_step2=x_step1*x_step1,
    cx_xs,
    dist;
  
  cur_z=( (d->z_min)-z_val)/hz;
  if (cur_z<(-1)){
    z=(int)((double)(fabs(cur_z)-1)/z_step);
    cur_z+=z*z_step;
  }
  else
    z=0;

  y0=( (d->y_min)-y_val)/hy;
  if (y0<(-1)){
    y_start=(int)((double)(fabs(y0)-1)/y_step);
    y0+=y_start*y_step;
  }
  else
    y_start=0;

  x0=( (d->x_min)-x_val)/hx;
  if (x0<(-1)){
    x_start=(int)((double)(fabs(x0)-1)/x_step1);
    x0+=x_start*x_step1;
  }
  else
    x_start=0;

  x_step1*=x0;

  while (z<d->z_bins && cur_z<=1){
    cur_y = y0;
    y=y_start;
    while (y<d->y_bins && cur_y<=1){
      cx_xs=x_step1;
      x=x_start;
      dist=cur_z*cur_z+cur_y*cur_y+x0*x0;
      while (cx_xs<0 && dist>1){
	dist+=(cx_xs+cx_xs+x_step2);
	cx_xs+=x_step2;
	x++;
      }
      while ( x<d->x_bins && dist<=1){
	(d->map[z][y][x])+= (1-dist)/h3;   /* normalization for h */
	dist+=(cx_xs+cx_xs+x_step2);
	cx_xs+=x_step2;
	x++;
      }
      cur_y+=y_step;
      y++;
    }
    cur_z+=z_step;
    z++;
  }
}

void add_one_point_1d ( double       x_val,
                        map1d       *d,
                        double       h    )
{ short x,x_start;
  double x0,
        x_step1=(d->x_max-
                d->x_min)/(h*(d->bins-1)),
        x_step2=x_step1*x_step1,
        cx_xs,
        dist;
  x0=( (d->x_min)-x_val)/h;
  if (x0<(-1))
    { x_start=(int)((double)(fabs(x0)-1)/x_step1);
      x0+=x_start*x_step1;
    }
  else
    x_start=0;
  x_step1*=x0;
  cx_xs=x_step1;
  x=x_start;
  dist=x0*x0;
  while (cx_xs<0 && dist>1)
    { dist+=(cx_xs+cx_xs+x_step2);
      cx_xs+=x_step2;
      x++;
    }
  while ( x<d->bins && dist<=1)
    { (d->map[x])+= (1-dist);
      dist+=(cx_xs+cx_xs+x_step2);
      cx_xs+=x_step2;
      x++;
    }
}


void add_one_point_epan1d ( double       x_val,
                            map1d       *d,
                            double       h    )
{ short x,x_start;
  double x0,
        x_step1=(d->x_max-
                d->x_min)/(h*(d->bins-1)),
        x_step2=x_step1*x_step1,
        cx_xs,
        dist;
  x0=( (d->x_min)-x_val)/h;
  if (x0<(-1))
    { x_start=(int)((double)(fabs(x0)-1)/x_step1);
      x0+=x_start*x_step1;
    }
  else
    x_start=0;
  x_step1*=x0;
  cx_xs=x_step1;
  x=x_start;
  dist=x0*x0;
  while (cx_xs<0 && dist>1)
    { dist+=(cx_xs+cx_xs+x_step2);
      cx_xs+=x_step2;
      x++;
    }
  while ( x<d->bins && dist<=1)
    { (d->map[x])+= (1-dist)/h;
      dist+=(cx_xs+cx_xs+x_step2);
      cx_xs+=x_step2;
      x++;
    }
}


void add_one_point_regress_1d ( double         x_val,
                                double         y_val,
                                regress_map1d *d,
                                double         h      )
{ short x,x_start;
  double x0,
        x_step1=(d->x_max-
                 d->x_min)/(h*(d->bins-1)),
        x_step2=x_step1*x_step1,
        cx_xs,
        dist;
  x0=( (d->x_min)-x_val)/h;
  if (x0<(-1))
    { x_start=(int)((double)(fabs(x0)-1)/x_step1);
      x0+=x_start*x_step1;
    }
  else
    x_start=0;
  x_step1*=x0;
  cx_xs=x_step1;
  x=x_start;
  dist=x0*x0;
  while (cx_xs<0 && dist>1)
    { dist+=(cx_xs+cx_xs+x_step2);
      cx_xs+=x_step2;
      x++;
    }
  while ( x<d->bins && dist<=1)
    {
      (d->x_map[x])+= (1-dist)/h;
      (d->y_map[x])+= y_val*(1-dist)/h;
      (d->sig_map[x])+= y_val*y_val*(1-dist)/h;
      dist+=(cx_xs+cx_xs+x_step2);
      cx_xs+=x_step2;
      x++;
    }
}

void add_one_point_regress_2d ( double         x_val,
                                double         y_val,
                                double         z_val,
                                regress_map2d *d,
                                double         hx,
                                double         hy    )
{ short x,y,x_start;
  double x0,
        h2=hx*hy,
        cur_y,
        x_step1=(d->x_max-
                d->x_min)/(hx*(d->x_bins-1)),
        y_step=(d->y_max-
                d->y_min)/(hy*(d->y_bins-1)),
        x_step2=x_step1*x_step1,
        cx_xs,
        dist;
  cur_y=( (d->y_min)-y_val)/hy;
  if (cur_y<(-1))
    { y=(int)((double)(fabs(cur_y)-1)/y_step);
      cur_y+=y*y_step;
    }
  else
    y=0;
  x0=( (d->x_min)-x_val)/hx;
  if (x0<(-1))
    { x_start=(int)((double)(fabs(x0)-1)/x_step1);
      x0+=x_start*x_step1;
    }
  else
    x_start=0;
  x_step1*=x0;
  while (y<d->y_bins && cur_y<=1)
    {
      cx_xs=x_step1;
      x=x_start;
      dist=cur_y*cur_y+x0*x0;
      while (cx_xs<0 && dist>1)
        { dist+=(cx_xs+cx_xs+x_step2);
          cx_xs+=x_step2;
          x++;
        }
      while ( x<d->x_bins && dist<=1)
        { (d->dens_map[y][x])+= ((1-dist)/h2); /* normalized for h */
          (d->z_map[y][x])+= (z_val*(1-dist)/h2); /* normalized for h */
          dist+=(cx_xs+cx_xs+x_step2);
          cx_xs+=x_step2;
          x++;
        }
      cur_y+=y_step;
      y++;
    }
}


void init_contours ( contour_rec *contours,
                     int         num_contours,
                     double       min,
                     double       max,
                     double       log_radix,
                     int         min_col,
                     int         max_col       )
{ int i;
  contours->num_contours=num_contours;
  contours->levels=calloc(num_contours,sizeof(contours->levels[0]));
  contours->colours=calloc(num_contours,sizeof(contours->colours[0]));
  for (i=0;i<num_contours;i++)
    { contours->colours[i]=min_col+i*(max_col-min_col)/(num_contours-1);
      contours->levels[i]=min+(double)i*(max-min)/(double)(num_contours-1);
    }
  if ( log_radix!=0 )
    { contours->levels[num_contours-1]=max;
      for (i=num_contours-2; i>=0; i--)
        contours->levels[i]=contours->levels[i+1]/log_radix;
    }
}

void exit_contours ( contour_rec *contours )
{ free(contours->levels);
  free(contours->colours);
}



double prob_from_map1d(  map1d *map,
                         double x,
                         double xstep )       
{
  int xbin=(int)((x-map->x_min)/xstep);
  double xratio=(x-map->x_min-(xstep*xbin))/xstep;
  return map->map[xbin]+( map->map[xbin+1]
                                    -map->map[xbin]  )*xratio;
}




double comp_data_probs_1d ( map1d  *density,
                            double win_width,
                            int    num_data,
			    double *xdata,
			    double *prob)
{
  int i,k;
  double normalization = 1/( (double)num_data*win_width*4.0/3.0 ), 
         gmean = 0.0;
  double xstep = (density->x_max - density->x_min)/(double)(density->bins - 1);
  for (i=0; i<num_data; i++)
    add_one_point_1d(xdata[i],density,win_width);
  density->data_max  =density->map[0]*normalization;
  density->data_min = density->map[0]*normalization;
  for (k=0; k<density->bins; k++)
    {
      density->map[k] = density->map[k]*normalization;
     // printf("%lf\n",density->map[k] );
      if (density->map[k]>density->data_max) 
        density->data_max=density->map[k];
      else
        if (density->map[k]< density->data_min)
          density->data_min = density->map[k];
    }

  for (i=0; i<num_data; i++)
    {
      prob[i]=prob_from_map1d(density,xdata[i],xstep);

      gmean += log(prob[i]);
    }

  for (k=0; k<density->bins; k++)
    density->map[k]=0;

  return exp(gmean/num_data);
}


void adaptive_dens_1d(map1d *density, 
		      double win_width,
		      double gmean,
		      int num_data,
		      double *xdata,
		      double *prob)
{
  int i,k;
  double normalization = 1/( (double)num_data*4.0/3.0 );
  for (i=0; i<num_data; i++)
    add_one_point_epan1d(xdata[i],density,win_width/sqrt(prob[i]/gmean));

  density->data_max  =density->map[0]*normalization;
  density->data_min = density->map[0]*normalization;

  for (k=0; k<density->bins; k++)
    {
      density->map[k] = density->map[k]*normalization;
      if (density->map[k]>density->data_max) 
        density->data_max=density->map[k];
      else
        if (density->map[k]< density->data_min)
          density->data_min = density->map[k];
    }

}


		      
double prob_from_map2d(  map2d *map,
                         double x,
                         double y,
                         double xstep,
                         double ystep )       
{
  int xbin=(int)((x - map->x_min)/xstep);
  double xratio=(x-map->x_min-(xstep*xbin))/xstep;
  int ybin=(int)((y-map->y_min)/ystep);
  double yratio=(y-map->y_min-(ystep*ybin))/ystep;
  double dens1,dens2;

  dens1 = map->map[ybin][xbin]+( map->map[ybin][xbin+1]
				 -map->map[ybin][xbin]  )*xratio; 
  dens2 = map->map[ybin+1][xbin]+( map->map[ybin+1][xbin+1]
				   -map->map[ybin+1][xbin]  )*xratio;
  return dens1 + ( dens2 - dens1 ) * yratio;
}

double prob_from_map3d(  map3d *map,
                         double x,
                         double y,
                         double z,
                         double xstep,
                         double ystep,
                         double zstep )       
{
  int xbin=(int)((x - map->x_min)/xstep);
  double xratio=(x-map->x_min-(xstep*xbin))/xstep;
  int ybin=(int)((y-map->y_min)/ystep);
  double yratio=(y-map->y_min-(ystep*ybin))/ystep;
  int zbin=(int)((z-map->z_min)/zstep);
  double zratio=(z-map->z_min-(zstep*zbin))/zstep;

  double dens1, dens2, dens3, dens4;

  dens1 = map->map[zbin][ybin][xbin]+( map->map[zbin][ybin][xbin+1]
				 -map->map[zbin][ybin][xbin]  )*xratio; 
  dens2 = map->map[zbin][ybin+1][xbin]+( map->map[zbin][ybin+1][xbin+1]
				   -map->map[zbin][ybin+1][xbin]  )*xratio;

  
  dens3 = dens1 + ( dens2 - dens1 ) * yratio;

  dens1 = map->map[zbin+1][ybin][xbin]+( map->map[zbin+1][ybin][xbin+1]
				 -map->map[zbin+1][ybin][xbin]  )*xratio; 
  dens2 = map->map[zbin+1][ybin+1][xbin]+( map->map[zbin+1][ybin+1][xbin+1]
				   -map->map[zbin+1][ybin+1][xbin]  )*xratio;


  dens4 = dens1 + ( dens2 - dens1 ) * yratio;
 

  return dens3 + ( dens4 - dens3 ) * zratio;
}



double comp_data_probs_2d ( map2d  *density,
                            double xwidth,
                            double ywidth,
                            int    num_data,
			    double *xdata,
			    double *ydata,
			    double *prob)
{
  int i,k,l;
  double normalization = 1/( (double)num_data*xwidth*ywidth*PI/2.0 ), 
         gmean = 0.0;
  double xstep = (density->x_max-density->x_min)/(double)(density->x_bins - 1);
  double ystep = (density->y_max-density->y_min)/(double)(density->y_bins - 1);


  for (i=0; i<num_data; i++)
    add_one_point_epan(xdata[i],ydata[i],density,xwidth,ywidth);

  density->data_max  =density->map[0][0]*normalization;
  density->data_min = density->map[0][0]*normalization;
  for (k=0; k<density->y_bins; k++)
    for (l=0; l<density->x_bins; l++)
      {
	density->map[k][l] = density->map[k][l]*normalization;
	if (density->map[k][l]>density->data_max) 
	  density->data_max=density->map[k][l];
	else
	  if (density->map[k][l]< density->data_min)
	    density->data_min = density->map[k][l];
      }

  for (i=0; i<num_data; i++)
    {
      prob[i]=prob_from_map2d(density,xdata[i],ydata[i],xstep,ystep);
      gmean += log(prob[i]);
    }

  for (k=0; k<density->y_bins; k++)
    for (l=0; l<density->x_bins; l++)
      density->map[k][l]=0;

  return exp(gmean/num_data);
}


void map3d_normalize( map3d *density, double normalization ){
  int x,y,z;

  density->data_max  =density->map[0][0][0]*normalization;
  density->data_min = density->map[0][0][0]*normalization;
  for (z=0; z<density->z_bins; z++)
    for (y=0; y<density->y_bins; y++)
      for (x=0; x<density->x_bins; x++){
	density->map[z][y][x] = density->map[z][y][x]*normalization;
	if (density->map[z][y][x]>density->data_max) 
	  density->data_max=density->map[z][y][x];
	else
	  if (density->map[z][y][x]< density->data_min)
	    density->data_min = density->map[z][y][x];
      }

}



double comp_data_probs_3d ( map3d  *density,
                            double xwidth,
                            double ywidth,
                            double zwidth,
                            int    num_data,
			    double *xdata,
			    double *ydata,
			    double *zdata,
			    double *prob)
{
  int i,x,y,z;
  double normalization = 15.0/( (double)num_data*xwidth*ywidth*zwidth*PI*8.0), 
         gmean = 0.0;
  double xstep = (density->x_max-density->x_min)/(double)(density->x_bins - 1);
  double ystep = (density->y_max-density->y_min)/(double)(density->y_bins - 1);
  double zstep = (density->z_max-density->z_min)/(double)(density->z_bins - 1);


  for (i=0; i<num_data; i++){
    add_one_point_epan_3d(xdata[i],ydata[i],zdata[i],
			  density,xwidth,ywidth,zwidth);
  }

  map3d_normalize( density, normalization );

  for (i=0; i<num_data; i++)
    {
      prob[i]=prob_from_map3d(density,
			      xdata[i],ydata[i],zdata[i],
			      xstep,ystep,zstep);
				 
      gmean += (prob[i]);
    // printf("gmean: %lf\n",gmean);
    }

  printf("gmean = %e \n",exp(gmean/num_data));

  for (z=0; z<density->z_bins; z++)
    for (y=0; y<density->y_bins; y++)
      for (x=0; x<density->x_bins; x++)
	density->map[z][y][x]=0;

  return exp(gmean/num_data);
}



void comp_density_2d ( map2d  *density,
		       double xwidth,
		       double ywidth,
		       double gmean,
		       int    num_data,
		       double *xdata,
		       double *ydata,
		       double *prob)
{
  int i,k,l;
  double normalization = 1/( (double)num_data*PI/2.0 );

  for (i=0; i<num_data; i++) {
    add_one_point_epan2(xdata[i],ydata[i],
			density,
			xwidth/sqrt(prob[i]/gmean),
			ywidth/sqrt(prob[i]/gmean));

}

  density->data_max  =density->map[0][0]*normalization;
  density->data_min = density->map[0][0]*normalization;
  for (k=0; k<density->y_bins; k++)
    for (l=0; l<density->x_bins; l++)
      {
	density->map[k][l] = density->map[k][l]*normalization;
         
	if (density->map[k][l]>density->data_max) 
	  density->data_max=density->map[k][l];
	else
	  if (density->map[k][l]< density->data_min)
	    density->data_min = density->map[k][l];
      }

}

void comp_density_3d ( map3d  *density,
		       double xwidth,
		       double ywidth,
		       double zwidth,
		       double gmean,
		       int    num_data,
		       double *xdata,
		       double *ydata,
		       double *zdata,
		       double *prob)
{
  int i;
  double normalization = 15.0/( (double)num_data*PI*8.0 );

  for (i=0; i<num_data; i++)
    add_one_point_epan2_3d(xdata[i],ydata[i],zdata[i],
			   density,
			   xwidth/sqrt(prob[i]/gmean),//instead of square root in the original code
			   ywidth/sqrt(prob[i]/gmean),
			   zwidth/sqrt(prob[i]/gmean)
			   );

  map3d_normalize( density, normalization );

}

void findminmax(int numdata, double *data, double *min, double *max){
  int i;

  *max = *min = data[0];
  for (i=1;i<numdata;i++){
    if (data[i]>(*max))
      *max=data[i];
    else
      if (data[i]<(*min))
	*min=data[i];
  }
}


int ImagePGMBinWrite2D(map2d *map, char *fname)
{
   FILE *outfile;
   int k,l,i;
   unsigned char *buf = malloc(map->x_bins*map->y_bins*sizeof(unsigned char));

   outfile = fopen(fname, "wb");
   if (outfile==NULL) {
      fprintf (stderr, "Error: Can't write the image: %s !", fname);
      return(0);
   }
   fprintf(outfile, "P5\n%d %d\n255\n", map->x_bins, map->y_bins);
 
   i=0;
   for(l=map->y_bins - 1;l>=0;l--)
     for (k=0;k<map->x_bins;k++,i++)
       buf[i] = (unsigned char) ((255.0*sqrt(map->map[l][k]))/sqrt(map->data_max)); 
   fwrite(buf, 1, (size_t)(map->x_bins*map->y_bins), outfile);
 
   fclose(outfile);
   free(buf);
   return(1);
} /* ImagePGMBinWrite */

int ImagePGMBinWrite1D(map1d *map, char *fname)
{
   FILE *outfile;
   int k,l,i;
   unsigned char *buf = malloc(map->bins*sizeof(unsigned char));

   outfile = fopen(fname, "wb");
   if (outfile==NULL) {
      fprintf (stderr, "Error: Can't write the image: %s !", fname);
      return(0);
   }
   fprintf(outfile, "P5\n%d %d\n255\n", map->bins,1);
 
   i=0;
   //for(l=map->y_bins - 1;l>=0;l--)
     for (k=0;k<map->bins;k++,i++)
       buf[i] = (unsigned char) ((255.0*sqrt(map->map[k]))/sqrt(map->data_max)); 
   fwrite(buf, 1, (size_t)(map->bins), outfile);
 
   fclose(outfile);
   free(buf);
   return(1);
} /* ImagePGMBinWrite */



void AVSdens3dwrite(int scaling,
		    map3d *map,
		    char *fname){
  FILE *outfile;
  int x,y,z,i;
  unsigned short *buf = malloc(map->x_bins*map->y_bins*map->z_bins*sizeof(unsigned short));
  avs_header avs_head;

  avs_head.ndim = 3;
  avs_head.dim1 = map->x_bins;
  avs_head.dim2 = map->y_bins;
  avs_head.dim3 = map->y_bins;
  avs_head.min_x = map->x_min;
  avs_head.min_y = map->y_min;
  avs_head.min_z = map->y_min;
  avs_head.max_x = map->x_max;
  avs_head.max_y = map->y_max;
  avs_head.max_z = map->z_max;
  avs_head.filetype = 0;
  avs_head.skip = 0;
  avs_head.nspace = 3;
  avs_head.veclen = 1;
  avs_head.dataname[0] = '\0';
  avs_head.datatype = 2;

  outfile = fopen(fname, "wb");
  if (outfile==NULL) {
    fprintf (stderr, "Error: Can't write the image: %s !", fname);
    return;
  }

  avs_write_header(outfile, &avs_head);
  i=0;
  for (z=0; z<map->z_bins; z++)
    for (y=0; y<map->y_bins; y++)
      for (x=0; x<map->x_bins; x++, i++){
	buf[i]=(unsigned short) (((double)scaling*(map->map[z][y][x]))/map->data_max);
      }
   fwrite(buf, sizeof(unsigned short), (size_t)(map->x_bins*map->y_bins*map->z_bins), outfile);
   fclose(outfile); 
   free(buf);
}


void parse( char *record, char *delim,  char **item)
{
    char *p;
    long fld=0;
    p=strtok(record,delim); 
    while(p)
    {
        strcpy(item[fld],p); 
        printf("%s\n",item[fld]);
        fld++;
	p=strtok('\0',delim); // continue tokenizing until the string is null
    }		
	
}
printHeaders(int dimstart,double *quality,int index[][50000],int numspaces,char **header,int *numLeaves)
{
   int i,j,swapI[numspaces];

  for (i=0;i<numspaces;i++)
    swapI[i]=i;
    
    pixel_qsort(quality,swapI,numspaces);
   

    FILE *outfile = fopen("rankingShellwCorr.txt","a");

  //  int cnt=0;
     fprintf(outfile,"Ranking and Quality of %dD Subspaces:\n",dimstart);

    for(i=numspaces-1;i>=0;i--)
    {
     // if(quality[i]>0)
{
	//int tmp =index[i];
      //for (cnt=0;cnt<dimstart;cnt++)
      //d[cnt]=comb[cnt][i];
     fprintf(outfile,"[");
     for (j=0;j<dimstart-1;j++)
     {
     // printf("%d\n",index[j][swapI[i]]);
      fprintf(outfile,"%s-",header[index[j][swapI[i]]]);
     
      //tmp =tmp/10;
     }
     fprintf(outfile,"%s]\t:%lf\t%d\n",header[index[j][swapI[i]]],quality[i],numLeaves[swapI[i]]);
}
    }
fclose(outfile);
}
//compute3Ddensity(dataPCA,densityGt3,numdata,prob)
double Compute3Ddensity(double **dataPCA,int numdata,double *prob,int *numLeaves)
{
double sx1,sy1,sz1,sx,sy,sz,gmean,quality,ksize;
double *minima,*maxima,**dataS;
map3d density3;
map2d density2;
int i,j,k=0,*dumy,flag=0,pd[3];
dumy = (int *)calloc(numdata, sizeof(int));
 minima= (double *)calloc(3,sizeof(double));
  maxima= (double *)calloc(3,sizeof(double));
dataS= (double **)calloc(3,sizeof(double*));
   
  for (j=0;j<3;j++){
    findminmax(numdata,dataPCA[j],minima+j,maxima+j);
    printf("mimimum[%d] = %f,maximum[%d] = %f \n ",j, minima[j], j, maxima[j] );
    dataS[j]= (double *)calloc(numdata,sizeof(double));
    
     }
    
      for (i=0;i<numdata;i++)
	for (j=0;j<3;j++)
           *(dataS[j]+i)=*(dataPCA[j]+i);

   for (j=0;j<3;j++){
        pixel_qsort (dataS[j],dumy,numdata);
     }
//printf("%lf\n",*(dataS[1]+100));
if(flag==0)
{
  init_map3d(&density3,minima[0]-0.06125*(maxima[0]-minima[0]),
	       maxima[0]+0.06125*(maxima[0]-minima[0]),128,
	       minima[1]-0.06125*(maxima[1]-minima[1]),
	       maxima[1]+0.06125*(maxima[1]-minima[1]),128,
	       minima[2]-0.06125*(maxima[2]-minima[2]),
	       maxima[2]+0.06125*(maxima[2]-minima[2]),128);
    
//     sx1=((*(sdata[k]+(int)ceil(numdata*0.80))-*(sdata[k]+(int)ceil(numdata*0.20))))/log(numdata);
//     sy1=((*(sdata[l]+(int)ceil(numdata*0.80))-*(sdata[l]+(int)ceil(numdata*0.20))))/log(numdata);
//     sz1=((*(sdata[m]+(int)ceil(numdata*0.80))-*(sdata[m]+(int)ceil(numdata*0.20))))/log(numdata);


    sx1=((*(dataS[0]+(int)ceil(numdata*0.90))-*(dataS[0]+(int)ceil(numdata*0.10))))/log(numdata);
    sy1=((*(dataS[1]+(int)ceil(numdata*0.90))-*(dataS[1]+(int)ceil(numdata*0.10))))/log(numdata);
    sz1=((*(dataS[2]+(int)ceil(numdata*0.90))-*(dataS[2]+(int)ceil(numdata*0.10))))/log(numdata);

//0.25 for 4D for higher 1 is ok
    sx=(maxima[0]-minima[0])/cbrt(numdata);
    sy=(maxima[1]-minima[1])/cbrt(numdata);
    sz=(maxima[2]-minima[2])/cbrt(numdata);

    if(sx1>sx)
    sx1=sx;

    if(sy1>sy)
    sy1=sy;

    if(sz1>sz)
    sz1=sz;
 //  printf("%f\t%f\t%f\t%f\t%f\t%f\n", sx1, sy1,sz1, sx, sy,sz);
  

    if (sx1>sy1)
      ksize=sx1;
    else  ksize=sy1;

    if (ksize<sz1)
      ksize=sz1;
    
    if(ksize<0)
      ksize=-ksize;

   ksize=1.4*ksize;
    printf("%lf\n", ksize);
//ksize=1.0/sy;
    //gmean = comp_data_probs_3d(&density3,sx1,sy1,sz1,numdata, data[k], data[l], data[m], prob);
       gmean = comp_data_probs_3d(&density3,ksize,ksize,ksize,numdata,dataPCA[0],dataPCA[1], dataPCA[2], prob);

    if(gmean==0)
    {
	printf("Automatic window width fails. Try with manual scaling \n");

    }

   // comp_density_3d ( &density3,sx1,sy1,sz1,gmean,numdata,data[k],data[l],data[m],prob);
       comp_density_3d ( &density3,ksize,ksize,ksize,gmean,numdata,dataPCA[0],dataPCA[1], dataPCA[2],prob);


	 AVSdens3dwrite(4095,&density3,"densityPCA.fld");
         quality=MTfor3D("densityPCA.fld",numLeaves);
	 //printf("Quality:%lf\n",quality);
	 exit_map3d (&density3);
}
if(flag==2)
{
init_map2d(&density2,minima[0]-0.06125*(maxima[pd[0]]-minima[pd[0]]),
	       maxima[0]+0.06125*(maxima[pd[0]]-minima[pd[0]]),128,
	       minima[1]-0.06125*(maxima[pd[1]]-minima[pd[1]]),
	       maxima[1]+0.06125*(maxima[pd[1]]-minima[pd[1]]),128);
    
//     sx1=((*(sdata[k]+(int)ceil(numdata*0.80))-*(sdata[k]+(int)ceil(numdata*0.20))))/log(numdata);
//     sy1=((*(sdata[l]+(int)ceil(numdata*0.80))-*(sdata[l]+(int)ceil(numdata*0.20))))/log(numdata);
//     sz1=((*(sdata[m]+(int)ceil(numdata*0.80))-*(sdata[m]+(int)ceil(numdata*0.20))))/log(numdata);


    sx1=((*(dataS[pd[0]]+(int)ceil(numdata*0.90))-*(dataS[pd[0]]+(int)ceil(numdata*0.10))))/log(numdata);
    sy1=((*(dataS[pd[1]]+(int)ceil(numdata*0.90))-*(dataS[pd[1]]+(int)ceil(numdata*0.10))))/log(numdata);
   

    sx=(maxima[pd[0]]-minima[pd[0]])/sqrt(numdata);
    sy=(maxima[pd[1]]-minima[pd[1]])/sqrt(numdata);


    if(sx1>sx)
    sx1=sx;

    if(sy1>sy)
    sy1=sy;

    if (sx1<sy1)
      ksize=sx1;
    else  ksize=sy1;

    if (ksize>sz1)
      ksize=sz1;
    
    if(ksize<0)
      ksize=-ksize;


   // printf("%lf\n", ksize);
//ksize=1.0/sy;
    //gmean = comp_data_probs_3d(&density3,sx1,sy1,sz1,numdata, data[k], data[l], data[m], prob);
       gmean = comp_data_probs_2d(&density2,ksize,ksize,numdata,dataPCA[pd[0]],dataPCA[pd[1]], prob);

    if(gmean==0)
    {
	printf("Automatic window width fails. Try with manual scaling \n");

    }

   // comp_density_3d ( &density3,sx1,sy1,sz1,gmean,numdata,data[k],data[l],data[m],prob);
       comp_density_2d ( &density2,ksize,ksize,gmean,numdata,dataPCA[pd[0]],dataPCA[pd[1]],prob);

    ImagePGMBinWrite2D(&density2,"density2PCA.pgm");
    quality=MaxTreeforSpaces("density2PCA.pgm",4,numLeaves);
	 //printf("Quality:%lf\n",quality);
	 exit_map2d (&density2);
}
         free(dumy);
         free(minima);
         free(maxima);
         
         for (i=0;i<3;i++)
	    free(dataS[i]);	
     //   printf("here\n");
    	  free(dataS);
	 return quality;
}
double pearsn(double *x, double *y, unsigned long n)
/*Given two arrays x[1..n] and y[1..n], this routine computes their correlation coecient
r (returned as r), the signicance level at which the null hypothesis of zero correlation is
disproved (prob whose small value indicates a signicant correlation), and Fisher's z (returned
as z), whose value can be used in further statistical tests as described above.*/
{
unsigned long j;
double yt,xt,t,df,r;
double syy=0.0,sxy=0.0,sxx=0.0,ay=0.0,ax=0.0;
//Find the means.
for (j=1;j<=n;j++) { 
ax += x[j];
ay += y[j];
}
ax /= n;
ay /= n;
//Compute the correlation coeficient.
for (j=1;j<=n;j++) { 
xt=x[j]-ax;
yt=y[j]-ay;
sxx += xt*xt;
syy += yt*yt;
sxy += xt*yt;
}
r=sxy/(sqrt(sxx*syy));
return r;
}
int main(int argc, char *argv[]){
  char *infilename,**header, line[1024],*delim={",\t;"};
  char denfDDP[100],*denfPGM,denfg[100],avsfilename[100],denftarget[100];
  
  int numdata,numcols, i,j;

  double **data,**sdata;
  double *minima,*maxima, *prob,*quality;
  double gmean,hscale;
  map1d density1;
  map2d density;
  map3d density3;
  clock_t start;
  struct tms tstruct;
  long tickspersec = sysconf(_SC_CLK_TCK);  
  double musec;
  int dimstart,dimend=numcols,dims,*dumy,numspaces,comb[30][50000];
  int k=0,l=1,m=2;
//-------------------------
double sx,sy,sz,sx1,sy1,sz1;
//----------------------------------------
  if(argc>=5)
 {
  numdata=atoi(argv[1]);
  numcols=atoi(argv[2]);
  infilename=argv[3];
  dimstart=atoi(argv[4]);
 }
 else {
      printf("Usage : <numdata> <numcol> <InputFileName> <dimstart> [dimend]\n");
      exit(0);
} 
if (argc>5){
    dimend = atoi(argv[5]);
  }

  FILE *infile = fopen(infilename,"r");
//FILE *gf = fopen("dataCopy.txt","w+");

  data = (double **)calloc(numcols, sizeof(double*));
  sdata = (double **)calloc(numcols, sizeof(double*));	
  minima= (double *)calloc(numcols,sizeof(double));
  maxima= (double *)calloc(numcols,sizeof(double));
  prob = (double *)calloc(numdata,sizeof(double));
  //comb = (int *)calloc(100, sizeof(int));
  dumy = (int *)calloc(numdata, sizeof(int));
  header= (char **)calloc(numcols,sizeof(char*));

fscanf(infile,"%s",line);
//fscanf(infile," \n");


  for (j=0;j<numcols;j++){
    data[j]= (double *)calloc(numdata,sizeof(double));
    sdata[j]= (double *)calloc(numdata,sizeof(double));
    header[j]=  (char *)calloc(20,sizeof(char));
   
    }

parse(line,delim,header);
 

  printf("reading data....................\n");
  for (i = 0; i<numdata; i++){
    for (j=0;j<numcols;j++)
      { if(j<numcols-1)
         { fscanf(infile," %lf",data[j]+i);
          *(sdata[j]+i)=*(data[j]+i);
           fscanf(infile,"%*c");
         //printf("%lf\t", *(data[j]+i));
         }
        else
           {fscanf(infile," %lf",data[j]+i);
           *(sdata[j]+i)=*(data[j]+i);
	   // printf("%lf", *(data[j]+i));
	   }
      }//printf("\n");
      
    fscanf(infile," \n");
  }
  printf("done reading data-----------------\n");

  for (j=0;j<numcols;j++){
    findminmax(numdata,data[j],minima+j,maxima+j);
    printf("mimimum[%d] = %f,maximum[%d] = %f \n ",j, minima[j], j, maxima[j] );
  //  pixel_qsort (sdata[j],dumy,numdata);
    //printf("%s\n",header[j]);
  }

for (i = 0; i<numdata; i++)
    for (j=0;j<numcols;j++)
    {
      *(data[j]+i)=((*(data[j]+i)-minima[j])/(maxima[j]-minima[j]))*100.0;
      *(sdata[j]+i)=*(data[j]+i);
    }

for (j=0;j<numcols;j++){
    findminmax(numdata,data[j],minima+j,maxima+j);
   // printf("mimimum[%d] = %f,maximum[%d] = %f \n ",j, minima[j], j, maxima[j] );
    pixel_qsort (sdata[j],dumy,numdata);
    //printf("%s\n",header[j]);
  }
free(dumy);
while(dimstart<=dimend)
{
  if (dimstart==2){

    int d[dimstart],tmp;    
    numspaces=combi(numcols,dimstart,comb);
    int numLeaves[numspaces];
    printf("Combination Done:nums:%d\n",numspaces);
 quality = (double*)calloc(numspaces,sizeof(double));
 //index= (int *)calloc(numspaces,sizeof(int));
int cnt;
printf("\nComputing quality for %dD subspaces....\n\n",dimstart);
 for (i=0;i<numspaces;i++)
{    
//       d[0]=comb[i]%10;
//       d[1] =comb[i]/10;
      for (cnt=0;cnt<dimstart;cnt++)
      d[cnt]=comb[cnt][i];
    
printf("%d\t%d\n",d[0],d[1]);
    //printf("index=%d\n",index[i]);
    init_map2d(&density,minima[d[0]]-0.06125*(maxima[d[0]]-minima[d[0]]),
	       maxima[d[0]]+0.06125*(maxima[d[0]]-minima[d[0]]),512,
	       minima[d[1]]-0.06125*(maxima[d[1]]-minima[d[1]]),
	       maxima[d[1]]+0.06125*(maxima[d[1]]-minima[d[1]]),512);

//     sx1=((*(sdata[d[0]]+(int)ceil(numdata*0.80))-*(sdata[d[0]]+(int)ceil(numdata*0.20))))/log(numdata);
//     sy1=((*(sdata[d[1]]+(int)ceil(numdata*0.80))-*(sdata[d[1]]+(int)ceil(numdata*0.20))))/log(numdata);
  
    	
	sx1=3.0*(maxima[d[0]]-minima[d[0]])/cbrt(numdata);
	sy1=3.0*(maxima[d[1]]-minima[d[1]])/cbrt(numdata);
	
         
	//if(sx1<sy1)
	//sy1=sx1;
	//else sx1=sy1;
  
    double factorx,factory;
    CImg<unsigned char> image(256,256,1,3,0);
    CImgDisplay main_disp(image,"Click left mouse button to interrupt");
    while(!main_disp.is_keyESC ()) //letf mouse=1; right mouse=2;
      {	
	
    gmean = comp_data_probs_2d(&density,sx1,sy1,numdata, data[d[0]], data[d[1]], prob);

    if(gmean==0) printf ("gmean:%lf\n",gmean);
    comp_density_2d ( &density,sx1,sy1,gmean,numdata, data[d[0]],data[d[1]],prob);
    char fn[30];
//     strcpy(fn,"density2");
//     strcat(fn,header[d[0]]);
//     strcat(fn,header[d[1]]);
//     strcat(fn,".pgm");
    FILE *outfile = fopen("InterestingSubspaces.txt","a");
     fprintf(outfile,"Dim:%d%d\n",d[0],d[1]);
    fclose(outfile);    

    ImagePGMBinWrite2D(&density,"Density2D.pgm");
    
    
	image.assign("Density2D.pgm");
	image.display(main_disp);
	for(i=0;i<2000000000;i++)
	  {
	    if(main_disp.button()==2)
	    {
	      printf("Current scalling: %lf\t%lf\n",sx1,sy1);
	      printf("New scalling factor:(ex. a*sx1 and b*sy1)");
	      scanf("%lf,%lf",&factorx,&factory);
	      sx1=sx1*factorx;sy1=sy1*factory;
	      break;
	    }
	    else if (main_disp.button()==1)
	    { printf("next image is in preparation\n");
	      break;
	    }
	    
	  }
         //main_disp.wait(25000);
//        printf("next image is in preparation\n");
        break;
     }
    quality[i]=MaxTreeforSpaces("Density2D.pgm",4,&numLeaves[i]);
    printf("%lf\n",quality[i]);
exit_map2d (&density);
}
    printHeaders(dimstart,quality,comb,numspaces,header,numLeaves);
    dimstart=3;
    free(quality);
  //  free(comb);
  } 
else if (dimstart==1){
    int loop,*index,numLeaves[numcols];
    quality = (double *)calloc(numcols,sizeof(double));
    index= (int *)calloc(numcols,sizeof(int)); 
printf("\nComputing quality for %dD subspaces....\n\n",dimstart);
    for(loop=0;loop<numcols;loop++)
{
       init_map1d(&density1,minima[loop],maxima[loop],512);
       index[loop]=loop;
	
    sx1=2.0*(maxima[loop]-minima[loop])/cbrt(numdata);
	
    gmean = comp_data_probs_1d(&density1,sx1,numdata, data[loop], prob);
    adaptive_dens_1d(&density1,sx1,gmean,numdata,data[loop],prob);

    FILE *outfile = fopen("InterestingSubspaces.txt","a");
     fprintf(outfile,"Dim:%d\n",loop);
    fclose(outfile); 

    ImagePGMBinWrite1D(&density1,"density1.pgm");
    quality[loop]=MaxTreeforSpaces("density1.pgm",2,&numLeaves[loop]);
printf("[%s]:%lf\n",header[index[loop]],quality[loop]);
 free(density1.map);
}
    pixel_qsort(quality,index,numcols);
    FILE *outfile = fopen("ranking.txt","a");
    fprintf(outfile,"Quality of %dD Subspaces:\n",dimstart);
    for(i=numcols-1;i>=0;i--)
    //for(i=0;i<numcols;i++)
    fprintf(outfile,"[%s]:%lf\t%d\n",header[index[i]],quality[i],numLeaves[index[i]]);
    fclose(outfile);
    dimstart=2;
    free(quality);
    
  } 

/*else if (dimstart==3) {
//int numLeaves[2];
      int flag=1;
      int d[dimstart],tmp;  
   // comb = (int *)calloc(1000, sizeof(int));
    numspaces=combi(numcols,dimstart,comb);
    int numLeaves[numspaces];
    printf("Combination Done:nums:%d\n",numspaces);
   quality = (double*)calloc(numspaces,sizeof(double));

int cnt=0,t1,t2;
double corr;
printf("\nComputing quality for %dD subspaces....\n\n",dimstart);
    // fprintf(outfile,"DIM,Quality,NumberOfLeaves,Dynamics\n");
 for (i=0;i<numspaces;i++)
{
//       d[0]=comb[i]%10;
//        tmp =comb[i]/10;
//        d[1]=tmp%10;
//        d[2] =tmp/10;
   for (cnt=0;cnt<dimstart;cnt++)
      d[cnt]=comb[cnt][i];


printf("Combinations%d-%d-%d\n",d[0],d[1],d[2]);
 for(t1=0;t1<dimstart;t1++)
      {if(flag==1)
       {
       for(t2=t1+1;t2<dimstart;t2++)
       {
        if(flag==1)
	{
	 corr=pearsn(data[d[t1]],data[d[t2]],numdata);
         if(corr>0.89)
          {flag=0;printf("Correlated: %lf\n",corr);}		
	}
	else break;
       }
       }
       else break;
	}
if(flag==1)
{  printf("correlated\n");
   init_map3d(&density3,minima[d[0]]-0.06125*(maxima[d[0]]-minima[d[0]]),
	       maxima[d[0]]+0.06125*(maxima[d[0]]-minima[d[0]]),128,
	       minima[d[1]]-0.06125*(maxima[d[1]]-minima[d[1]]),
	       maxima[d[1]]+0.06125*(maxima[d[1]]-minima[d[1]]),128,
	       minima[d[2]]-0.06125*(maxima[d[2]]-minima[d[2]]),
	       maxima[d[2]]+0.06125*(maxima[d[2]]-minima[d[2]]),128);
    
  sx1=12.0*((*(sdata[d[0]]+(int)ceil(numdata*0.80))-*(sdata[d[0]]+(int)ceil(numdata*0.20))))/log(numdata);
    sy1=12.0*((*(sdata[d[1]]+(int)ceil(numdata*0.80))-*(sdata[d[1]]+(int)ceil(numdata*0.20))))/log(numdata);
    sz1=12.0*((*(sdata[d[2]]+(int)ceil(numdata*0.80))-*(sdata[d[2]]+(int)ceil(numdata*0.20))))/log(numdata);

if(sx1<0)
sx1=-sx1;
if(sy1<0)
sy1=-sy1;
if(sz1<0)
sz1=-sz1;
/*
    sx1=15.0*(maxima[d[0]]-minima[d[0]])/sqrt(numdata);
    sy1=15.0*(maxima[d[1]]-minima[d[1]])/sqrt(numdata);
    sz1=15.0*(maxima[d[2]]-minima[d[2]])/sqrt(numdata);*/

//     if(sx1>sx)
//     sx1=sx;
// 
//     if(sy1>sy)
//     sy1=sy;
// 
//     if(sz1>sz)
//     sz1=sz;
   //printf("%f\t%f\t%f\t%f\t%f\t%f\n", sx1, sy1,sz1, sx, sy,sz);
  
// double ksize;
//     if (sx1<sy1)
//       ksize=sx1;
//     else  ksize=sy1;
// 
//     if (ksize>sz1)
//       ksize=sz1;
//     
//     if(ksize<0)
//       ksize=-ksize;
// // 
// // 
// //     printf("%lf\n", ksize);
// 
//    printf("%f\t%f\t%f\n", sx1, sy1,sz1);
//   ksize=0.1*ksize;
// //printf("%lf\n", ksize);
//     gmean = comp_data_probs_3d(&density3,sx1,sy1,sz1,numdata, data[d[0]], data[d[1]], data[d[2]], prob);
//      // gmean = comp_data_probs_3d(&density3,ksize,ksize,ksize,numdata, data[d[0]], data[d[1]], data[d[2]], prob);
//     if(gmean==0)
//     {
// 	printf("Automatic window width fails. Try with manual scaling \n");
// 
//     }
//    //printf("Here\n");
//    comp_density_3d ( &density3,sx1,sy1,sz1,gmean,numdata,data[d[0]], data[d[1]], data[d[2]],prob);
//      // comp_density_3d ( &density3,ksize,ksize,ksize,gmean,numdata,data[d[0]], data[d[1]], data[d[2]],prob);
// printf("3D density done\n");
//     FILE *outfile = fopen("InterestingSubspacesNew.txt","a");
//      fprintf(outfile,"Dim:%d-%d-%d,",d[0],d[1],d[2]);
//     fclose(outfile);
// 
// 	 AVSdens3dwrite(4095,&density3,"density3.fld");
//          quality[i]=MTfor3D("density3.fld",&numLeaves[i]);
// 	 printf("Quality:%lf\n",quality[i]);
// 	 exit_map3d (&density3);
// }
// else
// {
//        printf("correlated\n");
//        quality[i]=0;
//        numLeaves[i]=0;
//         printf("space:%d,Quality:%lf,Numleaves:%d\n",i,quality[i],numLeaves[i]);
// 	
// }
// }
//   
//   printHeaders(dimstart,quality,comb,numspaces,header,numLeaves);
//    dimstart=4;
//   free(quality);
// }
else
{
     int d[dimstart],tmp; 
     //map3d densityGt3;
     double **dataPCA,**dataPCAcopy;
     
    dataPCA= (double **)calloc((numdata+1), sizeof(double*));
     dataPCAcopy = (double **)calloc(3, sizeof(double*));

   // comb = (int *)calloc(1000, sizeof(int));
    numspaces=combi(numcols,dimstart,comb);
    int numLeaves[numspaces];
    printf("Combination Done:nums:%d\n",numspaces);
   quality = (double*)calloc(numspaces,sizeof(double));

    for (j=0;j<=numdata;j++){
    dataPCA[j]= (double *)calloc((dimstart+1),sizeof(double));
    }
 
    for (j=0;j<3;j++){
    dataPCAcopy[j]= (double *)calloc(numdata,sizeof(double));
    }
printf("\nComputing quality for %dD subspaces....\n\n",dimstart);
//i=0;
int nS,cnt;
double tmpC,corr;
int flag,t1,t2;
 for (nS=0;nS<numspaces;nS++)
{
flag=1;

FILE *outfile = fopen("InterestingSubspacesNew.txt","a");
     fprintf(outfile,"Dim:");
    
  for (cnt=0;cnt<dimstart;cnt++)
    {
      d[cnt]=comb[cnt][nS];
      fprintf(outfile,"%d-",d[cnt]);
      printf("%d-",d[cnt]);
    }
    printf("\n");
    fprintf(outfile,",");
    fclose(outfile);
 //     printf("%d\t%d\t%d\t%d\n",d[0],d[1],d[2],d[3]);
//        d[0]=comb[i]%10;
//        tmp =comb[i]/10;
//        d[1]=tmp%10;
//        d[2] =tmp/10;
       
    for(t1=0;t1<dimstart;t1++)
      {if(flag==1)
       {
       for(t2=t1+1;t2<dimstart;t2++)
       {
        if(flag==1)
	{
	 corr=pearsn(data[d[t1]],data[d[t2]],numdata);
         if(corr>0.89)
          {flag=0;printf("Correlated: %lf\n",corr);}		
	}
	else break;
       }
       }
       else break;
	}
       //do pca
      if(flag==1)
      {printf("Not correlated\n");
       for (i=1; i<=numdata; i++){
	for (j=1;j<=dimstart;j++)
        {
	tmpC=*(data[d[j-1]]+(i-1));
       // printf("%lf\n",tmpC);
        *(dataPCA[i]+j)=tmpC;
         //printf("%d\t%d\t%lf\n",i,j,*(dataPCA[i]+j));
	// printf("%lf\n",*(data[d[j-1]]+ (i-1)));
       // printf("%d\t%d\n",i,j);
	}
	}
     //printf("HERE\n");
      pca(dataPCA,numdata,dimstart);
 
     //  FILE *outf = fopen("PCA.txt","w+");
       for (i=0; i<numdata; i++){
	for (j=0;j<3;j++)
        {
	*(dataPCAcopy[j]+i)=*(dataPCA[i+1]+(j+1))*100;
        
        }
       // fprintf(outf,"%lf,%lf,%lf\n",*(dataPCAcopy[0]+i),*(dataPCAcopy[1]+i),*(dataPCAcopy[2]+i));
	}
       
      quality[nS]=Compute3Ddensity(dataPCAcopy,numdata,prob,&numLeaves[nS]);
      printf("Space:%d,Quality:%lf,Numleaves:%d\n",nS,quality[nS],numLeaves[nS]);
      }
      else
      {
       printf("correlated\n");
       quality[nS]=0;
       numLeaves[nS]=0;
        printf("space:%d,Quality:%lf,Numleaves:%d\n",nS,quality[nS],numLeaves[nS]);
	}
      //printf("Quality: %lf\n",quality[nS]);
 }   
  
printHeaders(dimstart,quality,comb,numspaces,header,numLeaves);
dimstart=dimstart+1;  

for (i=0;i<3;i++)
  free(dataPCAcopy[i]);
  free(dataPCAcopy);

for (i=0;i<=numdata;i++)
  free(dataPCA[i]);
  free(dataPCA);

  free(quality);
}
}

free(prob);
free(maxima);
free(minima);

for (i=0;i<numcols;i++)
 { free(data[i]);free(sdata[i]);free(header[i]);}
  free(data);
  free(sdata);
  free(header);
  return 0;
}



