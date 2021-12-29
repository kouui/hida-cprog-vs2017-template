; vimba_interface.pro
; a general purpose library to interface with vimba's DLL
;
; History: 
; 2021.12.29  u.k.  not yet test

;****************************************************
;
; NAME        : throw_error (procedure)
; PURPOSE     : throw error with an text description
; CALLING SEQUENCE :
;               throw_error, text
; INPUTS      :
;         text - error message, string
; HISTORY     :
;         k.u.  2021.09.20
;****************************************************

PRO throw_error, text
    ON_ERROR, 2   ; stop in caller
    MESSAGE, LEVEL=-1, text

END

;**************************************************************
; vimba cpp library utilizes different "feature keywords" to 
; control exposure time and frame rate
;**************************************************************
function struct_feature, camera_type

  case camera_type of
      'prosilica' : begin
		    ;ret = call_external(vimbadll,"Hello")
        struct={vimba_feature_words,   $
                exposure:  "ExposureTimeAbs",      $
                framerate: "AcquisitionFrameRateAbs"  $
        }
        print, 'Vimba with ', camera_type, " camera"
		  end
      'goldeye' : begin
        struct={vimba_feature_words,   $
                exposure:  "ExposureTime",      $
                framerate: "AcquisitionFrameRate"  $
        }
        print, 'Vimba with ', camera_type, " camera"
		  end
      else : throw_error, "undefined camera_type="+camera_type
    endcase
  
  return,struct
end

;**************************************************************
; "prosilica" and "goldeye" has different sensor format 
; and framerate, therefore we specify width, height, waiting time
; during preview while loop and scale to resize the preview window
; for different type of cameras.
;**************************************************************
function init_camera_size, p1, camera_type

  case camera_type of
      'prosilica' : begin
		    ;ret = call_external(vimbadll,"Hello")
        p1.Width =1600l
        p1.Height=1200l
        p1.WidthMax =1600l
        p1.HeightMax=1200l
        p1.WinResize=2
        p1.PreviewWait=0.03
		  end
      'goldeye' : begin
        p1.Width =640l
        p1.Height=512l
        p1.WidthMax =640l
        p1.HeightMax=512l
        p1.WinResize=1
        p1.PreviewWait=0.02
		  end
      else : throw_error, "undefined camera_type="+camera_type
    endcase
  
  return,p1
end

;**************************************************************
; when offsetx, offsety, width, height being modified,
; let the modified values stay in a available range
; ex., width cannot overseed (widthMax/binx-regionx)
;**************************************************************
function regular_image_format, p1

  binx = p1.binx
  biny = p1.biny

  regionx = p1.regionx
  regiony = p1.regiony
  height  = p1.height
  width   = p1.width

  heightMax = p1.HeightMax
  widthMax  = p1.WidthMax
  
  if (1) then begin
    ;; regular offset
    if (regionx ge widthMax/binx) then begin
      regionx = 0
      print, "regionx >= widthMax/binx, set to 0"
    endif

    if (regiony ge heightMax/biny) then begin
      regiony = 0
      print, "regiony >= heightMax/biny, set to 0"
    endif

    ;; regular width and height
    if (width gt (widthMax/binx-regionx)) then begin
      width = widthMax/binx-regionx
      print, "width ge (widthMax/binx-region), set to (widthMax/binx-region)"
    endif

    if (height gt (heightMax/biny-regiony)) then begin
      height = heightMax/biny - RegionY
      print, "height ge (heightMax/biny-regiony), set to (heightMax/biny-regiony)"
    endif
  endif

  p1.regionx = regionx
  p1.regiony = regiony
  p1.height  = height
  p1.width   = width

  ;;p1.heightMax = HeightMax
  ;;p1.widthMax  = WidthMax

  return,p1 

end



;**************************************************************
; struct to store camera parameters
;**************************************************************
function p_vimba

  p={vimba_param,   $
    expo:        0.015,      $   ; exposure time [sec]
    framerate:   float(30),$   ; frame rate [frame/sec]
 ;;    timeout:     100l,    $   ; timeout for grabing frame [sec]
    gain:        1,        $   ; FPA gain 0-2
    Temp:        20.,	   $   ; Temperature
    bin:         1,        $   ; Binning XY 1-8
    WidthMax:    640l,          $   ; maximum of Width
    HeightMax:   512l,          $   ; maximum of Height
    Width:       640l,          $   ; Width  max=2048 (binx=1), (cam pixel)/bin
    Height:      512l,          $   ; Height max=2048 (biny=1), (cam pixel)/bin
    RegionX:     0l,             $   ; left edge of ROI, pixel, (cam pixel)/bin
    RegionY:     0l,             $   ; top edge of ROI, pixel, (cam pixel)/bin
    Sparse:      0l,             $   ; if set, sample alternative images
    TrigMode:    'Internal',     $   ; trigger mode, 'Internal' or 'Start'
    TrigPol:     'Negative', 	$   ; trigger polarity, 'Negative' or 'Positive'
 ;;    AutoNUC:	 0,		$   ; auto NUC on/off
 ;;    NUC_intval: 30.d,		$   ; NUC interval [min]
 ;;    NUC_time:   0ul,		$   ; time of last NUC, unsigned long in msec
    date_obs:    '',             $   ; yyyy-mm-ddThh:mm:ss.sss
    date_obs2:   '',             $   ; yyyy-mm-ddThh:mm:ss.sss
    timesys :    'JST',          $   ; yyyy-mm-ddThh:mm:ss.sss
    observat:    'Hida',         $   ;
    telescop:    'DST',          $   ;
    instrume:    'VS',           $   ;
    camera:      'GOLDEYE_????',   $   ;
    wave:    	   '',             $   ; wavelength
    data_typ:  	 'OBJECT', 	$   ; 'OBJECT','DARK', 'FLAT'
    clock:       0l,     	$   ; TimeStanmpFrequency [Hz]
    timelo:      0,            $   ; Time stamp, lower 32-bits
    timehi:      0,            $   ; Time stamp, upper 32-bits
    status:      0,            $   ; Status
    binx  :      1,            $   ; binning X 1-4
    biny  :      1,            $   ; binning Y 1-4
    PreviewWait: 0.03,         $   ; preview waiting time
    cameraNo: 0,               $   ; No. of camera
    WinResize:   1             $   ; factor to resize display window
  }

  return,p
end

;**************************************************************
; camera info struct
;**************************************************************
function camera_info_struct, camera_no, camera_type, camera_id, camera_xml
  
  struct = {camera_info,   $
  camera_no   : camera_no,     $ ;; int, No of camera, starts from 0
  camera_type : camera_type,   $ ;; string, camera type, "prosilica" or "goldeye"
  camera_id   : camera_id,     $ ;; string, camera id ("DEV_*" as an identifier)
  camera_xml  : camera_xml     $ ;; string, path to the *.xml camera configuration file
  }

  return, struct
end
;**************************************************************
; camera handler struct
;**************************************************************
;;function camera_handler_struct, p, img, imgs, imgShape, feature
function camera_handler_struct, p, imgShape, feature

  struct = {camera_handler,   $
  p        : p,         $ ;; vimba_param, struct of camera parameter
  ;;img      : img,       $ ;; uintarr(width, height), 2d image for previewing
  ;;imgs     : imgs,      $ ;; uintarr(width, height, nimg), 3d images cube for image capturing (observation) to avoid memory copying
  imgShape : imgShape,  $ ;; long(intarr(3)), shape of array imgs
  feature  : feature    $ ;; vimba_feature_words, handler feature keyword for different cameras
  }
  

  return, struct

end


;**************************************************************
; init vimba software api and cameras
;**************************************************************
function vimba_init,camera_info_list,noDev=noDev0

  common vimba,vimbadll,noDev,cHandler_list,img_list,imgs_list

  vimbadll='Z:\Projects\cprog\VS2017\VimbaIDLMulti\x64\Debug\VimbaIDL.dll'
  if keyword_set(noDev0) then noDev=1 else noDev=0
  n_camera = n_elements(camera_info_list)
  print, "About to initialize ", n_camera, " cameras."
  
  cHandler_list = list()   ;; list of camera handler struct
  img_list = list()        ;; list of 2d image for previewing of cameras
  imgs_list = list()       ;; list of 3d images for capturing of cameras

  if not noDev then begin
    for i=0,n_camera-1 do begin
      cInfo = camera_info_list[i]
      err = call_external(vimbadll,'VimbaInit')
      err = call_external(vimbadll,'VimbaInitCamera', cInfo.camera_no, cInfo.camera_id, cInfo.camera_xml)
      
      p=p_vimba()
      p.CameraNo=cInfo.camera_no
      p=init_camera_size(p, cInfo.camera_type)

      img=uintarr(p.width,p.height)
      imgs=uintarr(p.width,p.height, 1)  ;; init with nimg=1, could be modified later
      imgShape=long([p.width,p.height,1]);; init with nimg=1, could be modified later
      
      feature=struct_feature(cInfo.camera_type)
      
      cHandler=camera_handler_struct(p, imgShape, feature)

      img_list.add, img
      imgs_list.add, imgs
      cHandler_list.add, cHandler
    endfor
  endif else begin
    print, "working without cameras"
  endelse


  return, cHandler_list

end

;**************************************************************
function vimba_close

  common vimba,vimbadll,noDev,cHandler_list,img_list,imgs_list

  if not noDev then err=call_external(vimbadll,'VimbaClose',/all_value,/cdecl)
  
  return,1
end

;**************************************************************
; 
function vimba_setParam,camera_no,expo=expo,gain=gain,binx=binx,biny=biny, $
  width=width,height=height,regionx=regionx,regiony=regiony, $
  framerate=framerate

  common vimba,vimbadll,noDev,cHandler_list,img_list,imgs_list

  cHandler = cHandler_list[camera_no]
  p=cHandler.p
  imgss=cHandler.imgShape
  
  ; if exposure revised
  if isvalid(expo) then begin
    ;revexpo = expo ne p.expo
    revexpo = 1b
  endif else revexpo = 0b
  if revexpo then begin
    expo_mili = expo * 1000.0
    if not noDev then ret = call_external(vimbadll, 'SetVimbaExpoTime', p.CameraNo, [expo_mili], features.exposure)
    p.expo = expo
  endif

  ; if framerate revised
  if isvalid(framerate) then begin
    ;;revexpo = expo ne p.expo
    ;;revfrate = 1b
    revfrate = 0b
  endif else revfrate = 0b
  if revfrate then begin
    frate = float(framerate)
    if not noDev then ret = call_external(vimbadll, 'SetVimbaFrameRate',p.CameraNo, [frate], features.framerate)
    p.framerate = frate
  endif
  
  ;revise binnig x?
  if isvalid(binx) then begin
    revbinx=binx ne p.binx
    revbinx=1b
  endif else revbinx=0b

  ;revise binnig y?
  if isvalid(biny) then begin
    revbiny=biny ne p.biny
    revbiny=1b
  endif else revbiny=0b

  revbin = revbinx or revbiny

  ;revise subarray?
  revsub=0b
  if isvalid(width) then begin
    revsub or=width ne p.width
  endif else revsub or=0b
  if isvalid(height) then begin
    revsub or=height ne p.height
  endif else revsub or=0b
  if isvalid(regionx) then begin
    revsub or=regionx ne p.regionx
  endif else revsub or=0b
  if isvalid(regiony) then begin
    revsub or=regiony ne p.regiony
  endif else revsub or=0b

  ;only subarray revised
  if revsub and ~revbin then begin
    if isvalid(regionx) then p.RegionX=regionx
    if isvalid(regiony) then p.RegionY=regiony
    if isvalid(width) then p.Width=width
    if isvalid(height) then p.Height=height
    
    p=regular_image_format(p)

    if ~noDev then err=call_external(vimbadll,'VimbaRoi', p.CameraNo, p.RegionX, p.RegionY, p.Width, p.Height)
  endif

  ;only bin revised
  if revbin and ~revsub then begin
    
    if (revbinx) then begin
      if (binx gt p.binx) then begin
        p.RegionX = 0l
        p.Width = p.WidthMax / binx
      endif
      p.binx=binx
    endif

    if (revbiny) then begin
      if (biny gt p.biny) then begin
        p.RegionY = 0l
        p.Height = p.HeightMax / biny
      endif
      p.biny=biny
    endif
    
    if ~noDev then err=call_external(vimbadll,'VimbaBin', p.CameraNo, p.binx, p.biny)
  endif
  
  ;; revise array size
  if revsub or revbin then begin
    img_list[camera_no] =uintarr(p.width,p.height)
    imgss[0:1]=[p.width,p.height]
    imgs_list[camera_no]=uintarr(imgss[0],imgss[1],imgss[2]>1)
  endif

 ; [currently no need to change gain, stay with gain1, which is preset in configuration *.xml file]
  ; if gain revised
 ;  if n_elements(gain) ne 0 then begin
 ;    ;revgain = gain ne p.gain
 ;    revgain = 1b
 ;  endif else revgain = 0b
 ;  if revgain then begin
 ;    if not noDev then ret = call_external(vimbadll, 'SetFPAGain', gain, /all_value, /cdecl)
 ;  endif
  
  cHandler.p=p
  cHandler.imgShape=imgss
  return, cHandler
end

;**************************************************************

function vimba_startPreview, camera_no

  common vimba,vimbadll,noDev,cHandler_list,img_list,imgs_list

  ;;cHandler = cHandler_list[camera_no]
  if not noDev then ret = call_external(vimbadll, 'StartVimbaPreview', camera_no)
  
  return,1
end

;**************************************************************

function vimba_stopPreview, camera_no

  common vimba,vimbadll,noDev,cHandler_list,img_list,imgs_list

  ;;cHandler = cHandler_list[camera_no]
  if not noDev then ret = call_external(vimbadll, 'StopVimbaPreview', camera_no)
  
  return,1
end


function vimba_getPreview, camera_no

  common vimba,vimbadll,noDev,cHandler_list,img_list,imgs_list

  ;;cHandler = cHandler_list[camera_no]

  if not noDev then begin
    ret = call_external(vimbadll, 'GetVimbaPreview', camera_no, img_list[camera_no])
  endif
  
  return, img_list[camera_no]
end

function vimba_getPreviewAll

  common vimba,vimbadll,noDev,cHandler_list,img_list,imgs_list
  
  n_camera = n_elements(cHandler_list)
  ;;cHandler = cHandler_list[camera_no]

  if not noDev then begin
    case n_camera of
      1 : begin
        err = call_external(vimbadll, 'GetVimbaPreviewAll', n_camera, img_list[0])
      end

      2 : begin
        err = call_external(vimbadll, 'GetVimbaPreviewAll', n_camera, img_list[0],img_list[1])
      end

      3 : begin
        err = call_external(vimbadll, 'GetVimbaPreviewAll', n_camera, img_list[0],img_list[1],img_list[2])
      end
      
      else : throw_error, "n_camera=", n_camera, "  not available. need implement."
    endcase
  endif
  
  return, img_list
end

;**************************************************************
function vimba_obs, camera_no, nimg=nimg, time_arr=time_arr, frate_arr=frate_arr

  common vimba,vimbadll,noDev,cHandler_list,img_list,imgs_list

  cHandler = cHandler_list[camera_no]
  p = cHandler.p

  if not keyword_set(nimg) then nimg = 1
  if nimg eq 0 then nimg = 1

  time_arr = uintarr(6, nimg)
  frate_arr = fltarr(nimg)

  imgs_list[camera_no] = uintarr(p.width,p.height,nimg)
  if not noDev then begin
    ret = call_external(vimbadll, 'VimbaObs', camera_no, nimg, imgs_list[camera_no], time_arr, frate_arr, cHandler.feature.framerate)
  endif
  cHandler.imgShape[2] = nimg

  return, imgs_list[camera_no]

end

function vimba_obsAll, nimg=nimg

  common vimba,vimbadll,noDev,cHandler_list,img_list,imgs_list
  
  if not keyword_set(nimg) then nimg = 1
  if nimg eq 0 then nimg = 1

  n_camera = n_elements(cHandler_list)

  for i=0,n_camera-1 do begin
    cHandler = cHandler_list[i]
    p = cHandler.p
    cHandler.imgShape[2] = nimg
    imgs_list[camera_no] = uintarr(p.width,p.height,nimg)
  endfor

  if not noDev then begin
    case n_camera of
      1 : begin
        err = call_external(vimbadll, 'VimbaObsAll', n_camera, imgs_list[0])
      end

      2 : begin
        err = call_external(vimbadll, 'VimbaObsAll', n_camera, imgs_list[0],imgs_list[1])
      end

      3 : begin
        err = call_external(vimbadll, 'VimbaObsAll', n_camera, imgs_list[0],imgs_list[1],imgs_list[2])
      end
      
      else : throw_error, "n_camera=", n_camera, "  not available. need implement."
    endcase
  endif

  return, imgs_list

end


;**************************************************************
;[ not yet implemented]
pro vimba_trigmode,mode

 common vimba,vimbadll,noDev,cHandler_list,img_list,imgs_list

  ;/*  camera trigger source
  ;/*  0 : internal trigger
  ;/*  1 : external trigger
  ;/*  2 : software trigger
  ;/*  3 : IRIG     trigger
  ;/*  camera trigger mode
  ;/*  0 : bhptmFreeRun               : No trigger is used, the camera outputs frames continuosly (free runs).
  ;/*  1 : bhptmTriggeredFreeRun      : A trigger cause the camera to output frames continuously.
  ;/*  2 : bhptmTriggeredSequence     : A trigger outputs a number of sequences (then stops). 
  ;/*  3 : bhptmTriggeredPresetAdvance: The current preset advances to the next preset in the sequence 
  ;					when a trigger is recieved.
  
  if not noDev then begin
    case mode of
      'Internal' : begin
		    ;ret = call_external(vimbadll,"Hello")
        print, 'trigger mode functionallity not yet implemented'
		  end
      'External' : begin
        ;ret = call_external(vimbadll,"Hello")
        print, 'trigger mode functionallity not yet implemented'
		  end
      else : print, 'trigger mode ',mode,' not defined!'
    endcase
  endif

end

;**************************************************************
;[ not yet implemented]
pro vimba_trigpolarity,polarity

  common vimba,vimbadll,noDev,cHandler_list,img_list,imgs_list
  
  ;/*  camera trigger polarity
  ;/*  0 : bhppolActiveLow : Signal is active when it is driven low.
  ;/*  1 : bhppolActiveHigh: Signal is active when it is driven high.
  
  if not noDev then begin
    case polarity of
      'Negative': print, 'trigger polarity functionallity not yet implemented'  ;r=call_external(vimbadll,"SetTriggerPolarity",0)
      'Positive': print, 'trigger polarity functionallity not yet implemented'  ;r=call_external(vimbadll,"SetTriggerPolarity",1)
      else:   print,'trigger polarity',polarity,' not defined!'
    endcase
  endif

end


