/*
 * Copyright (c) 2023 University of Michigan.
 *
 */

#ifndef GREEN_DFLR_H
#define GREEN_DFLR_H

#include <green/symmetry/symmetry.h>
#include <green/utils/mpi_shared.h>
#include <green/utils/mpi_utils.h>

#include <green/mbpt/except.h>

namespace green::mbpt {
  class df_legacy_reader{
    // prefixes for hdf5
    const std::string _chunk_basename    = "VQ";

    using bz_utils_t                     = symmetry::brillouin_zone_utils<symmetry::inv_symm_op>;
    using int_data                       = utils::shared_object<ztensor<4>>;

  public:
    df_legacy_reader(const std::string& path, int nao, int NQ, const bz_utils_t& bz_utils) :
        _base_path(path), _k0(-1), _current_chunk(-1), _chunk_size(0), _NQ(NQ), _bz_utils(bz_utils) {
      h5pp::archive ar(path + "/meta.h5");
      if(ar.has_attribute("__green_version__")) {
        std::string int_version = ar.get_attribute<std::string>("__green_version__");
        if (int_version.rfind(INPUT_VERSION, 0) != 0) {
          throw mbpt_outdated_input("Integral files at '" + path +"' are outdated, please run migration script python/migrate.py");
        }
      } else {
        throw mbpt_outdated_input("Integral files at '" + path +"' are outdated, please run migration script python/migrate.py");
      }
      ar["chunk_size"] >> _chunk_size;
      ar.close();
      _vij_Q = std::make_shared<int_data>(_chunk_size, NQ, nao, nao);
    }

    virtual ~df_legacy_reader() {}

    void read_integrals(size_t idx_red) {
      if ((idx_red / _chunk_size) == _current_chunk) return;  // we have data cached

      _current_chunk = idx_red / _chunk_size;

      size_t c_id    = _current_chunk * _chunk_size;
      (*_vij_Q).fence();
      if (!utils::context.node_rank) read_a_chunk(c_id, _vij_Q->object());
      (*_vij_Q).fence();
    }


    void read_a_chunk(size_t c_id, ztensor<4>& V_buffer) {
      std::string   fname = _base_path + "/" + _chunk_basename + "_" + std::to_string(c_id) + ".h5";
      h5pp::archive ar(fname);
      ar["/" + std::to_string(c_id)] >> reinterpret_cast<double*>(V_buffer.data());
      ar.close();
    }

    void reset() {
      _current_chunk = -1;
      _k0            = -1;
    }
    const std::shared_ptr<int_data> operator()() const{ return _vij_Q; }
    const std::complex<double> *operator()(int red_key_in_chunk) const{ 
      auto shape = _vij_Q->object().shape();
      std::size_t extent=shape[1]*shape[2]*shape[3];
      return _vij_Q->object().data()+red_key_in_chunk*extent; 
    }
    int current_chunk() const{ return _current_chunk;}
    int chunk_size() const{ return _chunk_size;}

  private:
    // Coulomb integrals stored in density fitting format
    std::shared_ptr<int_data> _vij_Q;
    // current leading index
    int                       _k0;
    long                      _current_chunk;
    long                      _chunk_size;
    long                      _NQ;
    const bz_utils_t&         _bz_utils;

    // base path to integral files
    std::string               _base_path;
  };

}  // namespace green::mbpt

#endif  // GF2_DFLR_H
