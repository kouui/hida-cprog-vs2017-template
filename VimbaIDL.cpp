
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

namespace vimba = AVT::VmbAPI;


//: get a reference to the vimba system api
vimba::VimbaSystem& sys = vimba::VimbaSystem::GetInstance();
std::atomic<bool> flag_sys = false;

//: camera pointer
vimba::CameraPtr pCamera;
std::atomic<bool> flag_cam = false;

VmbUint32_t TIMEOUT = 20 * 1000;

std::atomic<UINT16> anFrame;
std::atomic<UINT16> acFrame;


bool checkSuccess(VmbErrorType err, const char * text)
{
	if (err != VmbErrorSuccess) {

		std::string fullText("Failed in : ");
		fullText += std::string(text);

		INFO::error(fullText.c_str());
		return false;
	}
	return true;
}

VmbUint32_t get_image_size(const vimba::CameraPtr tmp_pcamera)
{
	vimba::FramePtr pframe;
	tmp_pcamera->AcquireSingleImage(pframe, TIMEOUT);
	VmbUint32_t image_size;
	pframe->GetImageSize(image_size);

	return image_size; // byte
}

VmbInt64_t get_nPLS(const vimba::CameraPtr tmp_pcamera)
{
	vimba::FeaturePtr pFeature;
	VmbInt64_t nPLS;
	//: Get the image size for the required buffer
	if (!checkSuccess(tmp_pcamera->GetFeatureByName("PayloadSize", pFeature), "get PayloadSize feature")) return 0;
	if (!checkSuccess(pFeature->GetValue(nPLS), "get PayloadSize")) return 0;

	return nPLS;
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

	UINT64 hour = total_seconds / 60 / 60;
	UINT64 minute = (total_seconds - hour * 60 * 60) / 60;
	UINT64 second = total_seconds - hour * 60 * 60 - minute * 60;

	UINT64 milisecond = total_nanoseconds / 1000 / 1000;
	UINT64 microsecond = (total_nanoseconds - milisecond * 1000 * 1000) / 1000;
	UINT64 nanosecond = total_nanoseconds - milisecond * 1000 * 1000 - microsecond * 1000;

	STime st;
	st.hour = (UINT16)hour;
	st.minute = (UINT16)minute;
	st.second = (UINT16)second;
	st.milisecond = (UINT16)milisecond;
	st.microsecond = (UINT16)microsecond;
	st.nanoosecond = (UINT16)nanosecond;

	return std::move(st);
}

/****************************************************************/
/*  class/struct definition
/****************************************************************/

class PreviewHandler
{
public:
	PreviewHandler(const vimba::CameraPtr tmp_pcamera)
	{
		//: allocate buffer according to nPLS

		VmbInt64_t my_nPLS = get_nPLS(tmp_pcamera);

		char message[100];
		sprintf_s(message, "image size = %lld [byte]", my_nPLS);
		INFO::info(message);

		imageSize = get_image_size(tmp_pcamera);

		//sprintf_s(message, "imageSize=%iu", imageSize);
		//INFO::info(message);

		pimage = new VmbUchar_t[imageSize];

		//: allocate frame for capturing
		//async_pframe.reset(new vimba::Frame(my_nPLS));

	}

	~PreviewHandler()
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

	vimba::FramePtrVector async_pframes = vimba::FramePtrVector(2);
	std::atomic<bool> isBusy = false;
	VmbUint32_t imageSize;
	VmbUchar_t* pimage;
	

};

class PreviewFrameObserver : virtual public vimba::IFrameObserver
{
public:
	PreviewFrameObserver(vimba::CameraPtr pCamera, std::shared_ptr<PreviewHandler> pHandler) : vimba::IFrameObserver(pCamera)
	{
		m_pPHandler = pHandler;
	}

	std::shared_ptr<PreviewHandler> m_pPHandler;

	void FrameReceived(const vimba::FramePtr pFrame)
	{
		//: copy to handler
		m_pPHandler->set_image(pFrame);

		//: send pFrame back to queue to recieve next frame buffer -> asynchronous
		m_pCamera->QueueFrame(pFrame);
	}
};

std::shared_ptr<PreviewHandler> pPreviewHandler;
//PreviewHandler* pPreviewHandler=nullptr;

class ObservationFrameObserver : virtual public vimba::IFrameObserver
{
public:
	ObservationFrameObserver(vimba::CameraPtr pCamera) : vimba::IFrameObserver(pCamera) {}
	void FrameReceived(const vimba::FramePtr pFrame)
	{
		// Send notification to working thread
		// Do not apply image processing within this callback (performance)
		// When the frame has been processed , requeue it


		// synchronous multiframe : without the following line
		// asynchronous preview   : with    the following line
		//m_pCamera->QueueFrame(pFrame);
		
		acFrame.store(acFrame.load()+1);
	}

	int myCount = 0;
};

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

	//char message[100];
	//sprintf_s(message, "Camera %s", cameraID.c_str());
	//INFO::info(message);

	flag_cam = true;
	return 1;
}

int init_preview_handler()
{	
	//if (pPreviewHandler != nullptr) 

	//pPreviewHandler = new PreviewHandler(pCamera);

	pPreviewHandler = std::make_shared<PreviewHandler>(pCamera);
	return 1;
}

int close_camera()
{
	if (!flag_cam) {
		INFO::error("flag_cam is flase before closing camera");
		//return 0;
	}
	if (!checkSuccess(pCamera->Close(), "close camera")) return 0;
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
	if (!checkSuccess(sys.Shutdown(), "close sys")) return 0;
	flag_sys = false;

	// release preview handler
	//delete pPreviewHandler;

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

		//INFO::info("GVSPAdjustPacketSize RunCommand completed");
	}

	return 1;
}

int set_exposure(const double value, const std::string featureWord) // [microsecond]
{
	if (!checkReady()) return 0;

	vimba::FeaturePtr pFeature;
	if (!checkSuccess(pCamera->GetFeatureByName(featureWord.c_str(), pFeature), "get exposure time feature")) return 0;

	if (!checkSuccess(pFeature->SetValue(value), "set exposure time")) return 0;

	double current_value;
	if (!checkSuccess(pFeature->GetValue(current_value), "get exposure time")) return 0;

	char message[100];
	sprintf_s(message, "Exposure = %.2f [ms]", current_value / 1000.);
	INFO::info(message);

	return 1;
}

int get_exposure(double & value, const std::string featureWord)
{
	if (!checkReady()) return 0;

	vimba::FeaturePtr pFeature;
	if (!checkSuccess(pCamera->GetFeatureByName(featureWord.c_str(), pFeature), "get exposure time feature")) return 0;
	if (!checkSuccess(pFeature->GetValue(value), "get exposure")) return 0;

	return 1;
}

// normally framerate is automatically set to 1/exposure, might deviate
int set_framerate(const double value, const std::string featureWord)
{
	if (!checkReady()) return 0;

	vimba::FeaturePtr pFeature;
	if (!checkSuccess(pCamera->GetFeatureByName(featureWord.c_str(), pFeature), "get framerate feature")) return 0;

	if (!checkSuccess(pFeature->SetValue(value), "set framerate")) return 0;

	double current_value;
	if (!checkSuccess(pFeature->GetValue(current_value), "get framerate")) return 0;

	char message[100];
	sprintf_s(message, "framerate = %.2f [hz]", current_value);
	INFO::info(message);

	return 1;
}

int get_framerate(double & value, const std::string featureWord)
{
	if (!checkReady()) return 0;

	vimba::FeaturePtr pFeature;
	if (!checkSuccess(pCamera->GetFeatureByName(featureWord.c_str(), pFeature), "get framerate feature")) return 0;
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
	if (!checkSuccess(pFeature->SetValue(mode), message)) return 0;

	return 1;
}

int set_roi(const INT32 offsetx, const INT32 offsety, const INT32 width, const INT32 height)
{
	if (!checkReady()) return 0;

	vimba::FeaturePtr pFeature;

	INT64 current;

	if (!checkSuccess(pCamera->GetFeatureByName("OffsetX", pFeature), "get OffsetX feature")) return 0;
	pFeature->GetValue(current);
	if (current != (INT64)offsetx)
		if (!checkSuccess(pFeature->SetValue(offsetx), "set OffsetX")) return 0;

	if (!checkSuccess(pCamera->GetFeatureByName("OffsetY", pFeature), "get OffsetY feature")) return 0;
	pFeature->GetValue(current);
	if (current != (INT64)offsety)
		if (!checkSuccess(pFeature->SetValue(offsety), "set OffsetY")) return 0;

	if (!checkSuccess(pCamera->GetFeatureByName("Height", pFeature), "get Height feature")) return 0;
	pFeature->GetValue(current);
	if (current != (INT64)height)
		if (!checkSuccess(pFeature->SetValue(height), "set Height")) return 0;

	if (!checkSuccess(pCamera->GetFeatureByName("Width", pFeature), "get Width feature")) return 0;
	pFeature->GetValue(current);
	if (current != (INT64)width)
		if (!checkSuccess(pFeature->SetValue(width), "set Width")) return 0;

	return 1;
}

//int set_bin(const INT16 binx, const INT16 biny, const INT32 widthMax, const INT32 heightMax)
int set_bin(const INT16 binx, const INT16 biny)
{
	if (!checkReady()) return 0;

	vimba::FeaturePtr pFeature;
	INT64 currentx, currenty;

	if (!checkSuccess(pCamera->GetFeatureByName("BinningHorizontal", pFeature), "get BinningHorizontal feature")) return 0;
	pFeature->GetValue(currentx);
	if (currentx != (INT64)binx)
		if (!checkSuccess(pFeature->SetValue(binx), "set BinningHorizontal")) return 0;

	if (!checkSuccess(pCamera->GetFeatureByName("BinningVertical", pFeature), "get BinningVertical feature")) return 0;
	pFeature->GetValue(currenty);
	if (currenty != (INT64)biny)
		if (!checkSuccess(pFeature->SetValue(biny), "set BinningVertical")) return 0;

	//if ((currentx < (INT64)binx) || (currenty < (INT64)biny)) {
	//	set_roi(0, 0, widthMax/binx, heightMax/biny);
	//}

	if (false) {// get current image format
		INT64 offsetx, offsety, width, height;
		pCamera->GetFeatureByName("OffsetX", pFeature);
		pFeature->GetValue(offsetx);
		pCamera->GetFeatureByName("OffsetY", pFeature);
		pFeature->GetValue(offsety);
		pCamera->GetFeatureByName("Width", pFeature);
		pFeature->GetValue(width);
		pCamera->GetFeatureByName("Height", pFeature);
		pFeature->GetValue(height);

		char message[100];
		sprintf_s(message, "offsetX=%lld, offsetY=%lld, width=%lld, height=%lld", offsetx, offsety, width, height);
		INFO::info(message);
	}

	return 1;
}



int grab_multiframe(vimba::FramePtrVector & pFrames)
{
	if (!checkReady()) return 0;

	VmbInt64_t nPLS; // Payload size value
	vimba::FeaturePtr pFeature; // Generic feature pointer

	//FramePtrVector pFrames(3);


	// Get the image size for the required buffer
	pCamera->GetFeatureByName("PayloadSize", pFeature);
	pFeature->GetValue(nPLS);

	// 
	//checkSuccess(pCamera->GetFeatureByName("AcquisitionMode", pFeature), "Get AcquisitionMode");
	//checkSuccess(pFeature->SetValue("Continuous"), "Set AcquisitionMode Value");

	for (vimba::FramePtrVector::iterator iter = pFrames.begin(); pFrames.end() != iter; ++iter)
	{
		// Allocate memory for frame buffer
		(*iter).reset(new vimba::Frame(nPLS));
		// [x] Register frame observer/callback for each frame
		(*iter)->RegisterObserver(vimba::IFrameObserverPtr(new ObservationFrameObserver(pCamera)));
		//announce frame to the API
		checkSuccess(pCamera->AnnounceFrame(*iter), " announce frame");
	}


	// start the capture engine (API)
	checkSuccess( pCamera->StartCapture(), "start capture");


	// put frame into the frame queue
	for (vimba::FramePtrVector::iterator iter = pFrames.begin(); pFrames.end() != iter; ++iter)
		checkSuccess( pCamera->QueueFrame(*iter), "queue frame");



	// Start the acuisition engine (camera)
	pCamera->GetFeatureByName("AcquisitionStart", pFeature);
	pFeature->RunCommand();

	// ... frame being filled asynchronously


	// check complete
	/*
	for (vimba::FramePtrVector::iterator iter = pFrames.begin(); pFrames.end() != iter; ++iter)
	{
		int count = 0;
		VmbFrameStatusType eReceiveStatus;
		do {
			(*iter)->GetReceiveStatus(eReceiveStatus);
			sleep_milisecond(2);
			count += 1;
			if (count > 10 * 1000) break; // timeout = 20 [sec] for each image
		} while (eReceiveStatus != VmbFrameStatusComplete);
	}
	*/
	{
		UINT64 count = 0;
		UINT64 target = 10 * 1000 * pFrames.size;
		while (acFrame.load() < anFrame.load())
		{
			sleep_milisecond(2);
			count += 1;
			if (count > target) break;
		}
	}
	



	// Stop the acquisition engine(camera)
	pCamera->GetFeatureByName("AcquisitionStop", pFeature);
	pFeature->RunCommand();

	// Stop the capture engine (API) (correspond to pCamera->StartCapture )
	checkSuccess( pCamera->EndCapture(), "end capture");
	// flush the frame queue (correspond to pCamera->QueueFrame )
	checkSuccess( pCamera->FlushQueue(), " flush queue");
	// revoke all frame from the API (correspond to pCamera->AnnounceFrame )
	checkSuccess( pCamera->RevokeAllFrames(), " revoke all frames");
	// unregister the frame observer
	for (vimba::FramePtrVector::iterator iter = pFrames.begin(); pFrames.end() != iter; ++iter)
		(*iter)->UnregisterObserver();

	// FramePtr is a smart pointer, so it will release itself 
	
	return 1;
}

int init_preview()
{
	if (!checkReady()) return 0;

	//: allocate frame
	VmbInt64_t nPLS = get_nPLS(pCamera);
	for (vimba::FramePtrVector::iterator iter = pPreviewHandler->async_pframes.begin(); pPreviewHandler->async_pframes.end() != iter; ++iter)
	{
		// Allocate memory for frame buffer
		(*iter).reset(new vimba::Frame(nPLS));
		// [x] Register frame observer/callback for each frame
		(*iter)->RegisterObserver(vimba::IFrameObserverPtr(new PreviewFrameObserver(pCamera, pPreviewHandler)));
		//announce frame to the API
		checkSuccess(pCamera->AnnounceFrame(*iter), " announce frame preview");
	}	
	//pPreviewHandler->async_pframe.reset(new vimba::Frame(nPLS));
	
	/*
	//:Register frame observer/callback for each frame
	checkSuccess(
		pPreviewHandler->async_pframe->RegisterObserver(vimba::IFrameObserverPtr(new PreviewFrameObserver(pCamera, pPreviewHandler))),
		"register observer preview"
	);
	//: announce frame to the API
	checkSuccess( pCamera->AnnounceFrame(pPreviewHandler->async_pframe), "announce frame preview");
	*/

	//: start the capture engine (API)
	checkSuccess( pCamera->StartCapture(), "start capture preview");
	
	//: put frame into the frame queue
	for (vimba::FramePtrVector::iterator iter = pPreviewHandler->async_pframes.begin(); pPreviewHandler->async_pframes.end() != iter; ++iter)
		checkSuccess( pCamera->QueueFrame((*iter)), "queue frame preview");
	
	//: Start the acuisition engine (camera)
	vimba::FeaturePtr pFeature;
	pCamera->GetFeatureByName("AcquisitionStart", pFeature);
	checkSuccess( pFeature->RunCommand(), "acquisition start preview");

	return 1;
}

int close_preview()
{
	if (!checkReady()) return 0;

	//: Stop the acquisition engine(camera)
	vimba::FeaturePtr pFeature;
	pCamera->GetFeatureByName("AcquisitionStop", pFeature);
	pFeature->RunCommand();

	//: Stop the capture engine (API) (correspond to pCamera->StartCapture )
	pCamera->EndCapture();
	//: flush the frame queue (correspond to pCamera->QueueFrame )
	pCamera->FlushQueue();
	//: revoke all frame from the API (correspond to pCamera->AnnounceFrame )
	pCamera->RevokeAllFrames();

	for (vimba::FramePtrVector::iterator iter = pPreviewHandler->async_pframes.begin();  pPreviewHandler->async_pframes.end() != iter; ++iter)
		(*iter)->UnregisterObserver();

	return 1;
}


/****************************************************************/
/*  IDL related functions
/****************************************************************/

std::string getIDLString(const void* argv)
{
	IDL_STRING* s_ptr = (IDL_STRING *)argv;
	std::string str(s_ptr[0].s, s_ptr[0].s + s_ptr[0].slen);

	return std::move(str);
}





/****************************************************************/
/*  camera connection
/*  IDL>  x=call_external(dll,'VimbaInit')
/****************************************************************/

int IDL_STDCALL VimbaInit(int argc, void* argv[])
{
	if (!init_sys()) return 0;
	return 1;
}

int IDL_STDCALL VimbaInitCamera(int argc, void* argv[])
{
	//std::string cameraID = "";
	std::string cameraID = getIDLString(argv[0]);
	if (!init_camera(cameraID)) return 0;


	//std::string xmlFile = "Z:\\conf\\Vimba\\demo.20211216.xml";
	std::string xmlFile = getIDLString(argv[1]);
	if (!load_configuration(xmlFile)) return 0;

	if (!set_maximum_gev_packet()) return 0;

	if (!init_preview_handler()) return 0;


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

	if (!close_camera()) {
		INFO::error("failed to close camera");
		return 0;
	}
	if (!close_sys()) {
		INFO::error("failed to close sys");
		return 0;
	}

	INFO::info("Camera disconnected");

	return 1;
}

/****************************************************************/
/*  set camera exposure
/*  IDL>  x=call_external(dll,'SetVimbaExpoTime',[50.],"ExposureTime")
/****************************************************************/

int IDL_STDCALL SetVimbaExpoTime(int argc, void* argv[])
{
	double expotime;         // [miliseconds]
	//double expotime_current; // [miliseconds]

	// expotime = (double)argv[0];
	float* expo_arr = (float*)argv[0];
	expotime = (double) expo_arr[0];

	std::string featureWord = getIDLString(argv[1]);

	if (!set_exposure(expotime*1000., featureWord)) return 0;

	return 1;
}

/****************************************************************/
/*  set camera framerate
/*  IDL>  x=call_external(dll,'SetVimbaFrameRate', [20.],"AcquisitionFrameRate")
/****************************************************************/

int IDL_STDCALL SetVimbaFrameRate(int argc, void* argv[])
{
	double framerate;         // [hz]
	//double framerate_current; // [hz]

	float* framerate_arr = (float*)argv[0];
	framerate = (double)framerate_arr[0];

	std::string featureWord = getIDLString(argv[1]);

	if (!set_framerate(framerate, featureWord)) return 0;

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
	if (!init_preview()) return 0;

	return 1;
}


int IDL_STDCALL GetVimbaPreview(int argc, void* argv[])
{
	UINT16* idlBuffer = (UINT16 *)argv[0];

	pPreviewHandler->copy_image(idlBuffer);

	return 1;
}

int IDL_STDCALL StopVimbaPreview(int argc, void* argv[])
{
	if (!close_preview()) return 0;

	return 1;
}

/****************************************************************/
/*  camera grab image sequence (synchronously) with header
/*  IDL>  x=call_external(dll,'VimbaObs', nimg, imgs, times, framerate, "AqcuisitionFrameRate")
/****************************************************************/


int IDL_STDCALL VimbaObs(int argc, void* argv[])
{

	
	UINT16 nFrame = (UINT16) *((UINT16*)argv[0]);
	anFrame.store(nFrame);
	acFrame.store(0);
	//char message[100];
	//sprintf_s(message, "nFrame=%u", nFrame);
	//INFO::info(message);
	
	UINT16* idlBuffer = (UINT16*)argv[1];              // idl buffer for 3d image array
	UINT16* idlBuffer_time = (UINT16*)argv[2];         // idl buffer for timestamp 2d array
	float*  idlBuffer_framerate = (float *)argv[3];    // idl buffer for framerate 1d array

	std::string featureWord = getIDLString(argv[4]);
	
	//: acquire image
	vimba::FramePtrVector pFrames(nFrame);
	if (!grab_multiframe(pFrames)) return 0;
	//Vhida::pCamera->AcquireMultipleImages(pFrames,Vhida::Observation::TIMEOUT);

	
	VmbUint32_t imageSize;
	pFrames.at(0)->GetImageSize(imageSize);

	// get framerate
	double frate;
	get_framerate(frate, featureWord);

	int nTimeParam = 6;
	//: copy memory data
	for (int i=0; i<nFrame; ++i) 
	{
		vimba::FramePtr frame = pFrames.at(i);

		// get image buffer
		VmbUchar_t* pimage;
		frame->GetImage(pimage);

		// get timestamp
		VmbUint64_t timestamp;
		frame->GetTimestamp(timestamp);
		//char message[100];
		//sprintf_s(message, "timestamp=%llu", timestamp);
		//INFO::info(message);

		

		// copy image memory
		memcpy( (void*)(idlBuffer + i*imageSize/2), (void*)(pimage) , imageSize);

		// copy timestamp memory
		auto st = timestamp_to_STime(timestamp);
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


/****************************************************************/
/*  camera grab image sequence (synchronously) with header
/*  IDL>  x=call_external(dll,'VimbaSnap', img)
/****************************************************************/


int IDL_STDCALL VimbaSnap(int argc, void* argv[])
{/*
	UINT16* idlBuffer = (UINT16*)argv[0];              // idl buffer for 2d image array

	Vhida::vimba::FramePtr pframe;
	Vhida::pCamera->AcquireSingleImage(pframe, Vhida::Observation::TIMEOUT);

	VmbUint32_t imageSize;
	pframe->GetImageSize(imageSize);

	VmbUchar_t* pimage;
	pframe->GetImage(pimage);

	// copy image memory
	memcpy((void*)(idlBuffer), (void*)(pimage), imageSize);
*/
	return 1;
	
}

/****************************************************************/
/*  subregion
/*  IDL>  x=call_external(dll,'VimbaRoi', regionX, regionY, width, height)
/****************************************************************/
int IDL_STDCALL VimbaRoi(int argc, void* argv[])
{
	using idl_int_t = INT32;

	idl_int_t regionX = *((idl_int_t*)argv[0]);
	idl_int_t regionY = *((idl_int_t*)argv[1]);
	idl_int_t width   = *((idl_int_t*)argv[2]);
	idl_int_t height  = *((idl_int_t*)argv[3]);

	if (false) {
		char message[100];
		sprintf_s(message, "regionX=%d, regionY=%d, width=%d, height=%d", regionX, regionY, width, height);
		INFO::info(message);
	}
	

	if (!set_roi(regionX, regionY, width, height))
	{
		INFO::error("failed to set roi in vimba");
		return 0;
	}

	if (!init_preview_handler())
	{
		INFO::error("failed to re-init preview handler");
		return 0;
	}

	return 1;
}

/****************************************************************/
/*  subregion
/*  IDL>  x=call_external(dll,'VimbaBin', binx, biny)
/****************************************************************/

int IDL_STDCALL VimbaBin(int argc, void* argv[])
{
	using idl_int_t = INT16;

	idl_int_t binx = *((idl_int_t*)argv[0]);
	idl_int_t biny = *((idl_int_t*)argv[1]);

	if (!set_bin(binx, biny))
	{
		INFO::error("failed to change binnding in vimba");
		return 0;
	}

	if (!init_preview_handler())
	{
		INFO::error("failed to re-init preview handler");
		return 0;
	}

	return 1;
}