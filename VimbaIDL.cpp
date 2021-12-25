
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

bool isHandleAllocated = false;

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

	void copy_image(const UINT16* idlbuffer)
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

	char message[100];
	sprintf_s(message, "Camera %s", cameraID.c_str());
	INFO::info(message);

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
	Preview::isHandleAllocated = true;

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

int set_exposure(const double value) // [microsecond]
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

	return 1;
}

int get_preview_frame()
{
	//if (!checkReady()) return 0;
	return 1;
}



struct STime {
	UINT16 hour;
	UINT16 minute;
	UINT16 second;
	UINT16 milisecond;
	UINT16 microsecond;
	UINT16 nanoosecond;
};

STime timestamp_to_STime(const VmbUint64_t timestamp)
{

	UINT64 factor = 1000 * 1000 * 1000;
	UINT64 total_seconds = timestamp / factor;
	UINT64 total_nanoseconds = timestamp / factor;

	UINT64 hour   = total_seconds / 60 / 60;
	UINT64 minute = (total_seconds - hour*60*60) / 60;
	UINT64 second = total_seconds - hour * 60 * 60 - minute * 60;

	UINT64 milisecond  = total_nanoseconds / 1000 / 1000;
	UINT64 microsecond = (total_nanoseconds - milisecond * 1000 * 1000) / 1000;
	UINT64 nanosecond  = total_nanoseconds - milisecond * 1000 * 1000 - microsecond * 1000;

	STime st;
	st.hour        = (UINT16) hour;
	st.minute      = (UINT16) minute;
	st.second      = (UINT16) second;
	st.milisecond  = (UINT16) milisecond;
	st.microsecond = (UINT16) microsecond;
	st.nanoosecond = (UINT16) nanosecond;

	return std::move(st);
}

}

std::string getIDLString(const void* argv)
{
	IDL_STRING* s_ptr = (IDL_STRING *)argv;
	std::string str(s_ptr[0].s, s_ptr[0].s + s_ptr[0].slen);
	
	return std::move(str);
}

/****************************************************************/
/*  camera connection
/*  IDL>  x=call_external(dll,'VimbaInit', "Z:\\conf\\Vimba\\demo.20211216.xml")
/****************************************************************/

int IDL_STDCALL VimbaInit(int argc, void* argv[])
{
	if (!Vhida::init_sys()) return 0;
	
	std::string cameraID = "";
	if (!Vhida::init_camera(cameraID)) return 0;
	
	if (!Vhida::init_nPLS()) return 0;
	
	//std::string xmlFile = "Z:\\conf\\Vimba\\demo.20211216.xml";
	auto xmlFile = getIDLString(argv[0]);
	if (!Vhida::load_configuration(xmlFile)) return 0;

	if (!Vhida::set_maximum_gev_packet()) return 0;

	char message[100];
	sprintf_s(message, "Camera %s initialized", cameraID.c_str());
	INFO::info(message);
	
	return 1;
}

/****************************************************************/
/*  camera disconnection
/*  IDL>  x=call_external(dll,'VimbaClose')
/****************************************************************/

int IDL_STDCALL VimbaClose(int argc, void* argv[])
{
	if (!Vhida::close_camera()) return 0;
	if (!Vhida::close_sys()) return 0;

	INFO::info("Camera disconnected");
	return 1;
}

/****************************************************************/
/*  set camera exposure
/*  IDL>  x=call_external(dll,'SetVimbaExpoTime',50.,/all_value,/cdecl)
/****************************************************************/

int IDL_STDCALL SetVimbaExpoTime(int argc, float argv[])
{
	double expotime;         // [miliseconds]
	//double expotime_current; // [miliseconds]

	expotime = (double)argv[0];

	if (!Vhida::set_exposure(expotime*1000.)) return 0;

	return 1;
}

/****************************************************************/
/*  set camera framerate
/*  IDL>  x=call_external(dll,'SetVimbaFrameRate', 20.,/all_value,/cdecl)
/****************************************************************/

int IDL_STDCALL SetVimbaFrameRate(int argc, float argv[])
{
	double framerate;         // [hz]
	//double framerate_current; // [hz]

	framerate = (double)argv[0];

	if (!Vhida::set_framerate(framerate)) return 0;

	return 1;
}



/****************************************************************/
/*  Preview functions
/*
/*  - before while loop, to start asynchronous capturing
/*  IDL>  x=call_external(dll,'StartVimbaPreview')
/*
/*  - inside while loop, to get current image
/*  IDL>  x=call_external(dll,'GetVimbaPreview',img)
/*
/*  - inside while loop, to close asynchronous capturing
/*  IDL>  x=call_external(dll,'StopVimbaPreview')
/****************************************************************/

int IDL_STDCALL StartVimbaPreview(int argc, void* argv[])
{
	if (!Vhida::init_preview()) return 0;
	return 1;
}


int IDL_STDCALL GetVimbaPreview(int argc, void* argv[])
{
	UINT16* idlBuffer = (UINT16 *)argv[0];

	Vhida::pPreviewHandler->copy_image(idlBuffer);

	return 1;
}

int IDL_STDCALL StopVimbaPreview(int argc, void* argv[])
{
	if (!Vhida::close_preview()) return 0;
	return 1;
}

/****************************************************************/
/*  camera grab image sequence (synchronously) with header
/*  IDL>  x=call_external(dll,'VimbaObs', nimg, imgs, times, framerate)
/****************************************************************/


int IDL_STDCALL VimbaObs(int argc, void* argv[])
{
	UINT16 nFrame = (UINT16) *((UINT16*)argv[0]);

	UINT16* idlBuffer = (UINT16*)argv[1];              // idl buffer for 3d image array
	UINT16* idlBuffer_time = (UINT16*)argv[2];         // idl buffer for timestamp 2d array
	float*  idlBuffer_framerate = (float *)argv[3];    // idl buffer for framerate 1d array

	//: acquire image
	Vhida::vimba::FramePtrVector pFrames(nFrame);
	if (!Vhida::grab_multiframe(pFrames)) return 0;

	VmbUint32_t imageSize = Vhida::pPreviewHandler->imageSize;

	// get framerate
	double frate;
	Vhida::get_framerate(frate);

	int nTimeParam = 6;
	//: copy memory data
	for (int i=0; i<nFrame; ++i) 
	{
		Vhida::vimba::FramePtr frame = pFrames.at(i);

		// get image buffer
		VmbUchar_t* pimage;
		frame->GetImage(pimage);

		// get timestamp
		VmbUint64_t timestamp;
		frame->GetTimestamp(timestamp);

		

		// copy image memory
		memcpy( (void*)(idlBuffer+i * imageSize * 2), (void*)(pimage) , imageSize);

		// copy timestamp memory
		auto st = Vhida::timestamp_to_STime(timestamp);
		*(idlBuffer_time + i * nTimeParam + 0) = st.hour;
		*(idlBuffer_time + i * nTimeParam + 1) = st.minute;
		*(idlBuffer_time + i * nTimeParam + 2) = st.second;
		*(idlBuffer_time + i * nTimeParam + 3) = st.milisecond;
		*(idlBuffer_time + i * nTimeParam + 4) = st.microsecond;
		*(idlBuffer_time + i * nTimeParam + 5) = st.microsecond;


		// copy framerate memory
		*(idlBuffer_framerate + i) = (float) frate;
	}


	return 1;

}



