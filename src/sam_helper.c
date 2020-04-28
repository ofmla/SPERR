#include "sam_helper.h"

#include <math.h>
#include <stdio.h>

int sam_get_statsf( const float* arr1, const float* arr2, size_t len,/* input  */
                    float* rmse,       float* lmax,    float* psnr,  /* output */
                    float* arr1min,    float* arr1max            )   /* output */
{
    *arr1min  = arr1[0];
    *arr1max  = arr1[0];
    float sum = 0.0f, c = 0.0f;
    float diff, y, t;
    size_t  i;
    *lmax = 0.0f;
    for( i = 0; i < len; i++) 
    {
        /* Kahan summation */
        diff = fabsf( arr1[i] - arr2[i] ); /* single precision version of abs() */
        y    = diff * diff - c;
        t    = sum + y;
        c    = t - sum - y;
        sum  = t;

        /* detect the lmax*/
        if( diff > *lmax )
            *lmax = diff;

        /* detect min and max of arr1 */
        if( arr1[i]  < *arr1min )
            *arr1min = arr1[i];
        if( arr1[i]  > *arr1max )
            *arr1max = arr1[i];
    }

    sum  /= (float)len;
    *rmse = sqrtf( sum );   /* single precision version of sqrt() */

    float range2 = (*arr1max - *arr1min) * (*arr1max - *arr1min );
    *psnr = -10.0f * log10f( sum / range2 ); /* single precision version of log10() */
    
    return  0;
}

int sam_get_statsd( const double* arr1, const double* arr2, size_t len, /* input  */
                    double* rmse,       double* lmax,   double* psnr,   /* output */
                    double* arr1min,    double* arr1max            )    /* output */
{
    *arr1min  = arr1[0];
    *arr1max  = arr1[0];
    double sum = 0.0, c = 0.0;
    double diff, y, t;
    size_t  i;
    *lmax = 0.0;
    for( i = 0; i < len; i++) 
    {
        /* Kahan summation */
        diff = fabs( arr1[i] - arr2[i] ); /* double precision version of abs() */
        y    = diff * diff - c;
        t    = sum + y;
        c    = t - sum - y;
        sum  = t;

        /* detect the lmax*/
        if( diff > *lmax )
            *lmax = diff;

        /* detect min and max of arr1 */
        if( arr1[i]  < *arr1min )
            *arr1min = arr1[i];
        if( arr1[i]  > *arr1max )
            *arr1max = arr1[i];
    }

    sum  /= (double)len;
    *rmse = sqrt( sum );   /* double precision version of sqrt() */

    double range2 = (*arr1max - *arr1min) * (*arr1max - *arr1min );
    *psnr = -10.0 * log10( sum / range2 ); /* double precision version of log10() */
    
    return  0;
}


int sam_read_n_bytes( const char* filename, size_t n_bytes,           /* input  */
                      void*       buffer               )              /* output */
{
    FILE* f = fopen( filename, "r" );
    if( f == NULL )
    {
        fprintf( stderr, "Error! Cannot open input file: %s\n", filename );
        return 1;
    }
    fseek( f, 0, SEEK_END );
    if( ftell(f) < n_bytes )
    {
        fprintf( stderr, "Error! Input file size error: %s\n", filename );
        fprintf( stderr, "  Expecting %ld bytes, got %ld bytes.\n", n_bytes, ftell(f) );
        fclose( f );
        return 1;
    }
    fseek( f, 0, SEEK_SET );

    if( fread( buffer, 1, n_bytes, f ) != n_bytes )
    {
        fprintf( stderr, "Error! Input file read error: %s\n", filename );
        fclose( f );
        return 1;
    }

    fclose( f );
    return 0;
}

int sam_write_n_bytes( const char*   filename, size_t n_bytes,
                       const void*   buffer              )
{
    FILE* f = fopen( filename, "w" );
    if( f == NULL )
    {
        fprintf( stderr, "Error! Cannot open output file: %s\n", filename );
        return 1;
    }
    if( fwrite(buffer, 1, n_bytes, f) != n_bytes )
    {
        fprintf( stderr, "Error! Output file write error: %s\n", filename );
        fclose( f );
        return 1;
    }
    fclose( f );
    return 0;
}
