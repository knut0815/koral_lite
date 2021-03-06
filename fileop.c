/*! \file fileop.c
 \brief File operations
 */


#include "ko.h"


/*************************************************/
/*  adds up current quantities to the pavg array */
/*************************************************/

int
save_avg(ldouble dtin)
{
  int ix,iy,iz,iv,ii;

#pragma omp parallel for private(ix,iy,iz,iv) 
  for(ix=0;ix<NX;ix++) 
    {
      for(iy=0;iy<NY;iy++)
	{
	  ldouble avgz[NV+NAVGVARS];
	   for(iv=0;iv<NV+NAVGVARS;iv++)
	     avgz[iv]=0.;
	   for(iz=0;iz<NZ;iz++)
	    {
	      ldouble avg[NV+NAVGVARS];
	      p2avg(ix,iy,iz,avg);

	      //timestep
	      ldouble dt=dtin;

#ifdef RADIATION //if implicit failed, do not take this step into account at all for failed cells
	      if(get_cflag(RADIMPFIXUPFLAG,ix,iy,iz)==0)
#endif
		{

#if (AVGOUTPUT==2) //phi-averaged
		  for(iv=0;iv<NV+NAVGVARS;iv++)
		    avgz[iv]+=avg[iv];
#else //regular, without phi-averaging
		  set_u_scalar(avgselftime,ix,iy,iz,get_u_scalar(avgselftime,ix,iy,iz)+dt);
		  for(iv=0;iv<NV+NAVGVARS;iv++)
		    {
		      set_uavg(pavg,iv,ix,iy,iz, get_uavg(pavg,iv,ix,iy,iz)+avg[iv]*dt);
		    }
#endif
		}
	    }

#if (AVGOUTPUT==2) //phi-averaged
	  for(iv=0;iv<NV+NAVGVARS;iv++)
	    avgz[iv]/=NZ;
	  set_u_scalar(avgselftime,ix,iy,0,get_u_scalar(avgselftime,ix,iy,0)+dt);
	  for(iv=0;iv<NV+NAVGVARS;iv++)
	    {
	      set_uavg(pavg,iv,ix,iy,0, get_uavg(pavg,iv,ix,iy,0)+avgz[iv]*dt);
	    }
#endif
	}
    }

  avgtime+=dtin;

  
  return 0;
}


/*********************************************/
/* opens files etc. */
/*********************************************/

int
fprint_openfiles(char* folder)
{
  char bufor[100];

#ifdef NORESTART
  if(PROCID==0)
    {
      sprintf(bufor,"rm %s/*",folder);
      int i=system(bufor);
    }
  nfout1=0;
  nfout2=0;
#endif

#ifndef MPI
  sprintf(bufor,"%s/scalars.dat",folder);
  fout_scalars=fopen(bufor,"a");

  sprintf(bufor,"%s/failures.dat",folder);
  fout_fail=fopen(bufor,"a");
#endif

  return 0;
}


/*********************************************/
/* closes file handles */
/*********************************************/

int
fprint_closefiles()
{

#ifndef MPI
  fclose(fout_scalars);
  fclose(fout_fail);  
#endif

  return 0;
}


/*********************************************/
// coordinate grid file *********************//
/*********************************************/

int
fprint_gridfile(char* folder)
{
  FILE* out;
  char bufor[50];
  sprintf(bufor,"%s/grid.dat",folder);
  out=fopen(bufor,"w");

  int ix,iy,iz,iv;
  ldouble pp[NV],uu[NV];
  for(iz=0;iz<NZ;iz++)
    {
      for(iy=0;iy<NY;iy++)
	{
	  for(ix=0;ix<NX;ix++)
	    {
	      struct geometry geom;
	      fill_geometry(ix,iy,iz,&geom);

	      ldouble xxcar[4],xxsph[4];

	      coco_N(geom.xxvec,xxcar,MYCOORDS,MINKCOORDS); 
	      coco_N(geom.xxvec,xxsph,MYCOORDS,KERRCOORDS); 

	      fprintf(out,"%d %d %d %f %f %f %f %f %f %f %f %f\n",
		      ix,iy,iz,
		      geom.xxvec[1],geom.xxvec[2],geom.xxvec[3],
		      xxcar[1],xxcar[2],xxcar[3],
		      xxsph[1],xxsph[2],xxsph[3]);

	    }
	}
    }

  fclose(out);

  return 0;
}


/*********************************************/
/* prints scalar quantities to scalars.dat */
/*********************************************/

int
fprint_scalars(ldouble t, ldouble *scalars, int nscalars)
{

#ifndef MPI
  calc_scalars(scalars,t);
  int iv;

#ifdef TIMEINSCALARSINSEC
      t=timeGU2CGS(t);
#endif

  fprintf(fout_scalars,"%e ",t);
  for(iv=0;iv<nscalars;iv++)
    fprintf(fout_scalars,"%.20e ",scalars[iv]);
  fprintf(fout_scalars,"\n");
  fflush(fout_scalars);
#endif //ifndef MPI

  return 0;
}


/*********************************************/
/* prints radial profiles to radNNNN.dat */
/*********************************************/
int
fprint_radprofiles(ldouble t, int nfile, char* folder, char* prefix)
{

      char bufor[50],bufor2[50];
      sprintf(bufor,"%s/%s%04d.dat",folder,prefix,nfile);

      fout_radprofiles=fopen(bufor,"w");

      ldouble mdotscale = (rhoGU2CGS(1.)*velGU2CGS(1.)*lenGU2CGS(1.)*lenGU2CGS(1.))/calc_mdotEdd();
      ldouble lumscale = (fluxGU2CGS(1.)*lenGU2CGS(1.)*lenGU2CGS(1.))/calc_lumEdd();

      fprintf(fout_radprofiles,"# mdotGU2Edd: %e lumGU2Edd: %e\n",mdotscale,lumscale);

      int ix,iv;
      //calculating radial profiles
      ldouble profiles[NRADPROFILES][NX];
      calc_radialprofiles(profiles);
      //printing radial profiles  
      for(ix=0;ix<NX;ix++)
	{
	  ldouble xx[4],xxout[4];
	  get_xx(ix,0,0,xx);
	  coco_N(xx,xxout,MYCOORDS,OUTCOORDS); 
	  if(xxout[1]<rhorizonBL) continue;
	  fprintf(fout_radprofiles,"%e ",xxout[1]);
	  for(iv=0;iv<NRADPROFILES;iv++)
	    fprintf(fout_radprofiles,"%e ",profiles[iv][ix]);
	  fprintf(fout_radprofiles,"\n");
	}
      fclose(fout_radprofiles);
  
  return 0;
}
 
 
/*********************************************/
/* prints theta profiles to thNNNN.dat */
/*********************************************/

int
fprint_thprofiles(ldouble t, int nfile, char* folder, char* prefix)
{

  char bufor[50],bufor2[50];
  sprintf(bufor,"%s/%s%04d.dat",folder,prefix,nfile);

  FILE *fout_thprofiles=fopen(bufor,"w");

  int ix,iy,iv;

  //search for appropriate radial index
  ldouble xx[4],xxBL[4];
  ldouble radius=1.e3;
  #ifdef THPROFRADIUS
  radius=THPROFRADIUS;
  #endif
  for(ix=0;ix<NX;ix++)
    {
      get_xx(ix,0,0,xx);
      coco_N(xx,xxBL,MYCOORDS,BLCOORDS);
      if(xxBL[1]>radius) break;
    }

  //calculating theta profiles
  ldouble profiles[NTHPROFILES][NY];
  calc_thetaprofiles(profiles);
  //printing th profiles  
  for(iy=0;iy<NY;iy++)
    {
      get_xx(ix,iy,0,xx);
      coco_N(xx,xxBL,MYCOORDS,BLCOORDS); 
      
      fprintf(fout_thprofiles,"%e ",xxBL[2]);
      for(iv=0;iv<NTHPROFILES;iv++)
	fprintf(fout_thprofiles,"%e ",profiles[iv][iy]);
      fprintf(fout_thprofiles,"\n");
    }
  fclose(fout_thprofiles);
  
  return 0;
}

/*********************************************/
/* prints restart files */
/*********************************************/

int
fprint_restartfile(ldouble t, char* folder)
{
  #ifdef MPI
  fprint_restartfile_mpi(t,folder);

  #else 

  fprint_restartfile_bin(t,folder); 

  #endif
  
  return 0;
}


/*********************************************/
//parallel output to a single restart file
/*********************************************/

int 
fprint_restartfile_mpi(ldouble t, char* folder)
{
  #ifdef MPI
  char bufor[250];

  //header
  if(PROCID==0)
    {
       sprintf(bufor,"%s/res%04d.head",folder,nfout1);
       fout1=fopen(bufor,"w"); 
       sprintf(bufor,"## %5d %5d %10.6e %5d %5d %5d %5d\n",nfout1,nfout2,t,PROBLEM,TNX,TNY,TNZ);
       fprintf(fout1,"%s",bufor);
       fclose(fout1);
    }
  
  //maybe not needed
  MPI_Barrier(MPI_COMM_WORLD);

  //body
  sprintf(bufor,"%s/res%04d.dat",folder,nfout1);

  MPI_File cFile;
  MPI_Status status;
  MPI_Request req;

  int rc = MPI_File_open( MPI_COMM_WORLD, bufor, MPI_MODE_WRONLY | MPI_MODE_CREATE, MPI_INFO_NULL, &cFile );
  if (rc)
  {
    printf( "Unable to open/create file %s\n", bufor );fflush(stdout); exit(-1);
  }

  /***** first write all the indices ******/

  int *indices;
  if((indices = (int *)malloc(NX*NY*NZ*3*sizeof(int)))==NULL) my_err("malloc err. - fileop 0\n");
  
  int ix,iy,iz,iv;
  int gix,giy,giz;

  for(ix=0;ix<NX;ix++)
    for(iy=0;iy<NY;iy++)
      for(iz=0;iz<NZ;iz++)
	{
	  mpi_local2globalidx(ix,iy,iz,&gix,&giy,&giz);
	  indices[ix*NY*NZ*3+iy*NZ*3+iz*3+0]=gix;
	  indices[ix*NY*NZ*3+iy*NZ*3+iz*3+1]=giy;
	  indices[ix*NY*NZ*3+iy*NZ*3+iz*3+2]=giz;
	}

  //set the initial location at each process for indices
  MPI_Offset pos;
  pos=PROCID*NX*NY*NZ*(3*sizeof(int));  
  MPI_File_seek( cFile, pos, MPI_SEEK_SET ); 

  //write all indices
  MPI_File_write( cFile, indices, NX*NY*NZ*3, MPI_INT, &status );
  
  /***** then primitives in the same order ******/
  
  //now let's try manually
  pos=TNX*TNY*TNZ*(3*sizeof(int)) + PROCID*NX*NY*NZ*(NV*sizeof(ldouble)); 
  MPI_File_seek( cFile, pos, MPI_SEEK_SET ); 

  ldouble *pout;
  if((pout = (ldouble *)malloc(NX*NY*NZ*NV*sizeof(ldouble)))==NULL) my_err("malloc err. - fileop 1\n");
    
  for(ix=0;ix<NX;ix++)
    for(iy=0;iy<NY;iy++)
      for(iz=0;iz<NZ;iz++)
	for(iv=0;iv<NV;iv++)
	  {
	    pout[ix*NY*NZ*NV+iy*NZ*NV+iz*NV+iv]=get_u(p,iv,ix,iy,iz);


	  }

  MPI_File_write( cFile, pout, NX*NY*NZ*NV, MPI_LDOUBLE, &status );  
  MPI_File_close( &cFile );

  //maybe not needed
  MPI_Barrier(MPI_COMM_WORLD);

  //move reslast.head and reslast.dat symbolic links
  if(PROCID==0)
    {

      sprintf(bufor,"rm %s/reslast.dat",folder);
      iv=system(bufor);
      sprintf(bufor,"ln -s res%04d.dat %s/reslast.dat",nfout1,folder);
      iv=system(bufor);

      sprintf(bufor,"rm %s/reslast.head",folder);
      iv=system(bufor);
      sprintf(bufor,"ln -s res%04d.head %s/reslast.head",nfout1,folder);
      iv=system(bufor);
    }

  free (indices);
  free (pout);
#endif
  return 0;
}


/*********************************************/
//serial binary restart file output
/*********************************************/

int 
fprint_restartfile_bin(ldouble t, char* folder)
{
  char bufor[250];
  
  //header
  if(PROCID==0)
    {
       sprintf(bufor,"%s/res%04d.head",folder,nfout1);
       fout1=fopen(bufor,"w"); 
       sprintf(bufor,"## %5d %5d %10.6e %5d %5d %5d %5d\n",nfout1,nfout2,t,PROBLEM,TNX,TNY,TNZ);
       fprintf(fout1,"%s",bufor);
       fclose(fout1);
    }

  //body
  sprintf(bufor,"%s/res%04d.dat",folder,nfout1);
  fout1=fopen(bufor,"wb"); 

  int ix,iy,iz,iv;
  int gix,giy,giz;
  ldouble pp[NV];
  
  //indices first
  for(ix=0;ix<NX;ix++)
    for(iy=0;iy<NY;iy++)
      for(iz=0;iz<NZ;iz++)
	{
	  mpi_local2globalidx(ix,iy,iz,&gix,&giy,&giz);
	  fwrite(&gix,sizeof(int),1,fout1);
	  fwrite(&giy,sizeof(int),1,fout1);
	  fwrite(&giz,sizeof(int),1,fout1);
	}

  //then, in the same order, primitives
  for(ix=0;ix<NX;ix++)
    for(iy=0;iy<NY;iy++)
      for(iz=0;iz<NZ;iz++)
	{
	  ldouble ppout[NV];
	  PLOOP(iv)
	    ppout[iv]=get_u(p,iv,ix,iy,iz);
#ifdef RESTARTOUTPUTINBL
	  struct geometry geom,geomBL;
	  fill_geometry(ix,iy,iz,&geom);
	  fill_geometry_arb(ix,iy,iz,&geomBL,BLCOORDS);
	  trans_pall_coco(ppout, ppout, MYCOORDS,BLCOORDS, geom.xxvec,&geom,&geomBL);
#endif

	  fwrite(ppout,sizeof(ldouble),NV,fout1);
	}

  fclose(fout1);

  //move reslast.head and reslast.dat symbolic links  
  if(PROCID==0)
    {
      sprintf(bufor,"rm %s/reslast.dat",folder);
      iv=system(bufor);
      sprintf(bufor,"ln -s res%04d.dat %s/reslast.dat",nfout1,folder);
      iv=system(bufor);

      sprintf(bufor,"rm %s/reslast.head",folder);
      iv=system(bufor);
      sprintf(bufor,"ln -s res%04d.head %s/reslast.head",nfout1,folder);
      iv=system(bufor);    
    }

  return 0;
}


/*********************************************/
/* reads dump file */
/* puts conserved into the memory */
/* converts them to primitives */
/*********************************************/

int
fread_restartfile(int nout1, char* folder,ldouble *t)
{


  int ret;
   
  
  #ifdef MPI
  ret=fread_restartfile_mpi(nout1,folder,t);

  #else //no MPI 
  ret=fread_restartfile_bin(nout1,folder,t);  

  #endif
  
  return ret;
}


/*********************************************/
//Read binary restart file to single thread
/*********************************************/

int 
fread_restartfile_bin(int nout1, char *folder, ldouble *t)
{
  int ret, ix,iy,iz,iv,i,ic,gix,giy,giz;
  char fname[400],fnamehead[400];
  if(nout1>=0)
    {
      sprintf(fname,"%s/res%04d.dat",folder,nout1);
      #ifdef MPI
      sprintf(fnamehead,"%s/../0/res%04d.head",folder,nout1);
      #else
      sprintf(fnamehead,"%s/res%04d.head",folder,nout1);
      #endif
    }
  else
    {
      sprintf(fname,"%s/reslast.dat",folder);
      #ifdef MPI
      sprintf(fnamehead,"%s/../0/reslast.head",folder);
      #else
      sprintf(fnamehead,"%s/reslast.head",folder);
      #endif
    }

  FILE *fdump;

  /***********/
  //header file
  fdump=fopen(fnamehead,"r");
  if(fdump==NULL) return 1; //request start from scratch

  //reading parameters, mostly time
  int intpar[6];
  ret=fscanf(fdump,"## %d %d %lf %d %d %d %d\n",&intpar[0],&intpar[1],t,&intpar[2],&intpar[3],&intpar[4],&intpar[5]);
  if(PROCID==0)
    printf("restart file (%s) read no. %d at time: %f of PROBLEM: %d with NXYZ: %d %d %d\n",
	 fname,intpar[0],*t,intpar[2],intpar[3],intpar[4],intpar[5]); 
  nfout1=intpar[0]+1; //global file no.
  nfout2=intpar[1]; //global file no. for avg
  fclose(fdump);

  /***********/
  //body file
  fdump=fopen(fname,"rb");
 
  struct geometry geom;
  ldouble xxvec[4],xxvecout[4];
  ldouble uu[NV],pp[NV],ftemp;
  char c;

  int **indices;
  if((indices = (int **)malloc(NX*NY*NZ*sizeof(int*)))==NULL) my_err("malloc err. - fileop 2\n");
  for(i=0;i<NX*NY*NZ;i++)
    if((indices[i]=(int *)malloc(3*sizeof(int)))==NULL) my_err("malloc err. - fileop 3\n");

  //first indices
  for(ic=0;ic<NX*NY*NZ;ic++)
    {
      ret=fread(&gix,sizeof(int),1,fdump);
      ret=fread(&giy,sizeof(int),1,fdump);
      ret=fread(&giz,sizeof(int),1,fdump);

      mpi_global2localidx(gix,giy,giz,&ix,&iy,&iz);

      indices[ic][0]=ix;
      indices[ic][1]=iy;
      indices[ic][2]=iz;
    }

  //then primitives
   for(ic=0;ic<NX*NY*NZ;ic++)
    {

#ifdef RESTARTFROMNORELEL
      int nvold=NV-NRELBIN;

#ifdef RESTARTFROMNORELEL_NOCOMPT
      nvold += 1;
#endif
      
      ldouble ppold[nvold]; 
      ret=fread(ppold,sizeof(ldouble),nvold,fdump);

      int ie;
      for (ie=0; ie<NVHD-NRELBIN; ie++) pp[ie] = ppold[ie];
      for (ie=0; ie<NRELBIN; ie++) pp[NVHD-NRELBIN+ie] = 0.0; 
      for (ie=0; ie<NV-NVHD; ie++) pp[NVHD+ie] = ppold[NVHD-NRELBIN+ie];

#else
      ret=fread(pp,sizeof(ldouble),NV,fdump);
#endif

      ix=indices[ic][0];
      iy=indices[ic][1];
      iz=indices[ic][2];

      fill_geometry(ix,iy,iz,&geom);

#ifdef RESTARTOUTPUTINBL
      struct geometry geomBL;
      fill_geometry_arb(ix,iy,iz,&geomBL,BLCOORDS);
      trans_pall_coco(pp, pp, BLCOORDS,MYCOORDS, geomBL.xxvec,&geomBL,&geom);
#endif

      
      #ifdef CONSISTENTGAMMA
      ldouble Te,Ti;
      calc_PEQ_Teifrompp(pp,&Te,&Ti,ix,iy,iz);
      ldouble gamma=calc_gammaintfromTei(Te,Ti);
      set_u_scalar(gammagas,ix,iy,iz,gamma);
      #endif
      
      p2u(pp,uu,&geom);


      //saving primitives
      for(iv=0;iv<NV;iv++)    
	{
	  set_u(u,iv,ix,iy,iz,uu[iv]);
	  set_u(p,iv,ix,iy,iz,pp[iv]);
	}
    }

  for(i=0;i<NX*NY*NZ;i++)
    free(indices[i]);
  free(indices);
  
  fclose(fdump);

  return 0;
}


/*********************************************/
//read restart file to individual MPI tile
/*********************************************/

int 
fread_restartfile_mpi(int nout1, char *folder, ldouble *t)
{
  #ifdef MPI
  int ret, ix,iy,iz,iv,i,ic,gix,giy,giz,tix,tiy,tiz;
  char fname[400],fnamehead[400];

  if(nout1>=0)
    {
      sprintf(fname,"%s/res%04d.dat",folder,nout1);
      sprintf(fnamehead,"%s/res%04d.head",folder,nout1);
    }
  else
    {
      sprintf(fname,"%s/reslast.dat",folder);
      sprintf(fnamehead,"%s/reslast.head",folder);
    }

  FILE *fdump;

  /***********/
  //header file
 
  fdump=fopen(fnamehead,"r");
  if(fdump==NULL) 
    {
      return 1; //request start from scratch
    }
  
  //reading parameters, mostly time
  int intpar[6];
  ret=fscanf(fdump,"## %d %d %lf %d %d %d %d\n",&intpar[0],&intpar[1],t,&intpar[2],&intpar[3],&intpar[4],&intpar[5]);
  if(PROCID==0)
  printf("restart file (%s) read no. %d at time: %f of PROBLEM: %d with NXYZ: %d %d %d\n",
	 fname,intpar[0],*t,intpar[2],intpar[3],intpar[4],intpar[5]); 
  nfout1=intpar[0]+1; //global file no.
  nfout2=intpar[1]; //global file no. for avg
  fclose(fdump);
    
  //maybe not needed
  MPI_Barrier(MPI_COMM_WORLD);

  /***********/
  //body file
  struct geometry geom;
  ldouble uu[NV],pp[NV],ftemp;

  MPI_File cFile;
  MPI_Status status;
  MPI_Request req;

  int rc = MPI_File_open( MPI_COMM_WORLD, fname, MPI_MODE_RDONLY, MPI_INFO_NULL, &cFile );
  if (rc)
  {
    printf( "Unable to open/create file %s\n", fname );fflush(stdout); exit(-1);
  }

  /***** first read ALL the indices ******/

 int nvold;
#ifdef RESTARTFROMNORELEL
  nvold=NV-NRELBIN;
#ifdef RESTARTFROMNORELEL_NOCOMPT
  nvold += 1;
#endif
#else
  nvold=NV;
#endif

  //first read the indices pretending to be a single process
  int *indices;
  if((indices = (int *)malloc(NX*NY*NZ*3*sizeof(int)))==NULL) my_err("malloc err. - fileop 5\n");
  int len=NX*NY*NZ;

  ldouble *pout;
  if((pout=(ldouble *)malloc(NX*NY*NZ*nvold*sizeof(ldouble)))==NULL) my_err("malloc err. - fileop 7\n");

  //set the initial location
  int procid=PROCID;
  MPI_Offset pos;

#ifdef RESTARTGENERALINDICES
  for(procid=0;procid<NTX*NTY*NTZ;procid++)
#endif
    {
      pos=procid*NX*NY*NZ*(3*sizeof(int));  

      MPI_File_seek( cFile, pos, MPI_SEEK_SET ); 
  
      //read them
      MPI_File_read( cFile, indices, 3*len, MPI_INT, &status );

      //convert to local
      for(ic=0;ic<len;ic++)
	{
	  gix=indices[ic*3+0];
	  giy=indices[ic*3+1];
	  giz=indices[ic*3+2];
	  mpi_global2localidx(gix,giy,giz,&ix,&iy,&iz);
	  indices[ic*3+0]=ix;
	  indices[ic*3+1]=iy;
	  indices[ic*3+2]=iz;
	}

      /***** then read primitives in the same order ******/

      pos=TNX*TNY*TNZ*(3*sizeof(int)) + procid*NX*NY*NZ*(nvold*sizeof(ldouble)); 
      MPI_File_seek( cFile, pos, MPI_SEEK_SET ); 
      MPI_File_read( cFile, pout, len*nvold, MPI_LDOUBLE, &status );
 
      //rewriting to p
      int ppos;
      for(ic=0;ic<len;ic++)
	{
	  ix=indices[ic*3+0];
	  iy=indices[ic*3+1];
	  iz=indices[ic*3+2];

	  ppos=ic*nvold;

	  if(if_indomain(ix,iy,iz))
	    {
	      fill_geometry(ix,iy,iz,&geom);

#ifdef RESTARTFROMNORELEL
          int ie;
          for (ie=0; ie<8; ie++) set_u(p,ie,ix,iy,iz,pout[ppos+ie]);
	  for (ie=0; ie<(NV-NVHD); ie++) set_u(p,NVHD+ie,ix,iy,iz,pout[ppos+8+ie]);
          for (ie=0; ie<NRELBIN; ie++) set_u(p,8+ie,ix,iy,iz, 0.0); //set relel bins to zero 
#else
	  PLOOP(iv) set_u(p,iv,ix,iy,iz,pout[ppos+iv]);
#endif

	      
#ifdef CONSISTENTGAMMA
	  ldouble Te,Ti;
	  calc_PEQ_Teifrompp(&get_u(p,0,ix,iy,iz),&Te,&Ti,ix,iy,iz);
	  ldouble gamma=calc_gammaintfromTei(Te,Ti);
	  set_u_scalar(gammagas,ix,iy,iz,gamma);
#endif


	  p2u(&get_u(p,0,ix,iy,iz),&get_u(u,0,ix,iy,iz),&geom);
	      

	    }
	}
    }


  MPI_File_close( &cFile );
  MPI_Barrier(MPI_COMM_WORLD);
  free(indices);
  free(pout);
#endif
  return 0;
}
							    
/*********************************************/
/* prints avg files */
/*********************************************/

int
fprint_avgfile(ldouble t, char* folder,char* prefix)
{
  #ifdef MPI

  fprint_avgfile_mpi(t,folder,prefix);

  #else

  fprint_avgfile_bin(t,folder,prefix); 

  #endif
  
  return 0;
}


/*********************************************/
//parallel output to a single avgfile
/*********************************************/

int
fprint_avgfile_mpi(ldouble t, char* folder, char* prefix)
{
  #ifdef MPI
  char bufor[250];
  
  //header
  if(PROCID==0)
    {
      sprintf(bufor,"%s/%s%04d.head",folder,prefix,nfout2);
      fout1=fopen(bufor,"w"); 
      sprintf(bufor,"## %5d %10.6e %10.6e %10.6e\n",nfout2,t-avgtime,t,avgtime);
      fprintf(fout1,"%s",bufor);
      fclose(fout1);
    }

  //body
  sprintf(bufor,"%s/%s%04d.dat",folder,prefix,nfout2);

  MPI_File cFile;
  MPI_Status status;
  MPI_Request req;

 
  int rc = MPI_File_open( MPI_COMM_WORLD, bufor, MPI_MODE_WRONLY | MPI_MODE_CREATE, MPI_INFO_NULL, &cFile );
  if (rc)
  {
    printf( "Unable to open/create file %s\n", bufor );fflush(stdout); exit(-1);
  }

  /***** first write all the indices ******/

  int nz=NZ;
  int tnz=TNZ;
#ifdef AVGOUTPUT
  if(AVGOUTPUT==2)
    {
      nz=1;
      tnz=1;
    }
#endif

  int *indices;
  if((indices = (int *)malloc(NX*NY*nz*3*sizeof(int)))==NULL) my_err("malloc err. - fileop 8\n");
  
  int ix,iy,iz,iv;
  int gix,giy,giz;

  for(ix=0;ix<NX;ix++)
    for(iy=0;iy<NY;iy++)
      for(iz=0;iz<nz;iz++)
	{
	  mpi_local2globalidx(ix,iy,iz,&gix,&giy,&giz);
	  indices[ix*NY*nz*3+iy*nz*3+iz*3+0]=gix;
	  indices[ix*NY*nz*3+iy*nz*3+iz*3+1]=giy;
	  indices[ix*NY*nz*3+iy*nz*3+iz*3+2]=giz;
	}

  //set the initial location at each process for indices
  MPI_Offset pos;
  pos=PROCID*NX*NY*nz*(3*sizeof(int));  
  MPI_File_seek( cFile, pos, MPI_SEEK_SET ); 

  //write all indices
  MPI_File_write( cFile, indices, NX*NY*nz*3, MPI_INT, &status );
  
  /***** then primitives in the same order ******/

  //now let's try manually
  pos=TNX*TNY*tnz*(3*sizeof(int)) + PROCID*NX*NY*nz*((NV+NAVGVARS)*sizeof(ldouble)); 
  MPI_File_seek( cFile, pos, MPI_SEEK_SET ); 

  ldouble *pout;
  if((pout=(ldouble *) malloc(NX*NY*nz*(NV+NAVGVARS)*sizeof(ldouble)))==NULL) my_err("malloc err. - fileop 9\n");
  for(ix=0;ix<NX;ix++)
    for(iy=0;iy<NY;iy++)
      for(iz=0;iz<nz;iz++)
	for(iv=0;iv<(NV+NAVGVARS);iv++)
	  pout[ix*NY*nz*(NV+NAVGVARS)+iy*nz*(NV+NAVGVARS)+iz*(NV+NAVGVARS)+iv]=get_uavg(pavg,iv,ix,iy,iz);

  MPI_File_write( cFile, pout, NX*NY*nz*(NV+NAVGVARS), MPI_LDOUBLE, &status );

  free(pout);
  free(indices);

  MPI_File_close( &cFile );

#endif
  return 0;
}


/*********************************************/
//serial binary output to avg file
/*********************************************/

int 
fprint_avgfile_bin(ldouble t, char* folder,char *prefix)
{
  char bufor[250];
  
  //header
  if(PROCID==0)
    {
      sprintf(bufor,"%s/%s%04d.head",folder,prefix,nfout2);
      fout1=fopen(bufor,"w"); 
      sprintf(bufor,"## %5d %10.6e %10.6e %10.6e\n",nfout2,t-avgtime,t,avgtime);
      fprintf(fout1,"%s",bufor);
      fclose(fout1);
    }

  //body
  sprintf(bufor,"%s/%s%04d.dat",folder,prefix,nfout2);
  fout1=fopen(bufor,"wb"); 

  int ix,iy,iz,iv;
  int gix,giy,giz;
  ldouble pp[NV];
  //indices first
  for(ix=0;ix<NX;ix++)
    for(iy=0;iy<NY;iy++)
      for(iz=0;iz<NZ;iz++)
	{
	  mpi_local2globalidx(ix,iy,iz,&gix,&giy,&giz);
	  fwrite(&gix,sizeof(int),1,fout1);
	  fwrite(&giy,sizeof(int),1,fout1);
	  fwrite(&giz,sizeof(int),1,fout1);
	}

  //then, in the same order, primitives
  for(ix=0;ix<NX;ix++)
    for(iy=0;iy<NY;iy++)
      for(iz=0;iz<NZ;iz++)
	{
	  fwrite(&get_uavg(pavg,0,ix,iy,iz),sizeof(ldouble),NV+NAVGVARS,fout1);
	}

  fclose(fout1);


  return 0;
}

							  
/*********************************************/
/* reads avg dump file */
/* puts conserved into the memory */
/* converts them to primitives */
/*********************************************/
int
fread_avgfile(int nout1, char* base,ldouble *pavg, ldouble *dt,ldouble *t)
{
  char bufor[250];

  #ifdef MPI
  my_err("fread_avgfile should not be used with MPI\n");
  exit(1);
  
  #else //no MPI

  fread_avgfile_bin(nout1,base,pavg,dt,t);

  #endif
  
  return 0;
}

int 
fread_avgfile_bin(int nout1, char *base, ldouble *pavg, ldouble *dt, ldouble *t)
{
  int ret, ix,iy,iz,iv,i,ic,gix,giy,giz;
  char fname[40],fnamehead[40];


  printf("%s%04d.dat\n",base,nout1);
  printf("%s%04d.head\n",base,nout1);
  
  sprintf(fname,"%s%04d.dat",base,nout1);
  sprintf(fnamehead,"%s%04d.head",base,nout1);

  FILE *fdump;

  /***********/
  //header file
  fdump=fopen(fnamehead,"r");

  //reading parameters, mostly time
  int intpar[5];
  ldouble ldpar[5];
  ret=fscanf(fdump,"## %d %lf %lf %lf\n",&intpar[0],&ldpar[0],&ldpar[1],&ldpar[2]);
  if(PROCID==0) printf("avg file (%s) read no. %d at times: %.6e to %.6e (dt=%.6e)\n",
	 fname,intpar[0],ldpar[0],ldpar[1],ldpar[2]); 
  
  *t=.5*(ldpar[0]+ldpar[1]);
  *dt=ldpar[2];
  fclose(fdump);
 
  /***********/
  //body file

  fdump=fopen(fname,"rb");

  struct geometry geom;
  ldouble xxvec[4],xxvecout[4];
  ldouble uu[NV],pp[NV],ftemp;
  char c;

  int **indices;
  if((indices = (int **)malloc(NX*NY*NZ*sizeof(int*)))==NULL) my_err("malloc err. - fileop 10\n");
  for(i=0;i<NX*NY*NZ;i++)
    if((indices[i]=(int *)malloc(3*sizeof(int)))==NULL) my_err("malloc err. - fileop 11\n");

  //to mark unfilled slots
  for(ix=0;ix<NX;ix++)
    for(iy=0;iy<NY;iy++)
      for(iz=0;iz<NZ;iz++)
	set_uavg(pavg,RHO,ix,iy,iz,-1.);

  //first indices
  for(ic=0;ic<NX*NY*NZ;ic++)
    {
      ret=fread(&gix,sizeof(int),1,fdump);
      ret=fread(&giy,sizeof(int),1,fdump);
      ret=fread(&giz,sizeof(int),1,fdump);

      mpi_global2localidx(gix,giy,giz,&ix,&iy,&iz);

      if(ix<0 || ix>=NX) {ix=0; printf("bad idx in avg: %d %d | %d %d %d\n",ic,NX*NY*NZ,ix,iy,iz);}
      if(iy<0 || iy>=NY) iy=0;
      if(iz<0 || iz>=NZ) iz=0;

      indices[ic][0]=ix;
      indices[ic][1]=iy;
      indices[ic][2]=iz;
    }

  //then averages
  for(ic=0;ic<NX*NY*NZ;ic++)
    {
      ix=indices[ic][0];
      iy=indices[ic][1];
      iz=indices[ic][2];

      ret=fread(&get_uavg(pavg,0,ix,iy,iz),sizeof(ldouble),NV+NAVGVARS,fdump);

#ifdef CONSISTENTGAMMA
      ldouble gamma = 1. + get_uavg(pavg,AVGPGAS,ix,iy,iz)/get_uavg(pavg,UU,ix,iy,iz);
      set_u_scalar(gammagas,ix,iy,iz,gamma);
#endif
    }

  for(i=0;i<NX*NY*NZ;i++)
    free(indices[i]);
  free(indices);

  fclose(fdump);

  return 0;
}

/*********************************************/
/* wrapper for coordinate output */
/*********************************************/

int fprint_coordfile(char* folder,char* prefix)
{
#if (COORDOUTPUT>0)
  fprint_coordBL(folder,prefix);
#endif

  return 0;
}

/*********************************************/
/* prints BL coordinates,  */
/*********************************************/
                              
int fprint_coordBL(char* folder,char* prefix)
 {
   char bufor[50];
   sprintf(bufor,"%s/%sBL.dat",folder,prefix);
   FILE* fout1=fopen(bufor,"w");

   int ix,iy,iz,iv;
   ldouble pp[NV];
   for(iz=0;iz<NZ;iz++)
     {
       for(iy=0;iy<NY;iy++)
	 {
	   for(ix=0;ix<NX;ix++)
	     {
	       struct geometry geom,geomBL;
	       fill_geometry(ix,iy,iz,&geom);
	       fill_geometry_arb(ix,iy,iz,&geomBL,BLCOORDS);

	       ldouble r=geomBL.xx;
	       ldouble th=geomBL.yy;
	       ldouble ph=geomBL.zz;
	     
	       fprintf(fout1,"%d %d %d ",ix,iy,iz);

	       fprintf(fout1,"%.5e %.5e %.5e ",r,th,ph);

	       fprintf(fout1,"\n");
	     }
	 }
     }

   fflush(fout1);
   fclose(fout1);

   return 0;
 }

                              
/*********************************************/
/* wrapper for simple output */
/*********************************************/
                              
int fprint_simplefile(ldouble t, int nfile, char* folder,char* prefix)
{
  //Cartesian output
#if (SIMOUTPUT==1)
  fprint_simplecart(t,nfile,folder,prefix);
#endif

  //Spherical output
#if (SIMOUTPUT==2)
  fprint_simplesph(t,nfile,folder,prefix);
#endif

  return 0;
}

/*********************************************/
/* prints in ASCII indices, cart coordinates,*/
/* primitives, velocities in cartesian       */
/*********************************************/
//ANDREW -- TODO: update

int fprint_simplecart(ldouble t, int nfile, char* folder,char* prefix)
 {
   char bufor[50];
   sprintf(bufor,"%s/%s%04d.dat",folder,prefix,nfile);
   fout1=fopen(bufor,"w");
  
   //header
   fprintf(fout1,"## %d %e %d %d %d %d\n",nfout1,t,PROBLEM,NX,NY,NZ);

   /***********************************/  
   /** writing order is fixed  ********/  
   /***********************************/  
 
   int ix,iy,iz,iv;
   ldouble pp[NV];
   int nz=NZ;
#if (PROBLEM==120)
   nz=1;
#endif
   for(iz=0;iz<nz;iz++)
     {
       for(iy=0;iy<NY;iy++)
	 {
	   for(ix=0;ix<NX+2;ix++)
	     {
	       for(iv=0;iv<NV;iv++)
		 {
                   pp[iv]=get_u(p,iv,ix,iy,iz);
		 }

	       struct geometry geom,geomcart,geomout,geomsph;
	       fill_geometry(ix,iy,iz,&geom);
	       fill_geometry_arb(ix,iy,iz,&geomcart,MINKCOORDS);
	       fill_geometry_arb(ix,iy,iz,&geomout,OUTCOORDS);
	       fill_geometry_arb(ix,iy,iz,&geomsph,SPHCOORDS);

	       ldouble dx[3];
	       dx[0]=get_size_x(ix,0);
	       dx[1]=get_size_x(iy,1);
	       dx[2]=get_size_x(iz,2);
	       ldouble gdet=geom.gdet;
	       ldouble volume=dx[0]*dx[1]*dx[2]*gdet;
	       trans_pall_coco(pp, pp, MYCOORDS,OUTCOORDS, geom.xxvec,&geom,&geomout);
	       ldouble rho=rhoGU2CGS(pp[RHO]);
	       #ifdef SIMOUTPUTINTERNAL
	       rho=pp[RHO];
	       #endif
	       ldouble temp=calc_PEQ_Tfromurho(pp[UU],pp[RHO],ix,iy,iz);
	       ldouble vel[4]={0,pp[VX],pp[VY],pp[VZ]};	
	       ldouble vx,vy,vz;
	       conv_vels(vel,vel,VELPRIM,VEL4,geomout.gg,geomout.GG);

	       vx=vel[1];
	       vy=vel[2];
	       vz=vel[3];
	       
	       //transform to cartesian
	       if (MYCOORDS==SCHWCOORDS || MYCOORDS==KSCOORDS   || MYCOORDS==KERRCOORDS || MYCOORDS==SPHCOORDS ||
		   MYCOORDS==MKS1COORDS || MYCOORDS==MKS2COORDS || MYCOORDS==MKS3COORDS || MYCOORDS==MSPH1COORDS || MYCOORDS==MKER1COORDS)
		 {
		   ldouble r=geomsph.xx;
		   ldouble th=geomsph.yy;
		   ldouble ph=geomsph.zz;

		   vel[2]*=r;
		   vel[3]*=r*sin(th);
		    
		   vx = sin(th)*cos(ph)*vel[1] 
		     + cos(th)*cos(ph)*vel[2]
		     - sin(ph)*vel[3];

		   vy = sin(th)*sin(ph)*vel[1] 
		     + cos(th)*sin(ph)*vel[2]
		     + cos(ph)*vel[3];

		   vz = cos(th)*vel[1] 
		     - sin(th)*vel[2];
		 }
	     
	       fprintf(fout1,"%d %d %d ",ix,iy,iz); //1-3

	       fprintf(fout1,"%.5e %.5e %.5e ",geomcart.xx,geomcart.yy,geomcart.zz);//4-6

	       fprintf(fout1,"%.5e %.5e ",rho,temp);//7-8

	       fprintf(fout1,"%.5e %.5e %.5e ",vx,vy,vz);//9-11

	       fprintf(fout1,"%.5e ",volume);//12

	       #ifdef RADIATION
	       ldouble Rtt,ehat,Rij[4][4];
	       ldouble ugas[4],Fx,Fy,Fz;
	       if(doingavg==0)
		{
		  calc_ff_Rtt(pp,&Rtt,ugas,&geomout);
		  ehat=-Rtt;  
		  calc_Rij(pp,&geomout,Rij); //calculates R^munu in OUTCOORDS
		  indices_2221(Rij,Rij,geomout.gg);	      							  
		}
	      else
		{
		  ehat=get_uavg(pavg,AVGEHAT,ix,iy,iz);
		  int i,j;
		  for(i=0;i<4;i++)
		    for(j=0;j<4;j++)
		      Rij[i][j]=get_uavg(pavg,AVGRIJ(i,j),ix,iy,iz);
		}

	       //transform to cartesian
	      if (MYCOORDS==SCHWCOORDS || MYCOORDS==KSCOORDS || MYCOORDS==KERRCOORDS || MYCOORDS==SPHCOORDS ||
		  MYCOORDS==MKS1COORDS || MYCOORDS==MKS2COORDS)
		{
		  ldouble r=geomsph.xx;
		  ldouble th=geomsph.yy;
		  ldouble ph=geomsph.zz;

		  Rij[2][0]*=r;
		  Rij[3][0]*=r*sin(th);

		  Fx = sin(th)*cos(ph)*Rij[1][0] 
		    + cos(th)*cos(ph)*Rij[2][0]
		    - sin(ph)*Rij[3][0];

		  Fy = sin(th)*sin(ph)*Rij[1][0] 
		    + cos(th)*sin(ph)*Rij[2][0]
		    + cos(ph)*Rij[3][0];

		  Fz = cos(th)*Rij[1][0] 
		    - sin(th)*Rij[2][0];
		}
	       
	      fprintf(fout1,"%.5e %.5e %.5e %.5e ",endenGU2CGS(ehat),fluxGU2CGS(Fx),fluxGU2CGS(Fy),fluxGU2CGS(Fz));//13-16
#endif

#if (PROBLEM==115 || PROBLEM==135) //SHOCKELECTRONTEST
	      ldouble uugas;
	      ldouble Tg,Te,Ti;
	      uugas=pp[UU];
	      Tg=calc_PEQ_Teifrompp(pp,&Te,&Ti,ix,iy,iz); //temperatures after explicit
	      
	      /**************/
	      //electrons
	      /**************/
	      ldouble ne=rho/MU_E/M_PROTON; //number density of photons and electrons
	      ldouble pe=K_BOLTZ*ne*Te;
	      ldouble gammae=GAMMAE;
#ifdef CONSISTENTGAMMA
#ifndef FIXEDGAMMASPECIES
	      gammae=calc_gammaintfromtemp(Te,ELECTRONS);
#endif
#endif
	      ldouble ue=pe/(gammae-1.);

	      /**************/
	      //ions
	      /**************/
	      ldouble ni=rho/MU_I/M_PROTON; //number density of photons and electrons
	      ldouble pi=K_BOLTZ*ni*Ti;
	      ldouble gammai=GAMMAI;
#ifdef CONSISTENTGAMMA
#ifndef FIXEDGAMMASPECIES
	      gammai=calc_gammaintfromtemp(Ti,IONS);
#endif
#endif
	      ldouble ui=pi/(gammai-1.);

	      ue = calc_ufromSerho(pp[ENTRE], rho, ELECTRONS,ix,iy,iz);
	      ui = calc_ufromSerho(pp[ENTRI], rho, IONS,ix,iy,iz);

	      ldouble gammagas=calc_gammagas(pp, ix, iy, iz);
	      gammagas=pick_gammagas(ix,iy,iz);
	      fprintf(fout1,"%e %e %e %.5e %.5e %.5e %.5e %.5e %.5e %.5e",
	      	      uugas,ui,ue,
		      get_u_scalar(vischeating,ix,iy,iz),
		      get_u_scalar(vischeatingnegebalance,ix,iy,iz),
		      gammagas,Te,Ti,gammae,gammai);//17-21 with rad, 13-17 without
#endif //PROBLEM==115 || PROBLEM==135

	      fprintf(fout1,"\n");
	     }
	 }
     }

   fflush(fout1);
   fclose(fout1);

   return 0;
 }

                              
/*********************************************/
/* prints in ASCII & BL coordinates,  */
/*********************************************/
                              
int fprint_simplesph(ldouble t, int nfile, char* folder,char* prefix)
 {
   char bufor[50];
   #if defined(GRTRANSSIMOUTPUT)
   sprintf(bufor,"%s/%s%04d_grtnew.dat",folder,prefix,nfile);
   #elif defined(GRTRANSSIMOUTPUT_2)
   sprintf(bufor,"%s/%s%04d_simcgs.dat",folder,prefix,nfile);
   #else
   sprintf(bufor,"%s/%s%04d.dat",folder,prefix,nfile);
   #endif
   fout1=fopen(bufor,"w");
  
   /***********************************/  
   /** writing order is fixed  ********/  
   /***********************************/  
 
   int ix,iy,iz,iv;
   int iix;
   ldouble pp[NV],phi,tausca,tauscar,lorentz,vel[4],vcon[4],tauscarloc;
   int nz=NZ;
   struct geometry geom,geomBL;
 
#if (PROBLEM==120)
   nz=1;
#endif

   int xmin=-2;
  
   //ANDREW -- header for grtrans
#if defined (GRTRANSSIMOUTPUT) || defined(GRTRANSSIMOUTPUT_2)
   for(iix=-2;iix<NX;iix++)
     {
        fill_geometry_arb(iix,NY/2,NZ/2,&geomBL,OUTCOORDS);
	if(geomBL.xx>=rhorizonBL)
	  {
	    xmin=iix;
	    break;
	  }
     }
       
   if(NZ==1)
     fprintf(fout1,"%.5e %5d %5d %.5e %.5e ",t,NX+2,NY,BHSPIN,MASS);
   else
     fprintf(fout1,"%.5e %5d %5d %5d %.5e %.5e ",t,NX+2,NY,NZ,BHSPIN,MASS);     
   fprintf(fout1,"%.5e %.5e %.5e %.5e %.5e\n",MKSR0,MKSH0,MKSMY1,MKSMY2,MKSMP0);
#ifdef RELELECTRONS
   fprintf(fout1,"%5d %.5e %.5e\n",NRELBIN, RELGAMMAMIN, RELGAMMAMAX);
#endif
#endif //GRTRANSSIMOUTPUT
   
#ifdef RELELECTRONS //ANDREW array for finding nonthermal gamma break
  int ie;
  ldouble gammapbrk[NRELBIN];
  for(ie=0; ie<NRELBIN; ie++) gammapbrk[ie] = pow(relel_gammas[ie], RELEL_HEAT_INDEX + 0.5);
#endif  

   // loop over all cells  
   for(iz=0;iz<nz;iz++)
     {
       #ifndef RAD_INTEGRATION
       for(iix=-2;iix<NX;iix++)
	 {
	   phi=0.;
	   tausca=0.;
       #else //Start from outermost radial cell
       for(iy=0;iy<NY;iy++)
	 {
	   tauscar=0.;
       #endif
           #ifndef RAD_INTEGRATION
	   for(iy=0;iy<NY;iy++)
	   {
           #else //Start from outermost radial cell
           for(iix=NX-1;iix>-3;iix--)
	   {
           #endif

               ix=iix;
 
	       fill_geometry(ix,iy,iz,&geom);
	       fill_geometry_arb(ix,iy,iz,&geomBL,OUTCOORDS);

#ifdef SIMOUTPUTWITHINDTHETA 
	       if(fabs(geomBL.yy-M_PI/2)>SIMOUTPUTWITHINDTHETA)
		 continue;
#endif

	       ldouble r=geomBL.xx;
	       ldouble th=geomBL.yy;
	       ldouble ph=geomBL.zz;

#if defined(GRTRANSSIMOUTPUT) || defined(GRTRANSSIMOUTPUT_2)
	       ldouble x1=geom.xx;
	       ldouble x2=geom.yy;
	       ldouble x3=geom.zz;
	       fprintf(fout1,"%d %d %d ",ix,iy,iz); //(1-3)
	       fprintf(fout1,"%.5e %.5e %.5e ",x1,x2,x3); //(4-6)
	       fprintf(fout1,"%.5e %.5e %.5e ",r,th,ph); //(7-9)
	       
               //ANDREW in grtrans, fill values below horizon with values right above horizon
	       if(r<rhorizonBL)
	       {
                 ix=xmin;
                 fill_geometry(ix,iy,iz,&geom);
	         fill_geometry_arb(ix,iy,iz,&geomBL,OUTCOORDS);
               }
#else	     
	       fprintf(fout1,"%d %d %d ",ix,iy,iz); //(1-3)
	       fprintf(fout1,"%.5e %.5e %.5e ",r,th,ph); //(4-6)
#endif

	      for(iv=0;iv<NV;iv++)
	      {
		  if(doingavg)
		    pp[iv]=get_uavg(pavg,iv,ix,iy,iz);
		  else
		    pp[iv]=get_u(p,iv,ix,iy,iz);
	      }

	      ldouble dxph[3],dx[3],xx1[4],xx2[4];
	      xx1[0]=0.;xx1[1]=get_xb(ix,0);xx1[2]=get_x(iy,1);xx1[3]=get_x(iz,2);
	      xx2[0]=0.;xx2[1]=get_xb(ix+1,0);xx2[2]=get_x(iy,1);xx2[3]=get_x(iz,2);
	      coco_N(xx1,xx1,MYCOORDS,OUTCOORDS);
	      coco_N(xx2,xx2,MYCOORDS,OUTCOORDS);
	      dx[0]=fabs(xx2[1]-xx1[1]);
	      xx1[0]=0.;xx1[1]=get_x(ix,0);xx1[2]=get_xb(iy,1);xx1[3]=get_x(iz,2);
	      xx2[0]=0.;xx2[1]=get_x(ix,0);xx2[2]=get_xb(iy+1,1);xx2[3]=get_x(iz,2);
	      coco_N(xx1,xx1,MYCOORDS,OUTCOORDS);
	      coco_N(xx2,xx2,MYCOORDS,OUTCOORDS);
	      dx[1]=fabs(xx2[2]-xx1[2]);
	      xx1[0]=0.;xx1[1]=get_x(ix,0);xx1[2]=get_x(iy,1);xx1[3]=get_xb(iz,2);
	      xx2[0]=0.;xx2[1]=get_x(ix,0);xx2[2]=get_x(iy,1);xx2[3]=get_xb(iz+1,2);
	      coco_N(xx1,xx1,MYCOORDS,OUTCOORDS);
	      coco_N(xx2,xx2,MYCOORDS,OUTCOORDS);
	      dx[2]=fabs(xx2[3]-xx1[3]);

	      dxph[0]=dx[0]*sqrt(geomBL.gg[1][1]);
	      dxph[1]=dx[1]*sqrt(geomBL.gg[2][2]);
	      dxph[2]=dx[2]*sqrt(geomBL.gg[3][3]);

	      ldouble gdet=geom.gdet;
	      ldouble volume=gdet*get_size_x(ix,0)*get_size_x(iy,1)*get_size_x(iz,2);
	       
               
#if defined(GRTRANSSIMOUTPUT) || defined(GRTRANSSIMOUTPUT_2)
	       volume=gdet; // For grtrans output, replace volume with gdet
#endif
	       ldouble rho,rhoucont,uint,pgas,temp,bsq,bcon[4],bcov[4];
	       ldouble utcon[4],utcov[4],ucon[4],ucov[4],Tij[4][4],Tij22[4][4];
	       ldouble Ti,Te;
	       ldouble gamma=GAMMA;

	       int i,j;

#ifdef CONSISTENTGAMMA
	       gamma=pick_gammagas(ix,iy,iz);
#endif
	     
	       if(doingavg)
		 {
                   //ANDREW we need pp for some relel computations below
		   trans_pall_coco(pp, pp, MYCOORDS,OUTCOORDS, geom.xxvec,&geom,&geomBL);
		   rhoucont=get_uavg(pavg,AVGRHOUCON(0),ix,iy,iz);

		   rho=get_uavg(pavg,RHO,ix,iy,iz);
		   uint=get_uavg(pavg,UU,ix,iy,iz);
		   pgas=get_uavg(pavg,AVGPGAS,ix,iy,iz);
		   temp=calc_PEQ_Tfromprho(pgas,rho,ix,iy,iz);

		   vel[0]=get_uavg(pavg,AVGRHOUCON(0),ix,iy,iz)/get_uavg(pavg,RHO,ix,iy,iz);
		   vel[1]=get_uavg(pavg,AVGRHOUCON(1),ix,iy,iz)/get_uavg(pavg,RHO,ix,iy,iz);
		   vel[2]=get_uavg(pavg,AVGRHOUCON(2),ix,iy,iz)/get_uavg(pavg,RHO,ix,iy,iz);
		   vel[3]=get_uavg(pavg,AVGRHOUCON(3),ix,iy,iz)/get_uavg(pavg,RHO,ix,iy,iz);
                   for(i=0;i<4;i++) vcon[i]=vel[i];
                   lorentz = fabs(vel[0])/sqrt(fabs(geomBL.GG[0][0]));

		   utcon[0]=get_uavg(pavg,AVGRHOUCON(0),ix,iy,iz)/get_uavg(pavg,RHO,ix,iy,iz);
		   utcon[1]=get_uavg(pavg,AVGRHOUCON(1),ix,iy,iz)/get_uavg(pavg,RHO,ix,iy,iz);
		   utcon[2]=get_uavg(pavg,AVGRHOUCON(2),ix,iy,iz)/get_uavg(pavg,RHO,ix,iy,iz);
		   utcon[3]=get_uavg(pavg,AVGRHOUCON(3),ix,iy,iz)/get_uavg(pavg,RHO,ix,iy,iz);

                   int ii,jj;
                   for(ii=0;ii<4;ii++)
		     for(jj=0;jj<4;jj++)
		       Tij[ii][jj]=get_uavg(pavg,AVGTIJ(ii,jj),ix,iy,iz);                 

		   indices_2122(Tij,Tij22,geomBL.gg);  
                 
#if defined(GRTRANSSIMOUTPUT) || defined(GRTRANSSIMOUTPUT_2)
                   //ANDREW NORMALIZE u^0 for grtrans
                   fill_utinucon(utcon,geomBL.gg,geomBL.GG);
		   indices_21(utcon,utcov,geomBL.gg); 
#endif
                   pp[RHO]=rho;
		   pp[UU]=uint;
#ifdef MAGNFIELD
		   bsq=get_uavg(pavg,AVGBSQ,ix,iy,iz);
		   bcon[0]=get_uavg(pavg,AVGBCON(0),ix,iy,iz);
		   bcon[1]=get_uavg(pavg,AVGBCON(1),ix,iy,iz);
		   bcon[2]=get_uavg(pavg,AVGBCON(2),ix,iy,iz);
		   bcon[3]=get_uavg(pavg,AVGBCON(3),ix,iy,iz);

#if defined(GRTRANSSIMOUTPUT) || defined(GRTRANSSIMOUTPUT_2)
                  //ANDREW NORMALIZE b^0 to be orthogonal with u^\mu
		  bcon[0]=-dot3nr(bcon,utcov)/utcov[0];
		  indices_21(bcon,bcov,geomBL.gg);

                  //ANDREW NORMALIZE b^mu to be equal to B^2
		  ldouble alphanorm = bsq/dotB(bcon,bcov);
		  if(alphanorm<0.) my_err("alpha.lt.0 in b0 norm !!\n");
                  for(i=0;i<4;i++)
		  {
		   bcon[i]*=sqrt(alphanorm);
		  }
#endif
#endif

		  
#ifdef EVOLVEELECTRONS
		  ldouble pe,pi;
		  pe=get_uavg(pavg,AVGPE,ix,iy,iz);
		  pi=get_uavg(pavg,AVGPI,ix,iy,iz);
		  //electrons
		  ldouble ne=get_uavg(pavg,RHO,ix,iy,iz)/MU_E/M_PROTON; 
		  //ions
		  ldouble ni=get_uavg(pavg,RHO,ix,iy,iz)/MU_I/M_PROTON; 

                  #ifdef RELELECTRONS
                  #ifndef NORELELAVGS
                  ne=get_uavg(pavg,AVGNETH,ix,iy,iz);            
                  #endif
                  #endif 
		  Te=pe/K_BOLTZ/ne;
		  Ti=pi/K_BOLTZ/ni;

		  //write these temperatures into the primitives as corresponding entropies
		  ldouble rhoeth=ne*MU_E*M_PROTON;
		  pp[ENTRE]=calc_SefromrhoT(rhoeth,Te,ELECTRONS);
		  pp[ENTRI]=calc_SefromrhoT(rho,Ti,IONS);
		 
#endif                
		 }
	       else //not doingavg; on the go from the primitives
		 { 
		   trans_pall_coco(pp, pp, MYCOORDS,OUTCOORDS, geom.xxvec,&geom,&geomBL);

		   rho=pp[0];
		   uint=pp[1];
		   pgas=(gamma-1.)*uint;
                   ldouble vel[4],vcon[4],tauscarloc;
                   
                   //obtain 4 velocity
	           vel[1]=pp[VX];
	           vel[2]=pp[VY];
	           vel[3]=pp[VZ];
	           conv_vels(vel,vel,VELPRIM,VEL4,geomBL.gg,geomBL.GG);
                   for(i=0;i<4;i++) vcon[i]=vel[i];
                   lorentz = fabs(vel[0])/sqrt(fabs(geomBL.GG[0][0]));
		   rhoucont=pp[RHO]*vcon[0];

                   calc_Tij(pp,&geomBL,Tij22);
		   indices_2221(Tij22,Tij,geomBL.gg);
                   calc_ucon_ucov_from_prims(pp, &geomBL, utcon, ucov);
                   temp=calc_PEQ_Tfromurho(uint,rho,ix,iy,iz);
                   temp=calc_PEQ_Teifrompp(pp,&Te,&Ti,geomBL.ix,geomBL.iy,geomBL.iz);
#ifdef MAGNFIELD
                   calc_bcon_bcov_bsq_from_4vel(pp, utcon, ucov, &geomBL, bcon, bcov, &bsq);
#endif
		 }

#ifdef OUTPUTINGU
	       rho=pp[RHO];
#else
	       rho=rhoGU2CGS(pp[RHO]);
#endif
	     
#ifdef RHOLABINSIM
	       rho*=utcon[0];
#endif

	       fprintf(fout1,"%.5e %.5e ",rho,temp); //(7-8) , (10-11) for grtrans 
	       fprintf(fout1,"%.5e %.5e %.5e %.5e ",utcon[0],utcon[1],utcon[2],utcon[3]); //(9-12) , (12-15) for grtrans
#ifndef GRTRANSSIMOUTPUT_2
	       fprintf(fout1,"%.5e ", volume); // (13) , (16) for grtrans
#endif
	       ldouble ehat=0.;
#ifdef RADIATION
	       ldouble Rtt,Rij[4][4],Rij22[4][4],vel[4];
	       ldouble ugas[4],Fx,Fy,Fz;
	       ldouble Gi[4],Giff[4]={0.,0.,0.,0.};
	       ldouble Gic[4],Gicff[4]={0.,0.,0.,0.};
	       ldouble Trad;

	       ldouble CoulombCoupling=0.;
	       if(doingavg==0) // from primitives
		{
		  calc_ff_Rtt(pp,&Rtt,ugas,&geomBL);
		  ehat=-Rtt;  
		  calc_Rij(pp,&geomBL,Rij22); //calculates R^munu in OUTCOORDS
		  indices_2221(Rij22,Rij,geomBL.gg);

		  //four fource
		  calc_Gi(pp,&geomBL,Giff,0.0,0,0); //ANDREW 0 for fluid frame
		  
#if defined(COMPTONIZATION) || defined(EVOLVEPHOTONNUMBER)
		  ldouble kappaes=calc_kappaes(pp,&geomBL);
		  //directly in ff
		  vel[1]=vel[2]=vel[3]=0.; vel[0]=1.;
		  calc_Compt_Gi(pp,&geomBL,Gicff,ehat,Te,kappaes,vel);
#endif 

#ifdef EVOLVEPHOTONNUMBER //the color temperature of radiation
  Trad = calc_ncompt_Thatrad_full(pp,&geomBL);
#endif
	
#ifdef EVOLVEELECTRONS	 
#ifndef  SKIPCOULOMBCOUPLING
  CoulombCoupling=calc_CoulombCoupling(pp,&geomBL); 
#endif
#endif
		}
	       else //from avg
		{
		  ehat=get_uavg(pavg,AVGEHAT,ix,iy,iz);
		  int i,j;
		  for(i=0;i<4;i++)
		    for(j=0;j<4;j++)
		      Rij[i][j]=get_uavg(pavg,AVGRIJ(i,j),ix,iy,iz); 
		  for(j=0;j<4;j++)
		    Giff[j]=get_uavg(pavg,AVGGHAT(j),ix,iy,iz);
		    
		  indices_2122(Rij,Rij22,geomBL.gg);  

		  //ANDREW recompute if Giff in avg, if we accidentally saved it as lab frame
                  #ifdef SIMOUTPUT_GILAB2FF
		  calc_Gi(pp,&geomBL,Giff,0.0,0,0); //ANDREW 0 for fluid frame, 2 for fluid frame thermal only
                  #endif

#if defined(COMPTONIZATION) || defined(EVOLVEPHOTONNUMBER)
		  for(j=0;j<4;j++)
		    Gicff[j]=get_uavg(pavg,AVGGHATCOMPT(j),ix,iy,iz);
#endif			  
		  Trad=calc_ncompt_Thatrad_fromEN(ehat,get_uavg(pavg,AVGNFHAT,ix,iy,iz));
		}
	       
	       //flux
	       Fx=Rij[1][0];
	       Fy=Rij[2][0];
	       Fz=Rij[3][0];
#ifndef GRTRANSSIMOUTPUT_2
#ifdef OUTPUTINGU
	       fprintf(fout1,"%.5e %.5e %.5e %.5e ",ehat,Fx,Fy,Fz); //(14) - (17) 
	       fprintf(fout1,"%.5e %.5e ",Giff[0],Gicff[0]); //(18)-(19)
               fprintf(fout1,"%.5e %.5e ",ehat, Trad); //(20), (21)         
#else
	       fprintf(fout1,"%.5e %.5e %.5e %.5e ",endenGU2CGS(ehat),fluxGU2CGS(Fx),fluxGU2CGS(Fy),fluxGU2CGS(Fz)); //(14) - (17), (17)-(20) for grtrans
	       ldouble conv=kappaGU2CGS(1.)*rhoGU2CGS(1.)*endenGU2CGS(1.)*CCC; //because (cE-4piB) in non-geom
	       fprintf(fout1,"%.5e %.5e ",Giff[0]*conv,Gicff[0]*conv); //(18)-(19) , (21)-(22) for grtrans
               fprintf(fout1,"%.5e %.5e ",CoulombCoupling*conv, Trad); //(20), (21) , (23)-(24) for grtrans      
#endif
#endif
#endif //RADIATION

	       ldouble gammam1=gamma-1.;
	       ldouble betarad=ehat/3./(pgas);
#ifdef RADIATION
	       //ldouble muBe = (-Tij[1][0]-Rij[1][0] - rhouconr)/rhouconr;
	       ldouble bernoulli=(-Tij[0][0] -Rij[0][0] - rhoucont)/rhoucont;
#else
	       //ldouble muBe = (-Tij[1][0] - rhouconr)/rhouconr;
	       ldouble bernoulli=(-Tij[0][0] - rhoucont)/rhoucont;
#endif
	       
	       //magn. field components
#ifdef MAGNFIELD
	       if(doingavg==0) 
	        {
	          calc_ucon_ucov_from_prims(pp, &geomBL, utcon, ucov);
                  calc_bcon_bcov_bsq_from_4vel(pp, utcon, ucov, &geomBL, bcon, bcov, &bsq);
		}
	      else
		{
		  bsq=get_uavg(pavg,AVGBSQ,ix,iy, iz);
		  bcon[0]=get_uavg(pavg,AVGBCON(0),ix,iy,iz);
		  bcon[1]=get_uavg(pavg,AVGBCON(1),ix,iy,iz);
		  bcon[2]=get_uavg(pavg,AVGBCON(2),ix,iy,iz);
		  bcon[3]=get_uavg(pavg,AVGBCON(3),ix,iy,iz);

                  #if defined(GRTRANSSIMOUTPUT) || defined(GRTRANSSIMOUTPUT_2)
                  //ANDREW NORMALIZE b^0 to be orthogonal with u^\mu
		  bcon[0]=-dot3nr(bcon,utcov)/utcov[0];
		  indices_21(bcon,bcov,geomBL.gg);

                  //ANDREW NORMALIZE b^mu to be equal to B^2
		  ldouble alphanorm = bsq/dotB(bcon,bcov);
		  if(alphanorm<0.) my_err("alpha.lt.0 in b0 norm !!\n");
                  for(i=0;i<4;i++)
		  {
		   bcon[i]*=sqrt(alphanorm);
		  }
                  #endif
		  
		}	       

	       ldouble betamag = bsq/2./(pgas + ehat/3.	+ bsq/2.);
	       
	       //to CGS!
	       #ifndef OUTPUTINGU
	       bsq=endenGU2CGS(bsq);
	       ldouble scaling=endenGU2CGS(1.);
	       for(i=0;i<4;i++)
		 {
		   bcon[i]*=sqrt(scaling);
		 }
               #endif

	       //magnetic flux parameter
	       int iphimin,iphimax;
	       iphimin=0;
	       iphimax=TNY-1;

#if defined(CORRECT_POLARAXIS) || defined(CORRECT_POLARAXIS_3D)
	       iphimin=NCCORRECTPOLAR; 
#ifndef HALFTHETA
	       iphimax=TNY-NCCORRECTPOLAR-1;
#endif
#endif
	      
	       if(iy>=iphimin && iy<=iphimax)
		 {
                   #ifndef RAD_INTEGRATION
		   phi+=geom.gdet*get_u(p,B1,ix,iy,iz)*get_size_x(iy,1)*2.*M_PI;
		   tausca+=rho*0.34*lenGU2CGS(dxph[1]); //rho already converted to cgs
                   #endif
		 }
               #ifdef RAD_INTEGRATION
               #ifdef RADIATION
	       if(ix>=0)
		 {
		   //tauscarloc = vcon[0]*(1.-abs(vcon[1]))*calc_kappaes(pp,&geomBL); //rho already converted to cgs
		   tauscarloc = (vcon[0]-vcon[1])*calc_kappaes(pp,&geomBL); //rho already converted to cgs

                   if(ix==NX-1)
		   {
		       tauscar=tauscarloc*dxph[0];
      	           }
                   else
                   {
                       tauscar += tauscarloc*dxph[0];
                   }
		 }
               #endif
               #endif

	       
	       //(14) - (19) or (22) - (27) if radiation included (+3 with grtrans 3d)
#if defined(GRTRANSSIMOUTPUT)
	       fprintf(fout1,"%.5e %.5e %.5e %.5e %.5e %.5e ",bcon[0],bcon[1],bcon[2],bcon[3],bsq,phi);
#elif defined(GRTRANSSIMOUTPUT_2)
	       fprintf(fout1,"%.5e %.5e %.5e %.5e %.5e ", bcon[0],bcon[1],bcon[2],bcon[3],bsq); 
#else    
	       fprintf(fout1,"%.5e %.5e %.5e %.5e %.5e %.5e ",bsq,bcon[1],bcon[2],bcon[3],phi,betamag);
#endif //GRTRANSSIMOUTPUT

	       // (28) - (29) when rad and magn field on, (20) - (21) with no radiation (+3 with grtrans 3d)
#ifndef GRTRANSSIMOUTPUT_2
#ifndef RAD_INTEGRATION
               fprintf(fout1,"%.5e %.5e ",betarad,bernoulli); 	       
	       //fprintf(fout1,"%.5e %.5e ",betarad,tausca); 
	       //fprintf(fout1,"%.5e %.5e ",betarad,muBe); 
#else
	       fprintf(fout1,"%.5e %.5e ",betarad,bernoulli); 	       
	       //fprintf(fout1,"%.5e %.5e ",betarad,tauscar); 
	       //fprintf(fout1,"%.5e %.5e ",betarad,muBe); 
#endif //RAD_INTEGRATION
#endif //GRTRANSSIMOUTPUT_2
#endif //MAGNFIELD
	       
	       

#ifdef EVOLVEELECTRONS

	       // (30) - (32) when rad and magn field on, (22) - (24) with no radiation (+3 with grtrans 3d)
	       fprintf(fout1,"%.5e %.5e %.5e ",Te, Ti, gamma); 
	       
	       ldouble vischeat,pe,ue,gammae,ne,tempeloc;
	       gammae=GAMMAE;
	       if(doingavg)
		 {
		   vischeat=get_uavg(pavg,AVGVISCHEATING,ix,iy,iz);
		   pe=get_uavg(pavg,AVGPE,ix,iy,iz);
		   ne=calc_thermal_ne(pp); 
		   tempeloc=pe/K_BOLTZ/ne;
	           gammae=GAMMAE;
                   #ifdef CONSISTENTGAMMA
		   #ifndef FIXEDGAMMASPECIES
		   gammae=calc_gammaintfromtemp(tempeloc,ELECTRONS);
                   #endif
		   #endif
		   ue=pe/(gammae-1.);
		 }
	       else
		 {
		   vischeat=get_u_scalar(vischeating,ix,iy,iz);
		   ldouble rhoeth=calc_thermal_ne(pp)*M_PROTON*MU_E;
		   ue=calc_ufromSerho(pp[ENTRE],rhoeth,ELECTRONS,ix,iy,iz); 
		 }


	       //ANDREW
	       //in avg, vischeat was averaged as du, not du/dtau
	       //recompute dt and use that as an estimate
               #ifdef DIVIDEVISCHEATBYDT
               dt=get_u_scalar(cell_dt,ix,iy,iz); //individual time step
	       ldouble dtau = dt/vel[0];
	       vischeat/=dtau;
               #endif
               
	       ldouble meandeltae=get_uavg(pavg,AVGVISCHEATINGTIMESDELTAE,ix,iy,iz)/get_uavg(pavg,AVGVISCHEATING,ix,iy,iz);

	       #ifndef OUTPUTINGU
               ue = endenGU2CGS(ue);
               vischeat=endenGU2CGS(vischeat)*timeCGS2GU(1.);
               #endif
	       
	       fprintf(fout1,"%.5e %.5e %.5e ",meandeltae,vischeat,ue); //(33) - (35) if rad and magn on, (25) - (27) if not (+3 with grtrans)

	       //ANDREW rel electron quantities
	       //(36) -- if rad and magn on, (28) -- if not (+3 with grtrans included)
#ifdef RELELECTRONS
   ldouble nrelel, urelel, G0relel, gammabrk;
               if(doingavg==0)
	       {
		  urelel=calc_relel_uint(pp);
		  nrelel=calc_relel_ne(pp);
               }
	       else
	       {
	       #ifndef NORELELAVGS
	          nrelel=get_uavg(pavg,AVGNRELEL,ix,iy,iz);
                  urelel=get_uavg(pavg,AVGURELEL,ix,iy,iz);
               #else
		  urelel=calc_relel_uint(pp);
		  nrelel=calc_relel_ne(pp);
               #endif
	       }

               G0relel = -1.*calc_relel_G0_fluidframe(pp, &geomBL, 0.0, 0); //ANDREW - fluid frame

	       gammabrk=RELEL_INJ_MIN;

	       //absolute maximum of g^4*n for g > RELGAMMAMIN
	       ldouble nbrk=pp[NEREL(0)]*gammapbrk[0];
	       ldouble nbrk2;
	       for(ie=1;ie<NRELBIN;ie++)
	       {
		 if (relel_gammas[ie] < RELEL_INJ_MIN)
		 {
		   gammabrk=RELEL_INJ_MIN;
		   nbrk =  pp[NEREL(ie)]*gammapbrk[ie];
		 }

	         else 
	         {
               	   nbrk2 =  pp[NEREL(ie)]*gammapbrk[ie];
		   if(nbrk2 > nbrk)
	           {
		     nbrk=nbrk2;
	             gammabrk=relel_gammas[ie];
         	   }
	         }
		}
	       
	       	 #ifndef OUTPUTINGU
                 nrelel = numdensGU2CGS(nrelel);
                 urelel = endenGU2CGS(urelel);
		 G0relel = G0relel*kappaGU2CGS(1.)*rhoGU2CGS(1.)*endenGU2CGS(1.)*CCC; //because (cE-4piB) in non-geom
                 #endif

		 fprintf(fout1,"%.5e %.5e %.5e %.5e",urelel,nrelel,G0relel,gammabrk); //(36) - (39) if rad and magn on , (38)-(41) with grtrans

	       ldouble nbin;
	       for(ie=0; ie<NRELBIN; ie++)
	       {
                 if(doingavg)
                   nbin=get_uavg(pavg,NEREL(ie),ix,iy,iz);
		 else
		   nbin=pp[NEREL(ie)];
		 #ifndef OUTPUTINGU
                 nbin = numdensGU2CGS(nbin);
		 #endif
	         fprintf(fout1," %.5e ",nbin); //(40)-- if rad and magn on, (42)-- with grtrans 3d
	       }	 	       
#endif //RELELECTRONS
#endif //EVOLVEELECTRONS

               //Full output - Leave off for output for HEROIC
#ifndef GRTRANSSIMOUTPUT
#ifdef FULLOUTPUT
               fprintf(fout1," %.5e ",lorentz); //(30) if rad and magn on and no relele, (40) with relele
	
               fprintf(fout1," %.5e %.5e %.5e %.5e ",ucon[0],ucon[1],ucon[2],ucon[3]); // 31-34  
               fprintf(fout1," %.5e %.5e %.5e %.5e ",ucov[0],ucov[1],ucov[2],ucov[3]); // 35-38
               #ifdef MAGNFIELD 
               fprintf(fout1," %.5e %.5e %.5e %.5e ",bcon[0],bcon[1],bcon[2],bcon[3]); // 39-42   
               fprintf(fout1," %.5e %.5e %.5e %.5e ",bcov[0],bcov[1],bcov[2],bcov[3]); // 43-46   
               #endif

               int iii,jjj;
               //output T^munu (columns 46-62)
               for(iii=0;iii<4;iii++)
               {
                 for(jjj=0;jjj<4;jjj++)
                 {
                   fprintf(fout1," %.5e ",Tij22[iii][jjj]);
                 }
               }
               //output T^mu_nu (columns 63-78)
               for(iii=0;iii<4;iii++)
               {
                 for(jjj=0;jjj<4;jjj++)
                 {
                   fprintf(fout1," %.5e ",Tij[iii][jjj]);
                 }
               }
               #ifdef RADIATION
               //output R^munu (columns 79-94)
               for(iii=0;iii<4;iii++)
               {
                 for(jjj=0;jjj<4;jjj++)
                 {
                   fprintf(fout1," %.5e ",Rij22[iii][jjj]);
                 }
               }
               //output R^mu_nu (columns 95-110)
               for(iii=0;iii<4;iii++)
               {
                 for(jjj=0;jjj<4;jjj++)
                 {
                   fprintf(fout1," %.5e ",Rij[iii][jjj]);
                 }
               }
               #endif
#endif
#endif

	       fprintf(fout1,"\n");
	     }
	 }
     }

   fflush(fout1);
   fclose(fout1);

   return 0;
 }

