/*
 * Copyright (C) Ralph Rudi schmidt <ralph.r.schmidt@outlook.com>

 *
 * This file is part of paparazzi
 *
 * paparazzi is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * paparazzi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with paparazzi; see the file COPYING.  If not, see
 * <http://www.gnu.org/licenses/>.

 */
/** @file "modules/wedgebug/wedgebug.h"
 * @author Ralph Rudi schmidt <ralph.r.schmidt@outlook.com>
 * An integration of the WegdeBug algorithm (Laubach 1999) for path finding, for drones with stereo vision.
 */
#include <stdio.h>
#include "modules/wedgebug/wedgebug.h"
#include "modules/wedgebug/wedgebug_opencv.h"
#include "modules/computer_vision/cv.h" // Required for the "cv_add_to_device" function
#include "pthread.h"
#include <stdint.h> // Needed for types like uint8_t
#include <math.h> // Need for sqaure root function for euclidean distance calculation
#include "state.h"


// New section ----------------------------------------------------------------------------------------------------------------
// Defines
#ifndef WEDGEBUG_CAMERA_RIGHT_FPS
#define WEDGEBUG_CAMERA_RIGHT_FPS 0 ///< Default FPS (zero means run at camera fps)
#endif
#ifndef WEDGEBUG_CAMERA_LEFT_FPS
#define WEDGEBUG_CAMERA_LEFT_FPS 0 ///< Default FPS (zero means run at camera fps)
#endif



// New section ----------------------------------------------------------------------------------------------------------------
// Define global variables
struct image_t img_left;
struct image_t img_right;

struct image_t img_YY;
struct image_t img_left_int8;
struct image_t img_right_int8;
struct image_t img_depth_int8;
struct image_t img_depth_int16;
struct image_t img_depth_int8_cropped;
struct image_t img_depth_int16_cropped;
struct image_t img_middle_int8_cropped;
struct image_t img_edges_int8_cropped;

struct crop_t img_cropped_info;

//Drone starting point coordinates
struct NedCoor_f OC;

//static pthread_mutex_t mutex;
int N_disparities = 64;
int block_size_disparities = 25;
int min_disparity = 0;
uint16_t crop_y;
uint16_t crop_height;
uint16_t crop_x;
uint16_t crop_width;

uint8_t cycle_counter = 0;


// New section ----------------------------------------------------------------------------------------------------------------
//Function - Declaration
//Supporting
const char* get_img_type(enum image_type img_type); // Function 1: Displays image type
void show_image_data(struct image_t *img); // Function 2: Displays image data
void show_image_entry(struct image_t *img, int entry_position, const char *img_name); // Function 3: Displays pixel value of image
//Core
static struct image_t *copy_left_img_func(struct image_t *img); // Function 1: Copies left image into a buffer (buf_left)
static struct image_t *copy_right_img_func(struct image_t *img); // Function 2: Copies left image into a buffer (buf_right)
void UYVYs_interlacing_V(struct image_t *YY, struct image_t *left, struct image_t *right); // Function 3: Copies gray pixel values of left and right UYVY images into merged YY image
void UYVYs_interlacing_H(struct image_t *merged, struct image_t *left, struct image_t *right);
void post_disparity_crop_rect(uint16_t* height_start, uint16_t* height_offset, uint16_t* width_start, uint16_t* width_offset, const uint16_t height_old,const uint16_t width_old, const int disp_n, const int block_size);
uint32_t maximum_intensity(struct image_t *img);
void thresholding_img(struct image_t *img, uint8_t threshold);
void principal_points(const struct point_t *c_old, struct point_t *c , struct crop_t *img_cropped_info);
void Ci_to_Cc(struct FloatVect3 *scene_point, int32_t image_point_y, int32_t image_point_x , const uint8_t d, const float b, const uint16_t f);
int32_t indx1d(const int32_t y, const int32_t x, const struct image_t *img_dimensions);



// New section ----------------------------------------------------------------------------------------------------------------
// Function - Definition
// Supporting:
// Function 1
const char* get_img_type(enum image_type img_type)
{
	switch(img_type)
	{
	case IMAGE_YUV422: return "IMAGE_YUV422";
	case IMAGE_GRAYSCALE: return "IMAGE_GRAYSCALE";
	case IMAGE_JPEG: return "IMAGE_JPEG";
	case IMAGE_GRADIENT: return "IMAGE_GRADIENT";
	default: return "Image type not found";
	}
}


// Function 2
void show_image_data(struct image_t *img)
{
	printf("Image-Type: %s\n", get_img_type(img->type));
	printf("Image-Width: %d\n", img->w);
	printf("Image-Height: %d\n", img->h);
	printf("Image-Buf_Size: %d\n", img->buf_size);
	printf("Image-Buf_Memory_Occupied: %lu\n", sizeof(img->buf));
}


// Function 3
void show_image_entry(struct image_t *img, int entry_position, const char *img_name)
{
	printf("Pixel %d value - %s: %d\n", entry_position, img_name ,((uint8_t*)img->buf)[entry_position]);
}

// Function 4


// Core:
// Function 1
static struct image_t *copy_left_img_func(struct image_t *img)
{
	image_copy(img, &img_left);
	//show_image_data(img);
	//show_image_entry(&img_left, 10, "img_left");
	return img;
}


// Function 2
static struct image_t *copy_right_img_func(struct image_t *img)
{
	image_copy(img, &img_right);
	//show_image_data(img);
	//show_image_entry(&img_right, 10, "img_right");
	return img;
}


// Function 3
void UYVYs_interlacing_V(struct image_t *merged, struct image_t *left, struct image_t *right)
{
	// Error messages
	if (left->w != right->w || left->h != right->h)
	{
		printf("The dimensions of the left and right image to not match!");
		return;
	}
	if ((merged->w * merged->h) != (2 * right->w) * right->w)
	{
		printf("The dimensions of the empty image template for merger are not sufficient to merge gray left and right pixel values.");
		return;
	}

	uint8_t *UYVY_left = left->buf;
	uint8_t *UYVY_right = right->buf;
	uint8_t *YY = merged->buf;
	uint32_t loop_length = left->w * right->h;

	// Incrementing pointers to get to first gray value of the UYVY images
	UYVY_left++;
	UYVY_right++;


	for (uint32_t i = 0; i < loop_length; i++)
	{
		*YY = *UYVY_left; // Copies left gray pixel (Y) to the merged image YY, in first position
		YY++; // Moving to second position of merged image YY
		*YY = *UYVY_right; // Copies right gray pixel (Y) to the merged image YY, in second position
		YY++; // Moving to the next position, in preparation to copy left gray pixel (Y) to the merged image YY
		UYVY_left+=2; // Moving pointer to next gray pixel (Y), in the left image
		UYVY_right+=2; // Moving pointer to next gray pixel (Y), in the right image
		/*
		 * Note: Since the loop lenth is based on the UYVY image the size of the data should be (2 x w) x h.
		 * This is also the same size as for the new merged YY image.
		 * Thus incrementing the pointer for UYVY_left and right, in each iteration, does not lead to problems (same for YY image)
		 */
	}
}


// Function 4
void UYVYs_interlacing_H(struct image_t *merged, struct image_t *left, struct image_t *right)
{
	// Error messages
	if (left->w != right->w || left->h != right->h)
	{
		printf("The dimensions of the left and right image to not match!");
		return;
	}
	if ((merged->w * merged->h) != (2 * right->w) * right->w)
	{
		printf("The dimensions of the empty image template for merger are not sufficient to merge gray left and right pixel values.");
		return;
	}

	uint8_t *UYVY_left = left->buf;
	uint8_t *UYVY_right = right->buf;
	uint8_t *YY1 = merged->buf; // points to first row for pixels of left image
	uint8_t *YY2 = YY1 + merged->w; // points to second row for pixels of right image

	// Incrementing pointers to get to first gray value of the UYVY images
	UYVY_left++;
	UYVY_right++;

	for (uint32_t i = 0; i < left->h; i++)
	{
		//printf("Loop 1: %d\n", i);
		for (uint32_t j = 0; j < left->w; j++)
		{
			//printf("Loop 1: %d\n", j);
			*YY1 = *UYVY_left;
			*YY2 = *UYVY_right;
			YY1++;
			YY2++;
			UYVY_left+=2;
			UYVY_right+=2;
		}
		YY1 += merged->w; // Jumping pointer to second next row (i.e. over row with pixels from right image)
		YY2 += merged->w; // Jumping pointer to second next row (i.e. over row with pixels from left image)
	}
}


// Function 5 - Return the upper left coordinates of a square (x and y coordinates) and the offset in terms of width and height,
// given the number of disparity levels and the block size used by the block matching algorithm. This is need to crop an image
void post_disparity_crop_rect(
		uint16_t* height_start,
		uint16_t* height_offset,
		uint16_t* width_start,
		uint16_t* width_offset,
		const uint16_t height_old,
		const uint16_t width_old,
		const int disp_n,
		const int block_size)
{

	uint16_t block_size_black = block_size / 2;
	uint16_t left_black = disp_n + block_size_black;


	*height_start = block_size_black;
	*height_offset = height_old - block_size_black;
	*height_offset = *height_offset - *height_start;

	*width_start = left_black - 1;
	*width_offset = width_old - block_size_black;
	*width_offset = *width_offset - *width_start;
}


// Function 6 - Returns the maximum value in a uint8_t image
uint32_t maximum_intensity(struct image_t *img)
{
	uint32_t max = 0;
	for (uint32_t i = 0; i < img->buf_size; i++)
	{
		uint8_t* intensity = &((uint8_t*)img->buf)[i];

		if (*intensity > max)
		{
			max = *intensity;
		}
	}
	return max;
}


// Function 7 - Thresholds 8bit images given and turns all values >= threshold to 255
void thresholding_img(struct image_t *img, uint8_t threshold)
{
	for (uint32_t i = 0; i < img->buf_size; i++)
	{
		uint8_t* intensity = &((uint8_t*)img->buf)[i];

		if (*intensity >= threshold)
		{
			*intensity = 127;
		}
		else
			*intensity = 0;

	}
}


// Function 8 - Calculates principal point coordinates for a cropped image, based on the x
// and y coordinates of the cropped area (upper left-hand side: crop_y and crop_x).
void principal_points(const struct point_t *c_old, struct point_t *c , struct crop_t *img_cropped_info)
{
	c->y = c_old->y - img_cropped_info->y;
	c->x = c_old->x - img_cropped_info->x;
}


// Function 9 - Calculates 3d points in a scene based on the 2d coordinates of the point in the
// image plane and the depth.
void Ci_to_Cc(struct FloatVect3 *scene_point, int32_t image_point_y, int32_t image_point_x , const uint8_t d, const float b, const uint16_t f)
{
	// Calculating Z
	// In case disparity is 0 Z will be very very small to avoid detection of algorithm that
	// calculates closest edge point to target goal
	//printf("y=%d\n", image_point_y);
	//printf("x=%d\n", image_point_x);
	//printf("d=%d\n", d);


	if (d==0)
	{
		scene_point->z = 0.0001;
	}
	else
	{
		scene_point->z = b * f / d;
	}


	//printf("Z=%f\n", scene_point->Z);

	// Calculating Y
	scene_point->y = image_point_y * scene_point -> z / f;

	// Calculating X
	scene_point->x = image_point_x * scene_point -> z / f;
	//printf("Y (y=%d) =  %f\n", image_point->y, scene_point->Y);
	//printf("X (x=%d) =  %f\n", image_point->x, scene_point->X);
	//printf("Z (d=%d) =  %f\n", d, scene_point->Z);

}

// Function 10 - Converts 2d coordinates into 1d coordinates (for 1d arrays)
int32_t indx1d(const int32_t y, const int32_t x, const struct image_t *img_dimensions)
{

	if (x >= (img_dimensions->w) || x < 0)
	{
		printf("Error: index %d is out of bounds for axis 0 with size %d. Returning -1\n", x, img_dimensions->w);
		return -1;
	}
	else if (y >= (img_dimensions->h) || y < 0)
	{
		printf("Error: index %d is out of bounds for axis 0 with size %d. Returning -1\n", y, img_dimensions->h);
		return -1;
	}
	else
	{
		return x + img_dimensions->w * y;
	}
}







// Function xx
void Vw_to_Va(struct FloatVect3 *Va, struct FloatVect3 *Vw, struct FloatRMat *Raw, struct NedCoor_f *VAw)
{
	// The following translates world vector coordinates into the agent coordinate system
	Vw->x = Vw->x - VAw->x;
	Vw->y = Vw->y - VAw->y;
	Vw->z = Vw->z - VAw->z;

	// In case the axes of the world coordinate system (w) and the agent coordinate system (a) do not
	// coincide, they are adjusted with the rotation matrix R
	float_rmat_vmult(Va, Raw, Vw);
}


// Function xx
void Va_to_Vw(struct FloatVect3 *Vw, struct FloatVect3 *Va, struct FloatRMat *Raw, struct NedCoor_f *VAw)
{
	// In case the axes of the agent coordinate system (a) and the world coordinate system (w) do not
	// coincide, they are adjusted with the inverse rotation matrix R
	float_rmat_transp_vmult(Vw, Raw, Va);

	// The following translates agent vector coordinates into the world coordinate system
	Vw->x = Vw->x + VAw->x;
	Vw->y = Vw->y + VAw->y;
	Vw->z = Vw->z + VAw->z;

}





// Function xx
void Va_to_Vc(struct FloatVect3 *Vc, struct FloatVect3 *Va, struct FloatRMat *Rca, struct FloatVect3 *VCa)
{
	// The following translates world vector coordinates into the agent coordinate system
	Va->x = Va->x - VCa->x;
	Va->y = Va->y - VCa->y;
	Va->z = Va->z - VCa->z;

	// In case the axes of the world coordinate system (w) and the agent coordinate system (a) do not
	// coincide, they are adjusted with the rotation matrix R
	float_rmat_vmult(Vc, Rca, Va);
}


// Function xx
void Vc_to_Va(struct FloatVect3 *Va, struct FloatVect3 *Vc, struct FloatRMat *Rca, struct FloatVect3 *VCa)
{
	// In case the axes of the agent coordinate system (a) and the world coordinate system (w) do not
	// coincide, they are adjusted with the inverse rotation matrix R
	float_rmat_transp_vmult(Va, Rca, Vc);

	// The following translates agent vector coordinates into the world coordinate system
	Va->x = Va->x + VCa->x;
	Va->y = Va->y + VCa->y;
	Va->z = Va->z + VCa->z;

}





/*
 * w: World coordinate system (i.e. the world coordinate frame = x is depth, y is left and right, and z is altitude))
 * a: Agent coordinate system (i.e. the agent coordinate frame = x is depth, y is left and right, and z is altitude))
 * c: Camera Coordinate system (i.e. the camera coordinate frame = x is left and right, y is altitude and z is depth)
 * i: Image coordinate system  (i.e. the image (plane) coordinate frame)
 * Vw: Vector coordinates, in the world coordinate system (i.e. a point in the world coordinate system)
 * Va: Vector coordinates, in the agent coordinate system (i.e. a point in the world coordinate system)
 * Vc: Vector coordinates, in the camera coordinate system (i.e. a point in the world coordinate system)
 * Vi: Vector coordinates, in the image coordinate system (i.e. a point in the image [plane] coordinates system)
 * R: Rotation matrix
 * Raw: Rotation matrix of world coordinate system expressed in the agent coordinate system
 * Rca: Rotation matrix of agent coordinate system expressed in the camera coordinate system
 * CSned: Coordinate system - NED (i.e. the NED coordinate system = x is depth, y is left and right, and z is altitude)
 * CSc: Coordinate system - Camera (i.e. the Camera coordinate system = x is left and right, y is altitude and z is depth)
 * VAw: Vector coordinates of agent in the world coordinate system
 * VCa: Vector coordinates of camera in the agent coordinate system
 */




// New section ----------------------------------------------------------------------------------------------------------------
void wedgebug_init(){
	//printf("Wedgebug init function was called\n");

	// Creating empty images
	image_create(&img_left, WEDGEBUG_CAMERA_LEFT_WIDTH, WEDGEBUG_CAMERA_LEFT_HEIGHT, IMAGE_YUV422); // To store left camera image
	image_create(&img_right,WEDGEBUG_CAMERA_RIGHT_WIDTH, WEDGEBUG_CAMERA_RIGHT_HEIGHT, IMAGE_YUV422);// To store right camera image
	image_create(&img_YY,WEDGEBUG_CAMERA_INTERLACED_WIDTH, WEDGEBUG_CAMERA_INTERLACED_HEIGHT, IMAGE_GRAYSCALE);// To store interlaced image
	image_create(&img_left_int8, WEDGEBUG_CAMERA_LEFT_WIDTH, WEDGEBUG_CAMERA_LEFT_HEIGHT, IMAGE_GRAYSCALE); // To store gray scale version of left image
	image_create(&img_right_int8, WEDGEBUG_CAMERA_RIGHT_WIDTH, WEDGEBUG_CAMERA_RIGHT_HEIGHT, IMAGE_GRAYSCALE); // To store gray scale version of left image
	image_create(&img_depth_int8,WEDGEBUG_CAMERA_DISPARITY_WIDTH, WEDGEBUG_CAMERA_DISPARITY_HEIGHT, IMAGE_GRAYSCALE);// To store depth - 8bit
	image_create(&img_depth_int16,WEDGEBUG_CAMERA_DISPARITY_WIDTH, WEDGEBUG_CAMERA_DISPARITY_HEIGHT, IMAGE_OPENCV_DISP);// To store depth - 16bit



	post_disparity_crop_rect(&crop_y, &crop_height, &crop_x, &crop_width, img_right.h , img_right.w, N_disparities, block_size_disparities);

	img_cropped_info.x = crop_x;
	img_cropped_info.y = crop_y;
	img_cropped_info.w = crop_width;
	img_cropped_info.h = crop_height;


	image_create(&img_depth_int8_cropped,crop_width, crop_height, IMAGE_GRAYSCALE);// To store cropped depth - 8 bit
	image_create(&img_depth_int16_cropped, crop_width, crop_height, IMAGE_OPENCV_DISP);// To store cropped depth - 16 bit
	image_create(&img_middle_int8_cropped,crop_width, crop_height, IMAGE_GRAYSCALE);// To store intermediate image data from processing - 8 bit
	image_create(&img_edges_int8_cropped,crop_width, crop_height, IMAGE_GRAYSCALE);// To store edges image data from processing - 8 bit


	// Adding callback functions
	cv_add_to_device(&WEDGEBUG_CAMERA_LEFT, copy_left_img_func, WEDGEBUG_CAMERA_LEFT_FPS);
	cv_add_to_device(&WEDGEBUG_CAMERA_RIGHT, copy_right_img_func, WEDGEBUG_CAMERA_RIGHT_FPS);



}

void wedgebug_periodic(){
  // your periodic code here.
  // freq = 4.0 Hz
	//printf("Wedgebug periodic function was called\n");


	// No imaes are capturin during the first call of the periodic function
	// So all processing must happen after the first cycle

	if(cycle_counter == 1)
	{

		// Obtaining original coordinates (starting coordinates)
		OC.x = stateGetPositionNed_f()->y;
		OC.y = stateGetPositionNed_f()->z;
		OC.z = stateGetPositionNed_f()->x;


		OC.x = stateGetPositionNed_f()->x;
		OC.y = stateGetPositionNed_f()->y;
		OC.z = stateGetPositionNed_f()->z;

	}

	if (cycle_counter != 0)
	{

		//UYVYs_interlacing_V(&img_YY, &img_left, &img_right); // Creating YlYr image from left and right YUV422 image
		//UYVYs_interlacing_H(&img_YY, &img_left, &img_right);
		image_to_grayscale(&img_left, &img_left_int8); // Converting left image from UYVY to gray scale for saving function
		image_to_grayscale(&img_right, &img_right_int8); // Converting right image from UYVY to gray scale for saving function


		//SBM_OCV(&img_depth_int8, &img_left_int8, &img_right_int8, N_disparities, block_size_disparities, 0);// Creating cropped disparity map image
		//SBM_OCV(&img_depth_int16, &img_left_int8, &img_right_int8, N_disparities, block_size_disparities, 0);// Creating cropped disparity map image

		SBM_OCV(&img_depth_int8_cropped, &img_left_int8, &img_right_int8, N_disparities, block_size_disparities, 1);// Creating cropped disparity map image
		//SBM_OCV(&img_depth_int16_cropped, &img_left_int8, &img_right_int8, N_disparities, block_size_disparities, 1);// Creating cropped disparity map image

		// Morphological operations 1
		// Neeeded to smoove object boundaries and to remove noise removing noise
		opening_OCV(&img_depth_int8_cropped, &img_middle_int8_cropped,13, 1);
		closing_OCV(&img_middle_int8_cropped, &img_middle_int8_cropped,13, 1);
		dilation_OCV(&img_middle_int8_cropped, &img_middle_int8_cropped,11, 1);

		// Edge detection
		sobel_OCV(&img_middle_int8_cropped, &img_edges_int8_cropped, 5);

		// Morphological  operations 2
		// This is needed so that when using the edges as filters (to work on disparity values
		// only found on edges) the underlying disparity values are those of the foreground
		// and not the background
		dilation_OCV(&img_middle_int8_cropped, &img_middle_int8_cropped,11, 1);

		// Thresholding edges
		uint8_t threshold = (maximum_intensity(&img_edges_int8_cropped) / 100.00 * 30);
		//printf("Threshold = %d\n", threshold);
		thresholding_img(&img_edges_int8_cropped, threshold);

		// Calculating PP -
		struct point_t c_old;
		struct point_t c;

		c_old.y = img_left_int8.h / 2;
		c_old.x = img_left_int8.w / 2;
		principal_points(&c_old, &c, &img_cropped_info); // Calculates principal points for cropped image, considerring the original dimensions







		struct FloatVect3 Vw;
		Vw.x = 1;
		Vw.y = 2;
		Vw.z = 3;

		struct FloatRMat *Raw = stateGetNedToBodyRMat_f();
		struct NedCoor_f *VAw = stateGetPositionNed_f();
		struct FloatVect3 Va;




		struct FloatRMat Rca;
		Rca.m[0] = 0; Rca.m[1] = 1;	Rca.m[2] = 0;
		Rca.m[3] = 0; Rca.m[4] = 0; Rca.m[5] = 1;
		Rca.m[6] = 1; Rca.m[7] = 0; Rca.m[8] = 0;

		struct FloatVect3 VCa;
		VCa.x = 0;
		VCa.y = 0;
		VCa.z = 0;
		struct FloatVect3 Vc;





		// Rotation matrix
		printf("Raw\n");
		printf("%f, %f, %f,\n", Raw->m[0],Raw->m[1], Raw->m[2]);
		printf("%f, %f, %f,\n", Raw->m[3],Raw->m[4], Raw->m[5]);
		printf("%f, %f, %f\n\n", Raw->m[6],Raw->m[7], Raw->m[8]);

		// Translation vector
		printf("VAw (drone location)\n");
		printf(" %f\n %f\n %f\n\n", VAw->x, VAw->y, VAw->z);

		// World coordinates
		printf("Vw\n");
		printf(" %f\n %f\n %f\n\n", Vw.x, Vw.y, Vw.z);

		// Agent coordinates from world coordinates
		Vw_to_Va(&Va, &Vw, Raw, VAw);

		// Agent coordinates
		printf("Va\n");
		printf(" %f\n %f\n %f\n\n", Va.x, Va.y, Va.z);




		// Rotation matrix
		printf("Rca\n");
		printf("%f, %f, %f,\n", Rca.m[0],Rca.m[1], Rca.m[2]);
		printf("%f, %f, %f,\n", Rca.m[3],Rca.m[4], Rca.m[5]);
		printf("%f, %f, %f\n\n", Rca.m[6],Rca.m[7], Rca.m[8]);

		// Translation vector
		printf("VCa (drone location)\n");
		printf(" %f\n %f\n %f\n\n", VCa.x, VCa.y, VCa.z);







		// Camera coordinates from agent coordinates
		Va_to_Vc(&Vc, &Va, &Rca, &VCa);


		// Camera coordinates
		printf("Vc\n");
		printf(" %f\n %f\n %f\n\n", Vc.x, Vc.y, Vc.z);



		// Agent coordinates from camera coordinates
		Vc_to_Va(&Va, &Vc, &Rca, &VCa);

		// Back to agent coordinates
		printf("Va2\n");
		printf(" %f\n %f\n %f\n\n", Va.x, Va.y, Va.z);


		// World coordinates from a coordinates
		Va_to_Vw(&Vw, &Va, Raw, VAw);


		// Back to world coordinates
		printf("Vw2\n");
		printf(" %f\n %f\n %f\n\n", Vw.x, Vw.y, Vw.z);

		printf("----------------------------\n");








		//void float_rmat_transp_vmult(struct FloatVect3 *vb, struct FloatRMat *m_b2a, struct FloatVect3 *va)
		//float_rmat_transp_vmult(struct FloatVect3 *vb, struct FloatRMat *m_b2a, struct FloatVect3 *va)














		struct FloatVect3 target_point;
		target_point.y = -0.251803;
		target_point.x = 0.0671475;
		target_point.z = 6;

		// Loop to calculate position (in image) of point closes to hypothetical target - Start
		float b = WEDGEBUG_CAMERA_BASELINE / 1000.00;
		uint16_t f = WEDGEBUG_CAMERA_FOCAL_LENGTH;
		float distance = 255;
		struct FloatVect3 scene_point;
		struct FloatVect3 target_scene_diff;
		struct FloatVect2 closest_edge;


		for (uint32_t y = 0; y < img_depth_int8_cropped.h; y++)
		{
			for (uint32_t x = 0; x < img_depth_int8_cropped.w; x++)
			{

				int32_t indx = indx1d(y, x, &img_depth_int8_cropped);
				uint8_t edge_value = ((uint8_t*) img_edges_int8_cropped.buf)[indx];

				// We only look for good edge points if the current point coincides with edges (from img_edges)
				// i.e. where the value of the image is not zero (as all non-edges have been set to 0)
				if (edge_value != 0)
				{
					// We determine the offset from the principle point
					int32_t y_from_c = y - c.y;
					int32_t x_from_c = x - c.x;
					// We save disparity and derive the 3d scene point using it
					uint8_t disparity = ((uint8_t*) img_middle_int8_cropped.buf)[indx];
					//Ci_to_Cc(&scene_point, y_from_c, x_from_c, disparity, b, f);
					Ci_to_Cc(&scene_point, y_from_c, x_from_c, disparity, b, f);

					// Calculating distance vector (needed to see which point is closest to goal)
					target_scene_diff.x = target_point.x - scene_point.x;
					target_scene_diff.y = target_point.y - scene_point.y;
					target_scene_diff.z = target_point.z - scene_point.z;

					// If current distance (using distance vector) is smaller than the previous minimum distanc
					// measure then save new distance and point coordinates associated with it
					if (((float)sqrt(pow(target_scene_diff.x ,2) + pow(target_scene_diff.y ,2)+ pow(target_scene_diff.z ,2))) < distance)
					{
						distance = (float)sqrt(pow(target_scene_diff.x ,2) + pow(target_scene_diff.y ,2)+ pow(target_scene_diff.z ,2));
						closest_edge.y = y;
						closest_edge.x = x;
					}


					//printf("x image = %d\n", x);
					//printf("y image = %d\n", y);
					//printf("x image from c = %d\n", x_from_c);
					//printf("y image from c = %d\n", y_from_c);
					//printf("d  = %d\n", disparity);
					//printf("X scene from c = %f\n", scene_point.X);
					//printf("Y scene from c = %f\n", scene_point.Y);
					//printf("Z scene from c = %f\n", scene_point.Z);

					//printf("Closest edge [y,x] = [%d, %d]\n", (int)closest_edge.y, (int)closest_edge.x);
					//printf("Distance to goal (m) = %f\n\n", distance);
				}
			}
		}

		((uint8_t*) img_edges_int8_cropped.buf)[indx1d(closest_edge.y, closest_edge.x, &img_edges_int8_cropped)] = 255;
		//printf("Closest edge [y,x] = [%d, %d]\n", closest_edge.y, closest_edge.x);
	    //printf("Distance to goal (m) = %f\n\n", distance);
	    // Loop to calculate position (in image) of point closes to hypothetical target - End




		//printf("Width/height = %d / %d\n", img_depth_int8_cropped.w, img_depth_int8_cropped.h);
		//printf("Position of y coordinate %d and x coordinate %d in 1d array: %d\n", position_2d.y, position_2d.x, position_1d);



		/*
		uint16_t x = 0;
		float depth;

		if (x==0)
		{
			depth = 0.0001;
		}
		else
		{
			depth = b * f / x;
		}
		printf("depth of 64 disparity: %f\n", depth);


		*/

		//save_image_gray(&img_YY, "/home/dureade/Documents/paparazzi_images/img_YY.bmp");
		save_image_gray(&img_left_int8, "/home/dureade/Documents/paparazzi_images/img_left_int8.bmp");
		save_image_gray(&img_right_int8, "/home/dureade/Documents/paparazzi_images/img_right_int8.bmp");

		//save_image_gray(&img_depth_int8, "/home/dureade/Documents/paparazzi_images/img_depth_int8.bmp");
		//save_image_gray(&img_depth_int16, "/home/dureade/Documents/paparazzi_images/img_depth_int16.bmp");

		save_image_gray(&img_depth_int8_cropped, "/home/dureade/Documents/paparazzi_images/img_depth_int8_cropped.bmp");
		//save_image_gray(&img_depth_int16_cropped, "/home/dureade/Documents/paparazzi_images/img_depth_int16_cropped.bmp");

		save_image_gray(&img_middle_int8_cropped, "/home/dureade/Documents/paparazzi_images/img_middle_int8_cropped.bmp");

		save_image_gray(&img_edges_int8_cropped, "/home/dureade/Documents/paparazzi_images/img_edges_int8_cropped.bmp");


		/*
		//save_image_gray(&img_YY, "/home/dureade/Documents/paparazzi_images/img_YY.bmp");
		save_image_gray(&img_left_int8, "/home/dureade/Documents/paparazzi_images/img_left_int8.bmp");
		save_image_gray(&img_right_int8, "/home/dureade/Documents/paparazzi_images/img_right_int8.bmp");

		//save_image_gray(&img_depth_int8, "/home/dureade/Documents/paparazzi_images/img_depth_int8.bmp");
		//save_image_gray(&img_depth_int16, "/home/dureade/Documents/paparazzi_images/img_depth_int16.bmp");

		save_image_gray(&img_depth_int8_cropped, "/home/dureade/Documents/paparazzi_images/img_depth_int8_cropped.bmp");
		//save_image_gray(&img_depth_int16_cropped, "/home/dureade/Documents/paparazzi_images/img_depth_int16_cropped.bmp");

		save_image_gray(&img_middle_int8_cropped, "/home/dureade/Documents/paparazzi_images/img_middle_int8_cropped.bmp");

		save_image_gray(&img_edges_int8_cropped, "/home/dureade/Documents/paparazzi_images/img_edges_int8_cropped.bmp");

		*/





		/*

			uint8_t min_8, max_8;
		int16_t min_16, max_16;

		for (int i = 0; i < (img_depth_int8.h * img_depth_int16.h); i++)
		{
			if  (i==0)
			{
				 min_8 = ((uint8_t*)img_depth_int8.buf)[i];
				 max_8 = ((uint8_t*)img_depth_int8.buf)[i];
				 min_16 = ((int16_t*)img_depth_int16.buf)[i];
				 max_16 = ((int16_t*)img_depth_int16.buf)[i];



				//max_8 = ((uint8_t)img_depth_int8).buf)[i];
				//min_16 = ((int16_t)img_depth_int8.buf)[i];
				//max_16 = ((int16_t)img_depth_int8.buf)[i];
				printf("Stuff called: %d\n", img_depth_int8.buf_size);
			}
			else
			{
				if (min_8 > ((uint8_t*)img_depth_int8.buf)[i]){min_8 = ((uint8_t*)img_depth_int8.buf)[i];;}
				if (max_8 < ((uint8_t*)img_depth_int8.buf)[i]){max_8 = ((uint8_t*)img_depth_int8.buf)[i];;}

				if (min_16 > ((int16_t*)img_depth_int16.buf)[i]){min_16 = ((int16_t*)img_depth_int16.buf)[i];;}
				if (max_16 < ((int16_t*)img_depth_int16.buf)[i]){max_16 = ((int16_t*)img_depth_int16.buf)[i];;}
			}

		}
		printf("8bit depth - min:max: %d:%d\n", min_8, max_8);
		printf("16bit depth - min:max: %d:%d\n", min_16, max_16);


		int i = 20000;

		printf("index 0 value of img_depth_int16: %f\n", ((int16_t*)img_depth_int16.buf)[0]);
		printf("index %d value of img_depth_int16: %d\n",i ,((int16_t*)img_depth_int16.buf)[i]);
		printf("index %d value of img_depth_int16: %f\n",i ,((int16_t*)img_depth_int16.buf)[i]);


		for (int i = 0; i < (img_depth2.w * img_depth2.h); i++)
		{
			if (i % 20000 == 0)
			{
				printf("New image (after C++) position %d: %d\n",i, ((uint8_t*)img_depth2.buf)[i]);

			}
		}*/


		//printf("Buf size  (bytes) of img_depth_uint16 (should be 66096): %d\n", img_depth_uint16.buf_size);
		//post_disparity_dims(&new_height_min, &new_height_max, &new_width_min, &new_width_max, 240 , 240, N_disparities, block_size_disparities);
		//post_disparity_dims2(&new_height, &new_width, 240 , 240, N_disparities, block_size_disparities);
		//post_disparity_crop_rect(&new_height_min, &new_height_max, &new_width_min, &new_width_max, 240 , 240, N_disparities, block_size_disparities);
		//printf("img_depth2 h:w = %d:%d\n", img_depth2.h, img_depth2.w);
		//printf("\n\n\n\n\n");
		//printf("height min:max = %d:%d\n width min:max = %d:%d\n", new_height_min, new_height_max, new_width_min, new_width_max);
		//printf("cropped image dims: [%d, %d]\n", new_height_max, new_width_max);
		//printf("cropped image dims2: [%d, %d]", new_height, new_width);
		//printf("height min:offset = %d:%d\n width min:offset = %d:%d\n", new_height_min, new_height_max, new_width_min, new_width_max);
		//printf("img_YY.buf_size: %d\n", img_YY.buf_size);
		//printf("img_YY.buf_size: %d\n", img_YY.h);
		//printf("img_YY.buf_size: %d\n", img_YY.w);
		//printf("img_YY.buf_size: %d\n", img_left.buf_size);
		//printf("img_YY_left.buf_size: %d", img_YY_left.buf_size);
		//show_image_entry(&img_YY_right, 0 , "img_YY_right");
		//show_image_entry(&img_right, 1 , "img_right");
		//show_image_entry(&img_YY_left, 0 , "img_YY_left");
		//show_image_entry(&img_left, 1 , "img_left");
		//printf("\n");

	}

	// Since the counter is not infinite it is only increased in the first 10 cycles
	if (cycle_counter < 10){cycle_counter++;}
}


