//
// Created by Andrew Pontzen on 18/11/2016.
//

#ifndef IC_TIPSY_HPP
#define IC_TIPSY_HPP

#include "../io.hpp"

namespace io {
  namespace tipsy {

    struct io_header_tipsy {
      double scalefactor;
      int n;
      int ndim;
      int ngas;
      int ndark;
      int nstar;
    } header_tipsy;


    template<typename MyFloat>
    void saveFieldTipsyArray(const std::string &filename,
                             shared_ptr<ParticleMapper<MyFloat>> pMapper) {
      ofstream outfile(filename.c_str(), ofstream::binary);
      int lengthField = pMapper->size();
      outfile.write(reinterpret_cast<char *>(&lengthField), 4);

      for (auto i = pMapper->begin(); i != pMapper->end(); ++i) {
        float data = float(i.getField().real());
        outfile.write(reinterpret_cast<char *>(&data), 4);
      }
    }

    namespace TipsyParticle {

      struct dark {
        float mass, x, y, z, vx, vy, vz, eps, phi;
      };

      struct gas {
        float mass, x, y, z, vx, vy, vz, rho, temp, eps, metals, phi;
      };

      template<typename T>
      void initialise(dark &p, const CosmologicalParameters<T> &cosmo) {
        p.phi = 0.0;
      }

      template<typename T>
      void initialise(gas &p, const CosmologicalParameters<T> &cosmo) {
        p.temp = cosmo.TCMB / cosmo.scalefactor;
        p.metals = 0.0;
        p.rho = 0.0;
      }
    }


    template<typename MyFloat>
    class TipsyOutput {
    protected:
      FILE *fd;
      ofstream photogenic_file;
      size_t iord;
      double pos_factor, vel_factor, mass_factor, min_mass, max_mass;
      double boxLength;
      shared_ptr<ParticleMapper<MyFloat>> pMapper;
      const CosmologicalParameters<MyFloat> &cosmology;


      template<typename ParticleType>
      void saveTipsyParticlesFromBlock(std::vector<Particle<MyFloat>> &particleAr) {

        const size_t n = particleAr.size();

        /*
        for(auto &q: {xAr,yAr,zAr,vxAr,vyAr,vzAr,massAr,epsAr}) {
            assert(q.size()==n);
        }
        */

        std::vector<ParticleType> p(n);

    #pragma omp parallel for
        for (size_t i = 0; i < n; i++) {
          TipsyParticle::initialise(p[i], cosmology);
          Particle<MyFloat> &thisParticle = particleAr[i];

          p[i].x = thisParticle.pos.x * pos_factor - 0.5;
          p[i].y = thisParticle.pos.y * pos_factor - 0.5;
          p[i].z = thisParticle.pos.z * pos_factor - 0.5;
          p[i].eps = thisParticle.soft * pos_factor;

          p[i].vx = thisParticle.vel.x * vel_factor;
          p[i].vy = thisParticle.vel.y * vel_factor;
          p[i].vz = thisParticle.vel.z * vel_factor;
          p[i].mass = thisParticle.mass * mass_factor;

          if (thisParticle.mass == min_mass && omp_get_thread_num() == 0) {
            photogenic_file << iord << endl;
          }

          ++iord;
        }

        fwrite(p.data(), sizeof(ParticleType), n, fd);
      }

      template<typename ParticleType>
      void saveTipsyParticles(MapperIterator<MyFloat> &&begin, MapperIterator<MyFloat> &&end) {

        ParticleType p;
        TipsyParticle::initialise(p, cosmology);
        std::vector<Particle<MyFloat>> particles;
        auto i = begin;
        while (i != end) {
          i.getNextNParticles(particles);
          saveTipsyParticlesFromBlock<ParticleType>(particles);
        }
      }

      template<typename ParticleType>
      void initTipsyParticle(ParticleType &p) {

      }


      template<typename ParticleType>
      void saveTipsyParticlesSingleThread(MapperIterator<MyFloat> &&begin, MapperIterator<MyFloat> &&end) {

        ParticleType p;
        MyFloat x, y, z, vx, vy, vz, mass, eps;

        for (auto i = begin; i != end; ++i) {
          auto thisParticle = i.getParticle();

          p.x = thisParticle.pos.x * pos_factor - 0.5;
          p.y = thisParticle.pos.y * pos_factor - 0.5;
          p.z = thisParticle.pos.z * pos_factor - 0.5;
          p.eps = thisParticle.soft * pos_factor;

          p.vx = thisParticle.vel.x * vel_factor;
          p.vy = thisParticle.vel.y * vel_factor;
          p.vz = thisParticle.vel.z * vel_factor;
          p.mass = thisParticle.mass * mass_factor;

          fwrite(&p, sizeof(ParticleType), 1, fd);

          if (mass == min_mass) {
            photogenic_file << iord << endl;
          }

          ++iord;
        }
      }


    public:

      TipsyOutput(double boxLength,
                  shared_ptr<ParticleMapper<MyFloat>> pMapper,
                  const CosmologicalParameters<MyFloat> &cosmology) : boxLength(boxLength), pMapper(pMapper),
                                                                      cosmology(cosmology) {

      }

      void operator()(const std::string &filename) {

        // originally:
        // pmass in 1e10 h^-1 Msol
        // pos in Mpc h^-1
        // vel in km s^-1 a^1/2


        min_mass = std::numeric_limits<double>::max();
        max_mass = 0.0;

        MyFloat mass, tot_mass = 0.0;

        for (auto i = pMapper->begin(); i != pMapper->end(); ++i) {
          // progress("Pre-write scan file",iord, totlen);
          mass = i.getMass(); // sometimes can be MUCH faster than getParticle
          if (min_mass > mass) min_mass = mass;
          if (max_mass < mass) max_mass = mass;
          tot_mass += mass;
        }

        // end_progress();

        if (min_mass != max_mass) {
          photogenic_file.open("photogenic.txt");
        }

        mass_factor = cosmology.OmegaM0 / tot_mass; // tipsy convention: sum(mass)=Om0
        pos_factor = 1. / boxLength;              // boxsize = 1

        double dKpcUnit = boxLength * 1000 / cosmology.hubble;
        double dMsolUnit = 1e10 / cosmology.hubble / mass_factor;
        double dKmsUnit = sqrt(4.30211349e-6 * dMsolUnit / (dKpcUnit));

        vel_factor = std::pow(cosmology.scalefactor, -0.5) / dKmsUnit;


        io_header_tipsy header;

        header.scalefactor = cosmology.scalefactor;
        header.n = pMapper->size();
        header.ndim = 3;
        header.ngas = pMapper->size_gas();
        header.ndark = pMapper->size_dm();
        header.nstar = 0;


        cout << "TIPSY parameters:" << endl;


        ofstream paramfile;
        paramfile.open("tipsy.param");

        paramfile << "dKpcUnit = " << dKpcUnit << endl;
        paramfile << "dMsolUnit = " << dMsolUnit << endl;
        paramfile << "dHubble0 = " << 0.1 * cosmology.hubble * dKpcUnit / dKmsUnit << endl;
        paramfile << "bComove = 1 " << endl;

        paramfile.close();

        fd = fopen(filename.c_str(), "w");
        if (!fd) throw std::runtime_error("Unable to open file for writing");


        fwrite(&header, sizeof(io_header_tipsy), 1, fd);

        saveTipsyParticles<TipsyParticle::gas>(pMapper->beginGas(), pMapper->endGas());
        saveTipsyParticles<TipsyParticle::dark>(pMapper->beginDm(), pMapper->endDm());

      }
    };

    template<typename T>
    void save(const std::string &filename, double Boxlength,
              shared_ptr<ParticleMapper<T>> pMapper,
              const CosmologicalParameters<T> &cosmology) {

      TipsyOutput<T> output(Boxlength, pMapper, cosmology);
      output(filename);
    }



  }
}

#endif //IC_TIPSY_HPP