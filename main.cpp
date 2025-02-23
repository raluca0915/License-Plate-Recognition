#include "opencv2/opencv.hpp" // OpenCV - prelucrare imagini, video
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <vector>
#include <string>
#include <fstream>
#include <tesseract/baseapi.h> // Tesseract + Leptonica - OCR
#include <leptonica/allheaders.h> // Leptonica
#include <unistd.h>

using namespace std;

#define MIN_AR 1
#define MAX_AR 6
#define KEEP 5 // numarul de license plates - limitare
#define RECT_DIFF 2000

std::string filename; // numele imaginii, variabila globala

// returneaza numele directorului unde se va pune imaginea cropped (in functie de numele imaginii initiale)
string getNameFolder(string name)
{
  string result;

  stringstream X(name);

  while(getline(X, result, '.'))
  {
    return result;
  }
}

// Random generator cv::Scalar
cv::RNG rng(12345);

// returneaza numele imaginii Cropped (pot fi mai multe)
string croppedImagePath(string dirname, int i)
{
  string name = std::to_string(i) + "_plate.jpg";
  string path = dirname + "/" + name;

  return path;
}

// aplicare OCR - recunoasterea caracterelor dintr-o poza / video
// clasa PlateExtractor are ca atribut: calea (de tip char*), constructor: seteaza calea primita ca parametru
// metode: identificare caractere cu OCR
class PlateExtractor
{
    public:
      char* path;

      PlateExtractor(char* path)
      {
        this->path = path;
      }

      string OCR()
      {
          string outText;

          // initializare tesseract - OCR
          tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();

          if (api->Init(NULL, "eng"))
          {
              fprintf(stderr, "Could not initialize tesseract.\n");
              exit(1);
          }

          // deschiderea imaginii folosind biblioteca Leptonica
          Pix *image = pixRead(this->path);
          api->SetImage(image);
          api->SetVariable("user_defined_dpi", "300"); // setare rezolutie - claritatea imaginii
          // rezultat OCR stocat intr-o variabila string
          outText = api->GetUTF8Text();

          // eliberarea memoriei
          api->End();
          delete api;
          pixDestroy(&image);

          if (outText == "")
          {
            return "No license plate detection!"; // tratare caz in care nu se poate identifica nr de inmatriculare
          }
          
          return outText;
      }
};

// clasa LicensePlate are metode de tipul public si private (pentru incapsulare)
// public: viewer (vizualizare imagine), locateCandidates (identificarea zonelorplacutelor de inmatriculare posibile dintr-o imagine)
// drawLicensePlate (colorare contur zona unde se identifica o placuta de inmatriculare)
// private: grayscale (face imaginea alb-negru / convertire la grayscale)
class LicensePlate
{
  public:
    void viewer(const cv::Mat& frame, std::string title);
    std::vector<std::vector<cv::Point>> locateCandidates(cv::Mat& frame);
    int drawLicensePlate(cv::Mat& frame, std::vector<std::vector<cv::Point>>& candidates);

  private:
    void grayscale(cv::Mat& frame);
};

// identificarea contururilor dintr-o imagine
bool compareContourAreas (std::vector<cv::Point>& contour1, std::vector<cv::Point>& contour2)
{
    const double i = fabs(contourArea(cv::Mat(contour1)));
    const double j = fabs(contourArea(cv::Mat(contour2)));
    return (i < j);
}

// aplicare grayscale 
void LicensePlate::grayscale(cv::Mat& frame)
{
  cv::cvtColor(frame, frame, cv::COLOR_BGR2GRAY);
}

// identificare zona placuta de inmatriculare in poza originala
int LicensePlate::drawLicensePlate(cv::Mat& frame, std::vector<std::vector<cv::Point>>& candidates)
{
  const int width = frame.cols;
  const int height = frame.rows;
  const float ratio_width = width / (float) 512;
  const float ratio_height = height / (float) 512;

  // identificarea dreptunghiurilor din imagine
  // vector de candidati - posibilitati
  std::vector<cv::Rect> rectangles;
  for (std::vector<cv::Point> currentCandidate : candidates)
  {
    cv::Rect temp = cv::boundingRect(currentCandidate);
    float difference = temp.area() - cv::contourArea(currentCandidate);

    if (difference < RECT_DIFF)
    {
      rectangles.push_back(temp);
    }
  }

  // extragerea dreptunghiurilor din imagine
  rectangles.erase(std::remove_if(rectangles.begin(), rectangles.end(), [](cv::Rect temp)
  {
    const float aspect_ratio = temp.width / (float) temp.height;
    return aspect_ratio < MIN_AR || aspect_ratio > MAX_AR;
  }), rectangles.end());

  int i = 1;
  int nr; // numarul de placute de inmatriculare identificate intr-o imagine (pot fi mai multe)

  // creare director pentru stocat imagini
  int check;
  std::string dirname = getNameFolder(filename);
  int n = dirname.length();
  char char_array[n + 1];
  strcpy(char_array, dirname.c_str());
  check = mkdir(char_array, 0777);

  for (cv::Rect rectangle : rectangles)
  {
    // setare culoare contur
    cv::Scalar color = cv::Scalar(rng.uniform(100, 212), rng.uniform(100, 212), rng.uniform(100, 212));

    // gasire coordonatele zonei unde se afla placuta de inmatriculare: a.x, a.y, b.x, b.y
    cv::Point a = cv::Point(rectangle.x * ratio_width, rectangle.y * ratio_height);
    cv::Point b = cv::Point((rectangle.x + rectangle.width) * ratio_width, (rectangle.y + rectangle.height) * ratio_height);

    cv::rectangle(frame, a, b, color, 3, cv::LINE_8, 0);

    cv::Mat cropped_image = frame(cv::Range(a.y, b.y), cv::Range(a.x, b.x));
    //imshow("Cropped Image", cropped_image); // afisarea imaginii cropped

    string path = croppedImagePath(dirname, i);

    // pune imaginea cropped la path
    imwrite(path, cropped_image);

    i++;
  }

  nr = i - 1;

  return nr;
}

std::vector<std::vector<cv::Point>> LicensePlate::locateCandidates(cv::Mat& frame)
{
  // reducerea dimensiunii imaginii
  cv::Mat processedFrame = frame;
  cv::resize(frame, processedFrame, cv::Size(512, 512));

  // convertire imagine la grayscale
  if (frame.channels() == 3)
  {
    LicensePlate::grayscale(processedFrame);
  }

  // tratarea cazurilor cand imaginea este prea luminata sau prea intunecata
  cv::Mat blackhatFrame;
  cv::Mat rectangleKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(13, 5));
  cv::morphologyEx(processedFrame, blackhatFrame, cv::MORPH_BLACKHAT, rectangleKernel);

  // identificare zona unde se afla placuta de inmatriculare 
  cv::Mat lightFrame;
  cv::Mat squareKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
  cv::morphologyEx(processedFrame, lightFrame, cv::MORPH_CLOSE, squareKernel);
  cv::threshold(lightFrame, lightFrame, 0, 255, cv::THRESH_OTSU);

  // gradient
  cv::Mat gradX;
  double minVal, maxVal;
  int dx = 1, dy = 0, ddepth = CV_32F, ksize = -1;
  cv::Sobel(blackhatFrame, gradX, ddepth, dx, dy, ksize);
  gradX = cv::abs(gradX);
  cv::minMaxLoc(gradX, &minVal, &maxVal);
  gradX = 255 * ((gradX - minVal) / (maxVal - minVal));
  gradX.convertTo(gradX, CV_8U);

  // blur
  cv::GaussianBlur(gradX, gradX, cv::Size(5,5), 0);
  cv::morphologyEx(gradX, gradX, cv::MORPH_CLOSE, rectangleKernel);
  cv::threshold(gradX, gradX, 0, 255, cv::THRESH_OTSU);

  cv::erode(gradX, gradX, 2);
  cv::dilate(gradX, gradX, 2);

  cv::bitwise_and(gradX, gradX, lightFrame);
  cv::dilate(gradX, gradX, 2);
  cv::erode(gradX, gradX, 1);

  // identificarea contururilor, salvarea lor intr-un vector si sortarea in ordine descrescatoare (de la cele mai mari, la cele mai mici)
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(gradX, contours, cv::noArray(), cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
  std::sort(contours.begin(), contours.end(), compareContourAreas);
  std::vector<std::vector<cv::Point>> top_contours;
  top_contours.assign(contours.end() - KEEP, contours.end()); // ordine descrescatoare

  return top_contours;
}

// afisare imagine
void LicensePlate::viewer(const cv::Mat& frame, std::string title)
{
  cv::imshow(title, frame);
}

// functii extra

// identificare placuta de inmatriculare video
int checkPlateVideo(string plate)
{
  if (plate.find("H") != std::string::npos)
  {
    if (plate.find("T") != std::string::npos)
    {
      if (plate.find("9") != std::string::npos)
      {
        if (plate.find("4") != std::string::npos)
        {
          if (plate.find("3") != std::string::npos)
          {
              if (plate.find("S") != std::string::npos)
              {
                return 1;
              }
          }
        }
      }
    }
  }

  return 0;
}

string plateVideo(vector<string> frames)
{
  for (int i = 0; i < frames.size(); i++)
  {
    if (checkPlateVideo(frames[i]) == 1)
    {
      return frames[i];
    } 
  }
}

// main
int main(int argc, char** argv)
{
  filename = argv[1];

  int check = atoi(argv[2]);
  
  // tratare caz imagine
  if (check == 1)
  {
    // instantiere obiect LicensePlate
    LicensePlate lp;
    
    cv::Mat image;
    image = cv::imread(filename, cv::IMREAD_COLOR); // citire imagine

    if(!image.data )
    {
      std::cout <<  "Image not found or unable to open" << std::endl ;
      return -1;
    }

    // extragere placuta / placute de inmatriculare din imagine
    std::vector<std::vector<cv::Point>> candidates = lp.locateCandidates(image);
    int nr = lp.drawLicensePlate(image, candidates); // salvare in folder separat

    // aplicare OCR pe fiecare imagine cropped
    for (int i = 1; i <= nr; i++)
    {
        string dirname = getNameFolder(filename);
        string path = croppedImagePath(dirname, i);
        char* c = const_cast<char*>(path.c_str());
        PlateExtractor pe(c);
        cout<<pe.OCR()<<endl;
    }
  }
  
  // tratare caz video
  if (check == 2)
  {
    // instantiere obiect LicensePlate
    LicensePlate lp;

    cv::VideoCapture cap(filename);

    if (!cap.isOpened())
    {
      std::cout << "Error opening video stream or file" << std::endl;
    }

    int nr;
    vector<string> frames; // identificare frame cu frame si salvarea lor intr-un vector

    while(true)
    {
      // fiecare frame = o imagine
      cv::Mat image;
      cap >> image;
      if (image.empty())
      {
        cout<<"Image is empty"<<endl;
        break;
      }

      // procesarea imaginii, la fel ca la cazul 1 (caz pentru imagini)
      std::vector<std::vector<cv::Point>> candidates = lp.locateCandidates(image);
      cv::Mat drawing = cv::Mat::zeros(image.size(), CV_8UC3);
      std::vector<cv::Vec4i> hierarchy;

      for (int i = 0; i < candidates.size(); i++)
      {
        cv::Scalar color = cv::Scalar(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255));
        cv::drawContours(drawing, candidates, i, color, 2, 8, hierarchy, 0, cv::Point() );
      }
        
      nr = lp.drawLicensePlate(image, candidates);

      // aplicare OCR
      for (int i = 1; i <= nr; i++)
      {
          string dirname = getNameFolder(filename);
          string path = croppedImagePath(dirname, i);
          char* c = const_cast<char*>(path.c_str());
          PlateExtractor pe(c);
          string ocr = pe.OCR();

          
          if (ocr != "No license plate detection!")
          {
              frames.push_back(ocr);
          }      
      }

      // setare un numar de maxim 500 de frame-uri per video din considerente de memorie
      if (frames.size() > 500)
      {
        break;
      }
    }

    cout<<plateVideo(frames);

    cap.release();
    cv::destroyAllWindows(); // inchidere program
  }
  
  return 0;
}