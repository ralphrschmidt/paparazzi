/*
 * Copyright (C) C. De Wagter
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
/**
 * @file "modules/computer_vision/opencv_example.cpp"
 * @author C. De Wagter
 * A simple module showing what you can do with opencv on the bebop.
 */


#include "wedgebug_opencv.h"

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc/types_c.h> // needed for CV_MOP_CLOSE
#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <iostream>
#include <stdint.h>

using namespace cv;

// Local declarations
int transfer(const Mat *from, const image_t *to);


// Local functions
int transfer(const Mat *from, const image_t *to)
{
	typedef uint8_t img_dip_type;
	for (int i = 0; i < (from->rows * from->cols); i++)
	{
		((img_dip_type*)to->buf)[i]  = from->at<img_dip_type>(i); // Using ".at" here are accessing buffer is problematic with a cropped image as it maintains a connection to oriinal image
	}
	return 0;
}




// Global functions
int save_image_gray(struct image_t *img, char *myString)
{
	// Create a new image, using the original bebop image.

	if (img->type == IMAGE_OPENCV_DISP )
	{
		Mat M(img->h, img->w, CV_16SC1, img->buf);
		imwrite(myString, M);

	}
	else if (img->type == IMAGE_GRAYSCALE)
	{
		Mat M(img->h, img->w, CV_8UC1, img->buf);
		imwrite(myString, M);
	}
	else
	{
		std::cout << "This function only worked with images of type IMAGE_GRAYSCALE and IAMGE_OPENCV_DISP. Leaving function." << std::endl;
		return -1;
	}



	// Code below is for testing
	/*
	int total = 0;
	int j = 0;

	for (int i = 0; i < (M.cols * M.rows); i++)
	{
		if (i % 10000 == 0)
		{
			std::cout << "Save:i position: " << i << std::endl;
			std::cout << "Save: Entry in position i in new image: " << + M.data[i] << std::endl;
			std::cout << "Save: Entry in position i in old image: " << +((uint8_t*)img)[i] << std::endl;

		}
		j++;
		total = total + M.data[i];
	}


	std::cout << "*img dims: " << height << ":" << width << std::endl;
	std::cout << "M: " << M.rows << ":" << M.cols << std::endl;
	std::cout << "j: " << j << std::endl;
	std::cout << "total: " << total << std::endl;
	std::cout << "Rows/height (240): " << M.size[0] << std::endl;
	std::cout << "Columns/width (480): " << M.size[1] << "\n" << std::endl;
	std::cout << M.at(5,5)[0] << std::endl;
	double minVal;
	double maxVal;
	Point minLoc;
	Point maxLoc;
	minMaxLoc(M ,&minVal, &maxVal, &minLoc, &maxLoc);
	std::cout << "Merged: Min=" << minVal << "; Max=" << maxVal << std::endl;
	 */
  return 0;
}



int save_image_color(void *img, int width, int height,char *myString)
{
	// Create a new image, using the original bebop image.
	Mat M(height, width, CV_8UC2, img); // CV_8UC2 is the openCV format for UYVY images
	// Definition of Mat: Mat::Mat(int _rows, int _cols, int _type, void* _data, size_t _step)
	// Remember that void takes any data type including char
	Mat image;
	cvtColor(M, image, CV_YUV2GRAY_Y422);
	imwrite(myString, image);

	// Code below is for testing
	//double minVal;
	//double maxVal;
	//Point minLoc;
	//Point maxLoc;
	//minMaxLoc(image ,&minVal, &maxVal, &minLoc, &maxLoc);
	//std::cout << "Left1: Min=" << minVal << "; Max=" << maxVal << std::endl;
  return 0;
}



int SBM_OCV(struct image_t *img_disp, const struct image_t *img_left, const struct image_t *img_right,  const int ndisparities, const int SADWindowSize, const bool cropped)
{

	// Defining variables
	Mat img_left_OCV(img_left->h, img_left->w, CV_8UC1, img_left->buf);
	Mat img_right_OCV(img_right->h, img_right->w, CV_8UC1, img_right->buf);
	Mat img_disp_OCV;


	// Block matching
	Ptr<StereoBM> sbm = StereoBM::create(ndisparities, SADWindowSize);
	sbm->compute(img_left_OCV, img_right_OCV, img_disp_OCV); //type of img_disp_OCV is CV_16S i.e. int16_t


	// Determining type of supplied image
	// 1) If image is of type int16_t
	if (img_disp->type == IMAGE_OPENCV_DISP)
	{
		//std::cout << "int16_t" << std::endl;
		typedef int16_t img_dip_type;

		// Cropping or not cropping:
		if (cropped == 0) // If image should not be cropped
		{
			for (int i = 0; i < (img_disp_OCV.rows * img_disp_OCV.cols); i++)
			{
				((img_dip_type*)img_disp->buf)[i]  = img_disp_OCV.at<img_dip_type>(i); // Using ".at" here are accessing buffer is problematic with a cropped image as it maintains a connection to oriinal image
			}
		}
		else if (cropped == 1) // If image should be cropped
		{
			uint16_t rec_y;
			uint16_t rec_height;
			uint16_t rec_x;
			uint16_t rec_width;

			post_disparity_crop_rect(&rec_y, &rec_height,&rec_x, &rec_width, img_left->h, img_left->w,  ndisparities, SADWindowSize); // Function from wedebug.h
			Rect crop_area = Rect(rec_x, rec_y, rec_width, rec_height);
			Mat img_cropped = img_disp_OCV(crop_area);

			for (int i = 0; i < (img_cropped.rows * img_cropped.cols); i++)
			{
				((img_dip_type*)img_disp->buf)[i]  = img_cropped.at<img_dip_type>(i); // Using ".at" here are accessing buffer is problematic with a cropped image as it maintains a connection to oriinal image
			}
		}
		else {return -1;}
	}


	// 2) If image is of type uint8_t
	else if (img_disp->type == IMAGE_GRAYSCALE) // If image is of type uint8_t
	{
		//std::cout << "uint8_t" << std::endl;
		typedef uint8_t img_dip_type;
		img_disp_OCV.convertTo(img_disp_OCV, CV_8UC1);

		// Cropping or not cropping:
		if (cropped == 0) // If image should not be cropped
		{
			for (int i = 0; i < (img_disp_OCV.rows * img_disp_OCV.cols); i++)
				{
					((img_dip_type*)img_disp->buf)[i]  = img_disp_OCV.data[i];
				}
		}
		else if (cropped == 1) // If image should be cropped
		{
			uint16_t rec_y;
			uint16_t rec_height;
			uint16_t rec_x;
			uint16_t rec_width;

			post_disparity_crop_rect(&rec_y, &rec_height,&rec_x, &rec_width, img_left->h, img_left->w,  ndisparities, SADWindowSize); // Function from wedebug.h
			Rect crop_area = Rect(rec_x, rec_y, rec_width, rec_height);
			Mat img_cropped = img_disp_OCV(crop_area);

			for (int i = 0; i < (img_cropped.rows * img_cropped.cols); i++)
			{
				((img_dip_type*)img_disp->buf)[i]  = img_cropped.at<img_dip_type>(i); // Using ".at" here are accessing buffer is problematic with a cropped image as it maintains a connection to oriinal image
			}
		}
		else {return -1;}

	}


	// 3) If image is of unsupported type
	else
	{
		std::cout << "This function only worked with images of type IMAGE_GRAYSCALE and IAMGE_OPENCV_DISP. Leaving function." << std::endl;
		return -1;
	}

	return 0;
}


int opening_OCV(struct image_t *img_input, const struct image_t *img_output, const int SE_size, const int iteration)
{
	Mat img_input_OCV(img_input->h, img_input->w, CV_8UC1, img_input->buf);
	Mat img_output_OCV;
	Mat kernel = getStructuringElement(MORPH_RECT, Size(SE_size, SE_size));

	morphologyEx(img_input_OCV, img_output_OCV, CV_MOP_OPEN, kernel, Point(-1,-1), iteration);
	//erode(img_input_OCV, img_output_OCV, kernel);
	transfer(&img_output_OCV, img_output);

	return 0;
}




int closing_OCV(struct image_t *img_input, const struct image_t *img_output, const int SE_size, const int iteration)
{
	Mat img_input_OCV(img_input->h, img_input->w, CV_8UC1, img_input->buf);
	Mat img_output_OCV;
	Mat kernel = getStructuringElement(MORPH_RECT, Size(SE_size, SE_size));

	morphologyEx(img_input_OCV, img_output_OCV, CV_MOP_CLOSE, kernel, Point(-1,-1), iteration);
	//erode(img_input_OCV, img_output_OCV, kernel);
	transfer(&img_output_OCV, img_output);
	return 0;
}



int dilation_OCV(struct image_t *img_input, const struct image_t *img_output, const int SE_size, const int iteration)
{
	Mat img_input_OCV(img_input->h, img_input->w, CV_8UC1, img_input->buf);
	Mat img_output_OCV;
	Mat kernel = getStructuringElement(MORPH_RECT, Size(SE_size, SE_size));

	dilate(img_input_OCV, img_output_OCV, kernel, Point(-1,-1), iteration);
	//erode(img_input_OCV, img_output_OCV, kernel);
	transfer(&img_output_OCV, img_output);
	return 0;
}



int sobel_OCV(struct image_t *img_input, const struct image_t *img_output, const int kernel_size)
{

	Mat img_input_OCV;
	Mat img_grad_x, img_grad_y, img_grad_mag;
	int ddepth = CV_32F;//CV_16S; // Format of gradient image output (CV_32F needed for normalization function)
	int delta  = 0; // Value added to each gradient pixel
	int scale = 1; // Factor by which gradient pixels is increased


	if (img_input->type == IMAGE_OPENCV_DISP)
	{
		img_input_OCV = Mat(img_input->h, img_input->w, CV_16S, img_input->buf);

	}
	else if (img_input->type == IMAGE_GRAYSCALE)
	{
		img_input_OCV = Mat(img_input->h, img_input->w, CV_8UC1, img_input->buf);
	}
	else
	{
		std::cout << "This function only worked with images of type IMAGE_GRAYSCALE and IAMGE_OPENCV_DISP. Leaving function." << std::endl;
		return -1;
	}


	Sobel(img_input_OCV, img_grad_x, ddepth, 1, 0, kernel_size, scale, delta, BORDER_DEFAULT ); // Horizontal gradient
	Sobel(img_input_OCV, img_grad_y, ddepth, 0, 1, kernel_size, scale, delta, BORDER_DEFAULT ); // Vertical gradient

	magnitude(img_grad_x, img_grad_y, img_grad_mag); // Calculating magnitude

	normalize(img_grad_mag, img_grad_mag,  0, 255, NORM_MINMAX); // Normalizing magnitude between 0 and 255

	img_grad_mag.convertTo(img_grad_mag, CV_8UC1); //Converting image to 8 bit image

	transfer(&img_grad_mag, img_output); // Saving image into output images




	/*
	double minVal;
	double maxVal;
	Point minLoc;
	Point maxLoc;
	minMaxLoc(img_grad_mag ,&minVal, &maxVal, &minLoc, &maxLoc);
	std::cout << "grad_x: Min=" << minVal << "; Max=" << maxVal << std::endl;


	imwrite("/home/dureade/Documents/paparazzi_images/img_grad_mag.bmp", img_grad_mag);
	//imwrite("/home/dureade/Documents/paparazzi_images/abs_grad_y.bmp", abs_grad_y);
	//imwrite("/home/dureade/Documents/paparazzi_images/grad.bmp", grad);
	//imwrite("/home/dureade/Documents/paparazzi_images/img_input_OCV.bmp", img_input_OCV);
	 *
	 */



	return 1;
}



