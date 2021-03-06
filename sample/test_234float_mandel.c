/**********************************************************/
/**
 * 234Compositor - Image data merging library
 *
 * Copyright (c) 2013-2015 Advanced Institute for Computational Science, RIKEN.
 * All rights reserved.
 *
 **/
/**********************************************************/

// @file   test_234float_mandel.c

// @brief  Test program for 234Compositor
//         Image generator and compositor for RGBA32 Images
//         mpicc -o test_234float_mandel test_234float_mandel.c -lm lib234comp.a 

// @author Jorji Nonaka (jorji@riken.jp)

#define WIDTH  512
#define HEIGHT 512

#include "234compositor.h"

int render_mandelbrotf ( int, int, int, int, float* );
int write_rgbaf_image  ( char*, float*,  unsigned int, unsigned int , unsigned int );

int main( int argc, char* argv[] )
{
	int rank;
	int nnodes;

	unsigned int width, height;
	unsigned int image_size;
	unsigned int image_type;

	float* rgba_image;
	char* image_filename;
	_Bool write_input;
	
	//=====================================
	write_input = false;

	if ( argc == 1 ) {
		width  = WIDTH;
		height = HEIGHT;
	}
	if ( argc == 2 ) {
		width  = atoi(argv[1]);
		height = width;
	}
	else if ( argc == 3 ) {
		width  = atoi(argv[1]);
		height = atoi(argv[2]);
	}
	else if ( argc == 4 ) {
		width  = atoi(argv[1]);
		height = atoi(argv[2]);
		write_input = true;
	}
	else { 
		printf ("\n Usage: %s Width Height [ Write_Input_Images ( 0:NO or 1:YES) ]\n\n", argv[0] );
		exit( EXIT_FAILURE );
	}		

	//=====================================
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &nnodes);

	//=====================
	// IMAGE TYPE
	pixel_ID   = ID_RGBA128;
	image_type = RGBA128;

	image_size = width * height * image_type;
	//=====================

	//=====================================
	// Generate Image Files
	//=====================================
	if (( rgba_image = (float *)allocate_float_memory_region( image_size )) == NULL ) {
			MPI_Finalize();
			exit ( EXIT_FAILURE );
	}

	render_mandelbrotf ( rank, nnodes, width, height, rgba_image );

	if (( image_filename = (char *)allocate_byte_memory_region( 255 )) == NULL ) {
			MPI_Finalize();
			exit ( EXIT_FAILURE );
	}

	if ( write_input == true ) {
		sprintf( image_filename, "input_%d_%dx%d.rgba128", (int)(rank+1), width, height);
		write_rgbaf_image( image_filename, rgba_image, width, height, image_type );
	}

	//=====================================
	// Execute 234Compositor
	//=====================================
	Init_234Composition ( rank, nnodes, width, height, pixel_ID );
    Do_234Composition   ( rank, nnodes, width, height, pixel_ID, ALPHA_BtoF, rgba_image, MPI_COMM_WORLD );
    Destroy_234Composition ( pixel_ID );
	//=====================================

	if ( rank == ROOT_NODE )
	{
		sprintf( image_filename, "output_%dx%d.rgba128", (int)width, (int)height );
		write_rgbaf_image( image_filename, rgba_image, width, height, image_type );
	}

	MPI_Finalize();
	return ( EXIT_SUCCESS );
}

/*===========================================================================*/
/**
 *  @brief Write output RGBA image. 
 *
 *  @param  rgba_filename [in] Filename 
 *  @param  image         [in] Image to be written
 *  @param  width         [in]  Image width
 *  @param  height        [in]  Image height
 *  @param  pixel_size    [in]  Pixel size
 */
/*===========================================================================*/
int write_rgbaf_image \
		( char*  rgba_filename, \
		  float* image, \
		  unsigned int  width, \
		  unsigned int  height, \
		  unsigned int  pixel_size )
{

	unsigned int image_size;
	FILE* out_fp;
	
	if ( (out_fp = fopen( rgba_filename, "wb")) == NULL ) 
	{
		printf( "<<< ERROR >>> Cannot open \"%s\" for writing \n", \
			    rgba_filename );
		return EXIT_FAILURE;
	}

	image_size = width * height * pixel_size;

	if ( fwrite( image, image_size, 1, out_fp ) != 1 ) 
	{
		printf("<<< ERROR >>> Cannot write image \n" );
		return EXIT_FAILURE;
	}

	fclose( out_fp );
	return EXIT_SUCCESS;
}

// ====================================================================
//							COMPUTE MANDELBROT
// ====================================================================
//
// 		Compute the mandelbrot-set function for a given point in the 
//  complex plane.
//
// 	http://www.mcs.csuhayward.edu/~tebo/Classes/6580/mandel/mandelpgm.c
//
//  Compute/draw mandelbrot set using MPI/MPE commands
//  Written Winter, 1998, W. David Laverell.
//  Simplified Winter 2002, Joel Adams. 
//  Converted for pgm output, 2005 Bill Thibault.
//===================================================================== 

void mandel_compute \
		( double x, double y, double c_real, double c_imag, \
		  double *ans_x, double *ans_y )
{
	*ans_x = x * x - y * y + c_real;
	*ans_y = 2 * x * y + c_imag;
}

//===================================================================== 

double mandel_distance(double x, double y)
{
	return( x * x + y * y );
}

//===================================================================== 

void mandel_convert_to_world \
		( int ix, int iy, int x_imgsize, int y_imgsize, \
		  double xmin, double xmax, double ymin, double ymax, \
		  double *xw, double *yw )
{
	double x, y;
	
	x = ix;
	y = y_imgsize - iy;
	
	x /= x_imgsize;    
	y /= y_imgsize;
	
	x = ( x * ( xmax - xmin ) + xmin );  
	y = ( y * ( ymax - ymin ) + ymin );
	
	*xw = x;
	*yw = y;
}

/*========================================================*/
/**
 *  @brief Render Scanline (Block) Color Image
 *
 *  @param  my_rank      [in]  MPI rank number
 *  @param  nnodes       [in]  Number of nodes
 *  @param  width        [in]  Image width
 *  @param  height       [in]  Image height
 *  @param  block1       [out] RGB pixel color
 *  @param  block2       [out] pixel depth
 */
/*========================================================*/
int render_mandelbrotf ( int my_rank, int nnodes, int width, int height, float* rgba_image )
{	
	//unsigned int start, end;
	int    mandel_n;
	double mandel_x;
	double mandel_y;
	double mandel_c_real;
	double mandel_c_imag;
	
	struct pixel 
	{
		float red;
		float green;
		float blue;
		float alpha;
	} foreground_pixel;

	int x, y;
	int i;
	int image_size;
	float pixel_color, R, G, B, A;
	float *rgba_image_ptr;

	unsigned int color;

	// ====================================================================
	//						IMAGE SUBDIVISION (BLOCK)
	// ====================================================================
	image_size = width * height;

	unsigned int block1, block2;
	unsigned int block_start, block_end;

	block1 = image_size / nnodes; 
	block2 = image_size % nnodes; 

	if ( my_rank < block2 ) {
		block_start = my_rank * block1 + my_rank;
	} else {
		block_start = my_rank * block1 + block2;
	}

	block_end = block_start + block1 - 1;
	if ( block2 > my_rank )	{
		block_end++;
	}

	// ====================================================================
	//						PREPARE RGBA IMAGE DATA 
	// ====================================================================

	R = B = G = 0.0f;
	A = 0.0f; // Tottally transparent background

	rgba_image_ptr = (float *)rgba_image;
	for ( i = 0; i < image_size; i++ ) {
		*rgba_image_ptr++ = (float)B;
		*rgba_image_ptr++ = (float)G;
		*rgba_image_ptr++ = (float)R;
		*rgba_image_ptr++ = (float)A; 
	}

	rgba_image_ptr = rgba_image + block_start * RGBA; // 4 ELEMENTS
	foreground_pixel.alpha = 0.5f;

	for ( i = block_start; i <= block_end; i++ ) 
	{
		y = ceil ( i / width );
		x = ( width * ( y + 1 )) - i;

			mandel_convert_to_world ( x, y, width, height, \
									  -2, 0.5, -1.25, 1.25, \
									  &mandel_c_real, &mandel_c_imag );
		
			mandel_x = 0.0;
			mandel_y = 0.0;
       		mandel_n = 0;
				
			while ( ( mandel_n < 500 ) \
				   	&& ( mandel_distance ( mandel_x, mandel_y ) < 1000.0 ) ) 
			{
				mandel_compute ( mandel_x, mandel_y, mandel_c_real, \
									 mandel_c_imag, &mandel_x, &mandel_y );
				mandel_n++;
			}

			pixel_color = 765 * mandel_n / 50;
				
			color = (unsigned int)pixel_color;
			pixel_color = (float)( color % 255 ); 

			if ( pixel_color > 500 ) 
			{
				foreground_pixel.red   = ( float )pixel_color/255.0f;
				foreground_pixel.green = ( float )0.0f;
				foreground_pixel.blue  = ( float )pixel_color/255.0f;
			}
			else if ( pixel_color > 175 ) 
			{
				foreground_pixel.red   = ( float )pixel_color/255.0f;
				foreground_pixel.green = ( float )pixel_color/255.0f;
				foreground_pixel.blue  = ( float )0.0f; 
			}
			else if ( pixel_color > 150 ) 
			{
				foreground_pixel.red   = ( float )0.0f;
				foreground_pixel.green = ( float )0.0f;
				foreground_pixel.blue  = ( float )pixel_color/255.0f;
			}
			else if ( pixel_color > 100 ) 
			{
				foreground_pixel.red   = ( float )0.0f;
				foreground_pixel.green = ( float )pixel_color/255.0f;
				foreground_pixel.blue  = ( float )0.0f;
			}
			else 
			{
				foreground_pixel.red   = ( float )pixel_color/255.0f;
				foreground_pixel.green = ( float )0.0f; 
				foreground_pixel.blue  = ( float )0.0f;  
			}

			if ( foreground_pixel.red > 1.0 )
				foreground_pixel.red = 1.0f;
			if ( foreground_pixel.green > 1.0 )
				foreground_pixel.green = 1.0f;
			if ( foreground_pixel.blue > 1.0 )
				foreground_pixel.blue = 1.0f;

			if ( foreground_pixel.red < 0.0 )
				foreground_pixel.red = 0.0f;
			if ( foreground_pixel.green < 0.0 )
				foreground_pixel.green = 0.0f;
			if ( foreground_pixel.blue < 0.0 )
				foreground_pixel.blue = 0.0f;

			*rgba_image_ptr++ = (float)foreground_pixel.red;
			*rgba_image_ptr++ = (float)foreground_pixel.green;
			*rgba_image_ptr++ = (float)foreground_pixel.blue;
			*rgba_image_ptr++ = (float)foreground_pixel.alpha;
		}

		return EXIT_SUCCESS;
}

