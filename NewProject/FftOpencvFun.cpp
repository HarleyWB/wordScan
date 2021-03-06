#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "FftOpencvFun.h"

#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;

extern "C" {

	#define LONG long

	#define IMAGE_WIDTH  256
	#define IMAGE_HEIGHT 256
	#define MAX_WM_LEX_LEN 6
	#define MAX_WM_LEN 8
	#define MIN_ABS_VALUE 100

	#define MIN_IMAGE_DIFFERENCE  100    //∂®“Â◊Ó–°µƒÕºœÒ≤Ó“Ï 100
	int     g_Image4Difference;
	int	    g_DC_Value;

	int     g_Img_Width, g_Img_Height;   //÷˜µ˜∫Ø ˝¥´»ÎµƒÕºœÒøÌ∂»∫Õ∏ﬂ∂»

	//==============================================================//
	//¥À¥¶¥Ê∑≈ºÏ≤‚≥ˆµƒ4øÈ ˝æ›£¨Œ™µ˜ ‘”√£¨Ω´‘≠¿¥µƒæ÷≤ø±‰¡ø–ﬁ∏ƒŒ™»´æ÷±‰¡ø
	//2014-6-22£¨◊®¿˚æ÷∞Ê±æ£¨∏ƒŒ™6øÈ£¨jflu
	//==============================================================//

	#define WM_TOTAL_BLOCKS 4
	#define WM_MAX_CHARS_PER_BLOCKS 20

	int     g_SO_buf[WM_TOTAL_BLOCKS][WM_MAX_CHARS_PER_BLOCKS] = {0};

	//#define DEBUG_ON

	//#define _SAVE_TEST_DATA

	//global filter to avoid some special image
	bool   invalidFlag;
	bool   g_Finished_One_Loop;  //512ÕºœÒ∑÷≥…256¿¥ºÏ≤‚£¨µ±ºÏ≤‚“ª¬÷£®4∏ˆ£©ÕÍ≥…∫Û£¨Œ™’Ê


	int    fftTransform(IplImage* pSource,  double* pRe,  double* pIm);
	int    maxindex(double *A, int col1, int col2);
	int    cvShiftDFT(CvArr* src_arr,CvArr* dst_arr);
	void   mir_inverse(double *A, int width, int height);
	void   GetABSPart(double *imageRe, double * InRe, double* InIm);
	void   GetABSPart2(double *imageRe, complex<double> * inArr);
	void   GetAnglePart(double *imageIm, double * InRe, double* InIm);
	void   ConvertFromWatermark(int * watermark, char * pOutWM);
	double maxvalue(double *A, int row1, int row2, int col1, int col2);
	double biLinInterp(double *imageRe, double x, double y, int width, int height);
	void   Detectmark(int * watermark, double * polarMap, int width, int height);

	void   fftShift(double *image, int width, int height);
	void   inverse(double *image, int width, int height);
	void   FFT(complex<double> * TD, complex<double> * FD, int r);
	bool   DFT2(complex<double>* TD2,complex<double>* FD2, LONG lWidth, LONG lHeight);
	bool   DIBDFT(double* lpDIBBits,complex<double>* FD2, LONG lWidth, LONG lHeight);

	void   imgV2H(const IplImage* src, IplImage* dst);
	int    calculateSimiliar(IplImage* img512);

	//Ω´Image∏Ò Ωƒ⁄»›±£¥ÊŒ™Œƒ±æ∏Ò Ω
	void   saveIplImage(IplImage* pSource, char* fileName);

	//Ω´RGBAÀƒÕ®µ¿ÕºœÒ±£¥ÊŒ™RGB»˝Õ®µ¿∏Ò ΩÕºœÒ
	void   saveRGBImage(IplImage* resizeimg, int Numbers, int bSaveImg);

	//±£¥Ê ˝◊È–≈œ¢µΩŒƒ±æŒƒº˛
	void   saveArray(double* pSource, char* fileName);

	//ºÏ≤‚256X256ÕºœÒ÷–µƒÀÆ”°–≈œ¢
	void   detectFrom256Image(IplImage* img, int* obuf);
	void   flipImage(IplImage* pSource);

	//Õ≥º∆Ω·π˚
	void   calculateResult(const int so_buf[WM_TOTAL_BLOCKS][WM_MAX_CHARS_PER_BLOCKS],
						   int* maxval, int* maxcount);
	//¥Ú”°≥ˆºÏ≤‚µ„∏ΩΩ¸∆µ∆◊÷µ
	void   printDetectmark(int * watermark, double * polarMap, int width, int height);

	const static int scanOrderTable[4][8] = {
			{0,1,2,3,4,5,6,7},      //◊Û…œ–Ú¡–
			{2,3,4,5,6,7,0,1},      //”“…œ–Ú¡–
			{4,5,6,7,0,1,2,3},      //◊Ûœ¬–Ú¡–
			{6,7,0,1,2,3,4,5}       //”“œ¬–Ú¡–
	};
    
 

	//=====================================================================//
	//C∫Ø ˝ µœ÷
	//java ÷˜µ˜∫Ø ˝»Îø⁄
	//
	//jflu, 2014-1-12
	//=====================================================================//
    bool FftOpencvFun(
					   char* buf,
					   int* pout,
					   int w, int h,
					   int bSaveImg)
    {

			static int Numbers = 0;

			jint *cbuf;
			jint *obuf;
			//int so_buf[4][20] = {0}; //Œ™µ˜ ‘”√£¨∏ƒ≥…»´æ÷±‰¡ø

			cbuf = env->GetIntArrayElements(buf, false);
			obuf = env->GetIntArrayElements(pout, false);

			bool flag = false;
			if(cbuf == NULL)
			{
				return 0;
			}

			long i,j;
			//¥¥Ω® ‰»ÎµƒÕºœÒ£¨≥ﬂ¥Áø…ƒ‹–°”⁄256X256
			IplImage* resizeimg = cvCreateImageHeader(cvSize(w,h),IPL_DEPTH_8U,4);                //------------1

			cvSetData(resizeimg,cbuf,resizeimg->widthStep);

			g_Img_Height  = w == (int)(w / 256) * 256 ? w : (int)(w / 256) * 256 + 256;
			g_Img_Width   = h == (int)(h / 256) * 256 ? h : (int)(h / 256) * 256 + 256;

			//¥¥Ω®512X512ÕºœÒ
			IplImage* img512 = cvCreateImage(cvSize(g_Img_Width, g_Img_Height),IPL_DEPTH_8U,4);                //------------1
			//¥¥Ω®256X256ÕºœÒ
			IplImage* img = cvCreateImage(cvSize(IMAGE_WIDTH,IMAGE_HEIGHT),IPL_DEPTH_8U,4);                //------------1

			int nchannels  = img->nChannels;
			int step       = img->widthStep;
			int depth      = img->depth;
			CvSize  cvsize = cvGetSize(img);

			//”…”⁄ÕºœÒ–˝◊™90£¨Ω√’˝
			imgV2H(resizeimg, img512);

			//∑≈¥ÛÕºœÒ--°∑512X512
//			if ( (w != g_Img_Width) || (h != g_Img_Height) )
//			{
//				cvResize(resizeimg,  img512, CV_INTER_CUBIC);//
//			}
//			else
//			{
//				cvCopy(resizeimg, img512, NULL);
//			}

			//cvSaveImage("/sdcard/w512.jpg",img512);
			{
				//save image
				char _file_[250];
				time_t t;
		    	struct tm *p;

		    	t = time(NULL);
		    	p = gmtime(&t);

		    	sprintf(_file_, "/sdcard/WM_%02d%02d_%02d%02d%02d.jpg",
							1+p->tm_mon, p->tm_mday,
        		            p->tm_hour + 8, p->tm_min, p->tm_sec);

		    	//cvSaveImage(_file_,img512);

			}

			// «∑Ò±£¥ÊÕºœÒ
			saveRGBImage(resizeimg, Numbers, bSaveImg);

			g_Image4Difference = calculateSimiliar(img512);

			//∑÷∏Ó512-°∑4∏ˆ256ÕºœÒ
			//µ˜”√∫Ø ˝DetectFrom256Image
			int _i = 0, _j = 0, _c = 0;
			char _file[100] = {0};

			//—≠ª∑ºÏ≤‚512ÕºœÒ
			g_Finished_One_Loop = false;

			for ( _i = 0; _i < g_Img_Width / 256; _i++)
				for ( _j = 0; _j < g_Img_Height / 256; _j++)   //jflu,2014-6-22 ∏ƒ 2-°∑3£¨‘≠¿¥512X512£¨œ÷‘⁄512X768£¨—≠ª∑6¥Œ
				{
					 cvSetImageROI(img512,cvRect(_i*256, _j*256, 256, 256));//…Ë÷√‘¥ÕºœÒROI
					 cvCopy(img512,img); //∏¥÷∆ÕºœÒ

					 //π≤4¥Œµ˜”√256ÕºœÒºÏ≤‚£¨ªÒµ√µƒ8Œª ˝◊÷∑≈‘⁄¥”µ⁄“ªŒªø™ º¡¨–¯8∏ˆ ˝◊ÈŒª
					 //    so_bufŒ™4X20µƒ∂˛Œ¨ ˝◊È
					 //if (_i == 1 && _j == 1)
					 //	 g_Finished_One_Loop = true;

					 detectFrom256Image(img, g_SO_buf[_c++]);//obuf+(_c++)*11);//so_buf[_c++]);
				}

//#define _SAVE_DEBUG_IMAGE_
#ifdef _SAVE_DEBUG_IMAGE_

			{

				char _file_[255] = {0};
				char _char_[10]  = {0};
				char _all_[10]   = {0};

			// ----------- “‘œ¬Œ™µ˜ ‘±£¥ÊÕºœÒ”√£¨∑¢≤º ±ø…»°œ˚ ---------------------------//
			 //Õ≥º∆Ω·π˚
			int maxval_[20]   = {0};
			int maxcount_[20] = {0};

		    calculateResult(g_SO_buf, maxval_, maxcount_);

		    // «∑ÒºÏ≤‚◊º»∑£ø
		    int flag_ = true;

		    for (int _i_ = 0; _i_ < WM_TOTAL_BLOCKS ; _i_++)
		    {
		    	if (!(maxval_[_i_*2] >= 1 && maxval_[_i_*2] <= 6 &&
		    		maxval_[_i_*2+1] >= 1 && maxval_[_i_*2+1] <= 6))
		    	{
		    		flag_ = false;
		    		break;
		    	}
		    }


		    if ((!invalidFlag) &&
		    	(g_Image4Difference > MIN_IMAGE_DIFFERENCE) &&
		    	flag_)
		    {
//		    int _cc = 0;
//		    for (int _i_ = 0; _i_ < 2; _i_++)
//				for (int _j_ = 0; _j_ < 2; _j_++)
//				{
//					 cvSetImageROI(img512,cvRect(_i_*256, _j_*256, 256, 256));//…Ë÷√‘¥ÕºœÒROI
//					 cvCopy(img512,img); //∏¥÷∆ÕºœÒ
//
//
//						sprintf(_all_,"%d%d%d%d%d%d%d%d",maxval_[0],maxval_[1],maxval_[2],maxval_[3],
//								maxval_[4],maxval_[5],maxval_[6],maxval_[7]);
//				    	{
//				    		int __i;
//				    		for (__i = 0; __i < 8; __i++)
//				    			_char_[__i] = so_buf[_cc][__i] + '0';
//				    		_char_[__i] = '\0';
//
//						    sprintf(_file_,"/sdcard/w%d%d-%s-%s.jpg", _i,_j,_all_,_char_);
//							cvSaveImage(_file_,img);
//				    	}
//
//					 // --- for debug
//				    _cc++;
//				}

		        //±£¥ÊÕºœÒπ©≤‚ ‘
		    	strcpy(_all_,"");
		    	for (int _i_ = 0; _i_ < WM_TOTAL_BLOCKS ; _i_++)
		    	{
		    		char _temp[20];
		    		sprintf(_temp,"%d%d",maxval_[_i*2],maxval_[_i*2+1]);
		    		strcat(_all_,_temp);
		    	}
				time_t t;
		    	struct tm *p;

		    	t = time(NULL);
		    	p = gmtime(&t);

		    	cvResetImageROI(img512);

		    	sprintf(_file_, "/sdcard/W%02d%02d_%02d%02d%02d-%s.jpg",
							1+p->tm_mon, p->tm_mday,
        		            p->tm_hour + 8, p->tm_min, p->tm_sec,_all_);

		    	cvSaveImage(_file_,img512);

				FILE *fp1;
				fp1 = fopen("/sdcard/wtlogsim-001.txt", "a+");
				fprintf(fp1,"\nW%02d%02d_%02d%02d%02d-%s.jpg : imgDiff:%d DC:%d\n",1+p->tm_mon, p->tm_mday,
    		            p->tm_hour + 8, p->tm_min, p->tm_sec,_all_,
    		            g_Image4Difference, g_DC_Value);
				fclose(fp1);

		    } //if flag

			}
#endif
			// ----------- “‘…œŒ™µ˜ ‘±£¥ÊÕºœÒ”√£¨∑¢≤º ±ø…»°œ˚ ---------------------------//

			cvResetImageROI(img512);//‘¥ÕºœÒ”√ÕÍ∫Û£¨«Âø’ROI

			 //Õ≥º∆Ω·π˚
			int maxval[20]   = {0};
			int maxcount[20] = {0};

		    calculateResult(g_SO_buf, maxval, maxcount);

#if 0
		    //‘Á∆⁄∞Ê±æ
		    _c = 0;
		    for (_i = 0; _i < 4 ; _i++)
		    	for (_j = 0; _j < 8; _j++)
		    	{
		    		obuf[_c++] = g_SO_buf[_i][_j];
		    	}

		    obuf[32] = maxval[0];
		    obuf[33] = maxcount[0];
		    obuf[34] = maxval[1];
		    obuf[35] = maxcount[1];
#endif

		    //◊®¿˚æ÷∞Ê±æ
		    _c   = 0;
		    flag = true;

		    for (_i = 0; _i < WM_TOTAL_BLOCKS ; _i++)
		    {
		    	for (_j = 0; _j < 8; _j++)
		    	{
		    		obuf[_c++] = g_SO_buf[_i][_j];
		    	}

		    }

		    for (_i = 0; _i < 16 ; _i++)
		    {
	    		obuf[_c++] = maxval[_i];

		    	if (!(maxval[_i] >= 1 && maxval[_i] <= 6 ))
		    	{
		    		flag = false;
		    	}
		    }


label_Quit:
		    // Õ∑≈◊ ‘¥
			// end
			env->ReleaseIntArrayElements(buf,  cbuf, 0);
			env->ReleaseIntArrayElements(pout, obuf, 0);

			// Õ∑≈512X512ÕºœÒ
			cvReleaseImage(&img512);
			img512=NULL;                            //------------1

			// Õ∑≈256X256ÕºœÒ
			cvReleaseImage(&img);
			img=NULL;                            //------------1

			// Õ∑≈‘≠ ºÕºœÒÕ∑
			cvReleaseImageHeader(&resizeimg);
			resizeimg=NULL;                            //------------1
#if 0
            //‘Á∆⁄∞Ê±æ
			if ((!invalidFlag) && (maxcount[0] >= 10) && (maxcount[1] >= 10))
				flag = true;
			else
				flag = false;
#endif

			if ((!invalidFlag) &&
				(g_Image4Difference > MIN_IMAGE_DIFFERENCE) &&
				flag)
				return true;
			else
				return false;

	}

	int calculateSimiliar(IplImage* img512)
	{
		int ret = 0;
		int _sum = 0;
		int x = 0, y = 0;


		if (img512->height < IMAGE_HEIGHT * 2 ||
			img512->width  < IMAGE_WIDTH * 2)
			return 0;

		for (y = 0; y < IMAGE_HEIGHT; y++)
		{
			char* ptr1 = img512->imageData + y * img512->widthStep;
			char* ptr2 = img512->imageData + (y+256) * img512->widthStep;

			for (x = 0; x < IMAGE_WIDTH; x++)
			{
				char* ptr_00 = ptr1 + x * img512->nChannels; //default is 8U
				char* ptr_01 = ptr1 + (IMAGE_WIDTH+x) * img512->nChannels;
				char* ptr_10 = ptr2 + x * img512->nChannels; //default is 8U
				char* ptr_11 = ptr2 + (IMAGE_WIDTH+x) * img512->nChannels;

				_sum = 0;
				_sum = _sum + abs( (ptr_00[0]+ptr_00[1]+ptr_00[2])/3.0 - (ptr_01[0]+ptr_01[1]+ptr_01[2])/3.0 );
				_sum = _sum + abs( (ptr_01[0]+ptr_01[1]+ptr_01[2])/3.0 - (ptr_10[0]+ptr_10[1]+ptr_10[2])/3.0 );
				_sum = _sum + abs( (ptr_10[0]+ptr_10[1]+ptr_10[2])/3.0 - (ptr_11[0]+ptr_11[1]+ptr_11[2])/3.0 );
				_sum = _sum + abs( (ptr_11[0]+ptr_11[1]+ptr_11[2])/3.0 - (ptr_00[0]+ptr_00[1]+ptr_00[2])/3.0 );

				ret = ret + _sum / 4;
			}
		}
		//IplImage* img00 = cvCreateImage(cvSize(IMAGE_WIDTH,IMAGE_HEIGHT),IPL_DEPTH_8U,4);
		//IplImage* img01 = cvCreateImage(cvSize(IMAGE_WIDTH,IMAGE_HEIGHT),IPL_DEPTH_8U,4);
		//IplImage* img10 = cvCreateImage(cvSize(IMAGE_WIDTH,IMAGE_HEIGHT),IPL_DEPTH_8U,4);
		//IplImage* img11 = cvCreateImage(cvSize(IMAGE_WIDTH,IMAGE_HEIGHT),IPL_DEPTH_8U,4);
		//IplImage* img_sub1 = cvCreateImage(cvSize(IMAGE_WIDTH,IMAGE_HEIGHT),IPL_DEPTH_32F,4);
		//IplImage* img_sub2 = cvCreateImage(cvSize(IMAGE_WIDTH,IMAGE_HEIGHT),IPL_DEPTH_32F,4);

	    //cvSetImageROI(img512,cvRect(0*256, 0*256, 256, 256));//…Ë÷√‘¥ÕºœÒROI
		//cvCopy(img512,img00); //∏¥÷∆ÕºœÒ
	    //cvSetImageROI(img512,cvRect(0*256, 1*256, 256, 256));//…Ë÷√‘¥ÕºœÒROI
		//cvCopy(img512,img01); //∏¥÷∆ÕºœÒ
	    //cvSetImageROI(img512,cvRect(1*256, 0*256, 256, 256));//…Ë÷√‘¥ÕºœÒROI
		//cvCopy(img512,img10); //∏¥÷∆ÕºœÒ
	    //cvSetImageROI(img512,cvRect(1*256, 1*256, 256, 256));//…Ë÷√‘¥ÕºœÒROI
		//cvCopy(img512,img11); //∏¥÷∆ÕºœÒ

		//cvAbsDiff(img00, img01, img_sub1);  //00 <-> 01

		//cvAbsDiff(img01, img10, img_sub2);
		//cvAdd(img_sub1, img_sub2, img_sub1);

		//cvAbsDiff(img10, img11, img_sub2);
		//cvAdd(img_sub1, img_sub2, img_sub1);

		//cvAbsDiff(img11, img00, img_sub2);
		//cvAdd(img_sub1, img_sub2, img_sub1);

		//CvScalar r = cvSum(img_sub1);
		//ret = (r.val[0]+r.val[1]+r.val[2])/3;

		//cvReleaseImage(&img00);
		//cvReleaseImage(&img01);
		//cvReleaseImage(&img10);
		//cvReleaseImage(&img11);
		//cvReleaseImage(&img_sub1);
		//cvReleaseImage(&img_sub2);

		return ret;
	}

	//=====================================================================//
	//∫Ø ˝√˚≥∆£∫calculateResult
	//◊®¿˚æ÷Ãÿ÷∆∞Ê±æ
	//π¶ƒ‹£∫ºÏ≤‚√ø∏ˆøÈ÷–µƒ ˝æ›£¨≈–∂œ◊Ó÷’ «∑ÒºÏ≤‚≥ˆÀÆ”°
	//≤Œ ˝£∫so_buf    √øøÈ÷– ˝æ›
	//      maxval    ºÏ≤‚≥ˆ ˝æ›
	//      maxcount  ºÏ≤‚≥ˆ ˝æ›µƒ∏ˆ ˝
	//=====================================================================//
	void calculateResult(const int so_buf[WM_TOTAL_BLOCKS][WM_MAX_CHARS_PER_BLOCKS],
						      int* maxval, int* maxcount)
	//¥À∞Ê±æŒ™2øÈ÷–∂‘”¶Œª÷√÷–»°2∏ˆ£¨…œœ¬±»Ωœ°£“Ú¥À»›¡ø¿©¥Û“ª±∂
	//
	{
#define MIN_NUMBERS 3
		    //ºÏ≤‚∑Ω∑®£∫
		    //Õ≥º∆block £∫ 1-4 ÷–8∏ˆŒª÷√£¨√ø∏ˆŒª÷√4∏ˆ÷–÷¡…Ÿ3∏ˆ≤≈ƒ‹»œŒ™’˝»∑
			//Õ≥º∆Ω·π˚
		    //6-12–ﬁ∏ƒ£∫//º”»Îœﬁ÷∆Ãıº˛£¨»Áπ˚4∏ˆblockœ‡À∆∂»≈–∂œ£¨»Áπ˚∫‹œ‡À∆£¨‘Ú»œŒ™ÕºœÒŒ¥º”»ÎÀÆ”°
			//
		    //  XXXX XXXX       XXXX XXXX
		    //  so_buf[0]       so_buf[1]
		    //  XXXX XXXX       XXXX XXXX
		    //  so_buf[2]       so_buf[3]
			int _i, _j, _c, _r;
			int keysum  = 0;
			int sum_[7] = {0,0,0,0,0,0,0};
			int maxid_  = 0;
			int maxval_ = 0;

			int bFailed = false;

#define _SAVE_RESULT_
#ifdef _SAVE_RESULT_
		    FILE* fp;
		    fp = fopen("/sdcard/w000--calculate.txt","a+");

		    for(int _r1=0; _r1 < 4; _r1++)
		    {
		    	for(int _c1=0; _c1 < 8; _c1++)
		    	{
		    		fprintf(fp,"%d ",so_buf[_r1][_c1]);
		    	}
		    	fprintf(fp,"\n");
		    }
#endif

	    	for(int _r = 0; _r < 16; _r++)
	    		maxval[_r] = 0;

		    for (int _r = 0; _r < 2 && !bFailed; _r++)
		    {
				for (_c = 0; _c < 8; _c++)   //’“16Œª ˝◊÷
				{
					if (so_buf[2*_r][_c] != so_buf[2*_r+1][_c])
					{
						bFailed = true;
						break;
					}

					maxval[_r*8 + _c]   = so_buf[2*_r][_c];
					maxcount[_r*8 + _c] = 2;

				}

		    }

#ifdef _SAVE_RESULT_

		    for (int _r = 0; _r < 16; _r++)
		    {
		    	fprintf(fp,"%d ",maxval[_r]);
		    }
			fprintf(fp,"\n");
		    fclose(fp);
#endif

	}

	//=====================================================================//
	//∫Ø ˝√˚≥∆£∫calculateResult
	//π¶ƒ‹£∫ºÏ≤‚√ø∏ˆøÈ÷–µƒ ˝æ›£¨≈–∂œ◊Ó÷’ «∑ÒºÏ≤‚≥ˆÀÆ”°
	//≤Œ ˝£∫so_buf    √øøÈ÷– ˝æ›
	//      maxval    ºÏ≤‚≥ˆ ˝æ›
	//      maxcount  ºÏ≤‚≥ˆ ˝æ›µƒ∏ˆ ˝
	//=====================================================================//
	void _calculateResult_3in4(const int so_buf[WM_TOTAL_BLOCKS][WM_MAX_CHARS_PER_BLOCKS],
						      int* maxval, int* maxcount)
	//¥À∞Ê±æŒ™4øÈ÷–∂‘”¶Œª÷√÷–»°3∏ˆ£¨∏¯◊®¿˚æ÷µƒ∞Ê±æŒ™Õ¨“ªøÈ÷–«∞4∏ˆ£¨∫Û4∏ˆ»°»˝∏ˆ
	//
	{
#define MIN_NUMBERS 3
		    //ºÏ≤‚∑Ω∑®£∫
		    //Õ≥º∆block £∫ 1-4 ÷–8∏ˆŒª÷√£¨√ø∏ˆŒª÷√4∏ˆ÷–÷¡…Ÿ3∏ˆ≤≈ƒ‹»œŒ™’˝»∑
			//Õ≥º∆Ω·π˚
		    //6-12–ﬁ∏ƒ£∫//º”»Îœﬁ÷∆Ãıº˛£¨»Áπ˚4∏ˆblockœ‡À∆∂»≈–∂œ£¨»Áπ˚∫‹œ‡À∆£¨‘Ú»œŒ™ÕºœÒŒ¥º”»ÎÀÆ”°
			//
		    //  XXXX XXXX       XXXX XXXX
		    //  so_buf[0]       so_buf[1]
		    //  XXXX XXXX       XXXX XXXX
		    //  so_buf[2]       so_buf[3]
			int _i, _j, _c, _r;
			int keysum  = 0;
			int sum_[7] = {0,0,0,0,0,0,0};
			int maxid_  = 0;
			int maxval_ = 0;

#ifdef _SAVE_RESULT_
		    FILE* fp;
		    fp = fopen("/sdcard/w000--calculate.txt","a+");

		    for(int _r1=0; _r1 < 4; _r1++)
		    {
		    	for(int _c1=0; _c1 < 8; _c1++)
		    	{
		    		fprintf(fp,"%d ",so_buf[_r1][_c1]);
		    	}
		    	fprintf(fp,"\n");
		    }
#endif
			for (_c = 0; _c < 8; _c++)   //’“8Œª ˝◊÷
			{
				//clear hist
				for (_i = 0; _i < 7; _i++) sum_[_i] = 0;

#ifdef _SAVE_RESULT_
				fprintf(fp,"(");
#endif

				for (_i = 0; _i < 4; _i++)  //º∆À„4Œª
				{
					if ((so_buf[_i][_c] > 0) && (so_buf[_i][_c] < 7))
						sum_[so_buf[_i][_c]]++;
#ifdef _SAVE_RESULT_
					fprintf(fp,"%d ",so_buf[_i][_c]);
#endif
				}
#ifdef _SAVE_RESULT_
				fprintf(fp,"-");
#endif

				maxval_ = 0;
				maxid_  = 0;

				//find the max value
				for (_i=1; _i<7; _i++)
					if (maxval_ < sum_[_i])
					{
						maxval_ = sum_[_i];
						maxid_  = _i;
					}

				if (maxval_ >= MIN_NUMBERS)      //4∏ˆ÷–÷¡…Ÿ¥ÔµΩ3∏ˆ≤≈À„’˝»∑£¨Ω´’˝»∑ ˝◊÷∑≈‘⁄√ø◊Èµ⁄9Œª£¨µ⁄10Œª£®A£¨B£©
				{	                                  //  XXXXXXXXAB     XXXXXXXXAB
					maxval[_c]   = maxid_;            //  XXXXXXXXAB     XXXXXXXXAB
					maxcount[_c] = maxval_;
#ifdef _SAVE_RESULT_
					fprintf(fp,"%d %d)",maxval_, maxid_);
#endif
				}
				else
				{
					maxval[_c]   = 0;
					maxcount[_c] = 0;
#ifdef _SAVE_RESULT_
					fprintf(fp,"%d %d)",maxval_, maxid_);
#endif
				}
			}
#ifdef _SAVE_RESULT_
			fprintf(fp,"\n");
		    fclose(fp);
#endif

	}

	//=====================================================================//
	//∫Ø ˝√˚≥∆£∫calculateResult
	//π¶ƒ‹£∫ºÏ≤‚√ø∏ˆøÈ÷–µƒ ˝æ›£¨≈–∂œ◊Ó÷’ «∑ÒºÏ≤‚≥ˆÀÆ”°
	//≤Œ ˝£∫so_buf    √øøÈ÷– ˝æ›
	//      maxval    ºÏ≤‚≥ˆ ˝æ›
	//      maxcount  ºÏ≤‚≥ˆ ˝æ›µƒ∏ˆ ˝
	//=====================================================================//
	void calculateResult_eachLine(const int so_buf[WM_TOTAL_BLOCKS][WM_MAX_CHARS_PER_BLOCKS],
						 int* maxval, int* maxcount)
	//¥À∞Ê±æŒ™4øÈ÷–, Õ¨“ªøÈ÷–«∞4∏ˆ£¨∫Û4∏ˆ»°»˝∏ˆ
	//»Á   xxxx    xxxx
	//   4∏ˆ÷–»°3
    //
	{
		//Õ≥º∆Ω·π˚
		//ºÏ≤‚Õ≥“ª◊÷∑˚∏ˆ ˝
		int _i, _j, _c;
		int keysum = 0;
		int sum_[7] = {0,0,0,0,0,0,0};
		int maxid_ = 0;
		int maxval_ = 0;

//#define _SAVE_RESULT_
#ifdef _SAVE_RESULT_
	    FILE* fp;
	    fp = fopen("/sdcard/w000--calculate.txt","a+");

	    fprintf(fp,"\n------------\n");
	    for(int _r1 = 0; _r1 < 4; _r1++)
	    {
	    	for(int _c1=0; _c1 < 8; _c1++)
	    	{
	    		fprintf(fp,"%d ",so_buf[_r1][_c1]);
	    	}
	    	fprintf(fp,"\n");
	    }
#endif

	    for (int _nChar = 0; _nChar < WM_TOTAL_BLOCKS; _nChar++)  //
	    {
			for (_c = 0; _c < 2; _c++)
			{
				for(_i=0; _i<7; _i++) sum_[_i] = 0;

				//calculate the hist of all values
				for (_i=0; _i < 4; ++_i)
					if ((so_buf[_nChar][_c*4 + _i] > 0) &&
						(so_buf[_nChar][_c*4 + _i] < 7))
						sum_[so_buf[_nChar][_c*4 + _i]]++;

				maxid_  = 0;
				maxval_ = 0;

				//find the max value
				for (_i=1; _i<7; _i++)
					if (maxval_ < sum_[_i])
					{
						maxval_ = sum_[_i];
						maxid_  = _i;
					}

				if (maxval_ >= MIN_NUMBERS)      //4∏ˆ÷–÷¡…Ÿ¥ÔµΩ3∏ˆ≤≈À„’˝»∑£¨Ω´’˝»∑ ˝◊÷∑≈‘⁄√ø◊Èµ⁄9Œª£¨µ⁄10Œª£®A£¨B£©
				{	                                  //  XXXXXXXXAB     XXXXXXXXAB
					maxval[_nChar*2 + _c]   = maxid_;
					maxcount[_nChar*2 + _c] = maxval_;
#ifdef _SAVE_RESULT_
					fprintf(fp,"\n(%d %d)\n",maxval_, maxid_);
#endif
				}
				else
				{
					maxval[_nChar*2 + _c]   = 0;
					maxcount[_nChar*2 + _c] = 0;
#ifdef _SAVE_RESULT_
					fprintf(fp,"\n(%d %d)\n", maxval_, maxid_);
#endif
				}

			}
	    }
#ifdef _SAVE_RESULT_
	    fclose(fp);
#endif
	}

	//--------------------------------------------------//
	//”…”⁄¥´»ÎµƒÕºœÒŒ™RGBA∏Ò Ω£¨±£¥ÊŒ™RGB∏Ò Ω
	//≤Œ ˝£∫
	//     IplImage* resizeimg £∫  ‰»ÎÕºœÒ
	//     int Numbers    :  ”√”⁄º∆ ˝
	//     int bSaveImg   :   «∑Ò±£¥Êº«∫≈
	//--------------------------------------------------//
	void saveRGBImage(IplImage* resizeimg,
			          int Numbers, int bSaveImg)
	{
		// for debug
		// output series images

		Numbers++;
		if (bSaveImg && (Numbers % 5 == 1))
		{
			char _filePath[100] = {0};
			time_t t;
			struct tm *p;

			int _w = resizeimg->width;
			int _h = resizeimg->height;

			t = time(NULL);
			p = gmtime(&t);

			sprintf(_filePath, "/sdcard/%04d%02d%02d_%02d%02d%02d.bmp",
							1900+p->tm_year,1+p->tm_mon, p->tm_mday,
        		            p->tm_hour + 8, p->tm_min, p->tm_sec);

			IplImage* imgout = cvCreateImage(cvSize(_w, _h),IPL_DEPTH_8U,3);
			IplImage* dst_c1 = cvCreateImage(cvSize(_w, _h),IPL_DEPTH_8U,1);
			IplImage* dst_c2 = cvCreateImage(cvSize(_w, _h),IPL_DEPTH_8U,1);
			IplImage* dst_c3 = cvCreateImage(cvSize(_w, _h),IPL_DEPTH_8U,1);

			cvSplit(resizeimg,   dst_c1, dst_c2, dst_c3, NULL);
			cvMerge(dst_c1, dst_c2, dst_c3, NULL, imgout);

			//sprintf(_filePath, "/sdcard/wt%03d-%s.bmp", Numbers/5, pOutWM);

			cvSaveImage(_filePath, imgout);

			cvReleaseImage(&imgout);   imgout=NULL;
			cvReleaseImage(&dst_c1);   dst_c1=NULL;
			cvReleaseImage(&dst_c2);   dst_c2=NULL;
			cvReleaseImage(&dst_c3);   dst_c3=NULL;
			// end;
		}


	}
	//--------------------------------------------------//
	//‘⁄256X256¥Û–°µƒÕºœÒ÷–£¨ºÏ≤‚8ŒªÀÆ”°–≈œ¢
	//≤Œ ˝£∫
	//     IplImage* img £∫  ‰»ÎÕºœÒ
	//     int* obuf     :  ‰≥ˆΩ·π˚
	//
	//--------------------------------------------------//
    void detectFrom256Image(IplImage* img, int* obuf)
    {
		char pOutWM[WM_MAX_CHARS_PER_BLOCKS] = {0};    //[20]

		//ÀÆ”°Ã·»° µº ¥¶¿Ì≤ø∑÷µƒÕºœÒ ˝æ›
		long nImagePixelsNum = IMAGE_WIDTH * IMAGE_HEIGHT;//œÒÀÿµ„ ˝

		complex<double> *fftimage = new complex<double> [nImagePixelsNum];
		double * pImg = new double[nImagePixelsNum];

	    double * imageRe = new double[nImagePixelsNum];  //∏¯∑˘∂»∑÷≈‰ƒ⁄¥Êø’º‰  //1  512k
		double * imageIm = new double[nImagePixelsNum];  //∏¯∑˘Ω«∑÷≈‰ƒ⁄¥Êø’º‰  //2
	    double * fftRe   = new double[nImagePixelsNum];  //∏¯∑˘∂»∑÷≈‰ƒ⁄¥Êø’º‰     //3
		double * fftIm   = new double[nImagePixelsNum];  //∏¯∑˘Ω«∑÷≈‰ƒ⁄¥Êø’º‰    //4

		double * polarMap = new double[nImagePixelsNum];//∏¯≤Ó÷µ∫Ûµƒ–¬æÿ’Û∑÷≈‰ƒ⁄¥Ê  512k
		int * watermark   = new int[MAX_WM_LEX_LEN*MAX_WM_LEN];//∏¯◊™ªª∫ÛµƒÀÆ”°–≈œ¢∑÷≈‰ƒ⁄¥Ê6*8


		//¥À¥¶≤…”√OpenCVµƒfft±‰ªª
		fftTransform(img, fftRe, fftIm);

#ifdef _SAVE_TEST_DATA
		char _name00[100] = {"output_fft1_re.log"};
		saveArray(fftRe, _name00);
		char _name01[100] = {"output_fft1_im.log"};
		saveArray(fftIm, _name01);
#endif

		//Ã·»°∑˘∂»∫Õ∑˘Ω«
		//GetABSPart2(imageRe, fftimage);//µ√µΩ∑˘∂»
		GetABSPart(imageRe, fftRe, fftIm);//µ√µΩ∑˘∂»

#ifdef _SAVE_TEST_DATA
		strcpy(_name00,"output_fft1_abs.log");
		saveArray(imageRe, _name00);
#endif

		fftShift(imageRe, IMAGE_WIDTH, IMAGE_HEIGHT);

#ifdef _SAVE_TEST_DATA
		strcpy(_name00,"output_fft1_shift.log");
		saveArray(imageRe, _name00);
#endif

		mir_inverse(imageRe, IMAGE_WIDTH, IMAGE_HEIGHT);

#ifdef _SAVE_TEST_DATA
		char _name1[100] = {"output_inverse.log"};
		saveArray(imageRe, _name1);
#endif

		//≤Â÷µ∫Ûµ√µΩ“ª∏ˆ–¬µƒ256*256æÿ’Û
		int midx = IMAGE_WIDTH/2-1;
		int midy = IMAGE_HEIGHT/2-1;

		int rmin = 35 ;
		int rmax = 80;
		int numr = 256;
		int numa = 256;

		double rrange[256];
		double arange[256];
		double rrange1=log(rmin);
		double rrange2=log(rmax);
		double rTab[256];
		double sinTab[256];
		double cosTab[256];

		for(int rr=0;rr<numr;rr++)  //256
		{
		 double x,y;
		 double imVal;

		 rrange[rr] = ((rrange2-rrange1)/(numr-1))*rr+rrange1;   //≤Â÷µ
		 rTab[rr]=exp(rrange[rr]);
		 for(int a=0;a<numa;a++){
			arange[a]=(2*PI*((double)numa-1)/(numa*(numa-1)))*a;
			sinTab[a]=sin(arange[a]);
			cosTab[a]=cos(arange[a]);
			x=midx+rTab[rr]*cosTab[a];
			y=midy+rTab[rr]*sinTab[a];
			//if(this->m_imgWidth>IMAGE_WIDTH||this->m_imgHeight<IMAGE_HEIGHT){
			  x+=0.5;
			  y+=0.5;
			//}
			imVal=biLinInterp(imageRe,x,y,IMAGE_WIDTH,IMAGE_HEIGHT);
			polarMap[rr*numa+a]=imVal;
		 }
		}

		//Ã·»°ÀÆ”°
		Detectmark(watermark,polarMap,IMAGE_WIDTH,IMAGE_HEIGHT);

		ConvertFromWatermark(watermark, pOutWM);

		//∑µªÿ–≈œ¢
		obuf[0] = pOutWM[0] - '0';
		obuf[1] = pOutWM[1] - '0';
		obuf[2] = pOutWM[2] - '0';
		obuf[3] = pOutWM[3] - '0';
		obuf[4] = pOutWM[4] - '0';
		obuf[5] = pOutWM[5] - '0';
		obuf[6] = pOutWM[6] - '0';
		obuf[7] = pOutWM[7] - '0';
		obuf[8] = '0';
		obuf[9] = '0';
		obuf[10] = '0';

// ¥À¥¶¥˙¬ÎŒ™µ˜ ‘◊˜”√£¨µ±ºÏ≤‚ÕÍ512’˚∑˘Õº∫Û£¨g_Finished_One_loopŒ™’Ê£¨≈–∂œ «∑ÒºÏ≤‚≥…π¶£¨»Ù≥…π¶£¨‘ÚΩ´ºÏ≤‚µ„∏ΩΩ¸µƒ∆µ”Ú ˝æ›¥Ú”°≥ˆ¿¥
#if 0
		if (g_Finished_One_Loop == true)
		{

			int maxval_[8]   = {0,0,0,0,0,0,0,0};
			int maxcount_[8] = {0,0,0,0,0,0,0,0};

		    calculateResult(g_SO_buf, maxval_, maxcount_);

		    // «∑ÒºÏ≤‚◊º»∑£ø
		    int flag_ = true;

		    for (int _i_ = 0; _i_ < 4 ; _i_++)
		    {
		    	if (!(maxval_[_i_*2] >= 1 && maxval_[_i_*2] <= 6 &&
		    		maxval_[_i_*2+1] >= 1 && maxval_[_i_*2+1] <= 6))
		    	{
		    		flag_ = false;
		    		break;
		    	}
		    }
		    if ((!invalidFlag) &&
		    	(g_Image4Difference > MIN_IMAGE_DIFFERENCE) &&
		    	flag_)
		    	printDetectmark(watermark,polarMap,IMAGE_WIDTH,IMAGE_HEIGHT);
		}
// µ˜ ‘¥˙¬ÎΩ· ¯
#endif

//big lloop
		delete []pImg;
		delete []fftimage;
		delete []imageRe;    //1
		delete []imageIm;    //2
		delete []fftRe;      //3
		delete []fftIm;      //4
		delete []polarMap;   //5
		delete []watermark;  //6

#if 0
		//∑µªÿ–≈œ¢
		obuf[0] = 9;
		obuf[1] = 9;
		obuf[2] = 9;
		obuf[3] = 9;
		obuf[4] = 9;
		obuf[5] = 9;
		obuf[6] = 9;
		obuf[7] = 9;
		obuf[8] = '0';

#endif



    }

	void Detectmark(int * watermark, double * polarMap, int width, int height)
	{
		long i,j;
		long nImagePixelsNum=width*height;//¥¶¿ÌÕºœÒµƒ ˝æ›µ„ ˝


		complex<double> * fftimage = new complex<double>[nImagePixelsNum];

		double *imageIm = new double[nImagePixelsNum];
		double *imageRe = new double[nImagePixelsNum];
		double *fftRe   = new double[nImagePixelsNum];
		double *fftIm   = new double[nImagePixelsNum];

		double *raw_data=new double[250];

#ifdef DEBUG_ON
		FILE *fp;

		fp = fopen("/sdcard/wtlogdetect.txt", "w");
#endif

		//º´◊¯±Í ˝æ›‘Ÿ◊˜∏µ¡¢“∂±‰ªª
		IplImage* pIn = cvCreateImage(cvSize(width,height),IPL_DEPTH_64F,1);

		for (i=0; i<height; i++)
		{
			for (j=0; j<width; j++)
			{
				cvSetReal2D(pIn, i, j, polarMap[i*width+j]);
#ifdef DEBUG_ON
				if (fp) fprintf(fp,"%15.1f", polarMap[i*width+j]);
#endif
			}
#ifdef DEBUG_ON
			if (fp) fprintf(fp,"\n");
#endif
		}

		//¥À¥¶‘› ±”…OpenCVµƒfft±‰ªª --°∑ ‘≠¿¥¿œµƒDIBFFT
		fftTransform(pIn, fftRe, fftIm);

#ifdef _SAVE_TEST_DATA
		char _name1[100] = {"output_DFT2.log"};
		saveArray(fftRe, _name1);
#endif

		//Ã·»°∑˘∂»∫Õ∑˘Ω«
		//GetABSPart2(imageRe, fftimage);//µ√µΩ∑˘∂»
		GetABSPart(imageRe, fftRe, fftIm);//µ√µΩ∑˘∂»

#ifdef _SAVE_TEST_DATA
		strcpy(_name1, "output_DFT2_ABS.log");
		saveArray(imageRe, _name1);
#endif

		fftShift(imageRe, IMAGE_WIDTH, IMAGE_HEIGHT);

		inverse(imageRe, IMAGE_WIDTH, IMAGE_HEIGHT);

#ifdef _SAVE_TEST_DATA
		strcpy(_name1, "output_DFT2_Inverse.log");
		saveArray(imageRe, _name1);
#endif
		//I think it is useless, jflu, 2013-9-21
		//GetAnglePart(imageIm, fftRe, fftIm);//µ√µΩ∑˘∂»

		//Ã·»°ÀÆ”°–≈œ¢
		int C = width/2;
		i=0;
		int m_r = 70;
		//Extract the magnitude coefficients along the circle.
		for(int x = C + 1; x <= C + 12; x++, i++)
		{
			int A = x - C;
			int B = (int)((sqrt(m_r*m_r-(x-C)*(x-C)))+i*2+0.5);
			double temp1,temp2,temp3,temp4;

			if(x <= C + 6)  //«∞6¥Œ
			{
				temp1 = maxvalue(imageRe,x-9,x-9,C+B-60-1,C+B-60+1);
				temp2 = maxvalue(imageRe,x-9,x-9,C-B+60-1,C-B+60+1);
				temp3 = maxvalue(imageRe,x-9,x-9,C+B-60-8-1,C+B-60-8+1);
				temp4 = maxvalue(imageRe,x-9+1,x-9+1,C-B+60+8-2-1,C-B+60+8-2+1);

			}
			else            //∫Û6¥Œ
			{
				temp1 = maxvalue(imageRe,x-3-1,x-3-1,C-B+50+3-4-1,C-B+50+3-4+1);
				temp2 = maxvalue(imageRe,x-17,x-17,C-B+62-1,C-B+62+1);
				temp3 = maxvalue(imageRe,x-4,x-4,C-B+42-1,C-B+42+1);
				temp4 = maxvalue(imageRe,x-18,x-18,C-B+55-1,C-B+55+1);

			}
			raw_data[x-C]    = temp1;
			raw_data[x-C+20] = temp2;	 //20..32 ¥Ê∑≈–¬‘ˆº”µƒ2◊È ˝æ›µ„
			raw_data[x-C+40] = temp3;	 //40..52 ¥Ê∑≈–¬‘ˆº”µƒ2◊È ˝æ›µ„
			raw_data[x-C+60] = temp4;	 //60..72 ¥Ê∑≈–¬‘ˆº”µƒ2◊È ˝æ›µ„

		}
		int index =maxindex(raw_data,1, 6);
		int index2=maxindex(raw_data,21,26);
		int index3=maxindex(raw_data,41,46);
		int index4=maxindex(raw_data,61,66);

		for(i=1;i<=6;i++){
			if(i!=index)
				watermark[i-1]=0;
			else
				watermark[i-1]=1;

			if(i!=index2-20)
				watermark[i-1+12]=0;
			else
				watermark[i-1+12]=1;

			if(i!=index3-40)
				watermark[i-1+24]=0;
			else
				watermark[i-1+24]=1;

			if(i!=index4-60)
				watermark[i-1+36]=0;
			else
				watermark[i-1+36]=1;
		}

		index  = maxindex(raw_data,7,12);
		index2 = maxindex(raw_data,27,32);
		index3 = maxindex(raw_data,47,52);
		index4 = maxindex(raw_data,67,72);

		for(i=7;i<=12;i++){
			if(i!=index)
				watermark[i-1]=0;
			else
				watermark[i-1]=1;

			if(i!=index2-20)
				watermark[i-1+12]=0;
			else
				watermark[i-1+12]=1;

			if(i!=index3-40)
				watermark[i-1+24]=0;
			else
				watermark[i-1+24]=1;

			if(i!=index4-60)
				watermark[i-1+36]=0;
			else
				watermark[i-1+36]=1;
		}

		delete []imageRe;
		delete []imageIm;
		delete []fftimage;
		delete []raw_data;

		delete []fftRe;
		delete []fftIm;

		cvReleaseImage(&pIn);pIn=NULL;
#ifdef DEBUG_ON
		if (fp) fclose(fp);
#endif

	}

	//--------------------------------------------------//
	//ªÒ»°fft±‰ªª∫Ûµƒ∑˘∂»
	//≤Œ ˝£∫
	//     imageRe:  ¥Ê∑≈∑˘∂»÷µ
	//     InRe:  ±‰ªª∫Ûµƒ µ≤ø
	//     InIm:  ±‰ªª∫Ûµƒ–È≤ø
	//--------------------------------------------------//
	void GetABSPart(double *imageRe, double * InRe, double* InIm)
	{
	    int i,j;
	    double x = 0, y= 0;

		for (i=0;i<IMAGE_HEIGHT;i++)
			for (j=0;j<IMAGE_WIDTH;j++)
			{
				x = InRe[i*IMAGE_WIDTH+j];
				y = InIm[i*IMAGE_WIDTH+j];

				imageRe[i*IMAGE_WIDTH+j]= sqrt(x*x + y*y);
			}
	}

	void GetABSPart2(double *imageRe, complex<double> * inArr)
	{
	    int i,j;
		for (i=0;i<IMAGE_HEIGHT;i++)
			for (j=0;j<IMAGE_WIDTH;j++)
			{
				imageRe[i*IMAGE_WIDTH+j]=
					sqrt(inArr[i*IMAGE_WIDTH + j].real() * inArr[i*IMAGE_WIDTH + j].real() +
						 inArr[i*IMAGE_WIDTH + j].imag() * inArr[i*IMAGE_WIDTH + j].imag() );
			}
	}

	//--------------------------------------------------//
	//ªÒ»°fft±‰ªª∫Ûµƒœ‡Œª
	//≤Œ ˝£∫
	//     imageRe:  ¥Ê∑≈œ‡Œª÷µ
	//     InRe:  ±‰ªª∫Ûµƒ µ≤ø
	//     InIm:  ±‰ªª∫Ûµƒ–È≤ø
	//--------------------------------------------------//
	void GetAnglePart(double *imageIm, double * InRe, double* InIm)
	{
		int i,j;
		for (i=0;i<IMAGE_HEIGHT;i++)
			for (j=0;j<IMAGE_WIDTH;j++)
			{
				if(InRe[i*IMAGE_WIDTH+j]==0){
					if(InIm[i*IMAGE_WIDTH+j]==0)
						imageIm[i*IMAGE_WIDTH+j]=0;
					else if(InIm[i*IMAGE_WIDTH+j]>0)
						imageIm[i*IMAGE_WIDTH+j]=PI/2;
					else if(InIm[i*IMAGE_WIDTH+j]<0)
						imageIm[i*IMAGE_WIDTH+j]=-PI/2;
				}
				else if (InRe[i*IMAGE_WIDTH+j]>0)
					imageIm[i*IMAGE_WIDTH+j]=
					atan((InIm[i*IMAGE_WIDTH+j])/(InRe[i*IMAGE_WIDTH+j]));
				else
					imageIm[i*IMAGE_WIDTH+j]=
					atan((InIm[i*IMAGE_WIDTH+j])/(InRe[i*IMAGE_WIDTH+j]))+PI;
			}
	}

	void mir_inverse(double *A, int width, int height)
	{
		int i,j;
		double exchange;
			for(i=0;i<height/2;i++)
				for(j=0;j<width;j++)
				{
					exchange=A[i*IMAGE_WIDTH+j];
					A[i*IMAGE_WIDTH+j]=A[(height-i-1)*IMAGE_WIDTH+j];
					A[(height-i-1)*IMAGE_WIDTH+j]=exchange;
				}
	}



	double biLinInterp(double *imageRe, double x, double y, int width, int height)
	{
		int x1 =(int) floor(x);
		int x2 = x1 + 1;
		int y1 =(int) floor(y);
		int y2 = y1 + 1;
		double dx2 = x - x1;
		double dy2 = y - y1;
		double dx1 = 1-dx2;
		double dy1 = 1-dy2;

		double v11;
		double v21;
		double v12;
		double v22;
		if(x1>=0&&x1<width && y1>=0&&y1<height)
			v11 = imageRe[x1*width+y1];
		else
			v11=0;
		if(x2>=0&&x2<width && y1>=0&&y1<height)
			v21 = imageRe[x2*width+y1];
		else
			v21=0;
		if(x1>=0&&x1<width && y2>=0&&y2<height)
			v12 = imageRe[x1*width+y2];
		else
			v12=0;
		if(x2>=0&&x2<width && y2>=0&&y2<height)
			v22 = imageRe[x2*width+y2];
		else
			v22=0;
		double val = dx1*dy1*v11 + dx1*dy2*v12 + dx2*dy1*v21 + dx2*dy2*v22;
		return val;
	}


	void ConvertFromWatermark(int * watermark, char * pOutWM)
	{
		int i,j;
		int tempint[MAX_WM_LEX_LEN];
		char tempchar[MAX_WM_LEN+1];

		for(i=0;i<MAX_WM_LEN;i++)
		{    //8
			for(j=0;j<MAX_WM_LEX_LEN;j++)
			{  //6
				tempint[j]=*(watermark+i*MAX_WM_LEX_LEN+j);
			}
			int k=0;
			for(j=0;j<MAX_WM_LEX_LEN;j++){
				k+=tempint[j];   //sum of the line
			}
			if(k==0)
				tempchar[i]=' ';
			else if(k==1){  //only one 1 in the whole line
				// 1 0 0 0 0 0
				// 5
				for(j=0;j<MAX_WM_LEX_LEN;j++){  //6
					if(tempint[j]==1)
					{
						//tempchar[i]=5-j+1+'0';
						tempchar[i]=5-j + 1 + '0';
						break;
					}
				}
			}
			else if(k==2){
				if(tempint[0]==0){
					tempchar[i]='0';
				}
				else{
					if(tempint[4]==1)
						tempchar[i]='7';
					else if(tempint[2]==1)
						tempchar[i]='8';
					else if(tempint[5]==1)
						tempchar[i]='9';
				}
			}
			else
				tempchar[i]='!';
		}
		tempchar[MAX_WM_LEN]='\0';
		strcpy(pOutWM, tempchar);

	}

	double maxvalue(double *A, int row1, int row2, int col1, int col2)
	{
		//int num=(int)sqrt((row2-row1)*(row2-row1)*(col2-col1)*(col2-col1));
		int i,j;
		double temp=A[row1*IMAGE_WIDTH+col1];
		for(i=row1;i<=row2;i++){
			for(j=col1;j<=col2;j++){
				if(A[i*IMAGE_WIDTH+j]>temp)
					temp=A[i*IMAGE_WIDTH+j];
			}
		}

//		if (g_4hit3_Flag)
//		{
//			FILE *fp1;
//			fp1 = fopen("/sdcard/wtlogmax-001.txt", "a+");
//
//			fprintf(fp1,"\n(%d,%d)-(%d,%d)\n", row1, col1, row2, col2);
//			for(i = row1-2; i <= row2 + 2 ; i++)
//			{
//				for(j = col1-2; j <= col2+2; j++)
//				{
//					fprintf(fp1,"%12d", A[i*IMAGE_WIDTH+j]);
//				}
//				fprintf(fp1,"\n");
//			}
//
//			fclose(fp1);
//
//		}

		return temp;
	}


	int maxindex(double *A, int col1, int col2)
	{
		double temp=A[col1];
		int index=col1;
		for(int i=col1;i<=col2;i++){
			if(A[i]>temp){
				temp=A[i];
				index=i;
			}
		}
		return index;
	}

	void flipImage(IplImage* pSource)
	{
		int i,j;

		long lLen = pSource->widthStep;

		double* ptrSrc = NULL;
		double* ptrDst = NULL;
		char* ptrTemp = new char[lLen];

		CvScalar s1,s2;
		for (i=0; i<IMAGE_HEIGHT / 2; ++i)
			for (j=0; j<IMAGE_WIDTH; ++j)
			{
				s1 = cvGet2D(pSource, i, j);
				s2 = cvGet2D(pSource, IMAGE_HEIGHT - 1 - i, j);
				//swap
				cvSet2D(pSource, i, j, s2);
				cvSet2D(pSource, IMAGE_HEIGHT - 1 - i, j, s1);
			}

#ifdef PP
		for(i=0; i<IMAGE_HEIGHT / 2; ++i)
		{
	        ptrSrc = (double*) ( pSource->imageData + i * pSource->widthStep );
			ptrDst = (double*) ( pSource->imageData + (IMAGE_HEIGHT - 1 - i) * pSource->widthStep );

			//swap two lines

			memcpy(ptrDst, ptrTemp, lLen*sizeof(double));
			memcpy(ptrSrc, ptrDst,  lLen*sizeof(double));
			memcpy(ptrTemp, ptrSrc, lLen*sizeof(double));
		}
#endif
		delete []ptrTemp;

	}

	//--------------------------------------------------//
	//¿˚”√OpenCV µœ÷fft±‰ªª∫Ø ˝
	//≤Œ ˝£∫pSource : ‘¥ÕºœÒ
	//     pRe:  fft±‰ªª µ≤ø
	//     pIm:  fft±‰ªª–È≤ø
	//∑µªÿ÷µ£∫
	//     0£∫Succeed, other: Failure
	//
	//     ◊¢“‚£∫fft±‰ªª∫Ûæ≠π˝“∆œ‡
	//     jflu£¨2013-9-18
	//--------------------------------------------------//
	int fftTransform(IplImage* pSource,
					  double* pRe,
					  double* pIm)
	{
		//
		int i,j;
		if(!pRe || !pIm)
			return -1;

		int nchannels  = pSource->nChannels;
		int step       = pSource->widthStep;
		int depth      = pSource->depth;
		CvSize  cvsize = cvGetSize(pSource);

		IplImage* green=NULL;

		//∏µ¡¢“∂±‰ªª
		IplImage* fourier=cvCreateImage(cvsize,IPL_DEPTH_64F,2);     //------------6
		IplImage* image_Re=cvCreateImage(cvsize,IPL_DEPTH_64F,1);  //XXX
		IplImage* image_Im=cvCreateImage(cvsize,IPL_DEPTH_64F,1);  //XXX
		IplImage* gray=cvCreateImage(cvsize, depth, 1);                   //------------2

		if (nchannels > 1)
		{
			//cvCvtColor(pSource,gray,CV_BGR2GRAY);
			//cvScale(gray, image_Re);
			//cvConvertScale(gray, image_Re, 1, 0);
			green=cvCreateImage(cvsize,IPL_DEPTH_8U,1);     //green channel
			cvSplit(pSource, NULL, green, NULL, NULL);
			cvConvertScale(green, image_Re, 1, 0);

			//char _nameo[100] = {"input_imageRe_ori.log"};
			//strcpy(_name, "input_imageRe.log");
			//saveIplImage(image_Re, _nameo);

			flipImage(image_Re);
			//cvFlip(image_Re, NULL, 0);
			//cvSaveImage("/sdcard/wtlog_inputgreen.bmp", green);
#ifdef _DDD
			uchar*  ptrSrc = NULL;
			double* ptrDst = NULL;

			cvZero(image_Re);
			for(i=0; i<IMAGE_HEIGHT; ++i)
			{
		        ptrSrc = (uchar*) ( green->imageData + i * green->widthStep );
				ptrDst = (double*) ( image_Re->imageData + i * image_Re->widthStep );

				for(j=0; j<IMAGE_WIDTH; ++j)
		        {
					ptrDst[j] = (double)ptrSrc[j]; //ptr[3*x+2]
		        }
		    }

			//char _name[100] = {"input_green.log"};
			//saveIplImage(green, _name);
#endif



		}
		else
		{
			//cvScale(pSource, image_Re);
			cvConvertScale(pSource, image_Re, 1, 0);
			//cvFlip(image_Re, NULL, 0);
			//flipImage(image_Re); ≤ª“™∑≠◊™
		}

#if 0
		char name_c1[100] = {"input_imageRe_2r.log"};
		char name_c2[100] = {"input_imageRe_1r.log"};
		if (nchannels == 1)
		{
			//strcpy(_name, "input_imageRe.log");
			saveIplImage(image_Re, name_c1);
		}
		else
		{
			//strcpy(_name, "input_imageRe.log");
			saveIplImage(image_Re, name_c2);
		}
#endif

		cvZero(image_Im);
		cvMerge(image_Re,image_Im,NULL,NULL,fourier);
		cvDFT(fourier,fourier,CV_DXT_FORWARD);
		cvReleaseImage(&image_Re);image_Re=NULL;
		cvReleaseImage(&image_Im);image_Im=NULL;


		//fftRe∫ÕfftIm∑÷±±£¥Ê∏µ¿Ô“∂±‰ªªΩ·π˚µƒ µ≤ø∫Õ–È≤ø£¨image_AmplitudeŒ™’Ò∑˘
		IplImage* fftRe=cvCreateImage(cvsize,IPL_DEPTH_64F,1);          //XXX
		IplImage* fftIm=cvCreateImage(cvsize,IPL_DEPTH_64F,1);         //XXX
		cvSplit(fourier,fftRe,fftIm,NULL,NULL);

		//cvShiftDFT(fftRe, fftRe);
		//cvShiftDFT(fftIm, fftIm);

		double* _re = pRe;
		double* _im = pIm;

		CvScalar sr,si;

		for (i=0; i<IMAGE_HEIGHT; ++i)
		{
			for(j=0; j<IMAGE_WIDTH; ++j)
			{
				//sr = cvGet2D(fftRe,i,j) ;
				_re[i*IMAGE_WIDTH+j] = cvGetReal2D(fftRe,i,j);
				//si = cvGet2D(fftIm,i,j) ;
				_im[i*IMAGE_WIDTH+j] = cvGetReal2D(fftIm,i,j);
			}
		}

		invalidFlag = (pRe[0] < MIN_ABS_VALUE) ? true : false;
		g_DC_Value  = pRe[0];

		double temp_ = 0;
		for (i=0; i<IMAGE_HEIGHT; ++i)
		{
			for(j=0; j<i; ++j)
			{
				temp_ = pRe[i*IMAGE_WIDTH + j];
				pRe[i*IMAGE_WIDTH + j] = pRe[j*IMAGE_WIDTH + i];
				pRe[j*IMAGE_WIDTH + i] = temp_;

				temp_ = pIm[i*IMAGE_WIDTH + j];
				pIm[i*IMAGE_WIDTH + j] = pIm[j*IMAGE_WIDTH + i];
				pIm[j*IMAGE_WIDTH + i] = temp_;

			}
		}
#ifdef _NOO
		if (nchannels == 1)
		{

			char _name1[100] = {"output_fftRe.log"};
			saveArray(pRe, _name1);
			//strcpy(_name1,"output_fftRe_ori.log");
			//saveIplImage(pRe, _name1);

			char _name2[100] = {"output_fftIm.log"};
			saveArray(pIm, _name2);
			//strcpy(_name1,"output_fftIm_ori.log");
			//saveIplImage(pRe, _name2);

			//saveIplImage(fftIm, _name2);
		}
#endif
		cvReleaseImage(&fftRe);fftRe=NULL;
		cvReleaseImage(&fftIm);fftIm=NULL;

		if (green)
			cvReleaseImage(&green);green=NULL;
		cvReleaseImage(&gray);gray=NULL;                                  //------------2
		cvReleaseImage(&fourier);   fourier=NULL;                      //------------6


	}

	//--------------------------------------------------//
	//¿˚”√OpenCV µœ÷fft±‰ªª∫Ûµƒ“∆œ‡
	//≤Œ ˝£∫
	//     src_arr:  ‘¥
	//     dst_arr:  ƒø±Í
	//∑µªÿ÷µ£∫
	//     0£∫Succeed, other: Failure
	//
	//     jflu£¨2013-9-18
	//--------------------------------------------------//

	int cvShiftDFT(CvArr* src_arr,CvArr* dst_arr)
	{

		CvMat* tmp;
		CvMat q1stub,q2stub;
		CvMat q3stub,q4stub;
		CvMat d1stub,d2stub;
		CvMat d3stub,d4stub;

		CvMat *q1,*q2,*q3,*q4;
		CvMat *d1,*d2,*d3,*d4;

		CvSize size=cvGetSize(src_arr);
		CvSize dst_size=cvGetSize(dst_arr);

		int cx,cy;

		if(dst_size.width!=size.width||dst_size.height!=size.height){
			return -1;
		}

		if(src_arr==dst_arr){
			tmp=cvCreateMat(size.height/2,size.width/2,cvGetElemType(src_arr));
		}

		cx=size.width/2;
		cy=size.height/2;

		q1=cvGetSubRect(src_arr,&q1stub,cvRect(0,0,cx,cy));
		q2=cvGetSubRect(src_arr,&q2stub,cvRect(cx,0,cx,cy));
		q3=cvGetSubRect(src_arr,&q3stub,cvRect(cx,cy,cx,cy));
		q4=cvGetSubRect(src_arr,&q4stub,cvRect(0,cy,cx,cy));
		d1=cvGetSubRect(src_arr,&d1stub,cvRect(0,0,cx,cy));
		d2=cvGetSubRect(src_arr,&d2stub,cvRect(cx,0,cx,cy));
		d3=cvGetSubRect(src_arr,&d3stub,cvRect(cx,cy,cx,cy));
		d4=cvGetSubRect(src_arr,&d4stub,cvRect(0,cy,cx,cy));

		if(src_arr!=dst_arr){
			if(!CV_ARE_TYPES_EQ(q1,d1)){
				return -1;
			}

		cvCopy(q3,d1,0);
		cvCopy(q4,d2,0);
		cvCopy(q1,d3,0);
		cvCopy(q2,d4,0);
		}
		else {

		cvCopy(q3,tmp,0);
		cvCopy(q1,q3,0);
		cvCopy(tmp,q1,0);
		cvCopy(q4,tmp,0);
		cvCopy(q2,q4,0);
		cvCopy(tmp,q2,0);
		}
	}

	void fftShift(double *image, int width, int height)
	{
		int i,j;
		int halfwidth,halfheight;
		halfwidth=width/2;
		halfheight=height/2;
		double exchange;
		for (i=0;i<halfheight;i++)
			for (j=0;j<halfwidth;j++)
			{
				exchange=image[i*IMAGE_HEIGHT+j];
				image[i*IMAGE_WIDTH+j]=image[(halfheight+i)*IMAGE_WIDTH + halfwidth+j];
				image[(halfheight+i)*IMAGE_WIDTH + halfwidth+j]=exchange;
				exchange=image[i*IMAGE_WIDTH + halfwidth+j];
				image[i*IMAGE_WIDTH + halfwidth+j]=image[(halfheight+i)*IMAGE_WIDTH + j];
				image[(halfheight+i)*IMAGE_WIDTH + j]=exchange;
			}

	}

	void inverse(double *image, int width, int height)
	{
		int i,j;
		double temp;
		for (i=0;i<height;i++)
			for (j=0;j<i;j++)
			{
				temp=image[i*width + j];
				image[i*width + j]=image[j*width + i];
				image[j*width + i]=temp;
			}
	}

	//--------------------------------------------------//
	//¿˚”√µ˙–ŒÀ„∑® µœ÷fft±‰ªª∫ÛµƒC¥˙¬Î
	//≤Œ ˝£∫
	//--------------------------------------------------//

	void FFT(complex<double> * TD, complex<double> * FD, int r)
	{
		// ∏∂¡¢“∂±‰ªªµ„ ˝
		LONG	count;

		// —≠ª∑±‰¡ø
		int		i,j,k;

		// ÷–º‰±‰¡ø
		int		bfsize,p;

		// Ω«∂»
		double	angle;

		complex<double> *W,*X1,*X2,*X;

		// º∆À„∏∂¡¢“∂±‰ªªµ„ ˝
		count = 1 << r;

		// ∑÷≈‰‘ÀÀ„À˘–Ë¥Ê¥¢∆˜
		W  = new complex<double>[count / 2];
		X1 = new complex<double>[count];
		X2 = new complex<double>[count];

		// º∆À„º”»®œµ ˝
		for(i = 0; i < count / 2; i++)
		{
			angle = -i * PI * 2 / count;
			W[i] = complex<double> (cos(angle), sin(angle));
		}

		// Ω´ ±”Úµ„–¥»ÎX1
		memcpy(X1, TD, sizeof(complex<double>) * count);

		// ≤…”√µ˚–ŒÀ„∑®Ω¯––øÏÀŸ∏∂¡¢“∂±‰ªª
		for(k = 0; k < r; k++)
		{
			for(j = 0; j < 1 << k; j++)
			{
				bfsize = 1 << (r-k);
				for(i = 0; i < bfsize / 2; i++)
				{
					p = j * bfsize;
					X2[i + p] = X1[i + p] + X1[i + p + bfsize / 2];
					X2[i + p + bfsize / 2] = (X1[i + p] - X1[i + p + bfsize / 2]) * W[i * (1<<k)];
				}
			}
			X  = X1;
			X1 = X2;
			X2 = X;
		}

		// ÷ÿ–¬≈≈–Ú
		for(j = 0; j < count; j++)
		{
			p = 0;
			for(i = 0; i < r; i++)
			{
				if (j&(1<<i))
				{
					p+=1<<(r-i-1);
				}
			}
			FD[j]=X1[p];
		}

		//  Õ∑≈ƒ⁄¥Ê
		delete  W;
		delete X1;
		delete X2;
	}

	bool DFT2(complex<double>* TD2,complex<double>* FD2, LONG lWidth, LONG lHeight)
	{
		// —≠ª∑±‰¡ø
		LONG	i;
		LONG	j;

		// Ω¯––∏∂¡¢“∂±‰ªªµƒøÌ∂»∫Õ∏ﬂ∂»£®2µƒ’˚ ˝¥Œ∑Ω£©
		LONG	w = lWidth;
		LONG	h = lHeight;
		int bitmove=9;
		if (lWidth==256)
			bitmove=8;

		// ∑÷≈‰ƒ⁄¥Ê
		complex<double> *TD = new complex<double>[w * h];
		complex<double> *FD = new complex<double>[w * h];


		for(i = 0; i < h; i++)
		{
			// ∂‘y∑ΩœÚΩ¯––øÏÀŸ∏∂¡¢“∂±‰ªª
			//IDFT(&FD2[w * i], &TD[w * i],  w);
			FFT(&TD2[w * i], &FD[w * i],  bitmove);
		}

		// ±£¥Ê±‰ªªΩ·π˚
		for(i = 0; i < h; i++)
		{
			for(j = 0; j < w; j++)
			{
			TD[j + w * i]=	FD[i + h * j] ;
			}
		}

		for(i = 0; i < w; i++)
		{
			// ∂‘x∑ΩœÚΩ¯––øÏÀŸ∏∂¡¢“∂±‰ªª
			//IDFT(&FD2[i * h], &TD[i * h],  h);
			FFT(&TD[i * h], &FD[i * h],  bitmove);
		}

		for(i=0; i<w; i++)
			for(j=0; j < h; j++)
			{

			FD2[i*h+j]= FD[i*h+j];

			}



		// …æ≥˝¡Ÿ ±±‰¡ø
		delete TD;
		delete FD;

		// ∑µªÿ
		return true;
	}


	bool DIBDFT(double* lpDIBBits,complex<double>* FD2, LONG lWidth, LONG lHeight)
	{
		// ÷∏œÚ‘¥ÕºœÒµƒ÷∏’Î
	//	unsigned char*	lpSrc;
		double *lpSrc;


		// —≠ª∑±‰¡ø
		LONG	i;
		LONG	j;

		// Ω¯––∏∂¡¢“∂±‰ªªµƒøÌ∂»∫Õ∏ﬂ∂»£®2µƒ’˚ ˝¥Œ∑Ω£©
		LONG	w = lWidth;
		LONG	h = lHeight;

		int bitmove=9;
		if (lWidth==256)
			bitmove=8;

		// ÕºœÒ√ø––µƒ◊÷Ω⁄ ˝
	//	LONG	lLineBytes;

		// º∆À„ÕºœÒ√ø––µƒ◊÷Ω⁄ ˝
	//	lLineBytes = WIDTHBYTES(lWidth * 8);

		// ∑÷≈‰ƒ⁄¥Ê
		complex<double> *TD = new complex<double>[w * h];
		complex<double> *FD = new complex<double>[w * h];

		// ––
		for(i = 0; i < h; i++)
		{
			// ¡–
			for(j = 0; j < w; j++)
			{
				// ÷∏œÚDIBµ⁄i––£¨µ⁄j∏ˆœÛÀÿµƒ÷∏’Î
				//lpSrc = (unsigned char*)lpDIBBits + lLineBytes * (lHeight - 1 - i) + j;
					lpSrc = lpDIBBits + lWidth * (lHeight - 1 - i) + j;

				// ∏¯ ±”Ú∏≥÷µ
				TD[j + w * i] = complex<double>(*(lpSrc), 0);
			}
		}

		for(i = 0; i < h; i++)
		{
			// ∂‘y∑ΩœÚΩ¯––øÏÀŸ∏∂¡¢“∂±‰ªª
			//DFT(&TD[w * i], &FD[w * i], w);
			FFT(&TD[w * i], &FD[w * i], bitmove);
		}

		// ±£¥Ê±‰ªªΩ·π˚
		for(i = 0; i < h; i++)
		{
			for(j = 0; j < w; j++)
			{
				TD[i + h * j] =FD[j + w * i];
			}
		}

		for(i = 0; i < w; i++)
		{
			// ∂‘x∑ΩœÚΩ¯––øÏÀŸ∏∂¡¢“∂±‰ªª
			//DFT(&TD[i * h], &FD[i * h], h);
			FFT(&TD[i * h], &FD[i * h], bitmove);

		}

		for(i=0; i<w; i++)
			for(j=0; j < h; j++)
			{

			FD2[i*h+j]= FD[i*h+j];

			}



		// …æ≥˝¡Ÿ ±±‰¡ø
		delete TD;
		delete FD;

		// ∑µªÿ
		return true;
	}

	void saveIplImage(IplImage* pSource, char* fileName)
	{
		int i,j;
		FILE *fpdata;
		char wholepath[50] = {"/sdcard/wtlog_"};

		strcat(wholepath, fileName);
		fpdata = fopen(wholepath, "w");

		double value;
		CvScalar s;
		bool isMultiCh = pSource->nChannels > 1 ? true : false;

		for(i=0; i<IMAGE_HEIGHT ; ++i)
		{
			for(j=0; j<IMAGE_WIDTH; ++j)
			{
				s=cvGet2D(pSource,i,j);
				if (isMultiCh)
					value = s.val[1]; //green channel
				else
					value = s.val[0];

				if(fpdata) fprintf(fpdata,"%11.1f", value);

			}
			if(fpdata) fprintf(fpdata,"\n");
		}

		fclose(fpdata);
	}

	void saveArray(double* pSource, char* fileName)
	{
		int i,j;
		FILE *fpdata;
		char wholepath[50] = {"/sdcard/wtlog_"};

		strcat(wholepath, fileName);
		fpdata = fopen(wholepath, "w");

		double value;

		for(i=0; i<IMAGE_HEIGHT ; ++i)
		{
			for(j=0; j<IMAGE_WIDTH; ++j)
			{
				value = pSource[i*IMAGE_WIDTH+j];

				if(fpdata) fprintf(fpdata,"%11.1f", value);

			}
			if(fpdata) fprintf(fpdata,"\n");
		}

		fclose(fpdata);
	}

	void imgV2H(const IplImage* src, IplImage* dst)
	{

		int cols, rows, ncol;

		cols = src->width;
		rows = src->height;

		for (int i=0; i<rows; i++)
		{
			for (int j=0; j<cols; j++)
			{
				ncol = rows - i - 1;
				//¥À¥¶ƒ¨»œ4Õ®µ¿£¨≤ª‘Ÿ¥¶¿Ì∆‰À˚
				((uchar *)(dst->imageData + j*dst->widthStep))[ncol*dst->nChannels + 0] =
						((uchar *)(src->imageData + i*src->widthStep))[j*src->nChannels + 0];
				((uchar *)(dst->imageData + j*dst->widthStep))[ncol*dst->nChannels + 1] =
						((uchar *)(src->imageData + i*src->widthStep))[j*src->nChannels + 1];
				((uchar *)(dst->imageData + j*dst->widthStep))[ncol*dst->nChannels + 2] =
						((uchar *)(src->imageData + i*src->widthStep))[j*src->nChannels + 2];
				((uchar *)(dst->imageData + j*dst->widthStep))[ncol*dst->nChannels + 3] =
						((uchar *)(src->imageData + i*src->widthStep))[j*src->nChannels + 3];

			}

		}

	}

	void imgV2H_Old(const IplImage* src)
	{

		int cols, rows, ncol;

		cols = src->width;
		rows = src->height;

		IplImage* imgDst = cvCreateImage(cvSize(cols,rows), IPL_DEPTH_8U, 4);
		cvCopy(src, imgDst, NULL);

		for (int i=0; i<rows; i++)
		{
			for (int j=0; j<cols; j++)
			{
				ncol = rows - i - 1;
				//¥À¥¶ƒ¨»œ4Õ®µ¿£¨≤ª‘Ÿ¥¶¿Ì∆‰À˚
				((uchar *)(src->imageData + j*src->widthStep))[ncol*src->nChannels + 0] =
						((uchar *)(imgDst->imageData + i*imgDst->widthStep))[j*imgDst->nChannels + 0];
				((uchar *)(src->imageData + j*src->widthStep))[ncol*src->nChannels + 1] =
						((uchar *)(imgDst->imageData + i*imgDst->widthStep))[j*imgDst->nChannels + 1];
				((uchar *)(src->imageData + j*src->widthStep))[ncol*src->nChannels + 2] =
						((uchar *)(imgDst->imageData + i*imgDst->widthStep))[j*imgDst->nChannels + 2];
				((uchar *)(src->imageData + j*src->widthStep))[ncol*src->nChannels + 3] =
						((uchar *)(imgDst->imageData + i*imgDst->widthStep))[j*imgDst->nChannels + 3];

			}

		}

		cvReleaseImage(&imgDst);
	}

	//printMaxvalue∫Ø ˝∫ÕprintDetectmark∫Ø ˝Ωˆ◊ˆ≤‚ ‘”√
	void printMaxvalue(double *A, int row1, int row2, int col1, int col2)
	{
		//int num=(int)sqrt((row2-row1)*(row2-row1)*(col2-col1)*(col2-col1));
		int i,j;
		double temp=A[row1*IMAGE_WIDTH+col1];

		FILE *fp1;
		fp1 = fopen("/sdcard/wtlogmax-001.txt", "a+");

		fprintf(fp1,"\n(%d,%d)-(%d,%d)\n", row1, col1, row2, col2);
		for(i = row1-2; i <= row2 + 2 ; i++)
		{
			for(j = col1-1; j <= col2+1; j++)
			{
				fprintf(fp1,"%12d", A[i*IMAGE_WIDTH+j]);
			}
			fprintf(fp1,"\n");
		}

		fclose(fp1);

	}

	void printDetectmark(int * watermark, double * polarMap, int width, int height)
	{
		static int times = 0;
		long i,j;
		long nImagePixelsNum=width*height;//¥¶¿ÌÕºœÒµƒ ˝æ›µ„ ˝


		complex<double> * fftimage = new complex<double>[nImagePixelsNum];

		double *imageIm = new double[nImagePixelsNum];
		double *imageRe = new double[nImagePixelsNum];
		double *fftRe   = new double[nImagePixelsNum];
		double *fftIm   = new double[nImagePixelsNum];

		double *raw_data=new double[250];

		//º´◊¯±Í ˝æ›‘Ÿ◊˜∏µ¡¢“∂±‰ªª
		IplImage* pIn = cvCreateImage(cvSize(width,height),IPL_DEPTH_64F,1);

		for (i=0; i<height; i++)
		{
			for (j=0; j<width; j++)
			{
				cvSetReal2D(pIn, i, j, polarMap[i*width+j]);
			}
		}

		//¥À¥¶‘› ±”…OpenCVµƒfft±‰ªª --°∑ ‘≠¿¥¿œµƒDIBFFT
		fftTransform(pIn, fftRe, fftIm);

		//Ã·»°∑˘∂»∫Õ∑˘Ω«
		GetABSPart(imageRe, fftRe, fftIm);//µ√µΩ∑˘∂»

		fftShift(imageRe, IMAGE_WIDTH, IMAGE_HEIGHT);

		inverse(imageRe, IMAGE_WIDTH, IMAGE_HEIGHT);

		//Ã·»°ÀÆ”°–≈œ¢
		int C = width/2;
		i=0;
		int m_r = 70;

		FILE *fp1;
		fp1 = fopen("/sdcard/wtlogmax-001.txt", "a+");

		fprintf(fp1,"\n=======================\n (%d)\n=======================\n", times++);

		fclose(fp1);

		//Extract the magnitude coefficients along the circle.
		for(int x = C + 1; x <= C + 12; x++, i++)
		{
			int A = x - C;
			int B = (int)((sqrt(m_r*m_r-(x-C)*(x-C)))+i*2+0.5);
			double temp1,temp2,temp3,temp4;

			if(x <= C + 6)  //«∞6¥Œ
			{
				printMaxvalue(imageRe,x-9,x-9,C+B-60-1,C+B-60+1);
				printMaxvalue(imageRe,x-9,x-9,C-B+60-1,C-B+60+1);
				printMaxvalue(imageRe,x-9,x-9,C+B-60-8-1,C+B-60-8+1);
				printMaxvalue(imageRe,x-9+1,x-9+1,C-B+60+8-2-1,C-B+60+8-2+1);

			}
			else            //∫Û6¥Œ
			{
				printMaxvalue(imageRe,x-3-1,x-3-1,C-B+50+3-4-1,C-B+50+3-4+1);
				printMaxvalue(imageRe,x-17,x-17,C-B+62-1,C-B+62+1);
				printMaxvalue(imageRe,x-4,x-4,C-B+42-1,C-B+42+1);
				printMaxvalue(imageRe,x-18,x-18,C-B+55-1,C-B+55+1);

			}
		}

		delete []imageRe;
		delete []imageIm;
		delete []fftimage;
		delete []raw_data;

		delete []fftRe;
		delete []fftIm;

		cvReleaseImage(&pIn);pIn=NULL;
	}


}  //extern C
