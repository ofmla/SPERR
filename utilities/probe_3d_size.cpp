#include "SPECK3D_Compressor.h"
#include "SPECK3D_Decompressor.h"

#include "SPECK3D_OMP_C.h"
#include "SPECK3D_OMP_D.h"

#include "CLI11.hpp"

#include <cstdlib>
#include <iostream>
#include <chrono>
#include <cstring>
#include <cctype> // std::tolower()

// This file should only be compiled in non QZ_TERM mode.
#ifndef QZ_TERM


auto test_configuration_omp( const float*         in_buf, 
                             speck::dims_type     dims,
                             speck::dims_type     chunks,
                             float                bpp, 
                             std::array<bool, 8>  condi_settings,
                             size_t               omp_num_threads,
                             std::vector<float>&  output_buf ) -> int
{
    // Setup
    const size_t total_vals = dims[0] * dims[1] * dims[2];
    SPECK3D_OMP_C compressor;
    compressor.set_dims(dims);
    compressor.prefer_chunk_dims( chunks );
    compressor.set_bpp( bpp );
    compressor.toggle_conditioning( condi_settings );
    compressor.set_num_threads( omp_num_threads );
    auto rtn = compressor.use_volume( in_buf, total_vals );
    if(  rtn != RTNType::Good )
        return 1;

    // Perform actual compression work
    auto start_time = std::chrono::steady_clock::now();
    rtn = compressor.compress();
    if(  rtn != RTNType::Good )
        return 1;
    auto end_time = std::chrono::steady_clock::now();
    auto diff_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time-start_time).count();
    std::cout << " -> Compression takes time: " << diff_time << "ms\n";

    auto encoded_stream = compressor.get_encoded_bitstream();
    if( encoded_stream.empty() )
        return 1;
    else
        printf("    Total compressed size in bytes = %ld, average bpp = %.2f\n",
               encoded_stream.size(), float(encoded_stream.size() * 8) / float(total_vals) );


    // Perform decompression
    SPECK3D_OMP_D decompressor;
    decompressor.set_num_threads( omp_num_threads );
    rtn = decompressor.use_bitstream( encoded_stream.data(), encoded_stream.size() );
    if( rtn != RTNType::Good )
        return 1;

    start_time = std::chrono::steady_clock::now();
    rtn = decompressor.decompress( encoded_stream.data() );
    if(  rtn != RTNType::Good )
        return 1;
    end_time = std::chrono::steady_clock::now();
    diff_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time-start_time).count();
    std::cout << " -> Decompression takes time: " << diff_time << "ms\n";

    output_buf = decompressor.get_data<float>();
    if( output_buf.size() != total_vals )
        return 1;


    // Collect statistics
    float rmse, lmax, psnr, arr1min, arr1max;
    speck::calc_stats( in_buf, output_buf.data(), total_vals,
                       &rmse, &lmax, &psnr, &arr1min, &arr1max);
    printf("    Original data range = (%.2e, %.2e)\n", arr1min, arr1max);
    printf("    Reconstructed data RMSE = %.2e, L-Infty = %.2e, PSNR = %.2fdB\n", rmse, lmax, psnr);

    return 0;

}


int main( int argc, char* argv[] )
{
    //
    // Parse command line options
    //
    CLI::App app("CLI options to probe_3d");

    std::string input_file;
    app.add_option("filename", input_file, "Input data file to probe")
            ->required()->check(CLI::ExistingFile);

    std::vector<size_t> dims_v;
    app.add_option("--dims", dims_v, "Dimensions of the input volume.\n"
            "For example, `--dims 128 128 128`.\n")->required()->expected(3);

    std::vector<size_t> chunks_v{ 64, 64, 64 };
    app.add_option("--chunks", chunks_v, "Dimensions of the preferred chunk size. "
            "E.g., `--chunks 64 64 64`.\n"
            "If not specified, then 64^3 will be used\n")->expected(3);

    float bpp;
    auto* bpp_ptr = app.add_option("--bpp", bpp, "Target bit-per-pixel value.\n"
                    "For example, `--bpp 0.5`.\n")->check(CLI::Range(0.0f, 64.0f));

    bool div_rms = false;
    app.add_flag("--div-rms", div_rms, "Conditioning: calculate rms of each chunk and divide every\n"
                                       "value by rms of its chunk. Default: not applied.\n");

    size_t omp_num_threads = 4;
    auto* omp_ptr = app.add_option("--omp", omp_num_threads, "Number of OpenMP threads to use. "
                                   "Default: 4\n"); 

    CLI11_PARSE(app, argc, argv);

    const auto dims           = std::array<size_t, 3>{dims_v[0], dims_v[1], dims_v[2]};
    const auto condi_settings = std::array<bool, 8>{ true,      // subtract mean
                                                     div_rms,   // divide by rms
                                                     false,     // unused
                                                     false,     // unused
                                                     false,     // unused
                                                     false,     // unused
                                                     false,     // unused
                                                     false };   // unused

    //
    // Read and keep a copy of input data (will be used for testing different configurations)
    // Also create a buffer to hold decompressed data.
    //
    const size_t total_vals = dims[0] * dims[1] * dims[2];
    auto input_buf = speck::read_whole_file<float>( input_file.c_str() );
    if( input_buf.size() != total_vals ) {
        std::cerr << "  -- reading input file failed!" << std::endl;
        return 1;
    }
    auto output_buf = std::vector<float>();

    //
    // Let's do an initial analysis
    //
    if( !(*bpp_ptr) ) {
        bpp = 4.0; // We decide to use 4 bpp for initial analysis
    }
    printf("Initial analysis: compression at %.2f bit-per-pixel...  \n", bpp);
    int rtn = test_configuration_omp( input_buf.data(), dims, {chunks_v[0], chunks_v[1], chunks_v[2]},
                                      bpp, condi_settings, omp_num_threads, output_buf );
    if( rtn != 0 )
        return rtn;

    //
    // Now it enters the interactive session
    //
    char answer;
    std::cout << "\nDo you want to explore other bit-per-pixel values             (y),\n"
                   "               output the current decompressed file to disk,  (o),\n"
                   "               or quit?                                       (q): ";
    std::cin >> answer;
    while ( std::tolower(answer) == 'y' || std::tolower(answer) == 'o' ) {

        switch (std::tolower(answer) ) {
          case 'y': 
            bpp = -1.0;
            std::cout << "\nPlease input a new bpp value to test [0.0 - 64.0]:  ";
            std::cin >> bpp;
            while (bpp <= 0.0 || bpp >= 64.0 ) {
                std::cout << "Please input a bpp value inbetween [0.0 - 64.0]:  ";
                std::cin >> bpp;
            }
            printf("\nNow testing bpp = %.2f ...\n", bpp);
        
            rtn = test_configuration_omp( input_buf.data(), dims, {chunks_v[0], chunks_v[1], chunks_v[2]},
                                          bpp, condi_settings, omp_num_threads, output_buf );
            if ( rtn != 0 )
                return rtn;

            break;

          case 'o':
            std::string fname;
            std::cout << std::endl << "Please input a filename to use: ";
            std::cin >> fname;
            auto rtn2 = speck::write_n_bytes( fname.c_str(), 
                               sizeof(decltype(output_buf)::value_type) * output_buf.size(),
                               output_buf.data() );
            if( rtn2 == RTNType::Good )
                std::cout << "written decompressed file: " << fname << std::endl;
            else {
                std::cerr << "writign decompressed file error: " << fname << std::endl;
                return 1;
            }
        } // end of switch

        std::cout << "\nDo you want to explore other bit-per-pixel values,            (y)\n"
                       "               output the current decompressed file to disk,  (o)\n"
                       "               or quit?                                       (q): ";
        std::cin >> answer;
        answer = std::tolower(answer);
    } // end of while
    
    std::cout << "\nHave a good day! \n";
    
    return 0;
}


#endif
