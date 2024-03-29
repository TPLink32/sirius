#include <math.h>
#include <assert.h>
#ifdef _OPENMP
#include <omp.h>
#endif
#include "bench_gtc.h"
#define _LARGE_FILE_API
#include <unistd.h>
#include <sys/stat.h> 
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#if USE_ADIOS
#   include "adios.h"
#endif


int calc_moments(gtc_bench_data_t *gtc_input){
#if CALC_MOMENTS
  gtc_global_params_t     *params;
  gtc_field_data_t        *field_data;
  gtc_particle_data_t     *particle_data;
  gtc_particle_decomp_t   *parallel_decomp;
  gtc_radial_decomp_t *radial_decomp;
  gtc_diagnosis_data_t *diagnosis;

  const int*  __restrict__ igrid;
  const int*  __restrict__ mtheta;
  const real* __restrict__ delt;
  const real* __restrict__ qtinv;
  const int* __restrict__ itran;
  const real* __restrict__ markeri;

  const real* __restrict__ z0;
  const real* __restrict__ z1;
  const real* __restrict__ z2;
  const real* __restrict__ z3;
  const real* __restrict__ z4;
  const real* __restrict__ z5;

  int mi, mpsi, mzeta, mpsi_max, mstepall, mmomentsoutput, mgrid;
  real delr, delz, zetamin, qion, aion, gyroradius, pi, vthi_inv, pi2_inv, a0, a1, aion_inv, a_diff, tstep, zetamax;
 
  real *phi, *densityi;
  real *phi00, *phip00, *zonali;

  int ipsi_moments_in, ipsi_moments_out, igrid_moments_in, nloc_calc_moments;
  real *sendl_moments, *recvr_moments;
  real* __restrict__ moments;
  real* __restrict__ momentstmp;
  int ipsi_nover_in, ipsi_nover_out, ipsi_valid_in, ipsi_valid_out, nloc_nover, igrid_in, igrid_nover_in;

#if USE_MPI
  int icount, idest, isource, isendtag, irecvtag;
  MPI_Status istatus;
#endif

#if USE_ADIOS
  int adios_err;
  uint64_t adios_groupsize, adios_totalsize;
  int64_t  adios_handle;
  int ntoroidal, myrank_toroidal;
  MPI_Comm  comm;
#endif



  char cdum[19];
  char cdum1[17];  
  /*******/
  
  params            = &(gtc_input->global_params);
  field_data        = &(gtc_input->field_data);
  particle_data     = &(gtc_input->particle_data);
  parallel_decomp   = &(gtc_input->parallel_decomp);
  radial_decomp     = &(gtc_input->radial_decomp);
  diagnosis         = &(gtc_input->diagnosis_data);

#if USE_ADIOS
  ntoroidal  = parallel_decomp->ntoroidal;
  myrank_toroidal = parallel_decomp->myrank_toroidal;
#endif

  mzeta = params->mzeta; mpsi = params->mpsi;
  mi    = params->mi;
  pi = params->pi;
  zetamin = params->zetamin;
  a1 = params->a1; a0 = params->a0;
  qion = params->qion; aion = params->aion;
  gyroradius = params->gyroradius;
  tstep = params->tstep;
  mgrid = params->mgrid;
  zetamax = params->zetamax;
  mstepall = params->mstepall;
  mmomentsoutput = params->mmomentsoutput;

  delr = params->delr; delz = params->delz;
  delt = field_data->delt;
  igrid = field_data->igrid;
  qtinv = field_data->qtinv;
  mtheta = field_data->mtheta;
  itran = field_data->itran;
  phi = field_data->phi;
  densityi = field_data->densityi;
  phi00 = field_data->phi00;
  phip00 = field_data->phip00;
  zonali = field_data->zonali;
  markeri = field_data->markeri;

  ipsi_nover_in = radial_decomp->ipsi_nover_in;
  ipsi_nover_out = radial_decomp->ipsi_nover_out;
  ipsi_valid_in = radial_decomp->ipsi_valid_in;
  ipsi_valid_out = radial_decomp->ipsi_valid_out;
  nloc_nover = radial_decomp->nloc_nover;
  igrid_in = radial_decomp->igrid_in;
  igrid_nover_in = radial_decomp->igrid_nover_in;

  pi2_inv = 0.5/pi;
  vthi_inv = aion/(gyroradius * fabs(qion));
  aion_inv = 1.0/aion;
  mpsi_max = mpsi - 1;
  a_diff = a1 - a0;

  z0 = particle_data->z0; z1 = particle_data->z1;
  z2 = particle_data->z2; z3 = particle_data->z3;
  z4 = particle_data->z4; z5 = particle_data->z5;

  ipsi_moments_in = diagnosis->ipsi_moments_in;
  ipsi_moments_out = diagnosis->ipsi_moments_out;
  igrid_moments_in = diagnosis->igrid_moments_in;
  nloc_calc_moments = diagnosis->nloc_calc_moments;
  moments = diagnosis->moments;
  momentstmp = diagnosis->momentstmp;
  sendl_moments = diagnosis->sendl_moments;
  recvr_moments = diagnosis->recvr_moments;

   fprintf(stderr,"myrank_toroidal=%d   nloc_calc_moments=%d   igrid_nover_in=%d  igrid_moments_in=%d\n",
       myrank_toroidal,nloc_calc_moments,igrid_nover_in,igrid_moments_in);

  real timing_b_inter=MPI_Wtime();
  //real sum_total = 0.0;
#pragma omp parallel
  {
    real psitmp, thetatmp, zetatmp, weight, r, b, upara, vperp2,
      energy, P_para, P_perp, q_para, q_perp, dz1, dz0, wz1, wz0, wzt, 
      rdum, wp1, wp0, tdum, tdum2, tdumtmp, tdumtmp2, wt0, wt10, wt00, wt1, wt11, wt01;  
    int i, j, m, kk, ii, im, im2, j00, jt0, j01, jt1, ij, ij1;
    //int ip, iptmp, jt, jttmp, ipjt;
    int tid, nthreads;
    real d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12, d13,
      d14, d15, d16, d17, d18, d19, d20, d21, d22, d23, d24, d25, d26, d27;
    real *moments_part;
    real sum_tid=0.0;

#ifdef _OPENMP
    tid = omp_get_thread_num();
    nthreads = omp_get_num_threads();
#else
    tid = 0;
    nthreads = 1;
#endif

    moments_part = moments + tid*7*(mzeta+1)*nloc_calc_moments;

    for (i=0; i<7*(mzeta+1)*nloc_calc_moments; i++){
      moments_part[i] = 0.0;
    }

#pragma omp for 
    for (m=0; m<mi; m++){
      psitmp = z0[m];
      thetatmp = z1[m];
      zetatmp = z2[m];
      weight  = z4[m];

      if (zetatmp == HOLEVAL) {
	continue;
      }

      //sum_tid += weight;

#if SQRT_PRECOMPUTED
      r        = psitmp;
#else
      r        = sqrt(2.0*psitmp);
#endif
      b = 1.0/(1.0 + r*cos(thetatmp));
      upara = z3[m]*b*qion*aion_inv*vthi_inv;
      vperp2 = 2.0*aion_inv*b*z5[m]*z5[m]*vthi_inv*vthi_inv;
      energy = max(1.0e-20, 0.5*upara*upara+0.5*vperp2);
      P_para = max(1.0e-20, 0.5*upara*upara);
      P_perp = max(1.0e-20, 0.5*vperp2);
      q_para = upara*P_para;
      q_perp = upara*P_perp;

      
      //iptmp    = (int) ((r-a0)*delr+0.5);
      //ip       = abs_min_int(mpsi, iptmp);
      //assert(ip>=ipsi_valid_in);
      //assert(ip<=ipsi_valid_out);

      //jttmp    = (int) (thetatmp*pi2_inv*delt[ip]+0.5);
      //jt       = abs_min_int(mtheta[ip], jttmp);

      //ipjt     = igrid[ip]+jt-igrid_moments_in;

      wzt      = (zetatmp-zetamin)*delz;
      kk       = abs_min_int(mzeta-1, (int) wzt);
      //assert(kk==0);
      dz1      = wzt - (real)kk;
      dz0      = 1.0 - dz1;
      //assert(dz1>=0.0);
      //assert(dz1<=1.0);
      wz1      = weight*dz1;
      wz0      = weight*dz0;

      rdum = delr * abs_min_real(a_diff, r-a0);
      ii = abs_min_int(mpsi_max, (int)rdum);
      wp1 = rdum - (real)ii;
      wp0 = 1.0 - wp1;
      //assert(wp1>=0.0);
      //assert(wp1<=1.0);

      // inner/outer flux surface
      im = ii;
      im2 = ii+1;

      tdumtmp = pi2_inv * (thetatmp - zetatmp * qtinv[im]) + 10.0;
      tdumtmp2 = pi2_inv * (thetatmp - zetatmp * qtinv[im2]) + 10.0;

      tdum = (tdumtmp - (int) tdumtmp) * delt[im];
      tdum2 = (tdumtmp2 - (int) tdumtmp2) * delt[im2];

      j00 = abs_min_int(mtheta[im]-1, (int) tdum);
      j01 = abs_min_int(mtheta[im2]-1, (int) tdum2);

      jt0 = igrid[im] + j00 - igrid_moments_in;
      jt1 = igrid[im2] + j01 - igrid_moments_in;
      //assert(jt0>=0);
      //assert(jt0+1<nloc_calc_moments);
      //assert(jt1>=0);
      //assert(jt1+1<nloc_calc_moments);

      wt0 = tdum - (real)j00;
      wt10 = wp0*wt0;
      wt00 = wp0 - wt10;
      //assert(wt0>=0.0);
      //assert(wt0<=1.0);

      wt1 = tdum2 - (real)j01;
      wt11 = wp1*wt1;
      wt01 = wp1 - wt11;
      //assert(wt1>=0.0);
      //assert(wt1<=1.0);

      ij = jt0;
      d0 = moments_part[ij*(mzeta+1)*7 + kk*7 + 0];
      d1 = moments_part[ij*(mzeta+1)*7 + kk*7 + 1];
      d2 = moments_part[ij*(mzeta+1)*7 + kk*7 + 2];
      d3 = moments_part[ij*(mzeta+1)*7 + kk*7 + 3];
      d4 = moments_part[ij*(mzeta+1)*7 + kk*7 + 4];
      d5 = moments_part[ij*(mzeta+1)*7 + kk*7 + 5];
      d6 = moments_part[ij*(mzeta+1)*7 + kk*7 + 6];
      
      d7 = moments_part[ij*(mzeta+1)*7 + (kk+1)*7 + 0];
      d8 = moments_part[ij*(mzeta+1)*7 + (kk+1)*7 + 1];
      d9 = moments_part[ij*(mzeta+1)*7 + (kk+1)*7 + 2];
      d10 = moments_part[ij*(mzeta+1)*7 + (kk+1)*7 + 3];
      d11 = moments_part[ij*(mzeta+1)*7 + (kk+1)*7 + 4];
      d12 = moments_part[ij*(mzeta+1)*7 + (kk+1)*7 + 5];
      d13 = moments_part[ij*(mzeta+1)*7 + (kk+1)*7 + 6];

      d14 = moments_part[(ij+1)*(mzeta+1)*7 + kk*7 + 0];
      d15 = moments_part[(ij+1)*(mzeta+1)*7 + kk*7 + 1];
      d16 = moments_part[(ij+1)*(mzeta+1)*7 + kk*7 + 2];
      d17 = moments_part[(ij+1)*(mzeta+1)*7 + kk*7 + 3];
      d18 = moments_part[(ij+1)*(mzeta+1)*7 + kk*7 + 4];
      d19 = moments_part[(ij+1)*(mzeta+1)*7 + kk*7 + 5];
      d20 = moments_part[(ij+1)*(mzeta+1)*7 + kk*7 + 6];

      d21 = moments_part[(ij+1)*(mzeta+1)*7 + (kk+1)*7 + 0];
      d22 = moments_part[(ij+1)*(mzeta+1)*7 + (kk+1)*7 + 1];
      d23 = moments_part[(ij+1)*(mzeta+1)*7 + (kk+1)*7 + 2];
      d24 = moments_part[(ij+1)*(mzeta+1)*7 + (kk+1)*7 + 3];
      d25 = moments_part[(ij+1)*(mzeta+1)*7 + (kk+1)*7 + 4];
      d26 = moments_part[(ij+1)*(mzeta+1)*7 + (kk+1)*7 + 5];
      d27 = moments_part[(ij+1)*(mzeta+1)*7 + (kk+1)*7 + 6];

      d0 += wz0*wt00;
      d1 += wz0*wt00*upara;
      d2 += wz0*wt00*P_para;
      d3 += wz0*wt00*P_perp;
      d4 += wz0*wt00*q_para;
      d5 += wz0*wt00*q_perp;
      d6 += dz0*wt00;

      d7 += wz1*wt00;
      d8 += wz1*wt00*upara;
      d9 += wz1*wt00*P_para;
      d10 += wz1*wt00*P_perp;
      d11 += wz1*wt00*q_para;
      d12 += wz1*wt00*q_perp;
      d13 += dz1*wt00;

      d14 += wz0*wt10;
      d15 += wz0*wt10*upara;
      d16 += wz0*wt10*P_para;
      d17 += wz0*wt10*P_perp;
      d18 += wz0*wt10*q_para;
      d19 += wz0*wt10*q_perp;
      d20 += dz0*wt10;

      d21 += wz1*wt10;
      d22 += wz1*wt10*upara;
      d23 += wz1*wt10*P_para;
      d24 += wz1*wt10*P_perp;
      d25 += wz1*wt10*q_para;
      d26 += wz1*wt10*q_perp;
      d27 += dz1*wt10;

      moments_part[ij*(mzeta+1)*7 + kk*7 + 0] = d0; 
      moments_part[ij*(mzeta+1)*7 + kk*7 + 1] = d1;
      moments_part[ij*(mzeta+1)*7 + kk*7 + 2] = d2;
      moments_part[ij*(mzeta+1)*7 + kk*7 + 3] = d3;
      moments_part[ij*(mzeta+1)*7 + kk*7 + 4] = d4;
      moments_part[ij*(mzeta+1)*7 + kk*7 + 5] = d5;
      moments_part[ij*(mzeta+1)*7 + kk*7 + 6] = d6;

      moments_part[ij*(mzeta+1)*7 + (kk+1)*7 + 0] = d7;
      moments_part[ij*(mzeta+1)*7 + (kk+1)*7 + 1] = d8;
      moments_part[ij*(mzeta+1)*7 + (kk+1)*7 + 2] = d9;
      moments_part[ij*(mzeta+1)*7 + (kk+1)*7 + 3] = d10;
      moments_part[ij*(mzeta+1)*7 + (kk+1)*7 + 4] = d11;
      moments_part[ij*(mzeta+1)*7 + (kk+1)*7 + 5] = d12;
      moments_part[ij*(mzeta+1)*7 + (kk+1)*7 + 6] = d13;

      moments_part[(ij+1)*(mzeta+1)*7 + kk*7 +0] = d14;
      moments_part[(ij+1)*(mzeta+1)*7 + kk*7 + 1] = d15;
      moments_part[(ij+1)*(mzeta+1)*7 + kk*7 + 2] = d16;
      moments_part[(ij+1)*(mzeta+1)*7 + kk*7 + 3] = d17;
      moments_part[(ij+1)*(mzeta+1)*7 + kk*7 + 4] = d18;
      moments_part[(ij+1)*(mzeta+1)*7 + kk*7 + 5] = d19;
      moments_part[(ij+1)*(mzeta+1)*7 + kk*7 + 6] = d20;

      moments_part[(ij+1)*(mzeta+1)*7 + (kk+1)*7 + 0] = d21; 
      moments_part[(ij+1)*(mzeta+1)*7 + (kk+1)*7 + 1] = d22;
      moments_part[(ij+1)*(mzeta+1)*7 + (kk+1)*7 + 2] = d23;
      moments_part[(ij+1)*(mzeta+1)*7 + (kk+1)*7 + 3] = d24;
      moments_part[(ij+1)*(mzeta+1)*7 + (kk+1)*7 + 4] = d25;
      moments_part[(ij+1)*(mzeta+1)*7 + (kk+1)*7 + 5] = d26;
      moments_part[(ij+1)*(mzeta+1)*7 + (kk+1)*7 + 6] = d27;

      ij1 = jt1;
      d0 = moments_part[ij1*(mzeta+1)*7 + kk*7 + 0];
      d1 = moments_part[ij1*(mzeta+1)*7 + kk*7 + 1];
      d2 = moments_part[ij1*(mzeta+1)*7 + kk*7 + 2];
      d3 = moments_part[ij1*(mzeta+1)*7 + kk*7 + 3];
      d4 = moments_part[ij1*(mzeta+1)*7 + kk*7 + 4];
      d5 = moments_part[ij1*(mzeta+1)*7 + kk*7 + 5];
      d6 = moments_part[ij1*(mzeta+1)*7 + kk*7 + 6];

      d7 = moments_part[ij1*(mzeta+1)*7 + (kk+1)*7 + 0];
      d8 = moments_part[ij1*(mzeta+1)*7 + (kk+1)*7 + 1];
      d9 = moments_part[ij1*(mzeta+1)*7 + (kk+1)*7 + 2];
      d10 = moments_part[ij1*(mzeta+1)*7 + (kk+1)*7 + 3];
      d11 = moments_part[ij1*(mzeta+1)*7 + (kk+1)*7 + 4];
      d12 = moments_part[ij1*(mzeta+1)*7 + (kk+1)*7 + 5];
      d13 = moments_part[ij1*(mzeta+1)*7 + (kk+1)*7 + 6];

      d14 = moments_part[(ij1+1)*(mzeta+1)*7 + kk*7 + 0];
      d15 = moments_part[(ij1+1)*(mzeta+1)*7 + kk*7 + 1];
      d16 = moments_part[(ij1+1)*(mzeta+1)*7 + kk*7 + 2];
      d17 = moments_part[(ij1+1)*(mzeta+1)*7 + kk*7 + 3];
      d18 = moments_part[(ij1+1)*(mzeta+1)*7 + kk*7 + 4];
      d19 = moments_part[(ij1+1)*(mzeta+1)*7 + kk*7 + 5];
      d20 = moments_part[(ij1+1)*(mzeta+1)*7 + kk*7 + 6];

      d21 = moments_part[(ij1+1)*(mzeta+1)*7 + (kk+1)*7 + 0];
      d22 = moments_part[(ij1+1)*(mzeta+1)*7 + (kk+1)*7 + 1];
      d23 = moments_part[(ij1+1)*(mzeta+1)*7 + (kk+1)*7 + 2];
      d24 = moments_part[(ij1+1)*(mzeta+1)*7 + (kk+1)*7 + 3];
      d25 = moments_part[(ij1+1)*(mzeta+1)*7 + (kk+1)*7 + 4];
      d26 = moments_part[(ij1+1)*(mzeta+1)*7 + (kk+1)*7 + 5];
      d27 = moments_part[(ij1+1)*(mzeta+1)*7 + (kk+1)*7 + 6];

      d0 += wz0*wt01;
      d1 += wz0*wt01*upara;
      d2 += wz0*wt01*P_para;
      d3 += wz0*wt01*P_perp;
      d4 += wz0*wt01*q_para;
      d5 += wz0*wt01*q_perp;
      d6 += dz0*wt01;

      d7 += wz1*wt01;
      d8 += wz1*wt01*upara;
      d9 += wz1*wt01*P_para;
      d10 += wz1*wt01*P_perp;
      d11 += wz1*wt01*q_para;
      d12 += wz1*wt01*q_perp;
      d13 += dz1*wt01;

      d14 += wz0*wt11;
      d15 += wz0*wt11*upara;
      d16 += wz0*wt11*P_para;
      d17 += wz0*wt11*P_perp;
      d18 += wz0*wt11*q_para;
      d19 += wz0*wt11*q_perp;
      d20 += dz0*wt11;

      d21 += wz1*wt11;
      d22 += wz1*wt11*upara;
      d23 += wz1*wt11*P_para;
      d24 += wz1*wt11*P_perp;
      d25 += wz1*wt11*q_para;
      d26 += wz1*wt11*q_perp;
      d27 += dz1*wt11;

      moments_part[ij1*(mzeta+1)*7 + kk*7 + 0] = d0;
      moments_part[ij1*(mzeta+1)*7 + kk*7 + 1] = d1;
      moments_part[ij1*(mzeta+1)*7 + kk*7 + 2] = d2;
      moments_part[ij1*(mzeta+1)*7 + kk*7 + 3] = d3;
      moments_part[ij1*(mzeta+1)*7 + kk*7 + 4] = d4;
      moments_part[ij1*(mzeta+1)*7 + kk*7 + 5] = d5;
      moments_part[ij1*(mzeta+1)*7 + kk*7 + 6] = d6;

      moments_part[ij1*(mzeta+1)*7 + (kk+1)*7 + 0] = d7;
      moments_part[ij1*(mzeta+1)*7 + (kk+1)*7 + 1] = d8;
      moments_part[ij1*(mzeta+1)*7 + (kk+1)*7 + 2] = d9;
      moments_part[ij1*(mzeta+1)*7 + (kk+1)*7 + 3] = d10;
      moments_part[ij1*(mzeta+1)*7 + (kk+1)*7 + 4] = d11;
      moments_part[ij1*(mzeta+1)*7 + (kk+1)*7 + 5] = d12;
      moments_part[ij1*(mzeta+1)*7 + (kk+1)*7 + 6] = d13;

      moments_part[(ij1+1)*(mzeta+1)*7 + kk*7 +0] = d14;
      moments_part[(ij1+1)*(mzeta+1)*7 + kk*7 + 1] = d15;
      moments_part[(ij1+1)*(mzeta+1)*7 + kk*7 + 2] = d16;
      moments_part[(ij1+1)*(mzeta+1)*7 + kk*7 + 3] = d17;
      moments_part[(ij1+1)*(mzeta+1)*7 + kk*7 + 4] = d18;
      moments_part[(ij1+1)*(mzeta+1)*7 + kk*7 + 5] = d19;
      moments_part[(ij1+1)*(mzeta+1)*7 + kk*7 + 6] = d20;

      moments_part[(ij1+1)*(mzeta+1)*7 + (kk+1)*7 + 0] = d21;
      moments_part[(ij1+1)*(mzeta+1)*7 + (kk+1)*7 + 1] = d22;
      moments_part[(ij1+1)*(mzeta+1)*7 + (kk+1)*7 + 2] = d23;
      moments_part[(ij1+1)*(mzeta+1)*7 + (kk+1)*7 + 3] = d24;
      moments_part[(ij1+1)*(mzeta+1)*7 + (kk+1)*7 + 4] = d25;
      moments_part[(ij1+1)*(mzeta+1)*7 + (kk+1)*7 + 5] = d26;
      moments_part[(ij1+1)*(mzeta+1)*7 + (kk+1)*7 + 6] = d27;
    }

#pragma omp barrier

    //#pragma omp atomic
    //sum_total += sum_tid;

#pragma omp for 
    for (i=0; i<(mzeta+1)*7*nloc_calc_moments; i++){
      real momloc_sum = 0.0;
      for (j=1; j<nthreads; j++){
	momloc_sum += moments[j*(mzeta+1)*7*nloc_calc_moments+i];
      }
      moments[i] += momloc_sum;
    }
  }
  /*
  real sum_moments = 0.0;
  for (int i=0; i<nloc_calc_moments; i++){
    for (int j=0; j<mzeta+1; j++){
      sum_moments += moments[(mzeta+1)*7*i+j*7];
    }
  }
  if (parallel_decomp->mype==0) printf("sum_total=%e sum_moments=%e\n", sum_total, sum_moments);

  real sum_mpi = 0.0;
  MPI_Allreduce(&sum_moments, &sum_mpi, 1, MPI_MYREAL, MPI_SUM, MPI_COMM_WORLD);
  */

#if USE_MPI
  /* sum radial particles */
  if (radial_decomp->nproc_radial_partd > 1){
#pragma omp parallel for
    for (int i=0; i<(mzeta+1)*7*nloc_calc_moments; i++){
      momentstmp[i] = moments[i];
      moments[i] = 0.0;
    }
    MPI_Allreduce(momentstmp, moments, (mzeta+1)*7*nloc_calc_moments,
		  MPI_MYWREAL, MPI_SUM,
		  radial_decomp->radial_partd_comm);
  }
  /* sum ghost cell*/
  sum_plane_moments(gtc_input);
#endif

  /* Poloidal end cell, discard ghost cell j = 0 */
#pragma omp parallel for
  for (int i=ipsi_moments_in; i<ipsi_moments_out+1; i++) {
    for (int j=0; j<mzeta+1; j++) {
      for (int k=0; k<7; k++){
	moments[(igrid[i] + mtheta[i] - igrid_moments_in)*(mzeta+1)*7 + j*7 + k]
	+= moments[(igrid[i] - igrid_moments_in)*(mzeta+1)*7 + j*7 + k];

	moments[(igrid[i] - igrid_moments_in)*(mzeta+1)*7 + j*7 + k] = 
	  moments[(igrid[i] + mtheta[i] - igrid_moments_in)*(mzeta+1)*7 + j*7 + k];
      }
    }
  }

#if USE_MPI
#pragma omp parallel for
  for (int i=0; i<nloc_calc_moments; i++) {
    for (int k=0; k<7; k++){
      sendl_moments[7*i+k] = moments[(mzeta+1)*7*i + k];
      recvr_moments[7*i+k] = 0.0;
    }
  }

  icount = 7*nloc_calc_moments;
  idest = parallel_decomp->left_pe;
  isource = parallel_decomp->right_pe;
  isendtag = parallel_decomp->myrank_toroidal;
  irecvtag = isource;

  MPI_Sendrecv(sendl_moments, icount, MPI_MYWREAL, idest, isendtag,
	       recvr_moments, icount, MPI_MYWREAL,
	       isource, irecvtag, parallel_decomp->toroidal_comm, &istatus);
#else
#pragma omp parallel for
  for (int i=0; i<nloc_calc_moments; i++) {
    for (int k=0; k<7; k++){
      recvr_moments[7*i+k] = moments[(mzeta+1)*7*i + k];
    }
  }

#endif

#pragma omp parallel for
  for (int i=0; i<nloc_calc_moments; i++) {
    for (int k=0; k<7; k++){
      moments[(mzeta+1)*7*i + mzeta*7 + k] +=recvr_moments[7*i+k];
    }
  }

  /*
  real sum_final=0.0;
  for (int i=ipsi_nover_in; i<ipsi_nover_out+1; i++){
    for (int j=1; j<mtheta[i]+1; j++){
      for (int k=1; k<mzeta+1; k++){
	sum_final += moments[(mzeta+1)*7*(igrid[i]+j-igrid_moments_in)+k*7];
      }
    }
  }
  real sum_final_mpi=0.0;
  MPI_Allreduce(&sum_final, &sum_final_mpi, 1, MPI_MYREAL, MPI_SUM, MPI_COMM_WORLD);
  if (parallel_decomp->mype==0) printf("sum_mpi=%e sum_final_mpi=%e\n", sum_mpi, sum_final_mpi);
  */

  int offset = igrid_moments_in - igrid_in;
  assert(offset>=0);
#pragma omp parallel for
  for (int i=0; i<nloc_calc_moments; i++) {
    for (int j=1; j<mzeta+1; j++ ){
      //for (int k=0; k<6; k++){
      //	moments[(mzeta+1)*7*i + 7*j + k] /= moments[(mzeta+1)*7*i + 7*j + 6];
      //}
      real marker = markeri[mzeta*(i+offset) + j-1];
      for (int k=0; k<7; k++){
	moments[(mzeta+1)*7*i + 7*j + k] *= marker;
      }
    }
  }

  // convert row-major (C format) multidimensional array to column-major (Fortran format) for output
  // store them in momentstmp
#pragma omp parallel for
  for (int i=0; i<nloc_calc_moments; i++){
    for (int k=0; k<7; k++){
      momentstmp[7*i+k] = moments[(mzeta+1)*7*i + 7*mzeta + k];
    }
  }
  /*
  if (parallel_decomp->mype==59) {                                                                                                                    
    char filename[50];                                                                                                                               
    sprintf(filename, "moment0.txt");                                                                                                               
    FILE *fp;                                                                                                                                        
    fp = fopen(filename, "w"); 
    for (int i=0; i<nloc_calc_moments; i++){
      for (int k=0; k<6; k++){
	fprintf(fp, "%d %d %e\n", i, k,  momentstmp[7*i+k]);
      }
    }                                                       
    fclose(fp);  
  }
  */

  real *phitmp = momentstmp + 7*nloc_calc_moments;
  real *densityitmp = momentstmp + 8*nloc_calc_moments;
#pragma omp parallel for
  for (int i=ipsi_moments_in; i<ipsi_moments_out+1; i++){
    for (int j=0; j<mtheta[i]; j++){
      int ij = igrid[i] + j - igrid_in;
      int ij2 = igrid[i] +j - igrid_moments_in;
      phitmp[ij2] = phi[ij*(mzeta+1) + mzeta]/(gyroradius*gyroradius);
      densityitmp[ij2] = densityi[ij*(mzeta+1) + mzeta];
    }
  }


#pragma omp parallel for
  for (int i=0; i<mpsi+1; i++){
    phip00[i] = phi00[i]/(gyroradius*gyroradius);
  }

  real timing_e_inter = MPI_Wtime() - timing_b_inter;
  real timing_inter_min, timing_inter_max;
  MPI_Allreduce(&timing_e_inter, &timing_inter_max, 1, MPI_MYREAL, MPI_MAX, parallel_decomp->partd_comm);
  MPI_Allreduce(&timing_e_inter, &timing_inter_min, 1, MPI_MYREAL, MPI_MIN, parallel_decomp->partd_comm);

  // in parallel enviorment where nproc_radiald > 1, all processes with the same myrank_toroidald write to the same file
  real timing_b = MPI_Wtime();
  
  /* moments output */
#if USE_ADIOS
  //fprintf(stderr,"name of ADIOS file, iste=%d\n", istep);
  sprintf(cdum, "MOMENTS_%5.5d.bp", istep);
  comm = MPI_COMM_WORLD;
  adios_open(&adios_handle, "moments", cdum, "w", comm);
  #include "gwrite_moments.ch"
  adios_close(adios_handle);
#else
  if (parallel_decomp->myrank_toroidal < 10) {
    sprintf(cdum, "MOMENTS/MOMENTS.0%d", parallel_decomp->myrank_toroidal);
    sprintf(cdum1, "PHIDEN/PHIDEN.0%d", parallel_decomp->myrank_toroidal);
  }
  else if (parallel_decomp->myrank_toroidal < 100){
    sprintf(cdum, "MOMENTS/MOMENTS.%d", parallel_decomp->myrank_toroidal);
    sprintf(cdum1, "PHIDEN/PHIDEN.%d", parallel_decomp->myrank_toroidal);
  }
  else {
    printf("no moments output file\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  
  off_t offset_all = (mstepall+istep)/mmomentsoutput*(4*sizeof(int)+2*sizeof(real)+sizeof(real)*7*mgrid);
  real time=(real)(mstepall+istep)*tstep;
  
  int file,rc;
  if (params->irun==0) {
    file = open64(cdum, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IXUSR);
  }
  else if (params->irun==1){
    file = open64(cdum, O_WRONLY);
  }
  else {
    printf(" other irun is not available\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  if (file==-1) {
    perror("open() for create failed\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  
  if (radial_decomp->myrank_radiald==0&&radial_decomp->myrank_radial_partd==0){
    int nbyte = 2*sizeof(real);
    int nbyte1 = sizeof(real)*7*mgrid;
    if (-1 == (rc=pwrite64(file, &nbyte, sizeof(int), offset_all))){
      perror("pwrite failed");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if (-1 == (rc=pwrite64(file, &time, sizeof(real), offset_all+(sizeof(int))))){
      perror("pwrite failed");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if (-1 == (rc=pwrite64(file, &zetamax, sizeof(real), offset_all+(sizeof(int)+sizeof(real))))){
	perror("pwrite failed");
	MPI_Abort(MPI_COMM_WORLD, 1);
      }
    if (-1 == (rc=pwrite64(file, &nbyte, sizeof(int), offset_all+(sizeof(int)+2*sizeof(real))))){
      perror("pwrite failed");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }

    if (-1 == (rc=pwrite64(file, &nbyte1, sizeof(int), offset_all+(2*sizeof(int)+2*sizeof(real))))){
      perror("pwrite failed");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if (-1 == (rc=pwrite64(file, momentstmp+7*(igrid_nover_in-igrid_moments_in), sizeof(real)*7*nloc_nover, offset_all+(3*sizeof(int)+2*sizeof(real))))){
      perror("pwrite failed");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
  }
  else if (radial_decomp->myrank_radiald>0&&radial_decomp->myrank_radial_partd==0){
    int nbyte1 = sizeof(real)*7*mgrid;
    off_t offset= offset_all + (3*sizeof(int)+2*sizeof(real)+sizeof(real)*7*(igrid_nover_in-1));
    off_t offset1= offset_all + (3*sizeof(int)+2*sizeof(real)+sizeof(real)*7*mgrid);
 
    if (-1 == (rc=pwrite64(file, momentstmp+7*(igrid_nover_in-igrid_moments_in), sizeof(real)*7*nloc_nover, offset))){
      perror("pwrite failed");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }

    if (radial_decomp->myrank_radiald == radial_decomp-> nproc_radiald-1)
      if (-1 == (rc=pwrite64(file, &nbyte1, sizeof(int), offset1))){
        perror("pwrite failed");
        MPI_Abort(MPI_COMM_WORLD, 1);
      }
  }
 
  if (0 != (rc= close(file))){
      perror("close failed");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
  
  /*
  // test the correctness of the output
  int file_r = open64(cdum, O_RDONLY);
  int rc_r;
  if (file_r == -1){
    perror("open() for cerate failed\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  if (radial_decomp->myrank_radiald==0&&radial_decomp->myrank_radial_partd==0){
    real time_r,zetamax_r;
    int nbyte, nbyte1;
    if (-1 == (rc_r=pread64(file_r, &nbyte, sizeof(int), offset_all))){
      perror("pread failed");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
    assert(nbyte==2*sizeof(real));
    if (-1 == (rc_r=pread64(file_r, &time_r, sizeof(real), offset_all+(sizeof(int))))){
      perror("pread failed");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
    assert(time_r==time);
    if (-1 == (rc_r=pread64(file_r, &zetamax_r, sizeof(real), offset_all+(sizeof(int)+sizeof(real))))){
      perror("pread failed");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
    assert(zetamax_r == zetamax);
    if (-1 == (rc_r=pread64(file_r, &nbyte, sizeof(int), offset_all+(sizeof(int)+2*sizeof(real))))){
      perror("pread failed");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
    assert(nbyte==2*sizeof(real));

    if (-1 == (rc_r=pread64(file_r, &nbyte1, sizeof(int), offset_all+(2*sizeof(int)+2*sizeof(real))))){
      perror("pwrite failed");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
    assert(nbyte1 == sizeof(real)*7*mgrid);
 
    if (-1 == (rc_r=pread64(file_r, momentstmp+7*(igrid_nover_in-igrid_moments_in), sizeof(real)*7*nloc_nover, (3*sizeof(int)+2*sizeof(real))))){
      perror("pwrite failed");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    //if (parallel_decomp->mype==0)
    //  printf("nbyte1=%d nyte=%d zetamax=%e time=%e\n", nbyte1, nbyte, zetamax, time);
  }
  else if (radial_decomp->myrank_radiald>0&&radial_decomp->myrank_radial_partd==0){
    int nbyte1;
    off_t offset=offset_all + (3*sizeof(int)+2*sizeof(real)+sizeof(real)*7*(igrid_nover_in-1));
    off_t offset1=offset_all + (3*sizeof(int)+2*sizeof(real)+sizeof(real)*7*mgrid);

    if (-1 == (rc_r=pread64(file_r, momentstmp+7*(igrid_nover_in-igrid_moments_in), sizeof(real)*7*nloc_nover, offset))){
      perror("pread failed");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }

    if (radial_decomp->myrank_radiald == radial_decomp-> nproc_radiald-1){
      if (-1 == (rc_r=pread64(file_r, &nbyte1, sizeof(int), offset1))){
        perror("pread failed");
        MPI_Abort(MPI_COMM_WORLD, 1);
      }
      assert(nbyte1 == sizeof(real)*7*mgrid);
      //printf("mype=%d nbyte1=%d\n",parallel_decomp->mype, nbyte1);
    }
  }
  if (0 != (rc_r= close(file_r))){
    perror("close failed");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  */

  /*
  if (parallel_decomp->mype==59) {
    char filename[50];
    sprintf(filename, "moment1.txt");
    FILE *fp;
    fp = fopen(filename, "w");
    for (int i=0; i<nloc_calc_moments; i++){
      for (int k=0; k<6; k++){
	fprintf(fp, "%d %d %e\n", i, k, momentstmp[7*i+k]);
      }
    }
    fclose(fp);
  }
  */

  off_t offset_all1 = (mstepall+istep)/mmomentsoutput*(15*sizeof(int)+sizeof(real)*(mpsi+3+mpsi+1+2*mgrid));
  
  int file1,rc1;
  if (params->irun==0) {
    file1 = open64(cdum1, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IXUSR);
  }
  else if (params->irun==1){
    file1 = open64(cdum1, O_WRONLY);
  }
  else {
    printf(" other irun is not available\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  if (file1==-1) {
    perror("open() for cerate failed\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  if (radial_decomp->myrank_radiald==0&&radial_decomp->myrank_radial_partd==(radial_decomp->nproc_radial_partd)-1){
    int mstepall1 = mstepall+istep;
    int nbyte = 2*sizeof(real);
    int nbyte1 = 3*sizeof(int);
    int nbyte2 = sizeof(real)*(mpsi+1);
    int nbyte3 = sizeof(real)*mgrid;
    pwrite64(file1, &nbyte, sizeof(int), offset_all1);
    pwrite64(file1, &time, sizeof(real), offset_all1+(sizeof(int)));
    pwrite64(file1, &zetamax, sizeof(real), offset_all1+(sizeof(int)+sizeof(real)));
    pwrite64(file1, &nbyte, sizeof(int), offset_all1+(sizeof(int)+2*sizeof(real)));

    pwrite64(file1, &nbyte1, sizeof(int), offset_all1+(2*sizeof(int)+2*sizeof(real)));
    pwrite64(file1, &mstepall1, sizeof(int), offset_all1+(3*sizeof(int)+2*sizeof(real)));
    pwrite64(file1, &mpsi, sizeof(int), offset_all1+(4*sizeof(int)+2*sizeof(real)));
    pwrite64(file1, &mgrid, sizeof(int), offset_all1+(5*sizeof(int)+2*sizeof(real)));
    pwrite64(file1, &nbyte1, sizeof(int), offset_all1+(6*sizeof(int)+2*sizeof(real)));

    pwrite64(file1, &nbyte2, sizeof(int), offset_all1+(7*sizeof(int)+2*sizeof(real)));
    pwrite64(file1, phip00+ipsi_nover_in, sizeof(real)*(ipsi_nover_out-ipsi_nover_in+1), offset_all1+(8*sizeof(int)+2*sizeof(real))); 

    pwrite64(file1, &nbyte3, sizeof(int), offset_all1+(9*sizeof(int)+sizeof(real)*(mpsi+3)));
    pwrite64(file1, phitmp+(igrid_nover_in-igrid_moments_in), sizeof(real)*nloc_nover, offset_all1+(10*sizeof(int)+sizeof(real)*(mpsi+3)));

    pwrite64(file1, &nbyte3, sizeof(int), offset_all1+(11*sizeof(int)+sizeof(real)*(mpsi+3+mgrid)));
    pwrite64(file1, densityitmp+(igrid_nover_in-igrid_moments_in), sizeof(real)*nloc_nover, offset_all1+(12*sizeof(int)+sizeof(real)*(mpsi+3+mgrid)));
    
    pwrite64(file1, &nbyte2, sizeof(int), offset_all1+(13*sizeof(int)+sizeof(real)*(mpsi+3+2*mgrid)));
    pwrite64(file1, zonali+ipsi_nover_in, sizeof(real)*(ipsi_nover_out-ipsi_nover_in+1), offset_all1+(14*sizeof(int)+sizeof(real)*(mpsi+3+2*mgrid)));

  } else if (radial_decomp->myrank_radiald>0&&radial_decomp->myrank_radial_partd==radial_decomp->nproc_radial_partd-1){
    int nbyte2 = sizeof(real)*(mpsi+1);
    int nbyte3 = sizeof(real)*mgrid;

    off_t offset = offset_all1+(10*sizeof(int)+sizeof(real)*(mpsi+3+igrid_nover_in-1));
    off_t offset0 = offset_all1+(10*sizeof(int)+sizeof(real)*(mpsi+3+mgrid));

    off_t offset1 = offset_all1+(12*sizeof(int)+sizeof(real)*(mpsi+3+mgrid+igrid_nover_in-1));
    off_t offset2 = offset_all1+(12*sizeof(int)+sizeof(real)*(mpsi+3+2*mgrid));

    pwrite64(file1, phip00+ipsi_nover_in, sizeof(real)*(ipsi_nover_out-ipsi_nover_in+1), offset_all1+(8*sizeof(int)+sizeof(real)*(ipsi_nover_in+2)));
    if (radial_decomp->myrank_radiald == radial_decomp->nproc_radiald -1)
      pwrite64(file1, &nbyte2, sizeof(int), offset_all1+(8*sizeof(int)+sizeof(real)*(mpsi+3)));

    pwrite64(file1, phitmp+(igrid_nover_in-igrid_moments_in), sizeof(real)*nloc_nover, offset);
    if (radial_decomp->myrank_radiald == radial_decomp->nproc_radiald -1)
      pwrite64(file1, &nbyte3, sizeof(int), offset0);

    pwrite64(file1, densityitmp+(igrid_nover_in-igrid_moments_in), sizeof(real)*nloc_nover, offset1);
    if (radial_decomp->myrank_radiald == radial_decomp->nproc_radiald -1)
      pwrite64(file1, &nbyte3, sizeof(int), offset2);

    pwrite64(file1, zonali+ipsi_nover_in, sizeof(real)*(ipsi_nover_out-ipsi_nover_in+1), offset_all1+(14*sizeof(int)+sizeof(real)*(mpsi+3+2*mgrid+ipsi_nover_in)));
    if (radial_decomp->myrank_radiald == radial_decomp->nproc_radiald -1)
      pwrite64(file1, &nbyte2, sizeof(int), offset_all1+(14*sizeof(int)+sizeof(real)*(mpsi+3+mpsi+1+2*mgrid)));
  }

  if (0 != (rc1= close(file1))){
    perror("close failed");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  
  /*
  // test the output
  int file1_r = open64(cdum1, O_RDONLY);
  int rc1_r;
  if (file1_r==-1) {
  perror("open() for cerate failed\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  real *zonali_r = (real *) malloc(sizeof(real)*(mpsi+1));
  if (radial_decomp->myrank_radiald==0&&radial_decomp->myrank_radial_partd==radial_decomp->nproc_radial_partd-1){
    int mstepall1 = mstepall+istep;
    int nbyte, nbyte1, nbyte2, nbyte3, mstepall1_r, mpsi_r, mgrid_r;
    real time_r, zetamax_r;
    pread64(file1_r, &nbyte, sizeof(int), offset_all1);
    pread64(file1_r, &time_r, sizeof(real), offset_all1+(sizeof(int)));
    pread64(file1_r, &zetamax_r, sizeof(real), offset_all1+(sizeof(int)+sizeof(real)));
    pread64(file1_r, &nbyte, sizeof(int), offset_all1+(sizeof(int)+2*sizeof(real)));
    assert(nbyte==2*sizeof(real));
    assert(time_r == time);
    assert(zetamax_r == zetamax);
    
    pread64(file1_r, &nbyte1, sizeof(int), offset_all1+(2*sizeof(int)+2*sizeof(real)));
    pread64(file1_r, &mstepall1_r, sizeof(int), offset_all1+(3*sizeof(int)+2*sizeof(real)));
    pread64(file1_r, &mpsi_r, sizeof(int), offset_all1+(4*sizeof(int)+2*sizeof(real)));
    pread64(file1_r, &mgrid_r, sizeof(int), offset_all1+(5*sizeof(int)+2*sizeof(real)));
    pread64(file1_r, &nbyte1, sizeof(int), offset_all1+(6*sizeof(int)+2*sizeof(real)));
    assert(nbyte1==3*sizeof(int));
    assert(mstepall1_r == mstepall1);
    assert(mpsi_r == mpsi);
    assert(mgrid_r == mgrid);
    
    pread64(file1_r, &nbyte2, sizeof(int), offset_all1+(7*sizeof(int)+2*sizeof(real)));
    pread64(file1_r, phip00+ipsi_nover_in, sizeof(real)*(ipsi_nover_out-ipsi_nover_in+1), offset_all1+(8*sizeof(int)+2*sizeof(real)));
    assert(nbyte2 == sizeof(real)*(mpsi+1));

    pread64(file1_r, &nbyte3, sizeof(int), offset_all1+(9*sizeof(int)+sizeof(real)*(mpsi+3)));
    pread64(file1_r, phitmp+(igrid_nover_in-igrid_moments_in), sizeof(real)*nloc_nover, offset_all1+(10*sizeof(int)+sizeof(real)*(mpsi+3)));
    assert(nbyte3 ==sizeof(real)*mgrid);

    pread64(file1_r, &nbyte3, sizeof(int), offset_all1+(11*sizeof(int)+sizeof(real)*(mpsi+3+mgrid)));
    pread64(file1_r, densityitmp+(igrid_nover_in-igrid_moments_in), sizeof(real)*nloc_nover, offset_all1+(12*sizeof(int)+sizeof(real)*(mpsi+3+mgrid)));
    assert(nbyte3 == sizeof(real)*mgrid);

    pread64(file1_r, &nbyte2, sizeof(int), offset_all1+(13*sizeof(int)+sizeof(real)*(mpsi+3+2*mgrid)));
    pread64(file1_r, zonali_r+ipsi_nover_in, sizeof(real)*(ipsi_nover_out-ipsi_nover_in+1), offset_all1+(14*sizeof(int)+sizeof(real)*(mpsi+3+2*mgrid)));
    assert(nbyte2 == sizeof(real)*(mpsi+1));

  } else if (radial_decomp->myrank_radiald>0&&radial_decomp->myrank_radial_partd==radial_decomp->nproc_radial_partd-1){
    int nbyte2, nbyte3;
    off_t offset = offset_all1+(10*sizeof(int)+sizeof(real)*(mpsi+3+igrid_nover_in-1));
    off_t offset0 = offset_all1+(10*sizeof(int)+sizeof(real)*(mpsi+3+mgrid));

    off_t offset1 = offset_all1+(12*sizeof(int)+sizeof(real)*(mpsi+3+mgrid+igrid_nover_in-1));
    off_t offset2 = offset_all1+(12*sizeof(int)+sizeof(real)*(mpsi+3+2*mgrid));

    pread64(file1_r, phip00+ipsi_nover_in, sizeof(real)*(ipsi_nover_out-ipsi_nover_in+1), offset_all1+(8*sizeof(int)+sizeof(real)*(ipsi_nover_in+2)));
    if (radial_decomp->myrank_radiald == radial_decomp->nproc_radiald -1){
      pread64(file1_r, &nbyte2, sizeof(int), offset_all1+(8*sizeof(int)+sizeof(real)*(mpsi+3)));
      assert(nbyte2 == sizeof(real)*(mpsi+1));
    }
    
    pread64(file1_r, phitmp+(igrid_nover_in-igrid_moments_in), sizeof(real)*nloc_nover, offset);
    if (radial_decomp->myrank_radiald == radial_decomp->nproc_radiald -1){
      pread64(file1_r, &nbyte3, sizeof(int), offset0);
      assert(nbyte3 == sizeof(real)*mgrid);
    }
    
    pread64(file1_r, densityitmp+(igrid_nover_in-igrid_moments_in), sizeof(real)*nloc_nover, offset1);
    if (radial_decomp->myrank_radiald == radial_decomp->nproc_radiald -1){
      pread64(file1_r, &nbyte3, sizeof(int), offset2);
      assert(nbyte3 == sizeof(real)*mgrid);
    }

    pread64(file1_r, zonali_r+ipsi_nover_in, sizeof(real)*(ipsi_nover_out-ipsi_nover_in+1), offset_all1+(14*sizeof(int)+sizeof(real)*(mpsi+3+2*mgrid+ipsi_nover_in)));
    if (radial_decomp->myrank_radiald == radial_decomp->nproc_radiald -1){
      pread64(file1_r, &nbyte2, sizeof(int), offset_all1+(14*sizeof(int)+sizeof(real)*(mpsi+3+mpsi+1+2*mgrid)));
      assert(nbyte2 == sizeof(real)*(mpsi+1));
    }
    
  }
  if (0 != (rc1_r= close(file1_r))){
    perror("close failed");
    MPI_Abort(MPI_COMM_WORLD, 1);
    }
  for (int i=ipsi_nover_in; i<ipsi_nover_out+1; i++){
    assert(zonali[i] == zonali_r[i]);
  }
  free(zonali_r);
  // test end
  */
#endif   /* #if USE_ADIOS */

  real timing_e = MPI_Wtime() - timing_b;
  real timing_max=0.0; real timing_min=0.0;
  MPI_Allreduce(&timing_e, &timing_max, 1, MPI_MYREAL, MPI_MAX, parallel_decomp->partd_comm);
  MPI_Allreduce(&timing_e, &timing_min, 1, MPI_MYREAL, MPI_MIN, parallel_decomp->partd_comm);

  if (parallel_decomp->mype==0)
    printf("istep=%d timing_e_inter=%e timing_inter_max=%e timing_inter_min=%e timing=%e, timing_max=%e timing_min=%e\n", istep, timing_e_inter, timing_inter_max, timing_inter_min, timing_e, timing_max, timing_min);

#endif  /* if CALC_MOMENTS */

  return 0;
}
