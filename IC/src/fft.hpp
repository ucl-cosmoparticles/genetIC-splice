#ifndef FFTH_INCLUDED
#define FFTH_INCLUDED

#ifdef FFTW3
// FFTW3 VERSION

#include <stdexcept>
#include <fftw3.h>

#ifdef _OPENMP
#include <omp.h>
#endif

void init_fftw_threads() {
    #ifdef FFTW_THREADS
        if(fftw_init_threads()==0)
            throw std::runtime_error("Cannot initialize FFTW threads");
    #ifndef _OPENMP
        fftw_plan_with_nthreads(FFTW_THREADS);
    #else
        fftw_plan_with_nthreads(omp_get_num_threads());
    #endif
    #endif
}

template<typename MyFloat>
void repackForReality(std::complex<MyFloat> *ft_full, std::complex<MyFloat> *ft_reduced, const int nx) {
    // for testing purposes only!
    const int nz = nx/2+1;

    for(size_t x=0; x<nx; x++)
        for(size_t y=0; y<nx; y++)
            for(size_t z=0; z<nz; z++)
                ft_reduced[z+nz*y+nz*nx*x] = ft_full[z+nx*y+nx*nx*x];

}

template<typename MyFloat>
void repackForReality(std::complex<MyFloat> *ft, const int nx) {
    repackForReality(ft,ft,nx);
}

template<typename MyFloat>
void fft(std::complex<MyFloat> *fto, std::complex<MyFloat> *ftin,
                             const unsigned int res, const  int dir)
{
    throw std::runtime_error("Sorry, the fourier transform has not been implemented for your specified precision");
    // you'll need to implement an alternative specialisation like the one below for the correct calls
    // see http://www.fftw.org/doc/Precision.html#Precision
}

template<typename MyFloat>
void fft_real(MyFloat *fto, std::complex<MyFloat> *ftin,
                             const unsigned int res, const  int dir)
{
    throw std::runtime_error("Sorry, the fourier transform has not been implemented for your specified precision");
    // you'll need to implement an alternative specialisation like the one below for the correct calls
    // see http://www.fftw.org/doc/Precision.html#Precision
}

template<>
void fft<double>(std::complex<double> *fto, std::complex<double> *ftin,
                             const unsigned int res, const  int dir)
{

  init_fftw_threads();

  fftw_plan plan;
  size_t i;
  double norm = pow(static_cast<double>(res), 1.5);
  size_t len = static_cast<size_t>(res*res);
  len*=res;

  if(dir==1)
    plan = fftw_plan_dft_3d(res,res,res,
                            reinterpret_cast<fftw_complex*>(&ftin[0]),
                            reinterpret_cast<fftw_complex*>(&fto[0]),
                            FFTW_FORWARD, FFTW_ESTIMATE);

  else if(dir==-1)
    plan = fftw_plan_dft_3d(res,res,res,
                            reinterpret_cast<fftw_complex*>(&ftin[0]),
                            reinterpret_cast<fftw_complex*>(&fto[0]),
                            FFTW_BACKWARD, FFTW_ESTIMATE);




  else throw std::runtime_error("Incorrect direction parameter to fft");

  fftw_execute(plan);
  fftw_destroy_plan(plan);

  #pragma omp parallel for schedule(static) private(i)
  for(i=0;i<len;i++)
      fto[i]/=norm;


}

template<>
void fft_real<double>(double *fto, std::complex<double> *ftin,
                             const unsigned int res, const  int dir)
{

  init_fftw_threads();

  fftw_plan plan;
  size_t i;
  double norm = pow(static_cast<double>(res), 1.5);
  size_t len = static_cast<size_t>(res*res);
  len*=res;

  if(dir==-1)
    plan = fftw_plan_dft_c2r_3d(res,res,res,
                                reinterpret_cast<fftw_complex*>(&ftin[0]),
                                reinterpret_cast<double*>(&fto[0]),
                                FFTW_BACKWARD | FFTW_ESTIMATE);


  else if(dir==-1)
    throw std::runtime_error("Not implemented");


  else throw std::runtime_error("Incorrect direction parameter to fft");

  fftw_execute(plan);
  fftw_destroy_plan(plan);

  #pragma omp parallel for schedule(static) private(i)
  for(i=0;i<len;i++)
      fto[i]/=norm;


}



#else



// FFTW2 VERSION

#ifndef DOUBLEPRECISION     /* default is single-precision */

    #include <srfftw.h>

    #ifdef HAVE_HDF5
        hid_t hdf_float = H5Tcopy (H5T_NATIVE_FLOAT);
        hid_t hdf_double = H5Tcopy (H5T_NATIVE_FLOAT);
    #endif
    //#else
    //#if (DOUBLEPRECISION == 2)   /* mixed precision, do we want to implement this at all? */
    //typedef float   MyFloat;
    //typedef double  MyDouble;
    //hid_t hdf_float = H5Tcopy (H5T_NATIVE_FLOAT);
    //hid_t hdf_double = H5Tcopy (H5T_NATIVE_DOUBLE);
#else                        /* everything double-precision */
    #ifdef FFTW_TYPE_PREFIX
    #include <drfftw.h>
    #else
    #include <rfftw.h>
    #endif
#endif




template<typename MyFloat>
void fft(std::complex<MyFloat> *fto, std::complex<MyFloat> *ftin, const int res, const int dir)
{ //works when fto and ftin and output are the same, but overwrites fto for the backwards transform so we have another temporary array there

    long i;

  if(dir==1){
    std::complex<MyFloat> *fti = (std::complex<MyFloat>*)calloc(res*res*res,sizeof(std::complex<MyFloat>));
    for(i=0;i<res*res*res;i++){fti[i]=ftin[i]/sqrt((MyFloat)(res*res*res));}
    //for(i=0;i<res*res*res;i++){fti[i]=ftin[i]/(MyFloat)(res*res*res);}
    fftwnd_plan pf = fftw3d_create_plan( res, res, res, FFTW_FORWARD, FFTW_ESTIMATE );
    fftwnd_one(pf, reinterpret_cast<fftw_complex*>(&fti[0]), reinterpret_cast<fftw_complex*>(&fto[0]));
    fftwnd_destroy_plan(pf);
    free(fti);
    }

  else if(dir==-1){
    std::complex<MyFloat> *fti = (std::complex<MyFloat>*)calloc(res*res*res,sizeof(std::complex<MyFloat>));
    //memcpy(fti,ftin,res*res*res*sizeof(fftw_complex));
    for(i=0;i<res*res*res;i++){fti[i]=ftin[i]/sqrt((MyFloat)(res*res*res));}
    fftwnd_plan pb;
     pb = fftw3d_create_plan( res,res,res, FFTW_BACKWARD, FFTW_ESTIMATE );
    fftwnd_one(pb, reinterpret_cast<fftw_complex*>(&fti[0]), reinterpret_cast<fftw_complex*>(&fto[0]));
    fftwnd_destroy_plan(pb);
    free(fti);
      }

  else {std::cerr<<"wrong parameter for direction in fft"<<std::endl;}

}

#endif

#endif // FFTH_INCLUDED
