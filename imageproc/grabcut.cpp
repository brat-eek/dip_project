// author : Prateek Singhi
// Requires : OpenCV3.0 to run


#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include <cv.h>
#include <highgui.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <stdio.h>

using namespace std;
using namespace cv;


const Scalar RED = Scalar(0, 0, 255);
const Scalar PINK = Scalar(230, 130, 255);
const Scalar BLUE = Scalar(255, 0, 0);
const Scalar LIGHTBLUE = Scalar(255, 255, 160);
const Scalar GREEN = Scalar(0, 255, 0);

const int BGD_KEY = EVENT_FLAG_CTRLKEY;
const int FGD_KEY = EVENT_FLAG_SHIFTKEY;

Mat imginst, final;
int dim[] = {1340, 750};

Mat inv(Mat n) {
    return 255 - n;
}

void addNamedWindow(string name, int type) {
    namedWindow(name, type);
    resizeWindow(name, dim[0], dim[1]);
}

void sepia(Mat &result, Mat &temp) {
    int nr = result.rows, nc = result.cols;
	result.convertTo(result, CV_32FC3, 1.0/255.0);
    for (int y = 0; y < nr; y++) {
		for (int x = 0; x < nc; x++) {
			Vec3f fp = result.at<Vec3f>(Point(x,y));
				float a = (fp[2] * .272) + (fp[1] * .534) + (fp[0] * .131);
				float b = (fp[2] * .349) + (fp[1] * .686) + (fp[0] * .168);
				float c = (fp[2] * .393) + (fp[1] * .769) + (fp[0] * .189);
				fp[0] = a * 255;
				fp[1] = b * 255;
				fp[2] = c * 255;
			result.at<Vec3f>(Point(x,y)) = fp;
		}
	}
	result.convertTo(result, CV_8UC3);
}

void dodge(Mat &image, Mat &mask, Mat &blend) {
  divide(image, inv(mask), blend, 256);
}

void burn(Mat &image, Mat &mask, Mat &blend) {
  divide(inv(image), inv(mask), blend, 256);
  blend = inv(blend);
}

void pencil(Mat &src, Mat &final) {
  Mat gray, grayneg, gaussblur;
  final = src.clone();
  cvtColor(src, gray, CV_BGR2GRAY);
  grayneg = inv(gray);
  GaussianBlur(grayneg, gaussblur, Size(21, 21), 0, 0);
  dodge(gray, gaussblur, final);
  cvtColor(final, final, CV_GRAY2RGB);
}

void cartooniser(Mat &src, Mat &final) {
  Mat temp1, gray, medblur, temp2, edge;
  
  int num_down = 2, num_bilateral = 7;
  temp1 = src;
  
  for(int j = 0; j < num_down; j++) {
    pyrDown(temp1, temp1, Size(temp1.cols / 2, temp1.rows / 2));
  }
  for(int j = 0; j < num_bilateral; j++) {
    bilateralFilter(temp1, temp2, 9, 9, 7 );
    temp1 = temp2.clone();
  }
  for(int j = 0; j < num_down; j++) {
    pyrUp(temp1, temp1, Size(temp1.cols * 2, temp1.rows * 2));
  }
  cvtColor(temp1, gray, CV_BGR2GRAY);
  medianBlur(gray, medblur, 7);
  adaptiveThreshold(medblur, edge, 255, ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY, 9, 2);
  cvtColor(edge, edge, CV_GRAY2RGB);
  bitwise_and(temp1, edge, final); 
}

static void getBinMask( const Mat& comMask, Mat& binMask ) {
    if( comMask.empty() || comMask.type()!=CV_8UC1 )
        CV_Error( Error::StsBadArg, "comMask is empty or has incorrect type (not CV_8UC1)" );
    if( binMask.empty() || binMask.rows!=comMask.rows || binMask.cols!=comMask.cols )
        binMask.create( comMask.size(), CV_8UC1 );
    binMask = comMask & 1;
}

class GCApplication
{
public:
    enum{ NOT_SET = 0, IN_PROCESS = 1, SET = 2 };
    static const int radius = 3;
    static const int thickness = -1;

    void reset();
    void setImageAndWinName( const Mat& _image, const string& _winName );
    void showImage() const;
    void mouseClick( int event, int x, int y, int flags, void* param );
    int nextIter();
    int getIterCount() const { return iterCount; }
private:
    void setRectInMask();
    void setLblsInMask( int flags, Point p, bool isPr );

    const string* winName;
    const Mat* image;
    Mat mask;
    Mat bgdModel, fgdModel;

    uchar rectState, lblsState, prLblsState;
    bool isInitialized;

    Rect rect;
    vector<Point> fgdPxls, bgdPxls, prFgdPxls, prBgdPxls;
    int iterCount;
};

void GCApplication::reset()
{
    if( !mask.empty() )
        mask.setTo(Scalar::all(GC_BGD));
    bgdPxls.clear(); fgdPxls.clear();
    prBgdPxls.clear();  prFgdPxls.clear();

    isInitialized = false;
    rectState = NOT_SET;
    lblsState = NOT_SET;
    prLblsState = NOT_SET;
    iterCount = 0;
}

void GCApplication::setImageAndWinName( const Mat& _image, const string& _winName  )
{
    if( _image.empty() || _winName.empty() )
        return;
    image = &_image;
    winName = &_winName;
    mask.create( image->size(), CV_8UC1);
    reset();
}

void GCApplication::showImage() const
{
    if( image->empty() || winName->empty() )
        return;

    Mat res;
    Mat binMask;
    if( !isInitialized )
        image->copyTo( res );
    else
    {
        getBinMask( mask, binMask );
        image->copyTo( res, binMask );
    }

    vector<Point>::const_iterator it;
    for( it = bgdPxls.begin(); it != bgdPxls.end(); ++it )
        circle( res, *it, radius, BLUE, thickness );
    for( it = fgdPxls.begin(); it != fgdPxls.end(); ++it )
        circle( res, *it, radius, RED, thickness );
    for( it = prBgdPxls.begin(); it != prBgdPxls.end(); ++it )
        circle( res, *it, radius, LIGHTBLUE, thickness );
    for( it = prFgdPxls.begin(); it != prFgdPxls.end(); ++it )
        circle( res, *it, radius, PINK, thickness );

    if( rectState == IN_PROCESS || rectState == SET )
        rectangle( res, Point( rect.x, rect.y ), Point(rect.x + rect.width, rect.y + rect.height ), GREEN, 2);

    imshow( *winName, res );
 	imginst = res;
}

void GCApplication::setRectInMask()
{
    CV_Assert( !mask.empty() );
    mask.setTo( GC_BGD );
    rect.x = max(0, rect.x);
    rect.y = max(0, rect.y);
    rect.width = min(rect.width, image->cols-rect.x);
    rect.height = min(rect.height, image->rows-rect.y);
    (mask(rect)).setTo( Scalar(GC_PR_FGD) );
}

void GCApplication::setLblsInMask( int flags, Point p, bool isPr )
{
    vector<Point> *bpxls, *fpxls;
    uchar bvalue, fvalue;
    if( !isPr )
    {
        bpxls = &bgdPxls;
        fpxls = &fgdPxls;
        bvalue = GC_BGD;
        fvalue = GC_FGD;
    }
    else
    {
        bpxls = &prBgdPxls;
        fpxls = &prFgdPxls;
        bvalue = GC_PR_BGD;
        fvalue = GC_PR_FGD;
    }
    if( flags & BGD_KEY )
    {
        bpxls->push_back(p);
        circle( mask, p, radius, bvalue, thickness );
    }
    if( flags & FGD_KEY )
    {
        fpxls->push_back(p);
        circle( mask, p, radius, fvalue, thickness );
    }
}

void GCApplication::mouseClick( int event, int x, int y, int flags, void* )
{
    // TODO add bad args check
    switch( event )
    {
    case EVENT_LBUTTONDOWN: // set rect or GC_BGD(GC_FGD) labels
        {
            bool isb = (flags & BGD_KEY) != 0,
                 isf = (flags & FGD_KEY) != 0;
            if( rectState == NOT_SET && !isb && !isf )
            {
                rectState = IN_PROCESS;
                rect = Rect( x, y, 1, 1 );
            }
            if ( (isb || isf) && rectState == SET )
                lblsState = IN_PROCESS;
        }
        break;
    case EVENT_RBUTTONDOWN: // set GC_PR_BGD(GC_PR_FGD) labels
        {
            bool isb = (flags & BGD_KEY) != 0,
                 isf = (flags & FGD_KEY) != 0;
            if ( (isb || isf) && rectState == SET )
                prLblsState = IN_PROCESS;
        }
        break;
    case EVENT_LBUTTONUP:
        if( rectState == IN_PROCESS )
        {
            rect = Rect( Point(rect.x, rect.y), Point(x,y) );
            rectState = SET;
            setRectInMask();
            CV_Assert( bgdPxls.empty() && fgdPxls.empty() && prBgdPxls.empty() && prFgdPxls.empty() );
            showImage();
        }
        if( lblsState == IN_PROCESS )
        {
            setLblsInMask(flags, Point(x,y), false);
            lblsState = SET;
            showImage();
        }
        break;
    case EVENT_RBUTTONUP:
        if( prLblsState == IN_PROCESS )
        {
            setLblsInMask(flags, Point(x,y), true);
            prLblsState = SET;
            showImage();
        }
        break;
    case EVENT_MOUSEMOVE:
        if( rectState == IN_PROCESS )
        {
            rect = Rect( Point(rect.x, rect.y), Point(x,y) );
            CV_Assert( bgdPxls.empty() && fgdPxls.empty() && prBgdPxls.empty() && prFgdPxls.empty() );
            showImage();
        }
        else if( lblsState == IN_PROCESS )
        {
            setLblsInMask(flags, Point(x,y), false);
            showImage();
        }
        else if( prLblsState == IN_PROCESS )
        {
            setLblsInMask(flags, Point(x,y), true);
            showImage();
        }
        break;
    }
}

int GCApplication::nextIter()
{
    if( isInitialized )
        grabCut( *image, mask, rect, bgdModel, fgdModel, 1 );
    else
    {
        if( rectState != SET )
            return iterCount;

        if( lblsState == SET || prLblsState == SET )
            grabCut( *image, mask, rect, bgdModel, fgdModel, 1, GC_INIT_WITH_MASK );
        else
            grabCut( *image, mask, rect, bgdModel, fgdModel, 1, GC_INIT_WITH_RECT );

        isInitialized = true;
    }
    iterCount++;

    bgdPxls.clear(); fgdPxls.clear();
    prBgdPxls.clear(); prFgdPxls.clear();

    return iterCount;
}

GCApplication gcapp;

static void on_mouse( int event, int x, int y, int flags, void* param )
{
    gcapp.mouseClick( event, x, y, flags, param );
}

void help() {
    printf("Usage: grabcut <file_name> <?output_file_prefix>\n");
    return;
}

int main(int argc, char** argv)
{
    string file_prefix = "out";
    if (argc < 2) {
        help();
        return 1;
    }
    string filename = argv[1];
    if (argc == 3) {
        file_prefix = argv[2];
    }
    if (filename.empty()) {
        cout << "Error in reading the file: " << argv[1] << endl;
        return 1;
    }

    Mat image = imread(filename, 1),
        orig = image.clone();

    if (image.empty()) {
        cout << "Image not found, " << filename << endl;
        return 1;
    }

    
    string test = "Output-Image";
    const string winName = "Original-Image";
    Mat output[3];
  
    // namedWindow(test, WINDOW_NORMAL);
    addNamedWindow(winName, WINDOW_NORMAL);
    
    imshow(test, image);
    waitKey(0);

    setMouseCallback(winName, on_mouse, 0);

    gcapp.setImageAndWinName( image, winName );
    gcapp.showImage();
    addNamedWindow("cartoon", WINDOW_NORMAL);
    addNamedWindow("pencil", WINDOW_NORMAL);
    addNamedWindow("sepia", WINDOW_NORMAL);
                        
    while(true) {
        char c = waitKey(0);
        switch (c) {
            case '\x1b':
                cout << "Exiting ..." << endl;
                destroyWindow(winName); destroyWindow(test);
                return 0;
            case 'r':
                gcapp.reset(); gcapp.showImage();
                
                imshow(test, imginst);
                // waitKey(0);
                break;
            case 'a':
                imwrite(file_prefix + "-cartoon.png",output[0]);
                imwrite(file_prefix + "-pencil.png",output[1]);
                imwrite(file_prefix + "-sepia.png",output[2]);
                cout << "Sucessfully saved all output images with file_prefix: " << file_prefix << endl;
                break;
            case 'n':
                int iterCount = gcapp.getIterCount();
                cout << "<" << iterCount << "... ";
                int newIterCount = gcapp.nextIter();
                if (newIterCount > iterCount) {
                    gcapp.showImage();
                    cout << "Result:" << endl;
                    // GaussianBlur(image, orig, Size( 53, 53 ), 0, 0);
                    // medianBlur(image, orig, 43);
                    // bilateralFilter (image, orig, 5, 5*2, 5/2);
                    for (int i = 0; i < 3; ++i) {
                        final = orig.clone();
                        if (i == 0) {
                            cartooniser(final, final);
                            test = "cartoon";
                        } else if(i == 1) {
                            pencil(final, final);
                            test = "pencil";
                        } else {
                            sepia(final, final);
                            test = "sepia";
                        }
                        int nr = image.rows, nc = image.cols;
                        for (int y = 0; y < nr; y++) {
                            for (int x = 0; x < nc; x++) {
                                Vec3b colornb = imginst.at<Vec3b>(Point(x,y));
                                Vec3b fp = final.at<Vec3b>(Point(x,y));
                                if((colornb[0] || colornb[1] || colornb[2]) && !(colornb[0] == 0 && colornb[1] == 255 && colornb[2] == 0)) {
                                    fp[0] = colornb[0];
                                    fp[1] = colornb[1];
                                    fp[2] = colornb[2];
                                }
                                // cout<< "val : " << format(color, Formatter::FMT_PYTHON) << endl;
                                final.at<Vec3b>(Point(x,y)) = fp;
                            }
                        }
                        imshow(test, final);
                        output[i] = final;
                    }
                    cout << iterCount << ">" << endl;
                } else {
                    cout << "Rect must be determined>" << endl;
                }
            break;
        }
    }
    return 0;
}
