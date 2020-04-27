#include "SPECK3D.h"
#include "CDF97.h"

#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <cstring>

#include "SpeckConfig.h"

extern "C"  // C Function calls, and don't include the C header!
{
    int sam_write_n_floats( const char*, size_t, const float* );
}


int main( int argc, char* argv[] )
{
    if( argc != 4 )
    {
        std::cerr << "Usage: ./a.out input_filename target_bit_per_pixel output_filename" << std::endl;
        return 1;
    }

    const char*   input  = argv[1];
          float   target_bpp = std::atof( argv[2] );
    target_bpp    = std::max( 0.0f, target_bpp );
    const char*   output = argv[3];

    // Do a speck decoding
    speck::SPECK3D  decoder;
    if( decoder.read_from_disk( input ) )
    {
        std::cerr << "input file read error: " << input << std::endl;
        return 1;
    }
#ifdef TIME_EXAMPLES
    const auto startT = std::chrono::high_resolution_clock::now();
#endif
    size_t dim_x, dim_y, dim_z;
    decoder.get_dims( dim_x, dim_y, dim_z );
    size_t total_vals = dim_x * dim_y * dim_z;
    size_t bit_budget = size_t( total_vals * target_bpp );
           bit_budget = std::min( bit_budget, decoder.get_bit_buffer_size() );
    decoder.set_bit_budget( bit_budget );
    decoder.decode();

    // Do an inverse wavelet transform
    speck::CDF97 idwt;
    idwt.set_dims( dim_x, dim_y, dim_z );
    idwt.set_mean( decoder.get_image_mean() );
    idwt.take_data( decoder.release_coeffs_double() );
    idwt.idwt3d();

#ifdef TIME_EXAMPLES
    // Finish timer and print timing
    const auto endT   = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double> diffT  = endT - startT;
    std::cout << "Time for decompression in milliseconds: " << diffT.count() * 1000.0f << std::endl;
#endif

    // Write the reconstructed data to disk in single-precision
    speck::buffer_type_f out_buf = std::make_unique<float[]>( total_vals );
    const auto& reconstruct = idwt.get_read_only_data();
    for( size_t i  = 0; i < total_vals; i++ )
        out_buf[i] = reconstruct[i];
    if( sam_write_n_floats( output, total_vals, out_buf.get() ) )
    {
        std::cerr << "output file write error: " << output << std::endl;
        return 1;
    }

    return 0;
}
