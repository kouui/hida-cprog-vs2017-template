; vimbalib.pro

; 2021.12.21  u.k.

;**************************************************************
function p_vimba

  p={vimba_param,   $
    expo:        0.015,      $   ; exposure time [sec]
    framerate:   float(30),$   ; frame rate [frame/sec]
 ;;    timeout:     100l,    $   ; timeout for grabing frame [sec]
    gain:        1,        $   ; FPA gain 0-2
    Temp:        20.,	   $   ; Temperature
    bin:         1,        $   ; Binning XY 1-8
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
    status:      0             $   ; Status
  }

  return,p
end

;**************************************************************
function vimba_init,noDev=noDev0

  common vimba,vimbadll,p,img,noDev,imgs,imgss

  flirdll='Z:\Projects\cprog\VS2017\VimbaIDL\x64\Debug\VimbaIDL.dll'
  if keyword_set(noDev0) then noDev=1 else noDev=0
  if not noDev then err=call_external(vimbadll,'VimbaInit',/all_value,/cdecl)

  p=p_flir()
  img=uintarr(p.width,p.height)

  imgss=[0l,0l,0l] ;; size of imgs

  return,p

end


;**************************************************************
pro vimba_close

  common vimba,vimbadll,p,img,noDev,imgs,imgss

  if not noDev then err=call_external(vimbadll,'VimbaClose',/all_value,/cdecl)

end

;**************************************************************
; 
function vimba_setParam,expo=expo,gain=gain,bin=bin, $
  width=width,height=height,regionx=regionx,regiony=regiony, $
  framerate=framerate

  common vimba,vimbadll,p,img,noDev,imgs,imgss
 ; print, p.TrigMode, " FlirSetParam"
  ;  expo      - exposure, sec, float
  ;  width, height, regionx,y, bin, gain, framerate  (not implemented)

  ; if exposure revised
  if isvalid(expo) then begin
    ;revexpo = expo ne p.expo
    revexpo = 1b
  endif else revexpo = 0b
  if revexpo then begin
    expo_mili = expo * 1000.0
    if not noDev then ret = call_external(vimbadll, 'SetVimbaExpoTime', expo_mili, /all_value, /cdecl)
    p.expo = expo
  endif

  ; if framerate revised
  if isvalid(framerate) then begin
    ;revexpo = expo ne p.expo
    revfrate = 1b
  endif else revfrate = 0b
  if revfrate then begin
    frate = float(framerate)
    if not noDev then ret = call_external(flirdll, 'SetVimbaFrameRate', frate, /all_value, /cdecl)
    p.framerate = frate
  endif

 ; [currently no need to change gain, stay with gain1, which is preset in configuration *.xml file]
  ; if gain revised
 ;  if n_elements(gain) ne 0 then begin
 ;    ;revgain = gain ne p.gain
 ;    revgain = 1b
 ;  endif else revgain = 0b
 ;  if revgain then begin
 ;    if not noDev then ret = call_external(flirdll, 'SetFPAGain', gain, /all_value, /cdecl)
 ;  endif

  return, p
end

;**************************************************************
pro vimba_startPreview

  common vimba,vimbadll,p,img,noDev,imgs,imgss
  if not noDev then ret = call_external(vimbadll, 'StartVimbaPreview')

end

;**************************************************************
pro vimba_stopPreview

  common vimba,vimbadll,p,img,noDev,imgs,imgss
  if not noDev then ret = call_external(vimbadll, 'StopVimbaPreview')

end

;**************************************************************
function vimba_getPreview

  common vimba,vimbadll,p,img,noDev,imgs,imgss
  if not noDev then begin
    ret = call_external(vimbadll, 'GetVimbaPreview', img)
  endif
  
  return, img
end

;**************************************************************
function vimba_obs, nimg=nimg, time_arr=time_arr, frate_arr=frate_arr

  common vimba,vimbadll,p,img,noDev,imgs,imgss

  if not keyword_set(nimg) then nimg = 1
  if nimg eq 0 then nimg = 1

  time_arr = uintarr(6, nimg)
  frate_arr = fltarr(nimg)

  imgs=uintarr(p.width,p.height,nimg)
  if not noDev then begin
    ret = call_external(vimbadll, 'VimbaObs', nimg, imgs, time_arr, frate_arr)
  endif
  imgss = [p.width,p.height,nimg]

  return, imgs

end


;**************************************************************
;[ not yet implemented]
pro vimba_trigmode,mode

 common vimba,vimbadll,p,img,noDev,imgs,imgss

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

  common vimba,vimbadll,p,img,noDev,imgs,imgss
  
  ;/*  camera trigger polarity
  ;/*  0 : bhppolActiveLow : Signal is active when it is driven low.
  ;/*  1 : bhppolActiveHigh: Signal is active when it is driven high.
  
  if not noDev then begin
    case polarity of
      'Negative': print, 'trigger polarity functionallity not yet implemented'  ;r=call_external(flirdll,"SetTriggerPolarity",0)
      'Positive': print, 'trigger polarity functionallity not yet implemented'  ;r=call_external(flirdll,"SetTriggerPolarity",1)
      else:   print,'trigger polarity',polarity,' not defined!'
    endcase
  endif

end


;**************************************************************
; [not use?]
pro vimbaprev,p0,img=img0

  common vimbaprv_com, wd, p, img1

  p = p0
  p = vimba_setParam(expo=p.expo)

  wd = {wd_prv, $
    START:    0l, $
    STOP:   0l, $
    SNAP:   0l, $
    PROFS:    0l, $
    EXPO:   0.01, $
    BIN:    1, $
    nx:   640, $
    ny:   512, $
    Exit:   0l $
  }

  window, 0, xs=wd.nx, ys=wd.ny

  base = WIDGET_BASE(title='VIMBA prev.', /column)
  b1=widget_base(base, /row )
  wd.START=widget_button(b1, value="Start", uvalue = "START")
  wd.STOP=widget_button(b1, value="Stop", uvalue = "STOP")
  wd.SNAP=widget_button(b1, value="Snap", uvalue = "SNAP")
  wd.PROFS=widget_button(b1, value="Profs", uvalue = "PROFS")
  b2=widget_base(base, /row )
  dmy = widget_label(b2, value='exp:')
  wd.EXPO = widget_text(b2,value=string(p.expo,form='(f5.3)'), xsize=5, uvalue='EXPO',/edit)
  dmy = widget_label(b2, value='s  bin:')
  wd.BIN = widget_text(b2,value=string(p.bin,form='(i2)'), xsize=2, uvalue='BIN',/edit)
  b3=widget_base(base, /row )
  wd.Exit=widget_button(b3, value="Exit", uvalue = "EXIT")


  widget_control, base, /realize
  XMANAGER, 'vimbaprev', base;, /modal

  p0=p
  img0=img1

end

;**************************************************************
pro vimba_handle, ev, wd, p0, img1
  
  common vimba,vimbadll,p,img,noDev,imgs,imgss
  p = p0
  ;--

  dbin=640./wd.p.wx/p.bin

  case (ev.id) of
    wd.PRV_START: begin
      
      nbins=128 & imax=2l^14 -1
      ii=findgen(nbins)/nbins*imax
      x0=p.regionx/dbin
      y0=p.regiony/dbin

      ;flir_trigmode,'Internal'

      ;continuous capturing start
      vimba_startPreview

      set_plot,'z'
      device,set_resolution=[p.Width,p.Height]/dbin
      set_plot,'win'

      while ev.id ne wd.PRV_STOP do begin
        ;img1=orca_getimg(nimg=1) ;,/nowait)
        ;img1=flir_getimg(nimg=1)
        img1=vimba_getPreview()

        img=rebin(img1,p.Width/dbin,p.Height/dbin)>0
        dmin=wd.p.min
        dmax=wd.p.max
        if wd.p.log_on then begin
          img=alog10(img>1)
          dmin=alog10(dmin>1)
          dmax=alog10(dmax>1)
        endif
        img=wd.p.AUTOSCL_ON?bytscl(img):bytscl(img,min=dmin,max=dmax)

        set_plot,'z'
        tv,img
        if wd.p.hist_on then begin
          h=histogram(img1,max=imax,min=0,nbins=nbins)
          plot,ii,h,psym=10, $
            /noerase,/xstyle,charsize=0.5,pos=[0.05,0.05,0.27,0.2],color=127
        endif
        if wd.p.mmm_on then begin
          mmm=uint([min(img1,max=max),mean(img1),max])
          xyouts,0,1,/norm,'!C'+strjoin(['MIN ','MEAN','MAX ']+' '+string(mmm),'!C'),color=127
        endif
        tmp=tvrd()
        set_plot,'win'
        tvscl,tmp,p.regionx/dbin,p.regiony/dbin

        ev = widget_event(wd.PRV_STOP,/nowait)
      endwhile

      vimba_stopPreview    ;capture stop

    end
    wd.EXPO: begin
      p.expo=float(gt_wdtxt(ev.id))
      p=vimba_setParam(expo=p.expo)
    end
    wd.NUC: begin
      ;;NUC_tim = get_systime(ctime=ctime,msec=msec)
      ;;wd_message,'  performing NUC '+ NUC_tim+'  ',base=b,xpos=350,ypos=400,title='FLIR'
      ;;flir_performnuc
      ;print,'NUC performed! '+NUC_tim
      ;;p.NUC_time = msec
      ;;wd_message,destroy=b
      print, "NUC not available in Vimba"
    end
    wd.AUTONUC: begin
      ;;p.AutoNUC=widget_info(wd.AUTONUC,/button_set)
      ;;if p.AutoNUC then print,'AutoNUC On' else print,'AutoNUC Off'
      ;flir_setAutoNUC, p.AutoNUC
      print, "NUC not available in Vimba"
    end
    wd.NUC_INTVAL: begin
      ;;p.NUC_intval=fix(gt_wdtxt(ev.id), type=2)
      ;;print,'NUC interval=',string(p.NUC_intval,form='(i4)'),' min'
      ;if p.AutoNUC then begin
      ;  flir_setAutoNUCInterval, p.NUC_intval
      ;endif
      print, "NUC not available in Vimba"
    end
    wd.GAIN: begin
      ;;widget_control, ev.id, get_value=value
      ;;p.gain=value
      ;;print,'Gain=',string(p.gain,form='(i1)')
      ;;flir_setFPAGain, p.gain
      ;;flir_selectFactoryNUC
      print, "GAIN is fixed to its configuration value (GAIN1)"
    end

    wd.BIN: begin
      ;;p.bin=fix(gt_wdtxt(ev.id))
      ;;dbin=2048./wd.p.wx/p.bin
      ;p=OrcaSetParam(bin=p.bin)
      ;;p=vimba_setParam(bin=p.bin)
      ;;set_wdroi,wd,p
    end
    wd.X0: begin
      ;;p=FlirSetParam(regionx=fix(gt_wdtxt(ev.id))/(4/p.bin)*(4/p.bin))
      ;;set_wdroi,wd,p
    end
    wd.Y0: begin
      ;;p=FlirSetParam(regiony=fix(gt_wdtxt(ev.id))/(4/p.bin)*(4/p.bin))
      ;;set_wdroi,wd,p
    end
    wd.WIDTH: begin
      ;;p=FlirSetParam(width=fix(gt_wdtxt(ev.id))/(4/p.bin)*(4/p.bin))
      ;;set_wdroi,wd,p
    end
    wd.HEIGHT: begin
      ;;p=FlirSetParam(height=fix(gt_wdtxt(ev.id))/(4/p.bin)*(4/p.bin))
      ;;set_wdroi,wd,p
    end
    wd.CBOX: begin
      ;;box_cur1,x0, y0, nx, ny
      ;;four=4/p.bin
      ;;x0-=(x0 mod four)
      ;;y0-=(y0 mod four)
      ;;nx-=(nx mod four)
      ;;ny-=(ny mod four)
      ;;p=FlirSetParam(regionx=x0*dbin,regiony=y0*dbin,width=nx*dbin,height=ny*dbin)
      ;;set_wdroi,wd,p
    end
    wd.FULL: begin
      ;;p=FlirSetParam(regionx=0,regiony=0,width=2048/p.bin,height=2048/p.bin)
      ;;set_wdroi,wd,p
    end
    wd.AUTOSCL:begin
      wd.p.AUTOSCL_ON=widget_info(wd.AUTOSCL,/button_set)
      widget_control,widget_info(wd.MIN,/parent),sensitive=~wd.p.AUTOSCL_ON
    end
    wd.LOG:begin
      wd.p.LOG_ON=widget_info(wd.LOG,/button_set)
    end
    wd.MIN:begin
      widget_control,wd.MIN,get_value=tmp
      wd.p.MIN=long(tmp)
    end
    wd.MAX:begin
      widget_control,wd.MAX,get_value=tmp
      wd.p.MAX=long(tmp)
    end
    else:
  endcase

  p0 = p              ; kouui, 20210817, global p issue

end

;************************************************************************
function vimba_widget,base,p

  wd={wd_vimba_V01,  $
    p: {flir_gui, $
      wx:   	640,   	$ ; window x-size for image display
      wy:   	512,   	$ ;        y-size
      hist_on:  1,  	$ ; histgram on/off
      mmm_on:  	1, 	$ min/mean/max on/off
      autoscl_on:  1, 	$ ; use TVSCL or TV
      log_on:  	0, 	$ ;log display
      min:  	0l,   	$ ;display min
      max:  	2l^14 	$ ;display max
    },$
    PRV_START:  0l,   $
    PRV_STOP: 0l,   $
    EXPO:   0l,   $
    NUC:    0l, $
    AutoNUC:    0l, $
    NUC_INTVAL:   0l, $
    GAIN:  0l,   $
    TEMP:  0l,   $
    BIN:    0l, $
    X0:   0l, $
    Y0:   0l, $
    WIDTH:    0l, $
    HEIGHT:   0l, $
    FULL:   0l, $
    CBOX:   0l, $
    HIST:   0l, $
    AUTOSCL:   0l, $
    LOG:  0l, $
    MIN:  0l, $
    MAX:  0l $
  }

  b2=widget_base(base, /colum, /frame)
  dmy = widget_label(b2, value='>>> VIMBA <<<')
  b21=widget_base(b2, /row)
  dmy = widget_label(b21, value='exp:')
  wd.EXPO = widget_text(b21,value=string(p.expo,form='(f5.3)'), xsize=5, uvalue='EXPO',/edit)
  dmy = widget_label(b21, value='sec, Prev.')
  wd.PRV_START=widget_button(b21, value="Start", uvalue = "PRV_START")
  wd.PRV_STOP=widget_button(b21, value="Stop", uvalue = "PRV_STOP")

  b21b=widget_base(b2, /row)
  wd.NUC=widget_button(b21b, value="NUC", uvalue = "NUC")
  dmy = widget_label(b21b, value=' Auto:')
  b21bb=widget_base(b21b, /row,/ nonex)
  wd.AutoNUC= widget_button(b21bb,value='')
  widget_control,wd.AutoNUC,set_button=p.AutoNUC
  dmy = widget_label(b21b, value='Intval:')
  wd.NUC_INTVAL = widget_text(b21b,value=string(p.NUC_intval,form='(f5.1)'), xsize=5, uvalue='NUC_INTVAL',/edit)
  dmy = widget_label(b21b, value='min')

  b21c=widget_base(b2, /row)
  dmy = widget_label(b21c, value='Gain: ')
  wd.GAIN = cw_bgroup(b21c,['L','M','H'],/row, $
    uvalue="GAIN",/no_release,set_value=p.gain,/exclusive,ysize=25,/frame)
  b21d=widget_base(b2, /row)
  dmy = widget_label(b21d, value='Temp: ')
  ;p.Temp = flir_getFPACurrentTemperature()
  wd.TEMP = widget_text(b21d,value=string(p.Temp,form='(f5.1)'), xsize=5, uvalue='Temp');,/edit)
  dmy = widget_label(b21d, value='C')

  b22=widget_base(b2, /row)
  dmy = widget_label(b22, value='ROI: ')
  wd.FULL=widget_button(b22, value="full", uvalue = "FULL")
  wd.CBOX=widget_button(b22, value="Cbox", uvalue = "CBOX")
  dmy = widget_label(b22, value='   bin:')
  wd.BIN = widget_text(b22,value=string(p.bin,form='(i2)'), xsize=2, uvalue='BIN',/edit)
  widget_control,widget_info(wd.FULL,/parent),sensitive=0

  b23=widget_base(b2, /row)
  dmy = widget_label(b23, value='x0:')
  wd.X0 = widget_text(b23,value=string(p.RegionX,form='(i4)'), xsize=4, uvalue='X0',/edit)
  dmy = widget_label(b23, value='y0:')
  wd.Y0 = widget_text(b23,value=string(p.RegionY,form='(i4)'), xsize=4, uvalue='Y0',/edit)
  dmy = widget_label(b23, value='nx:')
  wd.WIDTH = widget_text(b23,value=string(p.Width,form='(i4)'), xsize=4, uvalue='WIDTH',/edit)
  dmy = widget_label(b23, value='ny:')
  wd.HEIGHT = widget_text(b23,value=string(p.Height,form='(i4)'), xsize=4, uvalue='HEIGHT',/edit)
  widget_control,widget_info(wd.X0,/parent),sensitive=0

  b24=widget_base(b2, /row)

  dmy = widget_label(b24, value='AUTOSCL:')
  b24b=widget_base(b24, /row,/ nonex)
  wd.AUTOSCL= widget_button(b24b,value='')
  widget_control,wd.AUTOSCL,set_button=wd.p.autoscl_on

  dmy = widget_label(b24, value='LOG:')
  b24c=widget_base(b24, /row,/ nonex)
  wd.LOG= widget_button(b24c,value='')
  widget_control,wd.LOG,set_button=wd.p.log_on

  b25=widget_base(b2, /row,sensitive=~wd.p.autoscl_on)
  dmy = widget_label(b25, value='MIN:')
  wd.MIN = widget_text(b25,value=string(wd.p.MIN,form='(i6)'), xsize=6, uvalue='MIN',/edit)
  dmy = widget_label(b25, value='MAX:')
  wd.MAX = widget_text(b25,value=string(wd.p.MAX,form='(i6)'), xsize=6, uvalue='MAX',/edit)

  return,wd

end

