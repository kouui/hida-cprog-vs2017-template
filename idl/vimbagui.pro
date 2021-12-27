; vimbagui.pro
@vimbalib

;**************************************************************
function version
  ver='0.1'   ; 2021/12/21 u.k.    from flirgui  to vimbagui
  return,ver
end

;**************************************************************
pro make_dir,dir
  ;--------------------------------------------------------------
  if !version.arch eq 'x86_64' then $
    oscomdll='Z:\Projects\cprog\VS2010\oscom64\x64\Debug\oscom64.dll' $
  else  oscomdll='Z:\Projects\cprog\VS2008\oscom32\Debug\oscom32.dll'
  d=file_search(dir)
  if d[0] eq '' then dmy=call_external(oscomdll,'Dmkdirs',dir)

end

;**************************************************************
function wdevid, wd
  ;--------------------------------------------------------------
  ; return event ID's (long tags) in wd (widget) structure

  nt0=n_tags(wd)
  typ=lonarr(nt0)
  for i=0,nt0-1 do typ[i]=size(wd.(i),/type)
  ii=where(typ eq 3, nt)
  typ=typ[ii]
  wdid=lonarr(nt)
  for i=0,nt-1 do wdid[i]=wd.(ii[i])

  return,wdid
end

;**************************************************************
pro vimbagui_event, ev
  ;--------------------------------------------------------------
  common vimbagui, wd, wdp, evid_p, o, p, img1, imgs, tim, tem
  
  ; wd  - widget structure
  ; o - obs params
  ; p - VIMBA control parameters

  widget_control, ev.id, get_uvalue=value
  ;;dbin=640./wdp.p.wx/p.bin
  dbin=p.WinResize/p.bin
  x0=p.regionx/dbin
  y0=p.regiony/dbin
  hpos=float([x0,y0,x0,y0])/wdp.p.wx+[0.05,0.05,0.2,0.17]

  if n_elements(wdid_p) eq 0 then evid_p=wdevid(wdp)
  ii=where(ev.id eq evid_p, count)
  if count ne 0 then vimba_handle, ev, wdp, p, img1


  case (ev.id) of
    wd.NIMG: begin
      o.nimg=fix(gt_wdtxt(ev.id))
    end
    wd.SCANMAP: begin
      o.scanmap=widget_info(wd.SCANMAP,/button_set)
      if o.scanmap then begin
	      print,'click cut position'
	      cursor,x,y,/dev
	      if o.cut eq 0 then begin
		      draw,[0,p.Width/dbin],y*[1,1] 
		      o.pos=y
		      window,1,xs=p.Width/dbin,ys=o.nimg
	      endif else begin
		      draw,x*[1,1],[0,p.Height/dbin]
		      o.pos=x
		      window,1,xs=o.nimg,ys=p.Height/dbin
	      endelse
	    print,'pos=',o.pos
      wset,0
      endif
    end
    wd.CUT: begin
      widget_control, ev.id, get_value=value
      o.cut=value
      ccut=['x','y']
      print,'Cut=',ccut[o.cut]
    end
    wd.SNAP: begin
      p.date_obs=get_systime(ctime=ctime)
      img1=vimba_obs(nimg=1) ;;TODO: should we acquire time_arr and frate_arr in SNAP?
      p.date_obs2=get_systime()

      img=rebin(img1,p.Width/dbin,p.Height/dbin)>0
      dmin=wdp.p.min
      dmax=wdp.p.max
      if wdp.p.log_on then begin
        img=alog10(img>1)
        dmin=alog10(dmin>1)
        dmax=alog10(dmax>1)
      endif
      img=wdp.p.AUTOSCL_ON?bytscl(img):bytscl(img,min=dmin,max=dmax)
      tv,img,p.regionx/dbin,p.regiony/dbin
      o.filename=o.fnam+ctime
      widget_control,wd.FILENAME,set_value=o.filename
    end
    wd.GET: begin

      if o.ExTrig then begin
        print, "Trigger functionality not yet implemented"
	    ;;  flir_trigmode,'External'
      ;;  flir_rearmtrigger
	    ;;  print,'External'
      ;;  if o.polarity eq 0 then flir_trigpolarity,'Negative' else flir_trigpolarity,'Positive'
      endif

      ;wait,1.  ;for safety finishing orca_capturemode_****,nimg=o.nimg (when nimg is large)

      p.date_obs=get_systime(ctime=ctime)
      imgs=vimba_obs(nimg=o.nimg,time_arr=tim,frate_arr=fra)
      p.date_obs2=get_systime(ctime=ctime)
      img1=imgs[*,*,o.nimg-1]
      img=rebin(img1,p.Width/dbin,p.Height/dbin)>0
      dmin=wdp.p.min
      dmax=wdp.p.max
      if wdp.p.log_on then begin
        img=alog10(img>1)
        dmin=alog10(dmin>1)
        dmax=alog10(dmax>1)
      endif
      img=wdp.p.AUTOSCL_ON?bytscl(img):bytscl(img,min=dmin,max=dmax)
      tv,img,p.regionx/dbin,p.regiony/dbin
      if o.scanmap then begin
  	    wset,1
	      if o.cut eq 0 then $		
		      tvscl,reform(rebin(imgs[*,o.pos,*],p.Width/dbin,1,o.nimg),p.Width/dbin,o.nimg) ; x-cut
 	      if o.cut eq 1 then $
		      tvscl,transpose(reform(rebin(imgs[o.pos,*,*],1,p.Height/dbin,o.nimg),p.Height/dbin,o.nimg)) ; y-cut
	      wset,0
      endif
      o.filename=o.fnam+ctime
      widget_control,wd.FILENAME,set_value=o.filename

      ;; get temperature functionality not yet implemented
      ;;p.temp = tem[0]
      ;;print, p.temp
      ;;widget_control,wdp.TEMP,set_value=string(p.temp,form='(f5.2)')
    end
    wd.PROFS: begin
      img1=imgs[*,*,0]
      profiles,rebin(img1,wdp.p.wx,wdp.p.wy)
    end
    wd.REV: begin
      imgss=bytarr(p.Width/dbin,p.Height/dbin,o.nimg)
      for i=0,o.nimg-1 do begin
        img=rebin(imgs[*,*,i],p.Width/dbin,p.Height/dbin)>0
        dmin=wdp.p.min
        dmax=wdp.p.max
        if wdp.p.log_on then begin
          img=alog10(img>1)
          dmin=alog10(dmin>1)
          dmax=alog10(dmax>1)
        endif
        img=wdp.p.AUTOSCL_ON?bytscl(img):bytscl(img,min=dmin,max=dmax)
        tv,img,p.regionx/dbin,p.regiony/dbin
        xyouts,10,10,string(i,form='(i4.0)'),/dev
	      imgss[*,*,i]=img
      endfor
      xmovie,imgss,/no_temp
    end
    wd.DXY: begin
      dxy=get_correl_offsets(imgs[p.Width/3:p.Width/3*2,p.Height/3:p.Height/3*2,*])
      window,2,xs=700,ys=800
      !p.multi=[0,1,2]
      plot,dxy[0,*],ytitle='dx'
      plot,dxy[1,*],ytitle='dy'
      wdok,'XY displacement'
      wdelete,2
      !p.multi=0
    end
    wd.SAVE: begin
      outfile=o.outdir+o.filename+'.fits'
      ;;savefits_p,imgs,p,file=outfile
      ;;cwritefits_p,imgs,p,file=outfile,dst_status=dstst,s4pos=s4pos, rotp=18000.0/q.m1.vm,/threading
      cwritefits_p,imgs,p,file=outfile,/threading ;; need dst_status, time_arr, frate_arr
      print,'saved to '+outfile
    end
    wd.OBS_START: begin
      nbins=128 & imax=2l^16 -1
      ii=findgen(nbins)/nbins*imax
      lcount=0l

      ;wait,1.  ;for safety finishing orca_capturemode_****,nimg=o.nimg (when nimg is large)

      date_obs=get_systime(ctime=ctime,msec=msec)

      while ev.id ne wd.OBS_STOP do begin
        if o.ExTrig then begin
          print, "Trigger functionality not yet implemented"
	      ;;  flir_trigmode,'External'
	      ;;  flir_rearmtrigger
	      ;;  ;flir_trigmode,'External'
	      ;;  if o.polarity eq 0 then flir_trigpolarity,'Negative' else flir_trigpolarity,'Positive'
        endif
        if lcount gt 0 then begin
          date_obs=get_systime(ctime=ctime,msec=msec)
          while msec lt msec0+o.dt*1000 do begin
            wait,0.001
            date_obs=get_systime(ctime=ctime,msec=msec)
          endwhile
        endif
        p.date_obs=date_obs
        imgs=vimba_obs(nimg=o.nimg,time_arr=tim,frate_arr=fra)
        p.date_obs2=get_systime(ctime=ctime)
        img1=imgs[*,*,o.nimg-1]
        img=rebin(img1,p.Width/dbin,p.Height/dbin)>0
        dmin=wdp.p.min
        dmax=wdp.p.max
        if wdp.p.log_on then begin
          img=alog10(img>1)
          dmin=alog10(dmin>1)
          dmax=alog10(dmax>1)
        endif
        img=wdp.p.AUTOSCL_ON?bytscl(img):bytscl(img,min=dmin,max=dmax)
        tv,img,p.regionx/dbin,p.regiony/dbin
        if wdp.p.hist_on then begin
          h=histogram(img1,max=imax,min=0,nbins=nbins)
          plot,ii,h,psym=10, $
            /noerase,/xstyle,charsize=0.5,pos=[0.05,0.05,0.27,0.2],color=127
        endif
        if wdp.p.mmm_on then begin
          mmm=uint([min(img1,max=max),mean(img1),max])
          xyouts,p.regionx/dbin,(p.regiony+p.Height)/dbin,/dev,'!C'+strjoin(['MIN ','MEAN','MAX ']+' '+string(mmm),'!C'),color=127
        endif
        if o.scanmap then begin
  	      wset,1
	        if o.cut eq 0 then $		
		        tvscl,reform(rebin(imgs[*,o.pos,*],p.Width/dbin,1,o.nimg),p.Width/dbin,o.nimg) ; x-cut
 	        if o.cut eq 1 then $
		        tvscl,transpose(reform(rebin(imgs[o.pos,*,*],1,p.Height/dbin,o.nimg),p.Height/dbin,o.nimg)) ; y-cut
	        wset,0
        endif

        o.filename=o.fnam+ctime
        widget_control,wd.FILENAME,set_value=o.filename
        outfile=o.outdir+o.filename+'.fits'
        xyouts,x0+100,y0+p.Height/dbin-30,string(lcount,form='(i5)')+'  '+outfile,/dev
        ;savefits_p,imgs,p,file=outfile
        ;;cwritefits_p,imgs,p,file=outfile,dst_status=dstst,s4pos=s4pos, rotp=18000.0/q.m1.vm,/threading
        cwritefits_p,imgs,p,file=outfile,/threading ;; need dst_status, time_arr, frate_arr
	      print,'-> ',outfile
        lcount=lcount+1
        msec0=msec
        ev = widget_event(wd.OBS_STOP,/nowait)
      endwhile
      ;;if o.ExTrig then flir_trigmode,'Internal'
      print,'Obs. stop'
    end
    wd.ExTrig: begin
      widget_control, ev.id, get_value=value
      o.extrig=value
      if o.extrig then begin
        p.TrigMode='External' 
      endif else begin
	      p.TrigMode='Internal'
	      vimba_trigmode,'Internal'
      endelse
      print,'ExTrigger=',o.extrig
    end
    wd.POLARITY: begin
      widget_control, ev.id, get_value=value
      o.polarity=value
      if o.polarity eq 0 then p.TrigPol='Negative' else p.TrigPol='Positive'
      print,'Polarity=',p.TrigPol
    end
    wd.DT: begin
      o.dt=float(gt_wdtxt(ev.id))
    end
    wd.OUTDIR: begin
      o.outdir=gt_wdtxt(ev.id)
      make_dir,o.outdir
    end
    wd.FNAM: begin
      o.fnam=gt_wdtxt(ev.id)
    end
    wd.FILENAME: begin
      o.filename=gt_wdtxt(ev.id)
    end
    wd.LOAD: begin
      file=dialog_pickfile(path=wd.outdir,title='select file')
      imgs=uint(rfits(file,head=fh))
      byteorder,imgs
      imgsize,imgs,nx,ny,nn
      o.nimg=nn
      widget_control,wd.NIMG,set_value=string(o.nimg,form='(i4)')
      RegionX=fits_keyval(fh,'X0',/fix)
      RegionY=fits_keyval(fh,'Y0',/fix)
      bin=fits_keyval(fh,'BIN',/fix)
      Width=nx
      Height=ny
      p=vimba_setParam(bin=bin)
      p=vimba_setParam(regionx=RegionX,regiony=RegionY,width=Width,height=Height)
      set_wdroi,wdp,p
      widget_control,wdp.BIN,set_value=string(p.bin,form='(i2)')
      widget_control,wd.NIMG,set_value=string(o.nimg,form='(i4)')
      ; help,p,/st
      dbin=2048./wdp.p.wx/p.bin
    end
    wd.Exit: begin
      WIDGET_CONTROL, /destroy, ev.top
      ;caio_exit
    end
    else:
  endcase

end

;************************************************************************
function obs_widget,base,o
  ;--------------------------------------------------------------
  wd={wd_obs_V01, $
    NIMG:   0l, $
    DT:   0l,   $
    SCANMAP:   0l,   $
    CUT:   0l,   $
    OBS_START:  0l,   $
    OBS_STOP: 0l,   $
    ExTRIG:   0l, $
    POLARITY:   0l, $
    SNAP:   0l, $
    GET:    0l, $
    PROFS:    0l, $
    REV:    0l, $
    DXY:    0l, $
    OUTDIR:   0l, $
    FNAM:   0l, $
    FILENAME: 0l, $
    SAVE:   0l, $
    LOAD:   0l, $
    Exit:   0l  $
  }

  b1=widget_base(base, /column, /frame)
  dmy = widget_label(b1, value='>>> Obs. <<<')
  b11=widget_base(b1, /row)
  dmy = widget_label(b11, value='nimg=')
  wd.NIMG = widget_text(b11,value=string(o.nimg,form='(i4)'), xsize=5, uvalue='NIMG',/edit)
  dmy = widget_label(b11, value='   dt:')
  wd.DT = widget_text(b11,value=string(o.dt,form='(f5.1)'), xsize=5, uvalue='DT',/edit)
  dmy = widget_label(b11, value='sec')
  b11b=widget_base(b1, /row)
  dmy = widget_label(b11b, value='Scanmap:')
  b11b2=widget_base(b11b, /row,/ nonex)
  wd.SCANMAP= widget_button(b11b2,value='', uvalue='SCANMAP')
  widget_control,wd.SCANMAP,set_button=o.scanmap
  dmy = widget_label(b11b, value='  Cut:')
  wd.CUT  = cw_bgroup(b11b,['x','y'],/row, $
    uvalue="CUT",/no_release,set_value=o.cut,/exclusive,ysize=25,/frame)


  b12=widget_base(b1, /row)
  wd.SNAP=widget_button(b12, value="Snap", uvalue = "SNAP")
  wd.GET=widget_button(b12, value="Get", uvalue = "GET")
  wd.PROFS=widget_button(b12, value="Prof", uvalue = "PROFS")
  wd.REV=widget_button(b12, value="Rev", uvalue = "REV")
  wd.DXY=widget_button(b12, value="Dxy", uvalue = "DXY")
  b14=widget_base(b1, /row)
  wd_label = widget_label(b14,value='ExTrig:')
  setv=o.extrig
  wd.ExTrig  = cw_bgroup(b14,['Non','External'],/row, $
    uvalue="ExTrig",/no_release,set_value=setv,/exclusive,ysize=25,/frame)
  b14b=widget_base(b1, /row)
  wd_label = widget_label(b14b,value='Polarity:')
  setv=o.polarity
  wd.POLARITY  = cw_bgroup(b14b,['Neg.','Pos.'],/row, $
    uvalue="Polarity",/no_release,set_value=setv,/exclusive,ysize=25,/frame)
  b13=widget_base(b1, /row)
  dmy = widget_label(b13, value='Obs. ')
  wd.OBS_START=widget_button(b13, value="Start", uvalue = "PRV_START")
  wd.OBS_STOP=widget_button(b13, value="Stop", uvalue = "PRV_STOP")
  wd.SAVE=widget_button(b13, value="Save", uvalue = "SAVE")

  ;b3=widget_base(base, /column, /frame )
  b3=b1
  b31=widget_base(b3, /row )
  dmy = widget_label(b31, value='outdir: ')
  wd.OUTDIR=widget_text(b31,value=o.outdir, xsize=25, ysize=1,uvalue='outdir',/edit)
  b33=widget_base(b3, /row )
  dmy = widget_label(b33, value='filenam:')
  wd.FILENAME=widget_text(b33,value=o.filename, xsize=23, ysize=1,uvalue='filename',/edit)
  dmy = widget_label(b33, value='.fits')
  b32=widget_base(b3, /row )
  dmy = widget_label(b32, value='fnam: ')
  wd.FNAM=widget_text(b32,value=o.fnam, xsize=20, ysize=1,uvalue='fnam',/edit)
  ; b34=widget_base(b3, /row )
  wd.LOAD=widget_button(b32, value="Load", uvalue = "Load")
  b4=widget_base(base, /row )
  wd.Exit=widget_button(b4, value="Exit", uvalue = "Exit")

  return,wd
end

;************************************************************************
;  main
;--------------------------------------------------------------
; pro vimbagui
; .compile 'C:\Projects\IDLPRO\flir\flirgui.pro'
; resolve_all, skip_routines='YOH_IEEE2VAX'
; save,/routines,filename='C:\Projects\IDLPRO\flir\flirgui.sav'

common vimbagui, wd, wdp, evid_p, o, p, img1, imgs, tim, tem
;;COMMON bridge,bridge
; wd  - obs widget structure
; wdp - camera widget structure
; o - obs control parameters
; p - camera control parameters

xmlFile="Z:\\Projects\\conf\\Vimba\\prosilica.000F3101ABD0.20211225.xml"
cameraID = 'DEV_000F3101ABD0'
p=vimba_init("prosilica",cameraId,xmlFile);,/noDev)
p=vimba_setParam(expo=0.015)

;-----  prepare object array for parallel processing -----------
;;nCPU=!CPU.HW_NCPU
;;bridge=objarr(nCPU-1)
;;for i=0,nCPU-2 do bridge[i]=obj_new('IDL_IDLBridge')

;settriggermode
;settriggerpolarity

o={vimba_obs_v01, $  ; obs. control params
  nimg:     1,  $ ; # of image for 1 shot
  dt:       0., $ ; time step, sec
  scanmap:  0,  $ ; 1: display scan map, 0: not
  cut:      0,  $ ; 0: x, 1: y
  pos:    512,  $ ; position of cut
  extrig:   0,  $ ; 0: free, 1: extrn.trig.
  polarity: 0,  $ ; 0: negative, 1: positive
  date:     '',   $
  outdir:   '',   $
  fnam:     'vimba', $
  filename: ''  $
}

ccsds=get_systime(ctime=ctime)
o.date=strmid(ctime,0,8)
o.outdir='D:\data\'+o.date+'\'
make_dir,o.outdir

base = WIDGET_BASE(title='VIMBA-GUI:  Ver.'+version(), /column)

wdp=vimba_widget(base,p)
wd=obs_widget(base,o)


widget_control, base, /realize

window,xs=wdp.p.wx,ys=wdp.p.wy

XMANAGER, 'vimbagui', base

vimba_close

;;for i=0,n_elements(bridge)-1 do obj_destroy,bridge[i]

end
