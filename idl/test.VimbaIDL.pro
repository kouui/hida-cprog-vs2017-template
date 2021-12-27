

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

camera_type="prosilica"
features = struct_feature(camera_type)

dll = 'Z:\Projects\cprog\VS2017\VimbaIDL\x64\Debug\VimbaIDL.dll'
;;err=call_external(dll,'Hello')
;;xmlFile = "Z:\\conf\\Vimba\\demo.20211216.xml"
xmlFile = "Z:\\conf\\Vimba\\prosilica.000F3101ABD0.20211225.xml"
cameraID = 'DEV_000F3101ABD0'
;;cameraId = ''
err = call_external(dll,'VimbaInit')
print, "err = ", err 
err = call_external(dll,'VimbaInitCamera', cameraID, xmlFile)
print, "err = ", err 
err = call_external(dll,'SetVimbaExpoTime',[10.],features.exposure)
print, "err = ", err
;;err = call_external(dll,'SetVimbaFrameRate', [2.],features.framerate)
;;print, "err = ", err
err = call_external(dll,'StartVimbaPreview')
print, "err = ", err
wait, 1
height = 1200l
width  = 1600l
img=uintarr(width,height)

window, 1, xsize=width, ysize=height
for i=0,100 do begin
    wait, 0.1
    ;;err = call_external(dll,'VimbaSnap',img)
    err = call_external(dll,'GetVimbaPreview',img)
    tvscl, img
    print, "err = ", err
    print, "snapped"
endfor
;;wait, 1
err = call_external(dll,'StopVimbaPreview')
print, "err = ", err
;;wait, 1

nimg = 2
imgs=uintarr(width,height,nimg)
times=uintarr(6,nimg)
;; 5293528977 53853019911
;; 72547238663 - 72544480966
;; 76422724047 - 72552752167
framerate=fltarr(nimg)
x=call_external(dll,'VimbaObs', nimg, imgs, times, framerate, features.framerate)
print, "multiframe completed"
print, "err = ", err
wait, 1
x=call_external(dll,'VimbaObs', nimg, imgs, times, framerate, features.framerate)
print, "multiframe completed"
print, "err = ", err
err = call_external(dll,'VimbaClose')
print, "err = ", err


;;err = call_external(dll,'VimbaMainInit')
;;err = call_external(dll,'VimbaMain')
;;err = call_external(dll,'VimbaMain')
;;err = call_external(dll,'VimbaMainClose')


END