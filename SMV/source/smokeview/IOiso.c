// $Date$ 
// $Revision$
// $Author$

// svn revision character string
char IOiso_revision[]="$Revision$";

#include "options.h"
#include <stdio.h>  
#include <stdlib.h>
#include <string.h>
#ifdef pp_OSX
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include <math.h>

#include "egz_stdio.h"
#include "string_util.h"
#include "update.h"
#include "smokeviewvars.h"

void sync_isobounds(int isottype);
void unloadiso(mesh *gb);

/* ------------------ getisolevels ------------------------ */

void getisolevels(const char *isofile, int dataflag, float **levelsptr, float ***colorlevelsptr, int *nisolevels){
  int one;
  int version;
  int len[3],labellengths=0;
  int nlevels;
  EGZ_FILE *isostreamptr;
  int i;
  float **colorlevels=NULL;

#ifdef EGZ
  isostreamptr=EGZ_FOPEN(isofile,"rb",0,2);
#else
  isostreamptr=EGZ_FOPEN(isofile,"rb");
#endif

  EGZ_FREAD(&one,4,1,isostreamptr);
  if(dataflag!=0){
    EGZ_FREAD(&version,4,1,isostreamptr);
  }
  else{
    version=1;
  }
  EGZ_FREAD(len,4,3,isostreamptr);
  labellengths=len[0]+len[1]+len[2];
  EGZ_FSEEK(isostreamptr,labellengths+4,SEEK_CUR);
  EGZ_FREAD(&nlevels,4,1,isostreamptr);
  *nisolevels=nlevels;
  FREEMEMORY(*levelsptr);
  NewMemory((void **)levelsptr,nlevels*sizeof(float));
  EGZ_FREAD(*levelsptr,4,(unsigned int)(nlevels),isostreamptr);
  EGZ_FCLOSE(isostreamptr);
  NewMemory((void **)&colorlevels,nlevels*sizeof(float *));
  for(i=0;i<nlevels;i++){
    colorlevels[i]=NULL;
  }
  *colorlevelsptr=colorlevels;
  
}

/* ------------------ getisosizes ------------------------ */

void getisosizes(const char *isofile, int dataflag, EGZ_FILE **isostreamptr, int *nvertices, int *ntriangles, 
                 float **levelsptr, int *nisolevels, int *niso_times, 
                 float *tmin_local, float *tmax_local, int endian_local){
	int len[3],labellengths=0;
	int nlevels, n;
  int nvertices_i, ntriangles_i;
	int i;
	float time_local, time_max;
	int beg;
	int version;
  int one;
  int skip_local;
  float ttmin, ttmax;

#ifdef EGZ
  *isostreamptr=EGZ_FOPEN(isofile,"rb",0,2);
#else
  *isostreamptr=EGZ_FOPEN(isofile,"rb");
#endif

  *tmin_local=1000000000.;
  *tmax_local=-1000000000.;
  EGZ_FREAD(&one,4,1,*isostreamptr);
  if(dataflag!=0){
    EGZ_FREAD(&version,4,1,*isostreamptr);
  }
  else{
    version=1;
  }
  EGZ_FREAD(len,4,3,*isostreamptr);
  labellengths=len[0]+len[1]+len[2];
  EGZ_FSEEK(*isostreamptr,labellengths+4,SEEK_CUR);
  EGZ_FREAD(&nlevels,4,1,*isostreamptr);
  *nisolevels=nlevels;
  if(*levelsptr==NULL){
    if(NewMemory((void **)levelsptr,*nisolevels*sizeof(float))==0)return;
  }
  else{
    if(ResizeMemory((void **)levelsptr,*nisolevels*sizeof(float))==0)return;
  }
  EGZ_FREAD(*levelsptr,4,(unsigned int)(nlevels),*isostreamptr);
  *niso_times=0; *nvertices=0; *ntriangles=0;
  beg=EGZ_FTELL(*isostreamptr);
  i=0;
  time_max=-1000000.0;
  for(;;){
    int skip_frame;

    {EGZ_FREAD(&time_local,4,1,*isostreamptr);}
    skip_frame=1;
    if(time_local>time_max){
      skip_frame=0;
      time_max=time_local;
    }
    nvertices_i=0;
    ntriangles_i=0;
    if(EGZ_FEOF(*isostreamptr)!=0)break;
    for(n=0;n<nlevels;n++){
      {EGZ_FREAD(&nvertices_i,4,1,*isostreamptr);}
      if(EGZ_FEOF(*isostreamptr)!=0)break;
      {EGZ_FREAD(&ntriangles_i,4,1,*isostreamptr);}
      if(EGZ_FEOF(*isostreamptr)!=0)break;
      skip_local=0;
      if(nvertices_i>0){
        skip_local += 6*nvertices_i;
        EGZ_FSEEK(*isostreamptr,skip_local,SEEK_CUR);
        skip_local=0;
        if(dataflag==1){
          EGZ_FREAD(&ttmin,4,1,*isostreamptr);
          if(ttmin<*tmin_local)*tmin_local=ttmin;
          EGZ_FREAD(&ttmax,4,1,*isostreamptr);
          if(ttmax>*tmax_local)*tmax_local=ttmax;
          skip_local += 2*nvertices_i;
        }
      }
      if(nvertices_i<256){                 /* number of triangles */
	      skip_local+=ntriangles_i;
      }
      else if(nvertices_i>=256&&nvertices_i<65536){
	      skip_local+=ntriangles_i*2;
      }
      else{
	      skip_local+=ntriangles_i*4;
      }
      {EGZ_FSEEK(*isostreamptr,skip_local,SEEK_CUR);}
    }
    if(skip_frame==1)continue;
    i++;
    if(i%isoframestep_global!=0)continue;
    if((settmin_i==1&&time_local<tmin_i))continue;
    if((settmax_i==1&&time_local>tmax_i))continue;

    *nvertices += nvertices_i;
	  *ntriangles += ntriangles_i;
	  *niso_times += 1;
  }
  EGZ_FSEEK(*isostreamptr,beg,SEEK_SET);
  if(dataflag==1&&axissmooth==1){
    smoothlabel(tmin_local,tmax_local,nrgb);
  }
}

/* ------------------ readiso_geom ------------------------ */

void readiso_geom(const char *file, int ifile, int flag, int *errorcode){
  isodata *isoi;
  geomdata *geomi;
  int ilevel,error;
  mesh *meshi;
  int i;
  surfdata *surfi;

  isoi = isoinfo + ifile;
  meshi = meshinfo + isoi->blocknumber;
  geomi = isoi->geominfo;
  unloadiso(meshi);

  read_geom(geomi,flag,GEOM_ISO,errorcode);
  if(flag==UNLOAD){
    FREEMEMORY(meshi->iso_times);
    FREEMEMORY(meshi->showlevels);
    meshi->isofilenum=-1;
    return;
  }

  surfi = surfinfo + nsurfinfo+1;
  update_isocolors();
  if(strcmp(isoi->surface_label.shortlabel,"hrrpuv")==0){
    surfi->color=getcolorptr(hrrpuv_iso_color);
  }

  meshi->isofilenum=ifile;
  meshi->niso_times=geomi->ntimes;
  if(NewMemory((void **)&meshi->iso_times,sizeof(float)*meshi->niso_times)==0){
    readiso("",ifile,UNLOAD,&error);
    *errorcode=1;
    return;
  }
  for(i=0;i<geomi->ntimes;i++){
    meshi->iso_times[i]=geomi->times[i];
  }

  meshi->nisolevels=geomi->nfloat_vals;
  FREEMEMORY(meshi->isolevels);
  if(
    NewMemory((void **)&meshi->showlevels,sizeof(int)*meshi->nisolevels)==0||
    NewMemory((void **)&meshi->isolevels,sizeof(int)*meshi->nisolevels)==0
    ){
    *errorcode=1;
    readiso("",ifile,UNLOAD,&error);
    return;
  }
  for(ilevel=0;ilevel<meshi->nisolevels;ilevel++){
    meshi->showlevels[ilevel]=1;
    meshi->isolevels[ilevel]=geomi->float_vals[ilevel];
  }
  isoi->loaded=1;
  isoi->display=1;
  loaded_isomesh=get_loaded_isomesh();
  update_iso_showlevels();
  ReadIsoFile=1;
  plotstate=getplotstate(DYNAMIC_PLOTS);
  updatemenu=1;
  iisotype=getisotype(isoi);

  FREEMEMORY(geominfoptrs);
  ngeominfoptrs=0;
  GetGeomInfoPtrs(&geominfoptrs,&ngeominfoptrs);
  update_triangles();

  updatetimes();
  get_faceinfo();
#ifdef _DEBUG
  printf("After iso load: ");
  PrintMemoryInfo;
#endif
  Idle();

  glutPostRedisplay();
  CheckMemory;

}

/* ------------------ readiso_orig ------------------------ */

void readiso_orig(const char *file, int ifile, int flag, int *errorcode){
  int itime,ilevel,itri,ivert,iitime;
  isosurface *asurface;
  int nisopoints, nisotriangles;
#ifdef _DEBUG
  int ntotal_isotris=0, ntotal_isoverts=0;
#endif
  float time_local, time_max;
  EGZ_FILE *isostream;
  int break_frame;
  int skip_local;

  int blocknumber;
  int error;
  float factor, offset[3];
  float *iso_colors;
  int n_iso_colors;
  
  mesh *meshi;
  isodata *ib;

  int local_starttime=0, local_stoptime=0;
  FILE_SIZE file_size=0;
  int local_starttime0=0, local_stoptime0=0;  
  float delta_time, delta_time0;

  local_starttime0 = glutGet(GLUT_ELAPSED_TIME);
  
  ASSERT(ifile>=0&&ifile<nisoinfo);
  ib = isoinfo+ifile;
  if(ib->loaded==0&&flag==UNLOAD)return;
  blocknumber=ib->blocknumber;
  ib->isoupdate_timestep=-1;
  meshi = meshinfo+blocknumber;
  unloadiso(meshi);
  unload_iso_trans();
  ib->loaded=0;
  ib->display=0;
  plotstate=getplotstate(DYNAMIC_PLOTS);
  updatetimes();
  *errorcode = 0;

#ifdef _DEBUG
  if(flag==UNLOAD){
    printf("After iso unload: ");
    PrintAllMemoryInfo;
  }
#endif
  update_isotype();
  if(flag==UNLOAD){
    updatemenu=1;
    loaded_isomesh=get_loaded_isomesh();
    update_iso_showlevels();
    return;
  }
  meshi->isofilenum=ifile;
  highlight_mesh = blocknumber;
  
  factor = (meshi->xyzmaxdiff/xyzmaxdiff)/65535.0;
  offset[0]=(meshi->xbar0-xbar0)/xyzmaxdiff;
  offset[1]=(meshi->ybar0-ybar0)/xyzmaxdiff;
  offset[2]=(meshi->zbar0-zbar0)/xyzmaxdiff;

  if(iso_ambient_ini==NULL||n_iso_ambient_ini==0){
    iso_colors=iso_ambient;
    n_iso_colors=n_iso_ambient;
  }
  else{
    iso_colors=iso_ambient_ini;
    n_iso_colors=n_iso_ambient_ini;
  }

  getisosizes(file, ib->dataflag, &isostream, &nisopoints, &nisotriangles, 
    &meshi->isolevels, &meshi->nisolevels, &meshi->niso_times, 
    &ib->tmin, &ib->tmax, endian_data);

  file_size=get_filesize(file);

  if(meshi->isolevels==NULL){
    readiso("",ifile,UNLOAD,&error);
    *errorcode=1;
    return;
  }               
  if(NewMemory((void **)&meshi->iso_times,sizeof(float)*meshi->niso_times)==0){
    readiso("",ifile,UNLOAD,&error);
    *errorcode=1;
    return;
  }
  if(NewMemory((void **)&meshi->showlevels,sizeof(int)*meshi->nisolevels)==0){
    *errorcode=1;
    readiso("",ifile,UNLOAD,&error);
    return;
  }
  for(ilevel=0;ilevel<meshi->nisolevels;ilevel++){
    meshi->showlevels[ilevel]=1;
  }
  isomin=meshi->isolevels[0];
  isomax=meshi->isolevels[0];
  meshi->isomin_index=0;
  meshi->isomax_index=0;
  for(ilevel=1;ilevel<meshi->nisolevels;ilevel++){
    if(meshi->isolevels[ilevel]<isomin){
      isomin=meshi->isolevels[ilevel];
      meshi->isomin_index=ilevel;
    }
    if(meshi->isolevels[ilevel]>isomax){
      isomax=meshi->isolevels[ilevel];
      meshi->isomax_index=ilevel;
    }
  }
  ASSERT(meshi->animatedsurfaces==NULL);
  if(NewMemory((void **)&meshi->animatedsurfaces,meshi->nisolevels*meshi->niso_times*sizeof(isosurface))==0){
    *errorcode=1;
    readiso("",ifile,UNLOAD,&error);
    return;
  }
  if(ResizeMemory((void **)&meshi->iso_times,sizeof(float)*meshi->niso_times)==0){
    *errorcode=1;
    readiso("",ifile,UNLOAD,&error);
    return;
  }

  asurface=meshi->animatedsurfaces;
  break_frame=0;
  iitime=0;
  itime=0;
  time_max = -1000000.0;
  local_starttime = glutGet(GLUT_ELAPSED_TIME);
  for(;;){
    int skip_frame;
    int ntri_total;

    skip_frame=0;
    iitime++;

    EGZ_FREAD(&time_local,4,1,isostream);
    if(EGZ_FEOF(isostream)!=0)break;
    skip_frame=1;
    if(time_local>time_max){
      skip_frame=0;
      time_max=time_local;
    }
    meshi->iso_times[itime]=time_local;
    if(iitime%isoframestep_global!=0||(settmin_i==1&&time_local<tmin_i)||(settmax_i==1&&time_local>tmax_i)||skip_frame==1){
    }
    else{
      printf("isosurface time=%f\n",time_local);
    }
    ntri_total=0;
    for(ilevel=0;ilevel<meshi->nisolevels;ilevel++){
      int nvertices_i, ntriangles_i;
          
      asurface->dataflag=ib->dataflag;
        
      EGZ_FREAD(&nvertices_i,4,1,isostream);
#ifdef _DEBUG
      ntotal_isoverts+=nvertices_i;
#endif
      if(EGZ_FEOF(isostream)!=0)break;
      EGZ_FREAD(&ntriangles_i,4,1,isostream);
#ifdef _DEBUG
      ntotal_isotris+=ntriangles_i;
#endif
      if(EGZ_FEOF(isostream)!=0)break;
      asurface->niso_triangles=ntriangles_i/3;
      asurface->niso_vertices=nvertices_i;
        
      if(iitime%isoframestep_global!=0||(settmin_i==1&&time_local<tmin_i)||(settmax_i==1&&time_local>tmax_i)||skip_frame==1){
        skip_local=0;
        if(nvertices_i<=0||ntriangles_i<=0)continue;
        skip_local += (6*nvertices_i);
        if(ib->dataflag==1)skip_local += (8 + 2*nvertices_i);
  	    if(nvertices_i<256){
  	      skip_local += (ntriangles_i);
        }
        else if(nvertices_i>=256&&nvertices_i<65536){
  	      skip_local += (ntriangles_i*2);
        }
	      else{
          skip_local += (ntriangles_i*4);
        }
        EGZ_FSEEK(isostream,skip_local,SEEK_CUR);
        continue;
      }
      
      asurface->iso_triangles=NULL;
      asurface->iso_vertices=NULL; 
      if(nvertices_i>0){
        unsigned short *verti;
        unsigned short *vertices_i;
          
        if(NewMemory((void **)&asurface->iso_vertices,nvertices_i*sizeof(isovert))==0){
          break_frame=1;
          break;
        }
        if(NewMemory((void **)&vertices_i,3*nvertices_i*sizeof(unsigned short))==0){
          break_frame=1;
          break;
        }
        verti = vertices_i;
        EGZ_FREAD(vertices_i,2,(unsigned int)(3*nvertices_i),isostream);
        for(ivert=0;ivert<nvertices_i;ivert++){
          isovert *isoverti;
          float *xyz;
            
          isoverti = asurface->iso_vertices+ivert;
          xyz = isoverti->xyz;
          xyz[0]=offset[0]+factor*(*verti++); 
          xyz[1]=offset[1]+factor*(*verti++); 
          xyz[2]=offset[2]+factor*(*verti++); 
          isoverti->flag=0;
            
          if(ilevel==0&&strcmp(ib->surface_label.shortlabel,"hrrpuv")==0){
            isoverti->color=hrrpuv_iso_color;
          }
          else{
            isoverti->color=iso_colors+4*ilevel;
          }
        }
        FREEMEMORY(vertices_i);

        if(ib->dataflag==1){
          unsigned short *tvertices_i;
          float tcolor0, tcolorfactor, tcolor;
          
          EGZ_FREAD(&asurface->tmin,4,1,isostream);
          EGZ_FREAD(&asurface->tmax,4,1,isostream);
          if(NewMemory((void **)&tvertices_i,nvertices_i*sizeof(unsigned short))==0){
            break_frame=1;
            break;
          }
          EGZ_FREAD(tvertices_i,2,(unsigned int)nvertices_i,isostream);
          if(ib->tmax>ib->tmin){
            tcolor0 = (asurface->tmin-ib->tmin)/(ib->tmax-ib->tmin);
            tcolorfactor = (asurface->tmax-asurface->tmin)/65535.;
            tcolorfactor /= (ib->tmax-ib->tmin);
          }
          else{
            tcolor0=0.5;
            tcolorfactor=0.0;
          }
          for(ivert=0;ivert<nvertices_i;ivert++){
            isovert *isoverti;
            unsigned char colorindex;
                          
            isoverti = asurface->iso_vertices+ivert;
            tcolor = tcolor0 + tvertices_i[ivert]*tcolorfactor;
            if(tcolor<0.0)tcolor=0.0;
            if(tcolor>1.0)tcolor=1.0;
            colorindex = (unsigned char)(tcolor*255);
            isoverti->color = rgb_iso+4*colorindex;
            isoverti->ctexturecolor=colorindex;
          }
          FREEMEMORY(tvertices_i);
        }
      }
      if(EGZ_FEOF(isostream)!=0)break;
      if(ntriangles_i>0){
        unsigned char *triangles1_i;
        unsigned short *triangles2_i;
        int *triangles_i;
          
        if(NewMemory((void **)&triangles_i,ntriangles_i*sizeof(int))==0){
          break_frame=1;
          break;
        }
        if(nvertices_i<256&&nvertices_i>0){
          if(NewMemory((void **)&triangles1_i,ntriangles_i*sizeof(unsigned char))==0){
            break_frame=1;
            break;
          }
          EGZ_FREAD(triangles1_i,1,(unsigned int)ntriangles_i,isostream);
          for(itri=0;itri<ntriangles_i;itri++){
            triangles_i[itri]=triangles1_i[itri];
          }
          FREEMEMORY(triangles1_i);
        }
        else if(nvertices_i>=256&&nvertices_i<65536){
          if(NewMemory((void **)&triangles2_i,ntriangles_i*sizeof(unsigned short))==0){
            break_frame=1;
            break;
          }
          EGZ_FREAD(triangles2_i,2,(unsigned int)ntriangles_i,isostream);
          for(itri=0;itri<ntriangles_i;itri++){
            triangles_i[itri]=triangles2_i[itri];
          }
          FREEMEMORY(triangles2_i);
        }
        else{
          EGZ_FREAD(triangles_i,4,(unsigned int)ntriangles_i,isostream);
        } 
        if(NewMemory((void **)&asurface->iso_triangles,(ntriangles_i/3)*sizeof(isotri))==0){
          break_frame=1;
          break;
        }
        for(itri=0;itri<ntriangles_i/3;itri++){
          isotri *isotrii;
          float **color;
              
          isotrii=asurface->iso_triangles+itri;
          isotrii->v1=asurface->iso_vertices+triangles_i[3*itri];
          isotrii->v2=asurface->iso_vertices+triangles_i[3*itri+1];
          isotrii->v3=asurface->iso_vertices+triangles_i[3*itri+2];
          if(ilevel==0&&strcmp(ib->surface_label.shortlabel,"hrrpuv")==0){
            ib->colorlevels[ilevel]=hrrpuv_iso_color;
          }
          else{
            ib->colorlevels[ilevel]=iso_colors+4*ilevel;
          }
          color=ib->colorlevels+ilevel;
          if(ib->dataflag==0){
            isotrii->v1->color=*color;
            isotrii->v2->color=*color;
            isotrii->v3->color=*color;
          }
        }
        FREEMEMORY(triangles_i);
      }
        
      if(EGZ_FEOF(isostream)!=0)break;

      if(nvertices_i>0){
        float *vertnorms=NULL;
        if(NewMemory((void **)&vertnorms,3*nvertices_i*sizeof(float))==0){
          break_frame=1;
          break;
        }
        for(ivert=0;ivert<nvertices_i;ivert++){
          vertnorms[3*ivert]=0.0;
          vertnorms[3*ivert+1]=0.0;
          vertnorms[3*ivert+2]=0.0;
        }
        for(itri=0;itri<ntriangles_i/3;itri++){
          isotri *isotrii;
          float *v1, *v2, *v3;
          float *vertnorm;
          float area;
          float out[3];
                    
          isotrii = asurface->iso_triangles+itri;
          v1=isotrii->v1->xyz;
          v2=isotrii->v2->xyz;
          v3=isotrii->v3->xyz;
          calcNormal2f(v1,v2,v3,out,&area);
          
          vertnorm = vertnorms + 3*(isotrii->v1-asurface->iso_vertices);
          vertnorm[0] += out[0]*area;
          vertnorm[1] += out[1]*area;
          vertnorm[2] += out[2]*area;
        
          vertnorm = vertnorms + 3*(isotrii->v2-asurface->iso_vertices);
          vertnorm[0] += out[0]*area;
          vertnorm[1] += out[1]*area;
          vertnorm[2] += out[2]*area;
          
          vertnorm = vertnorms + 3*(isotrii->v3-asurface->iso_vertices);
          vertnorm[0] += out[0]*area;
          vertnorm[1] += out[1]*area;
          vertnorm[2] += out[2]*area;
        }
        for(ivert=0;ivert<nvertices_i;ivert++){
          isovert *v1;

          v1 = asurface->iso_vertices + ivert;
          ReduceToUnit(vertnorms+3*ivert);
          v1->cnorm=(unsigned char)getnormalindex(sphereinfo,vertnorms+3*ivert);
        }
        FREEMEMORY(vertnorms);
      }
      ntri_total+=asurface->niso_triangles;
      asurface++;
    }
   
    if(break_frame==1){
      printf("*** warning: memory allocation attempt failed at time step: %i while reading isosurface file\n",itime);
      meshi->niso_times=itime;
      break;
    }
    if(skip_frame==1||iitime%isoframestep_global!=0||(settmin_i==1&&time_local<tmin_i)||(settmax_i==1&&time_local>tmax_i)){
    }
    else{
      itime++;
      if(itime>=meshi->niso_times)break;
    }
  }
#ifdef _DEBUG
  printf("nverts=%i ntris=%i\n",ntotal_isoverts,ntotal_isotris);
  printf("size verts=%i tris=%i\n",(int)(ntotal_isoverts*sizeof(isovert)),(int)(ntotal_isotris*sizeof(isotri)/3));
#endif

  local_stoptime = glutGet(GLUT_ELAPSED_TIME);
  delta_time = (local_stoptime-local_starttime)/1000.0;
  EGZ_FCLOSE(isostream);
  if(*errorcode!=0){
    unloadiso(meshi);
    readiso("",ifile,UNLOAD,&error);
    return;
    }


  ib->loaded=1;
  ib->display=1;
  loaded_isomesh=get_loaded_isomesh();
  update_iso_showlevels();
  ReadIsoFile=1;
  plotstate=getplotstate(DYNAMIC_PLOTS);
  updatemenu=1;
  iisotype=getisotype(ib);

  if(ib->dataflag==1){
    iisottype = getisottype(ib);
    sync_isobounds(iisottype);
    setisolabels(ib->tmin, ib->tmax, ib, errorcode);
    CheckMemory;
  }

  updatetimes();
#ifdef _DEBUG
  printf("After iso load: ");
  PrintMemoryInfo;
#endif
  Idle();

  local_stoptime0 = glutGet(GLUT_ELAPSED_TIME);
  delta_time0=(local_stoptime0-local_starttime0)/1000.0;

  if(file_size!=0&&delta_time>0.0){
    float loadrate;

    loadrate = ((float)file_size*8.0/1000000.0)/delta_time;
    printf(" %.1f MB loaded in %.2f s - rate: %.1f Mb/s (overhead: %.2f s)\n",
    (float)file_size/1000000.,delta_time,loadrate,delta_time0-delta_time);
  }
  else{
    printf(" %.1f MB downloaded in %.2f s (overhead: %.2f s)",
    (float)file_size/1000000.,delta_time,delta_time0-delta_time);
  }

  glutPostRedisplay();
  CheckMemory;
}

/* ------------------ readiso ------------------------ */

void readiso(const char *file, int ifile, int flag, int *errorcode){
  isodata *isoi;

  if(ifile>=0&&ifile<nisoinfo){
    isoi = isoinfo + ifile;
    if(isoi->is_fed==1){
      readfed(ifile, flag, FED_ISO, errorcode);
    }
    else{
      if(isoi->geomflag==1){
        readiso_geom(file,ifile,flag,errorcode);
      }
      else{
        readiso_orig(file,ifile,flag,errorcode);
      }
    }
  }
}

/* ------------------ unloadiso_iso_trans ------------------------ */

void unload_iso_trans(void){
  if(iso_trans_list!=NULL){
    int i;

    for(i=0;i<niso_timesteps;i++){
      FREEMEMORY(iso_trans_list[i]);
    }
    FREEMEMORY(niso_trans_list);
    FREEMEMORY(iso_trans_list);
  }
  if(iso_opaques_list!=NULL){
      int i;

      for(i=0;i<niso_timesteps;i++){
        FREEMEMORY(iso_opaques_list[i]);
      }
    FREEMEMORY(niso_opaques_list);
    FREEMEMORY(iso_opaques_list);
  }

  niso_trans=0;
  niso_opaques=0;
}

/* ------------------ unloadiso ------------------------ */

void unloadiso(mesh *meshi){
  isosurface *asurface;
  isodata *ib;
  int nloaded=0;
  int i;
  mesh *meshi2;

  if(meshi->isofilenum==-1)return;
  ib = isoinfo + meshi->isofilenum;
  if(meshi->niso_times>0&&meshi->nisolevels>0){
    if(meshi->animatedsurfaces!=NULL){
      for(i=0;i<meshi->niso_times*meshi->nisolevels;i++){
        asurface=meshi->animatedsurfaces+i;
        FREEMEMORY(asurface->iso_triangles);
        FREEMEMORY(asurface->iso_vertices);
      }
    }
    CheckMemoryOff;
    FREEMEMORY(meshi->iso_times);
    FREEMEMORY(meshi->animatedsurfaces);
    FREEMEMORY(meshi->showlevels);
  }
  meshi->niso_times=0;
  FREEMEMORY(ib->normaltable);

  unload_iso_trans();

  ib->loaded=0;
  ib->display=0;
  plotstate=getplotstate(DYNAMIC_PLOTS);
  meshi->isofilenum=-1;
  for(i=0;i<nmeshes;i++){
    meshi2 = meshinfo+i;
    if(meshi2->isofilenum!=-1)nloaded++;
  }
  if(nloaded==0){
    ReadIsoFile=0;
  }

  updatetimes();
  updatemenu=1;
  Idle();

  return;
}

/* ------------------ drawiso_geom ------------------------ */

void drawiso_geom(int tranflag){
  draw_geom(tranflag,1);
}

/* ------------------ drawiso ------------------------ */

void drawiso_orig(int tranflag){
  int i;
  isosurface *asurface;
  float *iso_colors;
  int n_iso_colors;
  int *showlevels, nisolevels;
  isodata *isoi=NULL;
  int iso_lighting;
  mesh *meshi;

  meshi = loaded_isomesh;

  CheckMemory;
  if(tranflag==DRAW_TRANSPARENT&&((visAIso&1)==0))return;
  if(meshi->isofilenum>=0){
    isoi = isoinfo + meshi->isofilenum;
  }

  iso_lighting=1;

  showlevels=meshi->showlevels;
  nisolevels=meshi->nisolevels;

  if(iso_ambient_ini==NULL||n_iso_ambient_ini==0){
    iso_colors=iso_ambient;
    n_iso_colors=n_iso_ambient;
  }
  else{
    iso_colors=iso_ambient_ini;
    n_iso_colors=n_iso_ambient_ini;
  }

  if((visAIso&1)==1){
    isotri **iso_list_start;
    int niso_list_start;

    asurface = meshi->animatedsurfaces + meshi->iso_itime*meshi->nisolevels;
    if(cullfaces==1)glDisable(GL_CULL_FACE);

    iso_specular[3] = 1.0;
    if(tranflag==DRAW_TRANSPARENT)transparenton();

    if(usetexturebar==1&&isoi->dataflag==1){
      glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
      glEnable(GL_TEXTURE_1D);
      glBindTexture(GL_TEXTURE_1D,texture_iso_colorbar_id);
    }

    glPushAttrib(GL_LIGHTING_BIT);
    if(iso_lighting==1){
      glEnable(GL_LIGHTING);
      glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,iso_specular);
      glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,iso_shininess);
      glEnable(GL_COLOR_MATERIAL);
    }
    glBegin(GL_TRIANGLES);

    if(tranflag==DRAW_TRANSPARENT){
      iso_list_start=iso_trans;
      niso_list_start=niso_trans;
    }
    else{
      iso_list_start=iso_opaques;
      niso_list_start=niso_opaques;
    }
    CheckMemory;
    if(usetexturebar==1&&isoi->dataflag==1){
      for(i=0;i<niso_list_start;i++){
        isotri *tri;
        isovert *v1, *v2, *v3;
        
        tri=iso_list_start[i];

        v1 = tri->v1;
        v2 = tri->v2;
        v3 = tri->v3;

        glTexCoord1f(255.0*v1->ctexturecolor);
        glNormal3fv(getnormalvectorptr(sphereinfo,v1->cnorm));
        glVertex3fv(v1->xyz);
        
        glTexCoord1f(255.0*v2->ctexturecolor);
        glNormal3fv(getnormalvectorptr(sphereinfo,v2->cnorm));
        glVertex3fv(v2->xyz);
        
        glTexCoord1f(255.0*v3->ctexturecolor);
        glNormal3fv(getnormalvectorptr(sphereinfo,v3->cnorm));
        glVertex3fv(v3->xyz);
      }
    }
    else{
      for(i=0;i<niso_list_start;i++){
        isotri *tri;
        isovert *v1, *v2, *v3;
        
        tri=iso_list_start[i];

        v1 = tri->v1;
        v2 = tri->v2;
        v3 = tri->v3;

        glColor4fv(v1->color);
        glNormal3fv(getnormalvectorptr(sphereinfo,v1->cnorm));
        glVertex3fv(v1->xyz);
        
        glColor4fv(v2->color);
        glNormal3fv(getnormalvectorptr(sphereinfo,v2->cnorm));
        glVertex3fv(v2->xyz);
        
        glColor4fv(v3->color);
        glNormal3fv(getnormalvectorptr(sphereinfo,v3->cnorm));
        glVertex3fv(v3->xyz);
      }
    }
    glEnd();

    glPopAttrib();
    if(usetexturebar==1&&isoi->dataflag==1)glDisable(GL_TEXTURE_1D);


    if(tranflag==DRAW_TRANSPARENT)transparentoff();
    if(cullfaces==1)glEnable(GL_CULL_FACE);
    CheckMemory;
  }

  if((visAIso&2)==2){
    asurface = meshi->animatedsurfaces + meshi->iso_itime*meshi->nisolevels;

    glPushAttrib(GL_LIGHTING_BIT);
    antialias(1);
    glLineWidth(isolinewidth);
    glBegin(GL_LINES);
    for(i=0;i<niso_trans;i++){
      isotri *tri;
      float *xyz1, *xyz2, *xyz3;
      float *color1, *color2, *color3;
        
      tri=iso_trans[i];

      xyz1 = tri->v1->xyz;
      xyz2 = tri->v2->xyz;
      xyz3 = tri->v3->xyz;
      color1 = tri->v1->color;
      color2 = tri->v2->color;
      color3 = tri->v3->color;

      glColor3fv(color1);
      glVertex3fv(xyz1);
      glColor3fv(color2);
      glVertex3fv(xyz2);
        
      glVertex3fv(xyz2);
      glColor3fv(color3);
      glVertex3fv(xyz3);
        
      glVertex3fv(xyz3);
      glColor3fv(color1);
      glVertex3fv(xyz1);
    }
    for(i=0;i<niso_opaques;i++){
      isotri *tri;
      float *xyz1, *xyz2, *xyz3;
      float *color1, *color2, *color3;
        
      tri=iso_opaques[i];

      xyz1 = tri->v1->xyz;
      xyz2 = tri->v2->xyz;
      xyz3 = tri->v3->xyz;
      color1 = tri->v1->color;
      color2 = tri->v2->color;
      color3 = tri->v3->color;

      glColor3fv(color1);
      glVertex3fv(xyz1);
      glColor3fv(color2);
      glVertex3fv(xyz2);
        
      glVertex3fv(xyz2);
      glColor3fv(color3);
      glVertex3fv(xyz3);
        
      glVertex3fv(xyz3);
      glColor3fv(color1);
      glVertex3fv(xyz1);
    }
    glEnd();
    antialias(0);
    glPopAttrib();
  }

  if((visAIso&4)==4){
    asurface = meshi->animatedsurfaces + meshi->iso_itime*meshi->nisolevels;

    antialias(1);
    glPointSize(isopointsize);
    asurface--;
    glBegin(GL_POINTS);
    for(i=0;i<niso_trans;i++){
      isotri *tri;
      float *xyz1, *xyz2, *xyz3;
      float *color1, *color2, *color3;
        
      tri=iso_trans[i];

      xyz1 = tri->v1->xyz;
      xyz2 = tri->v2->xyz;
      xyz3 = tri->v3->xyz;
      color1 = tri->v1->color;
      color2 = tri->v2->color;
      color3 = tri->v3->color;
      
      glColor3fv(color1);
      glVertex3fv(xyz1);
      glColor3fv(color2);
      glVertex3fv(xyz2);
      glColor3fv(color3);
      glVertex3fv(xyz3);
    }
    for(i=0;i<niso_opaques;i++){
      isotri *tri;
      float *xyz1, *xyz2, *xyz3;
      float *color1, *color2, *color3;
        
      tri=iso_opaques[i];

      xyz1 = tri->v1->xyz;
      xyz2 = tri->v2->xyz;
      xyz3 = tri->v3->xyz;
      color1 = tri->v1->color;
      color2 = tri->v2->color;
      color3 = tri->v3->color;
      
      glColor3fv(color1);
      glVertex3fv(xyz1);
      glColor3fv(color2);
      glVertex3fv(xyz2);
      glColor3fv(color3);
      glVertex3fv(xyz3);
    }
    glEnd();
    antialias(0);
  }
}

/* ------------------ drawiso ------------------------ */

void drawiso(int tranflag){
  if(niso_opaques>0||niso_trans>0){
    drawiso_orig(tranflag);
  }
  else{
    int i;

    for(i=0;i<nisoinfo;i++){
      isodata *isoi;

      isoi = isoinfo + i;
      if(isoi->geomflag==0)continue;
    }
  }
}

/* ------------------ drawstaticiso ------------------------ */

void drawstaticiso(const isosurface *asurface,int surfacetype, 
                   int smoothnorm, int trans_flag, int data_type, 
                   float line_width){
  int j,k;
  float vv1[3],vv2[3],vv3[3];
  float vv1n[3],vv2n[3],vv3n[3];
  short *norm=NULL;
  unsigned short *v1, *v2, *v3;
  unsigned short *vertices_i=NULL;
  int *triangles_i=NULL;
  int nvertices;
  int i1, i2, i3;
  short *norm1,*norm2,*norm3,*vertexnorm=NULL;
  int ntriangles;
  float xyzmin[3], xyzmaxdiff_local;
  int drawing_transparent, drawing_blockage_transparent, drawing_vent_transparent;
  int drawing_smooth;
  int transparenton_flag=0;

  get_drawing_parms(&drawing_smooth, &drawing_transparent, &drawing_blockage_transparent, &drawing_vent_transparent);

  xyzmin[0] = asurface->xmin;
  xyzmin[1] = asurface->ymin;
  xyzmin[2] = asurface->zmin;
  xyzmaxdiff_local = asurface->xyzmaxdiff;

  nvertices=asurface->nvertices;
  ntriangles=asurface->ntriangles/3;
  if(ntriangles==0)return;
  if(surfacetype==1||surfacetype==-1){
    float rgbtemp[4];
    float *col;

    col = asurface->color;
    if(setbw==1){
      rgbtemp[0]=0.299*col[0]+0.587*col[1]+0.114*col[2];
      rgbtemp[1]=rgbtemp[0];
      rgbtemp[2]=rgbtemp[0];
    }
    else{
      rgbtemp[0]=col[0];
      rgbtemp[1]=col[1];
      rgbtemp[2]=col[2];
    }

    if(smooth_block_solid==0){
      rgbtemp[3]=asurface->color[3];
    }
    else{
      rgbtemp[3]=1.0;
    }
    if(data_type!=0){
      if(rgbtemp[3]<1.0&&trans_flag!=DRAW_TRANSPARENT)return;
      if(rgbtemp[3]>=1.0&&trans_flag==DRAW_TRANSPARENT)return;
    }
    if(
      trans_flag==DRAW_TRANSPARENT&&
      (
      (data_type==0&&use_transparency_data==1)||
      (data_type==1&&drawing_blockage_transparent==1)
      )
      ){
        if(rgbtemp[3]<0.99){
          drawing_transparent=1;
          drawing_blockage_transparent=1;
          transparenton_flag=1;
          transparenton();
        }
    }
    iso_specular[3] = 1.0;
    if(asurface->cullfaces==1)glDisable(GL_CULL_FACE);
    glPushAttrib(GL_LIGHTING_BIT);
    if(surfacetype==1){
      glEnable(GL_LIGHTING);
      glEnable(GL_COLOR_MATERIAL);
    }
    glBegin(GL_TRIANGLES);
    if(surfacetype==1){
      glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,asurface->color);
      glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,iso_specular);
    }

    if(transparenton_flag==1){
	  glColor4fv(rgbtemp);
    }
    else{
	  glColor3fv(rgbtemp);
    }
    vertices_i=asurface->vertices;
    triangles_i=asurface->triangles;
    norm=asurface->norm;
    vertexnorm=asurface->vertexnorm;
    for(j=0;j<ntriangles;j++){
      i1=3*triangles_i[3*j];
      i2=3*triangles_i[3*j+1];
      i3=3*triangles_i[3*j+2];
      v1=vertices_i+i1;
      v2=vertices_i+i2;
      v3=vertices_i+i3;
      for(k=0;k<3;k++){
        vv1[k]=xyzmin[k]+xyzmaxdiff_local*v1[k]/65535.;
        vv2[k]=xyzmin[k]+xyzmaxdiff_local*v2[k]/65535.;
        vv3[k]=xyzmin[k]+xyzmaxdiff_local*v3[k]/65535.;
      }
      if(smoothnorm==1){
        norm1 = vertexnorm+i1;
        norm2 = vertexnorm+i2;
        norm3 = vertexnorm+i3;
        glNormal3sv(norm1);
        glVertex3fv(vv1);
        glNormal3sv(norm2);
        glVertex3fv(vv2);
        glNormal3sv(norm3);
        glVertex3fv(vv3);
      }
      else{
        glNormal3sv(norm);
        glVertex3fv(vv1);
        glVertex3fv(vv2);
        glVertex3fv(vv3);
        norm += 3;
      }
    }
    glEnd();
    if(asurface->cullfaces==1)glEnable(GL_CULL_FACE);
    if(surfacetype==1){
      glDisable(GL_COLOR_MATERIAL);
      glDisable(GL_LIGHTING);
    }

    glPopAttrib();
    if(transparenton_flag==1)transparentoff();  
  }

  if(surfacetype==2){
    glPushMatrix();
    antialias(1);
    glLineWidth(line_width);
    glBegin(GL_LINES);
    glColor3fv(asurface->color);
    vertices_i=asurface->vertices;
    triangles_i=asurface->triangles;
    for(j=0;j<ntriangles;j++){
      i1=3*triangles_i[3*j];
      i2=3*triangles_i[3*j+1];
      i3=3*triangles_i[3*j+2];
      v1=vertices_i+i1;
      v2=vertices_i+i2;
      v3=vertices_i+i3;
      for(k=0;k<3;k++){
        vv1[k]=xyzmin[k]+xyzmaxdiff_local*v1[k]/65535.;
        vv2[k]=xyzmin[k]+xyzmaxdiff_local*v2[k]/65535.;
        vv3[k]=xyzmin[k]+xyzmaxdiff_local*v3[k]/65535.;
      }
      glVertex3fv(vv1);
      glVertex3fv(vv2);
      glVertex3fv(vv2);
      glVertex3fv(vv3);
      glVertex3fv(vv3);
      glVertex3fv(vv1);
    }
    glEnd();
    antialias(0);
    glPopMatrix();
  }

  if(surfacetype==3){
    glPushMatrix();
    antialias(1);
    glPointSize(plot3dpointsize);
    glBegin(GL_POINTS);
    glColor3fv(asurface->color);
    nvertices=asurface->nvertices;
    ntriangles=asurface->ntriangles/3;
    vertices_i=asurface->vertices;
    triangles_i=asurface->triangles;
    for(j=0;j<nvertices;j++){
      v1=vertices_i+3*j;
      for(k=0;k<3;k++){
        vv1[k]=xyzmin[k]+xyzmaxdiff_local*v1[k]/65535.;
      }

      glVertex3fv(vv1);
    }
    glEnd();
    antialias(0);
    glPopMatrix();
  }

  if(showtrinormal==1){

    glPushMatrix();
    antialias(1);
    glLineWidth(line_width);
    glBegin(GL_LINES);
    glColor3f((float)1.,(float)1.,(float)1.);
    for(j=0;j<ntriangles;j++){
      i1=3*triangles_i[3*j];
      i2=3*triangles_i[3*j+1];
      i3=3*triangles_i[3*j+2];
      v1=vertices_i+i1;
      v2=vertices_i+i2;
      v3=vertices_i+i3;
      for(k=0;k<3;k++){
        vv1[k]=xyzmin[k]+xyzmaxdiff_local*v1[k]/65535.;
        vv2[k]=xyzmin[k]+xyzmaxdiff_local*v2[k]/65535.;
        vv3[k]=xyzmin[k]+xyzmaxdiff_local*v3[k]/65535.;
      }

      if(smoothtrinormal==1){
        norm1 = vertexnorm+i1;
        norm2 = vertexnorm+i2;
		    norm3 = vertexnorm+i3;
        for(k=0;k<3;k++){
          vv1n[k]=vv1[k]+norm1[k]/(8.*32768.)/4.0;
          vv2n[k]=vv2[k]+norm2[k]/(8.*32768.)/4.0;
          vv3n[k]=vv3[k]+norm3[k]/(8.*32768.)/4.0;
        }

        glVertex3fv(vv1);
        glVertex3fv(vv1n);
        glVertex3fv(vv2);
        glVertex3fv(vv2n);
        glVertex3fv(vv3);
        glVertex3fv(vv3n);
      }
		  else{
        for(k=0;k<3;k++){
          vv1n[k]=vv1[k]+norm[k]/(8.*32768.)/4.0;
          vv2n[k]=vv2[k]+norm[k]/(8.*32768.)/4.0;
          vv3n[k]=vv3[k]+norm[k]/(8.*32768.)/4.0;
        }

        glVertex3fv(vv1);
        glVertex3fv(vv1n);
        glVertex3fv(vv2);
        glVertex3fv(vv2n);
        glVertex3fv(vv3);
        glVertex3fv(vv3n);
        norm += 3;
      }
    }
    glEnd();
    antialias(0);
    glPopMatrix();
  }
}

/* ------------------ updateslicetypes ------------------------ */

void updateisotypes(void){
  int i;
  isodata *isoi;

  nisotypes = 0;
  for(i=0;i<nisoinfo;i++){
    isoi = isoinfo+i;
    if(getisoindex(isoi)==-1)isotypes[nisotypes++]=i;
  }
  for(i=0;i<nisoinfo;i++){
    isoi = isoinfo+i;
    isoi->type=getisotype(isoi);
  }
}

/* ------------------ getisoindex ------------------------ */

int getisoindex(const isodata *isoi){
  isodata *isoi2;
  int j;

  for(j=0;j<nisotypes;j++){
    isoi2 = isoinfo+isotypes[j];
    if(strcmp(isoi->surface_label.longlabel,isoi2->surface_label.longlabel)==0)return isotypes[j];
  }
  return -1;
}

/* ------------------ getisotype ------------------------ */

int getisotype(const isodata *isoi){
  isodata *isoi2;
  int j;

  for(j=0;j<nisotypes;j++){
    isoi2 = isoinfo+isotypes[j];

    if(strcmp(isoi->surface_label.longlabel,isoi2->surface_label.longlabel)==0)return j;
  }
  return -1;
}

/* ------------------ getisottype ------------------------ */

int getisottype(const isodata *isoi){
  isodata *isoi2;
  int j;
  int jj;

  if(isoi->dataflag==0)return -1;
  jj = 0;
  for(j=0;j<nisoinfo;j++){
    isoi2 = isoinfo+j;

    if(isoi2->dataflag==0)continue;
    if(isoi2->firstshort==0)continue;
    if(strcmp(isoi->color_label.longlabel,isoi2->color_label.longlabel)==0)return jj;
    jj++;
  }
  return -1;
}

/* ------------------ update_isotype ------------------------ */

void update_isotype(void){
  int i;
  isodata *isoi;


  for(i=0;i<nisoinfo;i++){
    isoi = isoinfo + i;
    if(isoi->loaded==0)continue;
    if(isoi->display==1&&isoi->type==iisotype)return;
  }

  for(i=0;i<nisoinfo;i++){
    isoi = isoinfo + i;
    if(isoi->loaded==0)continue;
    if(isoi->display==1){
      iisotype = getisoindex(isoi);
      return;
    }
  }

  iisotype = -1;
  return;
    
}

/* ------------------ isocompare ------------------------ */

int isocompare( const void *arg1, const void *arg2 ){
  isodata *isoi, *isoj;

  isoi = isoinfo + *(int *)arg1;
  isoj = isoinfo + *(int *)arg2;

  if(strcmp(isoi->surface_label.longlabel,isoj->surface_label.longlabel)<0)return -1;
  if(strcmp(isoi->surface_label.longlabel,isoj->surface_label.longlabel)>0)return 1;
  if(isoi->blocknumber<isoj->blocknumber)return -1;
  if(isoi->blocknumber>isoj->blocknumber)return 1;
  return 0;
}

/* ------------------ updateisomenulabels ------------------------ */

void updateisomenulabels(void){
  int i;
  isodata *isoi;
  char label[128];

  if(nisoinfo>0){
    FREEMEMORY(isoorderindex);
    NewMemory((void **)&isoorderindex,sizeof(int)*nisoinfo);
    for(i=0;i<nisoinfo;i++){
      isoorderindex[i]=i;
    }
    qsort( (int *)isoorderindex, (size_t)nisoinfo, sizeof(int), isocompare );

    for(i=0;i<nisoinfo;i++){
      isoi = isoinfo + i;
      STRCPY(isoi->menulabel,isoi->surface_label.longlabel);
      if(nmeshes>1){
	      mesh *isomesh;

		    isomesh = meshinfo + isoi->blocknumber;
        sprintf(label,"%s",isomesh->label);
        STRCAT(isoi->menulabel,", ");
        STRCAT(isoi->menulabel,label);
      }
      if(showfiles==1){
        STRCAT(isoi->menulabel,", ");
        STRCAT(isoi->menulabel,isoi->file);
      }
    } 
  }
}

/* ------------------ update_iso_showlevels ------------------------ */

void update_iso_showlevels(void){
  int nisolevels;
  int *showlevels;
  int i, j;
  mesh *meshi;

  if(loaded_isomesh==NULL)return;

  nisolevels=loaded_isomesh->nisolevels;
  showlevels=loaded_isomesh->showlevels;

  for(j=0;j<nmeshes;j++){
    meshi = meshinfo+j;
    if(meshi->isofilenum==-1)continue;
    for(i=0;i<nisolevels;i++){
      if(i<meshi->nisolevels)meshi->showlevels[i]=showlevels[i];
    }
  }
}

/* ------------------ setisolabels ------------------------ */

void setisolabels(float smin, float smax, 
                    isodata *sd, int *errorcode){
  char *scale;
  int isotype;
  databounds *sb;

  isotype=getisottype(sd);
  sb = isobounds + isotype;
  sb->label=&(sd->color_label);


  *errorcode=0;
  printf("setting up iso labels \n");
  scale=sb->scale;
  getIsoLabels(smin,smax,nrgb,
                sb->colorlabels,&scale,sb->levels256);
}

/* ------------------ sync_isobounds ------------------------ */

void sync_isobounds(int isottype){
  int i,j,ii,kk,ncount;
  isosurface *asurface;
  int firsttime=1;
  float tmin_local, tmax_local;

  // find number of iso-surfaces with values 

  ncount=0;
  for(i=0;i<nisoinfo;i++){
    isodata *isoi;

    isoi = isoinfo + i;
    if(isoi->loaded==0||isoi->type!=iisotype||isoi->dataflag==0)continue;
    if(iisottype!=getisottype(isoi))continue;
    ncount++;
  }
  if(ncount<=1)return;

  // find min and max bounds for valued iso-surfaces

  for(i=0;i<nisoinfo;i++){
    isodata *isoi;

    isoi = isoinfo + i;
    if(isoi->loaded==0||isoi->type!=iisotype||isoi->dataflag==0)continue;
    if(iisottype!=getisottype(isoi))continue;
    if(firsttime==1){
      firsttime=0;
      tmin_local=isoi->tmin;
      tmax_local=isoi->tmax;
    }
    else{
      if(tmin_local<isoi->tmin)isoi->tmin=tmin_local;
      if(tmax_local>isoi->tmax)isoi->tmax=tmax_local;
    }
  }

  // set min and max bounds for valued iso-surfaces

  for(i=0;i<nisoinfo;i++){
    isodata *isoi;

    isoi = isoinfo + i;
    if(isoi->loaded==0||isoi->type!=iisotype||isoi->dataflag==0)continue;
    if(iisottype!=getisottype(isoi))continue;
    isoi->tmin=tmin_local;
    isoi->tmax=tmax_local;
  }

  // rescale all data

  for(i=0;i<nisoinfo;i++){
    isodata *isoi;
    mesh *meshi;

    isoi = isoinfo + i;
    if(isoi->loaded==0||isoi->type!=iisotype||isoi->dataflag==0)continue;
    if(iisottype!=getisottype(isoi))continue;
    
    meshi = meshinfo + isoi->blocknumber;
    asurface=meshi->animatedsurfaces;

    for(ii=0;ii<meshi->niso_times;ii++){
      for(j=0;j<meshi->nisolevels;j++){
        float tcolor, tcolor0, tcolorfactor;

        if(isoi->tmax>isoi->tmin){
          tcolor0 = (asurface->tmin-isoi->tmin)/(isoi->tmax-isoi->tmin);
          tcolorfactor = (asurface->tmax-asurface->tmin)/65535.;
          tcolorfactor /= (isoi->tmax-isoi->tmin);
        }
        else{
          tcolor0=0.5;
          tcolorfactor=0.0;
        }
        for(kk=0;kk<asurface->nvertices;kk++){
          tcolor = tcolor0 + asurface->tvertices[kk]*tcolorfactor;
          if(tcolor<0.0)tcolor=0.0;
          if(tcolor>1.0)tcolor=1.0;
          asurface->color8[kk] = (unsigned char)(tcolor*255);
        }
        asurface++;
      }
    }
  }
}

/* ------------------ compareisonodes ------------------------ */

int compare_iso_triangles( const void *arg1, const void *arg2 ){
  isotri *trii, *trij;
  float disti, distj;


  trii = *(isotri **)arg1;
  trij = *(isotri **)arg2;

  disti = trii->v1->distance+trii->v2->distance+trii->v3->distance;
  distj = trij->v1->distance+trij->v2->distance+trij->v3->distance;

  if(disti<distj)return  1;
  if(disti>distj)return -1;
  return 0;
}

/* ------------------ sort_triangles ------------------------ */

void Sort_Iso_Triangles(float *mm){
  int itri;
  int newflag;
  int dosort=0;

  if(niso_trans==0)return;
  newflag=1-iso_trans[0]->v1->flag;
  for(itri=0;itri<niso_trans;itri++){
    isotri *tri;
    float xyzeye[3];
    float *xyz;
    isovert *v1, *v2, *v3;
    float dist1, dist2;
    isotri *trim1;

    tri = iso_trans[itri];
    v1 = tri->v1;
    v2 = tri->v2;
    v3 = tri->v3;
    if(v1->flag!=newflag){
      v1->flag=newflag;
      xyz = v1->xyz;
      xyzeye[0] = mm[0]*xyz[0] + mm[4]*xyz[1] +  mm[8]*xyz[2] + mm[12];
      xyzeye[1] = mm[1]*xyz[0] + mm[5]*xyz[1] +  mm[9]*xyz[2] + mm[13];
      xyzeye[2] = mm[2]*xyz[0] + mm[6]*xyz[1] + mm[10]*xyz[2] + mm[14];
      xyzeye[0]/=mscale[0];
      xyzeye[1]/=mscale[1];
      xyzeye[2]/=mscale[2];
      v1->distance=xyzeye[0]*xyzeye[0]+xyzeye[1]*xyzeye[1]+xyzeye[2]*xyzeye[2];
    }
    if(v2->flag!=newflag){
      v2->flag=newflag;
      xyz = v2->xyz;
      xyzeye[0] = mm[0]*xyz[0] + mm[4]*xyz[1] +  mm[8]*xyz[2] + mm[12];
      xyzeye[1] = mm[1]*xyz[0] + mm[5]*xyz[1] +  mm[9]*xyz[2] + mm[13];
      xyzeye[2] = mm[2]*xyz[0] + mm[6]*xyz[1] + mm[10]*xyz[2] + mm[14];
      xyzeye[0]/=mscale[0];
      xyzeye[1]/=mscale[1];
      xyzeye[2]/=mscale[2];
      v2->distance=xyzeye[0]*xyzeye[0]+xyzeye[1]*xyzeye[1]+xyzeye[2]*xyzeye[2];
    }
    if(v3->flag!=newflag){
      v3->flag=newflag;
      xyz = v3->xyz;
      xyzeye[0] = mm[0]*xyz[0] + mm[4]*xyz[1] +  mm[8]*xyz[2] + mm[12];
      xyzeye[1] = mm[1]*xyz[0] + mm[5]*xyz[1] +  mm[9]*xyz[2] + mm[13];
      xyzeye[2] = mm[2]*xyz[0] + mm[6]*xyz[1] + mm[10]*xyz[2] + mm[14];
      xyzeye[0]/=mscale[0];
      xyzeye[1]/=mscale[1];
      xyzeye[2]/=mscale[2];
      v3->distance=xyzeye[0]*xyzeye[0]+xyzeye[1]*xyzeye[1]+xyzeye[2]*xyzeye[2];
    }
    dist1=(v1->distance+v2->distance+v3->distance);
    trim1 = iso_trans[itri-1];
    dist2=trim1->v1->distance+trim1->v2->distance+trim1->v3->distance;

    if(itri>0&&dosort==0&&dist1>dist2)dosort=1;
  }
  if(dosort==1)qsort((isotri **)iso_trans,(size_t)niso_trans,sizeof(isotri **),compare_iso_triangles);
}

/* ------------------ update_isotri_list ------------------------ */

void Update_Isotris(int flag){
  int itri;
  isosurface *asurfi;
  isotri **iso_trans_tmp,**iso_opaques_tmp;
  int *showlevels;
  mesh *meshi;
  float *colorptr;
  isosurface *asurface;
  int ntris;
  isodata *loaded_iso;

  if(loaded_isomesh==NULL||loaded_isomesh->isofilenum==-1)return;
  loaded_iso=isoinfo + loaded_isomesh->isofilenum;
 
  if(iso_trans_list==NULL||iso_opaques_list==NULL){
    int iitime;

    niso_timesteps=loaded_isomesh->niso_times;
    if(iso_trans_list==NULL){
      int i;

      NewMemory((void **)&niso_trans_list,niso_timesteps*sizeof(int));
      NewMemory((void **)&iso_trans_list,niso_timesteps*sizeof(isotri **));
      for(i=0;i<niso_timesteps;i++){
        iso_trans_list[i]=NULL;
      }
    }
    if(iso_opaques_list==NULL){
      int i;

      NewMemory((void **)&niso_opaques_list,niso_timesteps*sizeof(int));
      NewMemory((void **)&iso_opaques_list,niso_timesteps*sizeof(isotri **));
      for(i=0;i<niso_timesteps;i++){
        iso_opaques_list[i]=NULL;
      }
    }
    for(iitime=0;iitime<niso_timesteps;iitime++){
      int i;

      ntris=0;
      for(i=0;i<nisoinfo;i++){
        isodata *isoi;
        int ilev;
    
        isoi = isoinfo+i;
        if(isoi->geomflag==1||isoi->loaded==0||isoi->display==0)continue;

        meshi = meshinfo + isoi->blocknumber;
        asurface = meshi->animatedsurfaces + iitime*meshi->nisolevels;
        for(ilev=0;ilev<meshi->nisolevels;ilev++){
          asurfi = asurface + ilev;
          ntris+=asurfi->niso_triangles;
        }
      }
      if(ntris>0){
        NewMemory((void **)&iso_trans,ntris*sizeof(isotri *));
        iso_trans_list[iitime]=iso_trans;
        NewMemory((void **)&iso_opaques,ntris*sizeof(isotri *));
        iso_opaques_list[iitime]=iso_opaques;
      }
    }
    flag=1;
  }
  if(flag==1){
    int iitime;

    for(iitime=0;iitime<niso_timesteps;iitime++){
      niso_trans_list[iitime]=-1;
      niso_opaques_list[iitime]=-1;
    }
  }

  iso_trans=iso_trans_list[loaded_isomesh->iso_itime];
  iso_opaques=iso_opaques_list[loaded_isomesh->iso_itime];
  niso_trans=niso_trans_list[loaded_isomesh->iso_itime];
  niso_opaques=niso_opaques_list[loaded_isomesh->iso_itime];

  if(niso_trans==-1||niso_opaques==-1){
    int i;

    flag=1;
    iso_trans_tmp=iso_trans;
    iso_opaques_tmp=iso_opaques;
    niso_trans=0;
    niso_opaques=0;
    for(i=0;i<nisoinfo;i++){
      isodata *isoi;
    
      isoi = isoinfo+i;
      if(isoi->geomflag==1||isoi->loaded==0||isoi->display==0)continue;
  
      CheckMemory;
      meshi = meshinfo + isoi->blocknumber;
      asurface = meshi->animatedsurfaces + meshi->iso_itime*meshi->nisolevels;
      showlevels=meshi->showlevels;
  
      if(transparent_state==ALL_TRANSPARENT){
        int ilev;

        for(ilev=0;ilev<meshi->nisolevels;ilev++){
          if(showlevels[ilev]==0)continue;
          asurfi = asurface + ilev;
          if(asurfi->niso_triangles>0){
            niso_trans += asurfi->niso_triangles;
            for(itri=0;itri<asurfi->niso_triangles;itri++){
              *iso_trans_tmp++=asurfi->iso_triangles+itri;
            }
            colorptr=isoi->colorlevels[ilev];
            colorptr[3]=transparent_level;
          }
        }
      }
      else if(transparent_state==MIN_SOLID){
        int ilev;

        for(ilev=0;ilev<1;ilev++){
          if(showlevels[ilev]==0)continue;
          asurfi = asurface + ilev;
          if(asurfi->niso_triangles>0){
            niso_opaques += asurfi->niso_triangles;
            for(itri=0;itri<asurfi->niso_triangles;itri++){
              *iso_opaques_tmp++=asurfi->iso_triangles+itri;
            }
            colorptr=isoi->colorlevels[ilev];
            colorptr[3]=1.0;
          }
        }
        for(ilev=1;ilev<meshi->nisolevels;ilev++){
          if(showlevels[ilev]==0)continue;
          asurfi = asurface + ilev;
          if(asurfi->niso_triangles>0){
            niso_trans += asurfi->niso_triangles;
            for(itri=0;itri<asurfi->niso_triangles;itri++){
              *iso_trans_tmp++=asurfi->iso_triangles+itri;
            }
            colorptr=isoi->colorlevels[ilev];
            colorptr[3]=transparent_level;
          }
        }
      }
      else if(transparent_state==MAX_SOLID){
        int ilev;

        for(ilev=0;ilev<meshi->nisolevels-1;ilev++){
          if(showlevels[ilev]==0)continue;
          asurfi = asurface + ilev;
          if(asurfi->niso_triangles>0){
            niso_trans += asurfi->niso_triangles;
            for(itri=0;itri<asurfi->niso_triangles;itri++){
              *iso_trans_tmp++=asurfi->iso_triangles+itri;
            } 
            colorptr=isoi->colorlevels[ilev];
            colorptr[3]=transparent_level;
          }
        }
        for(ilev=meshi->nisolevels-1;ilev<meshi->nisolevels;ilev++){
          if(showlevels[ilev]==0)continue;
          asurfi = asurface + ilev;
          if(asurfi->niso_triangles>0){
            niso_opaques += asurfi->niso_triangles;
            for(itri=0;itri<asurfi->niso_triangles;itri++){
              *iso_opaques_tmp++=asurfi->iso_triangles+itri;
            }
            colorptr=isoi->colorlevels[ilev];
            colorptr[3]=1.0;
          }
        }
      }
      else if(transparent_state==ALL_SOLID){
        int ilev;

        for(ilev=0;ilev<meshi->nisolevels;ilev++){
          CheckMemory;
          if(showlevels[ilev]==0)continue;
          asurfi = asurface + ilev;
          if(asurfi->niso_triangles>0){
            niso_opaques += asurfi->niso_triangles;
            for(itri=0;itri<asurfi->niso_triangles;itri++){
              *iso_opaques_tmp++=asurfi->iso_triangles+itri;
            }
            colorptr=isoi->colorlevels[ilev];
            colorptr[3]=1.0;
          }
        }
      }
    }
  }

  if(sort_iso_triangles==1&&niso_trans>0){
    Sort_Iso_Triangles(modelview_scratch);
  }
  niso_trans_list[loaded_isomesh->iso_itime]=niso_trans;
  niso_opaques_list[loaded_isomesh->iso_itime]=niso_opaques;

  CheckMemory;
}

/* ------------------ get_loaded_isomesh ------------------------ */

mesh *get_loaded_isomesh(void){
  mesh *return_mesh;
  int i,nsteps=-1;

  if(isoinfo==NULL)return NULL;
  return_mesh=NULL;
  for(i=0;i<nisoinfo;i++){
    mesh *mesh2;
    isodata *isoi;

    isoi = isoinfo + i;
    if(isoi->loaded==0)continue;
    mesh2 = meshinfo + isoi->blocknumber;
    if(nsteps==-1||mesh2->niso_times<nsteps){
      return_mesh = mesh2;
      nsteps=mesh2->niso_times;
    }
  }
  return return_mesh;
}

/* ------------------ update_isocolors ------------------------ */

void update_isocolors(void){
  int i;
  float *iso_colors;

  if(iso_ambient_ini==NULL){
    iso_colors=iso_ambient;
  }
  else{
    iso_colors=iso_ambient_ini;
  }

  for(i=nsurfinfo+1;i<nsurfinfo+n_iso_ambient+1;i++){
    surfdata *surfi;
    float *color;

    surfi = surfinfo + i;
    color=iso_colors+4*(i-(nsurfinfo+1));
    surfi->color=getcolorptr(color);
    surfi->transparent_level=0.8;
    surfi->iso_level=i-nsurfinfo;
  }
}

