
/*
History:

2021.12.20    u.k.    project initialized

*/

#include "pch.h"
#include "stdio.h"
#include "idl_export.h"
#include <malloc.h>


#include <vector>
#include <chrono>
#include <thread>
#include <atomic>

#include "VimbaCPP/Include/VimbaCPP.h"



/****************************************************************/
/*  test
/****************************************************************/
int IDL_STDCALL Hello(int argc, void* argv[])
{

	{
		MessageBox(NULL, "Hello", "Hello", MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
		return 0;
	}

}

/****************************************************************/
/*  common non-camera functions
/****************************************************************/
void sleep_milisecond(int milisecond)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(milisecond));
}

/****************************************************************/
/*  error / info functions
/****************************************************************/

namespace INFO
{
	inline void error(const char * text)
	{
		MessageBox(NULL, text, "Error", MB_OK | MB_ICONERROR | MB_TASKMODAL);
	}

	inline void info(const char * text)
	{
		MessageBox(NULL, text, "Info", MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
	}
}

/****************************************************************/
/*  vimba for hida observation
/****************************************************************/
namespace Vhida
{


namespace vimba = AVT::VmbAPI;


namespace Preview 
{

//: handler to store the current frame during preview
class Handler
{
public:
	Handler(VmbInt64_t my_nPLS)
	{
		//: register frame for asynchronous acuisition
		async_pframe.reset( new vimba::Frame(my_nPLS) );
		
		//: allocate memory space to the buffer
		async_pframe->GetImageSize(imageSize);
		pimage = new VmbUchar_t[imageSize];
	}

	~Handler() 
	{
		// release memory space of buffer
		delete[] pimage;
	}

	void set_image(const vimba::FramePtr pframe)
	{
		do {
			sleep_milisecond(1);
		} while (isBusy.load());
		
		isBusy.store(true);
		
		VmbUchar_t* buffer;
		pframe->GetImage(buffer);
		memcpy((void*)pimage, (void*)buffer, imageSize);

		isBusy.store(false);
	}

	void copy_image(const VmbUchar_t* idlbuffer)
	{
		do {
			sleep_milisecond(1);
		} while (isBusy.load());

		isBusy.store(true);
		
		memcpy((void*)idlbuffer, (void*)pimage, imageSize);

		isBusy.store(false);
	}


	std::atomic<bool> isBusy=false;
	VmbUint32_t imageSize;
	VmbUchar_t* pimage;
	vimba::FramePtr async_pframe;
};

/****************************************************************/
/*  frameobserver for preview
/****************************************************************/
class FrameObserver : virtual public vimba::IFrameObserver
{
public:
	FrameObserver(vimba::CameraPtr pCamera, std::shared_ptr<Handler> pHandler) : vimba::IFrameObserver(pCamera)
	{
		m_pHandler = pHandler;
	}

	std::shared_ptr<Handler> m_pHandler;

	void FrameReceived(const vimba::FramePtr pFrame)
	{
		//: copy to handler
		m_pHandler->set_image(pFrame);

		//: send pFrame back to queue to recieve next frame buffer -> asynchronous
		m_pCamera->QueueFrame(pFrame);
	}
};

}


namespace Observation
{

/****************************************************************/
/*  frameobserver for observation
/****************************************************************/
class FrameObserver : virtual public vimba::IFrameObserver
{
public:
	FrameObserver(vimba::CameraPtr pCamera) : vimba::IFrameObserver(pCamera) {}

	void FrameReceived(const vimba::FramePtr pFrame)
	{
		//: might do some process here


		//: during observation, we mainly perform multi-frame sequential capturing -> synchronous
		//: don't send pFrame back to queue to recieve next frame buffer
		//m_pCamera->QueueFrame(pFrame);
	}
};

}



/****************************************************************/
/*  
/****************************************************************/


//: get a reference to the vimba system api
vimba::VimbaSystem& sys = vimba::VimbaSystem::GetInstance();
std::atomic<bool> flag_sys = false;

//: camera pointer
vimba::CameraPtr pCamera = vimba::CameraPtr();
std::atomic<bool> flag_cam = false;

//: preview handler 
//std::auto_ptr<Preview::Handler> pPreviewHandler;
std::shared_ptr<Preview::Handler> pPreviewHandler;
//Preview::Handler* pPreviewHandler;

//: payload size value
VmbInt64_t nPLS;



bool checkSuccess(VmbErrorType err, const char * text)
{
	if (err != VmbErrorSuccess) {

		std::string fullText("Failed in :");
		fullText += std::string(text);

		INFO::error(fullText.c_str());
		return false;
	}
	return true;
}

bool checkReady()
{
	if (!flag_sys) {
		INFO::error("Vimba System not ready");
		return false;
	}
	if (!flag_cam) {
		INFO::error("Camera not ready");
		return false;
	}
	return true;
}

int init_sys()
{
	if (!checkSuccess(sys.Startup(), "init VimbaSystem")) return 0;
	flag_sys = true;
	return 1;
}

int init_camera(std::string & cameraID)
{

	if (!flag_sys) {
		INFO::error("Vimba System not ready");
		return 0;
	}

	
	if (cameraID.empty()) 
	{
		vimba::CameraPtrVector cameras;
		
		if (!checkSuccess(sys.GetCameras(cameras), "searching available camera")) return 0;

		if (cameras.empty()) 
		{
			INFO::error("no cameras found");
			return 0;
		}
		
		if (!checkSuccess(cameras[0]->Open(VmbAccessModeFull), "open camera")) return 0;

		pCamera = cameras[0];
		if (!checkSuccess(pCamera->GetID(cameraID), "get current camera ID")) return 0;
	}
	else
	{
		if (!checkSuccess(sys.OpenCameraByID(cameraID.c_str(), VmbAccessModeFull, pCamera), "open camera by ID"))
			return 0;
	}

	flag_cam = true;
	return 1;
}

int close_camera()
{
	if (!flag_cam) {
		INFO::error("flag_cam is flase before closing camera");
		//return 0;
	}
	if (checkSuccess(pCamera->Close(), "close camera")) return 0;
	flag_cam = false;
	return 1;
}

int close_sys()
{
	if (flag_cam) {
		INFO::error("flag_cam is true. close camera before closing sys");
		close_camera();
	}

	if (!flag_sys) {
		INFO::error("flag_sys is flase before closing sys");
		//return 0;
	}
	if (checkSuccess(sys.Shutdown(), "close sys")) return 0;
	flag_sys = false;

	// release preview handler
	//delete pPreviewHandler;

	return 1;
}

int init_nPLS()
{
	vimba::FeaturePtr pFeature;

	//: Get the image size for the required buffer
	if (checkSuccess(pCamera->GetFeatureByName("PayloadSize", pFeature),"get PayloadSize feature")) return 0;
	if (checkSuccess(pFeature->GetValue(nPLS),"get PayloadSize")) return 0;

	//pPreviewHandler = new Preview::Handler(nPLS);
	pPreviewHandler = std::make_shared<Preview::Handler>(nPLS);

	return 1;
}

int load_configuration(const std::string xmlFile)
{
	if (!checkReady()) return 0;

	VmbFeaturePersistSettings_t settingsStruct;
	checkSuccess(pCamera->LoadCameraSettings(xmlFile, &settingsStruct), "loading xml configuration file");

	return 1;
}

int set_maximum_gev_packet()
{
	if (!checkReady()) return 0;

	vimba::FeaturePtr pFeature;
	if (!checkSuccess(pCamera->GetFeatureByName("GVSPAdjustPacketSize", pFeature), "get GeV packet feature"))
		return 0;

	if (!checkSuccess(pFeature->RunCommand(), "set GeV packet feature")) return 0;
	
	{
		bool bIsCommandDone = false;
		do
		{
			if (VmbErrorSuccess != pFeature->IsCommandDone(bIsCommandDone)) break;
		} while (false == bIsCommandDone);

		INFO::info("GVSPAdjustPacketSize RunCommand completed");
	}

	return 1;
}

int set_exposure(const double value)
{
	if (!checkReady()) return 0;

	vimba::FeaturePtr pFeature;
	if (!checkSuccess(pCamera->GetFeatureByName("ExposureTime", pFeature), "get exposure time feature")) return 0;
	
	if (!checkSuccess(pFeature->SetValue(value), "set exposure time")) return 0;

	double current_value;
	if (!checkSuccess(pFeature->GetValue(current_value), "get exposure time")) return 0;

	char message[100];
	sprintf_s(message, "Exposure = %.2f [ms]", current_value/1000.);
	INFO::info(message);

	return 1;
}

int get_exposure(double & value)
{
	if (!checkReady()) return 0;

	vimba::FeaturePtr pFeature;
	if (!checkSuccess(pCamera->GetFeatureByName("ExposureTime", pFeature), "get exposure time feature")) return 0;
	if (!checkSuccess(pFeature->GetValue(value), "get exposure")) return 0;

	return 1;
}

// normally framerate is automatically set to 1/exposure, might deviate
int set_framerate(const double value)
{
	if (!checkReady()) return 0;

	vimba::FeaturePtr pFeature;
	if (!checkSuccess(pCamera->GetFeatureByName("AcquisitionFrameRate", pFeature), "get framerate feature")) return 0;
	
	if (!checkSuccess(pFeature->SetValue(value), "set framerate")) return 0;

	double current_value;
	if (!checkSuccess(pFeature->GetValue(current_value), "get framerate")) return 0;

	char message[100];
	sprintf_s(message, "framerate = %.2f [hz]", current_value);
	INFO::info(message);

	return 1;
}

int get_framerate(double & value)
{
	if (!checkReady()) return 0;

	vimba::FeaturePtr pFeature;
	if (!checkSuccess(pCamera->GetFeatureByName("AcquisitionFrameRate", pFeature), "get framerate feature")) return 0;
	if (!checkSuccess(pFeature->GetValue(value), "get framerate")) return 0;

	return 1;
}

int set_acquisition(const char * mode)
{
	if (!checkReady()) return 0;

	vimba::FeaturePtr pFeature;
	if (!checkSuccess(pCamera->GetFeatureByName("AcquisitionMode", pFeature), "get acquisition mode feature")) return 0;

	char message[100];
	sprintf_s(message, "set acuisition mode %s", mode);
	if (checkSuccess(pFeature->SetValue(mode), message)) return 0;

	return 1;
}



int grab_multiframe(vimba::FramePtrVector & pFrames)
{
	if (!checkReady()) return 0;

	//: we init nPLS at camera initialization

	//: payload size value
	//VmbInt64_t nPLS;
	vimba::FeaturePtr pFeature;

	//: Get the image size for the required buffer
	//pCamera->GetFeatureByName("PayloadSize", pFeature);
	//pFeature->GetValue(nPLS);

	//: we set AcquisitionMode to continuous ahead (at camera initialization?)

	for (vimba::FramePtrVector::iterator iter = pFrames.begin(); pFrames.end() != iter; ++iter)
	{
		//: Allocate memory for frame buffer
		(*iter).reset(new vimba::Frame(nPLS));
		//: Register frame observer/callback for each frame
		(*iter)->RegisterObserver(vimba::IFrameObserverPtr(new Observation::FrameObserver(pCamera)));
		//: announce frame to the API
		pCamera->AnnounceFrame(*iter);
	}

	//: start the capture engine (API)
	pCamera->StartCapture();
	
	//: put frame into the frame queue
	for (vimba::FramePtrVector::iterator iter = pFrames.begin(); pFrames.end() != iter; ++iter)
		pCamera->QueueFrame(*iter);

	//: Start the acuisition engine (camera)
	pCamera->GetFeatureByName("AcquisitionStart", pFeature);
	pFeature->RunCommand();


	//: ... frame being filled asynchronously

	//: wait for complete
	for (vimba::FramePtrVector::iterator iter = pFrames.begin(); pFrames.end() != iter; ++iter)
	{
		VmbFrameStatusType eReceiveStatus;
		do {
			(*iter)->GetReceiveStatus(eReceiveStatus);
			sleep_milisecond(2);
		} while (eReceiveStatus != VmbFrameStatusComplete);
	}

	//: Stop the acquisition engine(camera)
	pCamera->GetFeatureByName("AcquisitionStop", pFeature);
	pFeature->RunCommand();
	//: Stop the capture engine (API) (correspond to pCamera->StartCapture )	pCamera->EndCapture();	//: flush the frame queue (correspond to pCamera->QueueFrame )	pCamera->FlushQueue();	//: revoke all frame from the API (correspond to pCamera->AnnounceFrame )	pCamera->RevokeAllFrames();	// unregister the frame observer	for (vimba::FramePtrVector::iterator iter = pFrames.begin(); pFrames.end() != iter; ++iter)		(*iter)->UnregisterObserver();

	return 1;
}

int init_preview()
{
	if (!checkReady()) return 0;

	//:Register frame observer/callback for each frame
	pPreviewHandler->async_pframe->RegisterObserver(vimba::IFrameObserverPtr(new Preview::FrameObserver(pCamera, pPreviewHandler)));
	//: announce frame to the API
	pCamera->AnnounceFrame(pPreviewHandler->async_pframe);

	//: start the capture engine (API)
	pCamera->StartCapture();

	//: put frame into the frame queue
	pCamera->QueueFrame(pPreviewHandler->async_pframe);

	//: Start the acuisition engine (camera)
	vimba::FeaturePtr pFeature;
	pCamera->GetFeatureByName("AcquisitionStart", pFeature);
	pFeature->RunCommand();

	return 1;
}

int close_preview()
{
	if (!checkReady()) return 0;

	//: Stop the acquisition engine(camera)
	vimba::FeaturePtr pFeature;
	pCamera->GetFeatureByName("AcquisitionStop", pFeature);
	pFeature->RunCommand();

	//: Stop the capture engine (API) (correspond to pCamera->StartCapture )	pCamera->EndCapture();	//: flush the frame queue (correspond to pCamera->QueueFrame )	pCamera->FlushQueue();	//: revoke all frame from the API (correspond to pCamera->AnnounceFrame )	pCamera->RevokeAllFrames();

	pPreviewHandler->async_pframe->UnregisterObserver();
}

int get_preview_frame()
{
	//if (!checkReady()) return 0;
}



}






