#pragma once
#ifdef MACHINEVISIONLIB_EXPORTS
#define FACERECOGNITION_API __declspec(dllexport)
#else
#define FACERECOGNITION_API __declspec(dllimport)
#endif

#include "caffe/caffe.hpp"

class FACERECOGNITION_API FaceDatabase {
public:
	FaceDatabase() {
#if NEED_GPU
		caffe::Caffe::set_mode(caffe::Caffe::GPU);
#else
		caffe::Caffe::set_mode(caffe::Caffe::CPU);
#endif
	}

	BOOL LoadCaffeVGGNet(string strCaffePrototxtFile, string strCaffeModelFile);

	~FaceDatabase(){}

	caffe::Net<FLOAT> *caflNet;
};
